/* Lobby -> game server admission roster. SPDX-License-Identifier: AGPL-3.0-or-later */
/* Only this translation unit needs wide filesystem metadata. A 32-bit Linux
 * stat() rejects otherwise readable Windows bind-mount files when st_ino does
 * not fit ino_t (EOVERFLOW). Keep the public admission ABI unchanged. */
#if defined(__linux__) && !defined(_LARGEFILE64_SOURCE)
#define _LARGEFILE64_SOURCE 1
#endif
#include "admission.h"
#include "ol_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <limits.h>
#ifdef OL_SERVER
#include "q_shared.h"
#include "cvar.h"
#include "qcommon_io.h"
#endif

#if defined(__linux__)
typedef struct stat64 admission_stat_t;
#define admission_stat stat64
#else
typedef struct stat admission_stat_t;
#define admission_stat stat
#endif

#define ROSTER_MAX 256
#define ROSTER_BYTES (64 * 1024)

static admission_entry_t roster[ROSTER_MAX];
static size_t roster_count;
/* Room guests have no DTLS credential of their own. Their authorization is
 * bound to the host authenticated by the association, their uid and source IP. */
static struct {
    uint64_t host_uid;
    admission_entry_t seat;
} members[ROSTER_MAX];
static size_t member_count;
static struct {
    unsigned port, epoch;
    char session[33];
    uint64_t uid;
    long long expires;
} quick_tickets[ROSTER_MAX];
static size_t quick_ticket_count;
static struct {
    unsigned port;
    char session[33];
    uint64_t uid;
    long long expires;
    party_assignment_t assignment;
} party_tickets[ROSTER_MAX];
static size_t party_ticket_count;
static struct {
    unsigned port;
    char preparation[33], lease[33];
    uint64_t uid;
    long long expires;
} warm_tickets[ROSTER_MAX];
static size_t warm_ticket_count;
static char roster_path[1024];
static long long roster_mtime = -1, roster_size = -1, roster_mtime_ns = -1, roster_inode = -1;
#ifdef OL_SERVER
static cvar_t *admission_path, *admission_required, *warm_admission_path;
#endif

static void erase(void *ptr, size_t n)
{ volatile uint8_t *p = ptr; while (n--) *p++ = 0; }

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static const char *field(const char *p, char *out, size_t cap)
{
    size_t n = 0;
    while (*p == ' ' || *p == '\t') ++p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
        if (n + 1 >= cap) return NULL;
        out[n++] = *p++;
    }
    out[n] = 0;
    return n ? p : NULL;
}

/* Website names use ASCII plus CJK ideographs, carried as complete UTF-8.
 * Reject truncated/overlong encodings and separators before the name enters
 * an info-string. Never reinterpret bytes through the host ANSI code page. */
static int valid_gamertag(const char *tag)
{
    const unsigned char *p = (const unsigned char *)tag;
    while (*p) {
        unsigned cp;
        if (*p < 128) {
            if (*p <= 32 || *p == 127 || *p == '\\' || *p == '"' || *p == ';' || *p == '%') return 0;
            ++p;
            continue;
        }
        if (*p < 0xe0 || *p > 0xef || !p[1] || (p[1] & 0xc0) != 0x80 ||
            !p[2] || (p[2] & 0xc0) != 0x80) return 0;
        cp = ((p[0] & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63);
        if (cp < 0x3400 || cp > 0x9fff) return 0;
        p += 3;
    }
    return 1;
}

int AdmissionParseLine(const char *line, admission_entry_t *out)
{
    char ip[16], key[33], uid[17], expires[24], tag[33];
    admission_entry_t e;
    unsigned a, b, c, d;
    size_t i;
    char *end;
    const char *p = line;
    if (!line || !out) return 0;
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p || *p == '#' || *p == '\r' || *p == '\n') return 0;
    memset(&e, 0, sizeof(e));
    if (!(p = field(p, ip, sizeof(ip))) || !(p = field(p, key, sizeof(key))) ||
        !(p = field(p, uid, sizeof(uid))) || !(p = field(p, expires, sizeof(expires))) ||
        !(p = field(p, tag, sizeof(tag)))) return 0;
    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
        return 0;
    e.ip[0] = (uint8_t)a; e.ip[1] = (uint8_t)b; e.ip[2] = (uint8_t)c; e.ip[3] = (uint8_t)d;
    if (strlen(key) != 32) return 0;
    for (i = 0; i < 16; ++i) {
        int hi = hexval(key[2 * i]), lo = hexval(key[2 * i + 1]);
        if (hi < 0 || lo < 0) return 0;
        e.key[i] = (uint8_t)(hi << 4 | lo);
    }
    if (strcmp(uid, "0")) {
        if (strlen(uid) != 16) return 0;
        for (i = 0; i < 16; ++i) {
            int v = hexval(uid[i]);
            if (v < 0) return 0;
            e.uid = e.uid << 4 | (uint64_t)v;
        }
    }
    e.expires = strtoll(expires, &end, 10);
    if (*end || e.expires <= 0) return 0;
    if (!valid_gamertag(tag)) return 0;
    i = strlen(tag);
    /* "-" is the lobby's placeholder for a seat whose gamertag is unknown. */
    if (strcmp(tag, "-")) memcpy(e.gamertag, tag, i + 1);
    *out = e;
    erase(&e, sizeof(e));
    return 1;
}

static int parse_member(const char *line, uint64_t *host_uid, admission_entry_t *seat)
{
    char kind[7], ip[16], host[17], uid[17], expires[24], tag[33], normalized[160];
    const char *p = line;
    uint64_t host_value = 0;
    size_t i;
    if (!(p = field(p, kind, sizeof(kind))) || strcmp(kind, "member") ||
        !(p = field(p, ip, sizeof(ip))) || !(p = field(p, host, sizeof(host))) ||
        !(p = field(p, uid, sizeof(uid))) || !(p = field(p, expires, sizeof(expires))) ||
        !(p = field(p, tag, sizeof(tag)))) return 0;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p || strlen(host) != 16 || strlen(uid) != 16) return 0;
    for (i = 0; i < 16; ++i) {
        int v = hexval(host[i]);
        if (v < 0) return 0;
        host_value = (host_value << 4) | (uint64_t)v;
    }
    snprintf(normalized, sizeof(normalized), "%s 00000000000000000000000000000000 %s %s %s",
             ip, uid, expires, tag);
    if (!host_value || !AdmissionParseLine(normalized, seat) || !seat->uid ||
        seat->uid == host_value) return 0;
    *host_uid = host_value;
    return 1;
}

static int decimal(const char *s, unsigned long long max, unsigned long long *out)
{
    unsigned long long n=0;
    if (!s || !*s || (s[0]=='0' && s[1])) return 0;
    for (;*s;++s) {
        unsigned d=(unsigned char)*s-'0';
        if (d>9 || d>max || n>(max-d)/10) return 0;
        n=n*10+d;
    }
    *out=n;
    return 1;
}

static int lower_hex32(const char *s)
{
    unsigned i;
    if (!s || strlen(s)!=32) return 0;
    for (i=0;i<32;++i)
        if (!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f'))) return 0;
    return 1;
}

static int parse_party(const char *line)
{
    char kind[6],port[6],session[33],uid[17],group[33],team[2],expiry[24];
    const char *p=line;
    unsigned long long po, side, ex;
    uint64_t value=0;
    unsigned i;
    size_t slot=party_ticket_count;
    if (slot>=ROSTER_MAX || !(p=field(p,kind,sizeof(kind))) || strcmp(kind,"party") ||
        !(p=field(p,port,sizeof(port))) || !(p=field(p,session,sizeof(session))) ||
        !(p=field(p,uid,sizeof(uid))) || !(p=field(p,group,sizeof(group))) ||
        !(p=field(p,team,sizeof(team))) || !(p=field(p,expiry,sizeof(expiry)))) return 0;
    while (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n') ++p;
    if (*p || !decimal(port,65535,&po) || po<1024 ||
        !decimal(team,2,&side) || side<1 ||
        !decimal(expiry,LLONG_MAX,&ex) || !ex || !lower_hex32(session) ||
        !lower_hex32(group) || !strcmp(group,"00000000000000000000000000000000") ||
        strlen(uid)!=16) return 0;
    for (i=0;i<16;++i) {
        int digit=hexval(uid[i]);
        if (digit<0) return 0;
        value=(value<<4)|(unsigned)digit;
    }
    if (!value) return 0;
    party_tickets[slot].port=(unsigned)po;
    memcpy(party_tickets[slot].session,session,sizeof(session));
    party_tickets[slot].uid=value;
    party_tickets[slot].expires=(long long)ex;
    memcpy(party_tickets[slot].assignment.group,group,sizeof(group));
    party_tickets[slot].assignment.team=(unsigned)side;
    ++party_ticket_count;
    return 1;
}

static int parse_quick(const char *line)
{
    char kind[6], port[6], session[33], epoch[11], uid[17], expiry[24];
    const char *p=line;
    unsigned long long po, ep, ex;
    uint64_t value=0;
    unsigned i;
    size_t slot=quick_ticket_count;
    if (slot>=ROSTER_MAX || !(p=field(p,kind,sizeof(kind))) || strcmp(kind,"quick") ||
        !(p=field(p,port,sizeof(port))) || !(p=field(p,session,sizeof(session))) ||
        !(p=field(p,epoch,sizeof(epoch))) || !(p=field(p,uid,sizeof(uid))) ||
        !(p=field(p,expiry,sizeof(expiry)))) return 0;
    while (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n') ++p;
    if (*p || !decimal(port,65535,&po) || po<1024 ||
        !decimal(epoch,INT_MAX,&ep) || !ep ||
        !decimal(expiry,LLONG_MAX,&ex) || !ex ||
        strlen(session)!=32 || strlen(uid)!=16) return 0;
    for (i=0;i<32;++i)
        if (!((session[i]>='0' && session[i]<='9') ||
              (session[i]>='a' && session[i]<='f'))) return 0;
    for (i=0;i<16;++i) {
        int v=hexval(uid[i]);
        if (v<0) return 0;
        value=(value<<4)|(unsigned)v;
    }
    if (!value) return 0;
    quick_tickets[slot].port=(unsigned)po;
    quick_tickets[slot].epoch=(unsigned)ep;
    memcpy(quick_tickets[slot].session,session,sizeof(session));
    quick_tickets[slot].uid=value;
    quick_tickets[slot].expires=(long long)ex;
    ++quick_ticket_count;
    return 1;
}

static int parse_warm(const char *line)
{
    char kind[5],port[6],preparation[33],lease[33],uid[17],expiry[24];
    const char *p=line;
    unsigned long long po,ex;
    uint64_t value=0;
    unsigned i;
    size_t slot=warm_ticket_count;
    if (slot>=ROSTER_MAX || !(p=field(p,kind,sizeof(kind))) || strcmp(kind,"warm") ||
        !(p=field(p,port,sizeof(port))) || !(p=field(p,preparation,sizeof(preparation))) ||
        !(p=field(p,lease,sizeof(lease))) || !(p=field(p,uid,sizeof(uid))) ||
        !(p=field(p,expiry,sizeof(expiry)))) return 0;
    while (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n') ++p;
    if (*p || !decimal(port,65535,&po) || po<1024 ||
        !decimal(expiry,LLONG_MAX,&ex) || !ex ||
        !lower_hex32(preparation) || !lower_hex32(lease) || strlen(uid)!=16 ||
        !strcmp(preparation,"00000000000000000000000000000000") ||
        !strcmp(lease,"00000000000000000000000000000000")) return 0;
    for (i=0;i<16;++i) {
        int digit=hexval(uid[i]);
        if (digit<0) return 0;
        value=(value<<4)|(unsigned)digit;
    }
    if (!value) return 0;
    warm_tickets[slot].port=(unsigned)po;
    memcpy(warm_tickets[slot].preparation,preparation,33);
    memcpy(warm_tickets[slot].lease,lease,33);
    warm_tickets[slot].uid=value;
    warm_tickets[slot].expires=(long long)ex;
    ++warm_ticket_count;
    return 1;
}

static const char *configured_path(void)
{
#ifdef OL_SERVER
    /* Empty: the lobby writes the roster beside securitykey.bin, so derive it
     * from securityKeyPath and existing launchers need no new setting. */
    static char derived[1024];
    cvar_t *key;
    const char *slash;
    size_t dir;
    if (admission_path && admission_path->string[0]) return admission_path->string;
    key = Cvar_FindVar("securityKeyPath");
    if (!key || !key->string[0]) return "";
    slash = strrchr(key->string, '/');
    if (!slash || (strrchr(key->string, '\\') && strrchr(key->string, '\\') > slash))
        slash = strrchr(key->string, '\\');
    dir = slash ? (size_t)(slash - key->string) + 1 : 0;
    if (dir + sizeof("game_admissions.txt") > sizeof(derived)) return "";
    memcpy(derived, key->string, dir);
    memcpy(derived + dir, "game_admissions.txt", sizeof("game_admissions.txt"));
    return derived;
#else
    return getenv("CODOL_ADMISSION_PATH") ? getenv("CODOL_ADMISSION_PATH") : "";
#endif
}

/* Nanoseconds of the modification time where the platform keeps them. */
static long long mtime_ns(const admission_stat_t *st)
{
#if defined(__linux__) && defined(_STATBUF_ST_NSEC)
    return (long long)st->st_mtim.tv_nsec;
#else
    (void)st; return 0;
#endif
}

/* Re-read the roster when its path, inode, size or modification time changed.
 * The lobby replaces the file (temp + os.replace), so every write is a new
 * inode; seconds and size alone missed a second write in the same second with
 * the same size -- which the lobby does on every createSession (the uid-latch
 * refresh republishes the old key, then the new key replaces it), leaving the
 * new key off the roster until some later write. A missing or unreadable file
 * is an empty roster. */
static void reload(void)
{
    const char *path = configured_path();
    admission_stat_t st;
    FILE *file;
    char *text, *line, *next;
    size_t n;
    if (!path || !*path || strlen(path) >= sizeof(roster_path)) {
        quick_ticket_count=0; party_ticket_count=0; warm_ticket_count=0;
        erase(roster, sizeof(roster)); roster_count = 0;
        erase(members, sizeof(members)); member_count = 0; roster_path[0] = 0; return;
    }
    if (admission_stat(path, &st) != 0) {
        quick_ticket_count=0; party_ticket_count=0; warm_ticket_count=0;
        erase(roster, sizeof(roster)); roster_count = 0;
        erase(members, sizeof(members)); member_count = 0;
        roster_mtime = roster_size = roster_mtime_ns = roster_inode = -1;
        strcpy(roster_path, path); return;
    }
    /* Windows CRT stat can expose only second-resolution modification time
     * and no useful replacement identity. The lobby can publish two same-size
     * rosters in that interval. Admission is consulted at handshake/connect,
     * so re-reading this bounded file is preferable to retaining an old key. */
#if !defined(_WIN32) && !defined(TEST_COARSE_STAT)
    if (!strcmp(roster_path, path) && roster_mtime == (long long)st.st_mtime &&
        roster_mtime_ns == mtime_ns(&st) && roster_inode == (long long)st.st_ino &&
        roster_size == (long long)st.st_size) return;
#endif
    strcpy(roster_path, path);
    roster_mtime = (long long)st.st_mtime; roster_size = (long long)st.st_size;
    roster_mtime_ns = mtime_ns(&st); roster_inode = (long long)st.st_ino;
    erase(roster, sizeof(roster)); roster_count = 0;
    erase(members, sizeof(members)); member_count = 0;
    erase(quick_tickets,sizeof(quick_tickets)); quick_ticket_count=0;
    erase(party_tickets,sizeof(party_tickets)); party_ticket_count=0;
    erase(warm_tickets,sizeof(warm_tickets)); warm_ticket_count=0;
    if (st.st_size <= 0 || st.st_size > ROSTER_BYTES || !(file = fopen(path, "rb"))) return;
    text = malloc((size_t)st.st_size + 1);
    if (!text) { fclose(file); return; }
    n = fread(text, 1, (size_t)st.st_size, file);
    fclose(file);
    text[n] = 0;
    for (line = text; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        if (roster_count < ROSTER_MAX && AdmissionParseLine(line, &roster[roster_count]))
            ++roster_count;
        else if (member_count < ROSTER_MAX &&
                 parse_member(line, &members[member_count].host_uid, &members[member_count].seat))
            ++member_count;
        else if (!parse_quick(line) && !parse_party(line)) parse_warm(line);
    }
    erase(text, n);
    free(text);
}

int AdmissionWarmTicket(unsigned port, const char *preparation,
                               const char *lease, uint64_t uid)
{
    size_t i;
    const char *path;
    long long now=(long long)time(NULL);
    if (!uid || !lower_hex32(preparation) || !lower_hex32(lease)) return 0;
#ifdef OL_SERVER
    path=warm_admission_path ? warm_admission_path->string : "";
#else
    path=getenv("CODOL_WARM_ADMISSION_PATH");
#endif
    if (path && *path) {
        admission_stat_t st;
        FILE *file;
        char *text, *line, *next;
        size_t n;
        /* A private assignment file must never replace the central DTLS/UID
         * roster. Guests are published there after Ready by the normal lobby
         * flow. Read this bounded extra authority only at client admission. */
        erase(warm_tickets,sizeof(warm_tickets)); warm_ticket_count=0;
        if (strlen(path)>=sizeof(roster_path) || admission_stat(path,&st) ||
            st.st_size<=0 || st.st_size>ROSTER_BYTES || !(file=fopen(path,"rb"))) return 0;
        text=malloc((size_t)st.st_size+1);
        if (!text) { fclose(file); return 0; }
        n=fread(text,1,(size_t)st.st_size,file);
        if (ferror(file) || n!=(size_t)st.st_size) {
            fclose(file); erase(text,n); free(text); return 0;
        }
        fclose(file); text[n]=0;
        for (line=text; line && *line; line=next) {
            next=strchr(line,'\n'); if (next) *next++=0;
            parse_warm(line);
        }
        erase(text,n); free(text);
    } else reload();
    for (i=0;i<warm_ticket_count;++i)
        if (warm_tickets[i].port==port && warm_tickets[i].uid==uid &&
            !strcmp(warm_tickets[i].preparation,preparation) &&
            !strcmp(warm_tickets[i].lease,lease) &&
            warm_tickets[i].expires>now && warm_tickets[i].expires<=now+120) return 1;
    return 0;
}

static int live(const admission_entry_t *e)
{ return e->expires > (long long)time(NULL); }

int AdmissionQuickTicket(unsigned port, const char *session, unsigned epoch,
                               uint64_t uid)
{
    size_t i;
    long long now=(long long)time(NULL);
    if (!session || !uid || !epoch) return 0;
    reload();
    for (i=0;i<quick_ticket_count;++i)
        if (quick_tickets[i].port==port && quick_tickets[i].epoch==epoch &&
            quick_tickets[i].uid==uid && !strcmp(quick_tickets[i].session,session) &&
            quick_tickets[i].expires>now && quick_tickets[i].expires<=now+120) return 1;
    return 0;
}

int AdmissionQuickParty(unsigned port, const char *session, uint64_t uid,
                               party_assignment_t *out)
{
    size_t i,j,found=ROSTER_MAX;
    long long now=(long long)time(NULL);
    if (out) memset(out,0,sizeof(*out));
    if (!out || !uid || !lower_hex32(session)) return 0;
    reload();
    for (i=0;i<party_ticket_count;++i) {
        if (party_tickets[i].port!=port || strcmp(party_tickets[i].session,session) ||
            party_tickets[i].expires<=now || party_tickets[i].expires>now+120) continue;
        if (party_tickets[i].uid!=uid) continue;
        if (found!=ROSTER_MAX &&
            (party_tickets[i].assignment.team!=party_tickets[found].assignment.team ||
             strcmp(party_tickets[i].assignment.group,party_tickets[found].assignment.group))) return 0;
        found=i;
    }
    if (found==ROSTER_MAX) return 0;
    /* One group cannot straddle the fixed two team bins. Conflicting duplicate
     * UID rows or group sides fail closed instead of using file order. */
    for (j=0;j<party_ticket_count;++j)
        if (party_tickets[j].port==port && !strcmp(party_tickets[j].session,session) &&
            party_tickets[j].expires>now && party_tickets[j].expires<=now+120 &&
            !strcmp(party_tickets[j].assignment.group,party_tickets[found].assignment.group) &&
            party_tickets[j].assignment.team!=party_tickets[found].assignment.team) return 0;
    *out=party_tickets[found].assignment;
    return 1;
}

size_t AdmissionCandidates(const uint8_t ip[4], uint8_t keys[][16], size_t max)
{
    size_t i, count = 0;
    int pass;
    if (!keys || !max) return 0;
    reload();
    for (pass = 0; pass < 2; ++pass)
        for (i = 0; i < roster_count && count < max; ++i) {
            int same = ip && !memcmp(roster[i].ip, ip, 4);
            if (!live(&roster[i]) || same != !pass) continue;
            memcpy(keys[count++], roster[i].key, 16);
        }
    return count;
}

int AdmissionIdentify(const uint8_t ip[4], const uint8_t shared_x[28],
                            const uint8_t data_key[24], admission_entry_t *out)
{
    uint8_t derived[24];
    size_t i;
    int pass;
    if (!shared_x || !data_key || !out) return 0;
    reload();
    for (pass = 0; pass < 2; ++pass)
        for (i = 0; i < roster_count; ++i) {
            int same = ip && !memcmp(roster[i].ip, ip, 4);
            if (!live(&roster[i]) || same != !pass) continue;
            if (DeriveKey(derived, shared_x, roster[i].key) && !memcmp(derived, data_key, 24)) {
                *out = roster[i];
                erase(derived, sizeof(derived));
                return 1;
            }
        }
    erase(derived, sizeof(derived));
    return 0;
}

int AdmissionMember(const uint8_t ip[4], uint64_t host_uid, uint64_t uid,
                          admission_entry_t *out)
{
    size_t i;
    if (!ip || !host_uid || !uid || !out) return 0;
    reload();
    for (i = 0; i < member_count; ++i)
        if (members[i].host_uid == host_uid && live(&members[i].seat) &&
            members[i].seat.uid == uid && !memcmp(members[i].seat.ip, ip, 4)) {
            *out = members[i].seat;
            return 1;
        }
    /* Preserve the existing path for guests with their own createSession key. */
    for (i = 0; i < roster_count; ++i)
        if (live(&roster[i]) && roster[i].uid == uid && !memcmp(roster[i].ip, ip, 4)) {
            *out = roster[i];
            return 1;
        }
    return 0;
}

#ifdef OL_SERVER
void AdmissionInit(void)
{
    admission_path = Cvar_RegisterString("admissionPath", "", 0,
        "Lobby admission roster; empty = game_admissions.txt beside securityKeyPath");
    admission_required = Cvar_RegisterBool("admissionRequired", qfalse, 0,
        "Refuse clients the lobby has not admitted (needs admissionPath)");
    warm_admission_path = Cvar_RegisterString("warmAdmissionPath", "", 0,
        "Private prepared-process assignment roster; ordinary lobby credentials remain separate");
}
int AdmissionRequired(void)
{ return admission_required && admission_required->boolean; }
#else
void AdmissionInit(void) {}
int AdmissionRequired(void) { return getenv("CODOL_ADMISSION_REQUIRED") != NULL; }
#endif

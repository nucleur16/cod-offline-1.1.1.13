/* Readiness is the requested running match, not merely an answering process. */
#ifndef K33_P2P_STATUS_H
#define K33_P2P_STATUS_H
#include <string.h>

static int P2PStatusTokenEquals(const char *line, const char *key, const char *value)
{
    size_t keylen, valuelen;
    int found=0;
    if (!line || !key || !value) return 0;
    keylen=strlen(key);valuelen=strlen(value);
    while (*line) {
        const char *end;
        while (*line==' ' || *line=='\t' || *line=='\r' || *line=='\n') ++line;
        if (!*line) break;
        end=line;
        while (*end && *end!=' ' && *end!='\t' && *end!='\r' && *end!='\n') ++end;
        if ((size_t)(end-line)>keylen && !memcmp(line,key,keylen) && line[keylen]=='=') {
            /* Duplicate identity keys are ambiguous and cannot prove readiness. */
            if (found || (size_t)(end-line)!=keylen+1+valuelen ||
                memcmp(line+keylen+1,value,valuelen)) return 0;
            found=1;
        }
        line=end;
    }
    return found;
}

static int P2PStatusMatches(const char *line, const char *map, const char *session,
                           const char *gametype, const char *category)
{
    if (!map || !*map || !gametype || !*gametype || !category || !*category) return 0;
    return P2PStatusTokenEquals(line,"running","1") &&
           P2PStatusTokenEquals(line,"parked","0") &&
           P2PStatusTokenEquals(line,"map",map) &&
           P2PStatusTokenEquals(line,"session_map",session && *session ? session : map) &&
           P2PStatusTokenEquals(line,"gametype",gametype) &&
           P2PStatusTokenEquals(line,"type",category);
}

/* The value of a key that is on the line exactly once, non-empty and shorter than outSize; otherwise 0. */
static int P2PStatusToken(const char *line, const char *key, char *out, size_t outSize)
{
    size_t keylen;
    int found=0;
    if (!line || !key || !out || !outSize) return 0;
    out[0]=0;
    keylen=strlen(key);
    while (*line) {
        const char *end;
        while (*line==' ' || *line=='\t' || *line=='\r' || *line=='\n') ++line;
        if (!*line) break;
        end=line;
        while (*end && *end!=' ' && *end!='\t' && *end!='\r' && *end!='\n') ++end;
        if ((size_t)(end-line)>keylen && !memcmp(line,key,keylen) && line[keylen]=='=') {
            size_t n=(size_t)(end-line)-keylen-1;
            if (found || !n || n>=outSize) { out[0]=0; return 0; }
            memcpy(out,line+keylen+1,n); out[n]=0;
            found=1;
        }
        line=end;
    }
    return found;
}

/* What a sh1x_status reply from our own server says, for the host watchdogs (p2p.c, p2p_inproc.c):
 *   P2P_STATUS_BAD    no usable status line (no reply, a malformed or ambiguous line): not an answer.
 *   P2P_STATUS_IDLE   a well-formed line with no level running (running=0 / parked=1: a map load under way, or one
 *                     that ended in ERR_DROP, map=none): the process answers, but nobody can play on it.
 *   P2P_STATUS_LIVE   a well-formed line with a level running, but not the requested one: the match moved on
 *                     (a console/rcon `map`, a gametype change). The session follows it.
 *   P2P_STATUS_EXACT  the requested match is running (P2PStatusMatches). */
enum { P2P_STATUS_BAD, P2P_STATUS_IDLE, P2P_STATUS_LIVE, P2P_STATUS_EXACT };
static int P2PStatusClassify(const char *line, const char *map, const char *session,
                             const char *gametype, const char *category)
{
    char v[64];
    int running, parked;
    if (!P2PStatusToken(line,"map",v,sizeof(v)) || !P2PStatusToken(line,"session_map",v,sizeof(v)) ||
        !P2PStatusToken(line,"gametype",v,sizeof(v)) || !P2PStatusToken(line,"type",v,sizeof(v)))
        return P2P_STATUS_BAD;
    running=P2PStatusTokenEquals(line,"running","1") ? 1 : P2PStatusTokenEquals(line,"running","0") ? 0 : -1;
    parked=P2PStatusTokenEquals(line,"parked","0") ? 0 : P2PStatusTokenEquals(line,"parked","1") ? 1 : -1;
    if (running<0 || parked<0) return P2P_STATUS_BAD;
    if (P2PStatusMatches(line,map,session,gametype,category)) return P2P_STATUS_EXACT;
    if (!running || parked) return P2P_STATUS_IDLE;
    /* map=none is what the server reports with no level; a running line never names it */
    P2PStatusToken(line,"map",v,sizeof(v));
    return strcmp(v,"none") ? P2P_STATUS_LIVE : P2P_STATUS_IDLE;
}

/* "map session_map gametype type" of a status line, to notice when the running match changes. */
static void P2PStatusIdentity(const char *line, char *out, size_t outSize)
{
    static const char *const keys[4]={"map","session_map","gametype","type"};
    char v[64];
    size_t used=0;
    int i;
    if (!out || !outSize) return;
    out[0]=0;
    for (i=0; i<4; ++i) {
        size_t n;
        if (!P2PStatusToken(line,keys[i],v,sizeof(v))) { v[0]='?'; v[1]=0; }
        n=strlen(v);
        if (used+n+2>outSize) break;
        if (used) out[used++]=' ';
        memcpy(out+used,v,n+1);
        used+=n;
    }
}
#endif

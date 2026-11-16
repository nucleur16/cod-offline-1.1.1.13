/*
 * dw_session.cpp - who a connection is and what it is about to play.
 * Port of demonware/dw_session.py (the per-connection session descriptor, the security-key channel,
 * the match claim) and demonware/dw_game_admission.py (the game_admissions.txt roster the native
 * match server reads next to securitykey.bin).
 *
 * Not ported: the session-descriptor pickle (session_desc.pickle). Nothing survives a restart, so a
 * held key is never one from a previous process; the SUSPECT mark is kept because a new lobby
 * handshake still poisons the shared last-seen descriptor exactly as in the Python lobby.
 * dw_admission.py (launch tokens) is not part of the offline product: every connection is admitted.
 */
#include <string.h>
#include <algorithm>
#include "dw.h"
#include "dw_state.h"
#include "dw_rooms.h"

namespace dw {

const char *const kSenderName = "EDITz";

Bytes SessionId()
{
    Bytes id(8, '\0');
    id[0] = (char)70;
    return id;
}

SessionState &Session(Server &s)
{
    if (!s.st->session) s.st->session = new SessionState();
    return *s.st->session;
}

/* ---- the session descriptor ---- */

uint64_t DescUidOf(const Vals *desc)
{
    if (!desc) return 0;
    for (size_t i = 0; i < desc->size(); i++)
        if ((*desc)[i].tag == T_UINT64 && (*desc)[i].u) return (*desc)[i].u;
    return 0;
}

/* ownOnly is the IDENTITY rule: a connection without a descriptor of its own may use the shared
 * last-seen one only when that descriptor's u64 is this connection's latched uid. */
const Vals *Desc(Server &s, Conn *c, bool ownOnly)
{
    SessionState &st = Session(s);
    const Vals *shared = st.hasDesc ? &st.desc : NULL;
    if (c) {
        if (c->hasDesc) return &c->desc;
        if (ownOnly) {
            if (!shared || shared->empty()) return NULL;
            return (c->uid && DescUidOf(shared) == c->uid) ? shared : NULL;
        }
    }
    return ownOnly ? NULL : shared;
}

void SetDesc(Server &s, Conn *c, const Vals &vals)
{
    if (c) {
        c->desc = vals;
        c->hasDesc = true;
    }
    SessionState &st = Session(s);
    st.desc = vals; /* the last-resort answer for paths with no connection in hand */
    st.hasDesc = true;
}

uint64_t DescPlayerUid(Server &s, Conn *c) { return DescUidOf(Desc(s, c, c != NULL)); }

/* The descriptor's name, unless the local player renamed that profile with the console's `name` (Host::DisplayName). */
std::string LivePlayerName(Server &s, Conn *c)
{
    std::string name = DescPlayerName(Desc(s, c, c != NULL));
    std::string shown = s.host ? s.host->DisplayName(name) : std::string();
    return shown.empty() ? name : shown;
}

uint64_t ConnUid(Server &s, Conn &c) { return c.uid ? c.uid : DescPlayerUid(s, &c); }

std::string ConnName(Server &s, Conn &c)
{
    if (!c.name.empty()) return c.name;
    std::string live = LivePlayerName(s, &c);
    return live.empty() ? std::string(kSenderName) : live;
}

/* ---- the security key channel ---- */

static std::string SeckeyPath(Server &s) { return PathJoin(s.cfg.state, "securitykey.bin"); }

/* Atomic temp + replace: the match server re-derives its DTLS key from this file on a MAC failure. */
static bool SeckeyWriteFile(Server &s, const Bytes &key)
{
    if (s.cfg.state.empty()) return false;
    MakeDirs(s.cfg.state);
    if (!WriteFileAtomic(SeckeyPath(s), key)) return false;
    Session(s).lastKey = key;
    return true;
}

/* A 16-byte descriptor key arrived live (21/1 createSession, 21/12 updateSession): write it NOW. */
std::string SeckeyObserve(Server &s, Conn *c, const Bytes &key)
{
    if (key.size() != 16) return "";
    SessionState &st = Session(s);
    if (c) c->seckey = key; /* MC-2: the key belongs to THIS client's session */
    st.suspect = false;
    st.observedGen = st.connGen;
    bool ok = SeckeyWriteFile(s, key);
    return " | SECKEY live-observed -> securitykey.bin " + KeyFp(key) + (ok ? "" : " (WRITE FAILED)") +
           (c ? GameAdmissionPublish(s, *c, key) : std::string());
}

/* A new lobby handshake == potentially a new client process: the shared held key is SUSPECT until
 * the next live observation. A connection that observed its own key is unaffected. */
void OnLobbyHandshake(Server &s, Conn &)
{
    SessionState &st = Session(s);
    st.connGen++;
    st.suspect = st.hasDesc;
}

/* SK-2, the READY-press write: this connection's own observed key, else the held one unless SUSPECT. */
std::string SeckeyStartgameWrite(Server &s, Conn *c, const Bytes &key)
{
    SessionState &st = Session(s);
    if (c && c->seckey.size() == 16) {
        bool ok = SeckeyWriteFile(s, c->seckey);
        return " | SECKEY startgame (this connection's own) -> securitykey.bin " + KeyFp(c->seckey) + (ok ? "" : " (WRITE FAILED)") +
               GameAdmissionPublish(s, *c, c->seckey);
    }
    if (st.suspect)
        return " | !!! SECKEY SUSPECT -- securitykey.bin NOT written: held key " + KeyFp(key) +
               Fmt(" predates lobby connection %d; the client must create a session", st.connGen);
    bool ok = SeckeyWriteFile(s, key);
    return " | SECKEY startgame -> securitykey.bin " + KeyFp(key) + (ok ? "" : " (WRITE FAILED)");
}

/* ---- the match claim (MC-3): one host runs one match ---- */

static const double kMatchHoldSeconds = 900.0;

bool MatchClaim(Server &s, Conn &c, const Selection &sel, std::string &note)
{
    SessionState &st = Session(s);
    std::string key = sel.sessionMap + "\x1f" + sel.gametype + "\x1f" + sel.type;
    double now = s.host->Mono();
    bool live = st.claimed && (now - st.claimWhen) < kMatchHoldSeconds;
    uint64_t requester = ConnUid(s, c);
    /* The claim protects whoever is PLAYING from someone else's Ready, never from themselves. */
    bool owner = st.claimed && (st.claimBy == c.idx || (st.claimUid && st.claimUid == requester));
    if (live && st.claimKey != key && !owner) {
        note = Fmt(" | match control HELD OFF: the host is already running %s for uid %llu (%.0fs ago); starting %s would drop that "
                   "player",
                   st.claimMap.c_str(), (unsigned long long)st.claimUid, now - st.claimWhen, sel.sessionMap.c_str());
        return false;
    }
    if (live && st.claimKey == key) {
        note = Fmt(" | match control: %s is already the running match (started %.0fs ago) -- this press joins it", sel.sessionMap.c_str(),
                   now - st.claimWhen);
        return false;
    }
    note = (live && owner) ? " | match control re-pointed by its own player: " + st.claimMap + " -> " + sel.sessionMap : std::string();
    st.claimed = true;
    st.claimKey = key;
    st.claimWhen = now;
    st.claimBy = c.idx;
    st.claimUid = requester;
    st.claimMap = sel.sessionMap;
    SetMirrorOwner(s, st.claimUid); /* MC-4: while this match runs only its player re-points the shared mpdata_s */
    return true;
}

/* Drop the claim when the connection that started it goes away. */
void MatchRelease(Server &s, Conn &c)
{
    if (!s.st || !s.st->session) return;
    SessionState &st = *s.st->session;
    if (st.claimed && st.claimBy == c.idx) {
        st.claimed = false;
        st.claimKey.clear();
        st.claimMap.clear();
        st.claimWhen = 0;
        st.claimBy = 0;
        st.claimUid = 0;
        SetMirrorOwner(s, 0);
    }
}

/* ---- game_admissions.txt (dw_game_admission.py) ----
 *   <ipv4> <key hex> <uid hex16 | 0> <expires unix> <gamertag | ->
 * One line per key a seat holds. A seat keeps its newest 8 keys: C1 dials with the first key it
 * registered for the (constant) session id until a game connection ends, so the newest key alone
 * cannot authenticate a second createSession's association. */

enum { ADMISSION_TTL = 21600, ADMISSION_SEAT_KEYS = 8 };

static bool TagOk(const std::string &tag)
{
    if (tag.empty() || tag.size() > 32) return false;
    for (size_t i = 0; i < tag.size(); i++) {
        unsigned char ch = (unsigned char)tag[i];
        if (ch < 0x21 || ch > 0x7E || ch == '"' || ch == '%' || ch == ';' || ch == '\\') return false;
    }
    return true;
}

static bool RosterNewerFirst(const AdmissionEntry &a, const AdmissionEntry &b) { return a.last > b.last; }
static bool RosterOlderFirst(const AdmissionEntry *a, const AdmissionEntry *b) { return a->first < b->first; }

static bool RosterWrite(Server &s, int64_t now)
{
    SessionState &st = Session(s);
    std::vector<AdmissionEntry> keep;
    for (size_t i = 0; i < st.roster.size(); i++)
        if (now - st.roster[i].last <= ADMISSION_TTL) keep.push_back(st.roster[i]);
    st.roster.swap(keep);
    std::vector<AdmissionEntry> sorted = st.roster;
    std::stable_sort(sorted.begin(), sorted.end(), RosterNewerFirst);
    std::string text = "# lobby -> game server admission roster (demonware/dw_game_admission.py)\n"
                       "# <ipv4> <key> <uid hex16|0> <expires unix> <gamertag|->\n";
    for (size_t i = 0; i < sorted.size(); i++) {
        const AdmissionEntry &e = sorted[i];
        text += e.ip + " " + e.key + " " + (e.uid ? Fmt("%016llx", (unsigned long long)e.uid) : std::string("0")) +
                Fmt(" %lld ", (long long)(e.last + ADMISSION_TTL)) + (TagOk(e.gamertag) ? e.gamertag : std::string("-")) + "\n";
    }
    if (s.cfg.state.empty()) return false;
    MakeDirs(s.cfg.state);
    return WriteFileAtomic(PathJoin(s.cfg.state, "game_admissions.txt"), text);
}

std::string GameAdmissionPublish(Server &s, Conn &c, const Bytes &key)
{
    if (key.size() != 16) return "";
    SessionState &st = Session(s);
    int64_t now = s.Now();
    std::string tag = LivePlayerName(s, &c);
    uint64_t uid = c.uid ? c.uid : DescPlayerUid(s, &c);
    std::string khex = Hex(key.data(), key.size());

    /* The seat's other keys, oldest first: drop all but the newest 7 and give those this uid / tag. */
    std::vector<AdmissionEntry *> seat;
    int64_t first = now;
    for (size_t i = 0; i < st.roster.size(); i++) {
        AdmissionEntry &e = st.roster[i];
        if (e.key == khex) first = e.first;
        else if ((!tag.empty() && e.gamertag == tag) || (uid && e.uid == uid)) seat.push_back(&e);
    }
    std::stable_sort(seat.begin(), seat.end(), RosterOlderFirst);
    size_t drop = seat.size() > (size_t)(ADMISSION_SEAT_KEYS - 1) ? seat.size() - (size_t)(ADMISSION_SEAT_KEYS - 1) : 0;
    std::set<std::string> dropped;
    for (size_t i = 0; i < seat.size(); i++) {
        if (i < drop) { dropped.insert(seat[i]->key); continue; }
        if (uid) seat[i]->uid = uid;
        if (!tag.empty()) seat[i]->gamertag = tag;
    }
    std::vector<AdmissionEntry> next;
    bool placed = false;
    AdmissionEntry mine;
    mine.ip = "127.0.0.1"; /* every seat of the offline lobby is on loopback */
    mine.key = khex;
    mine.uid = uid;
    mine.gamertag = tag;
    mine.last = now;
    mine.first = first;
    for (size_t i = 0; i < st.roster.size(); i++) {
        if (dropped.count(st.roster[i].key)) continue;
        if (st.roster[i].key == khex) { next.push_back(mine); placed = true; }
        else next.push_back(st.roster[i]);
    }
    if (!placed) next.push_back(mine);
    st.roster.swap(next);
    bool ok = RosterWrite(s, now);
    return Fmt(" | GAME ADMISSION %s uid=%llu ip=%s%s", tag.empty() ? "?" : tag.c_str(), (unsigned long long)uid, mine.ip.c_str(),
               ok ? "" : " (WRITE FAILED)");
}

} // namespace dw

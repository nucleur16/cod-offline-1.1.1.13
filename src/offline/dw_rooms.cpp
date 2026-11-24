/*
 * dw_rooms.cpp - ROOM (service 11), the fan-out, and everything the READY press drives.
 * Port of demonware/dw_rooms.py with the Python lobby's default switches (CODOL_ROOM_FANOUT=1,
 * CODOL_JOIN_FIX=0, CODOL_SETMAP_JOINRESP=1, CODOL_INSTANCES=1).
 *
 *   13 CREATE   -> the room record: u64 roomId, u32 members, u32 0, u64 ROOM_ID (admin), bool, bool, u32 0
 *   11 JOIN     -> the same record, after a SET_MAP push (the JOIN is what clears amHost)
 *    2 LEAVE
 *   15 MEMBERS  -> u64 uid, string name (base64), u32 flags  per member (record class 0x58, C1 0x00A41B40)
 *    4 MESSAGES -> relayed back as push 7 (the echo is how the sender learns the state is
 *                  authoritative) and fanned out to the other members; a READY press is followed by
 *                  SET_MAP (typeId 5 + 15; 15 alone when the owner keeps its room, ReadyPress) and START_GAME
 *                  (typeId 3).
 *
 * Not ported: the join fix (CODOL_JOIN_FIX=0 is the serving default: a JoinResponse with members is
 * fatal on C1 and one without leaves the joiner in a room nobody starts), and the in-match chat
 * fan-out (it needs a match server's player list; with none answering, the Python lobby delivers
 * to nobody).
 */
#include <string.h>
#include "dw.h"
#include "dw_state.h"
#include "dw_rooms.h"

namespace dw {

/* The exact blob the client emits when Ready is pressed: one typeId-1 manifold record. */
static const char kReadyPressBlob[16] = { 0x0c, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, (char)0x80, 0x08, 0x00 };

RoomsState &Rooms(Server &s)
{
    if (!s.st->rooms) {
        s.st->rooms = new RoomsState();
        s.st->rooms->handoff = HandoffNew();
    }
    return *s.st->rooms;
}

ConnRoom &ConnState(Server &s, Conn &c) { return Rooms(s).conn[c.idx]; }

void RoomsInit(Server &s)
{
    Rooms(s);
    Session(s);
}

void RoomsFree(Server &s)
{
    if (s.st->rooms) {
        HandoffFree(s.st->rooms->handoff);
        delete s.st->rooms;
        s.st->rooms = NULL;
    }
    delete s.st->session;
    s.st->session = NULL;
}

/* ---- the registry ---- */

Room *RoomFind(Server &s, uint64_t rid)
{
    std::vector<Room> &rooms = Rooms(s).rooms;
    for (size_t i = 0; i < rooms.size(); i++)
        if (rooms[i].rid == rid) return &rooms[i];
    return NULL;
}

static bool RoomHasConn(const Room &r, int idx)
{
    for (size_t i = 0; i < r.members.size(); i++)
        if (r.members[i].second == idx) return true;
    return false;
}

static int MemberIdx(const Room &r, uint64_t uid)
{
    for (size_t i = 0; i < r.members.size(); i++)
        if (r.members[i].first == uid) return r.members[i].second;
    return -1;
}

static void SetMember(Room &r, uint64_t uid, int idx, const std::string &name)
{
    size_t i;
    for (i = 0; i < r.members.size() && r.members[i].first != uid; i++) {}
    if (i < r.members.size()) r.members[i].second = idx;
    else r.members.push_back(std::make_pair(uid, idx));
    for (i = 0; i < r.names.size() && r.names[i].first != uid; i++) {}
    if (i < r.names.size()) r.names[i].second = name;
    else r.names.push_back(std::make_pair(uid, name));
}

/* Take THIS connection out of room `rid`. Only the connection's own entry goes: matching on the uid
 * too would let a player's stale connection remove the one they reconnected on. */
static std::string RoomLeave(Server &s, Conn &c, uint64_t rid, const char *why)
{
    RoomsState &st = Rooms(s);
    ConnRoom &cr = ConnState(s, c);
    Room *r = RoomFind(s, rid);
    /* The owner leaves its room: nothing stays armed for it and a launched host is stopped. */
    if (!s.cfg.differential && r && r->hasHost && MemberIdx(*r, r->host) == c.idx) {
        s.host->LocalPlayLeave(r->host);
        cr.armed.clear();
    }
    if (cr.inRoom && cr.room == rid) {
        cr.inRoom = false;
        c.room = 0;
    }
    if (!r) return "not in registry";
    std::vector<std::pair<uint64_t, int> > keep;
    for (size_t i = 0; i < r->members.size(); i++)
        if (r->members[i].second != c.idx) keep.push_back(r->members[i]);
    r->members.swap(keep);
    if (r->hasHost && MemberIdx(*r, r->host) < 0) { /* the host is gone: nobody inherits the room or its descriptor */
        r->hasHost = false;
        r->hasDesc = false;
        r->desc.clear();
    }
    size_t left = r->members.size();
    if (!left) st.rooms.erase(st.rooms.begin() + (r - &st.rooms[0]));
    return Fmt("%s: uid=%llu left room %llu, %u remain", why, (unsigned long long)ConnUid(s, c), (unsigned long long)rid, (unsigned)left);
}

/* A connection is in at most ONE room (C1 has one dwRoom): CREATE and JOIN first take it out of
 * every other. */
static void RoomLeaveOthers(Server &s, Conn &c, bool hasKeep, uint64_t keep, const char *why)
{
    std::vector<uint64_t> rids;
    const std::vector<Room> &rooms = Rooms(s).rooms;
    for (size_t i = 0; i < rooms.size(); i++)
        if (!(hasKeep && rooms[i].rid == keep) && RoomHasConn(rooms[i], c.idx)) rids.push_back(rooms[i].rid);
    for (size_t i = 0; i < rids.size(); i++) RoomLeave(s, c, rids[i], why);
}

/* The connection-close hook: leave every room and drop the connection's room-side state. */
void RoomLeaveAll(Server &s, Conn &c)
{
    if (!s.st || !s.st->rooms) return;
    RoomLeaveOthers(s, c, false, 0, "disconnect");
    Rooms(s).conn.erase(c.idx);
}

/* CREATE: a room id no room holds or has held. The first is ROOM_ID; ids are never reissued, so a
 * stale id (a findSessions result, a pending join) can only name a dead room. */
static uint64_t RoomAlloc(Server &s, Conn &c)
{
    RoomsState &st = Rooms(s);
    uint64_t uid = ConnUid(s, c);
    RoomLeaveOthers(s, c, false, 0, "implicit leave");
    uint64_t rid = st.nextId > (uint64_t)ROOM_ID ? st.nextId : (uint64_t)ROOM_ID;
    while (RoomFind(s, rid)) rid++;
    if (st.nextId < rid + 1) st.nextId = rid + 1;
    Room r;
    r.rid = rid;
    r.hasHost = true;
    r.host = uid;
    SetMember(r, uid, c.idx, ConnName(s, c));
    st.rooms.push_back(r);
    ConnRoom &cr = ConnState(s, c);
    cr.inRoom = true;
    cr.room = rid;
    c.room = rid;
    return rid;
}

static unsigned RoomJoin(Server &s, Conn &c, uint64_t rid)
{
    RoomsState &st = Rooms(s);
    uint64_t uid = ConnUid(s, c);
    RoomLeaveOthers(s, c, true, rid, "implicit leave");
    Room *r = RoomFind(s, rid);
    if (!r) {
        Room fresh; /* a room nobody hosts */
        fresh.rid = rid;
        st.rooms.push_back(fresh);
        r = &st.rooms.back();
    }
    SetMember(*r, uid, c.idx, ConnName(s, c));
    ConnRoom &cr = ConnState(s, c);
    cr.inRoom = true;
    cr.room = rid;
    c.room = rid;
    return (unsigned)r->members.size();
}

struct Member {
    uint64_t uid;
    std::string name;
    int idx;
};

static std::vector<Member> RoomMembers(Server &s, uint64_t rid)
{
    std::vector<Member> out;
    Room *r = RoomFind(s, rid);
    if (!r) return out;
    for (size_t i = 0; i < r->members.size(); i++) {
        Member m;
        m.uid = r->members[i].first;
        m.idx = r->members[i].second;
        for (size_t k = 0; k < r->names.size(); k++)
            if (r->names[k].first == m.uid) m.name = r->names[k].second;
        if (m.name.empty()) { /* resolved PER MEMBER, out of that member's own connection */
            std::map<int, Conn *>::iterator it = s.conns.find(m.idx);
            Conn *mc = it == s.conns.end() ? NULL : it->second;
            m.name = mc && !mc->name.empty() ? mc->name : LivePlayerName(s, mc);
            if (m.name.empty()) m.name = kSenderName;
        }
        out.push_back(m);
    }
    return out;
}

static Bytes RoomRecord(uint64_t rid, unsigned members)
{
    return w_u64(rid) + w_u32(members) + w_u32(0) + w_u64(ROOM_ID) + w_bool(false) + w_bool(false) + w_u32(0);
}

/* ---- who is on the match host ---- */

/* A start placed this player on the match host moments ago (match_instances.locate: a live
 * reservation; the status half needs a match server that answers, and none does). */
bool InMatch(Server &s, uint64_t uid)
{
    if (!uid) return false;
    std::map<uint64_t, double> &res = Rooms(s).reserved;
    std::map<uint64_t, double>::iterator it = res.find(uid);
    if (it == res.end()) return false;
    if (s.host->Mono() - it->second > MATCH_RESERVE_SECONDS) {
        res.erase(it);
        return false;
    }
    return true;
}

/* ---- the fan-out ---- */

/* Deliver `msg` to every OTHER live member of the room. -> count delivered. */
static int RoomFanout(Server &s, Conn &c, uint64_t rid, const Bytes &msg, const std::string &note, bool skipInMatch)
{
    int n = 0;
    std::vector<Member> members = RoomMembers(s, rid);
    for (size_t i = 0; i < members.size(); i++) {
        if (members[i].idx == c.idx) continue;
        if (skipInMatch && InMatch(s, members[i].uid)) continue; /* not pulled out of a match by someone else's Ready */
        std::map<int, Conn *>::iterator it = s.conns.find(members[i].idx);
        if (it == s.conns.end()) continue;
        it->second->Queue(msg, note + Fmt(" [fan-out from conn#%d uid=%llu]", c.idx, (unsigned long long)ConnUid(s, c)));
        n++;
    }
    return n;
}

/* Why this batch must not reach other members ("" = it may). Room ids are sequential and JOIN adds
 * anyone, so the sender must be a member, and StartGame (3) / JoinResponse (5) / room info (15)
 * must come from the room's host - else any client could point strangers at a host of its choosing. */
static std::string FanoutRefusal(Server &s, Conn &c, uint64_t rid, const std::set<int> &types)
{
    Room *r = RoomFind(s, rid);
    if (!r || !RoomHasConn(*r, c.idx)) return Fmt("sender is not a member of room %llu", (unsigned long long)rid);
    bool isHost = r->hasHost && (r->host == ConnUid(s, c) || MemberIdx(*r, r->host) == c.idx);
    if ((types.count(3) || types.count(5) || types.count(15)) && !isHost)
        return Fmt("host-only typeIds and uid %llu is not the host", (unsigned long long)ConnUid(s, c));
    return "";
}

/* The join-request gate: a batch made only of typeId-6 join requests is not relayed to a host who
 * is in a match (C1 forwards each one to its game server as a reliable command; a join storm
 * overflows that window and drops the host), and to any other host at most once per
 * Config::joinreqRelayGap seconds (3). A wall-clock decision (as the 60 s match reservation is): two
 * servers replaying the same storm agree only while no batch lands within a clock tick of the gap. */
static std::string JoinreqGate(Server &s, Conn &c, uint64_t rid, const std::set<int> &types)
{
    if (!types.count(6) || types.size() != 1) return "";
    Room *r = RoomFind(s, rid);
    if (!r || !r->hasHost || r->host == ConnUid(s, c)) return "";
    if (InMatch(s, r->host)) return Fmt("join request not relayed: host uid=%llu is in a match", (unsigned long long)r->host);
    double now = s.host->Mono();
    std::map<uint64_t, double> &last = ConnState(s, c).joinreqRelayed;
    std::map<uint64_t, double>::iterator it = last.find(rid);
    double gap = s.cfg.joinreqRelayGap; /* the Python lobby's CODOL_JOINREQ_RELAY_MINGAP, default 3 */
    if (gap > 0 && it != last.end() && now - it->second < gap) return Fmt("join request not relayed: throttled, last relayed %.1f s ago", now - it->second);
    last[rid] = now;
    return "";
}

/* ---- the selection ---- */

/* The selection as the embedded host is told it (p2p_match_options.h): Survival runs on the
 * unchanged MP level with gametype survival; the room's MP mode names are the native ones. */
static void HostSelection(const Selection &sel, std::string &session, std::string &gametype, std::string &type)
{
    type = sel.category;
    session = sel.sessionMap.empty() ? sel.map : sel.sessionMap;
    gametype = sel.gametype;
    if (type == "survival") {
        gametype = "survival";
        session = sel.map;
    } else if (type == "mp") {
        if (gametype == "tdm") gametype = "war";
        else if (gametype == "ffa") gametype = "dm";
        else if (gametype == "hq") gametype = "koth";
    }
}

static bool Ident(const std::string &v, size_t max)
{
    if (v.empty() || v.size() > max || v[0] < 'a' || v[0] > 'z') return false;
    for (size_t i = 0; i < v.size(); i++)
        if (!((v[i] >= 'a' && v[i] <= 'z') || (v[i] >= '0' && v[i] <= '9') || v[i] == '_')) return false;
    return true;
}

/* The player who owns the room this connection is in; the connection's own uid when it is in none. */
static uint64_t RoomOwner(Server &s, Conn &c)
{
    ConnRoom &cr = ConnState(s, c);
    if (cr.inRoom) {
        Room *r = RoomFind(s, cr.room);
        if (r && r->hasHost) return r->host;
    }
    return ConnUid(s, c);
}

/* match_instances.localhost_selection (Python 2089907, 58db75a): the room owner's CURRENT room
 * context - what the room UI told Kernel33 the room shows (p2p_room) - beats the lobby's own
 * persisted room state, which may still name another room. Only the owner's own context counts
 * (another player's is not a global pin), and only a coherent one: a level of the right kind for its
 * category, mp / survival on the level's own spelling, co-op on a session spelling of its level.
 * The Python lobby also drops a context older than 15 s (its intent file's age); in-process the
 * snapshot is the owner's live state, and the lobby clears it when the owner leaves the room
 * (Host::LocalPlayLeave), so a context of an earlier room cannot be met here. */
static void LocalhostSelection(Server &s, Conn *c, Selection &sel)
{
    if (s.cfg.differential || !c) return;
    LocalRoute in;
    uint64_t owner = RoomOwner(s, *c);
    if (!owner || !s.host->GetLocalRoute(owner, in) || !in.enabled || in.ownerUid != owner) return;
    if (in.map.empty() || in.gametype.empty() || in.category.empty()) return; /* no room context */
    std::string session = in.sessionMap.empty() ? in.map : in.sessionMap;
    if (!Ident(in.map, 63) || !Ident(session, 63) || !Ident(in.gametype, 31)) return;
    const std::string &type = in.category;
    if (type != "mp" && type != "coop" && type != "zm" && type != "survival") return;
    if (type == "zm" ? in.map.compare(0, 3, "zm_") != 0 : in.map.compare(0, 3, "mp_") != 0) return;
    if ((type == "mp" || type == "survival") && in.map != session) return;
    if (type == "survival" && in.gametype != "survival") return;
    if (type == "coop" && (in.gametype != "coop" || BaseMap(session) != in.map)) return;
    std::string hs, hg, ht;
    HostSelection(sel, hs, hg, ht);
    if (sel.error.empty() && sel.map == in.map && hs == session && hg == in.gametype && ht == type) return; /* the same room */
    sel.map = in.map;
    sel.sessionMap = session;
    sel.gametype = in.gametype;
    sel.type = sel.category = type;
    sel.source = "localhost";
    sel.resolved = true;
    sel.playlist = -1;
    sel.playlistName.clear();
    sel.rotated = false;
    sel.rotationIndex = -1;
    sel.error.clear();
    if (type != "zm") sel.difficulty = -1;
    else if (sel.difficulty < 0) sel.difficulty = 0;
    sel.chain.push_back("current room: " + session + "/" + in.gametype + " (" + type + ") from the owner's explicit localhost context");
}

Selection ResolveSelection(Server &s, Conn *c, const char *when)
{
    Selection sel = HandoffSelect(s, Desc(s, c));
    if (s.cfg.differential && !sel.error.empty()) {
        /* dw_rooms._resolve_selection: select() raised -> the fixed fallback record */
        std::string why = "select() raised ValueError('" + sel.error + "')";
        sel = Selection();
        sel.map = sel.sessionMap = "mp_killhouse_sh";
        sel.gametype = "combat";
        sel.type = sel.category = "mp";
        sel.source = "exception";
        sel.chain.push_back(why);
    }
    LocalhostSelection(s, c, sel);
    sel.playerName = LivePlayerName(s, c);
    /* The round-robin advances ONCE per match, at the READY press only: the join resolutions inside
     * the same match keep seeing the index this match was resolved with. */
    std::string advance = !strcmp(when, "ready") ? HandoffRotationAdvance(s, sel) : std::string();
    std::string line = Fmt("conn#%d    selection[%s] %s", c ? c->idx : 0, when, sel.Summary().c_str());
    for (size_t i = 0; i < sel.chain.size(); i++) line += "\n        - " + sel.chain[i];
    if (!advance.empty()) line += "\n        rotation: " + advance;
    s.Log(line);
    return sel;
}

/* The owner's room changed: hand the embedder the selection (see the policy below). Only a selection
 * the room really named - not the last-resort pin - and only when it differs from the last one. */
void ArmOwnRoom(Server &s, Conn &c, const char *why)
{
    if (s.cfg.differential) return;
    ConnRoom &cr = ConnState(s, c);
    uint64_t uid = ConnUid(s, c);
    if (!uid || !cr.inRoom || RoomOwner(s, c) != uid) return;
    Selection sel = HandoffSelect(s, Desc(s, &c));
    LocalhostSelection(s, &c, sel);
    if (!sel.error.empty() || sel.source == "pin" || sel.source == "default" || !HandoffSelectionSafe(sel, NULL)) return;
    std::string session, gametype, type, err;
    HostSelection(sel, session, gametype, type);
    std::string key = session + "/" + gametype + "/" + type + Fmt("/%d", sel.difficulty);
    if (key == cr.armed) return;
    cr.armed = key;
    int port = 0;
    bool ok = s.host->LocalPlayStart(uid, session, gametype, type, sel.difficulty, false, port, err);
    s.Log(Fmt("conn#%d    local host: room selection %s (%s) -> %s", c.idx, key.c_str(), why, ok ? Fmt("armed, 127.0.0.1:%d", port).c_str() : err.c_str()));
}

/* Look inside an inbound batch for the client's OWN dwRoomInfo (the host's map click) and latch it. */
static std::string ObserveRoomInfo(Server &s, const Bytes &blob)
{
    std::vector<RoomInfo> recs = ScanRoomInfo(blob);
    const RoomInfo *last = NULL;
    /* gametype "playlist" is the mode-1 "nothing chosen yet" sentinel carrying the ctor default map */
    for (size_t i = 0; i < recs.size(); i++)
        if (!recs[i].map.empty() && recs[i].gametype != "playlist") last = &recs[i];
    if (!last) return "";
    std::string note = Fmt(" | ROOM INFO %s/%s maxp=%d", last->map.c_str(), last->gametype.c_str(), last->maxPlayers);
    if (HandoffLatchRoom(s, last->map, &last->gametype, last->maxPlayers, "typeId-15 from the client")) note += " -> LATCHED";
    return note;
}

/* ============================================================================================
 * THE MATCH HANDOFF POLICY - the one deliberate difference from the Python lobby.
 *
 * The Python lobby places a start on an external match server (handoff/match_instances.py),
 * dispatches `map` to it over RCON and points START_GAME at it. The offline product has no
 * external server: the match is hosted by Kernel33's embedded host on 127.0.0.1.
 *
 *  - differential (tests): what the Python lobby tells the client when its single instance never
 *    answers sh1x_status. place() then takes its "no server answered yet" arm: the selection is
 *    unchanged, START_GAME addresses the instance (Config::mmHostIp : mmHostPort) and the player's
 *    seat is reserved for 60 s. No RCON is sent, nothing is spawned.
 *  - the product: EVERY room the local player owns is hosted by the embedded server. There is no
 *    choice to tick and nothing to fall back to (docs/P2P_DEFERRED_HOST_START.md, "Offline mode").
 *    On a start the lobby names the selection it resolved - the one SET_MAP is about to give the
 *    client - to the embedder (Host::LocalPlayStart -> K33LocalPlayAuto), which arms its host for
 *    exactly that selection, whatever was armed before:
 *      - accepted: START_GAME addresses 127.0.0.1:<port>; nothing is probed and no map is
 *        dispatched (the host boots straight into the selection). The launch is raised when the
 *        security key and the admission roster are on disk (NotifyLocalStart); if that is refused
 *        the whole start is withheld.
 *      - refused (an unknown category or map, a missing zone or server, a host that already ran in
 *        this client run, a Survival room with no installed level): START_GAME is withheld with the
 *        reason, which also goes to the player (the UI's host error and a ticker line). No other
 *        server is ever substituted.
 *    A player in a room somebody else owns is handed off only to that owner's host once it answers
 *    with this selection (rule 4 of the document); until then that press is withheld.
 *    Withholding pushes no START_GAME and no SET_MAP (a JoinResponse would blank the room card): the
 *    room stays usable.
 * ============================================================================================ */

MatchRoute RouteMatch(Server &s, Conn &c, const Selection &sel)
{
    MatchRoute r;
    uint64_t uid = ConnUid(s, c);
    if (s.cfg.differential) {
        r.start = true;
        r.ip = s.cfg.mmHostIp;
        r.port = s.cfg.mmHostPort;
        r.note = Fmt("instances: no server answered sh1x_status yet -- dispatching to :%d (not sent: differential)", r.port);
    } else {
        std::string unsafe, session, gametype, type, why;
        if (!sel.error.empty()) { /* nothing playable was selected (a Survival room with no installed level) */
            r.reason = sel.error;
            r.note = "local host: START_GAME withheld: " + sel.error;
            return r;
        }
        if (!HandoffSelectionSafe(sel, &unsafe)) {
            r.reason = "The room's map or mode is not a valid name.";
            r.note = "local host: START_GAME withheld: " + unsafe;
            return r;
        }
        HostSelection(sel, session, gametype, type);
        uint64_t owner = RoomOwner(s, c);
        int port = 0;
        if (owner && owner != uid) {
            /* somebody else's room: only that owner's host, and only once it runs this selection */
            LocalRoute in;
            bool running = s.host->GetLocalRoute(owner, in) && in.enabled && in.ownerUid == owner && in.ready && in.state == "ready" &&
                           !in.failed && in.sessionMap == session && in.gametype == gametype && in.category == type;
            if (!running || in.port < 1 || in.port > 65535) {
                r.reason = "The room owner's match host is not running this match yet.";
                r.note = "local host: the room owner has not launched the host for this selection yet -- START_GAME withheld for this guest";
                return r;
            }
            port = in.port;
        } else {
            if (!s.host->LocalPlayStart(uid, session, gametype, type, sel.difficulty, false, port, why) || port < 1 || port > 65535) {
                r.reason = why.empty() ? Fmt("bad host port %d", port) : why;
                r.note = "local host: START_GAME withheld: " + r.reason;
                return r;
            }
            r.owner = true;
        }
        r.start = r.local = true;
        r.ip = "127.0.0.1";
        r.port = port;
        r.launchSession = session;
        r.launchGametype = gametype;
        r.launchType = type;
        r.note = Fmt("local host: %s on 127.0.0.1:%d <- %s/%s/%s", r.owner ? "the owner's room; the host starts with the owner's map load" : "joining the owner's running host",
                     r.port, session.c_str(), gametype.c_str(), type.c_str());
    }
    if (uid) Rooms(s).reserved[uid] = s.host->Mono();
    return r;
}

/* The local start, once securitykey.bin and the admission roster are in Config::state: export the
 * player's match profile, raise the host launch and tell the embedder. -> false when the launch was
 * refused (the intent changed under us): the caller withholds START_GAME. */
bool NotifyLocalStart(Server &s, Conn &c, const Selection &sel, const MatchRoute &route, const Bytes &key, std::string &note)
{
    uint64_t uid = ConnUid(s, c);
    std::string claim;
    MatchClaim(s, c, sel, claim); /* bookkeeping only: the single player may always re-point their own host */
    std::string blob = ExportMatchBlob(s, uid);
    if (!blob.empty()) note += " | " + blob;
    if (route.owner) {
        int port = 0;
        std::string why;
        if (!s.host->LocalPlayStart(uid, route.launchSession, route.launchGametype, route.launchType, sel.difficulty, true, port, why) ||
            port != route.port) {
            note += " | local host: the launch was REFUSED (" + (why.empty() ? std::string("the host port changed") : why) + ") -- START_GAME withheld";
            return false;
        }
    }
    StartInfo info;
    info.uid = uid;
    info.playerName = sel.playerName.empty() ? ConnName(s, c) : sel.playerName;
    info.map = sel.map;
    HostSelection(sel, info.sessionMap, info.gametype, info.zoneType);
    info.roomMap = sel.sessionMap;
    info.roomGametype = route.launchGametype.empty() ? sel.gametype : route.launchGametype;
    info.difficulty = sel.difficulty;
    info.hostIp = route.ip;
    info.hostPort = route.port;
    info.securityKey = key;
    info.local = route.local;
    s.host->OnStartGame(info);
    note += claim + Fmt(" | local start -> host %s:%d map=%s gametype=%s zone=%s", route.ip.c_str(), route.port, info.map.c_str(),
                        info.gametype.c_str(), info.zoneType.c_str());
    return true;
}

/* ---- dwRoomMessageStartGame (typeId 3) ----
 * Body, LSB-first bit packed: 1 bit 0, 8 bits typeId 3, then four blobs each as [u32 len][bytes]
 * with NO type tags ("notag", the one shape the client accepts):
 *   bdCommonAddr (37 B)   the host to dial: 6 x {ip4, port le16}, slots 0 and 5 = the host, natType 1
 *   session id   (8 B)
 *   clientSecurityKey (16 B)  also the bdDTLS key, so it goes to securitykey.bin for the host
 *   trailing block (57 B; the descriptor carries 56, padded)
 * The record is [u32 body length][body]. The map is NOT in it: the client reads it from its
 * dwRoomInfo, which the SET_MAP pushes write first. */
static bool BuildStartgameRecord(Server &s, Conn &c, const Vals *desc, const std::string &ip, int port, Bytes &rec, std::string &note, Bytes &keyOut)
{
    if (!desc || desc->empty()) { note = "no session descriptor captured yet"; return false; }
    const Bytes *key = NULL, *tail = NULL;
    for (size_t i = 0; i < desc->size(); i++) {
        const Val &v = (*desc)[i];
        if (v.tag != T_BLOB) continue;
        if (!key && v.s.size() == 16) key = &v.s;
        if (!tail && (v.s.size() == 56 || v.s.size() == 57)) tail = &v.s;
    }
    if (!key || !tail) { note = Fmt("missing blob(s): key=%d tail=%d", key != NULL, tail != NULL); return false; }
    Bytes blobs[4] = { CommonAddr(ip, port), SessionId(), *key, *tail };
    if (blobs[3].size() == 56) blobs[3] += '\0';
    std::string sk = SeckeyStartgameWrite(s, &c, *key);
    MsgWriter w;
    w.Bits(0, 1);
    w.Bits(3, 8);
    for (int i = 0; i < 4; i++) {
        w.Bits(blobs[i].size(), 32);
        w.Raw(blobs[i]);
    }
    rec = w_u32_raw((uint32_t)w.buf.size()) + w.buf;
    keyOut = c.seckey.size() == 16 ? c.seckey : *key;
    note = Fmt("StartGame typeId=3 addr=%s:%d key=%s -> %uB body", ip.c_str(), port, KeyFp(*key).c_str(), (unsigned)w.buf.size()) + sk;
    return true;
}

static Bytes Push7(uint64_t sender, const std::string &name, uint64_t rid, const Bytes &blob, uint32_t trailer)
{
    Bytes msg(1, (char)LOBBY_SERVICE_PUSH_MESSAGE);
    msg += w_u32(7);
    msg += w_u64(sender);
    msg += w_name(name, 64);
    msg += w_u64(rid);
    msg += w_blob(blob);
    msg += w_u32(trailer);
    return msg;
}

/* Ready pressed: the echo alone starts nothing, so follow it with SET_MAP and a real START_GAME
 * built from the session the client described. */
static std::string ReadyPress(Server &s, Conn &c, uint64_t rid, uint32_t trailer)
{
    Selection sel = ResolveSelection(s, &c, "ready");
    MatchRoute route = RouteMatch(s, c, sel);
    std::string note = " | " + route.note;
    if (!route.start) {
        if (!s.cfg.differential && !route.reason.empty()) PushLobbyNotice(s, c, "Match not started: " + route.reason);
        return note;
    }

    Bytes rec, key;
    std::string why;
    bool built = BuildStartgameRecord(s, c, Desc(s, &c), route.ip, route.port, rec, why, key);
    if (!built && route.local) return note + " | [READY] cannot build StartGame: " + why + " -- local match NOT started";
    /* The launch is raised before anything is pushed: a refused launch withholds the whole start. */
    if (route.local && !NotifyLocalStart(s, c, sel, route, key, note)) {
        PushLobbyNotice(s, c, "Match not started: the local match host refused the launch.");
        return note;
    }

    /* START_GAME carries no map: the client reads it from dwRoom+0x15a88, which only a dwRoomInfo
     * writes - so the map goes first. session_map, not map: the string picks the ZONE SET the
     * client preloads (coop_training_mp_killhouse_sh for co-op). typeId 5 (JoinResponse, sender 0)
     * is the shape a HOST client accepts; typeId 15 (sender = the room record's admin id) the one a
     * joiner does. */
    const std::string &map = sel.sessionMap.empty() ? sel.map : sel.sessionMap;
    /* A local host is told the native mode name (tdm -> war, ffa -> dm, hq -> koth) and answers with
     * it; the client's dwRoomInfo gets that same name (match_instances._localhost_placement, Python
     * 8381ddd: the selection sent on is the host's). */
    const std::string &gt = route.local && !route.launchGametype.empty() ? route.launchGametype : sel.gametype;
    /* The JoinResponse empties the receiver's member roster (it must carry 0 members: C1 Com_Error 702), which
     * the owner's room view shows after the match as a card of placeholder texts. The owner of a multiplayer
     * room whose own dwRoomInfo already names this map does not need it: its START_GAME reads that map. Every
     * other owner's client is told the names beside the room (Survival: room coop_survival_<level> / coop, match
     * <level> / survival). */
    bool ownRoom = route.owner && sel.roomMap && (sel.type == "mp" || sel.type == "zm") && sel.sessionMap == sel.map;
    bool beside = false;
    if (route.owner) {
        bool want = built && !ownRoom && !map.empty();
        beside = s.host->StartNames(want ? map : std::string(), want ? gt : std::string()) && want;
    }
    if (!map.empty()) {
        Bytes jr = BuildJoinResponseRecord(map, gt), mr = BuildRoomInfoRecord(map, gt);
        if (beside)
            note += " | no JoinResponse: the owner's client loads " + map + "/" + gt + " and keeps its room";
        else if (!ownRoom)
            c.Queue(MapPushPayload(jr, rid, 0, kSenderName, trailer), Fmt("push 7 SET_MAP(joinresp) %s/%s, %uB", map.c_str(), gt.c_str(), (unsigned)jr.size()));
        else
            note += " | no JoinResponse: the owner's own room already names " + map;
        c.Queue(MapPushPayload(mr, rid, ROOM_ID, kSenderName, trailer), Fmt("push 7 SET_MAP %s/%s, %uB", map.c_str(), gt.c_str(), (unsigned)mr.size()));
        note += Fmt(" | SET_MAP -> %s gametype=%s playlist=%d %s", map.c_str(), gt.c_str(), sel.playlist, sel.resolved ? "RESOLVED" : "DEFAULTED");
    }
    if (!built) return note + " | [READY] cannot build StartGame: " + why;
    c.Queue(Push7(SENDER_USER_ID, kSenderName, rid, rec, trailer), Fmt("push 7 START_GAME, %uB", (unsigned)rec.size()));
    note += " | READY -> pushed " + why;
    return note;
}

/* ---- task 4: the batch of queued room messages ---- */

static Reply RoomMessages(Server &s, Conn &c, const Vals &args)
{
    const Val *ridV = First(args, T_UINT64), *blobV = First(args, T_BLOB), *trailerV = First(args, T_UINT32);
    uint64_t rid = ridV ? ridV->u : 0;
    uint32_t trailer = trailerV ? (uint32_t)trailerV->u : 0;
    bool known = ridV && (rid == ROOM_ID || RoomFind(s, rid));
    size_t npush0 = c.push.size();
    Reply rep = Reply::Ok(Fmt("room messages roomId=%llu", (unsigned long long)rid));
    if (known) rep.note += " *** OURS, the room is live ***";
    else if (ridV && !rid) rep.note += " *** ZERO -- task 13 was rejected ***";

    if (known && blobV) {
        const Bytes &blob = blobV->s;
        std::string latched = ObserveRoomInfo(s, blob); /* read it before relaying it: the host's map click lives in here */
        rep.note += latched;
        if (latched.find("LATCHED") != std::string::npos) ArmOwnRoom(s, c, "room info");
        std::set<int> types = RoomBatchTypeIds(blob);
        /* The echo keeps the historical envelope sender, except for a batch carrying the client's
         * typeId-18 AVATAR record: the receiver (0x007135F6) looks the envelope sender up in its
         * member array, so that one is stamped with the client's own uid. The name is the
         * connection's own gamertag - it is what the room chat draws next to a relayed line. */
        uint64_t sender = SENDER_USER_ID;
        std::string name = ConnName(s, c);
        if (types.count(18)) {
            uint64_t own = c.uid ? c.uid : DescPlayerUid(s, &c);
            if (own) sender = own;
        }
        c.Queue(Push7(sender, name, rid, blob, trailer), Fmt("push 7 relay, %uB", (unsigned)blob.size()));
        std::string tl;
        for (std::set<int>::const_iterator it = types.begin(); it != types.end(); ++it) tl += Fmt("%s%d", tl.empty() ? "" : ",", *it);
        rep.note += Fmt(" -> relaying %uB [typeIds %s] as sender=%llu name='%s'", (unsigned)blob.size(), tl.c_str(), (unsigned long long)sender, name.c_str());

        /* Fan-out to the other members, stamped with the true sender. */
        uint64_t ownUid = ConnUid(s, c);
        std::string whyNot = FanoutRefusal(s, c, rid, types);
        if (whyNot.empty()) whyNot = JoinreqGate(s, c, rid, types);
        if (!whyNot.empty()) {
            rep.note += " | NOT fanned out: " + whyNot;
        } else if (ownUid) {
            /* A host's typeId-5 JoinResponse is guarded on the joiner by sender == dwRoom+0x15b20,
             * whose every writer stores ZERO - so that one batch travels with sender 0. */
            int nf = RoomFanout(s, c, rid, Push7(types.count(5) ? 0 : ownUid, name, rid, blob, trailer),
                                Fmt("push 7 relay, %uB", (unsigned)blob.size()), false);
            if (nf) rep.note += Fmt(" | fanned out to %d member(s) as sender=%llu", nf, (unsigned long long)ownUid);
        }

        if (blob.size() == sizeof(kReadyPressBlob) && !memcmp(blob.data(), kReadyPressBlob, sizeof(kReadyPressBlob)))
            rep.note += ReadyPress(s, c, rid, trailer);
    }
    /* Lobby-synthesized pushes queued by THIS batch (SET_MAP / START_GAME) go to the other members
     * too, so a joiner follows the host into the match. */
    int nf = 0;
    for (size_t i = npush0, n = c.push.size(); i < n; i++) {
        const std::string &pn = c.push[i].note;
        if (pn.find("START_GAME") != std::string::npos || pn.find("SET_MAP") != std::string::npos) nf += RoomFanout(s, c, rid, c.push[i].msg, pn, true);
    }
    if (nf) rep.note += Fmt(" | start/map pushes fanned out x%d", nf);
    return rep;
}

Reply RoomTask(Server &s, Conn &c, int task, const Vals &args)
{
    const Val *ridV = First(args, T_UINT64);
    uint64_t rid = ridV ? ridV->u : 0;

    if (task == 13) { /* CREATE ROOM (no arguments): a non-zero room id back, or the client tears the room down */
        uint64_t id = RoomAlloc(s, c);
        Reply rep = Reply::Ok(Fmt("CREATE ROOM -> roomId %llu (host uid=%llu)", (unsigned long long)id, (unsigned long long)ConnUid(s, c)));
        rep.results.push_back(RoomRecord(id, 1));
        return rep;
    }
    if (task == 2) { /* LEAVE ROOM */
        std::string how = ridV ? RoomLeave(s, c, rid, "leave") : std::string("no room id");
        return Reply::Ok(Fmt("LEAVE ROOM roomId=%llu -> %s", (unsigned long long)rid, how.c_str()));
    }
    if (task == 11) { /* JOIN ROOM */
        /* The JOIN clears dwRoom+0x44f8 (amHost): the first instant a typeId-15 room-info push can
         * be accepted, so set the map here too. */
        Selection sel = ResolveSelection(s, &c, "join");
        if (!ridV) return Reply::Ok("JOIN ROOM without a room id -- empty success"); /* the Python handler raises: its guard's answer */
        const std::string &map = sel.sessionMap.empty() ? sel.map : sel.sessionMap;
        if (!map.empty()) {
            Bytes mr = BuildRoomInfoRecord(map, sel.gametype);
            c.Queue(MapPushPayload(mr, rid, ROOM_ID, kSenderName, 0),
                    Fmt("push 7 SET_MAP %s/%s (post-join), %uB", map.c_str(), sel.gametype.c_str(), (unsigned)mr.size()));
        }
        /* The join needs the room record back: a bare ack makes the client leave after 10 s. */
        unsigned n = RoomJoin(s, c, rid);
        Reply rep = Reply::Ok(Fmt("JOIN ROOM roomId=%llu -> room record (%u members in registry)", (unsigned long long)rid, n));
        rep.results.push_back(RoomRecord(rid, n));
        return rep;
    }
    if (task == 15) { /* ROOM MEMBER LIST (getChannelMembers): args u64 roomId, u32 maxResults */
        const Val *maxV = First(args, T_UINT32);
        uint64_t maxr = maxV && maxV->u ? maxV->u : 100;
        uint64_t uid = c.uid ? c.uid : DescPlayerUid(s, &c);
        if (!uid) uid = SENDER_USER_ID;
        std::string name = ConnName(s, c);
        std::vector<Bytes> recs;
        recs.push_back(w_u64(uid) + w_name(name, 64) + w_u32(0));
        std::vector<Member> mem = ridV ? RoomMembers(s, rid) : std::vector<Member>();
        if (mem.size() > 1 || (!mem.empty() && mem[0].uid != uid)) {
            recs.clear();
            bool self = false;
            for (size_t i = 0; i < mem.size(); i++) {
                recs.push_back(w_u64(mem[i].uid) + w_name(mem[i].name, 64) + w_u32(0));
                if (mem[i].uid == uid) self = true;
            }
            if (!self) recs.push_back(w_u64(uid) + w_name(name, 64) + w_u32(0));
        }
        if (recs.size() > maxr) recs.resize((size_t)maxr);
        Reply rep = Reply::Ok(Fmt("MEMBER LIST roomId=%llu max=%llu -> %u member(s)", (unsigned long long)rid, (unsigned long long)maxr, (unsigned)recs.size()));
        rep.results = recs;
        return rep;
    }
    if (task == 4) return RoomMessages(s, c, args);
    return Reply();
}

} // namespace dw

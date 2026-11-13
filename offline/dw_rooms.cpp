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
 *                  SET_MAP (typeId 5 + 15) and START_GAME (typeId 3).
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
 *  - the product: the owner's Local Play intent decides (docs/P2P_DEFERRED_HOST_START.md, "What the
 *    lobby must do"). Ticking LOCAL PLAY only arms a host; Kernel33 starts it when the match
 *    launches. There is no public server to fall back to, and another server is never substituted:
 *      1. no intent / not ticked          -> withhold START_GAME (nothing to join)
 *      2. failed, stopping, exited, idle  -> withhold
 *      3. the intent's selection differs from the room's -> withhold
 *      4. ready                           -> hand off to 127.0.0.1:<port> (the host already runs it)
 *      5. armed / starting, not ready     -> the owner: hand off to 127.0.0.1:<port> without probing
 *         or dispatching anything, and raise the launch (Host::LocalPlayLaunch) at that moment;
 *         withhold if it is refused. Anybody else: withhold until 4 holds.
 *    Withholding pushes no START_GAME and no SET_MAP (a JoinResponse would blank the room card): the
 *    room stays usable and the player can tick Local Play and press Start again.
 * ============================================================================================ */

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
        LocalRoute in;
        std::string unsafe, session, gametype, type;
        if (!sel.error.empty()) { /* nothing playable was selected (a Survival room with no installed level) */
            r.note = "local play: START_GAME withheld: " + sel.error;
            return r;
        }
        if (!s.host->GetLocalRoute(uid, in) || !in.enabled) {
            r.note = "LOCAL PLAY IS OFF -- no match host to join, START_GAME withheld (tick Local Play and press Start again)";
            return r;
        }
        if (in.failed || in.state == "failed" || in.state == "stopping" || in.state == "exited" || in.state == "idle") {
            r.note = "local play: the host choice is " + (in.state.empty() ? std::string("failed") : in.state) +
                     (in.error.empty() ? "" : " (" + in.error + ")") + " -- START_GAME withheld";
            return r;
        }
        HostSelection(sel, session, gametype, type);
        /* The intent names the level the host loads (map) and the room's spelling (session map); a
         * co-op / Survival room's two spellings differ, so either may carry the room's. */
        bool sameMap = in.sessionMap == session || in.sessionMap == sel.sessionMap || (!in.map.empty() && in.map == sel.map && in.sessionMap == sel.map);
        if (!sameMap || in.gametype != gametype || in.category != type) {
            r.note = Fmt("local play: the armed selection %s/%s/%s is not this room's %s/%s/%s -- START_GAME withheld",
                         in.sessionMap.c_str(), in.gametype.c_str(), in.category.c_str(), session.c_str(), gametype.c_str(), type.c_str());
            return r;
        }
        if (in.port < 1 || in.port > 65535 || !HandoffSelectionSafe(sel, &unsafe)) {
            r.note = "local play: START_GAME withheld: " + (unsafe.empty() ? Fmt("bad host port %d", in.port) : unsafe);
            return r;
        }
        bool running = in.ready && in.state == "ready";
        if (!running) {
            if (!(in.armed || in.state == "armed" || in.state == "starting")) {
                r.note = "local play: the host is neither armed nor ready (" + in.state + ") -- START_GAME withheld";
                return r;
            }
            if (in.ownerUid && uid != in.ownerUid) {
                r.note = "local play: the owner has not launched the host yet -- START_GAME withheld for this guest";
                return r;
            }
        }
        r.start = r.local = true;
        r.ip = "127.0.0.1";
        r.port = in.port;
        r.launchSession = in.sessionMap;
        r.launchGametype = in.gametype;
        r.launchType = in.category;
        r.note = Fmt("local play: %s on 127.0.0.1:%d <- %s/%s/%s", running ? "joining the running host" : "owner launches; the host starts with the owner's map load",
                     r.port, in.sessionMap.c_str(), in.gametype.c_str(), in.category.c_str());
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
    if (!s.host->LocalPlayLaunch(uid, route.launchSession, route.launchGametype, route.launchType)) {
        note += " | local play: the launch was REFUSED (the owner's intent is no longer this armed selection) -- START_GAME withheld";
        return false;
    }
    StartInfo info;
    info.uid = uid;
    info.playerName = sel.playerName.empty() ? ConnName(s, c) : sel.playerName;
    info.map = sel.map;
    HostSelection(sel, info.sessionMap, info.gametype, info.zoneType);
    info.roomMap = sel.sessionMap;
    info.roomGametype = sel.gametype;
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
    if (!route.start) return note;

    Bytes rec, key;
    std::string why;
    bool built = BuildStartgameRecord(s, c, Desc(s, &c), route.ip, route.port, rec, why, key);
    if (!built && route.local) return note + " | [READY] cannot build StartGame: " + why + " -- local match NOT started";
    /* The launch is raised before anything is pushed: a refused launch withholds the whole start. */
    if (route.local && !NotifyLocalStart(s, c, sel, route, key, note)) return note;

    /* START_GAME carries no map: the client reads it from dwRoom+0x15a88, which only a dwRoomInfo
     * writes - so the map goes first. session_map, not map: the string picks the ZONE SET the
     * client preloads (coop_training_mp_killhouse_sh for co-op). typeId 5 (JoinResponse, sender 0)
     * is the shape a HOST client accepts; typeId 15 (sender = the room record's admin id) the one a
     * joiner does. */
    const std::string &map = sel.sessionMap.empty() ? sel.map : sel.sessionMap;
    if (!map.empty()) {
        Bytes jr = BuildJoinResponseRecord(map, sel.gametype), mr = BuildRoomInfoRecord(map, sel.gametype);
        c.Queue(MapPushPayload(jr, rid, 0, kSenderName, trailer), Fmt("push 7 SET_MAP(joinresp) %s/%s, %uB", map.c_str(), sel.gametype.c_str(), (unsigned)jr.size()));
        c.Queue(MapPushPayload(mr, rid, ROOM_ID, kSenderName, trailer), Fmt("push 7 SET_MAP %s/%s, %uB", map.c_str(), sel.gametype.c_str(), (unsigned)mr.size()));
        note += Fmt(" | SET_MAP -> %s gametype=%s playlist=%d %s", map.c_str(), sel.gametype.c_str(), sel.playlist, sel.resolved ? "RESOLVED" : "DEFAULTED");
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
        rep.note += ObserveRoomInfo(s, blob); /* read it before relaying it: the host's map click lives in here */
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
        rep.note += Fmt(" -> relaying %uB as sender=%llu name='%s'", (unsigned)blob.size(), (unsigned long long)sender, name.c_str());

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

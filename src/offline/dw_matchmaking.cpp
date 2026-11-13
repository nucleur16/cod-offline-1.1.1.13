/*
 * dw_matchmaking.cpp - bdMatchMaking (service 21): createSession (1), updateSession (12),
 * findSessions (5), findSessionsPaged (13). Port of matchmaking_task, _session_search_record,
 * _common_addr and _live_hosted_rooms in dw-capture.py.
 *
 * A findSessions RESULT is the MatchMakingInfo the client sent in createSession, with the 8-byte
 * session id spliced in after the address blob and numPlayers after maxPlayers - so the lobby
 * mirrors a held descriptor back instead of reconstructing this build's field layout.
 */
#include <stdlib.h>
#include "dw.h"
#include "dw_state.h"
#include "dw_rooms.h"

namespace dw {

/* bdCommonAddr: `slots` x 6-byte {ip4, port le16} then a natType byte. Empty slots are the sentinel
 * 00 FF 00 FF 00 00 (zeros would present as 0.0.0.0:0). Slot 0 = local, last = public; both are the
 * host. This build's blob is 37 bytes = 6 slots + natType 1 (open). */
Bytes CommonAddr(const std::string &ip, int port, int slots)
{
    static const char empty[6] = { 0x00, (char)0xFF, 0x00, (char)0xFF, 0x00, 0x00 };
    Bytes buf;
    for (int i = 0; i < slots; i++) buf.append(empty, 6);
    unsigned char ipb[4] = { 0, 0, 0, 0 };
    const char *p = ip.c_str();
    for (int i = 0; i < 4 && *p; i++) {
        ipb[i] = (unsigned char)strtoul(p, NULL, 10);
        while (*p && *p != '.') p++;
        if (*p == '.') p++;
    }
    int at[2] = { 0, slots - 1 };
    for (int k = 0; k < 2; k++) {
        size_t o = (size_t)at[k] * 6;
        if (at[k] < 0 || o + 6 > buf.size()) continue;
        for (int i = 0; i < 4; i++) buf[o + (size_t)i] = (char)ipb[i];
        buf[o + 4] = (char)(port & 0xFF);
        buf[o + 5] = (char)((port >> 8) & 0xFF);
    }
    buf += (char)1;
    return buf;
}

/* THE session record both search entry points return. `ds` turns it into a dedicated-server row of
 * the developer panel. The query's Int32 criteria are spliced over the descriptor's
 * (query[0] -> desc[5], [1] -> desc[6], [2] -> desc[9]) or the client filters the session out. */
Bytes SessionSearchRecord(Server &s, const Vals &args, const Vals &desc, const DsRow *ds, std::string *shown)
{
    Vals vals = desc;
    std::vector<uint64_t> crit;
    for (size_t i = 0; i < args.size() && crit.size() < 6; i++)
        if (args[i].tag == T_INT32) crit.push_back(args[i].u);
    static const int splice[3][2] = { { 0, 5 }, { 1, 6 }, { 2, 9 } };
    for (int k = 0; k < 3; k++) {
        size_t qi = (size_t)splice[k][0], di = (size_t)splice[k][1];
        if (qi < crit.size() && di < vals.size() && vals[di].tag == T_INT32) vals[di].u = crit[qi];
    }
    /* The session must not look like the client itself: our address, and a host uid that is not the
     * player's (the unset u64 of the descriptor). */
    if (!vals.empty()) vals[0] = Val(T_BLOB, CommonAddr(s.cfg.mmHostIp, ds ? ds->port : s.cfg.mmHostPort));
    if (ds) {
        /* The client books the server by the row's FIRST u64 and reads the host / room names from
         * the two descriptor strings; it keeps the row only while numPlayers <= 1. */
        for (size_t i = 0; i < vals.size(); i++)
            if (vals[i].tag == T_UINT64) { vals[i].u = ds->uid; break; }
        int nth = 0;
        for (size_t i = 0; i < vals.size() && nth < 2; i++)
            if (vals[i].tag == T_STRING) vals[i].s = nth++ == 0 ? ds->host : ds->room;
    } else {
        for (size_t i = 0; i < vals.size(); i++)
            if (vals[i].tag == T_UINT64 && vals[i].u == 0) { vals[i].u = s.cfg.mmHostUid; break; }
    }
    if (shown) {
        shown->clear();
        for (size_t i = 0; i < crit.size() && i < 3; i++) *shown += Fmt(i ? " %llu" : "%llu", (unsigned long long)crit[i]);
    }
    /* [addr] [blob sessionId] [gameType, maxPlayers] [u32 numPlayers] [the rest] */
    return Reserialize(vals, 0, 1) + w_blob(SessionId()) + Reserialize(vals, 1, 3) + w_u32(ds ? 0 : 1) + Reserialize(vals, 3);
}

/* Rooms whose HOST is another live connection and whose createSession descriptor we hold. The
 * caller's own room and rooms it already sits in are excluded. */
static std::vector<const Room *> LiveHostedRooms(Server &s, Conn &c)
{
    std::vector<const Room *> out;
    const std::vector<Room> &rooms = Rooms(s).rooms;
    for (size_t i = 0; i < rooms.size(); i++) {
        const Room &r = rooms[i];
        if (!r.hasDesc || r.desc.empty() || !r.hasHost) continue;
        int hidx = -1;
        bool mine = false;
        for (size_t k = 0; k < r.members.size(); k++) {
            if (r.members[k].first == r.host) hidx = r.members[k].second;
            if (r.members[k].second == c.idx) mine = true;
        }
        if (hidx < 0 || !s.conns.count(hidx) || hidx == c.idx || mine) continue;
        out.push_back(&r);
    }
    return out;
}

static const Bytes *FirstBlobOfSize(const Vals &vals, size_t size)
{
    for (size_t i = 0; i < vals.size(); i++)
        if (vals[i].tag == T_BLOB && vals[i].s.size() == size) return &vals[i].s;
    return NULL;
}

Reply MatchmakingTask(Server &s, Conn &c, int task, const Vals &args)
{
    if (task == 1 || task == 12) HandoffObserveDescriptor(s, task, args);

    if (task == 1) { /* createSession: keep the descriptor, answer the session id */
        Vals vals;
        for (size_t i = 0; i < args.size(); i++)
            if (args[i].tag != 0) vals.push_back(args[i]);
        SetDesc(s, &c, vals);
        /* Remember it on the room this connection sits in, so a findSessions from another client
         * advertises the live room (the joiner then JOINs the host's real room id). */
        ConnRoom &cr = ConnState(s, c);
        if (cr.inRoom) {
            Room *r = RoomFind(s, cr.room);
            if (r) {
                r->desc = vals;
                r->hasDesc = true;
            }
        }
        Reply rep = Reply::Ok(Fmt("createSession -> sessionId %s  desc=%u fields", Hex(SessionId().data(), 8).c_str(), (unsigned)vals.size()));
        const Bytes *key = FirstBlobOfSize(args, 16);
        if (key) rep.note += "  clientSecurityKey=" + KeyFp(*key) + SeckeyObserve(s, &c, *key); /* SK-1: THE live key carrier */
        rep.results.push_back(w_blob(SessionId()));
        ArmOwnRoom(s, c, "createSession");
        return rep;
    }

    if (task == 12) { /* updateSession: [blob sessionId][u32][MatchMakingInfo], the fresher copy */
        Vals vals;
        for (size_t i = 0; i < args.size(); i++)
            if (args[i].tag != 0) vals.push_back(args[i]);
        if (vals.size() > 2 && vals[2].tag == T_BLOB) {
            Vals desc(vals.begin() + 2, vals.end());
            SetDesc(s, &c, desc);
            const Bytes *key = FirstBlobOfSize(desc, 16); /* the second live key carrier */
            ArmOwnRoom(s, c, "updateSession");
            return Reply::Ok(Fmt("updateSession -> descriptor refreshed (%u fields)", (unsigned)desc.size()) +
                             (key ? SeckeyObserve(s, &c, *key) : std::string()));
        }
        return Reply::Ok("updateSession");
    }

    if (task != 5 && task != 13) return Reply();
    const Vals *own = Desc(s, &c);
    bool haveDesc = own && !own->empty();

    if (DevPanelIsDsSearch(args)) { /* the developer panel's server list (criterion #4 == 15) */
        if (!haveDesc) return Reply::Ok("DS list -- no session descriptor captured yet, 0 rows");
        std::vector<DsRow> rows = DevPanelServers(s, c);
        Reply rep = Reply::Ok(Fmt("DS list (dev panel) -> %u server(s)", (unsigned)rows.size()));
        for (size_t i = 0; i < rows.size(); i++) {
            rep.results.push_back(SessionSearchRecord(s, args, *own, &rows[i], NULL));
            rep.note += (i ? "; " : ": ") + rows[i].host + " [" + rows[i].room + "]";
        }
        return rep;
    }

    /* Live hosted rooms first, each from its host's own descriptor; none -> the phantom session. */
    std::vector<const Room *> live = LiveHostedRooms(s, c);
    if (!live.empty()) {
        std::vector<uint64_t> u32s;
        for (size_t i = 0; i < args.size(); i++)
            if (args[i].tag == T_UINT32) u32s.push_back(args[i].u);
        uint64_t cap = u32s.size() > 1 ? u32s[1] : (u32s.empty() ? 20 : u32s[0]);
        if (!cap) cap = 20;
        Reply rep = Reply::Ok(Fmt("%s -> LIVE hosted room(s)", task == 5 ? "findSessions" : "findSessionsPaged"));
        for (size_t i = 0; i < live.size() && i < cap; i++) {
            rep.results.push_back(SessionSearchRecord(s, args, live[i]->desc, NULL, NULL));
            rep.note += Fmt(" %llu", (unsigned long long)live[i]->rid);
        }
        return rep;
    }

    /* A zero-result answer is what stalls the start: the client gives up and hosts its own room. */
    if (!haveDesc)
        return Reply::Ok(std::string(task == 5 ? "findSessions" : "findSessionsPaged") + " -- no session descriptor captured yet, 0 results");
    /* The record is built from the SHARED last-seen descriptor, as the Python lobby does (callers
     * guarantee one exists: a connection's own descriptor is also the shared one, or newer than it). */
    const Vals *shared = Desc(s, NULL);
    if (!shared || shared->empty()) /* the Python handler raises here and its guard answers an empty success */
        return Reply::Ok("findSessions -- the shared descriptor is empty, 0 results");
    std::string shown;
    Bytes rec = SessionSearchRecord(s, args, *shared, NULL, &shown);
    Reply rep = Reply::Ok(Fmt("%s -> 1 session (%uB) crit[%s] host=%s:%d uid=%llu players=1", task == 5 ? "findSessions" : "findSessionsPaged",
                              (unsigned)rec.size(), shown.c_str(), s.cfg.mmHostIp.c_str(), s.cfg.mmHostPort,
                              (unsigned long long)s.cfg.mmHostUid));
    rep.results.push_back(rec);
    return rep;
}

} // namespace dw

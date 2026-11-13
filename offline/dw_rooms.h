/*
 * dw_rooms.h - state and internal interfaces shared by the room-side modules:
 * dw_rooms.cpp, dw_session.cpp, dw_matchmaking.cpp, dw_devpanel.cpp, dw_pushes.cpp, dw_handoff.cpp.
 * These six are one unit (leave all or none of them out with OFFLINE_SKIP). Other modules use the
 * declarations in dw_state.h only.
 */
#pragma once
#include "dw.h"
#include "dw_state.h"
#include "dw_handoff.h"

namespace dw {

enum {
    ROOM_ID = 69421,        /* the first room id, and every room record's SECOND u64 (the admin id) */
    SENDER_USER_ID = 69420, /* the historical echo envelope sender */
    MATCH_RESERVE_SECONDS = 60,
};
extern const char *const kSenderName; /* "EDITz": last-resort display name, never a gamertag */
Bytes SessionId();                    /* the 8-byte bdSecurityID every session gets: 46 00 .. 00 */

/* dw_rooms._ROOMS[rid]. `members` and `names` are Python dicts keyed by uid: unique keys in
 * insertion order, and assigning an existing key keeps its position. */
struct Room {
    uint64_t rid;
    bool hasHost;
    uint64_t host;
    std::vector<std::pair<uint64_t, int> > members;       /* uid -> connection idx */
    std::vector<std::pair<uint64_t, std::string> > names; /* uid -> display name (kept after a leave) */
    bool hasDesc;
    Vals desc; /* the createSession descriptor of whoever created a session while in this room */
    Room() : rid(0), hasHost(false), host(0), hasDesc(false) {}
};

/* The per-connection scalars the Python lobby keeps in its `state` dict for these modules. */
struct ConnRoom {
    bool inRoom;
    uint64_t room;
    std::map<uint64_t, double> joinreqRelayed; /* room id -> when a join-request batch was last relayed */
    std::string armed; /* the selection last handed to the embedder for this connection's own room */
    int mpTasks, invPushes, tickPushes;
    bool invPushed;
    double invPushAt, tickPushAt;
    Bytes tickText;
    ConnRoom() : inRoom(false), room(0), mpTasks(0), invPushes(0), tickPushes(0), invPushed(false), invPushAt(0), tickPushAt(0) {}
};

struct RoomsState {
    std::vector<Room> rooms;            /* insertion order, as dict iteration in Python */
    uint64_t nextId;                    /* the next room id never issued */
    std::map<int, ConnRoom> conn;       /* by connection idx; dropped when the connection closes */
    std::map<uint64_t, double> reserved; /* uid -> when a start placed that player on the match host */
    HandoffState *handoff;
    RoomsState() : nextId(ROOM_ID), handoff(NULL) {}
};

struct AdmissionEntry { /* one line of game_admissions.txt */
    std::string ip, key, gamertag;
    uint64_t uid;
    int64_t last, first;
};

struct SessionState {
    bool hasDesc;
    Vals desc; /* the most recent descriptor of ANY connection (dw_session._session["desc"]) */
    int connGen, observedGen;
    bool suspect;
    Bytes lastKey; /* what this process last wrote to securitykey.bin */
    /* the match claim (dw_session._MATCH) */
    bool claimed;
    std::string claimKey, claimMap;
    double claimWhen;
    int claimBy;
    uint64_t claimUid;
    std::vector<AdmissionEntry> roster;
    SessionState() : hasDesc(false), connGen(0), observedGen(-1), suspect(false), claimed(false), claimWhen(0), claimBy(0), claimUid(0) {}
};

RoomsState &Rooms(Server &s);
SessionState &Session(Server &s); /* created on first use */
ConnRoom &ConnState(Server &s, Conn &c);

/* ---- dw_session.cpp ---- */
/* -> true when a start for `sel` may (re)point the match host; `note` says why not. */
bool MatchClaim(Server &s, Conn &c, const Selection &sel, std::string &note);
/* Put this seat's key on game_admissions.txt. -> note fragment. */
std::string GameAdmissionPublish(Server &s, Conn &c, const Bytes &key);

/* ---- dw_rooms.cpp ---- */
Room *RoomFind(Server &s, uint64_t rid);
/* Resolve the selection for this connection's room; when == "ready" also advances the rotation. */
Selection ResolveSelection(Server &s, Conn *c, const char *when);
bool InMatch(Server &s, uint64_t uid);

/* THE MATCH HANDOFF POLICY: where a room start sends the players (see dw_rooms.cpp). */
struct MatchRoute {
    bool start;      /* false: nothing to join, START_GAME must not be pushed */
    bool local;      /* the embedded host on 127.0.0.1 (the offline product) */
    std::string ip;
    int port;
    bool owner;      /* the local route is this player's own host (launch it), not somebody else's (join it) */
    std::string note;
    std::string reason; /* why nothing is started, in words for the player ("" when start) */
    std::string launchSession, launchGametype, launchType; /* the selection as the embedded host is told it */
    MatchRoute() : start(false), local(false), port(0), owner(false) {}
};
MatchRoute RouteMatch(Server &s, Conn &c, const Selection &sel);
/* A room the local player owns changed (its descriptor, its dwRoomInfo): tell the embedder the
 * selection, so its host is armed for what the room shows now. Quiet; never starts anything. */
void ArmOwnRoom(Server &s, Conn &c, const char *why);
/* dw_pushes.cpp: one line on the client's lobby ticker (push 35). */
void PushLobbyNotice(Server &s, Conn &c, const std::string &text);
/* After the security key and the roster are on disk: export the profile and tell the embedder. */
/* -> false when the embedder refused the launch: START_GAME must then be withheld. */
bool NotifyLocalStart(Server &s, Conn &c, const Selection &sel, const MatchRoute &route, const Bytes &key, std::string &note);

/* ---- dw_matchmaking.cpp ---- */
Bytes CommonAddr(const std::string &ip, int port, int slots = 6);
struct DsRow { /* a dedicated-server row of the developer panel's list */
    int port;
    uint64_t uid;
    std::string host, room;
};
Bytes SessionSearchRecord(Server &s, const Vals &args, const Vals &desc, const DsRow *ds, std::string *shown);

/* ---- dw_devpanel.cpp ---- */
bool DevPanelIsDsSearch(const Vals &args);
std::vector<DsRow> DevPanelServers(Server &s, Conn &c);

} // namespace dw

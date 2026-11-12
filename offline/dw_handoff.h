/*
 * dw_handoff.h - which map / gametype is a room about to play, and the room-message records that
 * tell the client. Port of handoff/map_handoff.py (select and its latches) and
 * handoff/startgame_map.py (the dwRoomInfo / JoinResponse records and the push-7 envelope).
 * Internal to the room modules (dw_rooms / dw_matchmaking / dw_devpanel).
 */
#pragma once
#include <set>
#include "dw.h"
#include "dw_state.h"

namespace dw {

/* The one selection record (map_handoff.select). */
struct Selection {
    std::string map;        /* level zone: mp_killhouse_sh */
    std::string sessionMap; /* playlist spelling, what the client's dwRoomInfo gets: coop_training_mp_killhouse_sh */
    std::string gametype;   /* rules name: war / combat / coop / zm */
    std::string type;       /* zone category the Python lobby announces: mp / coop / zm */
    std::string category;   /* the embedded host's category: mp / coop / zm / survival */
    int playlist;           /* -1 = none */
    std::string playlistName, source, playerName;
    bool resolved, rotated;
    int rotationIndex;      /* -1 = not a round-robin pick */
    std::vector<std::string> chain; /* every resolution step, for the log */
    Selection() : playlist(-1), resolved(false), rotated(false), rotationIndex(-1) {}
    std::string Summary() const;
};

struct RoomInfo { /* the head of a dwRoomInfo the client broadcast (typeId 15) */
    std::string gametype, map, hostName;
    int flag0c, flag0d, maxPlayers;
    RoomInfo() : flag0c(0), flag0d(0), maxPlayers(0) {}
};

struct HandoffState;
HandoffState *HandoffNew();
void HandoffFree(HandoffState *h);

/* desc = the live session descriptor, or NULL. Never fails: the last resort is the killhouse pin. */
Selection HandoffSelect(Server &s, const Vals *desc);
/* Every 21/1 and 21/12 request passes here: the newest full descriptor seen on ANY connection is what
 * select() falls back to when it is handed none (the Python lobby re-reads its own log for it). */
void HandoffObserveDescriptor(Server &s, int task, const Vals &args);
/* The room's own pick, broadcast in a typeId-15 dwRoomInfo. -> true when the latch changed. */
bool HandoffLatchRoom(Server &s, const std::string &map, const std::string *gametype, int maxPlayers, const std::string &why);
/* -> true and the latched room gametype when a live room latch holds one. */
bool HandoffRoomLatchGametype(Server &s, std::string &gametype);
/* Advance the playlist round-robin after a start (READY press only). -> note or "". */
std::string HandoffRotationAdvance(Server &s, const Selection &sel);
/* The zone-availability check: <Config::zoneDir>/<name>.ff, else the shipped zone manifest. */
bool HandoffZoneInstalled(Server &s, const std::string &name);
/* map / session_map / type / gametype are bare identifiers (map_handoff.unsafe_selection). */
bool HandoffSelectionSafe(const Selection &sel, std::string *why);
/* desc[3]: the gamertag the client put in its own descriptor, "" when none. */
std::string DescPlayerName(const Vals *desc);
std::string BaseMap(const std::string &playlistMap);

/* ---- room-message records (LSB-first bit packing, one leading bool bit, no type tags) ---- */
struct MsgWriter {
    Bytes buf;
    size_t pos;
    MsgWriter() : pos(0) {}
    void Bits(uint64_t value, int n);
    void Raw(const Bytes &data);
    void Cstr(const std::string &s);
};
struct MsgReader {
    const Bytes &b;
    size_t pos;
    bool eof; /* a read ran off the end (Python: EOFError) */
    MsgReader(const Bytes &data) : b(data), pos(0), eof(false) {}
    uint64_t Bits(int n);
    std::string Cstr();
    size_t Left() const { return b.size() * 8 - pos; }
};

/* typeId 15: [u32 len][body]. Only a non-host client reads it (guard: amHost == 0). */
Bytes BuildRoomInfoRecord(const std::string &map, const std::string &gametype, int flag0c = 0xFF);
/* typeId 5 with an EMPTY roster: the same dwRoomInfo, accepted by a host client. */
Bytes BuildJoinResponseRecord(const std::string &map, const std::string &gametype);
/* The push-7 envelope: [2][u32 7][u64 sender][name b64][u64 roomId][blob rec][u32 trailer]. */
Bytes MapPushPayload(const Bytes &rec, uint64_t roomId, uint64_t sender, const std::string &senderName, uint32_t trailer);
std::vector<RoomInfo> ScanRoomInfo(const Bytes &blob);
/* The room-message typeIds inside a service-11/task-4 batch (manifold aware). */
std::set<int> RoomBatchTypeIds(const Bytes &blob);

} // namespace dw

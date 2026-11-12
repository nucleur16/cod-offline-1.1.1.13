/*
 * dw_state.h - shared server state and the internal interfaces between the module files.
 * Internal to offline/; the embedder includes dw_api.h, tests include dw.h.
 *
 * One module = one dw_<name>.cpp, which owns its own state struct (hung off ServerState) and
 * provides <Name>Init / <Name>Free. A function another module calls is declared HERE and gets a
 * placeholder in dw_stubs.cpp under that module's DW_HAVE_<NAME> guard.
 */
#pragma once
#include "dw.h"
#include "dw_json.h"

namespace dw {

struct EmbeddedFile {
    const char *name;
    const unsigned char *data;
    unsigned size;        /* unpacked size */
    unsigned packedSize;  /* bytes at `data` */
    int packed;           /* 1 = zlib stream */
    long long mtime;      /* reported as the pubfile modifiedTime */
};
extern const EmbeddedFile kEmbedded[];
extern const int kEmbeddedCount;

class Store;             /* dw_store.h */
struct RoomsState;       /* dw_rooms.cpp */
struct SessionState;     /* dw_session.cpp */
struct ProfilesState;    /* dw_profiles.cpp */
struct MarketState;      /* dw_marketplace.cpp */
struct CareerState;      /* dw_career.cpp */

struct ServerState {
    std::map<std::pair<int, uint64_t>, std::vector<Bytes> > chat; /* (channelType, channelId) -> payloads */
    Store *store;
    RoomsState *rooms;
    SessionState *session;
    ProfilesState *profiles;
    MarketState *market;
    CareerState *career;
    ServerState() : store(NULL), rooms(NULL), session(NULL), profiles(NULL), market(NULL), career(NULL) {}
};

/* ---- module lifecycle (StateInit calls these in this order; StateFree in reverse) ---- */
void StoreInit(Server &s);     void StoreFree(Server &s);
void ProfilesInit(Server &s);  void ProfilesFree(Server &s);
void CareerInit(Server &s);    void CareerFree(Server &s);
void MarketInit(Server &s);    void MarketFree(Server &s);
void RoomsInit(Server &s);     void RoomsFree(Server &s);

/* ---- dw_storage.cpp (demonware/dw_storage.py + the uid latch of dw-capture.py) ---- */
void OnUidLatched(Server &s, Conn &c);

/* ---- dw_profiles.cpp (store/player_profiles.py) ---- */
/* Render players/<uid>.json into the store's "mpdata_s" storage file. -> note, "" when current. */
std::string EnsureBlob(Server &s, uint64_t uid);
/* The player's level (1-based) as the marketplace gates on it (dw_marketplace._player_level). */
int PlayerLevel(Server &s, uint64_t uid);

/* ---- dw_career.cpp (career/rank_writeback.py) ---- */
/* Fold any waiting match_report.json into the profiles. uid 0 = no per-uid target. -> note or "". */
std::string RankWritebackMaybeApply(Server &s, uint64_t uid, bool background);

/* ---- dw_session.cpp (demonware/dw_session.py) ---- */
const Vals *Desc(Server &s, Conn *c, bool ownOnly = false);
void SetDesc(Server &s, Conn *c, const Vals &vals);
uint64_t DescUidOf(const Vals *desc);
uint64_t DescPlayerUid(Server &s, Conn *c);
std::string LivePlayerName(Server &s, Conn *c);
uint64_t ConnUid(Server &s, Conn &c);
std::string ConnName(Server &s, Conn &c);
std::string SeckeyObserve(Server &s, Conn *c, const Bytes &key);
std::string SeckeyStartgameWrite(Server &s, Conn *c, const Bytes &key);
void MatchRelease(Server &s, Conn &c);

/* ---- dw_rooms.cpp (demonware/dw_rooms.py) ---- */
void RoomLeaveAll(Server &s, Conn &c);

/* ---- dw_devpanel.cpp (demonware/dw_devpanel.py): the in-room developer panel ---- */
Reply DevPanelInstantMessage(Server &s, Conn &c, int task, const Vals &args);

} // namespace dw

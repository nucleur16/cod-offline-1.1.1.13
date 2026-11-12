/*
 * dw_state.h - shared server state and the internal interfaces between the service files.
 * Internal to offline/; the embedder includes dw_api.h, tests include dw.h.
 */
#pragma once
#include "dw.h"

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

struct RoomsState;       /* dw_rooms.cpp */
struct StoreState;       /* dw_store.cpp */
struct MarketState;      /* dw_marketplace.cpp */
struct CareerState;      /* dw_career.cpp */

struct ServerState {
    std::map<std::pair<int, uint64_t>, std::vector<Bytes> > chat; /* (channelType, channelId) -> payloads */
    bool hasSessionDesc;
    Vals sessionDesc;    /* the most recent createSession descriptor (Python _session["desc"]) */
    int seckeyConnGen;
    bool seckeySuspect;
    RoomsState *rooms;
    StoreState *store;
    MarketState *market;
    CareerState *career;
    ServerState() : hasSessionDesc(false), seckeyConnGen(0), seckeySuspect(false), rooms(NULL), store(NULL), market(NULL), career(NULL) {}
};

/* dw-capture.py build_task_reply: the account row / admission hooks at the uid latch. */
void OnUidLatched(Server &s, Conn &c);
/* demonware/dw_devpanel.py: the in-room developer panel's instant messages (service 6). */
Reply DevPanelInstantMessage(Server &s, Conn &c, int task, const Vals &args);

/* demonware/dw_session.py */
const Vals *Desc(Server &s, Conn *c, bool ownOnly = false);
void SetDesc(Server &s, Conn *c, const Vals &vals);
uint64_t DescUidOf(const Vals *desc);
uint64_t DescPlayerUid(Server &s, Conn *c);
std::string LivePlayerName(Server &s, Conn *c);
uint64_t ConnUid(Server &s, Conn &c);
std::string ConnName(Server &s, Conn &c);
std::string SeckeyObserve(Server &s, Conn *c, const Bytes &key);
std::string SeckeyStartgameWrite(Server &s, Conn *c, const Bytes &key);

} // namespace dw

/*
 * dw.h - the offline Demonware lobby core (docs/OFFLINE_LOBBY.md).
 *
 * A C++ port of the CODOL-EMU 1.1.1.13 Demonware emulator (Python). Everything under offline/ is
 * independent of C1 and of Kernel33: it is compiled into Kernel33.dll (through the C boundary in
 * dw_api.h) and into the standalone test server tests/offline/dwtest.exe, which the differential
 * tests run against the Python lobby.
 *
 * Built like the repo's other C++ (build-dev.sh): -fno-exceptions -fno-rtti, static libstdc++.
 * No exception or C++ object crosses into C code; the embedder talks to dw_api.h only.
 *
 * Threading: one Server is driven by ONE thread (the listener thread of dw_net.cpp, or a test).
 * The embedder reaches it only through Host callbacks, which run on that thread, and through the
 * thread-safe mailbox calls documented in dw_api.h.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <map>
#include <string>
#include <vector>

void DwTiger(const void *data, size_t n, unsigned char out[24]);
void DwDes3Cbc(const unsigned char key[24], const unsigned char iv[8], int decrypt,
               const unsigned char *in, unsigned char *out, size_t n);

namespace dw {

typedef std::string Bytes; /* binary-safe */

/* ---- protocol constants (demonware/dw_wire.py) ---- */
enum {
    LOBBY_SERVICE_TASK_REPLY = 1,
    LOBBY_SERVICE_PUSH_MESSAGE = 2,
    BD_NO_ERROR = 0,
    BD_NO_FILE = 1000,
    DECLINED = -1, /* a handler that does not know the task: Python's `None` error */
};
enum { /* bdByteBuffer type tags: the client's own enum positions */
    T_NONE = 0, T_BOOL = 1, T_CHAR8 = 2, T_UCHAR8 = 3, T_WCHAR16 = 4, T_INT16 = 5, T_UINT16 = 6, T_INT32 = 7,
    T_UINT32 = 8, T_INT64 = 9, T_UINT64 = 10, T_FLOAT32 = 13, T_FLOAT64 = 14, T_STRING = 16, T_STRING17 = 17,
    T_MBSTRING = 18, T_BLOB = 19, T_U32ARRAY = 0x6C, T_UNKNOWN = -1,
};

/* One parsed tagged value. Strings and blobs keep the wire bytes (Python decodes strings as latin1,
 * which is byte transparent). T_UNKNOWN holds the rest of the body in `s`. */
struct Val {
    int tag;
    uint64_t u;
    Bytes s;
    std::vector<uint32_t> arr;
    Val() : tag(T_NONE), u(0) {}
    Val(int t, uint64_t v) : tag(t), u(v) {}
    Val(int t, const Bytes &b) : tag(t), u(0), s(b) {}
    bool isStr() const { return tag == T_STRING || tag == T_STRING17 || tag == T_MBSTRING; }
};
typedef std::vector<Val> Vals;

Vals ParseValues(const Bytes &body, uint8_t pad);
Bytes Reserialize(const Vals &vals, size_t from = 0, size_t to = (size_t)-1);
/* First value carrying one of the tags, or NULL (dw_wire._first). */
const Val *First(const Vals &vals, int tagA, int tagB = -100, int tagC = -100);
std::string FmtValues(const Vals &vals); /* log text only */

Bytes w_u8(unsigned v);
Bytes w_bool(bool v);
Bytes w_u16(unsigned v);
Bytes w_u32(uint32_t v);
Bytes w_i32(int32_t v);
Bytes w_u64(uint64_t v);
Bytes w_i64(int64_t v);
Bytes w_blob(const Bytes &data);
Bytes w_str(const Bytes &s, size_t maxlen = 64);
Bytes w_name(const Bytes &s, size_t maxlen = 64); /* base64, for dwLogOnAccount_DecodeString */
Bytes w_u32_raw(uint32_t v);
Bytes B64Name(const Bytes &plain);
Bytes Base64(const Bytes &raw);
bool Base64Decode(const Bytes &text, Bytes &out);
uint32_t Crc32(const void *data, size_t n); /* zlib.crc32 */
void Sha256(const void *data, size_t n, unsigned char out[32]);
std::string Hex(const void *data, size_t n);
std::string KeyFp(const Bytes &key); /* "sha256:<12 hex>" log-safe key fingerprint */
std::string Fmt(const char *fmt, ...) __attribute__((format(gnu_printf, 1, 2)));

Bytes EncryptFrame(const Bytes &message, uint32_t messageCount);
/* payload = [u8 encType][u32 mc][ciphertext] -> false when not an encrypted, decodable frame. */
bool DecryptClientFrame(const Bytes &payload, uint32_t &mc, int &service, Bytes &body);

/* ---- handler result ---- */
struct Reply {
    int err;                    /* DECLINED = not handled here */
    std::vector<Bytes> results; /* one serialized record each */
    std::string note;           /* log text; never on the wire */
    Reply() : err(DECLINED) {}
    Reply(int e, const std::string &n) : err(e), note(n) {}
    static Reply Ok(const std::string &n = "") { return Reply(BD_NO_ERROR, n); }
};

/* ---- embedder interface ---- */
struct LocalRoute {
    bool enabled;     /* the owner ticked Local Play for the current room */
    int port;         /* UDP port of the embedded host on 127.0.0.1 */
    std::string map, sessionMap, gametype, category;
    LocalRoute() : enabled(false), port(0) {}
};
struct StartInfo { /* what a room start resolved to */
    uint64_t uid;
    std::string playerName, map, sessionMap, gametype, zoneType;
    std::string hostIp;
    int hostPort;
    Bytes securityKey; /* the client's 16-byte bdSecurityKey */
    bool local;
    StartInfo() : uid(0), hostPort(0), local(false) {}
};
struct Host {
    virtual ~Host() {}
    virtual void Log(const std::string &line) { (void)line; }
    virtual int64_t Now();    /* Unix seconds */
    virtual double Mono();    /* monotonic seconds */
    /* -> false when the embedder has no local host (the standalone server). */
    virtual bool GetLocalRoute(uint64_t uid, LocalRoute &out) { (void)uid; (void)out; return false; }
    /* A room start was answered with START_GAME. Runs on the server thread: copy and return. */
    virtual void OnStartGame(const StartInfo &info) { (void)info; }
};

struct Config {
    std::string resources;   /* optional override folder for served/embedded resources */
    std::string profile;     /* the persistent store (players, storage, accounts) */
    std::string state;       /* securitykey.bin, game_admissions.txt (shared with the match server) */
    std::string matchProfile;/* the match server's codol_profilePath: mpdata_s.<uid>, match_report.json */
    std::string zoneDir;     /* the client's zone folder (map availability); may be empty */
    std::string mmHostIp;
    int mmHostPort;
    uint64_t mmHostUid;
    int64_t fixedTime;       /* != 0: getServerTime and every clock read answer this (tests) */
    int stunPort, stunAltPort; /* the changed-address STUN replies advertise */
    int stunMappedPort;      /* the "your address" port in STUN replies (the Python lobby: always 3074) */
    bool differential;       /* reproduce the Python lobby's defaults exactly (tests) */
    Config();
};

struct Server;
struct Room;

struct Push { Bytes msg; std::string note; };

/* One TCP connection (Python: the `state` dict of serve()). */
struct Conn {
    int idx;
    Server *server;
    Bytes inbuf, outbuf;
    bool preambleSeen, closing;
    uint32_t outMc;
    uint64_t txn;
    uint64_t uid;          /* latched from the first STORAGE GETFILE ownerId */
    std::string name;
    bool hasDesc;
    Vals desc;             /* this connection's createSession descriptor */
    Bytes seckey;
    std::vector<Push> push;
    double opened, lastRx, lastTx;
    std::map<std::string, int64_t> num;      /* per-service scalars (Python state[...] ints) */
    std::map<std::string, std::string> str;  /* per-service strings */
    uint64_t room;         /* 0 = none */
    Conn();
    void Queue(const Bytes &msg, const std::string &note);
};

/* A service handler. `task` is the UChar8 task id, `args` the values after it. */
typedef Reply (*TaskFn)(Server &s, Conn &c, int task, const Vals &args);

struct ServerState; /* dw_state.h: rooms, store, career caches */

struct Server {
    Config cfg;
    Host *host;
    ServerState *st;
    std::map<int, Conn *> conns;
    int nextConn;
    Server(const Config &config, Host *h);
    ~Server();

    Conn *Open();
    void Close(Conn *c);
    /* Bytes from the client; replies and pushes accumulate in every connection's outbuf. */
    void Feed(Conn *c, const void *data, size_t n);
    /* Idle work: keepalives, deferred pushes, the match-report poll. Call about once a second. */
    void Tick();
    void FlushPushes(Conn *c);

    int64_t Now() const;
    void Log(const std::string &line) const;
    /* A served resource: <cfg.resources>/<name> when present, else the embedded copy. */
    bool Resource(const std::string &name, Bytes &out) const;
    bool ResourceExists(const std::string &name) const;
    int64_t ResourceMtime(const std::string &name) const;
};

/* STUN (bdIPDiscovery / bdNATTypeDiscovery): -> false when `data` is not a request we answer.
 * `via` 0 = primary socket, 2 = changed port, 3 = changed IP and port. */
bool StunReply(const Config &cfg, const Bytes &data, Bytes &out, int &via, std::string &what);

/* ---- files ---- */
bool ReadFile(const std::string &path, Bytes &out);
bool WriteFileAtomic(const std::string &path, const Bytes &data);
bool FileExists(const std::string &path);
int64_t FileMtime(const std::string &path);
void MakeDirs(const std::string &path);
bool RemoveFile(const std::string &path);
std::string PathJoin(const std::string &a, const std::string &b);
std::vector<std::string> ListDir(const std::string &dir);
bool Inflate(const Bytes &in, Bytes &out);              /* zlib stream */
bool Deflate(const Bytes &in, Bytes &out, int level);

/* ---- services (one file each) ---- */
Reply StorageTask(Server &s, Conn &c, int task, const Vals &args);        /* 10 */
Reply RoomTask(Server &s, Conn &c, int task, const Vals &args);           /* 11 */
Reply TitleUtilitiesTask(Server &s, Conn &c, int task, const Vals &args); /* 12 */
Reply MatchmakingTask(Server &s, Conn &c, int task, const Vals &args);    /* 21 */
Reply MessagingTask(Server &s, Conn &c, int task, const Vals &args);      /* 6 */
Reply TencentTask(Server &s, Conn &c, int task, const Vals &args);        /* 71 */
Reply MarketplaceTask(Server &s, Conn &c, int task, const Vals &args);    /* 80 */
Reply StatsTask(Server &s, Conn &c, int task, const Vals &args);          /* 4 */
Reply ChatTask(Server &s, Conn &c, int task, const Vals &args);           /* 102 */

/* Hooks the dispatcher calls around every task (dw-capture.py build_task_reply). */
void AfterTask(Server &s, Conn &c, int service, int task);   /* inventory / ticker pushes */
void OnConnClosed(Server &s, Conn &c);                       /* room leave, match release */
void OnLobbyHandshake(Server &s, Conn &c);                   /* seckey conn_gen */
void ServiceTick(Server &s);                                 /* match-report poller */
void StateInit(Server &s);
void StateFree(Server &s);

} // namespace dw

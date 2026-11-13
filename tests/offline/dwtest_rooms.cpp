/*
 * dwtest --selftest-rooms: the offline product's room start (the non-differential match handoff
 * policy of src/offline/dw_rooms.cpp) against a fake embedder.
 *
 * For an mp room, a zombies room, a survival room and a coop room:
 *   create room -> createSession -> Ready with Local Play OFF  => only the echo relay comes back: no
 *       SET_MAP, no START_GAME, the embedder is not told to start anything;
 *   Ready again with Local Play ON => SET_MAP (typeId 5 and 15) name the room's map, START_GAME
 *       (typeId 3) addresses 127.0.0.1:<the embedded host's port>, OnStartGame runs exactly once with
 *       the right map / gametype / zone, and securitykey.bin + game_admissions.txt are in Config::state.
 *
 * Self-contained: without --resources / --state before the flag it works in a scratch folder under
 * %TEMP% with a three-line playlist file (the gametype names "coop" and "zm" come from that file).
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../../src/offline/dw.h"
#include "../../src/offline/dw_handoff.h"

namespace {

struct FakeHost : dw::Host {
    bool enabled, refuse;
    int port, starts, asked, launches;
    dw::StartInfo last;
    dw::LocalRoute intent; /* the owner's published Local Play intent (p2p_local_play_api.h) */
    std::string launched;  /* the last launch call's "session/gametype/type" */
    FakeHost() : enabled(false), refuse(false), port(28999), starts(0), asked(0), launches(0) {}
    virtual double Mono() { return GetTickCount64() / 1000.0; }
    virtual bool GetLocalRoute(uint64_t, dw::LocalRoute &out)
    {
        asked++;
        out = intent;
        out.enabled = enabled;
        out.port = port;
        return true;
    }
    virtual bool LocalPlayLaunch(uint64_t owner, const std::string &session, const std::string &gametype, const std::string &type)
    {
        launches++;
        launched = session + "/" + gametype + "/" + type;
        return !refuse && owner == intent.ownerUid;
    }
    virtual void OnStartGame(const dw::StartInfo &info)
    {
        starts++;
        last = info;
    }
};

int g_fail, g_checks;
void Check(bool ok, const char *room, const char *what)
{
    g_checks++;
    if (ok) return;
    g_fail++;
    printf("  FAIL [%s] %s\n", room, what);
}

/* One client request: [service][UChar8 task][tagged args], framed as the client frames it. */
void Send(dw::Server &server, dw::Conn *c, uint32_t &mc, int service, int task, const dw::Bytes &args)
{
    dw::Bytes msg(1, (char)service);
    msg += dw::w_u8((unsigned)task);
    msg += args;
    msg += '\0'; /* the trailing NoType every request carries */
    mc += 2;
    if ((mc & 0xFF) == 0) mc++; /* the pad byte must differ from that NoType */
    dw::Bytes frame = dw::EncryptFrame(msg, mc);
    server.Feed(c, frame.data(), frame.size());
}

struct Frame {
    int type; /* 1 = task reply, 2 = push */
    dw::Vals vals;
};

/* Everything the server wrote to the connection since the last call. */
std::vector<Frame> Drain(dw::Conn *c)
{
    std::vector<Frame> out;
    dw::Bytes &b = c->outbuf;
    size_t at = 0;
    while (at + 4 <= b.size()) {
        uint32_t n = 0;
        for (int i = 0; i < 4; i++) n |= (uint32_t)(unsigned char)b[at + (size_t)i] << (8 * i);
        if (at + 4 + n > b.size()) break;
        dw::Bytes payload = b.substr(at + 4, n), body;
        at += 4 + (size_t)n;
        uint32_t mc = 0;
        Frame f;
        if (n && dw::DecryptClientFrame(payload, mc, f.type, body)) {
            f.vals = dw::ParseValues(body, (uint8_t)(mc & 0xFF));
            out.push_back(f);
        }
    }
    b.clear();
    return out;
}

/* A push-7 room message: [u32 7][u64 sender][string name][u64 roomId][blob record][u32 trailer]. */
bool RoomPush(const Frame &f, int &typeId, dw::Bytes &body)
{
    if (f.type != 2 || f.vals.size() < 5 || f.vals[0].u != 7 || f.vals[4].tag != dw::T_BLOB || f.vals[4].s.size() < 6) return false;
    body = f.vals[4].s.substr(4);
    dw::MsgReader r(body);
    r.Bits(1);
    typeId = (int)r.Bits(8);
    return !r.eof;
}

dw::Bytes Descriptor(const char *name, uint64_t uid, uint64_t rid, const char *gametype, const char *map, const dw::Bytes &key)
{
    dw::Bytes d = dw::w_blob(dw::Bytes(37, '\x11')) + dw::w_u32(0) + dw::w_u32(6) + dw::w_str(name) + dw::w_str(name);
    static const int32_t head[7] = { 0, 0, 406, 0, 12345, 0, 255 }; /* mode, -, version, -, criterion, -, league 255 */
    for (int i = 0; i < 7; i++) d += dw::w_i32(head[i]);
    for (int i = 0; i < 7; i++) d += dw::w_i32(0);
    d += dw::w_blob(key) + dw::w_u64(uid) + dw::w_blob(dw::Bytes(56, '\0')) + dw::w_u64(rid);
    d += dw::w_i32((int32_t)dw::Crc32(gametype, strlen(gametype))) + dw::w_i32((int32_t)dw::Crc32(map, strlen(map)));
    d += dw::w_i32(0) + dw::w_i32(0) + dw::w_i32(0);
    return d;
}

struct Case {
    const char *room, *descGametype, *descMap; /* what the client's descriptor names */
    const char *map, *sessionMap, *gametype, *zone; /* what the embedded host must be started with */
    const char *roomMap, *roomGametype;             /* what the client's dwRoomInfo must be told */
};

void RunCase(const dw::Config &cfg, const Case &t, int n)
{
    FakeHost host;
    host.port = 28960 + n;
    dw::Server server(cfg, &host);
    dw::Conn *c = server.Open();
    uint32_t mc = 0x40;
    const uint64_t uid = 0x1122334455667700ULL + (uint64_t)n;
    c->uid = uid;
    dw::Bytes key(16, '\0');
    for (int i = 0; i < 16; i++) key[(size_t)i] = (char)(0xA0 + n * 16 + i);
    const std::string keyPath = dw::PathJoin(cfg.state, "securitykey.bin"), rosterPath = dw::PathJoin(cfg.state, "game_admissions.txt");
    dw::RemoveFile(keyPath);
    dw::RemoveFile(rosterPath);

    Send(server, c, mc, 11, 13, dw::Bytes()); /* CREATE ROOM */
    std::vector<Frame> f = Drain(c);
    uint64_t rid = f.size() == 1 && f[0].type == 1 && f[0].vals.size() > 5 ? f[0].vals[5].u : 0;
    Check(rid != 0, t.room, "CREATE ROOM answers a room id");

    Send(server, c, mc, 21, 1, Descriptor("Tester", uid, rid, t.descGametype, t.descMap, key));
    f = Drain(c);
    Check(f.size() == 1 && f[0].type == 1, t.room, "createSession answers");
    dw::Bytes onDisk;
    Check(dw::ReadFile(keyPath, onDisk) && onDisk == key, t.room, "createSession writes securitykey.bin (live-observed key)");
    dw::RemoveFile(keyPath); /* so that the start below has to write it */
    dw::RemoveFile(rosterPath);

    const dw::Bytes ready("\x0c\x00\x00\x00\x02\x02\x00\x00\x00\x08\x00\x00\x00\x80\x08\x00", 16);
    const dw::Bytes press = dw::w_u64(rid) + dw::w_blob(ready) + dw::w_u32(0);

    /* 1. Local Play off: nothing to join. */
    host.enabled = false;
    Send(server, c, mc, 11, 4, press);
    f = Drain(c);
    int pushes = 0, synth = 0, typeId = 0;
    dw::Bytes body;
    for (size_t i = 0; i < f.size(); i++)
        if (f[i].type == 2) {
            pushes++;
            if (RoomPush(f[i], typeId, body) && (typeId == 3 || typeId == 5 || typeId == 15)) synth++;
        }
    Check(f.size() == 2 && f[0].type == 1 && pushes == 1, t.room, "route off: the reply and the echo relay only");
    Check(synth == 0, t.room, "route off: no SET_MAP / START_GAME push");
    Check(host.starts == 0 && host.asked == 1, t.room, "route off: the embedder is asked once and not started");
    Check(!dw::FileExists(keyPath), t.room, "route off: securitykey.bin is not written");
    Check(c->room == rid, t.room, "route off: the connection is still in its room");

    /* 1b. Ticked, but the intent does not allow this start (docs/P2P_DEFERRED_HOST_START.md rules
     * 2, 3 and a refused launch): START_GAME is withheld each time and nothing is launched early. */
    host.enabled = true;
    host.intent.ownerUid = uid;
    host.intent.armed = true;
    host.intent.state = "armed";
    host.intent.map = t.map;
    host.intent.sessionMap = t.sessionMap;
    host.intent.gametype = t.gametype;
    host.intent.category = t.zone;
    struct Withheld {
        static int Pushes(dw::Server &server, dw::Conn *c, uint32_t &mc, const dw::Bytes &press)
        {
            Send(server, c, mc, 11, 4, press);
            std::vector<Frame> f = Drain(c);
            int synth = 0, typeId = 0;
            dw::Bytes body;
            for (size_t i = 0; i < f.size(); i++)
                if (f[i].type == 2 && RoomPush(f[i], typeId, body) && (typeId == 3 || typeId == 5 || typeId == 15)) synth++;
            return synth;
        }
    };
    host.intent.failed = true;
    host.intent.state = "failed";
    Check(Withheld::Pushes(server, c, mc, press) == 0 && host.launches == 0 && host.starts == 0, t.room, "a failed host choice withholds START_GAME");
    host.intent.failed = false;
    host.intent.state = "idle";
    Check(Withheld::Pushes(server, c, mc, press) == 0 && host.launches == 0, t.room, "an idle intent withholds START_GAME");
    host.intent.state = "armed";
    host.intent.sessionMap = "mp_other_map";
    host.intent.map = "mp_other_map";
    Check(Withheld::Pushes(server, c, mc, press) == 0 && host.launches == 0, t.room, "another armed selection withholds START_GAME");
    host.intent.sessionMap = t.sessionMap;
    host.intent.map = t.map;
    host.intent.gametype = "sab";
    Check(Withheld::Pushes(server, c, mc, press) == 0 && host.launches == 0, t.room, "another armed gametype withholds START_GAME");
    host.intent.gametype = t.gametype;
    host.intent.ownerUid = uid + 1;
    Check(Withheld::Pushes(server, c, mc, press) == 0 && host.launches == 0, t.room, "a guest of an armed, unlaunched host is withheld");
    host.intent.ownerUid = uid;
    host.refuse = true;
    Check(Withheld::Pushes(server, c, mc, press) == 0 && host.launches == 1 && host.starts == 0, t.room, "a refused launch withholds START_GAME");
    host.refuse = false;
    dw::RemoveFile(keyPath);
    dw::RemoveFile(rosterPath);

    /* 2. Armed for exactly this room: the same press hands the match to the owner and launches. */
    Send(server, c, mc, 11, 4, press);
    f = Drain(c);
    int nStart = 0, nJoinResp = 0, nRoomInfo = 0, nRelay = 0;
    for (size_t i = 0; i < f.size(); i++) {
        if (f[i].type != 2 || !RoomPush(f[i], typeId, body)) continue;
        dw::MsgReader r(body);
        r.Bits(9);
        if (typeId == 3) {
            nStart++;
            Check(nJoinResp == 1 && nRoomInfo == 1, t.room, "SET_MAP is pushed before START_GAME");
            dw::Bytes addr;
            uint32_t len = (uint32_t)r.Bits(32);
            for (uint32_t k = 0; k < len && !r.eof; k++) addr += (char)r.Bits(8);
            Check(len == 37 && addr.size() == 37, t.room, "START_GAME: the first blob is a 37-byte bdCommonAddr");
            if (addr.size() == 37) {
                const unsigned char *a = (const unsigned char *)addr.data();
                bool slot0 = a[0] == 127 && a[1] == 0 && a[2] == 0 && a[3] == 1 && (a[4] | (a[5] << 8)) == host.port;
                bool slot5 = a[30] == 127 && a[31] == 0 && a[32] == 0 && a[33] == 1 && (a[34] | (a[35] << 8)) == host.port;
                Check(slot0 && slot5 && a[36] == 1, t.room, "START_GAME addresses 127.0.0.1:<embedded host port> (slots 0 and 5, natType 1)");
            }
            uint32_t sidLen = (uint32_t)r.Bits(32);
            for (uint32_t k = 0; k < sidLen && !r.eof; k++) r.Bits(8);
            uint32_t keyLen = (uint32_t)r.Bits(32);
            dw::Bytes k2;
            for (uint32_t k = 0; k < keyLen && !r.eof; k++) k2 += (char)r.Bits(8);
            uint32_t tailLen = (uint32_t)r.Bits(32);
            Check(sidLen == 8 && k2 == key && tailLen == 57, t.room, "START_GAME carries the session id, the client's key and the 57-byte tail");
            Check(f[i].vals[1].u == 69420 && f[i].vals[3].u == rid, t.room, "START_GAME envelope: sender 69420, this room");
        } else if (typeId == 5 || typeId == 15) {
            if (typeId == 5) {
                nJoinResp++;
                Check(r.Bits(32) == 0 && r.Bits(32) == 0, t.room, "JoinResponse: code 0, empty roster");
            } else {
                nRoomInfo++;
            }
            std::string gt = r.Cstr(), map = r.Cstr();
            Check(gt == t.roomGametype && map == t.roomMap, t.room, "SET_MAP names the room's gametype and session map");
        } else {
            nRelay++;
        }
    }
    Check(nStart == 1 && nJoinResp == 1 && nRoomInfo == 1 && nRelay == 1, t.room, "route on: relay, SET_MAP x2, START_GAME x1");
    Check(host.starts == 1, t.room, "route on: OnStartGame runs exactly once");
    Check(host.launches == 2 && host.launched == std::string(t.sessionMap) + "/" + t.gametype + "/" + t.zone, t.room,
          "route on: the launch is raised once, for the armed selection");
    if (host.starts == 1) {
        const dw::StartInfo &i = host.last;
        Check(i.map == t.map && i.sessionMap == t.sessionMap && i.gametype == t.gametype && i.zoneType == t.zone, t.room,
              "OnStartGame: map / session map / gametype / zone");
        Check(i.roomMap == t.roomMap && i.roomGametype == t.roomGametype, t.room, "OnStartGame: the room strings");
        Check(i.local && i.hostIp == "127.0.0.1" && i.hostPort == host.port, t.room, "OnStartGame: the local host address");
        Check(i.uid == uid && i.playerName == "Tester" && i.securityKey == key, t.room, "OnStartGame: uid, name, security key");
        if (!(i.map == t.map && i.gametype == t.gametype && i.zoneType == t.zone))
            printf("       got map=%s session=%s gametype=%s zone=%s\n", i.map.c_str(), i.sessionMap.c_str(), i.gametype.c_str(), i.zoneType.c_str());
    }
    dw::Bytes roster;
    Check(dw::ReadFile(keyPath, onDisk) && onDisk == key, t.room, "the start writes securitykey.bin");
    Check(dw::ReadFile(rosterPath, roster) && roster.find(dw::Hex(key.data(), 16)) != dw::Bytes::npos, t.room,
          "the start puts the seat's key on game_admissions.txt");
    /* 3. The host answers with this selection (rule 4): a later press joins it. */
    host.intent.armed = false;
    host.intent.launched = host.intent.ready = true;
    host.intent.state = "ready";
    Check(Withheld::Pushes(server, c, mc, press) == 3 && host.starts == 2, t.room, "a ready host is joined on 127.0.0.1");
    server.Close(c);
}

} // namespace

int DwRoomsSelfTest(const dw::Config &given)
{
    dw::Config cfg = given;
    cfg.differential = false;
    char tmp[MAX_PATH];
    GetTempPathA(sizeof(tmp), tmp);
    const std::string scratch = dw::PathJoin(tmp, dw::Fmt("dwtest_rooms_%lu", (unsigned long)GetCurrentProcessId()));
    if (cfg.state.empty()) cfg.state = dw::PathJoin(scratch, "state");
    dw::MakeDirs(cfg.state);
    if (cfg.resources.empty()) {
        cfg.resources = dw::PathJoin(scratch, "resources");
        dw::MakeDirs(cfg.resources);
        dw::WriteFileAtomic(dw::PathJoin(cfg.resources, "mp_playlists_shipRetail_cb2.info"),
                            "version 406\nplaylist 30 league 1\ncoop_training_mp_killhouse_sh,coop,1\n"
                            "playlist 94 league 1\nzm_asylum_sh,zm,1\nzm_prototype_sh,zm,1\n");
    }
    static const Case cases[] = {
        { "mp", "war", "mp_rust", "mp_rust", "mp_rust", "war", "mp", "mp_rust", "war" },
        { "zm", "zm", "zm_asylum_sh", "zm_asylum_sh", "zm_asylum_sh", "zm", "zm", "zm_asylum_sh", "zm" },
        { "survival", "coop", "coop_survival_mp_afghan", "mp_afghan", "mp_afghan", "survival", "survival", "coop_survival_mp_afghan", "coop" },
        { "coop", "coop", "coop_training_mp_killhouse_sh", "mp_killhouse_sh", "coop_training_mp_killhouse_sh", "coop", "coop",
          "coop_training_mp_killhouse_sh", "coop" },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) RunCase(cfg, cases[i], (int)i);
    printf("%s (%d checks, %d failed)\n", g_fail ? "SELFTEST-ROOMS FAIL" : "SELFTEST-ROOMS PASS", g_checks, g_fail);
    return g_fail ? 1 : 0;
}

/*
 * dwtest --selftest-rooms: the offline product's room start (the non-differential match handoff
 * policy of src/offline/dw_rooms.cpp) against a fake embedder, and the room selection it starts from.
 *
 * Every room the local player owns is hosted by the embedded server; nothing is ticked. For an mp
 * room (and one whose mode the UI spells "tdm"), a zombies room, a survival room and a coop room:
 *   create room -> createSession     => the embedder is told the room's selection (armed, no launch);
 *   Ready                            => SET_MAP (typeId 5 and 15) name the room's map, START_GAME
 *       (typeId 3) addresses 127.0.0.1:<the embedded host's port>, the launch is raised once for the
 *       selection, OnStartGame runs exactly once, securitykey.bin + game_admissions.txt are written;
 *   Ready while the embedder refuses (a host that already ran, an unknown map, ...) or refuses the
 *       launch => no SET_MAP, no START_GAME, a ticker line with the reason, the room stays usable;
 *   the owner's own room context (p2p_room) beats the lobby's resolution; another player's does not;
 *   a guest of the room is handed off only to the owner's running host of this selection;
 *   the owner leaves             => the embedder is told (nothing stays armed).
 *
 * Self-contained: without --resources / --state before the flag it works in a scratch folder under
 * %TEMP% with a three-line playlist file (the gametype names "coop" and "zm" come from that file).
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../../src/offline/dw.h"
#include "../../src/offline/dw_handoff.h"
#include "../../src/offline/dw_rooms.h"

namespace {

struct FakeHost : dw::Host {
    bool refuse, refuseLaunch, haveIntent;
    int port, starts, asked, arms, launches, leaves;
    dw::StartInfo last;
    dw::LocalRoute intent; /* Kernel33's published snapshot (p2p_local_play_api.h): the owner's room context and host state */
    std::string told;      /* the last LocalPlayStart selection: "session/gametype/type/difficulty" */
    uint64_t toldOwner, leftOwner;
    FakeHost() : refuse(false), refuseLaunch(false), haveIntent(false), port(28999), starts(0), asked(0), arms(0), launches(0), leaves(0),
                 toldOwner(0), leftOwner(0) {}
    virtual double Mono() { return GetTickCount64() / 1000.0; }
    virtual bool GetLocalRoute(uint64_t, dw::LocalRoute &out)
    {
        asked++;
        if (!haveIntent) return false;
        out = intent;
        out.enabled = true;
        out.port = port;
        return true;
    }
    virtual bool LocalPlayStart(uint64_t owner, const std::string &session, const std::string &gametype, const std::string &type, int difficulty,
                                bool launch, int &outPort, std::string &error)
    {
        (launch ? launches : arms)++;
        told = session + "/" + gametype + "/" + type + dw::Fmt("/%d", difficulty);
        toldOwner = owner;
        if (refuse || (launch && refuseLaunch)) {
            error = "Restart the client before hosting another Local Play match.";
            return false;
        }
        outPort = port;
        return true;
    }
    virtual void LocalPlayLeave(uint64_t owner)
    {
        leaves++;
        leftOwner = owner;
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

dw::Bytes Descriptor(const char *name, uint64_t uid, uint64_t rid, const char *gametype, const char *map, const dw::Bytes &key,
                     int difficulty = 0)
{
    dw::Bytes d = dw::w_blob(dw::Bytes(37, '\x11')) + dw::w_u32(0) + dw::w_u32(6) + dw::w_str(name) + dw::w_str(name);
    static const int32_t head[7] = { 0, 0, 406, 0, 12345, 0, 255 }; /* mode, -, version, -, criterion, -, league 255 */
    for (int i = 0; i < 7; i++) d += dw::w_i32(head[i]);
    for (int i = 0; i < 7; i++) d += dw::w_i32(0);
    d += dw::w_blob(key) + dw::w_u64(uid) + dw::w_blob(dw::Bytes(56, '\0')) + dw::w_u64(rid);
    d += dw::w_i32((int32_t)dw::Crc32(gametype, strlen(gametype))) + dw::w_i32((int32_t)dw::Crc32(map, strlen(map)));
    d += dw::w_i32(0) + dw::w_i32(difficulty) + dw::w_i32(0); /* desc[25], desc[26] = the Zombies difficulty, desc[27] */
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

    const std::string want = std::string(t.sessionMap) + "/" + t.gametype + "/" + t.zone + (strcmp(t.zone, "zm") ? "/-1" : "/0");
    Check(host.arms == 1 && host.launches == 0 && host.told == want && host.toldOwner == uid, t.room,
          "createSession: the embedder is told the owner's room selection, without a launch");
    if (host.told != want) printf("       told %s, want %s\n", host.told.c_str(), want.c_str());

    const dw::Bytes ready("\x0c\x00\x00\x00\x02\x02\x00\x00\x00\x08\x00\x00\x00\x80\x08\x00", 16);
    const dw::Bytes press = dw::w_u64(rid) + dw::w_blob(ready) + dw::w_u32(0);
    struct Pressed { /* what one Ready press pushed back */
        int synth, ticker;
        std::string tickerText;
        static Pressed Press(dw::Server &server, dw::Conn *c, uint32_t &mc, const dw::Bytes &press)
        {
            Send(server, c, mc, 11, 4, press);
            std::vector<Frame> f = Drain(c);
            Pressed out;
            out.synth = out.ticker = 0;
            int typeId = 0;
            dw::Bytes body;
            for (size_t i = 0; i < f.size(); i++) {
                if (f[i].type != 2) continue;
                if (RoomPush(f[i], typeId, body) && (typeId == 3 || typeId == 5 || typeId == 15)) out.synth++;
                if (!f[i].vals.empty() && f[i].vals[0].u == 35) {
                    out.ticker++;
                    for (size_t k = 0; k < f[i].vals.size(); k++)
                        if (f[i].vals[k].tag == dw::T_BLOB) out.tickerText = f[i].vals[k].s;
                }
            }
            return out;
        }
    };
    int typeId = 0;
    dw::Bytes body;

    /* 1. The embedder cannot host this selection (here: a host already ran in this client run). Nothing is
     * started, nothing else is substituted, and the player is told why. */
    host.refuse = true;
    Pressed no = Pressed::Press(server, c, mc, press);
    Check(no.synth == 0 && host.starts == 0 && host.launches == 0, t.room, "a refused selection withholds SET_MAP and START_GAME");
    Check(no.ticker == 1 && no.tickerText.find("<MSG P=1 T=") != dw::Bytes::npos &&
          no.tickerText.find("Match not started: Restart the client before hosting another Local Play match.") != dw::Bytes::npos, t.room,
          "a refused selection: the reason goes to the player's ticker");
    Check(!dw::FileExists(keyPath), t.room, "a refused selection: securitykey.bin is not written");
    Check(c->room == rid, t.room, "a refused selection: the connection is still in its room");
    host.refuse = false;
    host.refuseLaunch = true; /* accepted when armed, refused at the launch */
    no = Pressed::Press(server, c, mc, press);
    Check(no.synth == 0 && host.launches == 1 && host.starts == 0 && no.ticker == 1, t.room, "a refused launch withholds START_GAME");
    host.refuseLaunch = false;
    dw::RemoveFile(keyPath);
    dw::RemoveFile(rosterPath);
    const int launchesBefore = host.launches;

    /* 2. The normal press, with nothing ticked anywhere: the match is handed to the owner and launched. The owner
     * of a multiplayer room that names its own map is not sent the JoinResponse (it would empty the room's
     * roster, which the room view shows after the match); every other start is. */
    const int wantJoinResp = strcmp(t.zone, "mp") ? 1 : 0;
    Send(server, c, mc, 11, 4, press);
    f = Drain(c);
    int nStart = 0, nJoinResp = 0, nRoomInfo = 0, nRelay = 0;
    for (size_t i = 0; i < f.size(); i++) {
        if (f[i].type != 2 || !RoomPush(f[i], typeId, body)) continue;
        dw::MsgReader r(body);
        r.Bits(9);
        if (typeId == 3) {
            nStart++;
            Check(nJoinResp == wantJoinResp && nRoomInfo == 1, t.room, "SET_MAP is pushed before START_GAME");
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
            Check(gt == t.roomGametype && map == t.roomMap, t.room, "SET_MAP names the session map and the host's mode name");
        } else {
            nRelay++;
        }
    }
    Check(nStart == 1 && nJoinResp == wantJoinResp && nRoomInfo == 1 && nRelay == 1, t.room,
          wantJoinResp ? "start: relay, SET_MAP x2, START_GAME x1" : "start: relay, SET_MAP (room info only), START_GAME x1");
    Check(host.starts == 1, t.room, "start: OnStartGame runs exactly once");
    Check(host.launches == launchesBefore + 1 && host.told == want && host.toldOwner == uid, t.room, "start: the launch is raised once, for the room's selection");
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

    /* 3. The owner's CURRENT room context (what the room UI told Kernel33 the room shows) beats the lobby's own
     * resolution; another player's context is not a pin, and an incoherent one is ignored. */
    host.haveIntent = true;
    host.intent = dw::LocalRoute();
    host.intent.ownerUid = uid + 7;
    host.intent.map = host.intent.sessionMap = "mp_killhouse_sh";
    host.intent.gametype = "sd";
    host.intent.category = "mp";
    Pressed::Press(server, c, mc, press);
    Check(host.told == want, t.room, "another player's room context does not move this room");
    host.intent.ownerUid = uid;
    Pressed::Press(server, c, mc, press);
    Check(host.told == "mp_killhouse_sh/sd/mp/-1" && host.last.map == "mp_killhouse_sh" && host.last.roomGametype == "sd", t.room,
          "the owner's own room context is the selection that is hosted and announced");
    host.intent.map = "zm_asylum_sh"; /* an mp context on a zombies level is not coherent */
    Pressed::Press(server, c, mc, press);
    Check(host.told == want, t.room, "an incoherent room context is ignored");
    host.haveIntent = false;

    /* 4. A guest of the room: only the owner's host, and only once it runs this selection. */
    dw::Conn *g = server.Open();
    uint32_t gmc = 0x40;
    g->uid = uid + 100;
    Send(server, g, gmc, 11, 11, dw::w_u64(rid));
    Drain(g);
    const int armsBefore = host.arms, launchesNow = host.launches, startsNow = host.starts;
    Pressed guest = Pressed::Press(server, g, gmc, press);
    Check(guest.synth == 0 && host.arms == armsBefore && host.launches == launchesNow && guest.ticker == 1, t.room,
          "a guest is withheld while the owner's host does not run this selection, and never arms a host");
    host.haveIntent = true;
    host.intent = dw::LocalRoute();
    host.intent.ownerUid = uid;
    host.intent.launched = host.intent.ready = true;
    host.intent.state = "ready";
    host.intent.map = t.map;
    host.intent.sessionMap = t.sessionMap;
    host.intent.gametype = t.gametype;
    host.intent.category = t.zone;
    Send(server, g, gmc, 21, 1, Descriptor("Guest", g->uid, rid, t.descGametype, t.descMap, key));
    Drain(g);
    guest = Pressed::Press(server, g, gmc, press);
    Check(guest.synth == 3 && host.starts == startsNow + 1 && host.launches == launchesNow && host.arms == armsBefore, t.room,
          "a guest joins the owner's running host of this selection on 127.0.0.1, without a launch");
    host.haveIntent = false;
    server.Close(g);

    /* 5. The owner leaves: nothing stays armed. */
    Send(server, c, mc, 11, 2, dw::w_u64(rid));
    Drain(c);
    Check(host.leaves == 1 && host.leftOwner == uid, t.room, "the owner leaving its room tells the embedder");
    server.Close(c);
}

/* The selection of a room (offline/dw_handoff.cpp HandoffSelect), as ported from the Python lobby's
 * fix/hosted-survival-spawn-20261028: Survival as its own category on an MP level with placements,
 * usermaps in the installed set, the room latch's category, the Zombies difficulty. */
struct Pick {
    dw::Server server;
    dw::Conn *c;
    uint32_t mc;
    uint64_t rid;
    FakeHost *host;
    Pick(const dw::Config &cfg, FakeHost *h) : server(cfg, h), c(server.Open()), mc(0x40), rid(0), host(h)
    {
        c->uid = 0x1122334455667788ULL;
        Send(server, c, mc, 11, 13, dw::Bytes());
        std::vector<Frame> f = Drain(c);
        rid = f.size() == 1 && f[0].vals.size() > 5 ? f[0].vals[5].u : 0;
    }
    dw::Selection Select(const char *gametype, const char *map, int difficulty = 0)
    {
        Send(server, c, mc, 21, 1, Descriptor("Tester", c->uid, rid, gametype, map, dw::Bytes(16, '\x33'), difficulty));
        Drain(c);
        return dw::ResolveSelection(server, c, "join");
    }
};

void Touch(const std::string &path)
{
    size_t cut = path.find_last_of("/\\");
    if (cut != std::string::npos) dw::MakeDirs(path.substr(0, cut));
    dw::WriteFileAtomic(path, "x");
}

bool Is(const dw::Selection &s, const char *map, const char *session, const char *gametype, const char *type)
{
    bool ok = s.map == map && s.sessionMap == session && s.gametype == gametype && s.type == type && s.category == type;
    if (!ok) printf("       got %s\n", s.Summary().c_str());
    return ok;
}

void SelectionChecks(const dw::Config &base, const std::string &scratch)
{
    const char *T = "select";
    FakeHost host;
    {   /* no game install known: every supported Survival level counts (the standalone lobby) */
        Pick p(base, &host);
        Check(Is(p.Select("coop", "coop_survival_mp_cross_fire"), "mp_cross_fire", "mp_cross_fire", "survival", "survival"), T,
              "the coop_survival_<level> descriptor is Survival on that MP level");
        Check(Is(p.Select("coop", "coop_survival_mp_afghan"), "mp_fav_tropical", "mp_fav_tropical", "survival", "survival"), T,
              "a level without survival data plays the first supported level");
        Check(Is(p.Select("coop", "coop_survival_mp_afghan"), "mp_fav_tropical", "mp_fav_tropical", "survival", "survival"), T,
              "... and it is not a rotation: the second resolution is the same level");
        Check(Is(p.Select("survival", "mp_storm_spring"), "mp_storm_spring", "mp_storm_spring", "survival", "survival"), T,
              "a room that names gametype survival is Survival on its MP level");
        Check(Is(p.Select("coop", "coop_training_mp_killhouse_sh"), "mp_killhouse_sh", "coop_training_mp_killhouse_sh", "coop", "coop"), T,
              "a co-op mission is still co-op");
        dw::Selection zm = p.Select("zm", "zm_asylum_sh", 2);
        Check(Is(zm, "zm_asylum_sh", "zm_asylum_sh", "zm", "zm") && zm.difficulty == 2, T, "Zombies carries the room's difficulty (desc[26])");
        Check(p.Select("zm", "zm_asylum_sh", 7).difficulty == 0, T, "a difficulty outside 0..2 is easy");
        Check(p.Select("war", "mp_rust").difficulty == -1, T, "no difficulty outside Zombies");
        /* the room latch of another category is some earlier room */
        std::string zmGt = "zm", warGt = "war";
        dw::HandoffLatchRoom(p.server, "zm_prototype_sh", &zmGt, 4, "test");
        Check(Is(p.Select("war", "mp_crash_tropical"), "mp_killhouse_sh", "mp_killhouse_sh", "war", "mp"), T,
              "a zm room latch does not answer an mp room whose map is the ctor default");
        dw::HandoffLatchRoom(p.server, "mp_crash_tropical", &warGt, 12, "test");
        Check(Is(p.Select("war", "mp_crash_tropical"), "mp_crash_tropical", "mp_crash_tropical", "war", "mp"), T,
              "a latch of the same category still names the room's map");
    }
    {   /* a game install: usermaps join the vocabulary; Survival needs zone + placement script (+ Kernel33's list) */
        dw::Config cfg = base;
        cfg.gameDir = dw::PathJoin(scratch, "game");
        const std::string zones = dw::PathJoin(dw::PathJoin(cfg.gameDir, "client_shipRetail"), "chinese");
        const std::string surv = dw::PathJoin(dw::PathJoin(dw::PathJoin(dw::PathJoin(dw::PathJoin(cfg.gameDir, "main"), "sh1x_rawfiles"), "maps"), "mp"), "survival");
        const std::string gts = dw::PathJoin(dw::PathJoin(dw::PathJoin(dw::PathJoin(dw::PathJoin(cfg.gameDir, "main"), "sh1x_rawfiles"), "maps"), "mp"), "gametypes");
        cfg.zoneDir = zones;
        static const char *const stock[] = { "mp_rust", "mp_rust_load", "mp_fav_tropical", "mp_cross_fire", "mp_highrise", "mp_crash_intro",
                                              "zm_sumpf_sh", "mp_playlists", "patch_mp_rust" };
        for (size_t i = 0; i < sizeof(stock) / sizeof(stock[0]); i++) Touch(dw::PathJoin(zones, std::string(stock[i]) + ".ff"));
        Touch(dw::PathJoin(dw::PathJoin(dw::PathJoin(cfg.gameDir, "usermaps"), "mp_seatown"), "mp_seatown.ff"));
        dw::MakeDirs(dw::PathJoin(dw::PathJoin(cfg.gameDir, "usermaps"), "not_a_map"));
        {
            Pick p(cfg, &host);
            dw::Selection none = p.Select("coop", "coop_survival_mp_fav_tropical");
            Check(!none.error.empty() && none.type == "survival", T, "Survival without the installed gametype script names no level and says why");
        }
        Touch(dw::PathJoin(gts, "survival.gsc"));
        Touch(dw::PathJoin(surv, "mp_cross_fire.gsc"));
        Touch(dw::PathJoin(surv, "mp_seatown.gsc"));
        Touch(dw::PathJoin(surv, "mp_storm_spring.gsc")); /* placements, but no zone */
        {
            Pick p(cfg, &host);
            Check(Is(p.Select("war", "mp_seatown"), "mp_seatown", "mp_seatown", "war", "mp"), T, "a usermaps folder's map resolves as the room's map");
            Check(Is(p.Select("war", "mp_crash_intro"), "mp_crash_intro", "mp_crash_intro", "war", "mp"), T, "a level zone needs no _load zone");
            Check(Is(p.Select("zm", "zm_sumpf_sh", 1), "zm_sumpf_sh", "zm_sumpf_sh", "zm", "zm"), T, "an installed zm zone outside the playlists resolves");
            std::vector<std::string> levels = dw::HandoffSurvivalLevels(p.server);
            Check(levels.size() == 2 && levels[0] == "mp_cross_fire" && levels[1] == "mp_seatown", T,
                  "Survival levels: the supported list cut to zone + placement script (placements without the zone do not count)");
            Check(Is(p.Select("coop", "coop_survival_mp_fav_tropical"), "mp_cross_fire", "mp_cross_fire", "survival", "survival"), T,
                  "a supported level without its placement script plays the first installed one");
            Check(Is(p.Select("coop", "coop_survival_mp_seatown"), "mp_seatown", "mp_seatown", "survival", "survival"), T,
                  "a usermap with placements hosts Survival");
        }
        dw::WriteFileAtomic(dw::PathJoin(dw::PathJoin(cfg.gameDir, "p2p"), "survival_maps.txt"), "# hostable\r\nmp_seatown\r\nmp_highrise # no script\r\n");
        {
            Pick p(cfg, &host);
            std::vector<std::string> levels = dw::HandoffSurvivalLevels(p.server);
            Check(levels.size() == 1 && levels[0] == "mp_seatown", T, "Kernel33's p2p/survival_maps.txt narrows the levels to what the host accepts");
        }
    }
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
        /* the room UI's name of Team Deathmatch: the host is told, and the client's dwRoomInfo gets, the native name */
        { "mp-tdm", "tdm", "mp_rust", "mp_rust", "mp_rust", "war", "mp", "mp_rust", "war" },
        { "zm", "zm", "zm_asylum_sh", "zm_asylum_sh", "zm_asylum_sh", "zm", "zm", "zm_asylum_sh", "zm" },
        /* C1's survival button: a coop descriptor naming coop_survival_<level>. The host and the client both get the MP
         * level's own spelling with gametype survival; mp_afghan has no survival data, so the first supported level. */
        { "survival", "coop", "coop_survival_mp_afghan", "mp_fav_tropical", "mp_fav_tropical", "survival", "survival", "mp_fav_tropical", "survival" },
        { "coop", "coop", "coop_training_mp_killhouse_sh", "mp_killhouse_sh", "coop_training_mp_killhouse_sh", "coop", "coop",
          "coop_training_mp_killhouse_sh", "coop" },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) RunCase(cfg, cases[i], (int)i);
    SelectionChecks(cfg, scratch);
    printf("%s (%d checks, %d failed)\n", g_fail ? "SELFTEST-ROOMS FAIL" : "SELFTEST-ROOMS PASS", g_checks, g_fail);
    return g_fail ? 1 : 0;
}

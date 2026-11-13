/*
 * dwtest - the offline Demonware core as a standalone loopback server, for the differential tests
 * against the Python lobby (tests/offline/README.md). Never the client's real lobby port: the port is
 * required on the command line and 3074 / 17733 / 17760 are refused.
 *
 *   dwtest --port 3274 --alt-port 3278 --resources <dir> --profile <dir> --state <dir>
 *          [--match-profile <dir>] [--zone-dir <dir>] [--mm-port N] [--fixed-time T] [--differential]
 *          [--log <file>] [--joinreq-gap S] [--selftest]
 *   dwtest --resources <dir> --profile <empty scratch dir> [--match-profile <dir>] --selftest-store
 *   dwtest [--resources <dir>] [--state <dir>] --selftest-rooms
 *   dwtest --resources <pristine resources> --profile <empty scratch dir> --selftest-market
 *   dwtest --resources <dir> --profile <empty scratch dir> --match-profile <empty scratch dir> --selftest-career
 *   dwtest [--fixed-time T] [--differential] [--wb-background] [--wb-create <uid>] --writeback <resources> <profile> <match> <uid>
 *   dwtest [--resources <dir>] --profile <a COPY of a players folder> --career      print every career in it as the lobby reads it
 *   --local-name <name>   the profile name of the account that logs in (Config::localName)
 *   --legacy-store <dir>  a k33_offline\profile store of the first offline builds to import (Config::legacyStore)
 *   dwtest --selftest-net                 the listener on free ports (what the client uses): two at once, their STUN sockets
 *   A test SERVER is always given its private ports: --port 0 is not offered.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../../src/offline/dw.h"
#include "../../src/offline/dw_net.h"
#include "../../src/offline/dw_handoff.h"
#include "../../src/offline/dw_playerdata.h"

struct TestHost : dw::Host {
    FILE *log;
    TestHost() : log(NULL) {}
    virtual void Log(const std::string &line)
    {
        if (!log) return;
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(log, "[%02d:%02d:%02d.%03d] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line.c_str());
        fflush(log);
    }
    virtual double Mono() { return GetTickCount64() / 1000.0; }
};

int DwStoreSelfTest(const dw::Config &cfg); /* dwtest_store.cpp */
int DwRoomsSelfTest(const dw::Config &cfg); /* dwtest_rooms.cpp */
int DwMarketSelfTest(const dw::Config &cfg); /* dwtest_market.cpp */
int DwCareerSelfTest(const dw::Config &cfg); /* dwtest_career.cpp */
int DwWritebackRun(const dw::Config &cfg, const char *uid, bool background, const char *createUid); /* dwtest_career.cpp */
int DwCareerDump(const dw::Config &cfg); /* dwtest_career.cpp */

static volatile long g_stop;
static BOOL WINAPI OnCtrl(DWORD) { InterlockedExchange(&g_stop, 1); return TRUE; }

static int SelfTest()
{
    /* The client's own digests, captured live (demonware/a22_tiger.py). */
    unsigned char d[24];
    DwTiger("simplePassword", 14, d);
    int ok = dw::Hex(d, 24) == "fa4051e978015e02bcd6f3adf0663eee388aeecf9352d00a";
    DwTiger("ym9iewfdbgllbnq=", 16, d);
    ok = ok && dw::Hex(d, 8) == "588c2e26dab3b8c7";
    /* FIPS 46-3 / NIST SP 800-67 3DES known answer, ECB via CBC with a zero IV. */
    static const unsigned char key[24] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01,
                                           0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23 };
    static const unsigned char zero[8] = { 0 };
    unsigned char block[8], back[8];
    memcpy(block, "The quic", 8);
    DwDes3Cbc(key, zero, 0, block, block, 8);
    ok = ok && dw::Hex(block, 8) == "1ccf23869d09333e";
    DwDes3Cbc(key, zero, 1, block, back, 8);
    ok = ok && !memcmp(back, "The quic", 8);
    /* frame round trip */
    dw::Bytes msg("\x03\x07hello", 7), frame = dw::EncryptFrame(msg, 0x1234);
    ok = ok && frame.size() == 4 + 1 + 4 + 16;
    printf("%s\n", ok ? "SELFTEST PASS" : "SELFTEST FAIL");
    return ok ? 0 : 1;
}

/* ---- dwtest --selftest-net: the listener on FREE ports (NetConfig port 0 / altPort 0) ----
 * The only test that asks the operating system for ports; it never names one, so it cannot collide
 * with a lobby (3074), a match server or another test. Loopback only. */
struct NetRunArgs {
    dw::Server *server;
    dw::NetConfig net;
    volatile long stop, port, alt, rc;
};
static DWORD WINAPI NetRunThread(LPVOID p)
{
    NetRunArgs *a = (NetRunArgs *)p;
    InterlockedExchange(&a->rc, dw::NetRun(*a->server, a->net, &a->stop, &a->port, &a->alt) + 1);
    return 0;
}
static bool NetWait(NetRunArgs &a)
{
    for (int i = 0; i < 500 && !InterlockedCompareExchange(&a.port, 0, 0) && !InterlockedCompareExchange(&a.rc, 0, 0); i++) Sleep(10);
    return InterlockedCompareExchange(&a.port, 0, 0) != 0;
}
/* One STUN probe from a fresh socket: -> the port the answer came from (0 = none), *ip its address. */
static int StunProbe(int port, const char *req, int len, unsigned *ip, char *first)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in to, from;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    to.sin_port = htons((u_short)port);
    int got = 0, flen = sizeof(from);
    char buf[64];
    sendto(s, req, len, 0, (sockaddr *)&to, sizeof(to));
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(s, &rd);
    timeval tv = { 2, 0 };
    if (select(0, &rd, NULL, NULL, &tv) > 0 && recvfrom(s, buf, sizeof(buf), 0, (sockaddr *)&from, &flen) > 0) {
        got = ntohs(from.sin_port);
        *ip = ntohl(from.sin_addr.s_addr);
        *first = buf[0];
    }
    closesocket(s);
    return got;
}
static int NetSelfTest(const dw::Config &base)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    TestHost host; /* silent unless a listener fails */
    int checks = 0, fail = 0, rounds = 40;
#define NCHECK(cond) do { checks++; if (!(cond)) { fail++; printf("  FAIL line %d (round %d): %s\n", __LINE__, round, #cond); } } while (0)
    for (int round = 0; round < rounds; round++) {
        dw::Config cfg = base;
        cfg.profile.clear();
        cfg.matchProfile.clear();
        cfg.state.clear();
        dw::Server s1(cfg, &host), s2(cfg, &host), s3(cfg, &host);
        NetRunArgs a, b, c;
        a.stop = a.port = a.alt = a.rc = b.stop = b.port = b.alt = b.rc = c.stop = c.port = c.alt = c.rc = 0;
        a.server = &s1; b.server = &s2; c.server = &s3;
        a.net.port = a.net.altPort = b.net.port = b.net.altPort = 0; /* two lobbies on one PC, both on free ports */
        /* every other round the second one may not take the system's choice (the path a port the system
         * offers for TCP and refuses for UDP leads to: Windows' per-protocol excluded port ranges) */
        b.net.pickOwn = (round & 1) != 0;
        host.log = stdout;
        HANDLE ta = CreateThread(NULL, 0, NetRunThread, &a, 0, NULL), tb = CreateThread(NULL, 0, NetRunThread, &b, 0, NULL);
        bool up = NetWait(a) && NetWait(b);
        host.log = NULL;
        NCHECK(up);
        if (up) {
            int pa = (int)a.port, pb = (int)b.port, aa = (int)a.alt, ab = (int)b.alt;
            NCHECK(pa >= 1024 && pb >= 1024 && pa != pb && aa >= 1024 && ab >= 1024 && aa != pa && ab != pb && aa != ab);
            NCHECK(s1.cfg.stunPort == pa && s1.cfg.stunAltPort == aa && s2.cfg.stunPort == pb && s2.cfg.stunAltPort == ab);
            /* the client's NAT discovery finds every socket it needs on the ports that were obtained */
            unsigned ip = 0;
            char type = 0;
            NCHECK(StunProbe(pa, "\x1e\x02\x00", 3, &ip, &type) == pa && ip == 0x7F000001 && type == 31);       /* IP discovery */
            NCHECK(StunProbe(pa, "\x14\x02\x00\x00", 4, &ip, &type) == pa && ip == 0x7F000001 && type == 21);   /* NAT test 1 */
            NCHECK(StunProbe(pa, "\x14\x02\x00\x03", 4, &ip, &type) == aa && ip == 0x7F000002 && type == 21);   /* changed IP and port */
            NCHECK(StunProbe(pa, "\x14\x02\x00\x02", 4, &ip, &type) == aa && ip == 0x7F000001 && type == 21);   /* changed port */
            NCHECK(StunProbe(pb, "\x1e\x02\x00", 3, &ip, &type) == pb && type == 31);
            SOCKET t = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            sockaddr_in to;
            memset(&to, 0, sizeof(to));
            to.sin_family = AF_INET;
            to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            to.sin_port = htons((u_short)pa);
            NCHECK(connect(t, (sockaddr *)&to, sizeof(to)) == 0);
            closesocket(t);
            /* an EXPLICIT port that is taken is refused, never replaced by another one */
            c.net.port = pa; c.net.altPort = 0;
            HANDLE tc = CreateThread(NULL, 0, NetRunThread, &c, 0, NULL);
            NCHECK(!NetWait(c) && WaitForSingleObject(tc, 5000) == WAIT_OBJECT_0 && c.rc == 3);
            CloseHandle(tc);
        }
        InterlockedExchange(&a.stop, 1);
        InterlockedExchange(&b.stop, 1);
        NCHECK(WaitForSingleObject(ta, 5000) == WAIT_OBJECT_0 && WaitForSingleObject(tb, 5000) == WAIT_OBJECT_0);
        CloseHandle(ta);
        CloseHandle(tb);
    }
#undef NCHECK
    printf("%d rounds, %d checks, %d failed\n%s\n", rounds, checks, fail, fail ? "SELFTEST-NET FAIL" : "SELFTEST-NET PASS");
    return fail ? 1 : 0;
}

/* --select-file <file>: the selection of a room, one request per line, for tests/offline/select_diff.py:
 *     latch <map> <gametype|->                                     the typeId-15 room latch
 *     select <gametype> <map> <league> <mode> <maxplayers> <desc26> the descriptor's fields (names are crc32'd)
 * -> one line per select: map session_map gametype type difficulty source resolved error */
static int SelectFile(const dw::Config &cfg, dw::Host *host, const char *path)
{
    dw::Server server(cfg, host);
    FILE *f = fopen(path, "rb");
    if (!f) return 2;
    char line[512], a[64], b[64];
    long league, mode, maxp, diff;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "latch %63s %63s", a, b) == 2) {
            std::string gt = b;
            dw::HandoffLatchRoom(server, a, strcmp(b, "-") ? &gt : NULL, 12, "test");
        } else if (sscanf(line, "select %63s %63s %ld %ld %ld %ld", a, b, &league, &mode, &maxp, &diff) == 6) {
            dw::Vals v;
            for (int i = 0; i < 28; i++) v.push_back(dw::Val(dw::T_INT32, (uint64_t)0));
            v[2].u = (uint64_t)maxp;
            v[5].u = (uint64_t)mode;
            v[11].u = (uint64_t)league;
            v[23].u = !strcmp(a, "playlist") ? 3615625517u : dw::Crc32(a, strlen(a));
            v[24].u = (b[0] >= '0' && b[0] <= '9') ? (uint64_t)atol(b) : dw::Crc32(b, strlen(b));
            v[26].u = (uint64_t)(uint32_t)diff;
            dw::Selection s = dw::HandoffSelect(server, &v);
            printf("%s %s %s %s %d %s %d %s\n", s.map.c_str(), s.sessionMap.c_str(), s.gametype.c_str(), s.type.c_str(), s.difficulty,
                   s.source.c_str(), s.resolved ? 1 : 0, s.error.empty() ? "-" : "ERROR");
        }
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    dw::Config cfg;
    dw::NetConfig net;
    TestHost host;
    host.log = stdout;
    bool wbBackground = false;
    const char *wbCreate = "0";
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : "";
        if (!strcmp(a, "--selftest")) return SelfTest();
        else if (!strcmp(a, "--selftest-net")) return NetSelfTest(cfg);
        else if (!strcmp(a, "--selftest-store")) return DwStoreSelfTest(cfg); /* uses the folders given before it */
        else if (!strcmp(a, "--selftest-rooms")) return DwRoomsSelfTest(cfg); /* scratch folders unless given before it */
        else if (!strcmp(a, "--selftest-market")) return DwMarketSelfTest(cfg); /* needs --resources and --profile before it */
        else if (!strcmp(a, "--selftest-career")) return DwCareerSelfTest(cfg); /* needs --resources, --profile, --match-profile */
        else if (!strcmp(a, "--career")) return DwCareerDump(cfg); /* needs --profile before it */
        else if (!strcmp(a, "--select-file")) return SelectFile(cfg, &host, v);
        else if (!strcmp(a, "--attach-table")) { /* the weapon attachment table as "weapon,attachment,level" lines */
            dw::Server server(cfg, &host);
            std::string text = dw::PdAttachTableDump(server);
            fwrite(text.data(), 1, text.size(), stdout);
            return text.empty() ? 1 : 0;
        }
        else if (!strcmp(a, "--local-name")) { cfg.localName = v; i++; }
        else if (!strcmp(a, "--legacy-store")) { cfg.legacyStore = v; i++; }
        else if (!strcmp(a, "--wb-background")) wbBackground = true;
        else if (!strcmp(a, "--wb-create")) { wbCreate = v; i++; } /* start from a fresh level-1 profile of this uid */
        else if (!strcmp(a, "--writeback") && i + 4 < argc) { /* apply the reports waiting in <match-dir>, print the result */
            cfg.resources = argv[i + 1]; cfg.profile = argv[i + 2]; cfg.matchProfile = argv[i + 3];
            return DwWritebackRun(cfg, argv[i + 4], wbBackground, wbCreate);
        }
        else if (!strcmp(a, "--differential")) cfg.differential = true;
        else if (!strcmp(a, "--port")) { net.port = atoi(v); i++; }
        else if (!strcmp(a, "--alt-port")) { net.altPort = atoi(v); i++; }
        else if (!strcmp(a, "--resources")) { cfg.resources = v; i++; }
        else if (!strcmp(a, "--profile")) { cfg.profile = v; i++; }
        else if (!strcmp(a, "--state")) { cfg.state = v; i++; }
        else if (!strcmp(a, "--match-profile")) { cfg.matchProfile = v; i++; }
        else if (!strcmp(a, "--zone-dir")) { cfg.zoneDir = v; i++; }
        else if (!strcmp(a, "--game-dir")) { cfg.gameDir = v; i++; }
        else if (!strcmp(a, "--mm-port")) { cfg.mmHostPort = atoi(v); i++; }
        else if (!strcmp(a, "--joinreq-gap")) { cfg.joinreqRelayGap = atof(v); i++; } /* CODOL_JOINREQ_RELAY_MINGAP */
        else if (!strcmp(a, "--fixed-time")) { cfg.fixedTime = _atoi64(v); i++; }
        else if (!strcmp(a, "--log")) { host.log = fopen(v, "ab"); i++; }
        else { fprintf(stderr, "unknown argument %s\n", a); return 2; }
    }
    if (net.port < 1024 || net.port == 3074 || net.port == 17733 || net.port == 17760 || net.altPort == 3074) {
        fprintf(stderr, "dwtest: --port must be a private test port (not 3074 / 17733 / 17760)\n");
        return 2;
    }
    cfg.stunPort = net.port;
    cfg.stunAltPort = net.altPort;
    SetConsoleCtrlHandler(OnCtrl, TRUE);
    dw::Server server(cfg, &host);
    return dw::NetRun(server, net, &g_stop, NULL);
}

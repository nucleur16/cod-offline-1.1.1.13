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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../../offline/dw.h"
#include "../../offline/dw_net.h"

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
        else if (!strcmp(a, "--selftest-store")) return DwStoreSelfTest(cfg); /* uses the folders given before it */
        else if (!strcmp(a, "--selftest-rooms")) return DwRoomsSelfTest(cfg); /* scratch folders unless given before it */
        else if (!strcmp(a, "--selftest-market")) return DwMarketSelfTest(cfg); /* needs --resources and --profile before it */
        else if (!strcmp(a, "--selftest-career")) return DwCareerSelfTest(cfg); /* needs --resources, --profile, --match-profile */
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

/* The actual offline_lobby.c body (the Kernel33 side of the built-in lobby) in an owned Windows fixture:
 * that it needs no switch (and that the old one is ignored), the free lobby port and the in-memory dwauth.ini / stun.ini route that carries it, the
 * NAT traversal port patch, the profile name and its folder, the level gates, the embedded host's
 * port (free, reserved until the host starts) and folders, and the Local Play hand-over. The lobby
 * itself (offline/) and C1 are doubles; the only sockets are loopback UDP sockets bound to prove a
 * port is held or free, and no game is started.
 *
 *   offline_lobby_fixture.exe <case> [client arguments...]      (tests/test_offline_lobby_windows.py)
 */
#include <winsock2.h>
#include <windows.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODOL_OFFLINE_LOBBY 1
typedef struct { DWORD slot; const char *dll; const char *name; } C1Import;
static char all_log[8192];
static void Log(const char *fmt, ...)
{
    size_t n = strlen(all_log);
    va_list ap; va_start(ap, fmt); vsnprintf(all_log + n, sizeof(all_log) - n - 2, fmt, ap); va_end(ap);
    strcat(all_log, "\n");
}
static BOOL Readable(const void *p, SIZE_T n)
{
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(p, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
           (const BYTE *)p + n <= (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
}
static void WriteCode(void *dst, const void *src, SIZE_T n) { memcpy(dst, src, n); }

/* p2p.c, reduced to what offline_lobby.c reads and calls. */
#define FIXTURE_HOST_PORT 17761 /* what "p2p_host.ini" configures here */
typedef struct P2PConfig {
    char state[MAX_PATH], profile[MAX_PATH], rconFile[MAX_PATH], rollout[MAX_PATH], netIp[64], mode[16], extra[512];
    int port, admission;
} P2PConfig;
static void P2PDefaults(P2PConfig *c) { memset(c, 0, sizeof(*c)); c->port = FIXTURE_HOST_PORT; lstrcpyA(c->state, "X:\\lobby\\state"); }
static void OfflineP2PConfig(P2PConfig *c);
static void P2PLoadIni(P2PConfig *c)
{
    lstrcpyA(c->profile, "X:\\lobby\\resources"); lstrcpyA(c->rollout, "X:\\lobby\\rollout.txt"); lstrcpyA(c->extra, "+set x 1");
    OfflineP2PConfig(c); /* as p2p.c's P2PLoadIni ends */
}
/* p2p_local_play_api.h doubles: the published intent, the lobby's room selection and its leave. */
#include "../src/p2p/p2p_local_play_api.h"
static K33LocalPlayIntentInfo intent;
static int intent_published, auto_calls, auto_answer, leave_calls;
static char auto_seen[200];
static unsigned long long leave_seen;
int K33LocalPlayIntent(K33LocalPlayIntentInfo *out) { if (!intent_published) return 0; *out = intent; return 1; }
int K33LocalPlayAuto(unsigned long long owner_uid, const char *session_map, const char *gametype, const char *type, int difficulty,
                     int launch, int *port, char *error, int error_size)
{
    ++auto_calls;
    _snprintf(auto_seen, sizeof(auto_seen), "%llu/%s/%s/%s/%d/%d", owner_uid, session_map, gametype, type, difficulty, launch);
    if (!auto_answer) { lstrcpynA(error, "Restart the client before hosting another Local Play match.", error_size); return 0; }
    *port = 17999;
    return 1;
}
void K33LocalPlayLeave(unsigned long long owner_uid) { ++leave_calls; leave_seen = owner_uid; }

/* The NAT traversal constant of C1 (0x00A50454) is a local double here. */
static BYTE nat_site[9] = { 0xB9, 0x02, 0x0C, 0x00, 0x00, 0x66, 0x89, 0x4F, 0x14 };
#define OFL_NATTRAV_PORT_VA ((DWORD_PTR)nat_site)
/* So are AS_IsNotificationUnlocked's call (0x005AB321) and the function it calls (0x005B5EB0). */
static BYTE notify_site[5];
static int notify_calls, notify_answer;
static char notify_seen[64];
static unsigned char __cdecl C1NotificationUnlocked(const char *name)
{
    ++notify_calls;
    lstrcpynA(notify_seen, name, sizeof(notify_seen));
    return (unsigned char)notify_answer;
}
#define OFL_NOTIFY_CALL_VA ((DWORD_PTR)notify_site)
#define OFL_NOTIFY_FN_VA   ((DWORD_PTR)C1NotificationUnlocked)

/* src/offline/dw_api.h doubles: record what Kernel33 asked for. A free port is "obtained" as 41234 / 41238. */
#include "../src/offline/dw_api.h"
#define FIXTURE_FREE_PORT 41234
#define FIXTURE_FREE_ALT  41238
static DwOfflineConfig started;
static char started_name[64], started_legacy[MAX_PATH];
static int start_calls, stop_calls, listen_fails;
int DwOfflineStartServer(const DwOfflineConfig *config)
{
    ++start_calls;
    started = *config;
    lstrcpynA(started_name, config->playerName ? config->playerName : "<null>", sizeof(started_name));
    lstrcpynA(started_legacy, config->legacyStore ? config->legacyStore : "<null>", sizeof(started_legacy));
    return listen_fails ? 0 : config->port ? config->port : FIXTURE_FREE_PORT;
}
int DwOfflineAltPort(void) { return started.altPort ? started.altPort : FIXTURE_FREE_ALT; }
void DwOfflineStopServer(void) { ++stop_calls; }
int DwOfflineRunning(void) { return start_calls && !listen_fails && !stop_calls; }

#include "../src/offline_lobby.c"

static const C1Import kCreate = { 0x00B421D8, "KERNEL32.dll", "CreateFileA" };
static const C1Import kCmd = { 0x00B42284, "KERNEL32.dll", "GetCommandLineA" };
static const C1Import kOther = { 0x00B42328, "KERNEL32.dll", "CreateFileW" };

static void ReadAll(HANDLE h, char *out, DWORD cap)
{
    DWORD n = 0;
    assert(h != INVALID_HANDLE_VALUE);
    assert(ReadFile(h, out, cap - 1, &n, NULL));
    out[n] = 0;
    CloseHandle(h);
}

/* Is udp 127.0.0.1:<port> free right now (bound exclusively and released again)? */
static BOOL UdpFree(int port)
{
    SOCKET s = OflBindLoopbackUdp(port);
    if (s == INVALID_SOCKET) return FALSE;
    closesocket(s);
    return TRUE;
}

static BOOL EndsWith(const char *s, const char *tail)
{
    size_t a = strlen(s), b = strlen(tail);
    return a >= b && !_stricmp(s + a - b, tail);
}

typedef HANDLE (WINAPI *CreateFn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef LPSTR (WINAPI *CmdFn)(void);

int main(int argc, char **argv)
{
    assert(argc >= 2);
    const char *test = argv[1];
    WSADATA wsa;
    assert(!WSAStartup(MAKEWORD(2, 2), &wsa));
    LONG rel = (LONG)((DWORD_PTR)C1NotificationUnlocked - ((DWORD_PTR)notify_site + 5));
    notify_site[0] = 0xE8;
    memcpy(notify_site + 1, &rel, 4);
    if (!strcmp(test, "badbytes")) nat_site[1] = 0x03;
    if (!strcmp(test, "badgate")) notify_site[0] = 0xE9;
    if (!strcmp(test, "nolisten")) listen_fails = 1;
    const BOOL ini3999 = !strcmp(test, "ini3999"), ini3074 = !strcmp(test, "ini3074"), keepgates = !strcmp(test, "keepgates");
    const BOOL badgate = !strcmp(test, "badgate"), hostbusy = !strcmp(test, "hostbusy");

    FARPROC realCreate = (FARPROC)CreateFileA, realCmd = (FARPROC)GetCommandLineA;
    FARPROC create = OfflineRouteImport(&kCreate, realCreate), cmd = OfflineRouteImport(&kCmd, realCmd);
    assert(OfflineRouteImport(&kOther, realCreate) == realCreate); /* never another slot */

    /* There is no switch: with no argument and no ini (case "plain"), and with the `enabled = 0` line of
     * earlier builds, the two import slots are wrapped and the lobby starts all the same. */
    assert(create != realCreate && cmd != realCmd);
    /* Before the lobby is up the wrappers are transparent. */
    assert(((CmdFn)cmd)() == GetCommandLineA());

    if (!strcmp(test, "launcher") || !strcmp(test, "badbytes") || !strcmp(test, "nolisten")) {
        assert(!InstallOfflineLobby() && !OfflineLobbyActive());
        assert(!strcmp(test, "launcher") ? start_calls == 0 : start_calls == 1);
        if (!strcmp(test, "badbytes")) {
            /* a free port needs C1's constant; a differing site is never written and the listener is stopped again */
            assert(nat_site[1] == 0x03 && stop_calls == 1);
        }
        /* a refused start holds no port and lifts no gate */
        assert(g_ofl.hostSock == -1 && !g_ofl.gatesLifted && notify_site[0] == 0xE8 && !memcmp(notify_site + 1, &rel, 4));
        puts("offline lobby fixture passed");
        return 0;
    }

    /* "hostbusy": something else on this PC already serves on the configured match host port. */
    SOCKET busy = INVALID_SOCKET;
    if (hostbusy) { busy = OflBindLoopbackUdp(FIXTURE_HOST_PORT); assert(busy != INVALID_SOCKET); }
    const BOOL hostWasFree = UdpFree(FIXTURE_HOST_PORT);

    assert(InstallOfflineLobby() && OfflineLobbyActive() && start_calls == 1 && !stop_calls);

    /* THE LOBBY PORT is whatever was free; an ini port is an explicit override for tests. */
    const int port = ini3999 ? 3999 : ini3074 ? 3074 : FIXTURE_FREE_PORT;
    assert(started.port == (ini3999 ? 3999 : ini3074 ? 3074 : 0) && started.altPort == (ini3999 ? 4003 : 0));
    assert(g_ofl.port == port && g_ofl.altPort == (ini3999 ? 4003 : FIXTURE_FREE_ALT));
    assert(started.localRoute && started.start && started.leave && started.startGame && started.log);
    /* 3074 is C1's own NAT traversal port: any other port is written into the constant. */
    DWORD imm; memcpy(&imm, nat_site + 1, 4);
    assert(imm == (DWORD)(port == 3074 ? 0xC02 : port));
    assert(!memcmp(nat_site + 5, "\x66\x89\x4F\x14", 4));

    /* THE STORE: the game's own players folder and the profile name in it; run-time files apart. */
    assert(EndsWith(started.profile, "\\players") && EndsWith(started.state, "\\p2p\\offline") && !strcmp(started.state, started.matchProfile));
    const BOOL named = strstr(GetCommandLineA(), "dw_username") != NULL;
    if (!strcmp(test, "badname")) assert(!strcmp(started_name, "<null>")); /* not a folder name: named by player id */
    else assert(!strcmp(started_name, named || ini3999 || !strcmp(test, "oldswitch") ? "edtest" : "Player"));
    if (!strcmp(test, "legacy")) assert(EndsWith(started_legacy, "\\k33_offline\\profile"));
    else assert(!strcmp(started_legacy, "<null>"));

    /* C1's reads of the two route files are answered from memory, with the port obtained... */
    char text[512], want[128];
    ReadAll(((CreateFn)create)("dwauth.ini", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL), text, sizeof(text));
    _snprintf(want, sizeof(want), "127.0.0.1\n%d\n127.0.0.1\n%d\n3ea\n", port, port);
    assert(!strcmp(text, want));
    ReadAll(((CreateFn)create)(".\\STUN.INI", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL), text, sizeof(text));
    int lines = 0;
    for (char *p = text; *p; p += 10) { assert(!strncmp(p, "127.0.0.1\n", 10)); lines++; }
    assert(lines == 10);
    /* ...every other file, and a write to those names, goes to the real CreateFileA. */
    char self[MAX_PATH];
    GetModuleFileNameA(NULL, self, MAX_PATH);
    HANDLE h = ((CreateFn)create)(self, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD got = 0; char mz[2];
    assert(h != INVALID_HANDLE_VALUE && ReadFile(h, mz, 2, &got, NULL) && got == 2 && mz[0] == 'M' && mz[1] == 'Z');
    CloseHandle(h);
    SetLastError(0);
    h = ((CreateFn)create)("Q:\\k33-no-such-folder\\dwauth.ini", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(h == INVALID_HANDLE_VALUE && GetLastError() != 0);

    /* THE LEVEL GATES: the play notifications answer "unlocked" without asking C1; every other
     * notification is C1's own answer; the map list opens through C1's own dvar on the command line. */
    const BOOL lifted = !keepgates && !badgate;
    assert(g_ofl.gatesLifted == lifted);
    LONG now; memcpy(&now, notify_site + 1, 4);
    if (lifted) assert(notify_site[0] == 0xE8 && (DWORD_PTR)notify_site + 5 + now == (DWORD_PTR)OflNotificationUnlocked);
    else assert(now == rel && notify_site[0] == (badgate ? 0xE9 : 0xE8)); /* untouched */
    static const char *const play[] = { "un_room_list", "un_mp_room_creation", "un_mp_playlist", "un_training_playlist", "un_combat_training",
                                        "un_noob_tdm", "un_specops", "un_coop_room_creation", "un_zm_asylum_sh", "un_zm_prototype_sh" };
    static const char *const kept[] = { "un_weapon_customization", "un_avatar_customization", "un_mp_stattrack", "un_marketplace",
                                        "un_mp_tutorial", "", "un_room_list ", "UN_ROOM_LIST" };
    notify_answer = 0;
    for (unsigned i = 0; i < sizeof(play) / sizeof(play[0]); i++) assert(OflNotificationUnlocked(play[i]) == 1);
    assert(!notify_calls);
    for (unsigned i = 0; i < sizeof(kept) / sizeof(kept[0]); i++) {
        notify_answer = (int)(i & 1);
        assert(OflNotificationUnlocked(kept[i]) == (i & 1) && notify_calls == (int)i + 1 && !strcmp(notify_seen, kept[i]));
    }
    assert(strstr(all_log, lifted ? "level gates on playing lifted" : keepgates ? "level gates kept" : "its level gates stay"));

    /* The command line: the profile name only when it has none, the map-list dvar only with the gates lifted. */
    const char *line = ((CmdFn)cmd)();
    if (named && !lifted) assert(line == GetCommandLineA());
    else {
        assert(line != GetCommandLineA() && !strncmp(line, GetCommandLineA(), strlen(GetCommandLineA())));
        const char *tail = line + strlen(GetCommandLineA());
        char expect[128] = "";
        if (!named) { lstrcatA(expect, " +set dw_username "); lstrcatA(expect, ini3999 || !strcmp(test, "oldswitch") ? "edtest" : "Player"); }
        if (lifted) lstrcatA(expect, " +set 2653 1");
        assert(!strcmp(tail, expect));
        assert(((CmdFn)cmd)() == line); /* one stable string */
    }

    /* THE MATCH HOST PORT: the configured one when it is free, else a free one; held reserved from now
     * until the host is about to bind it, so a second client cannot be given the same port. */
    P2PConfig c; P2PDefaults(&c); P2PLoadIni(&c);
    assert(!strcmp(c.state, started.state) && !strcmp(c.profile, started.matchProfile) && !c.rollout[0] && !c.extra[0] &&
           !strcmp(c.netIp, "127.0.0.1") && !strcmp(c.mode, "inprocess") && c.admission == 1);
    assert(c.port == g_ofl.hostPort && started.hostPort == g_ofl.hostPort);
    if (ini3999) {
        assert(c.port == 17999 && g_ofl.hostSock == -1); /* host_port: used as it is, nothing held */
    } else {
        assert(g_ofl.hostSock != -1);
        if (hostWasFree) assert(c.port == FIXTURE_HOST_PORT);
        else assert(c.port != FIXTURE_HOST_PORT && c.port >= 1024);
        assert(!UdpFree(c.port)); /* reserved */
        OfflineHostPortRelease();
        assert(g_ofl.hostSock == -1 && UdpFree(c.port)); /* the host can bind it now */
        OfflineHostPortRelease();
        P2PDefaults(&c); P2PLoadIni(&c);
        assert(c.port == g_ofl.hostPort); /* the port of this run does not change with the release */
    }
    if (busy != INVALID_SOCKET) closesocket(busy);

    /* Local hosting: the lobby reads Kernel33's published snapshot, names the selection of the owner's
     * room (armed; with a launch when it hands START_GAME over) and says when the owner left, through
     * the p2p_local_play_api.h calls only; nothing here starts a host or reads an intent file. The
     * port in the snapshot is the port of this run (p2p.c publishes its P2PLoadIni'd configuration). */
    DwOfflineRoute route;
    assert(!started.localRoute(1, &route)); /* nothing published yet */
    intent_published = 1;
    intent.size = sizeof(intent); intent.owner_uid = 77; intent.enabled = 1; intent.armed = 1; intent.port = c.port;
    lstrcpyA(intent.map, "mp_rust"); lstrcpyA(intent.session_map, "mp_rust"); lstrcpyA(intent.gametype, "war");
    lstrcpyA(intent.type, "mp"); lstrcpyA(intent.state, "armed");
    assert(started.localRoute(77, &route) && route.ownerUid == 77 && route.enabled && route.armed && !route.ready && !route.launched &&
           !route.failed && route.port == c.port && !strcmp(route.sessionMap, "mp_rust") && !strcmp(route.gametype, "war") &&
           !strcmp(route.category, "mp") && !strcmp(route.state, "armed"));
    int hostPort = 0;
    char why[256] = "x";
    auto_answer = 0;
    assert(!started.start(77, "zm_asylum_sh", "zm", "zm", 2, 1, &hostPort, why, sizeof(why)) && auto_calls == 1 &&
           !strcmp(auto_seen, "77/zm_asylum_sh/zm/zm/2/1") && !strcmp(why, "Restart the client before hosting another Local Play match."));
    assert(strstr(all_log, "REFUSED") == NULL); /* the refusal goes to the lobby's own log file, not Kernel33.log */
    auto_answer = 1;
    assert(started.start(77, "mp_rust", "war", "mp", -1, 0, &hostPort, why, sizeof(why)) == 1 && auto_calls == 2 && hostPort == 17999 &&
           !strcmp(auto_seen, "77/mp_rust/war/mp/-1/0"));
    started.leave(77);
    assert(leave_calls == 1 && leave_seen == 77);
    DwOfflineStartInfo info; memset(&info, 0, sizeof(info)); info.local = 1; info.hostPort = c.port;
    started.startGame(&info); /* a log line only */
    assert(auto_calls == 2 && leave_calls == 1);
    puts("offline lobby fixture passed");
    return 0;
}

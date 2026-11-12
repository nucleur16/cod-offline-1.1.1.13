/* The actual offline_lobby.c body (the Kernel33 side of offline mode) in an owned Windows fixture:
 * the switch, the in-memory dwauth.ini / stun.ini route, the profile name, the NAT traversal port
 * patch, the embedded host's folders and the Local Play hand-over. The lobby itself (offline/) and
 * C1 are doubles; nothing listens and no game is started.
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
static char last_log[1024];
static void Log(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vsnprintf(last_log, sizeof(last_log), fmt, ap); va_end(ap); }
static BOOL Readable(const void *p, SIZE_T n)
{
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(p, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
           (const BYTE *)p + n <= (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
}
static void WriteCode(void *dst, const void *src, SIZE_T n) { memcpy(dst, src, n); }

/* p2p.c, reduced to what offline_lobby.c reads. */
#define P2P_DEFAULT_PORT 17760
enum { P2P_IDLE, P2P_STARTING, P2P_READY, P2P_STOPPING, P2P_EXITED, P2P_FAILED };
typedef struct { char map[64], session[64], gametype[32], category[16]; int difficultyPresent, difficulty; } P2PRoomContext;
typedef struct P2PConfig {
    char state[MAX_PATH], profile[MAX_PATH], rconFile[MAX_PATH], rollout[MAX_PATH], netIp[64], mode[16], extra[512];
    int port, admission;
} P2PConfig;
static struct { int state; P2PConfig cfg; } g_p2p;
static CRITICAL_SECTION g_p2pLock;
static BOOL g_p2pInit, g_p2pEnabled, g_p2pHasRoom;
static P2PRoomContext g_p2pRoom;
static P2PConfig g_p2pIntentConfig;
static int hosted, protocol_mode;
static void P2PDefaults(P2PConfig *c) { memset(c, 0, sizeof(*c)); c->port = 17761; lstrcpyA(c->state, "X:\\lobby\\state"); }
static void P2PLoadIni(P2PConfig *c) { lstrcpyA(c->profile, "X:\\lobby\\resources"); lstrcpyA(c->rollout, "X:\\lobby\\rollout.txt"); lstrcpyA(c->extra, "+set x 1"); }
static int P2PSelectedProtocolMode(void) { return protocol_mode; }
static BOOL P2PStartupBusy(void) { return FALSE; }
static void P2PHostCurrentRoom(void) { ++hosted; }
typedef void (*K33TickFn)(void);
static K33TickFn registered_tick;
static BOOL K33OnMainTick(K33TickFn fn) { registered_tick = fn; return TRUE; }

/* The NAT traversal constant of C1 (0x00A50454) is a local double here. */
static BYTE nat_site[9] = { 0xB9, 0x02, 0x0C, 0x00, 0x00, 0x66, 0x89, 0x4F, 0x14 };
#define OFL_NATTRAV_PORT_VA ((DWORD_PTR)nat_site)

/* offline/dw_api.h doubles: record what Kernel33 asked for. */
#include "../offline/dw_api.h"
static DwOfflineConfig started;
static int start_calls, listen_fails;
int DwOfflineStartServer(const DwOfflineConfig *config) { ++start_calls; started = *config; return listen_fails ? 0 : config->port; }
void DwOfflineStopServer(void) {}
int DwOfflineRunning(void) { return start_calls && !listen_fails; }

#include "../offline_lobby.c"

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

typedef HANDLE (WINAPI *CreateFn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef LPSTR (WINAPI *CmdFn)(void);

int main(int argc, char **argv)
{
    assert(argc >= 2);
    const char *test = argv[1];
    InitializeCriticalSection(&g_p2pLock);
    g_p2pInit = TRUE;
    if (!strcmp(test, "badbytes")) nat_site[1] = 0x03;
    if (!strcmp(test, "nolisten")) listen_fails = 1;

    FARPROC realCreate = (FARPROC)CreateFileA, realCmd = (FARPROC)GetCommandLineA;
    FARPROC create = OfflineRouteImport(&kCreate, realCreate), cmd = OfflineRouteImport(&kCmd, realCmd);
    assert(OfflineRouteImport(&kOther, realCreate) == realCreate); /* never another slot */

    if (!strcmp(test, "off")) { /* no switch: nothing is wrapped, nothing starts */
        assert(create == realCreate && cmd == realCmd);
        assert(InstallOfflineLobby() && !OfflineLobbyActive() && !start_calls);
        P2PConfig c; P2PDefaults(&c); P2PLoadIni(&c); OfflineP2PConfig(&c);
        assert(!strcmp(c.state, "X:\\lobby\\state") && !strcmp(c.profile, "X:\\lobby\\resources"));
        puts("offline lobby fixture passed");
        return 0;
    }
    assert(create != realCreate && cmd != realCmd);
    /* Before the lobby is up the wrappers are transparent. */
    assert(((CmdFn)cmd)() == GetCommandLineA());

    if (!strcmp(test, "launcher") || !strcmp(test, "badbytes") || !strcmp(test, "nolisten")) {
        assert(!InstallOfflineLobby() && !OfflineLobbyActive());
        assert(!strcmp(test, "nolisten") ? start_calls == 1 : start_calls == 0);
        if (!strcmp(test, "badbytes")) assert(nat_site[1] == 0x03); /* a differing site is never written */
        puts("offline lobby fixture passed");
        return 0;
    }

    int port = !strcmp(test, "ini") ? 3999 : 3074;
    assert(InstallOfflineLobby() && OfflineLobbyActive() && start_calls == 1);
    assert(started.port == port && started.hostPort == 17761 && started.localRoute && started.startGame && started.log);
    assert(strstr(started.profile, "k33_offline\\profile") && strstr(started.state, "k33_offline\\state") &&
           strstr(started.matchProfile, "k33_offline\\match"));
    /* 3074 is C1's own NAT traversal port: only another port patches the constant. */
    DWORD imm; memcpy(&imm, nat_site + 1, 4);
    assert(imm == (DWORD)(port == 3074 ? 0xC02 : port));

    /* C1's reads of the two route files are answered from memory... */
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

    /* The profile name: appended only when the command line has none. */
    const char *line = ((CmdFn)cmd)();
    if (strstr(GetCommandLineA(), "dw_username")) assert(line == GetCommandLineA());
    else {
        const char *tail = strstr(line, " +set dw_username ");
        assert(tail && !strncmp(line, GetCommandLineA(), strlen(GetCommandLineA())));
        assert(!strcmp(tail + 18, !strcmp(test, "ini") ? "edtest" : "Player"));
        assert(((CmdFn)cmd)() == line); /* one stable string */
    }

    /* The embedded match host shares the offline store, whatever p2p_host.ini said. */
    P2PConfig c; P2PDefaults(&c); P2PLoadIni(&c); OfflineP2PConfig(&c);
    assert(!strcmp(c.state, started.state) && !strcmp(c.profile, started.matchProfile) && !c.rollout[0] && !c.extra[0] &&
           !strcmp(c.netIp, "127.0.0.1") && !strcmp(c.mode, "inprocess") && c.admission == 1);

    /* Local Play: the lobby thread sees the main thread's snapshot; a room start it answered starts
     * the embedded host once, on the main thread, and only while Local Play is selected. */
    InstallOfflineLobbyTick();
    assert(registered_tick == OfflineLobbyTick);
    DwOfflineRoute route;
    OfflineLobbyTick();
    assert(started.localRoute(1, &route) && !route.enabled);
    DwOfflineStartInfo info; memset(&info, 0, sizeof(info)); info.local = 1; info.hostPort = 17761;
    started.startGame(&info);
    OfflineLobbyTick();
    assert(hosted == 0); /* not selected: nothing is hosted */
    g_p2pEnabled = g_p2pHasRoom = TRUE;
    lstrcpyA(g_p2pRoom.map, "mp_rust"); lstrcpyA(g_p2pRoom.session, "mp_rust"); lstrcpyA(g_p2pRoom.gametype, "war"); lstrcpyA(g_p2pRoom.category, "mp");
    g_p2pIntentConfig.port = 17761;
    OfflineLobbyTick();
    assert(started.localRoute(1, &route) && route.enabled && route.port == 17761 && !strcmp(route.sessionMap, "mp_rust") &&
           !strcmp(route.gametype, "war") && !strcmp(route.category, "mp"));
    assert(hosted == 0);
    started.startGame(&info);
    OfflineLobbyTick(); OfflineLobbyTick();
    assert(hosted == 1);
    g_p2p.state = P2P_READY; /* already hosting (the player's own click): no second start */
    started.startGame(&info);
    OfflineLobbyTick();
    assert(hosted == 1);
    g_p2p.state = P2P_IDLE; protocol_mode = 2; /* a launcher-managed protocol owns its own start */
    started.startGame(&info);
    OfflineLobbyTick();
    assert(hosted == 1);
    puts("offline lobby fixture passed");
    return 0;
}

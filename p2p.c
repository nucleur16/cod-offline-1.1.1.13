/*
 * p2p.c - player-hosted matches (docs/P2P_HOSTING.md).
 *
 * The verified-script local path loads sh1x_host.dll in the hosting C1 process
 * and drives its server fiber from the game thread. The legacy child-process
 * path is retained in source but refused for P2P hosting until it can install
 * the same immutable script inventory.
 *
 *   p2phost [map] [gametype] [zonetype]   start hosting (defaults from p2p\p2p_host.ini)
 *   p2pstop                               rcon quit, then end the server's job
 *   p2pstatus                             state, address, pid, ready time, last sh1x_status line
 *   +set k33_p2pHost 1                    on the command line: start hosting while the client loads
 *
 * The lobby's LOCAL PLAY choice (p2p_localhost 1) only ARMS a host: it records and publishes the intent
 * ("state=armed launch=deferred") and starts nothing. The server is started when the match the room started
 * launches for this client (docs/P2P_DEFERRED_HOST_START.md): hostgate's sendto wrapper sees C1's first
 * datagram to the armed loopback port, or the in-process lobby calls K33LocalPlayLaunch().
 *
 * Every path and setting comes from <exe dir>\p2p\p2p_host.ini (key=value, '#' comments); the defaults
 * keep everything under <exe dir>\p2p\. The server is in a job object with KILL_ON_JOB_CLOSE, so it
 * ends with the client even when the client crashes. While it answers sh1x_status the module writes
 * <state>\p2p_host.txt (the host descriptor) and, with rollout= set, an "extra <port>" line into the
 * lobby's match_rollout.txt, which a lobby on the same PC (CODOL_OFFLINE stack) already reads to place
 * Ready presses on a server it did not start (alpha0022 handoff/match_instances.py). Both are removed
 * when the server ends.
 *
 * Threads: the commands run on C1's main thread (Cbuf) and only print and signal; the host thread does
 * the process work, the rcon polling and the files. g_p2p is shared under g_p2pLock.
 */
#include <wincrypt.h>
#include <ws2tcpip.h>
#include "p2p_match_options.h"
#include "p2p_status.h"
#include "p2p_intent_heartbeat.h"
#include "p2p_client_identity.h"
#include "p2p_local_play_api.h"

#define P2P_DEFAULT_PORT 17760
#define P2P_STATUS_MS    2000

enum { P2P_IDLE, P2P_STARTING, P2P_READY, P2P_STOPPING, P2P_EXITED, P2P_FAILED };
static const char *const kP2PState[] = { "idle", "starting", "ready", "stopping", "exited", "failed" };

typedef struct P2PConfig {
    char dir[MAX_PATH];          /* <exe dir>\p2p */
    char exe[MAX_PATH];          /* the server executable */
    char basepath[MAX_PATH];     /* fs_basepath: the client install (the server reads its zones) */
    char home[MAX_PATH];         /* fs_homepath */
    char scripts[MAX_PATH];      /* GSC root (codol_rawfiles) */
    char state[MAX_PATH];        /* securitykey.bin, game_admissions.txt, native_server_rcon.txt */
    char profile[MAX_PATH];      /* codol_profilePath: the lobby's resources folder */
    char rconFile[MAX_PATH];
    char rollout[MAX_PATH];      /* optional: the lobby's match_rollout.txt */
    char map[64], gametype[32], zoneType[16], sessionMap[64];
    char netIp[64], botDifficulty[16], extra[512];
    char mode[16];               /* inprocess (default): sh1x_host.dll on the game thread; child: the exe */
    char dll[MAX_PATH];          /* the server as a DLL (mode inprocess) */
    int port, bots, admission, developer, readyTimeout;
    int pmemMegs;                /* codol_pmemMegs for the server: its fastfile block (0 = the server default) */
    int c1Assets;                /* in-process: take C1's records of startup zones (default 1) */
    int c1Preload;               /* in-process: preloadmap when C1 is not loading a hosted map (default 0) */
    int c1Zones;                 /* in-process: map zones C1 holds are C1-backed, mounted only on demand (default 1) */
    unsigned long startupGeneration; /* zero is exclusively the legacy startup protocol */
    int difficultyPresent,difficulty; /* v2 UI intent; authenticated by the retained SDK */
} P2PConfig;

static DWORD g_p2pMainThread;
static BOOL g_p2pLaunchPending;
static P2PConfig g_p2pPendingConfig;
static BOOL P2PEnsureInprocActivation(void); /* p2p_inproc.c; local hook readiness only */

static struct {
    int state;
    DWORD pid;
    DWORD startTick, readyMs;
    DWORD exitCode;
    char status[512];            /* the last sh1x_status line */
    char error[256];
    P2PConfig cfg;
} g_p2p;
static CRITICAL_SECTION g_p2pLock;
static BOOL g_p2pInit;
static HANDLE g_p2pThread;
static volatile LONG g_p2pStop;
static int P2PGuestRouteMode(void);
static DWORD WINAPI P2PGuestThread(LPVOID);
static void P2PGuestTick(void);
/* Main-thread commands own issuance. Workers inspect copied state only: no C1
 * memory, UI dvars or caller-provided owner booleans authorize an attempt. */
static unsigned long g_p2pStartupCounter, g_p2pStartupActive;
static BOOL g_p2pStartupExhausted, g_p2pStartupEnabled, g_p2pStartupRoom;
static unsigned long long g_p2pStartupOwner;
static void P2PInprocCancelStartup(void); /* deny-only API, safe while native frame is yielded */
static BOOL P2PStartupAttemptCurrent(unsigned long generation)
{
    int mode=P2PSelectedProtocolMode();
    if (mode==0) return generation==0;
    if ((mode!=1&&mode!=2&&mode!=3) || !generation || !g_p2pInit) return FALSE;
    EnterCriticalSection(&g_p2pLock);
    BOOL current=!g_p2pStartupExhausted && generation==g_p2pStartupActive &&
        g_p2pStartupEnabled && g_p2pStartupRoom && g_p2pStartupOwner &&
        g_p2p.state!=P2P_FAILED && g_p2p.state!=P2P_EXITED &&
        !InterlockedCompareExchange(&g_p2pStop,0,0);
    LeaveCriticalSection(&g_p2pLock);
    return current;
}
static void P2PStartupInvalidate(void)
{
    P2PProductionCancel();
    EnterCriticalSection(&g_p2pLock);
    g_p2pLaunchPending=FALSE;
    g_p2pStartupActive=0;
    if (g_p2pStartupCounter==0xfffffffful) g_p2pStartupExhausted=TRUE;
    else ++g_p2pStartupCounter;
    InterlockedExchange(&g_p2pStop,1);
    LeaveCriticalSection(&g_p2pLock);
    if(P2PSelectedProtocolMode()!=0)P2PInprocCancelStartup();
}
static unsigned long P2PStartupIssue(void)
{
    unsigned long generation=0;
    EnterCriticalSection(&g_p2pLock);
    if (g_p2pStartupCounter==0xfffffffful) g_p2pStartupExhausted=TRUE;
    if (!g_p2pStartupExhausted && g_p2pStartupEnabled && g_p2pStartupRoom && g_p2pStartupOwner) {
        generation=++g_p2pStartupCounter;
        g_p2pStartupActive=generation;
        InterlockedExchange(&g_p2pStop,0);
    }
    LeaveCriticalSection(&g_p2pLock);
    return generation;
}
static char g_p2pArgMap[64], g_p2pArgGametype[32], g_p2pArgZone[16];
static P2PConfig g_p2pOverride;
static BOOL g_p2pHasOverride;
struct P2PConfig;
static int P2PInprocRun(const struct P2PConfig *c, const char *password, const char *args); /* p2p_inproc.c */
static BOOL P2PInprocUsed(void); /* p2p_inproc.c: an in-process session already ran in this client */
static BOOL P2PInprocPrincipalFresh(void); /* game-thread native principal observation */

static void P2PSetState(int state, const char *error)
{
    EnterCriticalSection(&g_p2pLock);
    g_p2p.state = state;
    if (error) lstrcpynA(g_p2p.error, error, sizeof(g_p2p.error));
    LeaveCriticalSection(&g_p2pLock);
}

/* ---- deferred launch (docs/P2P_DEFERRED_HOST_START.md) -------------------------------------
 * Arming is main-thread state. The two launch inputs may come from any thread, so they are one atomic
 * word each: a worker or C1's network path never starts a host or touches C1; P2PLaunchTick (main
 * thread) consumes the signal and starts the armed selection, once. */
enum { P2P_LAUNCH_NONE, P2P_LAUNCH_DATAGRAM, P2P_LAUNCH_EXPLICIT };
#define P2P_LAUNCH_EMPTY_MS 120000u      /* a launched host nobody is on is stopped after this long */
static volatile LONG g_p2pLaunchSignal;  /* P2P_LAUNCH_*: raised on any thread, consumed on the main thread */
static volatile LONG g_p2pLaunchPort;    /* the armed loopback port while a launch may still be observed, else 0 */
static BOOL g_p2pArmed;                  /* an enabled, validated selection waits for its launch */
static BOOL g_p2pLaunched;               /* that selection's one host start was issued */
static BOOL g_p2pStartPermitted;         /* protocol 1/2: a matching Ready/Start intent was accepted */
static DWORD g_p2pLaunchTick, g_p2pEmptySince;
static BOOL g_p2pLaunchAnswered, g_p2pLaunchAborted; /* the launched host became ready / C1's connect was ended */
static K33LocalPlayIntentInfo g_p2pApi;  /* what the tick last published, under g_p2pLock, for other threads */

/* Any thread: hostgate's sendto wrapper reports every loopback datagram C1 sends. */
static void P2PLaunchObserve(unsigned port)
{
    if (port && (LONG)port == InterlockedCompareExchange(&g_p2pLaunchPort, 0, 0))
        InterlockedCompareExchange(&g_p2pLaunchSignal, P2P_LAUNCH_DATAGRAM, P2P_LAUNCH_NONE);
}
static void P2PDisarm(void)
{
    InterlockedExchange(&g_p2pLaunchPort, 0);
    InterlockedExchange(&g_p2pLaunchSignal, P2P_LAUNCH_NONE);
    g_p2pArmed = g_p2pLaunched = g_p2pStartPermitted = FALSE;
    g_p2pLaunchAnswered = g_p2pLaunchAborted = FALSE;
    g_p2pEmptySince = 0;
}
static void P2PArmLaunch(int port)
{
    g_p2pArmed = TRUE; g_p2pLaunched = FALSE; g_p2pEmptySince = 0;
    g_p2pLaunchAnswered = g_p2pLaunchAborted = FALSE;
    InterlockedExchange(&g_p2pLaunchSignal, P2P_LAUNCH_NONE);
    InterlockedExchange(&g_p2pLaunchPort, port);
}
static const char *P2PLaunchLabel(void)
{
    return !g_p2pArmed ? "none" : g_p2pLaunched ? "started" : "deferred";
}

/* ---- configuration ------------------------------------------------------------------------- */

static void P2PTrim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    char *b = s;
    while (*b == ' ' || *b == '\t') b++;
    if (b != s) memmove(s, b, strlen(b) + 1);
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') { memmove(s, s + 1, n - 2); s[n - 2] = 0; }
}

/* Identifiers that become command-line dvar values: the same rule as the lobby's handoff
 * (tools/codol_map_handoff.ps1), so a map or gametype can never add console commands. */
static BOOL P2PIdent(const char *s, size_t max)
{
    size_t n = strlen(s);
    if (!n || n > max || s[0] < 'a' || s[0] > 'z') return FALSE;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_')) return FALSE;
    return TRUE;
}

static BOOL P2PPathOk(const char *s) { return !strchr(s, '"'); }

static void P2PDefaults(P2PConfig *c)
{
    memset(c, 0, sizeof(*c));
    char root[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, root, MAX_PATH);
    while (n && root[n - 1] != '\\') n--;
    root[n ? n - 1 : 0] = 0;
    lstrcpynA(c->basepath, root, MAX_PATH);
    _snprintf(c->dir, MAX_PATH, "%s\\p2p", root);
    _snprintf(c->exe, MAX_PATH, "%s\\k33p2p_dedrun.exe", c->dir);
    _snprintf(c->home, MAX_PATH, "%s\\home", c->dir);
    _snprintf(c->dll, MAX_PATH, "%s\\sh1x_host.dll", c->dir);
    /* The GSC tree installed with the client (main\sh1x_rawfiles, docs/P2P_HOSTING.md). */
    _snprintf(c->scripts, MAX_PATH, "%s\\main\\sh1x_rawfiles", root);
    lstrcpyA(c->mode, "inprocess");
    _snprintf(c->state, MAX_PATH, "%s\\state", c->dir);
    _snprintf(c->profile, MAX_PATH, "%s\\resources", c->dir);
    lstrcpyA(c->map, "mp_killhouse_sh");
    lstrcpyA(c->gametype, "war");
    lstrcpyA(c->zoneType, "mp");
    lstrcpyA(c->netIp, "0.0.0.0");
    lstrcpyA(c->botDifficulty, "recruit");
    c->port = P2P_DEFAULT_PORT;
    c->admission = 1;
    c->readyTimeout = 180;
    /* The fastfile block (PMem) held 1.5 MB on mp_killhouse_sh, mp_rust and mp_crash_tropical (sh1x_mem,
     * 2026-10-27); the server default reserves 328 MB of the shared address space. */
    c->pmemMegs = 32;
    c->c1Assets = 1;
    c->c1Zones = 1;
}

#if CODOL_OFFLINE_LOBBY
static void OfflineP2PConfig(P2PConfig *c); /* offline_lobby.c: the offline store's state/profile folders */
#else
#define OfflineP2PConfig(c) ((void)0)
#endif
static void P2PLoadIni(P2PConfig *c)
{
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%s\\p2p_host.ini", c->dir);
    path[MAX_PATH - 1] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) { Log("p2p: no %s, defaults used", path); OfflineP2PConfig(c); return; }
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = line, *val = eq + 1;
        P2PTrim(key);
        P2PTrim(val);
        struct { const char *k; char *dst; size_t n; } strs[] = {
            { "exe", c->exe, MAX_PATH }, { "basepath", c->basepath, MAX_PATH }, { "home", c->home, MAX_PATH },
            { "scripts", c->scripts, MAX_PATH }, { "state", c->state, MAX_PATH }, { "profile", c->profile, MAX_PATH },
            { "rcon_file", c->rconFile, MAX_PATH }, { "rollout", c->rollout, MAX_PATH },
            { "map", c->map, sizeof(c->map) }, { "gametype", c->gametype, sizeof(c->gametype) },
            { "zone_type", c->zoneType, sizeof(c->zoneType) }, { "session_map", c->sessionMap, sizeof(c->sessionMap) },
            { "net_ip", c->netIp, sizeof(c->netIp) }, { "bot_difficulty", c->botDifficulty, sizeof(c->botDifficulty) },
            { "extra", c->extra, sizeof(c->extra) },
            { "mode", c->mode, sizeof(c->mode) }, { "dll", c->dll, MAX_PATH },
        };
        struct { const char *k; int *dst; } ints[] = {
            { "port", &c->port }, { "bots", &c->bots }, { "admission_required", &c->admission },
            { "developer", &c->developer }, { "ready_timeout", &c->readyTimeout }, { "pmem_megs", &c->pmemMegs }, { "c1_assets", &c->c1Assets }, { "c1_preload", &c->c1Preload }, { "c1_zones", &c->c1Zones },
        };
        BOOL known = FALSE;
        for (unsigned i = 0; i < sizeof(strs) / sizeof(strs[0]); i++)
            if (!_stricmp(key, strs[i].k)) { lstrcpynA(strs[i].dst, val, (int)strs[i].n); known = TRUE; }
        for (unsigned i = 0; i < sizeof(ints) / sizeof(ints[0]); i++)
            if (!_stricmp(key, ints[i].k)) { *ints[i].dst = atoi(val); known = TRUE; }
        if (!known && *key) Log("p2p: %s: unknown key \"%s\"", path, key);
    }
    fclose(f);
    Log("p2p: settings from %s", path);
    OfflineP2PConfig(c);
}

/* Installed, authored Survival maps published with the rawfiles deployment. */
static int P2PSurvivalMaps(char (*maps)[64], int maximum, const char *wanted)
{
    P2PConfig c; P2PDefaults(&c);
    char path[MAX_PATH]; _snprintf(path, sizeof(path), "%s\\survival_maps.txt", c.dir);
    path[sizeof(path)-1] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int count = 0, found = 0; char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *comment = strchr(line, '#'); if (comment) *comment = 0;
        P2PTrim(line);
        if (!P2PIdent(line, 63)) continue;
        if (wanted && !strcmp(line, wanted)) found = 1;
        if (maps && count < maximum) lstrcpynA(maps[count++], line, 64);
    }
    fclose(f);
    return wanted ? found : count;
}

/* -> NULL when usable, else what is wrong. */
static const char *P2PCheck(const P2PConfig *c)
{
    if (c->port < 1024 || c->port > 65535) return "port must be 1024..65535";
    if (!P2PIdent(c->map, 63) || !P2PIdent(c->sessionMap[0] ? c->sessionMap : c->map, 63)) return "bad map name";
    if (!P2PIdent(c->gametype, 31)) return "bad gametype";
    if (strcmp(c->zoneType, "mp") && strcmp(c->zoneType, "coop") && strcmp(c->zoneType, "zm") && strcmp(c->zoneType, "survival")) return "zone type must be mp, coop, zm or survival";
    if (!strcmp(c->zoneType, "survival")) {
        if (strcmp(c->gametype, "survival")) return "Survival requires gametype survival";
        if (!P2PSurvivalMaps(NULL, 0, c->map)) return "Survival map is absent from p2p/survival_maps.txt (installed maps with placements)";
    }
    for (const char *p = c->netIp; *p; p++) if (!((*p >= '0' && *p <= '9') || *p == '.')) return "net_ip must be an IPv4 address";
    if (!P2PPathOk(c->exe) || !P2PPathOk(c->basepath) || !P2PPathOk(c->home) || !P2PPathOk(c->scripts) ||
        !P2PPathOk(c->state) || !P2PPathOk(c->profile) || !P2PPathOk(c->rconFile) || !P2PPathOk(c->rollout))
        return "a path holds a quote";
    BOOL inproc = !_stricmp(c->mode, "inprocess");
    if (!inproc && _stricmp(c->mode, "child")) return "mode must be inprocess or child";
    if (c->admission != 1)
        return "P2P hosting requires DW admission";
    /* Offline mode hosts for this one player on loopback with the GSC tree installed with the client
     * (docs/OFFLINE_LOBBY.md); every other host needs the verified script service. */
    if (!(OfflineLobbyActive() && inproc) && !P2PVerifiedScriptsAllowsMode(inproc))
        return "P2P hosting requires the verified script service and a local-testing in-process host";
    /* This field is appended after the required +set options. Keep the local
     * trial's two known lifecycle switches, but never let it override an
     * admission, script or network cvar (or inject another console command). */
    if (c->extra[0] && strcmp(c->extra,
            "+set codol_exitToLobby 1 +set codol_oneShotMatch 1"))
        return "P2P extra commands are not permitted by the local host policy";
    if (!P2PPathOk(c->dll)) return "a path holds a quote";
    if (inproc && GetFileAttributesA(c->dll) == INVALID_FILE_ATTRIBUTES) return "server DLL not found (dll=)";
    if (!inproc && GetFileAttributesA(c->exe) == INVALID_FILE_ATTRIBUTES) return "server executable not found (exe=)";
    /* The selected service supplies the complete verified inventory from
     * memory. There is no loose-file fallback for a player-hosted game. */
    return NULL;
}

/* ---- helpers ------------------------------------------------------------------------------- */

static void P2PMakeDirs(const char *path)
{
    char tmp[MAX_PATH];
    lstrcpynA(tmp, path, MAX_PATH);
    for (char *p = tmp + 3; *p; p++)
        if (*p == '\\' || *p == '/') { char s = *p; *p = 0; CreateDirectoryA(tmp, NULL); *p = s; }
    CreateDirectoryA(tmp, NULL);
}

/* The lobby and this server share one rcon secret file (native_server_rcon.txt, as
 * START-CODOL-SERVER.cmd makes it): read it, or create it with 16 random bytes in hex. */
static BOOL P2PRconPassword(const char *file, char out[65])
{
    FILE *f = fopen(file, "rb");
    if (f) {
        char buf[128] = { 0 };
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[n] = 0;
        P2PTrim(buf);
        size_t len = strlen(buf);
        BOOL ok = len >= 8 && len <= 64;
        for (size_t i = 0; ok && i < len; i++) ok = buf[i] > 32 && buf[i] < 127 && buf[i] != '"' && buf[i] != ';';
        if (ok) { lstrcpyA(out, buf); return TRUE; }
        Log("p2p: %s is not a usable rcon password; not replacing it", file);
        return FALSE;
    }
    HCRYPTPROV prov = 0;
    BYTE raw[16];
    if (!CryptAcquireContextW(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return FALSE;
    BOOL ok = CryptGenRandom(prov, sizeof(raw), raw);
    CryptReleaseContext(prov, 0);
    if (!ok) return FALSE;
    for (int i = 0; i < 16; i++) sprintf(out + 2 * i, "%02x", raw[i]);
    out[32] = 0;
    char tmp[MAX_PATH + 8];
    _snprintf(tmp, sizeof(tmp), "%s.tmp", file);
    tmp[sizeof(tmp) - 1] = 0;
    f = fopen(tmp, "wb");
    if (!f) return FALSE;
    fprintf(f, "%s\n", out);
    fclose(f);
    if (!MoveFileExA(tmp, file, MOVEFILE_REPLACE_EXISTING)) { DeleteFileA(tmp); return FALSE; }
    Log("p2p: created rcon password file %s", file);
    return TRUE;
}

static BOOL P2PPortFree(const char *ip, int port)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return TRUE; /* cannot tell; the server reports its own bind failure */
    BOOL excl = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&excl, sizeof(excl));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = inet_addr(ip[0] ? ip : "0.0.0.0");
    BOOL ok = bind(s, (struct sockaddr *)&a, sizeof(a)) == 0;
    closesocket(s);
    return ok;
}

/* One rcon command over loopback; accept replies only from that server socket. */
static int P2PRcon(int port, const char *password, const char *command, char *out, int outSize, DWORD timeoutMs)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return -1;
    char pkt[512];
    int n = _snprintf(pkt, sizeof(pkt), "\xff\xff\xff\xffrcon \"%s\" %s", password, command);
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int got = 0;
    /* A connected UDP socket filters replies by source address and port. An
     * unconnected recv could accept a forged sh1x_status readiness line. */
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }
    if (n > 0 && n < (int)sizeof(pkt) && send(s, pkt, n, 0) == n && out && outSize > 1) {
        DWORD start = GetTickCount();
        for (;;) {
            DWORD left = timeoutMs - (GetTickCount() - start);
            if ((int)left <= 0) break;
            fd_set rd;
            FD_ZERO(&rd);
            FD_SET(s, &rd);
            struct timeval tv = { (long)(left / 1000), (long)(left % 1000) * 1000 };
            if (select(0, &rd, NULL, NULL, &tv) <= 0) break;
            char buf[1500];
            int r = recv(s, buf, sizeof(buf), 0);
            if (r <= 4) break;
            int len = r - 4;
            const char *text = buf + 4;
            if (len >= 6 && !memcmp(text, "print\n", 6)) { text += 6; len -= 6; }
            if (got + len >= outSize) len = outSize - 1 - got;
            memcpy(out + got, text, len);
            got += len;
            out[got] = 0;
            if (strstr(out, "sh1x_status ") && memchr(out, '\n', got)) { /* one line is all we ask for */
                out[got] = 0;
                if (strstr(strstr(out, "sh1x_status "), "\n")) break;
            }
        }
        out[got] = 0;
    }
    closesocket(s);
    return got;
}

/* The sh1x_status line from a reply, without the newline. */
static BOOL P2PStatusLine(const char *reply, char *out, int outSize)
{
    const char *p = strstr(reply, "sh1x_status ");
    if (!p) return FALSE;
    int n = 0;
    while (p[n] && p[n] != '\n' && p[n] != '\r' && n < outSize - 1) n++;
    memcpy(out, p, n);
    out[n] = 0;
    return TRUE;
}

/* The lobby rollout file (handoff/match_instances.py _read_rollout): add or drop our "extra <port>"
 * line, keeping every other line. Written through a temp file and a rename, as the lobby's own. */
static void P2PRollout(const char *file, int port, BOOL add)
{
    if (!file[0]) return;
    char mine[64];
    _snprintf(mine, sizeof(mine), "extra %d", port);
    char *text = NULL;
    size_t size = 0;
    FILE *f = fopen(file, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len > 0 && len < 65536) {
            text = (char *)malloc((size_t)len + 1);
            if (text) { size = fread(text, 1, (size_t)len, f); text[size] = 0; }
        }
        fclose(f);
    }
    char tmp[MAX_PATH + 8];
    _snprintf(tmp, sizeof(tmp), "%s.k33tmp", file);
    tmp[sizeof(tmp) - 1] = 0;
    FILE *o = fopen(tmp, "wb");
    if (!o) { free(text); Log("p2p: cannot write %s", tmp); return; }
    for (char *line = text, *next; line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        if (!next && !*line) break; /* the end of a file whose last line has its newline */
        size_t len = strlen(line);
        if (len && line[len - 1] == '\r') line[len - 1] = 0;
        char copy[256];
        lstrcpynA(copy, line, sizeof(copy));
        char *hash = strchr(copy, '#');
        if (hash) *hash = 0;
        P2PTrim(copy);
        if (!strcmp(copy, mine)) continue; /* ours: dropped, re-added below when adding */
        fprintf(o, "%s\n", line);
    }
    if (add) fprintf(o, "%s    # Kernel33 player-hosted server, client pid %lu\n", mine, GetCurrentProcessId());
    fclose(o);
    free(text);
    if (!MoveFileExA(tmp, file, MOVEFILE_REPLACE_EXISTING)) { DeleteFileA(tmp); Log("p2p: cannot replace %s", file); return; }
    Log("p2p: %s \"%s\" %s %s", add ? "registered" : "unregistered", mine, add ? "in" : "from", file);
}

static void P2PDescriptor(const P2PConfig *c, DWORD pid, BOOL ready)
{
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%s\\p2p_runtime.txt", c->state);
    path[MAX_PATH - 1] = 0;
    if (!ready) { DeleteFileA(path); return; }
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "port=%d\nnet_ip=%s\nserver_pid=%lu\nclient_pid=%lu\nmap=%s\ngametype=%s\ntype=%s\n", c->port, c->netIp,
            pid, GetCurrentProcessId(), c->map, c->gametype, c->zoneType);
    fclose(f);
}

/* ---- the host thread ----------------------------------------------------------------------- */

static DWORD WINAPI P2PHostThread(LPVOID unused)
{
    (void)unused;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    if (!g_p2pHasOverride) { P2PSetState(P2P_FAILED,"No frozen host configuration.");WSACleanup();return 1; }
    P2PConfig c = g_p2pOverride;
    if (g_p2pArgMap[0]) { lstrcpyA(c.map, g_p2pArgMap); c.sessionMap[0] = 0; }
    if (g_p2pArgGametype[0]) lstrcpyA(c.gametype, g_p2pArgGametype);
    if (g_p2pArgZone[0]) lstrcpyA(c.zoneType, g_p2pArgZone);
    if (!P2PStartupAttemptCurrent(c.startupGeneration)) {
        P2PSetState(P2P_FAILED,"The local startup attempt is no longer current.");WSACleanup();return 1;
    }
    if (!c.rconFile[0]) _snprintf(c.rconFile, MAX_PATH, "%s\\native_server_rcon.txt", c.state);
    EnterCriticalSection(&g_p2pLock);
    g_p2p.cfg = c;
    LeaveCriticalSection(&g_p2pLock);

    const char *bad = P2PCheck(&c);
    if (bad) { Log("p2p: not hosting: %s", bad); P2PSetState(P2P_FAILED, bad); WSACleanup(); return 1; }
    if (!P2PPortFree(c.netIp, c.port)) {
        Log("p2p: not hosting: udp %s:%d is in use", c.netIp, c.port);
        P2PSetState(P2P_FAILED, "the UDP port is in use");
        WSACleanup();
        return 1;
    }
    char homeMain[MAX_PATH + 8];
    _snprintf(homeMain, sizeof(homeMain), "%s\\main", c.home);
    homeMain[sizeof(homeMain) - 1] = 0;
    P2PMakeDirs(homeMain);
    P2PMakeDirs(c.state);
    char password[65];
    password[0]=0;
    if (P2PProductionMode()==0 && !P2PRconPassword(c.rconFile, password)) {
        Log("p2p: not hosting: no rcon password (%s)", c.rconFile);
        P2PSetState(P2P_FAILED, "no rcon password");
        WSACleanup();
        return 1;
    }

    static char cmd[8192];
    BOOL inproc = !_stricmp(c.mode, "inprocess");
    char pmem[48] = "";
    char relayArg[MAX_PATH + 96] = "";
    char relayPath[MAX_PATH + 64];
    if (c.pmemMegs >= 32 && c.pmemMegs <= 2048) _snprintf(pmem, sizeof(pmem), "+set codol_pmemMegs %d", c.pmemMegs);
    /* The explicit local Host/Guest trial arms this short-lived file before
     * forwarding any Guest packet. Normal solo hosting leaves it absent. */
    _snprintf(relayPath, sizeof(relayPath), "%s\\p2p_relay_identity.txt", c.state);
    relayPath[sizeof(relayPath)-1] = 0;
    _snprintf(relayArg, sizeof(relayArg), "+set codol_relayIdentityPath \"%s\"", relayPath);
    relayArg[sizeof(relayArg)-1] = 0;
    _snprintf(cmd, sizeof(cmd),
        "\"%s\" +set dedicated 2 +set fs_basepath \"%s\" +set fs_homepath \"%s\" +set net_ip %s +set net_port %d "
        "+set rcon_password \"%s\" +set logfile 2 +set logrcon 1 +set developer %d +set sv_fps 60 "
        "+set codol_scriptPath \"%s\" +set codol_scriptOverlay \"%s\" +set codol_scriptNativeOverlay \"%s\" "
        "+set codol_securityKeyPath \"%s\\securitykey.bin\" +set codol_admissionPath \"%s\\game_admissions.txt\" "
        "+set codol_admissionRequired %d %s +set codol_profilePath \"%s\" +set codol_zoneType %s +set codol_sessionMap %s "
        "+set g_gametype %s +set codol_hostParentPid %lu %s "
        "+codol_scriptPath +codol_scriptOverlay +codol_scriptNativeOverlay +map %s %s",
        c.exe, c.basepath, c.home, c.netIp, c.port, password, c.developer, c.scripts, c.scripts, c.scripts,
        c.state, c.state, c.admission ? 1 : 0, relayArg, c.profile, c.zoneType, c.sessionMap[0] ? c.sessionMap : c.map,
        c.gametype, inproc ? 0ul : GetCurrentProcessId(), pmem, c.map, c.extra);
    cmd[sizeof(cmd) - 1] = 0;
    if (inproc) {
        /* The same command line without the exe: Sh1xHost_Init takes what follows it. */
        const char *args = strchr(cmd + 1, '"');
        int r = P2PInprocRun(&c, password, args ? args + 2 : cmd);
        WSACleanup();
        return r;
    }

    HANDLE job = CreateJobObjectA(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim;
        memset(&lim, 0, sizeof(lim));
        lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &lim, sizeof(lim));
    }
    char workDir[MAX_PATH];
    lstrcpynA(workDir, c.exe, MAX_PATH);
    char *slash = strrchr(workDir, '\\');
    if (slash) *slash = 0;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    DWORD t0 = GetTickCount();
    EnterCriticalSection(&g_p2pLock);
    g_p2p.startTick = t0;
    g_p2p.readyMs = 0;
    g_p2p.status[0] = 0;
    g_p2p.error[0] = 0;
    LeaveCriticalSection(&g_p2pLock);
    if (!CreateProcessA(c.exe, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, workDir, &si, &pi)) {
        Log("p2p: CreateProcess failed (%lu): %s", GetLastError(), c.exe);
        P2PSetState(P2P_FAILED, "the server did not start");
        if (job) CloseHandle(job);
        WSACleanup();
        return 1;
    }
    if (job && !AssignProcessToJobObject(job, pi.hProcess))
        Log("p2p: job assignment failed (%lu); the server will not end with a crashed client", GetLastError());
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    EnterCriticalSection(&g_p2pLock);
    g_p2p.pid = pi.dwProcessId;
    LeaveCriticalSection(&g_p2pLock);
    Log("p2p: server pid %lu started: %s %s on %s:%d (home %s, state %s)", pi.dwProcessId, c.map, c.gametype,
        c.netIp, c.port, c.home, c.state);

    BOOL ready = FALSE, registered = FALSE;
    DWORD lastPoll = 0, lastAnswer = 0;
    for (;;) {
        DWORD w = WaitForSingleObject(pi.hProcess, ready ? P2P_STATUS_MS : 500);
        if (w == WAIT_OBJECT_0) break;
        if (g_p2pStop) {
            P2PSetState(P2P_STOPPING, NULL);
            char reply[256];
            P2PRcon(c.port, password, "quit", reply, sizeof(reply), 300);
            if (WaitForSingleObject(pi.hProcess, 5000) != WAIT_OBJECT_0) {
                Log("p2p: server did not quit within 5 s; ending its job");
                if (job) TerminateJobObject(job, 1); else TerminateProcess(pi.hProcess, 1);
                WaitForSingleObject(pi.hProcess, 5000);
            }
            break;
        }
        DWORD now = GetTickCount();
        if (now - lastPoll < (ready ? P2P_STATUS_MS : 1000)) continue;
        lastPoll = now;
        char reply[2048], line[512];
        if (P2PRcon(c.port, password, "sh1x_status", reply, sizeof(reply), 400) > 0 &&
            P2PStatusLine(reply, line, sizeof(line)) &&
            P2PStatusMatches(line,c.map,c.sessionMap,c.gametype,c.zoneType)) {
            lastAnswer = now;
            EnterCriticalSection(&g_p2pLock);
            lstrcpynA(g_p2p.status, line, sizeof(g_p2p.status));
            if (!ready) g_p2p.readyMs = now - t0;
            g_p2p.state = P2P_READY;
            LeaveCriticalSection(&g_p2pLock);
            if (!ready) {
                ready = TRUE;
                Log("p2p: server ready after %lu ms: %s", now - t0, line);
                P2PDescriptor(&c, pi.dwProcessId, TRUE);
                if (c.rollout[0]) { P2PRollout(c.rollout, c.port, TRUE); registered = TRUE; }
            }
        } else if (ready && now - lastAnswer > 20000) {
            /* A fatal error (a script compile error, a failed map load) leaves the process alive with its
             * network shut down: nobody can join it, so it is not hosting any more. */
            Log("p2p: server stopped answering sh1x_status for 20 s (see its qconsole.log); stopping it");
            P2PSetState(P2P_FAILED, "the server stopped answering");
            if (job) TerminateJobObject(job, 1); else TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            break;
        } else if (!ready && (int)(now - t0) > c.readyTimeout * 1000) {
            Log("p2p: server gave no sh1x_status within %d s; stopping it", c.readyTimeout);
            P2PSetState(P2P_FAILED, "the server did not answer in time");
            if (job) TerminateJobObject(job, 1); else TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            break;
        } else if (ready) {
            /* Stop releasing lobby Start while the previously ready match is
             * loading/recovering or has stopped answering. A later exact
             * status restores READY; the existing 20-second failure gate stays. */
            P2PSetState(P2P_STARTING, NULL);
        }
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    if (job) CloseHandle(job);
    if (registered) P2PRollout(c.rollout, c.port, FALSE);
    P2PDescriptor(&c, 0, FALSE);
    EnterCriticalSection(&g_p2pLock);
    g_p2p.exitCode = code;
    if (g_p2p.state != P2P_FAILED) g_p2p.state = P2P_EXITED;
    g_p2p.pid = 0;
    LeaveCriticalSection(&g_p2pLock);
    Log("p2p: server ended (exit code %lu) after %lu ms", code, GetTickCount() - t0);
    WSACleanup();
    return 0;
}

/* Main thread or TakeOver. -> FALSE when a server is already being hosted. */
static BOOL P2PStartConfigured(const char *map, const char *gametype, const char *zone, const P2PConfig *override)
{
    /* All Cbuf/UI/tick requests converge here. No IPC/background thread may patch C1. */
    if (!g_p2pMainThread || GetCurrentThreadId()!=g_p2pMainThread || g_p2pLaunchPending) return FALSE;
    if (!P2PStartupAttemptCurrent(override ? override->startupGeneration:0)) return FALSE;
    if (g_p2pThread) {
        if (WaitForSingleObject(g_p2pThread, 0) != WAIT_OBJECT_0) return FALSE;
        CloseHandle(g_p2pThread);
        g_p2pThread = NULL;
    }
    int guest=P2PGuestRouteMode();
    if(guest<0){P2PSetState(P2P_FAILED,"The Guest binding route is unavailable.");return FALSE;}
    P2PConfig selected;
    if(override) selected=*override;
    else { P2PDefaults(&selected);P2PLoadIni(&selected); }
    if(map && *map){lstrcpynA(selected.map,map,sizeof(selected.map));selected.sessionMap[0]=0;}
    if(gametype && *gametype)lstrcpynA(selected.gametype,gametype,sizeof(selected.gametype));
    if(zone && *zone)lstrcpynA(selected.zoneType,zone,sizeof(selected.zoneType));
    if(guest==0) {
        const char *bad=P2PCheck(&selected);
        if(bad){P2PSetState(P2P_FAILED,bad);return FALSE;}
        if(!_stricmp(selected.mode,"inprocess")) {
            if(P2PInprocUsed()) {
                P2PSetState(P2P_FAILED,"Restart the client before hosting another match.");return FALSE;
            }
            if(!g_k33TickInstalled) {
                /* Explicit TakeOver autostart waits for the ORIGINAL dispatcher.
                 * Never invert its chain with a late Com_EventLoop redirect. */
                g_p2pPendingConfig=selected;g_p2pLaunchPending=TRUE;
                InterlockedExchange(&g_p2pStop,0);P2PSetState(P2P_STARTING,"");return TRUE;
            }
            if(!P2PStartupAttemptCurrent(selected.startupGeneration) || !P2PEnsureInprocActivation()) {
                P2PSetState(P2P_FAILED,"Local host frame activation is unavailable. Restart the client.");return FALSE;
            }
        } /* Retained explicit child route has NO Pi activation or Pi-once dependency. */
    }
    /* The worker must not reread a changed INI or switch mode after activation. */
    g_p2pHasOverride=TRUE;g_p2pOverride=selected;
    g_p2pArgMap[0]=g_p2pArgGametype[0]=g_p2pArgZone[0]=0;
    InterlockedExchange(&g_p2pStop,0);P2PSetState(P2P_STARTING,"");
    g_p2pThread=CreateThread(NULL,0,guest==1?P2PGuestThread:P2PHostThread,NULL,0,NULL);
    if(!g_p2pThread){P2PSetState(P2P_FAILED,"no host thread");return FALSE;}
    return TRUE;
}

static BOOL P2PStart(const char *map, const char *gametype, const char *zone)
{
    return P2PStartConfigured(map, gametype, zone, NULL);
}

/* Scaleform calls the normal main-thread console dispatcher. Publish only
 * copied state through C1's own dvars; never touch the movie from the host thread. */
static unsigned g_p2pLobbyRequest;
static char g_p2pLobbyMap[64], g_p2pLobbyError[256];
static P2PRoomContext g_p2pRoom;
static P2PConfig g_p2pLobbyNext;
static BOOL g_p2pLobbyPending, g_p2pEnabled, g_p2pHasRoom;
static P2PConfig g_p2pIntentConfig;
static unsigned long long g_p2pIntentUid;
static DWORD g_p2pIntentAt;
static P2PIntentHeartbeat g_p2pIntentHeartbeat;
static BOOL P2PStartupBusy(void)
{
    return g_p2pLaunchPending || (g_p2pThread && WaitForSingleObject(g_p2pThread,0)!=WAIT_OBJECT_0);
}
#include "p2p_guest_binding.c"

static void P2PStartupSnapshot(void)
{
    EnterCriticalSection(&g_p2pLock);
    g_p2pStartupEnabled=g_p2pEnabled;
    g_p2pStartupRoom=g_p2pHasRoom;
    g_p2pStartupOwner=g_p2pIntentUid;
    LeaveCriticalSection(&g_p2pLock);
}
/* Arming records selected local intent. Only a subsequent explicit Ready/Start
 * command can issue a generation and create the native host thread. */
static void P2PArmCurrentRoom(void)
{
    P2PConfig c;
    P2PDefaults(&c);P2PLoadIni(&c);
    lstrcpyA(c.map,g_p2pRoom.map);lstrcpyA(c.sessionMap,g_p2pRoom.session);
    lstrcpyA(c.gametype,g_p2pRoom.gametype);lstrcpyA(c.zoneType,g_p2pRoom.category);
    c.difficultyPresent=g_p2pRoom.difficultyPresent;c.difficulty=g_p2pRoom.difficulty;
    lstrcpyA(c.netIp,"127.0.0.1");lstrcpyA(c.mode,"inprocess");
    g_p2pIntentConfig=c;
    lstrcpyA(g_p2pLobbyMap,c.sessionMap);
    P2PStartupSnapshot();
    int protocol=P2PSelectedProtocolMode();
    const char *bad=(protocol!=1&&protocol!=2&&protocol!=3) ? "The local startup protocol configuration is invalid.":
        !g_p2pHasRoom ? "Choose a match lobby first.":
        !g_p2pIntentUid ? "Player identity is not ready for local hosting.":
        P2PGuestRouteMode()<0 ? "The Guest binding route is unavailable.":
        P2PGuestRouteMode()==1 ? NULL:
        P2PProductionMode()>0 ? NULL:
        P2PInprocUsed() ? "Restart the client before hosting another Local Play match.":P2PCheck(&c);
    if (bad) { lstrcpynA(g_p2pLobbyError,bad,sizeof(g_p2pLobbyError));P2PSetState(P2P_FAILED,bad); }
    else { g_p2pLobbyError[0]=0;P2PSetState(P2PStartupBusy() ? P2P_STOPPING:P2P_IDLE,""); }
}
static BOOL P2PWriteIntent(const char *path, const char *text)
{
    char tmp[MAX_PATH+40];
    _snprintf(tmp,sizeof(tmp),"%s.%lu.tmp",path,GetCurrentProcessId());
    tmp[sizeof(tmp)-1]=0;
    FILE *file=fopen(tmp,"wb");
    if (!file) return FALSE;
    BOOL ok=fputs(text,file)>=0;
    if (fclose(file)) ok=FALSE;
    if (ok) ok=MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileA(tmp);
    return ok;
}
/* An explicit, owner-bound route request, including pending/failed states.
 * The legacy runtime descriptor is deliberately separate: worker cleanup
 * must never erase enabled intent and send a failed host to a dedicated exe. */
static BOOL P2PPublishIntent(const char *state, BOOL ready)
{
    if(P2PProductionMode()!=0)return FALSE;
    int guest=P2PGuestRouteMode();
    if(guest!=0)return guest==1; /* binding-only Guest has no Host intent route */
    char path[MAX_PATH],legacy[MAX_PATH],text[1024];DWORD serverPid;
    if (!g_p2pIntentUid) return !g_p2pEnabled;
    P2PConfig c=g_p2pIntentConfig;
    if (!c.state[0]) { P2PDefaults(&c);P2PLoadIni(&c); }
    if (GetFileAttributesA(c.state)==INVALID_FILE_ATTRIBUTES) P2PMakeDirs(c.state);
    int length=_snprintf(path,sizeof(path),"%s\\p2p_host.%016llx.txt",c.state,g_p2pIntentUid);
    if (length<0 || length>=(int)sizeof(path)) return FALSE;
    length=_snprintf(legacy,sizeof(legacy),"%s\\p2p_host.txt",c.state);
    if (length<0 || length>=(int)sizeof(legacy)) return FALSE;
    EnterCriticalSection(&g_p2pLock);serverPid=g_p2p.pid;LeaveCriticalSection(&g_p2pLock);
    _snprintf(text,sizeof(text),
        "uid=%016llx\nenabled=%d\nready=%d\nstate=%s\nport=%d\nnet_ip=127.0.0.1\n"
        "client_pid=%lu\nserver_pid=%lu\nmap=%s\nsession_map=%s\ngametype=%s\ntype=%s\nlaunch=%s\n",
        g_p2pIntentUid,g_p2pEnabled ? 1:0,ready ? 1:0,state,c.port,GetCurrentProcessId(),serverPid,
        g_p2pRoom.map,g_p2pRoom.session,g_p2pRoom.gametype,g_p2pRoom.category,
        g_p2pEnabled ? P2PLaunchLabel():"none");
    text[sizeof(text)-1]=0;
    BOOL ok=P2PWriteIntent(path,text);
    if (ok) P2PWriteIntent(legacy,text); /* compatibility; routing prefers the UID file */
    if (ok) g_p2pIntentAt=GetTickCount();
    return ok;
}
static void P2PHostCurrentRoom(void);
static void P2PStartIntentLaunch(BOOL prepared);
/* Main thread, once per frame: the armed selection's single host start. A datagram or a lobby call
 * alone grants nothing: the choice must be enabled, armed, error-free and (protocol 1/2) backed by
 * the owner's own matching Ready/Start intent. */
static void P2PLaunchTick(int protocol)
{
    LONG signal = InterlockedExchange(&g_p2pLaunchSignal, P2P_LAUNCH_NONE);
    if (!signal || !g_p2pEnabled || !g_p2pArmed || g_p2pLaunched || g_p2pLobbyError[0]) return;
    if (protocol != 0 && ((protocol != 1 && protocol != 2) || !g_p2pStartPermitted)) return;
    if (P2PStartupBusy()) {
        /* the previous room's host is still closing: keep the request for the next frame */
        InterlockedCompareExchange(&g_p2pLaunchSignal, signal, P2P_LAUNCH_NONE);
        return;
    }
    g_p2pLaunched = TRUE; g_p2pLaunchTick = GetTickCount(); g_p2pEmptySince = 0;
    InterlockedExchange(&g_p2pLaunchPort, 0);
    Log("p2p: match launch (%s): starting the local host for %s %s (%s)",
        signal == P2P_LAUNCH_DATAGRAM ? "C1's first datagram to the armed port" : "lobby call",
        g_p2pRoom.session, g_p2pRoom.gametype, g_p2pRoom.category);
    if (protocol == 0) P2PHostCurrentRoom();
    else P2PStartIntentLaunch(TRUE);
    if (g_p2pLobbyError[0]) Log("p2p: the local host was not started: %s", g_p2pLobbyError);
}
/* Main thread: the launched host failed before it ever answered. C1 is still on its connecting screen
 * with nothing to talk to; its own retry would run for minutes. End that connect once, so the player
 * is back in the lobby with the error instead of waiting out C1's timeout. */
static void P2PLaunchAbort(int state, const char *error)
{
    if (!g_p2pLaunched || g_p2pLaunchAborted) return;
    if (state == P2P_READY) { g_p2pLaunchAnswered = TRUE; return; }
    if (g_p2pLaunchAnswered || (state != P2P_FAILED && !error[0])) return;
    g_p2pLaunchAborted = TRUE;
    Log("p2p: the launched local host failed before it answered (%s); ending the client's connect", error[0] ? error : "failed");
    C1CbufAddText("disconnect\n");
}
/* Main thread: a launched legacy host that no player is on does not stay up (a start that was
 * cancelled after the launch, or a match everybody left). */
static void P2PLaunchWatch(int state, DWORD now)
{
    char status[512]; const char *humans;
    if (!g_p2pLaunched || state != P2P_READY) { g_p2pEmptySince = 0; return; }
    EnterCriticalSection(&g_p2pLock);
    lstrcpynA(status, g_p2p.status, sizeof(status));
    LeaveCriticalSection(&g_p2pLock);
    humans = strstr(status, " humans=");
    if (!humans || atoi(humans + 8) > 0) { g_p2pEmptySince = 0; return; }
    if (!g_p2pEmptySince) { g_p2pEmptySince = now ? now : 1; return; }
    if (now - g_p2pEmptySince < P2P_LAUNCH_EMPTY_MS) return;
    g_p2pEmptySince = 0;
    Log("p2p: nobody is on the launched local host after %u s; stopping it", P2P_LAUNCH_EMPTY_MS / 1000u);
    lstrcpyA(g_p2pLobbyError, "No player joined the local match. Restart the client to host again.");
    InterlockedExchange(&g_p2pStop, 1);
}
static void P2PLobbyTick(void)
{
    static char previous[1024];
    static char previousNative[1024];
    char snapshot[1024], error[256], number[24]; int state;
    if (!g_p2pInit) return;
    if(P2PProductionMode()!=0) { P2PProductionTick();return; }
    int guest=P2PGuestRouteMode();
    if(guest!=0) {
        static LONG guest_denied;
        if(guest<0&&InterlockedCompareExchange(&guest_denied,1,0)==0) {
            P2PStartupInvalidate();P2PSetState(P2P_FAILED,"Guest route changed or is unavailable.");
        }
        P2PGuestTick();return; /* no Host Ready/route publication from Guest */
    }
    if(!C1DvarsReady())return;
    if(g_p2pLaunchPending) {
        P2PConfig selected=g_p2pPendingConfig;g_p2pLaunchPending=FALSE;
        if(InterlockedCompareExchange(&g_p2pStop,0,0) || !P2PStartupAttemptCurrent(selected.startupGeneration) ||
           !P2PStartConfigured(NULL,NULL,NULL,&selected))
            P2PSetState(P2P_FAILED,"Queued local host activation was refused.");
    }
    int protocol=P2PSelectedProtocolMode();
    P2PLaunchTick(protocol);
    if (protocol!=0) {
        unsigned long long uid=P2PLocalUid();
        if (uid!=g_p2pIntentUid) {
            P2PStartupInvalidate();P2PDisarm();
            if (g_p2pEnabled) {
                lstrcpyA(g_p2pLobbyError,"Player identity changed. Choose the local room again.");
                P2PPublishIntent("failed",FALSE); /* retire the previous owner's ready route */
                g_p2pHasRoom=FALSE;
            }
            g_p2pIntentUid=uid;P2PStartupSnapshot();
        }
        g_p2pLobbyPending=FALSE;
    }
    if (protocol==0 && g_p2pLobbyPending && (!g_p2pThread || WaitForSingleObject(g_p2pThread,0)==WAIT_OBJECT_0)) {
        P2PConfig next=g_p2pLobbyNext;
        lstrcpyA(next.mode,"inprocess");
        g_p2pLobbyPending=FALSE;
        const char *bad=P2PCheck(&next);
        if (bad) { lstrcpynA(g_p2pLobbyError,bad,sizeof(g_p2pLobbyError));P2PSetState(P2P_FAILED,bad); }
        else if (!P2PStartConfigured(NULL,NULL,NULL,&next)) lstrcpyA(g_p2pLobbyError,"Host is still closing. Toggle hosting to retry.");
    }
    EnterCriticalSection(&g_p2pLock);
    state=g_p2pLobbyPending ? P2P_STOPPING : g_p2p.state;
    lstrcpynA(error,g_p2pLobbyError[0] ? g_p2pLobbyError : g_p2p.error,sizeof(error));
    LeaveCriticalSection(&g_p2pLock);
    BOOL currentAttempt=protocol==0 || P2PStartupAttemptCurrent(g_p2pIntentConfig.startupGeneration);
    BOOL currentReady=currentAttempt && (protocol==0 || P2PInprocPrincipalFresh());
    if (protocol!=0 && !currentAttempt && !error[0] && state!=P2P_FAILED && state!=P2P_EXITED)
        state=P2PStartupBusy() ? P2P_STOPPING:P2P_IDLE;
    if(protocol!=0&&currentAttempt&&!currentReady&&state==P2P_READY)state=P2P_STARTING;
    if (protocol<0 && g_p2pEnabled) {
        state=P2P_FAILED;lstrcpyA(error,"The local startup protocol configuration is invalid.");
    }
    if (g_p2pEnabled && state==P2P_EXITED && P2PInprocUsed() && !error[0])
        lstrcpyA(error,"Restart the client before hosting another Local Play match.");
    if (!g_p2pEnabled) g_p2pIntentUid=P2PLocalUid();
    if (!g_p2pEnabled && !g_p2pIntentConfig.state[0]) {
        P2PDefaults(&g_p2pIntentConfig);P2PLoadIni(&g_p2pIntentConfig);
    }
    DWORD now=GetTickCount();
    if (protocol==0 && g_p2pEnabled) P2PLaunchWatch(state,now);
    if (g_p2pEnabled) P2PLaunchAbort(state,error);
    /* Armed: the choice is recorded and valid, and nothing runs. It is never "ready". */
    BOOL armed=g_p2pEnabled && g_p2pArmed && !g_p2pLaunched && !error[0] && state==P2P_IDLE;
    BOOL launching=g_p2pEnabled && g_p2pLaunched && !error[0] && (state==P2P_IDLE || state==P2P_STARTING);
    const char *label=armed ? "armed":kP2PState[state];
    _snprintf(snapshot,sizeof(snapshot),"%d|%u|%d|%s|%s|%d|%d%d",state,g_p2pLobbyRequest,g_p2pEnabled,g_p2pLobbyMap,error,protocol,armed,launching);
    snapshot[sizeof(snapshot)-1]=0;
    if (P2PIntentHeartbeatDue(&g_p2pIntentHeartbeat,now,g_p2pIntentAt,strcmp(snapshot,previousNative)!=0)) {
        BOOL published=P2PPublishIntent(error[0] ? "failed":label,g_p2pEnabled && currentReady && state==P2P_READY && !error[0]);
        P2PIntentHeartbeatResult(&g_p2pIntentHeartbeat,now,published);
    }
    lstrcpynA(previousNative,snapshot,sizeof(previousNative));
    if (g_p2pIntentHeartbeat.failed && !error[0])
        lstrcpyA(error,"Waiting to publish the local host route.");
    /* The UI snapshot includes publication errors, so a later successful
     * heartbeat clears its temporary error and restores Ready without a toggle. */
    _snprintf(snapshot,sizeof(snapshot),"%d|%u|%d|%s|%s|%d|%d%d",state,g_p2pLobbyRequest,g_p2pEnabled,g_p2pLobbyMap,error,protocol,armed,launching);
    snapshot[sizeof(snapshot)-1]=0;
    BOOL changed=strcmp(snapshot,previous)!=0;
    if (!changed) return;
    /* Other threads (the in-process lobby) read this copy, never the main thread's working state. */
    EnterCriticalSection(&g_p2pLock);
    memset(&g_p2pApi,0,sizeof(g_p2pApi));
    g_p2pApi.size=sizeof(g_p2pApi);g_p2pApi.owner_uid=g_p2pIntentUid;g_p2pApi.enabled=g_p2pEnabled ? 1:0;
    g_p2pApi.armed=armed ? 1:0;g_p2pApi.launched=g_p2pEnabled && g_p2pLaunched ? 1:0;
    g_p2pApi.ready=g_p2pEnabled && currentReady && state==P2P_READY && !error[0] ? 1:0;
    g_p2pApi.failed=error[0] || state==P2P_FAILED ? 1:0;
    g_p2pApi.port=g_p2pIntentConfig.port;g_p2pApi.protocol=protocol;
    lstrcpynA(g_p2pApi.map,g_p2pRoom.map,sizeof(g_p2pApi.map));
    lstrcpynA(g_p2pApi.session_map,g_p2pRoom.session,sizeof(g_p2pApi.session_map));
    lstrcpynA(g_p2pApi.gametype,g_p2pRoom.gametype,sizeof(g_p2pApi.gametype));
    lstrcpynA(g_p2pApi.type,g_p2pRoom.category,sizeof(g_p2pApi.type));
    lstrcpynA(g_p2pApi.state,error[0] ? "failed":label,sizeof(g_p2pApi.state));
    lstrcpynA(g_p2pApi.error,error,sizeof(g_p2pApi.error));
    LeaveCriticalSection(&g_p2pLock);
    BOOL done=C1QueueSet("ui_p2p_enabled",g_p2pEnabled ? "1":"0");
    done=C1QueueSet("ui_p2p_armed",armed ? "1":"0") && done;
    done=C1QueueSet("ui_p2p_launching",launching ? "1":"0") && done;
    _snprintf(number,sizeof(number),"%d",protocol);
    done=C1QueueSet("ui_p2p_startup_protocol",number) && done;
    done=C1QueueSet("ui_p2p_state",label) && done;
    done=C1QueueSet("ui_p2p_error",error) && done;
    done=C1QueueSet("ui_p2p_map",g_p2pLobbyMap) && done;
    done=C1QueueSet("ui_p2p_ready",g_p2pEnabled && currentReady && state==P2P_READY && !error[0] ? "1":"0") && done;
    done=C1QueueSet("ui_p2p_busy",state==P2P_STARTING || state==P2P_READY || state==P2P_STOPPING ? "1":"0") && done;
    _snprintf(number,sizeof(number),"%u",g_p2pLobbyRequest);
    done=C1QueueSet("ui_p2p_request",number) && done;
    if (done) lstrcpynA(previous,snapshot,sizeof(previous));
}
static void P2PHostCurrentRoom(void)
{
    P2PConfig c;
    if (!g_p2pHasRoom) { lstrcpyA(g_p2pLobbyError,"Choose a match lobby first.");return; }
    if (P2PInprocUsed()) {
        lstrcpyA(g_p2pLobbyError,"Restart the client before hosting another Local Play match.");
        P2PSetState(P2P_FAILED,g_p2pLobbyError);
        return;
    }
    P2PDefaults(&c);P2PLoadIni(&c);
    lstrcpyA(c.map,g_p2pRoom.map);lstrcpyA(c.sessionMap,g_p2pRoom.session);
    lstrcpyA(c.gametype,g_p2pRoom.gametype);lstrcpyA(c.zoneType,g_p2pRoom.category);
    lstrcpyA(c.netIp,"127.0.0.1");
    /* Verified scripts are installed into the same-process host before its
     * first frame. The child path cannot receive this immutable inventory. */
    lstrcpyA(c.mode,"inprocess");g_p2pIntentConfig=c;
    if (!g_p2pIntentUid) { lstrcpyA(g_p2pLobbyError,"Player identity is not ready for local hosting.");return; }
    EnterCriticalSection(&g_p2pLock);
    BOOL busy=g_p2p.state==P2P_STARTING || g_p2p.state==P2P_READY || g_p2p.state==P2P_STOPPING;
    BOOL same=busy && g_p2p.state!=P2P_STOPPING && !g_p2pLobbyPending &&
        !strcmp(g_p2pLobbyMap,c.sessionMap) && !strcmp(g_p2p.cfg.gametype,c.gametype) && !strcmp(g_p2p.cfg.zoneType,c.zoneType);
    LeaveCriticalSection(&g_p2pLock);
    const char *bad=P2PCheck(&c);
    if (bad) { lstrcpynA(g_p2pLobbyError,bad,sizeof(g_p2pLobbyError));return; }
    g_p2pLobbyError[0]=0;
    if (same) return;
    lstrcpyA(g_p2pLobbyMap,c.sessionMap);++g_p2pLobbyRequest;
    if (!P2PPublishIntent("starting",FALSE)) {
        lstrcpyA(g_p2pLobbyError,"Cannot publish local host route. Toggle hosting to retry.");return;
    }
    if (busy) {
        g_p2pLaunchPending=FALSE; /* replace a queued pre-entry request, never launch its stale room */
        g_p2pLobbyNext=c;g_p2pLobbyPending=TRUE;
        InterlockedExchange(&g_p2pStop,1);
    } else {
        EnterCriticalSection(&g_p2pLock);g_p2p.cfg=c;LeaveCriticalSection(&g_p2pLock);
        if (!P2PStartConfigured(NULL,NULL,NULL,&c)) lstrcpyA(g_p2pLobbyError,"Host is still closing. Toggle hosting to retry.");
    }
}
/* Legacy protocol (0): the enabled choice is validated, recorded and published, and nothing is
 * started. No thread, no server DLL, no zone, no memory: P2PLaunchTick starts it at the launch. */
static void P2PArmLegacyRoom(void)
{
    P2PConfig c;
    const char *bad;
    BOOL busy,same;
    P2PDisarm();
    if (!g_p2pHasRoom) { lstrcpyA(g_p2pLobbyError,"Choose a match lobby first.");return; }
    P2PDefaults(&c);P2PLoadIni(&c);
    lstrcpyA(c.map,g_p2pRoom.map);lstrcpyA(c.sessionMap,g_p2pRoom.session);
    lstrcpyA(c.gametype,g_p2pRoom.gametype);lstrcpyA(c.zoneType,g_p2pRoom.category);
    lstrcpyA(c.netIp,"127.0.0.1");lstrcpyA(c.mode,"inprocess");
    g_p2pIntentConfig=c;
    if (!g_p2pIntentUid) { lstrcpyA(g_p2pLobbyError,"Player identity is not ready for local hosting.");return; }
    EnterCriticalSection(&g_p2pLock);
    busy=g_p2p.state==P2P_STARTING || g_p2p.state==P2P_READY || g_p2p.state==P2P_STOPPING;
    same=busy && g_p2p.state!=P2P_STOPPING && !g_p2pLobbyPending && !InterlockedCompareExchange(&g_p2pStop,0,0) &&
        !strcmp(g_p2pLobbyMap,c.sessionMap) && !strcmp(g_p2p.cfg.gametype,c.gametype) && !strcmp(g_p2p.cfg.zoneType,c.zoneType);
    LeaveCriticalSection(&g_p2pLock);
    if (same) { g_p2pArmed=g_p2pLaunched=TRUE;g_p2pLobbyError[0]=0;return; } /* the launched host runs this room */
    if (busy) {
        /* The room changed under a launched host: it is never left running for the old room. */
        g_p2pLaunchPending=FALSE;g_p2pLobbyPending=FALSE;InterlockedExchange(&g_p2pStop,1);
    }
    bad=P2PInprocUsed() ? "Restart the client before hosting another Local Play match.":P2PCheck(&c);
    lstrcpyA(g_p2pLobbyMap,c.sessionMap);++g_p2pLobbyRequest;
    if (bad) { lstrcpynA(g_p2pLobbyError,bad,sizeof(g_p2pLobbyError));return; }
    g_p2pLobbyError[0]=0;
    if (!busy) P2PSetState(P2P_IDLE,"");
    P2PArmLaunch(c.port);
    if (!P2PPublishIntent("armed",FALSE)) {
        P2PDisarm();lstrcpyA(g_p2pLobbyError,"Cannot publish local host route. Toggle hosting to retry.");
    }
}
/* Internal room context is separate from the user's one-bit hosting switch. */
static void __cdecl P2PCmdRoom(void)
{
    const char *args[5];P2PRoomContext room;
    int count=VoiceArgc()-1,protocol=P2PSelectedProtocolMode(),wanted=(protocol==2||protocol==3)?5:3;
    if (count!=wanted) { if((protocol==2||protocol==3))P2PStartupInvalidate();lstrcpyA(g_p2pLobbyError,"Choose a match lobby first.");P2PLobbyTick();return; }
    for (int i=0;i<wanted;++i) args[i]=VoiceArgv(i+1);
    const char *bad=(protocol==2||protocol==3)?P2PParseRoomContextV2(count,args,&room):P2PParseRoomContext(count,args,&room);
    if (bad) { if((protocol==2||protocol==3))P2PStartupInvalidate();lstrcpynA(g_p2pLobbyError,bad,sizeof(g_p2pLobbyError));P2PLobbyTick();return; }
    if (P2PSelectedProtocolMode()!=0) { P2PStartupInvalidate();P2PDisarm(); }
    g_p2pRoom=room;g_p2pHasRoom=TRUE;
    if (P2PSelectedProtocolMode()!=0) { P2PStartupSnapshot();if (g_p2pEnabled) P2PArmCurrentRoom(); }
    else if (g_p2pEnabled) P2PArmLegacyRoom();
    P2PLobbyTick();
}
static void __cdecl P2PCmdLocalhost(void)
{
    const char *value=VoiceArgc()==2 ? VoiceArgv(1):"";
    if (strcmp(value,"0") && strcmp(value,"1")) { C1Printf("p2p_localhost 1 or 0\n");return; }
    if (P2PSelectedProtocolMode()!=0) {
        P2PStartupInvalidate();P2PDisarm();
        g_p2pEnabled=!strcmp(value,"1");g_p2pLobbyPending=FALSE;g_p2pLobbyError[0]=0;
        unsigned long long uid=P2PLocalUid();
        if (uid!=g_p2pIntentUid && g_p2pIntentUid) {
            P2PPublishIntent("failed",FALSE);g_p2pHasRoom=FALSE;
        }
        g_p2pIntentUid=uid;P2PStartupSnapshot();
        if (g_p2pEnabled) P2PArmCurrentRoom();
        else P2PSetState(P2PStartupBusy() ? P2P_STOPPING:P2P_IDLE,"");
        P2PLobbyTick();return;
    }
    g_p2pEnabled=!strcmp(value,"1");g_p2pLobbyError[0]=0;
    if (g_p2pEnabled) g_p2pIntentUid=P2PLocalUid();
    /* Ticking the box only arms (P2PArmLegacyRoom). Clearing it disarms and ends a launched host. */
    if (g_p2pEnabled) P2PArmLegacyRoom();
    else { P2PDisarm();g_p2pLaunchPending=FALSE;g_p2pLobbyPending=FALSE;InterlockedExchange(&g_p2pStop,1); }
    P2PLobbyTick();
}

static void __cdecl P2PCmdStartIntent(void)
{
    const char *args[5];P2PRoomContext selected;
    int mode=P2PSelectedProtocolMode();
    if (mode==0) return; /* old UI/startup owns the unchanged legacy route */
    int wanted=(mode==2||mode==3)?5:3;
    if ((mode!=1&&mode!=2&&mode!=3) || VoiceArgc()!=wanted+2 ||
        (strcmp(VoiceArgv(1),"ready") && strcmp(VoiceArgv(1),"start"))) {
        if((mode==2||mode==3))P2PStartupInvalidate();
        lstrcpyA(g_p2pLobbyError,"The local start intent is invalid.");P2PLobbyTick();return;
    }
    for (int i=0;i<wanted;++i) args[i]=VoiceArgv(i+2);
    const char *bad=(mode==2||mode==3)?P2PParseRoomContextV2(wanted,args,&selected):P2PParseRoomContext(wanted,args,&selected);
    if (bad || !g_p2pEnabled || !g_p2pHasRoom || !g_p2pIntentUid || P2PLocalUid()!=g_p2pIntentUid ||
        strcmp(selected.map,g_p2pRoom.map) || strcmp(selected.session,g_p2pRoom.session) ||
        strcmp(selected.gametype,g_p2pRoom.gametype) || strcmp(selected.category,g_p2pRoom.category) ||
        ((mode==2||mode==3)&&(selected.difficultyPresent!=g_p2pRoom.difficultyPresent||selected.difficulty!=g_p2pRoom.difficulty))) {
        if((mode==2||mode==3))P2PStartupInvalidate();
        lstrcpyA(g_p2pLobbyError,"The local start intent does not match the owned room.");P2PLobbyTick();return;
    }
    /* Ready followed by Start is the same attempt. No room rewrite, renewed
     * lease, second worker or reset is permitted by a duplicate UI request. */
    if (P2PStartupAttemptCurrent(g_p2pIntentConfig.startupGeneration)) { P2PLobbyTick();return; }
    if (mode==1||mode==2) {
        /* The owner's matching Ready/Start only PERMITS the armed selection's launch. The generation
         * is issued, and the host started, by P2PLaunchTick when the match launches for this client. */
        if (g_p2pStartPermitted && !g_p2pLaunched) { P2PLobbyTick();return; } /* the pending launch; a spent one may be retried */
        if (P2PStartupBusy()) {
            lstrcpyA(g_p2pLobbyError,"Host is still closing. Wait before retrying.");P2PLobbyTick();return;
        }
        P2PStartupInvalidate();
        P2PArmCurrentRoom();
        if (g_p2pLobbyError[0]) { P2PLobbyTick();return; }
        P2PArmLaunch(g_p2pIntentConfig.port);g_p2pStartPermitted=TRUE;
        P2PPublishIntent("armed",FALSE);
        P2PLobbyTick();return;
    }
    P2PStartIntentLaunch(FALSE);
    P2PLobbyTick();
}
/* The actual start of an accepted intent: at once in the production protocol (3, sequenced by the
 * retained SDK), at the observed launch in protocols 1 and 2 (P2PLaunchTick). */
static void P2PStartIntentLaunch(BOOL prepared)
{
    if (P2PStartupBusy()) {
        lstrcpyA(g_p2pLobbyError,"Host is still closing. Wait before retrying.");return;
    }
    /* A terminal worker must never become current again while arming clears
     * its old failure state. A retry owns a separately issued token. The deferred
     * path did both when the intent was accepted; every later room, owner, enable
     * or cancel change disarmed it, so the armed selection is still that one. */
    if (!prepared) {
        P2PStartupInvalidate();
        P2PArmCurrentRoom();
    }
    if (g_p2pLobbyError[0]) { return; }
    unsigned long generation=P2PStartupIssue();
    if (!generation) {
        lstrcpyA(g_p2pLobbyError,"The local startup generation is exhausted. Restart the client.");return;
    }
    P2PConfig c=g_p2pIntentConfig;c.startupGeneration=generation;g_p2pIntentConfig=c;
    ++g_p2pLobbyRequest;
    if(P2PProductionMode()!=0) {
        P2PSetState(P2P_STARTING,"");
        if(!P2PProductionStart())P2PSetState(P2P_FAILED,"Local Play authority is unavailable.");
        return;
    }
    P2PSetState(P2P_STARTING,"");
    if (!P2PPublishIntent("starting",FALSE) || !P2PStartConfigured(NULL,NULL,NULL,&c)) {
        P2PStartupInvalidate();
        lstrcpyA(g_p2pLobbyError,"Cannot start the selected local host. Choose the room again to retry.");
        P2PSetState(P2P_FAILED,g_p2pLobbyError);
    }
}
static void __cdecl P2PCmdCancelStart(void)
{
    if (P2PSelectedProtocolMode()==0) return;
    P2PStartupInvalidate();P2PDisarm();g_p2pLobbyPending=FALSE;g_p2pLobbyError[0]=0;
    /* Retain selected enabled intent, so cancellation cannot fall through to
     * a dedicated server while native/SDK cleanup is still pending. */
    P2PSetState(P2PStartupBusy() ? P2P_STOPPING:P2P_IDLE,"");
    P2PPublishIntent(P2PStartupBusy() ? "stopping":"idle",FALSE);
    P2PLobbyTick();
}

/* "p2p_launch": the explicit launch input on the main thread (the UI or a test; the in-process
 * lobby calls K33LocalPlayLaunch). It carries no selection and grants nothing by itself: it only
 * tells the armed, enabled choice that its match is starting now. */
static void __cdecl P2PCmdLaunch(void)
{
    if (g_p2pEnabled && g_p2pArmed && !g_p2pLaunched)
        InterlockedCompareExchange(&g_p2pLaunchSignal,P2P_LAUNCH_EXPLICIT,P2P_LAUNCH_NONE);
    P2PLobbyTick();
}

/* ---- the in-process lobby's view (p2p_local_play_api.h) ------------------------------------- */

int K33LocalPlayIntent(K33LocalPlayIntentInfo *out)
{
    if (!out || !g_p2pInit) return 0;
    EnterCriticalSection(&g_p2pLock);
    *out=g_p2pApi;
    LeaveCriticalSection(&g_p2pLock);
    return out->size==sizeof(*out);
}
int K33LocalPlayLaunch(unsigned long long owner_uid,const char *session_map,const char *gametype,const char *type)
{
    K33LocalPlayIntentInfo now;
    if (!owner_uid || !session_map || !gametype || !type || !K33LocalPlayIntent(&now)) return 0;
    if (!now.enabled || now.failed || now.owner_uid!=owner_uid || strcmp(now.session_map,session_map) ||
        strcmp(now.gametype,gametype) || strcmp(now.type,type)) return 0;
    if (now.launched) return 1; /* this selection's host start was already issued */
    if (!now.armed) return 0;
    InterlockedCompareExchange(&g_p2pLaunchSignal,P2P_LAUNCH_EXPLICIT,P2P_LAUNCH_NONE);
    return 1;
}

/* ---- commands ------------------------------------------------------------------------------ */

static void __cdecl P2PCmdHost(void)
{
    if (P2PSelectedProtocolMode()!=0) { C1Printf("p2phost: use the selected room Ready or Start control\n");return; }
    const char *map = VoiceArgc() > 1 ? VoiceArgv(1) : "";
    const char *gt = VoiceArgc() > 2 ? VoiceArgv(2) : "";
    const char *zone = VoiceArgc() > 3 ? VoiceArgv(3) : "";
    if ((map[0] && !P2PIdent(map, 63)) || (gt[0] && !P2PIdent(gt, 31))) { C1Printf("p2phost: bad map or gametype\n"); return; }
    if (P2PInprocUsed()) { C1Printf("p2phost: this client already hosted a match; restart the client to host again\n"); return; }
    if (P2PStart(map, gt, zone)) C1Printf("p2phost: starting the match server (p2pstatus shows when it is ready)\n");
    else C1Printf("p2phost: already hosting; p2pstop first\n");
}

static void __cdecl P2PCmdStop(void)
{
    g_p2pLaunchPending=FALSE;
    if (P2PSelectedProtocolMode()!=0) P2PStartupInvalidate();
    P2PDisarm();
    g_p2pEnabled = FALSE;
    g_p2pLobbyPending = FALSE;
    g_p2pLobbyError[0] = 0;
    if (P2PSelectedProtocolMode()!=0) { P2PStartupSnapshot();P2PLobbyTick(); }
    if (!g_p2pThread || WaitForSingleObject(g_p2pThread, 0) == WAIT_OBJECT_0) { C1Printf("p2pstop: not hosting\n"); return; }
    InterlockedExchange(&g_p2pStop, 1);
    C1Printf("p2pstop: stopping the match server\n");
}

static void __cdecl P2PCmdStatus(void)
{
    EnterCriticalSection(&g_p2pLock);
    int st = g_p2p.state;
    C1Printf("p2p: %s%s%s\n", kP2PState[st], g_p2p.error[0] ? " - " : "", g_p2p.error);
    if (st != P2P_IDLE)
        C1Printf("p2p: %s:%d pid %lu, %s %s (%s), ready after %lu ms\n", g_p2p.cfg.netIp, g_p2p.cfg.port, g_p2p.pid,
                 g_p2p.cfg.map, g_p2p.cfg.gametype, g_p2p.cfg.zoneType, g_p2p.readyMs);
    if (g_p2p.status[0]) C1Printf("p2p: %s\n", g_p2p.status);
    if (st == P2P_EXITED) C1Printf("p2p: exit code %lu\n", g_p2p.exitCode);
    LeaveCriticalSection(&g_p2pLock);
}

static C1Cmd g_p2pCmds[] = {
    { NULL, "p2p_room", (void *)P2PCmdRoom },
    { NULL, "p2p_localhost", (void *)P2PCmdLocalhost },
    { NULL, "p2p_start_intent", (void *)P2PCmdStartIntent },
    { NULL, "p2p_cancel_start", (void *)P2PCmdCancelStart },
    { NULL, "p2p_launch", (void *)P2PCmdLaunch },
    { NULL, "p2phost", (void *)P2PCmdHost },
    { NULL, "p2pstop", (void *)P2PCmdStop },
    { NULL, "p2pstatus", (void *)P2PCmdStatus },
};

/* TakeOver, before the game entry: link the commands (the list head is plain data, as voice.c notes) and
 * start hosting at once with +set k33_p2pHost 1, so the server loads its map while the client starts. */
static void InstallP2PHost(void)
{
    g_p2pMainThread=GetCurrentThreadId(); /* TakeOver calls the original C1 entry on this thread. */
    if (!g_p2pInit) { InitializeCriticalSection(&g_p2pLock); g_p2pInit = TRUE; }
    C1GameInit();
    K33OnMainTick(P2PLobbyTick);
    int added = 0;
    for (unsigned i = 0; i < sizeof(g_p2pCmds) / sizeof(g_p2pCmds[0]); i++) added += C1AddCommand(&g_p2pCmds[i]);
    const char *cl = GetCommandLineA();
    BOOL autostart = cl && (strstr(cl, "k33_p2pHost 1") || strstr(cl, "k33_p2pHost \"1\""));
    Log("p2p: %d commands (p2phost, p2pstop, p2pstatus)%s", added, autostart ? "; hosting at startup (k33_p2pHost 1)" : "");
    if (autostart && P2PSelectedProtocolMode()==0) P2PStart(NULL, NULL, NULL);
}

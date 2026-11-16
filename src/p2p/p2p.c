/*
 * p2p.c - player-hosted matches (docs/P2P_HOSTING.md).
 *
 * The in-process path runs the sh1x server linked into Kernel33.dll (p2p_inproc.c: started on the first host,
 * kept for the client's run) on its fiber from the game thread; it compiles the GSC tree installed with the
 * client. This client is offline only:
 * the launcher startup protocols 1-3, the production SDK, the Guest binding route and the verified
 * script service are gone, and the room's Local Play host is the one (protocol 0) route.
 *
 *   p2phost [map] [gametype] [zonetype]   start hosting (defaults from p2p\p2p_host.ini)
 *   p2pstop                               end the hosted match (in-process: its level is unloaded, the server stays up)
 *   p2pstatus                             state, address, pid, ready time, last sh1x_status line
 *   +set sv_p2pHost 1                    on the command line: start hosting while the client loads
 *
 * The lobby's room (K33LocalPlayAuto, or the UI's p2p_room / p2p_localhost 1) only ARMS a host: it records and publishes the intent
 * ("state=armed launch=deferred") and starts nothing. The server is started when the match the room started
 * launches for this client (docs/P2P_DEFERRED_HOST_START.md): hostgate's sendto wrapper sees C1's first
 * datagram to the armed loopback port, or the in-process lobby posts the launch (K33LocalPlayAuto).
 *
 * Every path and setting comes from <exe dir>\p2p\p2p_host.ini (key=value, '#' comments); the defaults
 * keep everything under <exe dir>\p2p\. The server is in a job object with KILL_ON_JOB_CLOSE, so it
 * ends with the client even when the client crashes. No descriptor, intent or rollout file is written: the
 * in-DLL lobby reads the published snapshot (K33LocalPlayIntent).
 *
 * Threads: the commands run on C1's main thread (Cbuf) and only print and signal; the host thread does
 * the process work, the rcon polling and the files. g_p2p is shared under g_p2pLock.
 */
#include <wincrypt.h>
#include <ws2tcpip.h>
#include "p2p_match_options.h"
#include "p2p_status.h"
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
    char profile[MAX_PATH];      /* profilePath: the lobby's resources folder */
    char rconFile[MAX_PATH];
    char map[64], gametype[32], zoneType[16], sessionMap[64];
    char netIp[64], botDifficulty[16], extra[512];
    char mode[16];               /* inprocess (default): the server in Kernel33.dll on the game thread; child: the exe */
    char dll[MAX_PATH];          /* dll= of earlier builds, read and ignored: the server is part of Kernel33.dll */
    int port, bots, admission, developer, readyTimeout;
    int pmemMegs;                /* pmemMegs for the server: its fastfile block (0 = the server default) */
    int c1Assets;                /* in-process: take C1's records of startup zones (default 1) */
    int c1Preload;               /* in-process: preloadmap when C1 is not loading a hosted map (default 0) */
    int c1Zones;                 /* in-process: map zones C1 holds are C1-backed, mounted only on demand (default 1) */
    int difficultyPresent,difficulty; /* the Zombies room's difficulty (K33LocalPlayAuto) */
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
static char g_p2pArgMap[64], g_p2pArgGametype[32], g_p2pArgZone[16];
static P2PConfig g_p2pOverride;
static BOOL g_p2pHasOverride;
struct P2PConfig;
static int P2PInprocRun(const struct P2PConfig *c, const char *password, const char *args); /* p2p_inproc.c */
static BOOL P2PInprocUsed(void); /* p2p_inproc.c: an in-process session ran and could not be taken down */
static BOOL P2PInprocServerAlive(void); /* p2p_inproc.c: the server of this run is up (it holds its port) */
static volatile LONG g_piMatchOver; /* p2p_inproc.c: the hosted match ended; the room is re-armed for the next one */

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
    g_p2pArmed = g_p2pLaunched = FALSE;
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
 * (tools/ol_map_handoff.ps1), so a map or gametype can never add console commands. */
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
    /* dll stays empty: the server inside Kernel33.dll (p2p_embedded.c); dll= in p2p_host.ini names a DLL file. */
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

#if K33_OFFLINE_LOBBY
static void OfflineP2PConfig(P2PConfig *c); /* offline_lobby.c: the offline run-time folder and this run's host port */
static void OfflineHostPortRelease(void);   /* offline_lobby.c: the host port stops being held reserved */
static void OfflineHostPortReserve(void);   /* offline_lobby.c: held again after a session was taken down */
#else
#define OfflineP2PConfig(c) ((void)0)
#define OfflineHostPortRelease() ((void)0)
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
            { "rcon_file", c->rconFile, MAX_PATH },
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

/* The server pack scripts/build-server.sh stages next to the host DLL: <home>\main\sh1x_svr_scripts.iwd
 * (waypoints, node maps and mp/botclasstable.csv). The host reads mp/botclasstable.csv with FS_ReadFile
 * (sh1x bot_loadout.c), never from a mounted stringtable. Without the pack the host still runs,
 * but every bot class answers "none" for its whole loadout and character: bots spawn with
 * defaultweapon_mp and no body, head or shadow - only their names are drawn - and route over the
 * level's raw node_* records. Refuse to host instead of starting that match. A loose
 * mp\botclasstable.csv in the home (a server developer's layout) counts as the pack. */
static BOOL P2PServerPackPresent(const P2PConfig *c)
{
    static const char *const files[] = { "main\\sh1x_svr_scripts.iwd", "main\\mp\\botclasstable.csv" };
    char path[MAX_PATH + 64];
    for (unsigned i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        _snprintf(path, sizeof(path), "%s\\%s", c->home, files[i]);
        path[sizeof(path) - 1] = 0;
        DWORD a = GetFileAttributesA(path);
        if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) return TRUE;
    }
    return FALSE;
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
        !P2PPathOk(c->state) || !P2PPathOk(c->profile) || !P2PPathOk(c->rconFile))
        return "a path holds a quote";
    BOOL inproc = !_stricmp(c->mode, "inprocess");
    if (!inproc && _stricmp(c->mode, "child")) return "mode must be inprocess or child";
    if (c->admission != 1)
        return "P2P hosting requires DW admission";
    /* Offline mode hosts for this one player on loopback with the GSC tree installed with the client
     * (docs/OFFLINE_LOBBY.md). There is no other kind of host. */
    if (!(OfflineLobbyActive() && inproc))
        return "P2P hosting requires the built-in lobby and an in-process host";
    /* This field is appended after the required +set options. Keep the local
     * trial's two known lifecycle switches, but never let it override an
     * admission, script or network cvar (or inject another console command). */
    if (c->extra[0] && strcmp(c->extra,
            "+set exitToLobby 1 +set oneShotMatch 1"))
        return "P2P extra commands are not permitted by the local host policy";
    if (!P2PPathOk(c->dll)) return "a path holds a quote";
    if (!inproc && GetFileAttributesA(c->exe) == INVALID_FILE_ATTRIBUTES) return "server executable not found (exe=)";
    if (!P2PServerPackPresent(c))
        return "server pack missing: p2p\\home\\main\\sh1x_svr_scripts.iwd (bot classes, waypoints)";
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
    if (!c.rconFile[0]) _snprintf(c.rconFile, MAX_PATH, "%s\\native_server_rcon.txt", c.state);
    EnterCriticalSection(&g_p2pLock);
    g_p2p.cfg = c;
    LeaveCriticalSection(&g_p2pLock);

    const char *bad = P2PCheck(&c);
    if (bad) { Log("p2p: not hosting: %s", bad); P2PSetState(P2P_FAILED, bad); WSACleanup(); return 1; }
    /* The server of this run, once started, holds its port between matches; only the first match checks it. */
    BOOL serverUp = !_stricmp(c.mode, "inprocess") && P2PInprocServerAlive();
    if (!serverUp) OfflineHostPortRelease(); /* offline mode kept the port bound until now so that it is still free */
    if (!serverUp && !P2PPortFree(c.netIp, c.port)) {
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
    if (!P2PRconPassword(c.rconFile, password)) {
        Log("p2p: not hosting: no rcon password (%s)", c.rconFile);
        P2PSetState(P2P_FAILED, "no rcon password");
        WSACleanup();
        return 1;
    }

    static char cmd[8192];
    BOOL inproc = !_stricmp(c.mode, "inprocess");
    char pmem[256] = "";
    char relayArg[MAX_PATH + 96] = "";
    char relayPath[MAX_PATH + 64];
    if (c.pmemMegs >= 32 && c.pmemMegs <= 2048) _snprintf(pmem, sizeof(pmem), "+set pmemMegs %d", c.pmemMegs);
    /* Zombies: the room's difficulty is g_difficulty to the server's _zombiemode.gsc (the lobby's
     * selection, K33LocalPlayAuto); left unset it stays 0 and every room plays on easy. */
    if (!strcmp(c.zoneType, "zm") && c.difficultyPresent && c.difficulty >= 0 && c.difficulty <= 2)
        _snprintf(pmem + strlen(pmem), sizeof(pmem) - strlen(pmem), " +set g_difficulty %d", c.difficulty);
    /* A loopback host stays on loopback for IPv6 too: without this the server also binds [::]:<port>
     * (TCP and UDP, every interface - seen with netstat), next to its 127.0.0.1 sockets. */
    if (!strcmp(c.netIp, "127.0.0.1"))
        _snprintf(pmem + strlen(pmem), sizeof(pmem) - strlen(pmem), " +set net_ip6 ::1");
    /* Offline: one match per session. At its level exit the server keeps the finished map instead of rotating
     * (server g_sv_main.c ExitLevel: "a managed worker keeps the finished map loaded until clients leave"); the
     * player goes back to the lobby, the match ends (the server stays up, p2p_inproc.c), and the next Ready moves
     * the same server to the next match. Without it the client followed the rotation into the next map for a second and
     * that map's "disconnect" report credited a loss. */
    if (OfflineLobbyActive())
        _snprintf(pmem + strlen(pmem), sizeof(pmem) - strlen(pmem), " +set oneShotMatch 1 +set exitToLobby 1");
    pmem[sizeof(pmem) - 1] = 0;
    /* The explicit local Host/Guest trial arms this short-lived file before
     * forwarding any Guest packet. Normal solo hosting leaves it absent. */
    _snprintf(relayPath, sizeof(relayPath), "%s\\p2p_relay_identity.txt", c.state);
    relayPath[sizeof(relayPath)-1] = 0;
    _snprintf(relayArg, sizeof(relayArg), "+set codol_relayIdentityPath \"%s\"", relayPath);
    relayArg[sizeof(relayArg)-1] = 0;
    _snprintf(cmd, sizeof(cmd),
        "\"%s\" +set dedicated 2 +set fs_basepath \"%s\" +set fs_homepath \"%s\" +set net_ip %s +set net_port %d "
        "+set rcon_password \"%s\" +set logfile 2 +set logrcon 1 +set developer %d +set sv_fps 60 "
        "+set scriptPath \"%s\" +set scriptOverlay \"%s\" +set scriptNativeOverlay \"%s\" "
        "+set securityKeyPath \"%s\\securitykey.bin\" +set admissionPath \"%s\\game_admissions.txt\" "
        "+set admissionRequired %d %s +set profilePath \"%s\" +set codol_zoneType %s +set codol_sessionMap %s "
        "+set g_gametype %s +set hostParentPid %lu %s "
        "+scriptPath +scriptOverlay +scriptNativeOverlay +map %s %s",
        c.exe, c.basepath, c.home, c.netIp, c.port, password, c.developer, c.scripts, c.scripts, c.scripts,
        c.state, c.state, c.admission ? 1 : 0, relayArg, c.profile, c.zoneType, c.sessionMap[0] ? c.sessionMap : c.map,
        c.gametype, inproc ? 0ul : GetCurrentProcessId(), pmem, c.map, c.extra);
    cmd[sizeof(cmd) - 1] = 0;
    if (inproc) {
        /* The same command line without the exe: Sh1xHost_Init takes what follows it. */
        const char *args = strchr(cmd + 1, '"');
        int r = P2PInprocRun(&c, password, args ? args + 2 : cmd);
        /* until the next match's launch; a running server holds the port itself */
        if (OfflineLobbyActive() && !P2PInprocUsed() && !P2PInprocServerAlive()) OfflineHostPortReserve();
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

    BOOL ready = FALSE;
    DWORD lastPoll = 0, lastAnswer = 0;
    char current[160] = ""; /* the running match's identity (P2PStatusIdentity), to follow a map change */
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
        char reply[2048], line[512], ident[160];
        int kind = P2P_STATUS_BAD;
        if (P2PRcon(c.port, password, "sh1x_status", reply, sizeof(reply), 400) > 0 &&
            P2PStatusLine(reply, line, sizeof(line)))
            kind = P2PStatusClassify(line,c.map,c.sessionMap,c.gametype,c.zoneType);
        /* The first Ready needs the requested match (EXACT); after it, a running match on another map/gametype (a
         * console/rcon `map`) is followed, not taken for a hung server (p2p_inproc.c, the same rule). */
        if (kind == P2P_STATUS_EXACT || (ready && kind == P2P_STATUS_LIVE)) {
            lastAnswer = now;
            P2PStatusIdentity(line, ident, sizeof(ident));
            if (strcmp(ident, current)) {
                if (current[0]) Log("p2p: the match changed from %s to %s; the host follows it", current, ident);
                lstrcpynA(current, ident, sizeof(current));
            }
            EnterCriticalSection(&g_p2pLock);
            lstrcpynA(g_p2p.status, line, sizeof(g_p2p.status));
            if (!ready) g_p2p.readyMs = now - t0;
            g_p2p.state = P2P_READY;
            LeaveCriticalSection(&g_p2pLock);
            if (!ready) {
                ready = TRUE;
                Log("p2p: server ready after %lu ms: %s", now - t0, line);
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
             * loading/recovering or has stopped answering. A later running
             * status restores READY; the existing 20-second failure gate stays
             * (a server with no level, running=0, is not refreshed). */
            P2PSetState(P2P_STARTING, NULL);
        }
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    if (job) CloseHandle(job);
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
    if (g_p2pThread) {
        if (WaitForSingleObject(g_p2pThread, 0) != WAIT_OBJECT_0) return FALSE;
        CloseHandle(g_p2pThread);
        g_p2pThread = NULL;
    }
    P2PConfig selected;
    if(override) selected=*override;
    else { P2PDefaults(&selected);P2PLoadIni(&selected); }
    if(map && *map){lstrcpynA(selected.map,map,sizeof(selected.map));selected.sessionMap[0]=0;}
    if(gametype && *gametype)lstrcpynA(selected.gametype,gametype,sizeof(selected.gametype));
    if(zone && *zone)lstrcpynA(selected.zoneType,zone,sizeof(selected.zoneType));
    {
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
            if(!P2PEnsureInprocActivation()) {
                P2PSetState(P2P_FAILED,"Local host frame activation is unavailable. Restart the client.");return FALSE;
            }
        } /* Retained explicit child route has NO Pi activation or Pi-once dependency. */
    }
    /* The worker must not reread a changed INI or switch mode after activation. */
    g_p2pHasOverride=TRUE;g_p2pOverride=selected;
    g_p2pArgMap[0]=g_p2pArgGametype[0]=g_p2pArgZone[0]=0;
    InterlockedExchange(&g_p2pStop,0);P2PSetState(P2P_STARTING,"");
    g_p2pThread=CreateThread(NULL,0,P2PHostThread,NULL,0,NULL);
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
static BOOL P2PStartupBusy(void)
{
    return g_p2pLaunchPending || (g_p2pThread && WaitForSingleObject(g_p2pThread,0)!=WAIT_OBJECT_0);
}
static void P2PHostCurrentRoom(void);
static void P2PArmLegacyRoom(void);
/* ---- offline mode: every owned room is a Local Play room (K33LocalPlayAuto / K33LocalPlayLeave) ----
 * The in-process lobby knows the room's selection for every kind of room (created, matchmade,
 * playlist, Survival, co-op), with or without the room UI's p2p_room; it posts that selection here
 * from its own thread. One slot, the newest request wins; the main-thread tick applies it. */
enum { P2P_AUTO_NONE, P2P_AUTO_ROOM, P2P_AUTO_LEAVE, P2P_AUTO_ERROR };
static struct {
    int kind;
    BOOL launch;
    unsigned long long owner;
    P2PRoomContext room;
    char error[256];
} g_p2pAuto; /* under g_p2pLock */
static BOOL P2PAutoMode(void)
{
    return OfflineLobbyActive();
}
static BOOL P2PRoomSame(const P2PRoomContext *a,const P2PRoomContext *b)
{
    return !strcmp(a->map,b->map) && !strcmp(a->session,b->session) && !strcmp(a->gametype,b->gametype) &&
        !strcmp(a->category,b->category) && a->difficultyPresent==b->difficultyPresent && a->difficulty==b->difficulty;
}
/* Main thread: the lobby's newest word about the owner's room. */
static void P2PAutoTick(void)
{
    int kind;BOOL launch;unsigned long long owner;P2PRoomContext room;char error[256];
    if (!P2PAutoMode()) return;
    EnterCriticalSection(&g_p2pLock);
    kind=g_p2pAuto.kind;launch=g_p2pAuto.launch;owner=g_p2pAuto.owner;room=g_p2pAuto.room;
    lstrcpynA(error,g_p2pAuto.error,sizeof(error));
    g_p2pAuto.kind=P2P_AUTO_NONE;g_p2pAuto.launch=FALSE;
    LeaveCriticalSection(&g_p2pLock);
    /* There is no choice offline: hosting is on. The owner is whoever is logged in. */
    if (!g_p2pEnabled) { g_p2pEnabled=TRUE;g_p2pLobbyError[0]=0; }
    if (!g_p2pArmed && !g_p2pLaunched) g_p2pIntentUid=P2PLocalUid();
    if (kind==P2P_AUTO_NONE) return;
    if (kind==P2P_AUTO_LEAVE) {
        /* as clearing the box did: nothing stays armed, a launched host is asked to quit */
        if (g_p2pHasRoom || g_p2pArmed || g_p2pLaunched)
            Log("p2p: the owner left the room (lobby); disarmed%s",g_p2pLaunched ? ", stopping the local host":"");
        P2PDisarm();g_p2pHasRoom=FALSE;g_p2pLaunchPending=FALSE;g_p2pLobbyPending=FALSE;g_p2pLobbyError[0]=0;
        memset(&g_p2pRoom,0,sizeof(g_p2pRoom));g_p2pLobbyMap[0]=0;
        InterlockedExchange(&g_p2pStop,1);
        return;
    }
    if (kind==P2P_AUTO_ERROR) {
        lstrcpynA(g_p2pLobbyError,error,sizeof(g_p2pLobbyError));
        Log("p2p: the room cannot be hosted (lobby): %s",error);
        return;
    }
    if (!g_p2pIntentUid) g_p2pIntentUid=owner;
    if (!(g_p2pHasRoom && P2PRoomSame(&room,&g_p2pRoom) && (g_p2pArmed || g_p2pLaunched) && !g_p2pLobbyError[0])) {
        g_p2pRoom=room;g_p2pHasRoom=TRUE;
        P2PArmLegacyRoom();
        Log("p2p: room %s %s (%s) from the lobby: %s",room.session,room.gametype,room.category,
            g_p2pLobbyError[0] ? g_p2pLobbyError : g_p2pLaunched ? "its host already runs" : "armed");
    }
    if (!launch) return;
    if (g_p2pLobbyError[0] || !g_p2pArmed) {
        /* The lobby validated this selection and has handed START_GAME to the client, which is now on
         * its connecting screen with nothing to talk to: end that connect, the error is published. */
        Log("p2p: the local host cannot be started for the launched match (%s); ending the client's connect",
            g_p2pLobbyError[0] ? g_p2pLobbyError : "not armed");
        if (!g_p2pLobbyError[0]) lstrcpyA(g_p2pLobbyError,"The local host could not be started.");
        C1CbufAddText("disconnect\n");
        return;
    }
    if (!g_p2pLaunched) InterlockedCompareExchange(&g_p2pLaunchSignal,P2P_LAUNCH_EXPLICIT,P2P_LAUNCH_NONE);
}
/* Main thread, once per frame: the armed selection's single host start. A datagram or a lobby call
 * alone grants nothing: the choice must be enabled, armed and error-free. */
static void P2PLaunchTick(void)
{
    LONG signal = InterlockedExchange(&g_p2pLaunchSignal, P2P_LAUNCH_NONE);
    if (!signal || !g_p2pEnabled || !g_p2pArmed || g_p2pLaunched || g_p2pLobbyError[0]) return;
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
    P2PHostCurrentRoom();
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
    InterlockedExchange(&g_p2pStop, 1);
    InterlockedExchange(&g_piMatchOver, 1); /* the room stays armed: the next Ready starts a fresh host */
}
/* Main thread: the launched host's match is over (the match ends, the server stays up, p2p_inproc.c).
 * The owner is still in the room: it is armed again for the next Ready, which starts a fresh host. */
static void P2PMatchOverTick(void)
{
    if (!InterlockedExchange(&g_piMatchOver, 0)) return;
    if (!g_p2pLaunched) return;
    Log("p2p: the local match is over; the room is armed again for the next Ready");
    g_p2pLobbyError[0] = 0;
    if (g_p2pHasRoom && g_p2pEnabled) P2PArmLaunch(g_p2pIntentConfig.port);
    else P2PDisarm();
}
static void P2PLobbyTick(void)
{
    static char previous[1024];
    char snapshot[1024], error[256], number[24]; int state;
    if (!g_p2pInit) return;
    if(!C1DvarsReady())return;
    if(g_p2pLaunchPending) {
        P2PConfig selected=g_p2pPendingConfig;g_p2pLaunchPending=FALSE;
        if(InterlockedCompareExchange(&g_p2pStop,0,0) || !P2PStartConfigured(NULL,NULL,NULL,&selected))
            P2PSetState(P2P_FAILED,"Queued local host activation was refused.");
    }
    P2PAutoTick();
    P2PMatchOverTick();
    P2PLaunchTick();
    if (g_p2pLobbyPending && (!g_p2pThread || WaitForSingleObject(g_p2pThread,0)==WAIT_OBJECT_0)) {
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
    if (g_p2pEnabled && state==P2P_EXITED && P2PInprocUsed() && !error[0])
        lstrcpyA(error,"Restart the client before hosting another Local Play match.");
    if (!g_p2pEnabled) g_p2pIntentUid=P2PLocalUid();
    if (!g_p2pEnabled && !g_p2pIntentConfig.state[0]) {
        P2PDefaults(&g_p2pIntentConfig);P2PLoadIni(&g_p2pIntentConfig);
    }
    DWORD now=GetTickCount();
    if (g_p2pEnabled) P2PLaunchWatch(state,now);
    if (g_p2pEnabled) P2PLaunchAbort(state,error);
    /* Armed: the choice is recorded and valid, and nothing runs. It is never "ready". */
    BOOL armed=g_p2pEnabled && g_p2pArmed && !g_p2pLaunched && !error[0] && state==P2P_IDLE;
    BOOL launching=g_p2pEnabled && g_p2pLaunched && !error[0] && (state==P2P_IDLE || state==P2P_STARTING);
    const char *label=armed ? "armed":kP2PState[state];
    _snprintf(snapshot,sizeof(snapshot),"%d|%u|%d|%s|%s|%d%d",state,g_p2pLobbyRequest,g_p2pEnabled,g_p2pLobbyMap,error,armed,launching);
    snapshot[sizeof(snapshot)-1]=0;
    BOOL changed=strcmp(snapshot,previous)!=0;
    if (!changed) return;
    /* Other threads (the in-process lobby) read this copy, never the main thread's working state. */
    EnterCriticalSection(&g_p2pLock);
    memset(&g_p2pApi,0,sizeof(g_p2pApi));
    g_p2pApi.size=sizeof(g_p2pApi);g_p2pApi.owner_uid=g_p2pIntentUid;g_p2pApi.enabled=g_p2pEnabled ? 1:0;
    g_p2pApi.armed=armed ? 1:0;g_p2pApi.launched=g_p2pEnabled && g_p2pLaunched ? 1:0;
    g_p2pApi.ready=g_p2pEnabled && state==P2P_READY && !error[0] ? 1:0;
    g_p2pApi.failed=error[0] || state==P2P_FAILED ? 1:0;
    g_p2pApi.port=g_p2pIntentConfig.port; /* .protocol stays 0: the only one */
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
    done=C1QueueSet("ui_p2p_startup_protocol","0") && done; /* the UI still asks; 0 is the only protocol */
    done=C1QueueSet("ui_p2p_state",label) && done;
    done=C1QueueSet("ui_p2p_error",error) && done;
    done=C1QueueSet("ui_p2p_map",g_p2pLobbyMap) && done;
    done=C1QueueSet("ui_p2p_ready",g_p2pEnabled && state==P2P_READY && !error[0] ? "1":"0") && done;
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
    c.difficultyPresent=g_p2pRoom.difficultyPresent;c.difficulty=g_p2pRoom.difficulty;
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
}
/* Internal room context is separate from the user's one-bit hosting switch. */
static void __cdecl P2PCmdRoom(void)
{
    const char *args[3];P2PRoomContext room;
    int count=VoiceArgc()-1;
    if (count!=3) { lstrcpyA(g_p2pLobbyError,"Choose a match lobby first.");P2PLobbyTick();return; }
    for (int i=0;i<3;++i) args[i]=VoiceArgv(i+1);
    const char *bad=P2PParseRoomContext(count,args,&room);
    if (bad) { lstrcpynA(g_p2pLobbyError,bad,sizeof(g_p2pLobbyError));P2PLobbyTick();return; }
    g_p2pRoom=room;g_p2pHasRoom=TRUE;
    /* Offline mode: the room the UI shows is armed whether or not anything was ticked. */
    if (P2PAutoMode()) { g_p2pEnabled=TRUE;if (!g_p2pIntentUid) g_p2pIntentUid=P2PLocalUid(); }
    if (g_p2pEnabled) P2PArmLegacyRoom();
    P2PLobbyTick();
}
static void __cdecl P2PCmdLocalhost(void)
{
    const char *value=VoiceArgc()==2 ? VoiceArgv(1):"";
    if (strcmp(value,"0") && strcmp(value,"1")) { C1Printf("p2p_localhost 1 or 0\n");return; }
    if (P2PAutoMode() && !strcmp(value,"0")) {
        /* Offline mode has no off: the room UI's untick (or its reset on leaving a room) changes
         * nothing. The lobby disarms when the owner leaves the room (K33LocalPlayLeave). */
        Log("p2p: p2p_localhost 0 ignored: local hosting is always on in offline mode");
        P2PLobbyTick();return;
    }
    g_p2pEnabled=!strcmp(value,"1");g_p2pLobbyError[0]=0;
    if (g_p2pEnabled) g_p2pIntentUid=P2PLocalUid();
    /* Ticking the box only arms (P2PArmLegacyRoom). Clearing it disarms and ends a launched host. */
    if (g_p2pEnabled) P2PArmLegacyRoom();
    else { P2PDisarm();g_p2pLaunchPending=FALSE;g_p2pLobbyPending=FALSE;InterlockedExchange(&g_p2pStop,1); }
    P2PLobbyTick();
}

/* "p2p_start_intent" / "p2p_cancel_start": the Ready/Start intent and its cancel of the launcher
 * startup protocols 1-3, which are gone. The UI still sends them (protocol 0 ignored them), so the
 * names stay registered - as no-ops - rather than reaching the server as unknown commands. */
static void __cdecl P2PCmdStartIntent(void)
{
}
static void __cdecl P2PCmdCancelStart(void)
{
}

/* "p2p_launch": the explicit launch input on the main thread (the UI or a test; the in-process
 * lobby posts its launch with K33LocalPlayAuto). It carries no selection and grants nothing by itself: it only
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

int K33LocalPlayAuto(unsigned long long owner_uid,const char *session_map,const char *gametype,const char *type,int difficulty,int launch,int *port,char *error,int error_size)
{
    const char *args[3]={session_map,gametype,type};
    const char *bad;P2PRoomContext room;P2PConfig c;K33LocalPlayIntentInfo now;BOOL same=FALSE;
    if (error && error_size>0) error[0]=0;
    if (port) *port=0;
    if (!g_p2pInit || !P2PAutoMode()) { /* not this client's mode: nothing is posted */
        if (error && error_size>0) lstrcpynA(error,"Local hosting is not available in this session.",error_size);
        return 0;
    }
    if (!owner_uid || !session_map || !gametype || !type) bad="No room selection.";
    else bad=P2PParseRoomContext(3,args,&room);
    if (!bad) {
        if (!strcmp(room.category,"zm") && difficulty>=0 && difficulty<=2) { room.difficultyPresent=1;room.difficulty=difficulty; }
        P2PDefaults(&c);P2PLoadIni(&c);
        lstrcpyA(c.map,room.map);lstrcpyA(c.sessionMap,room.session);
        lstrcpyA(c.gametype,room.gametype);lstrcpyA(c.zoneType,room.category);
        lstrcpyA(c.netIp,"127.0.0.1");lstrcpyA(c.mode,"inprocess");
        if (port) *port=c.port;
        /* the launched host of exactly this selection is simply joined again */
        same=K33LocalPlayIntent(&now) && now.enabled && now.launched && !now.failed && !strcmp(now.session_map,room.session) &&
            !strcmp(now.gametype,room.gametype) && !strcmp(now.type,room.category);
        if (!same) bad=P2PInprocUsed() ? "Restart the client before hosting another Local Play match.":P2PCheck(&c);
    }
    EnterCriticalSection(&g_p2pLock);
    if (bad) {
        g_p2pAuto.kind=P2P_AUTO_ERROR;g_p2pAuto.launch=FALSE;g_p2pAuto.owner=owner_uid;
        lstrcpynA(g_p2pAuto.error,bad,sizeof(g_p2pAuto.error));
    } else {
        /* a launch already posted for this same selection is not taken back by a later arm */
        BOOL keep=g_p2pAuto.kind==P2P_AUTO_ROOM && g_p2pAuto.launch && g_p2pAuto.owner==owner_uid && P2PRoomSame(&g_p2pAuto.room,&room);
        g_p2pAuto.kind=P2P_AUTO_ROOM;g_p2pAuto.launch=launch || keep;g_p2pAuto.owner=owner_uid;g_p2pAuto.room=room;
        g_p2pAuto.error[0]=0;
    }
    LeaveCriticalSection(&g_p2pLock);
    if (bad && error && error_size>0) lstrcpynA(error,bad,error_size);
    return bad ? 0:1;
}
void K33LocalPlayLeave(unsigned long long owner_uid)
{
    if (!g_p2pInit || !P2PAutoMode()) return;
    EnterCriticalSection(&g_p2pLock);
    g_p2pAuto.kind=P2P_AUTO_LEAVE;g_p2pAuto.launch=FALSE;g_p2pAuto.owner=owner_uid;g_p2pAuto.error[0]=0;
    LeaveCriticalSection(&g_p2pLock);
}

/* ---- commands ------------------------------------------------------------------------------ */

static void __cdecl P2PCmdHost(void)
{
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
    P2PDisarm();
    g_p2pEnabled = FALSE;
    g_p2pLobbyPending = FALSE;
    g_p2pLobbyError[0] = 0;
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
 * start hosting at once with +set sv_p2pHost 1, so the server loads its map while the client starts. */
static void InstallP2PHost(void)
{
    g_p2pMainThread=GetCurrentThreadId(); /* TakeOver calls the original C1 entry on this thread. */
    if (!g_p2pInit) { InitializeCriticalSection(&g_p2pLock); g_p2pInit = TRUE; }
    C1GameInit();
    K33OnMainTick(P2PLobbyTick);
    int added = 0;
    for (unsigned i = 0; i < sizeof(g_p2pCmds) / sizeof(g_p2pCmds[0]); i++) added += C1AddCommand(&g_p2pCmds[i]);
    const char *cl = GetCommandLineA();
    BOOL autostart = cl && (strstr(cl, "sv_p2pHost 1") || strstr(cl, "sv_p2pHost \"1\""));
    Log("p2p: %d commands (p2phost, p2pstop, p2pstatus)%s", added, autostart ? "; hosting at startup (sv_p2pHost 1)" : "");
    if (autostart) P2PStart(NULL, NULL, NULL);
}

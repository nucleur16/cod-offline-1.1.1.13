/*
 * offline_lobby.c - offline mode: the Demonware lobby runs inside the client (docs/OFFLINE_LOBBY.md).
 *
 * Compiled in only with -DCODOL_OFFLINE_LOBBY=1 (K33_OFFLINE=1 in build-profile.sh) and active only
 * when the player asks for it:
 *
 *     +set k33_offline 1          on the command line, or
 *     <exe dir>\k33_offline.ini   with a line  enabled = 1
 *
 * With the switch off (or the feature not compiled in) nothing in this file runs and Kernel33
 * behaves exactly as before against the real lobby.
 *
 * What offline mode does, all before C1's own entry point runs:
 *   1. starts the lobby (offline/, C++) on its own thread, bound to 127.0.0.1 only;
 *   2. answers C1's reads of dwauth.ini and stun.ini from memory, so C1 logs on to that listener
 *      (the files on disk are neither read nor written);
 *   3. gives C1 a profile name when the command line has none (+set dw_username <name>);
 *   4. keeps the host gate on loopback: no lobby name, no gateway and no SSDP group is vouched;
 *   5. lets Local Play host with the GSC tree installed with the client and points the embedded
 *      match host's state / profile folders at the offline store (OfflineP2PConfig).
 *
 * The lobby thread never touches C1. It learns the Local Play state from a snapshot the main-thread
 * tick publishes (OfflineLobbyTick), and a room start it answers is handed to that tick, which starts
 * the embedded host through p2p.c on the main thread.
 */
#if CODOL_OFFLINE_LOBBY
#include "offline/dw_api.h"

#define OFL_DEFAULT_PORT     3074   /* what C1's bdNetStartParams uses for NAT traversal (0x00A50454) */
#define OFL_DEFAULT_ALT_PORT 3078
#ifndef OFL_NATTRAV_PORT_VA /* the Windows fixture supplies its own site */
#define OFL_NATTRAV_PORT_VA  0x00A50454u /* mov ecx, 0xC02 ; mov [edi+14h], cx  (m_natTravPort) */
#endif

static struct {
    int requested;               /* 0 unknown, 1 on, -1 off */
    BOOL active;                 /* the listener is up and C1 is routed to it */
    int port, altPort;
    char name[32];               /* profile name for a command line without dw_username */
    char root[MAX_PATH];         /* <exe dir>\k33_offline */
    char profile[MAX_PATH], state[MAX_PATH], match[MAX_PATH], resources[MAX_PATH], zones[MAX_PATH], logPath[MAX_PATH];
    FARPROC createFileA, getCommandLineA;
    char *commandLine;
    CRITICAL_SECTION lock;       /* guards route (main thread writes, lobby thread reads) and the log file */
    DwOfflineRoute route;
    volatile LONG startPending;
    FILE *log;
} g_ofl;

static void OflExeDir(char *out)
{
    DWORD n = GetModuleFileNameA(NULL, out, MAX_PATH);
    while (n && out[n - 1] != '\\') n--;
    out[n] = 0;
}

/* `+set k33_offline 1` (or "1") anywhere on the command line. */
static BOOL OflCommandLineSwitch(const char *cl)
{
    return cl && (strstr(cl, "k33_offline 1") || strstr(cl, "k33_offline \"1\""));
}

static BOOL OflNameOk(const char *s)
{
    size_t n = strlen(s);
    if (!n || n > 31) return FALSE;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9') ||
              s[i] == '_' || s[i] == '-' || s[i] == '.')) return FALSE;
    return TRUE;
}

/* Decide once, before FillImports. The ini is optional; the command line wins for the switch. */
static void OflReadSwitch(void)
{
    if (g_ofl.requested) return;
    char dir[MAX_PATH], path[MAX_PATH + 32], line[512];
    int enabled = 0;
    OflExeDir(dir);
    g_ofl.port = OFL_DEFAULT_PORT;
    g_ofl.altPort = OFL_DEFAULT_ALT_PORT;
    lstrcpyA(g_ofl.name, "Player");
    _snprintf(g_ofl.root, MAX_PATH, "%sk33_offline", dir);
    g_ofl.root[MAX_PATH - 1] = 0;
    _snprintf(path, sizeof(path), "%sk33_offline.ini", dir);
    path[sizeof(path) - 1] = 0;
    FILE *f = fopen(path, "rb");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *hash = strchr(line, '#');
            if (hash) *hash = 0;
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            char *key = line, *val = eq + 1;
            while (*key == ' ' || *key == '\t') key++;
            char *e = key + strlen(key);
            while (e > key && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
            while (*val == ' ' || *val == '\t') val++;
            e = val + strlen(val);
            while (e > val && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
            if (!_stricmp(key, "enabled")) enabled = !strcmp(val, "1");
            else if (!_stricmp(key, "name")) { if (OflNameOk(val)) lstrcpyA(g_ofl.name, val); }
            else if (!_stricmp(key, "port")) { int p = atoi(val); if (p >= 1024 && p <= 65535) g_ofl.port = p; }
            else if (!_stricmp(key, "alt_port")) { int p = atoi(val); if (p >= 1024 && p <= 65535) g_ofl.altPort = p; }
            else if (!_stricmp(key, "root")) { if (*val && !strchr(val, '"') && strlen(val) < MAX_PATH - 32) lstrcpyA(g_ofl.root, val); }
        }
        fclose(f);
    }
    if (OflCommandLineSwitch(GetCommandLineA())) enabled = 1;
    g_ofl.requested = enabled ? 1 : -1;
}

static BOOL OfflineLobbyRequested(void)
{
    OflReadSwitch();
    return g_ofl.requested == 1;
}

static BOOL OfflineLobbyActive(void) { return g_ofl.active; }

static void OflLog(const char *line) /* any thread */
{
    EnterCriticalSection(&g_ofl.lock);
    if (g_ofl.log) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(g_ofl.log, "[%02d:%02d:%02d.%03d] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line);
        fflush(g_ofl.log);
    }
    LeaveCriticalSection(&g_ofl.lock);
}

static int OflLocalRoute(unsigned long long uid, DwOfflineRoute *out) /* lobby thread */
{
    (void)uid; /* one player: the room owner is this client */
    EnterCriticalSection(&g_ofl.lock);
    *out = g_ofl.route;
    LeaveCriticalSection(&g_ofl.lock);
    return 1;
}

static void OflStartGame(const DwOfflineStartInfo *info) /* lobby thread */
{
    char line[256];
    _snprintf(line, sizeof(line), "kernel33: room start -> %s %s (%s) on 127.0.0.1:%d%s", info->sessionMap, info->gametype,
              info->zoneType, info->hostPort, info->local ? "" : " (NOT local)");
    line[sizeof(line) - 1] = 0;
    OflLog(line);
    if (info->local) InterlockedExchange(&g_ofl.startPending, 1);
}

/* A file C1 opens for reading, served from memory: a delete-on-close temporary holding `text`. */
static HANDLE OflSyntheticFile(const char *text)
{
    char dir[MAX_PATH], path[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, dir) || !GetTempFileNameA(dir, "k33", 0, path)) return INVALID_HANDLE_VALUE;
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h == INVALID_HANDLE_VALUE) { DeleteFileA(path); return h; }
    DWORD wrote = 0, len = (DWORD)strlen(text);
    if (!WriteFile(h, text, len, &wrote, NULL) || wrote != len || SetFilePointer(h, 0, NULL, FILE_BEGIN) != 0) {
        CloseHandle(h);
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    return h;
}

static const char *OflBaseName(const char *path)
{
    const char *b = path;
    for (const char *p = path; *p; p++) if (*p == '\\' || *p == '/') b = p + 1;
    return b;
}

/* C1's CRT fopen reaches CreateFileA through slot 0xB421D8 (local_multi.c). dwauth.ini is the five-line
 * route C1 logs on with (auth host, auth port, lobby host, lobby port, title id in hex); stun.ini is
 * up to ten NAT-discovery hosts (C1 0x00630CE2 / 0x00630D70). */
static HANDLE WINAPI OflCreateFileA(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD disposition,
                                    DWORD flags, HANDLE templateFile)
{
    if (g_ofl.active && path && access == GENERIC_READ && disposition == OPEN_EXISTING) {
        const char *base = OflBaseName(path);
        char text[256];
        if (!lstrcmpiA(base, "dwauth.ini")) {
            _snprintf(text, sizeof(text), "127.0.0.1\n%d\n127.0.0.1\n%d\n3ea\n", g_ofl.port, g_ofl.port);
            text[sizeof(text) - 1] = 0;
            return OflSyntheticFile(text);
        }
        if (!lstrcmpiA(base, "stun.ini"))
            return OflSyntheticFile("127.0.0.1\n127.0.0.1\n127.0.0.1\n127.0.0.1\n127.0.0.1\n127.0.0.1\n127.0.0.1\n127.0.0.1\n127.0.0.1\n127.0.0.1\n");
    }
    return ((HANDLE(WINAPI *)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE))g_ofl.createFileA)(
        path, access, share, security, disposition, flags, templateFile);
}

/* The profile name IS the career (C1 derives the player id from dw_username). */
static LPSTR WINAPI OflGetCommandLineA(void)
{
    LPSTR real = ((LPSTR(WINAPI *)(void))g_ofl.getCommandLineA)();
    if (!g_ofl.active || !real || strstr(real, "dw_username")) return real;
    if (!g_ofl.commandLine) {
        size_t n = strlen(real) + 64;
        char *text = (char *)HeapAlloc(GetProcessHeap(), 0, n);
        if (!text) return real;
        _snprintf(text, n, "%s +set dw_username %s", real, g_ofl.name);
        text[n - 1] = 0;
        g_ofl.commandLine = text;
    }
    return g_ofl.commandLine;
}

/* FillImports: wrap the two slots only when offline mode was asked for. */
static FARPROC OfflineRouteImport(const C1Import *imp, FARPROC original)
{
    if (!OfflineLobbyRequested() || lstrcmpiA(imp->dll, "KERNEL32.dll")) return original;
    if (imp->slot == 0x00B421D8 && !strcmp(imp->name, "CreateFileA")) { g_ofl.createFileA = original; return (FARPROC)OflCreateFileA; }
    if (imp->slot == 0x00B42284 && !strcmp(imp->name, "GetCommandLineA")) { g_ofl.getCommandLineA = original; return (FARPROC)OflGetCommandLineA; }
    return original;
}

/* A lobby port other than 3074: C1 sends its NAT-discovery probes to m_natTravPort, a constant in
 * bdNetStartParams' constructor. Point it at the offline listener. */
static BOOL OflPatchNatTravPort(int port)
{
    static const BYTE expected[] = { 0xB9, 0x02, 0x0C, 0x00, 0x00, 0x66, 0x89, 0x4F, 0x14 };
    BYTE *site = (BYTE *)(DWORD_PTR)OFL_NATTRAV_PORT_VA;
    if (!Readable(site, sizeof(expected)) || memcmp(site, expected, sizeof(expected))) return FALSE;
    DWORD imm = (DWORD)port;
    WriteCode(site + 1, &imm, sizeof(imm));
    return TRUE;
}

/* TakeOver, after FillImports and before the host gate. -> FALSE when offline mode was requested and
 * cannot be provided (the caller stops: falling back to the real lobby would not be what was asked). */
static BOOL InstallOfflineLobby(void)
{
    if (!OfflineLobbyRequested()) return TRUE;
    InitializeCriticalSection(&g_ofl.lock);
    if (!g_ofl.createFileA || !g_ofl.getCommandLineA) { Log("offline lobby: C1's file/command-line imports were not found"); return FALSE; }
    /* Launcher-managed Local Play (the SDK pipe protocols, the production host) is a different product. */
    if (GetEnvironmentVariableA("CODOL_P2P_PRODUCTION_PIPE", NULL, 0) || GetEnvironmentVariableA("CODOL_P2P_STARTUP_IPC_V1", NULL, 0) ||
        GetEnvironmentVariableA("CODOL_LOCAL_QA_DWAUTH_HANDLE", NULL, 0)) {
        Log("offline lobby: refused, this client was started by a launcher-managed Local Play session");
        return FALSE;
    }
    char dir[MAX_PATH];
    OflExeDir(dir);
    _snprintf(g_ofl.profile, MAX_PATH, "%s\\profile", g_ofl.root);
    _snprintf(g_ofl.state, MAX_PATH, "%s\\state", g_ofl.root);
    _snprintf(g_ofl.match, MAX_PATH, "%s\\match", g_ofl.root);
    _snprintf(g_ofl.resources, MAX_PATH, "%s\\resources", g_ofl.root);
    _snprintf(g_ofl.logPath, MAX_PATH, "%s\\lobby.log", g_ofl.root);
    _snprintf(g_ofl.zones, MAX_PATH, "%sclient_shipRetail\\chinese", dir);
    g_ofl.profile[MAX_PATH - 1] = g_ofl.state[MAX_PATH - 1] = g_ofl.match[MAX_PATH - 1] = 0;
    g_ofl.resources[MAX_PATH - 1] = g_ofl.logPath[MAX_PATH - 1] = g_ofl.zones[MAX_PATH - 1] = 0;
    CreateDirectoryA(g_ofl.root, NULL);
    CreateDirectoryA(g_ofl.profile, NULL);
    CreateDirectoryA(g_ofl.state, NULL);
    CreateDirectoryA(g_ofl.match, NULL);
    g_ofl.log = fopen(g_ofl.logPath, "wb"); /* one session per file; NULL just means no lobby log */

    P2PConfig p2p;
    P2PDefaults(&p2p);
    P2PLoadIni(&p2p); /* its port: the match host's UDP port (state/profile are overridden below) */
    DwOfflineConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.resources = GetFileAttributesA(g_ofl.resources) != INVALID_FILE_ATTRIBUTES ? g_ofl.resources : NULL;
    cfg.profile = g_ofl.profile;
    cfg.state = g_ofl.state;
    cfg.matchProfile = g_ofl.match;
    cfg.zoneDir = g_ofl.zones;
    cfg.port = g_ofl.port;
    cfg.altPort = g_ofl.altPort;
    cfg.hostPort = p2p.port;
    cfg.log = OflLog;
    cfg.localRoute = OflLocalRoute;
    cfg.startGame = OflStartGame;
    if (g_ofl.port != OFL_DEFAULT_PORT && !OflPatchNatTravPort(g_ofl.port)) {
        Log("offline lobby: port %d needs C1's NAT traversal port patched and its bytes differ; use port %d", g_ofl.port, OFL_DEFAULT_PORT);
        return FALSE;
    }
    int port = DwOfflineStartServer(&cfg);
    if (!port) {
        Log("offline lobby: could not listen on 127.0.0.1:%d (is another lobby running on this PC?)", g_ofl.port);
        return FALSE;
    }
    g_ofl.active = TRUE;
    Log("offline lobby: ACTIVE on 127.0.0.1:%d (store %s); C1 is routed to it in memory, dwauth.ini / stun.ini on disk are not used",
        port, g_ofl.root);
    return TRUE;
}

/* P2PLoadIni: the embedded match host shares its state and profile folders with the offline lobby,
 * whatever p2p_host.ini says (an install that also plays online points them at the real lobby). */
static void OfflineP2PConfig(P2PConfig *c)
{
    if (!g_ofl.active) return;
    lstrcpynA(c->state, g_ofl.state, MAX_PATH);
    lstrcpynA(c->profile, g_ofl.match, MAX_PATH);
    c->rconFile[0] = 0;   /* <state>\native_server_rcon.txt, created on first use */
    c->rollout[0] = 0;    /* no external lobby to register with */
    lstrcpyA(c->netIp, "127.0.0.1");
    lstrcpyA(c->mode, "inprocess");
    c->admission = 1;
    /* P2PCheck permits exactly one extra string (the two match-lifecycle switches); anything else an
     * older p2p_host.ini carries would refuse hosting, so it is dropped rather than passed on. */
    if (strcmp(c->extra, "+set codol_exitToLobby 1 +set codol_oneShotMatch 1")) c->extra[0] = 0;
}

/* Main thread, every frame: publish the Local Play state for the lobby thread, and start the embedded
 * host for a room start the lobby answered if the player's click has not already done so. */
static void OfflineLobbyTick(void)
{
    if (!g_ofl.active || !g_p2pInit) return;
    DwOfflineRoute r;
    memset(&r, 0, sizeof(r));
    r.enabled = g_p2pEnabled && g_p2pHasRoom;
    EnterCriticalSection(&g_p2pLock);
    int state = g_p2p.state;
    r.port = g_p2p.cfg.port ? g_p2p.cfg.port : (g_p2pIntentConfig.port ? g_p2pIntentConfig.port : P2P_DEFAULT_PORT);
    LeaveCriticalSection(&g_p2pLock);
    if (g_p2pHasRoom) {
        lstrcpynA(r.map, g_p2pRoom.map, sizeof(r.map));
        lstrcpynA(r.sessionMap, g_p2pRoom.session, sizeof(r.sessionMap));
        lstrcpynA(r.gametype, g_p2pRoom.gametype, sizeof(r.gametype));
        lstrcpynA(r.category, g_p2pRoom.category, sizeof(r.category));
    }
    EnterCriticalSection(&g_ofl.lock);
    g_ofl.route = r;
    LeaveCriticalSection(&g_ofl.lock);
    if (InterlockedExchange(&g_ofl.startPending, 0) && r.enabled && P2PSelectedProtocolMode() == 0 &&
        (state == P2P_IDLE || state == P2P_EXITED || state == P2P_FAILED) && !P2PStartupBusy())
        P2PHostCurrentRoom();
}

static void InstallOfflineLobbyTick(void)
{
    if (g_ofl.active && !K33OnMainTick(OfflineLobbyTick)) Log("offline lobby: no main-thread tick slot; Local Play state will not reach the lobby");
}
#else
static BOOL OfflineLobbyActive(void) { return FALSE; }
#endif

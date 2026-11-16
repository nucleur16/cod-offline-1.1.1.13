/*
 * offline_lobby.c - the Demonware lobby runs inside the client (docs/OFFLINE_LOBBY.md).
 *
 * This client is offline only. There is no switch: every build has the lobby and every run uses it.
 * <exe dir>\k33_offline.ini is an OPTIONAL settings file (profile name, test port overrides,
 * level_gates, folders); `+set dw_username <name>` on the command line names the profile, else the
 * ini's `name`, else "Player".
 *
 * All of it happens before C1's own entry point runs:
 *   1. starts the lobby (offline/, C++) on its own thread, bound to 127.0.0.1 only, on a FREE port
 *      the operating system picks (so it runs next to another lobby or a second client on this PC);
 *   2. answers C1's reads of dwauth.ini and stun.ini from memory with that port, and points C1's NAT
 *      traversal port constant at it, so C1 logs on to that listener (the files on disk are neither
 *      read nor written);
 *   3. gives C1 a profile name when the command line has none (+set dw_username <name>);
 *   4. keeps the host gate on loopback: no lobby name, no gateway and no SSDP group is vouched;
 *   5. hosts every room the player owns on the embedded match server, with the GSC tree installed
 *      with the client: gives that host a UDP port that is free (kept reserved until the host binds
 *      it) and points its state / profile folders at the run-time folder (OfflineP2PConfig);
 *   6. lifts C1's level gates on playing: the room list, Create Room, the playlists and the map list
 *      are usable by a level-1 profile (OflLiftGates). The career itself is not touched: a level-1
 *      player is shown, stored and reported as level 1, and loadout unlocks keep their levels.
 * If the lobby cannot start (no free port, an explicit port that is taken, C1's bytes at a patch site
 * differ, a client started by a launcher Local Play session) the client stops with the reason; it never contacts another lobby.
 *
 * The player's career is the game's own profile blobs in <exe dir>\players\<profile name>\
 * (src/offline/dw_store.h); there is no database.
 *
 * The lobby thread never touches C1. It reads Kernel33's published local-host snapshot with
 * K33LocalPlayIntent(), names the selection of the room the player owns with K33LocalPlayAuto() (which
 * arms the host and, when the lobby hands START_GAME over, raises its launch) and reports a left room
 * with K33LocalPlayLeave() (p2p_local_play_api.h, docs/P2P_DEFERRED_HOST_START.md): all are safe on any
 * thread and start nothing themselves - p2p.c's main-thread tick validates the request and starts the host.
 */
#if CODOL_OFFLINE_LOBBY
#include "offline/dw_api.h"

#define OFL_C1_NATTRAV_PORT  3074   /* what C1's bdNetStartParams uses for NAT traversal (0x00A50454) */
#ifndef OFL_NATTRAV_PORT_VA /* the Windows fixture supplies its own sites */
#define OFL_NATTRAV_PORT_VA  0x00A50454u /* mov ecx, 0xC02 ; mov [edi+14h], cx  (m_natTravPort) */
#endif
/* AS_IsNotificationUnlocked (0x005AB300, the Scaleform binding of every lobby UI lock) calls
 * IsNotificationUnlocked(name) (0x005B5EB0, its only caller - MEASURED): mp/uiNotifications.csv row ->
 * unlock flag -> a membership test against the unlock-flag set the lobby serves. */
#ifndef OFL_NOTIFY_CALL_VA
#define OFL_NOTIFY_CALL_VA   0x005AB321u /* call IsNotificationUnlocked */
#define OFL_NOTIFY_FN_VA     0x005B5EB0u /* bool __cdecl IsNotificationUnlocked(const char *name) -> al */
#endif
/* AS_IsMapUnlocked (0x005AC260) answers "unlocked" for every map while C1's own bool dvar "2653"
 * is set (registered at 0x005EB25D, default 0, no flags; read by name at 0x005AC266). */
#define OFL_UNLOCK_MAPS_DVAR "2653"

/* The console's `name` for a profile (player_name.c sets this once it is installed; NULL = the descriptor's name). */
static int (*volatile g_oflDisplayName)(const char *profileName, char *out, int outSize);
static int OflDisplayName(const char *profileName, char *out, int outSize) /* lobby thread */
{
    int (*fn)(const char *, char *, int) = g_oflDisplayName;
    return fn ? fn(profileName, out, outSize) : 0;
}

static struct {
    int settingsRead;            /* k33_offline.ini was looked for */
    BOOL active;                 /* the listener is up and C1 is routed to it */
    int wantPort, wantAltPort;   /* k33_offline.ini port / alt_port; 0 = a free port (the default) */
    int wantHostPort;            /* k33_offline.ini host_port; 0 = the configured one if free, else a free one */
    int keepGates;               /* k33_offline.ini level_gates = 1: leave C1's level gates in place */
    int port, altPort;           /* what the lobby listens on */
    int hostPort;                /* the embedded match host's UDP port in this run; 0 = p2p_host.ini's */
    volatile LONG hostSock;      /* the socket that keeps hostPort reserved until the host starts, or -1 */
    BOOL gatesLifted;
    char name[32];               /* ini profile name, for a command line without dw_username */
    char player[32];             /* the profile name in use = the folder under players\; "" = not a usable folder name */
    char players[MAX_PATH];      /* <exe dir>\players */
    char runtime[MAX_PATH];      /* <exe dir>\p2p\offline: what the lobby and the embedded match host exchange */
    char legacy[MAX_PATH];       /* <exe dir>\k33_offline\profile: the store of the first offline builds */
    char resources[MAX_PATH], zones[MAX_PATH], logPath[MAX_PATH];
    char game[MAX_PATH];         /* <exe dir>, without the trailing backslash */
    FARPROC createFileA, getCommandLineA;
    char *commandLine;
    CRITICAL_SECTION lock;       /* guards the log file (lobby thread and main thread) */
    FILE *log;
} g_ofl = { .hostSock = -1 };

static void OflExeDir(char *out)
{
    DWORD n = GetModuleFileNameA(NULL, out, MAX_PATH);
    while (n && out[n - 1] != '\\') n--;
    out[n] = 0;
}

/* A profile name is also the player's folder under players\: letters, digits, _ - . only. */
static BOOL OflNameOk(const char *s)
{
    size_t n = strlen(s);
    if (!n || n > 31 || s[0] == '.' || s[n - 1] == '.') return FALSE;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9') ||
              s[i] == '_' || s[i] == '-' || s[i] == '.')) return FALSE;
    return TRUE;
}

/* The value of `+set dw_username <name>` on a command line. -> FALSE when there is none. */
static BOOL OflCommandLineName(const char *cl, char *out, size_t cap)
{
    const char *p = cl ? strstr(cl, "dw_username") : NULL;
    size_t n = 0;
    out[0] = 0;
    if (!p) return FALSE;
    p += 11;
    while (*p == ' ' || *p == '\t') p++;
    BOOL quoted = *p == '"';
    if (quoted) p++;
    while (*p && (quoted ? *p != '"' : (*p != ' ' && *p != '\t')) && n + 1 < cap) out[n++] = *p++;
    out[n] = 0;
    return TRUE;
}

static BOOL OflPortOk(int p) { return p >= 1024 && p <= 65535; }

/* The optional settings file, read once, before FillImports. There is nothing to switch on: an
 * `enabled` line (the switch of earlier builds) is ignored. */
static void OflReadSettings(void)
{
    if (g_ofl.settingsRead) return;
    g_ofl.settingsRead = 1;
    char dir[MAX_PATH], path[MAX_PATH + 32], line[512];
    OflExeDir(dir);
    lstrcpyA(g_ofl.name, "Player");
    _snprintf(g_ofl.players, MAX_PATH, "%splayers", dir);
    _snprintf(g_ofl.runtime, MAX_PATH, "%sp2p\\offline", dir);
    _snprintf(g_ofl.legacy, MAX_PATH, "%sk33_offline\\profile", dir);
    g_ofl.players[MAX_PATH - 1] = g_ofl.runtime[MAX_PATH - 1] = g_ofl.legacy[MAX_PATH - 1] = 0;
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
            BOOL pathOk = *val && !strchr(val, '"') && strlen(val) < MAX_PATH - 32;
            if (!_stricmp(key, "name")) { if (OflNameOk(val)) lstrcpyA(g_ofl.name, val); }
            /* explicit ports are for tests; the default (0) is whatever is free */
            else if (!_stricmp(key, "port")) { int p = atoi(val); if (OflPortOk(p)) g_ofl.wantPort = p; }
            else if (!_stricmp(key, "alt_port")) { int p = atoi(val); if (OflPortOk(p)) g_ofl.wantAltPort = p; }
            else if (!_stricmp(key, "host_port")) { int p = atoi(val); if (OflPortOk(p)) g_ofl.wantHostPort = p; }
            else if (!_stricmp(key, "level_gates")) g_ofl.keepGates = !strcmp(val, "1");
            else if (!_stricmp(key, "players")) { if (pathOk) lstrcpyA(g_ofl.players, val); }
            else if (!_stricmp(key, "runtime")) { if (pathOk) lstrcpyA(g_ofl.runtime, val); }
        }
        fclose(f);
    }
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
    K33LocalPlayIntentInfo in;
    (void)uid; /* the lobby compares the owner itself */
    memset(&in, 0, sizeof(in));
    in.size = sizeof(in);
    if (!K33LocalPlayIntent(&in)) return 0;
    memset(out, 0, sizeof(*out));
    out->ownerUid = in.owner_uid;
    out->enabled = in.enabled;
    out->armed = in.armed;
    out->launched = in.launched;
    out->ready = in.ready;
    out->failed = in.failed;
    out->port = in.port;
    lstrcpynA(out->map, in.map, sizeof(out->map));
    lstrcpynA(out->sessionMap, in.session_map, sizeof(out->sessionMap));
    lstrcpynA(out->gametype, in.gametype, sizeof(out->gametype));
    lstrcpynA(out->category, in.type, sizeof(out->category));
    lstrcpynA(out->state, in.state, sizeof(out->state));
    lstrcpynA(out->error, in.error, sizeof(out->error));
    return 1;
}

/* The lobby's selection for the room the local player owns: every such room is hosted by the embedded
 * server (there is nothing to tick in offline mode). Arms the host for it; with `launch` the lobby is
 * handing START_GAME to the owner now. */
static int OflStart(unsigned long long owner, const char *sessionMap, const char *gametype, const char *type, int difficulty,
                    int launch, int *port, char *error, int errorSize) /* lobby thread */
{
    int ok = K33LocalPlayAuto(owner, sessionMap, gametype, type, difficulty, launch, port, error, errorSize);
    if (launch || !ok) {
        char line[512];
        _snprintf(line, sizeof(line), "kernel33: local host %s for %s %s (%s)%s%s", !ok ? "REFUSED" : "launch raised", sessionMap, gametype, type,
                  ok ? "" : ": ", ok ? "" : (error ? error : ""));
        line[sizeof(line) - 1] = 0;
        OflLog(line);
    }
    return ok;
}

static void OflLeave(unsigned long long owner) /* lobby thread */
{
    K33LocalPlayLeave(owner);
}

static void OflStartGame(const DwOfflineStartInfo *info) /* lobby thread */
{
    char line[256];
    _snprintf(line, sizeof(line), "kernel33: room start -> %s %s (%s) on 127.0.0.1:%d%s", info->sessionMap, info->gametype,
              info->zoneType, info->hostPort, info->local ? "" : " (NOT local)");
    line[sizeof(line) - 1] = 0;
    OflLog(line);
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
 * up to ten NAT-discovery hosts (C1 0x00630CE2 / 0x00630D70). Both ports are the one the lobby got. */
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

/* C1's command line in offline mode: the profile name when it has none (the name IS the career: C1
 * derives the player id from dw_username), and the dvar that opens the map list (OflLiftGates). */
static LPSTR WINAPI OflGetCommandLineA(void)
{
    LPSTR real = ((LPSTR(WINAPI *)(void))g_ofl.getCommandLineA)();
    BOOL name = real && !strstr(real, "dw_username");
    if (!g_ofl.active || !real || (!name && !g_ofl.gatesLifted)) return real;
    if (!g_ofl.commandLine) {
        size_t n = strlen(real) + 96;
        char *text = (char *)HeapAlloc(GetProcessHeap(), 0, n);
        if (!text) return real;
        lstrcpyA(text, real);
        if (name) { lstrcatA(text, " +set dw_username "); lstrcatA(text, g_ofl.name); }
        if (g_ofl.gatesLifted) lstrcatA(text, " +set " OFL_UNLOCK_MAPS_DVAR " 1");
        g_ofl.commandLine = text;
    }
    return g_ofl.commandLine;
}

/* FillImports: wrap the two slots C1 reads its lobby route and its command line through. */
static FARPROC OfflineRouteImport(const C1Import *imp, FARPROC original)
{
    OflReadSettings();
    if (lstrcmpiA(imp->dll, "KERNEL32.dll")) return original;
    if (imp->slot == 0x00B421D8 && !strcmp(imp->name, "CreateFileA")) { g_ofl.createFileA = original; return (FARPROC)OflCreateFileA; }
    if (imp->slot == 0x00B42284 && !strcmp(imp->name, "GetCommandLineA")) { g_ofl.getCommandLineA = original; return (FARPROC)OflGetCommandLineA; }
    return original;
}

/* C1 sends its NAT-discovery probes to m_natTravPort, a constant 3074 in bdNetStartParams'
 * constructor. Point it at the offline listener's port. */
static BOOL OflPatchNatTravPort(int port)
{
    static const BYTE expected[] = { 0xB9, 0x02, 0x0C, 0x00, 0x00, 0x66, 0x89, 0x4F, 0x14 };
    BYTE *site = (BYTE *)(DWORD_PTR)OFL_NATTRAV_PORT_VA;
    if (!Readable(site, sizeof(expected)) || memcmp(site, expected, sizeof(expected))) return FALSE;
    DWORD imm = (DWORD)port;
    WriteCode(site + 1, &imm, sizeof(imm));
    return TRUE;
}

/* ---- level gates ----
 *
 * Every lock of the lobby UI is one question, AS_IsNotificationUnlocked("<notification>"): is the
 * unlock flag of that row of mp/uiNotifications.csv in the set the lobby served. The flags are
 * uf_rank_N (the player's level) or a challenge. The notifications below are the ones that stand
 * between a player and PLAYING: seeing the room list, creating a room, the playlists, the co-op and
 * zombies room types. In offline mode they answer "unlocked" whatever the level; the question is
 * answered here and nothing is added to the unlock-flag set, because uf_rank_5 / 10 / 15 are also the
 * flags of loadout slots (perk 3, killstreak 1, ...) and those keep their levels. Left as C1 has them:
 * un_weapon_customization, un_avatar_customization, un_mp_stattrack, un_marketplace (not about
 * playing a match), and every row without a flag. */
static const char *const kOflPlayNotifications[] = {
    "un_room_list",           /* Rooms                   uf_rank_5  */
    "un_mp_room_creation",    /* Create Room             uf_rank_5  */
    "un_training_playlist",   /*                         uf_rank_5  */
    "un_combat_training",     /* bot training            uf_rank_10 */
    "un_noob_tdm",            /*                         uf_rank_10 */
    "un_mp_playlist",         /* the playlists           uf_rank_15 */
    "un_specops",             /* Spec Ops challenge tab  cf_ch_coop_take_mp_estate_tropical */
    "un_coop_room_creation",  /* co-op rooms             cf_ch_coop_take_mp_estate_tropical */
    "un_zm_asylum_sh",        /* zombies maps            cf_ch_zm_asylum_sh_1 */
    "un_zm_prototype_sh",     /*                         cf_ch_zm_prototype_sh_1 */
};

static unsigned char __cdecl OflNotificationUnlocked(const char *name)
{
    if (name && Readable(name, 1))
        for (unsigned i = 0; i < sizeof(kOflPlayNotifications) / sizeof(kOflPlayNotifications[0]); i++)
            if (!strcmp(name, kOflPlayNotifications[i])) return 1;
    return ((unsigned char(__cdecl *)(const char *))(DWORD_PTR)OFL_NOTIFY_FN_VA)(name);
}

/* -> FALSE when C1's call site is not the measured one (nothing is written; the gates stay). */
static BOOL OflLiftGates(void)
{
    BYTE *site = (BYTE *)(DWORD_PTR)OFL_NOTIFY_CALL_VA;
    LONG rel;
    if (!Readable(site, 5) || site[0] != 0xE8) return FALSE;
    memcpy(&rel, site + 1, 4);
    if ((DWORD_PTR)site + 5 + rel != (DWORD_PTR)OFL_NOTIFY_FN_VA) return FALSE;
    rel = (LONG)((DWORD_PTR)OflNotificationUnlocked - ((DWORD_PTR)site + 5));
    WriteCode(site + 1, &rel, sizeof(rel));
    g_ofl.gatesLifted = TRUE; /* and OflGetCommandLineA sets the map-list dvar */
    return TRUE;
}

/* ---- the embedded match host's UDP port ---- */

static SOCKET OflBindLoopbackUdp(int port)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return s;
    BOOL excl = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&excl, sizeof(excl));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0) { closesocket(s); return INVALID_SOCKET; }
    return s;
}

static int OflBoundPort(SOCKET s)
{
    struct sockaddr_in a;
    int n = sizeof(a);
    return getsockname(s, (struct sockaddr *)&a, &n) == 0 ? ntohs(a.sin_port) : 0;
}

/* The sh1x host also listens on TCP at its port. Can that be bound right now? */
static BOOL OflLoopbackTcpFree(int port)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return TRUE; /* cannot tell */
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    BOOL ok = bind(s, (struct sockaddr *)&a, sizeof(a)) == 0;
    closesocket(s);
    return ok;
}

/* The port the embedded host will serve on in this run: p2p_host.ini's (17760 by default) when it is
 * free, else a free one. The socket that proved it free is KEPT until the host is about to bind
 * (OfflineHostPortRelease), so nothing else on this PC - a second offline client in particular - can
 * take the port between now and the match launch. An explicit host_port is used as it is.
 * A free one: the system's choice of a UDP port first; if that port cannot also be had for TCP
 * (Windows excludes port blocks per protocol, src/offline/dw_net.cpp), ports drawn at random from the
 * dynamic range. */
static void OflChooseHostPort(int configured)
{
    if (g_ofl.wantHostPort) { g_ofl.hostPort = g_ofl.wantHostPort; return; }
    g_ofl.hostPort = configured;
    SOCKET s = OflBindLoopbackUdp(configured);
    if (s == INVALID_SOCKET) {
        unsigned seed = GetTickCount() ^ (GetCurrentProcessId() * 2654435761u);
        for (int attempt = 0; attempt < 64 && s == INVALID_SOCKET; attempt++) {
            seed = seed * 1103515245u + 12345u;
            s = OflBindLoopbackUdp(attempt ? 49152 + (int)((seed >> 8) % 16384) : 0);
            if (s != INVALID_SOCKET && !OflLoopbackTcpFree(OflBoundPort(s))) { closesocket(s); s = INVALID_SOCKET; }
        }
    }
    if (s == INVALID_SOCKET) return; /* no port at all: the launch reports its own failure */
    int port = OflBoundPort(s);
    if (!port) { closesocket(s); return; }
    g_ofl.hostPort = port;
    InterlockedExchange(&g_ofl.hostSock, (LONG)s);
}

/* p2p.c, immediately before the host's own port check: give the reserved port up. Any thread. */
static void OfflineHostPortRelease(void)
{
    LONG s = InterlockedExchange(&g_ofl.hostSock, -1);
    if (s != -1) closesocket((SOCKET)(ULONG)s);
}

/* p2p.c, after a host session ended and was taken down: hold this run's host port again until the next launch,
 * so that another program cannot take it between two matches. Any thread. */
static void OfflineHostPortReserve(void)
{
    if (g_ofl.wantHostPort || g_ofl.hostPort < 1 || g_ofl.hostPort > 65535) return;
    SOCKET s = OflBindLoopbackUdp(g_ofl.hostPort);
    if (s == INVALID_SOCKET) { Log("offline lobby: the match host port %d could not be held again after the match", g_ofl.hostPort); return; }
    LONG old = InterlockedExchange(&g_ofl.hostSock, (LONG)s);
    if (old != -1) closesocket((SOCKET)(ULONG)old);
}

/* TakeOver, after FillImports and before the host gate. -> FALSE when the lobby cannot be provided:
 * the caller stops the client with the reason (there is no other lobby to fall back to). */
static BOOL InstallOfflineLobby(void)
{
    OflReadSettings();
    InitializeCriticalSection(&g_ofl.lock);
    if (!g_ofl.createFileA || !g_ofl.getCommandLineA) { Log("offline lobby: C1's file/command-line imports were not found"); return FALSE; }
    /* A launcher Local Play session (the SDK pipe protocols 1-3, the production host; their code was removed
     * from this client) is a different product: such a client is refused rather than run offline. */
    if (GetEnvironmentVariableA("CODOL_P2P_PRODUCTION_PIPE", NULL, 0) || GetEnvironmentVariableA("CODOL_P2P_STARTUP_IPC_V1", NULL, 0) ||
        GetEnvironmentVariableA("CODOL_LOCAL_QA_DWAUTH_HANDLE", NULL, 0)) {
        Log("offline lobby: refused, this client was started by a launcher-managed Local Play session (not supported: this client is offline only)");
        return FALSE;
    }
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) { Log("offline lobby: no sockets"); return FALSE; }
    char dir[MAX_PATH], parent[MAX_PATH];
    OflExeDir(dir);
    /* The profile name: the command line's dw_username, else the ini's. It is the career's folder. */
    if (!OflCommandLineName(((LPSTR(WINAPI *)(void))g_ofl.getCommandLineA)(), g_ofl.player, sizeof(g_ofl.player)))
        lstrcpyA(g_ofl.player, g_ofl.name);
    if (!OflNameOk(g_ofl.player)) g_ofl.player[0] = 0; /* not a folder name: the lobby names the folder by the player id */
    _snprintf(g_ofl.resources, MAX_PATH, "%s\\resources", g_ofl.runtime);
    _snprintf(g_ofl.logPath, MAX_PATH, "%s\\lobby.log", g_ofl.runtime);
    _snprintf(g_ofl.zones, MAX_PATH, "%sclient_shipRetail\\chinese", dir);
    g_ofl.resources[MAX_PATH - 1] = g_ofl.logPath[MAX_PATH - 1] = g_ofl.zones[MAX_PATH - 1] = 0;
    lstrcpynA(parent, g_ofl.runtime, MAX_PATH);
    char *cut = strrchr(parent, '\\');
    if (cut) { *cut = 0; CreateDirectoryA(parent, NULL); } /* <exe dir>\p2p */
    CreateDirectoryA(g_ofl.runtime, NULL);
    CreateDirectoryA(g_ofl.players, NULL);
    g_ofl.log = fopen(g_ofl.logPath, "wb"); /* one session per file; NULL just means no lobby log */

    P2PConfig p2p;
    P2PDefaults(&p2p);
    P2PLoadIni(&p2p); /* its port: the match host's configured UDP port */
    OflChooseHostPort(p2p.port);
    DwOfflineConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.resources = GetFileAttributesA(g_ofl.resources) != INVALID_FILE_ATTRIBUTES ? g_ofl.resources : NULL;
    cfg.profile = g_ofl.players;
    cfg.playerName = g_ofl.player[0] ? g_ofl.player : NULL;
    cfg.legacyStore = GetFileAttributesA(g_ofl.legacy) != INVALID_FILE_ATTRIBUTES ? g_ofl.legacy : NULL;
    cfg.state = g_ofl.runtime;
    cfg.matchProfile = g_ofl.runtime;
    cfg.zoneDir = g_ofl.zones;
    lstrcpynA(g_ofl.game, dir, MAX_PATH);
    if (g_ofl.game[0] && g_ofl.game[strlen(g_ofl.game) - 1] == '\\') g_ofl.game[strlen(g_ofl.game) - 1] = 0;
    cfg.gameDir = g_ofl.game; /* usermaps, the Survival placements and Kernel33's list of hostable Survival maps */
    cfg.port = g_ofl.wantPort;
    cfg.altPort = g_ofl.wantAltPort;
    cfg.hostPort = g_ofl.hostPort;
    cfg.log = OflLog;
    cfg.localRoute = OflLocalRoute;
    cfg.start = OflStart;
    cfg.leave = OflLeave;
    cfg.startGame = OflStartGame;
    cfg.displayName = OflDisplayName; /* player_name.c: the console's `name` for this profile */
    int port = DwOfflineStartServer(&cfg);
    if (!port) {
        if (g_ofl.wantPort) Log("offline lobby: could not listen on 127.0.0.1:%d (k33_offline.ini port; is it in use?)", g_ofl.wantPort);
        else Log("offline lobby: could not get a free loopback port to listen on");
        OfflineHostPortRelease();
        return FALSE;
    }
    if (port != OFL_C1_NATTRAV_PORT && !OflPatchNatTravPort(port)) {
        Log("offline lobby: C1's NAT traversal port constant is not where it was measured; port %d cannot be used", port);
        DwOfflineStopServer();
        OfflineHostPortRelease();
        return FALSE;
    }
    g_ofl.port = port;
    g_ofl.altPort = DwOfflineAltPort();
    g_ofl.active = TRUE;
    Log("offline lobby: ACTIVE on 127.0.0.1:%d%s (stun alt %d), match host udp %d%s; C1 is routed to it in memory, dwauth.ini / stun.ini on disk are not used",
        port, g_ofl.wantPort ? "" : " (free port)", g_ofl.altPort, g_ofl.hostPort,
        g_ofl.wantHostPort ? " (k33_offline.ini)" : g_ofl.hostPort != p2p.port ? " (free port: the configured one is in use)" : "");
    Log("offline lobby: profile \"%s\" in %s\\%s; run-time files in %s", g_ofl.player[0] ? g_ofl.player : "(named by player id)", g_ofl.players,
        g_ofl.player[0] ? g_ofl.player : "<player id>", g_ofl.runtime);
    if (g_ofl.keepGates) Log("offline lobby: level gates kept (k33_offline.ini level_gates = 1)");
    else if (OflLiftGates()) Log("offline lobby: level gates on playing lifted (rooms, Create Room, playlists, co-op and zombies rooms, maps); the career and loadout unlocks are untouched");
    else Log("offline lobby: C1's notification lock is not where it was measured; its level gates stay");
    return TRUE;
}

/* P2PLoadIni: the embedded match host shares its state and profile folders with the offline lobby,
 * whatever p2p_host.ini says (an install that also plays online points them at the real lobby), and
 * serves on the port chosen for this run. */
static void OfflineP2PConfig(P2PConfig *c)
{
    if (!g_ofl.active) return;
    lstrcpynA(c->state, g_ofl.runtime, MAX_PATH);
    lstrcpynA(c->profile, g_ofl.runtime, MAX_PATH);
    if (g_ofl.hostPort) c->port = g_ofl.hostPort;
    c->rconFile[0] = 0;   /* <state>\native_server_rcon.txt, created on first use */
    lstrcpyA(c->netIp, "127.0.0.1");
    lstrcpyA(c->mode, "inprocess");
    c->admission = 1;
    /* P2PCheck permits exactly one extra string (the two match-lifecycle switches); anything else an
     * older p2p_host.ini carries would refuse hosting, so it is dropped rather than passed on. */
    if (strcmp(c->extra, "+set codol_exitToLobby 1 +set codol_oneShotMatch 1")) c->extra[0] = 0;
}

#else
static BOOL OfflineLobbyActive(void) { return FALSE; }
#endif

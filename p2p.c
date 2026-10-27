/*
 * p2p.c - player-hosted matches (branch feat/p2p-hosting, stage 1 of docs/P2P_HOSTING.md).
 *
 * The host's client runs the sh1x match server (codol_dedrun.exe, built from CoD4x_Server) as its own
 * child process: Kernel33 starts it, watches it over loopback rcon (sh1x_status) and takes it down with
 * the client. The server keeps its own address space, heap and IW3 globals; nothing of it is loaded into
 * C1 (see the design doc for why the in-process options wait).
 *
 *   p2phost [map] [gametype] [zonetype]   start hosting (defaults from p2p\p2p_host.ini)
 *   p2pstop                               rcon quit, then end the server's job
 *   p2pstatus                             state, address, pid, ready time, last sh1x_status line
 *   +set k33_p2pHost 1                    on the command line: start hosting while the client loads
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
    char map[64], gametype[32], zoneType[8], sessionMap[64];
    char netIp[64], botDifficulty[16], extra[512];
    char mode[16];               /* inprocess (default): sh1x_host.dll on the game thread; child: the exe */
    char dll[MAX_PATH];          /* the server as a DLL (mode inprocess) */
    int port, bots, admission, developer, readyTimeout;
} P2PConfig;

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
static char g_p2pArgMap[64], g_p2pArgGametype[32], g_p2pArgZone[8];
struct P2PConfig;
static int P2PInprocRun(const struct P2PConfig *c, const char *password, const char *args); /* p2p_inproc.c */

static void P2PSetState(int state, const char *error)
{
    EnterCriticalSection(&g_p2pLock);
    g_p2p.state = state;
    if (error) lstrcpynA(g_p2p.error, error, sizeof(g_p2p.error));
    LeaveCriticalSection(&g_p2pLock);
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
}

static void P2PLoadIni(P2PConfig *c)
{
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%s\\p2p_host.ini", c->dir);
    path[MAX_PATH - 1] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) { Log("p2p: no %s, defaults used", path); return; }
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
            { "developer", &c->developer }, { "ready_timeout", &c->readyTimeout },
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
}

/* -> NULL when usable, else what is wrong. */
static const char *P2PCheck(const P2PConfig *c)
{
    if (c->port < 1024 || c->port > 65535) return "port must be 1024..65535";
    if (!P2PIdent(c->map, 63) || !P2PIdent(c->sessionMap[0] ? c->sessionMap : c->map, 63)) return "bad map name";
    if (!P2PIdent(c->gametype, 31)) return "bad gametype";
    if (strcmp(c->zoneType, "mp") && strcmp(c->zoneType, "coop") && strcmp(c->zoneType, "zm")) return "zone type must be mp, coop or zm";
    if (!P2PIdent(c->botDifficulty, 15)) return "bad bot difficulty";
    for (const char *p = c->netIp; *p; p++) if (!((*p >= '0' && *p <= '9') || *p == '.')) return "net_ip must be an IPv4 address";
    if (!P2PPathOk(c->exe) || !P2PPathOk(c->basepath) || !P2PPathOk(c->home) || !P2PPathOk(c->scripts) ||
        !P2PPathOk(c->state) || !P2PPathOk(c->profile) || !P2PPathOk(c->rconFile) || !P2PPathOk(c->rollout))
        return "a path holds a quote";
    BOOL inproc = !_stricmp(c->mode, "inprocess");
    if (!inproc && _stricmp(c->mode, "child")) return "mode must be inprocess or child";
    if (!P2PPathOk(c->dll)) return "a path holds a quote";
    if (inproc && GetFileAttributesA(c->dll) == INVALID_FILE_ATTRIBUTES) return "server DLL not found (dll=)";
    if (!inproc && GetFileAttributesA(c->exe) == INVALID_FILE_ATTRIBUTES) return "server executable not found (exe=)";
    char probe[MAX_PATH + 64];
    _snprintf(probe, sizeof(probe), "%s\\maps\\mp\\gametypes\\_callbacksetup.gsc", c->scripts);
    probe[sizeof(probe) - 1] = 0;
    if (GetFileAttributesA(probe) == INVALID_FILE_ATTRIBUTES) return "GSC root not found (scripts=)";
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

/* One rcon command over loopback; the reply text (all datagrams within timeoutMs) into out. */
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
    if (n > 0 && sendto(s, pkt, n, 0, (struct sockaddr *)&a, sizeof(a)) == n && out && outSize > 1) {
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
    _snprintf(path, MAX_PATH, "%s\\p2p_host.txt", c->state);
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
    P2PConfig c;
    P2PDefaults(&c);
    P2PLoadIni(&c);
    if (g_p2pArgMap[0]) { lstrcpyA(c.map, g_p2pArgMap); c.sessionMap[0] = 0; }
    if (g_p2pArgGametype[0]) lstrcpyA(c.gametype, g_p2pArgGametype);
    if (g_p2pArgZone[0]) lstrcpyA(c.zoneType, g_p2pArgZone);
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
    if (!P2PRconPassword(c.rconFile, password)) {
        Log("p2p: not hosting: no rcon password (%s)", c.rconFile);
        P2PSetState(P2P_FAILED, "no rcon password");
        WSACleanup();
        return 1;
    }

    static char cmd[8192];
    BOOL inproc = !_stricmp(c.mode, "inprocess");
    _snprintf(cmd, sizeof(cmd),
        "\"%s\" +set dedicated 2 +set fs_basepath \"%s\" +set fs_homepath \"%s\" +set net_ip %s +set net_port %d "
        "+set rcon_password \"%s\" +set logfile 2 +set logrcon 1 +set developer %d +set sv_fps 60 "
        "+set codol_scriptPath \"%s\" +set codol_scriptOverlay \"%s\" +set codol_scriptNativeOverlay \"%s\" "
        "+set codol_securityKeyPath \"%s\\securitykey.bin\" +set codol_admissionPath \"%s\\game_admissions.txt\" "
        "+set codol_admissionRequired %d +set codol_profilePath \"%s\" +set codol_zoneType %s +set codol_sessionMap %s "
        "+set g_gametype %s +set scr_codol_bots %d +set sv_botDifficulty %s +set codol_hostParentPid %lu "
        "+codol_scriptPath +codol_scriptOverlay +codol_scriptNativeOverlay +map %s %s",
        c.exe, c.basepath, c.home, c.netIp, c.port, password, c.developer, c.scripts, c.scripts, c.scripts,
        c.state, c.state, c.admission ? 1 : 0, c.profile, c.zoneType, c.sessionMap[0] ? c.sessionMap : c.map,
        c.gametype, c.bots, c.botDifficulty, inproc ? 0ul : GetCurrentProcessId(), c.map, c.extra);
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
        if (P2PRcon(c.port, password, "sh1x_status", reply, sizeof(reply), 400) > 0 && P2PStatusLine(reply, line, sizeof(line))) {
            lastAnswer = now;
            EnterCriticalSection(&g_p2pLock);
            lstrcpynA(g_p2p.status, line, sizeof(g_p2p.status));
            if (!ready) g_p2p.readyMs = now - t0;
            if (!ready) g_p2p.state = P2P_READY;
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
static BOOL P2PStart(const char *map, const char *gametype, const char *zone)
{
    if (g_p2pThread) {
        if (WaitForSingleObject(g_p2pThread, 0) != WAIT_OBJECT_0) return FALSE;
        CloseHandle(g_p2pThread);
        g_p2pThread = NULL;
    }
    lstrcpynA(g_p2pArgMap, map ? map : "", sizeof(g_p2pArgMap));
    lstrcpynA(g_p2pArgGametype, gametype ? gametype : "", sizeof(g_p2pArgGametype));
    lstrcpynA(g_p2pArgZone, zone ? zone : "", sizeof(g_p2pArgZone));
    InterlockedExchange(&g_p2pStop, 0);
    P2PSetState(P2P_STARTING, "");
    g_p2pThread = CreateThread(NULL, 0, P2PHostThread, NULL, 0, NULL);
    if (!g_p2pThread) { P2PSetState(P2P_FAILED, "no host thread"); return FALSE; }
    return TRUE;
}

/* ---- commands ------------------------------------------------------------------------------ */

static void __cdecl P2PCmdHost(void)
{
    const char *map = VoiceArgc() > 1 ? VoiceArgv(1) : "";
    const char *gt = VoiceArgc() > 2 ? VoiceArgv(2) : "";
    const char *zone = VoiceArgc() > 3 ? VoiceArgv(3) : "";
    if ((map[0] && !P2PIdent(map, 63)) || (gt[0] && !P2PIdent(gt, 31))) { C1Printf("p2phost: bad map or gametype\n"); return; }
    if (P2PStart(map, gt, zone)) C1Printf("p2phost: starting the match server (p2pstatus shows when it is ready)\n");
    else C1Printf("p2phost: already hosting; p2pstop first\n");
}

static void __cdecl P2PCmdStop(void)
{
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
    { NULL, "p2phost", (void *)P2PCmdHost },
    { NULL, "p2pstop", (void *)P2PCmdStop },
    { NULL, "p2pstatus", (void *)P2PCmdStatus },
};

/* TakeOver, before the game entry: link the commands (the list head is plain data, as voice.c notes) and
 * start hosting at once with +set k33_p2pHost 1, so the server loads its map while the client starts. */
static void InstallP2PHost(void)
{
    if (!g_p2pInit) { InitializeCriticalSection(&g_p2pLock); g_p2pInit = TRUE; }
    C1GameInit();
    int added = 0;
    for (unsigned i = 0; i < sizeof(g_p2pCmds) / sizeof(g_p2pCmds[0]); i++) added += C1AddCommand(&g_p2pCmds[i]);
    const char *cl = GetCommandLineA();
    BOOL autostart = cl && (strstr(cl, "k33_p2pHost 1") || strstr(cl, "k33_p2pHost \"1\""));
    Log("p2p: %d commands (p2phost, p2pstop, p2pstatus)%s", added, autostart ? "; hosting at startup (k33_p2pHost 1)" : "");
    if (autostart) P2PStart(NULL, NULL, NULL);
}

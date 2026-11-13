/*
 * console_server.c - the console's route to the match server this client hosts (included by console.c;
 * docs/OFFLINE_CONSOLE_20261112.md).
 *
 * This fork is offline only: the one server a console command can reach is the sh1x server p2p.c / p2p_inproc.c run inside
 * this client (p2p_inproc.c) on 127.0.0.1. The route is that server's own rcon over loopback, the channel
 * p2p.c already polls sh1x_status on: one datagram `rcon "<password>" <command>`, the reply is what the
 * command printed (the server redirects its Com_Printf into the reply, SVC_RemoteCommand in sv_main.c).
 *
 *   why rcon and not Sh1xHost_Command   the host DLL's Sh1xHost_Command is Cbuf_AddText: the command runs in a
 *                                       later server frame and nothing returns what it printed. rcon runs the
 *                                       same command with console power (invoker 100) and returns the output.
 *                                       Nothing here calls into sh1x_host.dll or keeps a pointer into it, so a
 *                                       host that is torn down and started again needs no care on this side.
 *   target                              P2PConsoleHost (p2p_inproc.c): the port and rcon file of the host session that
 *                                       is starting or ready right now, asked again for every command. No
 *                                       session: "no match running". Never another address, never a port or
 *                                       password from a dvar or a file the player edits.
 *   lists                               a running host is asked for `cmdlist` and `cvarlist` once per session
 *                                       (and again after a command went to it, for the dvar values); before
 *                                       that, and when it does not answer, sh1x_cmds.h (generated from the
 *                                       pinned server source) is the list.
 *
 * Threads: a command or a list fetch waits for the server, which runs on the game thread, so each is a short
 * worker thread (the window thread that owns the console is the game thread). g_svLock guards the lists; it
 * is taken after g_conUiLock and before g_conLock, and never held across a socket call.
 * Measured vs inferred: see the doc; the reply formats parsed here are read from cmd.c / cvar.c of the
 * pinned server source and were checked against a running host (docs, "Runtime check").
 */

#include "sh1x_cmds.h"

static int P2PConsoleHost(int *port, char *rconFile, DWORD *session); /* p2p_inproc.c: 1 while this client hosts */

typedef struct { const char *name, *value, *desc; } ConSvName;

#define CON_SV_REPLY (1024 * 1024) /* measured on the pinned server: cmdlist 2 KB in 2 datagrams, cvarlist 19 KB in 20 */

static CRITICAL_SECTION g_svLock;
static BOOL g_svReady;
static ConSvName g_svTableCmds[SH1X_CMD_COUNT], g_svTableDvars[SH1X_DVAR_COUNT];
static struct {
    const ConSvName *cmds, *dvars;   /* the table, or what the running host answered */
    int cmdCount, dvarCount;
    ConSvName *liveCmds, *liveDvars; /* owned; NULL while the table is in use */
    char *blobCmds, *blobDvars;      /* the two replies the live names point into */
    DWORD session;                   /* the host session the live lists came from (0: the table) */
    BOOL stale;                      /* a command went to the server since: fetch the values again */
    DWORD cheatHintSession;          /* the session the "cheats are off" hint was printed for */
} g_sv;
static volatile LONG g_svFetching;

static void ConSvUseTable(void)
{
    free(g_sv.liveCmds); free(g_sv.liveDvars); free(g_sv.blobCmds); free(g_sv.blobDvars);
    g_sv.liveCmds = g_sv.liveDvars = NULL;
    g_sv.blobCmds = g_sv.blobDvars = NULL;
    g_sv.cmds = g_svTableCmds;
    g_sv.cmdCount = (int)SH1X_CMD_COUNT;
    g_sv.dvars = g_svTableDvars;
    g_sv.dvarCount = (int)SH1X_DVAR_COUNT;
    g_sv.session = 0;
    g_sv.stale = FALSE;
}

static void ConSvInit(void)
{
    InitializeCriticalSection(&g_svLock);
    for (unsigned i = 0; i < SH1X_CMD_COUNT; i++) {
        g_svTableCmds[i].name = kSh1xCmds[i].name;
        g_svTableCmds[i].desc = kSh1xCmds[i].desc;
    }
    for (unsigned i = 0; i < SH1X_DVAR_COUNT; i++) {
        g_svTableDvars[i].name = kSh1xDvars[i].name;
        g_svTableDvars[i].desc = kSh1xDvars[i].desc;
    }
    ConSvUseTable();
    g_svReady = TRUE;
}

/* The host session that is alive right now: its port and password. FALSE: this client hosts nothing. */
static BOOL ConSvHost(int *port, char *password, int passwordSize, DWORD *session)
{
    char file[MAX_PATH];
    DWORD s = 0;
    if (!g_svReady || !P2PConsoleHost(port, file, &s) || *port <= 0 || *port > 65535) return FALSE;
    if (session) *session = s;
    return ConReadPassword(file, password, passwordSize);
}

static const ConSvName *ConSvFindIn(const ConSvName *list, int count, const char *name)
{
    for (int i = 0; i < count; i++)
        if (!_stricmp(list[i].name, name)) return &list[i];
    return NULL;
}

/* g_svLock held by the caller for as long as it uses the result. */
static const ConSvName *ConSvFindCmd(const char *name) { return ConSvFindIn(g_sv.cmds, g_sv.cmdCount, name); }
static const ConSvName *ConSvFindDvar(const char *name) { return ConSvFindIn(g_sv.dvars, g_sv.dvarCount, name); }

/* 0 neither, 1 a server command, 2 a server dvar. Takes g_svLock itself. */
static int ConSvKind(const char *name)
{
    if (!g_svReady || !name[0]) return 0;
    EnterCriticalSection(&g_svLock);
    int kind = ConSvFindCmd(name) ? 1 : ConSvFindDvar(name) ? 2 : 0;
    LeaveCriticalSection(&g_svLock);
    return kind;
}

static const char *ConSvDesc(const char *name)
{
    for (unsigned i = 0; i < SH1X_DVAR_COUNT; i++)
        if (!_stricmp(kSh1xDvars[i].name, name)) return kSh1xDvars[i].desc;
    return "";
}

static BOOL ConSvNameChars(const char *s)
{
    if (!*s) return FALSE;
    for (; *s; s++)
        if (!isalnum((BYTE)*s) && *s != '_' && *s != '.' && *s != '+' && *s != '-') return FALSE;
    return TRUE;
}

/* `cmdlist` (cmd.c Cmd_List_f): one name per line, then "<n> commands". The reply is cut up in place. */
static int ConSvParseCmds(char *reply, ConSvName *out, int max)
{
    int n = 0;
    for (char *line = strtok(reply, "\r\n"); line && n < max; line = strtok(NULL, "\r\n")) {
        while (*line == ' ') line++;
        if (!ConSvNameChars(line)) continue; /* the "<n> commands" line has a space */
        out[n].name = line;
        out[n].value = NULL;
        out[n].desc = "";
        n++;
    }
    return n;
}

/* `cvarlist` (cvar.c Cvar_List_f): seven flag columns (S U R I A L C or blanks), a space, the name, a space and
 * the value in quotes. Lines of any other shape (the banner, the totals) are skipped. */
static int ConSvParseDvars(char *reply, ConSvName *out, int max)
{
    int n = 0;
    for (char *line = strtok(reply, "\r\n"); line && n < max; line = strtok(NULL, "\r\n")) {
        if (strlen(line) < 11 || line[7] != ' ') continue;
        char *name = line + 8, *sp = strchr(name, ' ');
        if (!sp || sp[1] != '"') continue;
        char *end = strrchr(sp + 2, '"');
        if (!end) continue;
        *sp = 0;
        *end = 0;
        if (!ConSvNameChars(name)) continue;
        out[n].name = name;
        out[n].value = sp + 2;
        out[n].desc = ConSvDesc(name);
        n++;
    }
    return n;
}

typedef struct { int port; DWORD session; char password[128]; } ConSvFetchJob;

static DWORD WINAPI ConSvFetchThread(LPVOID param)
{
    ConSvFetchJob *job = (ConSvFetchJob *)param;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    char *cmds = (char *)malloc(CON_SV_REPLY), *dvars = (char *)malloc(CON_SV_REPLY);
    ConSvName *cl = (ConSvName *)calloc(2048, sizeof(ConSvName)), *dl = (ConSvName *)calloc(8192, sizeof(ConSvName));
    int nc = 0, nd = 0;
    if (cmds && dvars && cl && dl) {
        /* The server answers nothing while it loads its map (one long frame): try for a while, as long as the
         * same host session is the live one. */
        for (int attempt = 0; attempt < 90 && !nc; attempt++) {
            int port;
            char pw[128];
            DWORD session = 0;
            if (!ConSvHost(&port, pw, sizeof(pw), &session) || session != job->session) break;
            if (ConRconOnce(job->port, job->password, "cmdlist", cmds, CON_SV_REPLY, 1000, 300) > 0 &&
                !strstr(cmds, "Bad rcon"))
                nc = ConSvParseCmds(cmds, cl, 2048);
            else
                Sleep(1000);
        }
        if (nc && ConRconOnce(job->port, job->password, "cvarlist", dvars, CON_SV_REPLY, 3000, 300) > 0)
            nd = ConSvParseDvars(dvars, dl, 8192);
    }
    BOOL used = FALSE;
    if (nc && nd) {
        int port;
        char pw[128];
        DWORD session = 0;
        BOOL alive = ConSvHost(&port, pw, sizeof(pw), &session) && session == job->session;
        EnterCriticalSection(&g_svLock);
        if (alive) {
            ConSvUseTable(); /* frees the lists of the session before */
            g_sv.liveCmds = cl; g_sv.liveDvars = dl; g_sv.blobCmds = cmds; g_sv.blobDvars = dvars;
            g_sv.cmds = cl; g_sv.cmdCount = nc; g_sv.dvars = dl; g_sv.dvarCount = nd;
            g_sv.session = job->session;
            used = TRUE;
        }
        LeaveCriticalSection(&g_svLock);
    }
    if (used)
        Log("console: server lists from the running host (session %lu): %d commands, %d dvars (the generated table has %u and %u)",
            (unsigned long)job->session, nc, nd, (unsigned)SH1X_CMD_COUNT, (unsigned)SH1X_DVAR_COUNT);
    else {
        Log("console: the host did not answer cmdlist/cvarlist (session %lu); completion keeps the generated table",
            (unsigned long)job->session);
        free(cmds); free(dvars); free(cl); free(dl);
    }
    free(job);
    InterlockedExchange(&g_svFetching, 0);
    InterlockedExchange(&g_conDirty, 1);
    return 0;
}

/* Window thread (console opened, a key typed): bring the lists in line with the host session. Cheap when
 * nothing changed; never waits. */
static void ConSvRefresh(void)
{
    if (!g_svReady) return;
    ConSvFetchJob job;
    BOOL hosting = ConSvHost(&job.port, job.password, sizeof(job.password), &job.session);
    EnterCriticalSection(&g_svLock);
    BOOL want = hosting && (g_sv.session != job.session || g_sv.stale);
    if (!hosting && g_sv.session) ConSvUseTable(); /* the match is over: back to the table */
    if (want) g_sv.stale = FALSE;
    LeaveCriticalSection(&g_svLock);
    if (!want || InterlockedCompareExchange(&g_svFetching, 1, 0)) {
        if (want) { EnterCriticalSection(&g_svLock); g_sv.stale = TRUE; LeaveCriticalSection(&g_svLock); }
        return;
    }
    ConSvFetchJob *copy = (ConSvFetchJob *)malloc(sizeof(job));
    HANDLE t = NULL;
    if (copy) {
        *copy = job;
        t = CreateThread(NULL, 0, ConSvFetchThread, copy, 0, NULL);
    }
    if (t) CloseHandle(t);
    else { free(copy); InterlockedExchange(&g_svFetching, 0); }
}

/* ---- running a command on the server ---- */

typedef struct { int port; DWORD session, delay; char password[128]; char command[CON_INPUT_LEN]; BOOL devServer; } ConSvExecJob;

static DWORD WINAPI ConSvExecThread(LPVOID param)
{
    ConSvExecJob *job = (ConSvExecJob *)param;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    if (job->delay) Sleep(job->delay);
    if (job->devServer) {
        /* See ConSvCheatsOn. One rcon datagram runs one command (SVC_RemoteCommand -> Cmd_ExecuteSingleCommand),
         * so the switch goes first, on its own. Its reply ("codol_devServer" is set silently) is not shown. */
        char ack[512];
        ConRconOnce(job->port, job->password, "codol_devServer 1", ack, sizeof(ack), 1500, 100);
    }
    char *reply = (char *)malloc(CON_SV_REPLY);
    int got = reply ? ConRconOnce(job->port, job->password, job->command, reply, CON_SV_REPLY, 4000, 250) : -1;
    /* the reply arrives after ConExecute's result window closed: open it again for these lines */
    EnterCriticalSection(&g_conLock);
    g_resUntil = GetTickCount() + 500;
    if (!g_resUntil) g_resUntil = 1;
    LeaveCriticalSection(&g_conLock);
    if (got <= 0)
        ConAppend("sv: no reply from the match server (it does not answer while it loads a map; try again)\n");
    else if (strstr(reply, "Bad rcon"))
        ConAppend("sv: the match server refused the command (its rcon password changed?)\n");
    else {
        ConAppend(reply);
        if (reply[strlen(reply) - 1] != '\n') ConAppend("\n");
    }
    free(reply);
    EnterCriticalSection(&g_svLock);
    g_sv.stale = TRUE; /* a dvar may have changed */
    LeaveCriticalSection(&g_svLock);
    free(job);
    InterlockedExchange(&g_conDirty, 1);
    return 0;
}

/* Runs one server console line on the hosted server, delayMs from now; its output comes back into the
 * console. FALSE: this client hosts nothing right now (nothing was or will be sent). */
static BOOL ConSvExecDelayed(const char *command, DWORD delayMs)
{
    ConSvExecJob *job = (ConSvExecJob *)calloc(1, sizeof(*job));
    if (!job) return FALSE;
    if (!ConSvHost(&job->port, job->password, sizeof(job->password), &job->session)) { free(job); return FALSE; }
    job->delay = delayMs;
    int n = 0;
    for (const char *p = command; *p && n < (int)sizeof(job->command) - 1; p++)
        if ((BYTE)*p >= 0x20) job->command[n++] = *p; /* one line: no control characters into the datagram */
    job->command[n] = 0;
    HANDLE t = CreateThread(NULL, 0, ConSvExecThread, job, 0, NULL);
    if (t) CloseHandle(t);
    else free(job);
    return TRUE;
}

static BOOL ConSvExec(const char *command) { return ConSvExecDelayed(command, 0); }

/* "codol_dev_cheats on" typed in the console of the player who hosts this offline match.
 * The server refuses every cheat on a "lobby match server" (codol_admissionRequired 1, which hosting requires)
 * unless its operator declares a developer server (dvar codol_devServer, registered with flags 0 in
 * codol_dev_cheats.c, so the operator may set it at run time). The host's console speaks to the server with the
 * operator's rcon password, and in this offline client the server exists only for this one player on loopback:
 * so the console declares it right before switching cheats on, never otherwise. sv_cheats itself stays the
 * server's: codol_dev_cheats on sets it, and from then on that server records no progression (its own latch). */
static BOOL ConSvCheatsOn(const char *line)
{
    ConSvExecJob *job = (ConSvExecJob *)calloc(1, sizeof(*job));
    if (!job) return FALSE;
    if (!ConSvHost(&job->port, job->password, sizeof(job->password), &job->session)) { free(job); return FALSE; }
    lstrcpynA(job->command, line, sizeof(job->command));
    job->devServer = TRUE;
    HANDLE t = CreateThread(NULL, 0, ConSvExecThread, job, 0, NULL);
    if (t) CloseHandle(t);
    else free(job);
    return TRUE;
}

/* A cheat command went to the server as this player's client command. The server answers the player with a
 * game message (CODOL_DevPrint: server command 'e'), and C1 neither prints that to its log nor does this
 * client's HUD show it (measured: "viewpos", "give ammo" and a refusal left no line in console_mp.log or on
 * screen). So the console asks the server what the command left behind, half a second later, when the
 * command has run: dev_cheats (whether cheats are permitted and, per player, health, pm_type, the
 * god/demigod/notarget/noclip/ufo flags, the position and the weapon), or for devset the dvar it wrote. */
static void ConSvCheatEcho(const char *verb, const char *arg)
{
    if (!_stricmp(verb, "devset") && arg && ConSvNameChars(arg)) ConSvExecDelayed(arg, 500);
    else ConSvExecDelayed("dev_cheats", 500);
}

/* A cheat command is about to go to the server as this player's client command. The server answers a refusal
 * itself ("Cheats refused: ..."), with a hint that names devmap, which this client never uses: say once per
 * match how cheats are switched on here. */
static void ConSvCheatHint(void)
{
    int port;
    char pw[128];
    DWORD session = 0;
    if (!ConSvHost(&port, pw, sizeof(pw), &session)) return;
    BOOL say = FALSE, on = FALSE;
    EnterCriticalSection(&g_svLock);
    const ConSvName *c = g_sv.session == session ? ConSvFindDvar("sv_cheats") : NULL;
    on = c && c->value && atoi(c->value) != 0;
    if (!on && g_sv.cheatHintSession != session) { g_sv.cheatHintSession = session; say = TRUE; }
    LeaveCriticalSection(&g_svLock);
    if (say)
        ConPrintf("  ^3cheats are off in a new match: type ^7codol_dev_cheats on^3 first (that match then records no progression)\n");
}

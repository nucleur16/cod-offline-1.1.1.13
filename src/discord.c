/*
 * Discord Rich Presence for C1. Included by kernel33.c after voice.c and controller.c, so it shares
 * Log/Readable/WriteCode/CallsTarget/RedirectCall, c1game.c's dvar helpers, voice.c's connection-state
 * and clc addresses and parity.c's configstring addresses.
 *
 * This is CoDOLPresence.exe (the PyInstaller build of the server repo's tools/codol_discord_presence.py
 * that the codol-classic launcher starts beside the client with --client-pid) moved into the client:
 *
 *   application  1554730441825718272 ("Call of Duty Online"); its one art asset is "codol".
 *   lobby        details "In the lobby", state "Choosing a match", started when the client started.
 *   match        details "<mode> on <map>" (the tables below; unknown maps are prettified, unknown
 *                modes upper-cased), started when the client reached CA_ACTIVE. State: on a server
 *                at 127.0.0.1 whose getinfo reply agrees on map and mode and counts a human,
 *                "N player(s)[, M bots]"; otherwise "In a match" (the helper's remote-server state).
 *                With no map or mode known: "In a match" / "Playing".
 *   assets       large_image "codol", large_text "Call of Duty Online", in both.
 *   pid          this process, so Discord drops the activity by itself when the client exits.
 *
 * Where the helper read the game, the client now reads itself:
 *   connected    connection state [0x16FC2E8] == 9 (CA_ACTIVE) instead of console_mp.log's
 *                "CA_x -> CA_ACTIVE" lines.
 *   map, mode    configstring 0 (the server's serverinfo: mapname, g_gametype; gameState offsets at
 *                0x6D0D130, strings at 0x6D1370C) instead of the log's StartGameRecv "map:"/"type:".
 *   server       clc.serverAddress (clc = [0x16F8068] ^ 0x26F950CC, netadr at +0x14: type, ip +4,
 *                port +8 in network order) instead of the log's clConn[n|host:port].
 *   players      a plain "getinfo" (g_humanplayers, clients) to a 127.0.0.1 server instead of
 *                "rcon <password> sh1x_status", so no RCON password file is needed. The server only
 *                answers plaintext connectionless packets from loopback (NET_UDPPacketEvent); a
 *                remote server's traffic is bdDTLS only, which is why the helper had no counts there.
 *
 * Threads: the main thread (Com_EventLoop pass, the call at 0x595E9D, chained after voice.c's hook)
 * registers the dvar, starts the worker and every 250 ms copies the game state above under a lock;
 * nothing there waits. The worker owns the pipe and the socket: it connects to
 * \\.\pipe\discord-ipc-0..9 (overlapped, every I/O bounded by a timeout, so a hung Discord only
 * stalls this thread), handshakes, sends SET_ACTIVITY when the activity changes, answers PING and
 * reads replies. No Discord: one connect attempt every 5 s, a CreateFile that fails at once. Discord
 * closing or restarting: the pipe breaks, the worker drops it and reconnects, then sends again.
 *
 * Wire format (Discord IPC): u32 opcode, u32 length (little endian), then that much JSON. Opcodes
 * 0 HANDSHAKE {"v":1,"client_id":...}, 1 FRAME (commands, replies, the READY dispatch), 2 CLOSE,
 * 3 PING (answered with 4 PONG carrying the same body). Frames are capped at 16 KiB as in the helper.
 *
 * Dvar: k33_discord 1 (archived): 0 clears the activity and closes the pipe. "+set k33_discord 0"
 * on the command line installs nothing. Kernel33.log gets the connection changes and each new
 * activity, like the helper's diagnostics file.
 *
 * Not carried over: the helper's --once dry run, its C1.exe/tasklist lookup (it is the client now)
 * and its exit 10 s after the client. There is no Join button: the helper had none (the launcher's
 * discord_room_api.txt is written for a room watcher that does not exist in it).
 */

#define DISCORD_APP_ID      "1554730441825718272"
#define DISCORD_ICON_KEY    "codol"
#define DISCORD_ICON_TEXT   "Call of Duty Online"
#define DISCORD_MAX_FRAME   (16 * 1024)
#define DISCORD_RETRY_MS    5000
#define DISCORD_IO_MS       3000
#define DISCORD_INFO_MS     10000   /* getinfo cadence on a local server */
#define DISCORD_STATE_MS    250     /* main-thread sampling */
#define DISCORD_CALL_VA     VOICE_EVENTLOOP_CALL_VA
#define DISCORD_CALL_FUNC   VOICE_EVENTLOOP_FUNC_VA
#ifndef DISCORD_PIPE_FMT
#define DISCORD_PIPE_FMT    "\\\\.\\pipe\\discord-ipc-%d"   /* a test build can point it elsewhere */
#endif

enum { DC_HANDSHAKE = 0, DC_FRAME = 1, DC_CLOSE = 2, DC_PING = 3, DC_PONG = 4 };

/* What the main thread saw, copied out under g_dcLock. */
typedef struct DiscordGame {
    BOOL ready;              /* the dvar exists, so its value (archived or not) is known */
    BOOL enabled;
    BOOL active;             /* CA_ACTIVE */
    long long activeSince;   /* unix seconds */
    BOOL local;              /* the server is on this PC (127.x or loopback) */
    unsigned short port;     /* host order */
    char map[64], mode[32];
} DiscordGame;

typedef struct DiscordActivity {
    BOOL present;
    char details[128], state[128];
    long long start;
} DiscordActivity;

static CRITICAL_SECTION g_dcLock;
static DiscordGame g_dcGame;
static const BYTE *g_dvDiscord;
static BOOL g_dcDvarDone;
static long long g_dcLaunched;
static HANDLE g_dcThread;
__attribute__((used)) DWORD g_discordNext;   /* what the hooked call reached before us */

static long long DiscordNow(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (long long)(u.QuadPart / 10000000ULL) - 11644473600LL;
}

/* ---- names ------------------------------------------------------------------------------------------ */

static const char *const kDcMaps[][2] = {
    { "mp_killhouse_sh", "Killhouse" }, { "mp_rust", "Rust" }, { "mp_nuked", "Nuketown" },
    { "zm_prototype_sh", "Prototype" }, { "zm_asylum_sh", "Asylum" }, { "zm_zombie_training", "Zombie Training" },
};
static const char *const kDcModes[][2] = {
    { "combat", "Combat Training" }, { "war", "Team Deathmatch" }, { "dom", "Domination" },
    { "dm", "Free-for-All" }, { "ctf", "Capture the Flag" }, { "sd", "Search and Destroy" },
    { "sab", "Sabotage" }, { "koth", "Headquarters" }, { "oneflag", "One Flag CTF" }, { "arena", "Arena" },
    { "dd", "Demolition" }, { "vip", "VIP" }, { "conf", "Kill Confirmed" }, { "infect", "Infected" },
    { "hp", "Hardpoint" }, { "zm", "Zombies" }, { "coop", "Co-op" },
};

/* The helper showed an unknown map's raw name; "mp_abandon_sh" reads better as "Abandon". */
static void DiscordMapName(const char *map, char *out, int n)
{
    for (unsigned i = 0; i < sizeof(kDcMaps) / sizeof(kDcMaps[0]); i++)
        if (!_stricmp(map, kDcMaps[i][0])) { lstrcpynA(out, kDcMaps[i][1], n); return; }
    const char *s = map;
    if (!_strnicmp(s, "mp_", 3) || !_strnicmp(s, "zm_", 3) || !_strnicmp(s, "co_", 3)) s += 3;
    int len = (int)strlen(s);
    if (len > 3 && !_stricmp(s + len - 3, "_sh")) len -= 3;
    int o = 0;
    BOOL up = TRUE;
    for (int i = 0; i < len && o < n - 1; i++) {
        char c = s[i];
        if (c == '_') { c = ' '; up = TRUE; }
        else if (up) { c = (char)toupper((unsigned char)c); up = FALSE; }
        out[o++] = c;
    }
    out[o] = 0;
    if (!o) lstrcpynA(out, map, n);
}

static void DiscordModeName(const char *mode, char *out, int n)
{
    for (unsigned i = 0; i < sizeof(kDcModes) / sizeof(kDcModes[0]); i++)
        if (!_stricmp(mode, kDcModes[i][0])) { lstrcpynA(out, kDcModes[i][1], n); return; }
    int o = 0;
    for (; mode[o] && o < n - 1; o++) out[o] = (char)toupper((unsigned char)mode[o]);
    out[o] = 0;
}

/* \key\value\key\value... -> value of key, "" when absent. */
static void DiscordInfoValue(const char *info, const char *key, char *out, int n)
{
    out[0] = 0;
    int klen = (int)strlen(key);
    const char *p = info;
    while (p && *p == '\\') {
        const char *k = p + 1, *ke = strchr(k, '\\');
        if (!ke) return;
        const char *v = ke + 1, *ve = strchr(v, '\\');
        if (!ve) ve = v + strlen(v);
        if (ke - k == klen && !_strnicmp(k, key, klen)) {
            int len = (int)(ve - v);
            if (len > n - 1) len = n - 1;
            memcpy(out, v, len);
            out[len] = 0;
            /* the getinfo reply ends in a newline on some servers */
            while (len && (out[len - 1] == '\n' || out[len - 1] == '\r')) out[--len] = 0;
            return;
        }
        p = ve;
    }
}

/* ---- main thread -------------------------------------------------------------------------------------- */

/* Configstring 0 (serverinfo) once a gameState exists; NULL before. */
static const char *DiscordServerInfo(void)
{
    const DWORD *offsets = (const DWORD *)(DWORD_PTR)CS_OFFSETS_VA;
    if (!Readable(offsets, 4)) return NULL;
    DWORD off = offsets[0];
    if (!off || off >= CS_DATA_LIMIT - 1024) return NULL;
    const char *s = (const char *)(DWORD_PTR)(CS_DATA_VA + off);
    return Readable(s, 1024) && s[0] == '\\' ? s : NULL;
}

static DWORD WINAPI DiscordThread(LPVOID unused);

__attribute__((used)) void __cdecl DiscordMainTick(void)
{
    static DWORD last;
    static BOOL wasActive;
    static long long activeSince;
    if (!g_dcDvarDone) {
        if (!C1DvarsReady()) return;
        g_dcDvarDone = TRUE;
        g_dvDiscord = C1RegisterInt("k33_discord", 1, 0, 1, 0x1,
                                    "Discord Rich Presence: 1 show what you are playing in Discord, 0 off");
        Log("discord: k33_discord %d", C1DvarInt(g_dvDiscord, 1));
        g_dcThread = CreateThread(NULL, 0, DiscordThread, NULL, 0, NULL);
        if (g_dcThread) SetThreadPriority(g_dcThread, THREAD_PRIORITY_BELOW_NORMAL);
        else Log("discord: worker thread could not start (%lu)", GetLastError());
    }
    DWORD now = GetTickCount();
    if (now - last < DISCORD_STATE_MS) return;
    last = now;

    DiscordGame g;
    memset(&g, 0, sizeof(g));
    g.ready = TRUE;
    g.enabled = C1DvarInt(g_dvDiscord, 1) != 0;
    g.active = *(volatile int *)(DWORD_PTR)VOICE_CONNSTATE_VA == VOICE_CA_ACTIVE;
    if (g.active && !wasActive) activeSince = DiscordNow();
    wasActive = g.active;
    if (g.active) {
        g.activeSince = activeSince;
        const char *info = DiscordServerInfo();
        if (info) {
            DiscordInfoValue(info, "mapname", g.map, sizeof(g.map));
            DiscordInfoValue(info, "g_gametype", g.mode, sizeof(g.mode));
        }
        DWORD enc = *(volatile DWORD *)(DWORD_PTR)VOICE_CLC_PTR_VA;
        const BYTE *adr = enc ? (const BYTE *)(DWORD_PTR)(enc ^ VOICE_CLC_KEY) + 0x14 : NULL;
        int type = -1;
        BYTE ip[4] = { 0, 0, 0, 0 };
        if (adr && Readable(adr, 12)) {
            type = *(const int *)adr;
            memcpy(ip, adr + 4, 4);
            g.port = (unsigned short)(adr[8] << 8 | adr[9]);
            g.local = type == 2 /* NA_LOOPBACK */ || (type > 2 && adr[4] == 127);
        }
#if K33_DIAGNOSTICS
        static char logged[128];
        char line[128];
        _snprintf(line, sizeof(line), "%s %s %lld", g.map, g.mode, activeSince);
        line[sizeof(line) - 1] = 0;
        if (g.map[0] && strcmp(line, logged)) {
            lstrcpynA(logged, line, sizeof(logged));
            Log("discord: match: map %s, gametype %s, server %u.%u.%u.%u:%u (type %d, %s)", g.map, g.mode,
                ip[0], ip[1], ip[2], ip[3], g.port, type,
                g.local ? "local: player count from getinfo" : "remote");
        }
#endif
    }
    EnterCriticalSection(&g_dcLock);
    g_dcGame = g;
    LeaveCriticalSection(&g_dcLock);
}

/* ---- the worker: Discord IPC ------------------------------------------------------------------------ */

static HANDLE g_dcPipe = INVALID_HANDLE_VALUE;
static HANDLE g_dcEvent;

static BOOL DiscordIo(BOOL write, void *buf, DWORD n)
{
    BYTE *p = (BYTE *)buf;
    while (n) {
        OVERLAPPED ov;
        memset(&ov, 0, sizeof(ov));
        ov.hEvent = g_dcEvent;
        ResetEvent(g_dcEvent);
        BOOL ok = write ? WriteFile(g_dcPipe, p, n, NULL, &ov) : ReadFile(g_dcPipe, p, n, NULL, &ov);
        DWORD err = ok ? 0 : GetLastError();
        if (!ok && err != ERROR_IO_PENDING && err != ERROR_MORE_DATA) return FALSE;
        if (err == ERROR_IO_PENDING && WaitForSingleObject(g_dcEvent, DISCORD_IO_MS) != WAIT_OBJECT_0) {
            DWORD got;
            CancelIo(g_dcPipe);
            GetOverlappedResult(g_dcPipe, &ov, &got, TRUE);
            return FALSE;
        }
        DWORD got = 0;
        if (!GetOverlappedResult(g_dcPipe, &ov, &got, FALSE) && GetLastError() != ERROR_MORE_DATA) return FALSE;
        if (!got) return FALSE;
        p += got;
        n -= got;
    }
    return TRUE;
}

static BOOL DiscordWrite(DWORD opcode, const char *json)
{
    static BYTE frame[8 + DISCORD_MAX_FRAME];
    DWORD len = (DWORD)strlen(json);
    if (g_dcPipe == INVALID_HANDLE_VALUE || len > DISCORD_MAX_FRAME) return FALSE;
    memcpy(frame, &opcode, 4);
    memcpy(frame + 4, &len, 4);
    memcpy(frame + 8, json, len);
    return DiscordIo(TRUE, frame, 8 + len);
}

/* -1 broken pipe, else the bytes waiting. */
static long DiscordAvailable(void)
{
    DWORD avail = 0;
    if (!PeekNamedPipe(g_dcPipe, NULL, 0, NULL, &avail, NULL)) return -1;
    return (long)avail;
}

/* Reads one whole frame if it has arrived: 1 read, 0 not yet, -1 broken. body gets a 0 terminator. */
static int DiscordReadFrame(DWORD *opcode, char *body)
{
    BYTE hdr[8];
    DWORD got = 0, avail = 0;
    if (!PeekNamedPipe(g_dcPipe, hdr, 8, &got, &avail, NULL)) return -1;
    if (got < 8) return 0;
    DWORD len;
    memcpy(opcode, hdr, 4);
    memcpy(&len, hdr + 4, 4);
    if (len > DISCORD_MAX_FRAME) return -1;
    if (avail < 8 + len) return 0;
    if (!DiscordIo(FALSE, hdr, 8) || (len && !DiscordIo(FALSE, body, len))) return -1;
    body[len] = 0;
    return 1;
}

/* "key":"value" in a reply, unescaped well enough for a log line. */
static void DiscordJsonString(const char *json, const char *key, char *out, int n)
{
    char pat[48];
    out[0] = 0;
    _snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    pat[sizeof(pat) - 1] = 0;
    const char *p = strstr(json, pat);
    if (!p) {   /* "key": "value" */
        _snprintf(pat, sizeof(pat), "\"%s\": \"", key);
        p = strstr(json, pat);
        if (!p) return;
    }
    p += strlen(pat);
    int o = 0;
    while (*p && *p != '"' && o < n - 1) {
        if (*p == '\\' && p[1]) p++;
        out[o++] = *p++;
    }
    out[o] = 0;
}

static void DiscordClose(void)
{
    if (g_dcPipe != INVALID_HANDLE_VALUE) CloseHandle(g_dcPipe);
    g_dcPipe = INVALID_HANDLE_VALUE;
}

static BOOL DiscordConnect(void)
{
    static char body[DISCORD_MAX_FRAME + 1];
    for (int i = 0; i < 10; i++) {
        char name[32];
        _snprintf(name, sizeof(name), DISCORD_PIPE_FMT, i);
        name[sizeof(name) - 1] = 0;
        g_dcPipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (g_dcPipe == INVALID_HANDLE_VALUE) continue;
        if (!DiscordWrite(DC_HANDSHAKE, "{\"v\":1,\"client_id\":\"" DISCORD_APP_ID "\"}")) { DiscordClose(); continue; }
        DWORD opcode = 0, start = GetTickCount();
        int r = 0;
        while ((r = DiscordReadFrame(&opcode, body)) == 0 && GetTickCount() - start < DISCORD_IO_MS) Sleep(50);
        if (r == 1 && opcode == DC_FRAME && strstr(body, "\"READY\"")) {
            Log("discord: connected (%s)", name + 9);
            return TRUE;
        }
        if (r == 1) {
            char msg[160];
            DiscordJsonString(body, "message", msg, sizeof(msg));
            Log("discord: %s refused the handshake (opcode %lu: %s)", name + 9, (unsigned long)opcode, msg[0] ? msg : "no reason");
        }
        DiscordClose();
    }
    return FALSE;
}

/* ---- the worker: activity ------------------------------------------------------------------------- */

typedef struct DiscordJson { char *p; int n, len; } DiscordJson;

static void DjRaw(DiscordJson *j, const char *s)
{
    while (*s && j->len < j->n - 1) j->p[j->len++] = *s++;
    j->p[j->len] = 0;
}

/* A JSON string. Game text is ASCII here; any other byte (GBK) becomes '?' so the frame stays UTF-8. */
static void DjStr(DiscordJson *j, const char *s)
{
    DjRaw(j, "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        char esc[8];
        if (c == '"' || c == '\\') { esc[0] = '\\'; esc[1] = (char)c; esc[2] = 0; }
        else if (c < 0x20) _snprintf(esc, sizeof(esc), "\\u%04x", c);
        else { esc[0] = c >= 0x80 ? '?' : (char)c; esc[1] = 0; }
        DjRaw(j, esc);
    }
    DjRaw(j, "\"");
}

static BOOL DiscordSetActivity(const DiscordActivity *a)
{
    static char buf[2048];
    static unsigned long nonceCount;
    char tmp[64];
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    DiscordJson j = { buf, sizeof(buf), 0 };
    buf[0] = 0;
    _snprintf(tmp, sizeof(tmp), "%08lx%08lx%08lx%08lx", (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount(),
              ++nonceCount, (unsigned long)qpc.LowPart);
    tmp[sizeof(tmp) - 1] = 0;
    DjRaw(&j, "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":");
    DjStr(&j, tmp);
    _snprintf(tmp, sizeof(tmp), ",\"args\":{\"pid\":%lu,\"activity\":", (unsigned long)GetCurrentProcessId());
    DjRaw(&j, tmp);
    if (!a->present) DjRaw(&j, "null");
    else {
        DjRaw(&j, "{\"type\":0,\"details\":");
        DjStr(&j, a->details);
        DjRaw(&j, ",\"state\":");
        DjStr(&j, a->state);
        _snprintf(tmp, sizeof(tmp), ",\"timestamps\":{\"start\":%lld}", a->start);
        DjRaw(&j, tmp);
        DjRaw(&j, ",\"assets\":{\"large_image\":");
        DjStr(&j, DISCORD_ICON_KEY);
        DjRaw(&j, ",\"large_text\":");
        DjStr(&j, DISCORD_ICON_TEXT);
        DjRaw(&j, "}}");
    }
    DjRaw(&j, "}}");
    return DiscordWrite(DC_FRAME, buf);
}

/* Replies, CLOSE and PING. FALSE: the connection is gone. */
static BOOL DiscordPump(BOOL *accepted)
{
    static char body[DISCORD_MAX_FRAME + 1];
    static char lastError[160];
    for (int frames = 0; frames < 32; frames++) {
        DWORD opcode = 0;
        int r = DiscordReadFrame(&opcode, body);
        if (r < 0) return FALSE;
        if (r == 0) return DiscordAvailable() >= 0;
        if (opcode == DC_CLOSE) {
            char msg[160];
            DiscordJsonString(body, "message", msg, sizeof(msg));
            Log("discord: Discord closed the connection (%s)", msg[0] ? msg : "no reason");
            return FALSE;
        }
        if (opcode == DC_PING) {
            if (!DiscordWrite(DC_PONG, body)) return FALSE;
            continue;
        }
        if (opcode != DC_FRAME) continue;
        char evt[32], cmd[32];
        DiscordJsonString(body, "evt", evt, sizeof(evt));
        DiscordJsonString(body, "cmd", cmd, sizeof(cmd));
        if (!strcmp(evt, "ERROR")) {
            char msg[160];
            DiscordJsonString(body, "message", msg, sizeof(msg));
            if (strcmp(msg, lastError)) Log("discord: activity rejected by Discord: %s", msg[0] ? msg : "unknown reason");
            lstrcpynA(lastError, msg, sizeof(lastError));
        } else if (!strcmp(cmd, "SET_ACTIVITY")) {
            lastError[0] = 0;
            if (!*accepted) Log("discord: activity accepted: %.300s", body);
            *accepted = TRUE;
        }
    }
    return TRUE;
}

/* ---- the worker: local server player count --------------------------------------------------------- */

typedef struct DiscordInfo { BOOL ok; int humans, bots; char map[64], mode[32]; } DiscordInfo;

static SOCKET g_dcSock = INVALID_SOCKET;

static void DiscordQuery(unsigned short port, DiscordInfo *out)
{
    static char reply[4096];
    static unsigned long seq;
    memset(out, 0, sizeof(*out));
    if (g_dcSock == INVALID_SOCKET) {
        g_dcSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (g_dcSock == INVALID_SOCKET) return;
    }
    char challenge[24], pkt[64];
    _snprintf(challenge, sizeof(challenge), "k33rpc%lu", (unsigned long)(++seq ^ GetTickCount()));
    challenge[sizeof(challenge) - 1] = 0;
    int len = _snprintf(pkt, sizeof(pkt), "\xff\xff\xff\xffgetinfo %s", challenge);
    struct sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (sendto(g_dcSock, pkt, len, 0, (struct sockaddr *)&to, sizeof(to)) != len) return;
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 700) {
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(g_dcSock, &rd);
        struct timeval tv = { 0, 100 * 1000 };
        if (select(0, &rd, NULL, NULL, &tv) <= 0) continue;
        int n = recv(g_dcSock, reply, sizeof(reply) - 1, 0);
        if (n <= 0) continue;   /* WSAECONNRESET: nothing on that port */
        reply[n] = 0;
        if (n < 17 || memcmp(reply, "\xff\xff\xff\xffinfoResponse", 16)) continue;
        const char *info = strchr(reply, '\\');
        char got[32], v[16];
        if (!info) continue;
        DiscordInfoValue(info, "challenge", got, sizeof(got));
        if (strcmp(got, challenge)) continue;   /* a late reply to an earlier query */
        DiscordInfoValue(info, "mapname", out->map, sizeof(out->map));
        DiscordInfoValue(info, "gametype", out->mode, sizeof(out->mode));
        DiscordInfoValue(info, "g_humanplayers", v, sizeof(v));
        out->humans = atoi(v);
        DiscordInfoValue(info, "clients", v, sizeof(v));
        out->bots = atoi(v) - out->humans;
        if (out->bots < 0) out->bots = 0;
        out->ok = TRUE;
        return;
    }
}

static void DiscordBuild(const DiscordGame *g, const DiscordInfo *info, DiscordActivity *a)
{
    memset(a, 0, sizeof(*a));
    a->present = TRUE;
    if (!g->active) {
        lstrcpyA(a->details, "In the lobby");
        lstrcpyA(a->state, "Choosing a match");
        a->start = g_dcLaunched;
        return;
    }
    a->start = g->activeSince;
    if (!g->map[0] || !g->mode[0]) {
        lstrcpyA(a->details, "In a match");
        lstrcpyA(a->state, "Playing");
        return;
    }
    char map[64], mode[64];
    DiscordMapName(g->map, map, sizeof(map));
    DiscordModeName(g->mode, mode, sizeof(mode));
    _snprintf(a->details, sizeof(a->details), "%s on %s", mode, map);
    a->details[sizeof(a->details) - 1] = 0;
    if (info->ok && info->humans >= 1 && !_stricmp(info->map, g->map) && !_stricmp(info->mode, g->mode)) {
        int o = _snprintf(a->state, sizeof(a->state), "%d player%s", info->humans, info->humans != 1 ? "s" : "");
        if (info->bots && o > 0) _snprintf(a->state + o, sizeof(a->state) - o, ", %d bots", info->bots);
        a->state[sizeof(a->state) - 1] = 0;
    } else lstrcpyA(a->state, "In a match");
}

static DWORD WINAPI DiscordThread(LPVOID unused)
{
    (void)unused;
    WSADATA wsa;
    BOOL net = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    g_dcEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!g_dcEvent) { Log("discord: no event (%lu), presence off", GetLastError()); return 0; }
    DiscordActivity sent, want;
    DiscordInfo info;
    memset(&sent, 0, sizeof(sent));
    memset(&info, 0, sizeof(info));
    BOOL needSend = TRUE, accepted = FALSE, wasEnabled = TRUE, missingLogged = FALSE;
    DWORD retryAt = 0, infoAt = 0;
    char infoKey[96] = "";
    for (;;) {
        Sleep(1000);
        DiscordGame g;
        EnterCriticalSection(&g_dcLock);
        g = g_dcGame;
        LeaveCriticalSection(&g_dcLock);
        if (!g.ready) continue;
        DWORD now = GetTickCount();

        if (!g.enabled) {
            if (wasEnabled) {
                Log("discord: off (k33_discord 0)");
                if (g_dcPipe != INVALID_HANDLE_VALUE) {
                    DiscordActivity none;
                    memset(&none, 0, sizeof(none));
                    DiscordSetActivity(&none);
                }
                DiscordClose();
                wasEnabled = FALSE;
            }
            continue;
        }
        if (!wasEnabled) { wasEnabled = TRUE; needSend = TRUE; retryAt = 0; Log("discord: on (k33_discord 1)"); }

        /* the local server's player count: on joining, on a map or mode change, then every 10 s */
        if (g.active && g.local && g.port && net) {
            char key[96];
            _snprintf(key, sizeof(key), "%u|%s|%s|%lld", g.port, g.map, g.mode, g.activeSince);
            key[sizeof(key) - 1] = 0;
            if (strcmp(key, infoKey) || now - infoAt >= DISCORD_INFO_MS) {
                if (strcmp(key, infoKey)) memset(&info, 0, sizeof(info));
                lstrcpynA(infoKey, key, sizeof(infoKey));
                infoAt = now;
                DiscordInfo fresh;
                DiscordQuery(g.port, &fresh);
                if (fresh.ok) info = fresh;   /* a lost reply keeps the last count */
            }
        } else {
            memset(&info, 0, sizeof(info));
            infoKey[0] = 0;
        }
        DiscordBuild(&g, &info, &want);

        if (g_dcPipe == INVALID_HANDLE_VALUE) {
            if (retryAt && (LONG)(now - retryAt) < 0) continue;
            if (!DiscordConnect()) {
                if (!missingLogged) Log("discord: no Discord client running; trying every %d s", DISCORD_RETRY_MS / 1000);
                missingLogged = TRUE;
                retryAt = GetTickCount() + DISCORD_RETRY_MS;
                continue;
            }
            missingLogged = FALSE;
            needSend = TRUE;
            accepted = FALSE;
        }
        if (needSend || memcmp(&want, &sent, sizeof(want))) {
            if (!DiscordSetActivity(&want)) {
                Log("discord: connection lost while sending; reconnecting");
                DiscordClose();
                needSend = TRUE;
                retryAt = GetTickCount() + DISCORD_RETRY_MS;
                continue;
            }
            if (needSend || strcmp(want.details, sent.details) || strcmp(want.state, sent.state))
                Log("discord: %s | %s", want.details, want.state);
            sent = want;
            needSend = FALSE;
        }
        if (!DiscordPump(&accepted)) {
            Log("discord: connection closed; reconnecting");
            DiscordClose();
            needSend = TRUE;
            retryAt = GetTickCount() + DISCORD_RETRY_MS;
        }
    }
    return 0;
}

/* ---- install ---------------------------------------------------------------------------------------- */

void DISCORD_TickHook(void);
__asm__(
    ".text\n"
    ".globl _DISCORD_TickHook\n"
    "_DISCORD_TickHook:\n"            /* the call at 0x595E9D: our tick, then whatever it called before */
    "    pushal\n"
    "    call _DiscordMainTick\n"
    "    popal\n"
    "    jmp *_g_discordNext\n");

static void InstallDiscord(void)
{
    const char *cl = GetCommandLineA();
    if (cl && (strstr(cl, "k33_discord 0") || strstr(cl, "k33_discord \"0\""))) {
        Log("discord: off (k33_discord 0 on the command line)");
        return;
    }
    C1GameInit();
    const BYTE *call = (const BYTE *)(DWORD_PTR)DISCORD_CALL_VA;
    if (!Readable(call, 5) || call[0] != 0xE8) {
        Log("discord: Com_EventLoop call 0x%08X differs, presence not installed", DISCORD_CALL_VA);
        return;
    }
    LONG rel;
    memcpy(&rel, call + 1, 4);
    DWORD target = DISCORD_CALL_VA + 5 + (DWORD)rel;
    /* the original Com_EventLoop, or voice.c's hook in front of it */
    if (target != DISCORD_CALL_FUNC && target != (DWORD)(DWORD_PTR)VOICE_TickHook) {
        Log("discord: Com_EventLoop call 0x%08X reaches 0x%08lX, presence not installed", DISCORD_CALL_VA,
            (unsigned long)target);
        return;
    }
    if (!g_c1DvarOk) {
        Log("discord: Dvar_RegisterInt missing, presence not installed");
        return;
    }
    InitializeCriticalSection(&g_dcLock);
    g_dcLaunched = DiscordNow();
    g_discordNext = target;
    RedirectCall(DISCORD_CALL_VA, (void *)DISCORD_TickHook);
    Log("discord: installed (Com_EventLoop call 0x%08X, chained to 0x%08lX); application %s", DISCORD_CALL_VA,
        (unsigned long)target, DISCORD_APP_ID);
}

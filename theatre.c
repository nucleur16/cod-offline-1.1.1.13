/*
 * theatre.c - the THEATRE screen's client side, ported from codol-classic's d3d9.dll (Theatre.cpp,
 * TheatreMessages.cpp, NativeConsole::HideReplayMinimap) and its exe patch. Included by kernel33.c after k33http.c.
 *
 * The shared front-end movie (voice_ui_mp: Main, MatchMPCTMenuView, MatchRoomListView, BottomStartGameView) is
 * codol-classic's THEATRE screen; the pause movie (controller_ingame: MainPause) is its replay pause menu. They talk
 * to the client only through string dvars:
 *   movie -> client  codol_theatre_request  refresh | next | previous | filter:mp|zm|all | watch:<64 hex id> | return
 *   client -> movie  codol_theatre_count, _page, _pages, _total, _filter, _status, _revision (a change redraws),
 *                    per row i (0..9): codol_theatre_id_i, _date_i, _title_i, _map_i, _mode_i, _file_i, _duration_i,
 *                    codol_theatre_playback (1 while a recording plays: the pause menu becomes the replay menu),
 *                    codol_theatre_open (1 reopens the THEATRE page after a recording ends)
 * This file answers those requests (main thread, every 200 ms, k33tick.c); every dvar write is a queued "set".
 *
 * Where recordings come from:
 *   - Recordings on this PC: main\demo\*.demo (films downloaded from a service below, or copied there). Each row's
 *     id is the SHA-256 of its file name; map, mode and length come from "<name>.demo.k33", written when a film is
 *     downloaded, else they show as unknown. The client cannot record: C1 removes "record" / "stoprecord"
 *     (Cmd_RemoveCommand at 0x4990AC / 0x4990E0) and never adds them, so films are made server-side.
 *   - A recording service (codol-classic's): codol_theatre_url (e.g. https://codol.kewkew.org; "" = none, the
 *     default) plus a 43-character session token that the lobby pushes as a group-messaging broadcast
 *     "CODOL_THEATRE_SESSION_V1:<token>" (consumed here before the ticker sees it) or the movie copies into
 *     codol_theatre_session from a task-610 server message. GET <url>/api/v1/theatre/replays?mode=&page= lists
 *     them; GET .../replays/<id>/download gives {record:{id,sha256,bytes,zone_type,client_build},url}; the file is
 *     downloaded (no bearer, size and SHA-256 checked) to main\demo\archive_<hash20>.demo.
 *   With no service, or no token yet, the screen lists this PC's recordings.
 *
 * Playing: "cl_demo_play <name>" from the main menu only (connection state, clc demo flags and the demo allocation
 * all idle), codol_theatre_playback 1 until the client is idle again, then codol_theatre_open 1. "return" (the
 * replay pause menu's Back) queues disconnectwithkeepsession. In a Zombies recording the stuck minimap_fullscreen
 * HUD menu is closed while it plays and shown again afterwards (C1 Menus_Close 0x5F9AE0 / show 0x5F9690).
 *
 * Exe patch (codol-classic shipped it as a one-byte change to the exe): 0x4933D9 jz -> jmp, so Escape opens the
 * native pause menu during a replay ("Allow native pause menu during replay").
 *
 * Group-messaging broadcast: CommunicationEventHandler vtable 0xC64034 slot 25 (0x5BB560, stdcall-shaped, five
 * dwords, ret 0x14). Only a CODOL_THEATRE_SESSION_V1 message is consumed; every other one goes to 0x5BB560.
 */

#include <time.h>

#define TH_ESCAPE_JZ_VA      0x004933D9u   /* 74 1B -> EB 1B */
#define TH_BROADCAST_SLOT_VA (0x00C64034u + 25 * 4)
#define TH_BROADCAST_FN_VA   0x005BB560u
#define TH_CLC_PTR_VA        0x016F8068u
#define TH_CLC_KEY           0x26F950CCu
#define TH_DEMO_ALLOC_VA     0x06C267DCu
#define TH_UI_CONTEXT_VA     0x016CF898u
#define TH_MENU_CLOSE_VA     0x005F9AE0u
#define TH_MENU_SHOW_VA      0x005F9690u
#define TH_POLL_MS           200
#define TH_ROWS              10
#define TH_MAX_LOCAL         512
#define TH_SESSION_PREFIX    "CODOL_THEATRE_SESSION_V1:"

enum { TH_CHOOSE, TH_NONE, TH_LOADING, TH_DOWNLOADING, TH_STARTING, TH_MENU_FIRST, TH_READY, TH_LIST_FAIL,
       TH_ONE_FAIL, TH_SESSION, TH_LOCAL, TH_LOCAL_NONE, TH_MISSING, TH_BUSY };
/* English, then Chinese in UTF-8 (the movies read dvars through Scaleform's Dvar_GetString, which takes UTF-8). */
static const char *const kThText[][2] = {
    { "Choose a recording to watch", "\xE9\x80\x89\xE6\x8B\xA9\xE8\xA6\x81\xE8\xA7\x82\xE7\x9C\x8B\xE7\x9A\x84\xE5\xBD\x95\xE5\x83\x8F" }, /* 选择要观看的录像 */
    { "No match recordings yet", "\xE6\x9A\x82\xE6\x97\xA0\xE6\xAF\x94\xE8\xB5\x9B\xE5\xBD\x95\xE5\x83\x8F" }, /* 暂无比赛录像 */
    { "Loading recordings", "\xE6\xAD\xA3\xE5\x9C\xA8\xE5\x8A\xA0\xE8\xBD\xBD\xE5\xBD\x95\xE5\x83\x8F" }, /* 正在加载录像 */
    { "Downloading recording", "\xE6\xAD\xA3\xE5\x9C\xA8\xE4\xB8\x8B\xE8\xBD\xBD\xE5\xBD\x95\xE5\x83\x8F" }, /* 正在下载录像 */
    { "Starting recording", "\xE6\xAD\xA3\xE5\x9C\xA8\xE6\x92\xAD\xE6\x94\xBE\xE5\xBD\x95\xE5\x83\x8F" }, /* 正在播放录像 */
    { "Return to the main menu before watching a recording",
      "\xE8\xAF\xB7\xE5\x85\x88\xE8\xBF\x94\xE5\x9B\x9E\xE4\xB8\xBB\xE8\x8F\x9C\xE5\x8D\x95\xE5\x86\x8D\xE8\xA7\x82\xE7\x9C\x8B\xE5\xBD\x95\xE5\x83\x8F" }, /* 请先返回主菜单再观看录像 */
    { "Recording ready - return to the main menu to watch",
      "\xE5\xBD\x95\xE5\x83\x8F\xE5\xB7\xB2\xE4\xB8\x8B\xE8\xBD\xBD\xEF\xBC\x8C\xE8\xAF\xB7\xE8\xBF\x94\xE5\x9B\x9E\xE4\xB8\xBB\xE8\x8F\x9C\xE5\x8D\x95\xE8\xA7\x82\xE7\x9C\x8B" }, /* 录像已下载，请返回主菜单观看 */
    { "Recordings unavailable - refresh and try again",
      "\xE6\x97\xA0\xE6\xB3\x95\xE8\x8E\xB7\xE5\x8F\x96\xE5\xBD\x95\xE5\x83\x8F\xE5\x88\x97\xE8\xA1\xA8\xEF\xBC\x8C\xE8\xAF\xB7\xE5\x88\xB7\xE6\x96\xB0\xE5\x90\x8E\xE9\x87\x8D\xE8\xAF\x95" }, /* 无法获取录像列表，请刷新后重试 */
    { "Recording unavailable - refresh and try again",
      "\xE5\xBD\x95\xE5\x83\x8F\xE4\xB8\x8D\xE5\x8F\xAF\xE7\x94\xA8\xEF\xBC\x8C\xE8\xAF\xB7\xE5\x88\xB7\xE6\x96\xB0\xE5\x90\x8E\xE9\x87\x8D\xE8\xAF\x95" }, /* 录像不可用，请刷新后重试 */
    { "Game session refreshing - try again shortly",
      "\xE4\xBC\x9A\xE8\xAF\x9D\xE6\xAD\xA3\xE5\x9C\xA8\xE5\x88\xB7\xE6\x96\xB0\xEF\xBC\x8C\xE8\xAF\xB7\xE7\xA8\x8D\xE5\x90\x8E\xE9\x87\x8D\xE8\xAF\x95" }, /* 会话正在刷新，请稍后重试 */
    { "Recordings on this PC", "\xE6\x9C\xAC\xE6\x9C\xBA\xE5\xBD\x95\xE5\x83\x8F" }, /* 本机录像 */
    { "No recordings on this PC yet", "\xE6\x9C\xAC\xE6\x9C\xBA\xE6\x9A\x82\xE6\x97\xA0\xE5\xBD\x95\xE5\x83\x8F" }, /* 本机暂无录像 */
    { "Recording file missing - refresh", "\xE5\xBD\x95\xE5\x83\x8F\xE6\x96\x87\xE4\xBB\xB6\xE4\xB8\x8D\xE5\xAD\x98\xE5\x9C\xA8\xEF\xBC\x8C\xE8\xAF\xB7\xE5\x88\xB7\xE6\x96\xB0" }, /* 录像文件不存在，请刷新 */
    { "Recording service unavailable - try again",
      "\xE5\xBD\x95\xE5\x83\x8F\xE6\x9C\x8D\xE5\x8A\xA1\xE4\xB8\x8D\xE5\x8F\xAF\xE7\x94\xA8\xEF\xBC\x8C\xE8\xAF\xB7\xE9\x87\x8D\xE8\xAF\x95" }, /* 录像服务不可用，请重试 */
};

/* The 1.1.1.13 client builds a service recording may come from (codol-classic's record.client_build). */
static const char *const kThBuilds[] = {
    "2325685e6210062b9ffb4f435f9460c30ef0f9db9586b14f09bafe570baabd17",
    "a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976",
    "904436480938632a856b39dea73fb94c9682b5a0ebf7fa619ecd5dd5475ed3b6",
};

typedef int(__stdcall *ThBroadcastFn)(DWORD, DWORD, DWORD, const char *, unsigned);

typedef struct ThRow {
    char id[65], file[64], date[32], map[64], mode[32];
    unsigned seconds;
    FILETIME written;
} ThRow;

typedef struct ThJob {          /* worker input and result */
    char url[512], token[48], action[80], filter[8];
    int page;
    unsigned session;
    /* result */
    BOOL ok;
    int status, count, pages, total, current;
    ThRow rows[TH_ROWS];
    char demo[64], zone[16];
} ThJob;

static BOOL g_thMenusOk;
static ThBroadcastFn g_thBroadcast;
static CRITICAL_SECTION g_thLock;
static char g_thPendingToken[44];        /* from the broadcast hook (any thread), under g_thLock */
static char g_thToken[44];               /* main thread from here on */
static unsigned g_thSession, g_thRevision;
static char g_thFilter[8] = "all";
static int g_thPage;
static BOOL g_thBusy, g_thWatching, g_thStarted, g_thReturning;
static char g_thPlayingZone[16];
static DWORD g_thSubmittedAt;
static ThJob *volatile g_thDone;        /* finished job handed back by the worker */
static char g_thQueue[192][200];         /* commands waiting for room in the command buffer */
static int g_thQueueHead, g_thQueueLen;
static ThRow g_thLocal[TH_ROWS];         /* the page of local rows shown (watch looks the id up here) */
static int g_thLocalCount;
static ThRow g_thService[TH_ROWS];      /* the page of service rows shown (a download keeps its map/mode) */
static int g_thServiceCount;

static const char *ThText(int id) { return kThText[id][g_english ? 0 : 1]; }

static void ThQueue(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void ThQueue(const char *fmt, ...)
{
    if (g_thQueueLen >= (int)(sizeof(g_thQueue) / sizeof(g_thQueue[0]))) return;
    va_list ap;
    va_start(ap, fmt);
    char *slot = g_thQueue[(g_thQueueHead + g_thQueueLen) % (sizeof(g_thQueue) / sizeof(g_thQueue[0]))];
    _vsnprintf(slot, sizeof(g_thQueue[0]) - 1, fmt, ap);
    slot[sizeof(g_thQueue[0]) - 1] = 0;
    va_end(ap);
    g_thQueueLen++;
}

/* Field values the movie shows: printable ASCII or UTF-8; quotes, semicolons and controls dropped. */
static void ThSet(const char *field, const char *value)
{
    char clean[128];
    int n = 0;
    for (const char *p = value; p && *p && n < (int)sizeof(clean) - 1; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c == 127 || c == '"' || c == ';' || c == '\\') continue;
        clean[n++] = (char)c;
    }
    clean[n] = 0;
    ThQueue("set codol_theatre_%s \"%s\"\n", field, clean);
}

static void ThStatus(int id) { ThSet("status", ThText(id)); }

static BOOL ThHex64(const char *s)
{
    if (!s || strlen(s) != 64) return FALSE;
    for (int i = 0; i < 64; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return FALSE;
    return TRUE;
}

static BOOL ThSafe(const char *s)
{
    if (!s || strlen(s) > 120) return FALSE;
    for (; *s; s++)
        if (!(isalnum((unsigned char)*s) || strchr("_ :./-", *s))) return FALSE;
    return TRUE;
}

/* ---- C1 state ------------------------------------------------------------------------------------------- */

typedef struct ThState { DWORD connection, demo, handle, allocation; } ThState;

static BOOL ThGetState(ThState *s)
{
    memset(s, 0, sizeof(*s));
    s->connection = *(volatile DWORD *)(DWORD_PTR)VOICE_CONNSTATE_VA;
    s->allocation = *(volatile DWORD *)(DWORD_PTR)TH_DEMO_ALLOC_VA;
    DWORD p = *(volatile DWORD *)(DWORD_PTR)TH_CLC_PTR_VA;
    if (p && p != TH_CLC_KEY) {
        const BYTE *clc = (const BYTE *)(DWORD_PTR)(p ^ TH_CLC_KEY);
        if (!Readable(clc + 0x8025C, 8)) return FALSE;
        s->handle = *(const DWORD *)(clc + 0x8025C);
        s->demo = *(const DWORD *)(clc + 0x80260);
    }
    return s->connection <= 9 && s->demo <= 1;
}

static BOOL ThIdle(const ThState *s) { return !s->connection && !s->demo && !s->handle && !s->allocation; }

/* minimap_fullscreen (a classic HUD menu) sticks open over a Zombies replay; close it while one plays. */
static void ThMenuClose(DWORD ctx, DWORD menu)
{
    DWORD fn = TH_MENU_CLOSE_VA;
    __asm__ volatile("call *%[fn]" : "+c"(ctx), "+S"(menu) : [fn] "r"(fn) : "eax", "edx", "memory", "cc");
}

static void ThMenuShow(DWORD ctx, const char *name)
{
    DWORD fn = TH_MENU_SHOW_VA;
    __asm__ volatile("call *%[fn]" : "+a"(name), "+S"(ctx) : [fn] "r"(fn) : "ecx", "edx", "memory", "cc");
}

static void ThHideReplayMinimap(BOOL hide)
{
    static DWORD owned;
    if (!g_thMenusOk || (!hide && !owned)) return;
    const BYTE *ctx = (const BYTE *)(DWORD_PTR)TH_UI_CONTEXT_VA;
    if (!Readable(ctx, 0xA3C) || *(const DWORD *)ctx != 0) return;
    DWORD count = *(const DWORD *)(ctx + 0xA38), menu = 0;
    if (count > 640) return;
    for (DWORD i = 0; i < count; i++) {
        const DWORD *cand = (const DWORD *)(DWORD_PTR) * (const DWORD *)(ctx + 0x38 + i * 4);
        if (!cand || !Readable(cand, 0xD8)) continue;
        const char *name = (const char *)(DWORD_PTR)cand[0];
        if (name && Readable(name, 19) && !strcmp(name, "minimap_fullscreen")) { menu = (DWORD)(DWORD_PTR)cand; break; }
    }
    if (!menu) { owned = 0; return; }
    if (owned && owned != menu) owned = 0;
    DWORD flags = *(const DWORD *)(DWORD_PTR)(menu + 0x48);
    if (hide && (flags & 4)) {
        if (*(const DWORD *)(DWORD_PTR)(menu + 0xD4)) return;   /* never run a menu's onClose script */
        ThMenuClose(TH_UI_CONTEXT_VA, menu);
        owned = menu;
    } else if (!hide && owned) {
        if (!(flags & 4)) ThMenuShow(TH_UI_CONTEXT_VA, "minimap_fullscreen");
        owned = 0;
    }
}

/* ---- session token (group-messaging broadcast) --------------------------------------------------------------- */

/* TRUE: a reserved Theatre message (valid or not), never shown. */
static BOOL ThPrivateMessage(const char *text, unsigned size)
{
    if (!text || !size || !Readable(text, size < 1024 ? size : 1024)) return FALSE;
    char copy[1025];
    unsigned count = size < 1024 ? size : 1024;
    memcpy(copy, text, count);
    copy[count] = 0;
    const char *body = copy;
    const char *markup = strstr(copy, "<MSG ");
    if (markup) {
        const char *end = strstr(markup, "/>");
        if (!end) return FALSE;
        body = end + 2;
    }
    if (strncmp(body, TH_SESSION_PREFIX, sizeof(TH_SESSION_PREFIX) - 1)) return FALSE;
    const char *value = body + sizeof(TH_SESSION_PREFIX) - 1;
    size_t length = count - (size_t)(value - copy);
    BOOL valid = size <= 1024 && length == 43;
    for (size_t i = 0; valid && i < length; i++)
        valid = isalnum((unsigned char)value[i]) || value[i] == '_' || value[i] == '-';
    if (valid) {
        EnterCriticalSection(&g_thLock);
        memcpy(g_thPendingToken, value, 43);
        g_thPendingToken[43] = 0;
        LeaveCriticalSection(&g_thLock);
    }
    SecureZeroMemory(copy, sizeof(copy));
    return TRUE;
}

static int __stdcall ThBroadcastHook(DWORD a, DWORD b, DWORD c, const char *text, unsigned size)
{
    if (ThPrivateMessage(text, size)) return 0;
    return g_thBroadcast(a, b, c, text, size);
}

static void ThSetToken(const char *token)
{
    if (strlen(token) != 43) return;
    for (const char *p = token; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return;
    if (strcmp(token, g_thToken)) {
        g_thSession++;
        ThSet("count", "0");
        ThSet("total", "0");
        char r[16];
        _snprintf(r, sizeof(r), "%u", ++g_thRevision);
        ThSet("revision", r);
        ThSet("request", "refresh");
        Log("theatre: recording service session received");
    }
    lstrcpynA(g_thToken, token, sizeof(g_thToken));
}

/* ---- worker: the recording service -------------------------------------------------------------------------- */

static BOOL ThServiceGet(ThJob *job, const char *path, char **body)
{
    char url[700];
    DWORD len = 0;
    _snprintf(url, sizeof(url) - 1, "%s/api/v1/theatre%s", job->url, path);
    url[sizeof(url) - 1] = 0;
    if (!K33HttpGet(url, job->token, 15000, 1024 * 1024, body, &len, &job->status)) return FALSE;
    const char *p = K33JsonWs(*body);
    if (job->status != 200 || !p || *p != '{') { free(*body); *body = NULL; return FALSE; }
    return TRUE;
}

static void ThWorkCatalogue(ThJob *job)
{
    char path[96], *body = NULL;
    _snprintf(path, sizeof(path), "/replays?mode=%s&page=%d", job->filter, job->page);
    if (!ThServiceGet(job, path, &body)) return;
    const char *rows = K33JsonGet(body, "replays");
    char filter[16];
    job->pages = K33JsonInt(body, "pages", 0);
    job->total = K33JsonInt(body, "total", -1);
    job->current = K33JsonInt(body, "page", 0);
    K33JsonStr(K33JsonGet(body, "filter"), filter, sizeof(filter));
    BOOL ok = rows && *rows == '[' && job->pages >= 1 && job->total >= 0 && job->current >= 1 &&
              job->current <= job->pages && !strcmp(filter, job->filter);
    const char *p = ok ? K33JsonWs(rows + 1) : NULL;
    while (ok && p && *p == '{') {
        if (job->count >= TH_ROWS) { ok = FALSE; break; }
        ThRow *r = &job->rows[job->count];
        double ms = -1, at = 0;
        K33JsonStr(K33JsonGet(p, "id"), r->id, sizeof(r->id));
        K33JsonStr(K33JsonGet(p, "map"), r->map, sizeof(r->map));
        K33JsonStr(K33JsonGet(p, "gametype"), r->mode, sizeof(r->mode));
        K33JsonNum(K33JsonGet(p, "duration_ms"), &ms);
        K33JsonNum(K33JsonGet(p, "archived_at"), &at);
        if (!ThHex64(r->id) || !ThSafe(r->map) || !ThSafe(r->mode) || ms < 0) { ok = FALSE; break; }
        r->seconds = (unsigned)(ms / 1000);
        time_t t = (time_t)at;
        struct tm *lt = localtime(&t);
        if (lt) strftime(r->date, sizeof(r->date), g_english ? "%b %d %H:%M" : "%m-%d %H:%M", lt);
        job->count++;
        p = K33JsonWs(K33JsonSkip(p));
        if (p && *p == ',') p = K33JsonWs(p + 1);
    }
    job->ok = ok && p && *p == ']';
    free(body);
}

static void ThWorkDownload(ThJob *job)
{
    const char *id = job->action + 6;
    char path[96], *body = NULL;
    _snprintf(path, sizeof(path), "/replays/%s/download", id);
    if (!ThServiceGet(job, path, &body)) return;
    const char *rec = K33JsonGet(body, "record");
    char rid[72], hash[72], build[72], url[1024];
    double bytes = 0;
    K33JsonStr(K33JsonGet(rec, "id"), rid, sizeof(rid));
    K33JsonStr(K33JsonGet(rec, "sha256"), hash, sizeof(hash));
    K33JsonStr(K33JsonGet(rec, "client_build"), build, sizeof(build));
    K33JsonStr(K33JsonGet(rec, "zone_type"), job->zone, sizeof(job->zone));
    K33JsonNum(K33JsonGet(rec, "bytes"), &bytes);
    BOOL known = FALSE;
    for (unsigned i = 0; i < sizeof(kThBuilds) / sizeof(kThBuilds[0]); i++) known |= !strcmp(build, kThBuilds[i]);
    BOOL ok = rec && !strcmp(rid, id) && ThHex64(hash) && known && bytes >= 36 && bytes <= 512.0 * 1024 * 1024 &&
              bytes == (double)(ULONGLONG)bytes && K33JsonStr(K33JsonGet(body, "url"), url, sizeof(url));
    if (ok) {
        WCHAR file[MAX_PATH];
        DWORD n = GetModuleFileNameW(NULL, file, MAX_PATH);
        while (n && file[n - 1] != L'\\') n--;
        _snprintf(job->demo, sizeof(job->demo), "archive_%.20s", hash);
        if (n + 40 < MAX_PATH) {
            _snwprintf(file + n, MAX_PATH - n, L"main\\demo\\%hs.demo", job->demo);
            CreateDirectoryW(L"main", NULL);
            WCHAR dir[MAX_PATH];
            lstrcpynW(dir, file, n + 1);
            lstrcatW(dir, L"main\\demo");
            CreateDirectoryW(dir, NULL);
            char have[65];
            ok = (K33Sha256Hex(NULL, 0, file, have) && !strcmp(have, hash)) ||
                 K33HttpDownload(url, file, hash, (ULONGLONG)bytes);
        } else {
            ok = FALSE;
        }
    }
    job->ok = ok;
    free(body);
}

static DWORD WINAPI ThWorker(LPVOID arg)
{
    ThJob *job = (ThJob *)arg;
    if (!strncmp(job->action, "watch:", 6)) ThWorkDownload(job);
    else ThWorkCatalogue(job);
    SecureZeroMemory(job->token, sizeof(job->token));
    InterlockedExchangePointer((PVOID volatile *)&g_thDone, job);
    return 0;
}

static void ThStart(const char *url, const char *action)
{
    ThJob *job = (ThJob *)calloc(1, sizeof(ThJob));
    if (!job) return;
    lstrcpynA(job->url, url, sizeof(job->url));
    lstrcpynA(job->token, g_thToken, sizeof(job->token));
    lstrcpynA(job->action, action, sizeof(job->action));
    lstrcpynA(job->filter, g_thFilter, sizeof(job->filter));
    job->page = g_thPage + 1;
    job->session = g_thSession;
    HANDLE t = CreateThread(NULL, 0, ThWorker, job, 0, NULL);
    if (!t) { free(job); ThStatus(TH_BUSY); return; }
    CloseHandle(t);
    g_thBusy = TRUE;
}

/* ---- recordings on this PC ----------------------------------------------------------------------------------- */

static int ThDemoDir(WCHAR *dir)
{
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    while (n && dir[n - 1] != L'\\') n--;
    if (n + 16 >= MAX_PATH) return 0;
    lstrcpyW(dir + n, L"main\\demo\\");
    return (int)n + 10;
}

/* "<name>.demo.k33": map\tmode\tseconds (written by the auto-recorder). */
static void ThReadSidecar(const WCHAR *demoPath, ThRow *r)
{
    WCHAR p[MAX_PATH + 8];
    lstrcpynW(p, demoPath, MAX_PATH);
    lstrcatW(p, L".k33");
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    char buf[160];
    DWORD got = 0;
    ReadFile(f, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(f);
    buf[got] = 0;
    char *map = buf, *mode = strchr(map, '\t'), *secs = mode ? strchr(mode + 1, '\t') : NULL;
    if (!mode) return;
    *mode++ = 0;
    if (secs) { *secs++ = 0; r->seconds = (unsigned)strtoul(secs, NULL, 10); }
    if (ThSafe(map)) lstrcpynA(r->map, map, sizeof(r->map));
    if (ThSafe(mode)) lstrcpynA(r->mode, mode, sizeof(r->mode));
}

/* C1's demo envelope (C1 0x72BC80 / 0x72CB00, as codol-classic's website/replay_demo.py reads it): a 32800-byte
 * header whose first int is the tag (1..3), the record stream, then metadata (640 fixed bytes: tag, start, end, ...;
 * a table of at most 40 x 24 bytes; one of at most 300 x 8 bytes) and an 8-byte footer {metadata size, tag}. Only a
 * file with a consistent envelope is listed, so cl_demo_play never gets a file C1 would reject with
 * EXE_ERR_READING_DEMO (a structure check only: it does not prove the gameplay stream plays). */
static BOOL ThDemoEnvelope(const WCHAR *path, unsigned *seconds)
{
    BOOL ok = FALSE;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER size;
    int head = 0, foot[2], fixed[3], count1 = 0, count2 = 0;
    DWORD got;
    if (GetFileSizeEx(f, &size) && size.QuadPart >= 32800 + 640 + 8 + 8 && size.QuadPart < 0x7FFFFFFF &&
        ReadFile(f, &head, 4, &got, NULL) && got == 4 &&
        SetFilePointer(f, (LONG)size.QuadPart - 8, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
        ReadFile(f, foot, 8, &got, NULL) && got == 8 && foot[1] >= 1 && foot[1] <= 3 && head == foot[1] &&
        foot[0] >= 640 + 8 && foot[0] <= (LONG)size.QuadPart - 32800 - 8) {
        LONG meta = (LONG)size.QuadPart - 8 - foot[0];
        if (SetFilePointer(f, meta, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
            ReadFile(f, fixed, 12, &got, NULL) && got == 12 && fixed[0] == foot[1] &&
            SetFilePointer(f, meta + 640, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
            ReadFile(f, &count1, 4, &got, NULL) && got == 4 && count1 >= 0 && count1 <= 40 &&
            640 + 8 + count1 * 24 <= foot[0] &&
            SetFilePointer(f, meta + 644 + count1 * 24, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
            ReadFile(f, &count2, 4, &got, NULL) && got == 4 && count2 >= 0 && count2 <= 300 &&
            640 + 8 + count1 * 24 + count2 * 8 <= foot[0]) {
            ok = TRUE;
            if (seconds && fixed[2] > fixed[1]) *seconds = (unsigned)(fixed[2] - fixed[1]) / 1000;
        }
    }
    CloseHandle(f);
    return ok;
}

static int ThCompareRows(const void *a, const void *b)
{
    return -CompareFileTime(&((const ThRow *)a)->written, &((const ThRow *)b)->written);
}

static BOOL ThZombieRow(const ThRow *r) { return !_stricmp(r->mode, "zm") || !_strnicmp(r->map, "zm_", 3); }

static void ThLocalCatalogue(void)
{
    static ThRow all[TH_MAX_LOCAL];
    WCHAR dir[MAX_PATH], pattern[MAX_PATH];
    int base = ThDemoDir(dir), count = 0;
    if (base) {
        lstrcpyW(pattern, dir);
        lstrcatW(pattern, L"*.demo");
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                int len = lstrlenW(fd.cFileName);
                if (len < 6 || len - 5 >= (int)sizeof(all[0].file) || count >= TH_MAX_LOCAL) continue;
                ThRow *r = &all[count];
                memset(r, 0, sizeof(*r));
                if (!WideCharToMultiByte(CP_ACP, 0, fd.cFileName, len - 5, r->file, sizeof(r->file) - 1, NULL, NULL)) continue;
                if (!ThSafe(r->file) || strchr(r->file, ' ')) continue;   /* cl_demo_play takes one token */
                r->written = fd.ftLastWriteTime;
                K33Sha256Hex(r->file, (DWORD)strlen(r->file), NULL, r->id);
                WCHAR full[MAX_PATH];
                lstrcpyW(full, dir);
                if (lstrlenW(full) + len >= MAX_PATH) continue;
                lstrcatW(full, fd.cFileName);
                if (!ThDemoEnvelope(full, &r->seconds)) continue;      /* not a C1 film: never offered */
                ThReadSidecar(full, r);
                SYSTEMTIME st;
                FILETIME local;
                FileTimeToLocalFileTime(&fd.ftLastWriteTime, &local);
                FileTimeToSystemTime(&local, &st);
                static const char *const kMon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
                if (g_english)
                    _snprintf(r->date, sizeof(r->date), "%s %02u %02u:%02u", kMon[(st.wMonth + 11) % 12], st.wDay, st.wHour, st.wMinute);
                else
                    _snprintf(r->date, sizeof(r->date), "%02u-%02u %02u:%02u", st.wMonth, st.wDay, st.wHour, st.wMinute);
                if (!strcmp(g_thFilter, "mp") && ThZombieRow(r)) continue;
                if (!strcmp(g_thFilter, "zm") && !ThZombieRow(r)) continue;
                count++;
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    qsort(all, count, sizeof(all[0]), ThCompareRows);
    int pages = count ? (count + TH_ROWS - 1) / TH_ROWS : 1;
    if (g_thPage >= pages) g_thPage = pages - 1;
    if (g_thPage < 0) g_thPage = 0;
    g_thLocalCount = 0;
    for (int i = g_thPage * TH_ROWS; i < count && g_thLocalCount < TH_ROWS; i++) g_thLocal[g_thLocalCount++] = all[i];

    char num[16], field[24];
    for (int i = 0; i < g_thLocalCount; i++) {
        const ThRow *r = &g_thLocal[i];
        _snprintf(field, sizeof(field), "date_%d", i); ThSet(field, r->date);
        _snprintf(field, sizeof(field), "title_%d", i); ThSet(field, r->date);
        _snprintf(field, sizeof(field), "map_%d", i); ThSet(field, r->map);
        _snprintf(field, sizeof(field), "mode_%d", i); ThSet(field, r->mode);
        _snprintf(field, sizeof(field), "id_%d", i); ThSet(field, r->id);
        _snprintf(field, sizeof(field), "file_%d", i); ThSet(field, r->file);
        _snprintf(num, sizeof(num), "%u", r->seconds);
        _snprintf(field, sizeof(field), "duration_%d", i); ThSet(field, num);
    }
    _snprintf(num, sizeof(num), "%d", g_thLocalCount); ThSet("count", num);
    ThSet("filter", g_thFilter);
    _snprintf(num, sizeof(num), "%d", g_thPage + 1); ThSet("page", num);
    _snprintf(num, sizeof(num), "%d", pages); ThSet("pages", num);
    _snprintf(num, sizeof(num), "%d", count); ThSet("total", num);
    _snprintf(num, sizeof(num), "%u", ++g_thRevision); ThSet("revision", num);
    ThStatus(count ? TH_LOCAL : TH_LOCAL_NONE);
}

static void ThPublishService(const ThJob *job)
{
    char num[16], field[24];
    g_thPage = job->current - 1;
    g_thLocalCount = 0;
    g_thServiceCount = job->count;
    memcpy(g_thService, job->rows, sizeof(g_thService));
    for (int i = 0; i < job->count; i++) {
        const ThRow *r = &job->rows[i];
        _snprintf(field, sizeof(field), "date_%d", i); ThSet(field, r->date);
        _snprintf(field, sizeof(field), "title_%d", i); ThSet(field, r->date);
        _snprintf(field, sizeof(field), "map_%d", i); ThSet(field, r->map);
        _snprintf(field, sizeof(field), "mode_%d", i); ThSet(field, r->mode);
        _snprintf(field, sizeof(field), "id_%d", i); ThSet(field, r->id);
        _snprintf(field, sizeof(field), "file_%d", i); ThSet(field, "");
        _snprintf(num, sizeof(num), "%u", r->seconds);
        _snprintf(field, sizeof(field), "duration_%d", i); ThSet(field, num);
    }
    _snprintf(num, sizeof(num), "%d", job->count); ThSet("count", num);
    ThSet("filter", g_thFilter);
    _snprintf(num, sizeof(num), "%d", job->current); ThSet("page", num);
    _snprintf(num, sizeof(num), "%d", job->pages); ThSet("pages", num);
    _snprintf(num, sizeof(num), "%d", job->total); ThSet("total", num);
    _snprintf(num, sizeof(num), "%u", ++g_thRevision); ThSet("revision", num);
    ThStatus(job->total ? TH_CHOOSE : TH_NONE);
}

/* ---- sidecars ------------------------------------------------------------------------------------------- */

static void ThWriteSidecar(const char *name, const char *map, const char *mode, unsigned seconds)
{
    WCHAR dir[MAX_PATH];
    int base = ThDemoDir(dir);
    if (!base || base + 80 >= MAX_PATH) return;
    _snwprintf(dir + base, MAX_PATH - base, L"%hs.demo.k33", name);
    HANDLE f = CreateFileW(dir, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    char line[160];
    int n = _snprintf(line, sizeof(line), "%s\t%s\t%u\n", map, mode, seconds);
    DWORD w;
    if (n > 0) WriteFile(f, line, (DWORD)n, &w, NULL);
    CloseHandle(f);
}

/* ---- main thread --------------------------------------------------------------------------------------------- */

static void ThFinishReply(void)
{
    ThJob *job = (ThJob *)InterlockedExchangePointer((PVOID volatile *)&g_thDone, NULL);
    if (!job) return;
    g_thBusy = FALSE;
    if (job->session != g_thSession) { free(job); return; }   /* an older session's reply */
    if (strncmp(job->action, "watch:", 6)) {
        if (job->ok) ThPublishService(job);
        else if (job->status == 401) ThStatus(TH_SESSION);
        else ThStatus(TH_LIST_FAIL);
        free(job);
        return;
    }
    if (!job->ok) { ThStatus(job->status == 401 ? TH_SESSION : TH_ONE_FAIL); free(job); return; }
    ThState s;
    if (!ThGetState(&s) || !ThIdle(&s)) { ThStatus(TH_READY); free(job); return; }
    for (int i = 0; i < g_thServiceCount; i++)
        if (!strcmp(g_thService[i].id, job->action + 6))
            ThWriteSidecar(job->demo, g_thService[i].map, g_thService[i].mode, g_thService[i].seconds);
    ThSet("playback", "1");
    lstrcpynA(g_thPlayingZone, job->zone, sizeof(g_thPlayingZone));
    ThQueue("cl_demo_play %s\n", job->demo);
    g_thWatching = TRUE;
    g_thStarted = FALSE;
    g_thSubmittedAt = GetTickCount();
    ThStatus(TH_STARTING);
    free(job);
}

static void ThWatchLocal(const char *id, const ThState *s)
{
    for (int i = 0; i < g_thLocalCount; i++) {
        const ThRow *r = &g_thLocal[i];
        if (strcmp(r->id, id)) continue;
        WCHAR p[MAX_PATH];
        int base = ThDemoDir(p);
        if (!base || base + 72 >= MAX_PATH) break;
        _snwprintf(p + base, MAX_PATH - base, L"%hs.demo", r->file);
        if (!ThDemoEnvelope(p, NULL)) break;
        if (!ThIdle(s)) { ThStatus(TH_MENU_FIRST); return; }
        ThSet("playback", "1");
        lstrcpynA(g_thPlayingZone, ThZombieRow(r) ? "zm" : "mp", sizeof(g_thPlayingZone));
        ThQueue("cl_demo_play %s\n", r->file);
        g_thWatching = TRUE;
        g_thStarted = FALSE;
        g_thSubmittedAt = GetTickCount();
        ThStatus(TH_STARTING);
        Log("theatre: playing local recording %s", r->file);
        return;
    }
    ThStatus(TH_MISSING);
}

static void TheatreTick(void)
{
    if (!C1DvarsReady()) return;
    /* queued commands, as many a frame as leave the command buffer 1 KB for C1's own */
    if (g_thQueueLen) {
        for (int sent = 0; g_thQueueLen && sent < 32; sent++) {
            const char *cmd = g_thQueue[g_thQueueHead];
            if ((int)strlen(cmd) + 1024 >= C1CbufFree() || !C1CbufAddText(cmd)) break;
            g_thQueueHead = (g_thQueueHead + 1) % (int)(sizeof(g_thQueue) / sizeof(g_thQueue[0]));
            g_thQueueLen--;
        }
        return;
    }

    static DWORD next;
    DWORD now = GetTickCount();
    if ((LONG)(now - next) < 0) return;
    next = now + TH_POLL_MS;

    ThState s;
    BOOL known = ThGetState(&s);
    char gametype[16];
    C1DvarString("g_gametype", gametype, sizeof(gametype));
    ThHideReplayMinimap(known && s.connection == 9 && s.demo == 1 &&
                        ((g_thWatching && !strcmp(g_thPlayingZone, "zm")) || !strcmp(gametype, "zm")));

    char token[64];
    EnterCriticalSection(&g_thLock);
    lstrcpynA(token, g_thPendingToken, sizeof(token));
    SecureZeroMemory(g_thPendingToken, sizeof(g_thPendingToken));
    LeaveCriticalSection(&g_thLock);
    if (!token[0] && C1DvarString("codol_theatre_session", token, sizeof(token)) && token[0])
        ThSet("session", "");
    if (token[0]) ThSetToken(token);
    SecureZeroMemory(token, sizeof(token));

    ThFinishReply();
    if (g_thQueueLen || !known) return;

    if (g_thWatching) {
        static ThState last;
        static DWORD stuckSince;
        if (memcmp(&last, &s, sizeof(s))) {
            Log("theatre: playback state connection %lu demo %lu handle %08lX allocation %08lX",
                (unsigned long)s.connection, (unsigned long)s.demo, (unsigned long)s.handle, (unsigned long)s.allocation);
            last = s;
            stuckSince = now;
        }
        if (s.connection || s.allocation) g_thStarted = TRUE;
        /* a film C1 cannot read (EXE_ERR_READING_DEMO) leaves the viewer connected below CA_LOADING with the error
         * shown: disconnect it so the front end comes back (a readable film moves on to loading within a frame) */
        if (g_thStarted && s.connection && s.connection < 7 && now - stuckSince > 5000 && !g_thReturning) {
            Log("theatre: playback stuck at connection state %lu; disconnecting", (unsigned long)s.connection);
            ThQueue("disconnect\n");
            g_thReturning = TRUE;
            stuckSince = now;
        }
        if ((g_thStarted && ThIdle(&s)) || (!g_thStarted && now - g_thSubmittedAt > 30000)) {
            g_thWatching = g_thStarted = g_thReturning = FALSE;
            g_thPlayingZone[0] = 0;
            ThSet("playback", "0");
            ThSet("open", "1");
        }
    }
    if (g_thReturning) {
        if (ThIdle(&s)) {
            g_thReturning = g_thWatching = g_thStarted = FALSE;
            g_thPlayingZone[0] = 0;
            ThSet("playback", "0");
            ThSet("open", "1");
        }
        return;
    }
    if (g_thBusy) return;

    char action[80];
    if (!C1DvarString("codol_theatre_request", action, sizeof(action)) || !action[0]) return;
    ThSet("request", "");
    if (!strcmp(action, "return")) {
        if (s.connection == 9 && s.demo == 1) {
            ThQueue("disconnectwithkeepsession\n");
            g_thReturning = TRUE;
        }
        return;
    }
    char url[512];
    C1DvarString("codol_theatre_url", url, sizeof(url));
    for (int n = (int)strlen(url); n && url[n - 1] == '/'; ) url[--n] = 0;
    BOOL service = url[0] && g_thToken[0] &&
                   (!strncmp(url, "https://", 8) || !strncmp(url, "http://127.0.0.1:", 17) || !strncmp(url, "http://localhost:", 17));

    if (!strncmp(action, "watch:", 6) && ThHex64(action + 6)) {
        for (int i = 0; i < g_thLocalCount; i++)
            if (!strcmp(g_thLocal[i].id, action + 6)) { ThWatchLocal(action + 6, &s); return; }
        if (!service) { ThStatus(url[0] ? TH_SESSION : TH_MISSING); return; }
        if (!ThIdle(&s)) { ThStatus(TH_MENU_FIRST); return; }
        ThStatus(TH_DOWNLOADING);
        ThStart(url, action);
        return;
    }
    if (!strcmp(action, "next")) g_thPage++;
    else if (!strcmp(action, "previous")) g_thPage = g_thPage > 0 ? g_thPage - 1 : 0;
    else if (!strcmp(action, "filter:mp") || !strcmp(action, "filter:zm") || !strcmp(action, "filter:all")) {
        lstrcpynA(g_thFilter, action + 7, sizeof(g_thFilter));
        g_thPage = 0;
    } else if (strcmp(action, "refresh")) return;
    if (!service) { ThLocalCatalogue(); return; }
    ThStatus(TH_LOADING);
    ThStart(url, action);
}

static void InstallTheatre(void)
{
    static const BYTE kEscape[] = { 0x84, 0xC0, 0x74, 0x1B, 0x8B, 0x4D };
    static const BYTE kBroadcast[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0x20, 0x04, 0x00, 0x00,
                                       0xA1, 0x50, 0xDE, 0xCB, 0x00, 0x33, 0xC4, 0x89 };
    static const BYTE kClose[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x90, 0x01, 0x00, 0x00, 0xA1, 0x50, 0xDE, 0xCB, 0x00,
                                   0x33, 0xC5, 0x89, 0x45, 0xFC, 0x53 };
    static const BYTE kShow[] = { 0x50, 0x56, 0xE8, 0x29, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x08, 0x85, 0xC0, 0x74, 0x07,
                                  0x8B, 0x0E, 0x83, 0x4C, 0x88, 0x48, 0x04 };
    C1GameInit();
    if (!g_c1CbufOk || !g_c1DvarOk) {
        Log("theatre: Cbuf_AddText or Dvar_RegisterInt differs; THEATRE screen stays inert");
        return;
    }
    InitializeCriticalSection(&g_thLock);

    BYTE *esc = (BYTE *)(DWORD_PTR)(TH_ESCAPE_JZ_VA - 2);
    if (Readable(esc, sizeof(kEscape)) && !memcmp(esc, kEscape, sizeof(kEscape))) {
        BYTE jmp = 0xEB;
        WriteCode(esc + 2, &jmp, 1);
        Log("theatre: Escape opens the pause menu during a replay (0x%08X jz -> jmp)", TH_ESCAPE_JZ_VA);
    } else {
        Log("theatre: replay Escape branch 0x%08X differs; left alone", TH_ESCAPE_JZ_VA);
    }

    DWORD *slot = (DWORD *)(DWORD_PTR)TH_BROADCAST_SLOT_VA;
    const void *fn = (const void *)(DWORD_PTR)TH_BROADCAST_FN_VA;
    if (Readable(slot, 4) && *slot == TH_BROADCAST_FN_VA && Readable(fn, sizeof(kBroadcast)) &&
        !memcmp(fn, kBroadcast, sizeof(kBroadcast))) {
        g_thBroadcast = (ThBroadcastFn)fn;
        DWORD hook = (DWORD)(DWORD_PTR)ThBroadcastHook;
        WriteCode(slot, &hook, 4);
        Log("theatre: group-messaging broadcast slot 0x%08X takes the recording service session", TH_BROADCAST_SLOT_VA);
    } else {
        Log("theatre: broadcast slot 0x%08X differs; no recording service session from the lobby", TH_BROADCAST_SLOT_VA);
    }

    g_thMenusOk = Readable((void *)(DWORD_PTR)TH_MENU_CLOSE_VA, sizeof(kClose)) &&
                  !memcmp((void *)(DWORD_PTR)TH_MENU_CLOSE_VA, kClose, sizeof(kClose)) &&
                  Readable((void *)(DWORD_PTR)TH_MENU_SHOW_VA, sizeof(kShow)) &&
                  !memcmp((void *)(DWORD_PTR)TH_MENU_SHOW_VA, kShow, sizeof(kShow));
    if (!g_thMenusOk) Log("theatre: Menus_Close / show differ; Zombies replays keep their minimap menu");

    K33OnMainTick(TheatreTick);
    Log("theatre: THEATRE screen answered (recordings on this PC; codol_theatre_url for a recording service)");
}

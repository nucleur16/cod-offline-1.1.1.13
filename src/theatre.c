/*
 * theatre.c - the THEATRE screen's client side, ported from codol-classic's d3d9.dll (Theatre.cpp,
 * TheatreMessages.cpp, NativeConsole::HideReplayMinimap) and its exe patch. Included by kernel33.c after k33sha256.c
 * (K33Sha256Hex names the recordings).
 *
 * The shared front-end movie (voice_ui_mp: Main, MatchMPCTMenuView, MatchRoomListView, BottomStartGameView) is
 * codol-classic's THEATRE screen; the pause movie (controller_ingame: MainPause) is its replay pause menu. They talk
 * to the client only through string dvars:
 *   movie -> client  ui_theatreRequest  refresh | next | previous | filter:mp|zm|all | watch:<64 hex id> | return
 *   client -> movie  ui_theatreCount, ui_theatrePage, ui_theatrePages, ui_theatreTotal, ui_theatreFilter,
 *                    ui_theatreStatus, ui_theatreRevision (a change redraws),
 *                    per row i (0..9): ui_theatreId<i>, ui_theatreDate<i>, ui_theatreTitle<i>, ui_theatreMap<i>,
 *                    ui_theatreMode<i>, ui_theatreFile<i>, ui_theatreDuration<i>,
 *                    ui_theatrePlayback (1 while a recording plays: the pause menu becomes the replay menu),
 *                    ui_theatreOpen (1 reopens the THEATRE page after a recording ends)
 * This file answers those requests (main thread, every 200 ms, k33tick.c); every dvar write is a queued "set".
 *
 * Where recordings come from: the player's own folder, players\<profile>\demos\*.demo (OflDemoDir), and nowhere
 * else. theatre_record.c records every match played into it (C1 has no recorder of its own: CL_Init never adds the
 * "record" / "stoprecord" that CL_ShutdownOnceForAllClients removes at 0x4990AC / 0x4990E0, and the online films were
 * written by the servers). Each row's id is the SHA-256 of its file name; map, mode, length and start come from the
 * film's own metadata block (theatre_record.c). C1 opens a film it plays as demo/<name>.demo through its file
 * system; offline_lobby.c's CreateFileA wrapper (OflDemoRedirect) sends every such path to the same folder. The
 * online client's recording service (codol_theatre_url, the lobby's CODOL_THEATRE_SESSION_V1 broadcast token, the
 * catalogue and download worker) was removed: this client is offline only.
 *
 * Playing: "cl_demo_play <name>" from the main menu only (connection state, clc demo flags and the demo allocation
 * all idle), ui_theatrePlayback 1 until the client is idle again, then ui_theatreOpen 1. "return" (the
 * replay pause menu's Back) queues disconnectwithkeepsession. In a Zombies recording the stuck minimap_fullscreen
 * HUD menu is closed while it plays and shown again afterwards (C1 Menus_Close 0x5F9AE0 / show 0x5F9690).
 *
 * Exe patch (codol-classic shipped it as a one-byte change to the exe): 0x4933D9 jz -> jmp, so Escape opens the
 * native pause menu during a replay ("Allow native pause menu during replay").
 *
 * Replay weapon sync (ThWeaponSyncGuard): every frame of a replay the film player (0x72C4C0) points the viewer's
 * weapon selection at the recorded player's weapon, cl+0x344, which it takes from the newest snapshot it has parsed:
 * at 0x72C507 it calls CG's select-weapon function 0x483870 (eax = weapon) whenever cg.weaponSelect (cg+0xBF0A8)
 * differs. Weapons are numbered by the configstrings 5362 + n (CG_ConfigStringModified 0x4673C7 -> 0x483600 ->
 * BG_GetWeaponIndexForName 0x41F7A0, which appends to bg_weaponDefs 0x150D720, count at 0x6C3D28C). An offline match
 * creates a weapon for each player's class as it spawns ("*a <base> <attachments>" configstrings, weapons 120 on),
 * and the server sends that configstring in the same message as the first snapshot holding the weapon. The film
 * player reads that snapshot before CG has run the message's server commands, so for a moment the recorded weapon is
 * past the end of bg_weaponDefs; 0x483770 then reads NULL+0x34C (C1 crashes, "ShipRetail Crash": measured 6.8 s into
 * the mp_cha_quad film, where weapons 130/131 arrive). The guard lets the call through only for a registered weapon
 * (n <= count, defs[n] set) and otherwise skips it and the notify after it (resuming at 0x72C51B); CG registers the
 * weapon a few frames later and the next frame's sync selects it. Live play never runs this code.
 *
 * Effects with no FxEffectDef (ThFx*Guard): CG's effect table cgs.fxs[320] (0x16D9C88) is filled from configstrings
 * 3651 + n (DB_FindXAssetHeader type 0x1E, 0x4674E8); an index with no configstring stays NULL. An ET_FX entity
 * (0x442510 -> 0x4423D0) loads fxs[entity effect] at 0x44242D and spawns it (0x4FE430 or, bone-attached, 0x4FE550,
 * both -> 0x4FE0A0, which reads def+0x18). C1 skips effect index 0 itself, but only during a replay (0x442405..). A
 * replay that decodes an entity with an unregistered effect index crashed there (AV read 0x18 at 0x4FE0AD, a
 * killhouse TDM film about 35 s in, with smoke and grenades), as live play did before the server's flag-8 base rules
 * (third_party/sh1x .../ol_sv_snapshot.c, select_snapshot_base). Every effect a film's server uses is already in its
 * gamestate (no effect configstring changes mid-match in any recorded film), so the index is not one still on its
 * way: it is a stale or wrong entity state. So C1's own skip is widened: the ET_FX path treats an index >= 320 or a
 * NULL def like index 0 (resumes at 0x44241B, "no effect"), and the two spawn entries return 0 ("not spawned", what
 * they return with effects off) for a NULL def from any caller. A few skips are logged.
 */

#include <time.h>

#define TH_ESCAPE_JZ_VA      0x004933D9u   /* 74 1B -> EB 1B */
#define TH_CLC_PTR_VA        0x016F8068u
#define TH_CLC_KEY           0x26F950CCu
#define TH_DEMO_ALLOC_VA     0x06C267DCu
#define TH_UI_CONTEXT_VA     0x016CF898u
#define TH_MENU_CLOSE_VA     0x005F9AE0u
#define TH_MENU_SHOW_VA      0x005F9690u
#define TH_POLL_MS           200
#define TH_ROWS              10
#define TH_MAX_LOCAL         512

enum { TH_STARTING, TH_MENU_FIRST, TH_LOCAL, TH_LOCAL_NONE, TH_MISSING };
/* English, then Chinese in UTF-8 (the movies read dvars through Scaleform's Dvar_GetString, which takes UTF-8). */
static const char *const kThText[][2] = {
    { "Starting recording", "\xE6\xAD\xA3\xE5\x9C\xA8\xE6\x92\xAD\xE6\x94\xBE\xE5\xBD\x95\xE5\x83\x8F" }, /* 正在播放录像 */
    { "Return to the main menu before watching a recording",
      "\xE8\xAF\xB7\xE5\x85\x88\xE8\xBF\x94\xE5\x9B\x9E\xE4\xB8\xBB\xE8\x8F\x9C\xE5\x8D\x95\xE5\x86\x8D\xE8\xA7\x82\xE7\x9C\x8B\xE5\xBD\x95\xE5\x83\x8F" }, /* 请先返回主菜单再观看录像 */
    { "Your recorded matches", "\xE6\x9C\xAC\xE6\x9C\xBA\xE5\xBD\x95\xE5\x83\x8F" }, /* 本机录像 */
    { "No recorded matches yet", "\xE6\x9C\xAC\xE6\x9C\xBA\xE6\x9A\x82\xE6\x97\xA0\xE5\xBD\x95\xE5\x83\x8F" }, /* 本机暂无录像 */
    { "Recording file missing - refresh", "\xE5\xBD\x95\xE5\x83\x8F\xE6\x96\x87\xE4\xBB\xB6\xE4\xB8\x8D\xE5\xAD\x98\xE5\x9C\xA8\xEF\xBC\x8C\xE8\xAF\xB7\xE5\x88\xB7\xE6\x96\xB0" }, /* 录像文件不存在，请刷新 */
};

typedef struct ThRow {
    char id[65], file[64], date[32], map[64], mode[32];
    unsigned seconds, started;     /* started: time_t from the film's metadata (0 = unknown: the file time is shown) */
    FILETIME written;
} ThRow;

static BOOL g_thMenusOk;
static unsigned g_thSession, g_thRevision;

/* The main-thread Theatre flow owns this counter (it counted recording service sessions, which are gone, so it
 * stays 0). Zero and unsigned wrap are valid generations; expose the current value without interpreting it. */
__attribute__((unused)) static unsigned ThSessionGeneration(void)
{
    return g_thSession;
}

static char g_thFilter[8] = "all";
static int g_thPage;
static BOOL g_thWatching, g_thStarted, g_thReturning;
static char g_thPlayingZone[16];
static DWORD g_thSubmittedAt;
static char g_thQueue[192][200];         /* commands waiting for room in the command buffer */
static int g_thQueueHead, g_thQueueLen;
static ThRow g_thLocal[TH_ROWS];         /* the page of local rows shown (watch looks the id up here) */
static int g_thLocalCount;

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
    ThQueue("set ui_theatre%s \"%s\"\n", field, clean);
}

static void ThStatus(int id) { ThSet("Status", ThText(id)); }

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

/* ---- recording every match (theatre_record.c) ---------------------------------------------------------------- */

#include "theatre_record.c"

/* ---- the player's recordings ------------------------------------------------------------------------------------- */

static int ThDemoDir(WCHAR *dir) { return OflDemoDir(dir, FALSE); }

/* C1's demo envelope (C1 0x72BC80 / 0x72CB00 / 0x72CC90): a 32800-byte header whose first int is the tag (1..3),
 * the record stream, then metadata (640 fixed bytes: tag, start, end, gametype, map, ..., start time_t at +0x27C;
 * a table of at most 40 x 24 bytes; one of at most 300 x 8 bytes) and an 8-byte footer {metadata size, tag}. Only a
 * file with a consistent envelope is listed, so cl_demo_play never gets a file C1 would reject with
 * EXE_ERR_READING_DEMO (a structure check only: it does not prove the gameplay stream plays). With `r`, the row's
 * map, mode and start come from the metadata. */
static BOOL ThDemoEnvelope(const WCHAR *path, unsigned *seconds, ThRow *r)
{
    BOOL ok = FALSE;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER size;
    int head = 0, foot[2], count1 = 0, count2 = 0;
    static int fixed[0x280 / 4];
    DWORD got;
    if (GetFileSizeEx(f, &size) && size.QuadPart >= 32800 + 640 + 8 + 8 && size.QuadPart < 0x7FFFFFFF &&
        ReadFile(f, &head, 4, &got, NULL) && got == 4 &&
        SetFilePointer(f, (LONG)size.QuadPart - 8, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
        ReadFile(f, foot, 8, &got, NULL) && got == 8 && foot[1] >= 1 && foot[1] <= 3 && head == foot[1] &&
        foot[0] >= 640 + 8 && foot[0] <= (LONG)size.QuadPart - 32800 - 8) {
        LONG meta = (LONG)size.QuadPart - 8 - foot[0];
        if (SetFilePointer(f, meta, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
            ReadFile(f, fixed, sizeof(fixed), &got, NULL) && got == sizeof(fixed) && fixed[0] == foot[1] &&
            SetFilePointer(f, meta + 640, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
            ReadFile(f, &count1, 4, &got, NULL) && got == 4 && count1 >= 0 && count1 <= 40 &&
            640 + 8 + count1 * 24 <= foot[0] &&
            SetFilePointer(f, meta + 644 + count1 * 24, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
            ReadFile(f, &count2, 4, &got, NULL) && got == 4 && count2 >= 0 && count2 <= 300 &&
            640 + 8 + count1 * 24 + count2 * 8 <= foot[0]) {
            ok = TRUE;
            if (seconds && fixed[2] > fixed[1]) *seconds = (unsigned)(fixed[2] - fixed[1]) / 1000;
            if (r) {
                char text[64];
                lstrcpynA(text, (const char *)fixed + 0xC, sizeof(text));
                if (ThSafe(text)) lstrcpynA(r->mode, text, sizeof(r->mode));
                lstrcpynA(text, (const char *)fixed + 0x10C, sizeof(text));
                if (ThSafe(text)) lstrcpynA(r->map, text, sizeof(r->map));
                r->started = (unsigned)fixed[0x27C / 4];
            }
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
                if (!ThDemoEnvelope(full, &r->seconds, r)) continue;  /* not a C1 film: never offered */
                SYSTEMTIME st;
                FILETIME local, when = fd.ftLastWriteTime;
                if (r->started) {
                    ULONGLONG t = (ULONGLONG)r->started * 10000000ull + 116444736000000000ull;
                    when.dwLowDateTime = (DWORD)t;
                    when.dwHighDateTime = (DWORD)(t >> 32);
                    r->written = when;
                }
                FileTimeToLocalFileTime(&when, &local);
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
        _snprintf(field, sizeof(field), "Date%d", i); ThSet(field, r->date);
        _snprintf(field, sizeof(field), "Title%d", i); ThSet(field, r->date);
        _snprintf(field, sizeof(field), "Map%d", i); ThSet(field, r->map);
        _snprintf(field, sizeof(field), "Mode%d", i); ThSet(field, r->mode);
        _snprintf(field, sizeof(field), "Id%d", i); ThSet(field, r->id);
        _snprintf(field, sizeof(field), "File%d", i); ThSet(field, r->file);
        _snprintf(num, sizeof(num), "%u", r->seconds);
        _snprintf(field, sizeof(field), "Duration%d", i); ThSet(field, num);
    }
    _snprintf(num, sizeof(num), "%d", g_thLocalCount); ThSet("Count", num);
    ThSet("Filter", g_thFilter);
    _snprintf(num, sizeof(num), "%d", g_thPage + 1); ThSet("Page", num);
    _snprintf(num, sizeof(num), "%d", pages); ThSet("Pages", num);
    _snprintf(num, sizeof(num), "%d", count); ThSet("Total", num);
    _snprintf(num, sizeof(num), "%u", ++g_thRevision); ThSet("Revision", num);
    ThStatus(count ? TH_LOCAL : TH_LOCAL_NONE);
}

/* ---- main thread --------------------------------------------------------------------------------------------- */

static void ThWatchLocal(const char *id, const ThState *s)
{
    for (int i = 0; i < g_thLocalCount; i++) {
        const ThRow *r = &g_thLocal[i];
        if (strcmp(r->id, id)) continue;
        WCHAR p[MAX_PATH];
        int base = ThDemoDir(p);
        if (!base || base + 72 >= MAX_PATH) break;
        _snwprintf(p + base, MAX_PATH - base, L"%hs.demo", r->file);
        if (!ThDemoEnvelope(p, NULL, NULL)) break;
        if (!ThIdle(s)) { ThStatus(TH_MENU_FIRST); return; }
        ThSet("Playback", "1");
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

    if (!known) return;

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
            ThSet("Playback", "0");
            ThSet("Open", "1");
        }
    }
    if (g_thReturning) {
        if (ThIdle(&s)) {
            g_thReturning = g_thWatching = g_thStarted = FALSE;
            g_thPlayingZone[0] = 0;
            ThSet("Playback", "0");
            ThSet("Open", "1");
        }
        return;
    }
    char action[80];
    if (!C1DvarString("ui_theatreRequest", action, sizeof(action)) || !action[0]) return;
    ThSet("Request", "");
    if (!strcmp(action, "return")) {
        if (s.connection == 9 && s.demo == 1) {
            ThQueue("disconnectwithkeepsession\n");
            g_thReturning = TRUE;
        }
        return;
    }
    if (!strncmp(action, "watch:", 6) && ThHex64(action + 6)) {
        for (int i = 0; i < g_thLocalCount; i++)
            if (!strcmp(g_thLocal[i].id, action + 6)) { ThWatchLocal(action + 6, &s); return; }
        ThStatus(TH_MISSING);
        return;
    }
    if (!strcmp(action, "next")) g_thPage++;
    else if (!strcmp(action, "previous")) g_thPage = g_thPage > 0 ? g_thPage - 1 : 0;
    else if (!strcmp(action, "filter:mp") || !strcmp(action, "filter:zm") || !strcmp(action, "filter:all")) {
        lstrcpynA(g_thFilter, action + 7, sizeof(g_thFilter));
        g_thPage = 0;
    } else if (strcmp(action, "refresh")) return;
    ThLocalCatalogue();
}

/* ---- replay weapon sync guard (see the top of this file) -------------------------------------------------------- */

#define TH_SYNC_CALL_VA      0x0072C507u   /* push esi / call 0x483870 / add esp, 4 / mov eax, 1 */
#define TH_SYNC_SELECT_VA    0x00483870u
#define TH_SYNC_RESUME_VA    0x0072C51Bu   /* after the notify call: mov ecx, [0x6C267DC] */
#define TH_WEAPON_COUNT_VA   0x06C3D28Cu

DWORD g_thSyncSelect = TH_SYNC_SELECT_VA, g_thSyncResume = TH_SYNC_RESUME_VA;
static volatile LONG g_thSyncSkips;

__attribute__((used)) void __cdecl ThWeaponSyncSkipped(DWORD weapon)
{
    if (InterlockedIncrement(&g_thSyncSkips) <= 3)
        Log("theatre: replay weapon %lu is not registered yet (%lu weapons); its selection waits a frame",
            (unsigned long)weapon, (unsigned long)*(volatile DWORD *)(DWORD_PTR)TH_WEAPON_COUNT_VA);
}

/* Replaces the call at 0x72C507: [esp] = 0x72C50C, [esp+4] = the pushed esi, eax = the weapon. A registered weapon goes
 * on to 0x483870 with the stack untouched; any other drops both words and resumes at 0x72C51B (ebx, esi, edi kept). */
void ThWeaponSyncGuard(void);
__asm__(
    ".text\n"
    ".globl _ThWeaponSyncGuard\n"
    "_ThWeaponSyncGuard:\n"
    "    cmpl $700, %eax\n"
    "    jae 1f\n"
    "    cmpl 0x06C3D28C, %eax\n"
    "    ja 1f\n"
    "    cmpl $0, 0x0150D720(,%eax,4)\n"
    "    je 1f\n"
    "    jmp *_g_thSyncSelect\n"
    "1:\n"
    "    pushal\n"
    "    pushfl\n"
    "    pushl %eax\n"
    "    call _ThWeaponSyncSkipped\n"
    "    addl $4, %esp\n"
    "    popfl\n"
    "    popal\n"
    "    addl $8, %esp\n"
    "    jmp *_g_thSyncResume\n");

static void InstallReplayWeaponGuard(void)
{
    /* 56 / E8 rel32 (0x483870) / 83 C4 04 / B8 01 00 00 00 ... 0x72C51B: 8B 0D DC 67 C2 06 */
    static const BYTE kAfter[] = { 0x83, 0xC4, 0x04, 0xB8, 0x01, 0x00, 0x00, 0x00 };
    static const BYTE kResume[] = { 0x8B, 0x0D, 0xDC, 0x67, 0xC2, 0x06 };
    BYTE *site = (BYTE *)(DWORD_PTR)TH_SYNC_CALL_VA;
    if (!Readable(site - 1, 14) || site[-1] != 0x56 || site[0] != 0xE8 ||
        (DWORD)(TH_SYNC_CALL_VA + 5 + *(const LONG *)(site + 1)) != TH_SYNC_SELECT_VA ||
        memcmp(site + 5, kAfter, sizeof(kAfter)) || !Readable((void *)(DWORD_PTR)TH_SYNC_RESUME_VA, sizeof(kResume)) ||
        memcmp((void *)(DWORD_PTR)TH_SYNC_RESUME_VA, kResume, sizeof(kResume))) {
        Log("theatre: the replay weapon sync at 0x%08X differs; left alone", TH_SYNC_CALL_VA);
        return;
    }
    LONG rel = (LONG)((DWORD_PTR)ThWeaponSyncGuard - (TH_SYNC_CALL_VA + 5));
    WriteCode(site + 1, &rel, 4);
    Log("theatre: a replay selects the recorded weapon only once it is registered (0x%08X)", TH_SYNC_CALL_VA);
}

/* ---- effects with no FxEffectDef (see the top of this file) ----------------------------------------------------- */

#define TH_FX_LOAD_VA        0x0044242Du   /* ET_FX: mov esi, [ecx*4 + 0x16D9C88] (ecx = the entity's effect) */
#define TH_FX_LOADED_VA      0x00442434u
#define TH_FX_NONE_VA        0x0044241Bu   /* C1's own "no effect" return: xor eax, eax / pop esi / pop ebx ... */
#define TH_FX_SPAWN_VA       0x004FE0A0u   /* FX spawn core (system, params; params[0] = the def) */
#define TH_FX_ATTACH_VA      0x004FE550u   /* bone-attached spawn, esi = the def */
#define TH_FX_TABLE_VA       0x016D9C88u
#define TH_FX_MAX            320           /* configstrings 3651..3970; 0x44E950 clears 0x500 bytes */

DWORD g_thFxLoaded = TH_FX_LOADED_VA, g_thFxNone = TH_FX_NONE_VA;
DWORD g_thFxSpawnBody = TH_FX_SPAWN_VA + 6, g_thFxAttachBody = TH_FX_ATTACH_VA + 9;
static volatile LONG g_thFxSkips[3];

__attribute__((used)) void __cdecl ThFxSkipped(DWORD kind, DWORD value, DWORD from)
{
    static const char *const kWhere[] = { "an effect entity", "an effect spawn", "a bone-attached effect" };
    if (kind > 2 || InterlockedIncrement(&g_thFxSkips[kind]) > 3) return;
    int registered = 0;
    for (int i = 0; i < TH_FX_MAX; i++)
        if (*(volatile DWORD *)(DWORD_PTR)(TH_FX_TABLE_VA + i * 4)) registered++;
    Log("theatre: %s with no effect def was skipped (%s %lu, from 0x%08lX; %d of %d effects registered)", kWhere[kind],
        kind ? "def" : "index", (unsigned long)value, (unsigned long)from, registered, TH_FX_MAX);
}

void ThFxIndexGuard(void), ThFxSpawnGuard(void), ThFxAttachGuard(void);
__asm__(
    ".text\n"
    /* at 0x44242D (jmp, nop nop): edi = the centity, ecx = its effect, ebx/esi pushed by 0x4423D0 */
    ".globl _ThFxIndexGuard\n"
    "_ThFxIndexGuard:\n"
    "    cmpl $320, %ecx\n"
    "    jae 1f\n"
    "    movl 0x016D9C88(,%ecx,4), %esi\n"
    "    testl %esi, %esi\n"
    "    je 1f\n"
    "    jmp *_g_thFxLoaded\n"
    "1:\n"
    "    pushal\n"
    "    pushfl\n"
    "    pushl $0x00442542\n"
    "    pushl %ecx\n"
    "    pushl $0\n"
    "    call _ThFxSkipped\n"
    "    addl $12, %esp\n"
    "    popfl\n"
    "    popal\n"
    "    jmp *_g_thFxNone\n"
    /* at 0x4FE0A0 (jmp, nop): [esp] = return, [esp+8] = params; eax is not an input */
    ".globl _ThFxSpawnGuard\n"
    "_ThFxSpawnGuard:\n"
    "    movl 8(%esp), %eax\n"
    "    testl %eax, %eax\n"
    "    je 2f\n"
    "    cmpl $0, (%eax)\n"
    "    je 2f\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    subl $0x14, %esp\n"
    "    jmp *_g_thFxSpawnBody\n"
    "2:\n"
    "    pushal\n"
    "    pushfl\n"
    "    pushl 36(%esp)\n"
    "    pushl $0\n"
    "    pushl $1\n"
    "    call _ThFxSkipped\n"
    "    addl $12, %esp\n"
    "    popfl\n"
    "    popal\n"
    "    xorl %eax, %eax\n"
    "    ret\n"
    /* at 0x4FE550 (jmp, 4 x nop): esi = the def */
    ".globl _ThFxAttachGuard\n"
    "_ThFxAttachGuard:\n"
    "    testl %esi, %esi\n"
    "    je 3f\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    subl $0x9C, %esp\n"
    "    jmp *_g_thFxAttachBody\n"
    "3:\n"
    "    pushal\n"
    "    pushfl\n"
    "    pushl 36(%esp)\n"
    "    pushl $0\n"
    "    pushl $2\n"
    "    call _ThFxSkipped\n"
    "    addl $12, %esp\n"
    "    popfl\n"
    "    popal\n"
    "    xorl %eax, %eax\n"
    "    ret\n");

/* jmp rel32 to `to` at `va`, padded with nops to `len` bytes, if the `len` bytes there are still `expect`. */
static BOOL ThDetour(DWORD va, const BYTE *expect, int len, void (*to)(void), const char *what)
{
    BYTE *site = (BYTE *)(DWORD_PTR)va, code[16];
    if (len < 5 || len > (int)sizeof(code) || !Readable(site, (SIZE_T)len) || memcmp(site, expect, (size_t)len)) {
        Log("theatre: %s at 0x%08lX differs; left alone", what, (unsigned long)va);
        return FALSE;
    }
    code[0] = 0xE9;
    *(LONG *)(code + 1) = (LONG)((DWORD_PTR)to - (va + 5));
    memset(code + 5, 0x90, (size_t)len - 5);
    WriteCode(site, code, (SIZE_T)len);
    return TRUE;
}

static void InstallReplayFxGuard(void)
{
    static const BYTE kLoad[] = { 0x8B, 0x34, 0x8D, 0x88, 0x9C, 0x6D, 0x01 };
    static const BYTE kNone[] = { 0x33, 0xC0, 0x5E, 0x5B };
    static const BYTE kSpawn[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14 };
    static const BYTE kSpawnNext[] = { 0x56, 0x57, 0x8B, 0x7D, 0x0C };
    static const BYTE kAttach[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x9C, 0x00, 0x00, 0x00 };
    static const BYTE kAttachNext[] = { 0xA1, 0x50, 0xDE, 0xCB, 0x00 };
    if (!Readable((void *)(DWORD_PTR)TH_FX_NONE_VA, sizeof(kNone)) ||
        memcmp((void *)(DWORD_PTR)TH_FX_NONE_VA, kNone, sizeof(kNone)) ||
        !Readable((void *)(DWORD_PTR)(TH_FX_SPAWN_VA + 6), sizeof(kSpawnNext)) ||
        memcmp((void *)(DWORD_PTR)(TH_FX_SPAWN_VA + 6), kSpawnNext, sizeof(kSpawnNext)) ||
        !Readable((void *)(DWORD_PTR)(TH_FX_ATTACH_VA + 9), sizeof(kAttachNext)) ||
        memcmp((void *)(DWORD_PTR)(TH_FX_ATTACH_VA + 9), kAttachNext, sizeof(kAttachNext))) {
        Log("theatre: C1's effect spawn differs; effects with no def are left alone");
        return;
    }
    int n = ThDetour(TH_FX_LOAD_VA, kLoad, sizeof(kLoad), ThFxIndexGuard, "the effect entity's def load") +
            ThDetour(TH_FX_SPAWN_VA, kSpawn, sizeof(kSpawn), ThFxSpawnGuard, "the effect spawn") +
            ThDetour(TH_FX_ATTACH_VA, kAttach, sizeof(kAttach), ThFxAttachGuard, "the bone-attached effect spawn");
    Log("theatre: effects with no def are skipped (%d of 3 sites)", n);
}

static void InstallTheatre(void)
{
    static const BYTE kEscape[] = { 0x84, 0xC0, 0x74, 0x1B, 0x8B, 0x4D };
    static const BYTE kClose[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x90, 0x01, 0x00, 0x00, 0xA1, 0x50, 0xDE, 0xCB, 0x00,
                                   0x33, 0xC5, 0x89, 0x45, 0xFC, 0x53 };
    static const BYTE kShow[] = { 0x50, 0x56, 0xE8, 0x29, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x08, 0x85, 0xC0, 0x74, 0x07,
                                  0x8B, 0x0E, 0x83, 0x4C, 0x88, 0x48, 0x04 };
    C1GameInit();
    InstallTheatreRecorder();
    InstallReplayWeaponGuard();
    InstallReplayFxGuard();
    if (!g_c1CbufOk || !g_c1DvarOk) {
        Log("theatre: Cbuf_AddText or Dvar_RegisterInt differs; THEATRE screen stays inert");
        return;
    }

    BYTE *esc = (BYTE *)(DWORD_PTR)(TH_ESCAPE_JZ_VA - 2);
    if (Readable(esc, sizeof(kEscape)) && !memcmp(esc, kEscape, sizeof(kEscape))) {
        BYTE jmp = 0xEB;
        WriteCode(esc + 2, &jmp, 1);
        Log("theatre: Escape opens the pause menu during a replay (0x%08X jz -> jmp)", TH_ESCAPE_JZ_VA);
    } else {
        Log("theatre: replay Escape branch 0x%08X differs; left alone", TH_ESCAPE_JZ_VA);
    }

    g_thMenusOk = Readable((void *)(DWORD_PTR)TH_MENU_CLOSE_VA, sizeof(kClose)) &&
                  !memcmp((void *)(DWORD_PTR)TH_MENU_CLOSE_VA, kClose, sizeof(kClose)) &&
                  Readable((void *)(DWORD_PTR)TH_MENU_SHOW_VA, sizeof(kShow)) &&
                  !memcmp((void *)(DWORD_PTR)TH_MENU_SHOW_VA, kShow, sizeof(kShow));
    if (!g_thMenusOk) Log("theatre: Menus_Close / show differ; Zombies replays keep their minimap menu");

    K33OnMainTick(TheatreTick);
    Log("theatre: THEATRE screen answered (the player's recordings)");
}

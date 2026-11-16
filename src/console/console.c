/*
 * Developer console. Included by kernel33.c after parity.c, so it shares Log/Readable/
 * WriteCode/CallsTarget/RedirectCall and the parity logger's helpers.
 *
 * In this offline fork every build has it (docs/OFFLINE_CONSOLE_20261112.md): the shipping profile too, where
 * it was absent while the gate was K33_DIAGNOSTICS. What stays diagnostics-only is marked
 * `#if K33_DIAGNOSTICS` here: the k33shot built-in, k33shot.c's Present callbacks and the rcon to a server
 * other than the one this client hosts. A build without the console is -DK33_NO_CONSOLE.
 *
 * C1 has no console of its own any more. IW4's Con_Init (C1 0x489090) still registers "864"
 * (monkeytoy) and "865" (con_matchPrefixOnly) and the chat fields, but g_consoleField, the
 * "toggleconsole", "clear" and "condump" commands are gone, and CL_KeyEvent (C1 0x493240) no longer
 * tests '`'/'~' (IW4 0x4F64D0/0x4F65BD -> Con_ToggleConsole); only the chat/message-mode key paths
 * remain. Con_DrawConsole and the console font registration ("fonts/consoleFont" has no reference)
 * are gone too. So this is an overlay, not a re-enable:
 *
 *   input   the window class's WndProc (WNDCLASS.lpfnWndProc = 0x633590, stored at 0x62FB85 by
 *           0x62FB60) becomes ConsoleWndProc, which owns the keyboard while the console is open
 *           and passes everything else to the game's WndProc untouched. The toggle is the key left
 *           of 1 (scan code 0x29, '`'/'~' on US layouts) or the virtual key in dvar k33_consoleKey
 *           (default F1, 0 = none). Key-up messages always reach the game, so nothing held when
 *           the console opens stays down. Typed characters arrive as WM_CHAR in the code page of
 *           the active input language (the window is ANSI; a DBCS character comes as one WM_CHAR,
 *           lead byte low), not the system code page, so they are converted from that code page
 *           to GBK. The window's IME context is detached while the console is open: an East Asian
 *           IME (Microsoft Pinyin defaults to Chinese mode) otherwise turns every letter into a
 *           composition whose window is invisible over a fullscreen game, and nothing reaches the
 *           line. It is put back on close; paste (Ctrl+V) still takes Chinese text as GBK.
 *   output  every line C1 writes to its log: the four calls of Com_LogPrintMessage 0x594010
 *           (esi = text; from Com_PrintMessage 0x5941A4, Com_Printf 0x5942CE, 0x5944A2 and
 *           0x59B96E) get a pre-hook that copies the text into a 4096-line scrollback.
 *   drawing Direct3DCreate9's import slot (0xB42598) returns the real IDirect3D9 with CreateDevice
 *           hooked in its vtable; the device's Present (and swap chain 0's) draws the console on top
 *           of the finished frame (Scaleform menus included): GDI renders the panel into a DIB,
 *           which is uploaded to a managed texture and drawn as one quad inside a D3DSBT_ALL state
 *           block, so the renderer's own state is restored exactly.
 *   execute Cbuf_AddText 0x58BA60 (ecx = localClient, eax = text), as CL_KeyEvent's binds do.
 *           Readable dvar names (c1_dvars.h) are rewritten to C1's numeric ones first.
 *   lists   commands: the cmd_function_t list at 0x24F6424 {next, name, function} (Cmd_FindCommand
 *           0x58C790); dvars: the sorted dvar_t* array at 0x2AB98E0, count 0x2AB98C4, under the dvar
 *           reader count 0x2AB98D0 / writer flag 0x2AB98D4 (Dvar_ForEach 0x621710). dvar_t: name +0, flags +4, type byte +8,
 *           current +0xC, reset +0x2C, domain +0x3C/+0x40 (enum: count, strings).
 *
 * Every site is byte-checked against C1.exe; a mismatch is logged and that part stays off.
 */

#define COBJMACROS
#include <ctype.h>
#include <math.h>
#include <d3d9.h>
#include "../c1_dvars.h"

#define CON_LOGPRINT_VA       0x00594010u
/* Com_Printf, Cbuf_AddText, the command list, Dvar_RegisterInt and the dvar count: c1game.c */
#define CON_DVAR_FOREACH_VA   0x00621710u
#define CON_DVAR_ARRAY_VA     0x02AB98E0u
#define CON_DVAR_LOCK_VA      0x02AB98D0u
#define CON_DVAR_BUSY_VA      0x02AB98D4u
#define CON_WNDCLASS_VA       0x0062FB85u  /* mov dword [ebp-28h], 0x633590 */
#define CON_D3DCREATE_SLOT    0x00B42598u
#define CON_CHANNEL           8            /* logfile_only: the log and this console, not the HUD */

#define CON_LINES     4096
#define CON_LINE_LEN  320
#define CON_INPUT_LEN 256
#define CON_HISTORY   200
#define CON_SUGGEST   256
#define CON_SHOWN     24   /* IW4 con_inputMaxMatchesShown */
#define CON_RESULTS   12

/* ---- state ------------------------------------------------------------------------------------ */

static CRITICAL_SECTION g_conLock, g_conUiLock;
static HANDLE g_conWorkEvent;
static volatile LONG g_conHistoryDirty;
static volatile LONG g_conRequestedW, g_conRequestedH;
static DWORD WINAPI ConRasterWorker(void *unused);
static BOOL g_conReady, g_conCaptureOk, g_conDvarsOk, g_conDrawOk;
static volatile LONG g_conOpen, g_conDirty = 1;
static char g_conLines[CON_LINES][CON_LINE_LEN];
static int g_conHead, g_conCount;
static char g_conPartial[CON_LINE_LEN];
static int g_conPartialLen;
static int g_conScroll;
static volatile LONG g_conRows = 20;
static volatile LONG g_conBlink = 1;
static volatile LONG g_conShot; /* "k33shot": save the next rendered overlay to main\k33_console.bmp */
static volatile LONG g_conExpanded;   /* Shift+Tab / Shift+~: the full log; collapsed by default */
static char g_res[CON_RESULTS][CON_LINE_LEN]; /* what the last command printed */
static int g_resCount;
static DWORD g_resUntil;

static char g_conInput[CON_INPUT_LEN];
static int g_conInputLen, g_conCursor;
static char g_conHist[CON_HISTORY][CON_INPUT_LEN];
static int g_conHistCount, g_conHistPos;
static BOOL g_conHistLoaded, g_conEatChar, g_conBanner;
static const BYTE *volatile g_conKeyDvar;
static BOOL g_conKeyDvarTried;
static BYTE g_conMb[4];       /* WM_CHAR bytes of a character still incomplete (UTF-8, split DBCS) */
static int g_conMbLen;
static HWND g_conImeHwnd;     /* window whose IME context the open console detached */
static HIMC g_conImeSaved;

typedef struct {
    char name[80];   /* what Tab inserts */
    char value[72];
    char info[100];
    BYTE kind;       /* 0 dvar, 1 command, 2 sh1x developer command or argument */
    BYTE prefix;
} ConSuggest;
static ConSuggest g_sugg[CON_SUGGEST];
static int g_suggCount, g_suggTotal, g_suggSel = -1;
static BOOL g_suggCycling;
static int g_suggTokStart, g_suggTokEnd;
static char g_detail[5][200];
static int g_detailCount;

static int ConIsDigits(const char *s)
{
    if (!*s) return 0;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return 0;
    return 1;
}

/* ---- the name table ----------------------------------------------------------------------------- */

static unsigned short g_byName[C1_DVAR_NAME_COUNT];

static int ConCmpName(const void *a, const void *b)
{
    return _stricmp(kC1DvarNames[*(const unsigned short *)a].name, kC1DvarNames[*(const unsigned short *)b].name);
}

static void ConInitNames(void)
{
    for (unsigned i = 0; i < C1_DVAR_NAME_COUNT; i++) g_byName[i] = (unsigned short)i;
    qsort(g_byName, C1_DVAR_NAME_COUNT, sizeof(g_byName[0]), ConCmpName);
}

/* kC1DvarNames is sorted by numeric id. */
static const C1DvarName *DvarNameById(const char *id)
{
    if (!ConIsDigits(id) || strlen(id) > 6) return NULL;
    int want = atoi(id), lo = 0, hi = (int)C1_DVAR_NAME_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, v = atoi(kC1DvarNames[mid].id);
        if (v == want) return strcmp(kC1DvarNames[mid].id, id) == 0 ? &kC1DvarNames[mid] : NULL;
        if (v < want) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static const C1DvarName *DvarNameByName(const char *name)
{
    int lo = 0, hi = (int)C1_DVAR_NAME_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = _stricmp(kC1DvarNames[g_byName[mid]].name, name);
        if (!c) return &kC1DvarNames[g_byName[mid]];
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* ---- scrollback (any thread) ---------------------------------------------------------------------- */

/* '591' / "591" -> '591' (cg_drawFPS), so the game's own dvar messages read. */
static void ConAnnotate(const char *in, char *out, int outSize)
{
    int o = 0;
    for (const char *p = in; *p && o < outSize - 1;) {
        if ((*p == '"' || *p == '\'') && p[1] >= '0' && p[1] <= '9') {
            const char *e = p + 1;
            while (*e >= '0' && *e <= '9') e++;
            if (*e == *p && e - p - 1 <= 5) {
                char id[8];
                memcpy(id, p + 1, e - p - 1);
                id[e - p - 1] = 0;
                const C1DvarName *n = DvarNameById(id);
                int len = (int)(e - p + 1);
                if (o + len >= outSize - 1) break;
                memcpy(out + o, p, len);
                o += len;
                p = e + 1;
                if (n) o += _snprintf(out + o, outSize - 1 - o > 0 ? outSize - 1 - o : 0, " (%s)", n->name);
                if (o > outSize - 1) o = outSize - 1;
                continue;
            }
        }
        out[o++] = *p++;
    }
    out[o] = 0;
}

static void ConPushLine(const char *line)
{
    char *dst = g_conLines[(g_conHead + g_conCount) % CON_LINES];
    ConAnnotate(line, dst, CON_LINE_LEN);
    if (g_conCount < CON_LINES) g_conCount++;
    else g_conHead = (g_conHead + 1) % CON_LINES;
    if (g_resUntil && (LONG)(GetTickCount() - g_resUntil) < 0 && strncmp(dst, "[DWLOG]", 7) && strncmp(dst, "[TLOG", 5)) {
        if (g_resCount == CON_RESULTS) {
            memmove(g_res[0], g_res[1], sizeof(g_res[0]) * (CON_RESULTS - 1));
            g_resCount--;
        }
        lstrcpynA(g_res[g_resCount++], dst, CON_LINE_LEN);
    }
    if (g_conScroll) g_conScroll++; /* keep the view where it is while scrolled up */
}

static void ConAppend(const char *text)
{
    if (!g_conReady || !text) return;
    EnterCriticalSection(&g_conLock);
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            g_conPartial[g_conPartialLen] = 0;
            ConPushLine(g_conPartial);
            g_conPartialLen = 0;
        } else if (*p != '\r' && g_conPartialLen < CON_LINE_LEN - 1) {
            g_conPartial[g_conPartialLen++] = *p;
        }
    }
    BOOL visible = g_conExpanded || (g_resUntil && (LONG)(GetTickCount() - g_resUntil) < 0);
    LeaveCriticalSection(&g_conLock);
    if (visible) InterlockedExchange(&g_conDirty, 1);
}

__attribute__((used)) void __cdecl ConCapture(const char *text)
{
    ConAppend(text);
}

/* Main thread: print through Com_Printf so it lands in console_mp.log too. */
static void ConPrintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void ConPrintf(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    if (g_conCaptureOk)
        C1Print(CON_CHANNEL, buf);
    else
        ConAppend(buf);
}

/* ---- C1 access (main thread) --------------------------------------------------------------------- */

static void ConCbufAddText(const char *text) { C1CbufAddText(text); }

/* The dvar table's reader lock (Dvar_FindVar 0x6232C0, Dvar_ForEach 0x621710): readers
 * InterlockedIncrement the count at 0x2AB98D0, then Sleep(1) while the writer flag at 0x2AB98D4 is
 * set; InterlockedDecrement releases. */
static void ConDvarLock(void)
{
    InterlockedIncrement((volatile LONG *)(DWORD_PTR)CON_DVAR_LOCK_VA);
    while (*(volatile LONG *)(DWORD_PTR)CON_DVAR_BUSY_VA) Sleep(1);
}

static void ConDvarUnlock(void)
{
    InterlockedDecrement((volatile LONG *)(DWORD_PTR)CON_DVAR_LOCK_VA);
}

#define CON_MAX_DVARS 8192
static const BYTE *g_dvarSnap[CON_MAX_DVARS];

static int ConSnapshotDvars(void)
{
    if (!g_conDvarsOk) return 0;
    ConDvarLock();
    int n = *(volatile int *)(DWORD_PTR)C1_DVAR_COUNT_VA;
    if (n < 0) n = 0;
    if (n > CON_MAX_DVARS) n = CON_MAX_DVARS;
    memcpy(g_dvarSnap, (const void *)(DWORD_PTR)CON_DVAR_ARRAY_VA, n * sizeof(g_dvarSnap[0]));
    ConDvarUnlock();
    int k = 0;
    for (int i = 0; i < n; i++)
        if (g_dvarSnap[i] && Readable(g_dvarSnap[i], 0x48) && Readable(*(const char *const *)g_dvarSnap[i], 1))
            g_dvarSnap[k++] = g_dvarSnap[i];
    return k;
}

static const char *ConDvarRawName(const BYTE *d) { return *(const char *const *)d; }

static const char *ConDvarShownName(const BYTE *d)
{
    const char *raw = ConDvarRawName(d);
    const C1DvarName *n = DvarNameById(raw);
    return n ? n->name : raw;
}

static const char *const kTypeNames[] = { "bool", "float", "vec2", "vec3", "vec4", "int", "enum", "string",
                                          "color", "rgb" };

static void ConDvarValue(const BYTE *d, int off, char *buf, int n)
{
    const BYTE *v = d + off;
    int type = d[8];
    const float *f = (const float *)v;
    buf[0] = 0;
    switch (type) {
    case 0: _snprintf(buf, n, "%d", v[0] ? 1 : 0); break;
    case 1: _snprintf(buf, n, "%g", f[0]); break;
    case 2: _snprintf(buf, n, "%g %g", f[0], f[1]); break;
    case 3: case 9: _snprintf(buf, n, "%g %g %g", f[0], f[1], f[2]); break;
    case 4: _snprintf(buf, n, "%g %g %g %g", f[0], f[1], f[2], f[3]); break;
    case 5: _snprintf(buf, n, "%d", *(const int *)v); break;
    case 6: {
        int idx = *(const int *)v, cnt = *(const int *)(d + 0x3C);
        const char *const *s = *(const char *const *const *)(d + 0x40);
        if (idx >= 0 && idx < cnt && s && Readable(s + idx, 4) && Readable(s[idx], 1)) _snprintf(buf, n, "%s", s[idx]);
        else _snprintf(buf, n, "%d", idx);
        break;
    }
    case 7: {
        const char *s = *(const char *const *)v;
        _snprintf(buf, n, "%s", s && Readable(s, 1) ? s : "");
        break;
    }
    case 8: _snprintf(buf, n, "%g %g %g %g", v[0] / 255.0, v[1] / 255.0, v[2] / 255.0, v[3] / 255.0); break;
    }
    buf[n - 1] = 0;
}

static void ConDvarDomain(const BYTE *d, char *buf, int n)
{
    int type = d[8];
    buf[0] = 0;
    switch (type) {
    case 0: _snprintf(buf, n, "0/1"); break;
    case 1: case 2: case 3: case 4: case 9: {
        float lo = *(const float *)(d + 0x3C), hi = *(const float *)(d + 0x40);
        if (lo <= -3.0e38f && hi >= 3.0e38f) _snprintf(buf, n, "any");
        else if (hi >= 3.0e38f) _snprintf(buf, n, "%g..", lo);
        else _snprintf(buf, n, "%g..%g", lo, hi);
        break;
    }
    case 5: {
        int lo = *(const int *)(d + 0x3C), hi = *(const int *)(d + 0x40);
        if (lo == (int)0x80000000 && hi == 0x7FFFFFFF) _snprintf(buf, n, "any");
        else _snprintf(buf, n, "%d..%d", lo, hi);
        break;
    }
    case 6: {
        int cnt = *(const int *)(d + 0x3C), o = 0;
        const char *const *s = *(const char *const *const *)(d + 0x40);
        for (int i = 0; s && i < cnt && i < 32 && o < n - 8; i++) {
            if (!Readable(s + i, 4) || !Readable(s[i], 1)) break;
            o += _snprintf(buf + o, n - o, "%s%s", i ? "|" : "", s[i]);
            if (o < 0) { o = n; break; }
        }
        break;
    }
    case 7: _snprintf(buf, n, "text"); break;
    case 8: _snprintf(buf, n, "r g b a 0..1"); break;
    }
    buf[n - 1] = 0;
}

static const BYTE *ConFindDvar(const char *name, int count)
{
    for (int i = 0; i < count; i++)
        if (_stricmp(ConDvarRawName(g_dvarSnap[i]), name) == 0) return g_dvarSnap[i];
    const C1DvarName *n = DvarNameByName(name);
    if (n)
        for (int i = 0; i < count; i++)
            if (strcmp(ConDvarRawName(g_dvarSnap[i]), n->id) == 0) return g_dvarSnap[i];
    return NULL;
}

typedef struct ConCmd { struct ConCmd *next; const char *name; void *fn; } ConCmd;

/* Commands the console handles itself (they also cover the IW4 ones C1 dropped). */
static const char *const kBuiltins[] = { "clear", "cmdlist", "condump", "dvarlist", "find", "k33help",
#if K33_DIAGNOSTICS
                                         "k33shot", /* the console frame as a BMP: a test aid, not shipped */
#endif
};
#define BUILTIN_COUNT (sizeof(kBuiltins) / sizeof(kBuiltins[0]))

static BOOL ConIsSetCommand(const char *t)
{
    return !_stricmp(t, "set") || !_stricmp(t, "seta") || !_stricmp(t, "reset") || !_stricmp(t, "toggle") ||
           !_stricmp(t, "togglep") || !_stricmp(t, "setfromdvar") || !_stricmp(t, "sets") || !_stricmp(t, "setu");
}

/* ---- sh1x developer commands and the server route ------------------------------------------------------
 *
 * The cheat commands live on the server (sh1x codol_dev_cheats.c, g_cmds.c). C1 has none of them, so
 * Cmd_ExecuteString (0x58CB16) hands an unknown word to CL_ForwardCommandToServer 0x494CE0, which
 * sends it as a reliable client command while connected (connection state >= 5): typing "noclip" here
 * reaches the server as THIS player's command, which is what a cheat needs (the server acts on the
 * client that sent it). This table gives them autocomplete, argument hints and help. It is the server's
 * set: tests/test_offline_console.py compares the names and usage strings with the pinned server source
 * (sh1x_cmds.h kSh1xCheats, generated from it), so neither side can gain or lose a command unnoticed.
 * The server decides whether a cheat runs: sv_cheats must be on (the server command "dev_cheats on",
 * host only) and the server must not be a lobby match server: typed here, "dev_cheats on" first sets
 * the server's own devServer 1 (ConSvCheatsOn, console_server.c).
 *
 * Server console commands and dvars go to the server this client hosts: "sv <command>" (alias "rcon"),
 * or the bare name when C1 has no command or dvar of that name. See console_server.c for the route and
 * ConExecute for the rules. Nothing but the hosted server is ever contacted (a diagnostics build keeps the
 * older rcon to another local server: ConRconLegacy). */

typedef struct { const char *name, *usage, *help; BYTE builtin; } ConDevCmd;
static const ConDevCmd kDevCmds[] = {
    { "god", "god", "take no damage", 0 },
    { "demigod", "demigod", "damage never takes the last health point", 0 },
    { "notarget", "notarget", "bots, actors and turrets ignore you", 0 },
    { "noclip", "noclip", "fly through walls (C1 pm_type 2; server and prediction run the same free move)", 0 },
    { "ufo", "ufo", "fly, 6x speed (C1 pm_type 3)", 0 },
    { "kill", "kill", "die now (respawn as normal)", 0 },
    { "give", "give <weapon [model]|health [n]|ammo|all>", "give a weapon (engine or *composed name) and switch to it, or refill", 0 },
    { "take", "take <weapon|weapons|health [n]|ammo|all>", "remove a weapon, all weapons, ammo or health", 0 },
    { "setviewpos", "setviewpos x y z [yaw [pitch]]", "teleport", 0 },
    { "viewpos", "viewpos", "print your position and angles as a setviewpos line", 0 },
    { "devset", "devset <dvar> <value>", "server movement/bot dvar: jump_height g_speed g_gravity timescale ...", 0 },
    { "addbot", "addbot [n]", "add training bots (scr_codol_bots)", 0 },
    { "kickbot", "kickbot [n|all]", "remove training bots", 0 },
    { "devhelp", "devhelp", "the server's list and whether cheats are on", 0 },
    { "sv", "sv <server command or dvar ...>", "console built-in: run a line on the match server this client hosts; its output prints here", 1 },
    { "rcon", "rcon <server command or dvar ...>", "console built-in: the same as sv (the name IW consoles use)", 1 },
    { "k33dev", "k33dev", "console built-in: this list and the state of the server route", 1 },
};
#define DEV_CMD_COUNT (sizeof(kDevCmds) / sizeof(kDevCmds[0]))

static const char *const kArgGive[] = { "health", "ammo", "all", NULL };
static const char *const kArgTake[] = { "weapons", "health", "ammo", "all", NULL };
static const char *const kArgKickbot[] = { "all", NULL };
static const char *g_argDevset[32]; /* sh1x_cmds.h kSh1xDevset, the server's own list: filled by InstallConsole */
typedef struct { const char *cmd; const char *const *args; } ConDevArgs;
static const ConDevArgs kDevArgs[] = {
    { "devset", g_argDevset }, { "give", kArgGive }, { "take", kArgTake }, { "kickbot", kArgKickbot },
};

static const ConDevCmd *ConDevFind(const char *name)
{
    for (unsigned i = 0; i < DEV_CMD_COUNT; i++)
        if (!_stricmp(kDevCmds[i].name, name)) return &kDevCmds[i];
    return NULL;
}

static const char *const *ConDevArgList(const char *cmd)
{
    for (unsigned i = 0; i < sizeof(kDevArgs) / sizeof(kDevArgs[0]); i++)
        if (!_stricmp(kDevArgs[i].cmd, cmd)) return kDevArgs[i].args;
    return NULL;
}

static void ConDevDetail(const ConDevCmd *c)
{
    int k = 0;
    _snprintf(g_detail[k++], 200, "^3%s^7  %s", c->usage, c->builtin ? "" :
              "^9(server cheat, in a match: dev_cheats on first)");
    _snprintf(g_detail[k++], 200, "^9  %s", c->help);
    for (int i = 0; i < k; i++) g_detail[i][199] = 0;
    g_detailCount = k;
}

static void ConDevHelp(void);

/* ---- rcon to the local server ---- */

/* Connected to a server (C1's connection state [0x16FC2E8] >= 5, the value CL_ForwardCommandToServer 0x494CE0
 * needs to send a client command; the same word voice.c and controller.c read, measured there). In this
 * client the only server is the one it hosts. */
static BOOL ConConnected(void)
{
    const volatile int *state = (const volatile int *)(DWORD_PTR)0x016FC2E8u;
    return Readable((const void *)state, 4) && *state >= 5;
}

#if K33_DIAGNOSTICS
static const BYTE *volatile g_conRconPortDvar;

/* The connected server's address when it is on this machine: port, else 0. */
static int ConLocalServerPort(void)
{
    if (*(volatile int *)(DWORD_PTR)0x016FC2E8u < 5) return 0; /* connection state: not connected */
    DWORD enc = *(volatile DWORD *)(DWORD_PTR)0x016F8068u;    /* clc ^ key; netadr at +0x14 (discord.c) */
    const BYTE *adr = enc ? (const BYTE *)(DWORD_PTR)(enc ^ 0x26F950CCu) + 0x14 : NULL;
    if (!adr || !Readable(adr, 12)) return 0;
    int type = *(const int *)adr;
    if (!(type == 2 /* loopback */ || (type > 2 && adr[4] == 127))) return -1; /* connected, not local */
    return adr[8] << 8 | adr[9];
}
#endif

static BOOL ConReadPassword(const char *path, char *out, int n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return FALSE;
    int len = (int)fread(out, 1, n - 1, f);
    fclose(f);
    if (len <= 0) return FALSE;
    out[len] = 0;
    while (len && (out[len - 1] == '\r' || out[len - 1] == '\n' || out[len - 1] == ' ')) out[--len] = 0;
    for (const char *p = out; *p; p++) if (*p == '"' || (BYTE)*p < 0x20) return FALSE;
    return len > 0;
}

/* One rcon datagram to 127.0.0.1:port; the reply (every datagram of it, from that socket only) into out.
 * firstMs: how long to wait for the first datagram; moreMs: for each continuation. */
static int ConRconOnce(int port, const char *password, const char *command, char *out, int outSize, DWORD firstMs,
                       DWORD moreMs)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return -1;
    int rcvbuf = 2 * 1024 * 1024; /* cvarlist is a burst of about a hundred datagrams */
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, sizeof(rcvbuf));
    char pkt[CON_INPUT_LEN + 192];
    int n = _snprintf(pkt, sizeof(pkt), "\xff\xff\xff\xffrcon \"%s\" %s", password, command);
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int got = 0;
    out[0] = 0;
    if (n > 0 && n < (int)sizeof(pkt) && sendto(s, pkt, n, 0, (struct sockaddr *)&a, sizeof(a)) == n) {
        DWORD start = GetTickCount();
        for (;;) {
            DWORD used = GetTickCount() - start;
            if (!got && used >= firstMs) break;
            fd_set rd;
            FD_ZERO(&rd);
            FD_SET(s, &rd);
            DWORD left = got ? moreMs : firstMs - used; /* after the first datagram, only its continuation */
            struct timeval tv = { (long)(left / 1000), (long)(left % 1000) * 1000 };
            if (select(0, &rd, NULL, NULL, &tv) <= 0) break;
            char buf[8192];
            struct sockaddr_in from;
            int fromLen = sizeof(from);
            int r = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromLen);
            if (r > 0 && (from.sin_addr.s_addr != a.sin_addr.s_addr || from.sin_port != a.sin_port)) continue;
            if (r <= 4) break;
            const char *text = buf + 4;
            int len = r - 4;
            if (len >= 6 && !memcmp(text, "print\n", 6)) { text += 6; len -= 6; }
            if (got + len >= outSize) len = outSize - 1 - got;
            if (len <= 0) break;
            memcpy(out + got, text, len);
            got += len;
            out[got] = 0;
        }
    }
    closesocket(s);
    return got;
}

/* The route to the server this client hosts: lists for completion, ConSvExec. */
#include "console_server.c"

static void ConDevHelp(void)
{
    int port, nc, nd;
    char pw[128];
    DWORD session = 0;
    BOOL hosting = ConSvHost(&port, pw, sizeof(pw), &session), live;
    ConPrintf("sh1x developer commands - the match server runs them for the player who typed them. In a match:\n");
    ConPrintf("  ^3dev_cheats on^7 switches cheats on (sv_cheats 1; that match then records no progression), off again with off.\n");
    for (unsigned i = 0; i < DEV_CMD_COUNT; i++) ConPrintf("  ^3%-40s^7 %s\n", kDevCmds[i].usage, kDevCmds[i].help);
    ConPrintf("  Server commands and dvars: type the name, or ^3sv <name ...>^7 when the client has one of the same name\n");
    ConPrintf("  (the client's wins). dev_cheats shows the cheat state, dev_clients the players, cmdlist/dvarlist/find list both sides.\n");
    EnterCriticalSection(&g_svLock);
    nc = g_sv.cmdCount; nd = g_sv.dvarCount; live = g_sv.session != 0;
    LeaveCriticalSection(&g_svLock);
    if (hosting)
        ConPrintf("  Server route: hosting on 127.0.0.1:%d; %d server commands, %d server dvars (%s).\n", port, nc, nd,
                  live ? "from the running server" : "generated table; the server has not answered yet");
    else
        ConPrintf("  Server route: no match running; %d server commands, %d server dvars (generated table).\n", nc, nd);
    ConPrintf("  Local toggles: k33_drawFPS 1 (or cg_drawFPS 1), k33_drawViewpos 1.\n");
}

#if K33_DIAGNOSTICS
/* Diagnostics builds only: rcon to another server on this machine (a developer's dedicated sh1x), when this
 * client hosts nothing. The shipped client has no such path: it contacts the server it hosts and nothing else. */
typedef struct { int port; char command[CON_INPUT_LEN]; } ConRconJob;

static DWORD WINAPI ConRconThread(LPVOID param)
{
    ConRconJob *job = (ConRconJob *)param;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    char root[MAX_PATH], files[3][MAX_PATH], password[128];
    static char reply[16384];
    DWORD n = GetModuleFileNameA(NULL, root, MAX_PATH);
    while (n && root[n - 1] != '\\') n--;
    root[n] = 0;
    _snprintf(files[0], MAX_PATH, "%smain\\k33_rcon.txt", root);
    _snprintf(files[1], MAX_PATH, "%sp2p\\state\\native_server_rcon.txt", root);
    _snprintf(files[2], MAX_PATH, "%sCODOL\\1.1.1.13_server\\alpha0022\\native_server_rcon.txt", root);
    int tried = 0, answered = 0;
    for (int i = 0; i < 3 && !answered; i++) {
        files[i][MAX_PATH - 1] = 0;
        if (!ConReadPassword(files[i], password, sizeof(password))) continue;
        tried++;
        int got = ConRconOnce(job->port, password, job->command, reply, sizeof(reply), 1500, 250);
        if (got <= 0) { ConAppend("rcon: no reply from 127.0.0.1 (is the server running on that port?)\n"); answered = 1; break; }
        if (strstr(reply, "Bad rcon") || strstr(reply, "bad rcon") || strstr(reply, "Invalid password")) continue;
        answered = 1;
        ConAppend(reply);
        if (reply[strlen(reply) - 1] != '\n') ConAppend("\n");
    }
    if (!answered)
        ConAppend(tried ? "rcon: the server refused every local password (main\\k33_rcon.txt overrides)\n"
                        : "rcon: no password file (main\\k33_rcon.txt, p2p\\state or the lobby's native_server_rcon.txt)\n");
    free(job);
    return 0;
}

static void ConRconLegacy(const char *command)
{
    int port = ConLocalServerPort();
    if (port < 0) { ConPrintf("rcon: the connected server is not on this machine; rcon only talks to a local one\n"); return; }
    if (!port) port = g_conRconPortDvar ? *(volatile const int *)(g_conRconPortDvar + 0xC) : 17733;
    if (port <= 0) port = 17733;
    ConRconJob *job = (ConRconJob *)calloc(1, sizeof(*job));
    if (!job) return;
    job->port = port;
    lstrcpynA(job->command, command, sizeof(job->command));
    ConPrintf("rcon 127.0.0.1:%d: %s\n", port, job->command);
    HANDLE t = CreateThread(NULL, 0, ConRconThread, job, 0, NULL);
    if (t) CloseHandle(t);
    else free(job);
}
#endif

/* "sv <line>" / "rcon <line>", or a bare server command or dvar: to the server this client hosts.
 * explicitRcon: the line was typed as "rcon ..." (a diagnostics build then falls back to another local server
 * when this client hosts nothing; "sv" and bare names never leave the hosted server). */
static void ConServerLine(const char *command, BOOL explicitRcon)
{
    while (*command == ' ') command++;
    if (!*command) { ConPrintf("usage: sv <server command or dvar ...>   (the match server this client hosts)\n"); return; }
    if (ConSvExec(command)) return;
#if K33_DIAGNOSTICS
    if (explicitRcon) { ConRconLegacy(command); return; }
#else
    (void)explicitRcon;
#endif
    ConPrintf("no match running: \"%s\" is for the match server, which this client runs only during a match\n", command);
}

/* ---- suggestions ---------------------------------------------------------------------------------- */

typedef struct {
    const char *shown; const BYTE *dvar; const char *cmd; BYTE prefix; const ConDevCmd *dev; const char *argOf;
    const ConSvName *sv; /* a server command (svKind 1) or dvar (2): console_server.c, under g_svLock */
    BYTE svKind;
} ConMatch;
static ConMatch g_matches[CON_MAX_DVARS + 1024 + 64 + 2048 + 8192];

static int ConCmpMatch(const void *a, const void *b)
{
    const ConMatch *x = (const ConMatch *)a, *y = (const ConMatch *)b;
    if (x->prefix != y->prefix) return y->prefix - x->prefix;
    return _stricmp(x->shown, y->shown);
}

static const char *ConStrIStr(const char *h, const char *n)
{
    size_t ln = strlen(n);
    for (; *h; h++)
        if (_strnicmp(h, n, ln) == 0) return h;
    return NULL;
}

/* Token boundaries of the command under the cursor: the text after the last ';' before it. */
typedef struct { int start[4], end[4], count, cur; } ConTokens;

static void ConTokenize(const char *s, int len, int cursor, ConTokens *t)
{
    int segStart = 0;
    BOOL q = FALSE;
    for (int i = 0; i < cursor && i < len; i++) {
        if (s[i] == '"') q = !q;
        else if (s[i] == ';' && !q) segStart = i + 1;
    }
    memset(t, 0, sizeof(*t));
    t->cur = -1;
    int i = segStart;
    while (i < len && t->count < 4) {
        while (i < len && s[i] == ' ') i++;
        if (i >= len || s[i] == ';') break;
        int st = i;
        BOOL quoted = FALSE;
        while (i < len && (quoted || (s[i] != ' ' && s[i] != ';'))) {
            if (s[i] == '"') quoted = !quoted;
            i++;
        }
        t->start[t->count] = st;
        t->end[t->count] = i;
        if (cursor >= st && cursor <= i) t->cur = t->count;
        t->count++;
    }
    if (t->cur < 0) { /* cursor in white space: a new token begins there */
        t->cur = 0;
        for (int k = 0; k < t->count; k++)
            if (t->end[k] < cursor) t->cur = k + 1;
        if (t->cur < 4) { t->start[t->cur] = t->end[t->cur] = cursor; }
    }
}

static void ConTokenText(const ConTokens *t, int k, char *buf, int n)
{
    int len = t->end[k] - t->start[k];
    if (k >= t->count && k != t->cur) len = 0;
    if (len >= n) len = n - 1;
    if (len < 0) len = 0;
    memcpy(buf, g_conInput + t->start[k], len);
    buf[len] = 0;
}

static void ConDetailFor(const BYTE *d)
{
    char val[96], def[96], dom[160];
    const char *raw = ConDvarRawName(d);
    const C1DvarName *n = DvarNameById(raw);
    ConDvarValue(d, 0x0C, val, sizeof(val));
    ConDvarValue(d, 0x2C, def, sizeof(def));
    ConDvarDomain(d, dom, sizeof(dom));
    DWORD flags = *(const DWORD *)(d + 4);
    int k = 0;
    /* as IW4's Con_DrawAutoCompleteChoice for one dvar: name and value, then default and domain */
    _snprintf(g_detail[k++], 200, "^3%s^7  \"%s^7\"%s%s%s", n ? n->name : raw, val, n ? "   ^9C1 " : "", n ? raw : "",
              n && n->conf < 85 ? " (name is a positional guess)" : "");
    _snprintf(g_detail[k++], 200, "^9  default^7  \"%s^7\"   ^9type^7 %s%s%s%s%s", def, d[8] < 10 ? kTypeNames[d[8]] : "?",
              flags & 1 ? ", archive" : "", flags & 4 ? ", cheat" : "", flags & 0x2000 ? ", read-only" : "",
              flags & 0x800 ? ", init only" : "");
    _snprintf(g_detail[k++], 200, "^9  domain^7   %s", dom[0] ? dom : "?");
    if (n && n->desc && n->desc[0]) _snprintf(g_detail[k++], 200, "^9  %s", n->desc);
    for (int i = 0; i < k; i++) {
        g_detail[i][199] = 0;
        for (char *c = g_detail[i]; *c; c++)
            if (*c == '\n') *c = ' ';
    }
    g_detailCount = k;
}

/* The client's own commands: C1's list and the console built-ins (kBuiltins, and sv/rcon/k33dev). */
static BOOL ConClientHasCommand(const char *name)
{
    for (unsigned b = 0; b < BUILTIN_COUNT; b++)
        if (!_stricmp(kBuiltins[b], name)) return TRUE;
    const ConDevCmd *dev = ConDevFind(name);
    if (dev && dev->builtin) return TRUE;
    for (const ConCmd *c = (const ConCmd *)C1CommandList(); c; c = c->next) {
        if (!Readable(c, sizeof(*c)) || !Readable(c->name, 1)) break;
        if (!_stricmp(c->name, name)) return TRUE;
    }
    return FALSE;
}

/* A server command or dvar under the cursor. g_svLock and g_conLock held. */
static void ConSvDetail(const ConSvName *n, int kind)
{
    int k = 0, port;
    char pw[128];
    if (kind == 2)
        _snprintf(g_detail[k++], 200, "^3%s^7  \"%s^7\"   ^9server dvar (sh1x match server)", n->name, n->value ? n->value : "?");
    else
        _snprintf(g_detail[k++], 200, "^3%s^7   ^9server command (sh1x match server)", n->name);
    if (n->desc && n->desc[0]) _snprintf(g_detail[k++], 200, "^9  %s", n->desc);
    _snprintf(g_detail[k++], 200, "^9  %s", ConSvHost(&port, pw, sizeof(pw), NULL) ?
              "runs on the match server this client hosts; what it prints comes back here" :
              "no match running: the match server exists only during a match");
    for (int i = 0; i < k; i++) g_detail[i][199] = 0;
    g_detailCount = k;
}

/* Appends the server names that match part to g_matches[m...]. skipClient: leave out a name the client
 * already put in the list (its own command, dvar or built-in). g_svLock held. */
static int ConSvMatches(const char *part, int plen, int m, BOOL skipClient)
{
    int first = m, max = (int)(sizeof(g_matches) / sizeof(g_matches[0]));
    for (int pass = 0; pass < 2; pass++) {
        const ConSvName *list = pass ? g_sv.dvars : g_sv.cmds;
        int count = pass ? g_sv.dvarCount : g_sv.cmdCount;
        for (int i = 0; i < count && m < max; i++) {
            BYTE prefix = _strnicmp(list[i].name, part, plen) == 0;
            if (plen && !prefix && !ConStrIStr(list[i].name, part)) continue;
            BOOL dup = FALSE;
            for (int k = 0; skipClient && k < first && !dup; k++) dup = !_stricmp(g_matches[k].shown, list[i].name);
            if (dup) continue;
            g_matches[m].shown = list[i].name;
            g_matches[m].dvar = NULL;
            g_matches[m].cmd = list[i].name;
            g_matches[m].prefix = prefix;
            g_matches[m].dev = NULL;
            g_matches[m].argOf = NULL;
            g_matches[m].sv = &list[i];
            g_matches[m].svKind = (BYTE)(pass ? 2 : 1);
            m++;
        }
    }
    return m;
}

static void ConFillSuggestion(ConSuggest *s, const ConMatch *x)
{
    lstrcpynA(s->name, x->shown, sizeof(s->name));
    s->prefix = x->prefix;
    s->kind = (BYTE)(x->svKind == 2 ? 4 : 3);
    if (x->svKind == 2) _snprintf(s->value, sizeof(s->value), "%s", x->sv->value ? x->sv->value : "^9server dvar");
    else _snprintf(s->value, sizeof(s->value), "^9server command");
    s->value[sizeof(s->value) - 1] = 0;
    lstrcpynA(s->info, x->svKind == 2 ? "sh1x server dvar" : "sh1x server command", sizeof(s->info));
}

static void ConUpdateSuggestionsLocked(void);

/* Recomputes the dropdown for the token under the cursor. Main thread. The server lists (console_server.c)
 * stay locked for the whole pass: the matches point into them. */
static void ConUpdateSuggestions(void)
{
    ConSvRefresh();
    EnterCriticalSection(&g_svLock);
    ConUpdateSuggestionsLocked();
    LeaveCriticalSection(&g_svLock);
}

static void ConUpdateSuggestionsLocked(void)
{
    ConTokens t;
    char tok0[96], part[96];
    int dcount;
    ConTokenize(g_conInput, g_conInputLen, g_conCursor, &t);
    ConTokenText(&t, 0, tok0, sizeof(tok0));

    EnterCriticalSection(&g_conLock);
    g_suggCount = g_suggTotal = 0;
    g_suggSel = -1;
    g_detailCount = 0;
    g_suggCycling = FALSE;
    LeaveCriticalSection(&g_conLock);

    /* An empty opening prompt has no matches: do not take C1's reader
     * lock or traverse every dvar just to show the console. */
    if (!g_conInputLen) {
        g_suggTokStart = g_suggTokEnd = 0;
        InterlockedExchange(&g_conDirty, 1);
        return;
    }
    dcount = ConSnapshotDvars();

    if (t.cur >= 1 && (!_stricmp(tok0, "sv") || !_stricmp(tok0, "rcon"))) {
        /* "sv <name ...>": the server's commands and dvars, then the one being given arguments */
        if (t.cur == 1) {
            ConTokenText(&t, 1, part, sizeof(part));
            g_suggTokStart = t.start[1];
            g_suggTokEnd = t.cur < t.count ? t.end[1] : g_conCursor;
            int m = ConSvMatches(part, (int)strlen(part), 0, FALSE);
            qsort(g_matches, m, sizeof(g_matches[0]), ConCmpMatch);
            EnterCriticalSection(&g_conLock);
            g_suggTotal = m;
            g_suggCount = m < CON_SUGGEST ? m : CON_SUGGEST;
            for (int i = 0; i < g_suggCount; i++) ConFillSuggestion(&g_sugg[i], &g_matches[i]);
            if (g_suggCount >= 1 && !_stricmp(g_sugg[0].name, part)) ConSvDetail(g_matches[0].sv, g_matches[0].svKind);
            else if (!g_suggCount) ConDevDetail(ConDevFind(tok0));
            LeaveCriticalSection(&g_conLock);
        } else {
            char name[96];
            ConTokenText(&t, 1, name, sizeof(name));
            const ConSvName *c = ConSvFindCmd(name), *d = c ? NULL : ConSvFindDvar(name);
            EnterCriticalSection(&g_conLock);
            if (c || d) ConSvDetail(c ? c : d, c ? 1 : 2);
            LeaveCriticalSection(&g_conLock);
        }
        InterlockedExchange(&g_conDirty, 1);
        return;
    }
    BOOL nameSlot = t.cur == 0 || (t.cur >= 1 && ConIsSetCommand(tok0) &&
                                   (t.cur == 1 || (t.cur == 2 && !_stricmp(tok0, "setfromdvar"))));
    const char *const *devArgs = t.cur == 1 ? ConDevArgList(tok0) : NULL;
    if (!nameSlot && !devArgs && t.cur >= 1 && ConDevFind(tok0)) {
        /* typing a developer command's arguments: its usage */
        EnterCriticalSection(&g_conLock);
        ConDevDetail(ConDevFind(tok0));
        LeaveCriticalSection(&g_conLock);
        InterlockedExchange(&g_conDirty, 1);
        return;
    }
    if (devArgs) {
        ConTokenText(&t, t.cur, part, sizeof(part));
        g_suggTokStart = t.start[t.cur];
        g_suggTokEnd = t.cur < t.count ? t.end[t.cur] : g_conCursor;
        int plen = (int)strlen(part), m = 0;
        for (int i = 0; devArgs[i]; i++) {
            BYTE prefix = _strnicmp(devArgs[i], part, plen) == 0;
            if (plen && !prefix && !ConStrIStr(devArgs[i], part)) continue;
            g_matches[m].shown = devArgs[i];
            g_matches[m].dvar = NULL;
            g_matches[m].cmd = devArgs[i];
            g_matches[m].prefix = prefix;
            g_matches[m].dev = NULL;
            g_matches[m].sv = NULL;
            g_matches[m].argOf = tok0;
            m++;
        }
        qsort(g_matches, m, sizeof(g_matches[0]), ConCmpMatch);
        EnterCriticalSection(&g_conLock);
        g_suggTotal = m;
        g_suggCount = m < CON_SUGGEST ? m : CON_SUGGEST;
        for (int i = 0; i < g_suggCount; i++) {
            ConSuggest *sg = &g_sugg[i];
            lstrcpynA(sg->name, g_matches[i].shown, sizeof(sg->name));
            sg->prefix = g_matches[i].prefix;
            sg->kind = 2;
            _snprintf(sg->value, sizeof(sg->value), "^9%s argument", !_stricmp(tok0, "rcon") ? "server command" : tok0);
            sg->value[sizeof(sg->value) - 1] = 0;
            lstrcpynA(sg->info, "sh1x", sizeof(sg->info));
        }
        if (ConDevFind(tok0)) ConDevDetail(ConDevFind(tok0));
        LeaveCriticalSection(&g_conLock);
        InterlockedExchange(&g_conDirty, 1);
        return;
    }
    if (!nameSlot) {
        /* typing a value: show the dvar it is for */
        char name[96];
        const BYTE *d = NULL;
        if (t.cur >= 1 && t.count >= 1) {
            if (ConIsSetCommand(tok0) && t.count >= 2) ConTokenText(&t, 1, name, sizeof(name));
            else lstrcpynA(name, tok0, sizeof(name));
            d = ConFindDvar(name, dcount);
        }
        /* not the client's: a server command or dvar being given its arguments */
        BOOL client = d || t.count < 1 || ConClientHasCommand(name) || ConDevFind(name);
        const ConSvName *sc = client ? NULL : ConSvFindCmd(name);
        const ConSvName *sd = client || sc ? NULL : ConSvFindDvar(name);
        EnterCriticalSection(&g_conLock);
        if (d) ConDetailFor(d);
        else if (sc || sd) ConSvDetail(sc ? sc : sd, sc ? 1 : 2);
        LeaveCriticalSection(&g_conLock);
        InterlockedExchange(&g_conDirty, 1);
        return;
    }
    ConTokenText(&t, t.cur, part, sizeof(part));
    int plen = (int)strlen(part);
    g_suggTokStart = t.start[t.cur];
    g_suggTokEnd = t.cur < t.count ? t.end[t.cur] : g_conCursor;
    if (!plen) { InterlockedExchange(&g_conDirty, 1); return; }

    int m = 0;
    BOOL digits = ConIsDigits(part);
    for (int i = 0; i < dcount; i++) {
        const BYTE *d = g_dvarSnap[i];
        const char *shown = ConDvarShownName(d), *raw = ConDvarRawName(d);
        BYTE prefix = _strnicmp(shown, part, plen) == 0 || (digits && strncmp(raw, part, plen) == 0);
        if (!prefix && !ConStrIStr(shown, part) && !(digits && strstr(raw, part))) continue;
        g_matches[m].shown = shown;
        g_matches[m].dvar = d;
        g_matches[m].cmd = NULL;
        g_matches[m].prefix = prefix;
        g_matches[m].dev = NULL;
        g_matches[m].sv = NULL;
        m++;
    }
    if (t.cur == 0) {
        for (const ConCmd *c = (const ConCmd *)C1CommandList();
             c && m < (int)(sizeof(g_matches) / sizeof(g_matches[0])) - (int)BUILTIN_COUNT; c = c->next) {
            if (!Readable(c, sizeof(*c)) || !Readable(c->name, 1)) break;
            BYTE prefix = _strnicmp(c->name, part, plen) == 0;
            if (!prefix && !ConStrIStr(c->name, part)) continue;
            BOOL dup = FALSE;
            for (unsigned b = 0; b < BUILTIN_COUNT; b++) dup |= !_stricmp(kBuiltins[b], c->name);
            if (dup) continue;
            g_matches[m].shown = c->name;
            g_matches[m].dvar = NULL;
            g_matches[m].cmd = c->name;
            g_matches[m].prefix = prefix;
            g_matches[m].dev = NULL;
            g_matches[m].sv = NULL;
            m++;
        }
        for (unsigned b = 0; b < BUILTIN_COUNT; b++) {
            BYTE prefix = _strnicmp(kBuiltins[b], part, plen) == 0;
            if (!prefix && !ConStrIStr(kBuiltins[b], part)) continue;
            g_matches[m].shown = kBuiltins[b];
            g_matches[m].dvar = NULL;
            g_matches[m].cmd = kBuiltins[b];
            g_matches[m].prefix = prefix;
            g_matches[m].dev = NULL;
            g_matches[m].sv = NULL;
            m++;
        }
        for (unsigned b = 0; b < DEV_CMD_COUNT; b++) {
            BYTE prefix = _strnicmp(kDevCmds[b].name, part, plen) == 0;
            if (!prefix && !ConStrIStr(kDevCmds[b].name, part)) continue;
            g_matches[m].shown = kDevCmds[b].name;
            g_matches[m].dvar = NULL;
            g_matches[m].cmd = kDevCmds[b].name;
            g_matches[m].prefix = prefix;
            g_matches[m].dev = &kDevCmds[b];
            g_matches[m].sv = NULL;
            m++;
        }
        /* the hosted server's commands and dvars, unless the client has the name (the client's wins: "sv <name>") */
        m = ConSvMatches(part, plen, m, TRUE);
    }
    qsort(g_matches, m, sizeof(g_matches[0]), ConCmpMatch);

    EnterCriticalSection(&g_conLock);
    g_suggTotal = m;
    g_suggCount = m < CON_SUGGEST ? m : CON_SUGGEST;
    for (int i = 0; i < g_suggCount; i++) {
        ConSuggest *s = &g_sugg[i];
        const ConMatch *x = &g_matches[i];
        lstrcpynA(s->name, x->shown, sizeof(s->name));
        s->prefix = x->prefix;
        if (x->dvar) {
            const char *raw = ConDvarRawName(x->dvar);
            char dom[64];
            s->kind = 0;
            ConDvarValue(x->dvar, 0x0C, s->value, sizeof(s->value));
            ConDvarDomain(x->dvar, dom, sizeof(dom));
            if (strcmp(raw, x->shown) != 0) _snprintf(s->info, sizeof(s->info), "%s  [%s]", raw, dom);
            else _snprintf(s->info, sizeof(s->info), "[%s]", dom);
            s->info[sizeof(s->info) - 1] = 0;
        } else if (x->dev) {
            s->kind = 2;
            _snprintf(s->value, sizeof(s->value), "^9%s", x->dev->usage);
            s->value[sizeof(s->value) - 1] = 0;
            lstrcpynA(s->info, "sh1x developer command", sizeof(s->info));
        } else if (x->sv) {
            ConFillSuggestion(s, x);
        } else {
            s->kind = 1;
            s->value[0] = 0;
            lstrcpynA(s->info, "command", sizeof(s->info));
        }
    }
    /* a developer command typed in full: its usage and help */
    if (g_suggCount >= 1 && g_sugg[0].kind == 2 && g_matches[0].dev && _stricmp(g_sugg[0].name, part) == 0)
        ConDevDetail(g_matches[0].dev);
    /* exactly one dvar typed in full: its details too */
    if (g_suggCount >= 1 && g_sugg[0].kind == 0 && _stricmp(g_sugg[0].name, part) == 0) ConDetailFor(g_matches[0].dvar);
    /* a server command or dvar typed in full */
    if (g_suggCount >= 1 && g_sugg[0].kind >= 3 && _stricmp(g_sugg[0].name, part) == 0)
        ConSvDetail(g_matches[0].sv, g_matches[0].svKind);
    LeaveCriticalSection(&g_conLock);
    InterlockedExchange(&g_conDirty, 1);
}

/* ---- editing -------------------------------------------------------------------------------------- */

static void ConReplaceRange(int start, int end, const char *text)
{
    int tl = (int)strlen(text);
    if (start < 0) start = 0;
    if (end > g_conInputLen) end = g_conInputLen;
    if (g_conInputLen - (end - start) + tl >= CON_INPUT_LEN) return;
    memmove(g_conInput + start + tl, g_conInput + end, g_conInputLen - end);
    memcpy(g_conInput + start, text, tl);
    g_conInputLen += tl - (end - start);
    g_conInput[g_conInputLen] = 0;
    g_conCursor = start + tl;
}

static void ConSetInput(const char *s)
{
    lstrcpynA(g_conInput, s, CON_INPUT_LEN);
    g_conInputLen = (int)strlen(g_conInput);
    g_conCursor = g_conInputLen;
}

static void ConInsert(const char *s)
{
    ConReplaceRange(g_conCursor, g_conCursor, s);
}

static void ConTab(BOOL back)
{
    if (g_suggCycling && g_suggCount > 0) {
        EnterCriticalSection(&g_conLock);
        int n = g_suggCount;
        g_suggSel = back ? (g_suggSel <= 0 ? n - 1 : g_suggSel - 1) : (g_suggSel + 1) % n;
        char name[80];
        lstrcpynA(name, g_sugg[g_suggSel].name, sizeof(name));
        LeaveCriticalSection(&g_conLock);
        ConReplaceRange(g_suggTokStart, g_suggTokEnd, name);
        g_suggTokEnd = g_suggTokStart + (int)strlen(name);
        InterlockedExchange(&g_conDirty, 1);
        return;
    }
    ConUpdateSuggestions();
    if (!g_suggCount) return;
    char part[96];
    int plen = g_suggTokEnd - g_suggTokStart;
    if (plen >= (int)sizeof(part)) plen = sizeof(part) - 1;
    memcpy(part, g_conInput + g_suggTokStart, plen);
    part[plen] = 0;
    if (g_suggTotal == 1) {
        char name[84];
        _snprintf(name, sizeof(name), "%s ", g_sugg[0].name);
        name[sizeof(name) - 1] = 0;
        ConReplaceRange(g_suggTokStart, g_suggTokEnd, name);
        ConUpdateSuggestions();
        return;
    }
    /* longest common prefix of the prefix matches */
    int prefixCount = 0;
    for (int i = 0; i < g_suggCount && g_sugg[i].prefix; i++) prefixCount++;
    int lcp = 0;
    if (prefixCount) {
        lcp = (int)strlen(g_sugg[0].name);
        for (int i = 1; i < prefixCount; i++) {
            int k = 0;
            while (k < lcp && g_sugg[i].name[k] && tolower((BYTE)g_sugg[i].name[k]) == tolower((BYTE)g_sugg[0].name[k])) k++;
            lcp = k;
        }
    }
    if (prefixCount && lcp > plen) {
        char name[84];
        lstrcpynA(name, g_sugg[0].name, lcp + 1 < (int)sizeof(name) ? lcp + 1 : (int)sizeof(name));
        ConReplaceRange(g_suggTokStart, g_suggTokEnd, name);
        ConUpdateSuggestions();
        return;
    }
    /* nothing more in common: cycle through the list */
    g_suggCycling = TRUE;
    g_suggSel = -1;
    ConTab(back);
}

static char *ConHistoryPath(char *path)
{
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    while (n && path[n - 1] != '\\') n--;
    lstrcpynA(path + n, "main\\k33_console_history.txt", MAX_PATH - n);
    return path;
}

/* Disk I/O belongs to the raster worker, never the input/render thread. */
static void ConLoadHistory(void)
{
    char path[MAX_PATH], line[CON_INPUT_LEN + 4];
    char loaded[CON_HISTORY][CON_INPUT_LEN];
    int count = 0;
    FILE *f = fopen(ConHistoryPath(path), "rb");
    size_t bytes = 0;
    while (f && bytes < 1024 * 1024 && fgets(line, sizeof(line), f)) {
        size_t l = strlen(line); bytes += l;
        while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
        if (!l) continue;
        if (count == CON_HISTORY) {
            memmove(loaded[0], loaded[1], sizeof(loaded[0]) * (CON_HISTORY - 1)); count--;
        }
        lstrcpynA(loaded[count++], line, CON_INPUT_LEN);
    }
    if (f) fclose(f);
    EnterCriticalSection(&g_conUiLock);
    /* Commands typed during the disk read remain newer than saved history. */
    int current = g_conHistCount;
    if (count + current > CON_HISTORY) {
        int discard = count + current - CON_HISTORY;
        memmove(loaded, loaded + discard, (size_t)(count - discard) * CON_INPUT_LEN); count -= discard;
    }
    memcpy(loaded + count, g_conHist, (size_t)current * CON_INPUT_LEN); count += current;
    memcpy(g_conHist, loaded, (size_t)count * CON_INPUT_LEN);
    g_conHistCount = count; g_conHistPos = count; g_conHistLoaded = TRUE;
    LeaveCriticalSection(&g_conUiLock);
}

static void ConSaveHistory(void)
{
    char lines[CON_HISTORY][CON_INPUT_LEN], path[MAX_PATH];
    if (!TryEnterCriticalSection(&g_conUiLock)) return;
    int count = g_conHistCount;
    memcpy(lines, g_conHist, (size_t)count * CON_INPUT_LEN);
    InterlockedExchange(&g_conHistoryDirty, 0);
    LeaveCriticalSection(&g_conUiLock);
    FILE *f = fopen(ConHistoryPath(path), "wb");
    if (!f) return;
    for (int i = 0; i < count; i++) fprintf(f, "%s\r\n", lines[i]);
    fclose(f);
}

static void ConAddHistory(const char *line)
{
    if (!*line) return;
    if (g_conHistCount && strcmp(g_conHist[g_conHistCount - 1], line) == 0) {
        g_conHistPos = g_conHistCount;
        return;
    }
    if (g_conHistCount == CON_HISTORY) {
        memmove(g_conHist[0], g_conHist[1], sizeof(g_conHist[0]) * (CON_HISTORY - 1));
        g_conHistCount--;
    }
    lstrcpynA(g_conHist[g_conHistCount++], line, CON_INPUT_LEN);
    g_conHistPos = g_conHistCount;
    InterlockedExchange(&g_conHistoryDirty, 1);
    if (g_conWorkEvent) SetEvent(g_conWorkEvent);
}

/* ---- built-in commands ----------------------------------------------------------------------------- */

static int ConCmpDvarShown(const void *a, const void *b)
{
    return _stricmp(ConDvarShownName(*(const BYTE *const *)a), ConDvarShownName(*(const BYTE *const *)b));
}

static int ConCmpStr(const void *a, const void *b)
{
    return _stricmp(*(const char *const *)a, *(const char *const *)b);
}

static void ConDvarList(const char *filter)
{
    int n = ConSnapshotDvars(), shown = 0;
    qsort(g_dvarSnap, n, sizeof(g_dvarSnap[0]), ConCmpDvarShown);
    for (int i = 0; i < n; i++) {
        const BYTE *d = g_dvarSnap[i];
        const char *raw = ConDvarRawName(d), *nm = ConDvarShownName(d);
        if (filter && *filter && !ConStrIStr(nm, filter) && !ConStrIStr(raw, filter)) continue;
        char val[96];
        DWORD fl = *(const DWORD *)(d + 4);
        ConDvarValue(d, 0x0C, val, sizeof(val));
        ConPrintf("%c%c%c %-34s %-6s \"%s^7\"\n", fl & 1 ? 'A' : ' ', fl & 4 ? 'C' : ' ', fl & 0x2000 ? 'R' : ' ', nm,
                  strcmp(raw, nm) ? raw : "", val);
        shown++;
    }
    ConPrintf("%d of %d dvars%s%s (A archive, C cheat, R read-only; the second column is C1's own name)\n", shown, n,
              filter && *filter ? " matching " : "", filter && *filter ? filter : "");
    /* the match server's dvars: "sv <name>" reaches one whose name the client has too */
    EnterCriticalSection(&g_svLock);
    int sv = 0;
    for (int i = 0; i < g_sv.dvarCount; i++) {
        const ConSvName *d = &g_sv.dvars[i];
        if (filter && *filter && !ConStrIStr(d->name, filter)) continue;
        if (d->value) ConPrintf("sv  %-34s \"%s^7\"\n", d->name, d->value);
        else ConPrintf("sv  %s\n", d->name);
        sv++;
    }
    ConPrintf("%d of %d server dvars%s%s (%s)\n", sv, g_sv.dvarCount, filter && *filter ? " matching " : "",
              filter && *filter ? filter : "", g_sv.session ? "from the running match server" :
              "generated table, no values: no match running or the server has not answered yet");
    LeaveCriticalSection(&g_svLock);
}

static void ConCmdList(const char *filter)
{
    static const char *names[4096];
    int n = 0;
    for (const ConCmd *c = (const ConCmd *)C1CommandList(); c && n < 4096; c = c->next) {
        if (!Readable(c, sizeof(*c)) || !Readable(c->name, 1)) break;
        names[n++] = c->name;
    }
    for (unsigned b = 0; b < BUILTIN_COUNT && n < 4096; b++) names[n++] = kBuiltins[b];
    for (unsigned b = 0; b < DEV_CMD_COUNT && n < 4096; b++) names[n++] = kDevCmds[b].name;
    qsort(names, n, sizeof(names[0]), ConCmpStr);
    int shown = 0;
    for (int i = 0; i < n; i++) {
        if (i && !_stricmp(names[i], names[i - 1])) continue;
        if (filter && *filter && !ConStrIStr(names[i], filter)) continue;
        ConPrintf("  %s\n", names[i]);
        shown++;
    }
    ConPrintf("%d commands%s%s\n", shown, filter && *filter ? " matching " : "", filter && *filter ? filter : "");
    EnterCriticalSection(&g_svLock);
    int sv = 0;
    for (int i = 0; i < g_sv.cmdCount; i++) {
        if (filter && *filter && !ConStrIStr(g_sv.cmds[i].name, filter)) continue;
        ConPrintf("  sv  %s\n", g_sv.cmds[i].name);
        sv++;
    }
    ConPrintf("%d of %d server commands%s%s (%s)\n", sv, g_sv.cmdCount, filter && *filter ? " matching " : "",
              filter && *filter ? filter : "", g_sv.session ? "from the running match server" : "generated table");
    LeaveCriticalSection(&g_svLock);
}

static void ConDump(const char *name)
{
    char path[MAX_PATH], file[96];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    while (n && path[n - 1] != '\\') n--;
    _snprintf(file, sizeof(file), "%s", name && *name ? name : "condump.txt");
    file[sizeof(file) - 1] = 0;
    for (char *c = file; *c; c++)
        if (*c == '\\' || *c == '/' || *c == ':') *c = '_';
    if (!strchr(file, '.')) lstrcatA(file, ".txt");
    _snprintf(path + n, MAX_PATH - n, "main\\%s", file);
    path[MAX_PATH - 1] = 0;
    FILE *f = fopen(path, "wb");
    if (!f) { ConPrintf("condump: cannot write %s\n", path); return; }
    EnterCriticalSection(&g_conLock);
    for (int i = 0; i < g_conCount; i++) fprintf(f, "%s\r\n", g_conLines[(g_conHead + i) % CON_LINES]);
    int lines = g_conCount;
    LeaveCriticalSection(&g_conLock);
    fclose(f);
    ConPrintf("condump: %d lines written to %s\n", lines, path);
}

static void ConHelp(void)
{
    ConPrintf("Kernel33 console - %u readable dvar names (c1_dvars.h)\n", (unsigned)C1_DVAR_NAME_COUNT);
    ConPrintf("  Type a command or a dvar by its readable name: cg_drawFPS 1, com_maxfps 0, set cg_fov 80.\n");
    ConPrintf("  Tab completes the common prefix, then cycles the matches. Up/Down: history (main\\k33_console_history.txt).\n");
    ConPrintf("  Shift+Tab or Shift+~ opens the full log (PgUp/PgDn/wheel scroll, Ctrl+Home/End). Esc closes.\n");
    ConPrintf("  Built-ins: dvarlist [filter], cmdlist [filter], find <text>, clear, condump [file], k33help.\n");
    ConPrintf("  Toggle: the key left of 1, or the virtual key in k33_consoleKey (default 112 = F1, 0 = none).\n");
    ConPrintf("  Match server (during a match this client hosts): its commands and dvars complete and run from here too;\n");
    ConPrintf("  sv <name ...> addresses the server when the client has the same name. Cheats (noclip, god, give, ...): k33dev.\n");
    ConPrintf("  HUD: k33_drawFPS 1 (cg_drawFPS 1 works too), k33_drawViewpos 1.\n");
}

/* ---- executing a line ------------------------------------------------------------------------------- */

static int ConReadToken(const char *s, int *pos, char *out, int n)
{
    int i = *pos, o = 0;
    while (s[i] == ' ') i++;
    if (!s[i]) { *pos = i; out[0] = 0; return 0; }
    BOOL q = FALSE;
    int start = i;
    while (s[i] && (q || s[i] != ' ')) {
        if (s[i] == '"') q = !q;
        else if (o < n - 1) out[o++] = s[i];
        i++;
    }
    out[o] = 0;
    *pos = i;
    return i - start;
}

/* name -> C1's numeric name when it is a readable one; anything else as typed. */
static BOOL ConMapName(const char *in, char *out, int n, int dcount)
{
    const C1DvarName *nm = DvarNameByName(in);
    BOOL exists = FALSE;
    for (int i = 0; i < dcount && !exists; i++) exists = !_stricmp(ConDvarRawName(g_dvarSnap[i]), in);
    lstrcpynA(out, nm && !exists ? nm->id : in, n);
    if (nm && !exists && nm->conf < 85)
        ConPrintf("  ^3%s -> %s is a positional guess (confidence %d); check the value it shows before relying on it\n",
                  nm->name, nm->id, nm->conf);
    return nm && !exists;
}

/* Dvars the front end owns. Setting cg_draw2D ("585") to 1 at the menu crashed C1 (user report:
 * "]cg_draw2D 0", "]cg_draw2D 1", then AV read 0x3374 at 0x43A205). The name was right (585 is IW4's
 * cg_draw2D: CG_Draw2D 0x43B780 tests it at 0x43B7B4, bool, default 1) and the set went through C1's
 * own Dvar_Command -> Dvar_SetCommand 0x625380 -> Dvar_SetVariant 0x622D30, which validates type,
 * domain and flags. The crash is C1's: the front end keeps 585 at 0 (the VM code around 0x60A6xx
 * sets it) because CG_Draw2D, once enabled, runs with no snapshot and dereferences it in several
 * places (cg.nextSnap at 0x43A205, then 0x453CCD once that one is guarded). So the console refuses
 * to change these while cgame has no snapshot (menus, loading), and allows them in a match. */
static const char *const kNeedsMatch[] = { "585" /* cg_draw2D */ };

static BOOL ConInMatch(void)
{
    BYTE *cg = ParityCg();
    return cg && *(void *const *)(cg + CG_SNAP) != NULL;
}

/* name is C1's (numeric) name; hasValue: the command changes it */
static BOOL ConRefuseSet(const char *name, BOOL hasValue)
{
    if (!hasValue || ConInMatch()) return FALSE;
    for (unsigned i = 0; i < sizeof(kNeedsMatch) / sizeof(kNeedsMatch[0]); i++) {
        if (strcmp(name, kNeedsMatch[i]) != 0) continue;
        const C1DvarName *n = DvarNameById(name);
        ConPrintf("  ^1%s (%s) can only be changed in a match: C1 draws the 2D HUD without a snapshot at the "
                  "menu and crashes. Not set.\n", n ? n->name : name, name);
        return TRUE;
    }
    return FALSE;
}

static void ConExecute(const char *line)
{
    char out[1200];
    int o = 0;
    BOOL mapped = FALSE;
    int dcount = ConSnapshotDvars();
    EnterCriticalSection(&g_conLock);
    g_resCount = 0;
    g_resUntil = GetTickCount() + 500; /* Cbuf runs it next frame; its output lands well inside this */
    if (!g_resUntil) g_resUntil = 1;
    LeaveCriticalSection(&g_conLock);
    ConPrintf("]%s\n", line);

    /* Split on ';' outside quotes. Each command goes to exactly one place (docs/OFFLINE_CONSOLE_20261112.md):
     *   1. a console built-in (clear, cmdlist, dvarlist, find, condump, k33help, k33dev)      here
     *   2. "sv <line>" / "rcon <line>"                                                         the hosted server
     *   3. a name the client has (a C1 command or dvar, by C1's name or the readable one)      C1's Cbuf
     *   4. one of the server's cheat commands (kDevCmds)        C1's Cbuf, which forwards it as this player's
     *                                                           client command; "no match running" when not connected
     *   5. a server command or dvar the client does not have (also behind set/seta/reset/toggle) the hosted server
     *   6. anything else                                         C1's Cbuf (C1 forwards an unknown word when connected)
     * So where both sides have a name the client's wins, and "sv" reaches the server's. */
    char seg[CON_INPUT_LEN];
    int i = 0;
    while (line[i]) {
        int k = 0;
        BOOL q = FALSE;
        while (line[i] && (q || line[i] != ';')) {
            if (line[i] == '"') q = !q;
            if (k < (int)sizeof(seg) - 1) seg[k++] = line[i];
            i++;
        }
        seg[k] = 0;
        if (line[i] == ';') i++;

        char t0[96], t1[96], m0[96], m1[96], m2[96];
        int p = 0;
        ConReadToken(seg, &p, t0, sizeof(t0));
        if (!t0[0]) continue;
        int afterT0 = p;
        if (!_stricmp(t0, "clear")) {
            EnterCriticalSection(&g_conLock);
            g_conCount = g_conHead = 0;
            g_conScroll = 0;
            LeaveCriticalSection(&g_conLock);
            InterlockedExchange(&g_conDirty, 1);
            continue;
        }
#if K33_DIAGNOSTICS
        if (!_stricmp(t0, "k33shot")) {
            int delay;
            ConReadToken(seg, &p, t1, sizeof(t1));
            delay = atoi(t1);
            if (delay < 0 || delay > 60000) delay = 0;
            InterlockedExchange(&g_conShot, (LONG)((GetTickCount() + delay) | 1));
            InterlockedExchange(&g_conDirty, 1);
            ConPrintf("k33shot: the console frame %d ms from now goes to main\\k33_console.bmp\n", delay);
            continue;
        }
#endif
        if (!_stricmp(t0, "sv") || !_stricmp(t0, "rcon")) {
            ConServerLine(seg + afterT0, !_stricmp(t0, "rcon"));
            continue;
        }
        if (!_stricmp(t0, "k33dev")) {
            ConDevHelp();
            continue;
        }
        if (!_stricmp(t0, "dvarlist") || !_stricmp(t0, "cmdlist") || !_stricmp(t0, "find") || !_stricmp(t0, "condump") ||
            !_stricmp(t0, "k33help")) {
            ConReadToken(seg, &p, t1, sizeof(t1));
            if (!_stricmp(t0, "dvarlist")) ConDvarList(t1);
            else if (!_stricmp(t0, "cmdlist")) ConCmdList(t1);
            else if (!_stricmp(t0, "find")) { if (t1[0]) { ConCmdList(t1); ConDvarList(t1); } else ConPrintf("usage: find <text>\n"); }
            else if (!_stricmp(t0, "condump")) ConDump(t1);
            else ConHelp();
            continue;
        }

        {
            /* rules 3-5: which side owns the name this command is about */
            char subject[96];
            int q = afterT0;
            BOOL isSet = ConIsSetCommand(t0);
            if (isSet) ConReadToken(seg, &q, subject, sizeof(subject));
            else lstrcpynA(subject, t0, sizeof(subject));
            BOOL clientHas = ConFindDvar(subject, dcount) != NULL || (!isSet && ConClientHasCommand(subject));
            const ConDevCmd *dev = isSet ? NULL : ConDevFind(t0);
            int svKind = ConSvKind(subject);
            if (!clientHas && dev) {
                if (!ConConnected()) {
                    ConPrintf("no match running: %s is a command of the match server (k33dev lists them)\n", t0);
                    continue;
                }
                if (_stricmp(t0, "devhelp")) {
                    char arg[96];
                    int q2 = afterT0;
                    ConReadToken(seg, &q2, arg, sizeof(arg));
                    ConSvCheatHint();
                    ConSvCheatEcho(t0, arg); /* the server's own reply does not reach the console */
                } else {
                    ConDevHelp(); /* devhelp's answer is a game message too: the same list, from here */
                }
            } else if (!clientHas && (isSet ? svKind == 2 : svKind != 0)) {
                char a1[16];
                int q3 = afterT0;
                ConReadToken(seg, &q3, a1, sizeof(a1));
                if (!isSet && !_stricmp(t0, "dev_cheats") && !_stricmp(a1, "on") && ConSvCheatsOn(seg)) continue;
                ConServerLine(seg, FALSE);
                continue;
            } else if (clientHas && svKind) {
                ConPrintf("  ^9%s: the client's own; the match server has one too: sv %s\n", subject, subject);
            }
        }

        char rebuilt[CON_INPUT_LEN + 64];
        if (ConIsSetCommand(t0)) {
            int r;
            ConReadToken(seg, &p, t1, sizeof(t1));
            mapped |= ConMapName(t1, m1, sizeof(m1), dcount);
            {
                int q = p;
                char v[96];
                if (ConRefuseSet(m1, _stricmp(t0, "reset") == 0 || ConReadToken(seg, &q, v, sizeof(v)) > 0 ||
                                         !_strnicmp(t0, "toggle", 6)))
                    continue;
            }
            r = _snprintf(rebuilt, sizeof(rebuilt), "%s %s", t0, m1);
            if (!_stricmp(t0, "setfromdvar") && r > 0) {
                char t2[96];
                ConReadToken(seg, &p, t2, sizeof(t2));
                mapped |= ConMapName(t2, m2, sizeof(m2), dcount);
                r += _snprintf(rebuilt + r, sizeof(rebuilt) - r, " %s", m2);
            }
            if (r > 0 && r < (int)sizeof(rebuilt)) _snprintf(rebuilt + r, sizeof(rebuilt) - r, "%s", seg + p);
        } else {
            mapped |= ConMapName(t0, m0, sizeof(m0), dcount);
            {
                int q = afterT0;
                char v[96];
                if (ConRefuseSet(m0, ConReadToken(seg, &q, v, sizeof(v)) > 0)) continue;
            }
            _snprintf(rebuilt, sizeof(rebuilt), "%s%s", m0, seg + afterT0);
        }
        rebuilt[sizeof(rebuilt) - 1] = 0;
        int w = _snprintf(out + o, sizeof(out) - o - 2, "%s%s", o ? ";" : "", rebuilt);
        o = w < 0 ? (int)sizeof(out) - 2 : o + w;
    }
    if (!o) return;
    out[o] = 0;
    if (mapped) ConPrintf("  -> %s\n", out);
    out[o++] = '\n';
    out[o] = 0;
    if (g_c1CbufOk) ConCbufAddText(out);
    else ConPrintf("Cbuf_AddText did not match, cannot execute\n");
}

/* ---- keyboard (main thread) ------------------------------------------------------------------------ */

static void ConHudRegister(void);

static void ConRegisterKeyDvar(void)
{
    g_conKeyDvarTried = TRUE;
    ConHudRegister();
    const BYTE *d = C1RegisterInt("k33_consoleKey", VK_F1, 0, 255, 0x1 /* archive */,
                                  "Kernel33 console: extra toggle key (Windows virtual-key code; 112 F1, 0 none)");
    if (d) g_conKeyDvar = d;
}

static BOOL ConIsToggle(WPARAM vk, LPARAM lp)
{
    if (((lp >> 16) & 0xFF) == 0x29 && !(lp & (1 << 24))) return TRUE; /* the key left of 1 */
    int extra = g_conKeyDvar ? *(volatile const int *)(g_conKeyDvar + 0xC) : VK_F1;
    return extra > 0 && (int)vk == extra;
}

static void ConSetExpanded(BOOL on)
{
    InterlockedExchange(&g_conExpanded, on);
    if (!on) g_conScroll = 0;
    InterlockedExchange(&g_conDirty, 1);
}

static void ConSetOpen(BOOL open)
{
    InterlockedExchange(&g_conOpen, open);
    InterlockedExchange(&g_conDirty, 1);
    if (!open) {
        EnterCriticalSection(&g_conLock);
        g_resCount = 0;
        g_resUntil = 0;
        LeaveCriticalSection(&g_conLock);
        InterlockedExchange(&g_conExpanded, 0);
        g_conScroll = 0;
    }
    if (open) {
        if (!g_conBanner) {
            g_conBanner = TRUE;
            ConPrintf("Kernel33 console: type k33help. %u readable dvar names; "
                      "hooks: output %s, input ok, draw %s, execute %s\n", (unsigned)C1_DVAR_NAME_COUNT,
                      g_conCaptureOk ? "ok" : "OFF", g_conDrawOk ? "ok" : "OFF", g_c1CbufOk ? "ok" : "OFF");
        }
        ConUpdateSuggestions();
    }
}

static void ConPaste(HWND hwnd)
{
    if (!OpenClipboard(hwnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    const WCHAR *w = h ? (const WCHAR *)GlobalLock(h) : NULL;
    if (w) {
        char gbk[CON_INPUT_LEN];
        int n = WideCharToMultiByte(GAME_CODEPAGE, 0, w, -1, gbk, sizeof(gbk), NULL, NULL);
        if (n > 0) {
            gbk[n - 1] = 0;
            for (char *c = gbk; *c; c++)
                if ((BYTE)*c < 0x20) *c = ' ';
            ConInsert(gbk);
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
}

/* GBK-aware cursor steps: a double-byte character is one step for the arrows, Backspace and Delete. */
static int ConCharBefore(int pos)
{
    int i = 0, prev = 0;
    while (i < pos) {
        prev = i;
        i += IsDBCSLeadByteEx(GAME_CODEPAGE, (BYTE)g_conInput[i]) && i + 1 < g_conInputLen ? 2 : 1;
    }
    return prev;
}

static int ConCharAfter(int pos)
{
    if (pos >= g_conInputLen) return g_conInputLen;
    return pos + (IsDBCSLeadByteEx(GAME_CODEPAGE, (BYTE)g_conInput[pos]) && pos + 1 < g_conInputLen ? 2 : 1);
}

static void ConKeyDown(HWND hwnd, WPARAM vk)
{
    /* AltGr is Ctrl+Alt: AltGr+L is a letter on some layouts (Polish l-stroke), not Ctrl+L */
    BOOL ctrl = GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) >= 0, shift = GetKeyState(VK_SHIFT) < 0;
    BOOL edited = FALSE;
    switch (vk) {
    case VK_ESCAPE:
        if (g_suggCycling || g_suggCount || g_detailCount) {
            EnterCriticalSection(&g_conLock);
            g_suggCount = g_suggTotal = g_detailCount = 0;
            g_suggCycling = FALSE;
            g_suggSel = -1;
            LeaveCriticalSection(&g_conLock);
            InterlockedExchange(&g_conDirty, 1);
        } else {
            ConSetOpen(FALSE);
        }
        return;
    case VK_RETURN: {
        char line[CON_INPUT_LEN];
        lstrcpynA(line, g_conInput, sizeof(line));
        ConSetInput("");
        g_conScroll = 0;
        ConAddHistory(line);
        ConExecute(line);
        edited = TRUE;
        break;
    }
    case VK_TAB:
        if (shift) { ConSetExpanded(!g_conExpanded); return; }
        ConTab(FALSE);
        return;
    case VK_BACK:
        if (ctrl) {
            int s = g_conCursor;
            while (s > 0 && g_conInput[s - 1] == ' ') s--;
            while (s > 0 && g_conInput[s - 1] != ' ') s--;
            ConReplaceRange(s, g_conCursor, "");
        } else if (g_conCursor > 0) {
            ConReplaceRange(ConCharBefore(g_conCursor), g_conCursor, "");
        }
        edited = TRUE;
        break;
    case VK_DELETE:
        if (g_conCursor < g_conInputLen) { int c = g_conCursor; ConReplaceRange(c, ConCharAfter(c), ""); g_conCursor = c; }
        edited = TRUE;
        break;
    case VK_LEFT:
        if (ctrl) { while (g_conCursor > 0 && g_conInput[g_conCursor - 1] == ' ') g_conCursor--;
                    while (g_conCursor > 0 && g_conInput[g_conCursor - 1] != ' ') g_conCursor--; }
        else if (g_conCursor > 0) g_conCursor = ConCharBefore(g_conCursor);
        edited = TRUE;
        break;
    case VK_RIGHT:
        if (ctrl) { while (g_conCursor < g_conInputLen && g_conInput[g_conCursor] != ' ') g_conCursor++;
                    while (g_conCursor < g_conInputLen && g_conInput[g_conCursor] == ' ') g_conCursor++; }
        else if (g_conCursor < g_conInputLen) g_conCursor = ConCharAfter(g_conCursor);
        edited = TRUE;
        break;
    case VK_HOME:
        if (ctrl) { EnterCriticalSection(&g_conLock); g_conScroll = g_conCount; LeaveCriticalSection(&g_conLock); }
        else g_conCursor = 0;
        edited = !ctrl;
        break;
    case VK_END:
        if (ctrl) g_conScroll = 0;
        else g_conCursor = g_conInputLen;
        edited = !ctrl;
        break;
    case VK_UP:
        if (g_conHistPos > 0) { g_conHistPos--; ConSetInput(g_conHist[g_conHistPos]); }
        edited = TRUE;
        break;
    case VK_DOWN:
        if (g_conHistPos < g_conHistCount - 1) { g_conHistPos++; ConSetInput(g_conHist[g_conHistPos]); }
        else { g_conHistPos = g_conHistCount; ConSetInput(""); }
        edited = TRUE;
        break;
    case VK_PRIOR:
        if (!g_conExpanded) break;
        EnterCriticalSection(&g_conLock);
        g_conScroll += g_conRows > 4 ? g_conRows - 2 : 2;
        LeaveCriticalSection(&g_conLock);
        break;
    case VK_NEXT:
        if (!g_conExpanded) break;
        EnterCriticalSection(&g_conLock);
        g_conScroll -= g_conRows > 4 ? g_conRows - 2 : 2;
        if (g_conScroll < 0) g_conScroll = 0;
        LeaveCriticalSection(&g_conLock);
        break;
    default:
        if (ctrl && vk == 'V') { ConPaste(hwnd); edited = TRUE; }
        else if (ctrl && vk == 'U') { ConReplaceRange(0, g_conCursor, ""); edited = TRUE; }
        else if (ctrl && vk == 'K') { g_conInput[g_conCursor] = 0; g_conInputLen = g_conCursor; edited = TRUE; }
        else if (ctrl && vk == 'L') { EnterCriticalSection(&g_conLock); g_conCount = g_conHead = g_conScroll = 0; LeaveCriticalSection(&g_conLock); }
        break;
    }
    if (edited) ConUpdateSuggestions();
    InterlockedExchange(&g_conDirty, 1);
}

/* The ANSI code page Windows uses for this thread's WM_CHAR: the active input language's, not the
 * system's (measured: Russian layout on a 1252 system gives 1251 bytes, Chinese gives GBK). */
static UINT ConInputCodePage(void)
{
    char buf[8];
    LANGID lang = LOWORD((DWORD_PTR)GetKeyboardLayout(0));
    if (GetLocaleInfoA(MAKELCID(lang, SORT_DEFAULT), LOCALE_IDEFAULTANSICODEPAGE, buf, sizeof(buf)) > 0) {
        UINT cp = (UINT)atoi(buf);
        if (cp) return cp;
    }
    return GetACP();
}

/* One WM_CHAR into the line as GBK. A double-byte character arrives as one message (lead byte in
 * the low byte, trail byte in the next); UTF-8 or a split DBCS pair arrives byte by byte. */
static void ConTypeChar(WPARAM wp)
{
    BYTE in[2] = { (BYTE)wp, (BYTE)(wp >> 8) };
    int count = in[1] ? 2 : 1;
    UINT cp = ConInputCodePage();
    for (int i = 0; i < count; i++) {
        BYTE c = in[i];
        if (!g_conMbLen && c < 0x80) {
            if (c >= 0x20 && c != 0x7F) { char t[2] = { (char)c, 0 }; ConInsert(t); }
            continue;
        }
        g_conMb[g_conMbLen++] = c;
        WCHAR wide[4];
        int n = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, (LPCSTR)g_conMb, g_conMbLen, wide, 4);
        if (n <= 0) {
            BOOL utf8 = cp == CP_UTF8;
            BOOL lead = utf8 ? g_conMb[0] >= 0xC2 && g_conMb[0] <= 0xF4 : IsDBCSLeadByteEx(cp, g_conMb[0]);
            if (lead && g_conMbLen < (utf8 ? 4 : 2)) continue; /* wait for the rest */
            g_conMbLen = 0;
            if (c >= 0x20 && c < 0x7F) { char t[2] = { (char)c, 0 }; ConInsert(t); }
            continue;
        }
        g_conMbLen = 0;
        char gbk[16];
        int m = WideCharToMultiByte(GAME_CODEPAGE, 0, wide, n, gbk, sizeof(gbk) - 1, NULL, NULL);
        if (m > 0) {
            gbk[m] = 0;
            if ((BYTE)gbk[0] >= 0x20 && gbk[0] != 0x7F) ConInsert(gbk);
        }
    }
}

/* A dead key used as the toggle (^ on German, ` on US-International) leaves its accent pending in
 * the layout; a throwaway ToUnicodeEx of Space consumes it, so the next letter is not accented. */
static void ConClearDeadKey(void)
{
    BYTE keys[256] = { 0 };
    WCHAR out[4];
    HKL hkl = GetKeyboardLayout(0);
    ToUnicodeEx(VK_SPACE, MapVirtualKeyExA(VK_SPACE, 0 /* MAPVK_VK_TO_VSC */, hkl), keys, out, 4, 0, hkl);
}

static HIMC ConImeDetach(HWND hwnd)
{
    HIMC cur = ImmGetContext(hwnd);
    if (!cur) return NULL;
    ImmNotifyIME(cur, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmReleaseContext(hwnd, cur);
    return ImmAssociateContext(hwnd, NULL);
}

static void ConImeRestore(void)
{
    HWND h = g_conImeHwnd;
    HIMC saved = g_conImeSaved;
    g_conImeHwnd = NULL;
    g_conImeSaved = NULL;
    if (!h || !IsWindow(h)) return;
    HIMC cur = ImmGetContext(h);
    if (cur) ImmReleaseContext(h, cur); /* the game attached its own meanwhile: keep that */
    else if (saved) ImmAssociateContext(h, saved);
}

/* No IME while the console is open: keys arrive as plain characters of the layout under it. */
static void ConImeSync(HWND hwnd, UINT msg)
{
    if (!g_conOpen) {
        if (g_conImeHwnd) ConImeRestore();
        return;
    }
    if (g_conImeHwnd != hwnd) {
        if (g_conImeHwnd) ConImeRestore();
        g_conImeHwnd = hwnd;
        g_conImeSaved = ConImeDetach(hwnd);
        g_conMbLen = 0;
    } else if (msg == WM_KEYDOWN || msg == WM_IME_STARTCOMPOSITION || msg == WM_INPUTLANGCHANGE ||
               msg == WM_IME_SETCONTEXT) {
        HIMC again = ConImeDetach(hwnd); /* Scaleform's text fields re-attach it (0x6B5461) */
        if (again && !g_conImeSaved) g_conImeSaved = again;
    }
}

static LRESULT CALLBACK ConWndProcOwned(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    WNDPROC game = (WNDPROC)(DWORD_PTR)GAME_WNDPROC_VA;
    if (!g_conKeyDvarTried && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) &&
        *(volatile int *)(DWORD_PTR)C1_DVAR_COUNT_VA > 100)
        ConRegisterKeyDvar();
    ConImeSync(hwnd, msg);
    BOOL open = g_conOpen != 0;
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (msg == WM_SYSKEYDOWN && (wp == VK_F4 || wp == VK_TAB || wp == VK_RETURN)) break; /* Alt+F4/Tab/Enter */
        if (ConIsToggle(wp, lp)) {
            if (!(lp & (1 << 30))) { /* not on auto-repeat */
                /* as IW4x: ~ opens the input bar, Shift+~ the full log */
                BOOL shift = GetKeyState(VK_SHIFT) < 0;
                if (!open) { ConSetOpen(TRUE); ConSetExpanded(shift); }
                else if (shift) ConSetExpanded(!g_conExpanded);
                else ConSetOpen(FALSE);
            }
            g_conEatChar = ((lp >> 16) & 0xFF) == 0x29;
            ConImeSync(hwnd, 0);
            return 0;
        }
        if (open) {
            if (msg == WM_KEYDOWN) ConKeyDown(hwnd, wp);
            return 0;
        }
        break;
    case WM_CHAR:
    case WM_SYSCHAR:
        if (g_conEatChar) { g_conEatChar = FALSE; return 0; }
        if (open) {
            if (msg == WM_CHAR) {
                if (g_suggCycling) g_suggCycling = FALSE;
                ConTypeChar(wp);
                ConUpdateSuggestions();
                InterlockedExchange(&g_conDirty, 1);
            }
            return 0;
        }
        break;
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
        if (g_conEatChar) { g_conEatChar = FALSE; ConClearDeadKey(); return 0; }
        if (open) return 0;
        break;
    case WM_MOUSEWHEEL:
        if (open && !g_conExpanded) return 0;
        if (open) {
            EnterCriticalSection(&g_conLock);
            g_conScroll += (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 3 : -3);
            if (g_conScroll < 0) g_conScroll = 0;
            LeaveCriticalSection(&g_conLock);
            InterlockedExchange(&g_conDirty, 1);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN:
    case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK: case WM_XBUTTONDBLCLK:
        if (open) return 0;
        break;
    case WM_IME_COMPOSITION:
        if (open) {
            if (lp & GCS_RESULTSTR) {
                HIMC imc = ImmGetContext(hwnd);
                LONG bytes = imc ? ImmGetCompositionStringW(imc, GCS_RESULTSTR, NULL, 0) : 0;
                if (bytes > 0) {
                    WCHAR wide[128];
                    char gbk[CON_INPUT_LEN];
                    if (bytes > (LONG)sizeof(wide)) bytes = sizeof(wide);
                    bytes = ImmGetCompositionStringW(imc, GCS_RESULTSTR, wide, bytes);
                    int n = bytes > 0 ? WideCharToMultiByte(GAME_CODEPAGE, 0, wide, bytes / 2, gbk, sizeof(gbk) - 1, NULL, NULL) : 0;
                    if (n > 0) { gbk[n] = 0; ConInsert(gbk); ConUpdateSuggestions(); }
                }
                if (imc) ImmReleaseContext(hwnd, imc);
                lp &= ~(GCS_RESULTSTR | GCS_RESULTCLAUSE | GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE);
                if (!lp) return 0;
            }
            return DefWindowProcA(hwnd, msg, wp, lp);
        }
        break;
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_CHAR:
        if (open) return msg == WM_IME_CHAR ? 0 : DefWindowProcA(hwnd, msg, wp, lp);
        break;
    }
    return game(hwnd, msg, wp, lp);
}

/* Editing/completion and C1 dvar access remain on their existing owner thread.
 * The worker holds this lock only for a bounded snapshot, never GDI or I/O. */
static LRESULT CALLBACK ConsoleWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    EnterCriticalSection(&g_conUiLock);
    LRESULT result = ConWndProcOwned(hwnd, msg, wp, lp);
    LeaveCriticalSection(&g_conUiLock);
    return result;
}

/* ---- drawing (render thread) ------------------------------------------------------------------------ */

typedef HRESULT(WINAPI *PresentFn)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef HRESULT(WINAPI *SCPresentFn)(IDirect3DSwapChain9 *, const RECT *, const RECT *, HWND, const RGNDATA *, DWORD);
typedef HRESULT(WINAPI *CreateDeviceFn)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *,
                                        IDirect3DDevice9 **);
typedef IDirect3D9 *(WINAPI *Direct3DCreate9Fn)(UINT);

static Direct3DCreate9Fn g_origCreate9;
static CreateDeviceFn g_origCreateDevice;
static PresentFn g_origPresent;
static SCPresentFn g_origSCPresent;
static volatile LONG g_inDraw;

static IDirect3DDevice9 *g_texDev;
static IDirect3DTexture9 *g_tex;
static int g_texW, g_texH;
static HDC g_dc;
static HBITMAP g_dib;
static HGDIOBJ g_conStockBitmap;
static DWORD *g_bits;
static int g_dibW, g_dibH, g_usedH, g_fontH;
static HFONT g_font;

typedef struct {
    char lines[160][CON_LINE_LEN];
    int lineCount, scroll, total, expanded, blink, cycling;
    char input[CON_INPUT_LEN];
    int cursor;
    ConSuggest sugg[CON_SHOWN];
    int suggCount, suggTotal, suggSel, suggFirst;
    char part[96];
    char detail[5][200];
    int detailCount;
    char res[CON_RESULTS][CON_LINE_LEN];
    int resCount;
} ConSnap;
static ConSnap g_snap;

static COLORREF ConColor(int code)
{
    static const COLORREF k[10] = { RGB(110, 110, 110), RGB(255, 92, 92), RGB(0, 255, 0), RGB(255, 255, 0),
                                    RGB(90, 130, 255), RGB(0, 255, 255), RGB(255, 92, 255), RGB(225, 225, 225),
                                    RGB(200, 200, 200), RGB(140, 140, 150) };
    return k[code < 0 || code > 9 ? 7 : code];
}

/* Draws GBK text with ^N colour codes at (x, y); returns the x after it. */
static int ConText(int x, int y, const char *s, COLORREF color, int maxX)
{
    WCHAR w[CON_LINE_LEN + 8];
    char seg[CON_LINE_LEN + 8];
    SetTextColor(g_dc, color);
    while (*s && x < maxX) {
        int n = 0;
        while (*s && n < CON_LINE_LEN) {
            if ((BYTE)*s >= 0x81 && s[1]) { seg[n++] = *s++; seg[n++] = *s++; continue; }
            if (*s == '^' && s[1] >= '0' && s[1] <= '9') break;
            seg[n++] = *s++;
        }
        if (n) {
            int wn = MultiByteToWideChar(GAME_CODEPAGE, 0, seg, n, w, CON_LINE_LEN + 8);
            if (wn > 0) {
                SIZE sz;
                TextOutW(g_dc, x, y, w, wn);
                GetTextExtentPoint32W(g_dc, w, wn, &sz);
                x += sz.cx;
            }
        }
        if (*s == '^' && s[1] >= '0' && s[1] <= '9') {
            SetTextColor(g_dc, ConColor(s[1] - '0'));
            s += 2;
        }
    }
    return x;
}

static int ConTextWidth(const char *s, int n)
{
    WCHAR w[CON_INPUT_LEN + 8];
    int wn = MultiByteToWideChar(GAME_CODEPAGE, 0, s, n, w, CON_INPUT_LEN + 8);
    SIZE sz = { 0, 0 };
    if (wn > 0) GetTextExtentPoint32W(g_dc, w, wn, &sz);
    return sz.cx;
}

static void ConFill(int x0, int y0, int x1, int y1, COLORREF c)
{
    RECT r = { x0, y0, x1, y1 };
    HBRUSH b = CreateSolidBrush(c);
    FillRect(g_dc, &r, b);
    DeleteObject(b);
}

static BOOL ConEnsureDib(int w, int h)
{
    int fh = h / 64; /* IW4's consoleFont is 16 px on a 1024-line screen */
    if (fh < 12) fh = 12;
    if (fh > 32) fh = 32;
    if (g_dc && g_dib && g_bits && g_dibW == w && g_dibH == h && g_fontH == fh) return TRUE;
    if (!g_dc) g_dc = CreateCompatibleDC(NULL);
    if (!g_dc) return FALSE;
    if (g_dib) { SelectObject(g_dc, g_conStockBitmap); DeleteObject(g_dib); g_dib = NULL; }
    g_bits = NULL;
    g_dibW = g_dibH = g_fontH = 0;
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    g_dib = CreateDIBSection(g_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!g_dib) return FALSE;
    g_bits = (DWORD *)bits;
    HGDIOBJ previousBitmap = SelectObject(g_dc, g_dib);
    if (!g_conStockBitmap) g_conStockBitmap = previousBitmap;
    if (g_font) { SelectObject(g_dc, GetStockObject(SYSTEM_FONT)); DeleteObject(g_font); }
    g_font = CreateFontA(-fh, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SelectObject(g_dc, g_font);
    SetBkMode(g_dc, TRANSPARENT);
    g_dibW = w;
    g_dibH = h;
    g_fontH = fh;
    return TRUE;
}

/* Translucent regions: GDI leaves alpha 0, so each box records its fill and the alpha it gets;
 * any pixel that is not the fill (text) is made opaque. */
typedef struct { int x0, y0, x1, y1; COLORREF fill; BYTE a; } ConAlphaRect;
static ConAlphaRect g_alpha[16];
static int g_alphaCount;

/* IW4's console dvar defaults (iw4x.exe Con_OneTimeInit): con_inputBoxColor 0.25 0.25 0.2,
 * con_inputHintBoxColor 0.4 0.4 0.35, con_outputWindowColor 0.35 0.35 0.3 0.75, con_outputBarColor
 * 1 1 0.95 0.6, con_outputSliderColor 0.15 0.15 0.1 0.6; dvar matches and values 1 1 0.8, inactive
 * values 0.8 0.8 0.8, command matches 0.8 0.8 1, con_versionColor 1 1 0. The prompt is drawn the way
 * IW4x draws its "IW4x MP >" prompt, in the version colour. */
#define CON_PROMPT     "SH1X >"
#define CON_INPUTBOX   RGB(64, 64, 51)
#define CON_HINTBOX    RGB(102, 102, 89)
#define CON_OUTBOX     RGB(89, 89, 77)
#define CON_DVARCOL    RGB(255, 255, 204)
#define CON_VALCOL     RGB(255, 255, 204)
#define CON_INACTIVE   RGB(204, 204, 204)
#define CON_CMDCOL     RGB(204, 204, 255)
#define CON_MATCHCOL   RGB(255, 255, 0)
#define CON_VERSIONCOL RGB(255, 255, 0)

static void ConBox(int x0, int y0, int x1, int y1, COLORREF fill, BYTE alpha)
{
    ConFill(x0, y0, x1, y1, fill);
    if (g_alphaCount < 16) g_alpha[g_alphaCount++] = (ConAlphaRect){ x0, y0, x1, y1, fill, alpha };
}

/* name with the typed part highlighted */
static int ConMatchText(int x, int y, const char *name, const char *part, COLORREF base, int maxX)
{
    char a[96], b[96], c[96];
    const char *hit = part[0] ? ConStrIStr(name, part) : NULL;
    if (!hit) return ConText(x, y, name, base, maxX);
    int pre = (int)(hit - name), len = (int)strlen(part);
    lstrcpynA(a, name, pre + 1 < 96 ? pre + 1 : 96);
    lstrcpynA(b, hit, len + 1 < 96 ? len + 1 : 96);
    lstrcpynA(c, hit + len, 96);
    x = ConText(x, y, a, base, maxX);
    x = ConText(x, y, b, CON_MATCHCOL, maxX);
    return ConText(x, y, c, base, maxX);
}

/* IW4 Con_DrawInput: the input line is one text line plus padding across the top of the screen,
 * inset by the console's screen margin; the hint box hangs under it from the input text's x; the full
 * console (Shift+Tab) is the output window under the same input line, down to the bottom margin. */
static int ConRender(int W, int H)
{
    ConSnap *s = &g_snap;
    int fh = g_fontH, pad = fh / 4 + 2, lh = fh + 2;
    int m = H / 120 + 2;                         /* con_screenMin/Max inset */
    g_alphaCount = 0;
    memset(g_bits, 0, (size_t)W * H * 4);

    int bx0 = m, by0 = m, bx1 = W - m, by1 = by0 + fh + 2 * pad;
    ConBox(bx0, by0, bx1, by1, CON_INPUTBOX, 0xE0);
    int iy = by0 + pad;
    int ix = ConText(bx0 + pad, iy, CON_PROMPT, CON_VERSIONCOL, bx1) + fh / 2;
    SetTextColor(g_dc, RGB(255, 255, 255));
    {
        WCHAR w[CON_INPUT_LEN + 8];
        int wn = MultiByteToWideChar(GAME_CODEPAGE, 0, s->input, (int)strlen(s->input), w, CON_INPUT_LEN + 8);
        if (wn > 0) TextOutW(g_dc, ix, iy, w, wn);
    }
    if (s->blink) {
        int cx = ix + ConTextWidth(s->input, s->cursor);
        ConFill(cx, iy + 1, cx + 2, iy + fh, RGB(255, 255, 255));
    }
    int used = by1;

    if (s->expanded) {
        int ox0 = bx0, oy0 = by1, ox1 = bx1, oy1 = H - m;
        ConBox(ox0, oy0, ox1, oy1, CON_OUTBOX, 0xC0);
        int rows = (oy1 - oy0 - 2 * pad - lh) / lh;
        g_conRows = rows;
        int y = oy1 - pad - 2 * lh;
        for (int i = s->lineCount - 1; i >= 0 && y >= oy0 + pad; i--, y -= lh)
            ConText(ox0 + pad, y, s->lines[i], RGB(255, 255, 255), ox1 - 3 * pad);
        /* scroll bar on the right: con_outputBarColor track, con_outputSliderColor thumb */
        int sx0 = ox1 - 2 * pad, sx1 = ox1 - pad / 2, sy0 = oy0 + pad, sy1 = oy1 - pad - lh;
        ConFill(sx0, sy0, sx1, sy1, RGB(255, 255, 242));
        if (s->total > rows) {
            int track = sy1 - sy0, len = track * rows / s->total;
            if (len < 8) len = 8;
            int bottom = s->total - s->scroll;
            int top = sy0 + (int)((long long)(track - len) * (bottom - rows) / (s->total - rows));
            ConFill(sx0, top, sx1, top + len, RGB(38, 38, 26));
        }
        static const char kVersion[] = "SH1X Kernel33 dev - C1 1.1.1.13 build 550136";
        int vw = ConTextWidth(kVersion, (int)sizeof(kVersion) - 1);
        ConText(ox1 - pad - vw, oy1 - pad - lh, kVersion, CON_VERSIONCOL, ox1);
        if (s->scroll) {
            char t[64];
            _snprintf(t, sizeof(t), "-- %d more below (PgDn) --", s->scroll);
            t[sizeof(t) - 1] = 0;
            ConText(ox0 + pad, oy1 - pad - lh, t, CON_VERSIONCOL, ox1 - vw - 2 * pad);
        }
        used = oy1;
    }

    /* the hint box, only while something is typed */
    int hy = by1;
    BOOL single = s->detailCount && s->suggTotal <= 1;
    BOOL tooMany = s->suggTotal > CON_SHOWN && !s->cycling;
    int rowsShown = single ? s->detailCount : tooMany ? 1 : s->suggCount + s->detailCount;
    if (s->input[0] && rowsShown) {
        int nameW = 0;
        for (int i = 0; i < s->suggCount; i++) {
            int w = ConTextWidth(s->sugg[i].name, (int)strlen(s->sugg[i].name));
            if (w > nameW) nameW = w;
        }
        nameW += fh * 2;
        int hx0 = ix - pad, hx1 = bx1;
        int hy1 = hy + rowsShown * lh + 2 * pad;
        if (hy1 > H) hy1 = H;
        ConBox(hx0, hy, hx1, hy1, CON_HINTBOX, 0xF0);
        int ry = hy + pad;
        if (tooMany && !single) {
            char more[128];
            _snprintf(more, sizeof(more), "%d matches (too many to show here, press shift+tab to view full list)",
                      s->suggTotal);
            more[sizeof(more) - 1] = 0;
            ConText(hx0 + pad, ry, more, RGB(255, 255, 255), hx1 - pad);
        } else if (!single) {
            for (int i = 0; i < s->suggCount && ry + lh <= hy1; i++, ry += lh) {
                const ConSuggest *g = &s->sugg[i];
                if (s->suggFirst + i == s->suggSel) ConFill(hx0, ry, hx1, ry + lh, RGB(140, 140, 120));
                ConMatchText(hx0 + pad, ry, g->name, s->part, g->kind ? CON_CMDCOL : CON_DVARCOL, hx0 + pad + nameW);
                if (g->kind != 1) {
                    char v[96];
                    _snprintf(v, sizeof(v), "%s^7", g->value);
                    v[sizeof(v) - 1] = 0;
                    ConText(hx0 + pad + nameW, ry, v, CON_VALCOL, hx1 - pad);
                }
            }
        }
        for (int i = 0; !tooMany && i < s->detailCount && ry + lh <= hy1; i++, ry += lh)
            ConText(hx0 + pad, ry, s->detail[i], i ? CON_INACTIVE : CON_VALCOL, hx1 - pad);
        hy = hy1;
        if (hy1 > used) used = hy1;
    }

    /* collapsed: what the last command printed, under the line */
    if (!s->expanded && s->resCount) {
        int ry0 = hy, ry1 = ry0 + s->resCount * lh + 2 * pad;
        if (ry1 > H) ry1 = H;
        ConBox(bx0, ry0, bx1, ry1, CON_OUTBOX, 0xC0);
        int y = ry0 + pad;
        for (int i = 0; i < s->resCount && y + lh <= ry1; i++, y += lh)
            ConText(bx0 + pad, y, s->res[i], RGB(255, 255, 255), bx1 - pad);
        if (ry1 > used) used = ry1;
    }
    GdiFlush();

    for (int r = 0; r < g_alphaCount; r++) {
        const ConAlphaRect *a = &g_alpha[r];
        DWORD fill = ((DWORD)GetRValue(a->fill) << 16) | ((DWORD)GetGValue(a->fill) << 8) | GetBValue(a->fill);
        for (int yy = a->y0 < 0 ? 0 : a->y0; yy < a->y1 && yy < H; yy++) {
            DWORD *row = g_bits + (size_t)yy * W;
            for (int xx = a->x0 < 0 ? 0 : a->x0; xx < a->x1 && xx < W; xx++) {
                DWORD p = row[xx] & 0xFFFFFF;
                BYTE al = p == fill ? a->a : 0xFF;
                if ((row[xx] >> 24) < al) row[xx] = p | ((DWORD)al << 24);
            }
        }
    }
    return used;
}

static BOOL ConTakeSnapshot(void)
{
    ConSnap *s = &g_snap;
    if (!TryEnterCriticalSection(&g_conUiLock)) return FALSE;
    if (!TryEnterCriticalSection(&g_conLock)) { LeaveCriticalSection(&g_conUiLock); return FALSE; }
    s->expanded = g_conExpanded != 0;
    s->blink = g_conBlink;
    s->cycling = g_suggCycling;
    int rows = g_conRows > 0 && g_conRows < 160 ? g_conRows : 20;
    if (g_conScroll > g_conCount - rows) g_conScroll = g_conCount - rows > 0 ? g_conCount - rows : 0;
    int end = g_conCount - g_conScroll;
    int start = end - rows;
    if (start < 0) start = 0;
    s->lineCount = 0;
    if (s->expanded)
        for (int i = start; i < end; i++) lstrcpynA(s->lines[s->lineCount++], g_conLines[(g_conHead + i) % CON_LINES], CON_LINE_LEN);
    s->scroll = g_conScroll;
    s->total = g_conCount;
    lstrcpynA(s->input, g_conInput, CON_INPUT_LEN);
    s->cursor = g_conCursor;
    int plen = g_suggTokEnd - g_suggTokStart;
    if (g_suggCycling || plen < 0 || plen >= (int)sizeof(s->part)) plen = 0;
    memcpy(s->part, g_conInput + g_suggTokStart, plen);
    s->part[plen] = 0;
    int first = 0;
    if (g_suggSel >= CON_SHOWN) first = g_suggSel - CON_SHOWN + 1;
    s->suggFirst = first;
    s->suggCount = g_suggCount - first < CON_SHOWN ? g_suggCount - first : CON_SHOWN;
    if (s->suggCount < 0) s->suggCount = 0;
    memcpy(s->sugg, g_sugg + first, s->suggCount * sizeof(ConSuggest));
    s->suggTotal = g_suggTotal;
    s->suggSel = g_suggSel;
    s->detailCount = g_detailCount;
    memcpy(s->detail, g_detail, sizeof(g_detail));
    s->resCount = g_resCount;
    memcpy(s->res, g_res, sizeof(g_res[0]) * g_resCount);
    LeaveCriticalSection(&g_conLock);
    LeaveCriticalSection(&g_conUiLock);
    return TRUE;
}

/* Slot ownership: FREE -> WRITING -> READY -> READING -> FREE.
 * Neither producer nor consumer ever waits for the other; stale READY frames
 * may be reclaimed, but READING pixels cannot be resized or overwritten. */
#include "console_mailbox.h"
static ConRasterFrame g_conFrames[2];
static DWORD g_conUploadedSerial;

static void ConSaveShot(int W, int used)
{
    LONG shot = g_conShot;
    if (!shot || (LONG)(GetTickCount() - (DWORD)shot) < 0 ||
        InterlockedCompareExchange(&g_conShot, 0, shot) != shot || used <= 0) return;
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    while (n && path[n - 1] != '\\') n--;
    lstrcpynA(path + n, "main\\k33_console.bmp", MAX_PATH - n);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    BITMAPFILEHEADER fh = { 0x4D42, 0, 0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) };
    BITMAPINFOHEADER ih = { sizeof(ih), W, -used, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
    fh.bfSize = fh.bfOffBits + (DWORD)W * used * 4;
    fwrite(&fh, sizeof(fh), 1, f); fwrite(&ih, sizeof(ih), 1, f);
    fwrite(g_bits, 4, (size_t)W * used, f); fclose(f);
}

static DWORD WINAPI ConRasterWorker(void *unused)
{
    (void)unused;
    ConLoadHistory();
    DWORD serial = 0, lastRaster = 0;
    for (;;) {
        WaitForSingleObject(g_conWorkEvent, 16);
        if (g_conHistoryDirty) ConSaveHistory();
        if (!g_conOpen) continue;
        LONG blink = !((GetTickCount() >> 8) & 1);
        if (InterlockedExchange(&g_conBlink, blink) != blink) InterlockedExchange(&g_conDirty, 1);
        int W = g_conRequestedW, H = g_conRequestedH;
        if (W < 320 || H < 200 || W > 8192 || H > 8192 || (size_t)W * H > 16777216) continue;
        if ((DWORD)(GetTickCount() - lastRaster) < 16) continue;
        if (!InterlockedExchange(&g_conDirty, 0)) continue;
        if (!ConTakeSnapshot()) { InterlockedExchange(&g_conDirty, 1); continue; }
        ConRasterFrame *frame = ConFrameAcquire(g_conFrames);
        if (!frame) { InterlockedExchange(&g_conDirty, 1); continue; }
        if (!ConEnsureDib(W, H)) { ConFrameRelease(frame); InterlockedExchange(&g_conDirty, 1); continue; }
        int used = ConRender(W, H);
        size_t size = (size_t)W * used * sizeof(DWORD);
        if (size > frame->capacity) {
            uint32_t *pixels = realloc(frame->pixels, size);
            if (!pixels) { ConFrameRelease(frame); InterlockedExchange(&g_conDirty, 1); continue; }
            frame->pixels = pixels; frame->capacity = size;
        }
        memcpy(frame->pixels, g_bits, size);
        frame->w = W; frame->h = H; frame->used = used; frame->serial = ++serial;
        lastRaster = GetTickCount();
        ConSaveShot(W, used);
        ConFramePublish(frame);
    }
    return 0;
}

typedef struct { float x, y, z, rhw; DWORD color; float u, v; } ConVertex;

/* One textured quad (tex, (0,0)-(u1,v1)) at (x0,y0) w x h on the back buffer, inside a D3DSBT_ALL state
 * block so the renderer's own state is restored exactly. FALSE when the state block cannot be made. */
static BOOL ConBlit(IDirect3DDevice9 *dev, IDirect3DSurface9 *bb, IDirect3DTexture9 *tex, float x0, float y0, float w,
                    float h, float u1, float v1, int W, int H)
{
    IDirect3DStateBlock9 *sb = NULL;
    if (FAILED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &sb)) || !sb) {
        return FALSE;
    }
    IDirect3DSurface9 *oldRt = NULL, *oldDs = NULL;
    IDirect3DDevice9_GetRenderTarget(dev, 0, &oldRt);
    IDirect3DDevice9_GetDepthStencilSurface(dev, &oldDs);
    IDirect3DDevice9_SetRenderTarget(dev, 0, bb);
    IDirect3DDevice9_SetDepthStencilSurface(dev, NULL);
    D3DVIEWPORT9 vp = { 0, 0, (DWORD)W, (DWORD)H, 0.0f, 1.0f };
    IDirect3DDevice9_SetViewport(dev, &vp);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
        IDirect3DDevice9_SetVertexShader(dev, NULL);
        IDirect3DDevice9_SetPixelShader(dev, NULL);
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_BLENDOP, D3DBLENDOP_ADD);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_CLIPPLANEENABLE, 0);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_FILLMODE, D3DFILL_SOLID);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_MULTISAMPLEANTIALIAS, FALSE);
        IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)tex);
        IDirect3DDevice9_SetTexture(dev, 1, NULL);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXCOORDINDEX, 0);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, 0);
        ConVertex q[4] = {
            { x0 - 0.5f, y0 - 0.5f, 0, 1, 0xFFFFFFFF, 0, 0 },
            { x0 + w - 0.5f, y0 - 0.5f, 0, 1, 0xFFFFFFFF, u1, 0 },
            { x0 - 0.5f, y0 + h - 0.5f, 0, 1, 0xFFFFFFFF, 0, v1 },
            { x0 + w - 0.5f, y0 + h - 0.5f, 0, 1, 0xFFFFFFFF, u1, v1 },
        };
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, q, sizeof(ConVertex));
        IDirect3DDevice9_EndScene(dev);
    }
    IDirect3DDevice9_SetRenderTarget(dev, 0, oldRt);
    IDirect3DDevice9_SetDepthStencilSurface(dev, oldDs);
    if (oldRt) IDirect3DSurface9_Release(oldRt);
    if (oldDs) IDirect3DSurface9_Release(oldDs);
    IDirect3DStateBlock9_Apply(sb);
    IDirect3DStateBlock9_Release(sb);
    return TRUE;
}

static void ConDraw(IDirect3DDevice9 *dev)
{
    static LONG failures;
    if (!g_conOpen || !g_conWorkEvent || InterlockedExchange(&g_inDraw, 1)) return;
    if (IDirect3DDevice9_TestCooperativeLevel(dev) != D3D_OK) goto out;

    IDirect3DSurface9 *bb = NULL;
    D3DSURFACE_DESC desc;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) goto out;
    IDirect3DSurface9_GetDesc(bb, &desc);
    int W = (int)desc.Width, H = (int)desc.Height;
    if (W < 320 || H < 200 || W > 8192 || H > 8192) { IDirect3DSurface9_Release(bb); goto out; }

    if (g_texDev != dev || g_texW != W || g_texH != H) {
        if (g_tex) { IDirect3DTexture9_Release(g_tex); g_tex = NULL; }
        if (g_texDev && g_texDev != dev) IDirect3DDevice9_Release(g_texDev);
        if (g_texDev != dev) { IDirect3DDevice9_AddRef(dev); g_texDev = dev; }
        if (FAILED(IDirect3DDevice9_CreateTexture(dev, W, H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_tex, NULL))) {
            g_tex = NULL;
            if (InterlockedIncrement(&failures) <= 3) Log("console: CreateTexture %dx%d failed", W, H);
            IDirect3DSurface9_Release(bb);
            goto out;
        }
        g_texW = W;
        g_texH = H;
        g_usedH = 0;
        g_conUploadedSerial = 0;
        InterlockedExchange(&g_conDirty, 1);
    }
    LONG oldW = InterlockedExchange(&g_conRequestedW, W);
    LONG oldH = InterlockedExchange(&g_conRequestedH, H);
    if (oldW != W || oldH != H)
        InterlockedExchange(&g_conDirty, 1);
    if (g_conWorkEvent) SetEvent(g_conWorkEvent);
    ConRasterFrame *held[2] = { NULL, NULL }, *newest = NULL;
    for (int i = 0; i < 2; i++) {
        ConRasterFrame *frame = &g_conFrames[i];
        if (!ConFrameRead(frame)) continue;
        held[i] = frame;
        if (frame->w == W && frame->h == H && frame->used > 0 &&
            (LONG)(frame->serial - g_conUploadedSerial) > 0 &&
            (!newest || (LONG)(frame->serial - newest->serial) > 0)) newest = frame;
    }
    /* At most one upload per Present, even if both CPU slots were READY. */
    if (newest) {
        D3DLOCKED_RECT lr;
        RECT rc = { 0, 0, W, newest->used };
        if (SUCCEEDED(IDirect3DTexture9_LockRect(g_tex, 0, &lr, &rc, 0))) {
            for (int y = 0; y < newest->used; y++)
                memcpy((BYTE *)lr.pBits + (size_t)y * lr.Pitch, newest->pixels + (size_t)y * W, (size_t)W * 4);
            IDirect3DTexture9_UnlockRect(g_tex, 0);
            g_usedH = newest->used;
            g_conUploadedSerial = newest->serial;
        } else InterlockedExchange(&g_conDirty, 1);
    }
    for (int i = 0; i < 2; i++) if (held[i]) ConFrameRelease(held[i]);

    if (g_usedH > 0 && !ConBlit(dev, bb, g_tex, 0.0f, 0.0f, (float)W, (float)g_usedH, 1.0f, (float)g_usedH / (float)H, W, H) &&
        InterlockedIncrement(&failures) <= 3)
        Log("console: CreateStateBlock failed, not drawing");
    IDirect3DSurface9_Release(bb);
out:
    InterlockedExchange(&g_inDraw, 0);
}

/* ---- developer HUD: k33_drawFPS / k33_drawViewpos -----------------------------------------------------
 * C1 registers cg_drawFPS ("591") but nothing reads it, and IW4's viewpos display is gone. These draw a
 * small box in the top-right corner of every frame while either dvar (or 591) is on, through the same
 * Present hook as the console: FPS from the Present rate, position/angles/speed from cg's predicted
 * playerState (cg+0: origin +0x1C, velocity +0x28, viewangles +0x14C; parity.c). Only in a match. */

static const BYTE *volatile g_hudFpsDvar, *volatile g_hudPosDvar, *volatile g_hudC1FpsDvar;
static IDirect3DTexture9 *g_hudTex;
static IDirect3DDevice9 *g_hudTexDev;
static HDC g_hudDc;
static HBITMAP g_hudDib;
static DWORD *g_hudBits;
static HFONT g_hudFont;
static int g_hudW, g_hudH, g_hudFontH, g_hudUsedW, g_hudUsedH;
static DWORD g_hudLastBuild, g_hudFpsStart;
static int g_hudFrames;
static float g_hudFps;

static BOOL ConHudWanted(int *fps, int *pos)
{
    *fps = (g_hudFpsDvar && *(volatile const int *)(g_hudFpsDvar + 0xC)) ||
           (g_hudC1FpsDvar && *(volatile const BYTE *)(g_hudC1FpsDvar + 0xC));
    *pos = g_hudPosDvar && *(volatile const int *)(g_hudPosDvar + 0xC);
    return *fps || *pos;
}

static void ConHudRegister(void)
{
    if (!g_hudFpsDvar)
        g_hudFpsDvar = C1RegisterInt("k33_drawFPS", 0, 0, 1, 0x1, "Kernel33: draw the frame rate (top right)");
    if (!g_hudPosDvar)
        g_hudPosDvar = C1RegisterInt("k33_drawViewpos", 0, 0, 1, 0x1,
                                     "Kernel33: draw the predicted position, view angles and speed (top right)");
#if K33_DIAGNOSTICS
    if (!g_conRconPortDvar)
        g_conRconPortDvar = C1RegisterInt("k33_rconPort", 17733, 1, 65535, 0x1,
                                          "Kernel33 console: rcon port when not connected to a local server");
#endif
    if (!g_hudC1FpsDvar) {
        const BYTE *d = C1DvarFind("591"); /* cg_drawFPS, bool */
        if (d && Readable(d, 0x10) && d[8] == 0) g_hudC1FpsDvar = d;
    }
}

static BOOL ConHudEnsureDib(int fh)
{
    int w = fh * 36, h = fh * 4;
    if (g_hudDc && g_hudFontH == fh) return TRUE;
    if (!g_hudDc) g_hudDc = CreateCompatibleDC(NULL);
    if (!g_hudDc) return FALSE;
    if (g_hudDib) { DeleteObject(g_hudDib); g_hudDib = NULL; }
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    g_hudDib = CreateDIBSection(g_hudDc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!g_hudDib) return FALSE;
    g_hudBits = (DWORD *)bits;
    SelectObject(g_hudDc, g_hudDib);
    if (g_hudFont) DeleteObject(g_hudFont);
    g_hudFont = CreateFontA(-fh, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SelectObject(g_hudDc, g_hudFont);
    SetBkMode(g_hudDc, TRANSPARENT);
    g_hudW = w;
    g_hudH = h;
    g_hudFontH = fh;
    return TRUE;
}

/* Renders the lines into the DIB; returns FALSE when there is nothing to show. */
static BOOL ConHudBuild(int fps, int pos)
{
    char lines[3][96];
    int count = 0;
    if (fps) _snprintf(lines[count++], 96, "%.0f fps", g_hudFps);
    if (pos) {
        BYTE *cg = ConInMatch() ? ParityCg() : NULL;
        if (cg && Readable(cg, 0x160)) {
            const float *o = (const float *)(cg + 0x1C), *v = (const float *)(cg + 0x28), *a = (const float *)(cg + 0x14C);
            _snprintf(lines[count++], 96, "pos %.1f %.1f %.1f", o[0], o[1], o[2]);
            _snprintf(lines[count++], 96, "ang %.1f %.1f  speed %.0f", a[0], a[1], sqrtf(v[0] * v[0] + v[1] * v[1]));
        }
    }
    if (!count) return FALSE;
    memset(g_hudBits, 0, (size_t)g_hudW * g_hudH * 4);
    int pad = g_hudFontH / 4 + 2, lh = g_hudFontH + 2, maxw = 0;
    for (int i = 0; i < count; i++) {
        lines[i][95] = 0;
        SIZE sz;
        GetTextExtentPoint32A(g_hudDc, lines[i], (int)strlen(lines[i]), &sz);
        if (sz.cx > maxw) maxw = sz.cx;
    }
    g_hudUsedW = maxw + 2 * pad;
    g_hudUsedH = count * lh + 2 * pad;
    if (g_hudUsedW > g_hudW) g_hudUsedW = g_hudW;
    if (g_hudUsedH > g_hudH) g_hudUsedH = g_hudH;
    SetTextColor(g_hudDc, RGB(255, 255, 255));
    for (int i = 0; i < count; i++) TextOutA(g_hudDc, pad, pad + i * lh, lines[i], (int)strlen(lines[i]));
    GdiFlush();
    /* text pixels opaque, the box translucent black */
    for (int y = 0; y < g_hudUsedH; y++) {
        DWORD *row = g_hudBits + (size_t)y * g_hudW;
        for (int x = 0; x < g_hudUsedW; x++) row[x] = (row[x] & 0xFFFFFF) ? (row[x] & 0xFFFFFF) | 0xFF000000u : 0x90000000u;
    }
    return TRUE;
}

static void ConHudDraw(IDirect3DDevice9 *dev)
{
    int fps, pos;
    DWORD now = GetTickCount();
    g_hudFrames++;
    if (!g_hudFpsStart) g_hudFpsStart = now;
    if (now - g_hudFpsStart >= 500) {
        g_hudFps = g_hudFrames * 1000.0f / (float)(now - g_hudFpsStart);
        g_hudFrames = 0;
        g_hudFpsStart = now;
    }
    if (!ConHudWanted(&fps, &pos) || InterlockedExchange(&g_inDraw, 1)) return;
    if (IDirect3DDevice9_TestCooperativeLevel(dev) != D3D_OK) goto out;
    IDirect3DSurface9 *bb = NULL;
    D3DSURFACE_DESC desc;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) goto out;
    IDirect3DSurface9_GetDesc(bb, &desc);
    int W = (int)desc.Width, H = (int)desc.Height, fh = H / 60;
    if (fh < 12) fh = 12;
    if (fh > 32) fh = 32;
    if (W < 320 || H < 200 || !ConHudEnsureDib(fh)) { IDirect3DSurface9_Release(bb); goto out; }
    if (g_hudTexDev != dev || !g_hudTex) {
        if (g_hudTex) { IDirect3DTexture9_Release(g_hudTex); g_hudTex = NULL; }
        if (g_hudTexDev && g_hudTexDev != dev) IDirect3DDevice9_Release(g_hudTexDev);
        if (g_hudTexDev != dev) { IDirect3DDevice9_AddRef(dev); g_hudTexDev = dev; }
        g_hudLastBuild = 0;
    }
    if (g_hudTex) {
        D3DSURFACE_DESC td;
        IDirect3DTexture9_GetLevelDesc(g_hudTex, 0, &td);
        if ((int)td.Width != g_hudW || (int)td.Height != g_hudH) { IDirect3DTexture9_Release(g_hudTex); g_hudTex = NULL; }
    }
    if (!g_hudTex &&
        FAILED(IDirect3DDevice9_CreateTexture(dev, g_hudW, g_hudH, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_hudTex, NULL))) {
        g_hudTex = NULL;
        IDirect3DSurface9_Release(bb);
        goto out;
    }
    if (!g_hudLastBuild || now - g_hudLastBuild >= 100) { /* ten updates a second */
        g_hudLastBuild = now ? now : 1;
        if (!ConHudBuild(fps, pos)) { g_hudUsedW = 0; }
        else {
            D3DLOCKED_RECT lr;
            RECT rc = { 0, 0, g_hudW, g_hudUsedH };
            if (SUCCEEDED(IDirect3DTexture9_LockRect(g_hudTex, 0, &lr, &rc, 0))) {
                for (int y = 0; y < g_hudUsedH; y++)
                    memcpy((BYTE *)lr.pBits + (size_t)y * lr.Pitch, g_hudBits + (size_t)y * g_hudW, (size_t)g_hudW * 4);
                IDirect3DTexture9_UnlockRect(g_hudTex, 0);
            }
        }
    }
    if (g_hudUsedW > 0) {
        int m = H / 120 + 2;
        float x0 = (float)(W - m - g_hudUsedW), y0 = (float)m + (g_conOpen ? (float)(g_fontH * 3) : 0.0f);
        ConBlit(dev, bb, g_hudTex, x0, y0, (float)g_hudUsedW, (float)g_hudUsedH,
                (float)g_hudUsedW / g_hudW, (float)g_hudUsedH / g_hudH, W, H);
    }
    IDirect3DSurface9_Release(bb);
out:
    InterlockedExchange(&g_inDraw, 0);
}

static volatile LONG g_inPresent;

static HRESULT WINAPI Hook_Present(IDirect3DDevice9 *dev, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    ConDraw(dev);
    ConHudDraw(dev);
#if K33_DIAGNOSTICS
    ShotBeforePresent(dev); /* k33shot.c: a pending screenshot or statistics request */
#endif
    InterlockedIncrement(&g_inPresent); /* d3d9 may present through the swap chain's vtable */
    HRESULT hr = g_origPresent(dev, a, b, w, r);
    InterlockedDecrement(&g_inPresent);
    return hr;
}

static HRESULT WINAPI Hook_SCPresent(IDirect3DSwapChain9 *sc, const RECT *a, const RECT *b, HWND w, const RGNDATA *r, DWORD f)
{
    IDirect3DDevice9 *dev = NULL;
    int hudFps, hudPos;
    BOOL hud = ConHudWanted(&hudFps, &hudPos);
#if K33_DIAGNOSTICS
    BOOL shot = ShotWanted(); /* k33shot.c */
#else
    BOOL shot = FALSE;
#endif
    if ((g_conOpen || hud || shot) && !g_inPresent && SUCCEEDED(IDirect3DSwapChain9_GetDevice(sc, &dev)) && dev) {
        ConDraw(dev);
        ConHudDraw(dev);
#if K33_DIAGNOSTICS
        ShotBeforePresent(dev);
#endif
        IDirect3DDevice9_Release(dev);
    }
    return g_origSCPresent(sc, a, b, w, r, f);
}

static void *ConPatchVtable(void *obj, int index, void *hook)
{
    void **vt = *(void ***)obj;
    if (vt[index] == hook) return NULL;
    DWORD old;
    void *orig = vt[index];
    if (!VirtualProtect(&vt[index], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) return NULL;
    vt[index] = hook;
    VirtualProtect(&vt[index], sizeof(void *), old, &old);
    return orig;
}

static HRESULT WINAPI Hook_CreateDevice(IDirect3D9 *d3d, UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
                                        D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    HRESULT hr = g_origCreateDevice(d3d, adapter, type, hwnd, flags, pp, out);
    if (SUCCEEDED(hr) && out && *out) {
        void *p = ConPatchVtable(*out, 17, (void *)Hook_Present);
        if (p) g_origPresent = (PresentFn)p;
        IDirect3DSwapChain9 *sc = NULL;
        if (SUCCEEDED(IDirect3DDevice9_GetSwapChain(*out, 0, &sc)) && sc) {
            p = ConPatchVtable(sc, 3, (void *)Hook_SCPresent);
            if (p) g_origSCPresent = (SCPresentFn)p;
            IDirect3DSwapChain9_Release(sc);
        }
        g_conDrawOk = g_origPresent != NULL;
        Log("console: D3D device %p created (flags 0x%lX, %ux%u), Present %s", *out, (unsigned long)flags,
            pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0, g_origPresent ? "hooked" : "NOT hooked");
    }
    return hr;
}

static IDirect3D9 *WINAPI Hook_Direct3DCreate9(UINT sdk)
{
    IDirect3D9 *d3d = g_origCreate9(sdk);
    if (d3d) {
        void *p = ConPatchVtable(d3d, 16, (void *)Hook_CreateDevice);
        if (p) g_origCreateDevice = (CreateDeviceFn)p;
    }
    return d3d;
}

/* ---- capture thunk and install ----------------------------------------------------------------- */

__attribute__((used)) DWORD g_conLogPrint = CON_LOGPRINT_VA;
void CON_LogHook(void);
__asm__(
    ".text\n"
    ".globl _CON_LogHook\n"
    "_CON_LogHook:\n"                  /* pre-hook: Com_LogPrintMessage, esi = text */
    "    pushal\n"
    "    pushl %esi\n"
    "    call _ConCapture\n"
    "    addl $4, %esp\n"
    "    popal\n"
    "    jmp *_g_conLogPrint\n");

typedef struct { DWORD site; BYTE before[5]; } ConLogSite;

static void InstallConsole(void)
{
    static const ConLogSite kLogSites[] = {
        { 0x005941A4u, { 0x83, 0xFF, 0x09, 0x74, 0x05 } }, /* Com_PrintMessage */
        { 0x005942CEu, { 0x83, 0xFF, 0x09, 0x74, 0x05 } }, /* Com_Printf (inlined print) */
        { 0x005944A2u, { 0x15, 0x08, 0x22, 0xB4, 0x00 } }, /* after OutputDebugStringA */
        { 0x0059B96Eu, { 0x83, 0xFF, 0x09, 0x74, 0x05 } },
    };
    static const BYTE kForEach[] = { 0x68, 0xD0, 0x98, 0xAB, 0x02 };              /* @0x621719 */
    static const BYTE kForEachArr[] = { 0x8B, 0x14, 0xB5, 0xE0, 0x98, 0xAB, 0x02 }; /* @0x621760 */
    static const BYTE kForEachCnt[] = { 0x39, 0x35, 0xC4, 0x98, 0xAB, 0x02 };       /* @0x621751 */
    static const BYTE kWndClass[] = { 0xC7, 0x45, 0xD8, 0x90, 0x35, 0x63, 0x00 };
    static const BYTE kWndProc[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x4C };

    if (GetCommandLineA() && strstr(GetCommandLineA(), "k33_console 0")) {
        Log("console: off (k33_console 0)");
        return;
    }
    InitializeCriticalSection(&g_conLock);
    InitializeCriticalSection(&g_conUiLock);
    g_conWorkEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (g_conWorkEvent) {
        HANDLE worker = CreateThread(NULL, 0, ConRasterWorker, NULL, 0, NULL);
        if (worker) CloseHandle(worker);
        else { CloseHandle(g_conWorkEvent); g_conWorkEvent = NULL; }
    }
    if (!g_conWorkEvent) Log("console: raster worker unavailable; overlay drawing disabled");
    ConInitNames();
    ConSvInit(); /* console_server.c: the generated server table until a hosted server answers */
    for (unsigned i = 0; i < SH1X_DEVSET_COUNT && i + 1 < sizeof(g_argDevset) / sizeof(g_argDevset[0]); i++)
        g_argDevset[i] = kSh1xDevset[i].name;
    g_conReady = TRUE;

    /* output */
    int logHooks = 0;
    C1GameInit();
    BOOL printfOk = g_c1PrintOk;
    for (unsigned i = 0; printfOk && i < sizeof(kLogSites) / sizeof(kLogSites[0]); i++) {
        const BYTE *site = (const BYTE *)(DWORD_PTR)kLogSites[i].site;
        if (!Readable(site - 5, 5) || memcmp(site - 5, kLogSites[i].before, 5) != 0 ||
            !CallsTarget(kLogSites[i].site, CON_LOGPRINT_VA)) {
            Log("console: log call at 0x%08lX does not match, that print path is not captured",
                (unsigned long)kLogSites[i].site);
            continue;
        }
        RedirectCall(kLogSites[i].site, (void *)CON_LogHook);
        logHooks++;
    }
    g_conCaptureOk = logHooks > 0;

    /* commands, dvars, Cbuf */
    g_conDvarsOk = Readable((void *)(DWORD_PTR)(CON_DVAR_FOREACH_VA + 9), 5) &&
                   memcmp((void *)(DWORD_PTR)(CON_DVAR_FOREACH_VA + 9), kForEach, sizeof(kForEach)) == 0 &&
                   memcmp((void *)(DWORD_PTR)(CON_DVAR_FOREACH_VA + 0x50), kForEachArr, sizeof(kForEachArr)) == 0 &&
                   memcmp((void *)(DWORD_PTR)(CON_DVAR_FOREACH_VA + 0x41), kForEachCnt, sizeof(kForEachCnt)) == 0;

    /* input */
    BYTE *wc = (BYTE *)(DWORD_PTR)CON_WNDCLASS_VA;
    BOOL inputOk = FALSE;
    if (Readable(wc, sizeof(kWndClass)) && memcmp(wc, kWndClass, sizeof(kWndClass)) == 0 &&
        Readable((void *)(DWORD_PTR)GAME_WNDPROC_VA, sizeof(kWndProc)) &&
        memcmp((void *)(DWORD_PTR)GAME_WNDPROC_VA, kWndProc, sizeof(kWndProc)) == 0) {
        DWORD proc = (DWORD)(DWORD_PTR)ConsoleWndProc;
        WriteCode(wc + 3, &proc, 4);
        inputOk = TRUE;
    }

    /* drawing */
    BOOL drawHook = FALSE;
    HMODULE d3d9 = ((HMODULE(WINAPI *)(LPCSTR))g_loadLibraryA)("d3d9.dll");
    FARPROC create = d3d9 ? ((FARPROC(WINAPI *)(HMODULE, LPCSTR))g_getProcAddress)(d3d9, "Direct3DCreate9") : NULL;
    FARPROC *slot = (FARPROC *)(DWORD_PTR)CON_D3DCREATE_SLOT;
    if (create && Readable(slot, 4) && *slot == create) {
        g_origCreate9 = (Direct3DCreate9Fn)create;
        FARPROC hook = (FARPROC)Hook_Direct3DCreate9;
        WriteCode(slot, &hook, sizeof(hook));
        drawHook = TRUE;
    }

    (void)inputOk; (void)drawHook; /* only logged, and a shipping build has no log */
    Log("console: server route ready (rcon to the hosted match server only); table: %u server commands, %u server "
        "dvars, %u cheat commands", (unsigned)SH1X_CMD_COUNT, (unsigned)SH1X_DVAR_COUNT, (unsigned)SH1X_CHEAT_COUNT);
    Log("console: output %d/4 print paths, input %s, draw %s, Cbuf_AddText %s, cmd list %s, dvar list %s, "
        "%u dvar names",
        logHooks, inputOk ? "hooked (WndProc)" : "NOT hooked (bytes differ)",
        drawHook ? "hooked (Direct3DCreate9)" : "NOT hooked", g_c1CbufOk ? "ok" : "bytes differ",
        g_c1CmdsOk ? "ok" : "bytes differ", g_conDvarsOk ? "ok" : "bytes differ", (unsigned)C1_DVAR_NAME_COUNT);
}

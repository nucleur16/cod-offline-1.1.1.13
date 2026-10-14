/*
 * Developer console (dev branch). Included by kernel33.c after parity.c, so it shares Log/Readable/
 * WriteCode/CallsTarget/RedirectCall and the parity logger's helpers.
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
 *           the console opens stays down. IME results go into the console line as GBK while it is
 *           open; otherwise the GBK patch (ImeDefWindowProc) works exactly as before.
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
#include <d3d9.h>
#include "c1_dvars.h"

#define CON_COM_PRINTF_VA     0x005941B0u
#define CON_LOGPRINT_VA       0x00594010u
#define CON_CBUF_VA           0x0058BA60u
#define CON_CMDFIND_VA        0x0058C790u
#define CON_CMD_LIST_VA       0x024F6424u
#define CON_DVAR_FOREACH_VA   0x00621710u
#define CON_DVAR_ARRAY_VA     0x02AB98E0u
#define CON_DVAR_COUNT_VA     0x02AB98C4u
#define CON_DVAR_LOCK_VA      0x02AB98D0u
#define CON_DVAR_BUSY_VA      0x02AB98D4u
#define CON_WNDCLASS_VA       0x0062FB85u  /* mov dword [ebp-28h], 0x633590 */
#define CON_D3DCREATE_SLOT    0x00B42598u
#define CON_REGISTER_INT_VA   0x00624420u
#define CON_CHANNEL           8            /* logfile_only: the log and this console, not the HUD */

#define CON_LINES     4096
#define CON_LINE_LEN  320
#define CON_INPUT_LEN 256
#define CON_HISTORY   200
#define CON_SUGGEST   256
#define CON_SHOWN     14

/* ---- state ------------------------------------------------------------------------------------ */

static CRITICAL_SECTION g_conLock;
static BOOL g_conReady, g_conCaptureOk, g_conCbufOk, g_conDvarsOk, g_conCmdsOk, g_conDrawOk;
static volatile LONG g_conOpen, g_conDirty = 1;
static char g_conLines[CON_LINES][CON_LINE_LEN];
static int g_conHead, g_conCount;
static char g_conPartial[CON_LINE_LEN];
static int g_conPartialLen;
static int g_conScroll, g_conRows = 20;

static char g_conInput[CON_INPUT_LEN];
static int g_conInputLen, g_conCursor;
static char g_conHist[CON_HISTORY][CON_INPUT_LEN];
static int g_conHistCount, g_conHistPos;
static BOOL g_conHistLoaded, g_conEatChar, g_conBanner;
static const BYTE *volatile g_conKeyDvar;
static BOOL g_conKeyDvarTried;

typedef struct {
    char name[80];   /* what Tab inserts */
    char value[72];
    char info[100];
    BYTE kind;       /* 0 dvar, 1 command */
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
    LeaveCriticalSection(&g_conLock);
    InterlockedExchange(&g_conDirty, 1);
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
        ((void(__cdecl *)(int, const char *, ...))(DWORD_PTR)CON_COM_PRINTF_VA)(CON_CHANNEL, "%s", buf);
    else
        ConAppend(buf);
}

/* ---- C1 access (main thread) --------------------------------------------------------------------- */

static void ConCbufAddText(const char *text)
{
    int lc = 0;
    DWORD fn = CON_CBUF_VA;
    __asm__ volatile("call *%[fn]" : "+a"(text), "+c"(lc) : [fn] "r"(fn) : "edx", "memory", "cc");
}

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
    int n = *(volatile int *)(DWORD_PTR)CON_DVAR_COUNT_VA;
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
static const char *const kBuiltins[] = { "clear", "cmdlist", "condump", "dvarlist", "find", "k33help" };
#define BUILTIN_COUNT (sizeof(kBuiltins) / sizeof(kBuiltins[0]))

static BOOL ConIsSetCommand(const char *t)
{
    return !_stricmp(t, "set") || !_stricmp(t, "seta") || !_stricmp(t, "reset") || !_stricmp(t, "toggle") ||
           !_stricmp(t, "togglep") || !_stricmp(t, "setfromdvar") || !_stricmp(t, "sets") || !_stricmp(t, "setu");
}

/* ---- suggestions ---------------------------------------------------------------------------------- */

typedef struct { const char *shown; const BYTE *dvar; const char *cmd; BYTE prefix; } ConMatch;
static ConMatch g_matches[CON_MAX_DVARS + 1024];

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
    char val[96], def[96], dom[200];
    const char *raw = ConDvarRawName(d);
    const C1DvarName *n = DvarNameById(raw);
    ConDvarValue(d, 0x0C, val, sizeof(val));
    ConDvarValue(d, 0x2C, def, sizeof(def));
    ConDvarDomain(d, dom, sizeof(dom));
    DWORD flags = *(const DWORD *)(d + 4);
    int k = 0;
    if (n)
        _snprintf(g_detail[k++], 200, "%s  (C1 \"%s\")  %s%s", n->name, raw, d[8] < 10 ? kTypeNames[d[8]] : "?",
                  n->conf < 85 ? "  [name is a positional guess]" : "");
    else
        _snprintf(g_detail[k++], 200, "%s  %s", raw, d[8] < 10 ? kTypeNames[d[8]] : "?");
    _snprintf(g_detail[k++], 200, "value \"%s\"   default \"%s\"   flags 0x%lX%s%s%s%s", val, def, (unsigned long)flags,
              flags & 1 ? " archive" : "", flags & 4 ? " cheat" : "", flags & 0x2000 ? " read-only" : "",
              flags & 0x800 ? " init" : "");
    _snprintf(g_detail[k++], 200, "domain %s", dom[0] ? dom : "?");
    if (n && n->desc && n->desc[0]) _snprintf(g_detail[k++], 200, "%s", n->desc);
    for (int i = 0; i < k; i++) {
        g_detail[i][199] = 0;
        for (char *c = g_detail[i]; *c; c++)
            if (*c == '\n') *c = ' ';
    }
    g_detailCount = k;
}

/* Recomputes the dropdown for the token under the cursor. Main thread. */
static void ConUpdateSuggestions(void)
{
    ConTokens t;
    char tok0[96], part[96];
    int dcount = ConSnapshotDvars();
    ConTokenize(g_conInput, g_conInputLen, g_conCursor, &t);
    ConTokenText(&t, 0, tok0, sizeof(tok0));

    EnterCriticalSection(&g_conLock);
    g_suggCount = g_suggTotal = 0;
    g_suggSel = -1;
    g_detailCount = 0;
    g_suggCycling = FALSE;
    LeaveCriticalSection(&g_conLock);

    BOOL nameSlot = t.cur == 0 || (t.cur >= 1 && ConIsSetCommand(tok0) &&
                                   (t.cur == 1 || (t.cur == 2 && !_stricmp(tok0, "setfromdvar"))));
    if (!nameSlot) {
        /* typing a value: show the dvar it is for */
        char name[96];
        const BYTE *d = NULL;
        if (t.cur >= 1 && t.count >= 1) {
            if (ConIsSetCommand(tok0) && t.count >= 2) ConTokenText(&t, 1, name, sizeof(name));
            else lstrcpynA(name, tok0, sizeof(name));
            d = ConFindDvar(name, dcount);
        }
        EnterCriticalSection(&g_conLock);
        if (d) ConDetailFor(d);
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
        m++;
    }
    if (t.cur == 0) {
        for (const ConCmd *c = g_conCmdsOk ? *(ConCmd *const *)(DWORD_PTR)CON_CMD_LIST_VA : NULL;
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
            m++;
        }
        for (unsigned b = 0; b < BUILTIN_COUNT; b++) {
            BYTE prefix = _strnicmp(kBuiltins[b], part, plen) == 0;
            if (!prefix && !ConStrIStr(kBuiltins[b], part)) continue;
            g_matches[m].shown = kBuiltins[b];
            g_matches[m].dvar = NULL;
            g_matches[m].cmd = kBuiltins[b];
            g_matches[m].prefix = prefix;
            m++;
        }
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
        } else {
            s->kind = 1;
            s->value[0] = 0;
            lstrcpynA(s->info, "command", sizeof(s->info));
        }
    }
    /* exactly one dvar typed in full: its details too */
    if (g_suggCount >= 1 && g_sugg[0].kind == 0 && _stricmp(g_sugg[0].name, part) == 0) ConDetailFor(g_matches[0].dvar);
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
        int n = g_suggCount < CON_SHOWN * 4 ? g_suggCount : CON_SHOWN * 4;
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

static void ConLoadHistory(void)
{
    char path[MAX_PATH], line[CON_INPUT_LEN + 4];
    g_conHistLoaded = TRUE;
    FILE *f = fopen(ConHistoryPath(path), "rb");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        size_t l = strlen(line);
        while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
        if (!l) continue;
        if (g_conHistCount == CON_HISTORY) {
            memmove(g_conHist[0], g_conHist[1], sizeof(g_conHist[0]) * (CON_HISTORY - 1));
            g_conHistCount--;
        }
        lstrcpynA(g_conHist[g_conHistCount++], line, CON_INPUT_LEN);
    }
    fclose(f);
    g_conHistPos = g_conHistCount;
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
    char path[MAX_PATH];
    FILE *f = fopen(ConHistoryPath(path), "wb");
    if (!f) return;
    for (int i = 0; i < g_conHistCount; i++) fprintf(f, "%s\r\n", g_conHist[i]);
    fclose(f);
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
}

static void ConCmdList(const char *filter)
{
    static const char *names[4096];
    int n = 0;
    for (const ConCmd *c = g_conCmdsOk ? *(ConCmd *const *)(DWORD_PTR)CON_CMD_LIST_VA : NULL; c && n < 4096; c = c->next) {
        if (!Readable(c, sizeof(*c)) || !Readable(c->name, 1)) break;
        names[n++] = c->name;
    }
    for (unsigned b = 0; b < BUILTIN_COUNT && n < 4096; b++) names[n++] = kBuiltins[b];
    qsort(names, n, sizeof(names[0]), ConCmpStr);
    int shown = 0;
    for (int i = 0; i < n; i++) {
        if (i && !_stricmp(names[i], names[i - 1])) continue;
        if (filter && *filter && !ConStrIStr(names[i], filter)) continue;
        ConPrintf("  %s\n", names[i]);
        shown++;
    }
    ConPrintf("%d commands%s%s\n", shown, filter && *filter ? " matching " : "", filter && *filter ? filter : "");
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
    ConPrintf("Kernel33 developer console - %u readable dvar names (c1_dvars.h)\n", (unsigned)C1_DVAR_NAME_COUNT);
    ConPrintf("  Type a command or a dvar by its readable name: cg_drawFPS 1, com_maxfps 0, set cg_fov 80.\n");
    ConPrintf("  Tab completes the common prefix, then cycles the matches (Shift+Tab back). Up/Down: history\n");
    ConPrintf("  (main\\k33_console_history.txt). PgUp/PgDn/wheel scroll, Ctrl+Home/End top/bottom, Esc closes.\n");
    ConPrintf("  Built-ins: dvarlist [filter], cmdlist [filter], find <text>, clear, condump [file], k33help.\n");
    ConPrintf("  Toggle: the key left of 1, or the virtual key in k33_consoleKey (default 112 = F1, 0 = none).\n");
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
    return nm && !exists;
}

static void ConExecute(const char *line)
{
    char out[1200];
    int o = 0;
    BOOL mapped = FALSE;
    int dcount = ConSnapshotDvars();
    ConPrintf("]%s\n", line);

    /* split on ';' outside quotes; built-ins run here, the rest goes to Cbuf with names mapped */
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

        char rebuilt[CON_INPUT_LEN + 64];
        if (ConIsSetCommand(t0)) {
            int r;
            ConReadToken(seg, &p, t1, sizeof(t1));
            mapped |= ConMapName(t1, m1, sizeof(m1), dcount);
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
    if (g_conCbufOk) ConCbufAddText(out);
    else ConPrintf("Cbuf_AddText did not match, cannot execute\n");
}

/* ---- keyboard (main thread) ------------------------------------------------------------------------ */

static void ConRegisterKeyDvar(void)
{
    g_conKeyDvarTried = TRUE;
    typedef const BYTE *(__cdecl *RegisterIntFn)(const char *, int, int, int, unsigned, const char *);
    if (!Readable((void *)(DWORD_PTR)CON_REGISTER_INT_VA, 1)) return;
    const BYTE *d = ((RegisterIntFn)(DWORD_PTR)CON_REGISTER_INT_VA)(
        "k33_consoleKey", VK_F1, 0, 255, 0x1 /* archive */,
        "Kernel33 console: extra toggle key (Windows virtual-key code; 112 F1, 0 none)");
    if (d && Readable(d, 0x10)) g_conKeyDvar = d;
}

static BOOL ConIsToggle(WPARAM vk, LPARAM lp)
{
    if (((lp >> 16) & 0xFF) == 0x29 && !(lp & (1 << 24))) return TRUE; /* the key left of 1 */
    int extra = g_conKeyDvar ? *(volatile const int *)(g_conKeyDvar + 0xC) : VK_F1;
    return extra > 0 && (int)vk == extra;
}

static void ConSetOpen(BOOL open)
{
    InterlockedExchange(&g_conOpen, open);
    InterlockedExchange(&g_conDirty, 1);
    if (open) {
        if (!g_conHistLoaded) ConLoadHistory();
        if (!g_conBanner) {
            g_conBanner = TRUE;
            ConPrintf("Kernel33 developer console (dev branch): type k33help. %u readable dvar names; "
                      "hooks: output %s, input ok, draw %s, execute %s\n", (unsigned)C1_DVAR_NAME_COUNT,
                      g_conCaptureOk ? "ok" : "OFF", g_conDrawOk ? "ok" : "OFF", g_conCbufOk ? "ok" : "OFF");
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

static void ConKeyDown(HWND hwnd, WPARAM vk)
{
    BOOL ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
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
        ConTab(shift);
        return;
    case VK_BACK:
        if (ctrl) {
            int s = g_conCursor;
            while (s > 0 && g_conInput[s - 1] == ' ') s--;
            while (s > 0 && g_conInput[s - 1] != ' ') s--;
            ConReplaceRange(s, g_conCursor, "");
        } else if (g_conCursor > 0) {
            ConReplaceRange(g_conCursor - 1, g_conCursor, "");
        }
        edited = TRUE;
        break;
    case VK_DELETE:
        if (g_conCursor < g_conInputLen) { int c = g_conCursor; ConReplaceRange(c, c + 1, ""); g_conCursor = c; }
        edited = TRUE;
        break;
    case VK_LEFT:
        if (ctrl) { while (g_conCursor > 0 && g_conInput[g_conCursor - 1] == ' ') g_conCursor--;
                    while (g_conCursor > 0 && g_conInput[g_conCursor - 1] != ' ') g_conCursor--; }
        else if (g_conCursor > 0) g_conCursor--;
        edited = TRUE;
        break;
    case VK_RIGHT:
        if (ctrl) { while (g_conCursor < g_conInputLen && g_conInput[g_conCursor] != ' ') g_conCursor++;
                    while (g_conCursor < g_conInputLen && g_conInput[g_conCursor] == ' ') g_conCursor++; }
        else if (g_conCursor < g_conInputLen) g_conCursor++;
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
        EnterCriticalSection(&g_conLock);
        g_conScroll += g_conRows > 4 ? g_conRows - 2 : 2;
        LeaveCriticalSection(&g_conLock);
        break;
    case VK_NEXT:
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

static LRESULT CALLBACK ConsoleWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    WNDPROC game = (WNDPROC)(DWORD_PTR)GAME_WNDPROC_VA;
    if (!g_conKeyDvarTried && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) &&
        *(volatile int *)(DWORD_PTR)CON_DVAR_COUNT_VA > 100)
        ConRegisterKeyDvar();
    BOOL open = g_conOpen != 0;
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (msg == WM_SYSKEYDOWN && (wp == VK_F4 || wp == VK_TAB || wp == VK_RETURN)) break; /* Alt+F4/Tab/Enter */
        if (ConIsToggle(wp, lp)) {
            if (!(lp & (1 << 30))) ConSetOpen(!open); /* not on auto-repeat */
            g_conEatChar = ((lp >> 16) & 0xFF) == 0x29;
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
            if (msg == WM_CHAR && (BYTE)wp >= 0x20 && wp != 0x7F) {
                char c[2] = { (char)wp, 0 };
                if (g_suggCycling) g_suggCycling = FALSE;
                ConInsert(c);
                ConUpdateSuggestions();
            }
            return 0;
        }
        break;
    case WM_DEADCHAR:
        if (open) return 0;
        break;
    case WM_MOUSEWHEEL:
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
static DWORD *g_bits;
static int g_dibW, g_dibH, g_usedH, g_fontH;
static HFONT g_font;

typedef struct {
    char lines[96][CON_LINE_LEN];
    int lineCount, scroll, total;
    char input[CON_INPUT_LEN];
    int cursor;
    ConSuggest sugg[CON_SHOWN];
    int suggCount, suggTotal, suggSel, suggFirst;
    char detail[5][200];
    int detailCount;
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
    int fh = h / 56;
    if (fh < 13) fh = 13;
    if (fh > 40) fh = 40;
    if (g_dc && g_dibW == w && g_dibH == h && g_fontH == fh) return TRUE;
    if (!g_dc) g_dc = CreateCompatibleDC(NULL);
    if (!g_dc) return FALSE;
    if (g_dib) { DeleteObject(g_dib); g_dib = NULL; }
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
    SelectObject(g_dc, g_dib);
    if (g_font) DeleteObject(g_font);
    g_font = CreateFontA(-fh, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SelectObject(g_dc, g_font);
    SetBkMode(g_dc, TRANSPARENT);
    g_dibW = w;
    g_dibH = h;
    g_fontH = fh;
    return TRUE;
}

/* Renders the snapshot into the DIB; returns the height used. */
static int ConRender(int W, int H)
{
    ConSnap *s = &g_snap;
    int lh = g_fontH + 3, pad = 6;
    int panelH = (H * 45 / 100) / lh * lh;
    if (panelH < lh * 8) panelH = lh * 8;
    int rows = (panelH - 2 * lh - 2 * pad) / lh;
    g_conRows = rows;

    int maxUsed = panelH + 2 + (CON_SHOWN + 6) * lh + pad;
    if (maxUsed > H) maxUsed = H;
    memset(g_bits, 0, (size_t)W * maxUsed * 4);
    ConFill(0, 0, W, panelH, RGB(10, 12, 18));
    ConFill(0, panelH - 2, W, panelH, RGB(70, 130, 255));
    ConFill(0, panelH - 2 - lh - pad, W, panelH - 2, RGB(22, 26, 36));

    /* title */
    char title[200];
    _snprintf(title, sizeof(title), "Kernel33 dev console  |  %d lines%s  |  ~ or F1 toggle, Tab complete, Up/Down history, PgUp/PgDn scroll",
              s->total, s->scroll ? " (scrolled)" : "");
    title[sizeof(title) - 1] = 0;
    ConText(pad, 2, title, RGB(90, 110, 150), W - pad);

    /* output, bottom up */
    int y = panelH - 2 - lh - pad - lh;
    for (int i = s->lineCount - 1; i >= 0 && y >= lh + 2; i--, y -= lh)
        ConText(pad, y, s->lines[i], RGB(220, 220, 220), W - pad);

    /* input */
    int iy = panelH - 2 - lh - pad / 2;
    int x = ConText(pad, iy, "]", RGB(110, 170, 255), W);
    int ix = x + 4;
    SetTextColor(g_dc, RGB(255, 255, 255));
    {
        WCHAR w[CON_INPUT_LEN + 8];
        int wn = MultiByteToWideChar(GAME_CODEPAGE, 0, s->input, (int)strlen(s->input), w, CON_INPUT_LEN + 8);
        if (wn > 0) TextOutW(g_dc, ix, iy, w, wn);
    }
    int cx = ix + ConTextWidth(s->input, s->cursor);
    ConFill(cx, iy + 1, cx + 2, iy + lh - 1, RGB(255, 210, 90));
    int used = panelH;

    /* dropdown */
    int rowsShown = s->suggCount + (s->suggTotal > s->suggCount ? 1 : 0) + s->detailCount;
    if (rowsShown) {
        int bx = ix, by = panelH + 2, bw = W - ix - pad;
        int bh = rowsShown * lh + pad;
        if (by + bh > H) bh = H - by;
        ConFill(bx, by, bx + bw, by + bh, RGB(18, 20, 28));
        ConFill(bx, by, bx + 2, by + bh, RGB(70, 130, 255));
        int ry = by + pad / 2;
        int nameW = 0;
        for (int i = 0; i < s->suggCount; i++) {
            int w = ConTextWidth(s->sugg[i].name, (int)strlen(s->sugg[i].name));
            if (w > nameW) nameW = w;
        }
        nameW += g_fontH * 2;
        if (nameW > bw / 2) nameW = bw / 2;
        for (int i = 0; i < s->suggCount && ry + lh <= by + bh; i++, ry += lh) {
            const ConSuggest *g = &s->sugg[i];
            if (s->suggFirst + i == s->suggSel) ConFill(bx + 2, ry, bx + bw, ry + lh, RGB(45, 65, 115));
            ConText(bx + pad, ry, g->name, g->kind ? RGB(170, 195, 255) : (g->prefix ? RGB(255, 255, 255) : RGB(200, 200, 200)),
                    bx + nameW);
            int vx = bx + pad + nameW;
            if (g->kind == 0) {
                char v[90];
                _snprintf(v, sizeof(v), "\"%s^7\"", g->value);
                v[sizeof(v) - 1] = 0;
                int ex = ConText(vx, ry, v, RGB(140, 230, 140), bx + bw - pad);
                ConText(ex + g_fontH, ry, g->info, RGB(130, 130, 155), bx + bw - pad);
            } else {
                ConText(vx, ry, g->info, RGB(110, 120, 160), bx + bw - pad);
            }
        }
        if (s->suggTotal > s->suggCount && ry + lh <= by + bh) {
            char more[96];
            _snprintf(more, sizeof(more), "... %d more (keep typing, or Tab to cycle)", s->suggTotal - s->suggCount);
            more[sizeof(more) - 1] = 0;
            ConText(bx + pad, ry, more, RGB(120, 120, 140), bx + bw - pad);
            ry += lh;
        }
        for (int i = 0; i < s->detailCount && ry + lh <= by + bh; i++, ry += lh)
            ConText(bx + pad, ry, s->detail[i], i == 0 ? RGB(255, 230, 140) : RGB(190, 200, 215), bx + bw - pad);
        used = by + bh;
    }
    GdiFlush();

    /* alpha: GDI leaves it 0; panel and dropdown are opaque-ish, text fully */
    for (int yy = 0; yy < used; yy++) {
        DWORD *row = g_bits + (size_t)yy * W;
        for (int xx = 0; xx < W; xx++) {
            DWORD p = row[xx];
            if (!p) continue;
            BYTE r = (BYTE)(p >> 16), g = (BYTE)(p >> 8), b = (BYTE)p;
            BYTE m = r > g ? (r > b ? r : b) : (g > b ? g : b);
            row[xx] = p | ((DWORD)(m > 60 ? 0xFF : 0xE8) << 24);
        }
    }
    return used;
}

static void ConTakeSnapshot(void)
{
    ConSnap *s = &g_snap;
    EnterCriticalSection(&g_conLock);
    int rows = g_conRows > 0 && g_conRows < 96 ? g_conRows : 20;
    if (g_conScroll > g_conCount - 1) g_conScroll = g_conCount > 0 ? g_conCount - 1 : 0;
    int end = g_conCount - g_conScroll;
    int start = end - rows;
    if (start < 0) start = 0;
    s->lineCount = 0;
    for (int i = start; i < end; i++) lstrcpynA(s->lines[s->lineCount++], g_conLines[(g_conHead + i) % CON_LINES], CON_LINE_LEN);
    s->scroll = g_conScroll;
    s->total = g_conCount;
    lstrcpynA(s->input, g_conInput, CON_INPUT_LEN);
    s->cursor = g_conCursor;
    int first = 0;
    if (g_suggSel >= CON_SHOWN) first = g_suggSel - CON_SHOWN + 1;
    s->suggFirst = first;
    s->suggCount = g_suggCount - first < CON_SHOWN ? g_suggCount - first : CON_SHOWN;
    if (s->suggCount < 0) s->suggCount = 0;
    memcpy(s->sugg, g_sugg + first, s->suggCount * sizeof(ConSuggest));
    s->suggTotal = g_suggTotal - first;
    s->suggSel = g_suggSel;
    s->detailCount = g_detailCount;
    memcpy(s->detail, g_detail, sizeof(g_detail));
    LeaveCriticalSection(&g_conLock);
}

typedef struct { float x, y, z, rhw; DWORD color; float u, v; } ConVertex;

static void ConDraw(IDirect3DDevice9 *dev)
{
    static LONG failures;
    if (!g_conOpen || InterlockedExchange(&g_inDraw, 1)) return;
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
        InterlockedExchange(&g_conDirty, 1);
    }
    if (InterlockedExchange(&g_conDirty, 0)) {
        if (!ConEnsureDib(W, H)) { IDirect3DSurface9_Release(bb); goto out; }
        ConTakeSnapshot();
        g_usedH = ConRender(W, H);
        D3DLOCKED_RECT lr;
        RECT rc = { 0, 0, W, g_usedH };
        if (g_usedH > 0 && SUCCEEDED(IDirect3DTexture9_LockRect(g_tex, 0, &lr, &rc, 0))) {
            for (int y = 0; y < g_usedH; y++) memcpy((BYTE *)lr.pBits + (size_t)y * lr.Pitch, g_bits + (size_t)y * W, (size_t)W * 4);
            IDirect3DTexture9_UnlockRect(g_tex, 0);
        }
    }

    IDirect3DStateBlock9 *sb = NULL;
    if (FAILED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &sb)) || !sb) {
        if (InterlockedIncrement(&failures) <= 3) Log("console: CreateStateBlock failed, not drawing");
        IDirect3DSurface9_Release(bb);
        goto out;
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
        IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)g_tex);
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
        float h = (float)g_usedH, v = (float)g_usedH / (float)H;
        ConVertex q[4] = {
            { -0.5f, -0.5f, 0, 1, 0xFFFFFFFF, 0, 0 },
            { W - 0.5f, -0.5f, 0, 1, 0xFFFFFFFF, 1, 0 },
            { -0.5f, h - 0.5f, 0, 1, 0xFFFFFFFF, 0, v },
            { W - 0.5f, h - 0.5f, 0, 1, 0xFFFFFFFF, 1, v },
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
    IDirect3DSurface9_Release(bb);
out:
    InterlockedExchange(&g_inDraw, 0);
}

static volatile LONG g_inPresent;

static HRESULT WINAPI Hook_Present(IDirect3DDevice9 *dev, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    ConDraw(dev);
    InterlockedIncrement(&g_inPresent); /* d3d9 may present through the swap chain's vtable */
    HRESULT hr = g_origPresent(dev, a, b, w, r);
    InterlockedDecrement(&g_inPresent);
    return hr;
}

static HRESULT WINAPI Hook_SCPresent(IDirect3DSwapChain9 *sc, const RECT *a, const RECT *b, HWND w, const RGNDATA *r, DWORD f)
{
    IDirect3DDevice9 *dev = NULL;
    if (g_conOpen && !g_inPresent && SUCCEEDED(IDirect3DSwapChain9_GetDevice(sc, &dev)) && dev) {
        ConDraw(dev);
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
    static const BYTE kCbuf[] = { 0x53, 0x56, 0x57, 0x68, 0xE0, 0x2D, 0xB6, 0x02, 0x8B, 0xF0, 0x8B, 0xF9 };
    static const BYTE kCmdFind[] = { 0xA1, 0x24, 0x64, 0x4F, 0x02, 0x53, 0x85, 0xC0 };
    static const BYTE kForEach[] = { 0x68, 0xD0, 0x98, 0xAB, 0x02 };              /* @0x621719 */
    static const BYTE kForEachArr[] = { 0x8B, 0x14, 0xB5, 0xE0, 0x98, 0xAB, 0x02 }; /* @0x621760 */
    static const BYTE kForEachCnt[] = { 0x39, 0x35, 0xC4, 0x98, 0xAB, 0x02 };       /* @0x621751 */
    static const BYTE kWndClass[] = { 0xC7, 0x45, 0xD8, 0x90, 0x35, 0x63, 0x00 };
    static const BYTE kWndProc[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x4C };
    static const BYTE kComPrintf[] = { 0x55, 0x8B, 0xEC, 0xB8, 0x28, 0x20, 0x00, 0x00 };

    if (GetCommandLineA() && strstr(GetCommandLineA(), "k33_console 0")) {
        Log("console: off (k33_console 0)");
        return;
    }
    InitializeCriticalSection(&g_conLock);
    ConInitNames();
    g_conReady = TRUE;

    /* output */
    int logHooks = 0;
    BOOL printfOk = Readable((void *)(DWORD_PTR)CON_COM_PRINTF_VA, sizeof(kComPrintf)) &&
                    memcmp((void *)(DWORD_PTR)CON_COM_PRINTF_VA, kComPrintf, sizeof(kComPrintf)) == 0;
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
    g_conCbufOk = Readable((void *)(DWORD_PTR)CON_CBUF_VA, sizeof(kCbuf)) &&
                  memcmp((void *)(DWORD_PTR)CON_CBUF_VA, kCbuf, sizeof(kCbuf)) == 0;
    g_conCmdsOk = Readable((void *)(DWORD_PTR)CON_CMDFIND_VA, sizeof(kCmdFind)) &&
                  memcmp((void *)(DWORD_PTR)CON_CMDFIND_VA, kCmdFind, sizeof(kCmdFind)) == 0;
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

    Log("console: output %d/4 print paths, input %s, draw %s, Cbuf_AddText %s, cmd list %s, dvar list %s; %u dvar names",
        logHooks, inputOk ? "hooked (WndProc)" : "NOT hooked (bytes differ)",
        drawHook ? "hooked (Direct3DCreate9)" : "NOT hooked", g_conCbufOk ? "ok" : "bytes differ",
        g_conCmdsOk ? "ok" : "bytes differ", g_conDvarsOk ? "ok" : "bytes differ", (unsigned)C1_DVAR_NAME_COUNT);
}

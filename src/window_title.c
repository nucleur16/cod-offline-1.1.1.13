/*
 * window_title.c - C1's window titles and message boxes in Unicode (included by kernel33.c after locale_switch.c).
 *
 * C1 is an ANSI program whose strings are GBK (code page 936). Its window title is the string dvar "1436"
 * (registered at 0xA7B417 with 0xC71934 "使命召唤online") that R_CreateWindow passes to CreateWindowExA
 * (0xA79DBF, borderless.c); the console window is created with "使命召唤online Multiplayer" (0x632957), the
 * "already running" and fatal error message boxes use "使命召唤online ..." captions (0x630C59, 0x631324). On a
 * Windows whose ANSI code page is not 936, the A APIs read those bytes in that code page: the title bar showed
 * "Ê¹ÃüÕÙ»½online".
 *
 * So C1's USER32 import slots (filled by kernel33.c FillImports) for CreateWindowExA, SetWindowTextA,
 * MessageBoxA, MessageBoxW, FindWindowA and GetWindowTextA go through the functions below when a string has a byte >= 0x80:
 * the text is read as GBK (MultiByteToWideChar 936). A top-level window's title is stored as Unicode with
 * DefWindowProcW(WM_SETTEXT) (C1's windows are ANSI windows: SetWindowTextW would reach their window
 * procedure as an ANSI WM_SETTEXT and lose the characters again); message boxes are MessageBoxW. Pure ASCII
 * strings and child windows (the console's edit controls) take C1's own path unchanged.
 *
 * Language. While the language on screen is English (loc_languageName en, the default), the game's name in
 * those strings is "Call of Duty Online" instead of "使命召唤online"; any other language keeps the Chinese name.
 * A language switch while the game runs retitles the windows (WindowTitleTick, from BorderlessTick).
 *
 * C1 compares window titles with its GBK bytes (the error path's EnumWindows callback 0x6126E0 keeps the window
 * titled "使命召唤online Multiplayer" visible; FindWindowA finds a running instance): GetWindowTextA of a window
 * titled here returns the GBK bytes C1 gave it, and FindWindowA looks for either spelling of the name.
 */

#define WT_GBK_CP     936u
#define WT_MAX_TITLES 8

typedef HWND(WINAPI *WtCreateWindowExAFn)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef BOOL(WINAPI *WtSetWindowTextAFn)(HWND, LPCSTR);
typedef int(WINAPI *WtMessageBoxAFn)(HWND, LPCSTR, LPCSTR, UINT);
typedef int(WINAPI *WtMessageBoxWFn)(HWND, LPCWSTR, LPCWSTR, UINT);
typedef HWND(WINAPI *WtFindWindowAFn)(LPCSTR, LPCSTR);
typedef int(WINAPI *WtGetWindowTextAFn)(HWND, LPSTR, int);

static WtCreateWindowExAFn g_wtCreateWindowExA;
static WtSetWindowTextAFn g_wtSetWindowTextA;
static WtMessageBoxAFn g_wtMessageBoxA;
static WtMessageBoxWFn g_wtMessageBoxW;
static WtFindWindowAFn g_wtFindWindowA;
static WtGetWindowTextAFn g_wtGetWindowTextA;

/* Top-level windows titled here: the GBK title C1 gave each one. */
static struct {
    HWND window;
    char gbk[256];
} g_wtTitles[WT_MAX_TITLES];
static CRITICAL_SECTION g_wtLock;
static volatile LONG g_wtLockReady;
static int g_wtEnglishApplied = -1;   /* the language the titles were last written in: -1 none, 0 Chinese, 1 English */

static const wchar_t kWtChineseName[] = L"\x4F7F\x547D\x53EC\x5524online";   /* 使命召唤online */
static const wchar_t kWtEnglishName[] = L"Call of Duty Online";

static void WtLockInit(void)
{
    if (InterlockedCompareExchange(&g_wtLockReady, 1, 0) == 0) {
        InitializeCriticalSection(&g_wtLock);
        InterlockedExchange(&g_wtLockReady, 2);
    }
    while (g_wtLockReady != 2) Sleep(0);
}

static BOOL WtHasHigh(LPCSTR s)
{
    if (!s || IS_INTRESOURCE(s)) return FALSE;
    for (; *s; s++)
        if ((BYTE)*s >= 0x80) return TRUE;
    return FALSE;
}

/* The language on screen is English (locale_switch.c's g_lsShown; before it is known, the startup language). */
static BOOL WtEnglish(void)
{
    LONG shown = g_lsShown;
    if (shown >= 0 && shown < g_langCount) return strcmp(g_langs[shown], "en") == 0;
    return g_english;
}

static wchar_t *WtEnglishName(const wchar_t *w);

/* GBK -> UTF-16 on the process heap (HeapFree it), with the game's name in the language asked for. */
static wchar_t *WtWiden(LPCSTR s, BOOL english)
{
    int n = MultiByteToWideChar(WT_GBK_CP, 0, s, -1, NULL, 0);
    UINT cp = WT_GBK_CP;
    if (n <= 0) {
        cp = CP_ACP;
        n = MultiByteToWideChar(cp, 0, s, -1, NULL, 0);
        if (n <= 0) return NULL;
    }
    wchar_t *w = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)n * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(cp, 0, s, -1, w, n);
    if (!english) return w;
    wchar_t *r = WtEnglishName(w);
    if (!r) return w;
    HeapFree(GetProcessHeap(), 0, w);
    return r;
}

/* w with the game's Chinese name (kWtChineseName) as "Call of Duty Online", on the process heap; NULL when w has none. */
static wchar_t *WtEnglishName(const wchar_t *w)
{
    if (!w || !wcsstr(w, kWtChineseName)) return NULL;

    const size_t from = wcslen(kWtChineseName), to = wcslen(kWtEnglishName);
    size_t hits = 0;
    for (const wchar_t *p = w; (p = wcsstr(p, kWtChineseName)) != NULL; p += from) hits++;
    size_t len = wcslen(w) + hits * (to - from) + 1;
    wchar_t *r = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, len * sizeof(wchar_t));
    if (!r) return NULL;
    wchar_t *o = r;
    for (const wchar_t *p = w;;) {
        const wchar_t *q = wcsstr(p, kWtChineseName);
        size_t k = q ? (size_t)(q - p) : wcslen(p);
        memcpy(o, p, k * sizeof(wchar_t));
        o += k;
        if (!q) break;
        memcpy(o, kWtEnglishName, to * sizeof(wchar_t));
        o += to;
        p = q + from;
    }
    *o = 0;
    return r;
}

static void WtFree(wchar_t *w)
{
    if (w) HeapFree(GetProcessHeap(), 0, w);
}

static BOOL WtTopLevel(HWND window)
{
    return window && IsWindow(window) && !(GetWindowLongW(window, GWL_STYLE) & WS_CHILD);
}

/* Stores gbk as the title of window (a top-level window of this thread) in Unicode. */
static BOOL WtApply(HWND window, LPCSTR gbk)
{
    DWORD pid = 0;
    if (GetWindowThreadProcessId(window, &pid) != GetCurrentThreadId()) return FALSE;
    BOOL english = WtEnglish();
    wchar_t *w = WtWiden(gbk, english);
    if (!w) return FALSE;
    BOOL ok = IsWindowUnicode(window) ? SetWindowTextW(window, w) : (BOOL)DefWindowProcW(window, WM_SETTEXT, 0, (LPARAM)w);
    WtFree(w);
    if (!ok) return FALSE;

    WtLockInit();
    EnterCriticalSection(&g_wtLock);
    int slot = -1;
    for (int i = 0; i < WT_MAX_TITLES; i++) {
        if (g_wtTitles[i].window == window) { slot = i; break; }
        if (slot < 0 && (!g_wtTitles[i].window || !IsWindow(g_wtTitles[i].window))) slot = i;
    }
    if (slot >= 0) {
        g_wtTitles[slot].window = window;
        lstrcpynA(g_wtTitles[slot].gbk, gbk, sizeof(g_wtTitles[slot].gbk));
    }
    LeaveCriticalSection(&g_wtLock);
    g_wtEnglishApplied = english;
    return TRUE;
}

/* After any CreateWindowExA of C1's (borderless.c's R_CreateWindow path too). */
static void WtCreatedWindow(HWND window, LPCSTR title, DWORD style)
{
    if (!window || (style & WS_CHILD) || !WtHasHigh(title)) return;
    if (WtApply(window, title)) {
        wchar_t now[128];
        int n = GetWindowTextW(window, now, 128);
        char utf8[384];
        WideCharToMultiByte(CP_UTF8, 0, now, n + 1, utf8, sizeof(utf8), NULL, NULL);
        utf8[sizeof(utf8) - 1] = 0;
        Log("window title: %p \"%s\" (GBK -> Unicode%s)", (void *)window, utf8, WtEnglish() ? ", English name" : "");
    }
}

static HWND WINAPI WtCreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int w, int h,
                                     HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    HWND window = g_wtCreateWindowExA(ex, cls, title, style, x, y, w, h, parent, menu, inst, param);
    WtCreatedWindow(window, title, style);
    return window;
}

static BOOL WINAPI WtSetWindowTextA(HWND window, LPCSTR text)
{
    if (WtHasHigh(text) && WtTopLevel(window) && WtApply(window, text)) return TRUE;
    return g_wtSetWindowTextA(window, text);
}

static int WINAPI WtMessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type)
{
    if (!WtHasHigh(text) && !WtHasHigh(caption)) return g_wtMessageBoxA(owner, text, caption, type);
    BOOL english = WtEnglish();
    wchar_t *wt = text ? WtWiden(text, english) : NULL;
    wchar_t *wc = caption ? WtWiden(caption, english) : NULL;
    int r;
    if ((text && !wt) || (caption && !wc)) r = g_wtMessageBoxA(owner, text, caption, type);
    else r = MessageBoxW(owner, wt, wc, type);
    WtFree(wt);
    WtFree(wc);
    return r;
}

/* C1's fatal error box converts its GBK text itself (MultiByteToWideChar 936 at 0x6315CE) and calls MessageBoxW
 * (0x631601): only the game's name changes, in English. */
static int WINAPI WtMessageBoxW(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type)
{
    if (!WtEnglish()) return g_wtMessageBoxW(owner, text, caption, type);
    wchar_t *et = WtEnglishName(text), *ec = WtEnglishName(caption);
    int r = g_wtMessageBoxW(owner, et ? et : text, ec ? ec : caption, type);
    WtFree(et);
    WtFree(ec);
    return r;
}

static HWND WINAPI WtFindWindowA(LPCSTR cls, LPCSTR name)
{
    if (!WtHasHigh(name) && !WtHasHigh(cls)) return g_wtFindWindowA(cls, name);
    BOOL english = WtEnglish();
    wchar_t *wcls = cls && !IS_INTRESOURCE(cls) ? WtWiden(cls, FALSE) : NULL;
    LPCWSTR c = cls && IS_INTRESOURCE(cls) ? (LPCWSTR)cls : wcls;
    HWND found = NULL;
    for (int pass = 0; pass < 2 && !found; pass++) {   /* the name as it is shown now, then the other spelling */
        wchar_t *wn = name ? WtWiden(name, pass ? !english : english) : NULL;
        if (name && !wn) break;
        found = FindWindowW(c, wn);
        WtFree(wn);
        if (!name) break;
    }
    WtFree(wcls);
    return found ? found : g_wtFindWindowA(cls, name);
}

static int WINAPI WtGetWindowTextA(HWND window, LPSTR out, int max)
{
    if (out && max > 0 && window && g_wtLockReady == 2) {
        int n = -1;
        EnterCriticalSection(&g_wtLock);
        for (int i = 0; i < WT_MAX_TITLES; i++)
            if (g_wtTitles[i].window == window) {
                lstrcpynA(out, g_wtTitles[i].gbk, max);
                n = lstrlenA(out);
                break;
            }
        LeaveCriticalSection(&g_wtLock);
        if (n >= 0) return n;
    }
    return g_wtGetWindowTextA(window, out, max);
}

/* kernel33.c FillImports: C1's USER32 slots that carry GBK text. */
static FARPROC WtImport(const C1Import *imp, FARPROC original)
{
    if (lstrcmpiA(imp->dll, "USER32.dll")) return original;
    if (imp->slot == 0x00B42428u && !strcmp(imp->name, "CreateWindowExA")) {
        g_wtCreateWindowExA = (WtCreateWindowExAFn)(void *)original;
        return (FARPROC)(void *)WtCreateWindowExA;
    }
    if (imp->slot == 0x00B42480u && !strcmp(imp->name, "SetWindowTextA")) {
        g_wtSetWindowTextA = (WtSetWindowTextAFn)(void *)original;
        return (FARPROC)(void *)WtSetWindowTextA;
    }
    if (imp->slot == 0x00B42430u && !strcmp(imp->name, "MessageBoxA")) {
        g_wtMessageBoxA = (WtMessageBoxAFn)(void *)original;
        return (FARPROC)(void *)WtMessageBoxA;
    }
    if (imp->slot == 0x00B42450u && !strcmp(imp->name, "MessageBoxW")) {
        g_wtMessageBoxW = (WtMessageBoxWFn)(void *)original;
        return (FARPROC)(void *)WtMessageBoxW;
    }
    if (imp->slot == 0x00B42414u && !strcmp(imp->name, "FindWindowA")) {
        g_wtFindWindowA = (WtFindWindowAFn)(void *)original;
        return (FARPROC)(void *)WtFindWindowA;
    }
    if (imp->slot == 0x00B423E4u && !strcmp(imp->name, "GetWindowTextA")) {
        WtLockInit();
        g_wtGetWindowTextA = (WtGetWindowTextAFn)(void *)original;
        return (FARPROC)(void *)WtGetWindowTextA;
    }
    return original;
}

/* Main thread, every frame (BorderlessTick): a language switch retitles the windows titled here. */
static void WindowTitleTick(void)
{
    if (g_wtEnglishApplied < 0 || (int)WtEnglish() == g_wtEnglishApplied) return;
    HWND windows[WT_MAX_TITLES];
    char titles[WT_MAX_TITLES][256];
    int count = 0;
    EnterCriticalSection(&g_wtLock);
    for (int i = 0; i < WT_MAX_TITLES; i++)
        if (g_wtTitles[i].window && IsWindow(g_wtTitles[i].window)) {
            windows[count] = g_wtTitles[i].window;
            memcpy(titles[count], g_wtTitles[i].gbk, sizeof(titles[count]));
            count++;
        }
    LeaveCriticalSection(&g_wtLock);
    g_wtEnglishApplied = WtEnglish();   /* once per switch, even for a window of another thread */
    for (int i = 0; i < count; i++) WtApply(windows[i], titles[i]);
    Log("window title: %d window(s) retitled for the %s name", count, g_wtEnglishApplied ? "English" : "Chinese");
}

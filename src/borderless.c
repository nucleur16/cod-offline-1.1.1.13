/*
 * borderless.c - r_noborder, IW4x's borderless window (Options > Video > Display: 无边框窗口 / Fullscreen Windowed).
 *
 * The shared Options movie (voice_ui_mp, SettingsService) offers three display modes: Windowed
 * (r_fullscreen 0), Fullscreen (r_fullscreen 1) and Fullscreen Windowed (r_fullscreen 0 plus
 * "seta r_noborder 1"). C1 has no r_noborder (the exe's only "noBorder" is a Scaleform scale mode); this file
 * implements it as IW4x does: with r_fullscreen 0 and r_noborder 1, the main window is a WS_POPUP window (no
 * caption, no frame) whose client area is the r_mode size. Nothing else about the mode changes: the D3D9 device
 * is C1's ordinary windowed device, its back buffer the r_mode size, so the game renders exactly what the framed
 * window would. When the r_mode size is the size of the monitor the window is on, the window is placed at that
 * monitor's origin and covers it (the usual "fullscreen windowed"); otherwise it sits at vid_xpos / vid_ypos like
 * the framed window.
 *
 *   R_InitGraphicsApi (0xA7AC50) builds the window parameters (R_SetWndParms 0xA7A0C0, on its stack:
 *   +0 hwnd, +4 hz, +8 fullscreen, +0xC x, +0x10 y, +0x14/+0x18 scene size, +0x1C/+0x20 display size),
 *   then calls R_CreateWindow (0xA79CF0, esi = parms) @ 0xA7AC89. That call goes through BorderlessCreateStub,
 *   which only reads the parms (windowed or not, the display size). R_CreateWindow's CreateWindowExA (call [IAT]
 *   @ 0xA79DBF; windowed style 0xC80000 = caption + system menu at vid_xpos / vid_ypos, its size grown by
 *   AdjustWindowRectEx) goes through BorderlessCreateWindowExA, which makes it WS_POPUP with the display size as
 *   its outer (= client) size. The style is chosen once per window, when it is created (startup, vid_restart).
 *
 *   The old Fullscreen Windowed replaced the parms with the monitor's rectangle, so the back buffer and the scene
 *   were always the monitor's native resolution whatever r_mode said (at 2560x1440 that is 1.8x the pixels of
 *   1920x1080, 4x those of 1280x720), and each frame it polled the window (MonitorFromWindow, GetMonitorInfo,
 *   GetWindowRect, a Dvar_FindVar of r_fullscreen) and queued vid_restarts when the window did not cover its
 *   monitor. That is gone: the per-frame tick reads two cached dvar bytes.
 *
 *   Switching Windowed <-> Fullscreen Windowed in Options (r_noborder alone) is seen by BorderlessTick (on the
 *   k33tick.c main-thread dispatcher; voice.c's tick is not hooked in this exe) as a windowed window whose style
 *   does not match r_noborder, and that window is restyled once in place (SetWindowLong, then SetWindowPos with
 *   SWP_FRAMECHANGED; its client size, so the back buffer, stays). No vid_restart: in a match C1's vid_restart is
 *   a server command that does nothing against the sh1x host, so an Apply mid-match would restart nothing.
 *   Fullscreen (r_fullscreen) is C1's own business.
 *
 * r_noborder is registered as an archived bool (saved in config_mp.cfg), default 0. It must be a bool (or a
 * string): the movie reads it with the Scaleform Dvar_GetBool (0x6A5000), which takes any other type's value
 * as a string pointer (an int dvar crashed it in strtol). Before registration, the value comes from the dvar
 * config_mp.cfg's "seta" created (a string dvar), read by name; a value saved under the old name (BD_OLD_DVAR)
 * is taken over.
 */

#define BD_CREATE_CALL_VA     0x00A7AC89u   /* call R_CreateWindow in R_InitGraphicsApi */
#define BD_CREATE_WINDOW_VA   0x00A79CF0u   /* R_CreateWindow (esi = GfxWindowParms) */
#define BD_CWEX_CALL_VA       0x00A79DBFu   /* call [CreateWindowExA] in R_CreateWindow */
#define BD_CWEX_IAT_VA        0x00B42428u   /* the exe's CreateWindowExA slot (C1.exe: 0x9A1D6F4) */
#define BD_DVAR_FIND_VA       0x006232C0u   /* Dvar_FindVar(name) -> dvar_t* */
#define BD_REGISTER_BOOL_VA   0x00624370u   /* Dvar_RegisterBool(name, value, flags, desc) */
#define BD_MAIN_HWND_VA       0x02B68104u
#define BD_SETWINDOWPOS_IAT_VA 0x00B42490u  /* the exe's SetWindowPos slot (QA background mode hooks it) */
#define BD_FRAMED_STYLE       0x00C80000u   /* R_CreateWindow's windowed style: WS_CAPTION | WS_SYSMENU */
#if K33_KERNEL_NAME_DISPLAY
#include "kernel/kernel_video_mode.c"
#endif

__attribute__((used)) DWORD g_bdCreateWindowFn = BD_CREATE_WINDOW_VA;
static void *g_bdCreateWindowExPtr;          /* the IAT-style slot the patched call reads */
static volatile LONG g_bdCreating;            /* R_CreateWindow is making a borderless window */
static volatile LONG g_bdWindowBorderless = -1; /* the window that exists: -1 none yet, 0 framed, 1 borderless,
                                                   2 fullscreen */
static int g_bdWidth, g_bdHeight;             /* the display (client) size of the window last created */
static BOOL g_bdInstalled;
static const BYTE *g_bdDvar;                  /* r_noborder once registered */
static const BYTE *g_bdFullscreenDvar;        /* r_fullscreen (bool or int) */

/* window_title.c (included after locale_switch.c): the window's GBK title in Unicode, and its language tick. */
static void WtCreatedWindow(HWND window, LPCSTR title, DWORD style);
static void WindowTitleTick(void);

typedef HWND(WINAPI *BdCreateWindowExFn)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);

/* A dvar's value as an int, whatever its type (bool 0, int 5, string 7 from a "seta" before registration). */
static int BdDvarValue(const char *name, int def)
{
    const BYTE *d = ((const BYTE *(__cdecl *)(const char *))(DWORD_PTR)BD_DVAR_FIND_VA)(name);
    if (!d || !Readable(d, 0x10)) return def;
    switch (d[8]) {
    case 0: return d[0xC] != 0;
    case 5: return *(const int *)(d + 0xC);
    case 1: return (int)*(const float *)(d + 0xC);
    case 7: {
        const char *s = *(const char *const *)(d + 0xC);
        return s && Readable(s, 1) ? atoi(s) : def;
    }
    default: return def;
    }
}

/* LEGACY_NAME: the old name of r_noborder (a config_mp.cfg saved before the rename has "seta codol_borderless 1"). */
#define BD_OLD_DVAR "codol_borderless" /* LEGACY_NAME */

static BOOL BorderlessWanted(void)
{
    if (g_bdDvar && g_bdDvar != (const BYTE *)1) return g_bdDvar[0xC] != 0;
    return BdDvarValue("r_noborder", BdDvarValue(BD_OLD_DVAR, 0)) != 0;
}

/* Called with R_InitGraphicsApi's window parameters, just before R_CreateWindow. They are not changed. */
__attribute__((used)) void __cdecl BorderlessParms(BYTE *p)
{
    InterlockedExchange(&g_bdCreating, 0);
    if (!p || !Readable(p, 0x24)) return;
    BOOL fullscreen = p[8] != 0;
    g_bdWidth = *(int *)(p + 0x1C);
    g_bdHeight = *(int *)(p + 0x20);
    if (fullscreen || !BorderlessWanted()) {
        InterlockedExchange(&g_bdWindowBorderless, fullscreen ? 2 : 0);
        Log("r_noborder: off for this window (%s)", fullscreen ? "fullscreen" : "windowed");
        return;
    }
    if (g_bdWidth < 320 || g_bdHeight < 200) {
        InterlockedExchange(&g_bdWindowBorderless, 0);
        Log("r_noborder: display size %dx%d unusable, framed window", g_bdWidth, g_bdHeight);
        return;
    }
    InterlockedExchange(&g_bdCreating, 1);
    InterlockedExchange(&g_bdWindowBorderless, 1);
}

static HWND WINAPI BorderlessCreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int w, int h,
                                             HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    static BdCreateWindowExFn real;
    if (!real) real = (BdCreateWindowExFn)GetProcAddress(GetModuleHandleA("user32.dll"), "CreateWindowExA");
    if (InterlockedExchange(&g_bdCreating, 0)) {
        style = (style & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_POPUP;
        ex &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME | WS_EX_TOPMOST);
        w = g_bdWidth;   /* no frame: the outer size is the client size */
        h = g_bdHeight;
        /* the monitor the window would be on; at that monitor's size (or larger) the window starts at its origin */
        RECT want = { x, y, x + w, y + h };
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        const char *where = "vid_xpos / vid_ypos";
        if (GetMonitorInfoA(MonitorFromRect(&want, MONITOR_DEFAULTTONEAREST), &mi) &&
            mi.rcMonitor.right - mi.rcMonitor.left <= w && mi.rcMonitor.bottom - mi.rcMonitor.top <= h) {
            x = mi.rcMonitor.left;
            y = mi.rcMonitor.top;
            where = "covering its monitor";
        }
        Log("r_noborder: borderless %dx%d window at %d,%d (%s)", w, h, x, y, where);
        (void)where;
    }
    QbgPrepareWindow(&ex, &x, &y);
    HWND window = real(ex, cls, title, style, x, y, w, h, parent, menu, inst, param);
    QbgCreatedWindow(window);
    WtCreatedWindow(window, title, style);
    return window;
}

void BorderlessCreateStub(void);
__asm__(".text\n"
        ".globl _BorderlessCreateStub\n"
        "_BorderlessCreateStub:\n"        /* replaces "call R_CreateWindow"; esi = the parms */
        "    pushal\n"
        "    pushl %esi\n"
        "    call _BorderlessParms\n"
        "    addl $4, %esp\n"
        "    popal\n"
        "    jmp *_g_bdCreateWindowFn\n");   /* R_CreateWindow returns to R_InitGraphicsApi */

static void PatchBorderless(void)
{
    /* call R_CreateWindow ; ff 15 [CreateWindowExA] */
    BYTE kCwex[6] = { 0xFF, 0x15, 0, 0, 0, 0 };
    DWORD iat = BD_CWEX_IAT_VA;
    memcpy(kCwex + 2, &iat, 4);
    BYTE *cw = (BYTE *)(DWORD_PTR)BD_CWEX_CALL_VA;
    if (!CallsTarget(BD_CREATE_CALL_VA, BD_CREATE_WINDOW_VA) || !Readable(cw, 6) || memcmp(cw, kCwex, 6) != 0) {
        Log("r_noborder: bytes at 0x%08X / 0x%08X do not match, Fullscreen Windowed acts as Windowed",
            BD_CREATE_CALL_VA, BD_CWEX_CALL_VA);
        return;
    }
    g_bdCreateWindowExPtr = (void *)BorderlessCreateWindowExA;
    DWORD slot = (DWORD)(DWORD_PTR)&g_bdCreateWindowExPtr;
    memcpy(kCwex + 2, &slot, 4);
    WriteCode(cw, kCwex, 6);
    RedirectCall(BD_CREATE_CALL_VA, (void *)BorderlessCreateStub);
    g_bdInstalled = TRUE;
    Log("r_noborder: borderless window ready");
}

/* Main thread: the existing windowed window becomes borderless (or framed) in place, its client size kept. */
static BOOL BdRestyle(HWND h, BOOL borderless)
{
    RECT cr;
    POINT org = { 0, 0 };
    if (!GetClientRect(h, &cr) || !ClientToScreen(h, &org)) return FALSE;
    int w = cr.right - cr.left, hgt = cr.bottom - cr.top;
    if (g_bdWidth >= 320 && g_bdHeight >= 200) {   /* the back buffer's size (Windows may have clipped the frame) */
        w = g_bdWidth;
        hgt = g_bdHeight;
    }
    DWORD style = (DWORD)GetWindowLongA(h, GWL_STYLE), ex = (DWORD)GetWindowLongA(h, GWL_EXSTYLE);
    const DWORD frame = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    int x, y, ow = w, oh = hgt;
    if (borderless) {
        style = (style & ~frame) | WS_POPUP;
        ex &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME);
        x = org.x;   /* the picture stays where it was */
        y = org.y;
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoA(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi) &&
            mi.rcMonitor.right - mi.rcMonitor.left <= w && mi.rcMonitor.bottom - mi.rcMonitor.top <= hgt) {
            x = mi.rcMonitor.left;
            y = mi.rcMonitor.top;
        }
    } else {
        style = (style & ~(frame | WS_POPUP)) | BD_FRAMED_STYLE;
        RECT r = { 0, 0, w, hgt };
        AdjustWindowRectEx(&r, style, FALSE, ex);
        ow = r.right - r.left;
        oh = r.bottom - r.top;
        x = org.x + r.left;   /* the frame grows around the picture */
        y = org.y + r.top;
    }
    SetWindowLongA(h, GWL_EXSTYLE, (LONG)ex);
    SetWindowLongA(h, GWL_STYLE, (LONG)style);
    typedef BOOL(WINAPI * SetWindowPosFn)(HWND, HWND, int, int, int, int, UINT);
    SetWindowPosFn swp = *(SetWindowPosFn *)(DWORD_PTR)BD_SETWINDOWPOS_IAT_VA;
    if (!swp) swp = SetWindowPos;
    swp(h, NULL, x, y, ow, oh, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    Log("r_noborder: window restyled %s, %dx%d client at %d,%d", borderless ? "borderless" : "framed", w, hgt, x, y);
    return TRUE;
}

/* Main thread, every frame. Once the dvars exist: two cached bytes compared, nothing else. */
static void BorderlessTick(void)
{
    WindowTitleTick();
    if (!g_bdInstalled) return;
    if (!g_bdDvar) {
        if (!C1DvarsReady()) return;
        int fallback=0;
#if K33_KERNEL_NAME_DISPLAY
        fallback=KvLoad(0);
#endif
        int cur = BdDvarValue("r_noborder", BdDvarValue(BD_OLD_DVAR, fallback)) != 0;
        typedef const BYTE *(__cdecl *RegisterBoolFn)(const char *, BYTE, unsigned, const char *);
        const BYTE *d = ((RegisterBoolFn)(DWORD_PTR)BD_REGISTER_BOOL_VA)(
            "r_noborder", (BYTE)cur, 0x1, "Remove the window border in windowed mode (r_fullscreen 0)");
        g_bdDvar = d && Readable(d, 0x10) && d[8] == 0 ? d : NULL;
        char old[8];
        if (g_bdDvar && C1DvarString(BD_OLD_DVAR, old, sizeof(old)) && old[0] && C1QueueSet(BD_OLD_DVAR, ""))
            Log("r_noborder: took the value saved as " BD_OLD_DVAR " (%s)", old);
        Log("r_noborder: registered as a bool (%d)", g_bdDvar ? g_bdDvar[0xC] : -1);
        if (!g_bdDvar) g_bdDvar = (const BYTE *)1;   /* do not try again; read it by name */
        const BYTE *fs = C1DvarFind("r_fullscreen");
        g_bdFullscreenDvar = fs && (fs[8] == 0 || fs[8] == 5) ? fs : NULL;
        if (!g_bdFullscreenDvar) Log("r_noborder: no r_fullscreen bool / int, a change applies on vid_restart");
    }
#if K33_KERNEL_NAME_DISPLAY
    if(g_bdDvar!=(const BYTE *)1)KvSave(BorderlessWanted());
#endif
    /* only a windowed window with r_fullscreen still 0 is restyled here; r_fullscreen is C1's */
    LONG have = g_bdWindowBorderless;
    if (have < 0 || have > 1 || g_bdDvar == (const BYTE *)1 || !g_bdFullscreenDvar ||
        (g_bdFullscreenDvar[8] ? *(const int *)(g_bdFullscreenDvar + 0xC) != 0 : g_bdFullscreenDvar[0xC] != 0) ||
        (g_bdDvar[0xC] != 0) == (have == 1))
        return;
    HWND h = *(HWND *)(DWORD_PTR)BD_MAIN_HWND_VA;
    if (!h || !IsWindow(h) || IsIconic(h)) return;   /* minimized: done when it is restored */
    BOOL want = g_bdDvar[0xC] != 0;
    if (BdRestyle(h, want)) InterlockedExchange(&g_bdWindowBorderless, want ? 1 : 0);
}

/*
 * borderless.c - "Fullscreen Windowed" (Options > Video > Display: 无边框窗口 / Fullscreen Windowed).
 *
 * The shared Options movie (voice_ui_mp, SettingsService) offers three display modes: Windowed
 * (r_fullscreen 0), Fullscreen (r_fullscreen 1) and Fullscreen Windowed (r_fullscreen 0 plus
 * "seta codol_borderless 1"). C1 has no such mode; this file implements it:
 *
 *   R_InitGraphicsApi (0xA7AC50) builds the window parameters (R_SetWndParms 0xA7A0C0, on its stack:
 *   +0 hwnd, +4 hz, +8 fullscreen, +0xC x, +0x10 y, +0x14/+0x18 scene size, +0x1C/+0x20 display size),
 *   then calls R_CreateWindow (0xA79CF0, esi = parms) @ 0xA7AC89, then creates the D3D9 device from the
 *   same parms (windowed: back buffer = display size). That call goes through BorderlessCreateStub:
 *   when borderless is wanted and the mode is windowed, the parms become the monitor's rectangle, so
 *   the back buffer, the scene and C1's vidConfig are the monitor's resolution (the r_mode resolution
 *   chosen in Options is not used while borderless, as in other games' borderless modes).
 *   R_CreateWindow's CreateWindowExA (call [IAT] @ 0xA79DBF; windowed style 0xC80000 = caption + system
 *   menu, its size grown by AdjustWindowRectEx) goes through BorderlessCreateWindowExA, which makes it
 *   WS_POPUP at the monitor's rectangle. The device stays a windowed D3D9 device, so Alt+Tab never
 *   loses it, and the dev console / vision editor hooks see an ordinary window.
 *
 *   The monitor is the one the game window was last on (else the one under vid_xpos / vid_ypos, else
 *   the primary). Each frame (BorderlessTick, from the voice main-thread tick) the wanted mode
 *   (codol_borderless and not r_fullscreen) is compared with the window that exists; when they differ
 *   (Options Apply switching Windowed <-> Fullscreen Windowed, which C1 alone would not restart for), or
 *   when a borderless window no longer covers its monitor (a desktop resolution change), one
 *   vid_restart is queued. vid_restart destroys and recreates the window and the device (0xA7AD90).
 *
 * codol_borderless is registered as an archived bool (saved in config_mp.cfg). It must be a bool (or a
 * string): the movie reads it with the Scaleform Dvar_GetBool (0x6A5000), which takes any other type's value
 * as a string pointer (an int dvar crashed it in strtol). Before registration, the value comes from the dvar
 * config_mp.cfg's "seta" created (a string dvar), read by name.
 */

#define BD_CREATE_CALL_VA     0x00A7AC89u   /* call R_CreateWindow in R_InitGraphicsApi */
#define BD_CREATE_WINDOW_VA   0x00A79CF0u   /* R_CreateWindow (esi = GfxWindowParms) */
#define BD_CWEX_CALL_VA       0x00A79DBFu   /* call [CreateWindowExA] in R_CreateWindow */
#define BD_CWEX_IAT_VA        0x00B42428u   /* the exe's CreateWindowExA slot (C1.exe: 0x9A1D6F4) */
#define BD_DVAR_FIND_VA       0x006232C0u   /* Dvar_FindVar(name) -> dvar_t* */
#define BD_MAIN_HWND_VA       0x02B68104u
#define BD_REGISTER_BOOL_VA   0x00624370u   /* Dvar_RegisterBool(name, value, flags, desc) */

__attribute__((used)) DWORD g_bdCreateWindowFn = BD_CREATE_WINDOW_VA;
static void *g_bdCreateWindowExPtr;          /* the IAT-style slot the patched call reads */
static volatile LONG g_bdCreating;            /* R_CreateWindow is making a borderless window */
static volatile LONG g_bdWindowBorderless = -1; /* the window that exists: -1 none yet, 0 framed / fullscreen, 1 borderless */
static RECT g_bdMonitor;                      /* the monitor the borderless window covers */
static BOOL g_bdHaveMonitor;
static BOOL g_bdInstalled;
static const BYTE *g_bdDvar;                  /* codol_borderless once registered */

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

static BOOL BorderlessWanted(void)
{
    if (g_bdDvar && g_bdDvar != (const BYTE *)1) return g_bdDvar[0xC] != 0;
    return BdDvarValue("codol_borderless", 0) != 0;
}

static BOOL BdMonitorRect(HMONITOR m, RECT *r)
{
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (!m || !GetMonitorInfoA(m, &mi)) return FALSE;
    *r = mi.rcMonitor;
    return TRUE;
}

/* Called with R_InitGraphicsApi's window parameters, just before R_CreateWindow. */
__attribute__((used)) void __cdecl BorderlessParms(BYTE *p)
{
    InterlockedExchange(&g_bdCreating, 0);
    if (!p || !Readable(p, 0x24)) return;
    BOOL fullscreen = p[8] != 0;
    if (fullscreen || !BorderlessWanted()) {
        InterlockedExchange(&g_bdWindowBorderless, 0);
        Log("borderless: off for this window (%s)", fullscreen ? "fullscreen" : "windowed");
        return;
    }
    RECT r;
    BOOL ok = g_bdHaveMonitor;
    r = g_bdMonitor;
    if (!ok) {
        POINT pt = { *(int *)(p + 0xC), *(int *)(p + 0x10) };
        ok = BdMonitorRect(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &r);
    } else {
        /* the remembered monitor may have changed resolution: take its current rectangle */
        POINT pt = { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
        ok = BdMonitorRect(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &r);
    }
    if (!ok || r.right - r.left < 640 || r.bottom - r.top < 480) {
        InterlockedExchange(&g_bdWindowBorderless, 0);
        Log("borderless: no usable monitor rectangle, framed window");
        return;
    }
    int w = r.right - r.left, h = r.bottom - r.top;
    Log("borderless: window %dx%d at %ld,%ld (r_mode size was %dx%d)", w, h, r.left, r.top, *(int *)(p + 0x1C),
        *(int *)(p + 0x20));
    *(int *)(p + 0xC) = r.left;
    *(int *)(p + 0x10) = r.top;
    *(int *)(p + 0x14) = w;
    *(int *)(p + 0x18) = h;
    *(int *)(p + 0x1C) = w;
    *(int *)(p + 0x20) = h;
    g_bdMonitor = r;
    g_bdHaveMonitor = TRUE;
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
        x = g_bdMonitor.left;
        y = g_bdMonitor.top;
        w = g_bdMonitor.right - g_bdMonitor.left;
        h = g_bdMonitor.bottom - g_bdMonitor.top;
    }
    return real(ex, cls, title, style, x, y, w, h, parent, menu, inst, param);
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
        Log("borderless: bytes at 0x%08X / 0x%08X do not match, Fullscreen Windowed acts as Windowed", BD_CREATE_CALL_VA,
            BD_CWEX_CALL_VA);
        return;
    }
    g_bdCreateWindowExPtr = (void *)BorderlessCreateWindowExA;
    DWORD slot = (DWORD)(DWORD_PTR)&g_bdCreateWindowExPtr;
    memcpy(kCwex + 2, &slot, 4);
    WriteCode(cw, kCwex, 6);
    RedirectCall(BD_CREATE_CALL_VA, (void *)BorderlessCreateStub);
    g_bdInstalled = TRUE;
    Log("borderless: Fullscreen Windowed (codol_borderless) ready");
}

/* Main thread, every frame. */
static void BorderlessTick(void)
{
    static DWORD lastRestart;
    static int coverRestarts;   /* restarts for "no longer covers its monitor" in a row: at most 2 */
    if (!g_bdInstalled) return;
    if (!g_bdDvar && C1DvarsReady()) {
        int cur = BdDvarValue("codol_borderless", 0) != 0;
        typedef const BYTE *(__cdecl *RegisterBoolFn)(const char *, BYTE, unsigned, const char *);
        const BYTE *d = ((RegisterBoolFn)(DWORD_PTR)BD_REGISTER_BOOL_VA)(
            "codol_borderless", (BYTE)cur, 0x1, "Fullscreen Windowed: a borderless window covering the monitor (with r_fullscreen 0)");
        g_bdDvar = d && Readable(d, 0x10) && d[8] == 0 ? d : NULL;
        Log("borderless: codol_borderless registered as a bool (%d)", g_bdDvar ? g_bdDvar[0xC] : -1);
        if (!g_bdDvar) g_bdDvar = (const BYTE *)1;   /* do not try again; read it by name */
    }
    HWND h = *(HWND *)(DWORD_PTR)BD_MAIN_HWND_VA;
    if (!h || !IsWindow(h) || g_bdWindowBorderless < 0) return;
    RECT mon;
    BOOL haveMon = BdMonitorRect(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mon);
    BOOL want = BorderlessWanted() && !BdDvarValue("r_fullscreen", 0);
    BOOL restart = FALSE;
    const char *why = "";
    if (want != (g_bdWindowBorderless == 1)) {
        restart = TRUE;
        why = want ? "Fullscreen Windowed on" : "Fullscreen Windowed off";
    } else if (want && haveMon && !IsIconic(h)) {
        RECT wr;
        if (GetWindowRect(h, &wr) && !EqualRect(&wr, &mon)) {
            if (coverRestarts < 2) {
                restart = TRUE;
                why = "the window no longer covers its monitor";
            }
        } else {
            coverRestarts = 0;
        }
    }
    if (haveMon && !IsIconic(h) && (g_bdWindowBorderless == 0 || !restart)) {
        g_bdMonitor = mon;   /* follow the window: a framed window dragged to another monitor */
        g_bdHaveMonitor = TRUE;
    }
    if (restart && GetTickCount() - lastRestart > 3000) {
        lastRestart = GetTickCount();
        if (want == (g_bdWindowBorderless == 1)) coverRestarts++;
        Log("borderless: vid_restart (%s)", why);
        C1CbufAddText("vid_restart\n");
    }
}

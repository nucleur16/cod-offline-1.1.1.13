/*
 * controller.c - gamepad support for C1 (feat/controller). Included by kernel33.c after voice.c.
 * See CONTROLLER.md for what C1 still has, the addresses and the phased plan.
 *
 * C1 is IW4 PC with its gamepad layer removed: no XInput, no gamepad keys, no CL_GamepadMove. What
 * this file adds, all byte-checked (a mismatch is logged to Kernel33.log and that part stays off):
 *
 *   input    SDL2's GameController API (2.32.10, linked statically; Xbox, PlayStation, Switch Pro and
 *            generic pads through SDL's mapping database, hot-plug), on its own thread at about 1 kHz,
 *            initialised with SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC only: no video, no window of
 *            the game's, no keyboard/mouse/IME, SDL's raw-input joystick driver off (Kernel33's raw
 *            mouse keeps WM_INPUT). If SDL cannot start, the same thread polls XInput instead.
 *   keys     IW4's gamepad keynums (BUTTON_A = 1 ... DPAD_RIGHT = 23, unused by C1 on PC) get names:
 *            the key-name table 0xCD5C88 (Key_StringToKeynum 0x492A20, Key_WriteBindings 0x492FD0) and
 *            its display twin 0xCD5F90 (Key_KeynumToString 0x492AF0) are copied with 16 more entries.
 *            So "bind BUTTON_A +gostand" works, and the binds are saved in config_mp.cfg like any key.
 *            Every button / trigger change is queued as an SE_KEY event through C1's own Sys_QueEvent
 *            0x62EF60 (the path WM_KEYDOWN takes; it holds critical section 0x2B62C60, as its consumers
 *            0x62F030 / 0x62F2A0 do, so the input thread may call it), so Com_EventLoop hands it to
 *            CL_KeyEvent 0x493240 in order with the keyboard, and a tap shorter than a frame is kept.
 *   frame    IN_Frame 0x62DA90 (once per client frame, front end included): dvars, default binds, the
 *            menu cursor and the console lines of the input thread.
 *   move     CL_CreateCmd 0x490D50 calls CL_MouseMove 0x490010 (eax = lc; cmd, frametime) at
 *            0x490E2F. That call goes through CTL_MouseMoveHook: the original runs, then the sticks are
 *            sampled: left stick -> cmd forwardmove (+0x1A) / rightmove (+0x1B), right stick -> clientActive
 *            viewangles (cl = [0x16FC318] ^ 0x9056BBC8; pitch +0x34AC, yaw +0x34B0), as C1's mouse does,
 *            so CL_FinishMove 0x490630 puts them in the usercmd.
 *   menus    with keyCatchers (0x16FC2E0) UI/console/chat bits or outside a match, the left stick moves
 *            the cursor, A clicks (WM_LBUTTON* to the game window) and B / Start are Escape.
 */
#include <math.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#define CTL_IN_FRAME_VA        0x0062DA90u
#define CTL_IN_FRAME_RESUME    0x0062DA96u
#define CTL_MOUSEMOVE_CALL_VA  0x00490E2Fu
#define CTL_MOUSEMOVE_VA       0x00490010u
#define CTL_QUEEVENT_VA        0x0062EF60u   /* Sys_QueEvent(eax = time or 0; type, value, value2, ptrLength, ptr) */
#define CTL_SE_KEY             1
#define CTL_SYS_MS_VA          0x006326A0u
#define CTL_CL_PTR_VA          0x016FC318u
#define CTL_CL_KEY             0x9056BBC8u
#define CTL_CL_PMFLAGS         0x14
#define CTL_CL_MAXPITCH        0x3454
#define CTL_CL_MAXYAW          0x3458
#define CTL_CL_PITCH           0x34AC
#define CTL_CL_YAW             0x34B0
#define CTL_CMD_BUTTONS        0x04
#define CTL_CMD_FORWARD        0x1A
#define CTL_CMD_RIGHT          0x1B
#define CTL_FRAMEMSEC_VA       0x06C45098u   /* cls.frametime, ms (C1's aim code reads it the same way) */
#define CTL_CONNSTATE_VA       0x016FC2E8u
#define CTL_CA_ACTIVE          9
#define CTL_KEYS_VA            0x016F515Cu   /* playerKeys[0].keys[k] {down, repeats, binding} 12 bytes */
#define CTL_KEYNAMES_VA        0x00CD5C88u
#define CTL_KEYNAMES_LOC_VA    0x00CD5F90u
#define CTL_MAIN_HWND_VA       0x02B68104u
#define CTL_REG_FLOAT_VA       0x006244D0u
#define CTL_KC_CONSOLE         0x01
#define CTL_KC_UI              0x10
#define CTL_KC_CHAT            0x20
#define CTL_FROZEN             0x800         /* the pm_flags bit C1's CL_MouseMove skips on */
#define CTL_ADS_BUTTONS        0x00000800u   /* usercmd ADS bit (+speed_throw sets 0x800 | 0x100000, measured) */

/* XInput (fallback backend), loaded at run time. Pad state everywhere uses XInput's layout. */
typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } CtlPad;
typedef struct { DWORD dwPacketNumber; CtlPad Gamepad; } CtlState;
typedef DWORD (WINAPI *XInputGetStateFn)(DWORD, CtlState *);
#define XI_DPAD_UP 0x0001
#define XI_DPAD_DOWN 0x0002
#define XI_DPAD_LEFT 0x0004
#define XI_DPAD_RIGHT 0x0008
#define XI_START 0x0010
#define XI_BACK 0x0020
#define XI_LTHUMB 0x0040
#define XI_RTHUMB 0x0080
#define XI_LSHOULDER 0x0100
#define XI_RSHOULDER 0x0200
#define XI_A 0x1000
#define XI_B 0x2000
#define XI_X 0x4000
#define XI_Y 0x8000
#define CTL_LT_BIT 0x0400   /* triggers past gpad_button_deadzone, in the two bits XInput leaves free */
#define CTL_RT_BIT 0x0800

/* IW4 keyNum_t gamepad codes. */
enum { K_BUTTON_A = 1, K_BUTTON_B, K_BUTTON_X, K_BUTTON_Y, K_BUTTON_LSHLDR, K_BUTTON_RSHLDR,
       K_BUTTON_START = 14, K_BUTTON_BACK, K_BUTTON_LSTICK, K_BUTTON_RSTICK, K_BUTTON_LTRIG,
       K_BUTTON_RTRIG, K_DPAD_UP, K_DPAD_DOWN, K_DPAD_LEFT, K_DPAD_RIGHT };

typedef struct { WORD xi; int key; const char *name; const char *display; const char *bind; } CtlButton;
/* Digital buttons, then the two triggers (xi 0: read from the analog value). Default binds after
 * IW4x's, with C1's own command names (C1's ADS is +speed_throw). */
static const CtlButton kCtlButtons[] = {
    { XI_A, K_BUTTON_A, "BUTTON_A", "Pad A", "+gostand" },
    { XI_B, K_BUTTON_B, "BUTTON_B", "Pad B", "+stance" },
    { XI_X, K_BUTTON_X, "BUTTON_X", "Pad X", "+usereload" },
    { XI_Y, K_BUTTON_Y, "BUTTON_Y", "Pad Y", "weapnext" },
    { XI_LSHOULDER, K_BUTTON_LSHLDR, "BUTTON_LSHLDR", "Pad LB", "+frag" },
    { XI_RSHOULDER, K_BUTTON_RSHLDR, "BUTTON_RSHLDR", "Pad RB", "+smoke" },
    { XI_START, K_BUTTON_START, "BUTTON_START", "Pad Start", "togglemenu" },
    { XI_BACK, K_BUTTON_BACK, "BUTTON_BACK", "Pad Back", "+scores" },
    { XI_LTHUMB, K_BUTTON_LSTICK, "BUTTON_LSTICK", "Pad L3", "+breath_sprint" },
    { XI_RTHUMB, K_BUTTON_RSTICK, "BUTTON_RSTICK", "Pad R3", "+melee" },
    { XI_DPAD_UP, K_DPAD_UP, "DPAD_UP", "Pad Up", "+actionslot 1" },
    { XI_DPAD_DOWN, K_DPAD_DOWN, "DPAD_DOWN", "Pad Down", "+actionslot 2" },
    { XI_DPAD_LEFT, K_DPAD_LEFT, "DPAD_LEFT", "Pad Left", "+actionslot 3" },
    { XI_DPAD_RIGHT, K_DPAD_RIGHT, "DPAD_RIGHT", "Pad Right", "+actionslot 4" },
    { 0, K_BUTTON_LTRIG, "BUTTON_LTRIG", "Pad LT", "+speed_throw" },
    { 0, K_BUTTON_RTRIG, "BUTTON_RTRIG", "Pad RT", "+attack" },
};
#define CTL_NBUTTONS ((int)(sizeof(kCtlButtons) / sizeof(kCtlButtons[0])))

typedef struct { const char *name; int key; } CtlKeyName;
static CtlKeyName g_ctlKeyNames[160], g_ctlKeyNamesLoc[160];

static BOOL g_ctlOn, g_ctlKeysOk, g_ctlMoveOk, g_ctlDvarsDone;
static const BYTE *g_gpEnabled, *g_gpDzMin, *g_gpDzMax, *g_gpSens, *g_gpInvert, *g_gpBtnDz, *g_gpYaw, *g_gpPitch,
    *g_gpYawAds, *g_gpPitchAds, *g_gpCurve, *g_gpAccel, *g_gpCursor, *g_gpDebug;

/* Published by the input thread: the pad that drives the game now (real or gpad_fake), raw values. */
static volatile LONG g_padActive, g_pubLX, g_pubLY, g_pubRX, g_pubRY, g_pubButtons;
static volatile LONG g_backend;            /* 0 not started, 1 SDL, 2 XInput, -1 nothing usable */
static char g_padName[96];                 /* written by the input thread under g_ctlLock */
static volatile LONG g_padConnected;

/* Sampled once per frame on the main thread, after deadzones. */
static float g_lx, g_ly, g_rx, g_ry;
static float g_yawVel, g_pitchVel;  /* accelerated turn rates, deg/s */
static float g_adsLerp;

/* gpad_fake: synthetic state for tests without a pad (main thread writes, input thread reads). */
static volatile LONG g_fakeOn;
static volatile DWORD g_fakeUntil;
static CtlPad g_fakePad;

/* Menu cursor; when the window is not in the foreground (tests), C1's import slots report it. */
static POINT g_ctlCursor;
static volatile LONG g_ctlCursorFake;
static FARPROC g_ctlRealCursorPos;

static CRITICAL_SECTION g_ctlLock;  /* g_padName and the console-line ring below */

static float CtlDvarFloat(const BYTE *d, float def) { return d ? *(volatile const float *)(d + 0xC) : def; }

static const BYTE *CtlRegisterFloat(const char *name, float value, float min, float max, unsigned flags, const char *desc)
{
    typedef const BYTE *(__cdecl *Fn)(const char *, float, float, float, unsigned, const char *);
    static int ok = -1;
    static const BYTE kFloat[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x1C, 0xD9, 0x45, 0x0C };
    if (ok < 0) ok = Readable((void *)(DWORD_PTR)CTL_REG_FLOAT_VA, sizeof(kFloat)) &&
                     !memcmp((void *)(DWORD_PTR)CTL_REG_FLOAT_VA, kFloat, sizeof(kFloat));
    if (!ok) return NULL;
    const BYTE *d = ((Fn)(DWORD_PTR)CTL_REG_FLOAT_VA)(name, value, min, max, flags, desc);
    return d && Readable(d, 0x10) ? d : NULL;
}

static void CtlRegisterDvars(void)
{
    g_ctlDvarsDone = TRUE;
    g_gpEnabled = C1RegisterInt("gpad_enabled", 1, 0, 1, 0x1, "Gamepad support: 1 on, 0 off");
    g_gpDzMin = CtlRegisterFloat("gpad_stick_deadzone_min", 0.2f, 0.0f, 1.0f, 0x1, "Stick deadzone at the centre (radial)");
    g_gpDzMax = CtlRegisterFloat("gpad_stick_deadzone_max", 0.01f, 0.0f, 1.0f, 0x1, "Stick deadzone at the edge (full deflection below 1)");
    g_gpSens = CtlRegisterFloat("gpad_view_sensitivity", 1.0f, 0.1f, 5.0f, 0x1, "Right stick look sensitivity (scales the turn rates)");
    g_gpInvert = C1RegisterInt("gpad_invert_y", 0, 0, 1, 0x1, "Invert the right stick's vertical axis");
    g_gpBtnDz = CtlRegisterFloat("gpad_button_deadzone", 0.13f, 0.0f, 1.0f, 0x1, "Trigger press threshold");
    g_gpYaw = CtlRegisterFloat("gpad_turnrate_yaw", 260.0f, 1.0f, 1000.0f, 0x1, "Right stick yaw speed at full deflection, deg/s");
    g_gpPitch = CtlRegisterFloat("gpad_turnrate_pitch", 90.0f, 1.0f, 1000.0f, 0x1, "Right stick pitch speed at full deflection, deg/s");
    g_gpYawAds = CtlRegisterFloat("gpad_turnrate_yaw_ads", 90.0f, 1.0f, 1000.0f, 0x1, "Yaw speed while aiming down sights, deg/s");
    g_gpPitchAds = CtlRegisterFloat("gpad_turnrate_pitch_ads", 55.0f, 1.0f, 1000.0f, 0x1, "Pitch speed while aiming down sights, deg/s");
    g_gpCurve = CtlRegisterFloat("gpad_view_curve", 2.0f, 1.0f, 4.0f, 0x1, "Right stick response exponent: 1 linear, 2 quadratic (fine aim near the centre)");
    g_gpAccel = CtlRegisterFloat("gpad_view_accel", 1200.0f, 0.0f, 10000.0f, 0x1, "Turn-rate acceleration, deg/s per s (0 = instant)");
    g_gpCursor = CtlRegisterFloat("gpad_menu_cursor_speed", 1100.0f, 50.0f, 5000.0f, 0x1, "Menu cursor speed at full deflection, pixels/s");
    g_gpDebug = C1RegisterInt("gpad_debug", 0, 0, 2, 0, "Gamepad diagnostics in the console (1 state twice a second, 2 also every button)");
}

static BOOL CtlEnabled(void) { return !g_ctlDvarsDone || C1DvarInt(g_gpEnabled, 1) != 0; }

/* "+set k33_cmdFile 1" (a test client next to the player's game): never the real pad, never the real cursor. */
static BOOL CtlTestMode(void)
{
    static int t = -1;
    if (t < 0) { const char *cl = GetCommandLineA(); t = cl && strstr(cl, "k33_cmdFile 1") != NULL; }
    return t;
}

static BOOL CtlFocused(void)
{
    if (CtlTestMode()) return FALSE;
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (!fg) return FALSE;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static int CtlKeyBinding(int key) { return *(volatile const int *)(DWORD_PTR)(CTL_KEYS_VA + key * 12 + 8); }

/* Console lines from the input thread (C1's Com_Printf is main-thread only): a small ring. */
static char g_dbgRing[32][160];
static int g_dbgHead, g_dbgCount;

static void CtlDebugf(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void CtlDebugf(int level, const char *fmt, ...)
{
    if (level > 0 && C1DvarInt(g_gpDebug, 0) < level) return;
    char line[160];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    line[sizeof(line) - 1] = 0;
    EnterCriticalSection(&g_ctlLock);
    if (g_dbgCount < 32) {
        lstrcpynA(g_dbgRing[(g_dbgHead + g_dbgCount) % 32], line, sizeof(line));
        g_dbgCount++;
    }
    LeaveCriticalSection(&g_ctlLock);
}

static void CtlDrainDebug(void)
{
    char line[160];
    for (;;) {
        EnterCriticalSection(&g_ctlLock);
        if (!g_dbgCount) { LeaveCriticalSection(&g_ctlLock); return; }
        lstrcpynA(line, g_dbgRing[g_dbgHead], sizeof(line));
        g_dbgHead = (g_dbgHead + 1) % 32;
        g_dbgCount--;
        LeaveCriticalSection(&g_ctlLock);
        C1Print(C1_PRINT_CHANNEL, line);
    }
}

/* IW4's GPad_ConvertStickToFloat: radial deadzone, rescaled so the stick still reaches 1. */
static void CtlStick(SHORT sx, SHORT sy, float *ox, float *oy)
{
    float x = sx / 32767.0f, y = sy / 32767.0f;
    float len = sqrtf(x * x + y * y);
    float dmin = CtlDvarFloat(g_gpDzMin, 0.2f), dmax = CtlDvarFloat(g_gpDzMax, 0.01f);
    *ox = *oy = 0.0f;
    if (len <= 0.0f || len < dmin) return;
    float scaled = len > 1.0f - dmax ? 1.0f : (len - dmin) / (1.0f - dmin - dmax);
    if (scaled > 1.0f) scaled = 1.0f;
    *ox = x / len * scaled;
    *oy = y / len * scaled;
}

/* ---- key names ----------------------------------------------------------------------------------- */

/* Copies a {name, key} table ending in {0, 0} and appends the pad keys. */
static int CtlBuildNames(CtlKeyName *dst, int cap, DWORD src, BOOL display)
{
    int n = 0;
    const CtlKeyName *s = (const CtlKeyName *)(DWORD_PTR)src;
    while (n < cap - CTL_NBUTTONS - 1 && Readable(&s[n], sizeof(*s)) && s[n].name) { dst[n] = s[n]; n++; }
    if (!Readable(&s[n], sizeof(*s)) || s[n].name) return 0;
    for (int i = 0; i < CTL_NBUTTONS; i++) {
        dst[n].name = display ? kCtlButtons[i].display : kCtlButtons[i].name;
        dst[n].key = kCtlButtons[i].key;
        n++;
    }
    dst[n].name = NULL;
    dst[n].key = 0;
    return n;
}

static void CtlInstallKeyNames(void)
{
    /* every immediate that holds 0xCD5C88 (Key_StringToKeynum x3 sites, Key_WriteBindings x2) */
    static const DWORD kRefs[] = { 0x00492AACu, 0x00492AB2u, 0x00492B5Bu, 0x0049303Cu, 0x00493042u };
    static const DWORD kLocRef = 0x00492B52u;
    BOOL ok = TRUE;
    for (unsigned i = 0; i < sizeof(kRefs) / sizeof(kRefs[0]); i++)
        ok = ok && Readable((void *)(DWORD_PTR)kRefs[i], 4) && *(const DWORD *)(DWORD_PTR)kRefs[i] == CTL_KEYNAMES_VA;
    ok = ok && Readable((void *)(DWORD_PTR)kLocRef, 4) && *(const DWORD *)(DWORD_PTR)kLocRef == CTL_KEYNAMES_LOC_VA;
    ok = ok && Readable((void *)(DWORD_PTR)CTL_KEYNAMES_VA, 8) &&
         !strcmp((const char *)(DWORD_PTR)*(const DWORD *)(DWORD_PTR)CTL_KEYNAMES_VA, "TAB");
    for (int i = 0; ok && i < CTL_NBUTTONS; i++) /* the pad keynums must still be unnamed */
        for (const CtlKeyName *k = (const CtlKeyName *)(DWORD_PTR)CTL_KEYNAMES_VA; k->name; k++)
            if (k->key == kCtlButtons[i].key) { ok = FALSE; break; }
    int n = ok ? CtlBuildNames(g_ctlKeyNames, 160, CTL_KEYNAMES_VA, FALSE) : 0;
    int m = ok ? CtlBuildNames(g_ctlKeyNamesLoc, 160, CTL_KEYNAMES_LOC_VA, TRUE) : 0;
    if (!n || !m) {
        Log("controller: key-name table bytes differ, gamepad keys have no names (bind 0x01 .. 0x17 still works)");
        return;
    }
    DWORD t = (DWORD)(DWORD_PTR)g_ctlKeyNames, tl = (DWORD)(DWORD_PTR)g_ctlKeyNamesLoc;
    for (unsigned i = 0; i < sizeof(kRefs) / sizeof(kRefs[0]); i++) WriteCode((void *)(DWORD_PTR)kRefs[i], &t, 4);
    WriteCode((void *)(DWORD_PTR)kLocRef, &tl, 4);
    g_ctlKeysOk = TRUE;
    Log("controller: key names BUTTON_A .. DPAD_RIGHT added (%d names, %d display names)", n, m);
}

/* Default binds for the pad keys, the first time and whenever C1 has cleared every one of them: its
 * first-run key-layout screen and Options > Controls defaults run unbindall and rebind only keyboard
 * keys. A pad key the player bound or unbound by hand is left alone while any pad key is bound. */
static BOOL CtlPadUnbound(void)
{
    for (int i = 0; i < CTL_NBUTTONS; i++)
        if (CtlKeyBinding(kCtlButtons[i].key)) return FALSE;
    return TRUE;
}

static void CtlApplyDefaultBinds(BOOL force)
{
    char cmd[96];
    int applied = 0;
    if (!g_c1CbufOk || !g_ctlKeysOk) return;
    for (int i = 0; i < CTL_NBUTTONS; i++) {
        if (!force && CtlKeyBinding(kCtlButtons[i].key)) continue;
        _snprintf(cmd, sizeof(cmd), "bind %s \"%s\"\n", kCtlButtons[i].name, kCtlButtons[i].bind);
        cmd[sizeof(cmd) - 1] = 0;
        C1CbufAddText(cmd);
        applied++;
    }
    Log("controller: %d default gamepad binds applied%s", applied, force ? " (gpad_defaultbinds)" : " (no pad key was bound)");
}

/* ---- menus --------------------------------------------------------------------------------------- */

/* Test clients only (k33_cmdFile 1, never focused): C1 treats a background window as inactive, so
 * while the virtual cursor is in use its GetCursorPos / GetForegroundWindow / GetActiveWindow import
 * slots (0xB423A4 / 0xB423F4 / 0xB42454, as voice.c's k33mouse) report the cursor and the game window,
 * and SetCursorPos (0xB423F0) is swallowed for the whole session, so a test client that thinks it is
 * active can never move the player's real mouse pointer. */
#define CTL_SLOT_GETCURSORPOS 0x00B423A4u
#define CTL_SLOT_SETCURSORPOS 0x00B423F0u
#define CTL_SLOT_FOREGROUND   0x00B423F4u
#define CTL_SLOT_ACTIVE       0x00B42454u
static FARPROC g_ctlRealFg, g_ctlRealActive;

static BOOL WINAPI CtlFakeCursorPos(LPPOINT p)
{
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    if (g_ctlCursorFake && h && p) { *p = g_ctlCursor; ClientToScreen(h, p); return TRUE; }
    return ((BOOL(WINAPI *)(LPPOINT))g_ctlRealCursorPos)(p);
}
static HWND WINAPI CtlFakeFg(void)
{
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    return g_ctlCursorFake && h ? h : ((HWND(WINAPI *)(void))g_ctlRealFg)();
}
static HWND WINAPI CtlFakeActive(void)
{
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    return g_ctlCursorFake && h ? h : ((HWND(WINAPI *)(void))g_ctlRealActive)();
}
static BOOL WINAPI CtlNoSetCursorPos(int x, int y) { (void)x; (void)y; return TRUE; }

static BOOL CtlInstallTestFakes(void)
{
    if (g_ctlRealCursorPos) return TRUE;
    if (!CtlTestMode()) return FALSE;
    g_ctlRealCursorPos = *(FARPROC *)(DWORD_PTR)CTL_SLOT_GETCURSORPOS;
    g_ctlRealFg = *(FARPROC *)(DWORD_PTR)CTL_SLOT_FOREGROUND;
    g_ctlRealActive = *(FARPROC *)(DWORD_PTR)CTL_SLOT_ACTIVE;
    FARPROC f1 = (FARPROC)CtlFakeCursorPos, f2 = (FARPROC)CtlFakeFg, f3 = (FARPROC)CtlFakeActive,
            f4 = (FARPROC)CtlNoSetCursorPos;
    WriteCode((void *)(DWORD_PTR)CTL_SLOT_SETCURSORPOS, &f4, 4);
    WriteCode((void *)(DWORD_PTR)CTL_SLOT_GETCURSORPOS, &f1, 4);
    WriteCode((void *)(DWORD_PTR)CTL_SLOT_FOREGROUND, &f2, 4);
    WriteCode((void *)(DWORD_PTR)CTL_SLOT_ACTIVE, &f3, 4);
    Log("controller: test client: virtual cursor, foreground/active window faked while in use, SetCursorPos disabled");
    return TRUE;
}

/* Moves the menu cursor by (dx, dy) client pixels. With the window in the foreground the real cursor
 * moves (Windows sends the WM_MOUSEMOVE); otherwise (gpad_fake tests on a background client) C1's
 * GetCursorPos import slot 0xB423A4 reports the position, as voice.c's k33mouse does. */
static void CtlCursorMove(HWND h, float dx, float dy, BOOL focused)
{
    static float fx, fy; /* sub-pixel position, so slow stick movement still adds up */
    RECT rc;
    POINT p;
    if (!GetClientRect(h, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
    if (focused) {
        GetCursorPos(&p);
        ScreenToClient(h, &p);
        g_ctlCursorFake = FALSE;
    } else {
        if (!CtlInstallTestFakes()) return; /* a background production client: no cursor */
        if (!g_ctlCursorFake) { /* the virtual cursor starts at the window centre */
            g_ctlCursor.x = rc.right / 2;
            g_ctlCursor.y = rc.bottom / 2;
            g_ctlCursorFake = TRUE;
        }
        p = g_ctlCursor;
    }
    if ((LONG)fx != p.x || (LONG)fy != p.y) { fx = (float)p.x; fy = (float)p.y; }
    fx += dx;
    fy += dy;
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > rc.right - 1) fx = (float)(rc.right - 1);
    if (fy > rc.bottom - 1) fy = (float)(rc.bottom - 1);
    g_ctlCursor.x = (LONG)fx;
    g_ctlCursor.y = (LONG)fy;
    if (focused) {
        POINT s = g_ctlCursor;
        ClientToScreen(h, &s);
        SetCursorPos(s.x, s.y);
    } else {
        g_ctlCursorFake = TRUE;
        PostMessageA(h, WM_MOUSEMOVE, 0, MAKELPARAM(g_ctlCursor.x, g_ctlCursor.y));
    }
}

static int CtlMode(void)
{
    int kc = C1KeyCatchers();
    return (kc & (CTL_KC_CONSOLE | CTL_KC_CHAT)) ? 2
         : ((kc & CTL_KC_UI) || *(volatile int *)(DWORD_PTR)CTL_CONNSTATE_VA != CTL_CA_ACTIVE) ? 1 : 0;
}

/* Menu buttons (input thread): A is the left mouse button at the cursor, B and Start are Escape,
 * posted to the game window like the real ones. */
static void CtlMenuButton(HWND h, WORD pressed, WORD released)
{
    LPARAM lp;
    if (!CtlFocused()) {
        if (!g_ctlCursorFake) { /* start the virtual cursor at the window centre */
            RECT rc;
            if (!CtlInstallTestFakes() || !GetClientRect(h, &rc)) return;
            g_ctlCursor.x = rc.right / 2;
            g_ctlCursor.y = rc.bottom / 2;
            g_ctlCursorFake = TRUE;
        }
        lp = MAKELPARAM(g_ctlCursor.x, g_ctlCursor.y);
    } else {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(h, &p);
        lp = MAKELPARAM(p.x, p.y);
    }
    if (pressed & XI_A) PostMessageA(h, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    if (released & XI_A) PostMessageA(h, WM_LBUTTONUP, 0, lp);
    if (pressed & (XI_B | XI_START)) PostMessageA(h, WM_KEYDOWN, VK_ESCAPE, 0x00010001);
    if (released & (XI_B | XI_START)) PostMessageA(h, WM_KEYUP, VK_ESCAPE, (LPARAM)0xC0010001u);
    if ((pressed | released) & (XI_A | XI_B | XI_START))
        CtlDebugf(2, "gamepad: menu %s%s%s%s at %d,%d\n", pressed & XI_A ? "click-down " : "", released & XI_A ? "click-up " : "",
                  pressed & (XI_B | XI_START) ? "escape-down" : "", released & (XI_B | XI_START) ? "escape-up" : "",
                  (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
}

/* ---- the input thread ---------------------------------------------------------------------------- */

static int CtlButtonKey(WORD bit)
{
    for (int i = 0; i < CTL_NBUTTONS; i++)
        if ((kCtlButtons[i].xi ? kCtlButtons[i].xi : (kCtlButtons[i].key == K_BUTTON_LTRIG ? CTL_LT_BIT : CTL_RT_BIT)) == bit)
            return kCtlButtons[i].key;
    return 0;
}

/* SE_KEY into C1's event queue; time 0 = Sys_QueEvent stamps it with Sys_Milliseconds. */
static void CtlQueueKey(int key, int down)
{
    int t = 0;
    DWORD fn = CTL_QUEEVENT_VA;
    __asm__ volatile("pushl $0\n\tpushl $0\n\tpushl %2\n\tpushl %1\n\tpushl %3\n\tcall *%4\n\taddl $20, %%esp"
                     : "+a"(t) : "r"(key), "r"(down), "i"(CTL_SE_KEY), "r"(fn) : "ecx", "edx", "memory", "cc");
}

/* Input-thread state. */
static CtlPad g_real;               /* the connected pad, XInput layout */
static WORD g_trig;                 /* trigger bits latched with hysteresis */
static WORD g_sent, g_ignore;       /* buttons acted on in the current mode; held across a mode switch */
static int g_sentMode = -1;

static void CtlRelease(int mode, WORD bits)
{
    if (!bits || mode < 0) return;
    if (mode == 0) {
        for (WORD b = 1; b; b <<= 1)
            if ((bits & b) && CtlButtonKey(b)) CtlQueueKey(CtlButtonKey(b), 0);
    } else {
        HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
        if (h) CtlMenuButton(h, 0, bits);
    }
}

/* Works out which pad drives the game and turns every button change into C1 input. Called after
 * each SDL event and once per loop, so a press and release in the same millisecond both arrive. */
static void CtlEvaluate(void)
{
    CtlPad p;
    BOOL have = FALSE;
    if (CtlEnabled()) {
        if (g_fakeOn) { p = g_fakePad; have = TRUE; }
        else if (g_padConnected && CtlFocused()) { p = g_real; have = TRUE; }
    }
    if (!have) memset(&p, 0, sizeof(p));
    float thr = CtlDvarFloat(g_gpBtnDz, 0.13f) * 255.0f;
    if (p.bLeftTrigger > ((g_trig & CTL_LT_BIT) ? thr * 0.7f : thr)) g_trig |= CTL_LT_BIT; else g_trig &= ~CTL_LT_BIT;
    if (p.bRightTrigger > ((g_trig & CTL_RT_BIT) ? thr * 0.7f : thr)) g_trig |= CTL_RT_BIT; else g_trig &= ~CTL_RT_BIT;
    WORD b = (p.wButtons & 0xF3FF) | g_trig;

    InterlockedExchange(&g_pubLX, p.sThumbLX);
    InterlockedExchange(&g_pubLY, p.sThumbLY);
    InterlockedExchange(&g_pubRX, p.sThumbRX);
    InterlockedExchange(&g_pubRY, p.sThumbRY);
    InterlockedExchange(&g_pubButtons, b);
    InterlockedExchange(&g_padActive, have);

    int mode = CtlMode();
    if (mode != g_sentMode) {
        CtlRelease(g_sentMode, g_sent);   /* let go of what the old mode was told */
        g_sent = 0;
        g_ignore = b;                     /* a button held across the switch counts once it is released */
        if (g_sentMode >= 0) CtlDebugf(1, "gamepad: mode %s\n", mode == 0 ? "game" : mode == 1 ? "menu" : "text");
        g_sentMode = mode;
    }
    g_ignore &= b;
    WORD act = b & ~g_ignore;
    if (mode == 2) act &= XI_B | XI_START; /* console / chat line: only Escape */
    WORD pressed = act & ~g_sent, released = g_sent & ~act;
    if (!(pressed | released)) return;
    if (mode == 0) {
        for (WORD bit = 1; bit; bit <<= 1) {
            int key = CtlButtonKey(bit);
            if (!key || !((pressed | released) & bit)) continue;
            CtlQueueKey(key, (pressed & bit) != 0);
            CtlDebugf(2, "gamepad: key %d %s (bind %d)\n", key, (pressed & bit) ? "down" : "up", CtlKeyBinding(key));
        }
    } else {
        HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
        if (h) CtlMenuButton(h, pressed, released);
    }
    g_sent = act;
}

/* SDL backend. */
static SDL_GameController *g_sdlPad;
static SDL_JoystickID g_sdlId = -1;

static WORD CtlSdlBit(int button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A: return XI_A;
    case SDL_CONTROLLER_BUTTON_B: return XI_B;
    case SDL_CONTROLLER_BUTTON_X: return XI_X;
    case SDL_CONTROLLER_BUTTON_Y: return XI_Y;
    case SDL_CONTROLLER_BUTTON_BACK: return XI_BACK;
    case SDL_CONTROLLER_BUTTON_TOUCHPAD: return XI_BACK; /* PlayStation touchpad click: scoreboard, like Back */
    case SDL_CONTROLLER_BUTTON_START: return XI_START;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return XI_LTHUMB;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return XI_RTHUMB;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return XI_LSHOULDER;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return XI_RSHOULDER;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return XI_DPAD_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return XI_DPAD_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return XI_DPAD_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return XI_DPAD_RIGHT;
    }
    return 0;
}

static void CtlSdlOpen(int index)
{
    if (g_sdlPad || !SDL_IsGameController(index)) return;
    if (CtlTestMode()) { /* the player's pad: list it, never open it (HIDAPI opens can switch a pad's report mode) */
        Log("controller: test client: SDL sees \"%s\" (type %d), not opened", SDL_GameControllerNameForIndex(index),
            (int)SDL_GameControllerTypeForIndex(index));
        return;
    }
    g_sdlPad = SDL_GameControllerOpen(index);
    if (!g_sdlPad) { Log("controller: SDL could not open device %d: %s", index, SDL_GetError()); return; }
    g_sdlId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_sdlPad));
    const char *name = SDL_GameControllerName(g_sdlPad);
    EnterCriticalSection(&g_ctlLock);
    lstrcpynA(g_padName, name ? name : "controller", sizeof(g_padName));
    LeaveCriticalSection(&g_ctlLock);
    memset(&g_real, 0, sizeof(g_real));
    InterlockedExchange(&g_padConnected, 1);
    Log("controller: SDL opened \"%s\" (type %d, instance %d)", g_padName, (int)SDL_GameControllerGetType(g_sdlPad), (int)g_sdlId);
    CtlDebugf(0, "^3gamepad: %s connected\n", g_padName);
}

static void CtlSdlClose(void)
{
    if (!g_sdlPad) return;
    Log("controller: SDL closed \"%s\"", g_padName);
    CtlDebugf(0, "^3gamepad: %s disconnected\n", g_padName);
    SDL_GameControllerClose(g_sdlPad);
    g_sdlPad = NULL;
    g_sdlId = -1;
    memset(&g_real, 0, sizeof(g_real));
    InterlockedExchange(&g_padConnected, 0);
    for (int i = 0; i < SDL_NumJoysticks() && !g_sdlPad; i++) CtlSdlOpen(i); /* another pad still plugged in */
}

static SHORT CtlSdlAxis(Sint16 v, BOOL flip)
{
    int x = flip ? -(int)v : v;
    return (SHORT)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
}

static BOOL CtlSdlInit(void)
{
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1"); /* focus is Kernel33's decision (CtlFocused) */
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");   /* no RegisterRawInputDevices: the game's raw mouse stays alone */
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");        /* Windows.Gaming.Input wants a focused CoreWindow; XInput covers those pads */
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");    /* leave the system timer to the game */
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0"); /* Nintendo pads by position: A is the bottom button */
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");     /* PlayStation / Switch Pro through HIDAPI (no window messages) */
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0) {
        Log("controller: SDL_Init failed: %s", SDL_GetError());
        return FALSE;
    }
    SDL_version v;
    SDL_GetVersion(&v);
    Log("controller: SDL %d.%d.%d game controller backend (%d joysticks present)", v.major, v.minor, v.patch, SDL_NumJoysticks());
    for (int i = 0; i < SDL_NumJoysticks() && !g_sdlPad; i++) CtlSdlOpen(i);
    return TRUE;
}

static void CtlSdlPump(void)
{
    SDL_Event ev[32];
    int n;
    SDL_PumpEvents();
    while ((n = SDL_PeepEvents(ev, 32, SDL_GETEVENT, SDL_JOYAXISMOTION, SDL_CONTROLLERSENSORUPDATE)) > 0) {
        for (int i = 0; i < n; i++) {
            const SDL_Event *e = &ev[i];
            switch (e->type) {
            case SDL_CONTROLLERDEVICEADDED: CtlSdlOpen(e->cdevice.which); break;
            case SDL_CONTROLLERDEVICEREMOVED: if (e->cdevice.which == g_sdlId) CtlSdlClose(); break;
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                if (e->cbutton.which != g_sdlId) break;
                if (e->cbutton.state) g_real.wButtons |= CtlSdlBit(e->cbutton.button);
                else g_real.wButtons &= ~CtlSdlBit(e->cbutton.button);
                CtlEvaluate();
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (e->caxis.which != g_sdlId) break;
                switch (e->caxis.axis) {
                case SDL_CONTROLLER_AXIS_LEFTX: g_real.sThumbLX = CtlSdlAxis(e->caxis.value, FALSE); break;
                case SDL_CONTROLLER_AXIS_LEFTY: g_real.sThumbLY = CtlSdlAxis(e->caxis.value, TRUE); break; /* SDL: down is + */
                case SDL_CONTROLLER_AXIS_RIGHTX: g_real.sThumbRX = CtlSdlAxis(e->caxis.value, FALSE); break;
                case SDL_CONTROLLER_AXIS_RIGHTY: g_real.sThumbRY = CtlSdlAxis(e->caxis.value, TRUE); break;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT: g_real.bLeftTrigger = (BYTE)(e->caxis.value < 0 ? 0 : e->caxis.value >> 7); break;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: g_real.bRightTrigger = (BYTE)(e->caxis.value < 0 ? 0 : e->caxis.value >> 7); break;
                }
                CtlEvaluate();
                break;
            }
        }
    }
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT); /* nothing else is wanted; keep SDL's queue empty */
}

/* XInput fallback backend. */
static XInputGetStateFn g_xiGetState;
static int g_xiSlot = -1;
static DWORD g_xiNextScan;

static BOOL CtlXInputInit(void)
{
    static const char *const kDlls[] = { "xinput1_4.dll", "xinput9_1_0.dll", "xinput1_3.dll" };
    for (unsigned i = 0; i < sizeof(kDlls) / sizeof(kDlls[0]) && !g_xiGetState; i++) {
        HMODULE m = LoadLibraryA(kDlls[i]);
        if (m) g_xiGetState = (XInputGetStateFn)GetProcAddress(m, "XInputGetState");
        if (g_xiGetState) Log("controller: XInput backend (%s)", kDlls[i]);
    }
    return g_xiGetState != NULL;
}

/* Polls the connected slot every pass; looks for a pad every 2 s (XInputGetState on an empty slot is slow). */
static void CtlXInputPoll(void)
{
    CtlState s;
    if (g_xiSlot >= 0) {
        memset(&s, 0, sizeof(s));
        if (g_xiGetState((DWORD)g_xiSlot, &s) == ERROR_SUCCESS) {
            if (memcmp(&g_real, &s.Gamepad, sizeof(g_real))) { g_real = s.Gamepad; CtlEvaluate(); }
            return;
        }
        Log("controller: XInput pad %d disconnected", g_xiSlot);
        CtlDebugf(0, "^3gamepad: controller %d disconnected\n", g_xiSlot + 1);
        g_xiSlot = -1;
        memset(&g_real, 0, sizeof(g_real));
        InterlockedExchange(&g_padConnected, 0);
        g_xiNextScan = GetTickCount() + 2000;
        return;
    }
    if ((LONG)(GetTickCount() - g_xiNextScan) < 0) return;
    g_xiNextScan = GetTickCount() + 2000;
    for (int i = 0; i < 4; i++) {
        memset(&s, 0, sizeof(s));
        if (g_xiGetState((DWORD)i, &s) != ERROR_SUCCESS) continue;
        g_xiSlot = i;
        g_real = s.Gamepad;
        EnterCriticalSection(&g_ctlLock);
        _snprintf(g_padName, sizeof(g_padName), "XInput controller %d", i + 1);
        LeaveCriticalSection(&g_ctlLock);
        InterlockedExchange(&g_padConnected, 1);
        Log("controller: XInput pad connected in slot %d", i);
        CtlDebugf(0, "^3gamepad: controller %d connected\n", i + 1);
        return;
    }
}

static DWORD WINAPI CtlInputThread(LPVOID unused)
{
    (void)unused;
    LONG backend = CtlSdlInit() ? 1 : CtlXInputInit() ? 2 : -1;
    InterlockedExchange(&g_backend, backend);
    if (backend < 0) Log("controller: no input backend (SDL and XInput both unavailable)");
    for (;;) {
        if (g_fakeOn && g_fakeUntil && (LONG)(GetTickCount() - g_fakeUntil) >= 0) {
            InterlockedExchange(&g_fakeOn, 0);
            CtlDebugf(1, "gamepad: gpad_fake ended\n");
        }
        if (backend == 1) CtlSdlPump();
        else if (backend == 2) CtlXInputPoll();
        CtlEvaluate();   /* focus, mode, gpad_enabled and gpad_fake changes */
        Sleep(1);
    }
    return 0;
}

/* ---- per frame (main thread) --------------------------------------------------------------------- */

static void CtlSampleSticks(void)
{
    CtlStick((SHORT)g_pubLX, (SHORT)g_pubLY, &g_lx, &g_ly);
    CtlStick((SHORT)g_pubRX, (SHORT)g_pubRY, &g_rx, &g_ry);
}

/* IN_Frame (every client frame, menus included). */
__attribute__((used)) void __cdecl ControllerFrame(void)
{
    static DWORD started, lastFrame, lastDebug, frames;
    static BOOL threadStarted;
    if (!started) { started = GetTickCount(); Log("controller: first frame"); }
    if (!g_ctlDvarsDone) {
        if (!C1DvarsReady()) return;
        CtlRegisterDvars();
        Log("controller: dvars registered (gpad_enabled %d)", C1DvarInt(g_gpEnabled, -1));
    }
    if (!threadStarted) {
        threadStarted = TRUE;
        HANDLE t = CreateThread(NULL, 0, CtlInputThread, NULL, 0, NULL);
        if (t) { SetThreadPriority(t, THREAD_PRIORITY_ABOVE_NORMAL); CloseHandle(t); }
        else Log("controller: input thread could not start (%lu)", GetLastError());
    }
    {   /* after config_mp.cfg has run (8 s), then every 2 s: no pad key bound -> the defaults */
        static DWORD lastBindCheck;
        static int applies;  /* a bind that never takes would otherwise repeat forever */
        DWORD t = GetTickCount();
        if (t - started > 8000 && t - lastBindCheck > 2000 && applies < 10) {
            lastBindCheck = t;
            if (CtlPadUnbound()) { applies++; CtlApplyDefaultBinds(FALSE); }
        }
    }
    CtlDrainDebug();
    DWORD now = GetTickCount();
    float dt = lastFrame ? (now - lastFrame) * 0.001f : 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    lastFrame = now;
    frames++;
    if (!g_padActive) return;

    CtlSampleSticks();
    int mode = CtlMode();
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    if (mode == 1 && h) {
        float speed = CtlDvarFloat(g_gpCursor, 1100.0f);
        float mag = sqrtf(g_lx * g_lx + g_ly * g_ly);
        if (mag > 0.0f) CtlCursorMove(h, g_lx * mag * speed * dt, -g_ly * mag * speed * dt, CtlFocused());
    }
    if (C1DvarInt(g_gpDebug, 0) && now - lastDebug > 500) {
        lastDebug = now;
        C1Printf("gamepad: %lu frames, %s, mode %d L %.2f,%.2f R %.2f,%.2f buttons 0x%04lX kc 0x%X cursor %ld,%ld\n",
                 (unsigned long)frames, g_fakeOn ? "fake" : "pad", mode, g_lx, g_ly, g_rx, g_ry,
                 (unsigned long)g_pubButtons, C1KeyCatchers(), g_ctlCursor.x, g_ctlCursor.y);
    }
}

/* After C1's CL_MouseMove (main thread, inside CL_CreateCmd): left stick -> forwardmove/rightmove,
 * right stick -> clientActive viewangles. IW4x's CL_GamepadMove and AimAssist_ApplyTurnRates, without
 * aim assist (CONTROLLER.md phase 2). */
__attribute__((used)) void __cdecl ControllerMove(int lc, BYTE *cmd, float frametime)
{
    (void)frametime;
    static DWORD lastDebug;
    if (lc != 0 || !g_padActive || CtlMode() != 0 || !cmd || !CtlEnabled()) return;
    CtlSampleSticks(); /* once per usercmd, next to the mouse */
    BYTE *cl = (BYTE *)(DWORD_PTR)(*(volatile DWORD *)(DWORD_PTR)CTL_CL_PTR_VA ^ CTL_CL_KEY);
    if (!Readable(cl + CTL_CL_YAW, 4)) return;

    /* move: IW4x's moveScale keeps diagonal full deflection at full speed */
    float fwd = g_ly, side = g_lx;
    if (fwd != 0.0f || side != 0.0f) {
        float scale = 127.0f;
        float r = fabsf(side) <= fabsf(fwd) ? side / fwd : fwd / side;
        scale *= sqrtf(r * r + 1.0f);
        int f = (signed char)cmd[CTL_CMD_FORWARD] + (int)floorf(fwd * scale);
        int s = (signed char)cmd[CTL_CMD_RIGHT] + (int)floorf(side * scale);
        cmd[CTL_CMD_FORWARD] = (BYTE)(signed char)(f < -128 ? -128 : f > 127 ? 127 : f);
        cmd[CTL_CMD_RIGHT] = (BYTE)(signed char)(s < -128 ? -128 : s > 127 ? 127 : s);
    }

    /* look */
    int msec = *(volatile int *)(DWORD_PTR)CTL_FRAMEMSEC_VA;
    float dt = msec > 0 && msec < 200 ? msec * 0.001f : 0.0f;
    float ads = (*(volatile const DWORD *)(cmd + CTL_CMD_BUTTONS) & CTL_ADS_BUTTONS) ? 1.0f : 0.0f;
    float lerpStep = dt / 0.15f; /* about the ADS transition time */
    g_adsLerp += ads > g_adsLerp ? (g_adsLerp + lerpStep > 1.0f ? 1.0f - g_adsLerp : lerpStep)
                                 : (g_adsLerp - lerpStep < 0.0f ? -g_adsLerp : -lerpStep);
    if (*(volatile const int *)(cl + CTL_CL_PMFLAGS) & CTL_FROZEN) { g_yawVel = g_pitchVel = 0.0f; return; }
    float yawAxis = -g_rx;
    float pitchAxis = C1DvarInt(g_gpInvert, 0) ? g_ry : -g_ry;
    float mag = sqrtf(yawAxis * yawAxis + pitchAxis * pitchAxis);
    if (mag > 1.0f) mag = 1.0f;
    float curve = CtlDvarFloat(g_gpCurve, 2.0f);
    float shape = mag > 0.0f ? powf(mag, curve - 1.0f) : 0.0f;
    yawAxis *= shape;
    pitchAxis *= shape;
    float sens = CtlDvarFloat(g_gpSens, 1.0f);
    float yawRate = CtlDvarFloat(g_gpYaw, 260.0f), pitchRate = CtlDvarFloat(g_gpPitch, 90.0f);
    yawRate += (CtlDvarFloat(g_gpYawAds, 90.0f) - yawRate) * g_adsLerp;
    pitchRate += (CtlDvarFloat(g_gpPitchAds, 55.0f) - pitchRate) * g_adsLerp;
    yawRate *= sens;
    pitchRate *= sens;
    float maxYaw = *(const float *)(cl + CTL_CL_MAXYAW), maxPitch = *(const float *)(cl + CTL_CL_MAXPITCH);
    if (maxYaw > 0.0f && maxYaw < yawRate) yawRate = maxYaw;      /* cgameMaxYawSpeed (turrets, vehicles) */
    if (maxPitch > 0.0f && maxPitch < pitchRate) pitchRate = maxPitch;
    float yawTarget = fabsf(yawAxis) * yawRate, pitchTarget = fabsf(pitchAxis) * pitchRate;
    float accel = CtlDvarFloat(g_gpAccel, 1200.0f) * sens;
    if (accel <= 0.0f) {
        g_yawVel = yawTarget;
        g_pitchVel = pitchTarget;
    } else { /* IW4 aim_accel_turnrate: speed up linearly, slow down at once */
        g_yawVel = yawTarget <= g_yawVel ? yawTarget : fminf(yawTarget, g_yawVel + accel * dt);
        g_pitchVel = pitchTarget <= g_pitchVel ? pitchTarget : fminf(pitchTarget, g_pitchVel + accel * dt);
    }
    float dyaw = g_yawVel * dt * (yawAxis >= 0.0f ? 1.0f : -1.0f);
    float dpitch = g_pitchVel * dt * (pitchAxis >= 0.0f ? 1.0f : -1.0f);
    *(float *)(cl + CTL_CL_YAW) += dyaw;
    *(float *)(cl + CTL_CL_PITCH) += dpitch;

    if (C1DvarInt(g_gpDebug, 0) && GetTickCount() - lastDebug > 500) {
        lastDebug = GetTickCount();
        C1Printf("gamepad: cmd fwd %d right %d buttons 0x%lX ads %.2f; view pitch %.2f yaw %.2f (%+.3f/%+.3f in %d ms)\n",
                 (signed char)cmd[CTL_CMD_FORWARD], (signed char)cmd[CTL_CMD_RIGHT],
                 (unsigned long)*(const DWORD *)(cmd + CTL_CMD_BUTTONS), g_adsLerp, *(const float *)(cl + CTL_CL_PITCH),
                 *(const float *)(cl + CTL_CL_YAW), dpitch, dyaw, msec);
    }
}

/* ---- commands ------------------------------------------------------------------------------------ */

/* gpad_fake <lx> <ly> <rx> <ry> [lt rt [buttons [ms]]] | off: a synthetic pad (sticks -1..1, triggers
 * 0..1, XInput button bits, e.g. 0x1000 = A), for tests without a controller. Works on a background
 * client; the real pad never does. */
static void __cdecl CtlCmdFake(void)
{
    int argc = VoiceArgc();
    if (argc >= 2 && !_stricmp(VoiceArgv(1), "off")) {
        InterlockedExchange(&g_fakeOn, 0);
        C1Printf("gamepad: gpad_fake off\n");
        return;
    }
    if (argc < 5) {
        C1Printf("usage: gpad_fake <lx> <ly> <rx> <ry> [lt rt [buttons [ms]]] | off   (sticks -1..1, triggers 0..1,"
                 " XInput bits: A 0x1000 B 0x2000 X 0x4000 Y 0x8000 LB 0x100 RB 0x200 Start 0x10 Back 0x20 L3 0x40"
                 " R3 0x80 dpad 1/2/4/8)\n");
        return;
    }
    CtlPad p;
#define CTL_AXIS(i) ((SHORT)(fmaxf(-1.0f, fminf(1.0f, (float)atof(VoiceArgv(i)))) * 32767.0f))
    p.sThumbLX = CTL_AXIS(1);
    p.sThumbLY = CTL_AXIS(2);
    p.sThumbRX = CTL_AXIS(3);
    p.sThumbRY = CTL_AXIS(4);
#undef CTL_AXIS
    p.bLeftTrigger = argc > 5 ? (BYTE)(fmaxf(0.0f, fminf(1.0f, (float)atof(VoiceArgv(5)))) * 255.0f) : 0;
    p.bRightTrigger = argc > 6 ? (BYTE)(fmaxf(0.0f, fminf(1.0f, (float)atof(VoiceArgv(6)))) * 255.0f) : 0;
    p.wButtons = argc > 7 ? (WORD)strtoul(VoiceArgv(7), NULL, 0) : 0;
    int ms = argc > 8 ? atoi(VoiceArgv(8)) : 0;
    InterlockedExchange(&g_fakeOn, 0);
    g_fakePad = p;
    DWORD until = ms > 0 ? GetTickCount() + (DWORD)ms : 0;
    g_fakeUntil = (until || ms <= 0) ? until : 1;
    InterlockedExchange(&g_fakeOn, 1);
    Log("controller: gpad_fake L %d,%d R %d,%d LT %d RT %d buttons 0x%04X %d ms", p.sThumbLX, p.sThumbLY,
        p.sThumbRX, p.sThumbRY, p.bLeftTrigger, p.bRightTrigger, p.wButtons, ms);
}

static void __cdecl CtlCmdStatus(void)
{
    char name[96];
    EnterCriticalSection(&g_ctlLock);
    lstrcpynA(name, g_padName, sizeof(name));
    LeaveCriticalSection(&g_ctlLock);
    C1Printf("gamepad: %s, backend %s, %s%s; mode %s, keyCatchers 0x%X; key names %s, move/look hook %s\n",
             CtlEnabled() ? "enabled" : "DISABLED (gpad_enabled 0)",
             g_backend == 1 ? "SDL2 GameController" : g_backend == 2 ? "XInput" : g_backend < 0 ? "NONE" : "starting",
             g_padConnected ? name : "no controller", g_fakeOn ? " (gpad_fake active)" : "",
             CtlMode() == 0 ? "game" : CtlMode() == 1 ? "menu" : "text", C1KeyCatchers(), g_ctlKeysOk ? "ok" : "MISSING",
             g_ctlMoveOk ? "ok" : "MISSING");
    for (int i = 0; i < CTL_NBUTTONS; i++) {
        int b = CtlKeyBinding(kCtlButtons[i].key);
        const char *bn = b > 0 && b < 89 && g_bindOk && g_bindNames[b] ? g_bindNames[b]
                       : b > 0 && b < 86 ? (const char *)(DWORD_PTR)((const DWORD *)(DWORD_PTR)0x00CD62C0u)[b] : "";
        C1Printf("gamepad:   %-14s %s\n", kCtlButtons[i].name, b ? bn : "(unbound)");
    }
}

static void __cdecl CtlCmdDefaults(void) { CtlApplyDefaultBinds(TRUE); }

static C1Cmd g_ctlCmds[] = {
    { NULL, "gpad_fake", (void *)CtlCmdFake },
    { NULL, "gpad_status", (void *)CtlCmdStatus },
    { NULL, "gpad_defaultbinds", (void *)CtlCmdDefaults },
};

/* ---- install ------------------------------------------------------------------------------------- */

__attribute__((used)) DWORD g_ctlInFrameResume = CTL_IN_FRAME_RESUME;
__attribute__((used)) DWORD g_ctlMouseMove = CTL_MOUSEMOVE_VA;
void CTL_InFrameHook(void);
void CTL_MouseMoveHook(void);
__asm__(
    ".text\n"
    ".globl _CTL_InFrameHook\n"
    "_CTL_InFrameHook:\n"             /* jmp from IN_Frame 0x62DA90 */
    "    pushal\n"
    "    call _ControllerFrame\n"
    "    popal\n"
    "    pushl %ecx\n"                /* the displaced push ecx ; push 0xC70A08 ("2287") */
    "    pushl $0x00C70A08\n"
    "    jmp *_g_ctlInFrameResume\n"
    ".globl _CTL_MouseMoveHook\n"
    "_CTL_MouseMoveHook:\n"           /* call from CL_CreateCmd 0x490E2F: eax = lc, [esp+4] cmd, [esp+8] frametime */
    "    pushl %eax\n"
    "    pushl 12(%esp)\n"
    "    pushl 12(%esp)\n"
    "    call *_g_ctlMouseMove\n"     /* C1's CL_MouseMove(cmd, frametime), eax = lc */
    "    movl 8(%esp), %eax\n"
    "    pushl %eax\n"
    "    call _ControllerMove\n"      /* (lc, cmd, frametime) */
    "    addl $16, %esp\n"
    "    ret\n");

static void InstallController(void)
{
    static const BYTE kInFrame[] = { 0x51, 0x68, 0x08, 0x0A, 0xC7, 0x00, 0xE8 };
    /* push ebp; mov ebp,esp; push esi; push edi; push 0x2B62C60 (the queue's critical section); mov edi,eax */
    static const BYTE kQueEvent[] = { 0x55, 0x8B, 0xEC, 0x56, 0x57, 0x68, 0x60, 0x2C, 0xB6, 0x02, 0x8B, 0xF8 };
    static const BYTE kSysMs[] = { 0x83, 0x3D, 0xE0, 0x4C, 0xC4, 0x06, 0x00, 0x56, 0x8B, 0x35 }; /* then the timeGetTime slot */
    static const BYTE kMouseMove[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x54, 0x53, 0x56, 0x57, 0x8B, 0x3D, 0x18, 0xC3, 0x6F, 0x01 };
    const char *cl = GetCommandLineA();
    if (cl && (strstr(cl, "gpad_enabled 0") || strstr(cl, "gpad_enabled \"0\""))) {
        Log("controller: off (gpad_enabled 0 on the command line)");
        return;
    }
    C1GameInit();
    InitializeCriticalSection(&g_ctlLock);
    BYTE *inFrame = (BYTE *)(DWORD_PTR)CTL_IN_FRAME_VA;
    BOOL frameOk = Readable(inFrame, sizeof(kInFrame)) && !memcmp(inFrame, kInFrame, sizeof(kInFrame)) &&
                   Readable((void *)(DWORD_PTR)CTL_QUEEVENT_VA, sizeof(kQueEvent)) &&
                   !memcmp((void *)(DWORD_PTR)CTL_QUEEVENT_VA, kQueEvent, sizeof(kQueEvent)) &&
                   Readable((void *)(DWORD_PTR)CTL_SYS_MS_VA, sizeof(kSysMs)) &&
                   !memcmp((void *)(DWORD_PTR)CTL_SYS_MS_VA, kSysMs, sizeof(kSysMs));
    if (!frameOk) {
        Log("controller: IN_Frame / Sys_QueEvent / Sys_Milliseconds bytes differ, gamepad support not installed");
        return;
    }
    CtlInstallKeyNames();
    g_ctlMoveOk = Readable((void *)(DWORD_PTR)CTL_MOUSEMOVE_VA, sizeof(kMouseMove)) &&
                  !memcmp((void *)(DWORD_PTR)CTL_MOUSEMOVE_VA, kMouseMove, sizeof(kMouseMove)) &&
                  *(const BYTE *)(DWORD_PTR)(CTL_MOUSEMOVE_CALL_VA - 1) == 0x56 && /* push esi (cmd) */
                  CallsTarget(CTL_MOUSEMOVE_CALL_VA, CTL_MOUSEMOVE_VA);
    if (g_ctlMoveOk) RedirectCall(CTL_MOUSEMOVE_CALL_VA, (void *)CTL_MouseMoveHook);
    BYTE jmp[6] = { 0xE9, 0, 0, 0, 0, 0x90 };
    LONG rel = (LONG)((DWORD_PTR)CTL_InFrameHook - (CTL_IN_FRAME_VA + 5));
    memcpy(jmp + 1, &rel, 4);
    WriteCode(inFrame, jmp, sizeof(jmp));
    int added = 0;
    for (unsigned i = 0; i < sizeof(g_ctlCmds) / sizeof(g_ctlCmds[0]); i++) added += C1AddCommand(&g_ctlCmds[i]);
    g_ctlOn = TRUE;
    Log("controller: installed: IN_Frame hooked (0x62DA90), move/look %s (CL_MouseMove call 0x490E2F), key names %s, "
        "%d commands; buttons queue through Sys_QueEvent 0x62EF60", g_ctlMoveOk ? "hooked" : "NOT hooked (bytes differ)",
        g_ctlKeysOk ? "added" : "missing", added);
}

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
#define CTL_XHOLD_BIT 0x10000 /* X held for gpad_use_hold_time: the BUTTON_X_HOLD key */

/* IW4 keyNum_t gamepad codes. */
enum { K_BUTTON_A = 1, K_BUTTON_B, K_BUTTON_X, K_BUTTON_Y, K_BUTTON_LSHLDR, K_BUTTON_RSHLDR,
       K_BUTTON_START = 14, K_BUTTON_BACK, K_BUTTON_LSTICK, K_BUTTON_RSTICK, K_BUTTON_LTRIG,
       K_BUTTON_RTRIG, K_DPAD_UP, K_DPAD_DOWN, K_DPAD_LEFT, K_DPAD_RIGHT, K_BUTTON_X_HOLD = 28 };

typedef struct { DWORD xi; int key; const char *name; const char *display; const char *bind; const char *oldBind; } CtlButton;
/* Digital buttons, the two triggers (xi 0: read from the analog value) and BUTTON_X_HOLD (X held for
 * gpad_use_hold_time; a shorter tap is BUTTON_X). Default binds after IW4 console MP with C1's own
 * commands: G/+frag is C1's lethal grenade, Q/+smoke the tactical one, 4/+tact the equipment, 3/weapalt
 * the attachment (underbarrel), and killstreaks sit in action slots 1..5 (keys 8,5,6,7 -> slots 1..4),
 * which gpad_ksnext / gpad_ksuse cycle and call in. oldBind is the phase-1 default, replaced when a
 * config still has it (gpad_binds_version). */
static const CtlButton kCtlButtons[] = {
    { XI_A, K_BUTTON_A, "BUTTON_A", "A", "+gostand", NULL },
    { XI_B, K_BUTTON_B, "BUTTON_B", "B", "+stance", NULL },
    { XI_X, K_BUTTON_X, "BUTTON_X", "X", "+reload", "+usereload" },
    { XI_Y, K_BUTTON_Y, "BUTTON_Y", "Y", "weapnext", NULL },
    { XI_LSHOULDER, K_BUTTON_LSHLDR, "BUTTON_LSHLDR", "LB", "+smoke", "+frag" },
    { XI_RSHOULDER, K_BUTTON_RSHLDR, "BUTTON_RSHLDR", "RB", "+frag", "+smoke" },
    { XI_START, K_BUTTON_START, "BUTTON_START", "Start", "togglemenu", NULL },
    { XI_BACK, K_BUTTON_BACK, "BUTTON_BACK", "Back", "+scores", NULL },
    { XI_LTHUMB, K_BUTTON_LSTICK, "BUTTON_LSTICK", "LS", "+breath_sprint", NULL },
    { XI_RTHUMB, K_BUTTON_RSTICK, "BUTTON_RSTICK", "RS", "+melee", NULL },
    { XI_DPAD_UP, K_DPAD_UP, "DPAD_UP", "D-pad Up", "+tact", "+actionslot 1" },
    { XI_DPAD_DOWN, K_DPAD_DOWN, "DPAD_DOWN", "D-pad Down", "gpad_ksnext", "+actionslot 2" },
    { XI_DPAD_LEFT, K_DPAD_LEFT, "DPAD_LEFT", "D-pad Left", "weapalt", "+actionslot 3" },
    { XI_DPAD_RIGHT, K_DPAD_RIGHT, "DPAD_RIGHT", "D-pad Right", "gpad_ksuse", "+actionslot 4" },
    { 0, K_BUTTON_LTRIG, "BUTTON_LTRIG", "LT", "+speed_throw", NULL },
    { 0, K_BUTTON_RTRIG, "BUTTON_RTRIG", "RT", "+attack", NULL },
    { CTL_XHOLD_BIT, K_BUTTON_X_HOLD, "BUTTON_X_HOLD", "Hold X", "+activate", NULL },
};
#define CTL_BINDS_VERSION 2
/* What C1 prints for each pad key (Key_KeynumToString display names, so every hint, prompt and the
 * Controls screen), by the pad's family from SDL_GameControllerGetType. Nintendo pads are read by
 * position (SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS 0), so the bottom button is labelled B. */
static const char *const kCtlNamesPs[] = { "Cross", "Circle", "Square", "Triangle", "L1", "R1", "Options", "Touchpad",
                                           "L3", "R3", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right", "L2", "R2",
                                           "Hold Square" };
static const char *const kCtlNamesNin[] = { "B", "A", "Y", "X", "L", "R", "+", "-", "L Stick", "R Stick", "D-pad Up",
                                            "D-pad Down", "D-pad Left", "D-pad Right", "ZL", "ZR", "Hold Y" };
#define CTL_NBUTTONS ((int)(sizeof(kCtlButtons) / sizeof(kCtlButtons[0])))

typedef struct { const char *name; int key; } CtlKeyName;
static CtlKeyName g_ctlKeyNames[160], g_ctlKeyNamesLoc[160];

static BOOL g_ctlOn, g_ctlKeysOk, g_ctlMoveOk, g_ctlDvarsDone;
static const BYTE *g_gpEnabled, *g_gpDzMin, *g_gpDzMax, *g_gpSens, *g_gpInvert, *g_gpBtnDz, *g_gpYaw, *g_gpPitch,
    *g_gpYawAds, *g_gpPitchAds, *g_gpCurve, *g_gpAccel, *g_gpCursor, *g_gpDebug, *g_gpUseHold, *g_gpAim,
    *g_gpAimRange, *g_gpAimSnap, *g_gpGlyphs, *g_gpBindsVer, *g_gpUserinfo;
static volatile LONG g_padType;            /* 0 Xbox (and XInput), 1 PlayStation, 2 Nintendo */
static int g_locBase = -1;                 /* first pad entry in g_ctlKeyNamesLoc */

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
    g_gpUseHold = C1RegisterInt("gpad_use_hold_time", 250, 0, 2000, 0x1,
                                "Hold X this long (ms) for BUTTON_X_HOLD (+activate: pickups, use prompts); a shorter tap is BUTTON_X (reload); 0 = no split");
    g_gpAim = CtlRegisterFloat("gpad_aimassist", 1.0f, 0.0f, 1.0f, 0x1,
                               "Aim assist strength for the pad's right stick (slowdown, lock-on, ADS snap): 1 IW4 console values, 0 off. Never applies to the mouse");
    g_gpAimRange = CtlRegisterFloat("gpad_aimassist_range", 3200.0f, 0.0f, 20000.0f, 0x1, "Aim assist range, units");
    g_gpAimSnap = C1RegisterInt("gpad_aimassist_snap", 1, 0, 1, 0x1, "Aim down sights (pad LT) turns onto a target near the crosshair");
    g_gpGlyphs = C1RegisterInt("gpad_glyphs", 0, 0, 3, 0x1, "Pad button names: 0 by pad type, 1 Xbox, 2 PlayStation, 3 Nintendo");
    g_gpBindsVer = C1RegisterInt("gpad_binds_version", 0, 0, 1000, 0x1, "Default pad binds last applied (read only)");
    g_gpUserinfo = C1RegisterInt("gpad", 0, 0, 1, 0x200, "1 while a gamepad is the last-used input (userinfo, visible to the server)");
    ((const BYTE *(__cdecl *)(const char *, const char *, unsigned, const char *))(DWORD_PTR)0x00624740u)(
        "k33nav", "", 0, "Pause-menu navigator: its last decision (voice_ui/StageKeyController.as, read with gpad_debug)");
}

/* Last-used input device, for the key names C1 shows (CtlCommandKeys) and for aim assist: 1 pad,
 * 0 keyboard / mouse. g_lastMouseMs is the last real mouse movement (CL_MouseMove, menu cursor). */
static volatile LONG g_lastDevice;
static volatile DWORD g_lastMouseMs, g_lastPadMs, g_selfPostMs;
static volatile LONG g_deviceChanged;
static void CtlSetDevice(int pad)
{
    if (g_lastDevice == pad) return;
    InterlockedExchange(&g_lastDevice, pad);
    InterlockedExchange(&g_deviceChanged, 1);
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
    g_locBase = m - CTL_NBUTTONS;
    Log("controller: key names BUTTON_A .. DPAD_RIGHT added (%d names, %d display names)", n, m);
}

/* C1's bind table (voice.c and this file extend it): the name of binding index i. */
static const char *CtlBindName(int i)
{
    const char *const *t = *(const char *const *const *)(DWORD_PTR)0x00492DC3u;
    int n = *(const BYTE *)(DWORD_PTR)0x00492E24u;
    return i > 0 && i < n && t && Readable(t + i, 4) && t[i] && Readable(t[i], 1) ? t[i] : "";
}

/* Default binds for the pad keys:
 *   - whenever C1 has cleared every pad key (its first-run key-layout screen and Options > Controls
 *     defaults run unbindall and rebind only keyboard keys);
 *   - once per CTL_BINDS_VERSION: a pad key still bound to the previous default (oldBind) or unbound
 *     gets the new default, so configs written with the phase-1 defaults move to the new layout while
 *     anything the player changed stays. gpad_binds_version records it. */
static BOOL CtlPadUnbound(void)
{
    for (int i = 0; i < CTL_NBUTTONS; i++)
        if (CtlKeyBinding(kCtlButtons[i].key)) return FALSE;
    return TRUE;
}

static void CtlApplyDefaultBinds(int how) /* 0 unbound only, 1 force all, 2 migrate */
{
    char cmd[96];
    int applied = 0;
    if (!g_c1CbufOk || !g_ctlKeysOk) return;
    for (int i = 0; i < CTL_NBUTTONS; i++) {
        int cur = CtlKeyBinding(kCtlButtons[i].key);
        const char *curName = CtlBindName(cur);
        BOOL doit = how == 1 || !cur || (how == 2 && kCtlButtons[i].oldBind && !_stricmp(curName, kCtlButtons[i].oldBind));
        if (!doit || !_stricmp(curName, kCtlButtons[i].bind)) continue;
        _snprintf(cmd, sizeof(cmd), "bind %s \"%s\"\n", kCtlButtons[i].name, kCtlButtons[i].bind);
        cmd[sizeof(cmd) - 1] = 0;
        C1CbufAddText(cmd);
        applied++;
        Log("controller: bind %s \"%s\" (was \"%s\")", kCtlButtons[i].name, kCtlButtons[i].bind, curName);
    }
    if (how != 0) {
        _snprintf(cmd, sizeof(cmd), "set gpad_binds_version %d\n", CTL_BINDS_VERSION);
        C1CbufAddText(cmd);
    }
    Log("controller: %d default gamepad binds applied (%s)", applied, how == 1 ? "gpad_defaultbinds" : how == 2 ? "layout update" : "no pad key was bound");
}

/* ---- C1's key-name display path ----------------------------------------------------------------------
 * Key_GetBindingNames 0x493920 (every "[{+attack}]" in hints and prompts, the HUD, the Controls screen
 * through AS_GetKeyBindingNamesFromCommand) asks Key_GetCommandAssignment 0x492E30 (eax = command, ecx =
 * localClient, [esp+4] = int keys[2]) for the first two keys bound to a command. That call (0x49393E)
 * comes here: the keys of the last-used device only, so the same prompt reads "LT" while the pad is in
 * use and "MOUSE2" after the mouse, from the next frame on (C1 rebuilds those strings when it draws them).
 * A command bound only on the other device still shows that device's key. The bind and unbind paths keep
 * calling 0x492E30 directly. */
static BOOL CtlIsPadKey(int k)
{
    return (k >= 1 && k <= 6) || (k >= 14 && k <= 23) || k == K_BUTTON_X_HOLD;
}

__attribute__((used)) int __cdecl CtlCommandKeys(const char *cmd, int lc, int *keys)
{
    keys[0] = keys[1] = -1;
    if (!cmd || (unsigned)lc > 3) return 0;
    int idx = ((int(__cdecl *)(const char *))(DWORD_PTR)0x00492DB0u)(cmd);
    if (!idx) return 0;
    int pad = g_lastDevice == 1, n = 0;
    const BYTE *kb = (const BYTE *)(DWORD_PTR)(CTL_KEYS_VA + lc * 0xD28);
    for (int pass = 0; pass < 2 && n == 0; pass++)   /* pass 1: any device, when the active one has none */
        for (int k = 0; k < 256 && n < 2; k++) {
            if (*(const int *)(kb + k * 12 + 8) != idx) continue;
            if (pass == 0 && CtlIsPadKey(k) != pad) continue;
            keys[n++] = k;
        }
    return n;
}

__attribute__((used)) DWORD g_ctlQueEventResume = 0x0062EF65u;
void CTL_CmdKeysHook(void);
void CTL_QueEventHook(void);
__attribute__((used)) void __cdecl CtlNoteEvent(int type, int value, int down);
__asm__(
    ".text\n"
    ".globl _CTL_CmdKeysHook\n"
    "_CTL_CmdKeysHook:\n"             /* call from 0x49393E: eax = command, ecx = lc, [esp+4] = keys */
    "    pushl 4(%esp)\n"
    "    pushl %ecx\n"
    "    pushl %eax\n"
    "    call _CtlCommandKeys\n"
    "    addl $12, %esp\n"
    "    ret\n"
    ".globl _CTL_QueEventHook\n"
    "_CTL_QueEventHook:\n"            /* jmp from Sys_QueEvent 0x62EF60: eax = time; type, value, value2, ... */
    "    pushal\n"
    "    pushl 44(%esp)\n"            /* value2 (down) */
    "    pushl 44(%esp)\n"            /* value (key) */
    "    pushl 44(%esp)\n"            /* type */
    "    call _CtlNoteEvent\n"
    "    addl $12, %esp\n"
    "    popal\n"
    "    pushl %ebp\n"                /* the displaced 5 bytes */
    "    movl %esp, %ebp\n"
    "    pushl %esi\n"
    "    pushl %edi\n"
    "    jmp *_g_ctlQueEventResume\n");

/* Every input event C1 queues (window thread and the pad thread): a keyboard key or mouse button makes
 * keyboard / mouse the last-used device. The pad's own keys, and keys Kernel33 posts for the pad
 * (Escape, the pause-menu keys, menu clicks) within 150 ms, do not. */
__attribute__((used)) void __cdecl CtlNoteEvent(int type, int value, int down)
{
    if (type != CTL_SE_KEY || !down || CtlIsPadKey(value)) return;
    if (GetTickCount() - g_selfPostMs < 150) return;
    if (value >= 200 && value <= 206) g_lastMouseMs = GetTickCount();   /* MOUSE1..5, wheel */
    CtlSetDevice(0);
}

/* ---- killstreaks: select and call in --------------------------------------------------------------
 * C1 keeps each killstreak in an action slot (maps/mp/killstreaks/_killstreaks.gsc setActionSlot 1..5;
 * playerState actionSlotType[5] at +0x544, 0 empty), used with +actionslot N (keyboard 8,5,6,7 = slots
 * 1..4). gpad_ksnext selects the next filled slot, gpad_ksuse presses +actionslot <selected> (down now,
 * up 120 ms later). Both are bind-table commands (89, 90), so the D-pad binds are ordinary binds. */
static int g_ksSel;
static DWORD g_ksUpAt;
static int g_ksUpIndex;

static int CtlKsType(int slot)
{
    BYTE *cg = ParityCg();
    return slot >= 1 && slot <= 5 && cg && Readable(cg + 0x544 + (slot - 1) * 4, 4) ? *(const int *)(cg + 0x544 + (slot - 1) * 4) : 0;
}

static void CtlRunBinding(int index, int key)
{
    int lc = 0;
    DWORD fn = 0x00491580u;
    __asm__ volatile("pushl %2\n\tpushl %1\n\tcall *%3\n\taddl $8, %%esp"
                     : "+a"(lc) : "r"(index), "r"(key), "r"(fn) : "ecx", "edx", "memory", "cc");
}

static void CtlKsNext(void)
{
    for (int i = 1; i <= 5; i++) {
        int slot = (g_ksSel + i - 1) % 5 + 1;
        if (CtlKsType(slot)) {
            g_ksSel = slot;
            C1Printf("^3gamepad: killstreak slot %d selected (D-pad right calls it in)\n", slot);
            return;
        }
    }
    g_ksSel = 0;
    C1Printf("gamepad: no killstreak ready\n");
}

static void CtlKsUse(void)
{
    if (!g_ksSel || !CtlKsType(g_ksSel)) CtlKsNext();
    if (!g_ksSel) return;
    int index = 17 + 2 * (g_ksSel - 1); /* "+actionslot N" in C1's bind table */
    CtlRunBinding(index, K_DPAD_RIGHT);
    g_ksUpIndex = index + 1;
    g_ksUpAt = GetTickCount() + 120;
    Log("controller: killstreak slot %d called in", g_ksSel);
}

static void __cdecl CtlCmdKsNext(void) { CtlKsNext(); }
static void __cdecl CtlCmdKsUse(void) { CtlKsUse(); }

#define CTL_BIND_KSNEXT 89
#define CTL_BIND_KSUSE  90
#define CTL_BIND_COUNT  91
static const char *g_ctlBindNames[CTL_BIND_COUNT];
static BOOL g_ctlBindOk;

__attribute__((used)) int __cdecl CtlExecBinding(int index, int key)
{
    (void)key;
    if (index == CTL_BIND_KSNEXT) { CtlKsNext(); return 1; }
    if (index == CTL_BIND_KSUSE) { CtlKsUse(); return 1; }
    return 0;
}

__attribute__((used)) DWORD g_ctlExecResume = 0x0049158Bu;
void CTL_ExecHook(void);
void CTL_ExecChain(void);
__asm__(
    ".text\n"
    ".globl _CTL_ExecHook\n"
    "_CTL_ExecHook:\n"                /* jmp from 0x491580 (no voice hook): eax = lc, [esp+4] index, [esp+8] key */
    "    cmpl $89, 4(%esp)\n"
    "    jb 2f\n"
    "    cmpl $90, 4(%esp)\n"
    "    ja 2f\n"
    "    pushal\n"
    "    pushl 40(%esp)\n"
    "    pushl 40(%esp)\n"
    "    call _CtlExecBinding\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    ret\n"
    "2:  pushl %ebp\n"                /* the displaced first 11 bytes */
    "    movl %esp, %ebp\n"
    "    pushl %ecx\n"
    "    cmpb $0, 0x16FC2DA\n"
    "    jmp *_g_ctlExecResume\n"
    ".globl _CTL_ExecChain\n"
    "_CTL_ExecChain:\n"               /* voice.c's hook ran the prologue: ebp frame, flags of the cmp */
    "    pushfl\n"
    "    cmpl $89, 8(%ebp)\n"
    "    jb 1f\n"
    "    cmpl $90, 8(%ebp)\n"
    "    ja 1f\n"
    "    popfl\n"
    "    pushal\n"
    "    pushl 12(%ebp)\n"
    "    pushl 8(%ebp)\n"
    "    call _CtlExecBinding\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    movl %ebp, %esp\n"
    "    popl %ebp\n"
    "    ret\n"
    "1:  popfl\n"
    "    jmp *_g_ctlExecResume\n");

/* Copies whatever bind table is current (C1's 86 names, or voice.c's 89) and adds gpad_ksnext (89) and
 * gpad_ksuse (90). The executor goes through voice.c's hook when it is there, else through our own. */
static void CtlInstallBindCommands(void)
{
    static const DWORD kTableRefs[] = { 0x00492C0Au, 0x00492C11u, 0x00492DC3u, 0x004930ACu };
    static const BYTE kExec[] = { 0x55, 0x8B, 0xEC, 0x51, 0x80, 0x3D, 0xDA, 0xC2, 0x6F, 0x01, 0x00 };
    static const BYTE kCount[] = { 0x83, 0xF8 };
    const char *const *cur = *(const char *const *const *)(DWORD_PTR)kTableRefs[2];
    int n = *(const BYTE *)(DWORD_PTR)0x00492E24u;
    BOOL ok = !memcmp((void *)(DWORD_PTR)0x00492E22u, kCount, 2) && (n == 86 || n == 89) && Readable(cur, n * 4);
    for (unsigned i = 0; ok && i < sizeof(kTableRefs) / sizeof(kTableRefs[0]); i++)
        ok = *(const DWORD *)(DWORD_PTR)kTableRefs[i] == (DWORD)(DWORD_PTR)cur;
    BYTE *exec = (BYTE *)(DWORD_PTR)0x00491580u;
    BOOL viaVoice = ok && g_bindOk && exec[0] == 0xE9 && n == 89;
    ok = ok && (viaVoice || (n == 86 && !memcmp(exec, kExec, sizeof(kExec))));
    if (!ok) {
        Log("controller: bind table differs, gpad_ksnext / gpad_ksuse work from the console only");
        return;
    }
    for (int i = 0; i < n; i++) g_ctlBindNames[i] = cur[i];
    for (int i = n; i < CTL_BIND_KSNEXT; i++) g_ctlBindNames[i] = "k33_reserved";
    g_ctlBindNames[CTL_BIND_KSNEXT] = "gpad_ksnext";
    g_ctlBindNames[CTL_BIND_KSUSE] = "gpad_ksuse";
    DWORD nt = (DWORD)(DWORD_PTR)g_ctlBindNames;
    for (unsigned i = 0; i < sizeof(kTableRefs) / sizeof(kTableRefs[0]); i++) WriteCode((void *)(DWORD_PTR)kTableRefs[i], &nt, 4);
    BYTE count = CTL_BIND_COUNT;
    WriteCode((void *)(DWORD_PTR)0x00492E24u, &count, 1);
    if (viaVoice) g_voiceExecResume = (DWORD)(DWORD_PTR)CTL_ExecChain;
    else {
        BYTE jmp[5] = { 0xE9 };
        LONG rel = (LONG)((DWORD_PTR)CTL_ExecHook - (0x00491580u + 5));
        memcpy(jmp + 1, &rel, 4);
        WriteCode(exec, jmp, 5);
    }
    g_ctlBindOk = TRUE;
    Log("controller: bind table: gpad_ksnext (89) / gpad_ksuse (90) added (%s)", viaVoice ? "after voice.c's +talk" : "own executor hook");
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
        g_selfPostMs = GetTickCount();
        PostMessageA(h, WM_MOUSEMOVE, 0, MAKELPARAM(g_ctlCursor.x, g_ctlCursor.y));
    }
}

/* 0 game, 1 front-end menus (stick cursor), 2 console / chat line, 3 in-match menu (the pause menu,
 * codo.gfx opened by togglemenu: focus navigation, no cursor). */
#define CTL_MODE_GAME  0
#define CTL_MODE_MENU  1
#define CTL_MODE_TEXT  2
#define CTL_MODE_PAUSE 3
/* C1's Scaleform UI mode switch 0x6B77A0 (eax = mode; called from 0x83DA993): 1 front end, 2 the in-match
 * pause menu (cpp_openPauseMenu), 7 scoreboard, 12 HUD. keyCatchers' UI bit cannot tell the pause menu: the
 * menu's ActionScript sets and clears it as the pointer moves over its panels (callback 0x5C4110), and the
 * scoreboard sets it too. CTL_UiModeHook records the last mode requested (gpad_debug); whether the pause menu
 * is up comes from C1's menu-type word 0x6C44D64 (measured: 3 open, 2 closed). */
static volatile LONG g_uiMode = -1;

__attribute__((used)) void __cdecl CtlUiMode(int mode)
{
    if (mode != g_uiMode) CtlDebugf(1, "gamepad: UI mode %d\n", mode);
    InterlockedExchange(&g_uiMode, mode);
}

static int CtlMode(void)
{
    int kc = C1KeyCatchers();
    BOOL inMatch = *(volatile int *)(DWORD_PTR)CTL_CONNSTATE_VA == CTL_CA_ACTIVE;
    if (kc & (CTL_KC_CONSOLE | CTL_KC_CHAT)) return CTL_MODE_TEXT;
    if (!inMatch) return CTL_MODE_MENU;
    /* C1's current Scaleform menu type (0x6C44D64, the AS MenuType: 3 = PAUSE, codoingame.gfx up). The UI
     * mode switch is only called to open the menu; the menu's own Return / Esc close it without one. */
    return *(volatile int *)(DWORD_PTR)0x06C44D64u == 3 ? CTL_MODE_PAUSE : CTL_MODE_GAME;
}


/* Menu buttons (input thread): A is the left mouse button at the cursor, B and Start are Escape,
 * posted to the game window like the real ones. */
static void CtlMenuButton(HWND h, DWORD pressed, DWORD released)
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
    if (pressed | released) g_selfPostMs = GetTickCount(); /* the pad's own clicks are not keyboard / mouse use */
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

static int CtlButtonKey(DWORD bit)
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
static DWORD g_trig;                 /* trigger bits latched with hysteresis */
static DWORD g_sent, g_ignore;       /* buttons acted on in the current mode; held across a mode switch */
static int g_sentMode = -1;

static void CtlPostKey(HWND h, UINT vk, int down);
static void CtlQueueSfKey(int flashCode, int down);

static void CtlRelease(int mode, DWORD bits)
{
    if (!bits || mode < 0) return;
    if (mode == 0) {
        for (DWORD b = 1; b <= CTL_XHOLD_BIT; b <<= 1)
            if ((bits & b) && CtlButtonKey(b)) CtlQueueKey(CtlButtonKey(b), 0);
    } else if (mode == CTL_MODE_PAUSE) {
        HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
        if (bits & XI_A) CtlQueueSfKey(244, 0);
        if (h && (bits & (XI_B | XI_START))) CtlPostKey(h, VK_ESCAPE, 0);
    } else {
        HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
        if (h) CtlMenuButton(h, 0, bits);
    }
}

/* In-match menu (CTL_MODE_PAUSE): the pad's directions and A become keys the codo.gfx focus navigator
 * (voice_ui/StageKeyController.as, onStageKeyDownHandle) understands: numpad 8 up, 2 down, 4 left, 6 right,
 * 5 select (Flash key codes 104 / 98 / 100 / 102 / 101; Scaleform drops F13 and above). B / Start are Escape. Posted to the game window like real keys; Scaleform hands them to the
 * stage's KEY_DOWN listener. Directions repeat after 350 ms, then every 140 ms. */
/* Keys for the pause menu's ActionScript. C1 gives Scaleform keyboard input only through CL_KeyEvent ->
 * 0x5F3760 -> 0x6B7F80, which maps C1 keynums to Flash key codes and calls the active movie's HandleEvent
 * (vtable +0x8C) with a 0x18-byte event {type 5 down / 6 up, byte, key code, byte, int, byte}, and only
 * while keyCatchers has the UI bit, which the pause menu keeps clearing. So the pad's codes are queued
 * here (input thread) and handed to the movie directly by CtlDeliverSfKeys (main thread, IN_Frame):
 * always codo.gfx 0x810C2A4, where the pause menu lives (0x6B7F80 sends in-match keys to the HUD movie 0x810C50C,
 * so in stock C1 the pause menu takes no keys at all). */
static volatile LONG g_sfQueue[32];
static volatile LONG g_sfHead, g_sfTail;

static void CtlQueueSfKey(int flashCode, int down)
{
    LONG t = g_sfTail;
    if (((t + 1) & 31) == (g_sfHead & 31)) return; /* full: drop */
    g_sfQueue[t & 31] = flashCode | (down ? 0x10000 : 0); /* 0x20000 in flashCode: the HUD movie */
    InterlockedExchange(&g_sfTail, (t + 1) & 31);
}

typedef struct { int type; BYTE b4; BYTE pad5[3]; int code; BYTE bC; BYTE padD[3]; int i10; BYTE b14; BYTE pad15[3]; } CtlSfKeyEvent;

static void CtlDeliverSfKeys(void)
{
    while (g_sfHead != g_sfTail) {
        LONG v = g_sfQueue[g_sfHead & 31];
        InterlockedExchange(&g_sfHead, (g_sfHead + 1) & 31);
        void *movie = *(void *volatile *)(DWORD_PTR)((v & 0x20000) ? 0x0810C50Cu : 0x0810C2A4u); /* codo.gfx: the pause menu (C1 itself routes in-match keys to the HUD movie) */
        if (!movie || !Readable(movie, 4)) continue;
        void **vtbl = *(void ***)movie;
        if (!Readable(vtbl + 0x8C / 4, 4)) continue;
        CtlSfKeyEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = (v & 0x10000) ? 5 : 6;
        ev.code = v & 0xFFFF;
        ((void(__attribute__((thiscall)) *)(void *, CtlSfKeyEvent *))vtbl[0x8C / 4])(movie, &ev);
        CtlDebugf(2, "gamepad: pause menu key %d %s\n", ev.code, ev.type == 5 ? "down" : "up");
    }
}

static void CtlPostKey(HWND h, UINT vk, int down)
{
    UINT sc = MapVirtualKeyA(vk, 0);
    g_selfPostMs = GetTickCount();
    PostMessageA(h, down ? WM_KEYDOWN : WM_KEYUP, vk, (LPARAM)(1u | (sc << 16) | (down ? 0u : 0xC0000000u)));
}

static void CtlPauseNav(HWND h, DWORD pressed, DWORD released, DWORD held, float lx, float ly)
{
    static int lastDir;            /* 1 up 2 down 3 left 4 right */
    static DWORD nextRepeat;
    static const int kCode[5] = { 0, 240, 241, 242, 243 }; /* private codes for StageKeyController.as */
    int dir = 0;
    if (held & XI_DPAD_UP) dir = 1;
    else if (held & XI_DPAD_DOWN) dir = 2;
    else if (held & XI_DPAD_LEFT) dir = 3;
    else if (held & XI_DPAD_RIGHT) dir = 4;
    else if (fabsf(ly) >= 0.55f || fabsf(lx) >= 0.55f) dir = fabsf(ly) >= fabsf(lx) ? (ly > 0 ? 1 : 2) : (lx < 0 ? 3 : 4);
    else if (lastDir && (fabsf(ly) >= 0.4f || fabsf(lx) >= 0.4f)) dir = lastDir; /* hysteresis */
    DWORD now = GetTickCount();
    if (dir != lastDir) {
        if (dir) { CtlQueueSfKey(kCode[dir], 1); CtlQueueSfKey(kCode[dir], 0); nextRepeat = now + 350; }
        lastDir = dir;
    } else if (dir && (LONG)(now - nextRepeat) >= 0) {
        CtlQueueSfKey(kCode[dir], 1);
        CtlQueueSfKey(kCode[dir], 0);
        nextRepeat = now + 140;
    }
    if (pressed & XI_A) CtlQueueSfKey(244, 1);   /* select */
    if (released & XI_A) CtlQueueSfKey(244, 0);
    if (pressed & (XI_B | XI_START)) CtlPostKey(h, VK_ESCAPE, 1);
    if (released & (XI_B | XI_START)) CtlPostKey(h, VK_ESCAPE, 0);
    if (dir && dir != 0 && (pressed | released) == 0) return;
    if ((pressed | released) & (XI_A | XI_B | XI_START))
        CtlDebugf(2, "gamepad: pause menu %s%s\n", (pressed | released) & XI_A ? "select" : "escape",
                  (pressed & (XI_A | XI_B | XI_START)) ? " down" : " up");
}

/* X: a tap shorter than gpad_use_hold_time is BUTTON_X (reload); holding it is BUTTON_X_HOLD (+activate:
 * pickups, planting, "Press [Use] to ..."), console style. 0, or nothing bound to BUTTON_X_HOLD = X is
 * plain BUTTON_X. */
static int g_xState;      /* 0 up, 1 pressed (undecided), 2 holding */
static DWORD g_xDownAt;

static void CtlGameButtons(DWORD act)
{
    int holdMs = C1DvarInt(g_gpUseHold, 250);
    BOOL split = holdMs > 0 && CtlKeyBinding(K_BUTTON_X_HOLD) != 0;
    DWORD now = GetTickCount();
    DWORD xNow = act & XI_X;
    if (split) {
        act &= ~XI_X;
        if (xNow && g_xState == 0) { g_xState = 1; g_xDownAt = now; }
        if (xNow && g_xState == 1 && now - g_xDownAt >= (DWORD)holdMs) {
            g_xState = 2;
            CtlQueueKey(K_BUTTON_X_HOLD, 1);
            CtlDebugf(2, "gamepad: X held %d ms -> BUTTON_X_HOLD down (bind %d)\n", holdMs, CtlKeyBinding(K_BUTTON_X_HOLD));
        }
        if (!xNow && g_xState == 1) {
            CtlQueueKey(K_BUTTON_X, 1);
            CtlQueueKey(K_BUTTON_X, 0);
            CtlDebugf(2, "gamepad: X tap -> BUTTON_X (bind %d)\n", CtlKeyBinding(K_BUTTON_X));
        }
        if (!xNow && g_xState == 2) CtlQueueKey(K_BUTTON_X_HOLD, 0);
        if (!xNow) g_xState = 0;
    }
    DWORD pressed = act & ~g_sent, released = g_sent & ~act;
    for (DWORD bit = 1; (pressed | released) && bit <= 0x8000; bit <<= 1) {
        int key = CtlButtonKey(bit);
        if (!key || !((pressed | released) & bit)) continue;
        CtlQueueKey(key, (pressed & bit) != 0);
        CtlDebugf(2, "gamepad: key %d %s (bind %d)\n", key, (pressed & bit) ? "down" : "up", CtlKeyBinding(key));
    }
    g_sent = act;
}

static void CtlReleaseGameX(void)
{
    if (g_xState == 2) CtlQueueKey(K_BUTTON_X_HOLD, 0);
    g_xState = 0;
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
    DWORD b = (p.wButtons & 0xF3FF) | g_trig;

    InterlockedExchange(&g_pubLX, p.sThumbLX);
    InterlockedExchange(&g_pubLY, p.sThumbLY);
    InterlockedExchange(&g_pubRX, p.sThumbRX);
    InterlockedExchange(&g_pubRY, p.sThumbRY);
    InterlockedExchange(&g_pubButtons, (LONG)b);
    InterlockedExchange(&g_padActive, have);

    /* the pad becomes the last-used device on a button, a trigger, or a stick well past its deadzone
     * (0.45 of full travel: drift and a resting thumb never count) */
    if (have) {
        int lx = abs(p.sThumbLX), ly = abs(p.sThumbLY), rx = abs(p.sThumbRX), ry = abs(p.sThumbRY);
        int big = lx > ly ? lx : ly;
        if (rx > big) big = rx;
        if (ry > big) big = ry;
        if (b || big > 14745) { g_lastPadMs = GetTickCount(); CtlSetDevice(1); }
    }

    int mode = CtlMode();
    if (mode != g_sentMode) {
        CtlRelease(g_sentMode, g_sent);   /* let go of what the old mode was told */
        if (g_sentMode == CTL_MODE_GAME) CtlReleaseGameX();
        g_sent = 0;
        g_ignore = b;                     /* a button held across the switch counts once it is released */
        if (g_sentMode >= 0)
            CtlDebugf(1, "gamepad: mode %s\n", mode == 0 ? "game" : mode == 1 ? "menu" : mode == 2 ? "text" : "pause menu");
        g_sentMode = mode;
    }
    g_ignore &= b;
    DWORD act = b & ~g_ignore;
    if (mode == CTL_MODE_GAME) { CtlGameButtons(act); return; }
    if (mode == CTL_MODE_TEXT) act &= XI_B | XI_START; /* console / chat line: only Escape */
    DWORD pressed = act & ~g_sent, released = g_sent & ~act;
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    if (h && mode == CTL_MODE_PAUSE) {
        float lx, ly;
        CtlStick(p.sThumbLX, p.sThumbLY, &lx, &ly);
        CtlPauseNav(h, pressed, released, act, lx, ly);
    } else if (h && (pressed | released))
        CtlMenuButton(h, pressed, released);
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
    {
        SDL_GameControllerType t = SDL_GameControllerGetType(g_sdlPad);
        LONG type = (t == SDL_CONTROLLER_TYPE_PS3 || t == SDL_CONTROLLER_TYPE_PS4 || t == SDL_CONTROLLER_TYPE_PS5) ? 1
                  : (t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO || t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
                     t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT || t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR) ? 2 : 0;
        InterlockedExchange(&g_padType, type);
    }
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
        InterlockedExchange(&g_padType, 0);
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

/* Pad family names in C1's display table (gpad_glyphs overrides). */
static void CtlApplyPadNames(void)
{
    static int applied = -1;
    int want = C1DvarInt(g_gpGlyphs, 0);
    want = want ? want - 1 : (int)g_padType;
    if (want == applied || g_locBase < 0) return;
    applied = want;
    for (int i = 0; i < CTL_NBUTTONS; i++)
        g_ctlKeyNamesLoc[g_locBase + i].name = want == 1 ? kCtlNamesPs[i] : want == 2 ? kCtlNamesNin[i] : kCtlButtons[i].display;
    Log("controller: pad button names: %s", want == 1 ? "PlayStation" : want == 2 ? "Nintendo" : "Xbox");
}

/* Test clients only: "gpad_testmouse <dx> <dy> <ms>" feeds mouse counts into C1's own accumulator
 * (cl+0x3424/+0x342C, as IN_MouseMove's 0x48FD40 does), so CL_MouseMove turns the view exactly like a
 * mouse would. For the mixed-input aim-assist test. */
static int g_testMouseDx, g_testMouseDy;
static DWORD g_testMouseUntil;

/* The pause-menu navigator's last decision (voice_ui/StageKeyController.as sets dvar "k33nav"). */
static void CtlNavLog(void)
{
    static char last[200];
    const BYTE *d = ((const BYTE *(__cdecl *)(const char *))(DWORD_PTR)0x006232C0u)("k33nav");
    if (!d || !Readable(d, 0x10)) return;
    const char *v = *(const char *const *)(d + 0xC);
    if (!v || !Readable(v, 1) || !strncmp(v, last, sizeof(last) - 1)) return;
    lstrcpynA(last, v, sizeof(last));
    C1Printf("gamepad: %s\n", last);
}

static void CtlFrameExtras(void)
{
    DWORD now = GetTickCount();
    if (C1DvarInt(g_gpDebug, 0)) CtlNavLog();
    CtlApplyPadNames();
    if (g_ksUpAt && (LONG)(now - g_ksUpAt) >= 0) { g_ksUpAt = 0; CtlRunBinding(g_ksUpIndex, K_DPAD_RIGHT); }
    if (g_deviceChanged && g_gpUserinfo && g_c1CbufOk) {  /* userinfo "gpad": at most once a second */
        static DWORD lastSent;
        if (now - lastSent > 1000) {
            char cmd[32];
            lastSent = now;
            InterlockedExchange(&g_deviceChanged, 0);
            _snprintf(cmd, sizeof(cmd), "set gpad %d\n", (int)g_lastDevice);
            C1CbufAddText(cmd);
            CtlDebugf(1, "gamepad: last-used input: %s\n", g_lastDevice ? "pad" : "keyboard/mouse");
        }
    }
    /* the real mouse pointer moving in the menus (not by us) */
    if (CtlFocused()) {
        static POINT last;
        static DWORD lastOwn;
        POINT pt;
        GetCursorPos(&pt);
        if (g_padActive && g_lastDevice == 1) lastOwn = now; /* our own SetCursorPos */
        if ((abs(pt.x - last.x) > 12 || abs(pt.y - last.y) > 12)) {
            if (now - lastOwn > 200) { g_lastMouseMs = now; CtlSetDevice(0); }
            last = pt;
        }
    }
    if (CtlTestMode() && g_testMouseUntil && (LONG)(now - g_testMouseUntil) < 0) {
        BYTE *cl = (BYTE *)(DWORD_PTR)(*(volatile DWORD *)(DWORD_PTR)CTL_CL_PTR_VA ^ CTL_CL_KEY);
        if (Readable(cl + 0x3434, 4)) {
            int idx = *(int *)(cl + 0x3434) & 1;
            *(int *)(cl + 0x3424 + idx * 4) += g_testMouseDx;
            *(int *)(cl + 0x342C + idx * 4) += g_testMouseDy;
        }
    } else g_testMouseUntil = 0;
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
    {   /* after config_mp.cfg has run (8 s), then every 2 s: layout update, or no pad key bound -> defaults */
        static DWORD lastBindCheck;
        static int applies;  /* a bind that never takes would otherwise repeat forever */
        DWORD t = GetTickCount();
        if (t - started > 8000 && t - lastBindCheck > 2000 && applies < 10) {
            lastBindCheck = t;
            if (C1DvarInt(g_gpBindsVer, 0) < CTL_BINDS_VERSION) { applies++; CtlApplyDefaultBinds(CtlPadUnbound() ? 1 : 2); }
            else if (CtlPadUnbound()) { applies++; CtlApplyDefaultBinds(0); }
        }
    }
    CtlDrainDebug();
    CtlDeliverSfKeys();
    CtlFrameExtras();
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

/* ---- aim assist (pad only) -----------------------------------------------------------------------------
 * C1 still runs IW4's AimAssist_UpdateScreenTargets: aaGlob[lc] at 0x1501A80 (stride 0xE60) holds up to
 * 64 targets at +0x134 (0x34 each: entIndex, clipMins[2], clipMaxs[2], aimPos[3], velocity[3], distSqr,
 * crosshairDistSqr), count at +0xE34, ordered nearest the crosshair first, and the view origin / angles
 * at +0xDC / +0xE8 (IW4 AimAssistGlobals: eyeOrigin, viewOrigin, viewAngles, viewAxis, ..., adsLerp +0x120). What IW4x's AimAssist_UpdateGamePadInput does with them is done here for the pad's
 * right stick only:
 *   slowdown  the stick's turn rate x0.4 (hip) .. x0.5 (ADS) while a target is in the 90x90 (of 640x480)
 *             box around the crosshair;
 *   lock-on   while the pad is moving the view or the player (any stick past 0.05), the view follows the
 *             target's motion relative to ours (strength 0.6);
 *   ADS snap  pressing LT turns onto a target in the 160x120 box over 0.25 s (gpad_aimassist_snap).
 * It never touches C1's mouse path (CL_MouseMove and its melee-lunge helper 0x4020D0 run unchanged before
 * this) and is off unless the pad is the last-used device and the mouse has not moved for 1.5 s, so a
 * pad held in ADS while the mouse aims gives nothing, and an idle connected pad enables nothing.
 * gpad_aimassist scales all three, 0 = off. */
#define CTL_AAGLOB_VA      0x01501A80u
#define CTL_AA_TARGETS     0x134
#define CTL_AA_COUNT       0xE34
#define CTL_AA_VIEWORG     0xDC   /* eyeOrigin +0xD0, viewOrigin +0xDC, viewAngles +0xE8, viewAxis +0xF4, adsLerp +0x120 */
#define CTL_AA_VIEWANG     0xE8
#define CTL_PS_VELOCITY    0x28

typedef struct { int ent; float mins[2], maxs[2], aim[3], vel[3], distSqr, xhairDistSqr; } CtlAimTarget;

static const CtlAimTarget *CtlAimFind(float range, float hw, float hh, int prefer)
{
    const BYTE *aa = (const BYTE *)(DWORD_PTR)CTL_AAGLOB_VA;
    int n = *(const int *)(aa + CTL_AA_COUNT);
    if (n <= 0 || n > 64) return NULL;
    const CtlAimTarget *t = (const CtlAimTarget *)(aa + CTL_AA_TARGETS);
    float r2 = range * range;
    for (int pass = prefer >= 0 ? 0 : 1; pass < 2; pass++)
        for (int i = 0; i < n; i++) {
            if (pass == 0 && t[i].ent != prefer) continue;
            if (t[i].distSqr > 0.0f && t[i].distSqr <= r2 && hw >= t[i].mins[0] && t[i].maxs[0] >= -hw &&
                hh >= t[i].mins[1] && t[i].maxs[1] >= -hh)
                return &t[i];
        }
    return NULL;
}

/* A region w x h in IW4's 640x480 virtual screen as clip-space half sizes, scaled like C1's own
 * tweakables: aaGlob +0xC0 / +0xC4 hold the auto-melee region (IW4 320 x 240), aspect included. */
static void CtlAimRegion(float w, float h, float *hw, float *hh)
{
    const float *f = (const float *)(DWORD_PTR)CTL_AAGLOB_VA;
    float mw = f[0xC0 / 4], mh = f[0xC4 / 4];
    *hw = mw > 0.05f && mw < 2.0f ? mw * w / 320.0f : w / 640.0f;
    *hh = mh > 0.05f && mh < 2.0f ? mh * h / 240.0f : h / 480.0f;
}

static float CtlAngleDiff(float a, float b)
{
    float d = fmodf(a - b, 360.0f);
    if (d > 180.0f) d -= 360.0f;
    if (d < -180.0f) d += 360.0f;
    return d;
}

static int g_lockEnt = -1;
static DWORD g_snapUntil;
static BOOL g_prevAds;
static float g_preYaw, g_prePitch, g_mouseAccum;
static DWORD g_mouseAccumAt;

static BOOL CtlAimAllowed(void)
{
    return CtlDvarFloat(g_gpAim, 1.0f) > 0.0f && g_lastDevice == 1 && GetTickCount() - g_lastMouseMs > 1500;
}

/* Before C1's CL_MouseMove: the view angles, so the mouse's own turn can be measured. */
__attribute__((used)) void __cdecl ControllerPreMove(int lc)
{
    BYTE *cl = (BYTE *)(DWORD_PTR)(*(volatile DWORD *)(DWORD_PTR)CTL_CL_PTR_VA ^ CTL_CL_KEY);
    if (lc != 0 || !Readable(cl + CTL_CL_YAW, 4)) return;
    g_prePitch = *(const float *)(cl + CTL_CL_PITCH);
    g_preYaw = *(const float *)(cl + CTL_CL_YAW);
}

/* After C1's CL_MouseMove (main thread, inside CL_CreateCmd): left stick -> forwardmove/rightmove,
 * right stick -> clientActive viewangles (IW4x's CL_GamepadMove and AimAssist_ApplyTurnRates), then aim
 * assist on the stick's contribution. */
__attribute__((used)) void __cdecl ControllerMove(int lc, BYTE *cmd, float frametime)
{
    (void)frametime;
    static DWORD lastDebug;
    BYTE *cl = (BYTE *)(DWORD_PTR)(*(volatile DWORD *)(DWORD_PTR)CTL_CL_PTR_VA ^ CTL_CL_KEY);
    if (lc != 0 || !cmd || !Readable(cl + CTL_CL_YAW, 4)) return;
    DWORD now = GetTickCount();
    {   /* the mouse turned the view in C1's CL_MouseMove: keyboard / mouse is in use (0.5 deg within
         * 300 ms switches the device, so a nudge does not) */
        float moved = fabsf(CtlAngleDiff(*(const float *)(cl + CTL_CL_YAW), g_preYaw)) +
                      fabsf(*(const float *)(cl + CTL_CL_PITCH) - g_prePitch);
        if (moved > 0.0001f) {
            g_lastMouseMs = now;
            if (now - g_mouseAccumAt > 300) g_mouseAccum = 0.0f;
            g_mouseAccumAt = now;
            g_mouseAccum += moved;
            if (g_mouseAccum > 0.5f) CtlSetDevice(0);
        }
    }
    if (!g_padActive || CtlMode() != CTL_MODE_GAME || !CtlEnabled()) { g_lockEnt = -1; return; }
    CtlSampleSticks(); /* once per usercmd, next to the mouse */

    /* move: IW4x's moveScale keeps diagonal full deflection at full speed */
    float fwd = g_ly, side = g_lx;
    if (fwd != 0.0f || side != 0.0f) {
        float scale = 127.0f;
        float r = fabsf(side) <= fabsf(fwd) ? side / fwd : fwd / side;
        scale *= sqrtf(r * r + 1.0f);
        int f = (signed char)cmd[CTL_CMD_FORWARD] + (int)floorf(fwd * scale);
        int s2 = (signed char)cmd[CTL_CMD_RIGHT] + (int)floorf(side * scale);
        cmd[CTL_CMD_FORWARD] = (BYTE)(signed char)(f < -128 ? -128 : f > 127 ? 127 : f);
        cmd[CTL_CMD_RIGHT] = (BYTE)(signed char)(s2 < -128 ? -128 : s2 > 127 ? 127 : s2);
    }

    /* look */
    int msec = *(volatile int *)(DWORD_PTR)CTL_FRAMEMSEC_VA;
    float dt = msec > 0 && msec < 200 ? msec * 0.001f : 0.0f;
    BOOL adsNow = (*(volatile const DWORD *)(cmd + CTL_CMD_BUTTONS) & CTL_ADS_BUTTONS) != 0;
    float lerpStep = dt / 0.15f; /* about the ADS transition time */
    g_adsLerp += adsNow ? (g_adsLerp + lerpStep > 1.0f ? 1.0f - g_adsLerp : lerpStep)
                        : (g_adsLerp - lerpStep < 0.0f ? -g_adsLerp : -lerpStep);
    if (*(volatile const int *)(cl + CTL_CL_PMFLAGS) & CTL_FROZEN) { g_yawVel = g_pitchVel = 0.0f; g_lockEnt = -1; return; }
    float yawAxis = -g_rx;
    float pitchAxis = C1DvarInt(g_gpInvert, 0) ? g_ry : -g_ry;
    float stickMag = sqrtf(yawAxis * yawAxis + pitchAxis * pitchAxis);
    float mag = stickMag > 1.0f ? 1.0f : stickMag;
    float curve = CtlDvarFloat(g_gpCurve, 2.0f);
    float shape = mag > 0.0f ? powf(mag, curve - 1.0f) : 0.0f;
    yawAxis *= shape;
    pitchAxis *= shape;

    BOOL assist = CtlAimAllowed();
    float aim = CtlDvarFloat(g_gpAim, 1.0f), range = CtlDvarFloat(g_gpAimRange, 3200.0f);
    const char *aaNote = "";
    float slowY = 1.0f, slowP = 1.0f;
    float rw, rh, sw, sh;
    CtlAimRegion(90.0f, 90.0f, &rw, &rh);       /* aim_slowdown_region / aim_lockon_region */
    CtlAimRegion(160.0f, 120.0f, &sw, &sh);     /* aim_autoaim_region */
    if (assist && CtlAimFind(range, rw, rh, -1)) {
        float sc = 0.4f + 0.1f * g_adsLerp;                  /* aim_slowdown_*_scale 0.4, _ads 0.5 */
        slowY = slowP = 1.0f - (1.0f - sc) * aim;
        aaNote = " slowdown";
    }

    float sens = CtlDvarFloat(g_gpSens, 1.0f);
    float yawRate = CtlDvarFloat(g_gpYaw, 260.0f), pitchRate = CtlDvarFloat(g_gpPitch, 90.0f);
    yawRate += (CtlDvarFloat(g_gpYawAds, 90.0f) - yawRate) * g_adsLerp;
    pitchRate += (CtlDvarFloat(g_gpPitchAds, 55.0f) - pitchRate) * g_adsLerp;
    yawRate *= sens * slowY;
    pitchRate *= sens * slowP;
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

    /* lock-on: needs the pad to be moving the view or the player (IW4 aim_lockon_deflection 0.05) */
    const BYTE *aa = (const BYTE *)(DWORD_PTR)CTL_AAGLOB_VA;
    BYTE *cg = ParityCg();
    BOOL deflected = stickMag > 0.05f || fabsf(g_lx) > 0.05f || fabsf(g_ly) > 0.05f;
    const CtlAimTarget *lock = assist && deflected ? CtlAimFind(range, rw, rh, g_lockEnt) : NULL;
    g_lockEnt = lock ? lock->ent : -1;
    if (lock && cg && Readable(cg + CTL_PS_VELOCITY, 12)) {
        const float *va = (const float *)(aa + CTL_AA_VIEWANG), *pv = (const float *)(cg + CTL_PS_VELOCITY);
        float yr = va[1] * (float)M_PI / 180.0f, pr = va[0] * (float)M_PI / 180.0f;
        float left[3] = { -sinf(yr), cosf(yr), 0.0f };                                          /* IW axis[1] */
        float up[3] = { cosf(yr) * sinf(pr), sinf(yr) * sinf(pr), cosf(pr) };                   /* IW axis[2] */
        float rel[3] = { lock->vel[0] - pv[0], lock->vel[1] - pv[1], lock->vel[2] - pv[2] };
        float arc = sqrtf(lock->distSqr) * (float)M_PI;
        float strength = 0.6f * aim;                                                            /* aim_lockon_strength */
        dyaw += (rel[0] * left[0] + rel[1] * left[1]) / arc * 180.0f * strength * dt;
        dpitch -= (rel[0] * up[0] + rel[1] * up[1] + rel[2] * up[2]) / arc * 180.0f * strength * dt;
        aaNote = *aaNote ? " slowdown+lock-on" : " lock-on";
    }

    /* ADS snap: the pad's LT going down turns onto the target nearest the crosshair over 0.25 s */
    BOOL ltNow = (g_pubButtons & CTL_LT_BIT) != 0;
    if (adsNow && !g_prevAds && ltNow && assist && C1DvarInt(g_gpAimSnap, 1)) g_snapUntil = now + 250;
    g_prevAds = adsNow;
    if (g_snapUntil && (!assist || !adsNow || (LONG)(now - g_snapUntil) >= 0)) g_snapUntil = 0;
    if (g_snapUntil) {
        const CtlAimTarget *t = CtlAimFind(range, sw, sh, g_lockEnt);
        if (t) {
            const float *vo = (const float *)(aa + CTL_AA_VIEWORG), *va = (const float *)(aa + CTL_AA_VIEWANG);
            float d[3] = { t->aim[0] - vo[0], t->aim[1] - vo[1], t->aim[2] - vo[2] };
            float wantYaw = atan2f(d[1], d[0]) * 180.0f / (float)M_PI;
            float wantPitch = -atan2f(d[2], sqrtf(d[0] * d[0] + d[1] * d[1])) * 180.0f / (float)M_PI;
            float k = fminf(1.0f, dt / 0.08f) * aim;
            dyaw += CtlAngleDiff(wantYaw, va[1]) * k;
            dpitch += CtlAngleDiff(wantPitch, va[0]) * k;
            aaNote = " ads-snap";
        }
    }
    *(float *)(cl + CTL_CL_YAW) += dyaw;
    *(float *)(cl + CTL_CL_PITCH) += dpitch;
    g_preYaw = *(const float *)(cl + CTL_CL_YAW);   /* our own turn is not the mouse's */
    g_prePitch = *(const float *)(cl + CTL_CL_PITCH);

    if (C1DvarInt(g_gpDebug, 0) && now - lastDebug > 500) {
        lastDebug = now;
        C1Printf("gamepad: cmd fwd %d right %d buttons 0x%lX ads %.2f; view pitch %.2f yaw %.2f (%+.3f/%+.3f in %d ms); "
                 "assist %s%s, targets %d, device %s, mouse %lu ms ago\n",
                 (signed char)cmd[CTL_CMD_FORWARD], (signed char)cmd[CTL_CMD_RIGHT],
                 (unsigned long)*(const DWORD *)(cmd + CTL_CMD_BUTTONS), g_adsLerp, *(const float *)(cl + CTL_CL_PITCH),
                 *(const float *)(cl + CTL_CL_YAW), dpitch, dyaw, msec, assist ? "on" : "OFF", aaNote,
                 *(const int *)(aa + CTL_AA_COUNT), g_lastDevice ? "pad" : "kb/mouse", (unsigned long)(now - g_lastMouseMs));
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
        const char *bn = CtlBindName(b);
        C1Printf("gamepad:   %-14s %s\n", kCtlButtons[i].name, b ? bn : "(unbound)");
    }
}

static void __cdecl CtlCmdDefaults(void) { CtlApplyDefaultBinds(1); }

/* Test: "gpad_sfkey <flash key code>" sends that key down and up to codo.gfx, as the pause-menu pad keys go. */
static void __cdecl CtlCmdSfKey(void)
{
    if (VoiceArgc() < 2) return;
    int hud = VoiceArgc() > 2 && !_stricmp(VoiceArgv(2), "hud") ? 0x20000 : 0;
    CtlQueueSfKey(atoi(VoiceArgv(1)) | hud, 1);
    CtlQueueSfKey(atoi(VoiceArgv(1)) | hud, 0);
}

static void __cdecl CtlCmdTestMouse(void)
{
    if (!CtlTestMode()) { C1Printf("gpad_testmouse: test clients only (+set k33_cmdFile 1)\n"); return; }
    if (VoiceArgc() < 4) { C1Printf("usage: gpad_testmouse <dx> <dy> <ms>   (mouse counts per frame)\n"); return; }
    g_testMouseDx = atoi(VoiceArgv(1));
    g_testMouseDy = atoi(VoiceArgv(2));
    g_testMouseUntil = GetTickCount() + (DWORD)atoi(VoiceArgv(3));
}

/* aaGlob[0] fields (aim-assist offsets check). */
static void __cdecl CtlCmdAaDump(void)
{
    const BYTE *aa = (const BYTE *)(DWORD_PTR)CTL_AAGLOB_VA;
    const float *f = (const float *)aa;
    BYTE *cg = ParityCg();
    C1Printf("aaGlob: init %d prevButtons 0x%X count %d adsLerp %.2f fovScale %.3f\n", aa[0xB0], *(const int *)(aa + 0xB4),
             *(const int *)(aa + CTL_AA_COUNT), f[0x120 / 4], f[0x118 / 4]);
    C1Printf("aaGlob 0xB8..0xD4: %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n", f[0xB8 / 4], f[0xBC / 4], f[0xC0 / 4], f[0xC4 / 4],
             f[0xC8 / 4], f[0xCC / 4], f[0xD0 / 4], f[0xD4 / 4]);
    C1Printf("aaGlob eye %.1f %.1f %.1f view %.1f %.1f %.1f angles %.2f %.2f %.2f\n", f[0xD0 / 4], f[0xD4 / 4], f[0xD8 / 4],
             f[CTL_AA_VIEWORG / 4], f[CTL_AA_VIEWORG / 4 + 1], f[CTL_AA_VIEWORG / 4 + 2], f[CTL_AA_VIEWANG / 4],
             f[CTL_AA_VIEWANG / 4 + 1], f[CTL_AA_VIEWANG / 4 + 2]);
    if (cg) C1Printf("ps origin %.1f %.1f %.1f velocity %.1f %.1f %.1f\n", *(float *)(cg + 0x1C), *(float *)(cg + 0x20),
                     *(float *)(cg + 0x24), *(float *)(cg + 0x28), *(float *)(cg + 0x2C), *(float *)(cg + 0x30));
    int n = *(const int *)(aa + CTL_AA_COUNT);
    const CtlAimTarget *t = (const CtlAimTarget *)(aa + CTL_AA_TARGETS);
    for (int i = 0; i < n && i < 6; i++)
        C1Printf("  target ent %d clip %.2f,%.2f..%.2f,%.2f aim %.0f %.0f %.0f dist %.0f\n", t[i].ent, t[i].mins[0], t[i].mins[1],
                 t[i].maxs[0], t[i].maxs[1], t[i].aim[0], t[i].aim[1], t[i].aim[2], sqrtf(t[i].distSqr));
}

static C1Cmd g_ctlCmds[] = {
    { NULL, "gpad_fake", (void *)CtlCmdFake },
    { NULL, "gpad_status", (void *)CtlCmdStatus },
    { NULL, "gpad_defaultbinds", (void *)CtlCmdDefaults },
    { NULL, "gpad_ksnext", (void *)CtlCmdKsNext },
    { NULL, "gpad_ksuse", (void *)CtlCmdKsUse },
    { NULL, "gpad_testmouse", (void *)CtlCmdTestMouse },
    { NULL, "gpad_sfkey", (void *)CtlCmdSfKey },
    { NULL, "gpad_aadump", (void *)CtlCmdAaDump },
};

/* ---- install ------------------------------------------------------------------------------------- */

__attribute__((used)) DWORD g_ctlInFrameResume = CTL_IN_FRAME_RESUME;
__attribute__((used)) DWORD g_ctlUiModeResume = 0x006B77A8u;
void CTL_UiModeHook(void);
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
    ".globl _CTL_UiModeHook\n"
    "_CTL_UiModeHook:\n"             /* jmp from 0x6B77A0: eax = mode */
    "    pushal\n"
    "    pushl %eax\n"
    "    call _CtlUiMode\n"
    "    addl $4, %esp\n"
    "    popal\n"
    "    pushl %ecx\n"                /* the displaced push ecx ; cmp dword [0x6C44D5C], 0 */
    "    cmpl $0, 0x06C44D5C\n"
    "    jmp *_g_ctlUiModeResume\n"
    ".globl _CTL_MouseMoveHook\n"
    "_CTL_MouseMoveHook:\n"           /* call from CL_CreateCmd 0x490E2F: eax = lc, [esp+4] cmd, [esp+8] frametime */
    "    pushl %eax\n"
    "    pushl %eax\n"
    "    call _ControllerPreMove\n"   /* (lc): the view before the mouse */
    "    addl $4, %esp\n"
    "    movl (%esp), %eax\n"
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
    CtlInstallBindCommands();
    {   /* UI mode switch: push ecx ; cmp dword [0x6C44D5C], 0 ; push ebx ; push esi ; mov esi, eax */
        static const BYTE kUi[] = { 0x51, 0x83, 0x3D, 0x5C, 0x4D, 0xC4, 0x06, 0x00, 0x53, 0x56, 0x8B, 0xF0 };
        BYTE *ui = (BYTE *)(DWORD_PTR)0x006B77A0u;
        if (Readable(ui, sizeof(kUi)) && !memcmp(ui, kUi, sizeof(kUi))) {
            BYTE j[8] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
            LONG r = (LONG)((DWORD_PTR)CTL_UiModeHook - (0x006B77A0u + 5));
            memcpy(j + 1, &r, 4);
            WriteCode(ui, j, sizeof(j));
            Log("controller: UI mode switch hooked (0x6B77A0): pause menu detection");
        } else Log("controller: UI mode switch bytes differ, the pause menu keeps the stick cursor");
    }
    /* key names by last-used device: Key_GetBindingNames' call to Key_GetCommandAssignment */
    BOOL namesOk = CallsTarget(0x0049393Eu, 0x00492E30u) && !memcmp((void *)(DWORD_PTR)0x00493935u, "\x8b\x45\x08", 3);
    if (namesOk) RedirectCall(0x0049393Eu, (void *)CTL_CmdKeysHook);
    /* keyboard / mouse activity: Sys_QueEvent's first five bytes (push ebp; mov ebp,esp; push esi; push edi) */
    BYTE *que = (BYTE *)(DWORD_PTR)CTL_QUEEVENT_VA;
    BOOL queOk = !memcmp(que, "\x55\x8b\xec\x56\x57", 5);
    if (queOk) {
        BYTE jq[5] = { 0xE9 };
        LONG rq = (LONG)((DWORD_PTR)CTL_QueEventHook - (CTL_QUEEVENT_VA + 5));
        memcpy(jq + 1, &rq, 4);
        WriteCode(que, jq, 5);
    }
    Log("controller: key names by last-used device %s, keyboard/mouse detection %s", namesOk ? "hooked (0x49393E)" : "NOT hooked",
        queOk ? "hooked (Sys_QueEvent)" : "NOT hooked");
    int added = 0;
    for (unsigned i = 0; i < sizeof(g_ctlCmds) / sizeof(g_ctlCmds[0]); i++) added += C1AddCommand(&g_ctlCmds[i]);
    g_ctlOn = TRUE;
    Log("controller: installed: IN_Frame hooked (0x62DA90), move/look %s (CL_MouseMove call 0x490E2F), key names %s, "
        "%d commands; buttons queue through Sys_QueEvent 0x62EF60", g_ctlMoveOk ? "hooked" : "NOT hooked (bytes differ)",
        g_ctlKeysOk ? "added" : "missing", added);
}

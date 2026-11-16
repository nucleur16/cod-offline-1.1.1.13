/* padsnap.c -- the gamepad menu cursor snaps to Scaleform controls (included by kernel33.c after controller.c;
 * CONTROLLER.md section 14).
 *
 * In the front end (CTL_MODE_MENU) the left stick moves the mouse cursor (controller.c). This file makes that
 * cursor aware of the controls on screen, the way console pointer UIs (Destiny, IW4x's pad cursor) work:
 *   - the targets come from the movies themselves: codo.k33.PadSnap (zones/voice_ui_mp/.../as/codo/k33/PadSnap.as,
 *     compiled into codo.gfx and codologin.gfx) walks the display list while k33_snapscan is set and publishes
 *     every control a click would reach (k33_snapn header, k33_snap0..7 rectangles and snap points); with a dialog
 *     up only the dialog's controls; the lists the mouse wheel scrolls come with a text signature;
 *   - magnetism: over a button-sized control a partial push is slowed and kept in the control's row (a full push
 *     and large cards keep their speed);
 *   - stick released: the cursor settles on the control under / next to it, or back onto the control it passed
 *     in the last ~0.3 s when the push overshot it;
 *   - flick (a short full push) or D-pad: the cursor jumps to the next control in that direction (spatial
 *     navigation: the next row of a grid wins); in a list the wheel scrolls, a press past its last visible row
 *     scrolls it a notch, and only a list at its end lets the cursor leave;
 *   - every move is a real cursor move, so the control shows its own hover state; A clicks where the cursor is,
 *     and while A is held the cursor moves freely (dragging sliders and scroll bars).
 * Where the movie publishes nothing (no targets, or a stale list), the free cursor is unchanged. Nothing here moves
 * the cursor unless the left stick or the D-pad was used; the movie only publishes while the pad is the last-used
 * device, and a cursor moved by anything else (the mouse) ends a settle and any pending jump.
 *
 * Dvars: gpad_menu_snap (1 on), gpad_menu_snap_strength (0..1: magnetism and settle reach; 0 = jumps only; both
 * in Options > Controller), gpad_menu_snap_debug (not archived). Commands: gpad_snapdump (targets with their
 * classes, what covers the hidden ones), gpad_snapcount; test clients: gpad_snapfake, gpad_snapgoto. */

#define PS_MAX 256
#define PS_FLICK_MS 220          /* a stick push shorter than this ... */
#define PS_FLICK_PEAK 0.75f      /* ... that reached this far is a flick */
#define PS_STALE_MS 1500         /* targets older than this are not used */
#define PS_DPAD_DELAY 380
#define PS_DPAD_REPEAT 150

typedef struct { float x, y, w, h, px, py; int kind, sig; } PsTarget;   /* sig: a list's text signature (kind 5) */

static PsTarget g_ps[PS_MAX];
static PsTarget g_psWheel[16];             /* lists the mouse wheel scrolls (kind 5) */
static int g_psWheelCount;
static int g_psCount, g_psSeq = -1, g_psSeqTotal;
static DWORD g_psAt;                       /* GetTickCount of the last new target list */
static float g_psSw = 1280.0f, g_psSh = 960.0f;
static char g_psHeader[128];
static const BYTE *g_psHid[4];
static const BYTE *g_psOn, *g_psStrength, *g_psDebug, *g_psScan, *g_psN, *g_psChunk[8], *g_psCls[8];
static volatile LONG g_psDirQueue;         /* D-pad presses from the input thread: bits 1 up 2 down 4 left 8 right */
static int g_psScanSent = -1;
static DWORD g_psDumpUntil;

/* main-thread cursor state */
static float g_psCx, g_psCy;               /* where we last put the cursor (client px), -1 = unknown */
static BOOL g_psOwn;                       /* the cursor is where we left it */
static int g_psGlide;                      /* 0 none, 1 settling toward g_psGx/Gy */
static float g_psGx, g_psGy;
static DWORD g_psPushAt;                   /* stick push start (0 = at rest) */
static float g_psPeak, g_psPeakX, g_psPeakY, g_psStartX, g_psStartY;
static int g_psPendDir, g_psPendSeq;       /* a jump waiting for the list to scroll */
static DWORD g_psPendUntil;
static DWORD g_psDpadHeld, g_psDpadNext;
static DWORD g_psLastStick;
static float g_psCrossX, g_psCrossY;      /* the last control the cursor passed over during this push ... */
static DWORD g_psCrossAt;                  /* ... and when (0 = none) */
static float g_psFromX, g_psFromY;         /* the control the push started on (never "passed over") */
static LONG g_psDirWait;                   /* D-pad presses made before the list was there */
static DWORD g_psDirAt;

/* measurement: stick pushes, flicks and D-pad presses since gpad_snapcount reset */
static int g_psPushes, g_psFlicks, g_psDpads, g_psJumps, g_psSettles;

static const BYTE *PsFind(const char *name)
{
    const BYTE *d = ((const BYTE *(__cdecl *)(const char *))(DWORD_PTR)0x006232C0u)(name);
    return d && Readable(d, 0x10) ? d : NULL;
}

static const char *PsStr(const BYTE *d)
{
    const char *v = d && Readable(d, 0x10) ? *(const char *const *)(d + 0xC) : NULL;
    return v && Readable(v, 1) ? v : "";
}

static void PadSnapRegisterDvars(void)
{
    typedef const BYTE *(__cdecl *RegStr)(const char *, const char *, unsigned, const char *);
    RegStr reg = (RegStr)(DWORD_PTR)0x00624740u;
    /* C1 keeps a registered dvar's name pointer: the names must be static */
    static const char *const kChunk[8] = { "k33_snap0", "k33_snap1", "k33_snap2", "k33_snap3", "k33_snap4", "k33_snap5", "k33_snap6", "k33_snap7" };
    static const char *const kCls[8] = { "k33_snapcls0", "k33_snapcls1", "k33_snapcls2", "k33_snapcls3", "k33_snapcls4", "k33_snapcls5", "k33_snapcls6", "k33_snapcls7" };
    g_psOn = C1RegisterInt("gpad_menu_snap", 1, 0, 1, 0x1, "Menus: the pad's cursor snaps to buttons, list rows and sliders (1 on)");
    g_psStrength = CtlRegisterFloat("gpad_menu_snap_strength", 0.6f, 0.0f, 1.0f, 0x1,
                                    "Menu cursor magnetism: how much it slows down and pulls toward a control, and how far it settles (0 = D-pad / flick jumps only)");
#if K33_DIAGNOSTICS
    g_psDebug = C1RegisterInt("gpad_menu_snap_debug", 0, 0, 2, 0, "Menu cursor snapping diagnostics (1 decisions, 2 also every target list)");
#endif
    g_psScan = C1RegisterInt("k33_snapscan", 0, 0, 2, 0, "Kernel33 -> codo.k33.PadSnap: publish the controls (1), with their classes (2)");
    g_psN = reg("k33_snapn", "", 0, "codo.k33.PadSnap: seq targets chunks stageW stageH mouseX mouseY ms nodes hidden movie (read only)");
    reg("k33_snaperr", "", 0, "codo.k33.PadSnap: its last ActionScript error (read only)");
    reg("k33_snapprobe", "", 0, "codo.k33.PadSnap: \"x y\" (stage) to describe instead of the mouse position (gpad_snapdump)");
    reg("k33_snapat", "", 0, "codo.k33.PadSnap: with k33_snapscan 2, what is under the mouse (read only)");
    static const char *const kHid[4] = { "k33_snaphid0", "k33_snaphid1", "k33_snaphid2", "k33_snaphid3" };
    for (int i = 0; i < 4; i++)
        g_psHid[i] = reg(kHid[i], "", 0, "codo.k33.PadSnap: with k33_snapscan 2, the controls a click cannot reach and what covers them (read only)");
    for (int i = 0; i < 8; i++) {
        g_psChunk[i] = reg(kChunk[i], "", 0, "codo.k33.PadSnap: targets \"x y w h px py kind\" (read only)");
        g_psCls[i] = reg(kCls[i], "", 0, "codo.k33.PadSnap: the targets' classes with k33_snapscan 2 (read only)");
    }
}

/* gpad_menu_snap_debug: decisions to the console and Kernel33.log */
static void PsLog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void PsLog(const char *fmt, ...)
{
    char line[300];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    line[sizeof(line) - 1] = 0;
    C1Print(C1_PRINT_CHANNEL, line);
    size_t n = strlen(line);
    if (n && line[n - 1] == 10) line[n - 1] = 0;
    Log("%s", line);
}

static BOOL PsEnabled(void) { return g_psOn && C1DvarInt(g_psOn, 1) != 0; }
static float PsStrength(void) { float s = CtlDvarFloat(g_psStrength, 0.6f); return s < 0 ? 0 : s > 1 ? 1 : s; }
static int PsDebug(void) { return C1DvarInt(g_psDebug, 0); }

/* The movie's latest list -> g_ps (client pixels). The stage is the visible area (stageW x stageH) around the
 * movie's authored area (aw x ah, 1280 x 960), scaled uniformly to the client height and centred: a 16:9 window
 * shows 1706 x 960 stage units, x from -213. Measured against the published stage mouse position (gpad_snapdump). */
static float g_psK = 1.0f, g_psOx, g_psOy, g_psMouseX, g_psMouseY;

static void PsRead(HWND h)
{
    const char *hd = PsStr(g_psN);
    int seq, n, chunks, sw, sh, mx, my, ms, nodes, hidden, aw, ah;
    if (!hd[0] || sscanf(hd, "%d %d %d %d %d %d %d %d %d %d %d %d", &seq, &n, &chunks, &sw, &sh, &mx, &my, &ms, &nodes, &hidden,
                         &aw, &ah) != 12 || seq == g_psSeq) return;
    g_psSeq = seq;
    g_psSeqTotal++;
    lstrcpynA(g_psHeader, hd, sizeof(g_psHeader));
    RECT rc;
    if (!GetClientRect(h, &rc) || rc.right <= 0 || rc.bottom <= 0 || sw <= 0 || sh <= 0) return;
    float k = (float)rc.bottom / sh, ox = (rc.right - aw * k) / 2.0f, oy = (rc.bottom - ah * k) / 2.0f;
    g_psK = k; g_psOx = ox; g_psOy = oy;
    g_psMouseX = mx * k + ox;
    g_psMouseY = my * k + oy;
    g_psSw = (float)sw;
    g_psSh = (float)sh;
    int count = 0, wheels = 0;
    for (int c = 0; c < chunks && c < 8; c++) {
        const char *s = PsStr(g_psChunk[c]);
        while (*s && count < PS_MAX) {
            int x, y, w, hh, px, py, kd, used = 0;
            if (sscanf(s, "%d %d %d %d %d %d %d%n", &x, &y, &w, &hh, &px, &py, &kd, &used) != 7) break;
            PsTarget *t = kd == 5 ? (wheels < 16 ? &g_psWheel[wheels++] : NULL) : &g_ps[count++];
            s += used;
            while (*s == ',' || *s == ' ') s++;
            if (!t) continue;
            t->x = x * k + ox; t->y = y * k + oy; t->w = w * k; t->h = hh * k;
            t->px = px * k + ox; t->py = py * k + oy; t->kind = kd; t->sig = px;
        }
    }
    g_psCount = count;
    g_psWheelCount = wheels;
    g_psAt = GetTickCount();
    if (PsDebug() >= 2) PsLog("padsnap: list %s -> %d targets\n", hd, count);
}

/* Tells the movie whether to publish: only while the pad drives the menu cursor (no display-list walks for a
 * mouse player). */
static DWORD g_psFakeRestAt;
static void PsBotFrame(DWORD now);

static void PadSnapFrame(void)
{
    if (!g_ctlDvarsDone || !g_psScan) return;
    DWORD now = GetTickCount();
    PsBotFrame(now);
    if (g_psFakeRestAt && (LONG)(now - g_psFakeRestAt) >= 0) {   /* gpad_snapfake: the push is over, the pad stays */
        CtlPad rest;
        memset(&rest, 0, sizeof(rest));
        g_fakePad = rest;
        g_psFakeRestAt = 0;
    }
    int want = 0;
    if (PsEnabled() && CtlMode() == CTL_MODE_MENU && g_lastDevice == 1 && now - g_lastPadMs < 20000)
        want = (g_psDumpUntil && (LONG)(now - g_psDumpUntil) < 0) ? 2 : 1;
    if (want != g_psScanSent && g_c1CbufOk) {
        char cmd[40];
        _snprintf(cmd, sizeof(cmd), "set k33_snapscan %d\n", want);
        C1CbufAddText(cmd);
        g_psScanSent = want;
        if (!want) g_psCount = 0;
    }
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    if (want && h) PsRead(h);
}

/* Input thread (menu mode): D-pad presses. */
static void PadSnapPadButtons(DWORD pressed, DWORD held)
{
    (void)held;
    LONG bits = 0;
    if (pressed & XI_DPAD_UP) bits |= 1;
    if (pressed & XI_DPAD_DOWN) bits |= 2;
    if (pressed & XI_DPAD_LEFT) bits |= 4;
    if (pressed & XI_DPAD_RIGHT) bits |= 8;
    if (bits) InterlockedOr(&g_psDirQueue, bits);
}

static BOOL PsCursor(HWND h, float *x, float *y)
{
    POINT p;
    if (CtlFocused()) {
        if (!GetCursorPos(&p) || !ScreenToClient(h, &p)) return FALSE;
    } else {
        if (!g_ctlCursorFake) {
            RECT rc;
            if (!CtlInstallTestFakes() || !GetClientRect(h, &rc)) return FALSE;
            g_ctlCursor.x = rc.right / 2;
            g_ctlCursor.y = rc.bottom / 2;
            g_ctlCursorFake = TRUE;
        }
        p = g_ctlCursor;
    }
    *x = (float)p.x;
    *y = (float)p.y;
    return TRUE;
}

static void PsMoveTo(HWND h, float x, float y)
{
    float cx, cy;
    if (!PsCursor(h, &cx, &cy)) return;
    CtlCursorMove(h, x - cx, y - cy, CtlFocused());
    g_psCx = x;
    g_psCy = y;
    g_psOwn = TRUE;
}

/* Distance from (x, y) to the target's rectangle (0 inside). */
static float PsRectDist(const PsTarget *t, float x, float y)
{
    float dx = x < t->x ? t->x - x : x > t->x + t->w ? x - (t->x + t->w) : 0.0f;
    float dy = y < t->y ? t->y - y : y > t->y + t->h ? y - (t->y + t->h) : 0.0f;
    return sqrtf(dx * dx + dy * dy);
}

/* The control at the cursor, else the nearest within `reach` px. Of several containing it, a clearly smaller one
 * (a button inside a list row) wins; of overlapping ones of about one size (side-bar buttons whose drawn bounds overlap),
 * the one whose point is nearest. */
static const PsTarget *PsNear(float x, float y, float reach)
{
    const PsTarget *best = NULL;
    float bestD = reach, bestA = 0, bestP = 0;
    for (int i = 0; i < g_psCount; i++) {
        const PsTarget *t = &g_ps[i];
        float d = PsRectDist(t, x, y), a = t->w * t->h;
        float p = (t->px - x) * (t->px - x) + (t->py - y) * (t->py - y);
        BOOL better;
        if (d <= 0.5f && bestD <= 0.5f && best)
            better = a < bestA * 0.7f || (a < bestA * 1.43f && !(bestA < a * 0.7f) && p < bestP);
        else
            better = d < bestD - 0.5f;
        if (better) { best = t; bestD = d; bestA = a; bestP = p; }
    }
    return best;
}

/* Nearest control in direction (dx, dy) (unit) from (x, y), the way console menus move focus: a control counts when
 * its point lies ahead (within 60 degrees of the direction); the score is the gap to its rectangle along the
 * direction plus twice the gap across it (0 when the rectangle spans the cursor's line, so the next row of a grid
 * wins over a nearer-looking control off to the side), plus a quarter of the point's offset across (among one row,
 * the one most in line). */
static const PsTarget *PsInDir(float x, float y, float dx, float dy)
{
    const PsTarget *best = NULL, *here = PsNear(x, y, 1.0f);
    float bestS = 1e9f;
    for (int i = 0; i < g_psCount; i++) {
        const PsTarget *t = &g_ps[i];
        if (t == here) continue;
        float vx = t->px - x, vy = t->py - y;
        float along = vx * dx + vy * dy, across = fabsf(vx * dy - vy * dx);
        if (along < 6.0f || across > along * 1.732f) continue;
        /* the rectangle's point nearest the cursor */
        float ex = fmaxf(t->x, fminf(x, t->x + t->w)) - x, ey = fmaxf(t->y, fminf(y, t->y + t->h)) - y;
        float gapAlong = ex * dx + ey * dy, gapAcross = fabsf(ex * dy - ey * dx);
        if (gapAlong < 0.0f) gapAlong = 0.0f;
        float s = gapAlong + 2.0f * gapAcross + 0.25f * across;
        if (s < bestS) { bestS = s; best = t; }
    }
    return best;
}

static void PsDirVec(int bit, float *dx, float *dy)
{
    *dx = bit == 4 ? -1.0f : bit == 8 ? 1.0f : 0.0f;
    *dy = bit == 1 ? -1.0f : bit == 2 ? 1.0f : 0.0f;
}

/* The smallest list the wheel scrolls that holds (x, y), or NULL. */
static const PsTarget *PsListAt(float x, float y)
{
    const PsTarget *list = NULL;
    for (int i = 0; i < g_psWheelCount; i++)
        if (PsRectDist(&g_psWheel[i], x, y) <= 0.5f && (!list || g_psWheel[i].w * g_psWheel[i].h < list->w * list->h)) list = &g_psWheel[i];
    return list;
}

static BOOL PsInside(const PsTarget *list, const PsTarget *t)
{
    return list && t->px >= list->x && t->px <= list->x + list->w && t->py >= list->y && t->py <= list->y + list->h;
}

static int g_psPendSig;                    /* the list's signature before the wheel moved it */
static float g_psPendX, g_psPendY;         /* the list's rectangle, to find it again */

/* Jump in direction dir (1 up 2 down 4 left 8 right, or a stick direction): to the next control. Inside a list the
 * mouse wheel scrolls, a press up / down past its last visible row scrolls it one notch instead (the next row moves
 * under the cursor); only a list that no longer scrolls lets the cursor leave it. */
static void PsJump(HWND h, float x, float y, float dx, float dy, int dirBit)
{
    const PsTarget *t = PsInDir(x, y, dx, dy);
    const PsTarget *list = (dirBit == 1 || dirBit == 2) ? PsListAt(x, y) : NULL;
    if (list && (!t || !PsInside(list, t))) {
        POINT s = { (LONG)x, (LONG)y };
        ClientToScreen(h, &s);
        g_selfPostMs = GetTickCount();
        PostMessageA(h, WM_MOUSEWHEEL, MAKEWPARAM(0, dirBit == 1 ? WHEEL_DELTA : -WHEEL_DELTA), MAKELPARAM(s.x, s.y));
        g_psPendDir = dirBit;
        g_psPendSeq = g_psSeq;
        g_psPendSig = list->sig;
        g_psPendX = list->x + list->w / 2;
        g_psPendY = list->y + list->h / 2;
        g_psPendUntil = GetTickCount() + 700;
        if (PsDebug()) PsLog("padsnap: list end %s, wheel %s\n", dirBit == 1 ? "above" : "below", dirBit == 1 ? "up" : "down");
        return;
    }
    if (t) {
        PsMoveTo(h, t->px, t->py);
        g_psGlide = 0;
        g_psJumps++;
        if (PsDebug()) PsLog("padsnap: jump %.2f,%.2f from %.0f,%.0f -> %.0f,%.0f (%d targets)\n", dx, dy, x, y, t->px, t->py, g_psCount);
    } else if (PsDebug()) PsLog("padsnap: nothing in direction %.2f,%.2f\n", dx, dy);
}

/* After the wheel: the list scrolled (its text changed) -> the next row is under the cursor, settle on it; it did not
 * (its end) -> leave it in that direction. */
static void PsPendingJump(HWND h, float x, float y)
{
    int d = g_psPendDir;
    g_psPendDir = 0;
    if ((LONG)(GetTickCount() - g_psPendUntil) >= 0) return;
    const PsTarget *list = PsListAt(g_psPendX, g_psPendY);
    if (list && list->sig != g_psPendSig) {
        const PsTarget *t = PsNear(x, y, 4.0f);
        if (t) PsMoveTo(h, t->px, t->py);
        g_psJumps++;
        return;
    }
    float dx, dy;
    PsDirVec(d, &dx, &dy);
    const PsTarget *t = PsInDir(x, y, dx, dy);
    if (t && !PsInside(list, t)) { PsMoveTo(h, t->px, t->py); g_psJumps++; }
}


/* Main thread, menu mode, pad active: TRUE when the cursor was handled here (FALSE: controller.c's free cursor). */
static BOOL PadSnapCursor(HWND h, float dt)
{
    DWORD now = GetTickCount();
    LONG dirs = InterlockedExchange(&g_psDirQueue, 0);
    DWORD held = (DWORD)g_pubButtons;
    BOOL live = PsEnabled() && g_psCount > 0 && now - g_psAt < PS_STALE_MS;
    float x, y;
    if (!PsCursor(h, &x, &y)) return FALSE;
    /* the cursor moved by something else (the mouse): forget the settle and any pending jump */
    if (g_psOwn && (fabsf(x - g_psCx) > 3.0f || fabsf(y - g_psCy) > 3.0f)) { g_psOwn = FALSE; g_psGlide = 0; g_psPendDir = 0; }
    if (dirs) { g_psDpads++; g_psDirAt = now; }
    dirs |= g_psDirWait;
    g_psDirWait = 0;
    if (!live) {
        /* the movie starts publishing when the pad comes back: a D-pad press waits for the list (half a second) */
        if (dirs && now - g_psDirAt < 500) g_psDirWait = dirs;
        g_psGlide = 0;
        g_psPushAt = 0;
        return FALSE;
    }
    if (held & XI_A) { g_psGlide = 0; g_psPushAt = 0; return FALSE; }   /* dragging: free cursor */

    float scale = 1.0f;   /* the movie is authored for 960 lines */
    RECT rc;
    if (GetClientRect(h, &rc) && rc.bottom > 0) scale = rc.bottom / 960.0f;
    float strength = PsStrength();

    /* D-pad: a press jumps, holding repeats */
    DWORD dpad = held & (XI_DPAD_UP | XI_DPAD_DOWN | XI_DPAD_LEFT | XI_DPAD_RIGHT);
    int bit = 0;
    if (dirs) {
        bit = (dirs & 1) ? 1 : (dirs & 2) ? 2 : (dirs & 4) ? 4 : 8;
        g_psDpadHeld = dpad;
        g_psDpadNext = now + PS_DPAD_DELAY;
    } else if (dpad && dpad == g_psDpadHeld && (LONG)(now - g_psDpadNext) >= 0) {
        bit = (dpad & XI_DPAD_UP) ? 1 : (dpad & XI_DPAD_DOWN) ? 2 : (dpad & XI_DPAD_LEFT) ? 4 : 8;
        g_psDpadNext = now + PS_DPAD_REPEAT;
    } else if (!dpad) g_psDpadHeld = 0;
    if (bit) {
        float dx, dy;
        PsDirVec(bit, &dx, &dy);
        g_psPendDir = 0;
        PsJump(h, x, y, dx, dy, bit);
        return TRUE;
    }
    /* a jump that waited for the list to scroll */
    if (g_psPendDir && g_psSeq != g_psPendSeq) PsPendingJump(h, x, y);

    float sx = g_lx, sy = -g_ly, mag = sqrtf(sx * sx + sy * sy);
    if (mag > 0.0f) {
        g_psGlide = 0;
        g_psLastStick = now;
        if (!g_psPushAt) {
            g_psPushAt = now;
            g_psPeak = 0;
            g_psStartX = x;
            g_psStartY = y;
            g_psPushes++;
            g_psCrossAt = 0;
            const PsTarget *from = PsNear(x, y, 1.0f);
            g_psFromX = from ? from->px : -1e6f;
            g_psFromY = from ? from->py : -1e6f;
        }
        if (mag > g_psPeak) { g_psPeak = mag; g_psPeakX = sx / mag; g_psPeakY = sy / mag; }
        float speed = CtlDvarFloat(g_gpCursor, 1100.0f);
        float vx = sx * mag * speed, vy = sy * mag * speed;
        const PsTarget *t = strength > 0 ? PsNear(x, y, 36.0f * scale * strength) : NULL;
        if (t) {
            /* friction: for fine aim (a partial push) over controls of button size; a full push and a large card
             * (a mode card, a list panel) keep their speed, so crossing them is not slowed */
            float inside = PsRectDist(t, x, y) <= 0.5f;
            float small = (t->w < 320.0f * scale && t->h < 200.0f * scale) ? 1.0f : 0.0f;
            float slow = 1.0f - (inside ? 0.55f : 0.3f) * strength * small * (1.0f - 0.6f * mag);
            vx *= slow;
            vy *= slow;
            /* pull toward the point of the control the cursor is over, across the direction of travel only and never
             * against the push (it keeps the cursor in the row it moves along); like the friction, for button-sized
             * controls and mostly at partial deflection (a fast pass is not bent) */
            float ux = sx / mag, uy = sy / mag, k = (inside ? 5.0f : 0.0f) * strength * small * (1.0f - 0.7f * mag);
            float gx = (t->px - x) * k, gy = (t->py - y) * k;
            float along = gx * ux + gy * uy;
            gx -= ux * along;
            gy -= uy * along;
            vx += gx;
            vy += gy;
        }
        float nx = x + vx * dt, ny = y + vy * dt;
        {   /* remember a control passed over (not the one the push left), for the release below */
            const PsTarget *o = PsNear(nx, ny, 1.0f);
            if (o && (fabsf(o->px - g_psFromX) > 1.0f || fabsf(o->py - g_psFromY) > 1.0f)) {
                g_psCrossX = o->px;
                g_psCrossY = o->py;
                g_psCrossAt = now;
            }
        }
        if (PsDebug() >= 2) PsLog("padsnap: stick %.2f,%.2f at %.0f,%.0f -> %.1f,%.1f (dt %.3f, near %s)\n", sx, sy, x, y, nx, ny, dt,
                                  t ? "yes" : "no");
        PsMoveTo(h, nx, ny);
        return TRUE;
    }

    /* stick at rest */
    if (g_psPushAt) {
        DWORD dur = now - g_psPushAt;
        g_psPushAt = 0;
        if (dur < PS_FLICK_MS && g_psPeak >= PS_FLICK_PEAK) {
            int db = fabsf(g_psPeakY) > fabsf(g_psPeakX) ? (g_psPeakY < 0 ? 1 : 2) : (g_psPeakX < 0 ? 4 : 8);
            g_psFlicks++;
            if (PsDebug()) PsLog("padsnap: flick %lu ms peak %.2f dir %.2f,%.2f\n", (unsigned long)dur, g_psPeak, g_psPeakX, g_psPeakY);
            /* snap the flick to the nearest axis when it is within 25 degrees of it (menus are grids) */
            float fx = g_psPeakX, fy = g_psPeakY;
            if (fabsf(fx) > 0.906f) { fx = fx < 0 ? -1.0f : 1.0f; fy = 0; }
            else if (fabsf(fy) > 0.906f) { fy = fy < 0 ? -1.0f : 1.0f; fx = 0; }
            PsJump(h, g_psStartX, g_psStartY, fx, fy, (fx == 0 || fy == 0) ? db : 0);
            return TRUE;
        }
        /* settle on the control under or next to the cursor; past every control (the push overshot one by the time
         * the thumb let go: a player reacts in about 0.2 s), back onto the one passed last */
        const PsTarget *t = PsNear(x, y, (8.0f + 40.0f * strength) * scale);
        PsTarget back;
        if (!t && strength > 0 && g_psCrossAt && now - g_psCrossAt < 120 + (DWORD)(260 * strength)) {
            float bx = g_psCrossX - x, by = g_psCrossY - y;
            if (sqrtf(bx * bx + by * by) < 360.0f * scale) {
                memset(&back, 0, sizeof(back));
                back.px = g_psCrossX;
                back.py = g_psCrossY;
                t = &back;
                if (PsDebug()) PsLog("padsnap: overshoot, back to the control passed %lu ms ago\n", (unsigned long)(now - g_psCrossAt));
            }
        }
        if (t && (fabsf(t->px - x) > 1.0f || fabsf(t->py - y) > 1.0f)) {
            g_psGlide = 1;
            g_psGx = t->px;
            g_psGy = t->py;
            g_psSettles++;
            if (PsDebug()) PsLog("padsnap: settle from %.0f,%.0f to %.0f,%.0f\n", x, y, t->px, t->py);
            g_psOwn = TRUE;
            g_psCx = x;
            g_psCy = y;
        }
    }
    if (g_psGlide && g_psOwn) {
        float k = 1.0f - expf(-dt / 0.045f);
        float nx = x + (g_psGx - x) * k, ny = y + (g_psGy - y) * k;
        if (fabsf(g_psGx - nx) < 1.0f && fabsf(g_psGy - ny) < 1.0f) { nx = g_psGx; ny = g_psGy; g_psGlide = 0; }
        PsMoveTo(h, nx, ny);
    }
    return TRUE;
}

/* ---- commands ---------------------------------------------------------------------------------------- */

static void __cdecl PsCmdDump(void)
{
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    float x = -1, y = -1;
    if (h) PsCursor(h, &x, &y);
    C1Printf("padsnap: %s, header \"%s\", %d targets, age %lu ms, cursor %.0f,%.0f, err \"%s\"\n",
             PsEnabled() ? "on" : "off", g_psHeader, g_psCount, (unsigned long)(GetTickCount() - g_psAt), x, y, PsStr(PsFind("k33_snaperr")));
    Log("padsnap: dump: %s, scan %d, header \"%s\", %d targets, age %lu ms, cursor %.0f,%.0f (the movie's mouse maps to %.0f,%.0f;"
        " scale %.3f origin %.0f,%.0f), err \"%s\"", PsEnabled() ? "on" : "off", C1DvarInt(g_psScan, -1), g_psHeader, g_psCount,
        (unsigned long)(GetTickCount() - g_psAt), x, y, g_psMouseX, g_psMouseY, g_psK, g_psOx, g_psOy, PsStr(PsFind("k33_snaperr")));
    char cls[8][900];
    int nc = 0;
    for (int c = 0; c < 8; c++) { lstrcpynA(cls[c], PsStr(g_psCls[c]), sizeof(cls[c])); if (cls[c][0]) nc = c + 1; }
    const char *cp = cls[0];
    int ci = 0;
    for (int i = 0; i < g_psCount; i++) {
        char name[96] = "";
        if (nc) {
            while (ci < nc && !*cp) { ci++; cp = ci < nc ? cls[ci] : ""; }
            int k = 0;
            while (*cp && *cp != ',' && k < (int)sizeof(name) - 1) name[k++] = *cp++;
            name[k] = 0;
            if (*cp == ',') cp++;
        }
        const PsTarget *t = &g_ps[i];
        C1Printf("  %3d %4.0f,%4.0f %4.0fx%-4.0f -> %4.0f,%4.0f k%d %s\n", i, t->x, t->y, t->w, t->h, t->px, t->py, t->kind, name);
        Log("padsnap: %3d %4.0f,%4.0f %4.0fx%-4.0f -> %4.0f,%4.0f k%d %s", i, t->x, t->y, t->w, t->h, t->px, t->py, t->kind, name);
    }
    {
        char at[1100];
        lstrcpynA(at, PsStr(PsFind("k33_snapat")), sizeof(at));
        for (char *tok = strtok(at, "/"); tok; tok = strtok(NULL, "/")) Log("padsnap: at: %s", tok);
    }
    {   /* what covers each control a click cannot reach, one per line */
        char hid[1024];
        for (int c = 0; c < 4; c++) {
            lstrcpynA(hid, PsStr(g_psHid[c]), sizeof(hid));
            for (char *tok = strtok(hid, ","); tok; tok = strtok(NULL, ",")) Log("padsnap: hidden: %s", tok);
        }
    }
    g_psDumpUntil = GetTickCount() + 1500;   /* the next lists carry the classes */
}

/* Test clients: "gpad_snapcount [reset]" prints the pad inputs since the last reset (measurements). */
static void __cdecl PsCmdCount(void)
{
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    float x = -1, y = -1;
    if (h) PsCursor(h, &x, &y);
    C1Printf("padsnap: pushes %d flicks %d dpad %d jumps %d settles %d cursor %.0f,%.0f lists %d\n", g_psPushes, g_psFlicks, g_psDpads,
             g_psJumps, g_psSettles, x, y, g_psSeqTotal);
    Log("padsnap: count pushes %d flicks %d dpad %d jumps %d settles %d cursor %.0f,%.0f targets %d", g_psPushes, g_psFlicks, g_psDpads,
        g_psJumps, g_psSettles, x, y, g_psCount);
    if (VoiceArgc() > 1 && !_stricmp(VoiceArgv(1), "reset")) g_psPushes = g_psFlicks = g_psDpads = g_psJumps = g_psSettles = 0;
}

/* Test clients: "gpad_snapfake <lx> <ly> <ms> [buttons]": the synthetic pad pushes the left stick (and holds the
 * buttons) for ms, then stays connected at rest, the way a real pad does after a push (gpad_fake ... <ms> unplugs it,
 * which skips what happens on release). "gpad_snapfake off" unplugs it. */
static void __cdecl PsCmdFake(void)
{
    if (!CtlTestMode()) { C1Printf("gpad_snapfake: test clients only\n"); return; }
    if (VoiceArgc() >= 2 && !_stricmp(VoiceArgv(1), "off")) { InterlockedExchange(&g_fakeOn, 0); g_psFakeRestAt = 0; return; }
    if (VoiceArgc() < 4) { C1Printf("usage: gpad_snapfake <lx> <ly> <ms> [buttons] | off\n"); return; }
    CtlPad p;
    memset(&p, 0, sizeof(p));
    p.sThumbLX = (SHORT)(fmaxf(-1.0f, fminf(1.0f, (float)atof(VoiceArgv(1)))) * 32767.0f);
    p.sThumbLY = (SHORT)(fmaxf(-1.0f, fminf(1.0f, (float)atof(VoiceArgv(2)))) * 32767.0f);
    p.wButtons = VoiceArgc() > 4 ? (WORD)strtoul(VoiceArgv(4), NULL, 0) : 0;
    int ms = atoi(VoiceArgv(3));
    g_fakeUntil = 0;
    g_fakePad = p;
    g_psFakeRestAt = GetTickCount() + (DWORD)(ms > 0 ? ms : 1);
    InterlockedExchange(&g_fakeOn, 1);
}


/* ---- measurement: a player model on the synthetic pad (test clients) -------------------------------------
 * "gpad_snapgoto <x> <y> <w> <h> [reaction ms]" moves the menu cursor onto the client rectangle the way a player
 * with the stick does: push toward its centre (full deflection, aimed once at the start of the push), and let go `reaction` ms
 * (default 180) after the cursor is over it; if it is not over it once the cursor has come to rest, push again at
 * 0.4 deflection, steered every 200 ms, up to 10 pushes. Logs "padsnap: goto ... pushes N, stick time T ms, total ms, inside". The same
 * model with gpad_menu_snap 0 and 1 measures what snapping saves. */
static int g_botOn, g_botPhase, g_botPushes;
static float g_botX, g_botY, g_botW, g_botH, g_botMag;
static DWORD g_botReact, g_botStart, g_botPushAt, g_botHitAt, g_botRestAt, g_botStick;
static float g_botLastX, g_botLastY, g_botAx, g_botAy;
static int g_botAimed;
static DWORD g_botAimAt;

static void PsBotStick(float lx, float ly)
{
    CtlPad p;
    memset(&p, 0, sizeof(p));
    p.sThumbLX = (SHORT)(fmaxf(-1.0f, fminf(1.0f, lx)) * 32767.0f);
    p.sThumbLY = (SHORT)(fmaxf(-1.0f, fminf(1.0f, ly)) * 32767.0f);
    g_fakeUntil = 0;
    g_fakePad = p;
    InterlockedExchange(&g_fakeOn, 1);
}

static void PsBotFrame(DWORD now)
{
    if (!g_botOn) return;
    HWND h = *(HWND *)(DWORD_PTR)CTL_MAIN_HWND_VA;
    float x, y;
    if (!h || !PsCursor(h, &x, &y)) return;
    BOOL over = x >= g_botX && x <= g_botX + g_botW && y >= g_botY && y <= g_botY + g_botH;
    if (g_botPhase == 1) {                      /* pushing */
        float dx = g_botX + g_botW / 2 - x, dy = g_botY + g_botH / 2 - y, d = sqrtf(dx * dx + dy * dy);
        if (over && !g_botHitAt) g_botHitAt = now;
        if ((g_botHitAt && now - g_botHitAt >= g_botReact) || now - g_botPushAt > 6000) {
            PsBotStick(0, 0);
            g_botStick += now - g_botPushAt;
            g_botPhase = 2;
            g_botRestAt = now;
            g_botLastX = x; g_botLastY = y;
            return;
        }
        /* the first, fast push is aimed once; a slow correction is steered (re-aimed every 200 ms) */
        if (d > 0.5f && (!g_botAimed || (g_botMag < 1.0f && now - g_botAimAt >= 200))) {
            g_botAx = dx / d; g_botAy = dy / d; g_botAimed = 1; g_botAimAt = now;
        }
        PsBotStick(g_botAx * g_botMag, -g_botAy * g_botMag);
        return;
    }
    /* released: wait until the cursor has been still for 300 ms (a settle glide ends) */
    if (fabsf(x - g_botLastX) > 0.5f || fabsf(y - g_botLastY) > 0.5f) { g_botLastX = x; g_botLastY = y; g_botRestAt = now; return; }
    if (now - g_botRestAt < 300) return;
    if (over || g_botPushes >= 10) {
        Log("padsnap: goto %.0f,%.0f %.0fx%.0f (snap %d, strength %.2f, speed %.0f): pushes %d, stick time %lu ms, total %lu ms, cursor %.0f,%.0f %s",
            g_botX, g_botY, g_botW, g_botH, C1DvarInt(g_psOn, 1), PsStrength(), CtlDvarFloat(g_gpCursor, 1100.0f), g_botPushes,
            (unsigned long)g_botStick, (unsigned long)(g_botRestAt - g_botStart), x, y, over ? "inside" : "MISSED");
        g_botOn = 0;
        return;
    }
    g_botPushes++;
    g_botMag = 0.4f;                            /* a correction: a gentler push */
    g_botAimed = 0;
    g_botHitAt = 0;
    g_botPushAt = now;
    g_botPhase = 1;
}

static void __cdecl PsCmdGoto(void)
{
    if (!CtlTestMode() || VoiceArgc() < 5) { C1Printf("usage (test clients): gpad_snapgoto <x> <y> <w> <h> [reaction ms]\n"); return; }
    g_botX = (float)atof(VoiceArgv(1)); g_botY = (float)atof(VoiceArgv(2));
    g_botW = (float)atof(VoiceArgv(3)); g_botH = (float)atof(VoiceArgv(4));
    g_botReact = VoiceArgc() > 5 ? (DWORD)atoi(VoiceArgv(5)) : 180;
    g_botStart = g_botPushAt = GetTickCount();
    g_botHitAt = 0; g_botStick = 0; g_botPushes = 1; g_botMag = 1.0f; g_botPhase = 1; g_botAimed = 0; g_botOn = 1;
}

static C1Cmd g_psCmds[] = {
    { NULL, "gpad_snapdump", (void *)PsCmdDump },
    { NULL, "gpad_snapcount", (void *)PsCmdCount },
};
static C1Cmd g_psTestCmds[] = {
    { NULL, "gpad_snapfake", (void *)PsCmdFake },
    { NULL, "gpad_snapgoto", (void *)PsCmdGoto },
};

static void PadSnapInstall(void)
{
    int added = 0;
#if K33_DIAGNOSTICS
    for (unsigned i = 0; i < sizeof(g_psCmds) / sizeof(g_psCmds[0]); i++) added += C1AddCommand(&g_psCmds[i]);
    if (CtlTestCmdsEnabled())
        for (unsigned i = 0; i < sizeof(g_psTestCmds) / sizeof(g_psTestCmds[0]); i++) added += C1AddCommand(&g_psTestCmds[i]);
#endif
    Log("padsnap: menu cursor snapping installed (%d commands)", added);
}

/*
 * controller_glyphs.c - gamepad button icons inside C1's text (feat/controller phase 3). Included by
 * controller.c after CtlApplyPadNames.
 *
 * C1's text renderer still has IW4's inline icon escape: '^', 1 or 2, width, height, then a raw 4-byte
 * Material pointer (0xAAB26E / 0xAAB8B2 step over 7 bytes after the '^'; 0xAAA7D0 draws the icon at
 * ((size - 16) * fontHeight + 16) / 32 pixels, so 48 is one line high). C1's zones have no button
 * materials; ours come from a patch zone:
 *
 *   zone       client_shipRetail\patch\common_postgfx_mp.ff (zones/controller_glyphs_postgfx_mp, built into that
 *              consolidated zone by build_zones.py; an install with only the old controller_glyphs_postgfx_mp.ff
 *              still works: that name is looked up when the new one is not there). A *_postgfx_mp patch zone is
 *              appended to the code_post_gfx list (kernel33.c), so it
 *              loads once the renderer is up and stays loaded in the front end and in matches. It holds, for each
 *              set (xbox360, ps3, wiiu, generic) and glyph (the 17 kCtlButtons, then "arrow", the killstreak HUD's
 *              yellow selection arrow), an image k33pad_<set>_<glyph> (64x64 A8R8G8B8 with mips, Xelu's CC0
 *              prompts) and a material of the same name (C1's hud_status_dead layout: technique set 2d, alpha
 *              blended, one colorMap).
 *   lookup     Material_RegisterHandle (0xA78170) by name, on the main thread, once the zone has been submitted
 *              and the front end is up; every material must come back under its own name with its image's
 *              texture made, else the icons stay off.
 *   slots      the escape carries the material pointer as four raw bytes inside a C string, so it must hold no 0,
 *              '^', '%' or '&': a zone pointer can hold any of them. Each glyph therefore has a slot at an address
 *              whose four bytes are letters or digits (base 0xHHHH0000, + (0x41 + glyph) * 0x100 + 0x44)
 *              holding a copy of the set's zone material (its 0x60-byte header: it points at the zone's own
 *              technique set, texture table and image). Hints carry the slot; the set in use (gpad_glyphs 1..4, else
 *              the pad's SDL type: controller_glyph_policy.h) is copied into the slot on the render thread, where C1 draws the icon (the calls
 *              0xAAB320 / 0xAAB92D to 0xAAA7D0 come here first): a set change is live.
 *   names      Key_GetBindingNames 0x493920 (every "[{+attack}]" in hints, prompts and old-style menus)
 *              converts a key with Key_KeynumToString at 0x49397C and 0x4A19A1. Those calls come here: a pad
 *              key gives its icon escape. Scaleform (0x5B080D, AS key-binding names: Controls screen, pause
 *              menu) and GSC (0x55A8B6) keep the family's text names (Scaleform has no IW4 icon escape).
 *
 * Without the zone (or with a material missing) the icons stay off and every hint keeps the family's text names;
 * the reason is logged once and printed by gpad_glyphdump.
 */

#define CTL_GLYPH_SETS         4
#define CTL_GLYPH_ESC_SIZE     56            /* 48 = one text line high; a little more reads better */
#define CTL_GLYPH_ZONE         "common_postgfx_mp"
#define CTL_GLYPH_ZONE_OLD     "controller_glyphs_postgfx_mp"   /* before the zones were consolidated */
static const char *g_glyphZone = CTL_GLYPH_ZONE;
#define CTL_MATERIAL_REGISTER  0x00A78170u   /* Material_RegisterHandle(name, imageTrack) */
#define CTL_KEYNUM_TO_STRING   0x00492AF0u   /* Key_KeynumToString(eax = key, edi = translate) */
#define CTL_BINDING_NAMES      0x00493920u   /* Key_GetBindingNames(eax = buffer, ecx = lc; command) */
#define CTL_ICON_DRAW          0x00AAA7D0u   /* text icon drawer (ecx = escape after '^', eax = font) */
/* IW4 Material layout (the zone's material header is 0x60 bytes), checked at run time */
#define CTL_MAT_SIZE           0x60
#define CTL_MAT_TEXCOUNT       0x48
#define CTL_MAT_TEXTABLE       0x54
#define CTL_TEXDEF_IMAGE       8

#define CTL_GLYPH_COUNT (CTL_NBUTTONS + 1)   /* the buttons, then the yellow selection arrow */
#define CTL_GLYPH_ARROW CTL_NBUTTONS
static const char *const kGlyphSets[CTL_GLYPH_SETS] = { "xbox360", "ps3", "wiiu", "generic" };
static const char *const kGlyphNames[CTL_GLYPH_COUNT] = {
    "a", "b", "x", "y", "lb", "rb", "start", "back", "ls", "rs", "dpad_up", "dpad_down", "dpad_left", "dpad_right",
    "lt", "rt", "x_hold", "arrow" };

typedef struct {
    BYTE *mat;                       /* the slot (at its letter address) the escape points at */
    const BYTE *src[CTL_GLYPH_SETS]; /* the zone's material of each set */
    LONG shown;                      /* the set copied into the slot */
    char esc[9];                     /* '^', 1, w, h, Material * (4 bytes), 0 */
} CtlGlyph;

static CtlGlyph g_glyphs[CTL_GLYPH_COUNT];
static BYTE *g_glyphBase;            /* the 64 KB block that holds the slots */
static volatile LONG g_glyphReady;   /* 1 icons usable, -1 off for good */
static volatile LONG g_glyphSet;     /* the set the render thread uses; -1 text names */
__attribute__((used)) int g_ctlTextOnly;           /* inside the Scaleform / GSC callers of Key_GetBindingNames */
static int g_glyphTries;
static DWORD g_glyphNextTry, g_glyphFrontEndAt;
static char g_glyphWhy[200] = "not tried yet";

/* The image of a zone material's only texture def, once its texture has been made. */
static const BYTE *CtlGlyphImage(const BYTE *mat)
{
    if (!mat || !Readable(mat, CTL_MAT_SIZE) || *(const BYTE *)(mat + CTL_MAT_TEXCOUNT) != 1) return NULL;
    const BYTE *table = *(const BYTE *const *)(mat + CTL_MAT_TEXTABLE);
    if (!table || !Readable(table, 12)) return NULL;
    const BYTE *img = *(const BYTE *const *)(table + CTL_TEXDEF_IMAGE);
    return img && Readable(img, 0x24) && *(void *const *)img ? img : NULL;
}

static void CtlGlyphOff(const char *why)
{
    if (why != g_glyphWhy) lstrcpynA(g_glyphWhy, why, sizeof(g_glyphWhy));
    Log("controller: button icons off, hints keep the pad's text names: %s", g_glyphWhy);
    InterlockedExchange(&g_glyphReady, -1);
}

/* Main thread: look the 72 materials up and set up the slots. Returns FALSE to be tried again later. */
static BOOL CtlGlyphInit(void)
{
    typedef const BYTE *(__cdecl *RegFn)(const char *, int);
    static const BYTE kReg[] = { 0x8B, 0x44, 0x24, 0x04, 0x80, 0x38, 0x00 };
    if (!Readable((void *)(DWORD_PTR)CTL_MATERIAL_REGISTER, sizeof(kReg)) ||
        memcmp((void *)(DWORD_PTR)CTL_MATERIAL_REGISTER, kReg, sizeof(kReg))) {
        CtlGlyphOff("Material_RegisterHandle bytes differ");
        return TRUE;
    }
    for (int s = 0; s < CTL_GLYPH_SETS; s++)
        for (int b = 0; b < CTL_GLYPH_COUNT; b++) {
            char name[64], why[200];
            _snprintf(name, sizeof(name), "k33pad_%s_%s", kGlyphSets[s], kGlyphNames[b]);
            name[sizeof(name) - 1] = 0;
            const BYTE *mat = ((RegFn)(DWORD_PTR)CTL_MATERIAL_REGISTER)(name, 7);
            const char *got = mat && Readable(mat, CTL_MAT_SIZE) ? *(const char *const *)mat : NULL;
            if (!got || !Readable(got, 1) || strcmp(got, name)) {
                _snprintf(why, sizeof(why), "material %s not found (%s.ff did not load it)", name, g_glyphZone);
                why[sizeof(why) - 1] = 0;
                CtlGlyphOff(why);
                return TRUE;
            }
            if (!CtlGlyphImage(mat)) {
                _snprintf(g_glyphWhy, sizeof(g_glyphWhy), "material %s: no image texture yet", name);
                return FALSE;
            }
            g_glyphs[b].src[s] = mat;
        }
    if (!g_glyphBase) {
        /* a free 64 KB block whose two high address bytes are letters or digits (0xHHHH0000): letters first, from
         * 'K' up, as before; C1's zone memory takes some of these, so every combination is tried */
        static const char kSafe[] = "KLMNOPQRSTUVWXYZJIHGFEDCBAabcdefghijklmnopqrstuvwxyz0123456789";
        for (const char *h = kSafe; *h && !g_glyphBase; h++)
            for (const char *l = kSafe; *l && !g_glyphBase; l++) {
                DWORD at = ((DWORD)(BYTE)*h << 24) | ((DWORD)(BYTE)*l << 16);
                g_glyphBase = (BYTE *)VirtualAlloc((void *)(DWORD_PTR)at, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            }
        if (!g_glyphBase) {
            CtlGlyphOff("no free 64 KB block at a letter address");
            return TRUE;
        }
    }
    for (int b = 0; b < CTL_GLYPH_COUNT; b++) {
        CtlGlyph *g = &g_glyphs[b];
        g->mat = g_glyphBase + ((0x41 + b) << 8) + 0x44;
        memcpy(g->mat, g->src[0], CTL_MAT_SIZE);
        g->shown = 0;
        DWORD p = (DWORD)(DWORD_PTR)g->mat;
        g->esc[0] = '^';
        g->esc[1] = 1;
        g->esc[2] = CTL_GLYPH_ESC_SIZE;
        g->esc[3] = CTL_GLYPH_ESC_SIZE;
        memcpy(g->esc + 4, &p, 4);
        g->esc[8] = 0;
    }
    _snprintf(g_glyphWhy, sizeof(g_glyphWhy), "ready: %d sets x %d icons from %s.ff, slots at %p", CTL_GLYPH_SETS,
              CTL_GLYPH_COUNT, g_glyphZone, (void *)g_glyphBase);
    Log("controller: glyphs %s", g_glyphWhy);
    InterlockedExchange(&g_glyphReady, 1);
    return TRUE;
}

/* The set the render thread draws (-1 = text names). */
static void CtlGlyphFrame(void)
{
    if (g_glyphReady == 0 && g_ctlDvarsDone && C1DvarsReady()) {
        DWORD now = GetTickCount();
        int zone = PatchZoneState(CTL_GLYPH_ZONE);
        if (zone < 0 && (zone = PatchZoneState(CTL_GLYPH_ZONE_OLD)) >= 0) g_glyphZone = CTL_GLYPH_ZONE_OLD;
        if (zone < 0) {
            CtlGlyphOff(CTL_GLYPH_ZONE ".ff (or " CTL_GLYPH_ZONE_OLD ".ff) is not in the patch folder (or the folder is off)");
        } else if (zone == 0) {
            /* queued, not yet handed to the loader */
        } else if (!g_glyphFrontEndAt) {
            g_glyphFrontEndAt = now + 2000;   /* the startup zones are loaded by the time the front end runs */
        } else if ((LONG)(now - g_glyphFrontEndAt) >= 0 && (LONG)(now - g_glyphNextTry) >= 0) {
            g_glyphNextTry = now + 1000;
            if (!CtlGlyphInit() && ++g_glyphTries >= 30) CtlGlyphOff(g_glyphWhy);
        }
    }
    CtlGlyphDisplay disp = CtlDisplayNow();
    LONG set = disp.text_only || g_glyphReady != 1 ? -1 : disp.family;
    if (set != g_glyphSet) {
        InterlockedExchange(&g_glyphSet, set);
        static const char *const kSet[] = { "Xbox 360", "PS3", "Wii U", "generic" };
        Log("controller: button icons: %s", set < 0 ? "off (text names)" : kSet[set]);
    }
}

/* Key_KeynumToString in Key_GetBindingNames: the icon escape for a pad key, or NULL (C1's own name). */
__attribute__((used)) const char *__cdecl CtlGlyphName(int key)
{
    if (key == K_KS_SELECTED) {   /* the killstreak HUD's selected slot: arrow + the use key */
        static char sel[96];
        int b = 0;
        while (b < CTL_NBUTTONS && kCtlButtons[b].key != g_ksUseKey) b++;
        if (b == CTL_NBUTTONS) return ">";
        if (g_ctlTextOnly || g_glyphSet < 0 || g_glyphReady != 1) {
            const char *name = g_locBase >= 0 ? g_ctlKeyNamesLoc[g_locBase + b].name : kCtlButtons[b].display;
            _snprintf(sel, sizeof(sel), "> %s", name ? name : "");
        } else
            _snprintf(sel, sizeof(sel), "%s%s", g_glyphs[CTL_GLYPH_ARROW].esc, g_glyphs[b].esc);
        sel[sizeof(sel) - 1] = 0;
        return sel;
    }
    if (g_ctlTextOnly || g_glyphSet < 0 || g_glyphReady != 1) return NULL;
    for (int b = 0; b < CTL_NBUTTONS; b++)
        if (kCtlButtons[b].key == key) return g_glyphs[b].esc;
    return NULL;
}

/* ---- render thread ---- */
/* Returns 1 for one of our icons, 0 for anything else. For ours the hook sets the vertex colour to white
 * (alpha kept): C1 multiplies text icons by the text colour, and hint key names are yellow, so white is
 * what leaves the art's own colours (green A, red B, ...) untouched. */
/* Placement: C1 lays out text in runs (0x6B7510; measured by 0x6B7350) and hands the icon drawer the pen x
 * of the icon's slot (args[0]). The slot it then leaves for the icon is wider than the drawn icon, so the
 * icon sat at the slot's left edge with all the gap on its right (most visible at small HUD scales). It is
 * moved right by gpad_glyph_xshift (fraction of its width, default 0.08, measured on devtest) to sit centred.
 * args: [0] x, [4] x scale (C1 multiplies it by the icon width). */
static void CtlGlyphPlace(const BYTE *esc, const BYTE *font, float *args)
{
    if (!font || !Readable(font, 8) || !args) return;
    int fontH = *(const int *)(font + 4);
    int w = ((signed char)esc[1] - 16) * fontH + 16;
    w = w >= 0 ? w >> 5 : -((-w) >> 5);
    float k = g_gpGlyphShift ? *(volatile const float *)(g_gpGlyphShift + 0xC) : 0.08f;
    if (w > 0 && k != 0.0f && args[4] > 0.0f && args[4] < 100.0f) args[0] += k * (float)w * args[4];
}

__attribute__((used)) int __cdecl CtlGlyphPrepare(const BYTE *esc, const BYTE *font, float *args)
{
    if (!g_glyphBase || g_glyphReady != 1) return 0;
    const BYTE *mat = *(const BYTE *const *)(esc + 3);
    if (mat < g_glyphBase || mat >= g_glyphBase + 0x10000) return 0;
    int b = (int)(((DWORD_PTR)(mat - g_glyphBase) >> 8) - 0x41);
    LONG set = g_glyphSet;
    if (b < 0 || b >= CTL_GLYPH_COUNT || g_glyphs[b].mat != mat) return 0;
    if (set < 0) set = CtlPadFamily();   /* a string made before a switch to text names */
    CtlGlyph *g = &g_glyphs[b];
    if (g->shown != set) {
        memcpy(g->mat, g->src[set], CTL_MAT_SIZE);
        g->shown = set;
    }
    CtlGlyphPlace(esc, font, args);
    return 1;
}

/* ---- text width ----
 * R_TextWidth 0xA78580 (text, maxChars, font), which C1 uses to centre and right-align text (cursor hints,
 * menu items, the HUD), skips "^digit" colour codes but not the inline icon code: it measured an icon as
 * the eight characters '^', 1, w, h and four pointer letters. Centred or right-aligned text with an icon was
 * therefore measured far too wide and started far to the left of where it belongs. The hook measures the
 * text without the icon codes and adds each icon's slot: the drawn width ((w - 16) * fontHeight + 16) / 32
 * plus the room C1's layout leaves around it (2 * gpad_glyph_xshift of the width). */
__attribute__((used)) BYTE g_ctlTextWidthTramp[16];
static int CtlIconSlot(const BYTE *esc, const BYTE *font)
{
    int fontH = font && Readable(font, 8) ? *(const int *)(font + 4) : 0;
    int w = ((signed char)esc[1] - 16) * fontH + 16;
    w = w >= 0 ? w >> 5 : -((-w) >> 5);
    float k = g_gpGlyphShift ? *(volatile const float *)(g_gpGlyphShift + 0xC) : 0.08f;
    return w > 0 ? (int)(w * (1.0f + 2.0f * k) + 0.5f) : 0;
}

__attribute__((used)) int __cdecl CtlTextWidth(const char *text, int maxChars, const BYTE *font)
{
    typedef int(__cdecl * Fn)(const char *, int, const BYTE *);
    Fn orig = (Fn)(void *)g_ctlTextWidthTramp;
    const char *icon = NULL;
    if (text) {
        for (const char *c = text; *c; c++)
            if (c[0] == '^' && (c[1] == 1 || c[1] == 2) && c[2] && c[3] && c[4] && c[5] && c[6] && c[7]) { icon = c; break; }
    }
    if (!icon) return orig(text, maxChars, font);
    char buf[1024];
    int n = 0, extra = 0;
    for (const char *c = text; *c && n < (int)sizeof(buf) - 1;) {
        if (c[0] == '^' && (c[1] == 1 || c[1] == 2) && c[2] && c[3] && c[4] && c[5] && c[6] && c[7]) {
            extra += CtlIconSlot((const BYTE *)c + 1, font);
            c += 8;
            continue;
        }
        buf[n++] = *c++;
    }
    buf[n] = 0;
    return (n ? orig(buf, maxChars, font) : 0) + extra;
}

void CTL_TextWidthHook(void);
__asm__(
    ".text\n"
    ".globl _CTL_TextWidthHook\n"
    "_CTL_TextWidthHook:\n"           /* jmp from R_TextWidth 0xA78580: [esp+4] text, maxChars, font */
    "    jmp _CtlTextWidth\n");

static void CtlInstallTextWidth(void)
{
    static const BYTE kTw[] = { 0x83, 0xEC, 0x10, 0x83, 0x7C, 0x24, 0x18, 0x00 };   /* sub esp,10h; cmp [esp+18h],0 */
    BYTE *tw = (BYTE *)(DWORD_PTR)0x00A78580u;
    if (!Readable(tw, sizeof(kTw)) || memcmp(tw, kTw, sizeof(kTw))) { Log("controller: R_TextWidth bytes differ, icon text keeps C1's width"); return; }
    DWORD old;
    memcpy(g_ctlTextWidthTramp, kTw, sizeof(kTw));               /* the displaced instructions ... */
    g_ctlTextWidthTramp[8] = 0xE9;                                 /* ... then jmp 0xA78588 */
    LONG back = (LONG)(0x00A78588u - ((DWORD_PTR)g_ctlTextWidthTramp + 13));
    memcpy(g_ctlTextWidthTramp + 9, &back, 4);
    VirtualProtect(g_ctlTextWidthTramp, sizeof(g_ctlTextWidthTramp), PAGE_EXECUTE_READWRITE, &old);
    BYTE j[8] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
    LONG rel = (LONG)((DWORD_PTR)CTL_TextWidthHook - (0x00A78580u + 5));
    memcpy(j + 1, &rel, 4);
    WriteCode(tw, j, sizeof(j));
    Log("controller: R_TextWidth hooked (0xA78580): icon codes measured as icons");
}

/* ---- hooks ---- */
__attribute__((used)) DWORD g_ctlKeyToString = CTL_KEYNUM_TO_STRING;
__attribute__((used)) DWORD g_ctlBindingNames = CTL_BINDING_NAMES;
__attribute__((used)) DWORD g_ctlIconDraw = CTL_ICON_DRAW;
void CTL_KeyNameHook(void);
void CTL_BindNamesTextHook(void);
void CTL_IconDrawHook(void);
__asm__(
    ".text\n"
    ".globl _CTL_KeyNameHook\n"
    "_CTL_KeyNameHook:\n"             /* call to Key_KeynumToString: eax = key, edi = translate */
    "    pushl %edx\n"
    "    pushl %ecx\n"
    "    pushl %eax\n"
    "    call _CtlGlyphName\n"
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    popl %eax\n"
    "    popl %ecx\n"
    "    popl %edx\n"
    "    jmp *_g_ctlKeyToString\n"
    "1:  addl $4, %esp\n"
    "    popl %ecx\n"
    "    popl %edx\n"
    "    ret\n"
    ".globl _CTL_BindNamesTextHook\n"
    "_CTL_BindNamesTextHook:\n"       /* call to Key_GetBindingNames from Scaleform / GSC: text names */
    "    incl _g_ctlTextOnly\n"
    "    pushl 4(%esp)\n"
    "    call *_g_ctlBindingNames\n"
    "    addl $4, %esp\n"
    "    decl _g_ctlTextOnly\n"
    "    ret\n"
    ".globl _CTL_IconDrawHook\n"
    "_CTL_IconDrawHook:\n"            /* call to the text icon drawer: ecx = escape after '^', [esp+1Ch] = colour */
    "    pushal\n"
    "    leal 36(%esp), %edx\n"          /* the drawer's stack arguments */
    "    pushl %edx\n"
    "    pushl %eax\n"                   /* font */
    "    pushl %ecx\n"
    "    call _CtlGlyphPrepare\n"
    "    addl $12, %esp\n"
    "    testl %eax, %eax\n"
    "    jz 1f\n"
    "    movl 0x3C(%esp), %eax\n"        /* packed RGBA; the shadow pass (black) stays black */
    "    testl $0x00FFFFFF, %eax\n"
    "    jz 1f\n"
    "    orl $0x00FFFFFF, %eax\n"
    "    movl %eax, 0x3C(%esp)\n"
    "1:  popal\n"
    "    jmp *_g_ctlIconDraw\n");

static void CtlInstallGlyphs(BOOL namesOk)
{
    static const DWORD kNameCalls[] = { 0x0049397Cu, 0x004A19A1u };
    static const DWORD kTextCalls[] = { 0x005B080Du, 0x0055A8B6u };
    static const DWORD kDrawCalls[] = { 0x00AAB320u, 0x00AAB92Du };
    BOOL ok = namesOk;
    for (int i = 0; i < 2; i++) {
        ok = ok && CallsTarget(kNameCalls[i], CTL_KEYNUM_TO_STRING) && *(const BYTE *)(DWORD_PTR)(kNameCalls[i] - 5) == 0xBF &&
             *(const DWORD *)(DWORD_PTR)(kNameCalls[i] - 4) == 1;   /* mov edi, 1 */
        ok = ok && CallsTarget(kTextCalls[i], CTL_BINDING_NAMES) && CallsTarget(kDrawCalls[i], CTL_ICON_DRAW);
    }
    if (!ok) {
        lstrcpynA(g_glyphWhy, "call sites differ (0x49397C / 0x4A19A1 / 0x5B080D / 0x55A8B6 / 0xAAB320 / 0xAAB92D)", sizeof(g_glyphWhy));
        g_glyphReady = -1;
        Log("controller: glyphs off: %s", g_glyphWhy);
        return;
    }
    for (int i = 0; i < 2; i++) {
        RedirectCall(kTextCalls[i], (void *)CTL_BindNamesTextHook);
        RedirectCall(kDrawCalls[i], (void *)CTL_IconDrawHook);
        RedirectCall(kNameCalls[i], (void *)CTL_KeyNameHook);
    }
    CtlInstallTextWidth();
    Log("controller: glyph hooks installed (names 0x49397C/0x4A19A1, text-only 0x5B080D/0x55A8B6, draw 0xAAB320/0xAAB92D)");
}

static void __cdecl CtlCmdGlyphDump(void)
{
    static const char *const kSet[] = { "Xbox 360", "PS3", "Wii U", "generic" };
    C1Printf("glyphs: %s; set %s (gpad_glyphs %d, pad family %d); hints name %s\n", g_glyphWhy,
             g_glyphSet < 0 ? "text names" : kSet[g_glyphSet], C1DvarInt(g_gpGlyphs, 0), (int)g_padType,
             CtlDisplayPad() ? "pad buttons" : "keyboard / mouse keys");
    if (g_glyphReady != 1) return;
    int s = g_glyphSet < 0 ? CtlPadFamily() : (int)g_glyphSet;
    for (int b = 0; b < CTL_GLYPH_COUNT; b++) {
        const BYTE *mat = g_glyphs[b].src[s], *img = CtlGlyphImage(mat);
        char line[200];
        _snprintf(line, sizeof(line), "k33pad_%s_%s: material %p, image %p, texture %p, slot %p (shows %ld)", kGlyphSets[s],
                  kGlyphNames[b], (const void *)mat, (const void *)img, img ? *(void *const *)img : NULL,
                  (void *)g_glyphs[b].mat, (long)g_glyphs[b].shown);
        line[sizeof(line) - 1] = 0;
        C1Printf("%s\n", line);
        Log("controller: glyphdump %s", line);
    }
}

/* gpad_glyphtest: a test row of every button's icon, added next to a text C1 already draws each frame
 * (R_AddCmdDrawText 0xA7EF80 is watched while the test is on; "gpad_glyphtest <caller>" picks the call
 * site, default the first one seen). Tests only. */
#define CTL_ADDCMD_TEXT 0x00A7EF80u
__attribute__((used)) DWORD g_ctlAddTextResume = CTL_ADDCMD_TEXT + 5;
static volatile LONG g_glyphOverlay;
static DWORD g_overlayCaller, g_seenCallers[32];
static int g_seenCount, g_inOverlay;
void CTL_AddTextHook(void);
__asm__(
    ".text\n"
    ".globl _CTL_AddTextHook\n"
    "_CTL_AddTextHook:\n"             /* jmp from R_AddCmdDrawText: [esp] = return, [esp+4] = text ... */
    "    pushal\n"
    "    leal 36(%esp), %eax\n"
    "    pushl %eax\n"
    "    pushl 36(%esp)\n"
    "    call _CtlAddTextSeen\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    movl 4(%esp), %eax\n"         /* the displaced mov eax, [esp+4] ; push ebx */
    "    pushl %ebx\n"
    "    jmp *_g_ctlAddTextResume\n");

typedef void(__cdecl *CtlAddTextFn)(const char *, int, void *, float, float, float, float, float, const float *, int);
__attribute__((used)) void __cdecl CtlAddTextSeen(DWORD ret, const DWORD *args)
{
    if (!g_glyphOverlay || g_inOverlay) return;
    if (ret >= 0x80000000u) {   /* a frame-stage call (see CtlCmdGlyphTest): no font from C1, register one */
        static void *font;
        if (!font) font = ((void *(__cdecl *)(const char *, int))(DWORD_PTR)0x00A78540u)("fonts/normalfont", 0);
        static DWORD args2[10];
        float one = 0.6f;
        args2[2] = (DWORD)(DWORD_PTR)font;
        memcpy(&args2[5], &one, 4);
        args = args2;
    }
    int known = 0;
    for (int i = 0; i < g_seenCount; i++) known |= g_seenCallers[i] == ret;
    if (!known && g_seenCount < 32) {
        g_seenCallers[g_seenCount++] = ret;
        Log("controller: glyphtest: R_AddCmdDrawText caller %08lX font %p scale %.2f \"%.40s\"", (unsigned long)ret,
            (void *)(DWORD_PTR)args[2], *(const float *)&args[5], Readable((const char *)(DWORD_PTR)args[0], 1) ? (const char *)(DWORD_PTR)args[0] : "");
    }
    if (!g_overlayCaller) g_overlayCaller = ret;
    if (ret != g_overlayCaller || g_glyphReady != 1) return;
    char line[600];
    int n = 0;
    for (int b = 0; b < CTL_NBUTTONS && n < (int)sizeof(line) - 40; b++)
        n += _snprintf(line + n, sizeof(line) - n, "%s%s", b ? " " : "", g_glyphs[b].esc);
    static const float white[4] = { 1, 1, 1, 1 };
    float sc = *(const float *)&args[5];
    if (sc < 0.3f) sc = 0.3f;
    g_inOverlay = 1;
    ((CtlAddTextFn)(DWORD_PTR)CTL_ADDCMD_TEXT)(line, 0x7FFFFFFF, (void *)(DWORD_PTR)args[2], 16.0f, 62.0f, sc * 1.25f, sc * 1.25f, 0.0f, white, 0);
    _snprintf(line, sizeof(line), "Press %s to reload, hold %s to use, %s to aim", g_glyphs[2].esc, g_glyphs[16].esc, g_glyphs[14].esc);
    ((CtlAddTextFn)(DWORD_PTR)CTL_ADDCMD_TEXT)(line, 0x7FFFFFFF, (void *)(DWORD_PTR)args[2], 16.0f, 128.0f, sc * 1.25f, sc * 1.25f, 0.0f, white, 0);
    /* placement rows: the icon between bars after a long run, at small, HUD-like and large scales */
    {
        /* "按住拾取" in UTF-8 and in GBK: C1's run measure gives every non-ASCII character one default width */
        static const char *const kRun[5] = { "", "WWWWWW", "Press to", "\xe6\x8c\x89\xe4\xbd\x8f\xe6\x8b\xbe\xe5\x8f\x96",
                                             "\xb0\xb4\xd7\xa1\xca\xb0\xc8\xa1" };
        static const float kScale[2] = { 0.35f, 0.6f };
        float y = 170.0f;
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < 5; i++) {
                _snprintf(line, sizeof(line), "%s|%s|x", kRun[i], g_glyphs[0].esc);
                ((CtlAddTextFn)(DWORD_PTR)CTL_ADDCMD_TEXT)(line, 0x7FFFFFFF, (void *)(DWORD_PTR)args[2], 16.0f, y, kScale[s], kScale[s], 0.0f, white, 0);
                y += 60.0f * kScale[s] + 6.0f;
            }
        /* measured width (R_TextWidth 0xA78580, what centred / right-aligned text uses) against the drawn text:
         * a red bar at x + width * scale; it should touch the end of the line */
        static const float red[4] = { 1, 0.2f, 0.2f, 1 };
        static int logged;
        if (!logged) {
            typedef int(__cdecl * TwFn)(const char *, int, void *);
            TwFn tw = (TwFn)(DWORD_PTR)0x00A78580u;
            void *f = (void *)(DWORD_PTR)args[2];
            char one[16], two[40];
            _snprintf(one, sizeof(one), "%s", g_glyphs[2].esc);
            _snprintf(two, sizeof(two), "%s%s", g_glyphs[2].esc, g_glyphs[2].esc);
            logged = 1;
            Log("controller: glyphtest widths: fontH %d, icon %d, two icons %d, \"X\" %d, \"WW\" %d, \"Press  to\" %d",
                Readable(f, 8) ? *(const int *)((const BYTE *)f + 4) : -1, tw(one, 0x7FFFFFFF, f), tw(two, 0x7FFFFFFF, f),
                tw("X", 0x7FFFFFFF, f), tw("WW", 0x7FFFFFFF, f), tw("Press  to", 0x7FFFFFFF, f));
        }
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < 3; i++) {
                if (i == 0) _snprintf(line, sizeof(line), "Press X to pick up");
                else if (i == 1) _snprintf(line, sizeof(line), "Press %s to pick up", g_glyphs[2].esc);
                else _snprintf(line, sizeof(line), "\xb0\xb4\xd7\xa1 %s \xca\xb0\xc8\xa1", g_glyphs[2].esc);
                int w = ((int(__cdecl *)(const char *, int, void *))(DWORD_PTR)0x00A78580u)(line, 0x7FFFFFFF, (void *)(DWORD_PTR)args[2]);
                ((CtlAddTextFn)(DWORD_PTR)CTL_ADDCMD_TEXT)(line, 0x7FFFFFFF, (void *)(DWORD_PTR)args[2], 16.0f, y, kScale[s], kScale[s], 0.0f, white, 0);
                ((CtlAddTextFn)(DWORD_PTR)CTL_ADDCMD_TEXT)("|", 0x7FFFFFFF, (void *)(DWORD_PTR)args[2], 16.0f + w * kScale[s], y, kScale[s], kScale[s], 0.0f, red, 0);
                y += 60.0f * kScale[s] + 6.0f;
            }
    }
    g_inOverlay = 0;
}

/* frame-stage test calls: the Scaleform calls 0x4A07E9 / 0x4A0810 in the screen update, then the test row */
__attribute__((used)) DWORD g_ctlStageA = 0x00A7E5F0u, g_ctlStageB = 0x00A90D30u;
void CTL_StageAHook(void);
void CTL_StageBHook(void);
__asm__(
    ".text\n"
    ".globl _CTL_StageAHook\n"
    "_CTL_StageAHook:\n"
    "    pushal\n"
    "    pushl $0\n"
    "    pushl $0x80000001\n"
    "    call _CtlAddTextSeen\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    jmp *_g_ctlStageA\n"
    ".globl _CTL_StageBHook\n"
    "_CTL_StageBHook:\n"
    "    pushl 4(%esp)\n"
    "    call *_g_ctlStageB\n"
    "    addl $4, %esp\n"
    "    pushal\n"
    "    pushl $0\n"
    "    pushl $0x80000002\n"
    "    call _CtlAddTextSeen\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    ret\n");

static void __cdecl CtlCmdGlyphTest(void)
{
    static BOOL hooked;
    if (g_glyphReady != 1) { C1Printf("glyphs: %s\n", g_glyphWhy); return; }
    if (!hooked) {
        static const BYTE kText[] = { 0x8B, 0x44, 0x24, 0x04, 0x53, 0x33, 0xDB };
        if (memcmp((void *)(DWORD_PTR)CTL_ADDCMD_TEXT, kText, sizeof(kText))) { C1Printf("glyphtest: R_AddCmdDrawText bytes differ\n"); return; }
        BYTE j[5] = { 0xE9 };
        LONG r = (LONG)((DWORD_PTR)CTL_AddTextHook - (CTL_ADDCMD_TEXT + 5));
        memcpy(j + 1, &r, 4);
        WriteCode((void *)(DWORD_PTR)CTL_ADDCMD_TEXT, j, 5);
        if (CallsTarget(0x0060AB19u, g_ctlStageA)) RedirectCall(0x0060AB19u, (void *)CTL_StageAHook);
        if (CallsTarget(0x004A0810u, g_ctlStageB)) RedirectCall(0x004A0810u, (void *)CTL_StageBHook);
        hooked = TRUE;
    }
    if (VoiceArgc() > 1) {
        g_overlayCaller = strtoul(VoiceArgv(1), NULL, 16);
        g_glyphOverlay = g_overlayCaller != 0 || strcmp(VoiceArgv(1), "0");
        if (!strcmp(VoiceArgv(1), "off")) g_glyphOverlay = 0;
    } else g_glyphOverlay = !g_glyphOverlay;
    C1Printf("glyphtest %s (caller %08lX)\n", g_glyphOverlay ? "on" : "off", (unsigned long)g_overlayCaller);
}

/*
 * hud_style.c - the selectable HUD style: k33_hudStyle and k33_hudStyleList. Included by kernel33.c after
 * k33tick.c (needs c1game.c and K33OnMainTick). docs/HUD_STYLES_20261112.md.
 *
 * The combat HUD is native menus (ui_mp/hud_720.txt). codol_common_postgfx_mp carries the later builds' HUD items
 * inside C1's own menus (zones/hud_styles_postgfx_mp, zones/survival_hud_postgfx_mp; tools/build_hud_styles.py),
 * each item with a visibility expression on dvarint("k33_hudStyle"): the stock items show while the dvar is not a
 * shipped style, a style's items while it is that style's number. C1 evaluates those expressions every frame, so
 * a "set k33_hudStyle N" (the pause and options menus' setdvar) changes the HUD at once: nothing is patched here,
 * no menu is reloaded. This file only owns the two dvars.
 *
 *   k33_hudStyle       int 0..4, archived (flag 0x1): 0 stock 1.1.1.13, 1 v12 (1.2.1.12), 2 v26 (2.6.9.7),
 *                      3 v33 (3.3.3.3), 4 v35 (3.5.2.6). The numbers are fixed whether or not a style ships.
 *   k33_hudStyleList   string, read only by convention (flag 0, as voice.c's k33_voiceInputList): the styles this
 *                      build ships as "number:id" pairs, HUD_STYLE_LIST. The menus cycle through these.
 *
 * A saved k33_hudStyle that is not in the list (a style a later package shipped) is set back to 0, logged once per
 * value; the menus' stock items show for any value that is not a shipped style, so the HUD is never empty meanwhile.
 *
 * C1 sites (1.1.1.13), each byte-checked before its first use; on a mismatch one line is logged and the dvars
 * are not registered (the HUD then stays stock: dvarint of a missing dvar is 0):
 *   Dvar_RegisterInt    0x00624420 (name, default, min, max, flags, description)   MEASURED: C1.exe prologue
 *                       55 8B EC 83 E4 F8 64 A1 2C 00 00 00; c1game.c C1RegisterInt calls it.
 *   Dvar_RegisterString 0x00624740 (name, value, flags, description)               MEASURED: C1.exe prologue
 *                       55 8B EC 83 E4 F8 83 EC 10 8B 55 14 8B 4D 0C 52; voice.c calls it the same way.
 * Registration waits for C1DvarsReady() on the main thread (K33OnMainTick): C1 has run its own registrations and
 * read the config by then, so an archived value is already in place when Dvar_RegisterInt finds the dvar.
 */

#define HUD_STYLE_DVAR      "k33_hudStyle"
#define HUD_STYLE_LIST_DVAR "k33_hudStyleList"
/* tools/build_hud_styles.py style_list(): the styles whose sources are in zones/hud_styles_postgfx_mp/sources
 * (tests/test_hud_styles.py compares the two). */
#define HUD_STYLE_LIST      "0:stock;1:v12;2:v26"
#define HUD_STYLE_MAX       4
#define HUD_STYLE_REGISTER_STRING_VA 0x00624740u

static const BYTE *g_hudStyleDvar;
static BOOL g_hudStyleDone;

/* Is `v` one of the numbers of HUD_STYLE_LIST ("n:id;n:id")? */
static BOOL HudStyleShipped(int v)
{
    for (const char *p = HUD_STYLE_LIST; *p;) {
        if (atoi(p) == v) return TRUE;
        p = strchr(p, ';');
        if (!p) break;
        p++;
    }
    return FALSE;
}

static void HudStyleRegister(void)
{
    static const BYTE kRegInt[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x64, 0xA1, 0x2C, 0x00, 0x00, 0x00 };
    static const BYTE kRegString[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x10, 0x8B, 0x55, 0x14,
                                       0x8B, 0x4D, 0x0C, 0x52 };
    typedef const BYTE *(__cdecl *RegStr)(const char *, const char *, unsigned, const char *);
    g_hudStyleDone = TRUE;
    if (!Readable((void *)(DWORD_PTR)C1_REGISTER_INT_VA, sizeof(kRegInt)) ||
        memcmp((void *)(DWORD_PTR)C1_REGISTER_INT_VA, kRegInt, sizeof(kRegInt)) ||
        !Readable((void *)(DWORD_PTR)HUD_STYLE_REGISTER_STRING_VA, sizeof(kRegString)) ||
        memcmp((void *)(DWORD_PTR)HUD_STYLE_REGISTER_STRING_VA, kRegString, sizeof(kRegString))) {
        Log("hud style: Dvar_RegisterInt 0x%08X or Dvar_RegisterString 0x%08X differs; %s not registered, HUD stays stock",
            C1_REGISTER_INT_VA, HUD_STYLE_REGISTER_STRING_VA, HUD_STYLE_DVAR);
        return;
    }
    g_hudStyleDvar = C1RegisterInt(HUD_STYLE_DVAR, 0, 0, HUD_STYLE_MAX, 0x1,
                                   "HUD style: 0 stock 1.1.1.13, 1 1.2.1.12, 2 2.6.9.7, 3 3.3.3.3, 4 3.5.2.6 (k33_hudStyleList: the ones in this build)");
    ((RegStr)(DWORD_PTR)HUD_STYLE_REGISTER_STRING_VA)(HUD_STYLE_LIST_DVAR, HUD_STYLE_LIST, 0,
                                                      "HUD styles in this build as number:id pairs (read only)");
    if (!g_hudStyleDvar) {
        Log("hud style: %s could not be registered; HUD stays stock", HUD_STYLE_DVAR);
        return;
    }
    /* C1 does not apply "+set" from the command line to a dvar registered this late (voice.c): re-issue ours. */
    int v = C1DvarInt(g_hudStyleDvar, 0);
    const char *cl = GetCommandLineA();
    const char *at = cl ? strstr(cl, "+set " HUD_STYLE_DVAR " ") : NULL;
    if (at) {
        char cmd[64];
        at += sizeof("+set " HUD_STYLE_DVAR " ") - 1;
        v = atoi(at[0] == '"' ? at + 1 : at);
        _snprintf(cmd, sizeof(cmd), "set %s %d\n", HUD_STYLE_DVAR, v);
        cmd[sizeof(cmd) - 1] = 0;
        C1CbufAddText(cmd);
    }
    Log("hud style: %s registered (%s %s), value %d%s", HUD_STYLE_DVAR, HUD_STYLE_LIST_DVAR, HUD_STYLE_LIST, v,
        at ? " from the command line" : "");
}

/* Main thread, every frame: register once the dvars are ready, then keep the value inside the shipped list. */
static void HudStyleTick(void)
{
    static int lastBad = -1;
    if (!g_hudStyleDone) {
        if (!C1DvarsReady()) return;
        HudStyleRegister();
    }
    if (!g_hudStyleDvar) return;
    int v = C1DvarInt(g_hudStyleDvar, 0);
    if (HudStyleShipped(v)) { lastBad = -1; return; }
    if (v == lastBad) return;        /* the "set" below is queued: it lands in this frame's Cbuf_Execute */
    lastBad = v;
    C1CbufAddText("set " HUD_STYLE_DVAR " 0\n");
    Log("hud style: %s %d is not in this build (%s); showing the stock HUD", HUD_STYLE_DVAR, v, HUD_STYLE_LIST);
}

static void InstallHudStyle(void)
{
    C1GameInit();
    if (!g_c1DvarOk || !g_c1CbufOk) {
        Log("hud style: Dvar_RegisterInt or Cbuf_AddText unavailable; %s not registered, HUD stays stock", HUD_STYLE_DVAR);
        return;
    }
    if (!K33OnMainTick(HudStyleTick))
        Log("hud style: no free main-thread tick; %s not registered, HUD stays stock", HUD_STYLE_DVAR);
}

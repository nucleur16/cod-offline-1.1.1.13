/*
 * hud_style.c - the selectable HUD style: cg_hudStyle and cg_hudStyleList, and the HUD logic of the later builds
 * that 1.1.1.13 lacks. Included by kernel33.c after k33tick.c and voice.c (needs c1game.c, K33OnMainTick and
 * voice.c's CG_OwnerDraw hook and draw thunks). docs/HUD_STYLES_20261112.md.
 *
 * The combat HUD is native menus (ui_mp/hud_720.txt). common_postgfx_mp carries the later builds' HUD items
 * inside C1's own menus (zones/hud_styles_postgfx_mp, zones/survival_hud_postgfx_mp; tools/build_hud_styles.py),
 * each item with a visibility expression on the style dvar (MENU_PENDING: dvarint("k33_hudStyle") until the menu
 * sources spell cg_hudStyle): the stock items show while the dvar is not a
 * shipped style, a style's items while it is that style's number. C1 evaluates those expressions every frame, so
 * a "set cg_hudStyle N" (the pause and options menus' setdvar) changes the HUD at once; no menu is reloaded.
 * MENU_PENDING: k33_hudStyle is the old name of cg_hudStyle, kept as a read-only int that HudStyleTick sets to
 * cg_hudStyle's value every frame (HUD_STYLE_MENU_DVAR) because the native menus' expressions still read it; it goes
 * when the menu sources (feat/menu-source) spell cg_hudStyle.
 *
 *   cg_hudStyle       int 0..5, archived (flag 0x1): 0 stock 1.1.1.13, 1 v12 (1.2.1.12), 2 v26 (2.6.9.7),
 *                      3 v33 (3.3.3.3), 4 v35 (3.5.2.6), 5 sm2 (authored as .menu source, an MW2-like HUD).
 *                      The numbers are fixed whether or not a style ships.
 *   cg_hudStyleList   string, read only by convention (flag 0, as voice.c's cl_voiceInputList): the styles this
 *                      build ships as "number:id" pairs, HUD_STYLE_LIST. The menus cycle through these.
 * A saved cg_hudStyle that is not in the list is set back to 0, logged once per value.
 *
 * What the later builds' HUD reads that 1.1.1.13 has no operator or ownerdraw for, implemented here:
 *   ownerDraw 3001     the clip count, drawn at the item's rect with its font, scale, colour and style
 *   ownerDraw 3002     the clip gauge: the item's rect, emptied from its left end as the clip empties
 *   ownerDraw 3003     the health fill (SM2): the item's rect filled from its left end by C1's own health fraction,
 *                      in the item's colour, red below 30 %
 *   ownerDraw 3004     the weapon icon (SM2): the weapon def's HUD icon, as high as the rect, as wide as its image's
 *                      own width / height gives, right-aligned (C1's ownerDraw 118 stretches it by the def's ratio)
 *   ownerDraw 3005     the compass strip (SM2): C1's heading tape (ownerDraw 145's 0x004321B0) without objectives
 *                      (no icons, no distances), at the heading's own rate, showing HUD_TAPE_DEGREES of it (below)
 *                      All five are read when the menu paints (no tick, no dvar): the same playerState C1's own
 *                      HUD reads, so they change in the same frame as player("stockAmmo") does.
 *   cg_hudSway        the HUD sway, every style (int 0/1, archived, on): cg_hudSwayX / Y (read only, hundredths
 *                      of a 640x480 unit), which every item of the four HUD hosts adds to its rect
 *                      (tools/build_hud_styles.py sway()), written on the main tick (HudSwayStep, below)
 *   cg_hudFireType    string: the "@SF_WEAPON_*_NAME" label of the weapon in hand (2.6.9.7's getweaponfiretype),
 *                      from weaponDef + 0x350 / + 0xB64 (hud_style_policy.h HudFireTypeKey)
 *   cg_hudAssists     int: the local player's assists (2.6.9.7's player("assists")), from C1's score records
 * The two dvars are published on the main tick while a style is on (they change with a weapon switch or a score
 * message, not per shot); the menus read them with dvarstring / dvarint.
 *
 * C1 sites (1.1.1.13, all MEASURED in C1.exe), each byte-checked before its first use; on a mismatch one line is
 * logged and that part stays off (the HUD then stays stock, or the item draws nothing / reads 0):
 *   Dvar_RegisterInt    0x00624420 (name, default, min, max, flags, description), prologue
 *                       55 8B EC 83 E4 F8 64 A1 2C 00 00 00; c1game.c C1RegisterInt calls it.
 *   Dvar_RegisterString 0x00624740 (name, value, flags, description), prologue 55 8B EC 83 E4 F8 83 EC 10 8B 55 14
 *                       8B 4D 0C 52; voice.c calls it the same way.
 *   0x0045C540  player("clipAmmo")'s getter: cg = [0x016D3B58 + [0x06C3D2AC] * 4] ^ 0x490B400C; weapon = [cg + 0x33C];
 *               weaponDef = [0x0150D720 + weapon * 4]; clip slot = [weaponDef + 0x570]; the player's 15 clips at
 *               cg + 0x3D8, 12 bytes each {slot, rounds right hand, rounds left hand}.
 *   0x00453D16  mov eax, [edx + 0xB60]: weaponDef's clip size (C1's low-ammo test divides the clip by it;
 *               1.2.1.12 has the same instruction with +0xB94 where its ammo display computes the bar).
 *   0x00417F6F  cmp byte [edi + 0xBD5], 0, inside C1's dual-wield test 0x00417F50, which also needs the weapon's
 *               akimbo byte (0x0040AB60: the weapon's index among cg + 0x2A0[15], record cg + 0x2DC + idx * 5,
 *               byte +1; index 14 while dvar [0x0238A908] byte +0xC is set). HudAkimbo repeats both.
 *   0x00465240  CG_ParseScores: 19 rows of 0x40 bytes at cg + 0xBEB70, row count cg + 0xBEB18; the 7th value of a
 *               row (the sh1x server's assists, server/sh1x src/online/stats/scoreboard_wire.c) at +0x1C,
 *               the client number at +0.
 *   weaponDef + 0x350 fire type, + 0xB64 class: located by value (hud_style_policy.h), no instruction to check;
 *               read only after the three sites above passed.
 *   0x00453EE0  C1's health fraction (CG_DrawPlayerBarHealth 0x004558C0 calls it): mov ecx, [eax + 190h] (health),
 *               mov ecx, [eax + 198h] (max health), cmp dword [eax + 4], 8; checked from 0x00453EE6.
 *   0x0042B9D9  CG_DrawPlayerWeaponIcon (ownerDraw 118, 0x0042B960): mov edx, [ecx + 554h] (hudIconRatio);
 *               mov eax, [ecx + 550h] (hudIcon Material*). The icon's image is the material's first texture
 *               (Material: texture table at +0x54, count byte +0x48; MaterialTextureDef image at +8; GfxImage
 *               width / height u16 at +0x14 / +0x16, the zone layouts zonekit/codecs.py and build_hud_styles.py
 *               write).
 *   0x0047C4D7  mov eax, [edi + 548h]; test eax, eax; jnz; mov eax, [edi + 540h]: C1 takes a weapon's icon from
 *               weaponDef + 0x548, else + 0x540, into its per-weapon icon table (0x016D4564; 0x00486875 draws
 *               + 0x540 the same way). A weapon whose HUD icon (+ 0x550) is empty (the sniper rifles) is drawn
 *               with that icon instead.
 *   0x004321B0  the heading tape (CG_OwnerDraw 0x00459E10 case 145 at 0x0045A13C; case 146, 0x0045A161, passes 0
 *               for its last argument and draws no objectives). ecx = the item's rectDef {x, y, w, h, horz byte,
 *               vert byte}, edx = the item's material; cdecl stack: local client, the address of CG_OwnerDraw's
 *               [ebp+0Ch], colour, font, text scale, text style, objectives (1 / 0); add esp, 1Ch. Each objective
 *               of type 1 or 4 gets its icon on the tape and, from 0x00432712 -> 0x00432090 -> 0x00432020, its
 *               distance as "%im" / "%.1fm" (0x00C41DD8 / 0x00C41DD0) in that text scale, the only use of the
 *               scale (0x004326CB fld [ebp+18h]). Under SM2 the strip lies across the minimap's top edge, so those
 *               numbers read as the minimap's; 3005 passes 0 for objectives, as case 146 does: no icons, no
 *               numbers. The tape centres its material on -(cg + 0xB441C, the view's yaw, - cg + 0xBFA08, the
 *               compass north) / 360 (0x004322B3, byte-checked); viper_heading_mp holds two laps, so 3005 sets the
 *               north to HudTapeNorth() for its call and back after (hud_style_policy.h). The tape reads its width from dvar 249 compassTickertapeStretch (c1_dvars.h), a pointer
 *               C1 keeps at 0x016C74E8 (stored at 0x0042EF25; read at 0x00432276 for the material and 0x004323AA
 *               for the icons, value at +0Ch); 3005 sets it to HudTapeStretch(HUD_TAPE_DEGREES, ..) for its own
 *               call and puts the value back after, so the stock killstreak menu's 400-unit tape is unchanged.
 *   cg + 0xB4418 the rendered view's angles {pitch, yaw, roll}: the playerState's (cg + 0x14C) as C1 copies them
 *               (0x0045648C) plus the camera shake 0x0042C577.. adds (sin-driven, explosions and damage). The sway
 *               reads the difference as the shake; the playerState's velocity is cg + 0x28 (console.c).
 * The ownerdraws are dispatched from voice.c's CG_OwnerDraw entry hook (0x00459E10, one hook for both files:
 * VoiceOwnerDraw calls HudStyleOwnerDraw first). Without that hook they draw nothing; that is logged once.
 */

#include "hud_style_policy.h"

#define HUD_STYLE_DVAR      "cg_hudStyle"
#define HUD_STYLE_LIST_DVAR "cg_hudStyleList"
/* The native HUD menus' visibility expressions still read the old name (MENU_PENDING: tools/drop_codol_prefix.py). */
#define HUD_STYLE_MENU_DVAR "k33_hudStyle" /* MENU_PENDING */
/* LEGACY_NAME: the old name on the command line ("+set k33_hudStyle N"), read when the new one is absent. */
#define HUD_STYLE_OLD_ARG   "+set k33_hudStyle " /* LEGACY_NAME */
/* tools/build_hud_styles.py style_list(): the styles whose sources are in zones/hud_styles_postgfx_mp/sources
 * (tests/test_hud_styles.py compares the two). */
#define HUD_STYLE_LIST      "0:stock;2:v26;3:v33;5:sm2"
#define HUD_STYLE_MAX       5
#define HUD_STYLE_REGISTER_STRING_VA 0x00624740u
#define HUD_FIRE_DVAR       "cg_hudFireType"
#define HUD_ASSISTS_DVAR    "cg_hudAssists"
#define HUD_SWAY_DVAR       "cg_hudSway"
#define HUD_SWAY_X_DVAR     "cg_hudSwayX"
#define HUD_SWAY_Y_DVAR     "cg_hudSwayY"
#define HUD_CG_NORTH        0xBFA08
#define HUD_CG_VIEWANGLES   0xB4418
#define HUD_OD_CLIP         3001    /* tools/build_hud_styles.py OD_CLIP */
#define HUD_OD_CLIP_BAR     3002    /* tools/build_hud_styles.py OD_CLIP_BAR */
#define HUD_OD_HEALTH       3003    /* tools/build_hud_styles.py OD_HEALTH */
#define HUD_OD_WEAPON_ICON  3004    /* tools/build_hud_styles.py OD_WEAPON_ICON */
#define HUD_OD_COMPASS      3005    /* tools/build_hud_styles.py OD_COMPASS */
#define HUD_TAPE_VA         0x004321B0u
#define HUD_TAPE_STRETCH_VA 0x016C74E8u
#define HUD_HEALTH_VA       0x00453EE6u
#define HUD_ICON_VA         0x0042B9D9u
#define HUD_ICON_ALT_VA     0x0047C4D7u
#define HUD_WEAPONDEF_VA    0x0150D720u
#define HUD_CLIP_GETTER_VA  0x0045C540u
#define HUD_CLIP_SIZE_VA    0x00453D16u
#define HUD_DUAL_WIELD_VA   0x00417F6Fu
#define HUD_WEAPON_SLOT_VA  0x0040AB60u
#define HUD_AKIMBO_DVAR_VA  0x0238A908u
#define HUD_SCORES_VA       0x00465254u
#define HUD_SCORE_ROWS      0xBEB70
#define HUD_SCORE_COUNT     0xBEB18

static const BYTE *g_hudStyleMenuDvar;
static const BYTE *g_hudStyleDvar, *g_hudAssistsDvar, *g_hudFireDvar, *g_hudSwayDvar, *g_hudSwayXDvar,
                  *g_hudSwayYDvar;
static BOOL g_hudStyleDone;
static BOOL g_hudClipOk, g_hudScoresOk, g_hudHealthOk, g_hudIconOk, g_hudIconAltOk, g_hudTapeOk, g_hudSwayOk;
static void *g_hudWhite;

/* The local client's cg, or NULL outside a match. Main thread. */
static const BYTE *HudCg(void)
{
    const BYTE *cg = ParityCg();     /* parity.c: [0x016D3B58 + [0x06C3D2AC] * 4] ^ key, the getter's own cg */
    return cg && Readable(cg + 0x2A0, 0x3D8 + 15 * 12 - 0x2A0) ? cg : NULL;
}

/* C1's dual-wield test 0x00417F50 (header comment). */
static BOOL HudAkimbo(const BYTE *cg, int weapon, const BYTE *def)
{
    int idx = -1;
    const BYTE *dv = *(const BYTE *volatile *)(DWORD_PTR)HUD_AKIMBO_DVAR_VA;
    if (dv && Readable(dv + 0xC, 1) && dv[0xC]) idx = 14;
    else
        for (int i = 0; i < 15; i++)
            if (*(const int *)(cg + 0x2A0 + 4 * i) == weapon) { idx = i; break; }
    if (idx < 0 || !Readable(cg + 0x2DC + 5 * idx, 2)) return FALSE;
    return cg[0x2DC + 5 * idx + 1] != 0 && def[0xBD5] != 0;
}

/* The weapon in hand: its def (NULL when there is none) and, through the out parameters, the clip count the
 * later display shows and the gauge's fill. Main thread. */
static const BYTE *HudWeapon(int *rounds, int *permille)
{
    *rounds = 0;
    *permille = 0;
    const BYTE *cg = HudCg();
    if (!g_hudClipOk || !cg) return NULL;
    int weapon = *(const int *)(cg + 0x33C);
    if (weapon <= 0 || weapon > 4095) return NULL;
    const BYTE *def = *(const BYTE *volatile *)(DWORD_PTR)(HUD_WEAPONDEF_VA + 4u * (DWORD)weapon);
    if (!def || !Readable(def + 0x350, 0xBD6 - 0x350)) return NULL;
    int slot = *(const int *)(def + 0x570), size = *(const int *)(def + 0xB60);
    BOOL akimbo = HudAkimbo(cg, weapon, def);
    for (int i = 0; i < 15; i++) {
        const int *clip = (const int *)(cg + 0x3D8 + 12 * i);
        if (clip[0] != slot) continue;
        *rounds = HudClipRounds(clip[1], clip[2], akimbo);
        *permille = HudClipPermille(*rounds, size, akimbo);
        break;
    }
    return def;
}

/* The local player's assists from the score records (MP rows), or 0. Main thread. */
static int HudAssists(void)
{
    const BYTE *cg = HudCg();
    if (!g_hudScoresOk || !cg || !Readable(cg + HUD_SCORE_COUNT, 4) ||
        !Readable(cg + HUD_SCORE_ROWS, 19 * 0x40) || !Readable(cg + PS_CLIENTNUM, 4))
        return 0;
    int me = *(const int *)(cg + PS_CLIENTNUM), n = *(const int *)(cg + HUD_SCORE_COUNT);
    for (int i = 0; i < n && i < 19; i++) {
        const BYTE *row = cg + HUD_SCORE_ROWS + 0x40 * i;
        if (*(const int *)row == me) return *(const int *)(row + 0x1C);
    }
    return 0;
}

/* The weapon icon's material and its image's size, or NULL. Main thread. */
static void *HudWeaponIcon(int *imageW, int *imageH)
{
    *imageW = *imageH = 0;
    const BYTE *cg = HudCg();
    if (!g_hudIconOk || !cg) return NULL;
    int weapon = *(const int *)(cg + 0x33C);
    if (weapon <= 0 || weapon > 4095) return NULL;
    const BYTE *def = *(const BYTE *volatile *)(DWORD_PTR)(HUD_WEAPONDEF_VA + 4u * (DWORD)weapon);
    if (!def || !Readable(def + 0x540, 0x18)) return NULL;
    /* the HUD icon; without one, the icon C1 itself falls back to (+ 0x548, else + 0x540: 0x0047C4D7) */
    const BYTE *mat = *(const BYTE *const *)(def + 0x550);
    if (!mat && g_hudIconAltOk) mat = *(const BYTE *const *)(def + 0x548);
    if (!mat && g_hudIconAltOk) mat = *(const BYTE *const *)(def + 0x540);
    if (!mat || !Readable(mat, 0x60)) return NULL;
    const BYTE *table = *(const BYTE *const *)(mat + 0x54);
    if (mat[0x48] && table && Readable(table, 12)) {
        const BYTE *image = *(const BYTE *const *)(table + 8);
        if (image && Readable(image + 0x14, 4)) {
            *imageW = *(const WORD *)(image + 0x14);
            *imageH = *(const WORD *)(image + 0x16);
        }
    }
    return (void *)mat;
}

/* ownerDraws 3003 (health fill) and 3004 (weapon icon); arguments as HudStyleOwnerDraw. 1 = drawn (or empty). */
static int HudStyleOwnerDraw2(const DWORD *args, int ownerDraw)
{
    int lc = (int)args[0];
    float x = *(const float *)&args[(0x20 - 8) / 4], y = *(const float *)&args[(0x24 - 8) / 4];
    float w = *(const float *)&args[(0x28 - 8) / 4], h = *(const float *)&args[(0x2C - 8) / 4];
    int horz = (BYTE)args[(0x30 - 8) / 4], vert = (BYTE)args[(0x34 - 8) / 4];
    const float *color = (const float *)(DWORD_PTR)args[(0x4C - 8) / 4];
    float c[4] = { 1, 1, 1, 1 };
    if (color && Readable(color, 16)) memcpy(c, color, sizeof(c));
    const void *scr = ((const void *(__cdecl *)(int))(DWORD_PTR)VOICE_SCRPLACE_VA)(lc);
    if (ownerDraw == HUD_OD_HEALTH) {
        const BYTE *cg = HudCg();
        if (!g_hudHealthOk || !cg || !Readable(cg, 0x19C)) return 1;
        int permille = HudHealthPermille(*(const int *)(cg + 0x190), *(const int *)(cg + 0x198), *(const int *)(cg + 4));
        if (!g_hudWhite)
            g_hudWhite = ((void *(__cdecl *)(const char *, int))(DWORD_PTR)VOICE_MAT_REGISTER_VA)("white", 7);
        /* the fill eases up while health regenerates and drops at once on damage (HudHealthLerp) */
        static float shown = -1.0f;
        static LARGE_INTEGER last, freq;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
        float dt = last.QuadPart && freq.QuadPart ? (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart)
                                                  : 0.0f;
        last = now;
        shown = HudHealthLerp(shown, permille, dt);
        if (shown < HUD_HEALTH_LOW_PERMILLE) { c[0] = 0.9f; c[1] = 0.22f; c[2] = 0.2f; }
        if (g_hudWhite && shown > 0.0f) VOICE_DrawPic(scr, c, x, y, w * shown / 1000.0f, h, horz, vert, g_hudWhite);
        return 1;
    }
    int iw, ih;
    void *mat = HudWeaponIcon(&iw, &ih);
    if (!mat) return 1;
    {
        static const void *lastMat;
        if (mat != lastMat) {
            const char *name = *(const char *const *)mat;
            lastMat = mat;
            Log("hud style: weapon icon %s, image %dx%d", name && Readable(name, 1) ? name : "?", iw, ih);
        }
    }
    float ix, iwid;
    HudIconFit(x, w, h, iw, ih, &ix, &iwid);
    VOICE_DrawPic(scr, c, ix, y, iwid, h, horz, vert, mat);
    return 1;
}

/* C1's heading tape 0x004321B0 (header comment): the thunk puts the rect in ecx and the material in edx and
 * re-pushes the other seven arguments (each "push [esp+36]" takes the next lower one). */
struct HudRectDef { float x, y, w, h; BYTE horz, vert, pad[2]; };
__attribute__((used)) DWORD g_hudTape = HUD_TAPE_VA;
void __cdecl HUD_DrawTape(const struct HudRectDef *rect, void *material, int lc, const void *ownerDrawRect,
                          const float *color, void *font, float scale, int style, int objectives);
__asm__(
    ".text\n"
    ".globl _HUD_DrawTape\n"
    "_HUD_DrawTape:\n"
    "    movl 4(%esp), %ecx\n"
    "    movl 8(%esp), %edx\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    pushl 36(%esp)\n"
    "    call *_g_hudTape\n"
    "    addl $28, %esp\n"
    "    ret\n");

/* ownerDraw 3005, the SM2 compass strip: C1's tape without objectives, one lap of labels per turn, over
 * HUD_TAPE_DEGREES of the heading. Arguments as HudStyleOwnerDraw. Main thread (C1's menu paint). */
static int HudStyleTape(const DWORD *args)
{
    if (!g_hudTapeOk) return 1;
    void *material = (void *)(DWORD_PTR)args[(0x50 - 8) / 4];
    if (!material) return 1;
    float *stretch = NULL;
    BYTE *dvar = *(BYTE *volatile *)(DWORD_PTR)HUD_TAPE_STRETCH_VA;
    if (dvar && Readable(dvar + 0xC, 4)) stretch = (float *)(dvar + 0xC);
    struct HudRectDef r;
    r.x = *(const float *)&args[(0x20 - 8) / 4];
    r.y = *(const float *)&args[(0x24 - 8) / 4];
    r.w = *(const float *)&args[(0x28 - 8) / 4];
    r.h = *(const float *)&args[(0x2C - 8) / 4];
    r.horz = (BYTE)args[(0x30 - 8) / 4];
    r.vert = (BYTE)args[(0x34 - 8) / 4];
    r.pad[0] = r.pad[1] = 0;
    BYTE *cg = ParityCg();
    if (!cg || !Readable(cg + HUD_CG_NORTH, 4) || !Readable(cg + HUD_CG_VIEWANGLES, 12)) return 1;
    float *north = (float *)(cg + HUD_CG_NORTH);
    float saved = stretch ? *stretch : 0.0f, savedNorth = *north;
    if (stretch) *stretch = HudTapeStretch(HUD_TAPE_DEGREES, HUD_TAPE_LAP_DEGREES);
    *north = HudTapeNorth(*(const float *)(cg + HUD_CG_VIEWANGLES + 4), savedNorth);
    HUD_DrawTape(&r, material, (int)args[0], &args[1], (const float *)(DWORD_PTR)args[(0x4C - 8) / 4],
                 (void *)(DWORD_PTR)args[(0x44 - 8) / 4], 0.0f, (int)args[(0x54 - 8) / 4], 0);
    *north = savedNorth;
    if (stretch) *stretch = saved;
    return 1;
}

/* CG_OwnerDraw's arguments as voice.c reads them (args = [ebp+8]). 1 = drawn (or ours and empty). */
static int HudStyleOwnerDraw(const DWORD *args)
{
    int ownerDraw = (int)args[(0x40 - 8) / 4];
    if (ownerDraw < HUD_OD_CLIP || ownerDraw > HUD_OD_COMPASS) return 0;
    if (ownerDraw == HUD_OD_COMPASS) return HudStyleTape(args);
    if (ownerDraw == HUD_OD_HEALTH || ownerDraw == HUD_OD_WEAPON_ICON) return HudStyleOwnerDraw2(args, ownerDraw);
    int rounds, permille;
    if (!HudWeapon(&rounds, &permille)) return 1;
    int lc = (int)args[0];
    float x = *(const float *)&args[(0x20 - 8) / 4], y = *(const float *)&args[(0x24 - 8) / 4];
    float w = *(const float *)&args[(0x28 - 8) / 4], h = *(const float *)&args[(0x2C - 8) / 4];
    int horz = (BYTE)args[(0x30 - 8) / 4], vert = (BYTE)args[(0x34 - 8) / 4];
    void *font = (void *)(DWORD_PTR)args[(0x44 - 8) / 4];
    float scale = *(const float *)&args[(0x48 - 8) / 4];
    const float *color = (const float *)(DWORD_PTR)args[(0x4C - 8) / 4];
    int style = (int)args[(0x54 - 8) / 4];
    float c[4] = { 1, 1, 1, 1 };
    if (color && Readable(color, 16)) memcpy(c, color, sizeof(c));
    const void *scr = ((const void *(__cdecl *)(int))(DWORD_PTR)VOICE_SCRPLACE_VA)(lc);
    if (ownerDraw == HUD_OD_CLIP) {
        char text[16];
        _snprintf(text, sizeof(text), "%i", rounds);
        text[sizeof(text) - 1] = 0;
        if (font)
            ((UiDrawTextFn)(DWORD_PTR)VOICE_DRAWTEXT_VA)(scr, text, 16, font, x, y + h, horz, vert, scale, c, style);
        return 1;
    }
    if (!g_hudWhite)
        g_hudWhite = ((void *(__cdecl *)(const char *, int))(DWORD_PTR)VOICE_MAT_REGISTER_VA)("white", 7);
    float bx, bw;
    HudClipBar(x, w, permille, &bx, &bw);
    if (g_hudWhite && bw > 0.0f) VOICE_DrawPic(scr, c, bx, y, bw, h, horz, vert, g_hudWhite);
    return 1;
}

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
    {
        static const BYTE kGetter[] = { 0x55, 0x8B, 0xEC, 0xA1, 0xAC, 0xD2, 0xC3, 0x06, 0x8B, 0x04, 0x85, 0x58, 0x3B,
                                        0x6D, 0x01, 0x35, 0x0C, 0x40, 0x0B, 0x49, 0x8B, 0x88, 0x3C, 0x03, 0x00, 0x00,
                                        0x8B, 0x14, 0x8D, 0x20, 0xD7, 0x50, 0x01, 0x8B, 0x92, 0x70, 0x05, 0x00, 0x00 };
        static const BYTE kSize[] = { 0x8B, 0x82, 0x60, 0x0B, 0x00, 0x00 };
        static const BYTE kDual[] = { 0x80, 0xBF, 0xD5, 0x0B, 0x00, 0x00, 0x00 };
        static const BYTE kSlot[] = { 0x85, 0xD2, 0x74, 0x39, 0xA1, 0x08, 0xA9, 0x38, 0x02, 0x85, 0xC0, 0x74, 0x16,
                                      0x80, 0x78, 0x0C, 0x00, 0x74, 0x10, 0xB8, 0x0E, 0x00, 0x00, 0x00, 0x8D, 0x0C,
                                      0x86, 0x8D, 0x84, 0x08, 0xDC, 0x02, 0x00, 0x00 };
        /* CG_ParseScores: push 4C0h (19 rows * 40h); lea edx, [ebx + 0BEB70h] */
        static const BYTE kScores[] = { 0x8D, 0x93, 0x70, 0xEB, 0x0B, 0x00 };
        static const BYTE kScoreRow[] = { 0x89, 0x46, 0x08 };   /* 0x0046541E mov [esi+8], eax: value 7 -> row + 1Ch */
        g_hudClipOk = Readable((void *)(DWORD_PTR)HUD_CLIP_GETTER_VA, sizeof(kGetter)) &&
                      !memcmp((void *)(DWORD_PTR)HUD_CLIP_GETTER_VA, kGetter, sizeof(kGetter)) &&
                      Readable((void *)(DWORD_PTR)HUD_CLIP_SIZE_VA, sizeof(kSize)) &&
                      !memcmp((void *)(DWORD_PTR)HUD_CLIP_SIZE_VA, kSize, sizeof(kSize)) &&
                      Readable((void *)(DWORD_PTR)HUD_DUAL_WIELD_VA, sizeof(kDual)) &&
                      !memcmp((void *)(DWORD_PTR)HUD_DUAL_WIELD_VA, kDual, sizeof(kDual)) &&
                      Readable((void *)(DWORD_PTR)HUD_WEAPON_SLOT_VA, sizeof(kSlot)) &&
                      !memcmp((void *)(DWORD_PTR)HUD_WEAPON_SLOT_VA, kSlot, sizeof(kSlot));
        static const BYTE kHealth[] = { 0x8B, 0x88, 0x90, 0x01, 0x00, 0x00, 0x89, 0x4D, 0xFC, 0x85, 0xC9, 0x74, 0x3D,
                                        0x8B, 0x88, 0x98, 0x01, 0x00, 0x00, 0x89, 0x4D, 0xF8, 0x85, 0xC9, 0x74, 0x30,
                                        0x83, 0x78, 0x04, 0x08 };
        static const BYTE kIcon[] = { 0x8B, 0x91, 0x54, 0x05, 0x00, 0x00, 0x8B, 0x81, 0x50, 0x05, 0x00, 0x00 };
        g_hudHealthOk = Readable((void *)(DWORD_PTR)HUD_HEALTH_VA, sizeof(kHealth)) &&
                        !memcmp((void *)(DWORD_PTR)HUD_HEALTH_VA, kHealth, sizeof(kHealth));
        static const BYTE kIconAlt[] = { 0x8B, 0x87, 0x48, 0x05, 0x00, 0x00, 0x85, 0xC0, 0x75, 0x06, 0x8B, 0x87, 0x40,
                                         0x05, 0x00, 0x00 };
        g_hudIconOk = g_hudClipOk && Readable((void *)(DWORD_PTR)HUD_ICON_VA, sizeof(kIcon)) &&
                      !memcmp((void *)(DWORD_PTR)HUD_ICON_VA, kIcon, sizeof(kIcon));
        g_hudIconAltOk = g_hudIconOk && Readable((void *)(DWORD_PTR)HUD_ICON_ALT_VA, sizeof(kIconAlt)) &&
                         !memcmp((void *)(DWORD_PTR)HUD_ICON_ALT_VA, kIconAlt, sizeof(kIconAlt));
        /* the tape (prologue), its stretch reads, its text scale and case 145's call (header comment) */
        static const BYTE kTape[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0x8C, 0x00, 0x00, 0x00 };
        static const BYTE kTapeStretch[] = { 0x8B, 0x15, 0xE8, 0x74, 0x6C, 0x01 };     /* 0x00432276 */
        static const BYTE kTapeIcons[] = { 0xA1, 0xE8, 0x74, 0x6C, 0x01 };            /* 0x004323AA */
        static const BYTE kTapeScale[] = { 0xD9, 0x45, 0x18 };                        /* 0x004326CB */
        /* fld [ebx + 0B441Ch]; fsub [ebx + 0BFA08h]: the heading the tape centres on */
        static const BYTE kTapeHeading[] = { 0xD9, 0x83, 0x1C, 0x44, 0x0B, 0x00, 0xD8, 0xA3, 0x08, 0xFA, 0x0B, 0x00 };
        /* fstp [ebx + 0B4418h]: the rendered view's angles (pitch) as CG copies them */
        static const BYTE kViewAngles[] = { 0xD9, 0x9B, 0x18, 0x44, 0x0B, 0x00 };   /* 0x0045648C */
        static const BYTE kTapeCase[] = { 0xD9, 0x45, 0x48, 0x6A, 0x01, 0x57, 0x51, 0xD9, 0x1C, 0x24, 0x50, 0x56,
                                          0x8D, 0x45, 0x0C, 0x50, 0x8B, 0xD1, 0x53, 0x8D, 0x4D, 0xE8, 0xE8, 0x59,
                                          0x80, 0xFD, 0xFF, 0x83, 0xC4, 0x1C };      /* 0x0045A13C */
        g_hudTapeOk = Readable((void *)(DWORD_PTR)HUD_TAPE_VA, sizeof(kTape)) &&
                      !memcmp((void *)(DWORD_PTR)HUD_TAPE_VA, kTape, sizeof(kTape)) &&
                      Readable((void *)(DWORD_PTR)0x00432276u, sizeof(kTapeStretch)) &&
                      !memcmp((void *)(DWORD_PTR)0x00432276u, kTapeStretch, sizeof(kTapeStretch)) &&
                      Readable((void *)(DWORD_PTR)0x004323AAu, sizeof(kTapeIcons)) &&
                      !memcmp((void *)(DWORD_PTR)0x004323AAu, kTapeIcons, sizeof(kTapeIcons)) &&
                      Readable((void *)(DWORD_PTR)0x004326CBu, sizeof(kTapeScale)) &&
                      !memcmp((void *)(DWORD_PTR)0x004326CBu, kTapeScale, sizeof(kTapeScale)) &&
                      Readable((void *)(DWORD_PTR)0x004322B3u, sizeof(kTapeHeading)) &&
                      !memcmp((void *)(DWORD_PTR)0x004322B3u, kTapeHeading, sizeof(kTapeHeading)) &&
                      Readable((void *)(DWORD_PTR)0x0045A13Cu, sizeof(kTapeCase)) &&
                      !memcmp((void *)(DWORD_PTR)0x0045A13Cu, kTapeCase, sizeof(kTapeCase));
        g_hudScoresOk = Readable((void *)(DWORD_PTR)HUD_SCORES_VA, sizeof(kScores)) &&
                        !memcmp((void *)(DWORD_PTR)HUD_SCORES_VA, kScores, sizeof(kScores)) &&
                        Readable((void *)(DWORD_PTR)0x0046541Eu, sizeof(kScoreRow)) &&
                        !memcmp((void *)(DWORD_PTR)0x0046541Eu, kScoreRow, sizeof(kScoreRow));
        g_hudFireDvar = ((RegStr)(DWORD_PTR)HUD_STYLE_REGISTER_STRING_VA)(
            HUD_FIRE_DVAR, "", 0, "HUD styles' fire-mode label of the weapon in hand (read only)");
        g_hudAssistsDvar = C1RegisterInt(HUD_ASSISTS_DVAR, 0, 0, 9999, 0, "HUD styles' assists of the local player (read only)");
        g_hudSwayOk = Readable((void *)(DWORD_PTR)0x0045648Cu, sizeof(kViewAngles)) &&
                      !memcmp((void *)(DWORD_PTR)0x0045648Cu, kViewAngles, sizeof(kViewAngles));
        g_hudSwayDvar = C1RegisterInt(HUD_SWAY_DVAR, 1, 0, 1, 0x1,
                                      "HUD sway, every style: the HUD moves with the view, the player's movement and camera shake (0 off)");
        g_hudSwayXDvar = C1RegisterInt(HUD_SWAY_X_DVAR, 0, -10000, 10000, 0, "HUD sway, x in hundredths of a 640x480 unit (read only)");
        g_hudSwayYDvar = C1RegisterInt(HUD_SWAY_Y_DVAR, 0, -10000, 10000, 0, "HUD sway, y in hundredths of a 640x480 unit (read only)");
        Log("hud style: clip / akimbo / fire type sites %s, score rows %s, health %s, weapon icon %s, compass tape %s, "
            "sway %s, CG_OwnerDraw hook %s",
            g_hudClipOk ? "ok" : "differ (clip count, gauge and fire type stay empty)",
            g_hudScoresOk ? "ok" : "differ (assists stay 0)",
            g_hudHealthOk ? "ok" : "differs (the health fill stays empty)",
            g_hudIconOk ? (g_hudIconAltOk ? "ok" : "ok, fallback icon site differs (no icon for weapons without a HUD icon)")
                        : "differs (the weapon icon is not drawn)",
            g_hudTapeOk ? "ok" : "differs (the SM2 compass strip is not drawn)",
            g_hudSwayOk ? "ok" : "view site differs (the HUD sways without camera shake)",
            g_voiceHudOk ? "shared with voice.c" : "missing (clip count and gauge are not drawn)");
    }
    g_hudStyleDvar = C1RegisterInt(HUD_STYLE_DVAR, 0, 0, HUD_STYLE_MAX, 0x1,
                                   "HUD style: 0 stock 1.1.1.13, 1 1.2.1.12, 2 2.6.9.7, 3 3.3.3.3, 4 3.5.2.6, 5 SM2 (cg_hudStyleList: the ones in this build)");
    ((RegStr)(DWORD_PTR)HUD_STYLE_REGISTER_STRING_VA)(HUD_STYLE_LIST_DVAR, HUD_STYLE_LIST, 0,
                                                      "HUD styles in this build as number:id pairs (read only)");
    if (!g_hudStyleDvar) {
        Log("hud style: %s could not be registered; HUD stays stock", HUD_STYLE_DVAR);
        return;
    }
    g_hudStyleMenuDvar = C1RegisterInt(HUD_STYLE_MENU_DVAR, 0, 0, HUD_STYLE_MAX, 0,
                                       "Old name of cg_hudStyle, read by the native HUD menus (read only: set cg_hudStyle)");
    /* C1 does not apply "+set" from the command line to a dvar registered this late (voice.c): re-issue ours. */
    int v = C1DvarInt(g_hudStyleDvar, 0);
    const char *cl = GetCommandLineA();
    const char *at = cl ? strstr(cl, "+set " HUD_STYLE_DVAR " ") : NULL;
    size_t skip = sizeof("+set " HUD_STYLE_DVAR " ") - 1;
    if (!at && cl && (at = strstr(cl, HUD_STYLE_OLD_ARG)) != NULL) skip = sizeof(HUD_STYLE_OLD_ARG) - 1;
    if (at) {
        char cmd[64];
        at += skip;
        v = atoi(at[0] == '"' ? at + 1 : at);
        _snprintf(cmd, sizeof(cmd), "set %s %d\n", HUD_STYLE_DVAR, v);
        cmd[sizeof(cmd) - 1] = 0;
        C1CbufAddText(cmd);
    }
    Log("hud style: %s registered (%s %s), value %d%s", HUD_STYLE_DVAR, HUD_STYLE_LIST_DVAR, HUD_STYLE_LIST, v,
        at ? " from the command line" : "");
}

/* Main thread, every frame (before the frame's menus paint): the HUD sway (every style) from the predicted playerState's view
 * angles (cg + 0x14C) and velocity (cg + 0x28), the rendered view's shake (cg + 0xB4418 minus the former) and the
 * health (cg + 0x190). Off or outside a match: 0. */
static void HudSwayTick(void)
{
    static struct HudSway sw;
    static LARGE_INTEGER last, freq;
    static int active;
    float x = 0.0f, y = 0.0f;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    float dt = last.QuadPart && freq.QuadPart ? (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart)
                                              : 0.0f;
    last = now;
    const BYTE *cg = C1DvarInt(g_hudSwayDvar, 0) ? HudCg() : NULL;
    if (cg && Readable(cg, 0x194) && (!g_hudSwayOk || Readable(cg + HUD_CG_VIEWANGLES, 12))) {
        const float *ang = (const float *)(cg + 0x14C), *vel = (const float *)(cg + 0x28);
        float shakeP = 0.0f, shakeY = 0.0f;
        if (g_hudSwayOk) {
            const float *view = (const float *)(cg + HUD_CG_VIEWANGLES);
            shakeP = HudSwayAngleDelta(view[0], ang[0]);
            shakeY = HudSwayAngleDelta(view[1], ang[1]);
        }
        int alive = *(const int *)(cg + 0x190) > 0;
        HudSwayStep(&sw, dt, ang[1], ang[0], shakeY, shakeP, sqrtf(vel[0] * vel[0] + vel[1] * vel[1]), vel[2], alive,
                    &x, &y);
    } else {
        /* written once on the way out, so the dvars can be set by hand while the sway is off */
        memset(&sw, 0, sizeof(sw));
        if (!active) return;
    }
    active = cg != NULL;
    int ix = (int)(x * 100.0f), iy = (int)(y * 100.0f);
    if (g_hudSwayXDvar && *(volatile const int *)(g_hudSwayXDvar + 0xC) != ix) *(volatile int *)(g_hudSwayXDvar + 0xC) = ix;
    if (g_hudSwayYDvar && *(volatile const int *)(g_hudSwayYDvar + 0xC) != iy) *(volatile int *)(g_hudSwayYDvar + 0xC) = iy;
}

/* Main thread, every frame: register once the dvars are ready, publish the fire type and the assists while a
 * style is on, and keep the value inside the shipped list. */
static void HudStyleTick(void)
{
    static int lastBad = -1;
    static char lastFire[48];
    if (!g_hudStyleDone) {
        if (!C1DvarsReady()) return;
        HudStyleRegister();
    }
    if (!g_hudStyleDvar) return;
    int v = C1DvarInt(g_hudStyleDvar, 0);
    if (g_hudStyleMenuDvar && *(volatile const int *)(g_hudStyleMenuDvar + 0xC) != v)
        *(volatile int *)(g_hudStyleMenuDvar + 0xC) = v;
    HudSwayTick();
    if (v) {
        if (g_hudAssistsDvar) {
            int a = HudAssists();
            if (*(volatile const int *)(g_hudAssistsDvar + 0xC) != a) *(volatile int *)(g_hudAssistsDvar + 0xC) = a;
        }
        int rounds, permille;
        const BYTE *def = HudWeapon(&rounds, &permille);
        const char *key = def ? HudFireTypeKey(*(const int *)(def + 0x350), *(const int *)(def + 0xB64)) : "";
        if (g_hudFireDvar && strcmp(key, lastFire) && C1QueueSet(HUD_FIRE_DVAR, key)) {
            strncpy(lastFire, key, sizeof(lastFire) - 1);
            lastFire[sizeof(lastFire) - 1] = 0;
        }
    }
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

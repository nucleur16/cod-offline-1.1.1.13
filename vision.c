/*
 * Realtime vision editor (dev branch, -DK33_VISION). Included by kernel33.c after c1game.c and before
 * console.c, whose D3D Present/Reset hooks and WndProc carry it. The window itself is Dear ImGui
 * (vision_ui.cpp, C++); this file is the model: C1's vision state, .vision files, main-thread jobs and
 * input capture. Everything below was read from C1.exe 1.1.1.13 and is byte-checked at install.
 *
 * What a .vision file can set. CG_LoadVisionFile 0x479970 reads rawfile "vision/<name>.vision"
 * (0x479450: DB_FindXAssetHeader(0x24 rawfile), falling back to "vision/default.vision"), zeroes a
 * 0x70-byte visionSetVars_t, presets the two "derived" fields to -1e8, and parses it with 0x479620:
 * one "key value" per line, the key compared case-insensitively against visionDefFields 0xCD67D8
 * (21 entries {name, offset, type}; type 0 bool "%i", 1 float "%f", 2 vec3 "%f %f %f", applied by
 * 0x479540). Unknown keys print "WARNING: unknown dvar '%s' in file '%s'" and are skipped. After the
 * parse an absent desaturationDark (524) becomes desaturation (523) and an absent mediumTint (527)
 * becomes (lightTint + darkTint) / 2; the character light scales default to 1. C1 renamed every
 * dvar-named string to a number, so the keys are "511".."531"; the table's order and offsets are
 * IW4's visionDefFields, which gives the readable names (r_glow, r_glowBloomCutoff, ...). That is the
 * complete list: fog (setexpfog), sun and the r_*Tweak dvars are not vision-file keys in C1 (or IW4).
 *
 * How C1 applies a set. cg (ParityCg) holds, per type (0 naked, 1 night, 2 missilecam, 3 thermal,
 * 4 pain, 5 unnamed; server commands 'N'..'R' and '3' via 0x469b94.. -> CG_SetVisionSetTransition
 * 0x47A1E0(eax type, ecx name, duration)):
 *   +0x1733F8 [4] cache sets, +0x1735B8 [4][64] their names, +0x1736B8 [4] use times (0x479B00
 *            CG_LookupVisionSet(name, out): cache hit or CG_LoadVisionFile, LRU replace)
 *   +0x1736C8 [6] from, +0x173968 [6] to, +0x173C08 [6] current   (0x70 each)
 *   +0x173EA8 [6] lerp {start, duration, style, glowFade, fadeTime} (0x14 each; style 0 never set,
 *            1 steady, 2/3 transition)
 *   +0x173F20 [6][64] the names the server sent
 * 0x47A0E0 (every frame) lerps from->to into current while a transition runs (0x479E80) and in the
 * steady state only advances glowFade (current+0x6C), so a value written into current stays until C1
 * applies another set. 0x47A5D0 then picks the type (thermal, missilecam, nightvision, pain blended
 * with naked by cg+0xC8AF0, the sixth type with cg+4 == 7) and copies current[type] into the refdef
 * (cg+0xB03EC..0xB0440): film enable/brightness/contrast/desat/desatDark/invert/tints, glow
 * enable/cutoff/desat, bloom intensity 0 * glowFade, radius 0, and the character light scales (1 when
 * 529 is off). Bloom intensity 1, radius 1 and both sky bleed intensities are parsed and lerped but
 * never reach the refdef: they edit and save, but show nothing. r_filmUseTweaks ("1635") and
 * r_glowUseTweaks ("1634") make the renderer use the r_*Tweak dvars instead of the refdef.
 *
 * The editor writes a changed field into from, to and current of the edited type (so a running
 * transition ends on it), optionally into the cache entry of the same name, and nothing else. Loading
 * a set by name runs C1's own CG_SetVisionSetTransition on the main thread (the WndProc, from a posted
 * message), exactly as a VisionSetNaked command would. Vision state exists only in a match (cgame
 * with a snapshot); at the menu the editor still opens, loads/saves disk files and edits the buffer,
 * but writes nothing into C1.
 */

#ifndef COBJMACROS
#define COBJMACROS
#endif
#include "vision_api.h"

#define VIS_FIELDTABLE_VA   0x00CD67D8u
#define VIS_LOOKUP_VA       0x00479B00u
#define VIS_TRANSITION_VA   0x0047A1E0u
#define VIS_SELECT_REF_VA   0x0047A6B5u  /* lea eax, [eax+esi+0x173C08] in 0x47A5D0 */
#define VIS_UPDATE_REF_VA   0x0047A127u  /* lea esi, [ebx+0x173968]; lea edi, [ebx+0x173EA8] in 0x47A0E0 */
#define VIS_REFDEF_REF_VA   0x0047A6E7u  /* the refdef copy */
#define VIS_CG_CACHE        0x1733F8
#define VIS_CG_CACHE_NAMES  0x1735B8
#define VIS_CG_FROM         0x1736C8
#define VIS_CG_TO           0x173968
#define VIS_CG_CUR          0x173C08
#define VIS_CG_LERP         0x173EA8
#define VIS_CG_NAMES        0x173F20
#define VIS_CG_THERMAL      0x1743F8
#define VIS_CG_MISSILECAM   0x1743C4
#define VIS_CG_END          0x174400
#define VIS_CG_REFDEF       0xB03EC
#define VIS_LERP_SIZE       0x14
#define VIS_CACHE_COUNT     4
#define VIS_GLOWFADE        0x6C

/* DB asset hash (DB_FindXAssetEntry 0x4CB540): ushort heads [60000] at 0x18FD9D8, 24-byte entries at
 * 0x1C402C8 {type, ?, header*, ..., nextHash ushort +0x12}; readers hold the count at 0x2007A5C while
 * the writer flag 0x2007A60 is clear (DB_FindXAssetHeader 0x4CB953). A rawfile header starts with
 * its name. */
#define VIS_DB_HASH_VA      0x018FD9D8u
#define VIS_DB_HASH_SIZE    60000
#define VIS_DB_ENTRIES_VA   0x01C402C8u
#define VIS_DB_READERS_VA   0x02007A5Cu
#define VIS_DB_WRITER_VA    0x02007A60u
#define VIS_ASSET_RAWFILE   0x24

#define VIS_DVAR_FIND_VA    0x006232C0u  /* Dvar_FindVar(name) (console.c notes) */
#define VIS_CMD_ARGS_VA     0x024F63A0u  /* Cmd_Argc/Argv: nesting index; argc [+0x44+i*4], argv [+0x64+i*4] */
#define VIS_JOB_MSG         (WM_APP + 0x533)

static const VisFieldInfo kVisFields[VIS_FIELDS] = {
    { "511", "r_glow", "Glow", 0x00, VIS_T_BOOL, VIS_G_GLOW, 0, 1, 1, "Enable glow (bloom) for this vision set." },
    { "512", "r_glowBloomCutoff", "Bloom cutoff", 0x04, VIS_T_FLOAT, VIS_G_GLOW, 0, 1, 1, "Brightness fraction above which pixels bloom." },
    { "513", "r_glowBloomDesaturation", "Bloom desaturation", 0x08, VIS_T_FLOAT, VIS_G_GLOW, 0, 1, 1, "Desaturation of the bloom." },
    { "514", "r_glowBloomIntensity0", "Bloom intensity", 0x0C, VIS_T_FLOAT, VIS_G_GLOW, 0, 20, 1, "Bloom intensity (C1 multiplies it by the glow fade)." },
    { "515", "r_glowBloomIntensity1", "Bloom intensity 1", 0x10, VIS_T_FLOAT, VIS_G_GLOW, 0, 20, 0, "Parsed and lerped, never copied to the refdef by C1." },
    { "516", "r_glowRadius0", "Glow radius", 0x14, VIS_T_FLOAT, VIS_G_GLOW, 0, 32, 1, "Glow radius in pixels at 640x480." },
    { "517", "r_glowRadius1", "Glow radius 1", 0x18, VIS_T_FLOAT, VIS_G_GLOW, 0, 32, 0, "Parsed and lerped, never copied to the refdef by C1." },
    { "518", "r_glowSkyBleedIntensity0", "Sky bleed intensity 0", 0x1C, VIS_T_FLOAT, VIS_G_GLOW, 0, 1, 0, "Parsed and lerped, never copied to the refdef by C1." },
    { "519", "r_glowSkyBleedIntensity1", "Sky bleed intensity 1", 0x20, VIS_T_FLOAT, VIS_G_GLOW, 0, 1, 0, "Parsed and lerped, never copied to the refdef by C1." },
    { "520", "r_filmEnable", "Film", 0x24, VIS_T_BOOL, VIS_G_FILM, 0, 1, 1, "Enable film colour (contrast, brightness, desaturation, tints)." },
    { "521", "r_filmBrightness", "Brightness", 0x28, VIS_T_FLOAT, VIS_G_FILM, -1, 1, 1, "Film colour brightness." },
    { "522", "r_filmContrast", "Contrast", 0x2C, VIS_T_FLOAT, VIS_G_FILM, 0, 4, 1, "Film colour contrast." },
    { "523", "r_filmDesaturation", "Desaturation", 0x30, VIS_T_FLOAT, VIS_G_FILM, -2, 2, 1, "Desaturation of light areas." },
    { "524", "r_filmDesaturationDark", "Desaturation (dark)", 0x34, VIS_T_FLOAT, VIS_G_FILM, -2, 2, 1, "Desaturation of dark areas (a file without it uses Desaturation)." },
    { "525", "r_filmInvert", "Invert", 0x38, VIS_T_BOOL, VIS_G_FILM, 0, 1, 1, "Invert the picture." },
    { "526", "r_filmLightTint", "Light tint", 0x3C, VIS_T_VEC3, VIS_G_FILM, 0, 2, 1, "Tint of light areas (0..2 per channel)." },
    { "527", "r_filmMediumTint", "Medium tint", 0x48, VIS_T_VEC3, VIS_G_FILM, 0, 2, 1, "Tint of mid tones (a file without it uses the light/dark average)." },
    { "528", "r_filmDarkTint", "Dark tint", 0x54, VIS_T_VEC3, VIS_G_FILM, 0, 2, 1, "Tint of dark areas (0..2 per channel)." },
    { "529", "r_primaryLightUseTweaks", "Character light tweaks", 0x60, VIS_T_BOOL, VIS_G_CHAR, 0, 1, 1, "Use the two scales below for characters' primary light (off: 1, 1)." },
    { "530", "r_primaryLightTweakDiffuseStrength", "Diffuse scale", 0x64, VIS_T_FLOAT, VIS_G_CHAR, 0, 10, 1, "Characters' primary light diffuse scale." },
    { "531", "r_primaryLightTweakSpecularStrength", "Specular scale", 0x68, VIS_T_FLOAT, VIS_G_CHAR, 0, 10, 1, "Characters' primary light specular scale." },
};

/* File layout, as C1's shipped files (e.g. zones/usermaps_patch/rawfile/vision/mp_terminal_cls.vision):
 * IW's key order and groups, a single space on the separator lines, LF line ends, and the whitespace
 * of IW's aligned files (value quote at column 26 of the IW name, 37 in the character-light group)
 * left as it was when C1 swapped the names for numbers (mp_killhouse_sh's file matches byte for byte). The four keys no shipped file has are written only when non-zero. */
static const struct { int field; int optional; } kVisFileOrder[] = {
    { 0, 0 }, { 5, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 1 }, { 6, 1 }, { 7, 1 }, { 8, 1 },
    { -1, 0 },
    { 9, 0 }, { 11, 0 }, { 10, 0 }, { 12, 0 }, { 13, 0 }, { 14, 0 }, { 15, 0 }, { 16, 0 }, { 17, 0 },
    { -1, 0 },
    { 18, 0 }, { 19, 0 }, { 20, 0 },
};

static const char *const kVisModeLabels[VIS_MODES] = { "naked", "night", "missilecam", "thermal", "pain", "type 5 (unnamed)" };
static const char *const kVisGroupLabels[VIS_GROUPS] = { "Glow / bloom", "Film colour", "Character primary light" };

static CRITICAL_SECTION g_visLock;     /* the model below */
static CRITICAL_SECTION g_visGuiLock;  /* ImGui and its DX9 objects (render thread, Reset, CreateDevice) */
static BOOL g_visReady;
static volatile LONG g_visOpen;
static volatile LONG g_visWantText;
static BYTE g_visEdit[VIS_SET_BYTES], g_visLoaded[VIS_SET_BYTES];
static char g_visSetName[VIS_NAME_LEN] = "";
static char g_visSource[200] = "nothing loaded";
static int g_visDirty, g_visEditMode = -1, g_visKeepCache;
static char g_visSyncName[VIS_NAME_LEN];   /* the edited type's name/lerp start when the buffer was synced */
static int g_visSyncStart = -1, g_visSyncMode = -1, g_visGameChanged;
static char g_visStatus[240];
static DWORD g_visStatusTick;
static char g_visUnkKey[VIS_MAX_UNKNOWN][64], g_visUnkVal[VIS_MAX_UNKNOWN][128];
static int g_visUnkCount;
static const BYTE *g_visDvFilmTweaks, *g_visDvGlowTweaks;
static BOOL g_visDvarsLooked;
static const BYTE *volatile g_visKeyDvar;
static BOOL g_visKeyDvarTried;
static volatile LONG g_visShot;            /* k33vision shot: tick | 1 */
static volatile LONG g_visShotNoUi;

static struct { int kind; char name[VIS_NAME_LEN]; int mode, duration; } g_visJob; /* main thread, latest wins */
static volatile LONG g_visJobPending;

#define VIS_INPUT_RING 256
static VisInputEvent g_visInput[VIS_INPUT_RING];
static int g_visInHead, g_visInTail;
static BYTE g_visMb[4];
static int g_visMbLen;

/* ---- small helpers ---------------------------------------------------------------------------------- */

static void VisStatus(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void VisStatus(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(g_visStatus, sizeof(g_visStatus) - 1, fmt, ap);
    va_end(ap);
    g_visStatus[sizeof(g_visStatus) - 1] = 0;
    g_visStatusTick = GetTickCount();
    Log("vision: %s", g_visStatus);
}

static BYTE *VisCg(void)
{
    BYTE *cg = ParityCg();
    if (!cg || !Readable(cg + VIS_CG_CACHE, VIS_CG_END - VIS_CG_CACHE) || !Readable(cg + VIS_CG_REFDEF, 0x58)) return NULL;
    return cg;
}

static BOOL VisCgInMatch(const BYTE *cg) { return cg && *(void *const volatile *)(cg + CG_SNAP) != NULL; }

static BYTE *VisSet(BYTE *cg, int base, int mode) { return cg + base + mode * VIS_SET_BYTES; }
static int *VisLerp(BYTE *cg, int mode) { return (int *)(cg + VIS_CG_LERP + mode * VIS_LERP_SIZE); }

static int VisFieldSize(int type) { return type == VIS_T_VEC3 ? 12 : type == VIS_T_FLOAT ? 4 : 1; }

static const VisFieldInfo *VisFindField(const char *key)
{
    for (int i = 0; i < VIS_FIELDS; i++)
        if (!_stricmp(key, kVisFields[i].id) || !_stricmp(key, kVisFields[i].iwName)) return &kVisFields[i];
    return NULL;
}

static void VisDefaults(BYTE *set)
{
    memset(set, 0, VIS_SET_BYTES);
    *(float *)(set + 0x64) = 1.0f;
    *(float *)(set + 0x68) = 1.0f;
}

/* The nightvision test of 0x47A290, replicated (C1 calls it only from 0x47A5D0). */
static BOOL VisNightActive(BYTE *cg)
{
    if (*(volatile int *)(DWORD_PTR)0x016FC2E8u < 9) return FALSE;
    const BYTE *dv = *(const BYTE *const *)(DWORD_PTR)0x016EC90Cu;
    if (!dv || !Readable(dv, 0x10) || dv[0xC]) return FALSE;
    if (*(int *)(cg + 0xC8B60) && *(int *)(cg + 0xC8B70)) return FALSE;
    DWORD flags = *(DWORD *)(cg + 0x344);
    int weapon = (flags & 2) ? *(int *)(cg + 0x328) : *(int *)(cg + 0x33C);
    if (cg[VIS_CG_THERMAL] || cg[VIS_CG_MISSILECAM]) return FALSE;
    if (weapon < 0 || weapon >= 4096) return FALSE;
    const BYTE *const *defs = (const BYTE *const *)(DWORD_PTR)0x0150D720u;
    if (!Readable(defs + weapon, 4)) return FALSE;
    const BYTE *wd = defs[weapon];
    if (!wd || !Readable(wd + 0x63C, 0x18)) return FALSE;
    int state = *(int *)(cg + 0x278), wtime = *(int *)(cg + 0x26C);
    if (state == 0x1D) return *(const int *)(wd + 0x63C) - wtime >= *(const int *)(wd + 0x644);
    if (state == 0x1E) return *(const int *)(wd + 0x648) - wtime <= *(const int *)(wd + 0x64C);
    return (flags & 0x40) != 0;
}

/* 0x47A5D0's choice of vision type, replicated. */
static int VisActiveMode(BYTE *cg)
{
    if (!cg) return -1;
    if (*(int *)(cg + 0xC8B60) && *(int *)(cg + 0xC8B70)) return 0;
    if (*(int *)(cg + 4) == 7 && VisLerp(cg, 5)[2]) return 5;
    if (cg[VIS_CG_THERMAL]) return 3;
    if (cg[VIS_CG_MISSILECAM]) return 2;
    if (VisNightActive(cg)) return 1;
    if (cg[0xC8AEC] && VisLerp(cg, 4)[2]) return 4;
    return 0;
}

static int VisResolvedMode(BYTE *cg)
{
    if (g_visEditMode >= 0 && g_visEditMode < VIS_MODES) return g_visEditMode;
    int m = VisActiveMode(cg);
    return m < 0 ? 0 : m;
}

/* The set C1 is heading for: to[] during a transition, current[] otherwise. */
static const BYTE *VisGameSet(BYTE *cg, int mode)
{
    int style = VisLerp(cg, mode)[2];
    return style >= 2 ? VisSet(cg, VIS_CG_TO, mode) : VisSet(cg, VIS_CG_CUR, mode);
}

static void VisNoteSync(BYTE *cg, int mode)
{
    g_visSyncMode = mode;
    g_visSyncStart = VisLerp(cg, mode)[0];
    lstrcpynA(g_visSyncName, (const char *)cg + VIS_CG_NAMES + mode * VIS_NAME_LEN, VIS_NAME_LEN);
    g_visGameChanged = 0;
}

/* Copy one field (or all, field < 0) from src into the edited type's from/to/current (+ cache). Lock held. */
static void VisWriteGame(BYTE *cg, int mode, const BYTE *src, int field)
{
    for (int i = 0; i < VIS_FIELDS; i++) {
        if (field >= 0 && i != field) continue;
        int off = kVisFields[i].offset, n = VisFieldSize(kVisFields[i].type);
        memcpy(VisSet(cg, VIS_CG_FROM, mode) + off, src + off, n);
        memcpy(VisSet(cg, VIS_CG_TO, mode) + off, src + off, n);
        memcpy(VisSet(cg, VIS_CG_CUR, mode) + off, src + off, n);
        if (g_visKeepCache && g_visSetName[0])
            for (int c = 0; c < VIS_CACHE_COUNT; c++)
                if (!_stricmp((const char *)cg + VIS_CG_CACHE_NAMES + c * VIS_NAME_LEN, g_visSetName))
                    memcpy(VisSet(cg, VIS_CG_CACHE, c) + off, src + off, n);
    }
    int *lerp = VisLerp(cg, mode);
    if (lerp[2] == 0) { /* never set: C1 draws no film/glow for this type until it is steady */
        lerp[2] = 1;
        *(float *)&lerp[3] = 1.0f;
        lerp[4] = *(int *)(cg + CG_TIME);
        *(float *)(VisSet(cg, VIS_CG_CUR, mode) + VIS_GLOWFADE) = 1.0f;
    }
}

static BOOL VisApplyLocked(int field)
{
    BYTE *cg = VisCg();
    if (!VisCgInMatch(cg)) return FALSE;
    int mode = VisResolvedMode(cg);
    VisWriteGame(cg, mode, g_visEdit, field);
    return TRUE;
}

/* Re-read the edited type from C1 into both buffers. Lock held. */
static BOOL VisRereadLocked(void)
{
    BYTE *cg = VisCg();
    if (!VisCgInMatch(cg)) return FALSE;
    int mode = VisResolvedMode(cg);
    memcpy(g_visEdit, VisGameSet(cg, mode), VIS_SET_BYTES);
    memcpy(g_visLoaded, g_visEdit, VIS_SET_BYTES);
    const char *n = (const char *)cg + VIS_CG_NAMES + mode * VIS_NAME_LEN;
    lstrcpynA(g_visSetName, n[0] ? n : (mode == 0 ? "default" : ""), VIS_NAME_LEN);
    _snprintf(g_visSource, sizeof(g_visSource), "C1's %s set (%s)", kVisModeLabels[mode],
              VisLerp(cg, mode)[2] ? "applied" : "never set: zeros");
    g_visSource[sizeof(g_visSource) - 1] = 0;
    g_visDirty = 0;
    g_visUnkCount = 0;
    VisNoteSync(cg, mode);
    return TRUE;
}

/* ---- dvars ------------------------------------------------------------------------------------------ */

static const BYTE *VisFindDvar(const char *name)
{
    typedef const BYTE *(__cdecl *FindFn)(const char *);
    static const BYTE kFind[] = { 0x55, 0x8B, 0xEC };
    if (!Readable((void *)(DWORD_PTR)VIS_DVAR_FIND_VA, 3) || memcmp((void *)(DWORD_PTR)VIS_DVAR_FIND_VA, kFind, 3)) return NULL;
    const BYTE *d = ((FindFn)(DWORD_PTR)VIS_DVAR_FIND_VA)(name);
    return d && Readable(d, 0x10) ? d : NULL;
}

static int VisDvarBool(const BYTE *d) { return d && Readable(d, 0x10) ? d[0xC] != 0 : 0; }

/* ---- the DB: which vision files the loaded zones carry ----------------------------------------------- */

static BOOL g_visDbOk;

static int VisDbWalk(char (*names)[VIS_NAME_LEN], int max, const char *want)
{
    if (!g_visDbOk) return -1;
    const unsigned short *hash = (const unsigned short *)(DWORD_PTR)VIS_DB_HASH_VA;
    int count = 0, found = 0;
    InterlockedIncrement((volatile LONG *)(DWORD_PTR)VIS_DB_READERS_VA);
    for (int spin = 0; *(volatile LONG *)(DWORD_PTR)VIS_DB_WRITER_VA && spin < 200; spin++) Sleep(1);
    if (*(volatile LONG *)(DWORD_PTR)VIS_DB_WRITER_VA) {
        InterlockedDecrement((volatile LONG *)(DWORD_PTR)VIS_DB_READERS_VA);
        return -1;
    }
    for (int h = 0; h < VIS_DB_HASH_SIZE && !found; h++) {
        unsigned idx = hash[h];
        for (int guard = 0; idx && guard < 256; guard++) {
            const BYTE *e = (const BYTE *)(DWORD_PTR)(VIS_DB_ENTRIES_VA + idx * 24u);
            if (!Readable(e, 24)) break;
            if (*(const int *)e == VIS_ASSET_RAWFILE) {
                const BYTE *hdr = *(const BYTE *const *)(e + 8);
                const char *n = hdr && Readable(hdr, 4) ? *(const char *const *)hdr : NULL;
                if (n && Readable(n, 8) && !_strnicmp(n, "vision/", 7)) {
                    char base[VIS_NAME_LEN];
                    lstrcpynA(base, n + 7, VIS_NAME_LEN);
                    int len = lstrlenA(base);
                    if (len > 7 && !_stricmp(base + len - 7, ".vision")) {
                        base[len - 7] = 0;
                        if (want) {
                            if (!_stricmp(base, want)) found = 1;
                        } else {
                            int dup = 0;
                            for (int k = 0; k < count && !dup; k++) dup = !_stricmp(names[k], base);
                            if (!dup && count < max) lstrcpynA(names[count++], base, VIS_NAME_LEN);
                        }
                    }
                }
            }
            idx = *(const unsigned short *)(e + 0x12);
        }
    }
    InterlockedDecrement((volatile LONG *)(DWORD_PTR)VIS_DB_READERS_VA);
    return want ? found : count;
}

static int VisCmpName(const void *a, const void *b) { return _stricmp((const char *)a, (const char *)b); }

/* ---- .vision text ------------------------------------------------------------------------------------ */

static const char *VisExeDir(void)
{
    static char dir[MAX_PATH];
    if (!dir[0]) {
        DWORD n = GetModuleFileNameA(NULL, dir, MAX_PATH);
        while (n && dir[n - 1] != '\\') n--;
        dir[n] = 0;
    }
    return dir;
}

static const char *VisSaveDirPath(void)
{
    static char dir[MAX_PATH];
    if (!dir[0]) _snprintf(dir, MAX_PATH - 1, "%smain\\vision\\", VisExeDir());
    return dir;
}

static BOOL VisSafeName(const char *name)
{
    if (!name || !name[0] || lstrlenA(name) >= VIS_NAME_LEN - 8) return FALSE;
    for (const char *p = name; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' || *p == '.') || (p[0] == '.' && p[1] == '.')) return FALSE;
    return TRUE;
}

static void VisFmtFloat(char *out, int n, float f)
{
    if (f == 0.0f) f = 0.0f; /* no "-0" */
    _snprintf(out, n, "%g", f);
    out[n - 1] = 0;
}

static void VisFmtValue(const BYTE *set, const VisFieldInfo *f, char *out, int n)
{
    const float *v = (const float *)(set + f->offset);
    char a[32], b[32], c[32];
    switch (f->type) {
    case VIS_T_BOOL: _snprintf(out, n, "%d", set[f->offset] ? 1 : 0); break;
    case VIS_T_FLOAT: VisFmtFloat(out, n, v[0]); break;
    default:
        VisFmtFloat(a, sizeof(a), v[0]);
        VisFmtFloat(b, sizeof(b), v[1]);
        VisFmtFloat(c, sizeof(c), v[2]);
        _snprintf(out, n, "%s %s %s", a, b, c);
        break;
    }
    out[n - 1] = 0;
}

/* C1's file format (LF line ends, a single space on the group separator lines, as shipped). Lock held. */
static int VisFormatLocked(const BYTE *set, char *buf, int size, BOOL withUnknown)
{
    int o = 0;
    for (unsigned k = 0; k < sizeof(kVisFileOrder) / sizeof(kVisFileOrder[0]) && o < size - 1; k++) {
        if (kVisFileOrder[k].field < 0) { o += _snprintf(buf + o, size - o, " \n"); continue; }
        const VisFieldInfo *f = &kVisFields[kVisFileOrder[k].field];
        if (kVisFileOrder[k].optional) {
            float v = *(const float *)(set + f->offset);
            if (v == 0.0f) continue;
        }
        char val[96];
        VisFmtValue(set, f, val, sizeof(val));
        int pad = (f->group == VIS_G_CHAR ? 37 : 26) - lstrlenA(f->iwName);
        if (pad < 1) pad = 1;
        o += _snprintf(buf + o, size - o, "%s%*s\"%s\"\n", f->id, pad, "", val);
        if (o < 0) { o = size - 1; break; }
    }
    if (withUnknown && g_visUnkCount && o < size - 1) {
        o += _snprintf(buf + o, size - o, " \n");
        for (int i = 0; i < g_visUnkCount && o > 0 && o < size - 1; i++) {
            int pad = 30 - lstrlenA(g_visUnkKey[i]);
            o += _snprintf(buf + o, size - o, "%s%*s\"%s\"\n", g_visUnkKey[i], pad < 1 ? 1 : pad, "", g_visUnkVal[i]);
        }
    }
    if (o < 0 || o >= size) o = size - 1;
    buf[o] = 0;
    return o;
}

/* Com_Parse-style token: quoted string or a run of non-space; // and block comments skipped. */
static const char *VisToken(const char *p, char *out, int n, BOOL crossLines)
{
    out[0] = 0;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || (crossLines && *p == '\n')) p++;
        if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }
        break;
    }
    if (!*p || *p == '\n') return p;
    int k = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"' && *p != '\n') { if (k < n - 1) out[k++] = *p; p++; }
        if (*p == '"') p++;
    } else {
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') { if (k < n - 1) out[k++] = *p; p++; }
    }
    out[k] = 0;
    return p;
}

/* As CG_LoadVisionFile: zeroed set, the derived fields, the per-type sscanf of 0x479540. Lock held. */
static int VisParseLocked(const char *text, BYTE *set, int *unknownOut, int *malformedOut)
{

    int keys = 0, unknown = 0, malformed = 0;
    memset(set, 0, VIS_SET_BYTES);
    *(float *)(set + 0x34) = -100000000.0f;
    *(float *)(set + 0x48) = -100000000.0f;
    *(float *)(set + 0x64) = 1.0f;
    *(float *)(set + 0x68) = 1.0f;
    g_visUnkCount = 0;
    const char *p = text;
    while (*p) {
        char key[64], val[128];
        p = VisToken(p, key, sizeof(key), TRUE);
        if (!key[0]) { if (*p) p++; continue; }
        p = VisToken(p, val, sizeof(val), FALSE);
        while (*p && *p != '\n') p++;
        const VisFieldInfo *f = VisFindField(key);
        if (!f) {
            unknown++;
            if (g_visUnkCount < VIS_MAX_UNKNOWN) {
                lstrcpynA(g_visUnkKey[g_visUnkCount], key, 64);
                lstrcpynA(g_visUnkVal[g_visUnkCount], val, 128);
                g_visUnkCount++;
            }
            continue;
        }

        float a, b, c;
        int iv;
        BOOL ok = FALSE;
        if (f->type == VIS_T_BOOL && sscanf(val, "%i", &iv) == 1) { set[f->offset] = iv != 0; ok = TRUE; }
        else if (f->type == VIS_T_FLOAT && sscanf(val, "%f", &a) == 1) { *(float *)(set + f->offset) = a; ok = TRUE; }
        else if (f->type == VIS_T_VEC3 && sscanf(val, "%f %f %f", &a, &b, &c) == 3) {
            float *v = (float *)(set + f->offset);
            v[0] = a; v[1] = b; v[2] = c;
            ok = TRUE;
        }
        if (ok) keys++; else malformed++;
    }
    if (*(float *)(set + 0x34) == -100000000.0f) *(float *)(set + 0x34) = *(float *)(set + 0x30);
    if (*(float *)(set + 0x48) == -100000000.0f)
        for (int i = 0; i < 3; i++) ((float *)(set + 0x48))[i] = (((float *)(set + 0x54))[i] + ((float *)(set + 0x3C))[i]) * 0.5f;
    if (unknownOut) *unknownOut = unknown;
    if (malformedOut) *malformedOut = malformed;
    return keys;
}

/* ---- main-thread jobs --------------------------------------------------------------------------------- */

static HWND VisGameHwnd(void) { return *(HWND volatile *)(DWORD_PTR)MAIN_HWND_VA; }

static void VisPostJob(void)
{
    InterlockedExchange(&g_visJobPending, 1);
    HWND h = VisGameHwnd();
    if (h) PostMessageA(h, VIS_JOB_MSG, 0, 0);
}

static unsigned char VisCallTransition(int mode, const char *name, int duration)
{
    int eax = mode;
    const char *ecx = name;
    DWORD fn = VIS_TRANSITION_VA;
    __asm__ volatile("pushl %[d]\n\tcall *%[fn]\n\taddl $4, %%esp"
                     : "+a"(eax), "+c"(ecx)
                     : [d] "r"(duration), [fn] "r"(fn)
                     : "edx", "memory", "cc");
    return (unsigned char)eax;
}

/* Main thread (the WndProc, or a k33vision command): load a set by name the way a VisionSet command does. */
static void VisLoadZoneNow(const char *name, int mode, int duration)
{
    BYTE *cg = VisCg();
    if (!VisCgInMatch(cg)) { VisStatus("not in a match: C1 keeps no vision state at the menu; load a disk file instead"); return; }
    int has = VisDbWalk(NULL, 0, name);
    if (has == 0) { VisStatus("vision/%s.vision is in no loaded zone (C1 would fall back to default)", name); return; }
    if (mode < 0) mode = VisResolvedMode(cg);
    char nm[VIS_NAME_LEN];
    lstrcpynA(nm, name, VIS_NAME_LEN);
    lstrcpynA((char *)cg + VIS_CG_NAMES + mode * VIS_NAME_LEN, nm, VIS_NAME_LEN);
    unsigned char ok = VisCallTransition(mode, (char *)cg + VIS_CG_NAMES + mode * VIS_NAME_LEN, duration < 0 ? 0 : duration);
    EnterCriticalSection(&g_visLock);
    if (g_visEditMode < 0 || g_visEditMode == mode) {
        int keep = g_visEditMode;
        g_visEditMode = mode;
        VisRereadLocked();
        g_visEditMode = keep;
        lstrcpynA(g_visSetName, nm, VIS_NAME_LEN);
        _snprintf(g_visSource, sizeof(g_visSource), "zone rawfile vision/%s.vision, applied to %s by C1 (%d ms)", nm,
                  kVisModeLabels[mode], duration < 0 ? 0 : duration);
        g_visSource[sizeof(g_visSource) - 1] = 0;
    }
    LeaveCriticalSection(&g_visLock);
    VisStatus("loaded %s into the %s set%s", nm, kVisModeLabels[mode], ok ? "" : " (C1 reported a failure)");
}

static void VisRunJobs(void)
{
    if (!InterlockedExchange(&g_visJobPending, 0)) return;
    char name[VIS_NAME_LEN];
    int kind, mode, duration;
    EnterCriticalSection(&g_visLock);
    kind = g_visJob.kind;
    mode = g_visJob.mode;
    duration = g_visJob.duration;
    lstrcpynA(name, g_visJob.name, VIS_NAME_LEN);
    g_visJob.kind = 0;
    LeaveCriticalSection(&g_visLock);
    if (kind == 1) VisLoadZoneNow(name, mode, duration);
}

/* ---- the API for vision_ui.cpp (render thread) ------------------------------------------------------- */

const VisFieldInfo *VisApi_Field(int i) { return i >= 0 && i < VIS_FIELDS ? &kVisFields[i] : NULL; }
const char *VisApi_GroupLabel(int g) { return g >= 0 && g < VIS_GROUPS ? kVisGroupLabels[g] : ""; }
const char *VisApi_ModeLabel(int m) { return m >= 0 && m < VIS_MODES ? kVisModeLabels[m] : "?"; }
const char *VisApi_SaveDir(void) { return VisSaveDirPath(); }
HWND VisApi_Hwnd(void) { return VisGameHwnd(); }
void VisApi_SetWantText(int on) { InterlockedExchange(&g_visWantText, on); }

const char *VisApi_IniPath(void)
{
    static char p[MAX_PATH];
    if (!p[0]) _snprintf(p, MAX_PATH - 1, "%smain\\k33_vision_editor.ini", VisExeDir());
    return p;
}

int VisApi_ToggleKey(void) { return g_visKeyDvar ? *(volatile const int *)(g_visKeyDvar + 0xC) : VK_F3; }

void VisApi_GetState(VisUiState *s)
{
    memset(s, 0, sizeof(*s));
    s->ok = g_visReady;
    BYTE *cg = VisCg();
    EnterCriticalSection(&g_visLock);
    s->inMatch = VisCgInMatch(cg);
    s->activeMode = s->inMatch ? VisActiveMode(cg) : -1;
    s->autoMode = g_visEditMode < 0;
    s->editMode = s->inMatch ? VisResolvedMode(cg) : (g_visEditMode < 0 ? 0 : g_visEditMode);
    if (s->inMatch) {
        for (int m = 0; m < VIS_MODES; m++) {
            s->modeStyle[m] = VisLerp(cg, m)[2];
            lstrcpynA(s->modeName[m], (const char *)cg + VIS_CG_NAMES + m * VIS_NAME_LEN, VIS_NAME_LEN);
        }
        /* follow C1: a new set on the edited type replaces the buffer unless it has edits */
        int mode = s->editMode;
        int start = VisLerp(cg, mode)[0];
        const char *nm = (const char *)cg + VIS_CG_NAMES + mode * VIS_NAME_LEN;
        BOOL changed = mode != g_visSyncMode || start != g_visSyncStart || strncmp(nm, g_visSyncName, VIS_NAME_LEN) != 0;
        if (changed) {
            if (!g_visDirty || mode != g_visSyncMode) VisRereadLocked();
            else g_visGameChanged = 1;
        }
        if (!g_visDvarsLooked) {
            g_visDvarsLooked = TRUE; /* render thread: Dvar_FindVar only takes the reader lock */
            g_visDvFilmTweaks = VisFindDvar("1635");
            g_visDvGlowTweaks = VisFindDvar("1634");
        }
        s->filmTweaks = VisDvarBool(g_visDvFilmTweaks);
        s->glowTweaks = VisDvarBool(g_visDvGlowTweaks);
    } else {
        g_visSyncMode = -1;
    }
    lstrcpynA(s->setName, g_visSetName, VIS_NAME_LEN);
    lstrcpynA(s->source, g_visSource, sizeof(s->source));
    s->dirty = g_visDirty;
    s->gameChanged = g_visGameChanged;
    s->keepInCache = g_visKeepCache;
    s->pending = g_visJobPending != 0;
    s->unknownCount = g_visUnkCount;
    lstrcpynA(s->status, g_visStatus, sizeof(s->status));
    s->statusTick = g_visStatusTick;
    LeaveCriticalSection(&g_visLock);
}

void VisApi_GetValues(BYTE *edit, BYTE *loaded)
{
    EnterCriticalSection(&g_visLock);
    if (edit) memcpy(edit, g_visEdit, VIS_SET_BYTES);
    if (loaded) memcpy(loaded, g_visLoaded, VIS_SET_BYTES);
    LeaveCriticalSection(&g_visLock);
}

void VisApi_GetLive(VisLiveRefdef *r)
{
    memset(r, 0, sizeof(*r));
    BYTE *cg = VisCg();
    if (!VisCgInMatch(cg)) return;
    const BYTE *rd = cg + VIS_CG_REFDEF; /* 0xB03EC.., the order 0x47A6E7 writes */
    r->filmEnable = rd[0x00];
    r->filmBrightness = *(const float *)(rd + 0x04);
    r->filmContrast = *(const float *)(rd + 0x08);
    r->filmDesaturation = *(const float *)(rd + 0x0C);
    r->filmDesaturationDark = *(const float *)(rd + 0x10);
    r->filmInvert = rd[0x14];
    memcpy(r->filmDarkTint, rd + 0x18, 12);
    memcpy(r->filmMediumTint, rd + 0x24, 12);
    memcpy(r->filmLightTint, rd + 0x30, 12);
    r->glowEnable = rd[0x3C];
    r->glowBloomCutoff = *(const float *)(rd + 0x40);
    r->glowBloomDesaturation = *(const float *)(rd + 0x44);
    r->glowBloomIntensity = *(const float *)(rd + 0x48);
    r->glowRadius = *(const float *)(rd + 0x4C);
    r->charDiffuseScale = *(const float *)(rd + 0x50);
    r->charSpecularScale = *(const float *)(rd + 0x54);
    int m = VisActiveMode(cg);
    r->glowFade = m >= 0 ? *(const float *)(VisSet(cg, VIS_CG_CUR, m) + VIS_GLOWFADE) : 0.0f;
    r->valid = 1;
}

void VisApi_SetField(int i, const void *value)
{
    if (i < 0 || i >= VIS_FIELDS || !value) return;
    const VisFieldInfo *f = &kVisFields[i];
    EnterCriticalSection(&g_visLock);
    if (f->type == VIS_T_BOOL) g_visEdit[f->offset] = *(const BYTE *)value ? 1 : 0;
    else memcpy(g_visEdit + f->offset, value, VisFieldSize(f->type));
    g_visDirty = memcmp(g_visEdit, g_visLoaded, VIS_SET_BYTES) != 0;
    VisApplyLocked(i);
    LeaveCriticalSection(&g_visLock);
}

void VisApi_SetEditMode(int mode)
{
    EnterCriticalSection(&g_visLock);
    g_visEditMode = mode >= 0 && mode < VIS_MODES ? mode : -1;
    g_visSyncMode = -1; /* resync on the next state read */
    g_visDirty = 0;
    LeaveCriticalSection(&g_visLock);
}

void VisApi_SetKeepInCache(int on)
{
    EnterCriticalSection(&g_visLock);
    g_visKeepCache = on != 0;
    LeaveCriticalSection(&g_visLock);
}

void VisApi_Revert(void)
{
    EnterCriticalSection(&g_visLock);
    memcpy(g_visEdit, g_visLoaded, VIS_SET_BYTES);
    g_visDirty = 0;
    BOOL live = VisApplyLocked(-1);
    LeaveCriticalSection(&g_visLock);
    VisStatus("reverted to the values loaded from %s%s", g_visSetName[0] ? g_visSetName : "the source", live ? "" : " (not applied: not in a match)");
}

void VisApi_Reread(void)
{
    EnterCriticalSection(&g_visLock);
    BOOL ok = VisRereadLocked();
    LeaveCriticalSection(&g_visLock);
    VisStatus(ok ? "re-read the set C1 is drawing with" : "not in a match: nothing to read");
}

void VisApi_Reapply(void)
{
    EnterCriticalSection(&g_visLock);
    BYTE *cg = VisCg();
    BOOL live = VisApplyLocked(-1);
    if (live) {
        VisNoteSync(cg, VisResolvedMode(cg));
        g_visDirty = memcmp(g_visEdit, g_visLoaded, VIS_SET_BYTES) != 0;
    }
    LeaveCriticalSection(&g_visLock);
    VisStatus(live ? "applied the edit buffer to C1 again" : "not in a match: not applied");
}

void VisApi_LoadZone(const char *name, int durationMs, int viaGame)
{
    (void)viaGame;
    if (!VisSafeName(name)) { VisStatus("bad vision name"); return; }
    EnterCriticalSection(&g_visLock);
    g_visJob.kind = 1;
    lstrcpynA(g_visJob.name, name, VIS_NAME_LEN);
    g_visJob.mode = -1;
    g_visJob.duration = durationMs < 0 ? 0 : durationMs > 60000 ? 60000 : durationMs;
    LeaveCriticalSection(&g_visLock);
    VisPostJob();
}

int VisApi_LoadDisk(const char *name)
{
    char path[MAX_PATH];
    if (!VisSafeName(name)) { VisStatus("bad file name"); return 0; }
    _snprintf(path, MAX_PATH - 1, "%s%s.vision", VisSaveDirPath(), name);
    path[MAX_PATH - 1] = 0;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { VisStatus("cannot open %s", path); return 0; }
    static char text[0x10000];
    DWORD got = 0;
    ReadFile(h, text, sizeof(text) - 1, &got, NULL);
    CloseHandle(h);
    text[got] = 0;
    int unknown = 0, malformed = 0, keys;
    EnterCriticalSection(&g_visLock);
    keys = VisParseLocked(text, g_visEdit, &unknown, &malformed);
    memcpy(g_visLoaded, g_visEdit, VIS_SET_BYTES);
    lstrcpynA(g_visSetName, name, VIS_NAME_LEN);
    _snprintf(g_visSource, sizeof(g_visSource), "disk file %s", path);
    g_visSource[sizeof(g_visSource) - 1] = 0;
    g_visDirty = 0;
    BOOL live = VisApplyLocked(-1);
    BYTE *cg = VisCg();
    if (live) VisNoteSync(cg, VisResolvedMode(cg));
    LeaveCriticalSection(&g_visLock);
    VisStatus("read %s: %d keys, %d unknown to C1 (kept for saving), %d malformed%s", path, keys, unknown, malformed,
              live ? ", applied" : " (not applied: not in a match)");
    return 1;
}

int VisApi_Save(const char *name, char *pathOut, int pathSize)
{
    static char text[0x4000];
    char path[MAX_PATH];
    if (!VisSafeName(name)) { VisStatus("bad file name (letters, digits, _ - . only)"); return 0; }
    CreateDirectoryA(VisSaveDirPath(), NULL);
    _snprintf(path, MAX_PATH - 1, "%s%s.vision", VisSaveDirPath(), name);
    path[MAX_PATH - 1] = 0;
    EnterCriticalSection(&g_visLock);
    int len = VisFormatLocked(g_visEdit, text, sizeof(text), TRUE);
    LeaveCriticalSection(&g_visLock);
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { VisStatus("cannot write %s (%lu)", path, GetLastError()); return 0; }
    DWORD w = 0;
    WriteFile(h, text, (DWORD)len, &w, NULL);
    CloseHandle(h);
    if (pathOut && pathSize > 0) lstrcpynA(pathOut, path, pathSize);
    VisStatus("saved %s (%lu bytes); zone source path: zones/<zone>/rawfile/vision/%s.vision", path, (unsigned long)w, name);
    return w == (DWORD)len;
}

int VisApi_ListZone(char (*names)[VIS_NAME_LEN], int max)
{
    int n = VisDbWalk(names, max, NULL);
    if (n > 0) qsort(names, n, VIS_NAME_LEN, VisCmpName);
    return n;
}

int VisApi_ListDisk(char (*names)[VIS_NAME_LEN], int max)
{
    char pat[MAX_PATH];
    WIN32_FIND_DATAA fd;
    int n = 0;
    _snprintf(pat, MAX_PATH - 1, "%s*.vision", VisSaveDirPath());
    pat[MAX_PATH - 1] = 0;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        int len = lstrlenA(fd.cFileName);
        if (len > 7 && len - 7 < VIS_NAME_LEN && n < max) {
            lstrcpynA(names[n], fd.cFileName, len - 7 + 1);
            n++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    qsort(names, n, VIS_NAME_LEN, VisCmpName);
    return n;
}

int VisApi_FormatFile(char *buf, int size)
{
    EnterCriticalSection(&g_visLock);
    int n = VisFormatLocked(g_visEdit, buf, size, TRUE);
    LeaveCriticalSection(&g_visLock);
    return n;
}

int VisApi_Unknown(int i, char *key, int keySize, char *value, int valueSize)
{
    int ok = 0;
    EnterCriticalSection(&g_visLock);
    if (i >= 0 && i < g_visUnkCount) {
        lstrcpynA(key, g_visUnkKey[i], keySize);
        lstrcpynA(value, g_visUnkVal[i], valueSize);
        ok = 1;
    }
    LeaveCriticalSection(&g_visLock);
    return ok;
}

static void VisSetOpen(BOOL open);
void VisApi_Close(void) { VisSetOpen(FALSE); }

int VisApi_PopInput(VisInputEvent *ev)
{
    int ok = 0;
    EnterCriticalSection(&g_visLock);
    if (g_visInTail != g_visInHead) {
        *ev = g_visInput[g_visInTail];
        g_visInTail = (g_visInTail + 1) % VIS_INPUT_RING;
        ok = 1;
    }
    LeaveCriticalSection(&g_visLock);
    return ok;
}

/* ---- input (main thread, from ConsoleWndProc) -------------------------------------------------------- */

static void VisPushInput(int type, int a, int b, unsigned lp)
{
    VisInputEvent ev = { type, a, b, lp, (GetKeyState(VK_CONTROL) < 0 ? 1 : 0) | (GetKeyState(VK_SHIFT) < 0 ? 2 : 0) |
                                             (GetKeyState(VK_MENU) < 0 ? 4 : 0) };
    EnterCriticalSection(&g_visLock);
    int next = (g_visInHead + 1) % VIS_INPUT_RING;
    if (next != g_visInTail) {
        g_visInput[g_visInHead] = ev;
        g_visInHead = next;
    }
    LeaveCriticalSection(&g_visLock);
}

static BOOL VisOwnsKeyboard(void) { return g_visOpen != 0; }

/* The cursor in client pixels, with the client size so the render thread can scale it to the back buffer.
 * From WM_MOUSEMOVE and the button messages (posted ones too: k33mouse drives the editor in tests), and
 * once from the real cursor when the editor opens. */
static void VisPushMousePos(HWND hwnd, int x, int y)
{
    RECT rc;
    if (!hwnd || !GetClientRect(hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
    VisPushInput(VIS_EV_MOUSEPOS, x, y, (unsigned)(rc.right & 0xFFFF) | ((unsigned)(rc.bottom & 0xFFFF) << 16));
}

static void VisSetOpen(BOOL open)
{
    if (!g_visReady) return;
    InterlockedExchange(&g_visOpen, open);
    InterlockedExchange(&g_k33OverlayMouse, open);
    if (open) {
        HWND h = VisGameHwnd();
        POINT p;
        if (h && GetCursorPos(&p) && ScreenToClient(h, &p)) VisPushMousePos(h, p.x, p.y);
    }
    if (!open) {
        InterlockedExchange(&g_visWantText, 0);
        VisPushInput(VIS_EV_FOCUS, 0, 0, 0);
    }
    Log("vision: editor %s", open ? "opened" : "closed");
}

static void VisRegisterKeyDvar(void)
{
    g_visKeyDvarTried = TRUE;
    const BYTE *d = C1RegisterInt("k33_visionKey", VK_F3, 0, 255, 0x1 /* archive */,
                                  "Kernel33 vision editor: toggle key (Windows virtual-key code; 114 F3, 0 none)");
    if (d) g_visKeyDvar = d;
}

static UINT VisInputCodePage(void)
{
    char buf[8];
    LANGID lang = LOWORD((DWORD_PTR)GetKeyboardLayout(0));
    if (GetLocaleInfoA(MAKELCID(lang, SORT_DEFAULT), LOCALE_IDEFAULTANSICODEPAGE, buf, sizeof(buf)) > 0) {
        UINT cp = (UINT)atoi(buf);
        if (cp) return cp;
    }
    return GetACP();
}

static void VisTypeChar(WPARAM wp)
{
    BYTE in[2] = { (BYTE)wp, (BYTE)(wp >> 8) };
    int count = in[1] ? 2 : 1;
    UINT cp = VisInputCodePage();
    for (int i = 0; i < count; i++) {
        BYTE c = in[i];
        if (!g_visMbLen && c < 0x80) {
            if (c >= 0x20 && c != 0x7F) VisPushInput(VIS_EV_CHAR, c, 0, 0);
            continue;
        }
        g_visMb[g_visMbLen++] = c;
        WCHAR wide[4];
        int n = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, (LPCSTR)g_visMb, g_visMbLen, wide, 4);
        if (n <= 0) {
            BOOL lead = cp == CP_UTF8 ? g_visMb[0] >= 0xC2 && g_visMb[0] <= 0xF4 : IsDBCSLeadByteEx(cp, g_visMb[0]);
            if (lead && g_visMbLen < (cp == CP_UTF8 ? 4 : 2)) continue;
            g_visMbLen = 0;
            continue;
        }
        g_visMbLen = 0;
        for (int k = 0; k < n; k++) VisPushInput(VIS_EV_CHAR, wide[k], 0, 0);
    }
}

/* Called first by ConsoleWndProc. conOpen: the console owns the keyboard; conToggle: this key opens or
 * closes the console. Returns TRUE when the message is consumed (*res is the result). */
static BOOL VisWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL conOpen, BOOL conToggle, LRESULT *res)
{
    *res = 0;
    if (msg == VIS_JOB_MSG) { VisRunJobs(); return TRUE; }
    if (!g_visReady) return FALSE;
    if (!g_visKeyDvarTried && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && C1DvarsReady()) VisRegisterKeyDvar();
    if (g_visJobPending) VisRunJobs();
    BOOL open = g_visOpen != 0;
    if (open && ((msg >= WM_MOUSEMOVE && msg <= WM_MBUTTONDBLCLK) || (msg >= WM_XBUTTONDOWN && msg <= WM_XBUTTONDBLCLK)))
        VisPushMousePos(hwnd, (short)LOWORD(lp), (short)HIWORD(lp));
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        if (msg == WM_SYSKEYDOWN && (wp == VK_F4 || wp == VK_TAB || wp == VK_RETURN)) return FALSE; /* Alt+F4/Tab/Enter */
        if (conToggle) return FALSE;
        int key = VisApi_ToggleKey();
        if (key > 0 && (int)wp == key) {
            if (!(lp & (1 << 30))) VisSetOpen(!open);
            return TRUE;
        }
        if (!open || conOpen) return FALSE;
        if (wp == VK_ESCAPE && !g_visWantText) { VisSetOpen(FALSE); return TRUE; }
        VisPushInput(VIS_EV_KEY, (int)wp, 1, (unsigned)lp);
        return TRUE;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (open && !conOpen) VisPushInput(VIS_EV_KEY, (int)wp, 0, (unsigned)lp);
        return FALSE; /* key-ups always reach the game: nothing held stays down */
    case WM_CHAR:
    case WM_SYSCHAR:
        if (!open || conOpen) return FALSE;
        if (msg == WM_CHAR) VisTypeChar(wp);
        return TRUE;
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
        return open && !conOpen;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: if (!open) return FALSE; VisPushInput(VIS_EV_MOUSE, 0, 1, 0); return TRUE;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: if (!open) return FALSE; VisPushInput(VIS_EV_MOUSE, 1, 1, 0); return TRUE;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: if (!open) return FALSE; VisPushInput(VIS_EV_MOUSE, 2, 1, 0); return TRUE;
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        if (!open) return FALSE;
        VisPushInput(VIS_EV_MOUSE, GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? 3 : 4, 1, 0);
        *res = TRUE;
        return TRUE;
    case WM_LBUTTONUP: if (open) VisPushInput(VIS_EV_MOUSE, 0, 0, 0); return FALSE;
    case WM_RBUTTONUP: if (open) VisPushInput(VIS_EV_MOUSE, 1, 0, 0); return FALSE;
    case WM_MBUTTONUP: if (open) VisPushInput(VIS_EV_MOUSE, 2, 0, 0); return FALSE;
    case WM_XBUTTONUP: if (open) VisPushInput(VIS_EV_MOUSE, GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? 3 : 4, 0, 0); return FALSE;
    case WM_MOUSEWHEEL:
        if (!open) return FALSE;
        VisPushInput(VIS_EV_WHEEL, GET_WHEEL_DELTA_WPARAM(wp), 0, 0);
        return TRUE;
    case WM_MOUSEHWHEEL:
        if (!open) return FALSE;
        VisPushInput(VIS_EV_HWHEEL, GET_WHEEL_DELTA_WPARAM(wp), 0, 0);
        return TRUE;
    case WM_KILLFOCUS:
        if (open) VisPushInput(VIS_EV_FOCUS, 0, 0, 0);
        return FALSE;
    case WM_SETFOCUS:
        if (open) VisPushInput(VIS_EV_FOCUS, 1, 0, 0);
        return FALSE;
    }
    return FALSE;
}

/* ---- drawing (render thread, from console.c's Present hooks) ----------------------------------------- */

static void VisSaveShot(IDirect3DDevice9 *dev, IDirect3DSurface9 *bb, int W, int H)
{
    D3DSURFACE_DESC d;
    IDirect3DSurface9 *rt = NULL, *sys = NULL;
    IDirect3DSurface9_GetDesc(bb, &d);
    if (d.Format != D3DFMT_X8R8G8B8 && d.Format != D3DFMT_A8R8G8B8) { VisStatus("shot: back buffer format %d not supported", (int)d.Format); return; }
    if (FAILED(IDirect3DDevice9_CreateRenderTarget(dev, W, H, d.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL))) rt = NULL;
    if (rt && FAILED(IDirect3DDevice9_StretchRect(dev, bb, NULL, rt, NULL, D3DTEXF_NONE))) { IDirect3DSurface9_Release(rt); rt = NULL; }
    if (!rt) { VisStatus("shot: could not copy the back buffer"); return; }
    if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, W, H, d.Format, D3DPOOL_SYSTEMMEM, &sys, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, rt, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
            char path[MAX_PATH];
            _snprintf(path, MAX_PATH - 1, "%smain\\k33_vision.bmp", VisExeDir());
            path[MAX_PATH - 1] = 0;
            FILE *f = fopen(path, "wb");
            if (f) {
                BITMAPFILEHEADER fh = { 0x4D42, 0, 0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) };
                BITMAPINFOHEADER ih = { sizeof(ih), W, -H, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
                fh.bfSize = fh.bfOffBits + (DWORD)W * H * 4;
                fwrite(&fh, sizeof(fh), 1, f);
                fwrite(&ih, sizeof(ih), 1, f);
                for (int y = 0; y < H; y++) {
                    DWORD *row = (DWORD *)((BYTE *)lr.pBits + (size_t)y * lr.Pitch);
                    for (int x = 0; x < W; x++) row[x] |= 0xFF000000u;
                    fwrite(row, 4, (size_t)W, f);
                }
                fclose(f);
                VisStatus("shot: %dx%d frame saved to %s", W, H, path);
            }
            IDirect3DSurface9_UnlockRect(sys);
        }
    }
    if (sys) IDirect3DSurface9_Release(sys);
    IDirect3DSurface9_Release(rt);
}

static volatile LONG g_visInDraw;

static void VisDraw(IDirect3DDevice9 *dev)
{
    static LONG failures;
    LONG shot = g_visShot;
    BOOL shotDue = shot && (LONG)(GetTickCount() - (DWORD)shot) >= 0;
    if (!g_visReady || (!g_visOpen && !shotDue) || InterlockedExchange(&g_visInDraw, 1)) return;
    if (IDirect3DDevice9_TestCooperativeLevel(dev) != D3D_OK) goto out;
    IDirect3DSurface9 *bb = NULL;
    D3DSURFACE_DESC desc;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) goto out;
    IDirect3DSurface9_GetDesc(bb, &desc);
    int W = (int)desc.Width, H = (int)desc.Height;
    if (W < 320 || H < 200 || W > 16384 || H > 16384) { IDirect3DSurface9_Release(bb); goto out; }
    if (g_visOpen && !(shotDue && g_visShotNoUi)) {
        IDirect3DStateBlock9 *sb = NULL;
        if (FAILED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &sb)) || !sb) {
            if (InterlockedIncrement(&failures) <= 3) Log("vision: CreateStateBlock failed, not drawing");
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
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_CLIPPLANEENABLE, 0);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_MULTISAMPLEANTIALIAS, FALSE);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, 0);
        if (SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
            EnterCriticalSection(&g_visGuiLock);
            int r = VisGui_Render(dev, W, H);
            LeaveCriticalSection(&g_visGuiLock);
            IDirect3DDevice9_EndScene(dev);
            if (!r && InterlockedIncrement(&failures) <= 3) Log("vision: ImGui could not render");
        }
        IDirect3DDevice9_SetRenderTarget(dev, 0, oldRt);
        IDirect3DDevice9_SetDepthStencilSurface(dev, oldDs);
        if (oldRt) IDirect3DSurface9_Release(oldRt);
        if (oldDs) IDirect3DSurface9_Release(oldDs);
        IDirect3DStateBlock9_Apply(sb);
        IDirect3DStateBlock9_Release(sb);
    }
    if (shotDue && InterlockedCompareExchange(&g_visShot, 0, shot) == shot) VisSaveShot(dev, bb, W, H);
    IDirect3DSurface9_Release(bb);
out:
    InterlockedExchange(&g_visInDraw, 0);
}

/* Before IDirect3DDevice9::Reset (the device's D3DPOOL_DEFAULT objects must go) and before a new
 * device replaces this one (ImGui's DX9 backend holds a reference to the old one). */
static void VisBeforeReset(void)
{
    if (!g_visReady) return;
    EnterCriticalSection(&g_visGuiLock);
    VisGui_InvalidateDevice();
    LeaveCriticalSection(&g_visGuiLock);
}

static void VisBeforeCreateDevice(void)
{
    if (!g_visReady) return;
    EnterCriticalSection(&g_visGuiLock);
    VisGui_Shutdown();
    LeaveCriticalSection(&g_visGuiLock);
}

/* ---- k33vision command (main thread) ---------------------------------------------------------------- */

static int VisArgc(void)
{
    int idx = *(volatile int *)(DWORD_PTR)VIS_CMD_ARGS_VA;
    if (idx < 0 || idx > 7) return 0;
    return *(volatile int *)(DWORD_PTR)(VIS_CMD_ARGS_VA + 0x44 + idx * 4);
}

static const char *VisArgv(int n)
{
    int idx = *(volatile int *)(DWORD_PTR)VIS_CMD_ARGS_VA;
    if (idx < 0 || idx > 7 || n >= VisArgc()) return "";
    const char *const *argv = *(const char *const *const *)(DWORD_PTR)(VIS_CMD_ARGS_VA + 0x64 + idx * 4);
    return argv && Readable(argv + n, 4) && argv[n] && Readable(argv[n], 1) ? argv[n] : "";
}

/* k33vision output: C1's console (and the overlay) and Kernel33.log, so tests can read it. */
static void VisOut(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void VisOut(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    C1Print(C1_PRINT_CHANNEL, buf);
    int n = lstrlenA(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
    Log("vision> %s", buf);
}

static void VisPrintState(void)
{
    VisUiState s;
    VisApi_GetState(&s);
    VisOut("k33vision: editor %s (toggle key %d), %s, active type %s, editing %s%s, set \"%s\" from %s%s\n",
             g_visOpen ? "open" : "closed", VisApi_ToggleKey(), s.inMatch ? "in a match" : "not in a match",
             s.activeMode >= 0 ? kVisModeLabels[s.activeMode] : "-", kVisModeLabels[s.editMode], s.autoMode ? " (auto)" : "",
             s.setName, s.source, s.dirty ? ", edited" : "");
    if (s.inMatch)
        for (int m = 0; m < VIS_MODES; m++)
            VisOut("  %-18s style %d  \"%s\"\n", kVisModeLabels[m], s.modeStyle[m], s.modeName[m]);
    if (s.filmTweaks || s.glowTweaks)
        VisOut("  ^3r_filmUseTweaks %d / r_glowUseTweaks %d: the renderer ignores the vision set's film/glow\n", s.filmTweaks, s.glowTweaks);
}

static void __cdecl VisCmd(void)
{
    const char *sub = VisArgv(1);
    if (!sub[0] || !_stricmp(sub, "status")) { VisPrintState(); return; }
    if (!_stricmp(sub, "open")) { VisSetOpen(TRUE); return; }
    if (!_stricmp(sub, "close")) { VisSetOpen(FALSE); return; }
    if (!_stricmp(sub, "toggle")) { VisSetOpen(!g_visOpen); return; }
    if (!_stricmp(sub, "load")) {
        int dur = VisArgc() > 3 ? atoi(VisArgv(3)) : 0;
        if (!VisSafeName(VisArgv(2))) { VisOut("usage: k33vision load <name> [transition ms]\n"); return; }
        VisLoadZoneNow(VisArgv(2), -1, dur);
        VisOut("k33vision: %s\n", g_visStatus);
        return;
    }
    if (!_stricmp(sub, "loadfile")) { VisApi_LoadDisk(VisArgv(2)); VisOut("k33vision: %s\n", g_visStatus); return; }
    if (!_stricmp(sub, "save")) {
        const char *n = VisArgc() > 2 ? VisArgv(2) : g_visSetName;
        VisApi_Save(n, NULL, 0);
        VisOut("k33vision: %s\n", g_visStatus);
        return;
    }
    if (!_stricmp(sub, "revert")) { VisApi_Revert(); VisOut("k33vision: %s\n", g_visStatus); return; }
    if (!_stricmp(sub, "reread")) { VisApi_Reread(); VisOut("k33vision: %s\n", g_visStatus); return; }
    if (!_stricmp(sub, "mode")) {
        const char *m = VisArgv(2);
        int mode = -1;
        for (int i = 0; i < VIS_MODES; i++) if (!_strnicmp(m, kVisModeLabels[i], 4)) mode = i;
        if (isdigit((unsigned char)m[0])) mode = atoi(m);
        VisApi_SetEditMode(mode);
        VisOut("k33vision: editing %s\n", mode < 0 ? "the active type (auto)" : kVisModeLabels[mode]);
        return;
    }
    if (!_stricmp(sub, "set")) {
        const VisFieldInfo *f = VisFindField(VisArgv(2));
        if (!f) { VisOut("k33vision set <key> <value>: key is a C1 id (511..531) or an IW name (r_filmContrast, ...)\n"); return; }
        BYTE v[12];
        float *fv = (float *)v;
        if (f->type == VIS_T_BOOL) v[0] = atoi(VisArgv(3)) != 0;
        else if (f->type == VIS_T_FLOAT) fv[0] = (float)atof(VisArgv(3));
        else { fv[0] = (float)atof(VisArgv(3)); fv[1] = (float)atof(VisArgv(4)); fv[2] = (float)atof(VisArgv(5)); }
        VisApi_SetField((int)(f - kVisFields), v);
        BYTE *cg = VisCg();
        VisOut("k33vision: %s (%s) set%s\n", f->id, f->iwName, VisCgInMatch(cg) ? " and applied" : " in the buffer (not in a match)");
        return;
    }
    if (!_stricmp(sub, "list")) {
        static char names[VIS_MAX_LIST][VIS_NAME_LEN];
        int n = VisApi_ListZone(names, VIS_MAX_LIST);
        VisOut("k33vision: %d vision files in the loaded zones\n", n);
        for (int i = 0; i < n; i++) if (!VisArgv(2)[0] || strstr(names[i], VisArgv(2))) VisOut("  %s\n", names[i]);
        return;
    }
    if (!_stricmp(sub, "dump")) {
        static char text[0x4000];
        VisApi_FormatFile(text, sizeof(text));
        VisOut("k33vision: %s (%s)\n%s", g_visSetName, g_visSource, text);
        return;
    }
    if (!_stricmp(sub, "shot")) {
        int delay = atoi(VisArgv(2));
        if (delay < 0 || delay > 60000) delay = 0;
        InterlockedExchange(&g_visShotNoUi, !_stricmp(VisArgv(3), "noui"));
        InterlockedExchange(&g_visShot, (LONG)((GetTickCount() + delay) | 1));
        VisOut("k33vision: the frame %d ms from now goes to main\\k33_vision.bmp%s\n", delay, g_visShotNoUi ? " (without the editor)" : "");
        return;
    }
    VisOut("k33vision [status|open|close|toggle|load <name> [ms]|loadfile <name>|save [name]|revert|reread|"
             "mode <auto|naked|night|missilecam|thermal|pain|5>|set <key> <value..>|list [filter]|dump|shot [ms] [noui]]\n");
}

static C1Cmd g_visCmd = { NULL, "k33vision", (void *)VisCmd };

/* ---- install ------------------------------------------------------------------------------------------ */

static void InstallVision(void)
{
    static const BYTE kLookup[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14, 0x53, 0x56, 0x57, 0x8B, 0x7D, 0x08 };
    static const BYTE kTransition[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x53, 0x56, 0x57, 0x8B, 0xF8 };
    static const BYTE kSelectRef[] = { 0x8D, 0x84, 0x30, 0x08, 0x3C, 0x17, 0x00 };
    static const BYTE kUpdateRef[] = { 0x8D, 0xB3, 0x68, 0x39, 0x17, 0x00, 0x8D, 0xBB, 0xA8, 0x3E, 0x17, 0x00 };
    static const BYTE kRefdefRef[] = { 0x0F, 0xB6, 0x48, 0x24, 0x88, 0x8E, 0xEC, 0x03, 0x0B, 0x00 };
    static const BYTE kDbHash[] = { 0x0F, 0xB7, 0x04, 0x45, 0xD8, 0xD9, 0x8F, 0x01 };            /* @0x4CB554 */
    static const BYTE kDbEntry[] = { 0x8D, 0x0C, 0x40, 0x8B, 0x04, 0xCD, 0xC8, 0x02, 0xC4, 0x01 }; /* @0x4CB564 */
    static const BYTE kDbLock[] = { 0x68, 0x5C, 0x7A, 0x00, 0x02 };                                /* @0x4CB953 */
    InitializeCriticalSection(&g_visLock);
    InitializeCriticalSection(&g_visGuiLock);
    C1GameInit();
    VisDefaults(g_visEdit);
    memcpy(g_visLoaded, g_visEdit, VIS_SET_BYTES);

    BOOL table = Readable((void *)(DWORD_PTR)VIS_FIELDTABLE_VA, VIS_FIELDS * 12);
    for (int i = 0; table && i < VIS_FIELDS; i++) {
        const DWORD *e = (const DWORD *)(DWORD_PTR)(VIS_FIELDTABLE_VA + i * 12);
        const char *n = (const char *)(DWORD_PTR)e[0];
        if (!Readable(n, 4) || strcmp(n, kVisFields[i].id) || (int)e[1] != kVisFields[i].offset || (int)e[2] != kVisFields[i].type) {
            Log("vision: visionDefFields entry %d differs (%s), editor off", i, Readable(n, 4) ? n : "?");
            table = FALSE;
        }
    }
    BOOL code = Readable((void *)(DWORD_PTR)VIS_LOOKUP_VA, sizeof(kLookup)) &&
                !memcmp((void *)(DWORD_PTR)VIS_LOOKUP_VA, kLookup, sizeof(kLookup)) &&
                !memcmp((void *)(DWORD_PTR)VIS_TRANSITION_VA, kTransition, sizeof(kTransition)) &&
                !memcmp((void *)(DWORD_PTR)VIS_SELECT_REF_VA, kSelectRef, sizeof(kSelectRef)) &&
                !memcmp((void *)(DWORD_PTR)VIS_UPDATE_REF_VA, kUpdateRef, sizeof(kUpdateRef)) &&
                !memcmp((void *)(DWORD_PTR)VIS_REFDEF_REF_VA, kRefdefRef, sizeof(kRefdefRef));
    g_visDbOk = !memcmp((void *)(DWORD_PTR)0x004CB554u, kDbHash, sizeof(kDbHash)) &&
                !memcmp((void *)(DWORD_PTR)0x004CB564u, kDbEntry, sizeof(kDbEntry)) &&
                !memcmp((void *)(DWORD_PTR)0x004CB953u, kDbLock, sizeof(kDbLock));
    if (!table || !code) {
        Log("vision: C1 vision code %s, field table %s: editor off", code ? "ok" : "differs", table ? "ok" : "differs");
        return;
    }
    g_visReady = TRUE;
    BOOL cmd = C1AddCommand(&g_visCmd);
    Log("vision: editor ready (toggle k33_visionKey, default F3; k33vision command %s; zone list %s; saves to %s)",
        cmd ? "added" : "NOT added", g_visDbOk ? "ok" : "off (DB bytes differ)", VisSaveDirPath());
}

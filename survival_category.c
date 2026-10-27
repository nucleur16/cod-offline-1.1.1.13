/* The survival match category ("survival"): its own gametype zones and its own constant configstring table, the way
 * C1 1.1.1.13 already does zm. Included by kernel33.c; the hooks are in survival_category_hooks.h, the names and the
 * checksum rule in survival_category_policy.h.
 *
 * C1 1.1.1.13 (measured in C1.exe; 2.6.9.7 is the model, codol2697-static-candidate.bin):
 *   - 0x4CCDF0 builds a map's zone list (ecx = category, edx = map; called by 0x499450, the loadingnewmap and
 *     preloadmap paths). After 0x4CCD80's common_mp part it compares the category: Q_strnicmp(cat, "zm", 2) -> the
 *     four common_gametype_zm zones, Q_strnicmp(cat, "coop", 4) -> the coop four, else the mp four (alloc flags 4,
 *     each set skipped when its common_gametype_* zone is already loaded, DB_IsZoneLoaded 0x4CD7D0); then
 *     DB_LoadXAssets, then the map: "coop_<mode>_<map>" adds patch_ + the session zone, and patch_<map>, <map>,
 *     ffotd_client. 2.6.9.7's builder (0x4D11F0) adds common_gametype_survival with the same four-zone helper
 *     (0x4D1020: patch_%s, %s, patch_localized_%s, localized_%s, flags 4); it picks it from the session map's
 *     "coop_survival" prefix, where the category is "survival" here (user direction 2026-10-27: survival loads
 *     common_gametype_survival and the plain mp_<map>, no coop zone).
 *   - 0x4B6380 (called first by 0x4CCDF0) adds the stock sizes of the zones about to load for the loading bar
 *     ([0x17203FC]), with the same category branch. Survival skips it: the survival zones are not in the stock size
 *     table 0xCD5448/0xC3CE68, which only lists the 90 stock zones.
 *   - 0x612850 (edi = category) selects mp/constConfigStrings_{coop,zm,mp}.csv by the same prefixes,
 *     DB_FindXAssetHeader(0x25 stringtable) -> [0x2A99A44], checksums it (0x83B094E, a TenProtect VM stub) ->
 *     [0x2A99A50], the value the gamestate's checksum must equal, and clears [0x2A99A40..0x2A99A4C].
 * The hooks run C1's own code for every other category string, byte for byte (tests/test_survival_category_x86.py
 * runs the patched and the stock functions side by side in Unicorn).
 *
 * The table, mp/constconfigstrings_survival.csv, ships in codol_common (zone_packs/, source zone
 * codol_survival_category). It is a copy of 1.1.1.13's coop table: 2.6.9.7's survival table is in 2.6.9.7's
 * configstring layout, which is not C1's (its menus start at 4315, C1's at 4243; models 1388 / 1316; weapons 5435 /
 * 5363; footsteps 6206 / 6134). The server reads the same CSV.
 *
 * "+set codol_survival 0" on the command line installs nothing. */
#define K33_SURV_LOG Log
#define K33_SURV_READABLE(p, n) Readable((p), (n))
#include "survival_category_hooks.h"

static BOOL SurvivalCategoryEnabled(void)
{
    const char *p = GetCommandLineA(), *q;
    BOOL on = TRUE;
    while ((q = strstr(p, "codol_survival")) != NULL) {
        q += 14;
        p = q;
        if (*q != ' ' && *q != '\t' && *q != '"') continue; /* codol_survivalXxx is another name */
        while (*q == ' ' || *q == '\t' || *q == '"') q++;
        on = *q != '0';
    }
    return on;
}

typedef struct { DWORD va; BYTE bytes[24]; int n; } SurvivalSite;

static BOOL SurvivalSiteMatches(const SurvivalSite *s)
{
    return Readable((const void *)(DWORD_PTR)s->va, s->n) && memcmp((const void *)(DWORD_PTR)s->va, s->bytes, s->n) == 0;
}

/* jmp rel32 to `to` at `at`, NOP-padded to `len` bytes. */
static void SurvivalJump(DWORD at, void (*to)(void), int len)
{
    BYTE code[8];
    LONG rel = (LONG)((DWORD_PTR)to - (at + 5));
    code[0] = 0xE9;
    memcpy(code + 1, &rel, 4);
    for (int i = 5; i < len; i++) code[i] = 0x90;
    WriteCode((void *)(DWORD_PTR)at, code, len);
}

static void PatchSurvivalCategory(void)
{
    /* the bytes each hook replaces or jumps to, with enough context to pin the site */
    static const SurvivalSite kSites[] = {
        { K33_SURV_ZONE_HOOK_VA, { 0x8B, 0xBD, 0x14, 0xFE, 0xFF, 0xFF, 0x57, 0xB8, 0x02, 0x00, 0x00, 0x00,
                                   0xBA, 0xAC, 0x89, 0xC6, 0x00, 0xE8 }, 18 },
        { K33_SURV_ZONE_LOAD_VA, { 0x8B, 0x85, 0x1C, 0xFE, 0xFF, 0xFF, 0x53, 0x50, 0x8D, 0x95, 0x20, 0xFE,
                                   0xFF, 0xFF, 0x52, 0xE8 }, 16 },
        { 0x004CCECEu, { 0xBE, 0x04, 0x00, 0x00, 0x00 }, 5 },                       /* mov esi, 4 */
        { K33_SURV_SIZE_HOOK_VA, { 0x56, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xBA, 0xAC, 0x89, 0xC6, 0x00, 0xE8 }, 12 },
        { K33_SURV_SIZE_SKIP_VA, { 0x68, 0x88, 0xE9, 0xC4, 0x00, 0xE8 }, 6 },       /* push "patch_common_mp" */
        { K33_SURV_CONST_HOOK_VA, { 0x56, 0x57, 0xB8, 0x04, 0x00, 0x00, 0x00, 0xBA, 0x78, 0x23, 0xC4, 0x00,
                                    0xBE, 0x94, 0xDF, 0xC6, 0x00 }, 17 },
        { K33_SURV_CONST_TAIL_VA, { 0x56, 0x6A, 0x25, 0xE8 }, 4 },                 /* push esi ; push 0x25 ; call */
        { 0x00612897u, { 0xA3, 0x44, 0x9A, 0xA9, 0x02, 0xE8 }, 6 },                 /* mov [0x2A99A44], eax */
        { 0x006128A1u, { 0xA3, 0x50, 0x9A, 0xA9, 0x02 }, 5 },                       /* mov [0x2A99A50], eax */
        { 0x006128BAu, { 0x5E, 0xC3 }, 2 },                                         /* pop esi ; ret */
    };
    if (!SurvivalCategoryEnabled()) {
        Log("survival category: off (codol_survival 0)");
        return;
    }
    for (unsigned i = 0; i < sizeof(kSites) / sizeof(kSites[0]); i++)
        if (!SurvivalSiteMatches(&kSites[i])) {
            Log("survival category: bytes at 0x%08X do not match, not installed", kSites[i].va);
            return;
        }
    SurvivalJump(K33_SURV_ZONE_HOOK_VA, K33SurvivalZoneHook, 6);
    SurvivalJump(K33_SURV_SIZE_HOOK_VA, K33SurvivalSizeHook, 6);
    SurvivalJump(K33_SURV_CONST_HOOK_VA, K33SurvivalConstHook, 7);
    Log("survival category: \"%s\" loads %s (+patch_, localized_) and %s", K33_SURVIVAL_CATEGORY,
        K33_SURVIVAL_GAMETYPE_ZONE, K33_SURVIVAL_CONST_TABLE);
}

/* The survival match category ("survival"): the multiplayer gametype zones plus survival's own, the way C1 1.1.1.13
 * loads one gametype set per category. Included by kernel33.c; the hook is in survival_category_hooks.h, the names in
 * survival_category_policy.h.
 *
 * C1 1.1.1.13 (measured in C1.exe; 2.6.9.7 is the model, codol2697-static-candidate.bin):
 *   - 0x4CCDF0 builds a map's zone list (ecx = category, edx = map; called by 0x499450, the loadingnewmap and
 *     preloadmap paths). 0x4CCD80 first adds patch_common_mp, common_mp, patch_localized_common_mp,
 *     localized_common_mp (flags 1). Then it compares the category: Q_strnicmp(cat, "zm", 2) -> the four
 *     common_gametype_zm zones, Q_strnicmp(cat, "coop", 4) -> the coop four, else (0x4CD03E) the mp four (flags 4,
 *     each set skipped when its common_gametype_* zone is already loaded, DB_IsZoneLoaded 0x4CD7D0); DB_LoadXAssets
 *     (sync 0); then the map: "coop_<mode>_<map>" adds patch_ + the session zone, then patch_<map>, <map>,
 *     ffotd_client (flags 4). 2.6.9.7's builder (0x4D11F0) adds common_gametype_survival with the same four-zone
 *     helper (0x4D1020: patch_%s, %s, patch_localized_%s, localized_%s, flags 4).
 *   - Survival runs on the MP rules (user direction 2026-10-27: MP weapons, create-a-class), so for "survival" this
 *     hook appends the four survival zones and then runs C1's own mp branch: one list, the survival set ahead of
 *     the mp set, so C1's first-linked-wins keeps the survival zone's copy of a name both carry.
 *   - Everything else that tests the category already treats "survival" as mp, unchanged: 0x4B6380 (the loading
 *     bar's size estimate, mp sizes; the survival zones are not in its stock size table) and 0x612850
 *     (mp/constConfigStrings_mp.csv and C1's native checksum, 0xAD1E1713 measured).
 * The hook runs C1's own code for every other category string, byte for byte (tests/test_survival_category_x86.py
 * runs the patched and the stock functions side by side in Unicorn).
 *
 * "+set codol_survival 0" on the command line installs nothing. */
#define K33_SURV_LOG Log
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
    /* the bytes the hook replaces or jumps to, with enough context to pin the site */
    static const SurvivalSite kSites[] = {
        { K33_SURV_ZONE_HOOK_VA, { 0x8B, 0xBD, 0x14, 0xFE, 0xFF, 0xFF, 0x57, 0xB8, 0x02, 0x00, 0x00, 0x00,
                                   0xBA, 0xAC, 0x89, 0xC6, 0x00, 0xE8 }, 18 },
        { 0x004CCECEu, { 0xBE, 0x04, 0x00, 0x00, 0x00 }, 5 },                       /* mov esi, 4 */
        { K33_SURV_ZONE_MP_VA, { 0x68, 0x38, 0xE9, 0xC4, 0x00, 0xE8 }, 6 },         /* push "common_gametype_mp" */
        { 0x004CD053u, { 0x8B, 0x85, 0x1C, 0xFE, 0xFF, 0xFF }, 6 },                 /* mov eax, [ebp-0x1E4] */
        { 0x004CD0DCu, { 0x83, 0xC0, 0x04, 0xEB, 0x06, 0x8B, 0x85, 0x1C, 0xFE, 0xFF, 0xFF, 0x53, 0x50 }, 13 },
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
    Log("survival category: \"%s\" loads %s (+patch_, localized_) ahead of the mp gametype zones",
        K33_SURVIVAL_CATEGORY, K33_SURVIVAL_GAMETYPE_ZONE);
}

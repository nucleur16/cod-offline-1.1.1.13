/* The survival match category: names and rules shared by survival_category.c (Kernel33) and its tests.
 * Pure C, no Windows and no C library (the x86 fixture links with -nostdlib).
 *
 * C1 1.1.1.13 knows three categories. Its zone-list builder (0x4CCDF0) loads "patch_common_gametype_<cat>",
 * "common_gametype_<cat>", "patch_localized_common_gametype_<cat>" and "localized_common_gametype_<cat>" (alloc flags
 * 4) for a category that starts "zm" or "coop" and the mp set for anything else. CoDOL 2.6.9.7 adds
 * common_gametype_survival with the same four-zone pattern (its builder 0x4D1020 formats patch_%s, %s,
 * patch_localized_%s, localized_%s).
 *
 * Survival runs on the multiplayer rules (MP weapons, create-a-class, the MP HUD): the category "survival" (exact,
 * ignoring case, as C1 compares categories with Q_strnicmp) keeps everything C1 does for an mp category -- the mp
 * gametype zones, mp/constConfigStrings_mp.csv and its native checksum -- and Kernel33 only puts the four survival
 * zones in the list ahead of the mp ones, so where both carry an asset the survival zone's wins (C1 keeps the first
 * definition it links). Every other category string takes C1's own code unchanged. */
#ifndef K33_SURVIVAL_CATEGORY_POLICY_H
#define K33_SURVIVAL_CATEGORY_POLICY_H

#define K33_SURVIVAL_CATEGORY "survival"
#define K33_SURVIVAL_GAMETYPE_ZONE "common_gametype_survival"
#define K33_SURVIVAL_ZONE_ALLOC_FLAGS 4 /* the gametype zones' flags in 0x4CCDF0 (and 2.6.9.7's 0x4D1020) */

/* In C1's order: patch_, the zone, patch_localized_, localized_. A missing patch_ zone only prints (0x8308DBD). */
static const char *const kK33SurvivalZones[4] = {
    "patch_common_gametype_survival",
    "common_gametype_survival",
    "patch_localized_common_gametype_survival",
    "localized_common_gametype_survival",
};

static int K33SurvLower(int c) { return c >= 'A' && c <= 'Z' ? c + 0x20 : c; }

/* a == b ignoring ASCII case; NULL never matches. */
static int K33SurvEqualI(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && K33SurvLower((unsigned char)*a) == K33SurvLower((unsigned char)*b)) a++, b++;
    return *a == 0 && *b == 0;
}

static int K33IsSurvivalCategory(const char *category)
{
    return K33SurvEqualI(category, K33_SURVIVAL_CATEGORY);
}

#endif

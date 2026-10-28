/* survival_category_policy.h: the category rule and the zone set. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../survival_category_policy.h"

static void categories(void)
{
    static const char *const yes[] = { "survival", "SURVIVAL", "Survival", "sUrViVaL" };
    static const char *const no[] = { "", "mp", "combat", "war", "coop", "zm", "coop_survival", "survival_", "survivals",
                                      "surviva", "xsurvival", " survival", "survival ", "evac", "cyborg" };
    for (unsigned i = 0; i < sizeof(yes) / sizeof(yes[0]); i++) assert(K33IsSurvivalCategory(yes[i]));
    for (unsigned i = 0; i < sizeof(no) / sizeof(no[0]); i++) assert(!K33IsSurvivalCategory(no[i]));
    assert(!K33IsSurvivalCategory(NULL));
}

static void zones(void)
{
    /* C1's order for zm/coop/mp (0x4CCEFD..0x4CCF60) and 2.6.9.7's helper 0x4D1020 */
    assert(!strcmp(kK33SurvivalZones[0], "patch_" K33_SURVIVAL_GAMETYPE_ZONE));
    assert(!strcmp(kK33SurvivalZones[1], K33_SURVIVAL_GAMETYPE_ZONE));
    assert(!strcmp(kK33SurvivalZones[2], "patch_localized_" K33_SURVIVAL_GAMETYPE_ZONE));
    assert(!strcmp(kK33SurvivalZones[3], "localized_" K33_SURVIVAL_GAMETYPE_ZONE));
    assert(K33_SURVIVAL_ZONE_ALLOC_FLAGS == 4);
}

int main(void)
{
    categories();
    zones();
    puts("survival category policy: ok");
    return 0;
}

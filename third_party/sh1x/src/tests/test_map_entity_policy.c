#include "map_entity_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(MapEntityUsesMpBase("mp"));
    assert(MapEntityUsesMpBase("survival"));
    assert(!MapEntityUsesMpBase("coop"));
    assert(!MapEntityUsesMpBase("zm"));
    assert(!MapEntityUsesMpBase(NULL));
    assert(MapEntityUsesCoopAddon("coop","mp_fav_tropical","coop_take_mp_fav_tropical"));
    assert(!MapEntityUsesCoopAddon("coop","mp_fav_tropical","mp_fav_tropical"));
    assert(!MapEntityUsesCoopAddon("coop","mp_fav_tropical",NULL));
    /* A legacy campaign/survival session ID cannot override category ownership. */
    assert(!MapEntityUsesCoopAddon("survival","mp_fav_tropical","coop_take_mp_fav_tropical"));
    assert(!MapEntityUsesCoopAddon("survival","mp_fav_tropical","coop_survival_mp_fav_tropical"));
    assert(!MapEntityUsesCoopAddon("mp","mp_fav_tropical","coop_take_mp_fav_tropical"));
    assert(!MapEntityUsesCoopAddon("zm","zm_factory_sh","coop_take_mp_fav_tropical"));
    assert(!MapEntityUsesCoopAddon(NULL,"mp_fav_tropical","coop_take_mp_fav_tropical"));
    puts("Map entity ownership: MP/Survival base, category-owned coop addons pass");
    return 0;
}

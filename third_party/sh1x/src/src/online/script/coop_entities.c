/* Coop (Spec Ops / campaign) map-entity spawn aliases.
 *
 * The nine coop playlist zones each ship an addon_map_ents overlay
 * (docs/ADDON_MAP_ENTS_20260920.md) written in IW4's SINGLEPLAYER
 * classname vocabulary.  Its trigger volumes carry the SP subclasses
 * trigger_multiple_spawn, trigger_multiple_flag_set,
 * trigger_multiple_flag_set_touching, trigger_multiple_breachIcon and
 * trigger_use_flag_set, and its goal volumes info_volume_pmcDefend and
 * info_volume_breachroom (MEASURED, docs/CAMPAIGN_MODE.md section 3).
 *
 * IW4 SP spawns every one of those through its base classname's spawn
 * function and lets maps\_load / maps\_spawner give the subclass its
 * behaviour from script, keyed on the classname that stays on the entity.
 * This server's G_SpawnEntitiesFromString matches classnames EXACTLY
 * (src/asmsource/_g_spawn_mp.asm, s_bspOnlySpawns), so an SP subclass falls
 * through to a bare G_Spawn: the entity exists with its key/values but has no
 * brush volume, never links as a trigger and never notifies "trigger".  The
 * coop mission script (coop/maps/mp/gametypes/_coopmission.gsc) could not
 * drive a single spawn or flag from it.
 *
 * CoopSpawnClassAlias answers the base classname whose spawn function
 * the entity should run.  It is consulted only for the TABLE LOOKUP: the
 * entity's own classname is still parsed from its spawn variables, so
 * getEntArray("trigger_multiple_flag_set", "classname") keeps working exactly
 * as IW4 SP scripts expect.
 *
 * MP ISOLATION.  The alias is applied only when the mounted asset category is
 * "coop" (ServerZoneType, latched before the entity string is parsed).
 * For "mp" and "zm" this returns its argument unchanged, so the spawn path
 * those categories take is byte-for-byte the one they took before.  No base
 * mp or zm map authors any of these SP subclasses either (the addon overlay is
 * coop-only), so even the comparison cost is confined to coop maps' strings.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include <string.h>
#include "sv_state.h"

typedef struct {
    const char *prefix;
    unsigned length;
    const char *base;
} coop_class_alias_t;

/* Longest-prefix-first is not needed: no prefix here is a prefix of another
 * entry's base classname, and each base is an s_bspOnlySpawns/
 * s_bspOrDynamicSpawns name. */
static const coop_class_alias_t coop_aliases[] = {
    { "trigger_multiple_", 17, "trigger_multiple" },
    { "trigger_radius_",   15, "trigger_radius" },
    { "trigger_once_",     13, "trigger_once" },
    { "trigger_damage_",   15, "trigger_damage" },
    { "info_volume_",      12, "info_volume" },
};

int CoopSpawnAliasesActive(void)
{
    const char *zone = ServerZoneType();
    return zone && !strcmp(zone, "coop");
}

const char *CoopSpawnClassAlias(const char *classname)
{
    unsigned i;
    if (!classname || !CoopSpawnAliasesActive())
        return classname;
    /* trigger_use_touch is a native classname of its own and keeps it;
     * trigger_use_flag_set and its siblings are SP subclasses of trigger_use. */
    if (!strncmp(classname, "trigger_use_", 12) && strcmp(classname, "trigger_use_touch"))
        return classname[12] ? "trigger_use" : classname;
    for (i = 0; i < sizeof(coop_aliases) / sizeof(coop_aliases[0]); ++i)
        if (!strncmp(classname, coop_aliases[i].prefix, coop_aliases[i].length) &&
            classname[coop_aliases[i].length])
            return coop_aliases[i].base;
    return classname;
}
#endif

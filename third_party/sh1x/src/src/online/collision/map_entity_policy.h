/* Entity ownership differs from the spelling of a lobby session ID.
 * Survival's coop_survival_* alias is still an MP base + authored Survival
 * GSC placements, never a request to compose a campaign addon_map_ents. */
#ifndef MAP_ENTITY_POLICY_H
#define MAP_ENTITY_POLICY_H
#include <string.h>

static inline int MapEntityUsesMpBase(const char *category)
{
    return category && (!strcmp(category,"mp") || !strcmp(category,"survival"));
}
static inline int MapEntityUsesCoopAddon(const char *category,
                                             const char *base_map,const char *session_map)
{
    return category && !strcmp(category,"coop") && base_map && *base_map &&
           session_map && *session_map && strcmp(base_map,session_map)!=0;
}
#endif

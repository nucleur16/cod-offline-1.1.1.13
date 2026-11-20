/* Native target admission for mixed Survival waves and MP attack dogs.
 * T5 Actor_ShouldIgnoreFromAttackerCount retains the current enemy and caps
 * new candidates. Survival is an authored mixed wave, not a two-dog MP
 * call-in: its infantry already consume that MP cap before dogs spawn.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ACTOR_THREAT_LIMITS_H
#define ACTOR_THREAT_LIMITS_H
#include <string.h>
static inline int ActorCandidateWithinAttackerLimit(const char *category,
    int isCurrentEnemy, int attackerCount, int configuredLimit, int actorCapacity)
{
    int limit = category && !strcmp(category,"survival") ? actorCapacity : configuredLimit;
    return isCurrentEnemy || attackerCount < limit;
}
#endif

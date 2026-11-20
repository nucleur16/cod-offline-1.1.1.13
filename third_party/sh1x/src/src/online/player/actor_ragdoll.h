/* Actor ragdolls: C1 startRagdollFromImpact, T5 launchRagdoll, and keeping a
 * started ragdoll across the actor's frame. See actor_ragdoll.c.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ACTOR_RAGDOLL_H
#define ACTOR_RAGDOLL_H
#ifdef OL_SERVER
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
typedef struct { int pos, apos; } ragdoll_hold_t;
void ActorRagdollHold(const struct gentity_s *ent, ragdoll_hold_t *hold);
void ActorRagdollRestore(struct gentity_s *ent, const ragdoll_hold_t *hold);
void ActorTraceEvents(const struct gentity_s *ent);
#ifdef __cplusplus
}
#endif
#endif
#endif

/* CoDOL actor-contact stuck probe (developer diagnostic, developer 1 only).
 * See actor_contact_probe.c for the verbs.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ACTOR_CONTACT_PROBE_H
#define ACTOR_CONTACT_PROBE_H
#ifdef __cplusplus
extern "C" {
#endif
#pragma pack(push, 8)
/* One live actor's movement frame as ActorRunFrame saw it. */
typedef struct {
    const char *species;
    int route;          /* ActorGoalDirection verdict this frame */
    int attacking, turning, hasGoal;
    int enemyNum;       /* -1 none */
    int goalSet, inRange; /* dog attack-spot verdicts, -1 not evaluated */
    float goal[3], dir[3];
    float rawDir[2];    /* route direction before separation steering */
    float steer;        /* magnitude of the separation steering vector */
    float meleeDistance;
    int physAccepted, physRejected, physBlocked, deltaFails;
    int routeMode;      /* 1 walking waypoint nodes, 0 the straight leg to the goal */
} actor_contact_sample_t;
#pragma pack(pop)
int ActorContactProbeActive(void);
void ActorContactProbeSample(struct gentity_s *entity, const actor_contact_sample_t *sample);
void RegisterActorContactProbe(void);
#ifdef __cplusplus
}
#endif
#endif

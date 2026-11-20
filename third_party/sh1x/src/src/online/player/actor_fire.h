#ifndef ACTOR_FIRE_H
#define ACTOR_FIRE_H
/* Coop actor weapon fire (actor_fire.c).  No structs. */
struct gentity_s;
#ifdef __cplusplus
extern "C" {
#endif
/* The bullet/projectile weapon's index, or 0 with *why set. */
int ActorWeaponIndex(const char *name, const char **why);
/* The weapon the actor's entity state names (C1 reads it for the DObj
 * and for every fire event on the actor). */
void ActorSetWeapon(struct gentity_s *actor, int weaponIndex);
/* One round from start toward end, owned by the actor, plus the actor's
 * EV_FIRE_WEAPON event. */
void ActorFireWeapon(struct gentity_s *actor, int weaponIndex,
                           const float *start, const float *end);
#ifdef __cplusplus
}
#endif
#endif

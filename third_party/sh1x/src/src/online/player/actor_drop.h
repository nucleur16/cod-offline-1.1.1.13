#ifndef ACTOR_DROP_H
#define ACTOR_DROP_H
struct gentity_s;
#ifdef __cplusplus
extern "C" {
#endif
/* Drop the actor's actual held gun/launcher through the native item lifecycle.
 * Success detaches the hand model and consumes the actor weapon, so a second
 * death/cleanup callback cannot duplicate the pickup. */
struct gentity_s *ActorDropWeapon(struct gentity_s *actor);
#ifdef __cplusplus
}
#endif
#endif

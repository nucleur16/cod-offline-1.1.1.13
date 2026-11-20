#ifndef ACTOR_AMMO_H
#define ACTOR_AMMO_H
struct gentity_s;
#ifdef __cplusplus
extern "C" {
#endif
void ActorAmmoReset(void);
void ActorAmmoForget(struct gentity_s *actor);
void ActorAmmoEquip(struct gentity_s *actor,int weapon);
int ActorAmmoConsume(struct gentity_s *actor,int weapon);
int ActorAmmoClip(struct gentity_s *actor,int weapon);
int ActorAmmoCapacity(struct gentity_s *actor);
int ActorAmmoRemaining(struct gentity_s *actor);
int ActorAmmoReloadComplete(struct gentity_s *actor);
#ifdef __cplusplus
}
#endif
#endif

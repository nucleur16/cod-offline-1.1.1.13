#ifndef ACTOR_DAMAGE_H
#define ACTOR_DAMAGE_H
#include "g_shared.h"
#ifdef __cplusplus
extern "C" {
#endif
void ActorPain(gentity_s *,gentity_s *,int,const float *,int,const float *,hitLocation_t,int);
void ActorDie(gentity_s *,gentity_s *,gentity_s *,int,int,int,const float *,hitLocation_t,int);
#ifdef __cplusplus
}
#endif
#endif

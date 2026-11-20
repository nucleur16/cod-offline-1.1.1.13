#ifndef RC_SPAWN_INFLUENCE_H
#define RC_SPAWN_INFLUENCE_H
#include "scr_vm.h"
#ifdef __cplusplus
extern "C" {
#endif
void Scr_SetRCSpawnInfluencer(scr_entref_t);
void Scr_RCSpawnWeight(void);
void ResetRCSpawnInfluencers(void);
void ResetRCSpawnInfluencer(unsigned);
#ifdef __cplusplus
}
#endif
#endif

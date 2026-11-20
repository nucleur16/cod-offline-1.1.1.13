#ifndef SCRIPT_PLAYERUSE_H
#define SCRIPT_PLAYERUSE_H
#ifdef OL_SERVER
#include "scr_vm.h"
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
void Scr_DisablePlayerUse(scr_entref_t);
void Scr_EnablePlayerUse(scr_entref_t);
int PlayerUseAllowed(const struct gentity_s *player,const struct gentity_s *target);
float PlayerUseRadius(void);
void PlayerUseResetClient(int slot);
void PlayerUseResetAll(void);
void PlayerUseClearTarget(int number);
#ifdef __cplusplus
}
#endif
#endif
#endif

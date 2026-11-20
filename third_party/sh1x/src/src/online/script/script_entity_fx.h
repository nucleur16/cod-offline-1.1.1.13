#ifndef SCRIPT_ENTITY_FX_H
#define SCRIPT_ENTITY_FX_H
#include "scr_vm.h"
/* Player-only method; registered native source, not runtime-verified. */
void PlayerCmd_PlayFX(scr_entref_t ref);
void Scr_PlayFXOnTagForClients(void);
void PlayerCmd_PlayViewModelFXOnTag(scr_entref_t ref);
void PlayerCmd_StopViewModelFXOnTag(scr_entref_t ref);
void Scr_ReserveEntityFx(void);
void PlayerCmd_ViewmodelFXReady(scr_entref_t ref);
#endif

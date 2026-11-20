#ifndef REVIVE_TOKEN_H
#define REVIVE_TOKEN_H

#include "scr_vm.h"

void ResetReviveTokenClient(unsigned client);
void TokenConsumed_f(void);
void TokenInventory_f(void);
void Scr_GetReviveTokenCount(scr_entref_t ref);
void Scr_ArmReviveToken(scr_entref_t ref);
void Scr_RequestReviveTokenSpend(scr_entref_t ref);
void Scr_ClearReviveToken(scr_entref_t ref);

#endif

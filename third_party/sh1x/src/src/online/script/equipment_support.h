/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef EQUIPMENT_SUPPORT_H
#define EQUIPMENT_SUPPORT_H
#include "scr_vm.h"
#include "entities.h"
#ifdef __cplusplus
extern "C" {
#endif
void Scr_MakeScrambler(scr_entref_t ref);
void Scr_SetScramblerActive(scr_entref_t ref);
int OverlayEquipmentScrambler(entity_state_t *wire,const entityState_t *entity);
#ifdef __cplusplus
}
#endif
#endif

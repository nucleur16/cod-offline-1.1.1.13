#ifndef ACTION_SLOTS_H
#define ACTION_SLOTS_H
#include "player.h"
#include "scr_vm.h"
#include "playerstate.h"
#include <stdint.h>
#define ACTION_SLOT_COUNT 5
#pragma pack(push, 8)
typedef struct {
    uint16_t weapon[ACTION_SLOT_COUNT];
    uint8_t type[ACTION_SLOT_COUNT];
} action_slots_t;
#pragma pack(pop)
#ifdef __cplusplus
extern "C" {
#endif
void Scr_SetActionSlot(scr_entref_t ref);
void ResetClientActionSlotState(unsigned client);
int OverlayActionSlots(player_fields_t *wire,const playerState_t *ps);
#ifdef __cplusplus
}
#endif
#endif

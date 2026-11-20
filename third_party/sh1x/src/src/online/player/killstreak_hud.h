#ifndef KILLSTREAK_HUD_H
#define KILLSTREAK_HUD_H
#include "player.h"
#include "scr_vm.h"
#include "playerstate.h"
#include <stdint.h>
#pragma pack(push, 8)
typedef struct {
    uint16_t count;
    uint8_t id[5];
    uint8_t target[5];
} killstreak_hud_t;
#pragma pack(pop)
#ifdef __cplusplus
extern "C" {
#endif
void Scr_SetKillstreak(scr_entref_t ref);
void Scr_GetKillstreak(scr_entref_t ref);
void Scr_SetKillstreakID(scr_entref_t ref);
void Scr_GetKillstreakID(scr_entref_t ref);
void Scr_SetKillstreakTarget(scr_entref_t ref);
void Scr_GetKillstreakTarget(scr_entref_t ref);
void ResetClientKillstreakHud(unsigned client);
int OverlayKillstreakHud(player_fields_t *wire,const playerState_t *ps);
#ifdef __cplusplus
}
#endif
#endif

#ifndef RADAR_TYPES_H
#define RADAR_TYPES_H
#include "player.h"
#include "playerstate.h"
#include <stdint.h>
typedef struct { uint8_t spyplane, satellite, motion_tracker; } radar_types_t;
#ifdef __cplusplus
extern "C" {
#endif
/* Client input slots: 0 spyplane, 1 satellite, 2 motion tracker enabled.
 * Slot2 preserves a script integer; its captured wire value is boolean. */
int GetClientRadarType(unsigned client,unsigned satellite);
void SetClientRadarType(unsigned client,unsigned satellite,int value);
void ResetClientRadarTypes(unsigned client);
void ResetAllRadarTypes(void);
void PublishClientRadarTypes(unsigned client);
int OverlayRadarTypes(player_fields_t *wire,const playerState_t *ps);
void Scr_SetTeamSpyplane(void);
void Scr_GetTeamSpyplane(void);
void Scr_SetTeamSatellite(void);
void Scr_GetTeamSatellite(void);
#ifdef __cplusplus
}
#endif
#endif

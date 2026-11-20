#ifndef REMOTE_VEHICLE_SCRIPT_H
#define REMOTE_VEHICLE_SCRIPT_H
#include "scr_vm.h"
#ifdef __cplusplus
extern "C" {
#endif
void Scr_UseVehicle(scr_entref_t);
void Scr_GetSeatOccupant(scr_entref_t);
void PlayerCmd_IsRemoteControlling(scr_entref_t);
/* Registered as "unlink": remote-control/player-link release, then the
 * retained tag unlink. */
void Scr_Unlink(scr_entref_t);
#ifdef __cplusplus
}
#endif
#endif

/* CoDOL variable-zoom scope state (C1 PM_UpdateVariableZoom 0x418480).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef VARIABLE_ZOOM_H
#define VARIABLE_ZOOM_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 8)

/* C1's three playerState words: ps+0x5c4 level (netfield, 2 bits), ps+0x5c8
 * the time of the last latch change (not networked), ps+0x5cc the latch byte
 * (netfield, 1 bit). */
typedef struct {
    int32_t level;
    int32_t time;
    uint8_t latch;
} variable_zoom_t;

#define VARIABLE_ZOOM_BUTTON 0x4u     /* C1 usercmd melee bit, 0x41851a */
#define VARIABLE_ZOOM_DEBOUNCE_MS 40  /* 0x418515 / 0x418571 */

/* One command of C1 0x418480. scoped is def(ps->weapon)+0x694 != 0, pos_frac
 * ps->fWeaponPosFrac, fov the viewmodel weapon's +0x944[3]. Returns 1 when the
 * command changed the level, i.e. when C1 queues EV_SCOPE_ZOOM (0x418545). */
int VariableZoomStep(variable_zoom_t *zoom, int scoped, float pos_frac,
                           const float fov[3], int32_t server_time, uint32_t buttons);

/* Per-client live state. C1 keeps it in the playerState, which ClientSpawn
 * zeroes and PM_BeginWeaponChange clears (0x4197c2..0x4197ce). */
variable_zoom_t *VariableZoomClient(unsigned client);
void VariableZoomResetClient(unsigned client);
void VariableZoomResetAll(void);
/* A snapshot carrying this client's own playerState was written. ps+0x5c8,
 * the debounce time, is not a netfield, so C1 replays the commands after that
 * snapshot from time 0; the server takes the same zero from here on, so its
 * decision for each later command is the one C1 makes from the newest
 * snapshot it holds, and both queue EV_SCOPE_ZOOM on the same command. */
void VariableZoomSnapshotSent(unsigned client);
/* The two netfields for a playerState describing this client: ps+0x5c4 level
 * (2 bits) and ps+0x5cc latch (1 bit). Zero for an unknown client. */
void VariableZoomWire(int client, uint32_t *level, uint32_t *latch);

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
#endif

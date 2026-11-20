/* Wading water: T5's player water level on a level-declared water plane.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef WATER_H
#define WATER_H
#pragma pack(push,8)
#ifdef __cplusplus
extern "C" {
#endif
struct playerState_s;

/* What CM_GetWaterHeight returns where there is no water. */
#define WATER_NONE (-1000000.0f)

/* The water surface height over pos, or WATER_NONE. */
float WaterHeight(const float *pos);
/* T5 PM_GetWaterLevel: 0 dry .. 5 submerged, from the feet's depth. */
int WaterLevelForDepth(float depth);
/* Per server frame from the session->playerstate producer: applies the
 * stance and sprint locks for this player's water level and returns the
 * movement scale the water gives (1 when dry). */
float UpdatePlayerWater(unsigned client, struct playerState_s *ps);
/* A script locked bits (pm_flags deny/no-sprint) the water held: they now
 * belong to the script and outlast the water. */
void WaterYieldBits(struct playerState_s *ps, unsigned bits);
/* The water level and scale last applied to this client (MoveStateText). */
int PlayerWaterLevel(unsigned client, float *scale);

void Scr_SetLevelWaterHeight(void);
void Scr_GetWaterHeight(void);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

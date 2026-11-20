/* Compass objectives widened to C1's 32 slots (script_objectives.c). */
#ifndef SCRIPT_OBJECTIVES_H
#define SCRIPT_OBJECTIVES_H
#include "q_shared.h"
#include <stdint.h>

/* C1 MSG_ReadDeltaPlayerstate loops 32 slots (0x5a1f9c); OBJECTIVE_SLOTS
 * in player_arrays.h states the same number for the wire. */
#define OBJECTIVE_TOTAL 32
/* level.objectives / ps->objective in the retained structs. */
#define OBJECTIVE_RETAINED 16
#define OBJECTIVE_NO_ENTITY 0x7ff

/* Called from G_InitGame right after level is cleared (map load and
 * map_restart alike). */
void ResetObjectives(void);
/* Slots 16..31 as client `client` sees them: G_RunFrame's team filter for
 * the slots it does not copy into ps->objective. out holds 16 entries. */
void FillHighObjectives(int client, objective_t *out);

void OL_Scr_Objective_Add(void);
void OL_Scr_Objective_Delete(void);
void OL_Scr_Objective_State(void);
void OL_Scr_Objective_Icon(void);
void OL_Scr_Objective_Position(void);
void OL_Scr_Objective_OnEntity(void);
void OL_Scr_Objective_Current(void);
void Scr_Objective_Team(void);
void Scr_Objective_Player(void);
/* Bit i set: slot i is restricted by objective_player to a client other
 * than `client`, so that client's snapshot carries it empty. */
uint32_t ObjectiveHiddenMask(int client);
#endif

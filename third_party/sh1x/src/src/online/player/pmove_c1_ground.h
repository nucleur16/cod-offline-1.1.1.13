/* Native C1 ground/air/step movement. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PMOVE_C1_GROUND_H
#define PMOVE_C1_GROUND_H
#include "pmove_c1.h"
void PM_GroundTraceC1(pmove_c1_t *move);
int PM_CheckJumpC1(pmove_c1_t *move);
void PM_NormalMoveC1(pmove_c1_t *move);
void PM_AirMoveC1(pmove_c1_t *move);
void PM_LadderMoveC1(pmove_c1_t *move);
void PM_MeleeChargeMoveC1(pmove_c1_t *move);
void PM_FreeMoveC1(pmove_c1_t *move);
void PM_SpectatorMoveC1(pmove_c1_t *move);
void PM_FootstepsC1(pmove_c1_t *move,int msec);
#endif

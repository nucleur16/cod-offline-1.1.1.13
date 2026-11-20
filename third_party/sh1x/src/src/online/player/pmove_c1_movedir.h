/* C1 movementDir producers and the angle helpers they use.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pure functions, no engine state, so the server mover and the unit test share
 * them. C1 runs the x87 with 24-bit precision and stores every intermediate
 * shown below as a float, so these are float sequences. Addresses are C1.exe
 * 1.1.1.13. */
#ifndef PMOVE_C1_MOVEDIR_H
#define PMOVE_C1_MOVEDIR_H
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif

/* Inlined AngleNormalize180: x = a/360; (x - floor(x + 0.5)) * 360. */
float C1AngleNormalize180(float angle);
/* AngleDelta 0x401140: AngleNormalize180(a - b). */
float C1AngleDelta(float a, float b);
/* vectoyaw 0x6193d0: 0 for x == y == 0, else atan2(y, x) * 57.2957763671875
 * with 360 added below zero. */
float C1VecToYaw(float x, float y);

typedef struct {
    int movement_dir;          /* ps+0xe8 before the call */
    unsigned pm_flags;         /* ps+0x0c */
    unsigned eflags;           /* ps+0xec */
    float prone_direction;     /* ps+0x1a0 */
    float ladder[3];           /* ps+0xac */
    float view_yaw;            /* ps+0x150 */
    signed char forwardmove;   /* pm->cmd +0x1e, after PM_WalkMove's sprint scale */
    signed char rightmove;     /* pm->cmd +0x1f */
    int ground_none;           /* ps+0xa8 == ENTITYNUM_NONE */
    float origin[3];
    float previous_origin[3];  /* pml +0x68 */
    float frametime;           /* pml +0x24 */
} c1_movement_dir_input_t;

/* PM_SetMovementDir 0x410310, called by PM_AirMove 0x4108da, PM_WalkMove
 * 0x410b29 and PM_MeleeChargeMove 0x415ef7/0x415f0a. Returns the new
 * movementDir. Prone (not on a turret): the prone direction relative to the
 * view. Ladder flag: the ladder facing relative to the view. Otherwise the
 * direction actually moved relative to the view, reversed when the command
 * moves backwards; with no movement input, no ground, no movement or less
 * than 5 units a second of it, the previous value is KEPT, not zeroed. Every
 * result is clamped to -90..90. */
int C1MovementDir(const c1_movement_dir_input_t *in);
/* PM_LadderMove 0x415b28..0x415baa: the ladder facing relative to the view,
 * clamped to -75..75, written on every ladder frame. */
int C1LadderMovementDir(const float ladder[3], float view_yaw);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

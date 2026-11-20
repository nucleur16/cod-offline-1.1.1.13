/* C1 PM_Footsteps walking bob: divisor, bob constant and the new bobCycle.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pure function, no engine state, so the server mover and the unit test share
 * it. Addresses are C1.exe 1.1.1.13.
 *
 * PM_Footsteps 0x413910, walking arm (after the turret, airborne/ladder and
 * idle exits):
 *
 *   00413a62  animIndex = stance + (pm_flags & 0x20 ? 4 : 0), the stance
 *             being 0 stand / 1 prone (vh 11) / 2 crouch (vh 40) / 3 last
 *             stand (vh 22), read from ps->viewHeightTarget at 0x413952
 *   00413ab3  divisor = 0x413690(ads, sprinting):
 *               (float)ps->speed
 *               * 0.925 (forward and strafe), then * 0.85 when backward;
 *                 * 0.70 (backward only); * 0.85 (strafe only)
 *               * 0.4 when ads (PMF_ADS or leaning), else * 1.5 when sprinting
 *               * moveSpeedScale (def +0x670) unless PMF_ADS or it is not
 *                 positive, else adsMoveSpeedScale (+0x674) when positive;
 *                 only while ps->weapon is not 0 (0x41370b)
 *               and finally the stance multiplier 0x40ff70 (the PM_CmdScale
 *               one, including its prone lerp)
 *             every step rounded to float (the x87 runs at 24-bit precision)
 *   00413ac1  bob = 0.5 when animIndex is 0 and sprinting, else
 *             table[ads + 2 * animIndex] at 0xc40938: standing backward is
 *             0.36 / 0.325, not the forward 0.335 / 0.305
 *   00413ae4  bobCycle = ftol((float)(xyspeed / divisor * bob) * msec + old)
 *             & 0xff (0xae0410 truncates)
 *
 * The server used the forward constants for a backpedalling player and took
 * the stance from pm_flags instead of the view height, so its bobCycle ran 7%
 * slow while backing up. bobCycle is a hint-3 field: C1 keeps its own copy
 * unless the origin check forces the group, so the two clocks drifted apart and
 * the server's footsteps landed a command or more away from C1's predicted
 * ones. CG_CheckPlayerstateEvents then replayed the footstep and the shot next
 * to it (PARITY event-replay FOOTSTEP <-> EV_SV_FIRE_WEAPON, 2026-10-13). */
#ifndef PMOVE_C1_BOB_H
#define PMOVE_C1_BOB_H
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int speed;                  /* ps->speed */
    int forwardmove, rightmove; /* the command bytes PM_Footsteps reads */
    int ads;                    /* PMF_ADS (0x40) or leanf != 0 */
    int pmf_ads;                /* PMF_ADS alone: the weapon scale selector */
    int sprinting;              /* pm_flags & 0x4000 */
    int has_weapon;             /* ps->weapon != 0 */
    float move_speed_scale, ads_move_speed_scale;
    float stance_scale;         /* C1 0x40ff70 */
    int anim_index;             /* stance + 4 when backward, 0..7 */
    float xyspeed;
    int msec;
    int old_cycle;
} c1_bob_input_t;

/* The divisor C1 0x413690 returns; 0 or less means no bob update. */
float C1BobDivisor(const c1_bob_input_t *in);
/* The bob constant C1 picks at 0x413ac1. */
float C1BobConstant(int anim_index, int ads, int sprinting);
/* The new bobCycle (0..255), or -1 when the divisor is not positive. */
int C1BobCycle(const c1_bob_input_t *in);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

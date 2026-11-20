/* C1 PmoveSingle tail: velocity correction and snap.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pure function, no engine state, so the server mover and the unit test share
 * it. Addresses are C1.exe 1.1.1.13. */
#ifndef PMOVE_C1_TAIL_H
#define PMOVE_C1_TAIL_H
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif

/* C1 PmoveSingle 0x0041697d..0x00416aa7, the last thing the ordinary movement
 * arm does (after PM_Footsteps, PM_Weapon and PM_FoliageSounds; the linked,
 * noclip, ufo, spectator, intermission, turret and mantle arms return before
 * it):
 *
 *   moved = origin - pml.previous_origin
 *   if |velocity|^2 * 0.25 > |moved|^2 / frametime^2:
 *       velocity = moved * (1 / frametime)          (0x416a1e..0x416a4d)
 *   velocity[i] = (float)(int)round(velocity[i])    (fistp/fild 0x416a50..)
 *
 * C1 runs with the x87 precision control at 24 bits, so every step is a float
 * operation; `fistp` rounds to nearest, ties to even. The client applies this
 * to every predicted command, so a server that skips it simulates from an
 * unsnapped velocity and its origin drifts away from the client's prediction
 * while the player accelerates, turns or brushes a wall.
 * A frametime that is not positive leaves the velocity unchanged. */
void PmoveC1VelocityTail(float velocity[3], const float origin[3],
                               const float previous_origin[3], float frametime);

/* C1 PM_DeadMove 0x00410b40, which PmoveSingle 0x004168c8..0x004168d4 runs
 * for a dead player (pm_type >= 8) on the ground (pml.walking), after the
 * timers and before PM_CheckLadder takes the ground away:
 *
 *   forward = |velocity| - 20
 *   forward <= 0: velocity = 0
 *   otherwise:    velocity = normalize(velocity) * forward
 *
 * Twenty units a second off the speed every COMMAND, whatever its length --
 * Quake 3's dead-body brake. The server never ran it, so a body knocked back
 * at death slid on at constant speed (PM_CheckLadder then airborne, no ground
 * friction) while C1 stopped it within a few commands. */
void PmoveC1DeadMove(float velocity[3]);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

/* C1 PM_CmdScale for the air, spectator and ladder moves, the spectator wish
 * vector, and the jump event's surface rule.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pure functions, no engine state, so the server mover and the unit test share
 * them. Addresses are C1.exe 1.1.1.13. */
#ifndef PMOVE_C1_CMDSCALE_H
#define PMOVE_C1_CMDSCALE_H
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif

/* C1 has two command scales. PM_WalkMove (0x4109a1) calls the full one,
 * 0x4100b0 times 0x4102c0: stance, sprint, weapon move speed, shellshock and
 * ps->moveSpeedScaleMultiplier. PM_AirMove (0x4106cf), the spectator move
 * (0x41058d) and the ladder move (0x41582e) call 0x40fea0, which knows none of
 * those:
 *
 *   biggest = max(|forwardmove|, |rightmove|)          integer, 0 -> 0
 *   total   = sqrtf((float)(f*f + r*r))                 0x40feba..0x40fec9
 *   scale   = speed * biggest / (total * 127)           0x40fef9..0x40ff0d
 *   scale  *= 0.4 when PMF_ADS (pm_flags 0x40) or leaning  0x40fef5/0x40ff12
 *   scale  *= 3 noclip, 6 ufo, player_spectateSpeedScale spectating
 *
 * C1 runs the x87 at 24-bit precision, so each step rounds to float. */
float C1AirCmdScale(int speed, int forwardmove, int rightmove,
                          unsigned pm_flags, float leanf, int pm_type,
                          float spectate_speed_scale);

/* C1 spectator move 0x410560 builds its wish vector from the full 3D view
 * axes and, unlike IW3's PM_FlyMove, scales only the forward term:
 *
 *   wishvel[i] = forward[i] * f * scale + right[i] * r   (0x4105cc..0x4105f9)
 *
 * (a zero scale gives a zero vector, 0x4105a9). The vector is normalised into
 * wishdir and its length is the wish speed (0x410614). */
float C1SpectatorWish(const float forward[3], const float right[3],
                            int forwardmove, int rightmove, float scale,
                            float wishdir[3]);

/* C1 PM_JumpStart's event 0x4095d0: on a ladder (pm_flags 0x8) the jump event
 * carries surface 21; otherwise the ground trace's surface type
 * ((sflags >> 20) & 31), and NO event at all when the surface is
 * SURF_NOSTEPS-flagged (sflags 0x2000) or its type is 0. Returns whether C1
 * queues the event and, if so, its parameter. */
int C1JumpEventParm(unsigned pm_flags, unsigned sflags, unsigned *parm);

/* C1 PM_FoliageSounds 0x413b60's test box (0x413c19..0x413c86). C1's pm+0xdc
 * hull is (midpoint, half size) -- (0 0 35) / (15 15 35) standing -- and the
 * foliage test scales BOTH by 0.75 (double 0xc84418), then raises both z by
 * 0.075 (0xc84cc0) * (half.z + mid.z):
 *
 *   standing: mid z 26.25 + 5.25 = 31.5, half z 26.25 + 5.25 = 31.5
 *             -> x/y +-11.25, z 0 .. 63 above the feet
 *
 * The server keeps the hull as (mins, maxs); reading C1's two vectors as mins
 * and maxs put the box 5.25 .. 57.75 above the feet, over every ground-level
 * leaf decal (contents 2) C1's box touches, so C1 predicted foliage sounds the
 * server never queued and replayed its neighbouring events. Takes and returns
 * (mins, maxs). */
void C1FoliageBox(const float hull_mins[3], const float hull_maxs[3],
                        float mins[3], float maxs[3]);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

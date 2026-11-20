/* C1 PmoveSingle tail: velocity correction and snap (see the header).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmove_c1_tail.h"
#include <math.h>

/* `fld dword; fstp dword tmp; fld tmp; fistp dword; fild dword; fstp dword`:
 * round to nearest even through a 32-bit integer, so -0.4 becomes +0.0. */
static float snap_component(float value)
{
    if(!(value > -2147483648.0f && value < 2147483648.0f)) return value;
    return (float)(int)lrintf(value);
}

void PmoveC1VelocityTail(float velocity[3], const float origin[3],
                               const float previous_origin[3], float frametime)
{
    volatile float moved_sq, speed_sq, actual_sq, inverse;
    float moved[3];
    unsigned i;
    if(!velocity || !origin || !previous_origin) return;
    if(frametime > 0.0f) {
        /* 0x41697d..0x41699b: the three differences are stored as floats. */
        for(i = 0; i < 3; ++i) moved[i] = origin[i] - previous_origin[i];
        /* 0x4169bf..0x4169e5: (mx*mx + my*my) + mz*mz, stored; divided by
         * frametime*frametime, stored. */
        moved_sq = moved[0] * moved[0] + moved[1] * moved[1];
        moved_sq = moved_sq + moved[2] * moved[2];
        actual_sq = moved_sq / (frametime * frametime);
        /* 0x4169eb..0x416a03: (vx*vx + vy*vy) + vz*vz, stored. */
        speed_sq = velocity[0] * velocity[0] + velocity[1] * velocity[1];
        speed_sq = speed_sq + velocity[2] * velocity[2];
        /* 0x416a09..0x416a1c: `fmul 0.25; fcompp; test ah,0x41; jne` keeps
         * the velocity unless speed_sq * 0.25 is strictly greater. */
        if(speed_sq * 0.25f > actual_sq) {
            inverse = 1.0f / frametime;
            for(i = 0; i < 3; ++i) velocity[i] = inverse * moved[i];
        }
    }
    for(i = 0; i < 3; ++i) velocity[i] = snap_component(velocity[i]);
}

void PmoveC1DeadMove(float velocity[3])
{
    volatile float length_sq, forward, length, inverse;
    if(!velocity) return;
    /* 0x410b44..0x410b62: (vx*vx + vy*vy) + vz*vz, stored. */
    length_sq = velocity[0] * velocity[0] + velocity[1] * velocity[1];
    length_sq = length_sq + velocity[2] * velocity[2];
    /* 0x410b6b..0x410b79: sqrt, minus the double 20.0, stored. */
    forward = (float)sqrt((double)length_sq) - 20.0f;
    /* 0x410b7c..0x410b86: `fldz; fcom forward; test ah,1; jne` -- keep
     * moving only while 0 < forward. */
    if(!(forward > 0.0f)) {
        velocity[0] = velocity[1] = velocity[2] = 0.0f;
        return;
    }
    /* Vec3Normalize 0x4090e0 (a zero length divides by one), then the
     * scale by forward (0x410b9f..0x410bb6). */
    length = (float)sqrt((double)(velocity[0] * velocity[0] + velocity[1] * velocity[1] +
                                  velocity[2] * velocity[2]));
    if(!(-length < 0.0f)) length = 1.0f;
    inverse = 1.0f / length;
    velocity[0] = velocity[0] * inverse * forward;
    velocity[1] = velocity[1] * inverse * forward;
    velocity[2] = velocity[2] * inverse * forward;
}

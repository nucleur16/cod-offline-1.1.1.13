/* C1 movementDir producers (see the header).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmove_c1_movedir.h"
#include <math.h>
#include <stddef.h>

/* 0xc849d0 (double) holds (float)(1/360); 0xc84248 0.5; 0xc849c8 360. */
#define C1_INV_360 0.0027777778450399637f

float C1AngleNormalize180(float angle)
{
    volatile float scaled, biased, wrapped;
    scaled = angle * C1_INV_360;
    biased = scaled + 0.5f;
    wrapped = scaled - floorf(biased);
    wrapped = wrapped * 360.0f;
    return wrapped;
}

float C1AngleDelta(float a, float b)
{
    volatile float delta = a - b;
    return C1AngleNormalize180(delta);
}

float C1VecToYaw(float x, float y)
{
    volatile float yaw;
    if(y == 0.0f && x == 0.0f) return 0.0f;
    /* 0x42eb90: CRT atan2 on the float arguments, result stored as float;
     * 0xc849d8 is (float)(180/pi). */
    yaw = (float)atan2((double)y, (double)x);
    yaw = yaw * 57.2957763671875f;
    if(yaw < 0.0f) yaw = yaw + 360.0f;
    return yaw;
}

/* ftol (0xae0410, truncation) and the shared clamp 0x410516..0x410533:
 * |dir| > limit becomes -limit for dir <= 0 and +limit otherwise; the
 * result is stored through a signed byte (movsx cl). */
static int clamp_dir(float value, int limit)
{
    int dir = (int)value;
    int magnitude = dir < 0 ? -dir : dir;
    if(magnitude > limit) dir = dir > 0 ? limit : -limit;
    return (int)(signed char)dir;
}

static float ladder_relative(const float ladder[3], float view_yaw)
{
    volatile float facing = C1VecToYaw(ladder[0], ladder[1]) + 180.0f;
    volatile float delta = facing - view_yaw;
    return C1AngleNormalize180(delta);
}

int C1LadderMovementDir(const float ladder[3], float view_yaw)
{
    if(!ladder) return 0;
    return clamp_dir(ladder_relative(ladder, view_yaw), 75);
}

int C1MovementDir(const c1_movement_dir_input_t *in)
{
    volatile float length_sq, length, inverse, yaw, threshold;
    float moved[3], dir[3];
    unsigned i;
    int result;
    if(!in) return 0;
    /* 0x410320: prone, unless the turret eFlags (0xc00) are up. */
    if((in->pm_flags & 1u) && !(in->eflags & 0xc00u))
        return clamp_dir(C1AngleDelta(in->prone_direction, in->view_yaw), 90);
    /* 0x41035b: on a ladder. */
    if(in->pm_flags & 8u)
        return clamp_dir(ladder_relative(in->ladder, in->view_yaw), 90);
    /* 0x4103f1..0x41040f: the move, stored as floats. */
    for(i = 0; i < 3; ++i) moved[i] = in->origin[i] - in->previous_origin[i];
    /* 0x410412..0x410429: no input or no ground keeps the old value. */
    if(!in->forwardmove && !in->rightmove) return in->movement_dir;
    if(in->ground_none) return in->movement_dir;
    /* Vec3NormalizeTo 0x6190b0: length of (mx*mx + my*my) + mz*mz. */
    length_sq = moved[0] * moved[0] + moved[1] * moved[1];
    length_sq = length_sq + moved[2] * moved[2];
    length = sqrtf(length_sq);
    /* 0x410460..0x410481: zero, or at most frametime * 5, keeps it too. */
    if(length == 0.0f) return in->movement_dir;
    threshold = in->frametime * 5.0f;
    if(!(threshold < length)) return in->movement_dir;
    inverse = 1.0f / length;
    for(i = 0; i < 3; ++i) dir[i] = moved[i] * inverse;
    /* vectoangles 0x619610 yaw of the normalised move. */
    yaw = C1VecToYaw(dir[0], dir[1]);
    result = (int)C1AngleDelta(yaw, in->view_yaw);
    /* 0x4104bd: a backward command reverses it, through AngleNormalize180. */
    if(in->forwardmove < 0) {
        volatile float reversed = (float)result + 180.0f;
        return clamp_dir(C1AngleNormalize180(reversed), 90);
    }
    return clamp_dir((float)result, 90);
}

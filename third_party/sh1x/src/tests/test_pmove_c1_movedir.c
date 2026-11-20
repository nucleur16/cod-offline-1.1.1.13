/* C1 PM_SetMovementDir and ladder movementDir (pmove_c1_movedir.c). */
#include "pmove_c1_movedir.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static c1_movement_dir_input_t walking(float dx, float dy, float yaw, signed char f, signed char r)
{
    c1_movement_dir_input_t in;
    memset(&in, 0, sizeof(in));
    in.movement_dir = 37;
    in.view_yaw = yaw;
    in.forwardmove = f;
    in.rightmove = r;
    in.origin[0] = 100.0f + dx; in.origin[1] = 200.0f + dy; in.origin[2] = 8.0f;
    in.previous_origin[0] = 100.0f; in.previous_origin[1] = 200.0f; in.previous_origin[2] = 8.0f;
    in.frametime = 0.008f;
    return in;
}

int main(void)
{
    c1_movement_dir_input_t in;
    float ladder[3] = { 1.0f, 0.0f, 0.0f };

    /* AngleNormalize180 rounds 180 to -180 (x = 0.5, floor(1.0) = 1). */
    assert(C1AngleNormalize180(190.0f) == -170.0f);
    assert(C1AngleNormalize180(180.0f) == -180.0f);
    assert(C1AngleNormalize180(-180.0f) == -180.0f);
    assert(fabsf(C1AngleDelta(10.0f, 350.0f) - 20.0f) < 1e-4f);
    assert(fabsf(C1VecToYaw(0.0f, 1.0f) - 90.0f) < 1e-4f);
    assert(fabsf(C1VecToYaw(0.0f, -1.0f) - 270.0f) < 1e-4f);
    assert(C1VecToYaw(0.0f, 0.0f) == 0.0f);

    /* Moving +y with the view at yaw 0: 90. Moving +x with the view at 30:
     * -30. Strafing at 45 degrees truncates toward zero. */
    in = walking(0.0f, 2.0f, 0.0f, 127, 0);
    assert(C1MovementDir(&in) == 90);
    in = walking(2.0f, 0.0f, 30.0f, 127, 0);
    assert(C1MovementDir(&in) == -30);
    in = walking(1.4f, 1.4f, 0.4f, 127, 127);
    assert(C1MovementDir(&in) == 44);

    /* Backwards: moving -x facing +x is a delta of -180, reversed to 0. */
    in = walking(-2.0f, 0.0f, 0.0f, -127, 0);
    assert(C1MovementDir(&in) == 0 || C1MovementDir(&in) == 1);
    /* Backpedal-strafe: moving (-1,-1) facing +x, delta -135, reversed 45. */
    in = walking(-1.4f, -1.4f, 0.0f, -127, -127);
    assert(C1MovementDir(&in) == 45 || C1MovementDir(&in) == 46);

    /* No input, no ground, no move or at most 5 units/s keep the old value. */
    in = walking(0.0f, 2.0f, 0.0f, 0, 0);
    assert(C1MovementDir(&in) == 37);
    in = walking(0.0f, 2.0f, 0.0f, 127, 0); in.ground_none = 1;
    assert(C1MovementDir(&in) == 37);
    in = walking(0.0f, 0.0f, 0.0f, 127, 0);
    assert(C1MovementDir(&in) == 37);
    in = walking(0.0f, 0.03f, 0.0f, 127, 0);    /* under 5 ups at 8 ms */
    assert(C1MovementDir(&in) == 37);
    /* The length is 3D: a vertical move counts toward the threshold. */
    in = walking(0.0f, 0.03f, 0.0f, 127, 0); in.origin[2] += 0.05f;
    assert(C1MovementDir(&in) != 37);

    /* Prone: the prone direction relative to the view, clamped to 90. */
    in = walking(0.0f, 2.0f, 0.0f, 127, 0); in.pm_flags = 1u; in.prone_direction = 40.5f;
    assert(C1MovementDir(&in) == 40);
    in.prone_direction = 100.0f;
    assert(C1MovementDir(&in) == 90);
    /* Prone on a turret uses the moved direction instead. */
    in.eflags = 0x400u;
    assert(C1MovementDir(&in) == 90);

    /* Ladder flag inside PM_SetMovementDir: ladder facing, clamp 90.
     * PM_LadderMove's own write clamps to 75. */
    in = walking(0.0f, 2.0f, 0.0f, 127, 0); in.pm_flags = 8u;
    in.ladder[0] = 0.0f; in.ladder[1] = -1.0f;   /* faces yaw 90 */
    assert(C1MovementDir(&in) == 89 || C1MovementDir(&in) == 90);
    assert(C1LadderMovementDir(ladder, 0.0f) == -75);
    ladder[0] = 0.0f; ladder[1] = -1.0f;
    assert(C1LadderMovementDir(ladder, 60.0f) == 29 || C1LadderMovementDir(ladder, 60.0f) == 30);

    puts("test_pmove_c1_movedir: ok");
    return 0;
}

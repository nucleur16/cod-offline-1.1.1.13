/* C1 PmoveSingle velocity tail (src/online/player/pmove_c1_tail.c). */
#include "pmove_c1_tail.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void set3(float v[3], float x, float y, float z) { v[0] = x; v[1] = y; v[2] = z; }

int main(void)
{
    float velocity[3], origin[3], previous[3];

    /* Unblocked movement: only the snap, ties to even, -0.4 to +0. */
    set3(previous, 0.0f, 0.0f, 0.0f);
    set3(origin, 1.733f, -0.1f, 0.0f);
    set3(velocity, 216.7f, -12.5f, -0.4f);
    PmoveC1VelocityTail(velocity, origin, previous, 0.008f);
    assert(velocity[0] == 217.0f && velocity[1] == -12.0f && velocity[2] == 0.0f);
    assert(!signbit(velocity[2]));
    set3(velocity, 13.5f, 14.5f, -3.5f);
    set3(origin, 0.108f, 0.116f, -0.028f);
    PmoveC1VelocityTail(velocity, origin, previous, 0.008f);
    assert(velocity[0] == 14.0f && velocity[1] == 14.0f && velocity[2] == -4.0f);

    /* Blocked by a wall: 300 ups requested, 0.5 units covered in 8 ms, so the
     * velocity becomes the covered 62.5 ups, snapped to 62. */
    set3(velocity, 300.0f, 0.0f, 0.0f);
    set3(origin, 0.5f, 0.0f, 0.0f);
    PmoveC1VelocityTail(velocity, origin, previous, 0.008f);
    assert(velocity[0] == 62.0f && velocity[1] == 0.0f && velocity[2] == 0.0f);

    /* Exactly half the speed is not "blocked" (strict compare at 0x416a1c). */
    set3(velocity, 300.0f, 0.0f, 0.0f);
    set3(origin, 1.5f, 0.0f, 0.0f);   /* 1.5 / 0.01 = 150 ups */
    PmoveC1VelocityTail(velocity, origin, previous, 0.01f);
    assert(velocity[0] == 300.0f);
    /* Just under half is. */
    set3(velocity, 300.0f, 0.0f, 0.0f);
    set3(origin, 1.49f, 0.0f, 0.0f);
    PmoveC1VelocityTail(velocity, origin, previous, 0.01f);
    assert(velocity[0] == 149.0f);

    /* A stationary player with leftover fractional speed: moved 0, speed
     * squared * 0.25 > 0, so the velocity collapses to zero. */
    set3(velocity, 0.6f, -0.7f, 0.0f);
    set3(origin, 10.0f, 10.0f, 10.0f);
    set3(previous, 10.0f, 10.0f, 10.0f);
    PmoveC1VelocityTail(velocity, origin, previous, 0.016f);
    assert(velocity[0] == 0.0f && velocity[1] == 0.0f && velocity[2] == 0.0f);

    /* No frametime: snap only. */
    set3(velocity, 1.6f, 2.4f, 0.0f);
    PmoveC1VelocityTail(velocity, origin, previous, 0.0f);
    assert(velocity[0] == 2.0f && velocity[1] == 2.0f);

    /* PM_DeadMove: 20 off the length per command, direction kept. */
    set3(velocity, 47.0f, 0.0f, 0.0f);
    PmoveC1DeadMove(velocity);
    /* 47 * (1/47) rounds to just under one, as C1's float x87 does. */
    assert(fabsf(velocity[0] - 27.0f) < 1e-4f && velocity[1] == 0.0f && velocity[2] == 0.0f);
    PmoveC1DeadMove(velocity);
    assert(fabsf(velocity[0] - 7.0f) < 1e-4f);
    PmoveC1DeadMove(velocity);
    assert(velocity[0] == 0.0f && velocity[1] == 0.0f && velocity[2] == 0.0f);
    set3(velocity, 30.0f, -40.0f, 0.0f);   /* |v| 50 -> 30, a 3-4-5 */
    PmoveC1DeadMove(velocity);
    assert(fabsf(velocity[0] - 18.0f) < 1e-4f && fabsf(velocity[1] + 24.0f) < 1e-4f);
    set3(velocity, 0.0f, 0.0f, -20.0f);    /* exactly 20: stops (0 < 0 fails) */
    PmoveC1DeadMove(velocity);
    assert(velocity[2] == 0.0f);
    set3(velocity, 0.0f, 0.0f, 0.0f);
    PmoveC1DeadMove(velocity);
    assert(velocity[0] == 0.0f);

    puts("test_pmove_c1_tail: ok");
    return 0;
}

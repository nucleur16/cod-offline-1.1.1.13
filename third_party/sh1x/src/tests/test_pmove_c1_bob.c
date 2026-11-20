/* C1 PM_Footsteps walking bob (src/online/player/pmove_c1_bob.c). */
#include "pmove_c1_bob.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static c1_bob_input_t base(void)
{
    c1_bob_input_t in;
    memset(&in, 0, sizeof(in));
    in.speed = 190;
    in.forwardmove = 127;
    in.has_weapon = 1;
    in.move_speed_scale = 1.0f;
    in.ads_move_speed_scale = 2.0f;
    in.stance_scale = 1.0f;
    in.xyspeed = 190.0f;
    in.msec = 8;
    return in;
}

int main(void)
{
    c1_bob_input_t in;

    /* The sixteen-entry table at 0xc40938. */
    assert(C1BobConstant(0, 0, 0) == 0.335f);
    assert(C1BobConstant(0, 1, 0) == 0.305f);
    assert(C1BobConstant(2, 0, 0) == 0.34f);   /* crouch */
    assert(C1BobConstant(1, 1, 0) == 0.24f);   /* prone, ads */
    /* Standing backward is NOT the forward pair. */
    assert(C1BobConstant(4, 0, 0) == 0.36f);
    assert(C1BobConstant(4, 1, 0) == 0.325f);
    assert(C1BobConstant(6, 1, 0) == 0.315f);  /* crouch backward, ads */
    /* Sprint 0.5 only when standing forward. */
    assert(C1BobConstant(0, 0, 1) == 0.5f);
    assert(C1BobConstant(2, 0, 1) == 0.34f);

    /* Forward run: divisor = 190 * mss 1.0 * stance 1.0. */
    in = base();
    assert(C1BobDivisor(&in) == 190.0f);
    /* 190/190 * 0.335 * 8 = 2.68 -> 2. */
    assert(C1BobCycle(&in) == 2);
    in.old_cycle = 254;
    assert(C1BobCycle(&in) == 0);              /* wraps */

    /* Backward ADS walk: 0.7 * 0.4 * adsMoveSpeedScale, bob 0.325. */
    in = base();
    in.forwardmove = -127;
    in.ads = in.pmf_ads = 1;
    in.anim_index = 4;
    in.xyspeed = 53.0f;
    {
        float d = C1BobDivisor(&in);
        float expect = (float)((double)(float)((double)190.0f * 0.699999988079071) * 0.4000000059604645) * 2.0f;
        assert(d == expect);
    }
    /* The backward constant runs the clock faster than the forward one the
     * server used before: 53 / 106.4 * 0.325 * 50 = 8.09 against 7.59. */
    {
        c1_bob_input_t fwd = in;
        in.msec = fwd.msec = 50;
        in.old_cycle = fwd.old_cycle = 0;
        fwd.anim_index = 0;
        assert(C1BobCycle(&in) == 8);
        assert(C1BobCycle(&fwd) == 7);
    }

    /* Strafe only and forward+strafe constants. */
    in = base();
    in.forwardmove = 0; in.rightmove = 127;
    assert(C1BobDivisor(&in) == (float)((double)190.0f * 0.8500000089406967));
    in.forwardmove = 127;
    assert(C1BobDivisor(&in) == (float)((double)190.0f * 0.9250000044703484));
    in.forwardmove = -127;
    assert(C1BobDivisor(&in) ==
           (float)((double)(float)((double)190.0f * 0.9250000044703484) * 0.8499999940395355));

    /* Sprint multiplies by 1.5; ads wins over sprint. */
    in = base();
    in.sprinting = 1;
    assert(C1BobDivisor(&in) == 285.0f);
    in.ads = in.pmf_ads = 1;
    assert(C1BobDivisor(&in) == (float)((double)190.0f * 0.4000000059604645) * 2.0f);

    /* Leaning (ads without PMF_ADS) keeps moveSpeedScale. */
    in = base();
    in.ads = 1; in.pmf_ads = 0;
    in.move_speed_scale = 0.9f;
    assert(C1BobDivisor(&in) == (float)((double)190.0f * 0.4000000059604645) * 0.9f);

    /* No weapon: no weapon scale (0x41370b). */
    in = base();
    in.has_weapon = 0; in.move_speed_scale = 0.5f;
    assert(C1BobDivisor(&in) == 190.0f);

    /* Stance multiplier applied last; a zero divisor is no update. */
    in = base();
    in.stance_scale = 0.65f;
    assert(C1BobDivisor(&in) == 0.65f * 190.0f);
    in.speed = 0;
    assert(C1BobCycle(&in) == -1);

    puts("test_pmove_c1_bob: ok");
    return 0;
}

/* C1 air/spectator command scale, spectator wish vector and jump event rule
 * (src/online/player/pmove_c1_cmdscale.c). */
#include "pmove_c1_cmdscale.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int close_to(float a, float b) { return fabsf(a - b) <= 1e-4f * (fabsf(b) > 1.0f ? fabsf(b) : 1.0f); }

int main(void)
{
    float dir[3], speed;
    unsigned parm = 0;
    static const float forward[3] = { 0.0f, 1.0f, 0.0f };
    static const float right[3] = { 1.0f, 0.0f, 0.0f };
    static const float pitched[3] = { 0.0f, 0.70710677f, -0.70710677f };

    /* Forward only: the full ps->speed, no stance/sprint/weapon terms. The
     * walk scale for a rifle class (0.95) gave 180.5; C1's air move asks 190. */
    assert(close_to(C1AirCmdScale(190, 127, 0, 0, 0.0f, 0, 1.0f) * 127.0f, 190.0f));
    /* Sprint (0x4000), crouch (0x2) and jumping (0x2000) flags change nothing. */
    assert(C1AirCmdScale(190, 127, 0, 0x6002u, 0.0f, 0, 1.0f) ==
           C1AirCmdScale(190, 127, 0, 0, 0.0f, 0, 1.0f));
    /* Diagonal: biggest / total = 1 / sqrt(2). */
    assert(close_to(C1AirCmdScale(190, 127, -127, 0, 0.0f, 0, 1.0f),
                    190.0f / (sqrtf(2.0f) * 127.0f)));
    /* The sprint walk entry's rightmove rewrite (84) is what the air move sees. */
    assert(close_to(C1AirCmdScale(190, 127, 84, 0, 0.0f, 0, 1.0f),
                    190.0f * 127.0f / (sqrtf(127.0f * 127.0f + 84.0f * 84.0f) * 127.0f)));
    /* No input: zero. */
    assert(C1AirCmdScale(190, 0, 0, 0, 0.0f, 0, 1.0f) == 0.0f);
    /* ADS flag or lean: 0.4. */
    assert(close_to(C1AirCmdScale(190, 127, 0, 0x40u, 0.0f, 0, 1.0f) * 127.0f, 76.0f));
    assert(close_to(C1AirCmdScale(190, 127, 0, 0, 0.5f, 0, 1.0f) * 127.0f, 76.0f));
    /* noclip x3, ufo x6, spectator x player_spectateSpeedScale. */
    assert(close_to(C1AirCmdScale(190, 127, 0, 0, 0.0f, 2, 1.0f) * 127.0f, 570.0f));
    assert(close_to(C1AirCmdScale(190, 127, 0, 0, 0.0f, 3, 1.0f) * 127.0f, 1140.0f));
    assert(close_to(C1AirCmdScale(190, 127, 0, 0, 0.0f, 5, 2.0f) * 127.0f, 380.0f));

    /* Spectator: forward term scaled, right term not. */
    speed = C1SpectatorWish(forward, right, 127, 0, 1.0f, dir);
    assert(close_to(speed, 127.0f) && close_to(dir[1], 1.0f) && dir[0] == 0.0f);
    speed = C1SpectatorWish(forward, right, 127, 0, 2.0f, dir);
    assert(close_to(speed, 254.0f));
    speed = C1SpectatorWish(forward, right, 0, 127, 2.0f, dir);
    assert(close_to(speed, 127.0f) && close_to(dir[0], 1.0f));
    /* The 3D view axis is kept: looking down flies down. */
    speed = C1SpectatorWish(pitched, right, 127, 0, 1.0f, dir);
    assert(close_to(speed, 127.0f) && dir[2] < -0.7f);
    /* A zero scale is a zero wish. */
    speed = C1SpectatorWish(forward, right, 0, 127, 0.0f, dir);
    assert(speed == 0.0f && dir[0] == 0.0f && dir[1] == 0.0f && dir[2] == 0.0f);

    /* Jump event: surface type from sflags >> 20. */
    assert(C1JumpEventParm(0, 0x500000u, &parm) && parm == 5);
    assert(C1JumpEventParm(0, 0x1500000u, &parm) && parm == 21);
    /* No event on a type-0 surface or a SURF_NOSTEPS one. */
    assert(!C1JumpEventParm(0, 0x0u, &parm));
    assert(!C1JumpEventParm(0, 0x502000u, &parm));
    /* A ladder jump always names surface 21. */
    assert(C1JumpEventParm(0x8u, 0x0u, &parm) && parm == 21);

    /* Foliage box: C1 scales (mid, half), so it runs from the feet to 0.9
     * of the hull height; reading the hull as mins/maxs gave 5.25 .. 57.75. */
    {
        const float smins[3] = { -15.0f, -15.0f, 0.0f }, smaxs[3] = { 15.0f, 15.0f, 70.0f };
        const float cmaxs[3] = { 15.0f, 15.0f, 50.0f };
        float fmins[3], fmaxs[3];
        C1FoliageBox(smins, smaxs, fmins, fmaxs);
        assert(fmins[0] == -11.25f && fmins[1] == -11.25f && fabsf(fmins[2]) < 1e-5f);
        assert(fmaxs[0] == 11.25f && fmaxs[1] == 11.25f && fabsf(fmaxs[2] - 63.0f) < 1e-4f);
        C1FoliageBox(smins, cmaxs, fmins, fmaxs);
        assert(fabsf(fmins[2]) < 1e-5f && fabsf(fmaxs[2] - 45.0f) < 1e-4f);
    }

    printf("test_pmove_c1_cmdscale: ok\n");
    return 0;
}

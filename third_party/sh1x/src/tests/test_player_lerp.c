/* The even publication timeline for human players (player_lerp.c).
 * Drives the engine-free algorithm with the measured C1 command cadence:
 * a command every ~11 ms, sent three per ~34 ms packet, executed a round trip
 * after its serverTime, published by a 60 Hz server. */
#include "player_lerp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SPEED 190.0f /* u/s, a running player */

static player_lerp_t lerp;

static void record_run(int time, float speed)
{
    float o[3] = { speed * (float)time * 0.001f, 0.0f, 0.0f };
    float a[3] = { 0.0f, 90.0f, 0.0f };
    float v[3] = { speed, 0.0f, 0.0f };
    PlayerLerpRecord(&lerp, time, o, a, v, 0);
}

/* Server frame times at sv_fps 60: 16/17/17 ms. */
static int frame_time(int frame) { return 1000 + frame * 50 / 3; }

/* The pre-fix publication: origin(commandTime) + velocity * clamp(now -
 * commandTime, 0, 50 ms), what C1 evaluated from the TR_LINEAR_STOP window. */
static float linear_stop_x(int now, int command_time)
{
    int e = now - command_time;
    if(e < 0) e = 0;
    if(e > 50) e = 50;
    return SPEED * (float)command_time * 0.001f + SPEED * (float)e * 0.001f;
}

static void run_cadence(int latency, int *stalls_old, float *worst_step_error)
{
    int frame, next_cmd = 1000 - 300, newest_executed = 0;
    float prev_x = 0.0f, prev_old = 0.0f;
    *stalls_old = 0;
    *worst_step_error = 0.0f;
    PlayerLerpReset(&lerp);
    for(frame = 0; frame < 600; ++frame) {
        int now = frame_time(frame), prev_now = frame_time(frame - 1);
        float o[3], a[3];
        /* Packets of three commands leave the client every 34 ms (with a
         * 0..6 ms jitter) and are executed `latency` ms later. */
        while(1) {
            int packet_last = next_cmd + 22;
            int jitter = (packet_last / 34) % 7;
            if(packet_last + latency + jitter > now) break;
            record_run(next_cmd, SPEED);
            record_run(next_cmd + 11, SPEED);
            record_run(next_cmd + 22, SPEED);
            newest_executed = next_cmd + 22;
            next_cmd += 34;
        }
        assert(PlayerLerpPublish(&lerp, now, o, a));
        assert(fabsf(a[1] - 90.0f) < 0.001f);
        if(frame > 120) {
            float expected = SPEED * (float)(now - prev_now) * 0.001f;
            float err = fabsf((o[0] - prev_x) - expected);
            float old_x = linear_stop_x(now, newest_executed);
            if(err > *worst_step_error) *worst_step_error = err;
            /* Published positions never move backwards. */
            assert(o[0] >= prev_x);
            if(old_x - prev_old < 0.5f) ++*stalls_old;
            prev_old = old_x;
        } else {
            prev_old = linear_stop_x(now, newest_executed);
        }
        prev_x = o[0];
    }
}

int main(void)
{
    int stalls_old;
    float worst;
    float o[3], a[3], v[3] = {0, 0, 0};

    /* Internet latency: the old 50 ms window is exhausted, so the old
     * publication stood still on many frames; the timeline advances by one
     * frame's travel on every frame. */
    run_cadence(90, &stalls_old, &worst);
    printf("latency 90: old stalls %d / 479 frames, new worst step error %.3f u\n", stalls_old, worst);
    assert(stalls_old > 100);
    assert(worst < 0.6f); /* below the whole-unit origin codec */

    /* Loopback: the old window worked here, and the timeline is smooth too. */
    run_cadence(5, &stalls_old, &worst);
    printf("latency 5: old stalls %d / 479 frames, new worst step error %.3f u\n", stalls_old, worst);
    assert(worst < 0.6f); /* below the whole-unit origin codec */

    /* A teleport (toggled bit) restarts the history: the next publish is the
     * new spot, not a slide towards it. */
    PlayerLerpReset(&lerp);
    record_run(1000, SPEED); record_run(1011, SPEED); record_run(1022, SPEED);
    assert(PlayerLerpPublish(&lerp, 1100, o, a));
    {
        float spot[3] = { 5000.0f, 5000.0f, 0.0f }, ang[3] = { 0, 0, 0 };
        PlayerLerpRecord(&lerp, 1033, spot, ang, v, 1);
        assert(PlayerLerpPublish(&lerp, 1117, o, a));
        assert(o[0] == 5000.0f && o[1] == 5000.0f);
    }

    /* A jump no velocity explains is a teleport even without the bit. */
    {
        float far[3] = { -3000.0f, 0.0f, 0.0f }, ang[3] = { 0, 0, 0 };
        unsigned resets = lerp.resets;
        PlayerLerpRecord(&lerp, 1044, far, ang, v, 1);
        assert(lerp.resets == resets + 1 && lerp.count == 1);
    }

    /* The same command republished (ClientEndFrame) replaces, not appends. */
    {
        float p[3] = { 1.0f, 2.0f, 3.0f }, ang[3] = { 0, 0, 0 };
        unsigned count = lerp.count;
        PlayerLerpRecord(&lerp, 1044, p, ang, v, 1);
        assert(lerp.count == count);
        assert(PlayerLerpPublish(&lerp, 1134, o, a));
        assert(o[0] == 1.0f && o[2] == 3.0f);
    }

    /* Yaw interpolates the short way across 360. */
    PlayerLerpReset(&lerp);
    {
        float p[3] = { 0, 0, 0 }, a0[3] = { 0, 350.0f, 0 }, a1[3] = { 0, 10.0f, 0 };
        PlayerLerpRecord(&lerp, 1000, p, a0, v, 0);
        PlayerLerpRecord(&lerp, 1020, p, a1, v, 0);
        /* First publish starts at the newest command; walk the timeline back
         * by forcing a large lag on a fresh history. */
        lerp.has_pub = 1; lerp.pub_frame = 1050; lerp.pub_time = 1009.0f;
        lerp.delay = 41.0f; lerp.peak = 50.0f;
        assert(PlayerLerpPublish(&lerp, 1060, o, a));
        assert(fmodf(a[1] - 350.0f + 720.0f, 360.0f) <= 20.0f);
    }

    /* A late packet: the timeline extrapolates at most 50 ms, then holds. */
    PlayerLerpReset(&lerp);
    record_run(1000, SPEED);
    assert(PlayerLerpPublish(&lerp, 1000, o, a));
    assert(PlayerLerpPublish(&lerp, 1200, o, a)); /* inside the 250 ms gap */
    assert(o[0] <= SPEED * 1.05f + 0.001f);

    puts("PASS: human players publish on an even timeline");
    return 0;
}

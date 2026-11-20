#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Remote human player publication timeline.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * See player_lerp.h for why this exists. */
#include "player_lerp.h"
#include <math.h>
#include <string.h>

/* The peak forgets 2 ms of lag per second, so it holds the top of the
 * packet-gap sawtooth (period ~34 ms) and of the arrival jitter, yet follows
 * a falling ping within seconds. */
#define PEAK_DECAY_PER_MS 0.002f
/* Margin above the peak, so the newest command normally lies ahead of the
 * published time even when it arrives a little late. */
#define DELAY_MARGIN_MS 4.0f
/* A rising delay slows the published clock (to 85% speed at most) rather
 * than moving it backwards; a falling delay speeds it up by at most 3%. At a
 * 190 u/s run and 60 Hz that is under half a unit per frame, below the
 * whole-unit origin codec (delta.c position()). */
#define DELAY_RISE_PER_MS 0.15f
#define DELAY_FALL_PER_MS 0.03f
/* A frame gap this long (unlinked, dead, map change) starts the timeline over. */
#define TIMELINE_GAP_MS 250

static const player_lerp_sample_t *sample_at(const player_lerp_t *l, unsigned back)
{
    return &l->samples[(l->head + PLAYER_LERP_SAMPLES - back) % PLAYER_LERP_SAMPLES];
}

void PlayerLerpReset(player_lerp_t *lerp)
{
    if(!lerp) return;
    lerp->head = 0;
    lerp->count = 0;
    lerp->has_pub = 0;
    lerp->delay = lerp->peak = 0.0f;
}

static float length3(const float v[3])
{
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

void PlayerLerpRecord(player_lerp_t *lerp, int time,
                            const float origin[3], const float angles[3],
                            const float velocity[3], int teleport_bit)
{
    player_lerp_sample_t *s;
    int restart = 0;
    if(!lerp || !origin || !angles || !velocity) return;
    teleport_bit = teleport_bit != 0;
    if(lerp->count) {
        const player_lerp_sample_t *last = sample_at(lerp, 0);
        float d[3], dist, speed, allowed;
        int dt = time - last->time;
        d[0] = origin[0]-last->origin[0]; d[1] = origin[1]-last->origin[1]; d[2] = origin[2]-last->origin[2];
        dist = length3(d);
        speed = length3(last->velocity);
        if(length3(velocity) > speed) speed = length3(velocity);
        if(speed < 300.0f) speed = 300.0f;
        allowed = 32.0f + 2.0f * speed * (float)(dt > 0 ? dt : 0) * 0.001f;
        if(teleport_bit != lerp->teleport_bit || dt < 0 || dt > 1000 || dist > allowed)
            restart = 1;
        else if(dt == 0) {
            /* The same command published again; keep its newest pose. */
            s = &lerp->samples[lerp->head];
            memcpy(s->origin, origin, sizeof(s->origin));
            memcpy(s->angles, angles, sizeof(s->angles));
            memcpy(s->velocity, velocity, sizeof(s->velocity));
            return;
        }
    }
    if(restart) {
        PlayerLerpReset(lerp);
        ++lerp->resets;
    }
    lerp->teleport_bit = teleport_bit;
    if(lerp->count) lerp->head = (lerp->head + 1) % PLAYER_LERP_SAMPLES;
    if(lerp->count < PLAYER_LERP_SAMPLES) ++lerp->count;
    s = &lerp->samples[lerp->head];
    s->time = time;
    memcpy(s->origin, origin, sizeof(s->origin));
    memcpy(s->angles, angles, sizeof(s->angles));
    memcpy(s->velocity, velocity, sizeof(s->velocity));
}

static float angle_lerp(float from, float to, float f)
{
    float d = fmodf(to - from, 360.0f);
    if(d > 180.0f) d -= 360.0f;
    else if(d < -180.0f) d += 360.0f;
    return from + d * f;
}

static void pose_at(player_lerp_t *l, float t, float origin[3], float angles[3])
{
    const player_lerp_sample_t *newest = sample_at(l, 0), *a, *b;
    unsigned i, k;
    if(t >= (float)newest->time) {
        /* The newest command is late: continue along its velocity for at
         * most C1's own extrapolation window, then hold. */
        float e = t - (float)newest->time;
        if(e > (float)PLAYER_LERP_EXTRAPOLATE) e = (float)PLAYER_LERP_EXTRAPOLATE;
        if(e > 0.0f) ++l->extrapolated;
        for(k = 0; k < 3; ++k) {
            origin[k] = newest->origin[k] + newest->velocity[k] * e * 0.001f;
            angles[k] = newest->angles[k];
        }
        return;
    }
    b = newest;
    for(i = 1; i < l->count; ++i) {
        a = sample_at(l, i);
        if((float)a->time <= t) {
            float f = (t - (float)a->time) / (float)(b->time - a->time);
            for(k = 0; k < 3; ++k) {
                origin[k] = a->origin[k] + (b->origin[k] - a->origin[k]) * f;
                angles[k] = angle_lerp(a->angles[k], b->angles[k], f);
            }
            return;
        }
        b = a;
    }
    /* Older than the whole history: its oldest pose. */
    memcpy(origin, b->origin, sizeof(b->origin));
    memcpy(angles, b->angles, sizeof(b->angles));
}

int PlayerLerpPublish(player_lerp_t *lerp, int now,
                            float origin[3], float angles[3])
{
    float lag, target, t;
    if(!lerp || !origin || !angles || !lerp->count) return 0;
    if(lerp->has_pub && now == lerp->pub_frame) {
        memcpy(origin, lerp->pub_origin, sizeof(lerp->pub_origin));
        memcpy(angles, lerp->pub_angles, sizeof(lerp->pub_angles));
        return 1;
    }
    lag = (float)(now - sample_at(lerp, 0)->time);
    if(lag < 0.0f) lag = 0.0f;
    if(lag > lerp->lag_max) lerp->lag_max = lag;
    if(lag > PLAYER_LERP_MAX_DELAY) lag = PLAYER_LERP_MAX_DELAY;
    if(!lerp->has_pub || now < lerp->pub_frame || now - lerp->pub_frame > TIMELINE_GAP_MS) {
        lerp->peak = lag;
        lerp->delay = lag;
        t = (float)now - lerp->delay;
    } else {
        float dt = (float)(now - lerp->pub_frame);
        lerp->peak -= PEAK_DECAY_PER_MS * dt;
        if(lerp->peak < lag) lerp->peak = lag;
        target = lerp->peak + DELAY_MARGIN_MS;
        if(target > PLAYER_LERP_MAX_DELAY) target = PLAYER_LERP_MAX_DELAY;
        if(target > lerp->delay) {
            float step = DELAY_RISE_PER_MS * dt;
            lerp->delay += target - lerp->delay < step ? target - lerp->delay : step;
        } else {
            float step = DELAY_FALL_PER_MS * dt;
            lerp->delay -= lerp->delay - target < step ? lerp->delay - target : step;
        }
        t = (float)now - lerp->delay;
        /* Never run the published clock backwards. */
        if(t < lerp->pub_time) t = lerp->pub_time;
    }
    pose_at(lerp, t, origin, angles);
    lerp->pub_time = t;
    lerp->pub_frame = now;
    lerp->has_pub = 1;
    ++lerp->frames;
    memcpy(lerp->pub_origin, origin, sizeof(lerp->pub_origin));
    memcpy(lerp->pub_angles, angles, sizeof(lerp->pub_angles));
    return 1;
}

#ifdef OL_SERVER
#include "ol_bot.h"
#include "qcommon_io.h"
#include "qcommon.h"
#include "q_shared.h"
#include "cvar.h"
#include <stdlib.h>

#define LERP_PM_NORMAL 0
#define LERP_PM_LASTSTAND 7 /* C1 numbering, pmove_c1.c */
#define LERP_EF_TELEPORT_BIT 0x4

static player_lerp_t lerps[MAX_CLIENTS];
static unsigned char active[MAX_CLIENTS];
static int report_time[MAX_CLIENTS];

static int lerp_enabled(void)
{
    cvar_t *v = Cvar_FindVar("smoothHumans");
    return !v || !v->string || atoi(v->string) != 0;
}

static int lerp_traced(void)
{
    cvar_t *v = Cvar_FindVar("tracePlayerLerp");
    return v && v->string && atoi(v->string) != 0;
}

void PlayerLerpResetClient(int client)
{
    if(client < 0 || client >= MAX_CLIENTS) return;
    memset(&lerps[client], 0, sizeof(lerps[client]));
    active[client] = 0;
    report_time[client] = 0;
}

void PlayerLerpCapture(int client, int commandTime, int pmType, int eFlags,
                             const float origin[3], const float angles[3],
                             const float velocity[3])
{
    if(client < 0 || client >= MAX_CLIENTS) return;
    if(ClientIsBotClient((unsigned)client) ||
       (pmType != LERP_PM_NORMAL && pmType != LERP_PM_LASTSTAND)) {
        if(active[client]) PlayerLerpReset(&lerps[client]);
        active[client] = 0;
        return;
    }
    if(!active[client]) {
        PlayerLerpReset(&lerps[client]);
        lerps[client].teleport_bit = (eFlags & LERP_EF_TELEPORT_BIT) != 0;
        active[client] = 1;
    }
    PlayerLerpRecord(&lerps[client], commandTime, origin, angles, velocity,
                           eFlags & LERP_EF_TELEPORT_BIT);
}

int PlayerLerpPose(int client, int now, float origin[3], float angles[3])
{
    player_lerp_t *l;
    if(client < 0 || client >= MAX_CLIENTS || !active[client]) return 0;
    if(ClientIsBotClient((unsigned)client) || !lerp_enabled()) return 0;
    l = &lerps[client];
    if(!PlayerLerpPublish(l, now, origin, angles)) return 0;
    if(now - report_time[client] >= 1000 || now < report_time[client]) {
        if(report_time[client] && lerp_traced())
            ((void)0);
        l->frames = l->extrapolated = l->resets = 0;
        l->lag_max = 0.0f;
        report_time[client] = now;
    }
    return 1;
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailLerpEmpty(void){return resident_tail_zero(lerps,sizeof(lerps))&&resident_tail_zero(active,sizeof(active))&&resident_tail_zero(report_time,sizeof(report_time));}
void PersistentTailLerpReset(void){for(unsigned i=0;i<MAX_CLIENTS;++i)PlayerLerpResetClient(i);}
#endif

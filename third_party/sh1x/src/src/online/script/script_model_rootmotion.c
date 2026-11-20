#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Animation root motion on a script_model. See the header for the method,
 * the clock and the wire encoding.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "script_model_rootmotion.h"
#include "scr_vm_functions.h"
#include "xassets/xanims.h"
#include "cvar.h"
#include <math.h>
#include <string.h>
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

/* Retained CoD4 XAnim (src/asmsource/_xanim.asm) and q_math.c. */
extern void __cdecl XAnimGetRelDelta(const XAnim_s *anims, unsigned animIndex,
                                     float *rot, float *trans, float time1, float time2);
extern float XAnimGetTime(const struct XAnimTree_s *tree, unsigned int anim);
extern int __cdecl XAnimGetLengthMsec(const void *anims, unsigned animIndex);
extern double RotationToYaw(float *rot);

#define ROOTMOTION_SLOTS 32
/* K in the header: how far back each frame's TR_LINEAR is anchored. The
 * integer base's rounding (at most 0.87 units) becomes a velocity error of
 * at most 0.87/K units per ms: 0.02 units over a 50 ms snapshot. */
#define ROOTMOTION_ANCHOR 2000
/* Keeps Reached_ScriptMover (trTime + trDuration <= level.time) from turning
 * the published TR_LINEAR into a stop for as long as the frames refresh it. */
#define ROOTMOTION_DURATION (ROOTMOTION_ANCHOR + 60000)

typedef struct {
    int used;
    unsigned entity;
    int useCount;
    struct XAnimTree_s *tree;
    unsigned anim;
    float rate;
    int lengthMsec;
    float origin[3], angles[3], axis[3][3];
    float lastFraction;
    int holding;          /* the clip completed: hold its last position */
    float held[3], heldYaw;
    int began;            /* level.time of the begin */
    int published;        /* pos/apos below are what this module last wrote */
    trajectory_t pos, apos;
} rootmotion_t;

static rootmotion_t slots[ROOTMOTION_SLOTS];
static cvar_t *s_trace;

static int trace_level(void)
{
    if (!s_trace)
        s_trace = Cvar_RegisterInt("rootMotionTrace", 0, 0, 2, 0,
            "Script-model root motion: 1 = begin/end, 2 = every server frame");
    return s_trace ? s_trace->integer : 0;
}

static rootmotion_t *slot_for(unsigned entity)
{
    unsigned i;
    for (i = 0; i < ROOTMOTION_SLOTS; ++i)
        if (slots[i].used && slots[i].entity == entity) return &slots[i];
    return NULL;
}

/* Where the clip's accumulated motion puts the model at fraction f. */
static void clip_position(const rootmotion_t *s, float f, float *out, float *yaw)
{
    float rot[2] = {0.0f, 1.0f}, trans[3] = {0.0f, 0.0f, 0.0f};
    int i;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    if (f > 0.0f) XAnimGetRelDelta(s->tree->anims, s->anim, rot, trans, 0.0f, f);
    /* x forward, y left, z up; AngleVectors gives right, so left = -right. */
    for (i = 0; i < 3; ++i)
        out[i] = s->origin[i] + trans[0] * s->axis[0][i] - trans[1] * s->axis[1][i]
                 + trans[2] * s->axis[2][i];
    *yaw = s->angles[1] + (float)RotationToYaw(rot);
}

/* The velocity each frame publishes is the clip's mean velocity over the next
 * ROOTMOTION_LOOKAHEAD ms (at least one server frame). C1 extrapolates it
 * until its next snapshot replaces it, so the position step at that switch is
 * how far the clip's path bends away from the line in the meantime. Measured
 * on the three walk-ups (60 Hz server): one frame (17 ms) leaves 0.007 units
 * at 60 Hz snapshots but 0.31 at 20 Hz; 33 ms leaves at most 0.08 at 60 Hz
 * and 0.09 at 20 Hz; 50 ms 0.11 and 0.003. C1's snapshot rate is the
 * client's (snaps, rate; every frame on loopback/LAN), so the middle one. */
#define ROOTMOTION_LOOKAHEAD 33

static int lookahead_msec(void)
{
    int h = level.time - level.previousTime;
    if (h <= 0 || h > 200) h = 50;
    return h > ROOTMOTION_LOOKAHEAD ? h : ROOTMOTION_LOOKAHEAD;
}

/* Publish p (moving at v units per ms) and yaw (turning at w degrees per ms)
 * for this frame. */
static void publish(rootmotion_t *s, gentity_t *ent, const float *p, const float *v,
                    float yaw, float w)
{
    int i;
    const int anchor = level.time - ROOTMOTION_ANCHOR;
    memset(&s->pos, 0, sizeof(s->pos));
    memset(&s->apos, 0, sizeof(s->apos));
    s->pos.trType = TR_LINEAR;
    s->pos.trTime = anchor;
    s->pos.trDuration = ROOTMOTION_DURATION;
    for (i = 0; i < 3; ++i) {
        float base = floorf(p[i] - v[i] * (float)ROOTMOTION_ANCHOR + 0.5f);
        s->pos.trBase[i] = base;
        s->pos.trDelta[i] = (p[i] - base) * (1000.0f / (float)ROOTMOTION_ANCHOR);
    }
    /* The angle codec keeps 1/182 degree, so the yaw needs no anchor. */
    s->apos.trType = TR_LINEAR;
    s->apos.trTime = level.time;
    s->apos.trDuration = ROOTMOTION_DURATION;
    s->apos.trBase[0] = s->angles[0];
    s->apos.trBase[1] = AngleNormalize360(yaw);
    s->apos.trBase[2] = s->angles[2];
    s->apos.trDelta[1] = w * 1000.0f;

    ent->s.lerp.pos = s->pos;
    ent->s.lerp.apos = s->apos;
    VectorCopy(p, ent->r.currentOrigin);
    ent->r.currentAngles[0] = s->angles[0];
    ent->r.currentAngles[1] = s->apos.trBase[1];
    ent->r.currentAngles[2] = s->angles[2];
    s->published = 1;
    SV_LinkEntity(ent);
}

static int trajectory_same(const trajectory_t *a, const trajectory_t *b)
{
    return a->trType == b->trType && a->trTime == b->trTime &&
           a->trDuration == b->trDuration &&
           !memcmp(a->trBase, b->trBase, sizeof(a->trBase)) &&
           !memcmp(a->trDelta, b->trDelta, sizeof(a->trDelta));
}

static void finish(rootmotion_t *s, const char *why)
{
    if (trace_level() >= 1)
        Com_Printf(CON_CHANNEL_SERVER,
            "CoDOL rootmotion: ent %u %s after %d ms\n", s->entity, why, level.time - s->began);
    memset(s, 0, sizeof(*s));
}

/* One frame of one slot. */
static void advance(rootmotion_t *s, gentity_t *ent)
{
    static const float still[3] = {0.0f, 0.0f, 0.0f};
    float p[3], q[3], v[3], yaw, yaw2, f, f2;
    int h, i;

    if (!s->holding) {
        float raw = XAnimGetTime(s->tree, s->anim);
        /* A non-looping clip stops at 1. A time that goes back means the clip
         * left the tree (replaced, cleared or blended out and freed): the
         * walk ended where it last was. */
        f = raw >= 1.0f ? 1.0f : raw;
        if (raw >= 1.0f || raw < s->lastFraction) {
            if (raw < s->lastFraction) f = s->lastFraction;
            clip_position(s, f, s->held, &s->heldYaw);
            s->holding = 1;
            if (trace_level() >= 1)
                Com_Printf(CON_CHANNEL_SERVER,
                    "CoDOL rootmotion: ent %u clip done at %.3f after %d ms, holds (%.2f %.2f %.2f) yaw %.2f\n",
                    s->entity, f, level.time - s->began,
                    s->held[0], s->held[1], s->held[2], s->heldYaw);
        } else {
            s->lastFraction = f;
            h = lookahead_msec();
            f2 = s->lengthMsec > 0 ? f + (float)h * s->rate / (float)s->lengthMsec : 1.0f;
            if (f2 > 1.0f) f2 = 1.0f;
            clip_position(s, f, p, &yaw);
            clip_position(s, f2, q, &yaw2);
            for (i = 0; i < 3; ++i) v[i] = (q[i] - p[i]) / (float)h;
            publish(s, ent, p, v, yaw, AngleDelta(yaw2, yaw) / (float)h);
            if (trace_level() >= 2)
                Com_Printf(CON_CHANNEL_SERVER,
                    "CoDOL rootmotion: ent %u t %d f %.4f origin (%.2f %.2f %.2f) yaw %.2f v (%.1f %.1f %.1f)/s base (%.0f %.0f %.0f) trTime %d\n",
                    s->entity, level.time - s->began, f, p[0], p[1], p[2], yaw,
                    v[0] * 1000.0f, v[1] * 1000.0f, v[2] * 1000.0f,
                    s->pos.trBase[0], s->pos.trBase[1], s->pos.trBase[2], s->pos.trTime);
            return;
        }
    }
    /* Holding: re-anchored every frame so the exact (sub-unit) position
     * survives the integer origin codec. */
    publish(s, ent, s->held, still, s->heldYaw, 0.0f);
}

void RunScriptModelRootMotion(void)
{
    unsigned i;
    for (i = 0; i < ROOTMOTION_SLOTS; ++i) {
        rootmotion_t *s = &slots[i];
        gentity_t *ent;
        if (!s->used) continue;
        ent = &g_entities[s->entity];
        if (!ent->r.inuse || ent->useCount != s->useCount) { finish(s, "freed"); continue; }
        /* The script took the model over (moveTo, rotateTo, .origin = ...). */
        if (s->published && (!trajectory_same(&ent->s.lerp.pos, &s->pos) ||
                             !trajectory_same(&ent->s.lerp.apos, &s->apos))) {
            finish(s, "released: the script moved the model");
            continue;
        }
        advance(s, ent);
    }
}

int ScriptModelRootMotionBegin(gentity_t *ent, struct XAnimTree_s *tree, unsigned anim,
                                     const float *origin, const float *angles, float rate)
{
    rootmotion_t *s = slot_for((unsigned)ent->s.number);
    unsigned i;
    if (!s)
        for (i = 0; i < ROOTMOTION_SLOTS && !s; ++i)
            if (!slots[i].used) s = &slots[i];
    if (!s) return 0;
    memset(s, 0, sizeof(*s));
    s->used = 1;
    s->entity = (unsigned)ent->s.number;
    s->useCount = ent->useCount;
    s->tree = tree;
    s->anim = anim;
    s->rate = rate;
    s->lengthMsec = XAnimGetLengthMsec(tree->anims, anim);
    VectorCopy(origin, s->origin);
    VectorCopy(angles, s->angles);
    AngleVectors(angles, s->axis[0], s->axis[1], s->axis[2]);
    s->began = level.time;
    if (trace_level() >= 1)
        Com_Printf(CON_CHANNEL_SERVER,
            "CoDOL rootmotion: ent %d begins anim %u (%d ms, rate %.2f) at (%.2f %.2f %.2f) yaw %.2f\n",
            ent->s.number, anim, s->lengthMsec, rate, origin[0], origin[1], origin[2], angles[1]);
    /* This frame's snapshot already carries the clip's start. */
    advance(s, ent);
    return 1;
}

void ScriptModelRootMotionStop(unsigned entity)
{
    rootmotion_t *s = slot_for(entity);
    if (s) finish(s, "stopped");
}

void ResetScriptModelRootMotion(unsigned entity)
{
    rootmotion_t *s = slot_for(entity);
    if (s) memset(s, 0, sizeof(*s));
}

void ResetScriptModelRootMotions(void)
{
    memset(slots, 0, sizeof(slots));
}
#endif

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentRootMotionEmpty(void){return resident_entity_zero(slots,sizeof(slots));}
#endif

#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* CoDOL per-entity model scale.
 *
 * C1's entityState carries a model scale at +0x54, which the client reads at
 * 0x441532 when it draws the entity (alpha0022/entity_codec.py). The retained
 * IW3 entityState has no such member, so the snapshot overlay
 * (src/online/snapshot/engine_state.c) used to publish 1.0 for every
 * entity. This table is the server's side of that field.
 *
 *   <entity> setModelScale(<scale>)
 *       draws the entity at <scale> times its size. 1.0 is normal size. The
 *       value is cleared when the entity is freed.
 *
 * The server follows the drawn size: hit traces (sv_world.cpp
 * SV_PointTraceToEntity) and tag positions (g_utils_mp.cpp G_DObjGetWorld*)
 * scale about the origin, and an actor's movement hull (r.mins/r.maxs, which
 * its capsule traces and players' collision use) is its unscaled hull times
 * the scale.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "script_model_scale.h"
#include "scr_vm_functions.h"
#include "server.h"
#include <string.h>

/* 0 means "never set", so a zero-initialised table is all normal size. */
static float s_modelScale[MAX_GENTITIES];
/* An actor's hull before its first scale, so scales never compound. */
static struct { int valid; float mins[3], maxs[3]; } s_baseHull[MAX_GENTITIES];

static void apply_hull(int num, float scale)
{
    gentity_t *ent = &g_entities[num];
    int i;
    if (!ent->r.inuse || ent->s.eType != ET_ACTOR) return;
    if (!s_baseHull[num].valid) {
        if (scale == 1.0f) return;
        for (i = 0; i < 3; ++i) {
            s_baseHull[num].mins[i] = ent->r.mins[i];
            s_baseHull[num].maxs[i] = ent->r.maxs[i];
        }
        s_baseHull[num].valid = 1;
    }
    for (i = 0; i < 3; ++i) {
        ent->r.mins[i] = s_baseHull[num].mins[i] * scale;
        ent->r.maxs[i] = s_baseHull[num].maxs[i] * scale;
    }
    SV_LinkEntity(ent);
}

float EntityModelScale(int num)
{
    if (num < 0 || num >= MAX_GENTITIES || s_modelScale[num] <= 0.0f) return 1.0f;
    return s_modelScale[num];
}

void EntityModelScaleFreed(int num)
{
    if (num >= 0 && num < MAX_GENTITIES) { s_modelScale[num] = 0.0f; s_baseHull[num].valid = 0; }
}

void SetEntityModelScale(int num, float scale)
{
    if (num < 0 || num >= MAX_GENTITIES || !(scale >= 0.01f && scale <= 100.0f)) return;
    s_modelScale[num] = scale == 1.0f ? 0.0f : scale;
    apply_hull(num, scale);
}

void Scr_SetModelScale(scr_entref_t ref)
{
    float scale;
    if (ref.classnum || ref.entnum >= MAX_GENTITIES) {
        Scr_ObjectError("not an entity");
        return;
    }
    scale = Scr_GetFloat(0);
    if (!(scale >= 0.01f && scale <= 100.0f)) {
        Scr_ParamError(0, va("setModelScale: scale %g outside 0.01..100", scale));
        return;
    }
    s_modelScale[ref.entnum] = scale == 1.0f ? 0.0f : scale;
    apply_hull(ref.entnum, scale);
}

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailModelScaleEmpty(void){for(unsigned i=0;i<MAX_GENTITIES;++i)if(s_modelScale[i]||s_baseHull[i].valid)return 0;return 1;}
void PersistentTailModelScaleReset(void){for(unsigned i=0;i<MAX_GENTITIES;++i)EntityModelScaleFreed(i);}
#endif

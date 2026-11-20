/* <script_model> codol_setClipBox( halfWidth, height ): see the header.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "clip_box.h"
#include "scr_vm_functions.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

static struct { unsigned char active; short halfWidth, height; } s_box[MAX_GENTITIES];

int ClipBoxSolid(int num, const float *mins, const float *maxs, int contents, int bmodel, int *solid)
{
    float w, h;
    if(num < 0 || num >= MAX_GENTITIES || !s_box[num].active) return 0;
    w = (float)s_box[num].halfWidth; h = (float)s_box[num].height;
    if(bmodel || !(contents & CLIP_BOX_CONTENTS) ||
       mins[0] != -w || mins[1] != -w || mins[2] != 0.0f ||
       maxs[0] != w || maxs[1] != w || maxs[2] != h) return 0;
    *solid = (int)ClipBoxPackedSolid(s_box[num].halfWidth, s_box[num].height);
    return *solid != 0;
}

void ClipBoxFreed(int num)
{
    if(num >= 0 && num < MAX_GENTITIES) s_box[num].active = 0;
}

void Scr_SetClipBox(scr_entref_t ref)
{
    gentity_t *self;
    int w, h, i;

    if(ref.classnum || ref.entnum >= MAX_GENTITIES) {
        Scr_ObjectError("codol_setClipBox: not an entity");
        return;
    }
    self = &g_entities[ref.entnum];
    if(!self->r.inuse || self->client || self->r.bmodel ||
       (self->s.eType != ET_SCRIPTMOVER && self->s.eType != ET_GENERAL)) {
        Scr_ObjectError(va("codol_setClipBox: entity %i is not a model script_model or script_origin", ref.entnum));
        return;
    }
    if(Scr_GetNumParam() == 0) {
        /* No argument: no box. */
        s_box[ref.entnum].active = 0;
        VectorClear(self->r.mins); VectorClear(self->r.maxs);
        self->r.contents = 0;
        SV_LinkEntity(self);
        return;
    }
    if(Scr_GetNumParam() != 2) {
        Scr_Error("usage: <entity> codol_setClipBox( <halfWidth>, <height> )");
        return;
    }
    w = Scr_GetInt(0); h = Scr_GetInt(1);
    if(w < 1 || w > CLIP_BOX_MAX_HALF_WIDTH) {
        Scr_ParamError(0, va("codol_setClipBox: halfWidth %i is outside 1..%i", w, CLIP_BOX_MAX_HALF_WIDTH));
        return;
    }
    if(h < 1 || h > CLIP_BOX_MAX_HEIGHT) {
        Scr_ParamError(1, va("codol_setClipBox: height %i is outside 1..%i", h, CLIP_BOX_MAX_HEIGHT));
        return;
    }
    s_box[ref.entnum].active = 1;
    s_box[ref.entnum].halfWidth = (short)w;
    s_box[ref.entnum].height = (short)h;
    for(i = 0; i < 2; ++i) { self->r.mins[i] = (float)-w; self->r.maxs[i] = (float)w; }
    self->r.mins[2] = 0.0f; self->r.maxs[2] = (float)h;
    self->r.contents = CLIP_BOX_CONTENTS;
    SV_LinkEntity(self);
}
#endif

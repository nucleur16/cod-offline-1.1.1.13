/* CoDOL brush-entity probe (developer diagnostic, developer 1 only).
 *
 * brushprobe
 *     For every linked bmodel entity (s.solid == SOLID_BMODEL): its number,
 *     classname, targetname, eType, clip handle, whether that handle is one
 *     of the server-only trigger models AppendNativeTriggers adds, and
 *     whether it is hidden (SVF_NOCLIENT).  Then the wire state the snapshot
 *     path would publish for it (ProbeEntityWire): +98 and +a8.
 *
 *     C1 indexes cm.cmodels with +98 unchecked whenever +a8 is 0xffffff
 *     (CG_ClipMoveToEntity 0x4856d0 -> 0x587140 -> 0x586cf2), so the probe
 *     ends with the count of entities that would publish a brush index the
 *     client does not have.  It must be 0.  Hidden entities are probed too:
 *     any script sound on a trigger unhides it (playSound, stopSounds,
 *     play/stopLoopSound clear SVF_NOCLIENT, as in C1).
 *
 * boxprobe
 *     For every linked non-player, non-actor entity with a packed box solid
 *     (codol_setClipBox and any other): the server bounds and contents, the
 *     wire +a8 the snapshot path publishes, and the box C1's
 *     CG_GetEntityBounds 0x485360 decodes from it, absolute.  For a clip box
 *     the two boxes must be equal.
 * boxprobe sweep
 *     Also sweeps the standing player capsule (-15 -15 0)-(15 15 70) at each
 *     clip box from 40 units past each of its four faces to its centre, 20
 *     units above its base, with the player clip mask 0x2814011 and the
 *     actor clip mask 0x2820015, once as it is and once with the box's
 *     contents cleared, and prints where each stops.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "g_public.h"
#include "qcommon.h"
#include "cmd.h"
#include "cscr_stringlist.h"
#include <string.h>
#include "entities.h"
#include "frame_builder.h"
#include "collision_native.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

extern char entity_seed_failure[160];
extern char entity_seed_reason[160];
extern int RefreshServerAssetNames(void);

static unsigned brush_probe_field(const entity_state_t *wire,unsigned offset,int *found)
{
    unsigned f;
    for(f=0;f<ENTITY_FIELDS;++f) {
        const entity_field_t *field=EntityField(wire->type,f);
        if(field && field->offset==offset) { *found=1; return wire->field[f]; }
    }
    *found=0; return 0;
}

static const char *brush_probe_string(unsigned handle)
{
    return handle ? SL_ConvertToString(handle) : "";
}

static void BrushProbe_f(void)
{
    unsigned client_models=ClientInlineModelCount();
    int n,bmodels=0,triggers=0,bad=0,beyond=0;
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"brushprobe needs developer 1\n"); return;
    }
    RefreshServerAssetNames();
    Com_Printf(CON_CHANNEL_DONT_FILTER,"client inline models %u, server clip models %d\n",
        client_models,CM_NumInlineModels());
    for(n=0;n<ENTITYNUM_WORLD;++n) {
        gentity_t *ent=&g_entities[n];
        entity_state_t wire;
        int result,trigger,f98=0,fa8=0,f08=0;
        unsigned v98=0,va8=0,v08=0;
        if(!ent->r.inuse || ent->s.solid!=SOLID_BMODEL) continue;
        ++bmodels;
        trigger=IsTriggerClipHandle(ent->s.index);
        if(trigger) ++triggers;
        /* What the pre-fix path bounded by cm.numSubModels would have sent. */
        if(ent->s.index>0 && (unsigned)ent->s.index>=client_models &&
           ent->s.index<CM_NumInlineModels()) ++beyond;
        entity_seed_failure[0]=entity_seed_reason[0]='\0';
        result=ProbeEntityWire(&ent->s,0,&wire);
        if(result==1) {
            v98=brush_probe_field(&wire,0x098,&f98);
            va8=brush_probe_field(&wire,0x0a8,&fa8);
            v08=brush_probe_field(&wire,0x008,&f08);
            if(fa8 && va8==0xffffffu && (!f98 || v98==0 || v98>=client_models)) ++bad;
        }
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "  ent %d %s '%s' eType %d clip %d%s%s -> ",
            n,brush_probe_string(ent->classname),brush_probe_string(ent->targetname),
            ent->s.eType,ent->s.index,trigger ? " (server-only trigger)" : "",
            (ent->r.svFlags & SVF_NOCLIENT) ? " hidden" : " PUBLISHED");
        if(result==1)
            /* +08 bit 1 is EF_NONSOLID_BMODEL: C1 predicts against the brush
             * unless it is set (CG_ClipMoveToEntity 0x48571e). */
            Com_Printf(CON_CHANNEL_DONT_FILTER,
                "wire +98=%u +a8=0x%x +08=0x%x%s | native eFlags 0x%x contents 0x%x"
                " abs (%.0f %.0f %.0f)-(%.0f %.0f %.0f)\n",
                v98,va8,v08,f08 ? "" : "(absent)",(unsigned)ent->s.lerp.eFlags,
                (unsigned)ent->r.contents,
                ent->r.absmin[0],ent->r.absmin[1],ent->r.absmin[2],
                ent->r.absmax[0],ent->r.absmax[1],ent->r.absmax[2]);
        else
            Com_Printf(CON_CHANNEL_DONT_FILTER,"refused (%s%s%s)\n",
                result ? "common overlay" : entity_seed_failure,
                entity_seed_reason[0] ? ": " : "",entity_seed_reason);
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "%d bmodel entities, %d on server-only trigger models (%d with a clip handle past the"
        " client's cmodels); %d would publish a brush index the client lacks\n",
        bmodels,triggers,beyond,bad);
}

static void box_probe_sweep(gentity_t *ent,int half)
{
    static const float mins[3]={-15.0f,-15.0f,0.0f},maxs[3]={15.0f,15.0f,70.0f};
    static const int masks[2]={0x2814011,0x2820015};
    static const char *mask_names[2]={"player","actor"};
    static const float dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    int d,m,pass;
    for(pass=0;pass<2;++pass) {
        int contents=ent->r.contents;
        if(pass) { ent->r.contents=0; SV_LinkEntity(ent); }
        for(m=0;m<2;++m) {
            Com_Printf(CON_CHANNEL_DONT_FILTER,"    %s sweep%s:",mask_names[m],pass ? " (box contents cleared)" : "");
            for(d=0;d<4;++d) {
                trace_t tr; float a[3],b[3],stop;
                int i,hit;
                for(i=0;i<2;++i) {
                    b[i]=ent->r.currentOrigin[i];
                    a[i]=b[i]+dirs[d][i]*(float)(half+15+40);
                }
                a[2]=b[2]=ent->r.currentOrigin[2]+20.0f;
                G_TraceCapsule(&tr,a,mins,maxs,b,ENTITYNUM_NONE,masks[m]);
                hit=Trace_GetEntityHitId(&tr);
                stop=(half+15+40)*(1.0f-tr.fraction);
                Com_Printf(CON_CHANNEL_DONT_FILTER," %s%c face: frac %.3f stop %.1f from centre%s%s hit %d;",
                    dirs[d][0]+dirs[d][1]>0 ? "+" : "-",dirs[d][0]!=0 ? 'x' : 'y',tr.fraction,stop,
                    tr.startsolid ? " startsolid" : "",tr.allsolid ? " allsolid" : "",hit);
            }
            Com_Printf(CON_CHANNEL_DONT_FILTER,"\n");
        }
        if(pass) { ent->r.contents=contents; SV_LinkEntity(ent); }
    }
}

static void BoxProbe_f(void)
{
    int n,boxes=0,equal=0,sweep=Cmd_Argc()>1 && !strcmp(Cmd_Argv(1),"sweep");
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"boxprobe needs developer 1\n"); return;
    }
    RefreshServerAssetNames();
    for(n=0;n<ENTITYNUM_WORLD;++n) {
        gentity_t *ent=&g_entities[n];
        entity_state_t wire;
        int fa8=0,result,half,minz,maxz,same;
        unsigned va8=0;
        if(!ent->r.inuse || !ent->r.linked || !ent->s.solid || ent->s.solid==SOLID_BMODEL ||
           ent->s.eType==ET_PLAYER || ent->s.eType==ET_ACTOR) continue;
        ++boxes;
        entity_seed_failure[0]=entity_seed_reason[0]='\0';
        result=ProbeEntityWire(&ent->s,0,&wire);
        if(result==1) va8=brush_probe_field(&wire,0x0a8,&fa8);
        /* 0x4853ba..0x48542f */
        half=(int)(va8&0xffu)-1; minz=2-(int)((va8>>8)&0xffu); maxz=(int)((va8>>16)&0xffu)-33;
        same=result==1 && fa8 &&
            ent->r.mins[0]==(float)-half && ent->r.mins[1]==(float)-half && ent->r.mins[2]==(float)minz &&
            ent->r.maxs[0]==(float)half && ent->r.maxs[1]==(float)half && ent->r.maxs[2]==(float)maxz;
        if(same) ++equal;
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "  ent %d %s '%s' eType %d%s origin (%.1f %.1f %.1f) server box (%.0f %.0f %.0f)-(%.0f %.0f %.0f)"
            " contents 0x%x solid 0x%06x -> ",
            n,brush_probe_string(ent->classname),brush_probe_string(ent->targetname),ent->s.eType,
            (ent->r.svFlags & SVF_NOCLIENT) ? " hidden" : " PUBLISHED",
            ent->r.currentOrigin[0],ent->r.currentOrigin[1],ent->r.currentOrigin[2],
            ent->r.mins[0],ent->r.mins[1],ent->r.mins[2],ent->r.maxs[0],ent->r.maxs[1],ent->r.maxs[2],
            (unsigned)ent->r.contents,(unsigned)ent->s.solid);
        if(result==1 && fa8)
            Com_Printf(CON_CHANNEL_DONT_FILTER,
                "wire +a8=0x%06x, C1 decodes (%d %d %d)-(%d %d %d), absolute (%.1f %.1f %.1f)-(%.1f %.1f %.1f)%s\n",
                va8,-half,-half,minz,half,half,maxz,
                ent->r.currentOrigin[0]-half,ent->r.currentOrigin[1]-half,ent->r.currentOrigin[2]+minz,
                ent->r.currentOrigin[0]+half,ent->r.currentOrigin[1]+half,ent->r.currentOrigin[2]+maxz,
                same ? " = server" : " DIFFERS");
        else
            Com_Printf(CON_CHANNEL_DONT_FILTER,"refused (%s%s%s)\n",
                result ? "no +a8" : entity_seed_failure,
                entity_seed_reason[0] ? ": " : "",entity_seed_reason);
        if(sweep && same) box_probe_sweep(ent,half);
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,"%d box entities, %d published exactly as the server's box\n",boxes,equal);
}

void RegisterBrushProbe(void)
{
    Cmd_AddCommand("brushprobe",BrushProbe_f);
    Cmd_AddCommand("boxprobe",BoxProbe_f);
}

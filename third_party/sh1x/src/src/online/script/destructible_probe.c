/* CoDOL destructible probe (developer diagnostic, developer 1 only).
 *
 * destructible list
 *     every script entity whose targetname is destructible_vehicle /
 *     destructible_toy or whose model names a destructible part set.
 * destructible <entnum>
 *     model slot and name, attachments, hide words (entity, DObj and what the
 *     snapshot overlay puts on the wire), the wire model selector.
 * destructible <entnum> bones
 *     every bone of the server DObj, hidden ones starred.
 * destructible <entnum> damage <amount> [bullet|splash] [part]
 *     G_Damage on the entity from the world, so the "damage" notify the
 *     destructible script waits on runs exactly as from a weapon; [part]
 *     names the bone hit, as a bullet trace reports it.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "dobj.h"
#include "cscr_stringlist.h"
#include "xassets/xmodel.h"
#include "qcommon.h"
#include "cmd.h"
#include <stdlib.h>
#include <string.h>
#include "entities.h"
#include "ol_events.h"
#include "engine_state.h"
#include "entitystate_ext.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

extern void G_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
    const float *dir, const float *point, int damage, int dflags, int mod, int weapon,
    int hitLoc, unsigned int modelIndex, unsigned int partName, int timeOffset);
extern unsigned OL_G_ModelName(int index);
extern int ServerModelToWire(unsigned index);
extern int RefreshServerAssetNames(void);

#define PROBE_MOD_RIFLE_BULLET 2
#define PROBE_MOD_GRENADE_SPLASH 4

static const char *probe_string(unsigned handle)
{
    return handle ? SL_ConvertToString(handle) : "";
}

static int probe_is_destructible(const gentity_t *ent)
{
    const char *target=probe_string(ent->targetname);
    const char *model=probe_string(OL_G_ModelName(ent->model));
    return !strcmp(target,"destructible_vehicle") || !strcmp(target,"destructible_toy") ||
           strstr(model,"destructible") || strstr(model,"destroyed") || strstr(model,"_dst");
}

static unsigned probe_hidden_count(const gentity_t *ent)
{
    uint32_t words[8];
    unsigned i,n=0;
    EntityStateHidePartBits(&ent->s,words);
    for(i=0;i<8;++i) { uint32_t w=words[i]; while(w){ n+=w&1u; w>>=1; } }
    return n;
}

static void probe_wire(const gentity_t *ent)
{
    entity_state_t wire;
    int type=EntityTypeToWire((unsigned)ent->s.eType);
    unsigned f,i;
    /* The gamestate/snapshot path registers the wire slots; with no client
     * primed nothing has yet, so do it here as the gamestate would. */
    RefreshServerAssetNames();
    memset(&wire,0,sizeof(wire));
    wire.number=(uint16_t)ent->s.number; wire.type=(uint16_t)(type>0?type:0);
    if(type<0) { Com_Printf(CON_CHANNEL_DONT_FILTER,"  wire: eType %d has no wire type\n",ent->s.eType); return; }
    if(!OverlayServerModelEntity(&wire,ent))
        Com_Printf(CON_CHANNEL_DONT_FILTER,"  wire: model overlay refused (eType %d, slot %d -> %d)\n",
            ent->s.eType,ent->s.index,ServerModelToWire((unsigned)ent->s.index));
    if(!OverlayEnginePartBits(&wire,&ent->s))
        Com_Printf(CON_CHANNEL_DONT_FILTER,"  wire: part-bit overlay refused\n");
    Com_Printf(CON_CHANNEL_DONT_FILTER,"  wire type %d:",type);
    for(f=0;f<ENTITY_FIELDS;++f) {
        unsigned off=EntityField(wire.type,f)->offset;
        if(off==0x098 || off==0x05c || off==0x060 || off==0x094) Com_Printf(CON_CHANNEL_DONT_FILTER," +%03x=%u",off,wire.field[f]);
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,"\n  wire hide:");
    for(i=0;i<8;++i) for(f=0;f<ENTITY_FIELDS;++f)
        if(EntityField(wire.type,f)->offset==0x0ec+4*i) {
            Com_Printf(CON_CHANNEL_DONT_FILTER," %08x",wire.field[f]); break;
        }
    Com_Printf(CON_CHANNEL_DONT_FILTER,"\n");
}

static void probe_detail(gentity_t *ent)
{
    DObj *obj=Com_GetServerDObj(ent->s.number);
    uint32_t words[8];
    unsigned i,b;
    Com_Printf(CON_CHANNEL_DONT_FILTER,"destructible %d: classname %s targetname %s eType %d solid %d takedamage %d health %d svFlags 0x%x eFlags 0x%x\n",
        ent->s.number,probe_string(ent->classname),probe_string(ent->targetname),ent->s.eType,
        ent->s.solid,ent->takedamage,ent->health,ent->r.svFlags,ent->s.lerp.eFlags);
    Com_Printf(CON_CHANNEL_DONT_FILTER,"  model field %d '%s' s.index %d '%s' wire slot %d\n",
        ent->model,probe_string(OL_G_ModelName(ent->model)),ent->s.index,
        probe_string(OL_G_ModelName(ent->s.index)),ServerModelToWire((unsigned)ent->s.index));
    for(i=0;i<19;++i) if(ent->attachModelNames[i])
        Com_Printf(CON_CHANNEL_DONT_FILTER,"  attach %u '%s' on '%s'\n",i,
            probe_string(OL_G_ModelName(ent->attachModelNames[i])),probe_string(ent->attachTagNames[i]));
    EntityStateHidePartBits(&ent->s,words);
    Com_Printf(CON_CHANNEL_DONT_FILTER,"  entity hide:");
    for(i=0;i<8;++i)Com_Printf(CON_CHANNEL_DONT_FILTER," %08x",words[i]);
    Com_Printf(CON_CHANNEL_DONT_FILTER,"\n  hidden bones:");
    for(b=0;b<256;++b) if(words[b>>5]&(0x80000000u>>(b&31))) {
        Com_Printf(CON_CHANNEL_DONT_FILTER," %u",b);
        if(obj && b<obj->numBones) {
            unsigned m,start=0;
            for(m=0;m<obj->numModels;++m) {
                unsigned count=(unsigned char)obj->models[m]->numBones;
                if(b<start+count){ Com_Printf(CON_CHANNEL_DONT_FILTER,"(%s)",
                    SL_ConvertToString(obj->models[m]->boneNames[b-start])); break; }
                start+=count;
            }
        }
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,"\n");
    if(obj) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"  dobj models %d bones %d:",obj->numModels,obj->numBones);
        for(i=0;i<(unsigned)obj->numModels;++i)Com_Printf(CON_CHANNEL_DONT_FILTER," %s",obj->models[i]->name);
        Com_Printf(CON_CHANNEL_DONT_FILTER,"\n  dobj hide:");
        for(i=0;i<8;++i)Com_Printf(CON_CHANNEL_DONT_FILTER," %08x",(uint32_t)obj->hidePartBits[i]);
        Com_Printf(CON_CHANNEL_DONT_FILTER,"\n");
    } else Com_Printf(CON_CHANNEL_DONT_FILTER,"  no server DObj\n");
    probe_wire(ent);
}

/* Every bone of the server DObj in skeleton order, '*' on hidden ones. */
static void probe_bones(gentity_t *ent)
{
    DObj *obj=Com_GetServerDObj(ent->s.number);
    uint32_t words[8];
    unsigned m,b,start=0;
    if(!obj) { Com_Printf(CON_CHANNEL_DONT_FILTER,"  no server DObj\n"); return; }
    EntityStateHidePartBits(&ent->s,words);
    for(m=0;m<(unsigned)obj->numModels;++m) {
        unsigned count=(unsigned char)obj->models[m]->numBones;
        Com_Printf(CON_CHANNEL_DONT_FILTER,"  %s:",obj->models[m]->name);
        for(b=0;b<count;++b) {
            unsigned bone=start+b;
            Com_Printf(CON_CHANNEL_DONT_FILTER," %u%s%s",bone,
                bone<256 && (words[bone>>5]&(0x80000000u>>(bone&31))) ? "*" : "",
                SL_ConvertToString(obj->models[m]->boneNames[b]));
        }
        Com_Printf(CON_CHANNEL_DONT_FILTER,"\n");
        start+=count;
    }
}

static void DestructibleProbe_f(void)
{
    int n;
    gentity_t *ent;
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"destructible needs developer 1\n"); return;
    }
    if(Cmd_Argc()<2) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"usage: destructible list | find <model> | <entnum> [bones | damage <amount> [bullet|splash] [part]]\n");
        return;
    }
    if(!strcmp(Cmd_Argv(1),"list")) {
        for(n=0;n<MAX_GENTITIES;++n) {
            ent=&g_entities[n];
            if(!ent->r.inuse || ent->client || !probe_is_destructible(ent)) continue;
            Com_Printf(CON_CHANNEL_DONT_FILTER,"  %4d %-20s eType %2d slot %4d %-44s hidden %3u takedamage %d at %.0f %.0f %.0f\n",
                n,probe_string(ent->targetname),ent->s.eType,ent->s.index,
                probe_string(OL_G_ModelName(ent->s.index)),probe_hidden_count(ent),ent->takedamage,
                ent->r.currentOrigin[0],ent->r.currentOrigin[1],ent->r.currentOrigin[2]);
        }
        return;
    }
    /* find <model>: every entity of that model, destructible or not (animated
     * props such as mid_wires_02_anim), its detail, and the world position of
     * the model's last bone through the animated skeleton (a tag past 128 on
     * a >128-bone model). */
    if(!strcmp(Cmd_Argv(1),"find") && Cmd_Argc()>2) {
        for(n=0;n<MAX_GENTITIES;++n) {
            DObj *obj;
            ent=&g_entities[n];
            if(!ent->r.inuse || ent->client || Q_stricmp(probe_string(OL_G_ModelName(ent->model)),Cmd_Argv(2))) continue;
            probe_detail(ent);
            obj=Com_GetServerDObj(ent->s.number);
            if(obj && obj->numModels && obj->models[0]->numBones) {
                unsigned last=(unsigned char)obj->models[0]->numBones-1;
                unsigned tag=obj->models[0]->boneNames[last];
                float mat[4][3];
                if(G_DObjGetWorldTagMatrix(ent,tag,mat))
                    Com_Printf(CON_CHANNEL_DONT_FILTER,"  bone %u '%s' world (%.1f %.1f %.1f)\n",last,
                        SL_ConvertToString(tag),mat[3][0],mat[3][1],mat[3][2]);
                else Com_Printf(CON_CHANNEL_DONT_FILTER,"  bone %u '%s' tag lookup FAILED\n",last,SL_ConvertToString(tag));
            }
        }
        return;
    }
    n=atoi(Cmd_Argv(1));
    if(n<0 || n>=MAX_GENTITIES || !g_entities[n].r.inuse) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"entity %d not in use\n",n); return;
    }
    ent=&g_entities[n];
    if(Cmd_Argc()>3 && !strcmp(Cmd_Argv(2),"damage")) {
        float point[3],dir[3]={0.0f,0.0f,-1.0f};
        int splash=Cmd_Argc()<=4 || strcmp(Cmd_Argv(4),"bullet");
        point[0]=ent->r.currentOrigin[0]; point[1]=ent->r.currentOrigin[1];
        point[2]=ent->r.currentOrigin[2]+32.0f;
        if(!ent->takedamage) Com_Printf(CON_CHANNEL_DONT_FILTER,"  entity %d does not take damage\n",n);
        G_Damage(ent,&g_entities[ENTITYNUM_WORLD],&g_entities[ENTITYNUM_WORLD],dir,point,
            atoi(Cmd_Argv(3)),splash?1:0,splash?PROBE_MOD_GRENADE_SPLASH:PROBE_MOD_RIFLE_BULLET,
            -1,0,0u, /* 0: the base model, else attachment index + 1 */
            Cmd_Argc()>5 ? SL_FindLowercaseString(Cmd_Argv(5)) : 0u,0);
        Com_Printf(CON_CHANNEL_DONT_FILTER,"  damaged %d by %s %s\n",n,Cmd_Argv(3),splash?"splash":"bullet");
        return;
    }
    if(Cmd_Argc()>2 && !strcmp(Cmd_Argv(2),"bones")) { probe_bones(ent); return; }
    probe_detail(ent);
}

void RegisterDestructibleProbe(void)
{
    Cmd_AddCommand("destructible",DestructibleProbe_f);
}

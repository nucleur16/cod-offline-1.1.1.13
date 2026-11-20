/* CoDOL explosion probe (developer diagnostic, developer 1 only).
 *
 * explodeprobe <owner clientnum> <weapon> [weapon ...]
 *     For each weapon: G_FireGrenade from the owner 400 units overhead,
 *     then the retained G_ExplodeMissile at once, exactly as the fuse would.
 *     Prints the explosion entity G_ExplodeMissile made (native events,
 *     parameters, weapon, surface, lerp word, flags) and the wire state the
 *     snapshot path would publish for it (ProbeEntityWire), so the
 *     explosion events of different grenades can be compared without a
 *     client.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "qcommon.h"
#include "cmd.h"
#include "bg.h"
#include "bg_public.h"
#include "xassets/weapondef.h"
#include <stdlib.h>
#include <string.h>
#include "ol_events.h"
#include "entities.h"
#include "frame_builder.h"
#include "sv_weapons.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

extern void G_ExplodeMissile(gentity_t *);
extern gentity_t *G_FireGrenade(gentity_t *parent,float *start,float *dir,int weapon,
    unsigned char weaponModel,int rotate,int fuseTime);
extern int BG_FindWeaponIndexForName(const char *name);
extern int RefreshServerAssetNames(void);
extern char entity_seed_failure[160];

static unsigned probe_wire_field(const entity_state_t *wire,unsigned offset,int *found)
{
    unsigned f;
    for(f=0;f<ENTITY_FIELDS;++f) {
        const entity_field_t *field=EntityField(wire->type,f);
        if(field && field->offset==offset) { *found=1; return wire->field[f]; }
    }
    *found=0; return 0;
}

static void probe_one(gentity_t *owner,const char *name)
{
    static const unsigned shown[]={0x008,0x058,0x07c,0x090,0x0b0,0x0b4,0x0b8,0x0bc,0x0c0,
                                   0x0c4,0x0c8,0x0cc,0x0d0,0x0d4,0x0e4};
    unsigned char before[MAX_GENTITIES];
    float start[3],dir[3]={0.0f,0.0f,0.0f};
    int weapon=BG_FindWeaponIndexForName(name),n,count=0;
    unsigned i;
    gentity_t *grenade;
    if(weapon<=0) { Com_Printf(CON_CHANNEL_DONT_FILTER,"%s: no loaded weapon\n",name); return; }
    start[0]=owner->r.currentOrigin[0]; start[1]=owner->r.currentOrigin[1];
    start[2]=owner->r.currentOrigin[2]+400.0f;
    for(n=0;n<MAX_GENTITIES;++n) before[n]=g_entities[n].r.inuse;
    grenade=G_FireGrenade(owner,start,dir,weapon,0,1,3000);
    if(!grenade) { Com_Printf(CON_CHANNEL_DONT_FILTER,"%s: G_FireGrenade failed\n",name); return; }
    before[grenade->s.number]=1;
    Com_Printf(CON_CHANNEL_DONT_FILTER,"%s: native weapon %d projExplosion %d, grenade entity %d\n",
        name,weapon,(int)BG_GetWeaponDef(weapon)->projExplosion,grenade->s.number);
    G_ExplodeMissile(grenade);
    RefreshServerAssetNames(); RefreshServerWeaponMappings();
    for(n=0;n<MAX_GENTITIES;++n) {
        gentity_t *ent=&g_entities[n];
        entity_state_t wire;
        uint32_t events[4],parms[4]; /* vec4_t in the retained declaration */
        int result;
        if(before[n] || !ent->r.inuse) continue;
        memcpy(events,ent->s.events,sizeof(events)); memcpy(parms,ent->s.eventParms,sizeof(parms));
        ++count;
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "  explosion entity %d eType %d events %u/%u/%u/%u parms %u/%u/%u/%u seq %d weapon %d surf %d"
            " lerp.u0 %d eFlags 0x%x time2 %d (level.time %d)\n",
            n,ent->s.eType,
            events[0],events[1],events[2],events[3],parms[0],parms[1],parms[2],parms[3],
            ent->s.eventSequence,ent->s.weapon,ent->s.surfType,ent->s.lerp.u.anonymous.data[0],
            (unsigned)ent->s.lerp.eFlags,ent->s.time2,level.time);
        for(i=0;i<4;++i) if(events[i])
            Com_Printf(CON_CHANNEL_DONT_FILTER,"    native event %u -> wire %d\n",
                events[i],EventToWire(events[i]));
        result=ProbeEntityWire(&ent->s,0,&wire);
        if(result!=1) {
            Com_Printf(CON_CHANNEL_DONT_FILTER,"    wire: refused (%s)\n",
                result ? "common overlay" : entity_seed_failure);
            continue;
        }
        Com_Printf(CON_CHANNEL_DONT_FILTER,"    wire type %u:",wire.type);
        for(i=0;i<sizeof(shown)/sizeof(shown[0]);++i) {
            int found; unsigned value=probe_wire_field(&wire,shown[i],&found);
            if(found) Com_Printf(CON_CHANNEL_DONT_FILTER," +%03x=%u",shown[i],value);
        }
        Com_Printf(CON_CHANNEL_DONT_FILTER,"\n");
    }
    if(!count) Com_Printf(CON_CHANNEL_DONT_FILTER,"  no explosion entity was spawned\n");
}

static void ExplodeProbe_f(void)
{
    int owner,arg;
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"explodeprobe needs developer 1\n"); return;
    }
    if(Cmd_Argc()<3) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"usage: explodeprobe <owner clientnum> <weapon> [weapon ...]\n");
        return;
    }
    owner=atoi(Cmd_Argv(1));
    if(owner<0 || owner>=MAX_CLIENTS || !g_entities[owner].r.inuse || !g_entities[owner].client) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"client %d has no entity\n",owner); return;
    }
    for(arg=2;arg<Cmd_Argc();++arg) probe_one(&g_entities[owner],Cmd_Argv(arg));
}

void RegisterExplosionProbe(void)
{
    Cmd_AddCommand("explodeprobe",ExplodeProbe_f);
}

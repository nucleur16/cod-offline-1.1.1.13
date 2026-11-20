/* Developer diagnostics for the coop soldier fixes (2026-10-08):
 *
 *   codol_actorAudit 1   every actor frame, test the live actor's capsule
 *                        against the collision world and log an actor that
 *                        stands inside solid geometry, inside player clip,
 *                        or that moved THROUGH solid/player clip since its
 *                        last frame (a line at chest height from the old
 *                        origin to the new one is blocked).
 *   codol_traceFireEvents 1
 *                        log every weapon-fire event the snapshot writer
 *                        puts on the wire: native entity, native and wire
 *                        entity type, event, and the weapon word (+d4) C1's
 *                        CG_FireWeapon (0x481f50) returns on when it is 0.
 *
 * Both are read-only probes gated on `developer`; neither changes what the
 * server does.  SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "server.h"
#include "g_shared.h"
#include "bg.h"
#include "cscr_stringlist.h"
#include "cmd.h"
#include "actor_audit.h"
#include "entities.h"
#include "ol_events.h"
#include "entitystate_ext.h"
#include "engine_state.h"
#include "clients.h"
int OverlayServerClient(client_state_t *wire, const clientState_t *engine);
#include <math.h>
#include <string.h>

static cvar_t *auditDvar, *fireTraceDvar;

static int dev_on(void)
{
    return com_developer && com_developer->integer;
}
int ActorAuditEnabled(void)
{ return 0; /* Automatic diagnostic probe disabled. Explicit commands remain. */ }
int TraceFireEventsEnabled(void)
{ return 0; /* Automatic diagnostic probe disabled. Explicit commands remain. */ }

/* IW3 contents: 1 solid, 0x10 glass, 0x10000 player clip, 0x20000 monster
 * clip, 0x800000 utility clip.  A player collides with 0x2810011, a live
 * actor with its own clipmask (0x2820015 at spawn, see actor_spawn.cpp). */
#define AUDIT_SOLID      0x00000011
#define AUDIT_PLAYERCLIP 0x00010000

typedef struct {
    int useCount, valid, logTime;
    float last[3];
} audit_slot_t;
static audit_slot_t slots[MAX_GENTITIES];
static int auditFrames, auditSolid, auditDeep, auditClip, auditCross, auditSummaryTime;

static int capsule_inside(gentity_t *e,const float *origin,int mask,int *hit)
{
    trace_t t;
    /* One unit inside the live hull: touching a wall is not being in it. */
    float mins[3]={e->r.mins[0]+1,e->r.mins[1]+1,e->r.mins[2]+2};
    float maxs[3]={e->r.maxs[0]-1,e->r.maxs[1]-1,e->r.maxs[2]-1};
    G_TraceCapsule(&t,origin,mins,maxs,origin,e->s.number,mask);
    if(t.startsolid || t.allsolid) { *hit=Trace_GetEntityHitId(&t); return 1; }
    return 0;
}
/* How deep: the smallest further shrink of the capsule (all sides) that is
 * clear, 2..16 units; 99 when even 16 is not. */
static int inside_depth(gentity_t *e,const float *origin,int mask)
{
    static const float steps[]={2,4,8,12,16};
    unsigned i; trace_t t;
    for(i=0;i<sizeof(steps)/sizeof(steps[0]);++i) {
        float mins[3]={e->r.mins[0]+1+steps[i],e->r.mins[1]+1+steps[i],e->r.mins[2]+2+steps[i]};
        float maxs[3]={e->r.maxs[0]-1-steps[i],e->r.maxs[1]-1-steps[i],e->r.maxs[2]-1-steps[i]};
        G_TraceCapsule(&t,origin,mins,maxs,origin,e->s.number,mask);
        if(!t.startsolid && !t.allsolid) return (int)steps[i];
    }
    return 99;
}
static int line_blocked(gentity_t *e,const float *from,const float *to,int mask,float *where,int *hit)
{
    trace_t t; float a[3],b[3]; int i;
    for(i=0;i<3;++i){a[i]=from[i];b[i]=to[i];}
    a[2]+=40;b[2]+=40;
    G_TraceCapsule(&t,a,vec3_origin,vec3_origin,b,e->s.number,mask);
    if(t.fraction<1 && !t.startsolid) {
        for(i=0;i<3;++i)where[i]=a[i]+(b[i]-a[i])*t.fraction;
        *hit=Trace_GetEntityHitId(&t);
        return 1;
    }
    return 0;
}

void ActorAuditFrame(gentity_t *e)
{
    audit_slot_t *s; const float *o; int hit=0,logged=0;
    if(!e || !ActorAuditEnabled() || e->s.number<0 || e->s.number>=MAX_GENTITIES) return;
    if(e->s.eType!=ET_ACTOR || e->health<=0) return;
    s=&slots[e->s.number];
    o=e->r.currentOrigin;
    ++auditFrames;
    if(s->valid && s->useCount!=e->useCount) s->valid=0;
    if(capsule_inside(e,o,AUDIT_SOLID,&hit)) {
        /* More than 3 units in (the capsule shrunk by 4 is still solid) is
         * an actor visibly in a wall; less is a graze along one. */
        int depth=inside_depth(e,o,AUDIT_SOLID);
        ++auditSolid;
        if(depth>2) ++auditDeep;
        if(level.time-s->logTime>=1000) {
            Com_Printf(CON_CHANNEL_SERVER,"CoDOL actor audit: ent %d INSIDE SOLID at (%.0f %.0f %.0f) hit ent %d depth <=%d\n",
                e->s.number,o[0],o[1],o[2],hit,depth);
            logged=1;
        }
    } else if(capsule_inside(e,o,AUDIT_PLAYERCLIP,&hit)) {
        ++auditClip;
        if(level.time-s->logTime>=1000) {
            Com_Printf(CON_CHANNEL_SERVER,"CoDOL actor audit: ent %d inside PLAYER CLIP at (%.0f %.0f %.0f) hit ent %d\n",
                e->s.number,o[0],o[1],o[2],hit);
            logged=1;
        }
    }
    if(s->valid) {
        float d[3]={o[0]-s->last[0],o[1]-s->last[1],o[2]-s->last[2]},where[3];
        if(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]>0.25f &&
           line_blocked(e,s->last,o,AUDIT_SOLID|AUDIT_PLAYERCLIP,where,&hit)) {
            ++auditCross;
            Com_Printf(CON_CHANNEL_SERVER,"CoDOL actor audit: ent %d CROSSED solid/clip moving (%.0f %.0f %.0f) -> (%.0f %.0f %.0f) at (%.0f %.0f %.0f) hit ent %d\n",
                e->s.number,s->last[0],s->last[1],s->last[2],o[0],o[1],o[2],where[0],where[1],where[2],hit);
        }
    }
    if(logged) s->logTime=level.time;
    memcpy(s->last,o,12);
    s->useCount=e->useCount;
    s->valid=1;
    if(level.time-auditSummaryTime>=10000) {
        Com_Printf(CON_CHANNEL_SERVER,"CoDOL actor audit: %d actor frames, %d inside solid (%d more than 3 units deep), %d inside player clip, %d crossings\n",
            auditFrames,auditSolid,auditDeep,auditClip,auditCross);
        auditSummaryTime=level.time;
    }
}
/* A scripted placement (forceteleport, ride, dismount) is not a crossing:
 * the next frame starts a new segment. */
void ActorAuditPlaced(gentity_t *e)
{
    if(e && e->s.number>=0 && e->s.number<MAX_GENTITIES) slots[e->s.number].valid=0;
}

/* The snapshot writer only builds frames for connected clients; a probe
 * server's test clients get none.  So each new fire event is pushed through
 * the same seed + overlay path a snapshot uses (ProbeEntityWire, as the
 * brush / gib / explosion probes do), and the wire state it built is
 * reported. */
int ProbeEntityWire(const entityState_t *source,unsigned recipient,entity_state_t *out);
int RefreshServerWeaponMappings(void);
static int fireLastSequence[MAX_GENTITIES];
static int fireLastEventTime[MAX_GENTITIES];
void TraceFireEventsEntity(gentity_t *ent)
{
    int n,isFire=0,newest,result,event=-1,slot=-1;
    unsigned i,weapon=0;
    entity_state_t wire;
    if(!ent || !TraceFireEventsEnabled()) return;
    n=ent->s.number;
    if(n<0 || n>=MAX_GENTITIES) return;
    /* Effect and impact temp entities: what C1 is asked to draw, with the
     * wire state (+80 shooter, +90 surface, +ac effect/direction, +d4 weapon). */
    if(ent->s.eType==ET_EVENTS+EV_PLAY_FX || ent->s.eType==ET_EVENTS+EV_BULLET_HIT ||
       ent->s.eType==ET_EVENTS+EV_BULLET_HIT_CLIENT_SMALL ||
       ent->s.eType==ET_EVENTS+EV_BULLET_HIT_CLIENT_LARGE) {
        unsigned k; char fields[200]; size_t used=0;
        if(fireLastEventTime[n]==ent->eventTime) return;
        fireLastEventTime[n]=ent->eventTime;
        RefreshServerWeaponMappings();
        result=ProbeEntityWire(&ent->s,0,&wire);
        fields[0]=0;
        if(result==1) for(k=0;k<ENTITY_FIELDS;++k) {
            const entity_field_t *field=EntityField(wire.type,k);
            if(field && (field->offset==0x080 || field->offset==0x088 || field->offset==0x090 ||
               field->offset==0x09c || field->offset==0x0ac || field->offset==0x0d4 ||
               field->offset==0x0e0) && used<sizeof(fields)-24)
                used+=(size_t)snprintf(fields+used,sizeof(fields)-used," %x=%u",field->offset,wire.field[k]);
        }
        Com_Printf(CON_CHANNEL_SERVER,"CoDOL fx wire: t %d ent %d event %d (wire %d) parm %d other %d surf %d weapon %d probe %d%s\n",
            level.time,n,ent->s.eType-ET_EVENTS,EventToWire((unsigned)(ent->s.eType-ET_EVENTS)),
            ent->s.eventParm,ent->s.otherEntityNum,ent->s.surfType,ent->s.weapon,result,fields);
        return;
    }
    if(ent->s.eType==ET_EVENTS+EV_FIRE_WEAPON || ent->s.eType==ET_EVENTS+EV_FIRE_WEAPON_LASTSHOT) {
        if(fireLastEventTime[n]==ent->eventTime) return;
        fireLastEventTime[n]=ent->eventTime;
        isFire=1;
        event=ent->s.eType-ET_EVENTS;
    } else if(ent->s.eType<ET_EVENTS && ent->s.eventSequence!=fireLastSequence[n]) {
        uint32_t code;
        fireLastSequence[n]=ent->s.eventSequence;
        newest=(ent->s.eventSequence-1)&3;
        /* The retained entityState declares the ring as vec4_t; the words
         * are event numbers (script_events.c reads them the same way). */
        memcpy(&code,&ent->s.events[newest],sizeof(code));
        isFire=code==EV_FIRE_WEAPON || code==EV_FIRE_WEAPON_LASTSHOT;
        event=(int)code;
        slot=newest;
    }
    if(!isFire) return;
    /* What a connected client's snapshot does first (sv_weapons.c):
     * a probe server with only test clients never runs it after a map
     * change, and its engine->wire weapon map would be stale. */
    RefreshServerWeaponMappings();
    result=ProbeEntityWire(&ent->s,0,&wire);
    /* A player's weapon pair is overlaid after the common path, exactly as
     * the snapshot collector does (sv_entities.c). */
    if(result==1 && ent->s.eType==ET_PLAYER) {
        unsigned primary=0;
        if(!EntityStatePrimaryWeapon(&ent->s,&primary) ||
           !OverlayEnginePlayerEntityWeapons(&wire,&ent->s,primary)) result=-2;
    }
    if(result==1) {
        unsigned k; char ring[160]; size_t used=0;
        ring[0]=0;
        for(k=0;k<ENTITY_FIELDS;++k) {
            const entity_field_t *field=EntityField(wire.type,k);
            if(field && field->offset>=0x0b0 && field->offset<=0x0d0 && used<sizeof(ring)-16)
                used+=(size_t)snprintf(ring+used,sizeof(ring)-used," %x=%u",field->offset,wire.field[k]);
        }
        Com_Printf(CON_CHANNEL_SERVER,"CoDOL fire wire: t %d ent %d queue%s\n",level.time,n,ring);
        if(ent->s.eType==ET_PLAYER && n<MAX_CLIENTS) {
            /* The firing player's clientState as the snapshot publishes it:
             * +12c/+130 name the entities holding its right/left world gun,
             * which C1's CG flashes and takes the muzzle point from. */
            client_state_t cs; unsigned w0=0,w1=0;
            memset(&cs,0,sizeof(cs));
            if(OverlayServerClient(&cs,G_GetClientState(n))) {
                for(k=0;k<CLIENT_FIELDS;++k) {
                    if(ClientField(k)->offset==0x12c) w0=cs.field[k];
                    if(ClientField(k)->offset==0x130) w1=cs.field[k];
                }
                if(w0>0 && w0<MAX_GENTITIES && g_entities[w0].r.inuse) {
                    /* The gun entity's own wire: C1 flashes it only when its
                     * +68 weapon equals the fired weapon (480227..480234). */
                    entity_state_t gun; unsigned g;
                    if(ProbeEntityWire(&g_entities[w0].s,0,&gun)==1)
                        for(g=0;g<ENTITY_FIELDS;++g) {
                            const entity_field_t *field=EntityField(gun.type,g);
                            if(field && (field->offset==0x068 || field->offset==0x0e8))
                                Com_Printf(CON_CHANNEL_SERVER,"CoDOL fire wire: t %d gun ent %u type %u +%x=%u\n",
                                    level.time,w0,gun.type,field->offset,gun.field[g]);
                        }
                }
                Com_Printf(CON_CHANNEL_SERVER,"CoDOL fire wire: t %d ent %d clientState 12c=%u 130=%u%s",
                    level.time,n,w0,w1,"\n");
            } else Com_Printf(CON_CHANNEL_SERVER,"CoDOL fire wire: t %d ent %d clientState not convertible%s",level.time,n,"\n");
        }
    }
    if(result!=1) {
        Com_Printf(CON_CHANNEL_SERVER,"CoDOL fire wire: t %d ent %d type %d: NOT publishable (probe %d)\n",
            level.time,n,ent->s.eType,result);
        return;
    }
    for(i=0;i<ENTITY_FIELDS;++i) {
        const entity_field_t *field=EntityField(wire.type,i);
        if(field && field->offset==0x0d4) { weapon=wire.field[i]&0xffffu; break; }
    }
    Com_Printf(CON_CHANNEL_SERVER,"CoDOL fire wire: t %d ent %d type %d->%u event %d (wire %d) slot %d native weapon %d wire weapon %u%s\n",
        level.time,n,ent->s.eType,wire.type,event,EventToWire((unsigned)event),slot,ent->s.weapon,weapon,
        weapon ? "" : " (C1 CG_FireWeapon returns on weapon 0: no flash, no sound)");
}
#endif
#ifdef OL_SERVER
/* modelslot <name> (developer): where G_ModelIndex would find a model
 * name now -- the IW3 configstring slots (by script-string handle and by
 * text), the CoDOL high slots, and the constant-configstring index. */
unsigned int __cdecl SV_GetConfigstringConst(int index);
int FindHighModel(unsigned handle);
extern struct XModel *cached_models[];
static void ModelSlot_f(void)
{
    const char *name; unsigned handle; int i,found=0,ccs;
    if(Cmd_Argc()<2){Com_Printf(CON_CHANNEL_DONT_FILTER,"usage: modelslot <model>\n");return;}
    name=Cmd_Argv(1);
    if(!strcmp(name,"all")) {
        for(i=1;i<512;++i) {
            unsigned cs=SV_GetConfigstringConst(0x33e + i);
            const char *text=cs?SL_ConvertToString(cs):"";
            if(text && text[0])
                Com_Printf(CON_CHANNEL_DONT_FILTER,"CoDOL modelslot: slot %d '%s' cached %p '%s'\n",i,text,(void*)cached_models[i],
                    cached_models[i] && *(const char *const *)cached_models[i] ? *(const char *const *)cached_models[i] : "");
        }
        return;
    }
    handle=SL_FindLowercaseString(name);
    ccs=CCS_GetConstConfigStringIndex(name);
    Com_Printf(CON_CHANNEL_DONT_FILTER,"CoDOL modelslot: '%s' handle %u const index %d (cs %d) high slot %d initializing %d\n",
        name,handle,ccs,ccs>=0?CCS_GetConfigStringNumForConstIndex(ccs):-1,FindHighModel(handle),level.initializing);
    for(i=1;i<512;++i) {
        unsigned cs=SV_GetConfigstringConst(0x33e + i);
        const char *text=cs?SL_ConvertToString(cs):"";
        if(cs==handle || (text && !Q_stricmp(text,name))) {
            Com_Printf(CON_CHANNEL_DONT_FILTER,"CoDOL modelslot:   slot %d cs handle %u text '%s' cached %p\n",
                i,cs,text?text:"",(void*)cached_models[i]);
            ++found;
        }
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,"CoDOL modelslot: %d IW3 slot(s)\n",found);
}
void RegisterModelSlotProbe(void)
{
    Cmd_AddCommand("modelslot",ModelSlot_f);
}
#endif

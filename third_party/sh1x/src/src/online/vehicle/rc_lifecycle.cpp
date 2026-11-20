#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* T5 RC script interface adaptation. C1 has no proven T5 eFlags2 analogue:
 * these flags remain server-owned and have explicit native/script consumers.
 * Stun suppresses driving demands; countdown/timeout notify presentation.
 * They are never copied into unrelated C1 entity flags. */
#ifdef OL_SERVER
#include "rc_lifecycle.h"
#include "vehicle_ground_runtime.h"
#include "vehicle_native.h"
#include "g_shared.h"
#include "g_scr_vehicle.h"
#include "cscr_stringlist.h"
#include <cstring>
namespace {
struct state_t { unsigned generation;int initialized,owner,owner_generation;unsigned flags; };
state_t states[ENTITYNUM_MAX_NORMAL];
bool valid(const gentity_t *e) {
    return e&&e->s.number>=0&&e->s.number<ENTITYNUM_MAX_NORMAL&&e==&g_entities[e->s.number]&&
        e->r.inuse&&e->s.eType==ET_VEHICLE&&GroundVehicleOwned(e);
}
state_t *state(const gentity_t *e) {
    if(!valid(e))return NULL;
    state_t &s=states[e->s.number];
    if(!s.initialized||s.generation!=(unsigned)e->useCount) {
        std::memset(&s,0,sizeof(s));s.initialized=1;s.generation=(unsigned)e->useCount;s.owner=-1;
    }
    return &s;
}
gentity_t *receiver(scr_entref_t ref) {
    if(ref.classnum||ref.entnum>=ENTITYNUM_MAX_NORMAL||!valid(&g_entities[ref.entnum])) {
        Scr_ObjectError("method requires an owned native RC vehicle");return NULL;
    }
    return &g_entities[ref.entnum];
}
void flagMethod(scr_entref_t ref,int operation) {
    gentity_t *e=receiver(ref);if(!e)return;
    if(Scr_GetNumParam()!=1){Scr_Error("vehicle client flag requires one integer index");return;}
    int bit=Scr_GetInt(0);if((unsigned)bit>=16){Scr_ParamError(0,"vehicle client flag index must be0..15");return;}
    state_t *s=state(e);unsigned mask=1u<<bit;
    if(operation==0){Scr_AddInt((s->flags&mask)!=0);return;}
    const unsigned next=operation>0?s->flags|mask:s->flags&~mask;
    if(next==s->flags)return;
    s->flags=next;
    // Original T5 CSC flags9/1/2 are identified by pinned _clientflags.gsc.
    // C1 presentation is adapted by server GSC consuming this transition;
    // no T5 CSC is injected into the C1 client. No entity use after notify.
    unsigned event=SL_GetString("codol_vehicle_clientflag",0);
    Scr_AddInt(operation>0);Scr_AddInt(bit);Scr_Notify(e,event,2);
    SL_RemoveRefToString(event);
}
}
extern "C" void Scr_VehicleSetOwner(scr_entref_t ref) {
    gentity_t *e=receiver(ref);if(!e)return;
    if(Scr_GetNumParam()!=1){Scr_Error("setowner requires one player entity");return;}
    gentity_t *p=Scr_GetEntity(0);
    if(!p||!p->r.inuse||!p->client||p->s.number<0||p->s.number>=32||p!=&g_entities[p->s.number]) {
        Scr_ParamError(0,"setowner requires a live C1 player");return;
    }
    state_t *s=state(e);s->owner=p->s.number;s->owner_generation=p->useCount;
    // C1 native vehicle ownership presentation uses entityState.clientNum at
    // wire74. T5 packed faction bits and C1 exclusive remote r.ownerNum are
    // separate contracts; neither is overwritten with the other's encoding.
    CaptureVehicleSpawnClient(e,p);
}
extern "C" void Scr_VehicleGetClientFlag(scr_entref_t r){flagMethod(r,0);}
extern "C" void Scr_VehicleSetClientFlag(scr_entref_t r){flagMethod(r,1);}
extern "C" void Scr_VehicleClearClientFlag(scr_entref_t r){flagMethod(r,-1);}
extern "C" int RCVehicleStunned(const gentity_t *e) {
    state_t *s=state(e);return s&&(s->flags&(1u<<9));
}
/* T5 maps/mp/_rcbomb.gsc blowup: self radiusDamage( origin+(0,0,10), 256,
 * 350, 25, ... ) goes through T5 G_RadiusDamage, which measures a non-brush
 * target from its origin (a player's feet), not from the edge of its box as
 * C1 does. Measured on real T5 (mp_nuked, a ring of players): damage is
 * exactly 350-325*d/256 of the origin distance (51 u -> 284, 151 u -> 158),
 * where the box rule here gave 25-30 more at every range. */
extern "C" int RCVehicleBlastFromOrigin(const gentity_t *e) {
    return valid(e);
}
extern "C" gentity_t *RCVehicleOwner(const gentity_t *e) {
    state_t *s=state(e);if(!s||s->owner<0||s->owner>=32)return NULL;
    gentity_t *p=&g_entities[s->owner];
    return p->r.inuse&&p->client&&p->useCount==s->owner_generation?p:NULL;
}
extern "C" void RCEntityFreed(const gentity_t *e) {
    if(!e||e->s.number<0||e->s.number>=ENTITYNUM_MAX_NORMAL)return;
    state_t &s=states[e->s.number];
    if(s.initialized&&s.generation==(unsigned)e->useCount)std::memset(&s,0,sizeof(s));
}
extern "C" void RCResetLifecycle(void){std::memset(states,0,sizeof(states));}
#endif

#ifdef OL_SERVER
#include "persistent_world.h"
static int resident_world_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}
extern "C" int PersistentRCLifecycleEmpty(void){return resident_world_zero(states,sizeof(states));}

#endif

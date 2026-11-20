#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* C1 679b80/679e60 remote enter/exit ownership, not missile camera controls.
 * VehicleDef+2c8 gates saving body angles and skipping physical linking.
 * otherFlags400000, eFlags100000 and viewlocked_entNum are already captured
 * by the native snapshot stack. Type at C1 ps1f8 is NOT a netfield.
 * Script driver interfaces are available; the RC reward remains disabled until
 * the ground controller and original T5 lifecycle are integrated. */
#ifdef OL_SERVER
#include "remote_vehicle.h"
#include "vehicle_native.h"
#include "script_startup_extra.h"
#include "g_shared.h"
#include "g_scr_vehicle.h"
#include "scr_vm.h"
#include "cscr_stringlist.h"
#include "remote_vehicle_script.h"
#include "dobj.h"
#include "q_math.h"
#include "qcommon.h"
#include <math.h>
#include <string.h>
extern "C" scr_vehicle_s s_vehicles[8];

typedef struct {
    int active,vehicle,player_generation,vehicle_generation,type;
    float body_angles[3];
} remote_driver_t;
static remote_driver_t drivers[MAX_CLIENTS];
static gentity_t *live(unsigned number,int generation)
{
    if(number>=ENTITYNUM_MAX_NORMAL)return NULL;
    gentity_t *e=&g_entities[number];
    return e->r.inuse && e->useCount==generation ? e : NULL;
}
static int vehicle_record(const gentity_t *v)
{
    if(!v || !v->r.inuse || v->s.number<0 || v->s.number>=ENTITYNUM_MAX_NORMAL ||
       v!=&g_entities[v->s.number] || v->s.eType!=ET_VEHICLE || !v->scr_vehicle)return -1;
    uintptr_t address=(uintptr_t)v->scr_vehicle,base=(uintptr_t)s_vehicles;
    if(address<base || address-base>=sizeof(s_vehicles) ||
       (address-base)%sizeof(scr_vehicle_s) ||
       v->scr_vehicle->entNum!=v->s.number)return -1;
    return v->scr_vehicle->infoIdx;
}
void RemoteVehicleExit(unsigned client)
{
    if(client>=MAX_CLIENTS || !drivers[client].active)return;
    remote_driver_t old=drivers[client];
    memset(&drivers[client],0,sizeof(drivers[client]));
    gentity_t *p=live(client,old.player_generation);
    gentity_t *v=live((unsigned)old.vehicle,old.vehicle_generation);
    if(v && v->r.ownerNum.number==client+1) {
        v->r.ownerNum.setEnt(NULL);
        v->s.lerp.eFlags&=~0x100000;
        if(vehicle_record(v)>=0)v->scr_vehicle->flags&=~1;
    }
    if(!p || !p->client)return;
    /* Do not clear a replacement owner's state after an external unlink. */
    if(p->r.ownerNum.number && p->r.ownerNum.number!=old.vehicle+1)return;
    if(p->r.ownerNum.number)p->r.ownerNum.setEnt(NULL);
    p->client->ps.eFlags&=~0x100000;
    p->client->ps.otherFlags&=~0x1c00000;
    p->client->ps.viewlocked_entNum=ENTITYNUM_NONE;
    /* Release C1's controls-linked pmove bit (ps+0xc 0x80000) unless a
     * retained tag link still owns it. */
    if(!p->tagInfo)p->client->ps.pm_flags&=~0x80000u;
    ((void)0);
    SetClientViewAngle(p,old.body_angles);
    /* C1 leaves its saved linkAngles in place after exit. */
}
void RemoteVehicleValidate(unsigned client)
{
    if(client>=MAX_CLIENTS || !drivers[client].active)return;
    remote_driver_t *d=&drivers[client];
    gentity_t *p=live(client,d->player_generation);
    gentity_t *v=live((unsigned)d->vehicle,d->vehicle_generation);
    if(!p || !p->client || !v || vehicle_record(v)<0 ||
       p->r.ownerNum.number!=d->vehicle+1 || v->r.ownerNum.number!=client+1 ||
       !(p->client->ps.otherFlags&0x400000))RemoteVehicleExit(client);
}
int RemoteVehicleType(unsigned client)
{
    RemoteVehicleValidate(client);
    return client<MAX_CLIENTS && drivers[client].active ? drivers[client].type : -1;
}
int RemoteVehicleEnter(gentity_t *p,gentity_t *v,const float angles[3])
{
    if(!p || !v || !angles || !p->r.inuse || !p->client ||
       p->s.number<0 || p->s.number>=MAX_CLIENTS ||
       p!=&g_entities[p->s.number] || p==v)return 0;
    unsigned client=(unsigned)p->s.number;
    RemoteVehicleValidate(client);
    int index=vehicle_record(v);
    const vehicle_native_t *def=NativeVehicleDefinition(index);
    if(!def || !def->remote_control || def->type<0 || drivers[client].active ||
       p->tagInfo || p->r.ownerNum.number || v->r.ownerNum.number ||
       (v->scr_vehicle->flags&1) || (p->client->ps.eFlags&0x100000))return 0;
    for(int i=0;i<3;++i)
        if(!isfinite(angles[i]) || !isfinite(p->client->ps.viewangles[i]))return 0;
    remote_driver_t d={};
    d.active=1;d.vehicle=v->s.number;d.type=def->type;
    d.player_generation=p->useCount;d.vehicle_generation=v->useCount;
    memcpy(d.body_angles,p->client->ps.viewangles,sizeof(d.body_angles));
    /* Seed the actual live link record; its capture path owns wire1d4..1dc. */
    if(!PlayerLinkSetRemoteAngles(client,d.body_angles))return 0;
    float view[3]={angles[0],angles[1],0};
    SetClientViewAngle(p,view);
    p->r.ownerNum.setEnt(v);v->r.ownerNum.setEnt(p);
    v->scr_vehicle->flags|=1;
    v->s.lerp.eFlags|=0x100000;
    p->client->ps.eFlags|=0x100000;
    p->client->ps.otherFlags=(p->client->ps.otherFlags&~0x1000000)|0x400000;
    p->client->ps.viewlocked_entNum=v->s.number;
    /* C1 Pmove 0x416c40 clears locomotion/buttons only for ps+0xc bit
     * 0x80000 (the bit C1 0x548480 sets for a controls-linked player) or
     * usercmd button 0x200000. Remote enter alone never set it, so the
     * driver's own usercmd kept walking the player while it drove the car
     * (live 559438f6). The vehicle reads the raw executed usercmd, so the
     * filtered pmove copy does not starve the driver input. */
    p->client->ps.pm_flags|=0x80000u;
    ((void)0);
    p->r.currentAngles[1]=d.body_angles[1];
    drivers[client]=d;
    return 1;
}
void RemoteVehicleEntityFreed(const gentity_t *entity)
{
    if(!entity)return;
    for(unsigned i=0;i<MAX_CLIENTS;++i)
        if(drivers[i].active && (entity->s.number==(int)i ||
           entity->s.number==drivers[i].vehicle))RemoteVehicleExit(i);
}
void RemoteVehicleResetAll(void)
{
    for(unsigned i=0;i<MAX_CLIENTS;++i)RemoteVehicleExit(i);
}
#endif

#ifdef OL_SERVER
/* T5 seat0 is the driver. This query uses the same generation-checked record
 * that owns C1 reciprocal handles; an assigned damage owner is not a driver. */
gentity_t *RemoteVehicleSeatOccupant(gentity_t *vehicle,int seat)
{
    if(seat!=0 || vehicle_record(vehicle)<0)return NULL;
    for(unsigned i=0;i<MAX_CLIENTS;++i) {
        RemoteVehicleValidate(i);
        if(drivers[i].active && drivers[i].vehicle==vehicle->s.number &&
           drivers[i].vehicle_generation==vehicle->useCount)
            return live(i,drivers[i].player_generation);
    }
    return NULL;
}
/* Script exit notifies only after fully clearing control state. Silent reset
 * and entity-free paths retain their previous no-script-notify contract. */
int RemoteVehicleScriptExit(unsigned client)
{
    if(client>=MAX_CLIENTS)return 0;
    RemoteVehicleValidate(client);
    if(!drivers[client].active)return 0;
    remote_driver_t old=drivers[client];
    gentity_t *player=live(client,old.player_generation);
    gentity_t *vehicle=live((unsigned)old.vehicle,old.vehicle_generation);
    RemoteVehicleExit(client);
    if(!player || !vehicle)return 1;
    unsigned event=SL_GetString("exit_vehicle",0);
    Scr_AddInt(0);Scr_AddEntity(vehicle);Scr_Notify(player,event,2);
    /* Either notification may delete/reuse a participant. Never notify a new
     * entity or tear down newly acquired control after the first callback. */
    player=live(client,old.player_generation);
    vehicle=live((unsigned)old.vehicle,old.vehicle_generation);
    if(player && vehicle) {
        Scr_AddInt(0);Scr_AddEntity(player);Scr_Notify(vehicle,event,2);
    }
    SL_RemoveRefToString(event);
    return 1;
}
static gentity_t *script_vehicle(scr_entref_t ref)
{
    if(ref.classnum || ref.entnum>=ENTITYNUM_MAX_NORMAL) {
        Scr_ObjectError("not an entity");return NULL;
    }
    gentity_t *entity=&g_entities[ref.entnum];
    if(vehicle_record(entity)<0) {Scr_ObjectError("entity is not a vehicle");return NULL;}
    return entity;
}
void Scr_UseVehicle(scr_entref_t ref)
{
    gentity_t *vehicle=script_vehicle(ref);
    if(!vehicle)return;
    if(Scr_GetNumParam()!=2) {Scr_Error("usevehicle requires player and seat index");return;}
    gentity_t *player=Scr_GetEntity(0);int seat=Scr_GetInt(1);
    if(!player || !player->client)return; /* T5 nonplayer argument is a no-op. */
    const vehicle_native_t *def=NativeVehicleDefinition(vehicle_record(vehicle));
    if(!def || !def->remote_control || seat!=0) {
        Scr_Error("usevehicle: this vehicle has no native remote driver seat at that index");return;
    }
    /* C1 67a1aa stores tag_player in378; 679bb8 requires its world matrix.
     * Optional tag_barrel390 supplies aim axis; otherwise use tag_player.
     * Never use entity yaw or guessed camera height when a bone is absent. */
    float matrix[4][3],barrel[4][3],angles[3];
    if(!G_DObjGetWorldTagMatrix(vehicle,scr_const.tag_player,matrix)) {
        Scr_Error("usevehicle: vehicle model has no tag_player");return;
    }
    if(G_DObjGetWorldTagMatrix(vehicle,scr_const.tag_barrel,barrel))
        AxisToAngles(barrel,angles);
    else AxisToAngles(matrix,angles);
    int playerGeneration=player->useCount,vehicleGeneration=vehicle->useCount;
    unsigned client=(unsigned)player->s.number,number=(unsigned)vehicle->s.number;
    RemoteVehicleScriptExit(client);
    if(live(client,playerGeneration)!=player || live(number,vehicleGeneration)!=vehicle)return;
    if(!RemoteVehicleEnter(player,vehicle,angles))
        Scr_Error("usevehicle: occupied or conflicting native vehicle control ownership");
}
void Scr_GetSeatOccupant(scr_entref_t ref)
{
    gentity_t *vehicle=script_vehicle(ref);if(!vehicle)return;
    int seat=Scr_GetInt(0);
    gentity_t *occupant=RemoteVehicleSeatOccupant(vehicle,seat);
    if(occupant)Scr_AddEntity(occupant); /* Empty/absent seat returns undefined. */
}
/* "unlink" on a player. T5 VEH_UnlinkPlayer (OpenBLOPS g_scr_vehicle.cpp
 * 2263) is what the RC script reaches through `owner Unlink()`: it clears the
 * seat and notifies exit_vehicle on both player and vehicle, and the RC
 * lifecycle (_rcbomb::exitCarWaiter -> "rcbomb_done") depends on that
 * notification. The retained ScrCmd_Unlink only releases a tag link, and the
 * CoDOL receiver that knows about remote control and player links
 * (PlayerLinkUnlinkReceiver) was written for a registration wrapper
 * that never existed. This is that wrapper: the CoDOL receiver first, then
 * the retained tag unlink for everything it does not claim. */
extern "C" void ScrCmd_Unlink(scr_entref_t);
extern "C" qboolean PlayerLinkUnlinkReceiver(scr_entref_t);
void Scr_Unlink(scr_entref_t ref)
{
    if(PlayerLinkUnlinkReceiver(ref))return;
    ScrCmd_Unlink(ref);
}
void PlayerCmd_IsRemoteControlling(scr_entref_t ref)
{
    if(ref.classnum || ref.entnum>=MAX_CLIENTS ||
       !g_entities[ref.entnum].r.inuse || !g_entities[ref.entnum].client) {
        Scr_ObjectError("entity is not a player");return;
    }
    RemoteVehicleValidate(ref.entnum);
    Scr_AddBool((g_entities[ref.entnum].client->ps.otherFlags&0x400000)!=0);
}
#endif


#ifdef OL_SERVER
#include "persistent_room.h"
static int resident_room_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}
extern "C" int PersistentRemoteDriversEmpty(void) {return resident_room_zero(drivers,sizeof(drivers));}

#endif

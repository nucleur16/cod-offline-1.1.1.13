/* Production name publication and client-state assembly with controlled engine strings. */
#include "server.h"
#include "g_shared.h"
#include "sv_state.h"
#include "engine_state.h"
#include "qcommon_io.h"
#include "g_hud.h"
#include "hud_fontscale.h"
#include "sv_frame.h"
#include "frame_builder.h"
#include "usercmd.h"
#include "sv_weapons.h"
#include "weapon_native.h"
#include "weapon_lock.h"
#include "entitystate_ext.h"
#include "playerstate_ext.h"
#include "clientstate_ext.h"
#include "actor_delivery.h"
#include "actor_state.h"
#include "sv_entities.h"
#include "glass_snapshot.h"
#include "effects.h"
#include "voice.h"
#include <assert.h>
gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
struct XModel *cached_models[NATIVE_MODELS];
cvar_t *ol_debugTrace, *com_developer;
static struct WeaponDef native_none;
int BG_GetNumWeapons(void) { return 2; }
int CM_NumInlineModels(void) { return 4; }
/* The server appends the map's trigger hulls after the BSP's inline models;
 * here models 1-2 exist on the client and handle 3 is a server-only trigger. */
unsigned ClientInlineModelCount(void) { return 3; }
int IsTriggerClipHandle(int handle) { return handle==3; }
int ClientIsBotClient(unsigned client) { (void)client; return 1; }
struct WeaponDef *BG_GetWeaponDef(int index) { assert(index>=0 && index<2); return &native_none; }
int BuildServerAmmoBindings(engine_player_bindings_t *out,const playerState_t *ps)
{
    unsigned i; (void)ps;
    out->clip_mask=out->reserve_mask=0x7fff;
    for(i=0;i<16;++i) {
        out->clips[i]=(ammo_binding_t){-1,-1};
        out->reserves[i]=(ammo_binding_t){-1,0};
    }
    return 1;
}
game_hudelem_t g_hudelems[MAX_HUDELEMS];
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
_Static_assert(offsetof(player_array_updates_t,stats)==4,"CoDOL arrays ignore packed engine headers");
_Static_assert(_Alignof(hud_state_t)==_Alignof(uint32_t),"HUD state retains natural alignment");
_Static_assert(sizeof(player_field_t)==6,"netfield descriptors retain natural layout");
server_t sv;
serverStatic_t svs;
svsHeader_t svsHeader;
static cvar_t maximum;
cvar_t *sv_maxclients = &maximum;
static const char *strings[8] = {NULL, "", "body", "attachment", "tag_weapon"};
static int commands;
static char last_command[256];
static cvar_t replicated,private_var;
cvar_t *Cvar_FindVar(const char *name)
{
    if(!strcmp(name,"shared_setting")) return &replicated;
    if(!strcmp(name,"private_setting")) return &private_var;
    return NULL;
}
char *Cvar_DisplayableValue(const cvar_t *var) { return var->string; }
static unsigned field(const client_state_t *wire, unsigned offset)
{
    unsigned i;
    for (i = 0; i < CLIENT_FIELDS; ++i)
        if (ClientField(i)->offset == offset) return wire->field[i];
    assert(0); return 0;
}
int SV_GetConfigstringIndex(int index) { return sv.configstrings[index]; }
static unsigned entity_field(const entity_state_t *wire, unsigned offset)
{
    unsigned i;
    for(i=0;i<ENTITY_FIELDS;++i)
        if(EntityField(wire->type,i)->offset==offset) return wire->field[i];
    assert(0); return 0;
}
static void model_entity_test(void)
{
    gentity_t engine={0};
    entity_state_t wire, saved;
    unsigned i;
    memset(&wire,0xa5,sizeof(wire)); wire.type=ET_SCRIPTMOVER;
    engine.s.eType=ET_SCRIPTMOVER; engine.s.index=7;
    assert(OverlayServerModelEntity(&wire,&engine));
    assert(entity_field(&wire,0x5c)==1 && entity_field(&wire,0x88)==0xa5a5a5a5);
    /* C1 0x567960: a script mover takes no marks unless told (+58 = none) */
    assert(entity_field(&wire,0x58)==0x7ff);
    engine.s.index=9;
    assert(OverlayServerModelEntity(&wire,&engine) && entity_field(&wire,0x5c)==2);
    engine.s.index=0;
    assert(OverlayServerModelEntity(&wire,&engine) && entity_field(&wire,0x5c)==0);
    saved=wire;
    for(i=0;i<6;++i) {
        memset(&engine,0,sizeof(engine)); engine.s.eType=ET_SCRIPTMOVER; engine.s.index=7;
        switch(i) {
        case 0: engine.s.index=1; break; /* unpublished */
        case 1: engine.s.index=MAX_MODELS; break;
        case 2: engine.r.bmodel=1; break;
        case 3: engine.s.solid=SOLID_BMODEL; break;
        case 4: engine.s.eType=ET_PLAYER; break;
        case 5: engine.s.eType=ET_GENERAL; break; /* seed type mismatch */
        }
        assert(!OverlayServerModelEntity(&wire,&engine));
        assert(!memcmp(&wire,&saved,sizeof(wire)));
    }
    engine.s.eType=ET_GENERAL; wire.type=ET_GENERAL;
    assert(OverlayEngineModelEntity(&wire,&engine.s,2047,0));
    assert(entity_field(&wire,0x98)==2047);
    saved=wire;
    assert(!OverlayEngineModelEntity(&wire,&engine.s,2048,0));
    assert(!OverlayEngineModelEntity(&wire,&engine.s,0,0));
    assert(!OverlayEngineModelEntity(&wire,&engine.s,1,1));
    assert(!OverlayEngineModelEntity(&wire,&engine.s,1,-1));
    engine.s.index=0;
    assert(!OverlayEngineModelEntity(&wire,&engine.s,1,0));
    assert(!OverlayEngineModelEntity(&wire,NULL,1,0));
    assert(!OverlayEngineModelEntity(NULL,&engine.s,1,0));
    assert(!OverlayServerModelEntity(&wire,NULL));
    assert(!OverlayServerModelEntity(NULL,&engine));
    assert(!memcmp(&wire,&saved,sizeof(wire)));
}
static void brush_entities(void)
{
    entityState_t source={0}; entity_state_t wire={0}, before;
    source.eType=ET_SCRIPTMOVER; source.index=1; source.solid=SOLID_BMODEL;
    wire.type=ET_SCRIPTMOVER;
    assert(OverlayEngineBrushEntity(&wire,&source,2));
    assert(entity_field(&wire,0x98)==1 && entity_field(&wire,0xa8)==0xffffff);
    assert(entity_field(&wire,0x58)==0x7ff);
    assert(entity_field(&wire,0x08)==0);
    /* notSolid's EF_NONSOLID_BMODEL reaches C1 (CG_ClipMoveToEntity
     * 0x48571e skips such a brush); other native eFlags bits do not. */
    source.lerp.eFlags=0x41;
    assert(OverlayEngineBrushEntity(&wire,&source,2));
    assert(entity_field(&wire,0x08)==1);
    source.lerp.eFlags=0;
    assert(OverlayEngineBrushEntity(&wire,&source,2));
    assert(entity_field(&wire,0x08)==0);
    before=wire;
    assert(!OverlayEngineBrushEntity(&wire,&source,1));
    source.index=0; assert(!OverlayEngineBrushEntity(&wire,&source,2));
    source.index=2048; assert(!OverlayEngineBrushEntity(&wire,&source,3000));
    source.index=1; source.solid=0; assert(!OverlayEngineBrushEntity(&wire,&source,2));
    source.solid=SOLID_BMODEL; source.eType=ET_ITEM;
    assert(!OverlayEngineBrushEntity(&wire,&source,2));
    assert(!memcmp(&wire,&before,sizeof(wire)));
    /* A trigger leaves as C1's link type 3 does: no solid, no model. */
    {
        entityState_t trigger={0}; entity_state_t out={0};
        trigger.eType=ET_GENERAL; trigger.index=163; trigger.solid=SOLID_BMODEL;
        out.type=ET_GENERAL;
        assert(OverlayEngineTriggerEntity(&out,&trigger));
        assert(entity_field(&out,0x98)==0 && entity_field(&out,0xa8)==0);
        trigger.solid=0; assert(!OverlayEngineTriggerEntity(&out,&trigger));
        trigger.solid=SOLID_BMODEL; trigger.eType=ET_ITEM;
        assert(!OverlayEngineTriggerEntity(&out,&trigger));
    }
}
const char *SL_ConvertToString(unsigned index) { assert(index < 8); return strings[index]; }
void SL_RemoveRefToString(unsigned index) { assert(!index); }
void Com_Printf(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
void Com_DPrintf(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
void Q_strncpyz(char *dest,const char *source,int capacity)
{ assert(dest && source && capacity>0); snprintf(dest,(size_t)capacity,"%s",source); }
void KillcamTraceFrame(void) { }
/* No client talks in this fixture: the voice relay (voice.c) would set
 * C1's talking bit 0x100 in a speaking player's engine-state flags. */
int VoiceIsTalking(int clientNum) { assert(clientNum>=0 && clientNum<MAX_GENTITIES); return 0; }
/* No zone effect sources are mounted here. Byte-effect slot reclaim asks
 * whether an inherited effect exists; this fixture publishes no script
 * effects, so the question is never asked. */
int LoadMountedEffectSource(const char *name,const effect_source_t **out)
{ (void)name; (void)out; assert(0 && "no mounted effects in fixture"); return 0; }
/* This fixture has no map shadow state and no native weapon registry. Weapon
 * publication itself is covered elsewhere; an attempt to refresh weapon
 * definitions here is a fixture setup error. */
int RefreshMapSunSampleSize(void) { return 1; }
void ResetMapSunSampleSize(void) { }
int RefreshServerWeaponMappings(void) { assert(0 && "weapon registry absent"); return 0; }
const char *ServerWeaponRegistryError(void) { return "weapon registry absent"; }
/* No world-gun children exist in this fixture (sv_assets.c publishes
 * them at clientState +0x12c/+0x130). */
unsigned PlayerWorldWeaponEntity(unsigned client,unsigned hand)
{ (void)client; (void)hand; return 0; }
int RemoteVehicleType(unsigned client)
{ (void)client; assert(0 && "no live player or remote vehicle in fixture"); return -1; }
/* No C1 actor capture is mounted in this asset-name fixture. The frame's
 * explicit actor input is tested below, and the gameplay frame sees the
 * captured empty actor set for this synthetic snapshot. */
const char *actor_delivery_failure;
void ActorDeliveryClear(unsigned client) { assert(client<MAX_CLIENTS); }
int ActorDeliveryPrepare(unsigned client,unsigned sequence,unsigned acknowledged,
    const entity_state_t *entities,unsigned count,animcmd_t *out,
    unsigned capacity,unsigned *written)
{
    (void)sequence;(void)acknowledged;(void)out;
    assert(client<MAX_CLIENTS && count<=1 && (!count ||
        (entities && entities[0].type!=ET_ACTOR)) &&
        capacity>=ACTOR_ANIM_SNAPSHOT_LIMIT && written);
    *written=0; return 1;
}
int CopyClientSnapshotActors(unsigned client,uint32_t sequence,
    actor_state_t *out,size_t capacity,unsigned *count)
{
    (void)out; assert(client<2 && sequence==200 && capacity>=32 && count);
    *count=0; return 1;
}
/* The target test has no entity lifecycle clients (vehicles, missiles, glass).
 * Keep their reset boundary narrowed to the real snapshot epoch state. */
void ResetEntityEventGeneration(unsigned number)
{ ResetEntityEventEpoch(number); }
void ResetEntityEventGenerations(void)
{ ResetEntityEventEpochs(); EntityStateExtClearAll(); }
void SV_SendServerCommandNoLoss(client_t *client, const char *format, ...)
{
    va_list args;
    assert(client == &svs.clients[0]);
    va_start(args, format); vsnprintf(last_command, sizeof(last_command), format, args); va_end(args);
    ++commands;
}
static int frame_entity_seed(void *context,const entityState_t *source,snapshot_entity_bindings_t *out)
{
    unsigned i; (void)context;
    out->state.type=source->eType;
    for(i=0;i<ENTITY_FIELDS;++i)
        if(EntityField(out->state.type,i)->offset==0x98) out->state.field[i]=17;
    return ResolveEngineEntityEventParameters(source,out->event_parameters);
}
static int frame_client_seed(void *context,const clientState_t *source,client_state_t *out)
{
    unsigned i; (void)source;
    if(context) return 0;
    for(i=0;i<CLIENT_FIELDS;++i)
        if(ClientField(i)->offset==0xc0) out->field[i]=0x11223344;
    return 1;
}
static void frame_assembly_test(void)
{
    clientSnapshot_t *source=&svs.clients[0].frames[0];
    entityState_t entities[4]={{0}}; clientState_t clients[4]={{0}};
    player_fields_t seed={0}; player_array_updates_t arrays={0};
    engine_player_bindings_t player={0}; native_frame_bindings_t bindings={0},invalid;
    uint32_t followed_sequence=0;
    native_frame_t *frame=NULL,*before; const snapshot_state_t *state;
    actor_state_t actor={0}; animcmd_t command={0}; size_t count;
    unsigned event;
    ps_ext_t *extension;
    ps_ext_t abilities={0};
    memset(sv.configstrings,0,sizeof(sv.configstrings));
    ResetWeaponMap();
    memset(source,0,sizeof(*source));
    source->ps.clientNum=0;
    source->ps.moveSpeedScaleMultiplier=1; source->ps.groundEntityNum=ENTITYNUM_WORLD;
    source->ps.throwBackGrenadeOwner=ENTITYNUM_NONE;
    extension=PlayerStateExt(&source->ps);
    assert(extension); PlayerStateExtClear(&source->ps);
    extension->links_known=1;
    extension->links.linkWeaponEntNum=ENTITYNUM_NONE;
    extension->links.cameraEntNum=ENTITYNUM_NONE;
    extension->links.controlsEntNum=ENTITYNUM_NONE;
    for(event=0;event<3;++event) {
        ++source->ps.eventSequence; PlayerStateEventAdded(&source->ps);
    }
    source->first_entity=source->first_client=3; source->num_entities=source->num_clients=1;
    entities[3].number=2;
    clients[3].attachedVehEntNum=ENTITYNUM_NONE;
    svsHeader.snapshotEntities=entities; svsHeader.numSnapshotEntities=4; svsHeader.nextSnapshotEntities=4;
    svsHeader.snapshotClients=clients; svsHeader.numSnapshotClients=4; svsHeader.nextSnapshotClients=4;
    svsHeader.clients=svs.clients;
    svs.numSnapshotClients=4; svs.snapshotClients[3]=clients[3];
    abilities.abilities_known=1; abilities.abilities.masks[0]=0x11223344;
    assert(CaptureClientAbilities(3,&abilities));
    ResetEntityEventGenerations();
    EntityStateExtClear(&entities[3]);
    for(event=0;event<254;++event) {
        entities[3].eventSequence=(entities[3].eventSequence+1)&255;
        EntityStateEventAdded(&entities[3]);
    }
    actor.data[4]=55; command.arg2=7;
    bindings.player_seed=&seed; bindings.array_seed=&arrays; bindings.player=&player;
    bindings.entity_resolve=frame_entity_seed; bindings.client_resolve=frame_client_seed;
    bindings.actors=&actor; bindings.actor_count=1; bindings.commands=&command; bindings.command_count=1;
    assert(CaptureClientSnapshotHud(0,200,&source->ps.hud));
    assert(BuildNativeFrame(&frame,0,200,source,&bindings));
    /* The entity counter published is that entity state's own epoch. */
    assert(EntityStateEventEpoch(&entities[2])==0);
    state=NativeFrameState(frame);
    assert(state && state->sequence==200 && state->entity_count==1 && state->client_count==1 && state->actor_count==1);
    assert(state->entities[0].number==2 && entity_field(&state->entities[0],0x98)==17);
    assert(field(&state->clients[0],0xc0)==0x11223344 && field(&state->clients[0],0xbc)==2047);
    assert(NativeFrameUpdates(frame));
    actor.data[4]=77; command.arg2=9;
    assert(state->actors[0].data[4]==55 && NativeFrameCommands(frame,&count)[0].arg2==7 && count==1);
    before=frame;
    ++source->ps.eventSequence; PlayerStateEventAdded(&source->ps);
    entities[3].eventSequence=255; EntityStateEventAdded(&entities[3]);
    clients[3].modelindex=MAX_MODELS; /* Fail after entity generation advanced in staging. */
    assert(!BuildNativeFrame(&frame,0,200,source,&bindings));
    assert(frame==before);
    clients[3].modelindex=0;
    invalid=bindings; invalid.client_context=&invalid;
    assert(!BuildNativeFrame(&frame,0,200,source,&invalid));
    invalid=bindings; invalid.client_resolve=NULL;
    assert(!BuildNativeFrame(&frame,0,200,source,&invalid));
    assert(!BuildNativeFrame(&frame,0,201,source,&bindings));
    assert(frame==before);
    assert(state->actors[0].data[4]==55 && entity_field(&state->entities[0],0xb0)==254);
    assert(BuildNativeFrame(&frame,0,200,source,&bindings));

    state=NativeFrameState(frame); assert(state->actors[0].data[4]==77);
    assert(SetWeaponLock(PlayerStateWeaponLock(&source->ps),17,NULL,1,NULL,0));
    assert(SetWeaponLock(ClientWeaponLock(0),18,NULL,1,NULL,1));
    assert(SetWeaponLock(ClientWeaponLock(1),19,NULL,1,NULL,1));
    assert(BuildGameplayFrame(0,200,source));
    {
        const native_frame_t *live=ClientNativeFrame(0,200);
        const snapshot_state_t *built=NativeFrameState(live);
        assert(built && built->entity_count==1 && built->client_count==1 && !built->actor_count);
        for(unsigned i=0;i<PLAYER_FIELDS;++i) {
            if(OL_PlayerField(i)->offset==0x48c)assert(built->player->field[i]==11);
            if(OL_PlayerField(i)->offset==0x490)assert(built->player->field[i]==17);
        }
        /* Recipient 1 follows source player 0: their own lock must not leak. */
        assert(CaptureClientSnapshotHud(1,200,&source->ps.hud));
        assert(BuildGameplayFrame(1,200,source));
        { const snapshot_state_t *followed=NativeFrameState(ClientNativeFrame(1,200));
          assert(followed);
          for(unsigned i=0;i<PLAYER_FIELDS;++i) {
              if(OL_PlayerField(i)->offset==0x490)assert(followed->player->field[i]==17);
              if(OL_PlayerField(i)->offset==0x0f0)followed_sequence=followed->player->field[i];
          }
        }
        assert(field(&built->clients[0],0xc0)==0x11223344 && field(&built->clients[0],0xbc)==2047);
        /* The published counter is the source state's own, so both recipients
         * of that state publish the same one instead of each accumulating. */
        for(unsigned i=0;i<PLAYER_FIELDS;++i)
            if(OL_PlayerField(i)->offset==0x0f0) {
                assert(built->player->field[i]==(uint32_t)source->ps.eventSequence);
                assert(followed_sequence==built->player->field[i]);
            }

        {
            unsigned char bytes[8192]; ol_msg_t message;
            delta_context_t context={0};
            MsgWriteInit(&message,bytes,sizeof(bytes));
            assert(OL_WriteSnapshot(&message,built,NULL,NativeFrameUpdates(live),NULL,0,0,1,&context));
            assert(!message.failed && message.size>0);
        }
        {
            entityState_t original=entities[3];
            unsigned char bytes[8192]; ol_msg_t message; delta_context_t context={0};
            entities[3].eType=ET_SCRIPTMOVER; entities[3].index=1; entities[3].solid=SOLID_BMODEL;
            g_entities[2].r.bmodel=1;
            assert(BuildGameplayFrame(0,200,source));
            live=ClientNativeFrame(0,200); built=NativeFrameState(live);
            assert(entity_field(&built->entities[0],0x98)==1 && entity_field(&built->entities[0],0xa8)==0xffffff);
            MsgWriteInit(&message,bytes,sizeof(bytes));
            assert(OL_WriteSnapshot(&message,built,NULL,NativeFrameUpdates(live),NULL,0,0,1,&context));
            /* A server-only trigger handle (a trigger a script played a
             * sound on) is published, with no brush index and no solid. */
            entities[3].index=3;
            assert(BuildGameplayFrame(0,200,source));
            built=NativeFrameState(ClientNativeFrame(0,200));
            assert(built->entity_count==1 && entity_field(&built->entities[0],0x98)==0 &&
                   entity_field(&built->entities[0],0xa8)==0);
            /* Past the client's cmodels and not a trigger: never published. */
            entities[3].index=4;
            assert(BuildGameplayFrame(0,200,source));
            assert(NativeFrameState(ClientNativeFrame(0,200))->entity_count==0);
            entities[3]=original; g_entities[2].r.bmodel=0;
        }
        assert(MapWeapon(25,1));
        entities[3].eType=ET_ITEM; entities[3].index=1;
        assert(BuildGameplayFrame(0,200,source));
        built=NativeFrameState(ClientNativeFrame(0,200));
        assert(built->entities[0].type==ET_ITEM && entity_field(&built->entities[0],0x98)==25);
        {
            unsigned char bytes[8192]; ol_msg_t message; delta_context_t context={0};
            entity_state_t item=built->entities[0],saved=item;
            entityState_t invalid_item=entities[3];
            MsgWriteInit(&message,bytes,sizeof(bytes));
            assert(OL_WriteSnapshot(&message,built,NULL,
                NativeFrameUpdates(ClientNativeFrame(0,200)),NULL,0,0,1,&context));
            invalid_item.index=0;
            assert(!OverlayEngineItem(&item,&invalid_item) && !memcmp(&item,&saved,sizeof(item)));
            invalid_item.index=-1;
            assert(!OverlayEngineItem(&item,&invalid_item) && !memcmp(&item,&saved,sizeof(item)));
            invalid_item.index=2;
            assert(!OverlayEngineItem(&item,&invalid_item) && !memcmp(&item,&saved,sizeof(item)));
            assert(MapWeapon(700,2));
            assert(!OverlayEngineItem(&item,&invalid_item) && !memcmp(&item,&saved,sizeof(item)));
        }
        entities[3].index=129; /* Native variant one is not yet translated. */
        assert(BuildGameplayFrame(0,200,source));
        assert(NativeFrameState(ClientNativeFrame(0,200))->entity_count==0);
        entities[3].eType=ET_VEHICLE; entities[3].index=0;
        assert(BuildGameplayFrame(0,200,source));
        assert(NativeFrameState(ClientNativeFrame(0,200))->entity_count==0);

        entities[3].eType=ET_GENERAL;
        source->ps.weapon=2; /* no native definition or wire slot */
        assert(!BuildGameplayFrame(0,200,source));
        source->ps.weapon=0;
        assert(BuildGameplayFrame(0,200,source));
        {
            /* A custom explosion (event79: concussion, EMP, smoke) carries its
             * FX start time at +58, as C1 G_ExplodeMissile 52ef1e writes it;
             * left zero the client plays the effect as long finished. Grenade
             * and flash explosions keep +58 zero, like C1's other arms. */
            entityState_t original=entities[3]; entity_state_t wire;
            entities[3].weapon=1; entities[3].lerp.u.anonymous.data[0]=4321;
            /* Native event words (vec4_t declaration, integer contents):
             * EV_CUSTOM_EXPLODE 49, _NOMARKS 50, EV_GRENADE_EXPLODE 45. */
            uint32_t word=49;
            memcpy(&entities[3].events[0],&word,4);
            assert(ProbeEntityWire(&entities[3],0,&wire)==1);
            assert(entity_field(&wire,0x58)==4321 && entity_field(&wire,0xd4)==25);
            word=50; memcpy(&entities[3].events[0],&word,4);
            assert(ProbeEntityWire(&entities[3],0,&wire)==1 && entity_field(&wire,0x58)==4321);
            word=45; memcpy(&entities[3].events[0],&word,4);
            assert(ProbeEntityWire(&entities[3],0,&wire)==1);
            assert(entity_field(&wire,0x58)==0 && entity_field(&wire,0xd4)==25);
            entities[3]=original;
        }
        ResetEntityEventGeneration(0); ResetEntityEventGeneration(2);
        assert(!EntityStateEventEpoch(&entities[2]));
        ClearClientNativeFrame(0);
    }
    FreeNativeFrame(frame); ClearClientSnapshotHud(0);
    svsHeader.snapshotEntities=NULL; svsHeader.snapshotClients=NULL;
    puts("PASS: owned native frame assembly, captured HUD, explicit client seeds and cross-section clock rollback");
}
/* Bind the synthetic snapshot rings to the real extension implementation so
 * the producer hooks above and the frame builder read the same owned state. */
const void *EntityStateExtLiveBase(unsigned *count){if(count)*count=0;return NULL;}
const void *EntityStateExtSnapshotBase(unsigned *count)
{ if(count)*count=(unsigned)svsHeader.numSnapshotEntities;return svsHeader.snapshotEntities; }
const void *EntityStateExtArchiveBase(unsigned *count){if(count)*count=0;return NULL;}
const void *PlayerStateExtLiveBase(unsigned *count){if(count)*count=0;return NULL;}
const void *PlayerStateExtSnapshotBase(unsigned *count)
{ if(count)*count=MAX_CLIENTS;return svs.clients; }
const void *PlayerStateExtArchiveBase(unsigned *count){if(count)*count=0;return NULL;}
void Com_PrintWarning(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
/* Turret eFlags: only the fired frame (native 0x40 -> C1 0x200) and the
 * setturretminimapvisible bit 0x400000, which C1's CG_Turret needs before it
 * puts a sentry on the compass, reach the wire. */
static void turret_entities(void)
{
    entityState_t source={0}; entity_state_t wire={0};
    source.eType=ET_MG42; source.otherEntityNum=3;
    wire.type=ET_MG42;
    source.lerp.eFlags=0x400000;
    assert(OverlayEngineTurret(&wire,&source,0));
    assert(entity_field(&wire,0x08)==0x400000 && entity_field(&wire,0x80)==3);
    source.lerp.eFlags=0x40|0x400000|0x20000|0x800;
    assert(OverlayEngineTurret(&wire,&source,0) && entity_field(&wire,0x08)==0x400200);
    source.lerp.eFlags=0x40;
    assert(OverlayEngineTurret(&wire,&source,0) && entity_field(&wire,0x08)==0x200);
    source.lerp.eFlags=0;
    assert(OverlayEngineTurret(&wire,&source,0) && entity_field(&wire,0x08)==0);
}
int main(void)
{
    brush_entities();
    turret_entities();
    client_t *clients = svs.clients;
    client_state_t wire = {0}, saved;
    clientState_t engine = {0};
    maximum.integer = 2; sv.emptyConfigString = 1;
    clients[0].state = CS_PRIMED; clients[0].gamestateSent = 1;
    clients[1].state = CS_CONNECTED;
    sv.configstrings[CS_MODELS+7] = 2; sv.configstrings[CS_MODELS+9] = 3;
    sv.configstrings[CS_TAGS+5] = 4;
    assert(PublishAssetConfigstring(CS_MODELS+7)==1);
    assert(commands==2 && !strcmp(last_command,"d 1317 attachment"));
    assert(PublishAssetConfigstring(CS_TAGS+5)==1);
    assert(commands == 3 && !strcmp(last_command, "d 5300 tag_weapon"));
    assert(ServerModelToWire(7) == 1 && ServerModelToWire(9) == 2);
    assert(ServerTagToWire(5) == 1 && ServerTagToWire(0) == 0);
    assert(ServerModelToWire(1) == -1 && ServerModelToWire(MAX_MODELS) == -1);
    assert(RefreshServerAssetNames() && commands == 3);
    assert(PublishAssetConfigstring(CS_MODELS)==1);
    assert(PublishAssetConfigstring(CS_TAGS)==1);
    assert(PublishAssetConfigstring(CS_MODELS-1)==1); /* selector 829 */
    assert(PublishAssetConfigstring(CS_MODELS+MAX_MODELS)==1); /* sound sentinel */
    assert(PublishAssetConfigstring(CS_TAGS-1)==1); /* head icon */
    assert(PublishAssetConfigstring(CS_TAGS+MAX_TAGS)==1); /* native item mask 0x90a */
    assert(commands==3);
    engine.modelindex = 7; engine.attachModelIndex[0] = 9; engine.attachTagIndex[0] = 5;
    engine.attachedVehEntNum=ENTITYNUM_NONE;
    assert(OverlayServerClient(&wire, &engine));
    assert(field(&wire, 0x14) == 1 && field(&wire, 0x20) == 2 && field(&wire, 0x60) == 1);
    model_entity_test();
    {
        clientState_t ring[4] = {{0}};
        client_state_t result[2], unchanged[2];
        memset(result, 0xa5, sizeof(result)); memcpy(unchanged, result, sizeof(result));
        ring[3] = engine; ring[0] = engine; ring[0].clientIndex = 1;
        ring[0].attachedVehEntNum=17;
        svsHeader.snapshotClients = ring; svsHeader.numSnapshotClients = 4;
        svsHeader.nextSnapshotClients = 5;
        svs.numSnapshotClients=4;
        svs.snapshotClients[3]=ring[3]; svs.snapshotClients[0]=ring[0];
        assert(CaptureClientAbilities(3,NULL));
        assert(CaptureClientAbilities(4,NULL));
        assert(!CollectServerClients(3, 2, result, 1) && !memcmp(result, unchanged, sizeof(result)));
        assert(CollectServerClients(3, 2, result, 2));
        assert(result[0].number == 0 && result[1].number == 1);
        assert(field(&result[0],0xbc)==2047 && field(&result[1],0xbc)==17);
        assert(field(&result[1], 0x14) == 1 && field(&result[1], 0x20) == 2);
        memcpy(unchanged, result, sizeof(result));
        ring[0].attachedVehEntNum=ENTITYNUM_WORLD;
        assert(!CollectServerClients(3,2,result,2) && !memcmp(result,unchanged,sizeof(result)));
        ring[0].attachedVehEntNum=17;
        ring[0].clientIndex = 0;
        assert(!CollectServerClients(3, 2, result, 2));
        ring[0].clientIndex = 32;
        assert(!CollectServerClients(3, 2, result, 2));
        ring[0].clientIndex = 1; ring[0].modelindex = MAX_MODELS;
        assert(!CollectServerClients(3, 2, result, 2));
        assert(!memcmp(result, unchanged, sizeof(result)));
        svsHeader.nextSnapshotClients = 8;
        assert(!CollectServerClients(3, 2, result, 2)); /* overwritten */
        assert(!CollectServerClients(8, 1, result, 2)); /* future */
        assert(CollectServerClients(8, 0, NULL, 0));
        svsHeader.snapshotClients = NULL;
        assert(!CollectServerClients(7, 1, result, 2));
    }
    saved = wire; engine.attachModelIndex[1] = MAX_MODELS;
    assert(!OverlayServerClient(&wire, &engine) && !memcmp(&wire, &saved, sizeof(wire)));
    strings[2] = "replacement"; /* recycled handle with new text */
    assert(RefreshServerAssetNames() && commands == 4);
    assert(ServerModelToWire(7) == 3);
    {
        gentity_t model={0}; entity_state_t entity={0};
        model.s.eType=ET_SCRIPTMOVER; model.s.index=7; entity.type=ET_SCRIPTMOVER;
        assert(OverlayServerModelEntity(&entity,&model) && entity_field(&entity,0x5c)==3);
    }
    engine.attachModelIndex[1] = 0;
    assert(OverlayServerClient(&wire, &engine) && field(&wire, 0x14) == 3);
    assert(!strcmp(GetConfigstring(ServerConfigstrings(), OL_CS_MODELS+1), "body"));
    strings[2] = "invalid;command";
    assert(PublishAssetConfigstring(CS_MODELS+7)==-1 && commands == 4);
    sv.configstrings[CS_MODELS+7] = 1;
    assert(PublishAssetConfigstring(CS_MODELS+7)==1 && ServerModelToWire(7) == -1);
    assert(commands==4); /* Removal unbinds the native ID; retained wire names stay stable. */
    strings[5]="sound_alias"; sv.configstrings[CS_SOUNDS+255]=5;
    assert(PublishAssetConfigstring(CS_SOUNDS+255)==1);
    assert(commands==5 && !strcmp(last_command,"d 3650 sound_alias"));
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),3650),"sound_alias"));
    assert(!GetConfigstring(ServerConfigstrings(),CS_SOUNDS+255));
    assert(RefreshServerAssetNames() && commands==5);
    strings[5]="replacement_sound";
    assert(PublishAssetConfigstring(CS_SOUNDS+255)==1 && commands==6);
    assert(!strcmp(last_command,"d 3650 replacement_sound"));
    strings[5]="invalid;alias";
    assert(PublishAssetConfigstring(CS_SOUNDS+255)==-1 && commands==6);
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),3650),"replacement_sound"));
    sv.configstrings[CS_SOUNDS+255]=1;
    assert(PublishAssetConfigstring(CS_SOUNDS+255)==1 && commands==7);
    assert(!strcmp(last_command,"d 3650 ") && !*GetConfigstring(ServerConfigstrings(),3650));
    assert(PublishAssetConfigstring(CS_SOUNDS+MAX_SOUNDS)==1 && commands==7); /* FX bank */
    strings[6]="90"; strings[7]="\"compass_map_native\" -100 200 300 -400";
    sv.configstrings[822]=6; sv.configstrings[823]=7;
    assert(PublishAssetConfigstring(822)==1 && commands==9);
    assert(!strcmp(last_command,"d 1303 \"compass_map_native\" -100 200 300 -400"));
    assert(!strcmp(GetConfigstring(ServerConfigstrings(),1302),"90"));
    assert(!GetConfigstring(ServerConfigstrings(),822));
    assert(RefreshServerMapConfigstrings() && commands==9);
    assert(PublishAssetConfigstring(824)==0); /* Native vision, not CoDOL second minimap. */
    strings[7]="invalid\ncommand";
    assert(PublishAssetConfigstring(823)==-1 && commands==9);
    sv.configstrings[823]=1;
    assert(PublishAssetConfigstring(823)==1 && commands==10);
    assert(!strcmp(last_command,"d 1303 ") && !*GetConfigstring(ServerConfigstrings(),1303));
    {
        int prior=commands; configstrings_t *bank=ServerConfigstrings();
        strings[5]="600000"; sv.configstrings[11]=5;
        assert(PublishAssetConfigstring(11)==1 && commands==++prior);
        assert(!strcmp(last_command,"d 15 600000") && !strcmp(GetConfigstring(bank,15),"600000"));
        assert(!GetConfigstring(bank,11));
        strings[7]="-1024 2048 128"; sv.configstrings[12]=7;
        assert(PublishAssetConfigstring(12)==1 && commands==++prior);
        assert(!strcmp(last_command,"d 16 -1024 2048 128") && !strcmp(GetConfigstring(bank,16),strings[7]));
        assert(!GetConfigstring(bank,12));
        assert(RefreshServerMapConfigstrings() && commands==prior);
        strings[5]="700000\ncommand";
        assert(PublishAssetConfigstring(11)==-1 && commands==prior);
        assert(!strcmp(GetConfigstring(bank,15),"600000"));
        strings[5]="700000"; clients[0].gamestateSent=0;
        assert(RefreshServerMapConfigstrings() && commands==prior);
        assert(!strcmp(GetConfigstring(bank,15),"700000"));
        clients[0].gamestateSent=1;
        sv.configstrings[11]=sv.configstrings[12]=1;
        assert(PublishAssetConfigstring(11)==1 && commands==prior+2);
        assert(!*GetConfigstring(bank,15) && !*GetConfigstring(bank,16));
        assert(!strcmp(last_command,"d 16 "));
    }
    {
        const char *constants[]={"4276","white","681","MP_VICTORY",
            "24","shared_setting","224","10","25","private_setting","225","unchanged"};
        static gclient_t hudClient; hudElemState_t *hud=&hudClient.ps.hud; hud_state_t result={0},before;
        int prior=commands;
        strings[5]="white"; strings[6]="MP_VICTORY";
        sv.configstrings[CS_MATERIALS+7]=5; sv.configstrings[CS_LOCALIZEDSTRINGS+9]=6;
        assert(PublishAssetConfigstring(CS_MATERIALS+7)==-1 && commands==prior);
        replicated.flags=CVAR_SYSTEMINFO; replicated.string="10"; private_var.string="secret";
        assert(SetServerInheritedConfigstrings("mp",6,constants));
        assert(RefreshServerAssetNames() && commands==prior);
        assert(ServerMaterialToWire(7)==1 && ServerLocalizedToWire(9)==1);
        assert(!GetConfigstring(ServerConfigstrings(),4276));
        hud->current[0].type=HE_TYPE_TEXT; hud->current[0].text=9; hud->current[0].label=9;
        hud->current[0].materialIndex=7; hud->current[0].flags=5;
        CopyClientHudFontScale(&hudClient,0,&hud->current[0],&g_hudelems[0]);
        assert(CollectServerHud(&result,hud) && result.count[1]==1 && commands==prior);
        assert(CaptureClientSnapshotHud(0,200,hud));
        assert(result.elements[1][0].field[22]==1 && result.elements[1][0].field[33]==1 && result.elements[1][0].field[4]==1 && result.elements[1][0].field[14]==5);
        strings[5]="custom_material";
        assert(PublishAssetConfigstring(CS_MATERIALS+7)==1 && commands==++prior);
        assert(!strcmp(last_command,"d 4277 custom_material") && ServerMaterialToWire(7)==2);
        strings[6]="Hello player";
        assert(PublishAssetConfigstring(CS_LOCALIZEDSTRINGS+9)==1 && commands==++prior);
        assert(!strcmp(last_command,"d 682 Hello player") && ServerLocalizedToWire(9)==2);
        strings[7]="preallocated"; sv.configstrings[CS_MATERIALS+8]=7;
        assert(OL_SetConfigstring(ServerConfigstrings(),4278,"preallocated"));
        assert(PublishAssetConfigstring(CS_MATERIALS+8)==1 && commands==++prior);
        assert(!strcmp(last_command,"d 4278 preallocated") && ServerMaterialToWire(8)==3);
        assert(CollectServerHud(&result,hud) && result.elements[1][0].field[22]==2 && result.elements[1][0].field[4]==2 && commands==prior);
        {
            hud_state_t captured;
            assert(CopyClientSnapshotHud(0,200,&captured));
            assert(captured.elements[1][0].field[22]==1 && captured.elements[1][0].field[4]==1);
            ClearClientSnapshotHud(0);
        }
        before=result; hud->current[0].soundID=1;
        assert(!CollectServerHud(&result,hud) && !memcmp(&result,&before,sizeof(result)));
        hud->current[0].soundID=0; hud->current[0].flags=0x80; /* outside seven-bit C1 mask */
        assert(!CollectServerHud(&result,hud) && !memcmp(&result,&before,sizeof(result)));
        hud->current[0].flags=5; strings[6]="invalid\ncommand";
        assert(PublishAssetConfigstring(CS_LOCALIZEDSTRINGS+9)==-1 && commands==prior);
        sv.configstrings[CS_LOCALIZEDSTRINGS+9]=1;
        assert(PublishAssetConfigstring(CS_LOCALIZEDSTRINGS+9)==1 && ServerLocalizedToWire(9)==-1 && commands==prior);
        assert(!CollectServerHud(&result,hud) && !memcmp(&result,&before,sizeof(result)));
        assert(!strcmp(GetConfigstring(ServerConfigstrings(),682),"Hello player"));
    }
    {
        int prior=commands; configstrings_t *bank=ServerConfigstrings();
        assert(RefreshServerDvars() && commands==prior && !GetConfigstring(bank,224));
        assert(!GetConfigstring(bank,24) && !GetConfigstring(bank,225));
        replicated.string="20";
        assert(RefreshServerDvars() && commands==++prior && !strcmp(last_command,"d 224 20"));
        assert(!strcmp(GetConfigstring(bank,224),"20") && !GetConfigstring(bank,225));
        assert(RefreshServerDvars() && commands==prior);
        replicated.string="invalid\ncommand";
        assert(!RefreshServerDvars() && commands==prior && !strcmp(GetConfigstring(bank,224),"20"));
        replicated.string="";
        assert(RefreshServerDvars() && commands==++prior && !strcmp(last_command,"d 224 "));
        replicated.string="30";
        assert(OL_SetConfigstring(bank,24,""));
        assert(RefreshServerDvars() && commands==prior); /* Explicit name terminator wins over constants. */
        assert(OL_SetConfigstring(bank,24,"unknown_setting"));
        assert(RefreshServerDvars() && commands==prior); /* Unknown and private variables stay untouched. */
        assert(OL_SetConfigstring(bank,24,"shared_setting"));
        clients[0].gamestateSent=0;
        assert(RefreshServerDvars() && commands==prior && !strcmp(GetConfigstring(bank,224),"30"));
        clients[0].gamestateSent=1;
    }
    {
        int prior=commands;
        configstrings_t *bank=ServerConfigstrings();
        assert(OL_SetConfigstring(bank,CS_WEAPONS+4,"m4_mp"));
        assert(OL_SetConfigstring(bank,CS_WEAPONS+5,"gl_m4_mp"));
        clients[0].gamestateSent=0;
        BroadcastWeaponConfigstrings(4,2); assert(commands==prior);
        clients[0].gamestateSent=1;
        BroadcastWeaponConfigstrings(4,1);
        assert(commands==++prior && !strcmp(last_command,"d 5366 m4_mp"));
        BroadcastWeaponConfigstrings(5,1);
        assert(commands==++prior && !strcmp(last_command,"d 5367 gl_m4_mp"));
        BroadcastWeaponConfigstrings(WEAPON_SLOTS-1,2);
        assert(commands==prior);
    }
    {
        int prior = commands;
        clients[0].gamestateSent = 0;
        assert(PublishServerId(257) && commands == prior);
        assert(!strcmp(GetConfigstring(ServerConfigstrings(), 3), "257"));
        clients[0].gamestateSent = 1;
        assert(PublishServerId(258));
        assert(commands == ++prior && !strcmp(last_command, "d 3 258"));
        assert(PublishServerId(258) && commands == prior);
        assert(PublishServerId(INT32_MIN));
        assert(commands == ++prior && !strcmp(last_command, "d 3 -2147483648"));
    }
    ResetServerAssetNames(); ResetServerConfigstrings();
    assert(ServerMaterialToWire(7)==-1 && ServerLocalizedToWire(9)==-1);
    assert(ServerModelToWire(9) == -1 && ServerTagToWire(5) == -1);
    puts("PASS: production model/tag publication, handle recycling and client-state mapping");
    frame_assembly_test();
    return 0;
}

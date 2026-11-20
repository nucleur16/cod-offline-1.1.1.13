/* Native gameplay inputs for the CoDOL frame builder. */
#ifdef OL_SERVER
#include "weapon_slots.h"
#include "frame_builder.h"
#include "qcommon.h"
#include <stdio.h>
#include "script_events.h"
#include "sv_weapons.h"
#include "ol_events.h"
#include "usercmd.h"
#include "bg.h"
#include "weapon_lock.h"
#include "sv_state.h"
#include "weapon_state.h"
#include "actor_state.h"
#include "loop_sound.h"
#include "script_hud_notifications.h"
#include "script_client_material.h"
#include "action_slots.h"
#include "field_orders.h"
#include "equipment_support.h"
#include "entitystate_ext.h"
#include "playerstate_ext.h"
#include "radar_state.h"
#include "usehint_wire.h"
#include "script_objectives.h"
#include "cm_public.h"
#include "collision_native.h"
#include <string.h>

static int client_seed(void *context,const clientState_t *source,client_state_t *out)
{
    (void)context;
    if(!source || !out || source->clientIndex<0 || source->clientIndex>=32) return 0;
    /* Progression metadata still needs producers. Team, model, attachments,
     * vehicle and sequence-matched captured abilities are overlaid by the
     * collector; never read live player state from this resolver. */
    memset(out,0,sizeof(*out)); return 1;
}
/* The one place event parameters are resolved for this server: the generic
 * per-event mapping, plus the compound slots some events carry. Both event
 * queues use it - the entity queue below and the scripted player queue, whose
 * events reach the same client tables and include EV_PLAY_FX_ON_TAG since the
 * producer moved (script_events.h). Sharing it is the point: a second
 * copy would be a second chance to publish an unmapped slot. */
int ServerResolveEventParameters(const uint32_t *events,const uint32_t *parameters,
                                       uint32_t *out,unsigned count)
{
    uint32_t result[4]; unsigned i;
    if(!events || !parameters || !out || count>4) return 0;
    for(i=0;i<count;++i) {
        if(events[i]==EV_PLAY_FX_ON_TAG || events[i]==EV_STOP_FX_ON_TAG ||
           events[i]==EV_PLAY_FX_ON_TAG_FOR_CLIENTS) {
            unsigned tagged=parameters[i]; uint32_t audience;
            int slot;
            if(events[i]==EV_PLAY_FX_ON_TAG_FOR_CLIENTS &&
               !ReadMaskedTaggedEffect(parameters[i],&tagged,&audience)) return 0;
            slot=ServerTaggedEffectToWire(tagged);
            /* Native Scr_PlayFXOnTag queues event56 with the CS6a2 slot.
             * C1 543f4a..543f4f queues event89 with its CS3971 slot through
             * G_AddEvent572d30. The slot references the translated compound
             * effect/tag string; it is neither an effect ID nor a tag ID. */
            if(slot<=0) return 0;
            result[i]=(uint32_t)slot;
        } else if(EventParameterToWire(events[i],parameters[i],result+i)!=1) return 0;
    }
    memcpy(out,result,count*sizeof(*out)); return 1;
}
static int event_suppression(const uint32_t events[4],const uint32_t parameters[4],
                             unsigned recipient,unsigned *out)
{
    unsigned i,suppressed=0;
    if(recipient>=32 || !out) return 0;
    for(i=0;i<4;++i) if(events[i]==EV_PLAY_FX_ON_TAG_FOR_CLIENTS) {
        unsigned tagged; uint32_t audience;
        if(!ReadMaskedTaggedEffect(parameters[i],&tagged,&audience)) return 0;
        if(!(audience & (1u<<recipient))) suppressed|=1u<<i;
    }
    *out=suppressed; return 1;
}
/* Why entity_seed last refused, for the frame-rejection trace. */
char entity_seed_failure[160];
static int entity_event_parameters(const entityState_t *source,uint32_t out[4],
                                    unsigned recipient,unsigned *suppressed)
{
    uint32_t events[4],parameters[4]; unsigned i,unmapped=0,audience;
    /* Retained declarations are vec4_t, but G_AddEvent writes integer words. */
    memcpy(events,source->events,sizeof(events));
    memcpy(parameters,source->eventParms,sizeof(parameters));
    for(i=0;i<4;++i) {
        uint32_t one_event=events[i],one_parm=parameters[i],resolved;
        if(EventToWire(one_event)>=0 &&
           ServerResolveEventParameters(&one_event,&one_parm,&resolved,1) &&
           resolved<=2047) {
            out[i]=resolved;
            continue;
        }
        /* Adaptation: an event slot the mapper cannot translate is published
         * as no event instead of refusing every frame for as long as the
         * entity lives (a grenade in flight rejected the whole snapshot for
         * every client until it exploded). Each unmapped event is reported
         * once in developer mode so the mapping can be measured and added. */
        static uint32_t reported[32]; static unsigned reported_count;
        {
            unsigned k; int seen=0;
            for(k=0;k<reported_count;++k) if(reported[k]==one_event) { seen=1; break; }
            if(!seen && reported_count<32) {
                reported[reported_count++]=one_event;
                Com_DPrintf(CON_CHANNEL_SERVER,"CoDOL entity %d (type %d): unmapped event %u parameter %u dropped from the wire%s",
                    source->number,source->eType,one_event,one_parm,"\n");
            }
            events[i]=EV_NONE; parameters[i]=0; out[i]=0;
            unmapped|=1u<<i;
        }
    }
    if(!event_suppression(events,parameters,recipient,&audience)) {
        snprintf(entity_seed_failure,sizeof(entity_seed_failure),"event audience mask");
        return 0;
    }
    *suppressed=unmapped|audience;
    return 1;
}
static int explosion_event(unsigned event)
{
    switch(event) {
    case EV_GRENADE_EXPLODE:
    case EV_ROCKET_EXPLODE:
    case EV_ROCKET_EXPLODE_NOMARKS:
    case EV_FLASHBANG_EXPLODE:
    case EV_CUSTOM_EXPLODE:
    case EV_CUSTOM_EXPLODE_NOMARKS:
    case EV_CHANGE_TO_DUD:
    case EV_DUD_EXPLODE: return 1;
    default: return 0;
    }
}
static int explosion_weapon(entity_state_t *out,const entityState_t *source)
{
    uint32_t events[4]={0}; unsigned i,found=0; int needed=0,weapon;
    if(source->eType>ET_EVENTS)
        needed=explosion_event((unsigned)(source->eType-ET_EVENTS));
    else {
        memcpy(events,source->events,sizeof(events));
        for(i=0;i<4;++i) if(explosion_event(events[i])) { needed=1; break; }
    }
    if(!needed)return 1;
    /* C1 G_ExplodeMissile 52eddd..52ede4 copies the missile weapon word
     * into the new ET_GENERAL explosion's es+d4 before queuing event73/78/79.
     * The retained producer captures the same identity in source->weapon.
     * The event parameter remains DirToByte(normal), not the weapon index.
     * Resolve this captured identity; never inspect the freed missile or a
     * live entity slot when serializing an archived explosion. */
    weapon=WeaponToWire((unsigned)source->weapon);
    if(weapon<0 || weapon>65535) {
        snprintf(entity_seed_failure,sizeof(entity_seed_failure),
            "explosion weapon %u has no wire mapping",(unsigned)source->weapon);
        return 0;
    }
    /* Both explosion producers extract surface with (flags >> 20) & 31.
     * Native weaponModel is a four-bit captured/archive field (msg.c);
     * C1 stores it as byte +e6, independently of the model word at +e4. */
    if((unsigned)source->surfType>31 || (unsigned)source->weaponModel>15) {
        snprintf(entity_seed_failure,sizeof(entity_seed_failure),
            "explosion surface %d weapon model %d",source->surfType,source->weaponModel);
        return 0;
    }
    for(i=0;i<ENTITY_FIELDS;++i) {
        const entity_field_t *field=EntityField(out->type,i);
        if(!field)continue;
        if(field->offset==0x0d4) {
            out->field[i]=(out->field[i]&UINT32_C(0xffff0000))|(unsigned)weapon;
            found|=1;
        } else if(field->offset==0x090) {
            /* C1 52eec0..52eeca: traced material controls impact FX/audio. */
            out->field[i]=(unsigned)source->surfType;found|=2;
        } else if(field->offset==0x0e4) {
            /* C1 52edeb..52edf7: preserve model word and the adjacent byte. */
            out->field[i]=(out->field[i]&UINT32_C(0xff00ffff))|
                ((unsigned)source->weaponModel<<16);
            found|=4;
        }
    }
    if(found!=7) {
        snprintf(entity_seed_failure,sizeof(entity_seed_failure),"explosion asset fields");
        return 0;
    }
    if(source->eType==ET_GENERAL) {
        /* C1 G_ExplodeMissile 52edd6 creates the explosion with flags 0x10
         * (retained: 0x20). For kind 5 with an effect, 52ef94..52efb8 adds
         * 0x40|0x80000 (retained: 0x10000|0x80000), stores level.time at +58
         * (retained lerp.u word, native +54) and time+61000 at +7c (native
         * time2 +70). C1 persistent smoke 44ffd3..4500ed keys on exactly
         * these three fields, so without them smoke never re-plays for a
         * client who enters its PVS after the explosion event. */
        uint32_t native=(uint32_t)source->lerp.eFlags,flags=0;
        int smoke=(native&UINT32_C(0x10000))!=0,custom=smoke;
        /* C1 G_ExplodeMissile 52ef05..52ef1e: the custom arm (event79, every
         * kind but grenade/heavy/rocket/flash, so concussion and EMP) stores
         * level.time at +58 right after G_AddEvent; the retained producer
         * stores it at native lerp.u word0 (+54) on the same arm. The client
         * plays a custom explosion's projExplosionEffect from that time
         * (T5 cg_event.cpp custom arm: FX_PlayOrientedEffect(..., es->lerp.u
         * time, ...)), so left zero the effect starts at time 0, long over,
         * and draws nothing: the invisible concussion/EMP explosion. Grenade
         * and flash events play at the client's own time and keep +58 zero,
         * as C1's other arms never write it. */
        for(i=0;i<4;++i)
            if(events[i]==EV_CUSTOM_EXPLODE || events[i]==EV_CUSTOM_EXPLODE_NOMARKS) custom=1;
        if(native&UINT32_C(0x20)) flags|=UINT32_C(0x10);
        if(smoke) flags|=UINT32_C(0x40);
        flags|=native&UINT32_C(0x80000);
        for(i=0;i<ENTITY_FIELDS;++i) {
            const entity_field_t *field=EntityField(out->type,i);
            if(!field) continue;
            if(field->offset==0x008) out->field[i]=flags;
            else if(custom && field->offset==0x058) out->field[i]=(uint32_t)source->lerp.u.anonymous.data[0];
            else if(smoke && field->offset==0x07c) out->field[i]=(uint32_t)source->time2;
        }
    }
    return 1;
}
static int entity_seed(void *context,const entityState_t *source,snapshot_entity_bindings_t *out)
{
    int type;
    if(!context || !source || !out || source->number<0 || source->number>=ENTITYNUM_WORLD ||
       (type=EntityTypeToWire(source->eType))<0) return 0;
    memset(out,0,sizeof(*out)); out->state.type=(unsigned)type;
    snprintf(entity_seed_failure,sizeof(entity_seed_failure),"type-specific seed");
    if(!entity_event_parameters(source,out->event_parameters,*(const unsigned *)context,
         &out->event_suppressed_slots)) return 0;
    switch(source->eType) {
    case ET_EVENTS+EV_EARTHQUAKE:
    case ET_EVENTS+EV_PHYS_EXPLOSION_SPHERE:
    case ET_EVENTS+EV_PHYS_EXPLOSION_CYLINDER:
        return OverlayEnginePhysicsEvent(&out->state,source);
    case ET_EVENTS+EV_OBITUARY:
        return OverlayObituary(&out->state,source);
    case ET_ACTOR:
        /* Model/parts arrive in the matching archived actor record. The
         * identity here belongs to this captured entity, never a live lookup. */
        return source->lerp.u.actor.actorSlot>=0 && source->lerp.u.actor.actorSlot<32 &&
               source->lerp.u.actor.treeSelector>=0 && source->lerp.u.actor.treeSelector<5;
    case ET_ACTOR_CORPSE:
        /* A finished actor body (ActorBecomeCorpse): the same entity, now
         * naming its corpse slot. C1 keeps 16 corpse records and composes
         * the body from the actor it was (46C4DF -> 42A5C0). Without this
         * arm every actor corpse was dropped from the snapshot ("entity type
         * 20 cannot be seeded"), so a body vanished when its death
         * animscript ended. */
        return source->lerp.u.actor.actorSlot>=0 && source->lerp.u.actor.actorSlot<16 &&
               source->lerp.u.actor.treeSelector>=0 && source->lerp.u.actor.treeSelector<5;
    case ET_FX: {
        int effect=ServerByteEffectToWire((unsigned)source->un1.helicopterStage);
        return effect>0 && OverlayEngineTriggeredEffect(&out->state,source,(unsigned)effect);
    }
    case ET_LOOP_FX: {
        int effect=ServerByteEffectToWire((unsigned)source->un1.helicopterStage);
        return effect>0 && OverlayEngineLoopEffect(&out->state,source,(unsigned)effect);
    }
    case ET_EVENTS+EV_PLAY_FX: {
        int effect=ServerEffectToWire((unsigned)source->eventParm);
        return effect>0 && OverlayEngineEffectEvent(&out->state,source,(unsigned)effect);
    }
    case ET_EVENTS+EV_SOUND_ALIAS:
    case ET_EVENTS+EV_SOUND_ALIAS_AS_MASTER:
        /* Positional sound events: the registered bank index travels in
         * eventParm and the emitting entity in otherEntityNum. Retained
         * ScrCmd_PlaySound also produces this entity type, so without this
         * arm any script sound fails the whole frame for every client. */
        if(source->eventParm<=0 || source->eventParm>255 ||
           source->otherEntityNum<0 || source->otherEntityNum>ENTITYNUM_WORLD) return 0;
        return OverlayEngineSoundEvent(&out->state,source,(unsigned)source->eventParm,
            (unsigned)source->otherEntityNum);
    case ET_PLAYER:
        /* The weapon pair this entity state was captured with. Deriving it here
         * from source->weapon cannot work, and reading
         * g_entities[source->number].client->ps would be worse: source is a
         * snapshot ring slot that may have been copied from the archive, so the
         * live client's playerState is a different state and possibly a
         * different life. An alternate-mode weapon does not name its own
         * primary either - only the cached one in the playerState does - which
         * is why the previous test refused every weapon taking part in an
         * alternate pair instead of resolving it. BG_PlayerStateToEntityState
         * captures the pair beside the entity state; this reads what it
         * captured, and fails for a state that never had one rather than
         * publishing a guess. */
        return EntityStatePrimaryWeapon(source,&out->primary_weapon) &&
               OverlayEngineRadarJammer(&out->state,source);
    case ET_PLAYER_CORPSE:
        /* ClonePlayer captures the original client identity, animations and
         * corpse flags. The common overlay validates the reserved corpse slot
         * and publishes these from the archived state; no live model lookup
         * is needed because C1 copies the originating player's appearance. */
        return 1;
    case ET_INVISIBLE:
        /* Preserve the explicit invisible type when this entity is selected
         * for publication, including transitions out of a visible type. */
        return 1;
    case ET_MISSILE: return 1; /* Existing collector resolves weapon and trajectories. */
    case ET_ITEM: return 1; /* Collector translates the packed weapon selector. */
    case ET_PRIMARY_LIGHT:
        /* The common collector overlays the captured light union: color and
         * exponent, intensity, radius, inner/outer cone and BSP light index.
         * No model-bank lookup belongs here: index names a BSP primary light.
         * Rejecting this type here made that existing overlay unreachable. */
        return 1;
    case ET_PLANE: {
        int model=ServerModelToWire(source->index);
        return model>=0 && OverlayEnginePlane(&out->state,source,(unsigned)model);
    }
    case ET_MG42: {
        /* The turret's model, weapon, sentry owner, fired flag and heat/spin
         * block all come from the archived entity state, which the turret
         * producer refreshes every think. */
        int model=ServerModelToWire(source->index);
        return model>=0 && OverlayEngineTurret(&out->state,source,(unsigned)model);
    }
    case ET_HELICOPTER:
    case ET_VEHICLE: {
        /* Without this arm every aerial killstreak spawns server-side and is
         * then dropped from the snapshot, which is what
         * "CoDOL entity type 12 cannot be seeded" reported: the helicopter
         * existed, ran its light FX and its path, and no client ever saw it.
         *
         * All three overlays already existed and were simply unreachable.
         * Identity republishes the packed instance|definition<<4 and the
         * controlling client that EntityStateVehicleCaptured stored at
         * spawn - the retained SpawnVehicle and G_SpawnHelicopter both call
         * it - so a delayed or archived frame serializes the identity that
         * frame actually had instead of reading the live scr_vehicle, which
         * may since have been freed or recycled. Assets publish the visual
         * model from un2 (+d8; index/+88 is the collision brush) and the
         * turret weapon. Pose publishes the five interpolated body/steer/gun
         * values and the suspension travel the client traces wheels along.
         *
         * Model 0 is the legitimate "no visual model" case and
         * ServerModelToWire maps it to 0, which is exactly what the
         * asset overlay's both-or-neither test expects. */
        int model=ServerModelToWire((unsigned)source->un2);
        /* Name the step that refused, so a vehicle that still fails to
         * publish says which of the three it was instead of reporting the
         * type once and going quiet. */
        if(model<0) {
            snprintf(entity_seed_reason,sizeof(entity_seed_reason),
                "visual model %d has no wire slot",source->un2);
            return 0;
        }
        if(!OverlayEngineVehicleIdentity(&out->state,source)) {
            snprintf(entity_seed_reason,sizeof(entity_seed_reason),
                "identity not captured at spawn (clientNum %d)",
                source->clientNum);
            return 0;
        }
        if(!OverlayEngineVehicleAssets(&out->state,source,(unsigned)model)) {
            snprintf(entity_seed_reason,sizeof(entity_seed_reason),
                "assets rejected: model %d weapon %d brush %d",
                source->un2,source->weapon,source->index);
            return 0;
        }
        unsigned compass=EntityStateVehicleCompass(source);
        if(!OverlayEngineVehicleCompass(&out->state,compass)) {
            snprintf(entity_seed_reason,sizeof(entity_seed_reason),
                "vehicle compass material %u has no wire slot",compass);
            return 0;
        }
        if(!OverlayEngineVehiclePose(&out->state,source)) {
            snprintf(entity_seed_reason,sizeof(entity_seed_reason),
                "pose fields out of range");
            return 0;
        }
        return 1;
    }
    case ET_GENERAL:
    case ET_SCRIPTMOVER: {
        int model;
        /* SV_LinkEntity stamps SOLID_BMODEL into the captured entity state.
         * A delayed snapshot must not inspect a live slot that may since have
         * been freed, reused, or changed from brush to model (or the reverse). */
        if(source->solid==SOLID_BMODEL && IsTriggerClipHandle(source->index)) {
            /* A trigger a script played a sound on (zm wall buys, perks,
             * box, doors): C1 publishes it unsolid and modelless, and its
             * clip handle exists only on this server.  See the overlay. */
            if(!OverlayEngineTriggerEntity(&out->state,source))return 0;
        } else if(source->solid==SOLID_BMODEL) {
            /* Bound by the client's own cmodel count: C1 indexes cm.cmodels
             * with this word unchecked (0x586cf2). */
            if(!OverlayEngineBrushEntity(&out->state,source,ClientInlineModelCount())) {
                snprintf(entity_seed_reason,sizeof(entity_seed_reason),
                    "brush model %d is not one of the client's %u inline models",
                    source->index,ClientInlineModelCount());
                return 0;
            }
        } else {
            if((model=ServerModelToWire(source->index))<0) return 0;
            if(!OverlayEngineModelEntity(&out->state,source,(unsigned)model,0))return 0;
        }
        return OverlayEquipmentScrambler(&out->state,source) &&
               explosion_weapon(&out->state,source);
    }
    default:
        if(source->eType>ET_EVENTS) {
            /* Generic event temp entity (melee hit/miss/blood, impacts, ...):
             * the event is the type itself, already translated above; the
             * parameter rides in +0xac through the shared event mapper.
             * If its semantics are unavailable, omit this temporary entity;
             * zero may be a valid but entirely different asset or target. */
            static uint32_t reported[32]; static unsigned reported_count; unsigned k; int seen=0;
            uint32_t event=(uint32_t)(source->eType-ET_EVENTS),parm=0;
            int mapped;
            if(event==EV_PLAY_VIEWMODEL_FX_ON_TAG || event==EV_STOP_VIEWMODEL_FX_ON_TAG) {
                /* Same compound effect/tag slot as EV_PLAY_FX_ON_TAG: C1
                 * PlayViewModelFXOnTag 545109..5451e8 formats "%03d%s" and
                 * stores its CS3971 slot in +ac at 545217. */
                int slot=ServerTaggedEffectToWire((unsigned)source->eventParm);
                mapped=slot>0 ? (parm=(uint32_t)slot,1) : -1;
            } else mapped=EventParameterToWire(event,(uint32_t)source->eventParm,&parm);
            if(mapped!=1) {
                for(k=0;k<reported_count;++k) if(reported[k]==event) { seen=1; break; }
                if(!seen && reported_count<32) {
                    reported[reported_count++]=event;
                    ((void)0);
                }
                snprintf(entity_seed_failure,sizeof(entity_seed_failure),
                    "temporary event %u parameter %d",event,source->eventParm);
                return 0;
            }
            if(!OverlayEngineEventParameter(&out->state,parm)) return 0;
            /* A magicBullet's EV_FIRE_WEAPON temp entity carries the weapon
             * (IW4 498081..4980ad sets it); C1 CG_FireWeapon 0x481f50 plays
             * the fire sound only when +d4 names it and returns on 0, which
             * is all this server ever published. Coop only for now: the same
             * gap silences scripted magicBullet fire in mp and zm too
             * (docs/CAMPAIGN_MODE.md, coop AI combat). */
            if((event==EV_FIRE_WEAPON || event==EV_FIRE_WEAPON_LASTSHOT) && source->weapon>0) {
                const char *zone=ServerZoneType();
                if(ZoneTypeRunsSoldiers(zone)) {
                    int weapon=WeaponToWire((unsigned)source->weapon);
                    unsigned k;
                    if(weapon>0 && weapon<=65535)
                        for(k=0;k<ENTITY_FIELDS;++k) {
                            const entity_field_t *field=EntityField(out->state.type,k);
                            if(field && field->offset==0x0d4) {
                                out->state.field[k]=(out->state.field[k]&UINT32_C(0xffff0000))|(unsigned)weapon;
                                break;
                            }
                        }
                }
            }
            if((event==EV_MELEE_HIT || event==EV_MELEE_MISS) &&
               !OverlayEngineMeleeEvent(&out->state,source)) {
                snprintf(entity_seed_failure,sizeof(entity_seed_failure),
                    "melee event %u target %d weapon %d",event,source->otherEntityNum,source->weapon);
                return 0;
            }
            if((event==EV_BULLET_HIT || event==EV_BULLET_HIT_CLIENT_SMALL ||
                event==EV_BULLET_HIT_CLIENT_LARGE) &&
               !OverlayEngineBulletHit(&out->state,source)) {
                snprintf(entity_seed_failure,sizeof(entity_seed_failure),
                    "bullet event %u shooter %d surface %d",event,source->otherEntityNum,source->surfType);
                return 0;
            }
            return explosion_weapon(&out->state,source);
        }
        snprintf(entity_seed_failure,sizeof(entity_seed_failure),"no seed arm for entity type %d",source->eType);
        return 0;
    }
}
/* Developer probes: the wire state one captured entity would publish to
 * recipient, through the same seed and common overlay as the snapshot path.
 * 1 converted, 0 seed refused (entity_seed_failure says why), -1 the
 * common overlay refused. */
int ProbeEntityWire(const entityState_t *source,unsigned recipient,entity_state_t *out)
{
    snapshot_entity_bindings_t bindings;
    if(!source || !out) return 0;
    if(!entity_seed(&recipient,source,&bindings)) return 0;
    if(!OverlayEngineEntityVisible(&bindings.state,source,bindings.event_parameters,
         bindings.event_suppressed_slots^15u)) return -1;
    *out=bindings.state;
    return 1;
}
int BuildGameplayFrame(unsigned recipient,uint32_t sequence,const clientSnapshot_t *source)
{
    player_fields_t seed={0}; player_array_updates_t arrays={0};
    actor_state_t actors[32]; unsigned actor_count=0;
    engine_player_bindings_t player={0}; native_frame_bindings_t bindings={0};
    frame_failure="gameplay frame arguments";
    if(recipient>=32) return 0;
    ClearClientNativeFrame(recipient);
    if(!source || source->ps.clientNum<0 || source->ps.clientNum>=32) return 0;
    /* C1's equipped-weapon array carries whether each owned weapon has been
     * raised before.  The retained playerState already owns the matching
     * weaponold bitset; bind the captured state so first-raise animations do
     * not restart on every snapshot. */
    {
        const ps_ext_t *raised=PlayerStateExtConst(&source->ps);
        player.used_before=raised ? raised->weaponold : NULL;
    }
    frame_failure="player cursor hint";
    {
        use_hint_t hint;
        const ps_ext_t *extension=PlayerStateExtConst(&source->ps);
        /* Resolve from this snapshot's native hint and captured weapon binding.
         * A weapon hint with no binding fails; never reinterpret it through
         * the current weapon map or the followed player's live state. */
        int weapon=extension && extension->use_hint_weapon ?
            (int)extension->use_hint_weapon-1 : -1;
        if(!ResolveEngineUseHint(&hint,&source->ps,weapon) ||
           !OverlayUseHint(&seed,&hint)) return 0;
    }
    /* The lock belongs to the state being serialized, not to the client it
     * names: a spectator following an archived playerState publishes the lock
     * that state carried, not the followed player's current one. Seed from it
     * before the native C1 field overlays. */
    frame_failure="player weapon lock";
    if(!OverlayWeaponLock(&seed,PlayerStateWeaponLockRead(&source->ps)))return 0;
    frame_failure="player perk slots";
    {
        const ps_ext_t *extension=PlayerStateExtConst(&source->ps);
        if(!extension || !OverlayPlayerPerks(&seed,&extension->perks)) return 0;
    }
    /* The looping sound belongs to the state as well: C1 carries it at
     * playerState 0x1f4 and copies it into the entity state in every
     * BG_PlayerStateToEntityState, so the owner of a delayed or archived state
     * hears what that state carried. Read from source->ps, never from the live
     * client or from g_entities. */
    frame_failure="player loop sound";
    {
        unsigned loop=0;
        PlayerStateLoopSoundRead(&source->ps,&loop);
        if(!OverlayPlayerLoopSound(&seed,loop)) return 0;
    }
    /* The weapon HUD icon overrides belong to the state as well, because the
     * producer published them into it: IW4 copies the script-owned gclient
     * array into ps+0x470..0x484 in ClientEndFrame (0x005d80bc) and every
     * archive, capture and spectator copy carries the result, which is also how
     * a follower ends up serializing the followed player's icons. Read the
     * captured state only - never the script store or a live client - and
     * translate its native material indices through the CoDOL material bank. */
    frame_failure="player weapon hud icon overrides";
    if(!OverlayServerWeaponHudIcons(&seed,&source->ps)) return 0;
    /* SetClientMaterial's ps+0x53c/0x540, from the captured state: C1 keeps
     * them inside the playerState, so a follower publishes the followed
     * player's, and the zombies HUD draws ps+0x540 as the local portrait. */
    frame_failure="player client materials";
    if(!OverlayServerClientMaterials(&seed,&source->ps)) return 0;
    /* Compass objective icons belong to the captured state too. The retained
     * objective_icon stores G_MaterialIndex's native CS_MATERIALS index in
     * ps->objective[i].icon; C1 reads section 5's icon as a slot of its own
     * material bank, CS[4275 + icon] (C1 0x453e72 via the compass drawer
     * 0x4325fb and the on-screen drawer 0x432a96), the bank hudelem materials
     * are translated into. Forwarding the native index named whatever
     * material happened to own that C1 slot, which is why bomb sites and
     * flags drew the wrong icons. An unresolvable name carries 0, C1's
     * built-in fallback, rather than another material's slot. */
    {
        unsigned i;
        player.objective_icons_bound=1;
        /* Slots 16..31 have no retained storage; take them from the level
         * table with G_RunFrame's team rule for this state's client. */
        FillHighObjectives(source->ps.clientNum,player.high_objectives);
        player.high_objectives_bound=1;
        /* objective_player: slots shown to one client only are empty for
         * every other client's state (a follower sees the followed one's). */
        player.objective_hidden_mask=ObjectiveHiddenMask(source->ps.clientNum);
        for(i=0;i<32;++i) {
            const objective_t *o=i<16 ? &source->ps.objective[i] : &player.high_objectives[i-16];
            int wire=o->state!=OBJST_EMPTY && o->icon>0 ?
                ServerMaterialToWire((unsigned)o->icon) : 0;
            player.objective_icon[i]=(uint16_t)(wire>0 && wire<4096 ? wire : 0);
        }
    }
    frame_failure="player action slots";
    if(!OverlayActionSlots(&seed,&source->ps)) return 0;
    frame_failure="player killstreak HUD";
    if(!OverlayKillstreakHud(&seed,&source->ps)) return 0;
    /* Field orders (C1 ps+0x1b4..0x1d0): the recipient's own, even while the
     * death cam or killcam shows another player's archived state; only a
     * spectator draws the followed player's (field_orders.c). */
    frame_failure="player field orders";
    if(!OverlayFieldOrdersFor(&seed,&source->ps,recipient)) return 0;
    if(!OverlayRadarTypes(&seed,&source->ps)) return 0;
    frame_failure="player primary weapon";
    if(!WeaponStatePartnerWeapon(&source->ps,&player.alt_weapon)) return 0;
    frame_failure="player offhand selection";
    {
        unsigned slot,weapon;
        /* Read from source->ps like every other per-state producer here, so a
         * delayed or archived state publishes the selection it carried. A
         * state with no recorded selection publishes three empty slots, which
         * is what it can raise: nothing. */
        for(slot=0;slot<3;++slot)
            player.offhand_slot[slot]=
                WeaponStateOffhandSlot(&source->ps,slot,&weapon)?weapon:0u;
    }
    frame_failure="player secondary weapon hand";
    {
        weapon_hand_t hand;
        unsigned slot,shot_hand,shot_last;
        /* These accessors read the extension carried by source->ps, including
         * archived and spectator copies, rather than the live client slot. */
        if(!WeaponStateHand(&source->ps,1,&hand)) return 0;
        player.dual_wield=WeaponStateIsDualWielding(&source->ps);
        player.second_hand.state=(unsigned)hand.state;
        player.second_hand.animation=(unsigned)hand.animation;
        player.second_hand.time=hand.time;
        player.second_hand.delay=hand.delay;
        player.second_hand.restrict_time=hand.restrict_time;
        player.second_hand.shot_count=hand.shot_count;
        player.second_hand.fresh_pull=hand.fresh_pull;
        for(slot=0;slot<4;++slot) {
            if(!WeaponStateShotRecord(&source->ps,slot,&shot_hand,&shot_last)) continue;
            player.shots[slot].known=1;
            player.shots[slot].hand=(unsigned char)shot_hand;
            player.shots[slot].last_shot=(unsigned char)shot_last;
        }
        if(!WeaponStateSpinUpCharge(&source->ps,&player.spinup_charge)) return 0;
    }
    /* The scripted queue this state carries, resolved with the same mapping
     * the entity queue uses: its events reach the same client tables and now
     * include EV_PLAY_FX_ON_TAG, whose parameter is a compound slot. The
     * translation layer receives the result and never reaches into server
     * asset state itself. */
    frame_failure="player scripted event parameters";
    {
        uint32_t events[PS_SCRIPT_EVENT_SLOTS],parameters[PS_SCRIPT_EVENT_SLOTS];
        unsigned sequence,i;
        static char script_event_failure[128];
        if(!PlayerStateScriptEventQueue(&source->ps,&sequence,events,parameters)) return 0;
        for(i=0;i<PS_SCRIPT_EVENT_SLOTS;++i) {
            if(ServerResolveEventParameters(events+i,parameters+i,
                                                  player.script_event_parameters+i,1)) continue;
            snprintf(script_event_failure,sizeof(script_event_failure),
                "player scripted event %u parameter %u slot %u sequence %u",
                events[i],parameters[i],i,sequence);
            frame_failure=script_event_failure;
            return 0;
        }
        if(!event_suppression(events,parameters,recipient,&player.script_event_suppressed_slots)) return 0;
    }
    /* alpha0022 spawn_player.build_spawn_player_fields uses a zero baseline,
     * with required live movement/weapon/health values supplied by overlays.
     * Animation commands still have no producer here; actors do, below. */
    /* Actors come from the capture taken when this snapshot was built, so an
     * archived recipient publishes the composition that was live then. A
     * missing capture fails the frame rather than publishing another
     * snapshot's actors or silently removing every actor. */
    frame_failure="actor capture";
    if(!CopyClientSnapshotActors(recipient,sequence,actors,32,&actor_count)) return 0;
    bindings.actors=actors; bindings.actor_count=actor_count;
    bindings.player_seed=&seed; bindings.array_seed=&arrays; bindings.player=&player;
    bindings.entity_resolve=entity_seed; bindings.client_resolve=client_seed;
    /* Synchronous resolver uses the actual receiving connection, including
     * spectators, not the followed player's live or archived client number. */
    bindings.entity_context=&recipient;
    /* No event clocks are passed at all: both counters published for this
     * frame are epochs of the states being serialized, so a delayed spectator
     * carries those states' counters instead of advancing recipient-side ones. */
    return BuildClientNativeFrame(recipient,sequence,source,&bindings);
}
#endif

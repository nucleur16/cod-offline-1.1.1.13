/* Translate named IW3 fields; CoDOL wire offsets are never engine offsets. */
#ifndef ENGINE_STATE_H
#define ENGINE_STATE_H
#include "player.h"
#include "playerstate.h"
#include "playerstate_ext.h"
#include "clients.h"
#include "weapon_registry.h"
#pragma pack(push, 8)
int OverlayObituary(entity_state_t *,const entityState_t *);
int OverlayEngineViewmodel(player_fields_t *,const playerState_t *,unsigned);
int OverlayEngineViewhands(player_fields_t *,unsigned);
/* Overlay measured movement/camera fields, stance-height transition metadata
 * and the verified sprint flag. Lerp target is signed eight-bit; direction is
 * boolean and the start time retains its full signed representation.
 * Preserve other flags/fields for their weapon, animation and gameplay adapters.
 * Atomic: invalid engine data leaves the destination unchanged. */
int OverlayEngineMovement(player_fields_t *wire, const playerState_t *engine);
/* Translate IW3 movement mode and follow/player presence. Other CoDOL flags
 * stay owned by their adapters, rather than copying incompatible IW3 bits. */
int OverlayEnginePlayerMode(player_fields_t *wire, const playerState_t *engine);
/* Shared teleport/crouch/prone/dead/ADS eFlags only. Preserve every other
 * destination bit; clearing an engine flag clears its owned destination bit. */
int OverlayEnginePlayerFlags(player_fields_t *,const playerState_t *);
/* CoDOL-native leg/torso animation indices, including the 0x400 restart bit.
 * Animation timers and other animation state remain separate producers. */
int OverlayEnginePlayerAnimations(player_fields_t *,const playerState_t *);
/* alt_weapon is the native index of C1's ps+0x340 word
 * (WeaponStatePartnerWeapon: the owning primary of an alternate mode,
 * else the selecting command's partner word, 0 from a C1 client). All three IDs
 * must have registered asset mappings. */
/* C1 playerState 0x32c/0x330/0x334, wire fields of the same names: the three
 * selected offhands PM_Weapon_CheckForOffHand raises from. A slot of 0 is a
 * selection of nothing, which is what an unrecorded state publishes as well --
 * neither raises anything. */
int OverlayEngineOffhandSelection(player_fields_t *wire,
                                        const unsigned slots[3]);
int OverlayEngineWeaponSelection(player_fields_t *wire, const playerState_t *engine,
                                      unsigned alt_weapon);
/* Exact ADS fraction (0..1) and aim-spread scale (0..255), with no rounding
 * or clamping. Both must be finite; all output is preserved on failure. */
int OverlayEngineAim(player_fields_t *,const playerState_t *);
/* Primary-hand phase. Gameplay must supply resolved CoDOL state/animation IDs,
 * including its animation toggle; IW3 enum values are not copied. Timing fields
 * are signed 16-bit on the client. Preserves secondary-hand fields; atomic. */
int OverlayEngineWeaponPhase(player_fields_t *,const playerState_t *,unsigned state,unsigned animation);
/* Verified primary ready/raise/fire/rechamber/reload/melee/sprint transitions. Other native states
 * require separate measurement. Failure preserves out. */
int TranslateEngineWeaponState(unsigned engine_state,unsigned *out);
/* Resolve a retained hand state and animation to the exact C1 phase. Drop
 * states need the animation because C1 gives alternate drop its own state;
 * night-vision phases are also inserted later in the C1 enum. */
int ResolveEngineWeaponState(unsigned engine_state,unsigned engine_animation,
                                   unsigned *out);
/* Translate the native state and publish primary-hand timing; animation remains
 * an explicitly resolved CoDOL ID including its toggle. Atomic output. */
int OverlayEnginePrimaryWeaponPhase(player_fields_t *,const playerState_t *,unsigned animation);
/* Complete primary reload phase adapter: translates the engine's reload
 * state and animation including its restart bit, then overlays timing.
 * Accepts only reload states/animations; leaves all output intact on failure. */
int OverlayEngineReloadPhase(player_fields_t *,const playerState_t *);
/* Primary sprint entry/loop/exit state, animation/restart and timing adapter.
 * Accepts only native sprint states and animations; failure preserves output.
 * Movement flags remain owned by the movement producer, not copied here. */
int OverlayEngineSprintPhase(player_fields_t *,const playerState_t *);
/* CoDOL dolphin-dive weapon phases 28..30 -> C1 31..33, animations 35..40. */
int OverlayEngineDivePhase(player_fields_t *,const playerState_t *);
/* Normal/quick/empty/alternate drop adapter. Alternate drops need both the
 * native state and animation to select CoDOL state 5; the state-only mapper
 * deliberately does not accept native drop states. Atomic, primary hand only. */
int OverlayEngineWeaponDropPhase(player_fields_t *,const playerState_t *);
/* Normal/first/quick/empty/alternate raise state, animation and restart bit.
 * Preserves engine timers and secondary-hand fields; atomic on failure. */
int OverlayEngineWeaponRaisePhase(player_fields_t *,const playerState_t *);
/* Dispatch measured primary idle/fire/rechamber/raise/drop/reload/melee/sprint
 * and offhand phases from native state and animation together. Unsupported
 * phases (such as night vision) reject rather than defaulting
 * to idle. Preserves secondary-hand fields and all output on failure. */
int OverlayEnginePrimaryWeapon(player_fields_t *,const playerState_t *);
/* Offhand phases plus main-weapon detonation. Native fire/post-fire merge
 * into CoDOL state 19. Only measured animation roles accepted; atomic.
 * Offhand selection, usage flags and grenade timers have separate producers. */
int OverlayEngineOffhandPhase(player_fields_t *,const playerState_t *);
/* Signed grenade/throwback timers, translated throwback owner and the
 * offhand-use weapon flag. Clears the owned flag on exit; preserves all other
 * flags and fields. No clamping of negative timer sentinels; atomic. */
int OverlayEngineGrenadeState(player_fields_t *,const playerState_t *);
/* Own the three CoDOL offhand-disable bits as a group. Does not own global
 * weapon-disable or active-offhand flags. Individual offhand restrictions
 * require extending this producer when their native controls are implemented. */
int OverlayEngineOffhandControl(player_fields_t *,const playerState_t *);
/* Second weapon hand. C1 runs its whole weapon machine once per hand and
 * keeps the second hand's phase in the block at +0x284..+0x29c, with the
 * akimbo flag at +0x35c (PM_Weapon 0x41dc36, PM_Weapon_Hand 0x41ace0,
 * PM_StartWeaponAnim 0x4180b0/0x4180ec). The caller supplies that hand's own
 * native state, animation (including the retained 0x200 restart bit) and
 * timers; they are translated with the same tables as the primary hand and
 * are not copied from it. dual_wield 0 clears the whole block. Atomic. */
typedef struct {
    unsigned state, animation, shot_count, fresh_pull;
    int time, delay, restrict_time;
} engine_hand_t;
/* Which hand produced the fire event in each player-state ring slot, and
 * whether it emptied that hand's magazine. known 0 leaves the slot to the
 * ordinary event mapping. */
typedef struct { unsigned char hand, last_shot, known; } engine_shot_record_t;
/* Same primary player-event queue, with those records applied to the fire
 * events so each shot reaches the client as its own hand's event. Slots with
 * no record behave exactly like OverlayEnginePlayerEvents. */
int OverlayEnginePlayerEventsWithShots(player_fields_t *,const playerState_t *,
    const uint32_t parameters[4],const engine_shot_record_t shots[4]);
/* Recipient-safe form of the primary queue. Physical slots absent from
 * visibleSlots are encoded as EV_NONE while the captured queue epoch remains
 * unchanged, so one unsupported presentation event cannot stall snapshots. */
int OverlayEnginePlayerEventsWithShotsVisible(player_fields_t *,const playerState_t *,
    const uint32_t parameters[4],const engine_shot_record_t shots[4],unsigned visibleSlots);
/* C1 ps+0x5c0, wire field 171: the spin-up charge, 0..1. */
int OverlayEngineSpinUpCharge(player_fields_t *,float charge);
int OverlayEngineSecondaryWeapon(player_fields_t *,int dual_wield,
    const engine_hand_t *);
int OverlayEngineWeaponSwitchControl(player_fields_t *,const playerState_t *);
/* Shared use-release/reload request (0x1) and global weapon-disable (0x80).
 * Owns only those two CoDOL flags. Private extension bits are never copied. */
int OverlayEngineWeaponUseControl(player_fields_t *,const playerState_t *);
/* Publish health/max-health as absolute array updates; preserve other stats
 * and sections for their producers. CoDOL reads these values as signed shorts. */
int OverlayEngineHealth(player_array_updates_t *updates, const playerState_t *engine);
/* HUD presentation fields only: geometry, colors, timing, value and alignment.
 * Preserves type/font/flags/target/text/material/sound fields for semantic and
 * configstring adapters. IW3 has no font-scale animation: initialize its CoDOL
 * interpolation fields to the current scale with zero duration. Atomic output. */
int OverlayEngineHudPresentation(hud_element_t *,const hudelem_t *);
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    unsigned label,text,material,sound,flags; /* Resolved CoDOL values, not IW3 IDs/bits. */
    float font_scale_from;
    int font_scale_start,font_scale_duration;
} hud_bindings_t;
/* Verified named flags only: foreground, hide-when-dead, hide-when-in-menu.
 * Other IW3 bits require semantic translation; failure preserves out. */
int TranslateEngineHudFlags(unsigned engine_flags,unsigned *out);
/* Builds a complete element using verified shared types/fonts and explicit
 * semantic bindings. Map-name/gametype types and offscreen material variants
 * need separate conversion and are rejected. Font transition bindings must
 * belong to the same captured native element. Output is atomic. */
int BuildEngineHudElement(hud_element_t *,const hudelem_t *,const hud_bindings_t *);
typedef int (*hud_resolver_t)(void *,const hudelem_t *,hud_bindings_t *);
/* Gather packed native arrays until HE_TYPE_FREE; emit archival before current.
 * Resolver owns configstring/flag/sound translation and must not change source.
 * Empty arrays need no resolver. A failed element leaves the whole out intact. */
int CollectEngineHud(hud_state_t *,const hudElemState_t *,hud_resolver_t,void *);
/* Refresh/publish HUD asset names, then resolve native indices and named flags.
 * Source must be a live HUD copy registered by HudElem_UpdateClient; historical
 * states need captured semantic bindings. Nonzero sound IDs remain unsupported. */
int CollectServerHud(hud_state_t *,const hudElemState_t *);
/* Full inventory publication in ascending native weapon order. used_before is
 * a native-ID bitset maintained by gameplay for CoDOL's first-raise flag;
 * NULL means no weapon has been raised. Explicitly clears all unoccupied slots.
 * Fails atomically for unmapped ownership or more than 15 equipped weapons. */
int OverlayEngineInventory(player_array_updates_t *updates, const playerState_t *engine,
                                const uint32_t used_before[WEAPON_BIT_WORDS]);
/* Asset loaders supply keys, gameplay supplies target slots. engine_index=-1
 * writes zero ammo explicitly (including dropped/unused slots). Clip wire_key
 * -1 is the empty-clip sentinel; with an engine index it is the record of an
 * unlimited-ammo weapon, whose C1 clip index is -1. Reserve keys are 0..1023. */
/* left_ammo is the second hand's magazine for this clip; -1 means the slot
 * has no second magazine, which is every clip of a single-hand weapon. */
typedef struct { int16_t engine_index, wire_key, left_ammo; } ammo_binding_t;
int OverlayEngineAmmo(player_array_updates_t *updates, const playerState_t *engine,
                           uint16_t clip_mask, const ammo_binding_t clips[16],
                           uint16_t reserve_mask, const ammo_binding_t reserves[16]);
/* Resolved CoDOL configstring slots for the retained IW3 client's six attachment
 * slots. Zero means absent in both structures; loaders must resolve nonzero IDs. */
typedef struct {
    unsigned model, attach_model[6], attach_tag[6];
    /* C1 G_UpdateClientState 0x50d0f0..0x50d1b8 derives these from the
     * client's own playerState: 0x018 dualWielding (ps+0x35c == 1), 0x01c
     * riotShieldNext ((weapFlags >> 12) & 1) and 0x0a8 diveState
     * ((pm_flags & 0x400000) && !(pm_flags & 0x800) && pm_time == 0 ?
     * ps+0x3334 + 1 : 0). 0x12c/0x130 are the entities holding the player's
     * right/left world gun (C1 gclient+0x3594/+0x3598; CG clientInfo
     * +0xaf4/+0xaf8 names the DObj its muzzle flash and muzzle point use;
     * 0 = none); 0x134 the colour selector 0x460452 reads (0 = default
     * white). */
    unsigned dual_wield, riot_shield_next, dive_state, colour_index;
    unsigned world_weapon_entity[2];
    /* 0x010, three bits: ACTION_SLOT_HELP slot - 1 (C1 0x46c95e -> ci+0x5c,
     * read by the key substitution 0x603db8). */
    unsigned action_slot_help;
} client_assets_t;
/* Shared team/model/attachments and attached-vehicle entity (ENTITYNUM_NONE 2047).
 * Reserved world entity cannot be a vehicle. Other fields preserve their seed. */
int OverlayEngineClient(client_state_t *wire, const clientState_t *engine,
                             const client_assets_t *assets);
/* Uses the server's published model/tag map. Refresh asset names before
 * assembling a frame; unresolved native indices fail without changing wire. */
int OverlayServerClient(client_state_t *wire, const clientState_t *engine);
/* General/script-mover XModel selectors use CoDOL +98, not native +88.
 * Explicitly rejects brush entities; their indices address inline geometry.
 * model is a resolved CoDOL model-bank slot, with zero meaning absent. */
int OverlayEngineModelEntity(entity_state_t *,const entityState_t *,unsigned model,int brush_model);
int OverlayEnginePlane(entity_state_t *,const entityState_t *,unsigned model);
int OverlayEngineEffectEvent(entity_state_t *,const entityState_t *,unsigned effect);
/* Sound event entity: registered alias index and the emitting entity, using
 * the engine-side owner convention in script_sound.h. */
int OverlayEngineSoundEvent(entity_state_t *,const entityState_t *,unsigned sound,unsigned owner);
int OverlayEngineEventParameter(entity_state_t *,unsigned parameter);
/* Melee hit/miss temp entity: struck entity (+80) and attacker weapon. */
int OverlayEngineMeleeEvent(entity_state_t *,const entityState_t *);
/* Bullet hit temp entities (60, 63, 64): shooter, surface, weapon and victim. */
int OverlayEngineBulletHit(entity_state_t *,const entityState_t *);
/* Knife-table row (1-based, 0 = none) every player carries on this server. */
unsigned ServerMeleeRow(void);
int OverlayEngineTriggeredEffect(entity_state_t *,const entityState_t *,unsigned effect);
int OverlayEngineLoopEffect(entity_state_t *,const entityState_t *,unsigned effect);
/* model_count is the CLIENT's inline-model count (ClientInlineModelCount),
 * never cm.numSubModels, which also counts the server-only trigger models. */
int OverlayEngineBrushEntity(entity_state_t *,const entityState_t *,unsigned model_count);
/* A bmodel entity whose index is a server-only trigger clip handle: published
 * with no model and solid 0, as C1's link routine leaves a trigger. */
int OverlayEngineTriggerEntity(entity_state_t *,const entityState_t *);
/* Uses the published model map and gentity.r.bmodel to select the asset path.
 * Refresh asset publication before constructing the frame. Atomic output. */
int OverlayServerModelEntity(entity_state_t *,const gentity_t *);
/* Collect the client span recorded by an engine snapshot (first_client,
 * num_clients) from svsHeader's ring. Output is sorted, owned by the caller,
 * and untouched on any stale span, invalid ID, or unresolved asset. */
int CollectServerClients(int first, int count, client_state_t *out, size_t capacity);
/* Full-frame variant: require an explicit semantic seed for every client.
 * The resolver owns fields not represented by retained clientState_t. */
typedef int (*snapshot_client_resolver_t)(void *,const clientState_t *,client_state_t *);
int CollectServerClientsResolved(int,int,client_state_t *,size_t,
    snapshot_client_resolver_t,void *);
/* Caller first selects the translated CoDOL entity type and initializes its
 * other fields. Copies named IW3 trajectories, never an entityState byte image.
 * Maps the six basic retained IW3 modes and client physics (8 -> 9) to
 * CoDOL modes. Sine requires a positive duration. Physics preserves launch
 * position/angles and linear/angular deltas for the client's rigid body. */
int OverlayEngineTrajectories(entity_state_t *wire, const entityState_t *engine);
/* ET_MISSILE's measured weapon/model selector is 0xD4. Player entities use
 * a packed current/primary pair there instead. Resolves the native weapon map,
 * sets normal scale and trajectories;
 * preserves fields owned by event/flags/ownership adapters. Atomic on failure. */
int OverlayEngineMissile(entity_state_t *wire, const entityState_t *engine);
/* Base-variant weapon pickup: native index packs weapon + 128*variant;
 * CoDOL +98 uses weapon + 700*variant. Only variant zero is established.
 * Resolves the weapon namespace, rejects none/unmapped/other variants. */
int OverlayEngineItem(entity_state_t *,const entityState_t *);
/* Captured earthquake and spherical physics-explosion payloads. */
int OverlayEnginePhysicsEvent(entity_state_t *,const entityState_t *);
/* CoDOL turret (eType 11). Publishes the archived turret lerp block, the
 * direction-byte word, the model and weapon selectors, the sentry owner,
 * the turret-fired flag and the minimap-visible flag (eFlags 0x400000).
 * Native 0x54..0x6c map to CoDOL 0x58..0x70 and the native turretState word
 * at 0x70 maps to CoDOL 0x74. Reads only archived
 * entity state, never the live gentity.
 * model is a resolved CoDOL model-bank slot, with zero meaning absent. */
int OverlayEngineTurret(entity_state_t *,const entityState_t *,unsigned model);
/* Five measured vehicle controller angles and suspension travel, from the
 * captured native state.
 * This is only the pose portion: identity, flags, controlling client, model, weapon and
 * other vehicle data remain the full vehicle adapter's responsibility.
 * Does not interpret native materialTime as C1's +7c value. Atomic output. */
int OverlayEngineVehiclePose(entity_state_t *,const entityState_t *);
/* Map-static C1 realm-0 animtree id (1..15, 0 none) that vehicles of one
 * captured definition slot (1..31) publish at +094. Set by the RC wheel
 * animation producer when it binds; slot 0 clears every slot. */
void SetVehicleDefinitionAnimTree(unsigned definition_slot,unsigned client_tree);
/* Captured instance/VehicleDef identity and spawning client only. Does not
 * read live vehicle pools or infer identity from a legacy packed field.
 * Both fields commit atomically; full vehicle flags/assets remain separate. */
int OverlayEngineVehicleCompass(entity_state_t *wire, unsigned material);
int OverlayEngineVehicleIdentity(entity_state_t *,const entityState_t *);
/* Model bank slot is resolved by the server layer from the captured index.
 * Maps the captured turret weapon into the low word at+d4, preserving the
 * upper word for its own producer. Atomic; no live gentity/VehicleDef lookup. */
/* model is the server-resolved visual model from captured es.un2. The
 * captured es.index is a collision brush and is copied without asset remap. */
int OverlayEngineVehicleAssets(entity_state_t *,const entityState_t *,unsigned model);
/* Per-entity generation state, sampled once per engine frame, independently
 * of client ACKs. Reset when an entity slot is reused or the map is reset.
 * Fewer than 256 events may elapse between samples (IW3 retains only four).
 * Translates IDs and expands IW3's 8-bit sequence to CoDOL's 11-bit sequence.
 * Event parameters remain owned by semantic parameter/asset adapters.
 * Both outputs stay unchanged on invalid type, sequence or unsupported ID. */
/* The recipient-side clock this file used to extend the retained 8-bit
 * sequence with is gone: both event counters are now epochs produced beside
 * the state that owns them (playerstate_ext.h, entitystate_ext.h)
 * and published from it, so a rewind to an archived state carries that
 * state's counter instead of advancing a shared one. */
/* Resolve verified queue semantics from named native fields. Entity events
 * are integer words despite the retained header declaring float vectors;
 * these helpers copy their representation, never numerically convert floats. */
int ResolveEngineEntityEventParameters(const entityState_t *,uint32_t out[4]);
int ResolveEnginePlayerEventParameters(const playerState_t *,uint32_t out[4]);
/* Resolve every supported physical slot and return its bit in visibleSlots.
 * Unsupported event/parameter pairs are cleared in out and do not make the
 * complete player frame fail. Invalid arguments still fail atomically. */
int ResolveEnginePlayerEventParametersVisible(const playerState_t *,uint32_t out[4],
    unsigned *visibleSlots);
int OverlayEngineEntityEvents(entity_state_t *,const entityState_t *);
/* Complete entity event queue, with four explicitly resolved CoDOL parameters.
 * Asset/ownership parameter semantics must be resolved before calling; do not
 * pass the native parameter array by default. All fields and clock are atomic. */
int OverlayEngineEntityEventQueue(entity_state_t *,const entityState_t *,
                                      const uint32_t parameters[4]);
/* Recipient selection is resolved before encoding from the captured event's
 * audience. Bit i retains slot i; excluded events keep their queue epoch. */
int OverlayEngineEntityEventQueueVisible(entity_state_t *,const entityState_t *,
                                             const uint32_t parameters[4],unsigned visibleSlots);
/* Compose trajectories, normal IW3 model scale, primary entity events, player
 * client-info index, and the missile weapon selector. Caller supplies an already
 * typed CoDOL seed and resolved event parameters; other fields retain their
 * type-specific producers. Never retag an existing field array (table order
 * varies by type). Both state and generation clock commit together. */
int OverlayEngineEntity(entity_state_t *,const entityState_t *,
                             const uint32_t parameters[4]);
/* Explicit slot suppression is applied during conversion, before unmapped
 * event IDs can reject the common overlay. Epoch and source remain intact. */
/* The coop actor fire (actor_fire.c) registers which actors' entity
 * weapon (+d4) reaches the wire; none until it does. */
void SetActorWeaponFilter(int (*filter)(const entityState_t *));
int OverlayEngineEntityVisible(entity_state_t *,const entityState_t *,
                                    const uint32_t parameters[4],unsigned visibleSlots);
/* The same conversion with the generic parent link (wire +0xe8) cleared, for
 * a child whose parent is absent from the recipient's snapshot. */
int OverlayEngineEntityVisibleUnlinked(entity_state_t *,const entityState_t *,
                                            const uint32_t parameters[4],unsigned visibleSlots);
int OverlayEnginePlayerEntityFlags(entity_state_t *,const entityState_t *);
/* Captured clone identity, appearance-copy/stance flags, animation words and
 * ground identity. Atomic on failure. This is not a complete corpse adapter:
 * body pose, part visibility and ragdoll producers require separate mapping
 * before the server frame resolver can admit corpse entities. */
int OverlayEngineCorpseIdentity(entity_state_t *,const entityState_t *);
/* Remote player and corpse pose words C1 draws from: lean (+58), movement
 * direction (+5c), waist/torso pitch (+68/+6c); for a player also the ground
 * entity (+88) and, on a turret, the turret entity (+80). Atomic on failure;
 * an out-of-range captured value publishes the neutral value. */
int OverlayEnginePlayerEntityPose(entity_state_t *,const entityState_t *);
/* Hidden-part bits (+ec..+108) for general and script-mover entities; other
 * types are left untouched and succeed. Atomic on failure. */
int OverlayEnginePartBits(entity_state_t *,const entityState_t *);
/* CoDOL-native BG animation records use a 10-bit tree index and 0x400 restart
 * toggle. Retained BG_PlayerStateToEntityState copies those words unchanged.
 * Only remote players use this adapter; actors have a separate record section.
 * Does not translate an upstream IW3 tree/index namespace. Atomic on failure. */
int OverlayEnginePlayerEntityAnimations(entity_state_t *,const entityState_t *);
/* Remote-player weapon pair: low half is current, high half is the primary
 * weapon for alternate inventory type (otherwise current again). Both arguments
 * are native weapon IDs resolved through the registered weapon map. Gameplay
 * must supply the primary explicitly; entityState alone does not retain it. */
int OverlayEnginePlayerEntityWeapons(entity_state_t *,const entityState_t *,unsigned primary_weapon);
/* Remote-player composition: shared entity fields plus current/primary weapon
 * pair. Supply gameplay's primary weapon for alt inventory, current otherwise.
 * Typed seed and resolved parameters remain required. State and event clock
 * commit together, including on a failed weapon lookup after event mapping. */
int OverlayEnginePlayerEntity(entity_state_t *,const entityState_t *,
    unsigned primary_weapon,const uint32_t parameters[4]);
/* Primary predictable-player event queue. Caller supplies four semantically
 * resolved CoDOL parameters (0..2047); never pass the IW3 array by default.
 * Uses a separate clock from entity events and the secondary CoDOL queue.
 * Preserves that secondary queue and fields owned by other producers.
 * Both outputs remain unchanged on failure. Same clock lifecycle as above. */
/* Publishes the state's own event epoch (playerstate_ext.h) modulo 2048,
 * so an archived or followed state carries the counter it was produced with. */
int OverlayEnginePlayerEvents(player_fields_t *,const playerState_t *,
                                   const uint32_t parameters[4]);
typedef struct {
    unsigned alt_weapon;
    unsigned viewmodel_index; /* Resolved CoDOL model-bank slot, not native index. */
    unsigned viewhands_index; /* Separate gloves, C1 ps+0x148. */
    /* The three selected offhands of the state being serialized. */
    unsigned offhand_slot[3];
    unsigned dual_wield;           /* Second hand active; C1 ps+0x35c. */
    engine_hand_t second_hand; /* That hand's own phase and timers. */
    engine_shot_record_t shots[4]; /* Per ring slot; see above. */
    float spinup_charge;             /* Fire type 6 charge, C1 ps+0x5c0. */
    const uint32_t *used_before; /* Optional native weapon bitset, WEAPON_BIT_WORDS words. */
    uint16_t clip_mask,reserve_mask;
    ammo_binding_t clips[16],reserves[16];
    uint32_t event_parameters[4]; /* Explicitly resolved CoDOL event parameters. */
    unsigned event_suppressed_slots; /* Physical primary-queue slots; zero = all visible. */
    /* The same, for C1's scripted queue (script_events.h). Resolved by
     * the server layer, which owns the compound mappings some events need,
     * and passed through untouched by this translation layer. */
    uint32_t script_event_parameters[PS_SCRIPT_EVENT_SLOTS];
    unsigned script_event_suppressed_slots; /* Physical slots; zero = all visible. */
    hud_resolver_t hud_resolve;
    void *hud_context;
    /* Section 5 compass icons, already translated. ps->objective[i].icon is a
     * native index relative to the retained CS_MATERIALS range (the value
     * G_MaterialIndex returned to objective_icon), but C1 reads the wire icon
     * as a slot of its own material bank, CS[4275 + icon] (0x453e72, called
     * from the compass drawer 0x4325fb and the on-screen drawer 0x432a96 with
     * objective+0x20). The server layer resolves each native index through the
     * CoDOL material bank, exactly as it does for hudelem materials; slots it
     * cannot resolve carry 0, the client's built-in fallback. When
     * objective_icons_bound is zero the native index is forwarded unchanged. */
    unsigned objective_icons_bound;
    uint16_t objective_icon[32];
    /* Objective slots 16..31, which the retained ps->objective[16] cannot
     * hold (C1 reads 32, 0x5a1f9c). Already filtered for the state's client
     * by FillHighObjectives; icons above are translated for all 32.
     * Zero high_objectives_bound leaves slots 16..31 free. */
    unsigned high_objectives_bound;
    objective_t high_objectives[16];
    /* Bit i: slot i is restricted by objective_player to another client
     * (ObjectiveHiddenMask), so this state carries it empty. Zero
     * leaves every slot to the team rule above. */
    uint32_t objective_hidden_mask;
} engine_player_bindings_t;
struct WeaponDef;
/* Build all fifteen ammo slots from owned native weapons, their runtime
 * indices, and replayed client pool keys. Clear dropped slots explicitly.
 * Reject conflicting aliases, missing definitions/mappings and overflow;
 * preserve all bindings on failure and unrelated bindings on success. */
int BuildEngineAmmoBindings(engine_player_bindings_t *,const playerState_t *,
    const weapon_registry_t *,struct WeaponDef *const definitions[WEAPON_SLOTS]);
/* Compose measured movement/mode, leg/torso indices, selection/phase/aim/grenade, health, inventory,
 * ammo, predictable events and HUD. All four outputs commit together or remain
 * byte-for-byte unchanged. Array masks retain their update semantics; callers
 * must explicitly request ammo-slot clears. Other player fields stay owned by
 * their producers: this is not a complete frame or a zero-filled substitute.
 * HUD is a complete packed state; empty source arrays clear old elements.
 * Resolver side effects are not part of the output transaction.
 * Clock belongs to the engine event generation, not a client's ACK history. */
int OverlayEnginePlayer(player_fields_t *,player_array_updates_t *,
    hud_state_t *,const playerState_t *,const engine_player_bindings_t *);
/* Same atomic composition using already-resolved HUD captured with this native
 * frame. Does not inspect ps->hud or call its resolver. Wire reachability remains
 * the serializer's responsibility; requires valid packed capture counts. */
int OverlayEnginePlayerWithHud(player_fields_t *,player_array_updates_t *,
    hud_state_t *,const playerState_t *,const engine_player_bindings_t *,const hud_state_t *);
#pragma pack(pop)
#pragma pack(pop)
#endif

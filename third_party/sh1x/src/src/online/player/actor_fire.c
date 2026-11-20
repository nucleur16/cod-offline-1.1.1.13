/* Coop actor weapon fire: the shot an AI soldier takes with the weapon in
 * his hands (coop category only; see actor_lifecycle.cpp
 * codol_actorShoot / codol_actorSetWeapon).
 *
 * WHY.  The coop soldiers used to fire with magicBullet(), which (like IW4's)
 * puts EV_FIRE_WEAPON on a temporary event entity at the muzzle point.  C1
 * draws nothing for it: CG_FireWeapon (0x481f50) returns at once when the
 * event entity's weapon word (nextState+d4) is 0, and the snapshot writer
 * never published that word for a temp event (MEASURED with
 * codol_traceFireEvents: "type 47->51 event 26 native weapon 29 wire weapon
 * 0").  Even with the word, a temp entity has no model, so C1's flash
 * placement (0x481b10, a switch on eType) has no arm for it.
 *
 * WHAT C1 DOES FOR AN ACTOR.  The same switch sends eType 0x12 (actor) to
 * 0x481bf8: the weapon's world flash effect (WeaponDef+0x360) played on the
 * actor's own DObj at the flash tag (0x444d80), and 0x481d60 plays the
 * weapon's fire sound from the actor.  C1's actor DObj builder (0x428b40)
 * reads the same weapon word (+0x1b8 of the centity = nextState+d4) and
 * rebuilds the actor's model when it changes.  So the IW4 SP shape is the
 * one C1 wants: the event on the actor entity, the actor's entity state
 * naming the weapon, the gun model attached to the actor (tag_flash), and
 * the bullet's impacts naming the actor as the shooter (C1 draws the tracer
 * from the shooter's flash tag).  IW4 Actor_Shoot / T5 Actor_ShootWeapon
 * do exactly that: Bullet_Fire with the actor as attacker and weapon
 * entity, then G_AddEvent( actor, EV_FIRE_WEAPON, 0 ).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "server.h"
#include "g_shared.h"
#include "bg.h"
#include "q_math.h"
#include "xassets/weapondef.h"
#include "actor_fire.h"
#include "actor_ammo.h"
#include "engine_state.h"
#include "sv_state.h"
#include <string.h>

/* The retained weaponParms layout Bullet_Fire reads (three axes, the muzzle,
 * the shot direction, the WeaponDef at +0x3c); the same local copy
 * script_magicbullet.c and script_turret_target.cpp keep. */
typedef struct
{
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    vec3_t start;
    vec3_t dir;
    struct WeaponDef *weapDef;
} actorFireParams_t;
typedef char actorFireParams_size_check[
    (sizeof(actorFireParams_t) == 0x40) ? 1 : -1];

extern void Bullet_Fire(gentity_t *attacker, float spread, void *fireParams,
                        gentity_t *weaponEnt, int time);
extern gentity_t *Weapon_GrenadeLauncher_Fire(gentity_t *, int, unsigned char, void *);
extern gentity_t *Weapon_RocketLauncher_FireScript(gentity_t *, int, float,
    void *, const float *, gentity_t *, const float *);
extern void __cdecl G_AddEvent(gentity_t *, int, int);

int ActorWeaponIndex(const char *name, const char **why)
{
    int index;
    struct WeaponDef *def;
    if (!name || !*name) { if (why) *why = "no weapon name"; return 0; }
    index = G_GetWeaponIndexForName(name);
    if (index <= 0) { if (why) *why = "unknown weapon"; return 0; }
    def = BG_GetWeaponDef(index);
    if (!def) { if (why) *why = "weapon has no definition"; return 0; }
    if (def->weapType != WEAPTYPE_BULLET && def->weapType != WEAPTYPE_PROJECTILE) {
        if (why) *why = "not a bullet or projectile weapon";
        return 0;
    }
    return index;
}

/* The snapshot overlay publishes an actor's weapon word only for a coop
 * match (src/online/snapshot/engine_state.c, the actor arm). */
static int coop_actor_weapon(const entityState_t *es)
{
    const char *zone = ServerZoneType();
    (void)es;
    return ZoneTypeRunsSoldiers(zone);
}

void ActorSetWeapon(gentity_t *actor, int weaponIndex)
{
    if (!actor || weaponIndex < 0) return;
    SetActorWeaponFilter(coop_actor_weapon);
    ActorAmmoEquip(actor,weaponIndex);
    actor->s.weapon = weaponIndex;
}

void ActorFireWeapon(gentity_t *actor, int weaponIndex, const float *start, const float *end)
{
    actorFireParams_t params;
    vec3_t dir;
    if (!actor || weaponIndex <= 0 || !start || !end) return;
    VectorSubtract(end, start, dir);
    Vec3NormalizeTo(dir, dir);
    VectorCopy(dir, params.forward);
    VectorClear(params.right);
    VectorClear(params.up);
    VectorCopy(start, params.start);
    VectorCopy(dir, params.dir);
    params.weapDef = BG_GetWeaponDef(weaponIndex);
    if (!params.weapDef || (params.weapDef->weapType != WEAPTYPE_BULLET &&
        params.weapDef->weapType != WEAPTYPE_PROJECTILE)) return;
    ActorSetWeapon(actor, weaponIndex);
    if(params.weapDef->weapType == WEAPTYPE_PROJECTILE) {
        gentity_t *missile;
        if(ActorAmmoClip(actor,weaponIndex)<=0)return;
        /* T5 FireWeapon dispatches grenade-class projectiles to the native
         * grenade launcher; all other projectile classes use the rocket path.
         * These retained launchers create real missile entities, with the
         * actor as owner/attacker and the composed WeaponDef's speed, gravity,
         * impact/explosion behavior. The script rocket entry omits only the
         * player recoil write. Neither launch requires actor->client. */
        if(params.weapDef->weapClass == WEAPCLASS_GRENADE)
            missile=Weapon_GrenadeLauncher_Fire(actor,weaponIndex,0,&params);
        else
            missile=Weapon_RocketLauncher_FireScript(actor,weaponIndex,0,
                &params,vec3_origin,NULL,NULL);
        if(!missile)return; /* A failed spawn spends no round or fire event. */
        ActorAmmoConsume(actor,weaponIndex);
        G_AddEvent(actor,EV_FIRE_WEAPON,0);
        return;
    }
    if(!ActorAmmoConsume(actor,weaponIndex))return;
    /* Attacker and weapon entity are the actor: its own capsule and hit
     * boxes are passed through, the damage callbacks see the soldier as the
     * attacker, and the impact events name him as the shooter. Spread 0:
     * the animscript has already chosen where the round goes. */
    Bullet_Fire(actor, 0.0f, &params, actor, level.time);
    G_AddEvent(actor, EV_FIRE_WEAPON, 0);
}
#endif

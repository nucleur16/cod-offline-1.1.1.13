/* Native actor magazines. SPDX-License-Identifier: AGPL-3.0-or-later
 * Magazine capacity comes from the composed native WeaponDef, not the AI's
 * family-based burst scheduler. Reserve ammunition is never part of a drop.
 * A magazine is refilled only by the completed reload animscript callback. */
#ifdef OL_SERVER
#include "g_shared.h"
#include "entity.h"
#include "bg.h"
#include "xassets/weapondef.h"
#include "actor_ammo.h"
#include <string.h>
typedef struct {int weapon,clip;} magazine_t;
typedef struct {int valid,generation;magazine_t magazines[2];} actor_ammo_t;
static actor_ammo_t ammo[MAX_GENTITIES];
static actor_ammo_t *record(gentity_t *actor,int create)
{
    actor_ammo_t *r;
    if(!actor || !actor->r.inuse || actor->client || actor->s.eType!=ET_ACTOR ||
       (unsigned)actor->s.number>=MAX_GENTITIES)return NULL;
    r=&ammo[actor->s.number];
    if(!r->valid || r->generation!=actor->useCount) {
        if(!create)return NULL;
        memset(r,0,sizeof(*r));r->valid=1;r->generation=actor->useCount;
    }
    return r;
}
static int capacity(int weapon)
{
    struct WeaponDef *def=weapon>0?BG_GetWeaponDef(weapon):NULL;
    return def && def->iClipSize>0?def->iClipSize:0;
}
void ActorAmmoReset(void){memset(ammo,0,sizeof(ammo));}
void ActorAmmoForget(gentity_t *actor)
{if(actor && (unsigned)actor->s.number<MAX_GENTITIES)memset(&ammo[actor->s.number],0,sizeof(ammo[0]));}
void ActorAmmoEquip(gentity_t *actor,int weapon)
{
    actor_ammo_t *r;struct WeaponDef *def;unsigned i;
    if(weapon<=0){ActorAmmoForget(actor);return;}
    r=record(actor,1);if(!r)return;
    for(i=0;i<2;i++)if(r->magazines[i].weapon==weapon)return;
    def=BG_GetWeaponDef(weapon);if(!def)return;
    r->magazines[0].weapon=weapon;r->magazines[0].clip=capacity(weapon);
    r->magazines[1].weapon=def->altWeaponIndex!=(unsigned)weapon?(int)def->altWeaponIndex:0;
    r->magazines[1].clip=capacity(r->magazines[1].weapon);
}
int ActorAmmoClip(gentity_t *actor,int weapon)
{
    actor_ammo_t *r=record(actor,0);unsigned i;
    if(r && weapon>0)for(i=0;i<2;i++)if(r->magazines[i].weapon==weapon)return r->magazines[i].clip;
    return 0;
}
int ActorAmmoConsume(gentity_t *actor,int weapon)
{
    actor_ammo_t *r=record(actor,0);unsigned i;
    if(r && weapon>0)for(i=0;i<2;i++)if(r->magazines[i].weapon==weapon && r->magazines[i].clip>0) {
        --r->magazines[i].clip;return 1;
    }
    return 0;
}
int ActorAmmoCapacity(gentity_t *actor)
{
    actor_ammo_t *r=record(actor,0);unsigned i;
    if(r)for(i=0;i<2;i++)if(r->magazines[i].weapon==actor->s.weapon && actor->s.weapon>0)return capacity(actor->s.weapon);
    return 0;
}
int ActorAmmoRemaining(gentity_t *actor)
{return actor?ActorAmmoClip(actor,actor->s.weapon):0;}
int ActorAmmoReloadComplete(gentity_t *actor)
{
    actor_ammo_t *r=record(actor,0);unsigned i;
    if(!r || actor->health<=0)return 0;
    for(i=0;i<2;i++)if(r->magazines[i].weapon==actor->s.weapon && actor->s.weapon>0) {
        r->magazines[i].clip=capacity(actor->s.weapon);return 1;
    }
    return 0;
}
#endif

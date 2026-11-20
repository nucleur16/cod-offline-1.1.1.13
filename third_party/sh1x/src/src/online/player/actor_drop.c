/* Survival soldier weapon drops. SPDX-License-Identifier: AGPL-3.0-or-later
 * T5 animscripts/shared::DropAllAIWeapons consumes the carried weapon before
 * the corpse transition; g_items.cpp ThrowDownWeapon supplies native item
 * physics, ownership and ring cleanup. Its randomized NPC ammunition is
 * replaced with the actual held magazines, with no reserve ammunition.
 * C1's item snapshot/pickup uses that same retained ET_ITEM representation. */
#ifdef OL_SERVER
#include "g_shared.h"
#include "entity.h"
#include "bg.h"
#include "cscr_stringlist.h"
#include "scr_vm.h"
#include "xassets/weapondef.h"
#include "xassets/xmodel.h"
#include "actor_drop.h"
#include "actor_ammo.h"
extern gentity_t *Drop_Weapon(gentity_t *,int,unsigned char,unsigned int);
extern int G_EntDetach(gentity_t *,const char *,unsigned int);
gentity_t *ActorDropWeapon(gentity_t *actor)
{
    struct WeaponDef *def;
    gentity_t *item;
    int weapon;
    if(!actor || !actor->r.inuse || actor->client || actor->s.eType!=ET_ACTOR)return NULL;
    weapon=actor->s.weapon;
    if(weapon<=0 || !(def=BG_GetWeaponDef(weapon)))return NULL;
    /* T5 ThrowDownWeapon normalizes the held alternate mode to its actual
     * carried primary item. Both magazines still come from this actor. */
    if(def->inventoryType==WEAPINVENTORY_ALTMODE) {
        weapon=(int)def->altWeaponIndex;
        if(weapon<=0 || !(def=BG_GetWeaponDef(weapon)))return NULL;
    }
    if((def->weapType!=WEAPTYPE_BULLET && def->weapType!=WEAPTYPE_PROJECTILE) ||
       (def->inventoryType!=WEAPINVENTORY_PRIMARY && def->inventoryType!=WEAPINVENTORY_EXCLUSIVE))return NULL;
    /* Model zero is the model supplied by getWeaponModel to the soldier's
     * tag_weapon_right attachment. Drop before detaching so the real tag
     * transform still exists for Drop_Weapon's placement trace. */
    item=Drop_Weapon(actor,weapon,0,scr_const.tag_weapon_right);
    if(!item)return NULL;
    item->item[0].index=weapon;
    item->item[0].ammoCount=0;
    item->item[0].clipAmmoCount=ActorAmmoClip(actor,weapon);
    item->item[1].index=def->altWeaponIndex!=(unsigned)weapon?(int)def->altWeaponIndex:0;
    item->item[1].ammoCount=0;
    item->item[1].clipAmmoCount=ActorAmmoClip(actor,item->item[1].index);
    if(def->worldModel[0] && def->worldModel[0]->name)
        G_EntDetach(actor,def->worldModel[0]->name,scr_const.tag_weapon_right);
    actor->s.weapon=0;
    ActorAmmoForget(actor);
    return item;
}
#endif

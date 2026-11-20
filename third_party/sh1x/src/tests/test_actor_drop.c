#include "g_shared.h"
#include "entity.h"
#include "bg.h"
#include "scr_vm.h"
#include "q_math.h"
#include "xassets/weapondef.h"
#include "xassets/xmodel.h"
#include "actor_drop.h"
#include "actor_ammo.h"
#include "actor_fire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
scr_const_t scr_const;
level_locals_t level;
static gentity_t actor,item;
static struct WeaponDef def,alt;
static XModel model;
static int drops,detaches,fail,bullets,events,grenades,rockets,launchfail;
static gentity_t missile;
vec3_t vec3_origin={0,0,0};
int G_GetWeaponIndexForName(const char *name){return !strcmp(name,"m79_mp")?17:!strcmp(name,"alt")?18:0;}
struct WeaponDef *BG_GetWeaponDef(int index){return index==17?&def:index==18?&alt:NULL;}
const char *ServerZoneType(void){return "survival";}
int ZoneTypeRunsSoldiers(const char *zone){return !strcmp(zone,"survival");}
void SetActorWeaponFilter(int (*filter)(const entityState_t *)){assert(filter(NULL));}
float Vec3NormalizeTo(const float *v,float *out){memcpy(out,v,12);return 1;}
void Bullet_Fire(gentity_t *attacker,float spread,void *parms,gentity_t *weaponEnt,int time)
{assert(attacker==&actor && weaponEnt==&actor && parms && spread==0);(void)time;++bullets;}
typedef struct {vec3_t forward,right,up,start,dir;struct WeaponDef *weapDef;} fireParams_t;
static void check_launch(gentity_t *ent,int weapon,void *p)
{
    fireParams_t *wp=p;
    assert(ent==&actor && !ent->client && weapon==17 && wp->weapDef==&def);
    assert(wp->forward[0]==1 && wp->dir[0]==1 && wp->start[0]==0);
    assert(ActorAmmoClip(ent,weapon)>0);
}
gentity_t *Weapon_GrenadeLauncher_Fire(gentity_t *ent,int weapon,unsigned char mode,void *p)
{check_launch(ent,weapon,p);assert(!mode);++grenades;return launchfail?NULL:&missile;}
gentity_t *Weapon_RocketLauncher_FireScript(gentity_t *ent,int weapon,float spread,
    void *p,const float *velocity,gentity_t *target,const float *offset)
{check_launch(ent,weapon,p);assert(!spread && !velocity[0] && !velocity[1] && !velocity[2] && !target && !offset);++rockets;return launchfail?NULL:&missile;}
void G_AddEvent(gentity_t *ent,int event,int param){assert(ent==&actor && event==EV_FIRE_WEAPON && !param);++events;}
gentity_t *Drop_Weapon(gentity_t *ent,int weapon,unsigned char variant,unsigned tag)
{
    assert(ent==&actor && weapon==17 && !variant && tag==123);
    assert((actor.s.weapon==17 || actor.s.weapon==18) && !detaches);++drops;
    memset(&item,0,sizeof(item));item.item[0].ammoCount=99;item.item[0].clipAmmoCount=0;
    item.item[1].ammoCount=55;item.item[1].clipAmmoCount=5;
    return fail?NULL:&item;
}
int G_EntDetach(gentity_t *ent,const char *name,unsigned tag)
{assert(ent==&actor && !strcmp(name,"world_model") && tag==123 && actor.s.weapon>0);++detaches;return 1;}
static void equip(void)
{
    ActorAmmoForget(&actor);actor.s.weapon=0;actor.health=100;detaches=0;
    ActorSetWeapon(&actor,17);
}
static void shot(void){const float start[3]={0},end[3]={1,0,0};ActorFireWeapon(&actor,17,start,end);}
static void check_drop(int clip,int altclip)
{
    int before=drops;actor.health=0;
    assert(ActorDropWeapon(&actor)==&item && drops==before+1 && !actor.s.weapon);
    assert(item.item[0].index==17 && item.item[0].clipAmmoCount==clip && !item.item[0].ammoCount);
    assert(item.item[1].index==(def.altWeaponIndex==17?0:(int)def.altWeaponIndex) && item.item[1].clipAmmoCount==altclip && !item.item[1].ammoCount);
    assert(!ActorDropWeapon(&actor) && drops==before+1);
}
int main(void)
{
    scr_const.tag_weapon_right=123;actor.s.number=100;actor.useCount=1;
    actor.r.inuse=1;actor.s.eType=ET_ACTOR;
    def.weapType=WEAPTYPE_BULLET;def.iClipSize=4;def.altWeaponIndex=18;
    alt.weapType=WEAPTYPE_PROJECTILE;alt.iClipSize=1;
    model.name="world_model";def.worldModel[0]=&model;
    equip();assert(ActorAmmoClip(&actor,17)==4);check_drop(4,1);
    equip();shot();shot();assert(bullets==2 && events==2);check_drop(2,1);
    equip();shot();shot();shot();shot();shot();assert(bullets==6 && events==6);check_drop(0,1);
    equip();shot();fail=1;
    assert(!ActorDropWeapon(&actor) && actor.s.weapon==17 && !detaches && ActorAmmoClip(&actor,17)==3);
    fail=0;check_drop(3,1);
    equip();shot();assert(ActorAmmoClip(&actor,17)==3);
    assert(ActorAmmoReloadComplete(&actor) && ActorAmmoClip(&actor,17)==4);
    assert(ActorAmmoConsume(&actor,18));check_drop(4,0);
    equip();shot();alt.inventoryType=WEAPINVENTORY_ALTMODE;alt.altWeaponIndex=17;
    ActorSetWeapon(&actor,18);assert(ActorAmmoConsume(&actor,18));check_drop(3,0);
    alt.inventoryType=WEAPINVENTORY_PRIMARY;alt.altWeaponIndex=0;
    def.altWeaponIndex=0;equip();check_drop(4,0);
    def.altWeaponIndex=17;equip();check_drop(4,0); /* A self-reference is not a second magazine. */
    def.altWeaponIndex=18;
    equip();shot();actor.health=0;
    assert(!ActorAmmoReloadComplete(&actor) && ActorAmmoClip(&actor,17)==3);check_drop(3,1);
    equip();shot();actor.useCount++;
    assert(ActorAmmoClip(&actor,17)==0);ActorSetWeapon(&actor,17);check_drop(4,1);
    equip();def.inventoryType=WEAPINVENTORY_OFFHAND;
    assert(!ActorDropWeapon(&actor));def.inventoryType=WEAPINVENTORY_PRIMARY;
    ActorAmmoReset();assert(ActorAmmoClip(&actor,17)==0);
    def.iClipSize=32;equip();
    assert(ActorAmmoCapacity(&actor)==32 && ActorAmmoRemaining(&actor)==32);
    shot();assert(ActorAmmoRemaining(&actor)==31);
    assert(ActorAmmoReloadComplete(&actor) && ActorAmmoRemaining(&actor)==32);
    /* Actual projectile producer dispatch, not bullet substitution. */
    def.weapType=WEAPTYPE_PROJECTILE;def.weapClass=WEAPCLASS_GRENADE;def.iClipSize=1;
    {const char *why=NULL;assert(ActorWeaponIndex("m79_mp",&why)==17 && !why);}
    equip();{int before=events,b=bullets;shot();shot();
        assert(grenades==1 && !rockets && events==before+1 && bullets==b);
        assert(ActorAmmoRemaining(&actor)==0);
        assert(ActorAmmoReloadComplete(&actor));shot();assert(grenades==2);}
    equip();launchfail=1;{int before=events;shot();
        assert(grenades==3 && events==before && ActorAmmoRemaining(&actor)==1);}
    launchfail=0;shot();assert(grenades==4 && !ActorAmmoRemaining(&actor));
    def.weapClass=WEAPCLASS_ROCKETLAUNCHER;equip();shot();
    assert(rockets==1 && !ActorAmmoRemaining(&actor));check_drop(0,1);
    def.weapType=WEAPTYPE_GRENADE;
    {const char *why=NULL;assert(!ActorWeaponIndex("m79_mp",&why) && why);}
    puts("PASS: production firing consumes actual magazines; full/partial/empty drops have zero reserve, alt clips preserved, failed/interrupted reloads and slot reuse safe");
}

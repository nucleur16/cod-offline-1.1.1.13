/* Native missile recovery with explicit engine seams. This fixture does not
 * establish that a C1 client can throw, touch, or render the projectile. */
#include "g_shared.h"
#include "bg_public.h"
#include "scr_vm.h"
#include "missile_pickup.h"
#include "knife_pickup.h"
#include "weapon_slots.h"
#include "perk_query.h"
#include "missile_stickiness.h"
#include "bolt_attachment.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

level_locals_t level;
scr_const_t scr_const;
static gentity_t player, missile;
static gclient_t client;
static struct WeaponDef definitions[4];
static int owned[4], compatible[4], capacity[4];
static unsigned classes[4];
static int ammo_calls, predictable_events, normal_events, notifications, frees;
static int event_weapon, event_type, nextthink_at_trigger;
static int trigger_delete, trigger_reuse, trigger_convert, trigger_reindex;
static int death_delete, death_reuse, death_convert, death_reindex;
static int engineer, perk_queries;
static int stickiness[4], bolt_indicator;

int GetNativeWeaponMissileStickiness(const struct WeaponDef *def,
    missile_stickiness_t *out)
{
    int i;
    for(i=1;i<BG_GetNumWeapons();++i) if(def==&definitions[i]) {
        out->mode=(missile_stickiness_mode_t)stickiness[i];
        out->stick_to_player_or_actor=0;
        return 1;
    }
    return 0;
}
int IsBoltIndicator(const gentity_t *ent)
{
    return bolt_indicator && ent==&missile;
}

int PlayerHasNamedPerk(const playerState_t *ps,const char *name)
{
    assert(ps==&client.ps && !strcmp(name,"specialty_engineer"));
    ++perk_queries;
    return engineer;
}

int BG_GetNumWeapons(void) { return 4; }
struct WeaponDef *BG_GetWeaponDef(int weapon)
{
    assert(weapon >= 0 && weapon < BG_GetNumWeapons());
    return &definitions[weapon];
}
int GetNativeWeaponClass(const struct WeaponDef *def,unsigned *value)
{
    int i;
    for(i=1;i<BG_GetNumWeapons();++i) if(def==&definitions[i]) {
        *value=classes[i];
        return 1;
    }
    return 0;
}
int WeaponBitsOwned(const playerState_t *ps,unsigned weapon)
{
    assert(ps==&client.ps && weapon<4);
    return owned[weapon];
}
unsigned char BG_PlayerHasCompatibleWeapon(const playerState_t *ps,unsigned weapon)
{
    assert(ps==&client.ps && weapon<4);
    return compatible[weapon];
}
int BG_GetMaxPickupableAmmo(const playerState_t *ps,unsigned weapon)
{
    assert(ps==&client.ps && weapon<4);
    return capacity[weapon];
}
int Add_Ammo(gentity_t *recipient,unsigned weapon,unsigned char variant,int count,int fillClip)
{
    assert(recipient==&player && weapon<4 && count==1 && fillClip==1);
    assert(variant==3);
    ++ammo_calls;
    return capacity[weapon]>0 && (owned[weapon] || compatible[weapon]);
}
void G_AddPredictableEvent(gentity_t *recipient,int type,int weapon)
{
    assert(recipient==&player);
    ++predictable_events;
    event_type=type;
    event_weapon=weapon;
}
void G_AddEvent(gentity_t *recipient,int type,int weapon)
{
    assert(recipient==&player);
    ++normal_events;
    event_type=type;
    event_weapon=weapon;
}
void Scr_AddUndefined(void) {}
void Scr_AddEntity(gentity_t *recipient) { assert(recipient==&player); }
void Scr_Notify(gentity_t *recipient,unsigned short name,unsigned int count)
{
    assert(recipient==&missile);
    if(name==scr_const.trigger) {
        assert(count==2 && notifications==0);
        nextthink_at_trigger=missile.nextthink;
        if(trigger_delete) missile.r.inuse=0;
        if(trigger_reuse) ++missile.useCount;
        if(trigger_convert) missile.s.eType=ET_GENERAL;
        if(trigger_reindex) missile.s.index=2;
    } else {
        assert(name==scr_const.death && count==0 && notifications==1);
        if(death_delete) missile.r.inuse=0;
        if(death_reuse) ++missile.useCount;
        if(death_convert) missile.s.eType=ET_GENERAL;
        if(death_reindex) missile.s.index=2;
    }
    ++notifications;
}
void G_FreeEntity(gentity_t *recipient)
{
    assert(recipient==&missile);
    ++frees;
    missile.r.inuse=0;
}
qboolean G_GetHintStringIndex(int *index,const char *text)
{
    assert(!strcmp(text,"WEAPON_PICKUP_GENERIC\024Equipment"));
    *index=27;
    return qtrue;
}
static void reset(unsigned weapon)
{
    memset(&player,0,sizeof(player));
    memset(&missile,0,sizeof(missile));
    memset(&client,0,sizeof(client));
    memset(owned,0,sizeof(owned));
    memset(compatible,0,sizeof(compatible));
    memset(capacity,0,sizeof(capacity));
    ammo_calls=predictable_events=normal_events=notifications=frees=0;
    event_weapon=event_type=nextthink_at_trigger=-1;
    trigger_delete=trigger_reuse=trigger_convert=trigger_reindex=0;
    death_delete=death_reuse=death_convert=death_reindex=0;
    level.time=100;
    scr_const.trigger=11;
    scr_const.death=12;
    player.r.inuse=1;
    player.client=&client;
    player.health=100;
    player.s.number=1;
    missile.r.inuse=1;
    missile.s.eType=ET_MISSILE;
    missile.s.index=(int)(weapon+3*ITEM_STRIDE);
    missile.useCount=5;
    missile.nextthink=200;
    missile.r.ownerNum.number=player.s.number+1;
    owned[weapon]=1;
    capacity[weapon]=1;
}
static void no_transfer(unsigned weapon)
{
    assert(RecoverMissileAmmo(&player,&definitions[weapon],weapon,3,&missile)==0);
    assert(notifications==0 && frees==0);
}
int main(void)
{
    classes[1]=9;
    classes[2]=2;
    classes[3]=2;
    definitions[2].timedDetonation=1;
    definitions[2].offhandClass=2;
    definitions[2].szDisplayName="Equipment";
    definitions[3].timedDetonation=0;
    definitions[3].offhandClass=2;

    reset(1);
    assert(IsKnifeEntity(&missile));
    assert(IsInventoryMissile(&missile));
    assert(InventoryMissileCanBeGrabbed(&client.ps,1)==1);
    assert(InventoryMissileHint(&missile)==-1);
    assert(RecoverMissileAmmo(&player,&definitions[1],1,3,&missile)==2);
    assert(ammo_calls==1 && notifications==2 && frees==1);
    assert(normal_events==1 && predictable_events==0 && event_type==10 && event_weapon==1);
    assert(nextthink_at_trigger==0);

    reset(1);
    owned[1]=0; compatible[1]=0;
    assert(InventoryMissileCanBeGrabbed(&client.ps,1)==0);
    no_transfer(1);
    reset(1);
    owned[1]=0; compatible[1]=1;
    assert(InventoryMissileCanBeGrabbed(&client.ps,1)==1);
    assert(RecoverMissileAmmo(&player,&definitions[1],1,3,&missile)==2);
    assert(ammo_calls==1);
    reset(1);
    capacity[1]=0;
    no_transfer(1);
    assert(ammo_calls==1);
    reset(1);
    missile.s.index=2+3*ITEM_STRIDE;
    no_transfer(1);
    assert(ammo_calls==0);
    reset(1);
    client.sess.predictItemPickup=1;
    trigger_convert=1;
    assert(RecoverMissileAmmo(&player,&definitions[1],1,3,&missile)==2);
    assert(ammo_calls==1 && notifications==1 && frees==0 && predictable_events==1);

    reset(2);
    assert(!IsKnifeEntity(&missile) && IsInventoryMissile(&missile));
    assert(InventoryMissileHint(&missile)==27);
    assert(InventoryMissileUseAllowed(&player,&missile));
    client.sess.predictItemPickup=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==2 && frees==1 && predictable_events==1);
    assert(normal_events==0 && event_weapon==2 && nextthink_at_trigger==0);

    reset(2);
    owned[2]=0; compatible[2]=1;
    assert(InventoryMissileCanBeGrabbed(&client.ps,2)==0);
    no_transfer(2);
    reset(2);
    missile.r.ownerNum.number=9;
    no_transfer(2);
    assert(ammo_calls==0);
    reset(2);
    missile.nextthink=level.time;
    no_transfer(2);
    assert(ammo_calls==0);
    reset(2);
    capacity[2]=0;
    no_transfer(2);
    assert(ammo_calls==0);
    reset(2);
    player.health=0;
    no_transfer(2);
    assert(ammo_calls==0);
    reset(3);
    assert(!IsInventoryMissile(&missile));
    assert(InventoryMissileCanBeGrabbed(&client.ps,3)==-1);
    assert(RecoverMissileAmmo(&player,&definitions[3],3,3,&missile)==-1);

    reset(2);
    trigger_delete=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==1 && frees==0);
    reset(2);
    trigger_reuse=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==1 && frees==0);
    reset(2);
    trigger_convert=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==1 && frees==0 && missile.r.inuse);
    reset(2);
    trigger_reindex=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==1 && frees==0 && missile.r.inuse);

    reset(2);
    death_delete=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==2 && frees==0);
    reset(2);
    death_reuse=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==2 && frees==0);
    reset(2);
    death_convert=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==2 && frees==0 && missile.r.inuse);
    reset(2);
    death_reindex=1;
    assert(RecoverMissileAmmo(&player,&definitions[2],2,3,&missile)==2);
    assert(ammo_calls==1 && notifications==2 && frees==0 && missile.r.inuse);

    /* Throwback pickup fuse: the remainder carries over unless Engineer
     * resets it to the definition's full fuse; never shortened. */
    assert(ThrowbackFuseLeft(800,3500,0)==800);
    assert(ThrowbackFuseLeft(800,3500,1)==3500);
    assert(ThrowbackFuseLeft(4000,3500,1)==4000);
    definitions[2].fuseTime=3500;
    reset(2);
    client.ps.throwBackGrenadeTimeLeft=1200;
    engineer=0; perk_queries=0;
    assert(ThrowbackGrenadeTimeLeft(&player,&missile)==1200 && perk_queries==1);
    engineer=1;
    assert(ThrowbackGrenadeTimeLeft(&player,&missile)==3500);
    engineer=-1; /* unknown perk table row: no reset */
    assert(ThrowbackGrenadeTimeLeft(&player,&missile)==1200);
    engineer=1;
    missile.s.eType=ET_GENERAL; /* not a live missile: remainder only */
    assert(ThrowbackGrenadeTimeLeft(&player,&missile)==1200);
    player.client=NULL;
    assert(ThrowbackGrenadeTimeLeft(&player,&missile)==0);

    /* Live throwback candidates: a plain frag stays one; a sticky grenade
     * (zm semtex, C1 stickiness +0x7c8 nonzero) and a bolt's danger-indicator
     * companion never are. */
    reset(3);
    missile.s.weapon=3;
    assert(InventoryMissileUseAllowed(&player,&missile)==1);
    stickiness[3]=STICK_ALL;
    assert(InventoryMissileUseAllowed(&player,&missile)==0);
    stickiness[3]=STICK_NONE;
    bolt_indicator=1;
    assert(InventoryMissileUseAllowed(&player,&missile)==0);
    bolt_indicator=0;
    /* A thrown knife is C1 stickiness 5 (IW4 WEAPSTICKINESS_KNIFE) and is
     * no throwback grenade: it stays a use candidate and is recovered,
     * settled (no fuse) and by a player who did not throw it. */
    reset(1);
    missile.s.weapon=1;
    stickiness[1]=STICK_FLESH;
    missile.nextthink=0;
    missile.r.ownerNum.number=9;
    assert(InventoryMissileUseAllowed(&player,&missile)==1);
    assert(RecoverMissileAmmo(&player,&definitions[1],1,3,&missile)==2);
    assert(ammo_calls==1 && notifications==2 && frees==1);
    reset(1);
    missile.s.weapon=1;
    capacity[1]=0;
    no_transfer(1);
    stickiness[1]=STICK_NONE;
    /* The owned special-equipment recovery path keeps its own rules. */
    reset(2);
    missile.s.weapon=2;
    stickiness[2]=STICK_ALL;
    assert(InventoryMissileUseAllowed(&player,&missile)==1);
    stickiness[2]=STICK_NONE;

    puts("PASS: class-9 and timed missile recovery guards transfer and script-mutated lifecycle; Engineer throwback fuse reset; sticky grenades and bolt indicators are never thrown back; stuck knives stay recoverable");
    return 0;
}

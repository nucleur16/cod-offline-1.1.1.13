/* Native recovery of deployed equipment; class1 keeps C1 live throwback. */
#ifdef OL_SERVER
#include "missile_pickup.h"
#include "knife_pickup.h"
#include "weapon_native.h"
#include "g_shared.h"
#include "bg_public.h"
#include "weapon_bits.h"
#include "weapon_slots.h"
#include "perk_query.h"
#include "missile_stickiness.h"
#include "bolt_attachment.h"
#include <stdio.h>
#include "scr_vm.h"
extern void G_AddPredictableEvent(gentity_t *,int,int);
extern void G_AddEvent(gentity_t *,int,int);
extern int Add_Ammo(gentity_t *,unsigned int,unsigned char,int,int);
extern int BG_GetMaxPickupableAmmo(const playerState_t *,unsigned);
extern qboolean G_GetHintStringIndex(int *,const char *);

static int special_definition(const struct WeaponDef *def)
{
    unsigned source_class;
    if(!def || !GetNativeWeaponClass(def,&source_class) || source_class==9)
        return 0;
    /* +be2 timedDetonation and +354 offhandClass are independently loaded
     * from the C1 header. Untimed C4/Betty/scrambler keep their device paths. */
    return def->timedDetonation && (def->offhandClass==2 ||
        def->offhandClass==3 || def->offhandClass==5);
}
static unsigned missile_weapon(const gentity_t *ent)
{
    unsigned weapon;
    if(!ent || !ent->r.inuse || ent->s.eType!=ET_MISSILE)return 0;
    weapon=ItemIndexWeapon(ent->s.index);
    return weapon && weapon<(unsigned)BG_GetNumWeapons()?weapon:0;
}
int IsInventoryMissile(const gentity_t *ent)
{
    unsigned weapon=missile_weapon(ent);
    return weapon && (IsKnifeDefinition(BG_GetWeaponDef(weapon)) ||
        special_definition(BG_GetWeaponDef(weapon)));
}
int InventoryMissileCanBeGrabbed(const playerState_t *ps,unsigned weapon)
{
    int knife=KnifeCanBeGrabbed(ps,weapon);
    if(knife!=-1)return knife;
    if(!weapon || weapon>=(unsigned)BG_GetNumWeapons() ||
       !special_definition(BG_GetWeaponDef(weapon)))return -1;
    /* Exact current inventory identity, never grant a different class item
     * merely because it shares an ammo pool. */
    if(!ps || weapon>=WEAPON_SLOTS ||
       !WeaponBitsOwned(ps,weapon))return 0;
    return BG_GetMaxPickupableAmmo(ps,weapon)>0;
}
/* A sticky grenade is never thrown back. T5 offers the live pickup only for
 * a definition with bThrowBack (OpenBLOPS g_active_mp.cpp IsLiveGrenade,
 * g_missile.cpp G_InitGrenadeEntity), which its sticky grenades lack; IW4's
 * Semtex cannot be thrown back either. C1 has no such flag: its
 * G_InitGrenadeEntity (0x530654) makes every offhand class 1 or 4 grenade a
 * pickup item, and zm's semtex_grenade_zm is class 1 (C1 MP's semtex_mp is
 * class 5, which never reaches the throwback path). The C1 field that
 * separates them is stickiness (+0x7c8): semtex 1, frag 0. */
static int sticky_grenade(const gentity_t *ent)
{
    missile_stickiness_t metadata;
    const struct WeaponDef *def;
    if(!ent || !ent->r.inuse || ent->s.eType!=ET_MISSILE || !ent->s.weapon ||
       ent->s.weapon>=(unsigned)BG_GetNumWeapons() || !(def=BG_GetWeaponDef(ent->s.weapon)))
        return 0;
    return GetNativeWeaponMissileStickiness(def,&metadata) &&
        metadata.mode!=STICK_NONE;
}
int InventoryMissileUseAllowed(const gentity_t *player,const gentity_t *ent)
{
    unsigned weapon=missile_weapon(ent);
    /* The cursor-hint candidate filter (_player_use_mp.asm): C1 draws the
     * throwback prompt and icon from that hint (ps cursorHint and its entity),
     * and the throwback event acts on the same entity, so a refused candidate
     * is neither offered nor taken. A bolt's danger-indicator companion is
     * never a candidate at all. */
    if(IsBoltIndicator(ent))return 0;
    /* A thrown knife is stickiness 5 (IW4 WEAPSTICKINESS_KNIFE) but is no
     * throwback grenade: C1 528292 recovers it through the item pickup path
     * and its UI (4561e1..45621d) gives class 9 the ordinary pickup hint.
     * Ammo capacity is checked by the candidate's BG_CanItemBeGrabbed and
     * again by the transfer itself (RecoverKnifeAmmo). The sticky
     * test below excluded it from the use candidates and refused the touch
     * and use transfer (RecoverMissileAmmo), so no knife could be
     * picked up at all. */
    if(weapon && IsKnifeDefinition(BG_GetWeaponDef(weapon)))return 1;
    if(!weapon || !special_definition(BG_GetWeaponDef(weapon)))
        return !sticky_grenade(ent);
    return player && player->r.inuse && player->client && player->health>0 &&
        ent->r.ownerNum.number==(unsigned)player->s.number+1 &&
        ent->nextthink>level.time &&
        InventoryMissileCanBeGrabbed(&player->client->ps,weapon)==1;
}
int RecoverMissileAmmo(gentity_t *player,const struct WeaponDef *def,
    unsigned weapon,unsigned variant,gentity_t *missile)
{
    int knife=IsKnifeDefinition(def);
    if(!knife && !special_definition(def))return -1;
    if(missile_weapon(missile)!=weapon || !player || !player->r.inuse ||
       !player->client || player->health<=0 ||
       !InventoryMissileUseAllowed(player,missile))return 0;
    {
        int generation=missile->useCount;
        unsigned item_index=missile->s.index;
        if(knife) {
            if(RecoverKnifeAmmo(player,def,weapon,variant)<=0)return 0;
        } else if(Add_Ammo(player,weapon,(unsigned char)variant,1,1)<=0)return 0;
        /* Cancel the fuse before script notification. Waiters may delete,
         * reuse, or convert this slot; only the same pickup missile may get
         * the death notification and final free below. */
        missile->nextthink=0;
        if(player->client->sess.predictItemPickup)
            G_AddPredictableEvent(player,10,(int)weapon);
        else if(knife)
            G_AddEvent(player,10,(int)weapon);
        Scr_AddUndefined();
        Scr_AddEntity(player);
        Scr_Notify(missile,scr_const.trigger,2);
        if(!missile->r.inuse || missile->useCount!=generation ||
           missile->s.eType!=ET_MISSILE || missile->s.index!=item_index)return 2;
        Scr_Notify(missile,scr_const.death,0);
        if(missile->r.inuse && missile->useCount==generation &&
           missile->s.eType==ET_MISSILE && missile->s.index==item_index)
            G_FreeEntity(missile);
        return 2; /* World lifecycle completed; Touch_Item must return now. */
    }
}
int ThrowbackFuseLeft(int timeLeft,int fuseTime,int resetFuse)
{
    return resetFuse && fuseTime>timeLeft ? fuseTime : timeLeft;
}
int ThrowbackGrenadeTimeLeft(const gentity_t *player,const gentity_t *missile)
{
    const playerState_t *ps;
    const struct WeaponDef *def;
    unsigned weapon;
    if(!player || !player->client) return 0;
    ps=&player->client->ps;
    weapon=missile_weapon(missile);
    if(!weapon || !(def=BG_GetWeaponDef(weapon))) return ps->throwBackGrenadeTimeLeft;
    /* coPerkStatsTable row 90 specialty_engineer (perk2) carries the reset;
     * its Pro row 91 only adds stun/flash protection. No ability modifier
     * row names it, so the selected perk itself is the source. The full
     * fuse is the definition's (C1 def+0x654, frag 3500 ms), the same value
     * a fresh pull-back seeds at C1 0x41c55b. */
    return ThrowbackFuseLeft(ps->throwBackGrenadeTimeLeft,def->fuseTime,
        PlayerHasNamedPerk(ps,"specialty_engineer")==1);
}
int InventoryMissileHint(const gentity_t *ent)
{
    unsigned weapon=missile_weapon(ent);
    const struct WeaponDef *def;
    char text[1024];
    int index,n;
    if(!weapon || !special_definition(def=BG_GetWeaponDef(weapon)))return -1;
    if(!def->szDisplayName || !def->szDisplayName[0])return -2;
    /* Shipped localized_code_post_gfx_mp asset2564: localized pickup with
     * &&1 substitution. 0x14 separates localized message arguments. */
    n=snprintf(text,sizeof(text),"WEAPON_PICKUP_GENERIC%c%s",20,def->szDisplayName);
    if(n<0 || n>=(int)sizeof(text) || !G_GetHintStringIndex(&index,text))return -2;
    return index;
}
#endif

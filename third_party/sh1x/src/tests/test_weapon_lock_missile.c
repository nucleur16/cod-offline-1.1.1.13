#include "g_shared.h"
#include "bg_public.h"
#include "weapon_lock.h"
#include "qcommon_io.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
gentity_t g_entities[MAX_GENTITIES];
gentity_t *ApplyRocketWeaponLock(gentity_t *,gentity_t *);
_Static_assert(offsetof(gentity_t,missile.targetOffset)==0x1dc,"retained target-offset ABI");
_Static_assert(offsetof(gentity_t,missile.stage)==0x1e8,"retained missile-stage ABI");
_Static_assert(offsetof(gentity_t,missile.flightMode)==0x1ec,"retained missile-flight ABI");
_Static_assert(offsetof(gentity_t,missileTargetEnt)==0x214,"retained target handle ABI");
/* The lock lives in the playerState extension now, so the harness supplies the
 * live storage its resolver walks: clients[0].ps is client 0's state, and the
 * snapshot and archive classes are absent here. */
static gclient_t clients[1];
const void *PlayerStateExtLiveBase(unsigned *count){if(count)*count=1;return clients;}
const void *PlayerStateExtSnapshotBase(unsigned *count){if(count)*count=0;return NULL;}
const void *PlayerStateExtArchiveBase(unsigned *count){if(count)*count=0;return NULL;}
/* playerstate_ext.c reports untracked storage through this hook. */
void Com_PrintWarning(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
/* Weapon 83 stands for a guidance-2 (predator) weapon, anything else for an
 * unguided rocket; the launch stamps es+0x80's owner only for the former. */
struct WeaponDef *BG_GetWeaponDef(int weapon)
{
    static struct WeaponDef def;
    def.guidedMissileType=weapon==83 ? MISSILE_GUIDANCE_HELLFIRE : MISSILE_GUIDANCE_NONE;
    return &def;
}
int main(void)
{
    gentity_t missile={0},before;float offset[]={100,0,-32},point[]={1,2,3};
    gentity_t *owner=&g_entities[0],*target=&g_entities[17];
    missile.s.number=20;
    owner->client=&clients[0];target->r.inuse=1;
    assert(SetWeaponLock(ClientWeaponLock(0),17,NULL,1,offset,0));
    assert(ApplyRocketWeaponLock(owner,&missile)==target);
    assert(!memcmp(missile.missile.targetOffset,offset,sizeof(offset)));
    assert(missile.missile.flightMode==MISSILEFLIGHTMODE_DIRECT); /* topAttack false: bit 8 */
    assert(SetWeaponLock(ClientWeaponLock(0),17,NULL,1,NULL,1));
    assert(ApplyRocketWeaponLock(owner,&missile)==target);
    assert(missile.missile.flightMode==MISSILEFLIGHTMODE_TOP && !missile.missile.targetOffset[0]); /* bit 4 */
    before=missile;target->r.inuse=0;
    assert(!ApplyRocketWeaponLock(owner,&missile) && !memcmp(&before,&missile,sizeof(missile)));
    target->r.inuse=1;
    assert(SetWeaponLock(ClientWeaponLock(0),17,point,1,offset,0));
    assert(!ApplyRocketWeaponLock(owner,&missile));
    assert(missile_position_active[20] && !memcmp(missile_positions[20],point,sizeof(point)));
    assert(!missile.missile.targetOffset[0] && !missile.missile.targetOffset[2]);
    assert(missile.missile.flightMode==MISSILEFLIGHTMODE_TOP); /* Retail retains the prior top-attack bit. */
    FreeWeaponLock(ClientWeaponLock(0));
    assert(SetWeaponLock(ClientWeaponLock(0),17,point,1,offset,0));
    assert(!ApplyRocketWeaponLock(owner,&missile));
    assert(missile.missile.flightMode==MISSILEFLIGHTMODE_DIRECT);
    assert(SetWeaponLock(ClientWeaponLock(0),17,point,1,offset,1));
    ResetClientWeaponLock(0);
    assert(missile_position_active[20]); /* Launched missile owns its target. */
    assert(!ApplyRocketWeaponLock(owner,&missile));
    assert(!missile_position_active[20]); /* Reused launch slot clears old point. */
    missile_position_active[20]=1;ResetMissilePosition(20);
    assert(!missile_position_active[20]);
    missile_position_active[21]=1;ResetMissilePositions();
    assert(!missile_position_active[21]);
    /* Compass owner (C1 0x5310ce): guidance 2 stamps the parent's entity
     * number into otherEntityNum, any other weapon clears it. */
    ResetClientWeaponLock(0);
    missile.s.weapon=83;missile.s.otherEntityNum=9;
    ApplyRocketWeaponLock(owner,&missile);
    assert(missile.s.otherEntityNum==owner->s.number && missile.s.clientNum==owner->s.number);
    missile.s.weapon=7;missile.s.otherEntityNum=9;
    ApplyRocketWeaponLock(owner,&missile);
    assert(missile.s.otherEntityNum==0);
    missile.s.weapon=83;missile.s.otherEntityNum=9;
    ApplyRocketWeaponLock(NULL,&missile);
    assert(missile.s.otherEntityNum==0);
    puts("PASS: missile ABI, entity offset/mode (bit 4 = top attack), absolute position ownership, slot reuse and reset, compass owner stamp");
}

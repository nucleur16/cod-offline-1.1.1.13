#include "g_shared.h"
#include "bg.h"
#include "weapon_native.h"
#include "weapon_lock.h"
#include "qcommon_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int WeaponLockFireEvent(const playerState_t *,const struct WeaponDef *);
static int warning_gate;
int GetNativeWeaponLockWarningGate(const struct WeaponDef *weapon,int *out)
{ (void)weapon;*out=warning_gate;return 1; }
/* The lock lives in the playerState extension now, so the harness supplies the
 * live storage its resolver walks: clients[0].ps is client 0's state, and the
 * snapshot and archive classes are absent here. */
static gclient_t clients[1];
const void *PlayerStateExtLiveBase(unsigned *count){if(count)*count=1;return clients;}
const void *PlayerStateExtSnapshotBase(unsigned *count){if(count)*count=0;return NULL;}
const void *PlayerStateExtArchiveBase(unsigned *count){if(count)*count=0;return NULL;}
/* playerstate_ext.c reports untracked storage through this hook. */
void Com_PrintWarning(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
int main(void)
{
    unsigned char bytes[0xbfc]={0};struct WeaponDef weapon={0},before;
    mounted_weapon_t source={0};playerState_t *ps=&clients[0].ps;
    source.asset.type=28;source.asset.data=bytes;source.asset.size=sizeof(bytes);
    bytes[0xbbe]=1;
    assert(ApplyNativeWeaponLockRequirement(&source,&weapon) && weapon.requireLockonToFire);
    assert(WeaponLockFireEvent(ps,&weapon)==EV_LOCKON_REQUIRED_HINT);
    assert(SetWeaponLock(ClientWeaponLock(0),17,NULL,0,NULL,0));
    assert(WeaponLockFireEvent(ps,&weapon)==EV_LOCKON_REQUIRED_HINT);
    assert(SetWeaponLock(ClientWeaponLock(0),17,NULL,1,NULL,0));
    assert(!WeaponLockFireEvent(ps,&weapon));
    warning_gate=1;
    assert(SetWeaponLockWarning(ClientWeaponLock(0),0x10,1));
    assert(WeaponLockFireEvent(ps,&weapon)==EV_TARGET_TOO_CLOSE_HINT);
    assert(SetWeaponLockWarning(ClientWeaponLock(0),0x20,1));
    assert(WeaponLockFireEvent(ps,&weapon)==EV_TARGET_TOO_CLOSE_HINT);
    assert(SetWeaponLockWarning(ClientWeaponLock(0),0x10,0));
    assert(WeaponLockFireEvent(ps,&weapon)==EV_TARGET_NOT_ENOUGH_CLEARANCE);
    FreeWeaponLock(ClientWeaponLock(0));
    assert(WeaponLockFireEvent(ps,&weapon)==EV_LOCKON_REQUIRED_HINT);
    bytes[0xbbe]=0;assert(ApplyNativeWeaponLockRequirement(&source,&weapon));
    /* Clearance blocks even without a finalized lock; proximity does not. */
    assert(WeaponLockFireEvent(ps,&weapon)==EV_TARGET_NOT_ENOUGH_CLEARANCE);
    assert(SetWeaponLockWarning(ClientWeaponLock(0),0x20,0));
    assert(SetWeaponLockWarning(ClientWeaponLock(0),0x10,1));
    assert(!WeaponLockFireEvent(ps,&weapon));
    warning_gate=0;
    assert(SetWeaponLockWarning(ClientWeaponLock(0),0x20,1));
    assert(!WeaponLockFireEvent(ps,&weapon));
    before=weapon;bytes[0xbbe]=2;
    assert(!ApplyNativeWeaponLockRequirement(&source,&weapon) && !memcmp(&before,&weapon,sizeof(weapon)));
    puts("PASS: mounted lock requirement -> native firing gate; start/finalize/free transitions and atomic invalid flag");
}

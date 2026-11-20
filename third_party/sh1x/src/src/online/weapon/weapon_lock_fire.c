#ifdef OL_SERVER
#include "g_shared.h"
#include "bg.h"
#include "xassets/weapondef.h"
#include "weapon_lock.h"
#include "weapon_native.h"
/* Returns a retained predictable event, or zero to continue firing.
 * Called after weapon delay expires, before ammunition consumption. */
int WeaponLockFireEvent(const playerState_t *ps,const struct WeaponDef *weapon)
{
    const weapon_lock_t *lock;int warnings=0;
    if(!weapon)return 0;
    GetNativeWeaponLockWarningGate(weapon,&warnings);
    if(!weapon->requireLockonToFire && !warnings)return 0;
    if(!ps)return EV_LOCKON_REQUIRED_HINT;
    /* The gate reads the lock of the state that is firing, not of whatever
     * client that state names: server pmove runs on the shooter's own live
     * playerState, and an untracked one reads as unlocked. */
    lock=PlayerStateWeaponLockRead(ps);
    if(weapon->requireLockonToFire && !(lock->flags&2))return EV_LOCKON_REQUIRED_HINT;
    if(warnings) {
        if((lock->flags&0x12)==0x12)return EV_TARGET_TOO_CLOSE_HINT;
        if(lock->flags&0x20)return EV_TARGET_NOT_ENOUGH_CLEARANCE;
    }
    return 0;
}
#endif

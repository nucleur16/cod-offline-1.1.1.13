#ifndef WEAPON_LOCK_H
#define WEAPON_LOCK_H
#include <stdint.h>
#include "playerstate.h"
#include "playerstate_ext.h"
/* IW4 lock state, kept outside the retained IW3 playerState ABI but inside
 * the CoDOL playerState extension, because IW4 and C1 keep it in the
 * playerState itself: PlayerCmd_WeaponLockFinalize (iw4x 0x5dd500) writes
 * ps+0x404 flags, ps+0x408 entity and ps+0x40c position, and C1 transmits
 * those three through the measured netfields 0x48c/0x490/0x494..0x49c. The
 * state therefore travels with every playerState copy - snapshot frame,
 * archive slot, archive stream and spectator bundle - instead of being a
 * per-client global that a delayed spectator would read as "now".
 * Entity target and position remain stored when switching target modes.
 * Missile offset is server-side and is not the position target: IW4 keeps it
 * at client+0x3654 and only the missile launch (0x4a4f52) reads it. */
typedef ps_lock_t weapon_lock_t;
void ResetWeaponLock(weapon_lock_t *);
/* position != NULL selects a position target, otherwise entity (0..2046).
 * finalize adds locked and fire-mode flags; start clears those flags.
 * top_attack is weaponLockFinalize's third argument: bit 4 when true, bit 8
 * otherwise. The launch (IW4 0x4a4f6b/0x4910a0, C1 0x574a1a/0x53139a) flies
 * a javelin in top-attack mode when bit 4 is set and direct otherwise, which
 * is why IW4 _javelin.gsc passes true for ground points and false for
 * vehicles. Invalid arguments preserve all state. */
int SetWeaponLock(weapon_lock_t *,unsigned entity,const float *position,
    int finalize,const float *missile_offset,int top_attack);
void FreeWeaponLock(weapon_lock_t *);
int SetWeaponLockWarning(weapon_lock_t *,unsigned mask,int enabled);
/* C1 fields 48c/490/494/498/49c; missile offset is deliberately server-only. */
int OverlayWeaponLock(player_fields_t *,const weapon_lock_t *);
/* The lock of one playerState storage. The read accessor never returns NULL:
 * an untracked state reads as unlocked. The mutable accessor returns NULL for
 * an untracked state and records the reset lock on first use. */
weapon_lock_t *PlayerStateWeaponLock(const playerState_t *ps);
const weapon_lock_t *PlayerStateWeaponLockRead(const playerState_t *ps);
/* The live state of a connected client, for callers holding an index. */
weapon_lock_t *ClientWeaponLock(unsigned client);
/* Live half only, and only the lock: the recoil magnitude sharing the block
 * is another subsystem's, and captured snapshots and archived history keep
 * the lock they were taken with. The all-clients form is the map/game reset
 * and does clear every storage. */
void ResetClientWeaponLock(unsigned client);
void ResetClientWeaponLocks(void);
/* Source-visible position targets for retained missile guidance. */
extern unsigned char missile_position_active[ENTITYNUM_WORLD];
/* Persistent target state, separate from a handle that may be dissociated. */
extern unsigned char missile_lock_active[ENTITYNUM_WORLD];
void SetMissileEntityTarget(unsigned entity,int locked);
extern float missile_positions[ENTITYNUM_WORLD][3];
void ResetMissilePosition(unsigned entity);
void ResetMissilePositions(void);
#endif

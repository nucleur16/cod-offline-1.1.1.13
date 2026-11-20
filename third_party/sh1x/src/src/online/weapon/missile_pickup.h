#ifndef MISSILE_PICKUP_H
#define MISSILE_PICKUP_H
struct gentity_s;
struct playerState_s;
struct WeaponDef;
int IsInventoryMissile(const struct gentity_s *);
int InventoryMissileUseAllowed(const struct gentity_s *,const struct gentity_s *);
int InventoryMissileCanBeGrabbed(const struct playerState_s *,unsigned);
/* -1 ordinary missile, 0 refused, 2 inventory recovery/cleanup completed. */
int RecoverMissileAmmo(struct gentity_s *,const struct WeaponDef *,unsigned,unsigned,struct gentity_s *);
/* -2 unavailable hint, -1 knife/ordinary missile; otherwise interned hint. */
int InventoryMissileHint(const struct gentity_s *);
/* Fuse a recovered live grenade keeps in hand. C1's PERKS_DESC_ENGINEER /
 * PERKS_DESC_ENGINEERPRO: "Pick up grenades and reset the fuse"; without the
 * perk the remaining time carries over (C1 server 0x50b97a: ps+34 = ps+3c).
 * Never shortens the time left. */
int ThrowbackFuseLeft(int timeLeft,int fuseTime,int resetFuse);
/* HandleClientEvent throwback pickup: the held grenadeTimeLeft for player
 * picking up missile (a validated frag-class ET_MISSILE). */
int ThrowbackGrenadeTimeLeft(const struct gentity_s *player,const struct gentity_s *missile);
#endif

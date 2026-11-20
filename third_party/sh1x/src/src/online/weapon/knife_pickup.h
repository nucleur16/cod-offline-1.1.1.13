#ifndef KNIFE_PICKUP_H
#define KNIFE_PICKUP_H
struct gentity_s;
struct WeaponDef;
struct playerState_s;
/* -1: ordinary missile, 0: knife refused, 1: one knife recovered. */
int RecoverKnifeAmmo(struct gentity_s *,const struct WeaponDef *,unsigned,unsigned);
int IsKnifeDefinition(const struct WeaponDef *);
int IsKnifeEntity(const struct gentity_s *);
/* -1: ordinary weapon, otherwise real owned/compatible ammo capacity. */
int KnifeCanBeGrabbed(const struct playerState_s *,unsigned);
#endif

#ifndef BULLET_EXPLODE_H
#define BULLET_EXPLODE_H
/* Explosive bullets (C1 bBulletExplosiveDamage, WeaponDef +0xbcb): the
 * retained IW3 Bullet_Fire/Bullet_Process (src/asmsource/_bullet.asm) have no
 * such weapon; these are called from there under OL_SERVER.
 * SPDX-License-Identifier: AGPL-3.0-or-later */

struct WeaponDef;
struct gentity_s;

/* C1 0x505ee0: the means of death a bullet carries. An explosive-bullet
 * definition takes MOD_EXPLOSIVE_BULLET; anything else keeps the retained
 * pistol/rifle choice passed in. */
int BulletMeansOfDeath(const struct WeaponDef *def, int mod);

/* C1 0x506ad4..0x506aee, 0x505f90: after the direct hit, an explosive bullet
 * that did not stop in glass explodes at the impact. bp and br are the
 * retained BulletFireParams/BulletTraceResults Bullet_Process received. */
void BulletExplode(const void *bp, const void *br, const struct WeaponDef *def,
    struct gentity_s *attacker);

#endif

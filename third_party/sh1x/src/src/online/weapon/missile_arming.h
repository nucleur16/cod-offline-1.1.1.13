/* Projectile arming distance; SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef MISSILE_ARMING_H
#define MISSILE_ARMING_H
struct gentity_s;
struct WeaponDef;
/* IW3/IW4 MissileImpact: a projectile whose def names an activation distance
 * (iProjectileActivateDist, C1 WeaponDef +0x7a0) and has not yet travelled
 * that far is a dud: no explosion, impact damage only. The comparison is the
 * retained `ucomiss travelled, activate; jb`, so an unordered (NaN) distance
 * also counts as unarmed. Shipped C1 values: gl_* and m79_mp 375,
 * m203_*_so / m79_so (coop training) 320, harrier_ffar_mp 64,
 * harrier_missile_mp 2048; rockets, grenades and the zombies launchers 0. */
static inline int MissileArmingDud(int activate_dist,float travelled)
{
    return activate_dist>0 && !(travelled>=(float)activate_dist);
}
/* Called by retained MissileImpact (OL_SERVER) in place of its inline
 * comparison; reads the missile's accumulated travelDist (+0x1bc). */
int MissileImpactUnarmed(const struct gentity_s *missile,const struct WeaponDef *def);
#endif

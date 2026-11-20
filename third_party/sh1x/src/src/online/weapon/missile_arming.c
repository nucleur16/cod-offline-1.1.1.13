/* Projectile arming distance at impact; SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "missile_arming.h"
#include "g_shared.h"
#include "bg_public.h"
#include <stddef.h>

/* Retained G_RunMissile adds each frame's displacement to this float while the
 * def has an activation distance; G_FireGrenade/G_FireRocket zero it, and the
 * dud path of MissileImpact parks it at -1e10 so later impacts stay duds. */
_Static_assert(offsetof(gentity_t,missile.travelDist)==0x1bc,
               "MissileImpact reads the missile travel distance at +0x1bc");

int MissileImpactUnarmed(const struct gentity_s *missile,const struct WeaponDef *def)
{
    int dud;
    if(!missile || !def) return 0;
    dud=MissileArmingDud(def->iProjectileActivateDist,missile->missile.travelDist);
    if(def->iProjectileActivateDist>0)
        ((void)0);
    return dud;
}
#endif

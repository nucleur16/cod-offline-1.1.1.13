/* Explosive bullets, after C1's bullet code.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C1 (as IW4) has bullet weapons whose every impact explodes: the WeaponDef
 * byte +0xbcb (IW4 bBulletExplosiveDamage). The only shipped one is the
 * chopper gunner's cobra_player_minigun_mp. The retained IW3 bullet code has
 * no such weapon, so the gunner's rounds only hurt what they hit directly.
 *
 *   MOD       0x505ee0: +0xbcb -> 3 (C1's MOD_EXPLOSIVE_BULLET), else
 *             +0xbc4 (rifle) -> 2, else 1.
 *   no pen.   0x507cf7..0x507d11: +0xbcb skips the penetrating fire path.
 *   explode   0x5069b0 (Bullet_Process) after the direct G_Damage:
 *             0x506a5a  nothing when the trace is to ignore its hit entity;
 *             0x506ad4  bp.methodOfDeath == 3 and
 *             0x506ada  !(trace contents & 0x10, glass) -> 0x505f90.
 *   0x505f90  origin (0x505f10): the impact point pulled back along the
 *             bullet by 0.25 / -(normal . dir), or by 2 when that cosine is
 *             under 0.125;
 *             inner = (float)+0x588 (damage) * dvar "151" * +0x78c,
 *             outer = 0, radius = +0x790 * dvar "152", cone cos -1 (all
 *             round), ignore = the hit entity when it takes damage (it had
 *             the direct hit), mod = bp.methodOfDeath, weapon -1 (resolved
 *             from inflictor then attacker, 0x521060), inflictor and attacker
 *             = the shooter.
 *   dvars     0x40b9c6 "151" float 1, 0..10000 (IW4 bullet_explDmgFactor);
 *             0x40ba00 "152" float 250, 0..10000 (IW4 bullet_explRadius).
 *
 * cobra_player_minigun_mp in common_gametype_mp: damage 75, playerDamage 100,
 * +0x78c 1, +0x790 1, so 75 at the impact falling to 0 at 250 units. Its
 * explosionRadius/Inner/Outer (128/120/40) are projectile fields that this
 * path does not read, in C1 or IW4. Splash reach, distance to the box edge
 * and riot-shield cover are G_RadiusDamage's (radius_damage.c).
 */
#include "bullet_explode.h"
#include "weapon_native.h"
#include "g_shared.h"
#include "cvar.h"
#include "qcommon_io.h"
#include <float.h>
#include <stddef.h>

/* Retained IW3 layouts as _bullet.asm reads them. */
typedef struct {
    int weaponEntIndex, ignoreEntIndex;
    float damageMultiplier;
    int methodOfDeath;
    float origStart[3], start[3], end[3], dir[3];
} bullet_fire_params_t;
typedef struct {
    trace_t trace;
    struct gentity_s *hitEnt;
    float hitPos[3];
    unsigned char ignoreHitEnt;
} bullet_trace_results_t;
_Static_assert(offsetof(bullet_fire_params_t, methodOfDeath) == 0x0c, "BulletFireParams mod");
_Static_assert(offsetof(bullet_fire_params_t, dir) == 0x34, "BulletFireParams dir");
_Static_assert(offsetof(bullet_trace_results_t, hitEnt) == 0x2c, "BulletTraceResults hitEnt");
_Static_assert(offsetof(bullet_trace_results_t, hitPos) == 0x30, "BulletTraceResults hitPos");
_Static_assert(offsetof(bullet_trace_results_t, ignoreHitEnt) == 0x3c, "BulletTraceResults ignoreHitEnt");

/* Retained G_RadiusDamage (_g_combat_mp.asm). */
extern void G_RadiusDamage(const float *origin, struct gentity_s *inflictor,
    struct gentity_s *attacker, float innerDamage, float outerDamage, float radius,
    float coneAngleCos, float *coneDirection, struct gentity_s *ignore, int mod, int weapon);

#define CONTENTS_GLASS 0x10

static cvar_t *expl_damage_factor;
static cvar_t *expl_radius;
static cvar_t *expl_debug;

static int bullet_explode_dvars(void)
{
    if(!expl_damage_factor)
        expl_damage_factor=Cvar_RegisterFloat("151",1.0f,0.0f,10000.0f,0x84,
            "Explosive bullet damage at the centre, times the weapon's damage");
    if(!expl_radius)
        expl_radius=Cvar_RegisterFloat("152",250.0f,0.0f,10000.0f,0x84,
            "Explosive bullet splash radius, times the weapon's radius scale");
    if(!expl_debug)
        expl_debug=Cvar_RegisterBool("bulletExplodeDebug",qfalse,0,
            "Print every explosive bullet impact");
    return expl_damage_factor && expl_radius;
}

int BulletMeansOfDeath(const struct WeaponDef *def, int mod)
{
    return GetNativeWeaponBulletExplosion(def,NULL,NULL) ? MOD_EXPLOSIVE_BULLET : mod;
}

void BulletExplode(const void *bpv, const void *brv, const struct WeaponDef *def,
    struct gentity_s *attacker)
{
    const bullet_fire_params_t *bp=bpv;
    const bullet_trace_results_t *br=brv;
    float damageScale, radiusScale, inner, radius, cosine, pullback, origin[3];
    float coneDir[3]={0.0f,0.0f,0.0f};
    struct gentity_s *ignore;
    unsigned i;

    if(!bp || !br || !def || !attacker) return;
    if(bp->methodOfDeath!=MOD_EXPLOSIVE_BULLET || br->ignoreHitEnt) return;
    if(br->trace.cflags & CONTENTS_GLASS) return;
    if(!GetNativeWeaponBulletExplosion(def,&damageScale,&radiusScale)) return;
    if(!bullet_explode_dvars()) return;
    /* 0x505f10 */
    cosine=-(br->trace.normal[0]*bp->dir[0]+br->trace.normal[1]*bp->dir[1]+
             br->trace.normal[2]*bp->dir[2]);
    pullback=cosine<0.125f ? 2.0f : 0.25f/cosine;
    for(i=0;i<3;++i) origin[i]=br->hitPos[i]-bp->dir[i]*pullback;
    /* 0x505fd2..0x505ff3 */
    inner=(float)def->damage*expl_damage_factor->value*damageScale;
    radius=radiusScale*expl_radius->value;
    ignore=br->hitEnt && br->hitEnt->takedamage ? br->hitEnt : NULL;
    if(expl_debug && expl_debug->boolean)
        ((void)0);
    if(inner<=0.0f || radius<=0.0f) return;
    G_RadiusDamage(origin,attacker,attacker,inner,0.0f,radius,-1.0f,coneDir,ignore,
        bp->methodOfDeath,-1);
}

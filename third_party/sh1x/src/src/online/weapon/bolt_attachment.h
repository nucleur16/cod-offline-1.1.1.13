#ifndef BOLT_ATTACHMENT_H
#define BOLT_ATTACHMENT_H
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
struct trace_s;
/* C1 projExplosion kind 7: T5's WEAPPROJEXP_BOLT (OpenBLOPS bg_weapons.h,
 * value 9 there). The retained IW3 enum stops at 6, and no C1 reader tests
 * the value, so it is named here rather than added to weapProjExposion_t. */
#define PROJEXP_BOLT 7
/* MissileImpact entry. Nonzero when the missile was a bolt and the impact
 * was consumed (stuck, attached, announced); the retained impact must not run. */
int BoltImpact(struct gentity_s *missile,struct trace_s *trace,
 const float *velocity,const float *endpos);
/* G_RunMissile entry. Nonzero when this missile is a stuck bolt whose frame
 * was run here (attachment follow plus its think). */
int RunAttachedBolt(struct gentity_s *missile);
void ResetBoltAttachment(unsigned number);
/* Nonzero for the companion missile that makes C1 draw the grenade danger
 * indicator over an armed bolt. Never a pickup or use candidate. */
int IsBoltIndicator(const struct gentity_s *entity);
/* G_RadiusDamage: nonzero when the blast of this inflictor (an armed bolt)
 * measures this non-brush target from its origin, as T5 does, instead of
 * C1's box edge. */
int RadiusDamageFromOrigin(const struct gentity_s *inflictor,const struct gentity_s *target);
void ResetBoltAttachments(void);
#ifdef __cplusplus
}
#endif
#endif

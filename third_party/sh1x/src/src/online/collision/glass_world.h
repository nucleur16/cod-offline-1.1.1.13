/* Authoritative glass state, separate from client shard simulation. */
#ifndef GLASS_WORLD_H
#define GLASS_WORLD_H
#include "glass_map.h"
#include "glass_snapshot.h"
/* One-based brush/trace IDs versus zero-based script/wire IDs are explicit. */
int GlassWorldInstall(const glass_map_source_t *);
int GlassWorldEnsureMap(void);
void GlassWorldReset(void);
unsigned GlassWorldCount(void);
int GlassWorldDamage(unsigned piece, unsigned damage, const float *position,
    const float *direction, int time);
void GlassWorldRadiusDamage(const float *origin,float radius,
    float maximum,float minimum,const float *cone,float cone_cos,int time);
int GlassWorldRemove(unsigned piece,int time);
int GlassWorldShatter(unsigned piece,const float *direction,int time);
float GlassMissileShatterVelocity(void);
int GlassWorldShatterAt(unsigned piece,const float *position,
    const float *direction,int time);
int GlassWorldOrigin(unsigned piece,float *out);
int GlassWorldState(unsigned piece);
int GlassWorldCollides(unsigned one_based_id);
int GlassWorldCapture(glass_wire_piece_t *,size_t capacity,unsigned *count);
struct trace_s;
void GlassBulletDamage(const struct trace_s *,int damage,const float *position,
    const float *direction);
void GlassMeleeDamage(const struct trace_s *,const float *position,const float *direction);
void GlassExplosionDamage(const float *origin,float maximum,float minimum,
    float radius,float cone_cos,const float *cone);
/* Collision leaf routines read this byte array using one_based_id-1. It is
 * zeroed on reset, so an uninitialized map never silently loses collision. */
extern unsigned char glass_collision_state[GLASS_MAX_PIECES];
#endif

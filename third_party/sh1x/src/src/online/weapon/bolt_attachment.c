#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Explosive bolts: T5 MissileImpact's WEAPPROJEXP_BOLT arm for C1's kind 7.
 *
 * C1 ships two T5 bolt launchers in common_gametype_zm, crossbow_explosive_zm
 * and sniper_explosive_zm (the Scavenger), each with an upgraded twin. Their
 * definitions are T5's own, field for field: weapType projectile, explosion
 * kind 7 (T5 WEAPPROJEXP_BOLT), stickiness all, no impact explosion, the
 * crossbow's radius 1 / damage 150-100 / impact 750 exactly as T5's
 * crossbow_explosive_zm (MEASURED against Black Ops' zombie_cod5_prototype.ff
 * with OpenBLOPS' WeaponDef layout), and the spawned-grenade name at +b84
 * ("explosive_bolt_zm", "sniper_explosive_bolt_zm", ... -- T5's
 * szSpawnedGrenadeWeaponName, which C1 keeps in its tail but never reads).
 *
 * T5 (OpenBLOPS g_missile.cpp MissileImpact case 9, AttachBoltGrenade,
 * G_FireGrenade): the bolt does its impact damage, stays where it hit, a
 * grenade of the spawned weapon is fired at that point with that weapon's
 * fuse and linked to the hit entity's ragdoll bone, the owner is notified
 * grenade_fire( grenade, spawned name ), and the grenade explodes on its fuse.
 *
 * The retained IW3 MissileImpact has no such arm. A head-on bolt "crumpled"
 * (CheckCrumpleMissile: projectile, speed >= 500, dot > .707) or hit
 * non-damageable world and took the explode path, where kind 7 matched no
 * case and the bolt was freed with no event and no damage; a glancing bolt
 * stuck and sat for its 30 s lifetime, then made a radius-1 custom
 * explosion. Neither weapon could hurt a zombie.
 *
 * The spawned grenade weapons are in no CoDOL zone (docs/zone_assets/
 * ALL_weapon.txt), only in C1's zm constant weapon window, so there is no
 * definition to fire. This module therefore keeps the bolt itself as the
 * stuck entity, attaches it as T5 attaches the grenade, and raises T5's
 * grenade_fire with the spawned name on the owner; the spawned weapon's fuse
 * and blast are carried by the zm script that registers that name
 * (_zombiemode_weap_explosive_bolt.gsc), with T5's measured values.
 *
 * DANGER INDICATOR. T5 draws the grenade indicator over the stuck bolt
 * because the bolt is a grenade there: all four spawned bolt grenades
 * (explosive_bolt_zm, its upgrade, both Scavenger bolts) are timed, with
 * bShowIndicator 1 (Black Ops fastfiles decoded with OpenBLOPS WeaponDef;
 * explosive_bolt_zm: radius 256, indicator radius 170), which OpenBLOPS
 * cg_draw_indicators.cpp CG_AddHudGrenade accepts.
 *
 * C1 has no such definition, and its gate is narrower. C1 CG_Missile
 * (0x43f510, protected body .tvm0 0x8418652), emulated over a controlled
 * WeaponDef: the HUD-grenade path runs only for weapType (+0x340) 1
 * (grenade) with projExplosion (+0x7b4) 0, 2 or 6 and timedDetonation
 * (+0xbe2) set. It then calls the range test 0x438730 (distance against
 * max(cg_hudGrenadeIconMaxRangeFrag, explosionRadius +0x778); the flash
 * range for kind 2) and appends the entity to the HUD-grenade list at
 * cg+0x174180 (count cg+0x174380), drawn with hud_grenadeicon and
 * hud_grenadepointer (0x438420, called from 0x438c91). A crossbow bolt
 * (weapType 2, projExplosion 7) never qualifies, and C1 takes a missile's
 * model from that same weapon's projectileModel (0x443be5, +0x7b0), so the
 * bolt cannot be published as a grenade either.
 *
 * So an armed bolt carries a companion: a missile of a C1 timed frag
 * definition that passes the gate (stielhandgranate, which has no trail or
 * beacon effect, else frag_grenade_zm), drawn at the smallest model scale,
 * not solid, no pickup item, no think, moved onto the bolt every frame and
 * freed the frame after the bolt is gone (the script deletes the bolt when
 * it explodes). */
#ifdef OL_SERVER
#include "bolt_attachment.h"
#include "weapon_native.h"
#include "rc_lifecycle.h"
#include "g_shared.h"
#include "bg_public.h"
#include "cm_public.h"
#include "cscr_stringlist.h"
#include "q_math.h"
#include "server.h"
#include "qcommon_io.h"
#include "dobj.h"
#include "scr_vm.h"
#include "scr_vm_functions.h"
#include "script_combat_builtins.h"
#include "script_model_scale.h"
#include "usercmd.h"
#include <math.h>
#include <string.h>

extern int G_EntLinkTo(gentity_t *,gentity_t *,unsigned);
extern int NotifyMissileStationary(gentity_t *);
extern void G_RunThink(gentity_t *);
extern int BG_FindWeaponIndexForName(const char *name);
extern void G_Damage(gentity_t *,gentity_t *,gentity_t *,const float *,const float *,
 int,int,int,unsigned,int,unsigned,unsigned,int);

#define OL_MOD_IMPACT 15

typedef struct { unsigned number; int generation; } bolt_ref_t;
typedef struct { int active,generation; bolt_ref_t target,indicator; } bolt_attachment_t;
static bolt_attachment_t bolts[ENTITYNUM_WORLD];
/* Danger-indicator companions, by their own entity number. */
typedef struct { int active,generation; bolt_ref_t bolt; } bolt_indicator_t;
static bolt_indicator_t indicators[ENTITYNUM_WORLD];

static gentity_t *resolve(bolt_ref_t ref)
{
 gentity_t *e;
 if(!ref.number || ref.number>ENTITYNUM_WORLD)return NULL;
 e=&g_entities[ref.number-1];
 return e->r.inuse && e->useCount==ref.generation ? e : NULL;
}
static bolt_ref_t reference(gentity_t *e)
{
 bolt_ref_t r={0,0};
 if(e && e->r.inuse && (unsigned)e->s.number<ENTITYNUM_WORLD) {
  r.number=e->s.number+1; r.generation=e->useCount;
 }
 return r;
}
/* The spawned grenade name of a bolt missile, or NULL for anything else. */
static const char *bolt_grenade(gentity_t *e)
{
 const struct WeaponDef *def; const char *name=NULL; unsigned w;
 if(!e || !e->r.inuse || e->s.eType!=ET_MISSILE)return NULL;
 w=e->s.weapon;
 if(!w || w>=(unsigned)BG_GetNumWeapons())return NULL;
 def=BG_GetWeaponDef(w);
 if(!def || def->weapType!=WEAPTYPE_PROJECTILE || (unsigned)def->projExplosion!=PROJEXP_BOLT)return NULL;
 if(!GetNativeWeaponSpawnedGrenade(def,&name) || !name || !*name)return NULL;
 return name;
}
/* T5 MapHitLocationToRagdollBoneName, as the knife uses it; the receiver's
 * own DObj must resolve the tag, otherwise the link keeps the impact pose. */
static unsigned bone(unsigned location)
{
 static const char *names[HITLOC_NUM]={NULL,"j_head","j_head","j_neck",
  "j_spineupper","j_spinelower","j_elbow_ri","j_elbow_le","j_wrist_ri",
  "j_wrist_le","j_wrist_ri","j_wrist_le","j_knee_ri","j_knee_le",
  "j_ankle_ri","j_ankle_le","j_ankle_ri","j_ankle_le",NULL};
 return location<HITLOC_NUM && names[location] ? SL_FindString(names[location]) : 0;
}
static int link_target(gentity_t *e,gentity_t *target,unsigned tag)
{
 float matrix[4][3];
 if(tag && !G_DObjGetWorldTagMatrix(target,tag,matrix))tag=0;
 return G_EntLinkTo(e,target,tag);
}
static gentity_t *owner(gentity_t *e)
{
 unsigned n=e->parent.number;
 if(!n || n>ENTITYNUM_WORLD)return NULL;
 return g_entities[n-1].r.inuse ? &g_entities[n-1] : NULL;
}
/* A registered definition C1's HUD-grenade path accepts (see above): grenade,
 * projExplosion grenade, timed. Find-only; the zm bolt script precaches the
 * first name. */
static unsigned indicator_weapon(void)
{
 static const char *const names[]={"stielhandgranate","frag_grenade_zm"};
 unsigned i; int w; const struct WeaponDef *def;
 for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
  w=BG_FindWeaponIndexForName(names[i]);
  if(w<=0 || w>=BG_GetNumWeapons() || WeaponToWire((unsigned)w)<=0)continue;
  def=BG_GetWeaponDef((unsigned)w);
  if(def && def->weapType==WEAPTYPE_GRENADE && def->projExplosion==WEAPPROJEXP_GRENADE &&
     def->timedDetonation)return (unsigned)w;
 }
 return 0;
}
static void spawn_indicator(gentity_t *bolt,bolt_attachment_t *a,gentity_t *parent)
{
 gentity_t *mark; unsigned weapon=indicator_weapon();
 if(!weapon)return;
 mark=G_Spawn();
 if(!mark)return;
 if((unsigned)mark->s.number>=ENTITYNUM_WORLD) { G_FreeEntity(mark); return; }
 mark->s.eType=ET_MISSILE;
 mark->s.weapon=weapon;
 mark->s.weaponModel=0;
 /* C1 CG_Missile skips a missile whose launch time (es+0x58) is ahead. */
 mark->s.lerp.u.missile.launchTime=level.time;
 mark->s.clientNum=bolt->s.clientNum;
 mark->s.otherEntityNum=bolt->s.otherEntityNum;
 mark->s.iHeadIconTeam=bolt->s.iHeadIconTeam;
 if(parent) {
  EntHandleSetEnt(&mark->r.ownerNum,parent);
  EntHandleSetEnt(&mark->parent,parent);
 }
 /* Nothing to touch, use, shoot or collide with; no think, no fuse. */
 mark->r.contents=0;
 mark->clipmask=0;
 mark->takedamage=0;
 mark->nextthink=0;
 G_SetOrigin(mark,bolt->r.currentOrigin);
 G_SetAngle(mark,bolt->r.currentAngles);
 /* C1 draws the definition's projectile model; keep it out of sight. */
 SetEntityModelScale(mark->s.number,0.01f);
 indicators[mark->s.number].active=1;
 indicators[mark->s.number].generation=mark->useCount;
 indicators[mark->s.number].bolt=reference(bolt);
 a->indicator=reference(mark);
 SV_LinkEntity(mark);
 ((void)0);
}
/* The companion's frame: sit on its bolt, or go once the bolt has. */
static int run_indicator(gentity_t *mark)
{
 bolt_indicator_t *m; gentity_t *bolt; bolt_attachment_t *a;
 m=&indicators[mark->s.number];
 if(!m->active || m->generation!=mark->useCount)return 0;
 bolt=resolve(m->bolt);
 a=bolt ? &bolts[bolt->s.number] : NULL;
 if(!bolt || !a->active || a->generation!=bolt->useCount ||
    a->indicator.number!=(unsigned)mark->s.number+1 ||
    a->indicator.generation!=mark->useCount || !bolt_grenade(bolt)) {
  ((void)0);
  memset(m,0,sizeof(*m));
  G_FreeEntity(mark);
  return 1;
 }
 if(!VectorCompare(mark->r.currentOrigin,bolt->r.currentOrigin))
  G_SetOrigin(mark,bolt->r.currentOrigin);
 SV_LinkEntity(mark);
 return 1;
}
int BoltImpact(gentity_t *e,trace_t *tr,const float *velocity,const float *endpos)
{
 const struct WeaponDef *def; const char *grenade; gentity_t *target=NULL,*parent;
 bolt_attachment_t *a; bolt_ref_t self,victim; vec3_t dir,angles; unsigned hit,tag=0;
 float speed;
 if(!tr || !velocity || !(grenade=bolt_grenade(e)))return 0;
 if((unsigned)e->s.number>=ENTITYNUM_WORLD)return 0;
 a=&bolts[e->s.number];
 if(a->active && a->generation==e->useCount)return 1; /* Already stuck. */
 def=BG_GetWeaponDef(e->s.weapon);
 hit=(unsigned)(unsigned short)Trace_GetEntityHitId(tr);
 if(hit<ENTITYNUM_WORLD && g_entities[hit].r.inuse)target=&g_entities[hit];
 VectorCopy(velocity,dir);
 speed=sqrtf(DotProduct(dir,dir));
 if(!isfinite(speed) || speed<=0) { VectorSet(dir,0,0,1); speed=1; }
 VectorScale(dir,1.0f/speed,dir);
 if(endpos && isfinite(endpos[0]) && isfinite(endpos[1]) && isfinite(endpos[2]))
  G_SetOrigin(e,(float *)endpos);
 /* T5 applies the missile's impact damage before its explosion switch,
  * with the retained MissileImpact's own arguments (MOD_IMPACT, hit
  * location from partGroup). */
 if(target && target->takedamage && def->damage>0) {
  self=reference(e); victim=reference(target);
  G_Damage(target,e,owner(e),dir,e->r.currentOrigin,def->damage,0,OL_MOD_IMPACT,
   e->s.weapon,tr->partGroup,tr->modelIndex,tr->partName,0);
  e=resolve(self);
  if(!e || bolt_grenade(e)!=grenade)return 1; /* A callback removed the bolt. */
  target=resolve(victim);
 }
 /* T5 AttachBoltGrenade: stationary, oriented along its flight. */
 vectoangles(dir,angles);
 G_SetOrigin(e,e->r.currentOrigin); G_SetAngle(e,angles);
 e->s.lerp.pos.trType=TR_STATIONARY; e->s.lerp.apos.trType=TR_STATIONARY;
 memset(e->s.lerp.pos.trDelta,0,sizeof(vec3_t));
 memset(e->s.lerp.apos.trDelta,0,sizeof(vec3_t));
 e->s.groundEntityNum=ENTITYNUM_WORLD;
 memset(a,0,sizeof(*a));
 a->active=1; a->generation=e->useCount;
 /* Players, actors and corpses by ragdoll bone; vehicles by the hit part;
  * anything else that moves at its origin (T5 G_EntLinkTo(ent, hitEnt, 0)).
  * The world is no link at all. */
 if(target && target->s.eType!=ET_MISSILE) {
  if(target->client || target->s.eType==ET_ACTOR || target->s.eType==ET_ACTOR_CORPSE ||
     target->s.eType==ET_PLAYER_CORPSE)
   tag=bone(tr->partGroup);
  else if(target->s.eType==ET_VEHICLE)
   tag=tr->partName;
  if(link_target(e,target,tag) || (tag && link_target(e,target,0))) {
   a->target=reference(target);
   e->s.groundEntityNum=target->s.number;
  }
 }
 SV_LinkEntity(e);
 spawn_indicator(e,a,owner(e));
 /* T5 G_FireGrenade's notify, with the spawned weapon's name: the owner
  * learns of the armed bolt (grenade_fire( grenade, weaponName )). */
 self=reference(e);
 parent=owner(e);
 if(parent) {
  Scr_AddString(grenade);
  Scr_AddEntity(e);
  Scr_Notify(parent,scr_const.grenade_fire,2);
  e=resolve(self);
  if(!e)return 1;
 }
 NotifyMissileStationary(e); /* No entity access after script callbacks. */
 return 1;
}
int RunAttachedBolt(gentity_t *e)
{
 bolt_attachment_t *a; gentity_t *target;
 if(!e || (unsigned)e->s.number>=ENTITYNUM_WORLD)return 0;
 if(run_indicator(e))return 1;
 a=&bolts[e->s.number];
 if(!a->active || a->generation!=e->useCount)return 0;
 if(!bolt_grenade(e)) { memset(a,0,sizeof(*a)); return 0; }
 target=resolve(a->target);
 if(a->target.number && (!target || !e->tagInfo)) {
  /* The carrier is gone (a freed corpse tears down its tag children):
   * the bolt stays where it last was. */
  if(e->tagInfo)G_EntUnlink(e);
  a->target.number=0; a->target.generation=0;
  e->s.groundEntityNum=ENTITYNUM_WORLD;
  G_SetOrigin(e,e->r.currentOrigin); G_SetAngle(e,e->r.currentAngles);
 }
 if(e->tagInfo)G_GeneralLink(e);
 SV_LinkEntity(e);
 /* The missile's own lifetime think still runs; it may free the entity. */
 G_RunThink(e);
 return 1;
}
void ResetBoltAttachment(unsigned n)
{
 if(n<ENTITYNUM_WORLD) {
  memset(&bolts[n],0,sizeof(bolts[n]));
  memset(&indicators[n],0,sizeof(indicators[n]));
 }
}
void ResetBoltAttachments(void)
{
 memset(bolts,0,sizeof(bolts));
 memset(indicators,0,sizeof(indicators));
}
/* G_RadiusDamage's distance rule for one target. C1 (0x522aca..0x522bb3)
 * measures every entity from the edge of its absolute box, and the retained
 * code does so for every C1 explosion. The bolt's blast is not a C1
 * explosion: it is T5's spawned grenade (explosive_bolt_zm and the rest),
 * which explodes through T5 G_RadiusDamage (OpenBLOPS g_combat_mp.cpp
 * G_GetRadiusDamageDistanceSquared): a brush model by its box, anything else
 * from its origin, a sentient's feet. Box edges lengthened the T5 radius by
 * the half-width of every zombie and player and dropped the height of a bolt
 * stuck in a chest altogether: measured 2026-10-06, a zombie 269 units from
 * a 256-unit blast took 93. The bolt is the inflictor when the zm script
 * explodes it (bolt radiusDamage(...)), as T5's grenade is. */
int RadiusDamageFromOrigin(const gentity_t *inflictor,const gentity_t *ent)
{
 const bolt_attachment_t *a;
 if(!inflictor || !ent || ent->r.bmodel || (unsigned)inflictor->s.number>=ENTITYNUM_WORLD)return 0;
 /* The RC-XD's blast is T5's too (rc_lifecycle.cpp). */
 if(RCVehicleBlastFromOrigin(inflictor))return 1;
 a=&bolts[inflictor->s.number];
 return a->active && a->generation==inflictor->useCount && bolt_grenade((gentity_t *)inflictor)!=NULL;
}
int IsBoltIndicator(const gentity_t *e)
{
 return e && (unsigned)e->s.number<ENTITYNUM_WORLD && indicators[e->s.number].active &&
  indicators[e->s.number].generation==e->useCount;
}
#endif

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentBoltsEmpty(void){return resident_entity_zero(bolts,sizeof(bolts))&&resident_entity_zero(indicators,sizeof(indicators));}
#endif

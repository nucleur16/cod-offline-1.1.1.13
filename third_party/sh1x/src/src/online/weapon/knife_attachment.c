#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Same-missile native retrieval/link lifecycle of a thrown knife.
 *
 * C1's throwingknife_mp is stickiness 5 (IW4 WEAPSTICKINESS_KNIFE) with
 * +bde stick-to-player/actor 1 (common_gametype_mp WeaponDef), so a knife
 * that hits a body stays in it. Its MissileImpact is not readable in the
 * shipped client, so the body behaviour follows T5 (OpenBLOPS g_missile.cpp
 * StickMissile / AttachMissileToEntity / createRetrieveableProjectile): the
 * missile is moved onto the hit location's ragdoll bone, backed off along
 * its flight by GetAttachedMissileOffset, and G_EntLinkTo'd to that bone; a
 * hit on a dying player goes to the player's corpse. The link is published
 * to C1 as parent entity + tag (engine_state.c), and C1 4404a0 places
 * the child on the parent's client-side (ragdoll) bone, so the knife rides
 * the body. Wall incidence is an explicit sh1x policy, not recovered C1
 * authority.
 *
 * A clone made this frame has no server DObj yet (PlayerCmd_ClonePlayer
 * sets the model; the DObj is built on the next G_RunFrame), so the bone of
 * a fresh corpse cannot be resolved at impact. Linking it to the corpse
 * ORIGIN instead (the old fallback) left the knife at chest height where the
 * player stood while the body fell: measured 2026-10-13 on mp_killhouse_sh,
 * j_spinelower dropped from z 41 to z 10 while the knife held z 45. The knife
 * now waits for the bone (at most a second) and never takes an origin link
 * on a body. A knife whose carrier goes away drops to the floor. */
#ifdef OL_SERVER
#include "knife_attachment.h"
#include "knife_pickup.h"
#include "g_shared.h"
#include "bg_public.h"
#include "cm_public.h"
#include "cscr_stringlist.h"
#include "q_math.h"
#include "server.h"
#include "dobj.h"
#include "sentient.h"
#include "game/surfaceflags.h"
#include "qcommon_io.h"
#include <math.h>
#include <string.h>

extern int G_EntLinkTo(gentity_t *,gentity_t *,unsigned);
extern int NotifyMissileStationary(gentity_t *);
extern void G_Damage(gentity_t *,gentity_t *,gentity_t *,const float *,const float *,
 int,int,int,unsigned,int,unsigned,unsigned,int);

/* How long a knife waits for a body's bone before it drops to the floor. */
#define KNIFE_BONE_WAIT_MS 1000
/* The floor search below a dropped knife. */
#define KNIFE_DROP_DEPTH 1024.0f

typedef struct { unsigned number; int generation; } knife_ref_t;
typedef struct {
 int active,generation;
 /* The live player the knife is in, to follow it onto that death's corpse. */
 unsigned player; int player_generation,death_seen; unsigned corpse_serial;
 /* Body hit: ragdoll bone, hit location and unit flight direction. */
 unsigned tag; int hitloc; vec3_t dir;
 /* The bone of target is not resolvable yet; retried every frame. */
 int pending,pending_since;
 knife_ref_t target;
} knife_attachment_t;
static knife_attachment_t attachments[ENTITYNUM_WORLD];
static knife_ref_t corpses[MAX_CLIENTS];
static int corpse_player_generation[MAX_CLIENTS];
static unsigned corpse_serial[MAX_CLIENTS];

/* Developer trace of every knife state change (developer 1 only). */
static void trace_knife(const char *what,const gentity_t *e,const gentity_t *target,unsigned tag)
{
 ((void)0);
}
static gentity_t *resolve(knife_ref_t ref)
{
 gentity_t *e;
 if(!ref.number || ref.number>ENTITYNUM_WORLD)return NULL;
 e=&g_entities[ref.number-1];
 return e->r.inuse && e->useCount==ref.generation ? e : NULL;
}
static knife_ref_t reference(gentity_t *e)
{
 knife_ref_t r={0,0};
 if(e && e->r.inuse && (unsigned)e->s.number<ENTITYNUM_WORLD) {
  r.number=e->s.number+1; r.generation=e->useCount;
 }
 return r;
}
static int knife(gentity_t *e)
{
 unsigned w;
 if(!e || !e->r.inuse || e->s.eType!=ET_MISSILE)return 0;
 w=e->s.weapon;
 return w && w<(unsigned)BG_GetNumWeapons() &&
  IsKnifeDefinition(BG_GetWeaponDef(w));
}
static unsigned bone(unsigned location)
{
 /* Named T5 MapHitLocationToRagdollBoneName cases; actual receiver's DObj
  * must resolve the tag. No cross-engine bone-index equivalence. */
 static const char *names[HITLOC_NUM]={NULL,"j_head","j_head","j_neck",
  "j_spineupper","j_spinelower","j_elbow_ri","j_elbow_le","j_wrist_ri",
  "j_wrist_le","j_wrist_ri","j_wrist_le","j_knee_ri","j_knee_le",
  "j_ankle_ri","j_ankle_le","j_ankle_ri","j_ankle_le",NULL};
 return location<HITLOC_NUM && names[location] ? SL_FindString(names[location]) : 0;
}
/* T5 GetAttachedMissileOffset: how far back along its flight the missile
 * sits from the bone, by hit location. */
static float bone_offset(int hitloc)
{
 if(hitloc>=1 && hitloc<=3)return 5.0f;
 if(hitloc>=6 && hitloc<=17)return 2.0f;
 return 7.5f;
}
/* Anything with a skeleton a knife stays in: players, their corpses, actors
 * (dogs, zombies) and their corpses, and script sentients. */
static int body(gentity_t *target)
{
 return target->client || target->s.eType==ET_PLAYER_CORPSE || target->s.eType==ET_ACTOR ||
  target->s.eType==ET_ACTOR_CORPSE || Sentient_ForEntity(target);
}
static int corpse_type(const gentity_t *target)
{
 return target->s.eType==ET_PLAYER_CORPSE || target->s.eType==ET_ACTOR_CORPSE;
}
/* T5 AttachMissileToEntity + G_EntLinkTo: onto the bone, backed off along
 * the flight, linked so the offset is the bone-local one. 0 when the target's
 * DObj cannot resolve the bone (yet). */
static int attach_to_bone(gentity_t *e,gentity_t *target,knife_attachment_t *a)
{
 float m[4][3]; vec3_t pos;
 if(!a->tag || !G_DObjGetWorldTagMatrix(target,a->tag,m))return 0;
 if(e->tagInfo)G_EntUnlink(e);
 VectorMA(m[3],-bone_offset(a->hitloc),a->dir,pos);
 G_SetOrigin(e,pos);
 if(!G_EntLinkTo(e,target,a->tag))return 0;
 a->target=reference(target); a->pending=0;
 e->s.groundEntityNum=target->s.number;
 trace_knife("on bone",e,target,a->tag);
 return 1;
}
static void wait_for_bone(gentity_t *e,gentity_t *target,knife_attachment_t *a)
{
 if(e->tagInfo)G_EntUnlink(e);
 a->target=reference(target); a->pending=1; a->pending_since=level.time;
 e->s.groundEntityNum=ENTITYNUM_WORLD;
 trace_knife("waits for bone",e,target,a->tag);
}
/* The carrier is gone (a freed corpse, a respawn without a corpse, a bone
 * that never resolved): the knife falls straight down and lies on the floor
 * where the next player can still pick it up. It never stays in mid-air. */
static void drop(gentity_t *e,knife_attachment_t *a)
{
 trace_t tr; vec3_t start,end,angles;
 static const vec3_t mins={-1.5f,-1.5f,-1.5f},maxs={1.5f,1.5f,1.5f};
 if(e->tagInfo)G_EntUnlink(e);
 a->target.number=0; a->target.generation=0; a->player=0; a->pending=0;
 e->s.groundEntityNum=ENTITYNUM_WORLD;
 VectorCopy(e->r.currentOrigin,start);
 VectorCopy(start,end); end[2]-=KNIFE_DROP_DEPTH;
 G_TraceCapsule(&tr,start,mins,maxs,end,e->s.number,CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_MISSILECLIP);
 if(!tr.startsolid && tr.fraction<1.0f) { vec3_t floor; Vec3Lerp(start,end,tr.fraction,floor); VectorCopy(floor,start); }
 /* Flat on the floor, pointing the way it flew. */
 angles[0]=0; angles[1]=e->r.currentAngles[1]; angles[2]=0;
 G_SetOrigin(e,start); G_SetAngle(e,angles);
 trace_knife("drop",e,NULL,0);
}
static int settle(gentity_t *e,gentity_t *target,const trace_t *tr,const float *velocity)
{
 knife_attachment_t *a; vec3_t angles; float speed;
 if((unsigned)e->s.number>=ENTITYNUM_WORLD)return 0;
 if(!isfinite(velocity[0]) || !isfinite(velocity[1]) || !isfinite(velocity[2]))return 0;
 speed=sqrtf(DotProduct(velocity,velocity));
 if(!isfinite(speed) || speed<=0)return 0;
 vectoangles(velocity,angles);
 G_SetOrigin(e,e->r.currentOrigin); G_SetAngle(e,angles);
 /* Native stationary0, not retained IW3 physics8. Keep the existing model,
  * weapon/variant identity and owner. A settled knife must enter the player
  * touch query (G_TouchTriggers mask 0x405c0008) as well as the manual-use
  * query (0x200000); the flying missile is never auto-pickup eligible. */
 e->s.lerp.pos.trType=TR_STATIONARY; e->s.lerp.apos.trType=TR_STATIONARY;
 memset(e->s.lerp.pos.trDelta,0,sizeof(vec3_t));
 memset(e->s.lerp.apos.trDelta,0,sizeof(vec3_t));
 e->nextthink=0; /* Knife recovery owns lifetime after impact, no explosion. */
 e->s.groundEntityNum=ENTITYNUM_WORLD;
 e->r.contents |= CONTENTS_TRIGGER;
 a=&attachments[e->s.number]; memset(a,0,sizeof(*a));
 a->active=1; a->generation=e->useCount;
 VectorScale(velocity,1.0f/speed,a->dir);
 if(target && target->r.inuse && (unsigned)target->s.number<ENTITYNUM_WORLD) {
  if(body(target)) {
   a->hitloc=tr->partGroup;
   a->tag=bone(tr->partGroup);
   if(!a->tag)a->tag=bone(HITLOC_TORSO_UPR);
   if(target->client && (unsigned)target->s.number<MAX_CLIENTS) {
    a->player=target->s.number+1; a->player_generation=target->useCount;
    a->corpse_serial=corpse_serial[target->s.number];
    a->death_seen=target->health<=0;
   }
   if(!attach_to_bone(e,target,a))wait_for_bone(e,target,a);
  } else {
   /* Vehicles and movers: the hit part, else the impact pose relative to
    * the entity's origin (T5 StickMissile's vehicle arm). */
   unsigned part=tr->partName; float m[4][3];
   if(part && !G_DObjGetWorldTagMatrix(target,part,m))part=0;
   if(G_EntLinkTo(e,target,part) || (part && G_EntLinkTo(e,target,0))) {
    a->target=reference(target); a->tag=part;
    e->s.groundEntityNum=target->s.number;
   }
  }
 }
 trace_knife("settle",e,target,a->tag);
 SV_LinkEntity(e);
 NotifyMissileStationary(e); /* No entity access after script callbacks. */
 return 1;
}
/* The corpse of the death that followed this player knife's hit, or NULL. */
static gentity_t *death_corpse(const knife_attachment_t *a)
{
 unsigned p;
 if(!a->player)return NULL;
 p=a->player-1;
 if(p>=MAX_CLIENTS || corpse_player_generation[p]!=a->player_generation ||
    corpse_serial[p]==a->corpse_serial)return NULL;
 return resolve(corpses[p]);
}
int KnifeWorldImpact(gentity_t *e,trace_t *tr,const float *velocity)
{
 unsigned hit; gentity_t *target=NULL; float speed,dot;
 if(!knife(e) || !tr || !velocity)return 0;
 hit=(unsigned)(unsigned short)Trace_GetEntityHitId(tr);
 if(hit<ENTITYNUM_WORLD && g_entities[hit].r.inuse)target=&g_entities[hit];
 if(target && target->takedamage)return 0; /* Real G_Damage runs first. */
 if(target && target->client && target->health<=0 && (unsigned)target->s.number<MAX_CLIENTS &&
    corpse_player_generation[target->s.number]==target->useCount) {
  gentity_t *corpse=resolve(corpses[target->s.number]);
  if(corpse)target=corpse;
 }
 if(target && body(target))
  return settle(e,target,tr,velocity);
 speed=sqrtf(DotProduct(velocity,velocity));
 if(!isfinite(speed) || speed<=0)return 0;
 dot=-DotProduct(velocity,tr->normal)/speed;
 /* User-requested wall embedding: a tip-first impact within ~45 degrees.
  * C1 predictor uses the corresponding reflected-direction dot > .7, but
  * authoritative use here is a declared sh1x policy. Glancing hits bounce. */
 if(!isfinite(dot) || dot<=0.7f)return 0;
 return settle(e,target,tr,velocity);
}
int KnifeDamageImpact(gentity_t *target,gentity_t *missile,gentity_t *attacker,
 const float *direction,const float *point,int damage,int flags,int mod,unsigned weapon,
 int hitloc,unsigned model,unsigned part,int offset,trace_t *trace)
{
 int isknife=knife(missile); knife_ref_t projectile=reference(missile),victim=reference(target);
 unsigned prior_corpse=target && target->client && (unsigned)target->s.number<MAX_CLIENTS ? corpse_serial[target->s.number] : 0;
 vec3_t velocity; trace_t saved;
 if(isknife) { VectorCopy(direction,velocity); saved=*trace; }
 G_Damage(target,missile,attacker,direction,point,damage,flags,mod,weapon,hitloc,model,part,offset);
 if(!isknife)return 0;
 missile=resolve(projectile);
 if(!missile || !knife(missile))return 1; /* Callback destroyed/retyped it. */
 target=resolve(victim);
 /* The kill cloned the body synchronously (Callback_PlayerKilled's
  * clonePlayer): the knife goes to that corpse (T5 StickMissile, pm_type
  * dead -> playerCorpseInfo entity). */
 if(target && target->client && target->health<=0 && (unsigned)target->s.number<MAX_CLIENTS &&
    corpse_player_generation[target->s.number]==target->useCount &&
    corpse_serial[target->s.number]!=prior_corpse) {
  gentity_t *corpse=resolve(corpses[target->s.number]);
  if(corpse)target=corpse;
 }
 ((void)0);
 return settle(missile,target,&saved,velocity);
}
int RunAttachedKnife(gentity_t *e)
{
 knife_attachment_t *a; gentity_t *target,*corpse;
 if(!e || (unsigned)e->s.number>=ENTITYNUM_WORLD)return 0;
 a=&attachments[e->s.number];
 if(!a->active || a->generation!=e->useCount)return 0;
 if(!knife(e)) { memset(a,0,sizeof(*a)); return 0; }
 target=resolve(a->target);
 /* A player carrying the knife died and was cloned: the knife moves to the
  * same bone of the corpse, the body C1 draws and ragdolls. */
 if(a->player && (corpse=death_corpse(a))!=NULL && corpse!=target) {
  a->player=0;
  if(attach_to_bone(e,corpse,a))target=corpse;
  else { wait_for_bone(e,corpse,a); target=corpse; }
 } else if(a->player && target && target->client && !a->pending) {
  if(target->health<=0)a->death_seen=1; /* Await the same death's clone. */
  else if(a->death_seen)drop(e,a); /* Respawned with no corpse made. */
 }
 if(a->pending) {
  if(!target)drop(e,a);
  else if(!attach_to_bone(e,target,a) && level.time-a->pending_since>=KNIFE_BONE_WAIT_MS)
   drop(e,a);
 } else if(a->target.number && (!target || !e->tagInfo))drop(e,a);
 if(e->tagInfo)G_GeneralLink(e);
 SV_LinkEntity(e);
 return 1;
}
void KnifeCorpseCreated(unsigned corpse,unsigned player)
{
 if(player>=MAX_CLIENTS || corpse>=ENTITYNUM_WORLD)return;
 corpses[player]=reference(&g_entities[corpse]);
 corpse_player_generation[player]=g_entities[player].useCount;
 ++corpse_serial[player];
}
void KnifePlayerSpawning(gentity_t *player)
{
 unsigned i,p;
 if(!player || (p=(unsigned)player->s.number)>=MAX_CLIENTS)return;
 for(i=0;i<ENTITYNUM_WORLD;++i) {
  knife_attachment_t *a=&attachments[i]; gentity_t *e,*corpse;
  if(!a->active || a->player!=p+1 || a->player_generation!=player->useCount)continue;
  e=&g_entities[i];
  if(!e->r.inuse || e->useCount!=a->generation)continue;
  /* Before spawn writes origin: never teleport with the new life. */
  corpse=death_corpse(a);
  a->player=0;
  if(corpse && corpse_type(corpse)) {
   if(!attach_to_bone(e,corpse,a))wait_for_bone(e,corpse,a);
   continue;
  }
  drop(e,a);
 }
 memset(&corpses[p],0,sizeof(corpses[p]));
}
void ResetKnifeAttachment(unsigned n)
{
 unsigned i;
 if(n>=ENTITYNUM_WORLD)return;
 memset(&attachments[n],0,sizeof(attachments[n]));
 if(n<MAX_CLIENTS) { memset(&corpses[n],0,sizeof(corpses[n])); corpse_player_generation[n]=0; }
 for(i=0;i<MAX_CLIENTS;++i)if(corpses[i].number==n+1)memset(&corpses[i],0,sizeof(corpses[i]));
 /* Native G_FreeEntity tears down tag children. Their next update drops
  * against the saved generation; a reused target can never inherit a knife. */
}
void ResetKnifeAttachments(void)
{
 memset(attachments,0,sizeof(attachments)); memset(corpses,0,sizeof(corpses));
 memset(corpse_player_generation,0,sizeof(corpse_player_generation));
 memset(corpse_serial,0,sizeof(corpse_serial));
}
#endif

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentKnivesEmpty(void){return
    resident_entity_zero(attachments,sizeof(attachments))&&resident_entity_zero(corpses,sizeof(corpses))&&
    resident_entity_zero(corpse_player_generation,sizeof(corpse_player_generation))&&resident_entity_zero(corpse_serial,sizeof(corpse_serial));}
#endif

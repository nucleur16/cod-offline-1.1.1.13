#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* T5 Actor_Dog_GetEnemyPos, Actor_SetMeleeAttackSpot and
 * Actor_Dog_IsEnemyInAttackRange, adapted to native server entity handles.
 * Reference: OpenBLOPS Game/Server/game/actor_dog_exposed.cpp:323-644.
 * No T5/C1 structure offsets or animation indices are shared here.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "dog_combat.h"
#include "server.h"
#include "g_shared.h"
#include "cscr_stringlist.h"
#include <math.h>
#include <string.h>

typedef struct { int entity, generation; } dog_claim_t;
typedef struct { int targetGeneration; dog_claim_t slot[4]; } dog_target_t;
static dog_target_t targets[MAX_GENTITIES];
static const float offsets[4][2]={{1,0},{0,1},{-1,0},{0,-1}};
static const float attackMins[3]={-15,-15,18};
static const float attackMaxs[3]={15,15,48};
static const float actorMaxs[3]={15,15,48};

static float distance2(const float *a,const float *b)
{ float x=a[0]-b[0],y=a[1]-b[1]; return x*x+y*y; }
static void direction(const float *a,const float *b,float *v)
{
    float length;
    v[0]=a[0]-b[0];v[1]=a[1]-b[1];
    length=sqrtf(v[0]*v[0]+v[1]*v[1]);
    if(length>0){v[0]/=length;v[1]/=length;}
}
static int valid_entity(const gentity_t *ent)
{ return ent && ent>=g_entities && ent<g_entities+MAX_GENTITIES && ent->r.inuse; }
static gentity_t *claimant(dog_claim_t *claim)
{
    gentity_t *ent;
    if(claim->entity<=0 || claim->entity>=MAX_GENTITIES)return NULL;
    ent=&g_entities[claim->entity];
    if(!ent->r.inuse || ent->useCount!=claim->generation || ent->health<=0){
        memset(claim,0,sizeof(*claim));return NULL;
    }
    return ent;
}
void DogCombatReset(void){memset(targets,0,sizeof(targets));}
/* DIAG(doglunge): developer-only decision trace, scr_dogAttackDiag 1. */
static int diag_time[MAX_GENTITIES];
static int diag_enabled(void)
{ return 0; /* Diagnostic-only producer disabled. */ }
void DogCombatRelease(gentity_t *dog)
{
    int target,slot;
    if(!dog)return;
    for(target=0;target<MAX_GENTITIES;++target)
        for(slot=0;slot<4;++slot)
            if(targets[target].slot[slot].entity==dog->s.number &&
               targets[target].slot[slot].generation==dog->useCount)
                memset(&targets[target].slot[slot],0,sizeof(dog_claim_t));
}
static int attack_spot(gentity_t *dog,gentity_t *enemy,const float *enemyPos,
                       float radius,float *position)
{
    dog_target_t *target=&targets[enemy->s.number];
    float dots[4],dir[2],best[3],drop[3],bestFraction=0;
    int indices[4]={0,1,2,3},i,j,index;trace_t tr;
    gentity_t *occupier;
    if(target->targetGeneration!=enemy->useCount){
        memset(target,0,sizeof(*target));target->targetGeneration=enemy->useCount;
    }
    direction(dog->r.currentOrigin,enemyPos,dir);
    for(i=0;i<4;++i){
        dots[i]=dir[0]*offsets[i][0]+dir[1]*offsets[i][1];
        if(target->slot[i].entity==dog->s.number)
            memset(&target->slot[i],0,sizeof(dog_claim_t));
    }
    for(i=1;i<4;++i){
        index=indices[i];
        for(j=i-1;j>=0 && dots[indices[j]]<=dots[index];--j)indices[j+1]=indices[j];
        indices[j+1]=index;
    }
    for(i=0;i<4;++i){
        index=indices[i];
        position[0]=enemyPos[0]+radius*offsets[index][0];
        position[1]=enemyPos[1]+radius*offsets[index][1];position[2]=enemyPos[2];
        occupier=claimant(&target->slot[index]);
        if(occupier && distance2(dog->r.currentOrigin,position)>=distance2(occupier->r.currentOrigin,position))continue;
        G_TraceCapsule(&tr,position,attackMins,attackMaxs,position,enemy->s.number,0x20000);
        if(!tr.startsolid && !tr.allsolid)
            G_TraceCapsule(&tr,position,attackMins,attackMaxs,enemyPos,enemy->s.number,0x2800011);
        if(tr.fraction<1 || tr.startsolid || tr.allsolid){
            if(!tr.startsolid && !tr.allsolid && tr.fraction>bestFraction){bestFraction=tr.fraction;memcpy(best,position,12);}
            continue;
        }
        memcpy(drop,position,12);drop[2]-=90;
        G_TraceCapsule(&tr,position,attackMins,actorMaxs,drop,enemy->s.number,0x2800011);
        if(tr.fraction<1 && !tr.startsolid && !tr.allsolid){
            position[2]-=90*tr.fraction;position[2]+=1;
            target->slot[index].entity=dog->s.number;
            target->slot[index].generation=dog->useCount;return 1;
        }
    }
    if(bestFraction<=0.95f)return 0;
    memcpy(position,best,12);return 1;
}
static void combat_goal(gentity_t *dog,gentity_t *enemy,float attackDistance,
                        int enemyInSyncedMelee,dog_attack_goal_t *out)
{
    float buffer,height,a[2],b[2];int i;trace_t tr;
    memcpy(out->enemyPosition,enemy->r.currentOrigin,12);buffer=attackDistance+15;
    if(enemy->client){
        const float *velocity=enemy->client->ps.velocity;
        if(velocity[0]*velocity[0]+velocity[1]*velocity[1]>1){
            for(i=0;i<3;++i)out->enemyPosition[i]+=0.25f*velocity[i];
            buffer+=15;
        }
        if(enemyInSyncedMelee)out->enemyPosition[2]+=64;
    }
    height=dog->r.currentOrigin[2]-out->enemyPosition[2];
    out->inAttackRange=height*height<=80*80 && distance2(dog->r.currentOrigin,out->enemyPosition)<=buffer*buffer;
    out->goalSet=attack_spot(dog,enemy,out->enemyPosition,attackDistance,out->goal);
    if(out->goalSet && out->inAttackRange){
        direction(out->goal,out->enemyPosition,a);direction(dog->r.currentOrigin,out->enemyPosition,b);
        out->inAttackRange=a[0]*b[0]+a[1]*b[1]>0.707f;
    }else if(out->inAttackRange){
        G_TraceCapsule(&tr,dog->r.currentOrigin,attackMins,actorMaxs,out->enemyPosition,enemy->s.number,0x2820011);
        out->inAttackRange=(tr.hitType==TRACE_HITTYPE_ENTITY && tr.hitId==enemy->s.number) ||
            (tr.fraction>=1 && !tr.startsolid && !tr.allsolid);
        if(diag_enabled())((void)0);
    }
    if(diag_enabled() && level.time-diag_time[dog->s.number]>=500){
        float d=sqrtf(distance2(dog->r.currentOrigin,out->enemyPosition));
        diag_time[dog->s.number]=level.time;
        ((void)0);
    }
}
/* The asking dog is not an obstacle to its own attack decision.  Every trace
 * above starts at the dog (the no-spot line to the enemy) or at an attack
 * spot the dog is standing on once it has arrived, and the dog's own
 * CONTENTS_BODY capsule is in their masks.  MEASURED 2026-10-02 (mp_rust,
 * frozen targets): a dog that reached its spot rejected that spot as
 * startsolid in itself, fell to the no-spot branch, whose trace was
 * startsolid in itself too (hitId == the dog), and stood barking 100 units
 * from the target for the rest of the match.  T5 traces these for the
 * geometry and the other actors around the target, so only the asking dog
 * is taken out; other dogs still block, as in T5. */
void DogCombatGoal(gentity_t *dog,gentity_t *enemy,float attackDistance,
                        int enemyInSyncedMelee,dog_attack_goal_t *out)
{
    int contents;
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!valid_entity(dog) || !valid_entity(enemy) || dog==enemy ||
       dog->health<=0 || enemy->health<=0 || !isfinite(attackDistance) || attackDistance<=0)return;
    contents=dog->r.contents;dog->r.contents=0;
    combat_goal(dog,enemy,attackDistance,enemyInSyncedMelee,out);
    dog->r.contents=contents;
}
/* The zm hellhound.  T5 zombie mode's hounds take no attack spot: MEASURED
 * (t5cap, zombie_cod5_sumpf, six hounds at a frozen player, 2026-10-22) the
 * first hound runs in until its body touches the player (bites from 31-34
 * units) and the others queue behind it in a line, 64-77, 100 and 137 units
 * out, each stopped by the hound in front, and bite from there (dog_combat
 * at 68-75).  The four meleeAttackDist spots of actor_dog_exposed.cpp put
 * every hound on a ring 59-62 units out instead.  So: chase the player
 * itself; attack once touching, or once inside meleeAttackDist + 15 (T5
 * Actor_Dog_GetEnemyPos's buffer) with another body between the hound and
 * the player. */
#define ZM_HOUND_TOUCH 36.0f
void ZmHoundCombatGoal(gentity_t *dog,gentity_t *enemy,float attackDistance,
                             dog_attack_goal_t *out)
{
    float buffer,height,d2;trace_t tr;int contents;
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!valid_entity(dog) || !valid_entity(enemy) || dog==enemy ||
       dog->health<=0 || enemy->health<=0 || !isfinite(attackDistance) || attackDistance<=0)return;
    memcpy(out->enemyPosition,enemy->r.currentOrigin,12);
    buffer=attackDistance+15;
    height=dog->r.currentOrigin[2]-out->enemyPosition[2];
    d2=distance2(dog->r.currentOrigin,out->enemyPosition);
    if(height*height>80*80 || d2>buffer*buffer)return;
    if(d2<=ZM_HOUND_TOUCH*ZM_HOUND_TOUCH){out->inAttackRange=1;return;}
    contents=dog->r.contents;dog->r.contents=0;
    G_TraceCapsule(&tr,dog->r.currentOrigin,attackMins,actorMaxs,out->enemyPosition,enemy->s.number,0x2820011);
    dog->r.contents=contents;
    out->inAttackRange=tr.hitType==TRACE_HITTYPE_ENTITY && tr.hitId<MAX_GENTITIES &&
        tr.hitId!=enemy->s.number && g_entities[tr.hitId].health>0 && tr.fraction<1;
    if(diag_enabled() && level.time-diag_time[dog->s.number]>=500){
        diag_time[dog->s.number]=level.time;
        Com_Printf(CON_CHANNEL_SERVER,"CoDOL dogdiag: zm hound %d d2d %.0f blocked-by %d -> %d\n",
            dog->s.number,sqrtf(d2),tr.hitType==TRACE_HITTYPE_ENTITY?(int)tr.hitId:-1,out->inAttackRange);
    }
}
/* Every species but the dog: T5's humanoid melee approach.  T5 zombies have
 * no attack spots -- the four spots above are actor_dog_exposed.cpp only.
 * A zombie paths at its enemy (find_flesh sets the goal on the player,
 * goalradius 32) and Actor_Exposed_Think hands it the combat animscript once
 * it has no path left (OpenBLOPS actor_exposed.cpp, Actor_Zombie_ExposedCombat),
 * whose TryMelee (T5 animscripts/zombie_combat.gsc:273) swings when
 * zombie_melee::CanMeleeInternal passes: the enemy within anim.meleeRange
 * (64, the actor's meleeAttackDist, _zombiemode_spawner.gsc:3910) and
 * isMeleePathClear -- the hull can move to the point 28 units short of the
 * enemy (maymovetopoint), or failing that the knee (+20) and head (+72)
 * lines to it are clear (zombie_melee.gsc:467).  The goal is never set
 * here (goalSet 0): the caller chases the enemy itself.
 * Until 2026-10-02 zombies ran the dog code too (reported on zm_prototype_sh:
 * a zombie given the far-side spot walked round or into the player for it).
 * Other actors are not obstacles to the swing (T5 zombies swarm a target),
 * so the traces use the world/clip mask without CONTENTS_BODY. */
#define MELEE_PATH_MASK 0x820011
void ActorMeleeRangeGoal(gentity_t *actor,gentity_t *enemy,float attackDistance,
                               dog_attack_goal_t *out)
{
    static const float zero[3]={0,0,0};
    float v[3],flat,point[3],a[3],b[3];
    trace_t tr;int i;
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!valid_entity(actor) || !valid_entity(enemy) || actor==enemy ||
       actor->health<=0 || enemy->health<=0 || !isfinite(attackDistance) || attackDistance<=0)return;
    memcpy(out->enemyPosition,enemy->r.currentOrigin,12);
    for(i=0;i<3;++i)v[i]=enemy->r.currentOrigin[i]-actor->r.currentOrigin[i];
    if(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]>attackDistance*attackDistance)return;
    flat=sqrtf(v[0]*v[0]+v[1]*v[1]);
    if(flat<=28.0f){out->inAttackRange=1;return;}
    point[0]=enemy->r.currentOrigin[0]-v[0]/flat*28.0f;
    point[1]=enemy->r.currentOrigin[1]-v[1]/flat*28.0f;
    point[2]=enemy->r.currentOrigin[2];
    G_TraceCapsule(&tr,actor->r.currentOrigin,actor->r.mins,actor->r.maxs,point,actor->s.number,MELEE_PATH_MASK);
    if(!tr.startsolid && !tr.allsolid && tr.fraction>=1.0f){out->inAttackRange=1;return;}
    memcpy(a,actor->r.currentOrigin,12);memcpy(b,point,12);a[2]+=20;b[2]+=20;
    G_TraceCapsule(&tr,a,zero,zero,b,actor->s.number,MELEE_PATH_MASK);
    if(tr.startsolid || tr.allsolid || tr.fraction<1.0f)return;
    a[2]+=52;b[2]+=52;
    G_TraceCapsule(&tr,a,zero,zero,b,actor->s.number,MELEE_PATH_MASK);
    out->inAttackRange=!tr.startsolid && !tr.allsolid && tr.fraction>=1.0f;
}
#endif

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

void PersistentDogClaimsReset(void) {
    DogCombatReset();memset(diag_time,0,sizeof(diag_time));
}

int PersistentDogClaimsEmpty(void) {return resident_static_zero(targets,sizeof(targets)) && resident_static_zero(diag_time,sizeof(diag_time));}
#endif

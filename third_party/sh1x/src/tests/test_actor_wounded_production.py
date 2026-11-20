"""Run actual native wounded lifecycle functions with explicit VM/damage boundaries."""
from pathlib import Path
import re, subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[1]

def function(source, name):
    m = re.search(r'^(?:extern "C" )?(?:static )?(?:void|int) '+name+r'\([^;]*?\)\s*\{', source, re.M)
    assert m, name
    end = m.end(); depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}'); end += 1
    return source[m.start():end]

class ActorWoundedProduction(unittest.TestCase):
    def test_live_damage_bleedout_interruption_and_slot_ownership(self):
        source = (ROOT/'src/online/player/actor_lifecycle.cpp').read_text()
        funcs = '\n'.join(function(source, n) for n in ['ActorWoundedFrame', 'ActorBeginWounded', 'ActorPain', 'ActorDie', 'ActorFinishWounded'])
        fixture = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#define ET_ACTOR 18
#define ENTITYNUM_WORLD 63
#define VAR_DEAD_THREAD 2
#define MPDOG_EV_DEATH 1
using hitLocation_t=int;
struct scr_entref_t {unsigned short entnum,classnum;};
struct gentity_s {struct{int number,eType,eFlags,solid;}s;struct{int inuse,contents;float currentOrigin[3];}r;int health,maxHealth,takedamage,useCount,clipmask;};
struct actor_lifetime_t {gentity_s *entity;int wounded,woundAttackerNum,woundAttackerUseCount,woundMod,woundWeapon,woundHitLoc;float woundDirection[3];int deathTime,allowPain,painPending,safeToChangeScript,rideNum,negotiating,scripted,animFunction,animThread,hasGoal,routeCount,routeCursor,coverIndex,coverArrived;struct{int pain;}scripts;};
static gentity_s g_entities[64];static actor_lifetime_t actor;
static struct {int time;}level;
static struct {float value;}distance;
static auto ai_eventDistDeath=&distance;
static float vec3_origin[3];
static int argc,result,errors,eligible=1,threadDead,advanced,coverReleased,killed,deathNotifies,damageNotifies,damageCalls,lastAttacker,actorDamageFinishing;
static actor_lifetime_t *ActorMethodReceiver(scr_entref_t ref){if(ref.classnum || ref.entnum!=3){errors++;return nullptr;}return &actor;}
static actor_lifetime_t *ActorForEntity(gentity_s *e){return e==actor.entity?&actor:nullptr;}
static int ActorIsCoopSpecies(actor_lifetime_t*){return eligible;}
static int Scr_GetNumParam(){return argc;}
static void Scr_Error(const char*){++errors;}
static void Scr_AddBool(bool value){result=value;}
static void CoverRelease(int cover,int ent){assert(cover==4&&ent==3);++coverReleased;}
static int Scr_GetObjectType(unsigned short){return threadDead?VAR_DEAD_THREAD:1;}
static int ActorAdvanceGoalAnimation(gentity_s *e,int t,int goal){assert(e==actor.entity&&t==level.time&&!goal);++advanced;return 1;}
static void ActorRecordDamage(gentity_s*,gentity_s*,const float*){}
static int ActorIsMpDog(actor_lifetime_t*){return 0;}
static int ActorClientNum(gentity_s*){return -1;}
static void MpDogBroadcast(int,gentity_s*,const float*,float,gentity_s*){assert(false);}
static void DogCombatRelease(gentity_s*){}
static void ActorKilledCallback(gentity_s*,gentity_s*,gentity_s*,int,int,int,const float*,int,int){++killed;}
extern "C" void ActorPain(gentity_s*,gentity_s*,int,const float*,int,const float*,hitLocation_t,int);
extern "C" void ActorDie(gentity_s*,gentity_s*,gentity_s*,int,int,int,const float*,hitLocation_t,int);
static void G_Damage(gentity_s *e,gentity_s *inflictor,gentity_s *attacker,const float *dir,const float *point,int damage,int,int mod,int weapon,int hitLoc,unsigned,unsigned,int){
 ++damageCalls;lastAttacker=attacker->s.number;
 if(!e->takedamage||e->health<=0)return;
 e->health-=damage;++damageNotifies;
 if(e->health<=0){++deathNotifies;ActorDie(e,inflictor,attacker,damage,mod,weapon,dir,hitLoc,0);}
 else ActorPain(e,attacker,damage,point,mod,dir,hitLoc,weapon);
}
'''
        cases = r'''
static scr_entref_t ref={3,0};
static void setup(){memset(g_entities,0,sizeof(g_entities));memset(&actor,0,sizeof(actor));
 argc=result=errors=threadDead=advanced=coverReleased=killed=deathNotifies=damageNotifies=damageCalls=0;eligible=1;actorDamageFinishing=0;level.time=1000;g_entities[ENTITYNUM_WORLD].s.number=ENTITYNUM_WORLD;
 auto e=&g_entities[3];e->s.number=3;e->s.eType=ET_ACTOR;e->s.solid=0x68010f;e->r.inuse=1;e->r.contents=0x2000000;e->clipmask=0x2820015;e->health=25;e->maxHealth=100;e->takedamage=1;
 actor.entity=e;actor.deathTime=-1;actor.allowPain=1;actor.scripts.pain=77;actor.animFunction=77;actor.animThread=9;actor.hasGoal=1;actor.routeCount=4;actor.routeCursor=2;actor.coverIndex=4;
 auto a=&g_entities[0];a->r.inuse=1;a->s.number=0;a->useCount=7;
 float direction[3]={1,0,0};ActorPain(e,a,75,nullptr,2,direction,4,133);
}
int main(){
 setup();ActorBeginWounded(ref);assert(result&&actor.wounded&&coverReleased==1&&!actor.hasGoal&&!actor.routeCount&&!actor.painPending);
 auto e=actor.entity;assert(e->health==25&&e->takedamage&&e->s.eType==18&&e->s.solid==0x68010f&&e->r.contents==0x2000000&&e->clipmask==0x2820015);
 assert(!deathNotifies&&!killed);assert(ActorWoundedFrame(e,&actor)&&advanced==1);ActorBeginWounded(ref);assert(!result&&actor.wounded);
 float direction[3]={0,1,0};G_Damage(e,&g_entities[0],&g_entities[0],direction,nullptr,5,0,2,133,4,0,0,0);
 assert(e->health==20&&actor.wounded&&e->takedamage&&!actor.painPending&&damageNotifies==1&&!deathNotifies&&!killed);
 G_Damage(e,&g_entities[0],&g_entities[0],direction,nullptr,20,0,2,133,4,0,0,0);
 assert(e->health==0&&!actor.wounded&&!e->takedamage&&deathNotifies==1&&killed==1);ActorFinishWounded(ref);assert(deathNotifies==1&&killed==1);
 setup();ActorBeginWounded(ref);ActorFinishWounded(ref);assert(actor.entity->health==0&&deathNotifies==1&&killed==1&&lastAttacker==0&&!actorDamageFinishing);
 setup();ActorBeginWounded(ref);g_entities[0].useCount++;ActorFinishWounded(ref);assert(lastAttacker==ENTITYNUM_WORLD&&killed==1);
 setup();ActorBeginWounded(ref);e=actor.entity;e->takedamage=0;ActorFinishWounded(ref);assert(e->health==25&&actor.wounded&&!killed&&!deathNotifies);threadDead=1;assert(!ActorWoundedFrame(e,&actor)&&!actor.wounded&&actor.safeToChangeScript);
 setup();actor.entity->health=0;ActorBeginWounded(ref);assert(!result&&actor.entity->health==0&&!actor.wounded);
 setup();actor.entity->health=40;ActorBeginWounded(ref);assert(!result&&!actor.wounded);
 setup();actor.entity->maxHealth=1000;actor.entity->health=101;ActorBeginWounded(ref);assert(!result);
 setup();actor.animFunction=44;ActorBeginWounded(ref);assert(!result);
 setup();eligible=0;ActorBeginWounded(ref);assert(!result);
 setup();actor.entity->takedamage=0;ActorBeginWounded(ref);assert(!result);
 setup();actor.rideNum=2;ActorBeginWounded(ref);assert(!result);
 setup();argc=1;ActorBeginWounded(ref);assert(errors==1&&!actor.wounded);
 setup();ref.classnum=1;ActorBeginWounded(ref);assert(errors==1&&!actor.wounded);ref.classnum=0;
 puts("PASS actual native live wounded health/identity/collision, held AI animation, damage, single final death/bleedout, attribution generation, refusal/interruption cleanup");
}
'''
        frame = function(source, 'ActorRunFrameBody')
        self.assertLess(frame.index('if (entity->health <= 0)'), frame.index('ActorWoundedFrame(entity,a)'))
        with tempfile.TemporaryDirectory(prefix='actor-wounded-') as tmp:
            cpp=Path(tmp)/'wounded.cpp'; exe=Path(tmp)/'wounded'; cpp.write_text(fixture+funcs+cases)
            subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
if __name__ == '__main__': unittest.main()

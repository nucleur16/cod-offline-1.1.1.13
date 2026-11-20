"""Compile actual native dog threat selection with deterministic engine boundaries."""
import pathlib,re,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
def body(source,name):
    m=re.search(r'^static [^\n]*\b'+name+r'\([^\n]*\)\s*\{',source,re.M)
    assert m,name
    end=m.end(); depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[m.start():end]
class DogThreatProduction(unittest.TestCase):
    def test_mixed_wave_admission_and_native_senses(self):
        source=(ROOT/'src/online/player/actor_lifecycle.cpp').read_text()
        functions='\n'.join(body(source,n) for n in ('ActorMayTarget','ActorClientNum','MpDogKnow','ActorAttackerCount','ActorUpdateMpDogThreat'))
        harness=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include "actor_threat_limits.h"
#define MAX_CLIENTS 2
#define MAX_ACTOR_SLOTS 32
#define SESS_STATE_PLAYING 0
#define TRACE_HITTYPE_ENTITY 1
#define MPDOG_EV_NEW_ENEMY 0
struct client_t {struct {int sessionState;} sess;};
struct gentity_s {struct {int number;} s; struct {int inuse;float currentOrigin[3],mins[3],maxs[3];} r; client_t *client;int health,maxHealth,useCount,clipmask;};
struct sentient_t {gentity_s *entity;int team,ownerNum,isActor,dontTarget;};
struct actor_lifetime_t {gentity_s *entity;sentient_t *sentient;int initialized,enemyNum;unsigned enemyGeneration;int mpNotifyTime,mpVisibleTime[2],mpKnownTime[2],mpAttackedTime[2],diagCandidates,enemySeenTime,mpHadEnemy;};
struct trace_t {int startsolid,allsolid,hitType,hitId;float fraction;};
static struct {int time,maxclients;} level;
static struct {int integer;float value;} cap={2,0},dist={0,1000};
static auto ai_maxAttackerCount=&cap;static auto ai_eventDistNewEnemy=&dist;
static gentity_s g_entities[40];static actor_lifetime_t actors[32];static sentient_t sentients[40];
static const char *category;static int occluded,ignored;
static const char *ServerZoneType(){return category;}
static actor_lifetime_t *ActorForEntity(gentity_s *e){for(auto &a:actors)if(a.entity==e)return &a;return nullptr;}
static gentity_s *ActorEnemy(gentity_s *e){auto a=ActorForEntity(e);return a->enemyNum>=0?&g_entities[a->enemyNum]:nullptr;}
static int ActorSetEnemy(gentity_s *e,gentity_s *p){auto a=ActorForEntity(e);a->enemyNum=p->s.number;a->enemyGeneration=p->useCount;return 1;}
static sentient_t *Sentient_ForEntity(gentity_s *p){return &sentients[p->s.number];}
static void G_TraceCapsule(trace_t *t,const float*,const float*,const float*,const float*,int,int){*t={0,0,0,0,occluded?0.0f:1.0f};}
static void MpDogBroadcast(int,gentity_s*,const float*,float,gentity_s*){}
static float MpDogFootstepRadius(gentity_s*){return 0;}
static int ActorIgnoresGroup(actor_lifetime_t*,sentient_t*){return ignored;}
static int ActorThreatScore(actor_lifetime_t*,sentient_t*,float){return 0;}
static void MpDogLoseEnemy(gentity_s *e,actor_lifetime_t*){ActorForEntity(e)->enemyNum=-1;}
'''
        cases=r'''
static void setup(const char *mode,int infantry){
 memset(actors,0,sizeof(actors));memset(g_entities,0,sizeof(g_entities));memset(sentients,0,sizeof(sentients));
 static client_t client;category=mode;level={1000,1};ignored=occluded=0;
 auto p=&g_entities[0];p->s.number=0;p->r.inuse=1;p->client=&client;p->health=p->maxHealth=100;p->useCount=7;
 sentients[0].entity=p;sentients[0].team=1;
 for(int i=0;i<=infantry;i++){auto e=&g_entities[i+2];e->s.number=i+2;e->health=100;actors[i].initialized=1;actors[i].entity=e;actors[i].sentient=&sentients[i+2];actors[i].sentient->entity=e;actors[i].sentient->team=2;actors[i].enemyNum=i?0:-1;actors[i].enemyGeneration=7;}
}
int main(){
 // Actual production function counts four infantry, then admits the dog.
 setup("mp",4);assert(ActorAttackerCount(&g_entities[0],&actors[0])==4);ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==-1);
 setup("survival",4);ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==0 && actors[0].enemySeenTime==1000);
 setup("survival",31);ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==0);
 // Keep hostility, senses, ignore groups, live player and session gates.
 setup("survival",4);sentients[0].team=2;ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==-1);
 setup("survival",4);occluded=1;ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==-1);
 setup("survival",4);ignored=1;ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==-1);
 setup("survival",4);g_entities[0].health=0;ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==-1);
 setup("survival",4);g_entities[0].client->sess.sessionState=1;ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==-1);g_entities[0].client->sess.sessionState=0;
 setup("mp",4);actors[0].enemyNum=0;ActorUpdateMpDogThreat(actors[0].entity);assert(actors[0].enemyNum==0);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path=pathlib.Path(tmp)/'threat.cpp';path.write_text(harness+functions+cases)
            exe=pathlib.Path(tmp)/'threat'
            subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'src/online/player'),str(path),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
if __name__=='__main__':unittest.main()

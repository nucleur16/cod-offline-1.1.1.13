#include "actor_state.h"
#include "actor_lifecycle.h"
#include "late_corpse.h"
#include "configstrings.h"
#include "g_shared.h"
#include "server.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
level_locals_t level;
static gentity_s entity;
static sentient_t sentient;
static gclient_t viewer;
extern "C" int G_TagIndex(const char *){return 1;}
extern "C" const char *SL_ConvertToString(unsigned int){return "tag";}
bool EntHandle::isDefined(){return false;}
int EntHandle::entnum(){return 0;}
extern "C" int ServerModelToWire(unsigned model){return (int)model;}
extern "C" int ServerTagToWire(unsigned tag){return (int)tag;}
extern "C" unsigned ActorEnemyModel(gentity_s *){return 0;}
extern "C" sentient_t *Sentient_ForEntity(gentity_s *e){return e==&entity?&sentient:NULL;}
extern "C" sentient_t *Sentient_ActorBySlot(int slot){return slot==0?&sentient:NULL;}
extern "C" int LateCorpseAppendRecords(unsigned,uint32_t,actor_state_t *,unsigned,unsigned *,
                         int (*)(actor_state_t *,int,gentity_s *)){return 1;}
static unsigned word(const actor_state_t &s,unsigned offset)
{unsigned value;memcpy(&value,s.data+offset,4);return value;}
int main()
{
    actor_state_t out[32];unsigned count;
    entity.r.inuse=1;entity.s.number=100;entity.model=3;
    sentient.entity=&entity;sentient.team=SENTIENT_TEAM_AXIS;
    level.clients=&viewer;viewer.sess.cs.team=TEAM_BLUE;
    assert(ProduceActorStates(out,32,&count) && count==1 && word(out[0],8)==1);
    assert(out[0].actorTeam==1);
    assert(ArchiveSnapshotActors(7,350));
    sentient.team=SENTIENT_TEAM_ALLIES;
    assert(CaptureClientSnapshotActors(0,11,-1,0));
    assert(CopyClientSnapshotActors(0,11,out,32,&count) && word(out[0],8)==2);
    assert(CaptureClientSnapshotActors(0,12,7,350));
    assert(CopyClientSnapshotActors(0,12,out,32,&count) && word(out[0],8)==1 && out[0].actorTeam==1);
    assert(!CaptureClientSnapshotActors(0,13,7,351));
    sentient.team=SENTIENT_TEAM_COUNT;
    assert(!ProduceActorStates(out,32,&count));
    sentient.team=-1;assert(!ProduceActorStates(out,32,&count));
    ClearActorArchive();ClearClientSnapshotActors(0);
    puts("PASS: C1 actor wire team comes from native sentient, validates bounds and preserves historical team across live changes");
}

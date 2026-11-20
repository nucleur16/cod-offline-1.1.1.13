/* Exact merged endpoint selector + actual native CSV/graph/route/cache.
 * Hull/step/start-ban/edge policy are explicit doubles; this is not live AI. */
extern "C" {
#include "entity.h"
#include "filesystem.h"
#include "q_math.h"
#include "bot_nav.h"
#include "cm_public.h"
}
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
typedef struct actor_lifetime_s actor_lifetime_t;
static int s_goalEndUnroutable,queries,cases,hull_index=0,edge_allowed=1,banned=-1,blocked=-1;
static char graph[4096];
#ifdef HAS_ZM_TOPOLOGY
extern "C" { gentity_t g_entities[2]; }
extern "C" clipHandle_t SV_ClipHandleForEntity(gentity_t *) { assert(0);return 0; }
extern "C" void __cdecl CM_TransformedBoxTrace(trace_t *,const float *,const float *,const float *,const float *,clipHandle_t,int,const float *,const float *) { assert(0); }
#endif
extern "C" int FS_ReadFile(const char *s,void **out) { assert(!strcmp(s,"fixture.csv"));*out=strdup(graph);return (int)strlen(graph); }
extern "C" void FS_FreeFile(void *p) { free(p); }
static int ActorStartBanned(const actor_lifetime_t *,int n) { return n==banned; }
static int ActorClearSegment(gentity_s *,const float *,const float *) { return 1; }
static int ActorRouteContinuationBlocked(gentity_s *,const float *p) { return p==BotNavPoint(blocked); }
static int ActorHullWalk(gentity_s *,const float *from,const float *) { return from==BotNavPoint(hull_index); }
#include "walkable.inc"
static int ActorRouteEdge(int from,int to,void *)
{ return edge_allowed&&ActorRouteWalkable(BotNavStance(from))&&ActorRouteWalkable(BotNavStance(to)); }
static int CountedRoute(int a,int b,unsigned short *p,int n,bot_edge_filter_t f,void *c)
{ ++queries;return BotNavRouteFiltered(a,b,p,n,f,c); }
#define BotNavRouteFiltered CountedRoute
#include "endpoint.inc"
#undef BotNavRouteFiltered
#define CHECK(x) do {assert(x);++cases;}while(0)
static void load(int far)
{
 strcpy(graph,"9\n");
 for(int i=0;i<9;++i){char line[256];snprintf(line,sizeof(line),"%d 0 0,%s,stand,,,\n",i==8?2000:(far?1000:0)+i*10,i==8?"0 1 2 3 4 5 6 7":"8");strcat(graph,line);}
 CHECK(BotNavLoad("fixture.csv")==9);queries=s_goalEndUnroutable=0;edge_allowed=1;hull_index=0;banned=blocked=-1;
}
int main(void)
{
 gentity_s entity={};float goal[3]={};const auto *actor=reinterpret_cast<const actor_lifetime_t *>(&entity);
 load(0);CHECK(ActorNearestWalkableNode(&entity,actor,goal,2,8)==0&&queries==1&&!s_goalEndUnroutable);
 load(0);hull_index=1;CHECK(ActorNearestWalkableNode(&entity,actor,goal,2,8)==1&&queries==2&&!s_goalEndUnroutable);
 load(0);edge_allowed=0;CHECK(ActorNearestWalkableNode(&entity,actor,goal,2,8)==0&&queries==1&&s_goalEndUnroutable);CHECK(BotNavLastRouteStats()->stop==1);
 for(int i=0;i<8;++i)CHECK(BotNavLastSearchReached(i)==0);
 CHECK(BotNavLastSearchReached(8)==1);
 load(1);CHECK(ActorNearestWalkableNode(&entity,actor,goal,2,8)==0&&queries==1&&!s_goalEndUnroutable);
 load(0);blocked=0;CHECK(ActorNearestWalkableNode(&entity,actor,goal,1,8)==1&&queries==0);
 load(0);banned=0;CHECK(ActorNearestWalkableNode(&entity,actor,goal,1,8)==1&&queries==0);
 load(0);CHECK(ActorNearestWalkableNode(&entity,nullptr,goal,2,8)==0&&queries==0);
#ifdef HAS_ZM_TOPOLOGY
 /* Actual Zombies cache revision and upstream reference-counted cuts must
  * agree. Reconnecting one of two owners cannot reopen the other owner's
  * blocked path. Collision contents are zero: this exercises real box cuts,
  * while transformed collision calls are guarded doubles above. */
 uint32_t bits[(BOT_NODES+31)/32];uint64_t revision=BotNavTopologyRevision();
 load(0);CHECK(BotNavTopologyRevision()!=revision);
 CHECK(BotNavReachableNodes(8,bits,sizeof(bits)/sizeof(bits[0]),nullptr,nullptr)==9);CHECK((bits[0]&511)==511);
 CHECK(BotNavReachableNodes(8,bits,0,nullptr,nullptr)==0);
 float mins[3]={995,-1,0},maxs[3]={1005,1,30};
 revision=BotNavTopologyRevision();CHECK(BotNavDisconnectPaths(0,mins,maxs)>0);CHECK(BotNavTopologyRevision()!=revision);
 revision=BotNavTopologyRevision();CHECK(BotNavDisconnectPaths(1,mins,maxs)>0);CHECK(BotNavTopologyRevision()!=revision);
 CHECK(BotNavReachableNodes(8,bits,sizeof(bits)/sizeof(bits[0]),nullptr,nullptr)==1);CHECK(bits[0]==256);
 CHECK(BotNavConnectPaths(0)>0);CHECK(BotNavLinkBlocked(8,0)>0);
 CHECK(BotNavReachableNodes(8,bits,sizeof(bits)/sizeof(bits[0]),nullptr,nullptr)==1);
 revision=BotNavTopologyRevision();CHECK(BotNavConnectPaths(1)>0);CHECK(BotNavTopologyRevision()!=revision);
 CHECK(BotNavReachableNodes(8,bits,sizeof(bits)/sizeof(bits[0]),nullptr,nullptr)==9);
 revision=BotNavTopologyRevision();CHECK(BotNavConnectPaths(1)==0&&BotNavTopologyRevision()==revision);
#endif
 printf("route_endpoint assertions=%d exact_selector_and_native_planner=PASS\n",cases);return 0;
}

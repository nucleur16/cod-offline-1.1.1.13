/* Exercise production directed flood, live barrier changes and cached ownership. */
#include "../src/online/player/bot_nav.c"
#include "actor_targeting.h"
#include <assert.h>
#include <stdio.h>
gentity_t g_entities[MAX_GENTITIES];
vec3_t vec3_origin;
clipHandle_t SV_ClipHandleForEntity(gentity_t *e){(void)e;assert(!"fixture never uses brush traces");return 0;}
void CM_TransformedBoxTrace(trace_t *t,const float *a,const float *b,const float *mi,const float *ma,
 clipHandle_t h,int mask,const float *o,const float *ang)
{(void)t;(void)a;(void)b;(void)mi;(void)ma;(void)h;(void)mask;(void)o;(void)ang;assert(!"fixture never uses brush traces");}
static int reject;
static int edge(int from,int to,void *context){(void)context;return !(reject && from==1 && to==2);}
int main(void)
{
 actor_target_cache_t cache={0};uint32_t bits[(BOT_NODES+31)/32];
 const float mins[3]={9,-5,-5},maxs[3]={11,5,5};
 count=3;memset(nodes,0,sizeof(nodes));
 nodes[0].point[0]=0;nodes[1].point[0]=10;nodes[2].point[0]=20;
 nodes[0].links[0]=1;nodes[0].count=1;nodes[1].links[0]=2;nodes[1].count=1;
 assert(BotNavReachableNodes(0,bits,sizeof(bits)/sizeof(bits[0]),NULL,NULL)==3);
 assert((bits[0]&7)==7);
 assert(ActorTargetNodeReachable(&cache,0,2,0,edge,NULL));
 assert(!ActorTargetNodeReachable(&cache,2,0,0,edge,NULL)); /* Directed graph. */
 assert(ActorTargetNodeReachable(&cache,0,2,0,edge,NULL));
 assert(BotNavDisconnectPaths(ENTITYNUM_WORLD,mins,maxs)>0);
 assert(!ActorTargetNodeReachable(&cache,0,2,0,edge,NULL));
 assert(BotNavConnectPaths(ENTITYNUM_WORLD)>0);
 assert(ActorTargetNodeReachable(&cache,0,2,0,edge,NULL));
 reject=1;assert(!ActorTargetNodeReachable(&cache,0,2,1,edge,NULL));
 reject=0;assert(ActorTargetNodeReachable(&cache,0,2,2,edge,NULL));
 assert(!ActorTargetNodeReachable(&cache,-1,2,2,edge,NULL));
 count=0;nav_changed();assert(!ActorTargetNodeReachable(&cache,0,2,2,edge,NULL));
 puts("PASS production directed target graph, barrier cut/restore, start/filter/topology invalidation and absent graph");
}

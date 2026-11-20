"""Compile the actual production archived PVS and killcam union functions.
Run on a Linux test host; boundary doubles describe BSP clusters, not camera logic.
"""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]

def function(source, name):
    start=source.index("static void "+name+"(")
    brace=source.index("{",start); depth=1; end=brace+1
    while depth:
        depth += (source[end]=="{")-(source[end]=="}"); end+=1
    return source[start:end]

HARNESS=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define MAX_GENTITIES 32
#define MAX_CLIENTS 64
#define ENTITYNUM_NONE 2047
#define MAX_SNAPSHOT_ENTITIES 8
#define SVF_NOCLIENT 1
#define qfalse 0
typedef unsigned char byte;
typedef float vec3_t[3];
typedef struct { float trBase[3]; } trajectory_t;
typedef struct { int number,type; struct { trajectory_t pos; } lerp; int loopSound,index; } entityState_t;
typedef struct { entityState_t s; struct { unsigned svFlags,clientMask[2]; float absmin[3],absmax[3]; } r; } archivedEntity_t;
typedef struct { int num_entities,first_entity,time; } cachedSnapshot_t;
typedef struct { int deltaTime,killCamEntity; } playerState_t;
typedef struct { int numSnapshotEntities,snapshotEntities[MAX_SNAPSHOT_ENTITIES]; } snapshotEntityNumbers_t;
static struct { archivedEntity_t cachedSnapshotEntities[32]; } svs;
#define Com_Memset memset
static int CM_PointLeafnum(float *origin) { return (int)origin[0]; }
static int CM_LeafCluster(int leaf) { return leaf==99 ? -1:leaf; }
static byte pvs[4];
static byte *CM_ClusterPVS(int cluster) { memset(pvs,0,sizeof(pvs));pvs[cluster>>3]|=1u<<(cluster&7);return pvs; }
static float G_GetFogOpaqueDistSqrd(void) { return 0; }
static int CM_BoxLeafnums(float *a,float *b,uint16_t *out,int capacity,int *last) { (void)b;(void)capacity;(void)last;out[0]=(uint16_t)a[0];return 1; }
static int BoxDistSqrdExceeds(float *a,float *b,float *p,float d) { (void)a;(void)b;(void)p;(void)d;return 0; }
static int SV_MaskedLoopSoundAudible(const entityState_t *s,int model) { (void)model;return s->loopSound && !s->index; }
static void BG_EvaluateTrajectory(trajectory_t *tr,int time,float *out) { assert(time==500);memcpy(out,tr->trBase,12); }
static void SV_AddEntToSnapshot(int index,snapshotEntityNumbers_t *out) { if(out->numSnapshotEntities<MAX_SNAPSHOT_ENTITIES)out->snapshotEntities[out->numSnapshotEntities++]=index; }
/*PRODUCTION*/
static int contains(snapshotEntityNumbers_t *out,int i) { for(int j=0;j<out->numSnapshotEntities;j++)if(out->snapshotEntities[j]==i)return 1;return 0; }
static void setup(void) {
 memset(&svs,0,sizeof(svs));
 for(int i=0;i<10;i++) { archivedEntity_t *e=&svs.cachedSnapshotEntities[i];e->s.number=i+10;e->s.lerp.pos.trBase[0]=2;e->r.absmin[0]=e->r.absmax[0]=2; }
 svs.cachedSnapshotEntities[0].r.absmin[0]=0;
}
int main(void) {
 cachedSnapshot_t snap={10,0,500};playerState_t ps={1000,11};snapshotEntityNumbers_t out;float eye[3]={0};
 for(int type=0;type<=6;type+=2) {
  setup();svs.cachedSnapshotEntities[1].s.type=type;out=(snapshotEntityNumbers_t){0};
  SV_AddCachedEntitiesVisibleFromPoint(10,0,eye,0,&out);assert(contains(&out,0)&&!contains(&out,1));
  SV_AddCachedKillcamEntities(&snap,&ps,0,&out);assert(contains(&out,0)&&contains(&out,1)&&contains(&out,2));
  assert(out.numSnapshotEntities<=MAX_SNAPSHOT_ENTITIES);
  for(int j=1;j<out.numSnapshotEntities;j++)assert(out.snapshotEntities[j-1]<out.snapshotEntities[j]);
 }
 setup();out=(snapshotEntityNumbers_t){0};ps.deltaTime=0;SV_AddCachedKillcamEntities(&snap,&ps,0,&out);assert(!out.numSnapshotEntities);ps.deltaTime=1000;
 setup();svs.cachedSnapshotEntities[1].r.clientMask[0]=1;SV_AddCachedKillcamEntities(&snap,&ps,0,&out);assert(!out.numSnapshotEntities);
 setup();svs.cachedSnapshotEntities[1].r.svFlags=SVF_NOCLIENT|8;SV_AddCachedKillcamEntities(&snap,&ps,0,&out);assert(!out.numSnapshotEntities);
 setup();svs.cachedSnapshotEntities[2].r.clientMask[0]=1;svs.cachedSnapshotEntities[2].s.loopSound=1;svs.cachedSnapshotEntities[3].r.svFlags=SVF_NOCLIENT|8;
 SV_AddCachedKillcamEntities(&snap,&ps,0,&out);assert(contains(&out,1)&&!contains(&out,2)&&!contains(&out,3));
 setup();out=(snapshotEntityNumbers_t){0};svs.cachedSnapshotEntities[1].s.lerp.pos.trBase[0]=99;
 SV_AddCachedKillcamEntities(&snap,&ps,0,&out);assert(out.numSnapshotEntities==1&&contains(&out,1));
 puts("PASS selected grenade/claymore/script camera, additional archived PVS, normal viewer, sorted capacity, masks, no-client, outside-BSP camera");
}
'''

def main():
    source=(ROOT/"src/sv_snapshot.c").read_text()
    production=function(source,"SV_AddCachedEntitiesVisibleFromPoint")+"\n"+function(source,"SV_AddCachedKillcamEntities")
    # Both archive branches must admit linked zero-cluster entities while
    # retaining the unconditional no-client gate; no global broadcast rewrite.
    assert source.count("if (!(gent->r.svFlags & SVF_NOCLIENT))")==2
    with tempfile.TemporaryDirectory() as folder:
        p=Path(folder);(p/"test.c").write_text(HARNESS.replace("/*PRODUCTION*/",production))
        subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-fsanitize=address,undefined",str(p/"test.c"),"-o",str(p/"test")],check=True)
        subprocess.run([str(p/"test")],check=True)
if __name__=="__main__":main()

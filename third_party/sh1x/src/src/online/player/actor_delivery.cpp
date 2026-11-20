#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Native captured-pose delivery adapter. No gameplay callbacks or live entity
 * reads while building archived recipient frames. Parent integration stage. */
#ifdef OL_SERVER
#include "actor_delivery.h"
#include "actor_lifecycle.h"
#include "late_corpse.h"
#include "q_shared.h"
#include "qcommon_io.h"
#include <stdlib.h>
#include <string.h>
#define FAIL ActorDeliveryFail
/* Why this recipient's last capture failed, kept until Prepare reports it. */
static char captureFailure[MAX_CLIENTS][256];
struct identity { unsigned entity,generation; };
struct archive_identity {
 int frame,valid; unsigned time,count; identity actors[32];
};
struct recipient_pose {
 int valid,replay; unsigned sequence,count;
 actor_anim_cut_t cut;
 dog_anim_pose_t *poses;
};
struct delivery_roster {
 unsigned timeline,time,count; int valid,replay;
 identity actors[32];
};
static archive_identity archives[ACTOR_ANIM_ARCHIVES];
static recipient_pose captures[MAX_CLIENTS];
static delivery_roster rosters[MAX_CLIENTS];
void ActorDeliveryClear(unsigned client)
{
 if(client>=MAX_CLIENTS)return;
 free(captures[client].poses);memset(&captures[client],0,sizeof(captures[client]));
 memset(&rosters[client],0,sizeof(rosters[client]));ActorAnimClientClear(client);
}
void ActorDeliveryReset(void)
{
 for(unsigned i=0;i<MAX_CLIENTS;++i)ActorDeliveryClear(i);
 memset(archives,0,sizeof(archives));DogAnimArchiveReset();ActorAnimJournalReset();
}
int ActorDeliveryArchive(int frame,unsigned time)
{
 if(frame<0)return 0;
 archive_identity *slot=&archives[(unsigned)frame%ACTOR_ANIM_ARCHIVES];slot->valid=0;
 dog_anim_capture_source_t sources[32];unsigned count=0;
 if(!ActorCaptureSources(sources,32,&count) ||
    !DogAnimArchiveStore(frame,time,sources,count) ||
    !ActorAnimArchive(frame,time))return 0;
 for(unsigned i=0;i<count;++i)slot->actors[i]={sources[i].entity,sources[i].generation};
 slot->frame=frame;slot->time=time;slot->count=count;slot->valid=1;return 1;
}
int ActorDeliveryCapture(unsigned client,unsigned sequence,int frame,unsigned time)
{
 if(client>=MAX_CLIENTS)return 0;
 recipient_pose *out=&captures[client];out->valid=0;
 actor_anim_cut_t cut;unsigned count=0;
 dog_anim_pose_t *poses=NULL;
 actor_delivery_failure=NULL;captureFailure[client][0]=0;
 if(frame>=0) {
  const archive_identity *slot=&archives[(unsigned)frame%ACTOR_ANIM_ARCHIVES];
  if(!slot->valid || slot->frame!=frame || slot->time!=time)
   FAIL("archived actor roster for frame %d time %u missing (slot valid %d frame %d time %u)",
        frame,time,slot->valid,slot->frame,slot->time);
  else if(ActorAnimArchiveCut(frame,time,&cut)) {
   count=slot->count;
   if(count && !(poses=(dog_anim_pose_t*)malloc(count*sizeof(*poses))))FAIL("out of memory");
   else for(unsigned i=0;i<count;++i) {
    const dog_anim_pose_t *pose=DogAnimArchiveFind(frame,time,slot->actors[i].entity,slot->actors[i].generation);
    if(!pose){free(poses);poses=NULL;break;}poses[i]=*pose;
   }
  }
 } else {
  dog_anim_capture_source_t sources[32];
  int ok=ActorCaptureSources(sources,32,&count);
  if(!ok)FAIL("live actor capture sources unavailable (an initialized actor lost its record or server DObj)");
  // Corpses this client is shown as actors get their pose bootstrapped from
  // the corpse tree (late_corpse.h).
  if(ok)count+=LateCorpseSources(client,sequence,sources+count,32-count);
  if(ok && count && !(poses=(dog_anim_pose_t*)malloc(count*sizeof(*poses)))){ok=0;FAIL("out of memory");}
  for(unsigned i=0;ok && i<count;++i)
   if(!DogAnimCapture(sources[i].tree,sources[i].entity,sources[i].generation,time,poses+i))ok=0;
  if(ok && !ActorAnimJournalCut(time,&cut))ok=0;
  if(!ok && !actor_delivery_failure)FAIL("live pose capture refused without a reason");
 }
 if(actor_delivery_failure) {
  Q_strncpyz(captureFailure[client],actor_delivery_failure,sizeof(captureFailure[client]));
  free(poses);return 0;
 }
 free(out->poses);out->poses=poses;out->count=count;out->sequence=sequence;
 out->cut=cut;out->replay=frame>=0;out->valid=1;return 1;
}
struct visibility { identity actors[32]; unsigned count; };
static int visible(unsigned entity,unsigned generation,void *context)
{
 visibility *v=(visibility*)context;
 for(unsigned i=0;i<v->count;++i)if(v->actors[i].entity==entity && v->actors[i].generation==generation)return 1;
 return 0;
}
int ActorDeliveryPrepare(unsigned client,unsigned sequence,unsigned acknowledged,
 const entity_state_t *entities,unsigned count,animcmd_t *out,unsigned capacity,unsigned *written)
{
 if(client>=MAX_CLIENTS || !written || (count&&!entities))return FAIL("prepare: bad arguments");
 recipient_pose *capture=&captures[client];delivery_roster *roster=&rosters[client];
 actor_delivery_failure=NULL;
 if(!capture->valid || capture->sequence!=sequence) {
  // Old ordinary archives may predate this owner. A snapshot with no actor
  // entities needs no actor pose; never invent one when actors ARE present.
  for(unsigned i=0;i<count;++i)if(entities[i].type==18)
   return FAIL("no pose capture for sequence %u (capture valid %d seq %u): %s",sequence,capture->valid,
               capture->sequence,captureFailure[client][0]?captureFailure[client]:"capture was not attempted");
  ActorDeliveryClear(client);*written=0;return 1;
 }
 visibility v={};
 // Use emitted native entity states, not the global composition table or live
 // g_entities: archived generations and PVS membership remain immutable.
 for(unsigned i=0;i<capture->count;++i)
  for(unsigned j=0;j<count;++j)
   // 18 actor; 12/14 helicopter/vehicle carry a C1 realm-0 tree at +094
   // (0x443540) and get commands for their wheel clips.
   // A script model (6) bound by script_model_tree.h names its tree at
   // the same +094 (0x4435bb).
   if((entities[j].type==18 || entities[j].type==12 || entities[j].type==14 ||
       entities[j].type==6) &&
      entities[j].number==capture->poses[i].entity) {
    v.actors[v.count++]={capture->poses[i].entity,capture->poses[i].generation};break;
   }
 int changed=!roster->valid || roster->replay!=capture->replay ||
  capture->cut.time<roster->time || roster->count!=v.count ||
  memcmp(roster->actors,v.actors,v.count*sizeof(identity));
 unsigned timeline=roster->timeline;
 if(changed && ++timeline==0)timeline=1;
 // Membership changes reconstruct current poses at their exact captured time.
 // This avoids replaying an old, already-watermarked entry when an actor enters
 // PVS. Pending bootstrap remains frozen by the journal until acknowledged.
 actor_anim_event_t *bootstrap=NULL;unsigned bootstrapCount=0;
 if(changed) {
  bootstrap=(actor_anim_event_t*)malloc(ACTOR_ANIM_SNAPSHOT_LIMIT*sizeof(*bootstrap));
  if(!bootstrap)return FAIL("prepare: out of memory");
  for(unsigned i=0;i<capture->count;++i) {
   if(!visible(capture->poses[i].entity,capture->poses[i].generation,&v))continue;
   unsigned n=0;
   if(!DogAnimBootstrap(capture->poses+i,bootstrap+bootstrapCount,
      ACTOR_ANIM_SNAPSHOT_LIMIT-bootstrapCount,&n)){free(bootstrap);return 0;}
   bootstrapCount+=n;
  }
 }
 /* MEASUREMENT (docs/ZOMBIE_ANIM_SMOOTHNESS_20260921.md): every new
  * delivery timeline re-bootstraps every visible actor's tree on the client
  * (clear, re-knob, SetTime to a 1/31-quantised phase), so its cadence is
  * what decides whether the client's own clip time ever gets yanked. Bounded
  * to 64 lines per process. */
 if(changed) {
  static unsigned timelineLines;
  if(timelineLines<64) {
   ++timelineLines;
   ((void)0);
  }
 }
 int result=ActorAnimCapture(client,sequence,acknowledged,timeline,&capture->cut,
  bootstrap,bootstrapCount,changed,visible,&v);
 free(bootstrap);
 if(!result || !ActorAnimCopy(client,sequence,out,capacity,written))return 0;
 // This roster records pending capture intent, not delivered state. On
 // serialization failure the journal keeps its immutable bootstrap; without
 // a Published/ACK history entry the next Prepare resends it. Do not commit
 // its cut here or release bootstrap based on roster equality.
 roster->valid=1;roster->timeline=timeline;roster->time=capture->cut.time;
 roster->replay=capture->replay;roster->count=v.count;
 memcpy(roster->actors,v.actors,v.count*sizeof(identity));return 1;
}
#endif

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
extern "C" int PersistentActorDeliveryEmpty(void) {return resident_storage_zero(archives,sizeof(archives)) && resident_storage_zero(captures,sizeof(captures)) && resident_storage_zero(rosters,sizeof(rosters));}

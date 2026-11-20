/* Full empty-world supplements after actual game/VM resource release. */
#include "persistent_tail.h"
#include "persistent_runtime.h"
#include "persistent_cleanup.h"
#include "persistent_entities.h"
#include "g_shared.h"
#include "entity.h"
static unsigned tail_failure;
static int tail_fail(unsigned code){if(!tail_failure)tail_failure=code;return 0;}
unsigned PersistentTailFailure(void){return tail_failure;}
int PersistentTailOwnersEmpty(void){if(!PersistentTailVoiceEmpty())return tail_fail(1);
if(!PersistentTailOverheadEmpty())return tail_fail(2);
if(!PersistentTailFontScaleEmpty())return tail_fail(3);
if(!PersistentTailObjectivesEmpty())return tail_fail(4);
if(!PersistentTailModelScaleEmpty())return tail_fail(5);
if(!PersistentTailShieldEmpty())return tail_fail(6);
if(!PersistentTailActorStepEmpty())return tail_fail(7);
if(!PersistentTailActorScriptsEmpty())return tail_fail(8);
if(!PersistentTailUnstuckEmpty())return tail_fail(9);
if(!PersistentTailRestEmpty())return tail_fail(10);
if(!PersistentTailLerpEmpty())return tail_fail(11);
if(!PersistentTailSwayEmpty())return tail_fail(12);
if(!PersistentTailBotEmpty())return tail_fail(13);
if(!PersistentTailOffhandEmpty())return tail_fail(14);
if(!PersistentTailNavEmpty())return tail_fail(15);
if(!PersistentTailAnimTablesEmpty())return tail_fail(16);
if(!PersistentTailPathNodesEmpty())return tail_fail(17);
if(!PersistentTailVehicleNodesEmpty())return tail_fail(18);
if(!PersistentTailAnimNotifyEmpty())return tail_fail(19);
if(!PersistentTailCoopEmpty())return tail_fail(20);
if(!PersistentTailArtilleryEmpty())return tail_fail(21);return 1;}
int PersistentTailOwnersReset(void){
 if(!PersistentRuntimeEnabled()||!PersistentGameVMStopped()||!PersistentEntityOwnersEmpty()||
    level.num_entities||level.firstFreeEnt||level.lastFreeEnt)return tail_fail(101);
 for(unsigned i=0;i<MAX_GENTITIES;++i)if(g_entities[i].r.inuse)return tail_fail(102);
 /* GScr_Shutdown contains these releases but is not called by the retained
  * G_ShutdownGame. Invoke the actual VM-independent heap owners explicitly,
  * after VM/entities are gone and metadata/unique ownership validate. */
 if(!PersistentTailAnimTablesCanRelease())return tail_fail(103);
 if(!PersistentTailPathNodesCanRelease())return tail_fail(104);
 if(!PersistentTailVehicleNodesCanRelease())return tail_fail(105);
 if(!PersistentTailAnimNotifyQuiet())return tail_fail(106);
 if(!PersistentTailNavCanReset())return tail_fail(107);
 PersistentTailAnimTablesReset();PersistentTailPathNodesReset();PersistentTailVehicleNodesReset();
 PersistentTailVoiceReset();
PersistentTailOverheadReset();
PersistentTailFontScaleReset();
PersistentTailObjectivesReset();
PersistentTailModelScaleReset();
PersistentTailShieldReset();
PersistentTailActorStepReset();
PersistentTailActorScriptsReset();
PersistentTailUnstuckReset();
PersistentTailRestReset();
PersistentTailLerpReset();
PersistentTailSwayReset();
PersistentTailBotReset();
PersistentTailOffhandReset();
PersistentTailNavReset();
PersistentTailAnimNotifyReset();
PersistentTailCoopReset();
PersistentTailArtilleryReset();
 return PersistentTailOwnersEmpty();
}

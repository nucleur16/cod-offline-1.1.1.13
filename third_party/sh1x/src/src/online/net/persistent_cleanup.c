/* Full empty-world teardown without endpoint crypto/socket shutdown.
 * Each destructive operation runs once for an identity-bound reset. Remaining
 * cleanup groups deliberately keep the room out of Idle until independently
 * implemented and verified. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_cleanup.h"
#include "persistent_events.h"
#include "persistent_runtime.h"
#include "persistent_clean_state.h"
#include "persistent_cvars.h"
#include "persistent_ai.h"
#include "persistent_statics.h"
#include "persistent_room.h"
#include "persistent_world.h"
#include "persistent_entities.h"
#include "persistent_tail.h"
#include "match_report.h"
#include "connect.h"
#include "ol_profile.h"
#include "ol_stats.h"
#include "matchdata_server.h"
#include "actor_delivery.h"
#include "late_corpse.h"
#include "actor_state.h"
#include "playerstate_ext.h"
#include "entitystate_ext.h"
#include "script_slowmotion.h"
#include "script_notify.h"
#include "server.h"
#include "cvar.h"
#include "g_shared.h"
#include "entity.h"
#include <string.h>

extern void SV_ClearServer(void);
static uint64_t reset_world,reset_epoch;
static int torn_down,failed;
static unsigned failure;
unsigned PersistentCleanupFailure(void) {return failure;}
static int reject(unsigned code) {if(!failure)failure=code;return -1;}

static int empty(unsigned *bots)
{
    unsigned i;
    if(!com_sv_running||!sv_maxclients||sv_maxclients->integer<1||
       sv_maxclients->integer>MAX_CLIENTS)return 0;
    *bots=0;
    for(i=0;i<MAX_CLIENTS;++i)if(svs.clients[i].state>=CS_CONNECTED) {
        if(svs.clients[i].netchan.remoteAddress.type!=NA_BOT)return 0;
        ++*bots;
        /* SV_RemoveAllBots walks the current limit only. Refuse a hidden bot
         * rather than clearing live storage the real owner would not free. */
        if(i>=(unsigned)sv_maxclients->integer)return 0;
    }
    return 1;
}
static int entities_gone(void)
{
    unsigned i;
    if(level.num_entities||level.firstFreeEnt||level.lastFreeEnt)return 0;
    for(i=0;i<MAX_GENTITIES;++i)if(g_entities[i].r.inuse)return 0;
    return 1;
}
static int profiles_gone(void)
{
    return PersistentSecureProfilesEmpty() && PersistentPublicProfilesEmpty() &&
        PersistentProfileUpdatesEmpty() && PersistentScriptProfilesEmpty() &&
        PersistentClientMatchDataEmpty() && PersistentServerMatchDataEmpty();
}
static int snapshots_gone(void)
{
    return PersistentSnapshotHistoriesEmpty() && PersistentHudCapturesEmpty() &&
        PersistentNativeFramesEmpty() && PersistentActorCapturesEmpty() &&
        PersistentActorDeliveryEmpty() && PersistentLateCorpsesEmpty() &&
        PersistentDogPosesEmpty() && PersistentActorJournalEmpty() &&
        PersistentPlayerCapturesEmpty() && PersistentEntityCapturesEmpty();
}
static int script_timers_gone(void)
{
    return !level.savePersist && PersistentScriptQueuesEmpty() &&
        PersistentSlowMotionEmpty() && PersistentCommandNotificationsEmpty();
}
int PersistentCleanupEmpty(uint64_t expected_world,uint64_t expected_epoch)
{
    persistent_event_snapshot_t event;
    unsigned i,bots;
    if(!PersistentRuntimeEnabled()||!expected_world||!expected_epoch||
       !PersistentEventsSnapshot(&event)||event.world!=expected_world||
       event.cleanup_epoch!=expected_epoch||event.ready)return reject(1);
    if(!PersistentDeferredTransitionsEmpty())return reject(2);
    if(failed)return reject(3);
    if(reset_world && (reset_world!=expected_world||reset_epoch!=expected_epoch)) {
        if(!torn_down)return reject(4);
        reset_world=reset_epoch=0;torn_down=0;
    }
    if(!empty(&bots))return 0;
    if(torn_down) {
        /* A retry rechecks facts, never repeats VM/resource destruction. */
        if(bots||com_sv_running->boolean||sv.state!=SS_DEAD||svs.initialized)return reject(5);
        if(!PersistentGameVMStopped())return reject(6);
        if(!entities_gone())return reject(7);
        if(!profiles_gone())return reject(8);
        if(!snapshots_gone())return reject(9);
        if(!script_timers_gone())return reject(10);
        if(!PersistentCvarsEmpty())return reject(11);
        if(!PersistentAiEmpty())return reject(12);
        if(!PersistentInputOwnersEmpty())return reject(13);
        if(!PersistentRoomOwnersEmpty())return reject(14);
        if(!PersistentWorldOwnersEmpty())return reject(16);
        if(!PersistentEntityOwnersEmpty())return reject(18);
        if(!PersistentTailOwnersEmpty())return reject(20);
        return 1;
    }
    if(!MatchReportPrepareReset())return 0;
    reset_world=expected_world;reset_epoch=expected_epoch;
    SV_RemoveAllBots();
    if(!empty(&bots)||bots) {failed=1;return reject(22);}
    PersistentEventStep(PS_BOTS);
    /* An independent room cannot retain the previous GSC game object. This
     * controls the actual Scr_FreeGameVariable path in G_ShutdownGame. */
    G_SetSavePersist(0);
    SV_ShutdownGameProgs();
    if(!PersistentGameVMStopped()||!entities_gone()) {failed=1;return reject(23);}
    if(!PersistentAiReset()) {failed=1;return reject(24);}
    PersistentEventStep(PS_VM);
    PersistentEventStep(PS_ENTITIES);
    SV_DemoSystemShutdown();
    SV_FreeClients(); /* also clears the native verified-connection cache */
    /* Carried profiles and connect UIDs intentionally survive ordinary map
     * rotation, but may not confer ownership of a different private room. */
    for(i=0;i<MAX_CLIENTS;++i) {
        SetClientConnectIdentity(i,0);
        DropCarriedSecureProfile(i);
        ResetClientStats(i);
    }
    SV_ClearServer(); /* full existing report/profile/replication/input reset */
    PersistentInputOwnersReset();
    if(!PersistentInputOwnersEmpty()) {failed=1;return reject(25);}
    if(!PersistentRoomOwnersReset()) {failed=1;return reject(15);}
    if(!PersistentWorldOwnersReset()) {failed=1;return reject(17);}
    if(!PersistentEntityOwnersReset()) {failed=1;return reject(19);}
    if(!PersistentTailOwnersReset()) {failed=1;return reject(21);}
    /* Ordinary full map load releases this schema but retains its selected
     * name, record and submission view. A different private room starts empty. */
    ResetServerMatchData();
    /* These archival owners normally reset during next-map initialization.
     * A resident idle slot clears them now, before a new room is reserved. */
    ActorDeliveryReset();LateCorpseReset();ClearActorArchive();
    for(i=0;i<MAX_CLIENTS;++i)ClearClientSnapshotActors(i);
    PlayerStateExtClearAll();EntityStateExtClearAll();
    SlowMotionReset();ClearCommandNotifications();
    if(!PersistentCvarsRestore(PersistentRuntimeRequest())) {failed=1;return reject(26);}
    if(!profiles_gone()) {failed=1;return reject(27);}
    if(!snapshots_gone()) {failed=1;return reject(28);}
    PersistentEventStep(PS_PROFILES);
    for(i=0;i<MAX_CLIENTS;++i)if(ClientConnectIdentity(i)) {failed=1;return reject(30);}
    Cvar_SetBool(com_sv_running,qfalse);
    memset(&svs,0,sizeof(svs));
    torn_down=1;
    if(!empty(&bots)||bots||com_sv_running->boolean||sv.state!=SS_DEAD||svs.initialized||
       !PersistentGameVMStopped()||!entities_gone()||!profiles_gone()||
       !snapshots_gone()||!script_timers_gone()||!PersistentCvarsEmpty()||
       !PersistentAiEmpty()||!PersistentInputOwnersEmpty()||
       !PersistentRoomOwnersEmpty()||!PersistentWorldOwnersEmpty()||
       !PersistentEntityOwnersEmpty()||!PersistentTailOwnersEmpty()) {failed=1;return reject(29);}
    PersistentEventStep(PS_SNAPSHOTS);
    /* VM scheduler/notification and common-clock owners are gone. Remaining
     * native AI/player/world timers are part of MATCH_STATICS, still required. */
    PersistentEventStep(PS_TIMERS);
    PersistentEventStep(PS_MATCH_STATICS);
    /* No SV_Shutdown, final disconnect message, master shutdown, network reset,
     * endpoint counter reset or socket close. This owned process remains live. */
    return 1;
}

/* Source-native resident adapter. No gameplay hooks, no installed runtime edits.
 * Full world retirement and unbound cancellation are separate proof paths.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_runtime.h"
#include "persistent_policy.h"
#include "persistent_assignment.h"
#include "persistent_events.h"
#include "persistent_reports.h"
#include "persistent_cleanup.h"
#include "persistent_tail.h"
#include "persistent_cvars.h"
#include "match_report.h"
#include "connect.h"
#include "ol_profile.h"
#include "ol_stats.h"
#include "matchdata_server.h"
#include "persistent_build.h"
#include "server.h"
#include "cvar.h"
#include "cmd.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "sec_crypto.h"
#include "scr_vm.h"
#include "g_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern int Scr_IsSystemActive(unsigned char);
static persistent_policy_t policy;
static persistent_assignment_t assigned;
static persistent_reports_t reports;
static char accepted_digest[65];
static cvar_t *root,*slot,*catalogue;
static int registered,enabled,quarantined,ever_world;
static int idle_loading,idle_ready;
static persistent_assignment_t idle_world;
static uint64_t world,assigned_world;
static uint64_t grant_sequence;
static char grant_digest[65];
static struct {
    char request[65],lease[33],scope[65],report[65];
    uint64_t epoch,world;
} completed;
static struct {
    char request[65],lease[33];
    uint64_t epoch,world,basis_epoch;
} cancelled;
static struct {
    uint64_t requested,completed,deadline;
    uint64_t last_frame;
    int pending,queued,permit,entered;
} round_restart;

static uint64_t now(void) { time_t n=Com_GetRealtime();return n>0?(uint64_t)n:0; }
static int hex_id(const char *s,unsigned length) {
    unsigned i,nonzero=0;
    if(!s||strlen(s)!=length)return 0;
    for(i=0;i<length;i++) { if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;nonzero|=s[i]!='0'; }
    return nonzero!=0;
}
static int sha(const void *data,size_t size,char out[65]) {
    unsigned long length=65;
    if(!Sec_HashMemory(SEC_HASH_SHA256,(void *)data,size,out,&length,qfalse))return 0;
    out[64]=0;return 1;
}
static persistent_identity_t identity(void) {
    persistent_identity_t id={policy.boot,policy.lease,policy.epoch};return id;
}
static persistent_observation_t observe(void) {
    persistent_observation_t o={0};int i,max=sv_maxclients?sv_maxclients->integer:0;
    if(!com_sv_running)return o;
    /* This exact engine owns an INLINE clients[MAX_CLIENTS], not a nullable
     * allocation. Inspect every seat, including beyond a smaller new limit. */
    if(max<1||max>MAX_CLIENTS)return o;
    o.known=1;o.world_ready=sv.state==SS_GAME;
    for(i=0;i<MAX_CLIENTS;i++)if(svs.clients[i].state>=CS_CONNECTED) {
        if(svs.clients[i].netchan.remoteAddress.type==NA_BOT)++o.bots;else ++o.humans;
    }
    return o;
}
int PersistentRuntimeEnabled(void) {return enabled;}
int PersistentRuntimeClockHeld(void) {
    persistent_observation_t o;
    if(!enabled)return 0;
    o=observe();
    return o.known&&!o.humans&&(quarantined||!PersistentSimulate(&policy,&o));
}
void PersistentRuntimeQuarantine(void) {if(enabled)quarantined=1;}
const char *PersistentRuntimeRequest(void) {return accepted_digest;}
int PersistentRuntimeMapAllowed(const char *map) {
    return !enabled||(!quarantined&&map&&
        ((idle_loading&&!strcmp(map,idle_world.map))||
         (policy.phase==PS_CONFIGURING&&!assigned_world&&!strcmp(map,assigned.map))));
}

/* Keep an actual map loaded between bookings. The engine's normal SV_Map ->
 * SV_LoadLevel path owns VM/entity/map teardown and initialization. */
static void idle_set(const char *name,const char *value) {
    /* Unknown console settings must remain promotable by their native owner. */
    Cvar_Set2(name,value,Cvar_FindVar(name)?qtrue:qfalse);
}
static int load_idle_world(void) {
    static const char *zero[]={"scr_bots","scr_botfill","scr_botmax",
        "scr_botfill_testhumans","oneShotMatch","exitToLobby"};
    static const char *blank[]={"sv_mapRotation","sv_mapRotationCurrent","sv_mapRotation_zm",
        "sv_mapRotation_coop","nextmap","g_votedMapName","g_votedGametype","quickSession"};
    persistent_observation_t o=observe();unsigned i;int result;
    if(!o.known||o.humans||idle_loading||
       !PersistentCatalogueIdle(root->string,catalogue->string,RESIDENT_PROFILE_NAME,
           RESIDENT_PROFILE_SHA,(uint32_t)now()^(uint32_t)world,&idle_world,sha))return 0;
    SV_RemoveAllBots();o=observe();if(!o.known||o.humans||o.bots)return 0;
    if(!PersistentCvarsEmpty()&&!PersistentCvarsRestore(accepted_digest))return 0;
    G_SetSavePersist(0);
    for(i=0;i<sizeof(zero)/sizeof(*zero);++i)idle_set(zero[i],"0");
    for(i=0;i<sizeof(blank)/sizeof(*blank);++i)idle_set(blank[i],"");
    idle_set("fs_game",idle_world.category);
    idle_set("sv_zoneType",idle_world.category);
    idle_set("sv_sessionMap",idle_world.session_map);
    idle_set("g_gametype",idle_world.gametype);
    idle_loading=1;idle_ready=0;result=SV_Map(idle_world.map);idle_loading=0;
    o=observe();
    return result&&idle_ready&&!quarantined&&o.known&&o.world_ready&&!o.humans&&!o.bots;
}

void PersistentRuntimeBeforeMap(void) {
    persistent_observation_t o;unsigned i;
    if(!enabled)return;
    o=observe();if(!o.known||o.humans){quarantined=1;return;}
    /* No player or bot remains. Clear account ownership before the normal
     * SV_ClearServer initializes the next map, without closing the endpoint. */
    SV_FreeClients();
    for(i=0;i<MAX_CLIENTS;++i) {
        SetClientConnectIdentity(i,0);DropCarriedSecureProfile(i);ResetClientStats(i);
    }
    ResetServerMatchData();
}

/* Serialize ACTUAL engine dvars in the same complete order as the carrier.
 * Numeric formatting is normalized before comparison; copied labels cannot
 * manufacture readiness. Unrepresentable or out-of-domain values fail closed. */
static int actual_selection(char out[65]) {
    char text[4096],value[128];size_t used=0;unsigned i;int n;
    static const char *keys[]={"sv_zoneType","mapname","sv_sessionMap","g_gametype"};
    static const char *wire[]={"category","map","session_map","gametype"};
    for(i=0;i<4;i++) {
        const char *v=Cvar_GetVariantString(keys[i]);
        if(!v||strchr(v,'\n')||strchr(v,'\r'))return 0;
        n=snprintf(text+used,sizeof(text)-used,"%s=%s\n",wire[i],v);
        if(n<0||(size_t)n>=sizeof(text)-used)return 0;
        used+=(size_t)n;
    }
    for(i=0;i<PA_RULES;i++) {
        const char *v=Cvar_GetVariantString(PersistentRuleNames[i]);char *end;double parsed;
        if(!v||!*v||strlen(v)>=sizeof(value))return 0;
        if(i==20)strcpy(value,v);
        else {
            parsed=strtod(v,&end);if(*end||!isfinite(parsed)||parsed<0||parsed>65535)return 0;
            if(i==19) {
                size_t len;
                if(parsed>4)return 0;
                snprintf(value,sizeof(value),"%.2f",parsed);len=strlen(value);
                while(len&&value[len-1]=='0')value[--len]=0;
                if(len&&value[len-1]=='.')value[--len]=0;
                if(fabs(strtod(value,NULL)-parsed)>0.00001)return 0;
            } else {if(floor(parsed)!=parsed)return 0;snprintf(value,sizeof(value),"%.0f",parsed);}
        }
        n=snprintf(text+used,sizeof(text)-used,"%s=%s\n",PersistentRuleNames[i],value);
        if(n<0||(size_t)n>=sizeof(text)-used)return 0;
        used+=(size_t)n;
    }
    return sha(text,used,out);
}

static int round_world(void) {
    persistent_observation_t o=observe();char actual[65];
    return !quarantined&&policy.phase==PS_ACTIVE&&o.known&&
        sv.state==SS_GAME&&!sv.restarting&&world==assigned_world&&assigned_world&&
        !strcmp(sv.gametype,Cvar_GetVariantString("g_gametype"))&&
        actual_selection(actual)&&!strcmp(actual,assigned.selection);
}
static void round_enqueue(void) {
    char command[128];
    snprintf(command,sizeof(command),"sh1x_persistent_round %s %llu\n",accepted_digest,
        (unsigned long long)round_restart.requested);
    round_restart.queued=1;round_restart.last_frame=com_frameTime;
    Cbuf_AddText(command);
}
void PersistentRuntimeRoundQueue(const char *text) {
    if(!enabled){Cbuf_AddText(text);return;}
    /* Only this retained GSC call produces intent; neither a bare operator
     * fast_restart nor a forged typed RCON command creates a round sequence. */
    if(!text||strcmp(text,"fast_restart\n")||round_restart.pending||
       !round_world()||!now()||round_restart.requested==UINT64_MAX) {
        quarantined=1;return;
    }
    ++round_restart.requested;round_restart.pending=1;
    round_restart.deadline=now()+5;round_enqueue();
}
int PersistentRuntimeRoundAllowed(int fast_restart) {
    if(!enabled)return 1;
    return fast_restart&&round_restart.pending&&round_restart.permit&&
        !round_restart.entered&&round_world();
}
void PersistentRuntimeRoundBegin(void) {
    if(!enabled)return;
    if(!PersistentRuntimeRoundAllowed(1)){quarantined=1;return;}
    round_restart.entered=1;
}
void PersistentRuntimeRoundComplete(void) {
    if(!enabled)return;
    /* This callback is after real RestartGameProgs/reconnect/PostFastRestart.
     * Native state and exact selection are re-read, not inferred from time. */
    if(!round_restart.pending||!round_restart.permit||!round_restart.entered||
       !round_world()){quarantined=1;return;}
    round_restart.completed=round_restart.requested;
    round_restart.pending=round_restart.entered=round_restart.queued=0;
}
static void round_command(void) {
    const char *p;uint64_t sequence=0;int result=0;
    if(!enabled||quarantined||Cmd_Argc()!=3||!accepted_digest[0]||
       strcmp(Cmd_Argv(1),accepted_digest))goto done;
    p=Cmd_Argv(2);
    if(!*p||*p=='0')goto done;
    for(;*p;++p) {
        if(*p<'0'||*p>'9'||sequence>(UINT64_MAX-(unsigned)(*p-'0'))/10)goto done;
        sequence=sequence*10+(unsigned)(*p-'0');
    }
    if(!round_world())goto done;
    if(sequence<=round_restart.completed){result=2;goto done;}
    if(!round_restart.pending||sequence!=round_restart.requested||round_restart.permit)goto done;
    round_restart.queued=0;
    if(!now()||now()>round_restart.deadline){quarantined=1;goto done;}
    round_restart.last_frame=com_frameTime;
    round_restart.permit=1;SV_MapRestart(qtrue);round_restart.permit=0;
    /* The original first-frame early return leaves intent pending. Frame()
     * requeues it once a different actual engine frame is available. */
    result=round_restart.completed==sequence?1:0;
    if(round_restart.entered)quarantined=1;
done:
    Com_Printf(CON_CHANNEL_DONT_FILTER,"sh1x_persistent_round accepted=%d %s\n",result,PersistentRuntimeStatus());
}

static int matches_command(void) {
    /* Only the exact immutable current request may start/cancel this room. */
    return enabled&&!quarantined&&Cmd_Argc()==2&&accepted_digest[0]&&!strcmp(Cmd_Argv(1),accepted_digest);
}
static int completed_command(void) {
    return enabled&&Cmd_Argc()==2&&completed.request[0]&&!strcmp(Cmd_Argv(1),completed.request);
}
static int cancelled_command(void) {
    return enabled&&Cmd_Argc()==2&&cancelled.request[0]&&!strcmp(Cmd_Argv(1),cancelled.request);
}
static void reply(const char *command,int result) {
    Com_Printf(CON_CHANNEL_DONT_FILTER,"%s accepted=%d %s\n",command,result,PersistentRuntimeStatus());
}
static void bind_command(void) {
    persistent_assignment_t a;persistent_policy_t next;
    persistent_identity_t id;persistent_observation_t o;unsigned i;int r=0;
    if(completed_command()||cancelled_command()){r=2;goto done;}
    if(!enabled||quarantined||Cmd_Argc()!=2)goto done;
    if(accepted_digest[0]&&!strcmp(accepted_digest,Cmd_Argv(1))&&
       (policy.phase==PS_CONFIGURING||policy.phase==PS_READY||policy.phase==PS_ACTIVE)) {r=2;goto done;}
    if(!PersistentAssignmentRead(root->string,Cmd_Argv(1),&a,sha)||
       strcmp(a.slot,slot->string)||strcmp(a.boot,policy.boot)||
       strcmp(a.profile,RESIDENT_PROFILE_NAME)||strcmp(a.profile_sha,RESIDENT_PROFILE_SHA)||
       strcmp(a.catalogue_sha,catalogue->string)||!now()||a.expiry<=now()||a.expiry-now()>120||
       !PersistentCatalogueAllows(root->string,catalogue->string,&a,sha))goto done;
    o=observe();id.boot=a.boot;id.lease=a.lease;id.epoch=a.epoch;next=policy;
    if(PersistentBind(&next,&id,a.selection,&o)!=1)goto done;
    if(!PersistentReportsBegin(&reports,a.boot,a.lease,a.epoch,Cmd_Argv(1),sha)) {quarantined=1;goto done;}
    if(!PersistentCvarsBegin(Cmd_Argv(1))) {quarantined=1;goto done;}
    /* Commit reservation before touching dvars. A failed load is quarantined,
     * never made free by a timeout or by restoring only the old map label. */
    policy=next;assigned=a;strcpy(accepted_digest,Cmd_Argv(1));assigned_world=0;
    memset(&round_restart,0,sizeof(round_restart));grant_sequence=0;grant_digest[0]=0;
    for(i=0;i<PA_RULES;i++)if(!PersistentCvarsApply(PersistentRuleNames[i],a.rules[i])) {quarantined=1;goto done;}
    if(!PersistentCvarsApply("matchReportSpool","1")||
       !PersistentCvarsApply("fs_game",a.category)||!PersistentCvarsApply("sv_zoneType",a.category)||
       !PersistentCvarsApply("sv_sessionMap",a.session_map)||!PersistentCvarsApply("g_gametype",a.gametype)) {quarantined=1;goto done;}
    /* These carriers are intentionally always blank in the resident protocol. */
    {static const char *clear[]={"sv_mapRotation","sv_mapRotationCurrent","sv_mapRotation_zm",
         "sv_mapRotation_coop","quickSession","nextmap","g_votedMapName","g_votedGametype"};
     for(i=0;i<sizeof(clear)/sizeof(*clear);i++)if(!PersistentCvarsApply(clear[i],"")) {quarantined=1;goto done;}}
    if(!SV_Map(a.map)) {quarantined=1;goto done;}
    r=1;
done:reply("sh1x_persistent_bind",r);
}
static void start_command(void) {
    persistent_policy_t next;persistent_identity_t id=identity();
    persistent_observation_t o=observe();char digest[65];int r=0;
    if(completed_command()||cancelled_command()){r=2;goto done;}
    if(!matches_command()||!actual_selection(digest)||strcmp(digest,assigned.selection)||world!=assigned_world)goto done;
    if(policy.phase==PS_ACTIVE) {r=2;goto done;}
    next=policy;
    if(!PersistentGrant(&next,&id,assigned.members,assigned.member_count,assigned.expiry,now())||
       !PersistentStart(&next,&id,&o,now()))goto done;
    policy=next;r=1;
done:reply("sh1x_persistent_start",r);
}
static void grant_command(void) {
    persistent_grant_t g;persistent_policy_t next;
    persistent_identity_t id=identity();persistent_observation_t o=observe();
    char actual[65];uint64_t t=now();int r=0;
    if(!enabled||quarantined||Cmd_Argc()!=3||!accepted_digest[0]||strcmp(Cmd_Argv(1),accepted_digest)||
       (policy.phase!=PS_ACTIVE&&policy.phase!=PS_READY)||round_restart.pending||
       !o.known||sv.state!=SS_GAME||sv.restarting||world!=assigned_world||!actual_selection(actual)||
       strcmp(actual,assigned.selection)||!t)goto done;
    if(!PersistentGrantRead(root->string,Cmd_Argv(2),&g,sha)||
       strcmp(g.slot,slot->string)||strcmp(g.boot,policy.boot)||strcmp(g.lease,policy.lease)||
       strcmp(g.request,accepted_digest)||g.epoch!=policy.epoch||
       g.member_count>(unsigned)sv_maxclients->integer||g.expiry<=t||g.expiry-t>120||
       g.expiry<assigned.expiry)goto done;
    if(g.sequence==grant_sequence&&!strcmp(grant_digest,Cmd_Argv(2))){r=2;goto done;}
    if(g.sequence<=grant_sequence)goto done;
    next=policy;
    if(!PersistentGrant(&next,&id,g.members,g.member_count,g.expiry,t))goto done;
    policy=next;memcpy(assigned.members,g.members,sizeof(g.members));assigned.member_count=g.member_count;
    assigned.expiry=g.expiry;grant_sequence=g.sequence;strcpy(grant_digest,Cmd_Argv(2));r=1;
done:reply("sh1x_persistent_grant",r);
}
static void cancel_command(void) {
    persistent_identity_t id=identity();persistent_observation_t o=observe();int r=0;
    if(completed_command()||cancelled_command()){reply("sh1x_persistent_cancel",2);return;}
    if(matches_command())r=PersistentEnd(&policy,&id,PS_CANCELLED,&o);
    if(r)round_restart.pending=round_restart.queued=round_restart.permit=round_restart.entered=0;
    /* Cleanup/report completion is separate. Never disconnect a remaining guest. */
    reply("sh1x_persistent_cancel",r);
}
static void cancel_unbound_command(void) {
    persistent_assignment_t a;persistent_policy_t next;
    persistent_identity_t id;persistent_observation_t o=observe();
    persistent_event_snapshot_t snapshot;uint64_t basis=0;int r=0;
    if(completed_command()||cancelled_command()){reply("sh1x_persistent_cancel_unbound",2);return;}
    if(!enabled||quarantined||Cmd_Argc()!=2||policy.phase!=PS_IDLE||
       !o.known||o.humans||o.bots||!o.world_ready||!com_sv_running||!com_sv_running->boolean||
       reports.scope[0]||round_restart.pending)goto done;
    basis=completed.epoch;
    /* Expired immutable assignments may be cancelled, never started. Exact
     * profile/catalogue/boot/slot validation remains identical to BIND. */
    if(!PersistentAssignmentRead(root->string,Cmd_Argv(1),&a,sha)||
       strcmp(a.slot,slot->string)||strcmp(a.boot,policy.boot)||
       strcmp(a.profile,RESIDENT_PROFILE_NAME)||strcmp(a.profile_sha,RESIDENT_PROFILE_SHA)||
       strcmp(a.catalogue_sha,catalogue->string)||!PersistentCatalogueAllows(root->string,catalogue->string,&a,sha))goto done;
    id.boot=a.boot;id.lease=a.lease;id.epoch=a.epoch;next=policy;
    if(!PersistentCancelIdle(&next,&id,&o))goto done;
    policy=next;strcpy(accepted_digest,Cmd_Argv(1));
    strcpy(cancelled.request,Cmd_Argv(1));strcpy(cancelled.lease,a.lease);
    cancelled.epoch=a.epoch;cancelled.world=world;cancelled.basis_epoch=basis;
    grant_sequence=0;grant_digest[0]=0;r=1;
done:reply("sh1x_persistent_cancel_unbound",r);
}
void PersistentRuntimeWorldBegin(void) {
    if(!enabled)return;
    ever_world=1;if(world==UINT64_MAX){quarantined=1;return;}++world;
    if(idle_loading)return;
    if(policy.phase!=PS_CONFIGURING||assigned_world) {quarantined=1;return;}
    assigned_world=world;
}
void PersistentRuntimeMapReady(void) {
    persistent_identity_t id=identity();persistent_observation_t o=observe();char actual[65];
    if(!enabled||quarantined)return;
    if(idle_loading) {
        idle_ready=o.known&&o.world_ready&&!o.humans&&!o.bots&&
            !strcmp(Cvar_GetVariantString("mapname"),idle_world.map);
        if(!idle_ready)quarantined=1;
        return;
    }
    if(world!=assigned_world||!actual_selection(actual)||!PersistentWorldReady(&policy,&id,actual,&o))quarantined=1;
}
void PersistentRuntimeEnded(void) {
    persistent_identity_t id=identity();persistent_observation_t o=observe();
    if(enabled&&!quarantined&&!idle_loading&&policy.phase!=PS_IDLE&&policy.phase!=PS_BOOTING) {
        if(!PersistentEnd(&policy,&id,PS_GAME_ENDED,&o))quarantined=1;
        else round_restart.pending=round_restart.queued=round_restart.permit=round_restart.entered=0;
    }
}
void PersistentRuntimeUnexpectedShutdown(void) {
    /* Native error/operator shutdown is not a successful reusable reset. */
    if(enabled)quarantined=1;
}
const char *PersistentRuntimeReportScope(void) {
    return enabled&&!idle_loading?reports.scope:"";
}
const char *PersistentRuntimeReportEnvelope(void) {
    static char text[256];
    if(!enabled||idle_loading||!reports.scope[0])return NULL;
    /* Fields are validated lower-case hexadecimal identities and a uint64.
     * No player name or other unescaped external text is interpolated. */
    snprintf(text,sizeof(text),"{\"version\":1,\"boot\":\"%s\",\"lease\":\"%s\",\"epoch\":%llu,\"scope\":\"%s\"}",
        policy.boot,policy.lease,(unsigned long long)policy.epoch,reports.scope);
    return text;
}
void PersistentRuntimeReportPublished(const char *scope,const char *path,const void *payload,size_t bytes) {
    if(enabled && !PersistentReportsPublished(&reports,scope,path,payload,bytes,sha))quarantined=1;
}
int PersistentRuntimeAdmit(uint64_t uid) {
    persistent_identity_t id=identity();char actual[65];
    return !enabled||(!quarantined&&!round_restart.pending&&sv.state==SS_GAME&&!sv.restarting&&
        world==assigned_world&&actual_selection(actual)&&
        !strcmp(actual,assigned.selection)&&PersistentAdmit(&policy,&id,uid,now()));
}
int PersistentRuntimeFrame(void) {
    persistent_observation_t o=observe();
    if(!enabled)return 0;
    /* A held resident frame returns before Com_Frame's usual wall-clock
     * refresh. Keep admission deadlines and common network time current even
     * when the loaded idle world has no running match simulation. */
    Com_UpdateRealtime();
    if(!quarantined&&policy.phase==PS_BOOTING) {
        if(!load_idle_world())quarantined=1;
        else {o=observe();if(!PersistentBootClean(&policy,&o))quarantined=1;}
    }
    /* Actual humans retain simulation even after an invalid control event. */
    if(quarantined)return o.known&&!o.humans;
    if(round_restart.pending) {
        if(!now()||now()>round_restart.deadline)quarantined=1;
        else if(!round_restart.queued&&!round_restart.permit&&
                round_restart.last_frame!=com_frameTime)round_enqueue();
    }
    if(quarantined)return o.known&&!o.humans;
    if(policy.phase==PS_FINISHING && o.known && !o.humans && MatchReportPrepareReset()) {
        persistent_identity_t id=identity();unsigned i;
        if(!PersistentReportsSeal(&reports,1,sha)||
           !PersistentReportDurable(&policy,&id,reports.sealed)||
           !PersistentBeginReset(&policy,&id,&o))quarantined=1;
        else {
            /* Real preflight just drained every immutable payload. Old map-load
             * cleanup observations were cleared above, not borrowed as proof. */
            PersistentEventReportBarrier(1);
            memset(assigned.members,0,sizeof(assigned.members));assigned.member_count=0;assigned.expiry=0;
            for(i=0;i<18 && !policy.members[i];i++) { }
            if(policy.member_count||policy.expiry||i!=18)quarantined=1;
            else PersistentEventStep(PS_ADMISSIONS);
        }
    }
    if(!quarantined && policy.phase==PS_RESETTING) {
        if(!load_idle_world())quarantined=1;
        else {
            persistent_policy_t next=policy;
            persistent_identity_t id=identity();
            o=observe(); /* A real new idle map is loaded in this same process. */
            if(world!=assigned_world+1)quarantined=1;
            else {
                if(PersistentResetComplete(&next,&id,&o)!=1)quarantined=1;
                else {
                    /* Retire only the ledger sealed at the real durable
                     * barrier. Commit Idle only after that validation succeeds.
                     * A bounded last-completion ACK survives next-room BIND. */
                    strcpy(completed.request,accepted_digest);strcpy(completed.lease,policy.lease);
                    strcpy(completed.scope,reports.scope);strcpy(completed.report,reports.sealed);
                    completed.epoch=policy.epoch;completed.world=assigned_world;
                    if(!PersistentReportsRetire(&reports,completed.scope,completed.report)) {
                        memset(&completed,0,sizeof(completed));quarantined=1;
                    } else {policy=next;assigned_world=0;}
                }
            }
        }
    }
    return !PersistentSimulate(&policy,&o);
}
const char *PersistentRuntimeStatus(void) {
    static char text[1536];
    persistent_observation_t o=observe();
    snprintf(text,sizeof(text),"resident=%d resident_quarantined=%d resident_phase=%u resident_boot=%s resident_lease=%s resident_epoch=%llu resident_world=%llu resident_request=%s resident_selection=%s resident_reports=%u resident_report_digest=%s resident_cleanup_failure=%u resident_round=%llu resident_round_completed=%llu resident_round_pending=%d resident_tail_failure=%u resident_known=%u resident_humans=%u resident_bots=%u resident_running=%d",
        enabled,quarantined,(unsigned)policy.phase,policy.boot[0]?policy.boot:"-",policy.lease[0]?policy.lease:"-",
        (unsigned long long)policy.epoch,(unsigned long long)world,accepted_digest[0]?accepted_digest:"-",policy.selection[0]?policy.selection:"-",reports.count,reports.sealed[0]?reports.sealed:"-",PersistentCleanupFailure(),
        (unsigned long long)round_restart.requested,(unsigned long long)round_restart.completed,round_restart.pending,
        PersistentTailFailure(),o.known,o.humans,o.bots,com_sv_running&&com_sv_running->boolean);
    return text;
}
static void resident_status_command(void) {
    if(Cmd_Argc()==1)Com_Printf(CON_CHANNEL_DONT_FILTER,"%s\n",PersistentRuntimeStatus());
}
static void completed_status_command(void) {
    if(Cmd_Argc()!=1)return;
    /* Kept separate from the base status line to avoid the real RCON print
     * truncation observed when appending all completion fields to sh1x_status. */
    Com_Printf(CON_CHANNEL_DONT_FILTER,"resident_completed_boot=%s resident_completed_request=%s resident_completed_lease=%s resident_completed_epoch=%llu resident_completed_world=%llu resident_completed_scope=%s resident_completed_report=%s\n",
        policy.boot[0]?policy.boot:"-",completed.request[0]?completed.request:"-",completed.lease[0]?completed.lease:"-",
        (unsigned long long)completed.epoch,(unsigned long long)completed.world,
        completed.scope[0]?completed.scope:"-",completed.report[0]?completed.report:"-");
}
static void grant_status_command(void) {
    if(Cmd_Argc()!=1)return;
    Com_Printf(CON_CHANNEL_DONT_FILTER,"resident_grant_boot=%s resident_grant_lease=%s resident_grant_request=%s resident_grant_epoch=%llu resident_grant_sequence=%llu resident_grant_digest=%s resident_grant_expiry=%llu resident_grant_members=%u\n",
        policy.boot[0]?policy.boot:"-",policy.lease[0]?policy.lease:"-",accepted_digest[0]?accepted_digest:"-",
        (unsigned long long)policy.epoch,(unsigned long long)grant_sequence,grant_digest[0]?grant_digest:"-",
        (unsigned long long)policy.expiry,policy.member_count);
}
static void cancelled_status_command(void) {
    if(Cmd_Argc()!=1)return;
    Com_Printf(CON_CHANNEL_DONT_FILTER,"resident_cancelled_boot=%s resident_cancelled_request=%s resident_cancelled_lease=%s resident_cancelled_epoch=%llu resident_cancelled_world=%llu resident_cancelled_basis_epoch=%llu\n",
        policy.boot[0]?policy.boot:"-",cancelled.request[0]?cancelled.request:"-",cancelled.lease[0]?cancelled.lease:"-",
        (unsigned long long)cancelled.epoch,(unsigned long long)cancelled.world,(unsigned long long)cancelled.basis_epoch);
}
void PersistentRuntimeRegister(void) {
    cvar_t *boot;persistent_observation_t o;int virgin;
    if(registered)return;
    registered=1;
    boot=Cvar_RegisterString("persistentBoot","",CVAR_INIT,"Private supervisor boot identity; empty disables resident control");
    root=Cvar_RegisterString("persistentRoot","",CVAR_INIT,"Private immutable assignment directory");
    slot=Cvar_RegisterString("persistentSlot","",CVAR_INIT,"Owned resident slot identity");
    catalogue=Cvar_RegisterString("persistentCatalogue","",CVAR_INIT,"Trusted catalogue content digest");
    Cmd_AddCommand("sh1x_persistent_bind",bind_command);Cmd_AddCommand("sh1x_persistent_start",start_command);
    Cmd_AddCommand("sh1x_persistent_cancel",cancel_command);
    Cmd_AddCommand("sh1x_persistent_round",round_command);
    Cmd_AddCommand("sh1x_persistent_status",resident_status_command);
    Cmd_AddCommand("sh1x_persistent_completed",completed_status_command);
    Cmd_AddCommand("sh1x_persistent_grant",grant_command);
    Cmd_AddCommand("sh1x_persistent_grant_status",grant_status_command);
    Cmd_AddCommand("sh1x_persistent_cancel_unbound",cancel_unbound_command);
    Cmd_AddCommand("sh1x_persistent_cancelled",cancelled_status_command);
    if(!boot||!boot->string[0])return;
    enabled=1;
    if(!hex_id(RESIDENT_PROFILE_SHA,64)||!PersistentInit(&policy,boot->string)||root->string[0]!='/'||!hex_id(slot->string,32)||!hex_id(catalogue->string,64)||
       Cvar_GetVariantString("warmPreparation")[0]||Cvar_VariableBooleanValue("sv_quickPlay")){quarantined=1;return;}
    PersistentEventsEnable(1);o=observe();
    virgin=!ever_world&&com_sv_running&&!com_sv_running->boolean&&sv.state==SS_DEAD&&!svs.initialized&&
        !level.num_entities&&!Scr_IsSystemActive(1)&&!gScrVmPub.function_count;
    /* The slot becomes allocatable only after Frame loads a real idle map. */
    if(!virgin||!o.known||o.humans||o.bots)quarantined=1;
}

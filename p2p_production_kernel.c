/* Production direct-v1 owner integration. Included after hostgate.c so no
 * callback supplies guessed addresses or worker-owned native authority. */
#include "p2p_production_kernel.h"
#include <wchar.h>
#include <stdlib.h>
#include "shared/p2p_win_pipe.h"
#include "p2p_production_socket.inc"
#ifdef CODOL_P2P_PRODUCTION_POLICY_HEADER
#include CODOL_P2P_PRODUCTION_POLICY_HEADER
#else
/* Unqualified source builds cannot acquire production authority. The release
 * assembler must generate a read-only policy from the verified exact package. */
static const p2pr_release_policy g_p2prReleasePolicy={0,{0},{0},{0},{0}};
#endif
static struct {
    int mode,latched,begun,host_requested,host_started,stopping,retired;unsigned role;
    wchar_t pipe[128],parent_image[32768],host_image[32768],handle_text[32],ceiling_text[32];
    p2pr_selection selection;p2pr_native_api api;
    uint64_t work_deadline,cleanup_deadline,retire_deadline;
} g_prod;
static int P2PProductionMode(void) {
    wchar_t value[128],ceiling[32];DWORD n,error,cn,previous=GetLastError();int mode;
    SetLastError(ERROR_SUCCESS);n=GetEnvironmentVariableW(L"CODOL_P2P_PRODUCTION_PIPE",value,128);error=GetLastError();SetLastError(previous);
    mode=!n&&error==ERROR_ENVVAR_NOT_FOUND?0:n>0&&n<128?1:-1;
    cn=mode==1?GetEnvironmentVariableW(L"CODOL_P2P_PRODUCTION_SESSION_CEILING",ceiling,32):0;
    if(mode==1&&(!cn||cn>=32))mode=-1;
    if(!g_prod.latched){
        if(mode==1) {
            wchar_t *end=NULL;for(DWORD i=0;i<cn;i++)if(ceiling[i]<L'0'||ceiling[i]>L'9')mode=-1;
            g_prod.cleanup_deadline=wcstoull(ceiling,&end,10);
            if(!end||*end||!g_prod.cleanup_deadline)mode=-1;
        }
        g_prod.latched=1;g_prod.mode=mode;if(mode==1){wcscpy(g_prod.pipe,value);wcscpy(g_prod.ceiling_text,ceiling);}
    }
    else if(mode!=g_prod.mode||(mode==1&&(wcscmp(value,g_prod.pipe)||wcscmp(ceiling,g_prod.ceiling_text))))g_prod.mode=-1;
    SetLastError(previous);
    return g_prod.mode;
}
static int P2PSelectedProtocolMode(void){return P2PProductionMode()==0?P2PStartupProtocolMode():P2PProductionMode()>0?3:-1;}
static const wchar_t *P2PProductionHostPath(void){return g_prod.host_image;}
static int __cdecl ProdRunning(void){return GetCurrentThreadId()==g_p2pMainThread&&g_piState==PI_RUNNING&&g_piInTick&&PiCurrentFiber()==g_piFiber;}
static int __cdecl ProdInstallBoundary(void){return GetCurrentThreadId()==g_p2pMainThread&&g_piState==PI_INIT_PENDING&&g_piInTick&&PiCurrentFiber()==g_piFiber;}
static int __cdecl ProdReadSelection(p2pr_selection *out) {
    if(!out||GetCurrentThreadId()!=g_p2pMainThread||!g_prod.begun||!g_p2pEnabled||!g_p2pHasRoom||
       g_p2pStartupActive!=g_prod.selection.generation||!g_p2pIntentUid||P2PLocalUid()!=g_p2pIntentUid)return 0;
    memset(out,0,sizeof *out);lstrcpynA(out->map,g_p2pRoom.session,sizeof out->map);
    lstrcpynA(out->gametype,g_p2pRoom.gametype,sizeof out->gametype);lstrcpynA(out->category,g_p2pRoom.category,sizeof out->category);
    out->difficulty_present=g_p2pRoom.difficultyPresent;out->difficulty=g_p2pRoom.difficulty;
    out->uid=g_p2pIntentUid;out->generation=g_p2pStartupActive;out->role=g_prod.role;
    return 1;
}
static int __cdecl ProdCommitRole(unsigned role,uint64_t uid,uint64_t generation) {
    p2pr_selection current;
    if(g_prod.role||(role!=1&&role!=2)||!ProdReadSelection(&current)||current.uid!=uid||current.generation!=generation)return 0;
    g_prod.role=role;if(role==1)g_prod.host_requested=1;return 1;
}
static void __cdecl ProdQuarantine(void) {
    P2PProductionCancel();
}
static int P2PProductionStart(void) {
    DWORD n;wchar_t *end=NULL;unsigned long long number;HANDLE inherited;
    if(P2PProductionMode()!=1||g_prod.begun||GetCurrentThreadId()!=g_p2pMainThread||
       !g_p2pStartupActive||!g_p2pEnabled||!g_p2pHasRoom||!g_p2pIntentUid||!g_prodSocket.ready)return 0;
    n=GetEnvironmentVariableW(L"CODOL_P2P_PRODUCTION_PARENT_HANDLE",g_prod.handle_text,32);if(!n||n>=32)return 0;
    for(DWORD i=0;i<n;i++)if(g_prod.handle_text[i]<L'0'||g_prod.handle_text[i]>L'9')return 0;
    number=wcstoull(g_prod.handle_text,&end,10);if(!end||*end||!number||number>UINTPTR_MAX)return 0;
    inherited=(HANDLE)(uintptr_t)number;
    n=GetEnvironmentVariableW(L"CODOL_P2P_PRODUCTION_PARENT_IMAGE",g_prod.parent_image,32768);if(!n||n>=32768)return 0;
    n=GetModuleFileNameW(NULL,g_prod.host_image,32768);if(!n||n>=32768)return 0;
    end=wcsrchr(g_prod.host_image,L'\\');if(!end||end-g_prod.host_image>32730)return 0;
    wcscpy(end+1,L"p2p\\sh1x_host.dll"); /* exact accepted stable Host DLL path; never INI override */
    memset(&g_prod.selection,0,sizeof g_prod.selection);
    lstrcpynA(g_prod.selection.map,g_p2pRoom.session,sizeof g_prod.selection.map);
    lstrcpynA(g_prod.selection.gametype,g_p2pRoom.gametype,sizeof g_prod.selection.gametype);
    lstrcpynA(g_prod.selection.category,g_p2pRoom.category,sizeof g_prod.selection.category);
    g_prod.selection.difficulty_present=g_p2pRoom.difficultyPresent;g_prod.selection.difficulty=g_p2pRoom.difficulty;
    g_prod.selection.uid=g_p2pIntentUid;g_prod.selection.generation=g_p2pStartupActive;g_prod.selection.role=0;
    g_prod.api.read_selection=ProdReadSelection;g_prod.api.commit_role=ProdCommitRole;
    g_prod.api.running=ProdRunning;g_prod.api.install_boundary=ProdInstallBoundary;
    g_prod.api.socket_witness=ProdWitness;g_prod.api.guest_probe=ProdGuestProbe;
    g_prod.work_deadline=P2PT_Now();
    if(!g_prod.work_deadline||g_prod.work_deadline>UINT64_MAX-30000||g_prod.cleanup_deadline<=15000||
       g_prod.work_deadline>=g_prod.cleanup_deadline-15000)return 0;
    g_prod.work_deadline+=30000;
    if(g_prod.work_deadline>g_prod.cleanup_deadline-15000)g_prod.work_deadline=g_prod.cleanup_deadline-15000;
    g_prod.begun=1;
    if(!P2PR_Begin(inherited,g_prod.parent_image,g_prod.pipe,g_prod.host_image,&g_p2prReleasePolicy,
        &g_prod.selection,g_prod.work_deadline,g_prod.cleanup_deadline)){P2PR_RequestStop();return 0;}
    return 1;
}
static int P2PProductionLoadHost(HMODULE module) {
    if(P2PProductionMode()!=1||!g_prod.begun||g_prod.role!=1||!module||!P2PR_CurrentPermission())return 0;
#define PROD_API(field,name,type) g_prod.api.field=(type)(void *)GetProcAddress(module,name)
    typedef int (__cdecl *Install)(const void *,unsigned);
    typedef int (__cdecl *Active)(const void *,unsigned,uint64_t);
    typedef int (__cdecl *Observe)(const void *,unsigned,void *,unsigned);
    typedef int (__cdecl *Current)(void);typedef void (__cdecl *Quarantine)(void);
    typedef int (__cdecl *Send)(const unsigned char[4],unsigned,const void *,unsigned,uint64_t);
    typedef int (__cdecl *Socket)(void *,unsigned);
    PROD_API(scripts,"Sh1xHost_UseVerifiedScripts",Install);
    PROD_API(principals,"Sh1xHost_InstallDirectPrincipals",Install);
    PROD_API(options,"Sh1xHost_UseVerifiedMatchOptions",Install);
    PROD_API(observe_options,"Sh1xHost_ObserveMatchOptions",Install);
    PROD_API(active,"Sh1xHost_InstallActiveDirectPrincipals",Active);
    PROD_API(observe,"Sh1xHost_ObserveDirectPrincipals",Observe);
    PROD_API(observe_connected,"Sh1xHost_ObserveConnectedDirectPrincipals",Observe);
    PROD_API(observe_server_socket,"Sh1xHost_ObserveServerSocket",Socket);
    PROD_API(current,"Sh1xHost_RelayPrincipalsCurrent",Current);
    PROD_API(quarantine,"Sh1xHost_QuarantineRelayPrincipals",Quarantine);
    PROD_API(send_probe,"Sh1xHost_SendIngressProbe",Send);
    PROD_API(send_active_probe,"Sh1xHost_SendActiveIngressProbe",Send);
    PROD_API(send_initial_probe,"Sh1xHost_SendInitialIngressProbe",Send);
#undef PROD_API
    return g_prod.api.scripts&&g_prod.api.principals&&g_prod.api.options&&g_prod.api.observe_options&&
        g_prod.api.active&&g_prod.api.observe&&g_prod.api.observe_connected&&g_prod.api.observe_server_socket&&
        g_prod.api.current&&g_prod.api.quarantine&&g_prod.api.send_probe&&g_prod.api.send_active_probe&&g_prod.api.send_initial_probe;
}
static void P2PProductionOwnerTick(int fiber) {
    p2pr_selection current;
    if(!g_prod.begun||GetCurrentThreadId()!=g_p2pMainThread)return;
    if(!ProdReadSelection(&current)){ProdQuarantine();return;}
    if(g_prod.role==1&&g_piState==PI_INIT_PENDING&&!fiber)return;
    if(g_prod.role==1&&g_piState==PI_RUNNING&&!fiber)return;
    /* Native Host controls are consumed only at the real PI_RUNNING fiber
     * top; pre-Init the receiver may consume bootstrap installation only. */
    if(!P2PR_OwnerTick(&current,&g_prod.api))ProdQuarantine();
}
static int P2PProductionHostInstalled(void){return P2PR_AuthorityScope()>=P2PR_HOST_INIT_ONLY;}
static int P2PProductionHostCurrent(void){return g_prod.begun&&P2PR_CurrentPermission()&&
    g_prod.api.current&&g_prod.api.current()==1;}
static void P2PProductionCancel(void){if(g_prod.begun){
    if(!g_prod.stopping) {
        uint64_t now=P2PT_Now();g_prod.stopping=1;
        g_prod.retire_deadline=now&&now<=UINT64_MAX-2000?now+2000:g_prod.cleanup_deadline;
        if(g_prod.retire_deadline>g_prod.cleanup_deadline)g_prod.retire_deadline=g_prod.cleanup_deadline;
    }
    P2PR_RequestStop();
    if(g_prod.api.quarantine)g_prod.api.quarantine(); /* native deny-only atomic latch, safe while yielded */
    InterlockedExchange(&g_piQuit,1);}}
static void P2PProductionPublishUi(void) {
    static char previous[1024];char snapshot[1024],error[256],number[32];int state,initial=0;
    p2pr_selection current;
    if(!C1DvarsReady())return;
    if(g_prod.begun&&!g_prod.stopping&&ProdReadSelection(&current))initial=P2PR_InitialHandoffCurrent(&current);
    EnterCriticalSection(&g_p2pLock);state=g_p2p.state;lstrcpynA(error,g_p2pLobbyError[0]?g_p2pLobbyError:g_p2p.error,sizeof error);LeaveCriticalSection(&g_p2pLock);
    _snprintf(snapshot,sizeof snapshot,"%d|%d|%lu|%u|%d|%s|%s",g_p2pEnabled,state,g_p2pStartupActive,g_p2pLobbyRequest,initial,g_p2pLobbyMap,error);snapshot[sizeof snapshot-1]=0;
    if(!strcmp(snapshot,previous))return;
    BOOL done=C1QueueSet("ui_p2p_initial_handoff","0"); /* close before changing any lineage field */
    done=C1QueueSet("ui_p2p_startup_protocol",P2PProductionMode()>0?"3":"-1")&&done;
    done=C1QueueSet("ui_p2p_enabled",g_p2pEnabled?"1":"0")&&done;
    done=C1QueueSet("ui_p2p_state",kP2PState[state])&&done;done=C1QueueSet("ui_p2p_error",error)&&done;
    done=C1QueueSet("ui_p2p_map",g_p2pLobbyMap)&&done;done=C1QueueSet("ui_p2p_ready","0")&&done;
    done=C1QueueSet("ui_p2p_busy",state==P2P_STARTING||state==P2P_STOPPING?"1":"0")&&done;
    _snprintf(number,sizeof number,"%lu",g_p2pStartupActive);done=C1QueueSet("ui_p2p_generation",number)&&done;
    _snprintf(number,sizeof number,"%u",g_p2pLobbyRequest);done=C1QueueSet("ui_p2p_request",number)&&done;
    if(done&&initial)done=C1QueueSet("ui_p2p_initial_handoff","1");
    if(done)lstrcpynA(previous,snapshot,sizeof previous);
}
static void P2PProductionTick(void) {
    if(P2PProductionMode()==0)return;
    if(!g_prod.begun){P2PProductionPublishUi();return;}
    if(g_prod.retired){P2PProductionPublishUi();return;}
    P2PProductionOwnerTick(0);
    if(g_prod.host_requested&&!g_prod.host_started&&P2PR_CurrentPermission()) {
        P2PConfig selected=g_p2pIntentConfig;
        /* Actual remote Guests need the authenticated native server listener.
         * Its separate witness/probe supplies the observed public endpoint. */
        lstrcpyA(selected.netIp,"0.0.0.0");
        g_prod.host_requested=0;g_prod.host_started=1;
        if(!P2PStartConfigured(NULL,NULL,NULL,&selected))ProdQuarantine();
    }
    if(g_prod.stopping||P2PR_FirstError()) {
        P2PProductionCancel();
        if(P2PR_FirstError())P2PSetState(P2P_FAILED,"Local Play authorization ended.");
        /* IO retirement never frees an owned/yielded callback's inventory. */
        if(g_piState!=PI_INIT_PENDING&&g_piState!=PI_RUNNING&&P2PR_Retire(g_prod.retire_deadline)) {
            g_prod.retired=1;if(!P2PR_FirstError())P2PSetState(P2P_IDLE,"");
        }
    }
    P2PProductionPublishUi();
}
static int P2PProductionGuestAddress(DWORD ip,unsigned port) {
    unsigned char expected[4];unsigned native_port;uint64_t generation;
    if(P2PProductionMode()!=1||!g_prod.begun||g_prod.role!=2||!g_p2pEnabled)return 0;
    if(g_prod.stopping||g_prod.retired)return -1;
    if(!P2PR_GuestEndpoint(expected,&native_port,&generation))return -1;
    return !memcmp(expected,&ip,4)&&port==native_port&&generation==g_prod.selection.generation?1:0;
}

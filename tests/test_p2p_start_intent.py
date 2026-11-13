"""Execute the actual native command/tick/token bodies with deterministic OS I/O.

No Windows executable, game, socket, pipe or native DLL is launched. Linux i386
sanitizers test the production bodies, rather than a second state-machine model.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'p2p.c').read_text()


def function(name):
    import re
    match = re.search(r'^(?:static )?[^\n;]*\b' + name + r'\([^\n]*\)\n\{', SOURCE, re.M)
    if not match:
        raise AssertionError('missing actual function: ' + name)
    start = match.start()
    opening = SOURCE.index('{', match.start())
    depth, end = 1, opening + 1
    while depth:
        if SOURCE[end] == '{': depth += 1
        if SOURCE[end] == '}': depth -= 1
        end += 1
    return SOURCE[start:end]


PREFIX = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "p2p_match_options.h"
#include "p2p_intent_heartbeat.h"
#include "p2p_local_play_api.h"
typedef int BOOL, LONG, CRITICAL_SECTION;
typedef void *HANDLE;
typedef unsigned long DWORD;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define __cdecl
#define WAIT_OBJECT_0 0
#define _snprintf snprintf
#define lstrcpyA strcpy
#include <strings.h>
#define _stricmp strcasecmp
enum { P2P_IDLE, P2P_STARTING, P2P_READY, P2P_STOPPING, P2P_EXITED, P2P_FAILED };
static const char *const kP2PState[] = {"idle","starting","ready","stopping","exited","failed"};
'''

CONFIG = SOURCE[SOURCE.index('typedef struct P2PConfig {'):SOURCE.index('} P2PConfig;') + len('} P2PConfig;')]

GLOBALS = r'''
static struct {int state;DWORD pid,startTick,readyMs,exitCode;char status[512],error[256];P2PConfig cfg;} g_p2p;
static CRITICAL_SECTION g_p2pLock;
static BOOL g_p2pInit=TRUE;
static HANDLE g_p2pThread;
static volatile LONG g_p2pStop;
static unsigned long g_p2pStartupCounter,g_p2pStartupActive;
static BOOL g_p2pStartupExhausted,g_p2pStartupEnabled,g_p2pStartupRoom;
static unsigned long long g_p2pStartupOwner;
static unsigned g_p2pLobbyRequest;
static char g_p2pLobbyMap[64],g_p2pLobbyError[256];
static P2PRoomContext g_p2pRoom;
static P2PConfig g_p2pLobbyNext,g_p2pIntentConfig,g_p2pOverride;
static BOOL g_p2pLobbyPending,g_p2pEnabled,g_p2pHasRoom,g_p2pHasOverride;
static char g_p2pArgMap[64],g_p2pArgGametype[32],g_p2pArgZone[16];
static unsigned long long g_p2pIntentUid;
static DWORD g_p2pIntentAt;
static P2PIntentHeartbeat g_p2pIntentHeartbeat;
static int mode=1,thread_done=1,native_used,starts,checks,publications;
static int published_ready,argc_,lock_depth;
static unsigned long long uid=42;
static const char *check_answer; /* what P2PCheck says about a configuration; NULL = usable */
static unsigned long clock_now=10000;
static const char *argv_[8];
static char published_state[32],ui_ready[8],ui_state[32];
static int P2PStartupProtocolMode(void) {return mode;}
/* The production protocol (3) and the Guest route are separate modules with their own fixtures. */
static int P2PSelectedProtocolMode(void) {return mode;}
static int P2PProductionMode(void) {return 0;}
static void P2PProductionCancel(void) {}
static int P2PProductionStart(void) {abort();}
static void P2PProductionTick(void) {abort();}
static int P2PGuestRouteMode(void) {return 0;}
static DWORD P2PGuestThread(void *unused) {(void)unused;abort();}
static void P2PGuestTick(void) {abort();}
static DWORD g_p2pMainThread=7;
static DWORD GetCurrentThreadId(void) {return 7;}
static BOOL g_p2pLaunchPending,g_k33TickInstalled=TRUE;
static P2PConfig g_p2pPendingConfig;
static BOOL P2PEnsureInprocActivation(void) {return TRUE;}
static void Log(const char *fmt,...) {(void)fmt;}
enum { P2P_LAUNCH_NONE, P2P_LAUNCH_DATAGRAM, P2P_LAUNCH_EXPLICIT };
#define P2P_LAUNCH_EMPTY_MS 120000u
static volatile LONG g_p2pLaunchSignal,g_p2pLaunchPort;
static BOOL g_p2pArmed,g_p2pLaunched,g_p2pStartPermitted;
static DWORD g_p2pLaunchTick,g_p2pEmptySince;
static BOOL g_p2pLaunchAnswered,g_p2pLaunchAborted;
static int disconnects;
static void C1CbufAddText(const char *text) {assert(!lock_depth&&!strcmp(text,"disconnect\n"));++disconnects;}
static K33LocalPlayIntentInfo g_p2pApi;
static LONG InterlockedCompareExchange(volatile LONG *p,LONG value,LONG expected) {LONG old=*p;if(old==expected)*p=value;return old;}
static LONG InterlockedExchange(volatile LONG *p,LONG value) {LONG old=*p;*p=value;return old;}
static void EnterCriticalSection(CRITICAL_SECTION *p) {(void)p;assert(!lock_depth++);}
static void LeaveCriticalSection(CRITICAL_SECTION *p) {(void)p;assert(--lock_depth==0);}
static void lstrcpynA(char *out,const char *in,size_t size) {snprintf(out,size,"%s",in);}
static int WaitForSingleObject(HANDLE handle,int wait) {(void)handle;(void)wait;return thread_done?WAIT_OBJECT_0:258;}
static void CloseHandle(HANDLE handle) {(void)handle;}
static DWORD P2PHostThread(void *unused) {(void)unused;abort();}
static HANDLE CreateThread(void *a,size_t b,DWORD (*c)(void *),void *d,int e,void *f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;assert(!lock_depth);++starts;thread_done=0;return (HANDLE)1;
}
static void P2PDefaults(P2PConfig *c) {memset(c,0,sizeof(*c));strcpy(c->state,"fixture");c->port=17760;}
static void P2PLoadIni(P2PConfig *c) {(void)c;assert(!lock_depth);}
static const char *P2PCheck(const P2PConfig *c) {(void)c;++checks;return check_answer;}
static BOOL P2PInprocUsed(void) {return native_used;}
static unsigned long long P2PLocalUid(void) {assert(!lock_depth);return uid;}
static BOOL principal_fresh=TRUE;
static BOOL P2PInprocPrincipalFresh(void) {return principal_fresh;}
static void P2PInprocCancelStartup(void) {assert(!lock_depth);}
static BOOL C1DvarsReady(void) {return TRUE;}
static DWORD GetTickCount(void) {return clock_now+=1000;}
static BOOL C1QueueSet(const char *key,const char *value) {
    assert(!lock_depth);
    if(!strcmp(key,"ui_p2p_ready"))snprintf(ui_ready,sizeof(ui_ready),"%s",value);
    if(!strcmp(key,"ui_p2p_state"))snprintf(ui_state,sizeof(ui_state),"%s",value);
    return TRUE;
}
static BOOL P2PPublishIntent(const char *state,BOOL ready) {
    assert(!lock_depth);++publications;published_ready=ready;snprintf(published_state,sizeof(published_state),"%s",state);return TRUE;
}
static int VoiceArgc(void) {return argc_;}
static const char *VoiceArgv(int i) {assert(i>=0&&i<argc_);return argv_[i];}
static void C1Printf(const char *fmt,...) {(void)fmt;}
static BOOL P2PIdent(const char *s,size_t max) {return P2PMatchIdent(s,max);}
static void P2PLobbyTick(void);
static void P2PHostCurrentRoom(void);
static void P2PStartIntentLaunch(BOOL prepared);
static void P2PArmLegacyRoom(void);
/* Offline mode (always on in the product): every owned room is hosted locally. */
static int offline_on;
static BOOL OfflineLobbyActive(void) {return offline_on;}
enum { P2P_AUTO_NONE, P2P_AUTO_ROOM, P2P_AUTO_LEAVE, P2P_AUTO_ERROR };
static struct {int kind;BOOL launch;unsigned long long owner;P2PRoomContext room;char error[256];} g_p2pAuto;
'''

NAMES = ['P2PSetState', 'P2PLaunchObserve', 'P2PDisarm', 'P2PArmLaunch', 'P2PLaunchLabel',
         'P2PStartupAttemptCurrent', 'P2PStartupInvalidate', 'P2PStartupIssue',
         'P2PStartConfigured', 'P2PStart', 'P2PStartupBusy', 'P2PStartupSnapshot',
         'P2PArmCurrentRoom', 'P2PAutoMode', 'P2PRoomSame', 'P2PAutoTick', 'P2PLaunchTick', 'P2PLaunchAbort', 'P2PLaunchWatch', 'P2PLobbyTick', 'P2PHostCurrentRoom',
         'P2PArmLegacyRoom', 'P2PCmdRoom', 'P2PCmdLocalhost', 'P2PCmdStartIntent', 'P2PStartIntentLaunch',
         'P2PCmdCancelStart', 'P2PCmdLaunch', 'K33LocalPlayIntent', 'K33LocalPlayLaunch',
         'K33LocalPlayAuto', 'K33LocalPlayLeave', 'P2PCmdHost', 'P2PCmdStop']

MAIN = r'''
static void args(int n,const char *a,const char *b,const char *c,const char *d,const char *e) {
    argc_=n;argv_[0]=a;argv_[1]=b;argv_[2]=c;argv_[3]=d;argv_[4]=e;
}
static void room(void) {args(4,"p2p_room","mp_killhouse_sh","war","mp",NULL);P2PCmdRoom();}
static void enable(const char *value) {args(2,"p2p_localhost",value,NULL,NULL,NULL);P2PCmdLocalhost();}
static void start(const char *action) {args(5,"p2p_start_intent",action,"mp_killhouse_sh","war","mp");P2PCmdStartIntent();}
static void room2(const char *present,const char *tier) {
    argc_=6;argv_[0]="p2p_room";argv_[1]="zm_factory_sh";argv_[2]="zm";argv_[3]="zm";argv_[4]=present;argv_[5]=tier;P2PCmdRoom();
}
static void start2(const char *present,const char *tier) {
    argc_=7;argv_[0]="p2p_start_intent";argv_[1]="ready";argv_[2]="zm_factory_sh";argv_[3]="zm";argv_[4]="zm";argv_[5]=present;argv_[6]=tier;P2PCmdStartIntent();
}
/* The match launches for this client: C1's first datagram to the armed loopback port, then a frame. */
static void launch(void) {P2PLaunchObserve(17760);P2PLobbyTick();}
static void go(const char *action) {start(action);launch();}
static void go2(const char *present,const char *tier) {start2(present,tier);launch();}
static void reset(void) {
    P2PDisarm();memset(&g_p2pApi,0,sizeof(g_p2pApi));g_p2pLaunchPending=0;
    memset(&g_p2p,0,sizeof(g_p2p));memset(&g_p2pRoom,0,sizeof(g_p2pRoom));
    memset(&g_p2pIntentConfig,0,sizeof(g_p2pIntentConfig));memset(&g_p2pIntentHeartbeat,0,sizeof(g_p2pIntentHeartbeat));
    g_p2pStartupCounter=g_p2pStartupActive=0;g_p2pStartupExhausted=0;
    g_p2pStartupEnabled=g_p2pStartupRoom=0;g_p2pStartupOwner=0;
    g_p2pEnabled=g_p2pHasRoom=g_p2pLobbyPending=0;g_p2pIntentUid=uid=42;
    g_p2pThread=0;g_p2pStop=0;g_p2pLobbyError[0]=0;g_p2pLobbyMap[0]=0;
    disconnects=0;starts=checks=publications=native_used=published_ready=0;thread_done=1;mode=1;principal_fresh=TRUE;
    offline_on=0;check_answer=NULL;memset(&g_p2pAuto,0,sizeof(g_p2pAuto));
    ui_ready[0]=ui_state[0]=published_state[0]=0;
}
int main(void) {
    unsigned long token,next;
    reset();room();enable("1");
    assert(g_p2pEnabled&&!starts&&!g_p2pStartupActive&&!g_p2pLobbyPending);
    assert(!strcmp(published_state,"idle")&&!published_ready);
    assert(!P2PStartupAttemptCurrent(0));
    launch();assert(!starts); /* a datagram without the owner's Ready/Start intent grants nothing */
    start("ready");
    assert(!starts&&!g_p2pStartupActive&&g_p2pStartPermitted&&!strcmp(published_state,"armed")&&!published_ready);
    P2PLaunchObserve(17761);P2PLobbyTick();assert(!starts); /* another loopback port is not the launch */
    start("start");assert(!starts&&g_p2pStartPermitted); /* Ready then Start: still one pending launch */
    launch();token=g_p2pIntentConfig.startupGeneration;
    assert(token&&starts==1&&P2PStartupAttemptCurrent(token)&&g_p2pLaunched&&!g_p2pLaunchPort);
    launch();assert(starts==1); /* later datagrams of the same match start nothing */
    go("start");assert(starts==1&&g_p2pIntentConfig.startupGeneration==token);
    args(5,"p2p_start_intent","ready","mp_rust","war","mp");P2PCmdStartIntent();launch();
    assert(starts==1&&g_p2pStartupActive==token&&g_p2pLobbyError[0]);
    g_p2pLobbyError[0]=0;g_p2p.state=P2P_READY;P2PLobbyTick();assert(published_ready);
    principal_fresh=FALSE;P2PLobbyTick();assert(!published_ready&&!strcmp(ui_ready,"0"));
    principal_fresh=TRUE;P2PLobbyTick();assert(published_ready);
    room();assert(!P2PStartupAttemptCurrent(token)&&!g_p2pLobbyPending&&g_p2pStop&&!g_p2pArmed);
    assert(!published_ready&&!strcmp(published_state,"stopping"));
    thread_done=1;go("ready");next=g_p2pIntentConfig.startupGeneration;
    assert(next>token&&starts==2&&P2PStartupAttemptCurrent(next));
    P2PCmdCancelStart();assert(g_p2pEnabled&&!P2PStartupAttemptCurrent(next)&&!published_ready&&!g_p2pArmed);
    go("ready");assert(starts==2); /* no restart over an owned live thread */
    thread_done=1;go("ready");token=g_p2pIntentConfig.startupGeneration;assert(starts==3);
    enable("0");assert(!g_p2pEnabled&&!P2PStartupAttemptCurrent(token)&&!published_ready);

    /* A cancelled, unticked or re-roomed pending launch can never start a host afterwards. */
    reset();room();enable("1");start("ready");P2PCmdCancelStart();launch();assert(!starts&&!g_p2pStartPermitted);
    start("ready");enable("0");launch();assert(!starts);
    enable("1");start("ready");room();launch();assert(!starts&&!g_p2pStartPermitted);
    start("ready");args(1,"p2pstop",NULL,NULL,NULL,NULL);P2PCmdStop();launch();assert(!starts&&!g_p2pEnabled);

    reset();room();enable("1");go("ready");token=g_p2pIntentConfig.startupGeneration;
    uid=0;P2PLobbyTick();assert(!P2PStartupAttemptCurrent(token)&&!g_p2pHasRoom&&!published_ready);
    uid=99;P2PLobbyTick();assert(!g_p2pHasRoom&&!P2PStartupAttemptCurrent(token));
    thread_done=1;go("ready");assert(starts==1); /* new UID needs an explicit room */
    room();go("ready");assert(starts==2&&g_p2pStartupOwner==99);
    token=g_p2pIntentConfig.startupGeneration;P2PCmdStop();assert(!P2PStartupAttemptCurrent(token));
    reset();room();enable("1");start("ready");uid=77;P2PLobbyTick();launch();
    assert(!starts&&!g_p2pStartPermitted); /* an owner change revokes the pending launch */

    reset();mode=-1;room();enable("1");
    assert(g_p2pEnabled&&!starts&&!published_ready&&!strcmp(published_state,"failed"));
    go("ready");assert(!starts&&!P2PStartupAttemptCurrent(0));
    P2PCmdCancelStart();assert(g_p2pEnabled&&!starts&&!published_ready);
    reset();room();enable("1");native_used=1;go("ready");assert(!starts&&g_p2pLobbyError[0]);
    reset();room();enable("1");g_p2pStartupCounter=0xfffffffdul;
    go("ready");assert(g_p2pStartupActive==0xfffffffful&&P2PStartupAttemptCurrent(0xfffffffful));
    P2PCmdCancelStart();thread_done=1;go("ready");
    assert(g_p2pStartupExhausted&&!g_p2pStartupActive&&starts==1);
    assert(!P2PStartupAttemptCurrent(0xfffffffful)&&!P2PStartupAttemptCurrent(0));
    reset();room();enable("1");go("ready");token=g_p2pStartupActive;
    g_p2p.state=P2P_FAILED;assert(!P2PStartupAttemptCurrent(token));
    thread_done=1;go("ready");assert(g_p2pStartupActive>token&&!P2PStartupAttemptCurrent(token));
    g_p2p.state=P2P_EXITED;assert(!P2PStartupAttemptCurrent(g_p2pStartupActive));
    native_used=1;P2PLobbyTick();assert(!strcmp(published_state,"failed")&&!published_ready);
    reset();room();enable("1");assert(!P2PStart(NULL,NULL,NULL)&&!starts);
    args(1,"p2phost",NULL,NULL,NULL,NULL);P2PCmdHost();assert(!starts);

    /* Legacy protocol 0: ticking the box arms and starts nothing; the launch starts the host once. */
    reset();mode=0;assert(P2PStartupAttemptCurrent(0)&&!P2PStartupAttemptCurrent(1));
    launch();assert(!starts);
    room();launch();assert(!starts); /* a room without the choice never hosts */
    enable("1");
    assert(!starts&&g_p2pArmed&&!g_p2pLaunched&&g_p2pLaunchPort==17760&&!strcmp(published_state,"armed")&&!published_ready);
    for(int i=0;i<5;++i)P2PLobbyTick();
    assert(!starts&&!g_p2pThread); /* frames alone never start it */
    room();assert(!starts&&g_p2pArmed); /* a re-sent room context re-arms only */
    P2PCmdCancelStart();assert(g_p2pEnabled&&!g_p2pStop&&g_p2pArmed); /* new commands inert legacy */
    start("ready");assert(!starts);
    {K33LocalPlayIntentInfo info;
     assert(K33LocalPlayIntent(&info)&&info.enabled&&info.armed&&!info.launched&&!info.ready&&info.port==17760);
     assert(info.owner_uid==42&&!strcmp(info.state,"armed")&&!strcmp(info.session_map,"mp_killhouse_sh"));
     assert(!K33LocalPlayLaunch(43,"mp_killhouse_sh","war","mp")&&!K33LocalPlayLaunch(42,"mp_rust","war","mp"));
     assert(!K33LocalPlayLaunch(42,"mp_killhouse_sh","dm","mp")&&!K33LocalPlayLaunch(0,"mp_killhouse_sh","war","mp"));
     P2PLobbyTick();assert(!starts);
     assert(K33LocalPlayLaunch(42,"mp_killhouse_sh","war","mp"));P2PLobbyTick();
     assert(starts==1&&g_p2p.cfg.startupGeneration==0&&g_p2pLaunched&&!g_p2pLaunchPort);
     assert(K33LocalPlayIntent(&info)&&info.launched&&!info.armed);
     assert(K33LocalPlayLaunch(42,"mp_killhouse_sh","war","mp")); /* idempotent for the started selection */
    }
    launch();assert(starts==1);
    enable("0");assert(g_p2pStop&&!g_p2pArmed&&!g_p2pLaunched); /* unticking ends a launched host */
    reset();mode=0;room();enable("1");enable("0");launch();assert(!starts); /* tick and untick: nothing ever ran */
    enable("1");launch();assert(starts==1);
    reset();mode=0;room();enable("1");args(4,"p2p_room","mp_rust","war","mp",NULL);P2PCmdRoom();
    assert(!starts&&g_p2pArmed&&!strcmp(g_p2pIntentConfig.map,"mp_rust"));
    launch();assert(starts==1&&!strcmp(g_p2p.cfg.map,"mp_rust")); /* the launch hosts the room as it is then */
    g_p2p.state=P2P_READY;room();assert(g_p2pStop&&!g_p2pLaunched); /* a changed room never keeps the old host */
    reset();mode=0;room();enable("1");native_used=1;room();launch();assert(!starts&&g_p2pLobbyError[0]&&!g_p2pArmed);
    reset();mode=0;room();enable("1");launch();g_p2p.state=P2P_READY;
    strcpy(g_p2p.status,"sh1x_status map=mp_killhouse_sh humans=1 bots=0");
    for(int i=0;i<200;++i)P2PLobbyTick();
    assert(!g_p2pStop); /* a played match stays up */
    strcpy(g_p2p.status,"sh1x_status map=mp_killhouse_sh humans=0 bots=4");
    for(int i=0;i<60&&!g_p2pStop;++i)P2PLobbyTick();
    assert(!g_p2pStop);
    for(int i=0;i<200&&!g_p2pStop;++i)P2PLobbyTick();
    assert(g_p2pStop&&g_p2pLobbyError[0]); /* a launched host nobody is on is stopped */

    /* A launched host that fails before it answers ends C1's connect once; a host that answered does not. */
    reset();mode=0;room();enable("1");P2PLobbyTick();assert(!disconnects);
    launch();g_p2p.state=P2P_FAILED;strcpy(g_p2p.error,"the UDP port is in use");
    for(int i=0;i<4;++i)P2PLobbyTick();
    assert(disconnects==1&&!strcmp(published_state,"failed")&&!published_ready);
    reset();mode=0;room();enable("1");launch();g_p2p.state=P2P_READY;P2PLobbyTick();
    g_p2p.state=P2P_FAILED;strcpy(g_p2p.error,"the server stopped answering");P2PLobbyTick();assert(!disconnects);
    reset();mode=0;room();enable("1");g_p2p.state=P2P_FAILED;P2PLobbyTick();assert(!disconnects); /* never launched */

    /* OFFLINE MODE: every room the local player owns is hosted; nothing is ticked and nothing unticks it.
     * The in-process lobby names the room's selection (K33LocalPlayAuto) from its own thread; the tick applies it. */
    {K33LocalPlayIntentInfo info;int port=0;char why[256];
     reset();mode=0;offline_on=1;P2PLobbyTick();
     assert(g_p2pEnabled&&!g_p2pArmed&&!starts&&K33LocalPlayIntent(&info)&&info.enabled&&!info.armed); /* on, with no room yet */
     /* the room UI's context alone arms, with no p2p_localhost 1 from anybody */
     room();assert(g_p2pArmed&&!g_p2pLaunched&&!starts&&!strcmp(published_state,"armed")&&g_p2pLaunchPort==17760);
     enable("0");assert(g_p2pEnabled&&g_p2pArmed&&!g_p2pStop); /* there is no off */
     launch();assert(starts==1&&!strcmp(g_p2p.cfg.map,"mp_killhouse_sh")); /* C1's connect to the armed port starts it */
     /* the lobby's selection, with no room UI at all (a playlist room): armed by the tick, launched by the lobby */
     reset();mode=0;offline_on=1;P2PLobbyTick();
     assert(K33LocalPlayAuto(42,"mp_rust","tdm","mp",-1,0,&port,why,sizeof(why))==1&&port==17760&&!why[0]);
     assert(!g_p2pArmed&&!starts); /* nothing happens on the caller's thread */
     P2PLobbyTick();
     assert(g_p2pArmed&&!g_p2pLaunched&&!starts&&g_p2pHasRoom&&!strcmp(g_p2pRoom.session,"mp_rust")&&!strcmp(g_p2pRoom.gametype,"war"));
     assert(K33LocalPlayIntent(&info)&&info.armed&&info.owner_uid==42&&!strcmp(info.session_map,"mp_rust")&&!strcmp(info.type,"mp"));
     for(int i=0;i<5;++i)P2PLobbyTick();
     assert(!starts); /* armed is not started */
     assert(K33LocalPlayAuto(42,"mp_rust","tdm","mp",-1,1,&port,why,sizeof(why))==1);P2PLobbyTick();
     assert(starts==1&&g_p2pLaunched&&!strcmp(g_p2p.cfg.map,"mp_rust")&&!strcmp(g_p2p.cfg.gametype,"war")&&!disconnects);
     assert(K33LocalPlayAuto(42,"mp_rust","tdm","mp",-1,1,&port,why,sizeof(why))==1);P2PLobbyTick();
     assert(starts==1); /* the launched host of this selection is joined again, not restarted */
     /* arm and launch in one step, replacing what the room UI had armed: the lobby's selection is what the client loads */
     reset();mode=0;offline_on=1;room();
     assert(K33LocalPlayAuto(42,"coop_survival_mp_fav_tropical","coop","survival",-1,1,&port,why,sizeof(why))==1);
     P2PLobbyTick();
     assert(starts==1&&!strcmp(g_p2p.cfg.map,"mp_fav_tropical")&&!strcmp(g_p2p.cfg.sessionMap,"mp_fav_tropical")&&
            !strcmp(g_p2p.cfg.gametype,"survival")&&!strcmp(g_p2p.cfg.zoneType,"survival"));
     /* Zombies carries the room's difficulty */
     reset();mode=0;offline_on=1;
     assert(K33LocalPlayAuto(42,"zm_asylum_sh","zm","zm",2,1,&port,why,sizeof(why))==1);P2PLobbyTick();
     assert(starts==1&&g_p2p.cfg.difficultyPresent==1&&g_p2p.cfg.difficulty==2&&!strcmp(g_p2p.cfg.zoneType,"zm"));
     /* an arm posted after a launch for the same selection does not take the launch back */
     reset();mode=0;offline_on=1;
     assert(K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))==1&&K33LocalPlayAuto(42,"mp_rust","war","mp",-1,0,&port,why,sizeof(why))==1);
     P2PLobbyTick();assert(starts==1);
     /* refusals: an unknown category, a map of the wrong kind, a failed check, a host that already ran - never another server */
     reset();mode=0;offline_on=1;P2PLobbyTick();
     assert(!K33LocalPlayAuto(42,"mp_rust","war","campaign",-1,1,&port,why,sizeof(why))&&!strcmp(why,"Invalid match category."));
     assert(!K33LocalPlayAuto(42,"mp_rust","zm","zm",0,1,&port,why,sizeof(why))&&!strcmp(why,"Zombies needs a Zombies map."));
     assert(!K33LocalPlayAuto(42,"mp_rust;quit","war","mp",-1,1,&port,why,sizeof(why))&&why[0]);
     assert(!K33LocalPlayAuto(0,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))&&why[0]);
     check_answer="server DLL not found (dll=)";
     assert(!K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))&&!strcmp(why,"server DLL not found (dll=)"));
     P2PLobbyTick();
     assert(!starts&&!disconnects&&!strcmp(g_p2pLobbyError,"server DLL not found (dll=)")&&!strcmp(published_state,"failed")); /* and the UI is told */
     check_answer=NULL;native_used=1;
     assert(!K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))&&strstr(why,"Restart the client"));
     /* a selection that passed on the lobby's thread but cannot be armed on the main thread ends the client's connect */
     reset();mode=0;offline_on=1;P2PLobbyTick();
     assert(K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))==1);
     native_used=1;P2PLobbyTick();
     assert(!starts&&disconnects==1&&g_p2pLobbyError[0]);
     /* the owner leaves the room: nothing stays armed; a launched host is stopped */
     reset();mode=0;offline_on=1;room();assert(g_p2pArmed);
     K33LocalPlayLeave(42);assert(g_p2pArmed);P2PLobbyTick();
     assert(!g_p2pArmed&&!g_p2pHasRoom&&g_p2pEnabled&&!g_p2pLaunchPort);
     launch();assert(!starts); /* a datagram to the old port after the leave starts nothing */
     assert(K33LocalPlayIntent(&info)&&info.enabled&&!info.armed&&!info.session_map[0]);
     reset();mode=0;offline_on=1;room();launch();assert(starts==1);g_p2p.state=P2P_READY;
     K33LocalPlayLeave(42);P2PLobbyTick();assert(g_p2pStop&&!g_p2pLaunched&&!g_p2pArmed);
     /* online builds and the launcher-managed protocols never take the lobby's calls */
     reset();mode=0;offline_on=0;assert(!K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))&&why[0]);
     P2PLobbyTick();assert(!g_p2pEnabled&&!starts);
     reset();mode=1;offline_on=1;assert(!K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why)));
     enable("1");enable("0");assert(!g_p2pEnabled); /* their untick still unticks */
    }

    reset();mode=2;room2("1","0");enable("1");go2("1","0");token=g_p2pStartupActive;
    assert(token&&starts==1&&g_p2pIntentConfig.difficultyPresent==1&&g_p2pIntentConfig.difficulty==0);
    go2("1","0");assert(starts==1&&g_p2pStartupActive==token);
    room2("1","3");assert(!P2PStartupAttemptCurrent(token)&&g_p2pStop&&!published_ready);
    assert(g_p2pIntentConfig.difficultyPresent==1&&g_p2pIntentConfig.difficulty==3);
    thread_done=1;go2("1","0");assert(starts==1&&!g_p2pStartupActive);
    go2("1","3");assert(starts==2&&P2PStartupAttemptCurrent(g_p2pStartupActive));
    const char *bad_tiers[]={"-1","4","03","3.0","3;quit"};
    for(unsigned i=0;i<sizeof(bad_tiers)/sizeof(bad_tiers[0]);++i) {
        token=g_p2pStartupActive;room2("1",bad_tiers[i]);assert(!P2PStartupAttemptCurrent(token)&&g_p2pLobbyError[0]);
    }
    P2PRoomContext parsed;const char *mp[]={"mp_rust","war","mp","0","-1"};
    assert(!P2PParseRoomContextV2(5,mp,&parsed)&&!parsed.difficultyPresent&&parsed.difficulty==-1);
    mp[3]="1";mp[4]="1";assert(P2PParseRoomContextV2(5,mp,&parsed));
    room2("0","-1");assert(g_p2pLobbyError[0]&&!g_p2pStartupActive);
    assert(!lock_depth);
    puts("Actual native commands/tick/token: arm-only, deferred launch (datagram and lobby call), offline always-on hosting (lobby selection, refusals, leave), cancel/untick/room/owner revocation, empty-host stop, explicit start, duplicate idempotency, stale context/owner, cancel/disable, no busy restart, spent guard, malformed intent, ULONG exhaustion, gen0 refusal and legacy mode PASS");
    return 0;
}
'''


def main():
    # Tick calls the legacy function defined later in the original translation unit.
    code = PREFIX + CONFIG + GLOBALS + '\n'
    code += '\n\n'.join(function(name) for name in NAMES) + MAIN
    with tempfile.TemporaryDirectory(prefix='p2p-start-intent-') as tmp:
        source, binary = Path(tmp)/'fixture.c', Path(tmp)/'fixture'
        source.write_text(code)
        subprocess.run(['gcc','-m32','-std=gnu99','-O1','-g','-Wall','-Wextra','-Werror',
                        '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable',
                        '-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        '-I',str(ROOT),str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))


if __name__ == '__main__': main()

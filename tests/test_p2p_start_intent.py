"""Execute the actual native command/tick/token bodies with deterministic OS I/O.

No Windows executable, game, socket, pipe or native DLL is launched. Linux i386
sanitizers test the production bodies, rather than a second state-machine model.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'src/p2p/p2p.c').read_text()


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
static volatile LONG g_p2pStop,g_piMatchOver;
static unsigned g_p2pLobbyRequest;
static char g_p2pLobbyMap[64],g_p2pLobbyError[256];
static P2PRoomContext g_p2pRoom;
static P2PConfig g_p2pLobbyNext,g_p2pIntentConfig,g_p2pOverride;
static BOOL g_p2pLobbyPending,g_p2pEnabled,g_p2pHasRoom,g_p2pHasOverride;
static char g_p2pArgMap[64],g_p2pArgGametype[32],g_p2pArgZone[16];
static unsigned long long g_p2pIntentUid;
static int thread_done=1,native_used,starts,checks;
static int argc_,lock_depth;
static unsigned long long uid=42;
static const char *check_answer; /* what P2PCheck says about a configuration; NULL = usable */
static unsigned long clock_now=10000;
static const char *argv_[8];
static char ui_ready[8],ui_state[32],ui_protocol[8];
static DWORD g_p2pMainThread=7;
static DWORD GetCurrentThreadId(void) {return 7;}
static BOOL g_p2pLaunchPending,g_k33TickInstalled=TRUE;
static P2PConfig g_p2pPendingConfig;
static BOOL P2PEnsureInprocActivation(void) {return TRUE;}
static void Log(const char *fmt,...) {(void)fmt;}
enum { P2P_LAUNCH_NONE, P2P_LAUNCH_DATAGRAM, P2P_LAUNCH_EXPLICIT };
#define P2P_LAUNCH_EMPTY_MS 120000u
static volatile LONG g_p2pLaunchSignal,g_p2pLaunchPort;
static BOOL g_p2pArmed,g_p2pLaunched;
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
static BOOL C1DvarsReady(void) {return TRUE;}
static DWORD GetTickCount(void) {return clock_now+=1000;}
static BOOL C1QueueSet(const char *key,const char *value) {
    assert(!lock_depth);
    if(!strcmp(key,"ui_p2p_ready"))snprintf(ui_ready,sizeof(ui_ready),"%s",value);
    if(!strcmp(key,"ui_p2p_state"))snprintf(ui_state,sizeof(ui_state),"%s",value);
    if(!strcmp(key,"ui_p2p_startup_protocol"))snprintf(ui_protocol,sizeof(ui_protocol),"%s",value);
    return TRUE;
}
static int VoiceArgc(void) {return argc_;}
static const char *VoiceArgv(int i) {assert(i>=0&&i<argc_);return argv_[i];}
static void C1Printf(const char *fmt,...) {(void)fmt;}
static BOOL P2PIdent(const char *s,size_t max) {return P2PMatchIdent(s,max);}
static void P2PLobbyTick(void);
static void P2PHostCurrentRoom(void);
static void P2PArmLegacyRoom(void);
/* Offline mode (always on in the product): every owned room is hosted locally. */
static int offline_on;
static BOOL OfflineLobbyActive(void) {return offline_on;}
enum { P2P_AUTO_NONE, P2P_AUTO_ROOM, P2P_AUTO_LEAVE, P2P_AUTO_ERROR };
static struct {int kind;BOOL launch;unsigned long long owner;P2PRoomContext room;char error[256];} g_p2pAuto;
'''

NAMES = ['P2PSetState', 'P2PLaunchObserve', 'P2PDisarm', 'P2PArmLaunch',
         'P2PStartConfigured', 'P2PStart', 'P2PStartupBusy',
         'P2PAutoMode', 'P2PRoomSame', 'P2PAutoTick', 'P2PLaunchTick', 'P2PLaunchAbort', 'P2PLaunchWatch',
         'P2PMatchOverTick', 'P2PLobbyTick', 'P2PHostCurrentRoom',
         'P2PArmLegacyRoom', 'P2PCmdRoom', 'P2PCmdLocalhost', 'P2PCmdStartIntent',
         'P2PCmdCancelStart', 'P2PCmdLaunch', 'K33LocalPlayIntent',
         'K33LocalPlayAuto', 'K33LocalPlayLeave', 'P2PCmdHost', 'P2PCmdStop']

MAIN = r'''
static void args(int n,const char *a,const char *b,const char *c,const char *d,const char *e) {
    argc_=n;argv_[0]=a;argv_[1]=b;argv_[2]=c;argv_[3]=d;argv_[4]=e;
}
static void room(void) {args(4,"p2p_room","mp_killhouse_sh","war","mp",NULL);P2PCmdRoom();}
static void enable(const char *value) {args(2,"p2p_localhost",value,NULL,NULL,NULL);P2PCmdLocalhost();}
static void start(const char *action) {args(5,"p2p_start_intent",action,"mp_killhouse_sh","war","mp");P2PCmdStartIntent();}
/* The match launches for this client: C1's first datagram to the armed loopback port, then a frame. */
static void launch(void) {P2PLaunchObserve(17760);P2PLobbyTick();}
static const char *api_state(void) {return g_p2pApi.state;}
static void reset(void) {
    P2PDisarm();memset(&g_p2pApi,0,sizeof(g_p2pApi));g_p2pLaunchPending=0;
    memset(&g_p2p,0,sizeof(g_p2p));memset(&g_p2pRoom,0,sizeof(g_p2pRoom));
    memset(&g_p2pIntentConfig,0,sizeof(g_p2pIntentConfig));
    g_p2pEnabled=g_p2pHasRoom=g_p2pLobbyPending=0;g_p2pIntentUid=uid=42;g_piMatchOver=0;
    g_p2pThread=0;g_p2pStop=0;g_p2pLobbyError[0]=0;g_p2pLobbyMap[0]=0;
    disconnects=0;starts=checks=native_used=0;thread_done=1;
    offline_on=0;check_answer=NULL;memset(&g_p2pAuto,0,sizeof(g_p2pAuto));
    ui_ready[0]=ui_state[0]=ui_protocol[0]=0;
}
int main(void) {
    /* The room's choice (the legacy LOCAL PLAY box): ticking it arms and starts nothing; the launch starts the host once. */
    reset();
    launch();assert(!starts);
    room();launch();assert(!starts); /* a room without the choice never hosts */
    enable("1");
    assert(!starts&&g_p2pArmed&&!g_p2pLaunched&&g_p2pLaunchPort==17760&&!strcmp(ui_state,"armed")&&!strcmp(ui_ready,"0"));
    assert(!strcmp(ui_protocol,"0")); /* the UI still reads the protocol: always 0 */
    for(int i=0;i<5;++i)P2PLobbyTick();
    assert(!starts&&!g_p2pThread); /* frames alone never start it */
    room();assert(!starts&&g_p2pArmed); /* a re-sent room context re-arms only */
    P2PCmdCancelStart();assert(g_p2pEnabled&&!g_p2pStop&&g_p2pArmed); /* the protocol 1-3 commands are inert */
    start("ready");assert(!starts);
    {K33LocalPlayIntentInfo info;
     assert(K33LocalPlayIntent(&info)&&info.enabled&&info.armed&&!info.launched&&!info.ready&&info.port==17760&&!info.protocol);
     assert(info.owner_uid==42&&!strcmp(info.state,"armed")&&!strcmp(info.session_map,"mp_killhouse_sh"));
     args(1,"p2p_launch",NULL,NULL,NULL,NULL);P2PCmdLaunch(); /* the explicit launch input */
     assert(starts==1&&g_p2pLaunched&&!g_p2pLaunchPort&&!strcmp(g_p2p.cfg.map,"mp_killhouse_sh"));
     assert(K33LocalPlayIntent(&info)&&info.launched&&!info.armed);
     args(1,"p2p_launch",NULL,NULL,NULL,NULL);P2PCmdLaunch();assert(starts==1); /* once */
    }
    launch();assert(starts==1);
    enable("0");assert(g_p2pStop&&!g_p2pArmed&&!g_p2pLaunched); /* unticking ends a launched host */
    reset();room();enable("1");enable("0");launch();assert(!starts); /* tick and untick: nothing ever ran */
    enable("1");launch();assert(starts==1);
    reset();room();enable("1");args(4,"p2p_room","mp_rust","war","mp",NULL);P2PCmdRoom();
    assert(!starts&&g_p2pArmed&&!strcmp(g_p2pIntentConfig.map,"mp_rust"));
    launch();assert(starts==1&&!strcmp(g_p2p.cfg.map,"mp_rust")); /* the launch hosts the room as it is then */
    g_p2p.state=P2P_READY;room();assert(g_p2pStop&&!g_p2pLaunched); /* a changed room never keeps the old host */
    reset();room();enable("1");native_used=1;room();launch();assert(!starts&&g_p2pLobbyError[0]&&!g_p2pArmed);
    reset();room();args(3,"p2p_room","mp_rust","war",NULL,NULL);P2PCmdRoom();assert(g_p2pLobbyError[0]); /* 3 fields or nothing */
    reset();room();enable("1");assert(P2PStart(NULL,NULL,NULL)&&starts==1); /* p2phost's direct start */
    reset();room();enable("1");launch();g_p2p.state=P2P_READY;
    strcpy(g_p2p.status,"sh1x_status map=mp_killhouse_sh humans=1 bots=0");
    for(int i=0;i<200;++i)P2PLobbyTick();
    assert(!g_p2pStop); /* a played match stays up */
    strcpy(g_p2p.status,"sh1x_status map=mp_killhouse_sh humans=0 bots=4");
    for(int i=0;i<60&&!g_p2pStop;++i)P2PLobbyTick();
    assert(!g_p2pStop);
    for(int i=0;i<200&&!g_p2pStop;++i)P2PLobbyTick();
    assert(g_p2pStop&&g_piMatchOver); /* a launched host nobody is on is stopped, as a match that is over */
    P2PLobbyTick();assert(g_p2pArmed&&!g_p2pLaunched&&!g_piMatchOver); /* the room is armed again for the next Ready */

    /* A launched host that fails before it answers ends C1's connect once; a host that answered does not. */
    reset();room();enable("1");P2PLobbyTick();assert(!disconnects);
    launch();g_p2p.state=P2P_FAILED;strcpy(g_p2p.error,"the UDP port is in use");
    for(int i=0;i<4;++i)P2PLobbyTick();
    assert(disconnects==1&&!strcmp(api_state(),"failed")&&!g_p2pApi.ready&&!strcmp(ui_ready,"0"));
    reset();room();enable("1");launch();g_p2p.state=P2P_READY;P2PLobbyTick();
    assert(g_p2pApi.ready&&!strcmp(ui_ready,"1"));
    g_p2p.state=P2P_FAILED;strcpy(g_p2p.error,"the server stopped answering");P2PLobbyTick();assert(!disconnects);
    reset();room();enable("1");g_p2p.state=P2P_FAILED;P2PLobbyTick();assert(!disconnects); /* never launched */

    /* OFFLINE MODE: every room the local player owns is hosted; nothing is ticked and nothing unticks it.
     * The in-process lobby names the room's selection (K33LocalPlayAuto) from its own thread; the tick applies it. */
    {K33LocalPlayIntentInfo info;int port=0;char why[256];
     reset();offline_on=1;P2PLobbyTick();
     assert(g_p2pEnabled&&!g_p2pArmed&&!starts&&K33LocalPlayIntent(&info)&&info.enabled&&!info.armed); /* on, with no room yet */
     /* the room UI's context alone arms, with no p2p_localhost 1 from anybody */
     room();assert(g_p2pArmed&&!g_p2pLaunched&&!starts&&!strcmp(ui_state,"armed")&&g_p2pLaunchPort==17760);
     enable("0");assert(g_p2pEnabled&&g_p2pArmed&&!g_p2pStop); /* there is no off */
     launch();assert(starts==1&&!strcmp(g_p2p.cfg.map,"mp_killhouse_sh")); /* C1's connect to the armed port starts it */
     /* the lobby's selection, with no room UI at all (a playlist room): armed by the tick, launched by the lobby */
     reset();offline_on=1;P2PLobbyTick();
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
     reset();offline_on=1;room();
     assert(K33LocalPlayAuto(42,"coop_survival_mp_fav_tropical","coop","survival",-1,1,&port,why,sizeof(why))==1);
     P2PLobbyTick();
     assert(starts==1&&!strcmp(g_p2p.cfg.map,"mp_fav_tropical")&&!strcmp(g_p2p.cfg.sessionMap,"mp_fav_tropical")&&
            !strcmp(g_p2p.cfg.gametype,"survival")&&!strcmp(g_p2p.cfg.zoneType,"survival"));
     /* Zombies carries the room's difficulty */
     reset();offline_on=1;
     assert(K33LocalPlayAuto(42,"zm_asylum_sh","zm","zm",2,1,&port,why,sizeof(why))==1);P2PLobbyTick();
     assert(starts==1&&g_p2p.cfg.difficultyPresent==1&&g_p2p.cfg.difficulty==2&&!strcmp(g_p2p.cfg.zoneType,"zm"));
     /* an arm posted after a launch for the same selection does not take the launch back */
     reset();offline_on=1;
     assert(K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))==1&&K33LocalPlayAuto(42,"mp_rust","war","mp",-1,0,&port,why,sizeof(why))==1);
     P2PLobbyTick();assert(starts==1);
     /* refusals: an unknown category, a map of the wrong kind, a failed check, a host that already ran - never another server */
     reset();offline_on=1;P2PLobbyTick();
     assert(!K33LocalPlayAuto(42,"mp_rust","war","campaign",-1,1,&port,why,sizeof(why))&&!strcmp(why,"Invalid match category."));
     assert(!K33LocalPlayAuto(42,"mp_rust","zm","zm",0,1,&port,why,sizeof(why))&&!strcmp(why,"Zombies needs a Zombies map."));
     assert(!K33LocalPlayAuto(42,"mp_rust;quit","war","mp",-1,1,&port,why,sizeof(why))&&why[0]);
     assert(!K33LocalPlayAuto(0,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))&&why[0]);
     check_answer="server DLL not found (dll=)";
     assert(!K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))&&!strcmp(why,"server DLL not found (dll=)"));
     P2PLobbyTick();
     assert(!starts&&!disconnects&&!strcmp(g_p2pLobbyError,"server DLL not found (dll=)")&&!strcmp(api_state(),"failed")); /* and the UI is told */
     check_answer=NULL;native_used=1;
     assert(!K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))&&strstr(why,"Restart the client"));
     /* a selection that passed on the lobby's thread but cannot be armed on the main thread ends the client's connect */
     reset();offline_on=1;P2PLobbyTick();
     assert(K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))==1);
     native_used=1;P2PLobbyTick();
     assert(!starts&&disconnects==1&&g_p2pLobbyError[0]);
     /* the owner leaves the room: nothing stays armed; a launched host is stopped */
     reset();offline_on=1;room();assert(g_p2pArmed);
     K33LocalPlayLeave(42);assert(g_p2pArmed);P2PLobbyTick();
     assert(!g_p2pArmed&&!g_p2pHasRoom&&g_p2pEnabled&&!g_p2pLaunchPort);
     launch();assert(!starts); /* a datagram to the old port after the leave starts nothing */
     assert(K33LocalPlayIntent(&info)&&info.enabled&&!info.armed&&!info.session_map[0]);
     reset();offline_on=1;room();launch();assert(starts==1);g_p2p.state=P2P_READY;
     K33LocalPlayLeave(42);P2PLobbyTick();assert(g_p2pStop&&!g_p2pLaunched&&!g_p2pArmed);
     /* without the built-in lobby the lobby's calls are refused */
     reset();offline_on=0;assert(!K33LocalPlayAuto(42,"mp_rust","war","mp",-1,1,&port,why,sizeof(why))&&why[0]);
     P2PLobbyTick();assert(!g_p2pEnabled&&!starts);
    }
    assert(!lock_depth);
    puts("Actual native commands/tick: arm-only, deferred launch (datagram and explicit p2p_launch), offline always-on hosting (lobby selection, refusals, leave), untick/room revocation, empty-host stop and re-arm, launch failure disconnect, inert protocol 1-3 commands PASS");
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
                        '-I',str(ROOT/'src'/'p2p'),str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))


if __name__ == '__main__': main()

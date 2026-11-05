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
    match = re.search(r'^static [^\n]*\b' + name + r'\([^\n]*\)\n\{', SOURCE, re.M)
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
static unsigned long clock_now=10000;
static const char *argv_[8];
static char published_state[32],ui_ready[8],ui_state[32];
static int P2PStartupProtocolMode(void) {return mode;}
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
static const char *P2PCheck(const P2PConfig *c) {(void)c;++checks;return NULL;}
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
'''

NAMES = ['P2PSetState', 'P2PStartupAttemptCurrent', 'P2PStartupInvalidate', 'P2PStartupIssue',
         'P2PStartConfigured', 'P2PStart', 'P2PStartupBusy', 'P2PStartupSnapshot',
         'P2PArmCurrentRoom', 'P2PLobbyTick', 'P2PHostCurrentRoom', 'P2PCmdRoom',
         'P2PCmdLocalhost', 'P2PCmdStartIntent', 'P2PCmdCancelStart', 'P2PCmdHost', 'P2PCmdStop']

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
static void reset(void) {
    memset(&g_p2p,0,sizeof(g_p2p));memset(&g_p2pRoom,0,sizeof(g_p2pRoom));
    memset(&g_p2pIntentConfig,0,sizeof(g_p2pIntentConfig));memset(&g_p2pIntentHeartbeat,0,sizeof(g_p2pIntentHeartbeat));
    g_p2pStartupCounter=g_p2pStartupActive=0;g_p2pStartupExhausted=0;
    g_p2pStartupEnabled=g_p2pStartupRoom=0;g_p2pStartupOwner=0;
    g_p2pEnabled=g_p2pHasRoom=g_p2pLobbyPending=0;g_p2pIntentUid=uid=42;
    g_p2pThread=0;g_p2pStop=0;g_p2pLobbyError[0]=0;g_p2pLobbyMap[0]=0;
    starts=checks=publications=native_used=published_ready=0;thread_done=1;mode=1;principal_fresh=TRUE;
    ui_ready[0]=ui_state[0]=published_state[0]=0;
}
int main(void) {
    unsigned long token,next;
    reset();room();enable("1");
    assert(g_p2pEnabled&&!starts&&!g_p2pStartupActive&&!g_p2pLobbyPending);
    assert(!strcmp(published_state,"idle")&&!published_ready);
    assert(!P2PStartupAttemptCurrent(0));
    start("ready");token=g_p2pIntentConfig.startupGeneration;
    assert(token&&starts==1&&P2PStartupAttemptCurrent(token));
    start("start");assert(starts==1&&g_p2pIntentConfig.startupGeneration==token);
    args(5,"p2p_start_intent","ready","mp_rust","war","mp");P2PCmdStartIntent();
    assert(starts==1&&g_p2pStartupActive==token&&g_p2pLobbyError[0]);
    g_p2pLobbyError[0]=0;g_p2p.state=P2P_READY;P2PLobbyTick();assert(published_ready);
    principal_fresh=FALSE;P2PLobbyTick();assert(!published_ready&&!strcmp(ui_ready,"0"));
    principal_fresh=TRUE;P2PLobbyTick();assert(published_ready);
    room();assert(!P2PStartupAttemptCurrent(token)&&!g_p2pLobbyPending&&g_p2pStop);
    assert(!published_ready&&!strcmp(published_state,"stopping"));
    thread_done=1;start("ready");next=g_p2pIntentConfig.startupGeneration;
    assert(next>token&&starts==2&&P2PStartupAttemptCurrent(next));
    P2PCmdCancelStart();assert(g_p2pEnabled&&!P2PStartupAttemptCurrent(next)&&!published_ready);
    start("ready");assert(starts==2); /* no restart over an owned live thread */
    thread_done=1;start("ready");token=g_p2pIntentConfig.startupGeneration;assert(starts==3);
    enable("0");assert(!g_p2pEnabled&&!P2PStartupAttemptCurrent(token)&&!published_ready);

    reset();room();enable("1");start("ready");token=g_p2pIntentConfig.startupGeneration;
    uid=0;P2PLobbyTick();assert(!P2PStartupAttemptCurrent(token)&&!g_p2pHasRoom&&!published_ready);
    uid=99;P2PLobbyTick();assert(!g_p2pHasRoom&&!P2PStartupAttemptCurrent(token));
    thread_done=1;start("ready");assert(starts==1); /* new UID needs an explicit room */
    room();start("ready");assert(starts==2&&g_p2pStartupOwner==99);
    token=g_p2pIntentConfig.startupGeneration;P2PCmdStop();assert(!P2PStartupAttemptCurrent(token));

    reset();mode=-1;room();enable("1");
    assert(g_p2pEnabled&&!starts&&!published_ready&&!strcmp(published_state,"failed"));
    start("ready");assert(!starts&&!P2PStartupAttemptCurrent(0));
    P2PCmdCancelStart();assert(g_p2pEnabled&&!starts&&!published_ready);
    reset();room();enable("1");native_used=1;start("ready");assert(!starts&&g_p2pLobbyError[0]);
    reset();room();enable("1");g_p2pStartupCounter=0xfffffffdul;
    start("ready");assert(g_p2pStartupActive==0xfffffffful&&P2PStartupAttemptCurrent(0xfffffffful));
    P2PCmdCancelStart();thread_done=1;start("ready");
    assert(g_p2pStartupExhausted&&!g_p2pStartupActive&&starts==1);
    assert(!P2PStartupAttemptCurrent(0xfffffffful)&&!P2PStartupAttemptCurrent(0));
    reset();room();enable("1");start("ready");token=g_p2pStartupActive;
    g_p2p.state=P2P_FAILED;assert(!P2PStartupAttemptCurrent(token));
    thread_done=1;start("ready");assert(g_p2pStartupActive>token&&!P2PStartupAttemptCurrent(token));
    g_p2p.state=P2P_EXITED;assert(!P2PStartupAttemptCurrent(g_p2pStartupActive));
    native_used=1;P2PLobbyTick();assert(!strcmp(published_state,"failed")&&!published_ready);
    reset();room();enable("1");assert(!P2PStart(NULL,NULL,NULL)&&!starts);
    args(1,"p2phost",NULL,NULL,NULL,NULL);P2PCmdHost();assert(!starts);

    reset();mode=0;assert(P2PStartupAttemptCurrent(0)&&!P2PStartupAttemptCurrent(1));
    room();enable("1");assert(starts==1&&g_p2p.cfg.startupGeneration==0);
    P2PCmdCancelStart();assert(g_p2pEnabled&&!g_p2pStop); /* new commands inert legacy */
    start("ready");assert(starts==1);
    reset();mode=2;room2("1","0");enable("1");start2("1","0");token=g_p2pStartupActive;
    assert(token&&starts==1&&g_p2pIntentConfig.difficultyPresent==1&&g_p2pIntentConfig.difficulty==0);
    start2("1","0");assert(starts==1&&g_p2pStartupActive==token);
    room2("1","3");assert(!P2PStartupAttemptCurrent(token)&&g_p2pStop&&!published_ready);
    assert(g_p2pIntentConfig.difficultyPresent==1&&g_p2pIntentConfig.difficulty==3);
    thread_done=1;start2("1","0");assert(starts==1&&!g_p2pStartupActive);
    start2("1","3");assert(starts==2&&P2PStartupAttemptCurrent(g_p2pStartupActive));
    const char *bad_tiers[]={"-1","4","03","3.0","3;quit"};
    for(unsigned i=0;i<sizeof(bad_tiers)/sizeof(bad_tiers[0]);++i) {
        token=g_p2pStartupActive;room2("1",bad_tiers[i]);assert(!P2PStartupAttemptCurrent(token)&&g_p2pLobbyError[0]);
    }
    P2PRoomContext parsed;const char *mp[]={"mp_rust","war","mp","0","-1"};
    assert(!P2PParseRoomContextV2(5,mp,&parsed)&&!parsed.difficultyPresent&&parsed.difficulty==-1);
    mp[3]="1";mp[4]="1";assert(P2PParseRoomContextV2(5,mp,&parsed));
    room2("0","-1");assert(g_p2pLobbyError[0]&&!g_p2pStartupActive);
    assert(!lock_depth);
    puts("Actual native commands/tick/token: arm-only, explicit start, duplicate idempotency, stale context/owner, cancel/disable, no busy restart, spent guard, malformed intent, ULONG exhaustion, gen0 refusal and legacy mode PASS");
    return 0;
}
'''


def main():
    # Tick calls the legacy function defined later in the original translation unit.
    code = PREFIX + CONFIG + GLOBALS + '\nstatic void P2PHostCurrentRoom(void);\n'
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

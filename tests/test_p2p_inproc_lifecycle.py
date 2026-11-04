"""Execute the real in-process monitor loop with deterministic status/time I/O.

Linux gcc only: this fixture does not load a client, DLL, socket or game state.
The loop is extracted verbatim, so the pre-fix source fails the same scenarios.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

PREFIX = r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "p2p_status.h"
typedef uint32_t DWORD;
typedef int BOOL;
typedef int LONG;
typedef size_t SIZE_T;
#define TRUE 1
#define FALSE 0
#define P2P_STATUS_MS 2000u
enum { PI_NONE, PI_INIT_PENDING, PI_RUNNING, PI_ENDED, PI_CRASHED };
enum { P2P_IDLE, P2P_STARTING, P2P_READY, P2P_STOPPING, P2P_EXITED, P2P_FAILED };
typedef struct {char map[64],sessionMap[64],gametype[32],zoneType[16],rollout[32]; int port,readyTimeout;} P2PConfig;
static struct { int state; DWORD readyMs; char status[512]; } g_p2p;
static LONG g_piState,g_piQuit,g_p2pStop,g_piYielding;
static int g_p2pLock;
static long long g_piWinCalls,g_piWinTotalUs,g_piWinMaxUs,g_piMaxGapUs;
static long g_piGapsOver1s;
static DWORD g_piInitMs,g_piFirstFrameMs;
static const char *kPiState[]={"none","starting","running","ended","crashed"};
typedef struct {DWORD delta; int response,stop,yielding,expected;} Step;
static Step steps[40];
static int count,index_next,current_response,queries;
static DWORD clock_now;
static const char *scenario;
static void Sleep(int unused) {
    (void)unused;
    if(index_next && g_p2p.state!=steps[index_next-1].expected) {
        fprintf(stderr,"%s step %d: state %d expected %d\n",scenario,index_next,g_p2p.state,steps[index_next-1].expected);
        assert(0);
    }
    if(index_next==count) {g_piState=PI_ENDED;return;}
    Step s=steps[index_next++];
    clock_now+=s.delta;current_response=s.response;
    g_p2pStop=s.stop;g_piYielding=s.yielding;
}
static DWORD GetTickCount(void) {return clock_now;}
static DWORD GetCurrentProcessId(void) {return 1234;}
static void EnterCriticalSection(int *p) {(void)p;}
static void LeaveCriticalSection(int *p) {(void)p;}
static void lstrcpynA(char *out,const char *in,size_t size) {snprintf(out,size,"%s",in);}
static void Log(const char *fmt,...) {(void)fmt;}
static void P2PSetState(int state,const char *error) {(void)error;g_p2p.state=state;}
static void InterlockedExchange(LONG *target,LONG value) {*target=value;}
static void PiMemory(SIZE_T *a,SIZE_T *b) {*a=*b=0;}
static void P2PDescriptor(const P2PConfig *c,DWORD pid,BOOL ready) {(void)c;(void)pid;(void)ready;}
static void P2PRollout(const char *path,int port,BOOL ready) {(void)path;(void)port;(void)ready;}
static int P2PRcon(int port,const char *pw,const char *cmd,char *out,size_t size,int timeout) {
    (void)port;(void)pw;(void)cmd;(void)timeout;++queries;
    if(!current_response)return 0;
    snprintf(out,size,"map=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=%d",current_response==2);
    return (int)strlen(out);
}
static int P2PStatusLine(const char *reply,char *line,size_t size) {
    size_t n=strlen(reply);if(n>=size)return 0;memcpy(line,reply,n+1);return 1;
}
static int monitor(void) {
    P2PConfig c={"mp_rust","mp_rust","war","mp","",17760,2};
    const char *password="fixture";
    LONG st=PI_RUNNING;
    DWORD t0=0;
    SIZE_T priv0=0,virt0=0,priv1=0,virt1=0;
'''
SUFFIX = r'''
    (void)registered;
    return index_next;
}
static void reset(const char *name) {
    memset(steps,0,sizeof(steps));memset(&g_p2p,0,sizeof(g_p2p));
    g_p2p.state=P2P_STARTING;g_piState=PI_RUNNING;g_piQuit=0;
    g_p2pStop=0;g_piYielding=0;clock_now=0;count=0;index_next=0;queries=0;scenario=name;
}
static void add(int response,int stop,int yielding,int expected) {
    steps[count++]=(Step){2000,response,stop,yielding,expected};
}
int main(void) {
    reset("status loss, mismatched match and recovery");
    add(1,0,0,P2P_READY);add(0,0,0,P2P_STARTING);add(1,0,0,P2P_READY);
    add(2,0,0,P2P_STARTING);add(1,0,0,P2P_READY);
    assert(monitor()==count);assert(!g_piQuit);
    reset("zone yielding revokes readiness without declaring hung");
    add(1,0,0,P2P_READY);
    for(int n=0;n<12;++n)add(0,0,1,P2P_STARTING);
    add(1,0,0,P2P_READY);
    assert(monitor()==count);assert(!g_piQuit);
    reset("late matching reply during stop");
    add(1,0,0,P2P_READY);add(1,1,0,P2P_STOPPING);add(1,0,0,P2P_STOPPING);
    assert(monitor()==count);assert(g_piQuit);assert(queries==1);
    reset("startup timeout has a bounded quit deadline");
    add(0,0,0,P2P_STARTING);
    for(int n=0;n<24;++n)add(0,0,0,P2P_FAILED);
    assert(monitor()<count);assert(g_piQuit);assert(clock_now==36000);assert(queries==2);
    reset("ready host loss has a bounded quit deadline");
    add(1,0,0,P2P_READY);
    for(int n=0;n<10;++n)add(0,0,0,P2P_STARTING);
    for(int n=0;n<24;++n)add(0,0,0,P2P_FAILED);
    assert(monitor()<count);assert(g_piQuit);assert(clock_now==56000);
    puts("PASS: five actual monitor-loop lifecycle/recovery scenarios");
    return 0;
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cc",default="gcc")
    args=parser.parse_args()
    source=(args.source/"p2p_inproc.c").read_text()
    start=source.index("    BOOL ready = FALSE, registered = FALSE, quitAsked = FALSE;")
    end=source.index("\n    st = g_piState;\n    if (registered)",start)
    with tempfile.TemporaryDirectory(prefix="p2p-inproc-lifecycle-") as tmp:
        folder=Path(tmp)
        fixture=folder/"fixture.c"
        fixture.write_text(PREFIX+source[start:end]+SUFFIX)
        binary=folder/"fixture"
        subprocess.run([args.cc,"-std=c99","-O1","-g","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-fno-omit-frame-pointer","-fno-pie","-no-pie","-I",str(args.source),str(fixture),"-o",str(binary)],check=True)
        subprocess.run([str(binary)],check=True)

if __name__=="__main__":
    main()

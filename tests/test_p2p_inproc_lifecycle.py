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
/* This monitor fixture declares the full QA statistics profile explicitly. */
#define CODOL_DIAGNOSTICS 1
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
/* Explicit producer leaves: the actual monitor is extracted below. */
static int production_mode,production_permission;
static int P2PProductionMode(void){return production_mode;}
static int P2PR_CurrentPermission(void){return production_permission;}
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
    production_mode=0;production_permission=0;
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
    reset("production monitor does not use legacy RCON readiness");
    production_mode=1;production_permission=1;
    add(1,0,0,P2P_STARTING);add(1,0,0,P2P_STARTING);
    assert(monitor()==count);assert(!g_piQuit);assert(!queries);
    reset("production permission loss requests quit under original bound");
    production_mode=1;production_permission=0;
    for(int n=0;n<24;++n)add(1,0,0,P2P_STARTING);
    assert(monitor()<count);assert(g_piQuit);assert(!queries);assert(clock_now==34000);
    puts("PASS: seven actual monitor-loop lifecycle/recovery scenarios; production leaves modeled");
    return 0;
}
'''

RETIREMENT_PREFIX = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint32_t DWORD;
enum {PI_NONE,PI_INIT_PENDING,PI_RUNNING,PI_ENDED,PI_CRASHED};
enum {P2P_IDLE,P2P_STARTING,P2P_READY,P2P_STOPPING,P2P_EXITED,P2P_FAILED};
static struct {int state;DWORD exitCode,pid;char error[256];} g_p2p;
static int startup_mode;
static DWORD g_piFaultCode;static int g_piInitResult,g_p2pLock;
static int mockCode(void){return 0;}
static int (*g_piCode)(void)=mockCode;
static DWORD GetCurrentProcessId(void){return 1234;}
static void EnterCriticalSection(int *x){(void)x;}
static void LeaveCriticalSection(int *x){(void)x;}
static void lstrcpynA(char *out,const char *in,size_t n){snprintf(out,n,"%s",in);}
static void classify(int st,int incoming,int init_error){
 memset(&g_p2p,0,sizeof g_p2p);g_p2p.state=incoming;g_piInitResult=init_error;
'''
RETIREMENT_SUFFIX = r'''
}
int main(void){
 (void)GetCurrentProcessId; (void)startup_mode;
 classify(PI_RUNNING,P2P_STOPPING,0);
 assert(g_p2p.state==P2P_FAILED && g_p2p.pid==1234); /* old exact epilogue RED */
 classify(PI_INIT_PENDING,P2P_STARTING,0);assert(g_p2p.state==P2P_FAILED && g_p2p.pid==1234);
 classify(PI_RUNNING,P2P_FAILED,0);assert(g_p2p.state==P2P_FAILED && g_p2p.pid==1234);
 classify(PI_ENDED,P2P_STOPPING,0);assert(g_p2p.state==P2P_EXITED && !g_p2p.pid);
 classify(PI_CRASHED,P2P_READY,0);assert(g_p2p.state==P2P_FAILED && !g_p2p.pid);
 classify(PI_ENDED,P2P_STARTING,1);assert(g_p2p.state==P2P_FAILED && !g_p2p.pid);
 puts("PASS six exact-source retirement epilogue cases; native/OS doubles");
 return 0;
}
'''

# Explicit WinSock/engine doubles; only the two actual packet-filter bodies
# execute. This proves source control flow, not Windows ABI or network behavior.
HOSTGATE_PREFIX = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t DWORD;typedef uint32_t SOCKET;typedef uint32_t u_long;
typedef struct {unsigned n;} fd_set;
struct sockaddr {uint16_t sa_family;char data[14];};
struct in_addr {uint32_t s_addr;};
struct sockaddr_in {uint16_t sin_family,sin_port;struct in_addr sin_addr;char zero[8];};
struct sockaddr_storage {uint16_t family;char rest[126];};
struct timeval {long sec,usec;};
enum {AF_INET=2,SOCKET_ERROR=-1,MSG_PEEK=2,FIONREAD=1,WSAEWOULDBLOCK=10035};
#define WINAPI
#define FD_ZERO(p) ((p)->n=0)
#define FD_SET(s,p) ((p)->n=(s))
static int g_hgMode,decision,trusted,sent,received,refused,borrowed,last_error;
static struct sockaddr_in source;
static int recv_leaf(SOCKET s,char*b,int n,int flags,struct sockaddr*f,int*length){(void)s;(void)b;(void)n;(void)flags;++received;memcpy(f,&source,sizeof source);*length=sizeof source;return 7;}
static int send_leaf(SOCKET s,const char*b,int n,int flags,const struct sockaddr*t,int z){(void)s;(void)b;(void)flags;(void)t;(void)z;++sent;return n;}
static int (*g_hgRecvFrom)(SOCKET,char*,int,int,struct sockaddr*,int*)=recv_leaf;
static int (*g_hgSendTo)(SOCKET,const char*,int,int,const struct sockaddr*,int)=send_leaf;
static unsigned ntohs(unsigned n){return n;}
static int ioctlsocket(SOCKET s,long cmd,u_long*v){(void)s;(void)cmd;*v=0;return 0;}
static int select(int n,fd_set*r,void*w,void*x,struct timeval*t){(void)n;(void)r;(void)w;(void)x;(void)t;return 0;}
static void WSASetLastError(int e){last_error=e;}
static int P2PProductionGuestAddress(DWORD ip,unsigned port){(void)ip;(void)port;return decision;}
static int HostGateOk(DWORD ip){(void)ip;return trusted;}
static void HostGateRefused(DWORD ip,int out,int port){(void)ip;(void)out;(void)port;++refused;}
static void P2PProductionBorrowSend(SOCKET s){(void)s;++borrowed;}
'''
HOSTGATE_SUFFIX = r'''
static int failures,cases;
static void one(int mode,int production,int allow,const char*name){
 char buffer[16];struct sockaddr_in from;int length=sizeof from,result;
 int accepted=production>0||(production==0&&(allow||mode<2));
 g_hgMode=mode;decision=production;trusted=allow;sent=received=refused=borrowed=last_error=0;
 result=HostGateSendTo(17,buffer,7,0,(struct sockaddr*)&source,sizeof source);++cases;
 if(result!=7||sent!=accepted||borrowed!=1){printf("FAIL %s send: accepted=%d actual-wire-sends=%d return=%d\n",name,accepted,sent,result);++failures;}
 result=HostGateRecvFrom(17,buffer,sizeof buffer,0,(struct sockaddr*)&from,&length);++cases;
 if((accepted&&result!=7)||(!accepted&&(result!=SOCKET_ERROR||last_error!=WSAEWOULDBLOCK))){printf("FAIL %s recv: accepted=%d delivered=%d\n",name,accepted,result);++failures;}
}
int main(void){memset(&source,0,sizeof source);source.sin_family=AF_INET;source.sin_port=45011;source.sin_addr.s_addr=0xc633640b;
 one(1,-1,0,"production-negative/legacy-mode1");one(2,-1,0,"production-negative/legacy-mode2");
 one(1,0,0,"unselected-legacy-mode1");one(2,0,0,"unselected-legacy-mode2");
 one(2,0,1,"unselected-vouched-legacy-mode2");one(2,1,0,"production-exact-endpoint");
 printf("RESULT %d actual function cases, %d failures; explicit WinSock/engine doubles, no network\n",cases,failures);return failures;
}
'''


def hostgate_fixture(source):
    def body(name):
        begin = source.index('static int WINAPI '+name+'(')
        first = source.index('{', begin)
        depth = 0
        for end in range(first, len(source)):
            if source[end] == '{':
                depth += 1
            elif source[end] == '}':
                depth -= 1
                if depth == 0:
                    return source[begin:end+1]
        raise AssertionError('HostGate function end '+name)
    return HOSTGATE_PREFIX+'\n'.join(body(name) for name in
        ('HostGateRecvFrom', 'HostGateSendTo'))+HOSTGATE_SUFFIX


def retirement_fixture(source):
    assignment=source.index("    g_p2p.exitCode = st == PI_CRASHED")
    begin=source.rfind("    EnterCriticalSection(&g_p2pLock);",0,assignment)
    marker="    LeaveCriticalSection(&g_p2pLock);"
    end=source.index(marker,assignment)+len(marker)
    if not 0 <= begin < assignment < end:
        raise AssertionError("unique native retirement epilogue")
    return RETIREMENT_PREFIX+source[begin:end]+RETIREMENT_SUFFIX


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cc",default="gcc")
    args=parser.parse_args()
    source=(args.source/"src/p2p/p2p_inproc.c").read_text()
    start=source.index("    BOOL ready = FALSE, registered = FALSE, quitAsked = FALSE;")
    end=source.index("\n    st = g_piState;\n",start)
    with tempfile.TemporaryDirectory(prefix="p2p-inproc-lifecycle-") as tmp:
        folder=Path(tmp)
        fixture=folder/"fixture.c"
        fixture.write_text(PREFIX+source[start:end]+SUFFIX)
        binary=folder/"fixture"
        subprocess.run([args.cc,"-std=c99","-O1","-g","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-fno-omit-frame-pointer","-fno-pie","-no-pie","-I",str(args.source/"src"/"p2p"),str(fixture),"-o",str(binary)],check=True,timeout=30)
        subprocess.run([str(binary)],check=True,timeout=10)
        retirement=folder/"retirement.c"
        retirement.write_text(retirement_fixture(source))
        for local_testing in (0,1):
            executable=folder/("retirement-%d"%local_testing)
            subprocess.run([args.cc,"-std=c99","-O1","-g","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-fno-omit-frame-pointer","-fno-pie","-no-pie","-DCODOL_LOCAL_TESTING=%d"%local_testing,str(retirement),"-o",str(executable)],check=True,timeout=30)
            subprocess.run([str(executable)],check=True,timeout=10)

        hostgate=folder/"hostgate.c"
        hostgate.write_text(hostgate_fixture((args.source/"src/p2p/hostgate.c").read_text()))
        gate_binary=folder/"hostgate"
        subprocess.run([args.cc,"-std=c11","-O1","-g","-Wall","-Wextra","-Wpedantic","-Werror","-fsanitize=address,undefined","-fno-omit-frame-pointer","-fno-pie","-no-pie",str(hostgate),"-o",str(gate_binary)],check=True,timeout=30)
        subprocess.run([str(gate_binary)],check=True,timeout=10)

if __name__=="__main__":
    main()

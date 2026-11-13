/* Actual extracted client fiber/bridge bodies and native owner API. Windows
 * kernel/fiber/Init/Frame I/O is explicit scaffolding. Native CVSP and CPRB
 * parsing/cloning are real source. No Windows/game executable or DLL load. */
#include "principal_clock_io.inc"
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include "../src/p2p/p2p_startup_wire.h"
#include "../src/p2p/p2p_startup_ownership.h"
#include "codol_verified_sources.h"
#include "mbedtls/sha256.h"
#define __declspec(x)
#define WINAPI
typedef int32_t LONG;
typedef unsigned char BYTE;
typedef unsigned long ULONG;
typedef ULONG *PULONG;
typedef uint64_t ULONGLONG;
typedef struct { DWORD dwLowDateTime,dwHighDateTime; } FILETIME;
typedef union {struct {DWORD LowPart,HighPart;};ULONGLONG QuadPart;} ULARGE_INTEGER;
typedef void *HMODULE;
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define WAIT_TIMEOUT 258u
#define P2PVS_TIMEOUT_MS 30000u
#define P2P_FAILED 9
#define P2P_EXITED 10
#define PI_NONE 0
#define PI_INIT_PENDING 1
#define PI_RUNNING 2
#define PI_ENDED 3
#define PI_CRASHED 4
#define CODOL_LOCAL_TESTING 1
typedef int CRITICAL_SECTION;
static CRITICAL_SECTION g_p2pLock;
static int g_p2pInit=1,g_p2pStartupExhausted,g_p2pStartupEnabled=1,g_p2pStartupRoom=1;
static unsigned long g_p2pStartupActive=1,g_p2pStartupCounter=1;
static unsigned long long g_p2pStartupOwner=1001;
static volatile LONG g_p2pStop;
static struct { int state; } g_p2p;
static int parent_alive=1,protocol=1;
static ULONG actual_server_pid=42;
static HANDLE g_p2psParent=(HANDLE)42;
static ULONG g_p2psParentPid=42;
static unsigned fixture_thread=11;
static int fixture_fiber,freed,closed,ack_sent,script_calls,principal_calls,current_calls,init_calls,frame_calls,quit_calls,quarantines,lock_depth;
static int script_result=1,principal_result=1,init_result,mode;
static unsigned char ack_bytes[76];
static jmp_buf back_to_main;
enum { NORMAL,CANCEL_SCRIPT,CANCEL_PRINCIPAL,CANCEL_CURRENT,PARENT_CURRENT,DEADLINE_CURRENT,ABORT_OWNED,PIPE_PID_CURRENT,CANCEL_YIELD };
static LONG InterlockedExchange(volatile LONG *p,LONG n)
{return __atomic_exchange_n(p,n,__ATOMIC_SEQ_CST);}
#undef InterlockedCompareExchange
static LONG InterlockedCompareExchange(volatile LONG *p,LONG n,LONG expected)
{__atomic_compare_exchange_n(p,&expected,n,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return expected;}
static void EnterCriticalSection(CRITICAL_SECTION *p){(void)p;++lock_depth;}
static void LeaveCriticalSection(CRITICAL_SECTION *p){(void)p;assert(lock_depth>0);--lock_depth;}
static DWORD GetCurrentThreadId(void){return fixture_thread;}
qboolean Sys_IsMainThread(void){return fixture_thread==11;}
static DWORD GetTickCount(void){return clock_ms;}
static void GetSystemTimeAsFileTime(FILETIME *out)
{ULONGLONG ticks=116444736000000000ull+(ULONGLONG)realtime.tv_sec*10000000ull+(ULONGLONG)realtime.tv_nsec/100;
 out->dwLowDateTime=(DWORD)ticks;out->dwHighDateTime=(DWORD)(ticks>>32);}
static DWORD GetProcessId(HANDLE handle){return handle==(HANDLE)42?42:0;}
static DWORD WaitForSingleObject(HANDLE handle,DWORD timeout)
{(void)timeout;return handle==(HANDLE)42&&parent_alive?WAIT_TIMEOUT:0;}
static BOOL server_pid(HANDLE handle,PULONG out)
{if(handle!=(HANDLE)77)return FALSE;*out=actual_server_pid;return TRUE;}
static HMODULE GetModuleHandleA(const char *name){(void)name;return (HMODULE)1;}
static void *GetProcAddress(HMODULE module,const char *name)
{(void)module;return !strcmp(name,"GetNamedPipeServerProcessId")?(void *)server_pid:NULL;}
static DWORD GetEnvironmentVariableA(const char *name,char *out,DWORD size)
{(void)name;if(size<2)return 2;out[0]=protocol?'1':'0';out[1]=0;return 1;}
static BOOL CloseHandle(HANDLE handle){assert(handle==(HANDLE)77);++closed;return TRUE;}
#define SecureZeroMemory(p,n) memset(p,0,n)
static void P2PVerifiedScriptsFree(void *p,unsigned n)
{if(p){memset(p,0,n);free(p);++freed;}}
static int CODOL_HostMatchInitialize(void){return 1;} /* Initial provider seam only; no rehost permission. */
static int fixture_scripts(const void *,unsigned);
static int fixture_principals(const void *,unsigned);
static int fixture_current(void);
static void fixture_quarantine(void);
static int fixture_init(const char *);
static int fixture_frame(void);
static void fixture_cmd(const char *);
static void SwitchToFiber(void *fiber){(void)fiber;longjmp(back_to_main,1);}
static void *g_piMainFiber=(void *)1;
static volatile LONG g_piState,g_piQuit,g_piFaultTest,g_piStartupSelected=1,g_piPrincipalFresh,g_piMatchFresh;
static LONG g_piSession=1; /* Actual Run reserves before module/fiber dispatch; runner checks source ordering. */
static char g_piArgs[32]="fixture";
static int g_piInitResult;
static DWORD g_piInitMs,g_piFirstFrameMs;
static int (*g_piInit)(const char *)=fixture_init;
static int (*g_piFrame)(void)=fixture_frame;
static void (*g_piCmd)(const char *)=fixture_cmd;
static void (*g_piCallFn)(void *);
static void *g_piCallCtx;
static int (*g_piStartupScripts)(const void *,unsigned)=fixture_scripts;
static int (*g_piStartupPrincipals)(const void *,unsigned)=fixture_principals;
static int (*g_piStartupOptions)(const void *,unsigned);
static int (*g_piObserveOptions)(const void *,unsigned);
static int (*g_piPrincipalCurrent)(void)=fixture_current;
static void (*g_piPrincipalQuarantine)(void)=fixture_quarantine;
#include "actual_startup_declarations.inc"
static P2PStartupBridge g_piStartup;
static BOOL P2PSTransfer(P2PStartupBridge *b,void *raw,DWORD n,BOOL write);
#include "actual_startup_functions.inc"

static BOOL P2PSTransfer(P2PStartupBridge *b,void *raw,DWORD n,BOOL write)
{assert(write&&n==76&&P2PSCurrent(b));memcpy(ack_bytes,raw,n);++ack_sent;return TRUE;}
static int fixture_scripts(const void *p,unsigned n)
{
    assert(fixture_fiber&&fixture_thread==11&&!init_calls);++script_calls;
    int result=script_result==1?Sh1xHost_UseVerifiedScripts(p,n):
        script_result==0?Sh1xHost_UseVerifiedScripts(p,1):script_result;
    if(mode==CANCEL_SCRIPT)++g_p2pStartupActive;
    return result;
}
static int fixture_principals(const void *p,unsigned n)
{
    assert(fixture_fiber&&fixture_thread==11&&script_calls==1&&!init_calls);++principal_calls;
    int result=principal_result==1?Sh1xHost_InstallRelayPrincipals(p,n):
        principal_result==0?Sh1xHost_InstallRelayPrincipals(p,1):principal_result;
    if(mode==CANCEL_PRINCIPAL)++g_p2pStartupActive;
    if(mode==ABORT_OWNED){fixture_thread=22;PiAbortStartupWorker();fixture_thread=11;
        assert(g_piStartup.packet&&g_piStartup.phase==P2PS_FIBER_OWNED&&!freed);}
    return result;
}
static int fixture_current(void)
{
    assert(fixture_fiber&&fixture_thread==11);++current_calls;
    int result=Sh1xHost_RelayPrincipalsCurrent();
    if(current_calls==2) {
        if(mode==CANCEL_CURRENT)++g_p2pStartupActive;
        if(mode==PARENT_CURRENT)parent_alive=0;
        if(mode==DEADLINE_CURRENT)clock_ms+=P2PVS_TIMEOUT_MS;
        if(mode==PIPE_PID_CURRENT)actual_server_pid=43;
    }
    return result;
}
static void fixture_quarantine(void){assert(!lock_depth);++quarantines;Sh1xHost_QuarantineRelayPrincipals();}
static int fixture_init(const char *args)
{
    (void)args;assert(fixture_fiber&&fixture_thread==11&&script_calls==1&&principal_calls==1);
    assert(CODOL_VerifiedSourcesSelected()&&CODOL_VerifiedSourcesAvailable());
    assert(Sh1xHost_RelayPrincipalsCurrent()==1);++init_calls;
    if(!init_result)host_state=1;
    return init_result;
}
static int fixture_frame(void)
{
    assert(fixture_fiber&&fixture_thread==11);++frame_calls;
    if(mode==CANCEL_YIELD&&frame_calls==1) {
        /* Actual native Frame is suspended in a same-thread C1-fiber yield.
         * Only the atomic deny export is allowed here; query/install refuse. */
        host_in_call=1;fixture_fiber=0;
        codol_principal_policy_t before=principal_policy;BYTE *packet=g_piStartup.packet;
        assert(!Sh1xHost_RelayPrincipalsCurrent()&&!principal_faulted());
        P2PStartupInvalidate();
        assert(!lock_depth&&principal_faulted()&&g_piStartup.aborted&&!g_piPrincipalFresh&&g_piQuit==1);
        assert(!memcmp(&before,&principal_policy,sizeof before)&&host_in_call==1&&host_state==1);
        assert(g_piStartup.packet==packet&&packet&&!freed&&!closed&&!ack_sent&&g_piState==PI_RUNNING);
        assert(script_calls==1&&principal_calls==1&&init_calls==1&&frame_calls==1);
        fixture_fiber=1;host_in_call=0;
        assert(!Sh1xHost_RelayPrincipalsCurrent());
        assert(!Sh1xHost_InstallRelayPrincipals(g_piStartup.view.principals,g_piStartup.view.principals_length));
    }
    return !quit_calls;
}
static void fixture_cmd(const char *text){assert(!strcmp(text,"quit\n"));++quit_calls;}
static void resume_fiber(void)
{fixture_thread=11;fixture_fiber=1;if(!setjmp(back_to_main))PiServerFiber(NULL);fixture_fiber=0;}
static int sha256(const void *p,size_t n,unsigned char out[32])
{return mbedtls_sha256_ret(p,n,out,0)==0?1:0;}
static unsigned char *load(const char *name,size_t *length)
{
    FILE *f=fopen(name,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0&&n<1048576);
    rewind(f);unsigned char *p=malloc((size_t)n);assert(p&&fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*length=(size_t)n;return p;
}
int main(int argc,char **argv)
{
    assert(argc==4);size_t qn,pn;unsigned char *q=load(argv[2],&qn),*p=load(argv[3],&pn);
    g_piStartup.pipe=(HANDLE)77;g_piStartup.started=clock_ms;g_piStartup.attempt=1;
    g_piStartup.packet=p;g_piStartup.packet_size=(DWORD)pn;g_piStartup.phase=P2PS_FETCHED;g_piStartup.fetch_claimed=1;
    assert(P2PStartupParseEnvelope(q,qn,p,pn,sha256,&g_piStartup.view));free(q);
    g_piStartup.protocol=(int)g_piStartup.view.protocol;
    /* Golden native principal lease is wall1000000ms/mono10000ms. */
    realtime.tv_sec=1000;realtime.tv_nsec=0;monotime.tv_sec=10;monotime.tv_nsec=0;
    g_piStartup.expires_ms=1025000;g_piStartup.scope_started=clock_ms;g_piStartup.scope_lifetime=25000;
    if(!strcmp(argv[1],"script-fail"))script_result=0;
    else if(!strcmp(argv[1],"script-two"))script_result=2;
    else if(!strcmp(argv[1],"principal-fail"))principal_result=0;
    else if(!strcmp(argv[1],"principal-two"))principal_result=2;
    else if(!strcmp(argv[1],"cancel-before"))++g_p2pStartupActive;
    else if(!strcmp(argv[1],"cancel-script"))mode=CANCEL_SCRIPT;
    else if(!strcmp(argv[1],"cancel-principal"))mode=CANCEL_PRINCIPAL;
    else if(!strcmp(argv[1],"cancel-current"))mode=CANCEL_CURRENT;
    else if(!strcmp(argv[1],"parent-current"))mode=PARENT_CURRENT;
    else if(!strcmp(argv[1],"deadline-current"))mode=DEADLINE_CURRENT;
    else if(!strcmp(argv[1],"pipe-pid-current"))mode=PIPE_PID_CURRENT;
    else if(!strcmp(argv[1],"timeout-owned"))mode=ABORT_OWNED;
    else if(!strcmp(argv[1],"init-fail"))init_result=-5;
    else if(!strcmp(argv[1],"cancel-yield"))mode=CANCEL_YIELD;
    else if(!strcmp(argv[1],"cancel-wins")){fixture_thread=22;PiAbortStartupWorker();fixture_thread=11;
        assert(!g_piStartup.packet&&freed==1&&g_piStartup.phase==P2PS_CANCELLED);}
    g_piState=PI_INIT_PENDING;resume_fiber();
    assert(g_piSession==1); /* No guard-clearing path is executed. */
    if(mode==CANCEL_YIELD) {
        assert(g_piState==PI_RUNNING&&principal_faulted()&&!g_piPrincipalFresh&&g_piQuit==1&&!freed&&!closed);
        assert(!P2PInprocPrincipalFresh());resume_fiber();
        assert(g_piState==PI_ENDED&&quit_calls==1&&P2PInprocUsed()&&principal_faulted());
        fixture_thread=22;PiAbortStartupWorker();fixture_thread=11;
        assert(!g_piStartup.packet&&freed==1&&closed==1);
        puts("PASS cancel-yield: actual UI invalidation denies native ingress immediately while Frame is yielded; no buffer free or engine-state override; resume stays terminal");
        return 0;
    }
    int success=!strcmp(argv[1],"success")||!strcmp(argv[1],"wrong-thread")||!strcmp(argv[1],"runtime-expiry")||!strcmp(argv[1],"wall-expiry")||!strcmp(argv[1],"wall-expiry-latch");
    if(success) {
        assert(init_calls==1&&g_piState==PI_RUNNING&&g_piPrincipalFresh==1);
        assert(script_calls==1&&principal_calls==1&&g_piStartup.phase==P2PS_FIBER_DONE);
        fixture_thread=22;assert(P2PSAcknowledge(&g_piStartup));fixture_thread=11;
        assert(ack_sent==1&&!g_piStartup.aborted&&!g_piStartup.packet&&freed==1);
        assert(ack_bytes[72]==1&&ack_bytes[73]==0&&ack_bytes[74]==0&&ack_bytes[75]==0);
        char *copy=NULL;size_t n=0;assert(CODOL_ReadVerifiedSource("maps/start.gsc",&copy,&n)==1&&n==10);free(copy);
        if(!strcmp(argv[1],"wrong-thread")) {
            fixture_thread=22;assert(!Sh1xHost_RelayPrincipalsCurrent());
            assert(!Sh1xHost_InstallRelayPrincipals("x",1));assert(!principal_faulted());
            Sh1xHost_QuarantineRelayPrincipals();fixture_thread=11;
            assert(principal_faulted()&&!Sh1xHost_RelayPrincipalsCurrent());
        }
        if(!strcmp(argv[1],"runtime-expiry")||!strcmp(argv[1],"wall-expiry")||!strcmp(argv[1],"wall-expiry-latch")) {
            if(!strcmp(argv[1],"runtime-expiry"))clock_ms+=25000;
            else realtime.tv_sec=1025;
            assert(!P2PInprocPrincipalFresh());
            if(!strcmp(argv[1],"wall-expiry-latch")){realtime.tv_sec=1000;assert(!P2PInprocPrincipalFresh());}
            resume_fiber();
            assert(!g_piPrincipalFresh&&g_piQuit&&quarantines&&principal_faulted()&&quit_calls);
        }
    } else {
        assert(g_piState==PI_ENDED&&g_piInitResult!=0&&quarantines&&principal_faulted()&&!g_piPrincipalFresh);
        assert(P2PInprocUsed());
        if(init_result)assert(init_calls==1&&g_piInitResult==-5);
        else assert(!init_calls);
        if(!strcmp(argv[1],"script-fail")||!strcmp(argv[1],"script-two")||!strcmp(argv[1],"cancel-script"))assert(!principal_calls);
        if(mode==ABORT_OWNED)assert(g_piStartup.packet&&!freed&&g_piStartup.aborted);
        else if(g_piStartup.packet){fixture_thread=22;PiAbortStartupWorker();fixture_thread=11;assert(!g_piStartup.packet&&freed==1);}
        if(mode!=ABORT_OWNED)assert(!g_piStartup.packet);
    }
    printf("PASS %s: actual fiber/install/abort bodies, actual native owner API/provider/parser, explicit OS/engine I/O doubles\n",argv[1]);
    return 0;
}

/* Optional local-testing client for the owned SDK's versioned duplex pipe.
 * This is not enabled by existing launchers. No PID-only parent recovery,
 * cross-process native call, loose script fallback or active lease renewal. */
#if CODOL_LOCAL_TESTING
#include <wincrypt.h>
#include "p2p_startup_wire.c"
#include "p2p_startup_ownership.h"
#include "p2p_active_wire.h"

static BOOL P2PStartupAttemptCurrent(unsigned long generation); /* p2p.c */
static LONG P2PSAtomicCAS(volatile LONG *where,LONG value,LONG expected)
{ return InterlockedCompareExchange(where,value,expected); }
typedef struct {
    volatile LONG fetch_claimed;
    HANDLE pipe;
    DWORD started;
    unsigned long attempt;
    BYTE *packet;
    DWORD packet_size;
    p2p_startup_view_t view;
    p2pa_bootstrap_t active_bootstrap; /* value copy before packet reclamation */
    ULONGLONG expires_ms;
    DWORD scope_started,scope_lifetime;
    volatile LONG phase;
    volatile LONG aborted;
    int scripts_result,principals_result;
} P2PStartupBridge;
static HANDLE g_p2psParent;
static ULONG g_p2psParentPid;
static BOOL P2PSWallMillis(ULONGLONG *out)
{
    FILETIME time;ULARGE_INTEGER ticks;
    GetSystemTimeAsFileTime(&time);ticks.LowPart=time.dwLowDateTime;ticks.HighPart=time.dwHighDateTime;
    if(ticks.QuadPart<116444736000000000ull)return FALSE;
    *out=(ticks.QuadPart-116444736000000000ull)/10000ull;return TRUE;
}

static int P2PStartupProtocolMode(void)
{
    char value[4];DWORD size=GetEnvironmentVariableA("CODOL_P2P_STARTUP_IPC_V1",value,sizeof(value));
    if(!size||(size==1&&value[0]=='0'))return 0;
    return size==1&&value[0]=='1'?1:-1;
}
static BOOL P2PSParentAlive(void)
{
    return g_p2psParent&&GetProcessId(g_p2psParent)==g_p2psParentPid&&
        WaitForSingleObject(g_p2psParent,0)==WAIT_TIMEOUT;
}
static BOOL P2PSCaptureParent(void)
{
    char value[32],pid_text[16];DWORD size,pid_size;ULONG number,pid;
    HANDLE inherited,retained=NULL;
    if(g_p2psParent)return P2PSParentAlive();
    size=GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_HANDLE",value,sizeof(value));
    pid_size=GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_PID",pid_text,sizeof(pid_text));
    if(!size||size>=sizeof(value)||!pid_size||pid_size>=sizeof(pid_text)||
        !P2PVSPid(value,&number)||number==0xfffffffful||!P2PVSPid(pid_text,&pid))return FALSE;
    inherited=(HANDLE)(ULONG_PTR)number;
    if(GetProcessId(inherited)!=pid||WaitForSingleObject(inherited,0)!=WAIT_TIMEOUT)return FALSE;
    if(!DuplicateHandle(GetCurrentProcess(),inherited,GetCurrentProcess(),&retained,
        SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,0))return FALSE;
    /* This unique inherited handle was created for this child only. The SDK
     * keeps its own handle. Retain the private copy for the client lifetime. */
    CloseHandle(inherited);
    SetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PARENT_HANDLE",NULL);
    g_p2psParent=retained;g_p2psParentPid=pid;
    return P2PSParentAlive();
}
static BOOL P2PSRefuse(const P2PStartupBridge *bridge)
{
    if(bridge)InterlockedExchange((volatile LONG *)&bridge->aborted,1);
    return FALSE;
}
static BOOL P2PSCurrent(const P2PStartupBridge *bridge)
{
    typedef BOOL (WINAPI *ServerPidFn)(HANDLE,PULONG);
    ServerPidFn server_pid;ULONG actual=0;ULONGLONG wall;
    if(!bridge||InterlockedCompareExchange((volatile LONG *)&bridge->aborted,0,0)||
        P2PStartupProtocolMode()!=1||!P2PStartupAttemptCurrent(bridge->attempt)||
        !P2PSParentAlive()||GetTickCount()-bridge->started>=P2PVS_TIMEOUT_MS)return P2PSRefuse(bridge);
    if(bridge->expires_ms&&(!P2PSWallMillis(&wall)||wall>=bridge->expires_ms||
        GetTickCount()-bridge->scope_started>=bridge->scope_lifetime))return P2PSRefuse(bridge);
    if(bridge->pipe==INVALID_HANDLE_VALUE)return TRUE;
    server_pid=(ServerPidFn)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetNamedPipeServerProcessId");
    if(!server_pid||!server_pid(bridge->pipe,&actual)||actual!=g_p2psParentPid)return P2PSRefuse(bridge);
    return TRUE;
}
static int P2PSSha256(const void *data,size_t size,unsigned char output[32])
{
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;DWORD length=32;int result=0;
    if(size>0xfffffffful)return 0;
    if(!CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT)||
        !CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)||
        !CryptHashData(hash,(const BYTE *)data,(DWORD)size,0)||
        !CryptGetHashParam(hash,HP_HASHVAL,output,&length,0)||length!=32)goto done;
    result=1;
done:
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    return result;
}
static int P2PSNonce(unsigned char output[32])
{
    HCRYPTPROV provider=0;int result=0;
    if(CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT)) {
        result=CryptGenRandom(provider,32,output)&&p2ps_nonzero(output,32);
        CryptReleaseContext(provider,0);
    }
    return result;
}
/* One authorization deadline, retained parent wait, cancellation checks and drained
 * OVERLAPPED lifetime. A late kernel completion never targets a freed buffer. */
static BOOL P2PSTransfer(P2PStartupBridge *bridge,void *buffer,DWORD size,BOOL write)
{
    BYTE *cursor=buffer;
    while(size) {
        OVERLAPPED operation;DWORD count=0;BOOL done;
        if(!P2PSCurrent(bridge))return FALSE;
        ZeroMemory(&operation,sizeof(operation));
        operation.hEvent=CreateEventA(NULL,TRUE,FALSE,NULL);
        if(!operation.hEvent)return FALSE;
        done=write?WriteFile(bridge->pipe,cursor,size,&count,&operation):
            ReadFile(bridge->pipe,cursor,size,&count,&operation);
        if(!done&&GetLastError()==ERROR_IO_PENDING) {
            HANDLE waits[2]={operation.hEvent,g_p2psParent};
            for(;;) {
                DWORD elapsed=GetTickCount()-bridge->started,selected;
                if(elapsed>=P2PVS_TIMEOUT_MS||!P2PSCurrent(bridge))break;
                DWORD remaining=P2PVS_TIMEOUT_MS-elapsed;
                selected=WaitForMultipleObjects(2,waits,FALSE,remaining<50?remaining:50);
                if(selected==WAIT_OBJECT_0) {
                    done=GetOverlappedResult(bridge->pipe,&operation,&count,FALSE);break;
                }
                if(selected!=WAIT_TIMEOUT)break;
            }
            if(!done) {
                CancelIoEx(bridge->pipe,&operation);
                GetOverlappedResult(bridge->pipe,&operation,&count,TRUE);
            }
        }
        CloseHandle(operation.hEvent);
        if(!done||!count||count>size||!P2PSCurrent(bridge))return FALSE;
        cursor+=count;size-=count;
    }
    return P2PSCurrent(bridge);
}
/* Exactly one worker owns close/reclamation. The static context is never
 * reclaimed; the fiber reads packet fields ONLY after winning FETCHED->OWNED.
 * DONE is its final packet use and is published with InterlockedExchange. */
static void P2PSClose(P2PStartupBridge *bridge,BOOL abort)
{
    LONG phase;
    if(abort)InterlockedExchange(&bridge->aborted,1);
    phase=P2PSCancelOrObserve(&bridge->phase,P2PSAtomicCAS);
    if(bridge->pipe!=INVALID_HANDLE_VALUE)CloseHandle(bridge->pipe);
    bridge->pipe=INVALID_HANDLE_VALUE;
    /* The fiber owns a claimed packet until it publishes DONE. On timeout or
     * fault retain this single bounded inventory with the spent host; do not
     * guess that a cancelled worker means native copying has completed. */
    if(P2PSWorkerMayReclaim(phase)&&bridge->packet) {
        P2PVerifiedScriptsFree(bridge->packet,bridge->packet_size);
        bridge->packet=NULL;bridge->packet_size=0;
    }
}
static BOOL P2PSFetch(const char *map,const char *gametype,const char *category,
    unsigned long attempt,P2PStartupBridge *bridge,char *error,unsigned error_size)
{
    unsigned char nonce[32],request[P2P_STARTUP_MAX_REQUEST];
    char name[128];DWORD name_size,size;size_t request_size=0;
    const char *failure="Local Play startup could not be verified.";
    /* A zero-initialized static context may retry only a failed fetch that was
     * never published. Successful fetch permanently reserves it: no later
     * attempt can erase a retained inventory underneath a spent fiber. */
    if(!bridge||!P2PSReserveContext(&bridge->fetch_claimed,P2PSAtomicCAS)) {
        if(error&&error_size)lstrcpynA(error,failure,error_size);
        return FALSE;
    }
    bridge->pipe=INVALID_HANDLE_VALUE;bridge->packet=NULL;bridge->packet_size=0;
    bridge->expires_ms=0;bridge->scope_started=bridge->scope_lifetime=0;
    ZeroMemory(&bridge->view,sizeof(bridge->view));
    bridge->phase=0;bridge->aborted=0;bridge->scripts_result=bridge->principals_result=0;
    bridge->attempt=attempt;bridge->started=GetTickCount();
    if(P2PStartupProtocolMode()!=1||!P2PSCaptureParent()||!P2PSCurrent(bridge)||
        !P2PSNonce(nonce)||!P2PStartupMakeRequest(map,gametype,category,nonce,request,sizeof(request),&request_size))goto failed;
    name_size=GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE",name,sizeof(name));
    if(!name_size||name_size>=sizeof(name)||!P2PVSPipeName(name))goto failed;
    for(;;) {
        if(!P2PSCurrent(bridge))goto failed;
        if(WaitNamedPipeA(name,50))break;
        DWORD code=GetLastError();
        if(code!=ERROR_SEM_TIMEOUT&&code!=ERROR_PIPE_BUSY)goto failed;
    }
    bridge->pipe=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,NULL);
    if(bridge->pipe==INVALID_HANDLE_VALUE||!P2PSCurrent(bridge))goto failed;
    size=(DWORD)request_size;
    if(!P2PSTransfer(bridge,&size,4,TRUE)||!P2PSTransfer(bridge,request,size,TRUE)||
        !P2PSTransfer(bridge,&size,4,FALSE)||
        size<152+48+112||size>P2P_STARTUP_MAX_ENVELOPE)goto failed;
    bridge->packet=HeapAlloc(GetProcessHeap(),0,size);bridge->packet_size=size;
    if(!bridge->packet||!P2PSTransfer(bridge,bridge->packet,size,FALSE)||
        !P2PStartupParseEnvelope(request,request_size,bridge->packet,size,P2PSSha256,&bridge->view)||
        !P2PSCurrent(bridge)||!P2PACaptureBootstrap(&bridge->view,&bridge->active_bootstrap))goto failed;
    {
        ULONGLONG wall,expires=(ULONGLONG)p2ps_le32(bridge->view.principals+56)|
            (ULONGLONG)p2ps_le32(bridge->view.principals+60)<<32;
        if(!P2PSWallMillis(&wall)||expires<=wall)goto failed;
        bridge->expires_ms=expires;bridge->scope_started=GetTickCount();
        bridge->scope_lifetime=expires-wall>P2PVS_TIMEOUT_MS?P2PVS_TIMEOUT_MS:(DWORD)(expires-wall);
    }
    if(!P2PSCurrent(bridge))goto failed;
    InterlockedExchange(&bridge->phase,P2PS_FETCHED);
    SecureZeroMemory(request,sizeof(request));SecureZeroMemory(nonce,sizeof(nonce));
    return TRUE;
failed:
    P2PSClose(bridge,TRUE);SecureZeroMemory(request,sizeof(request));SecureZeroMemory(nonce,sizeof(nonce));
    InterlockedExchange(&bridge->fetch_claimed,0); /* No FETCHED publication occurred. */
    if(error&&error_size){lstrcpynA(error,failure,error_size);}
    return FALSE;
}
/* Worker only, after the game fiber's actual installers finish. The fixed ACK
 * precedes close/EOF; the SDK must validate both before enabling dispatch. */
static BOOL P2PSAcknowledge(P2PStartupBridge *bridge)
{
    unsigned char ack[P2P_STARTUP_ACK_SIZE];BOOL okay=FALSE;
    if(InterlockedCompareExchange(&bridge->phase,0,0)==P2PS_FIBER_DONE&&P2PSCurrent(bridge)&&
        P2PStartupMakeAck(&bridge->view,bridge->scripts_result,bridge->principals_result,ack))
        okay=P2PSTransfer(bridge,ack,sizeof(ack),TRUE);
    SecureZeroMemory(ack,sizeof(ack));
    P2PSClose(bridge,!okay||bridge->scripts_result!=1||bridge->principals_result!=1);
    return okay;
}
#else
static int P2PStartupProtocolMode(void) { return 0; }
#endif

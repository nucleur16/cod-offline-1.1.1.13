/* LOCAL_TESTING only, included by p2p_inproc.c. No worker native installer. */
#include "p2p_active_wire.c"
enum { P2PA_BOOTSTRAP, P2PA_PROMOTING, P2PA_ACTIVE, P2PA_DEAD };
static volatile LONG g_p2paMode,g_p2paStarted,g_p2paConnected,g_p2paAbort;
static SRWLOCK g_p2paLock=SRWLOCK_INIT;
static struct {
    HANDLE pipe;
    p2pa_bootstrap_t boot;
    p2pa_policy_t policy;
    unsigned char hello[P2PA_HELLO_SIZE],packet[P2PA_MAX_CONTROL];
    p2pa_control_t command;
    p2pa_query_t query;
    unsigned request_kind; /* 0 control, 1 current-scope query */
    uint64_t native_deadline,native_observed;
    volatile LONG phase; /* 0worker,1staged,2fiber-owned,3done */
    int result;
} g_p2pa;
static BOOL PiActiveClock(uint64_t *wall,uint64_t *mono)
{
    ULONGLONG w;LARGE_INTEGER tick,freq;uint64_t seconds,remainder;
    if(!P2PSWallMillis(&w)||!QueryPerformanceCounter(&tick)||!QueryPerformanceFrequency(&freq)||
       tick.QuadPart<0||freq.QuadPart<=0)return FALSE;
    seconds=(uint64_t)tick.QuadPart/(uint64_t)freq.QuadPart;
    remainder=(uint64_t)tick.QuadPart%(uint64_t)freq.QuadPart;
    if(seconds>UINT64_MAX/1000||remainder>UINT64_MAX/1000)return FALSE;
    *wall=w;*mono=seconds*1000+remainder*1000/(uint64_t)freq.QuadPart;return TRUE;
}
static void PiActiveDeny(int revoke)
{
    if(revoke)InterlockedCompareExchange(&g_p2paAbort,2,0);
    else InterlockedExchange(&g_p2paAbort,1);
    InterlockedExchange(&g_p2paMode,P2PA_DEAD);
    if(g_piPrincipalQuarantine)g_piPrincipalQuarantine();
    InterlockedExchange(&g_piPrincipalFresh,0);InterlockedExchange(&g_piQuit,1);
    InterlockedExchange(&g_piMatchFresh,0);
}
static void PiActiveBootstrapExpired(void)
{
    LONG mode=InterlockedCompareExchange(&g_p2paMode,0,0);
    while(mode==P2PA_BOOTSTRAP||mode==P2PA_PROMOTING) {
        LONG actual=InterlockedCompareExchange(&g_p2paMode,P2PA_DEAD,mode);
        if(actual==mode){PiActiveDeny(0);return;}mode=actual;
    }
}
static BOOL PiActivePeer(void)
{
    typedef BOOL (WINAPI *PidFn)(HANDLE,PULONG);
    PidFn pid=(PidFn)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetNamedPipeServerProcessId");
    ULONG actual=0;
    return P2PSParentAlive()&&g_p2pa.pipe!=INVALID_HANDLE_VALUE&&pid&&pid(g_p2pa.pipe,&actual)&&actual==g_p2psParentPid;
}
static BOOL PiActivePermission(void)
{
    LONG mode=InterlockedCompareExchange(&g_p2paMode,0,0);uint64_t wall,mono;BOOL okay;
    if(mode==P2PA_DEAD)return FALSE;
    if(mode!=P2PA_ACTIVE) {
        if(P2PSCurrent(&g_piStartup))return TRUE;
        PiActiveBootstrapExpired();
        return InterlockedCompareExchange(&g_p2paMode,0,0)==P2PA_ACTIVE ? PiActivePermission():FALSE;
    }
    okay=P2PStartupAttemptCurrent(g_piStartup.attempt)&&P2PSParentAlive()&&
        InterlockedCompareExchange(&g_p2paConnected,0,0);
    /* Sample under the same lock as high-water mutation: an older sample
     * waiting behind a later observer must not look like clock rollback. */
    if(okay){AcquireSRWLockExclusive(&g_p2paLock);okay=PiActiveClock(&wall,&mono)&&P2PAObserve(&g_p2pa.policy,wall,mono);ReleaseSRWLockExclusive(&g_p2paLock);}
    if(!okay)PiActiveDeny(0);
    return okay;
}
static BOOL PiActiveIoCurrent(int revoke_ack,DWORD started,DWORD budget)
{
    LONG abort=InterlockedCompareExchange(&g_p2paAbort,0,0);
    if(GetTickCount()-started>=budget||!PiActivePeer()) {
        PiActiveDeny(0); /* Deny before a potentially blocking kernel IO drain. */
        return FALSE;
    }
    return ((!abort&&PiActivePermission())||(revoke_ack&&abort==2));
}
static BOOL PiActiveTransfer(void *buffer,DWORD size,BOOL write,int revoke_ack,DWORD started,DWORD budget)
{
    BYTE *cursor=buffer;
    while(size) {
        OVERLAPPED operation;DWORD count=0;BOOL done;
        if(!PiActiveIoCurrent(revoke_ack,started,budget))return FALSE;
        ZeroMemory(&operation,sizeof(operation));operation.hEvent=CreateEventA(NULL,TRUE,FALSE,NULL);
        if(!operation.hEvent)return FALSE;
        done=write?WriteFile(g_p2pa.pipe,cursor,size,&count,&operation):ReadFile(g_p2pa.pipe,cursor,size,&count,&operation);
        if(!done&&GetLastError()==ERROR_IO_PENDING) {
            HANDLE waits[2]={operation.hEvent,g_p2psParent};
            while(PiActiveIoCurrent(revoke_ack,started,budget)) {
                DWORD selected=WaitForMultipleObjects(2,waits,FALSE,50);
                if(selected==WAIT_OBJECT_0){done=GetOverlappedResult(g_p2pa.pipe,&operation,&count,FALSE);break;}
                if(selected!=WAIT_TIMEOUT)break;
            }
            if(!done){PiActiveDeny(0);CancelIoEx(g_p2pa.pipe,&operation);GetOverlappedResult(g_p2pa.pipe,&operation,&count,TRUE);}
        }
        if(!done||!count||count>size)PiActiveDeny(0);
        CloseHandle(operation.hEvent);
        if(!done||!count||count>size||!PiActiveIoCurrent(revoke_ack,started,budget))return FALSE;
        cursor+=count;size-=count;
    }
    return TRUE;
}
/* Only the top of PiServerFiber calls this. Yielded Init/Frame cannot reach it. */
static void PiActiveBetweenFrames(void)
{
    uint64_t wall,mono,offered_deadline=0;int result=0;BOOL first;
    if(InterlockedCompareExchange(&g_p2pa.phase,2,1)!=1)return;
    if(g_p2pa.request_kind==1) {
        unsigned char observation[16]={0};
        g_p2pa.native_deadline=g_p2pa.native_observed=0;
        if(PiActivePermission()&&g_piObservePrincipals&&
           g_piObservePrincipals(g_p2pa.query.principals,g_p2pa.query.length,observation,sizeof(observation))==1&&
           PiActivePermission()) {
            uint64_t deadline=p2pa_u64(observation),observed=p2pa_u64(observation+8);
            if(deadline>observed){g_p2pa.native_deadline=deadline;g_p2pa.native_observed=observed;result=1;}
        }
        SecureZeroMemory(observation,sizeof(observation));goto done;
    }
    if(g_p2pa.request_kind!=0)goto done;
    if(g_p2pa.command.operation==P2PA_REVOKE) {
        if(g_piPrincipalQuarantine)g_piPrincipalQuarantine();
        result=g_piPrincipalCurrent&&g_piPrincipalCurrent()==0;
        AcquireSRWLockExclusive(&g_p2paLock);g_p2pa.policy.sequence=g_p2pa.command.sequence;g_p2pa.policy.dead=1;ReleaseSRWLockExclusive(&g_p2paLock);
    } else {
        first=InterlockedCompareExchange(&g_p2paMode,0,0)==P2PA_BOOTSTRAP;
        if(first&&InterlockedCompareExchange(&g_p2paMode,P2PA_PROMOTING,P2PA_BOOTSTRAP)!=P2PA_BOOTSTRAP)goto done;
        if(!PiActivePermission()||!g_piPrincipalCurrent||g_piPrincipalCurrent()!=1)goto done;
        AcquireSRWLockExclusive(&g_p2paLock);result=PiActiveClock(&wall,&mono)&&P2PAPrepare(&g_p2pa.policy,&g_p2pa.command,wall,mono);
        if(result)offered_deadline=g_p2pa.policy.offered_deadline;
        ReleaseSRWLockExclusive(&g_p2paLock);
        if(result&&g_piActivePrincipals)result=g_piActivePrincipals(g_p2pa.command.principals,g_p2pa.command.length,offered_deadline)==1;
        else result=0;
        if(!result||!PiActivePermission()||g_piPrincipalCurrent()!=1){result=0;goto done;}
        AcquireSRWLockExclusive(&g_p2paLock);result=PiActiveClock(&wall,&mono)&&P2PACommit(&g_p2pa.policy,&g_p2pa.command,result,wall,mono);ReleaseSRWLockExclusive(&g_p2paLock);
        /* Commit clock observation can block. The original bootstrap budget
         * must still be live after it, before the ACTIVE state replaces that
         * gate and makes obsolete startup timer observations harmless. */
        if(result&&first&&!PiActivePermission())result=0;
        if(result&&first&&InterlockedCompareExchange(&g_p2paMode,P2PA_ACTIVE,P2PA_PROMOTING)!=P2PA_PROMOTING)result=0;
        if(result&&InterlockedCompareExchange(&g_p2paMode,0,0)!=P2PA_ACTIVE)result=0;
        if(result)InterlockedExchange(&g_piPrincipalFresh,1);
    }
done:
    if(!result)PiActiveDeny(0);
    g_p2pa.result=result;InterlockedExchange(&g_p2pa.phase,3); /* final packet read */
}
/* Worker-only mailbox producer, shared by query and control framing. Never
 * replace bytes while the owner fiber is still reading an earlier request. */
static BOOL PiActiveStagePacket(DWORD size)
{
    uint64_t wall,mono;BOOL okay;
    if(InterlockedCompareExchange(&g_p2pa.phase,0,0)!=0||
       size<P2PA_QUERY_HEADER+112||size>P2PA_MAX_CONTROL||!PiActivePermission())return FALSE;
    if(!memcmp(g_p2pa.packet,"CPAQv1\0\0",8)) {
        if(!P2PAParseQuery(&g_p2pa.boot,g_p2pa.hello,g_p2pa.packet,size,P2PSSha256,&g_p2pa.query))return FALSE;
        g_p2pa.request_kind=1;
    } else {
        if(!P2PAParseControl(&g_p2pa.boot,g_p2pa.hello,g_p2pa.packet,size,P2PSSha256,&g_p2pa.command))return FALSE;
        AcquireSRWLockExclusive(&g_p2paLock);okay=PiActiveClock(&wall,&mono)&&P2PAPrepare(&g_p2pa.policy,&g_p2pa.command,wall,mono);ReleaseSRWLockExclusive(&g_p2paLock);
        if(!okay)return FALSE;
        g_p2pa.request_kind=0;
    }
    if(!PiActivePermission())return FALSE;
    if(g_p2pa.request_kind==0&&g_p2pa.command.operation==P2PA_REVOKE)PiActiveDeny(1);
    InterlockedExchange(&g_p2pa.phase,1);return TRUE;
}
static BOOL PiActiveMakeReply(unsigned char out[P2PA_ACK_SIZE])
{
    uint64_t native_seq;
    if(InterlockedCompareExchange(&g_p2pa.phase,0,0)!=3)return FALSE;
    if(g_p2pa.request_kind==1)
        return P2PAMakeQueryReply(&g_p2pa.query,g_p2pa.result,g_p2pa.native_deadline,g_p2pa.native_observed,out);
    if(g_p2pa.request_kind!=0)return FALSE;
    AcquireSRWLockExclusive(&g_p2paLock);native_seq=g_p2pa.policy.native_sequence;ReleaseSRWLockExclusive(&g_p2paLock);
    return P2PAMakeAck(&g_p2pa.command,g_p2pa.hello+16,g_p2pa.result,native_seq,out);
}
static DWORD WINAPI PiActiveWorker(void *unused)
{
    char name[128];DWORD n,size,started;unsigned char ack[P2PA_ACK_SIZE];int revoke;
    (void)unused;g_p2pa.pipe=INVALID_HANDLE_VALUE;
    n=GetEnvironmentVariableA("CODOL_P2P_ACTIVE_PIPE",name,sizeof(name));
    if(!n||n>=sizeof(name))goto end;
    {static const char prefix[]="\\\\.\\pipe\\CoDOLActiveControl-";size_t i,len=sizeof(prefix)-1;
     if(strlen(name)!=len+32||strncmp(name,prefix,len))goto end;
     for(i=len;i<len+32;i++)if(!((name[i]>='0'&&name[i]<='9')||(name[i]>='a'&&name[i]<='f')))goto end;}
    started=GetTickCount();
    while(!WaitNamedPipeA(name,50))if(!PiActivePermission()||GetTickCount()-started>=5000)goto end;
    g_p2pa.pipe=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,NULL);
    if(!PiActivePeer()||!P2PSNonce(ack)||!P2PAMakeHello(&g_p2pa.boot,ack,g_p2pa.hello))goto end;
    InterlockedExchange(&g_p2paConnected,1);size=P2PA_HELLO_SIZE;
    if(!PiActiveTransfer(&size,4,TRUE,0,started,5000)||!PiActiveTransfer(g_p2pa.hello,size,TRUE,0,started,5000))goto end;
    for(;;) {
        started=GetTickCount();
        /* Idle read remains bounded by the actual current lease and checks
         * parent/pipe/clock every50ms, even when the SDK has no queued update. */
        if(!PiActiveTransfer(&size,4,FALSE,0,started,30000)||size<P2PA_QUERY_HEADER+112||size>P2PA_MAX_CONTROL)break;
        started=GetTickCount();
        if(!PiActiveTransfer(g_p2pa.packet,size,FALSE,0,started,5000)||!PiActiveStagePacket(size))break;
        revoke=g_p2pa.request_kind==0&&g_p2pa.command.operation==P2PA_REVOKE;
        while(InterlockedCompareExchange(&g_p2pa.phase,0,0)!=3&&PiActiveIoCurrent(revoke,started,5000))Sleep(5);
        if(InterlockedCompareExchange(&g_p2pa.phase,0,0)!=3)break;
        if(!PiActiveMakeReply(ack))break;
        if(!PiActiveTransfer(ack,sizeof(ack),TRUE,revoke,started,5000)||!g_p2pa.result)break;
        if(revoke)break;
        InterlockedExchange(&g_p2pa.phase,0);
    }
end:
    PiActiveDeny(0);InterlockedExchange(&g_p2paConnected,0);
    if(g_p2pa.pipe!=INVALID_HANDLE_VALUE)CloseHandle(g_p2pa.pipe);
    g_p2pa.pipe=INVALID_HANDLE_VALUE;SecureZeroMemory(ack,sizeof(ack));
    /* Static packet/context are never reclaimed under a fiber still owning it. */
    return 0;
}
static BOOL PiActiveStart(void)
{
    char selected[2];HANDLE worker;
    if(!GetEnvironmentVariableA("CODOL_P2P_ACTIVE_PIPE",selected,sizeof(selected)))return TRUE;
    if(!g_piActivePrincipals||!g_piObservePrincipals||InterlockedCompareExchange(&g_p2paStarted,1,0)||!P2PSCurrent(&g_piStartup))return FALSE;
    g_p2pa.boot=g_piStartup.active_bootstrap;P2PAInitPolicy(&g_p2pa.policy,&g_p2pa.boot);
    if(g_p2pa.boot.valid!=1)return FALSE;
    worker=CreateThread(NULL,0,PiActiveWorker,NULL,0,NULL);
    if(!worker)return FALSE;
    CloseHandle(worker);return TRUE;
}

#include "p2p_win_pipe.h"
#include <wchar.h>
#include <string.h>
#include <stddef.h>

enum { P2PT_PIPE,P2PT_EVENT,P2PT_STOP,P2PT_PEER,P2PT_THREAD,P2PT_TOKEN,P2PT_PENDING_PEER,P2PT_HANDLES };
struct p2pt_owner {
    HANDLE handles[P2PT_HANDLES];
    unsigned close_attempted;
    DWORD first,pid;
    FILETIME birth;
    wchar_t image[32768];
    OVERLAPPED operation;
    volatile LONG pending,held,closed,io_claimed,start_in_progress;
    unsigned free_attempted;
    uint64_t cleanup_deadline;
    unsigned char buffer[65536];
};
static PVOID volatile enrolled;
static volatile LONG blocked;
static SRWLOCK lifecycle=SRWLOCK_INIT;
static void first(p2pt_owner *o,DWORD e) {
    InterlockedCompareExchange((volatile LONG *)&o->first,(LONG)(e?e:ERROR_INVALID_DATA),0);
}
static void hold(p2pt_owner *o,DWORD e) {
    first(o,e);InterlockedExchange(&o->held,1);InterlockedExchange(&blocked,1);
}
DWORD P2PT_FirstError(const p2pt_owner *o){return o?(DWORD)InterlockedCompareExchange((volatile LONG *)&o->first,0,0):ERROR_INVALID_PARAMETER;}
int P2PT_RetirementBlocked(void){return InterlockedCompareExchange(&blocked,0,0)!=0;}
uint64_t P2PT_Now(void) {
    LARGE_INTEGER tick,freq;uint64_t seconds,remainder;
    if(!QueryPerformanceCounter(&tick)||!QueryPerformanceFrequency(&freq)||tick.QuadPart<0||freq.QuadPart<=0)return 0;
    seconds=(uint64_t)tick.QuadPart/(uint64_t)freq.QuadPart;
    remainder=(uint64_t)tick.QuadPart%(uint64_t)freq.QuadPart;
    if(seconds>UINT64_MAX/1000||remainder>UINT64_MAX/1000)return 0;
    return seconds*1000+remainder*1000/(uint64_t)freq.QuadPart;
}
static DWORD remaining(uint64_t deadline,DWORD ceiling) {
    uint64_t now=P2PT_Now(),n;
    if(!now||now>=deadline)return 0;n=deadline-now;return n<ceiling?(DWORD)n:ceiling;
}
static int close_one(p2pt_owner *o,unsigned which) {
    HANDLE h=o->handles[which];unsigned bit=1u<<which;
    if(!h||h==INVALID_HANDLE_VALUE)return 1;
    if(o->close_attempted&bit)return 0;
    o->close_attempted|=bit;
    if(!CloseHandle(h)){hold(o,GetLastError());return 0;}
    o->handles[which]=NULL;return 1;
}
static int identity(p2pt_owner *o) {
    FILETIME born,ended,kernel,user;wchar_t path[32768];DWORD count=32768;
    if(!o||!o->handles[P2PT_PEER]||WaitForSingleObject(o->handles[P2PT_PEER],0)!=WAIT_TIMEOUT||
       GetProcessId(o->handles[P2PT_PEER])!=o->pid||
       !GetProcessTimes(o->handles[P2PT_PEER],&born,&ended,&kernel,&user)||
       born.dwLowDateTime!=o->birth.dwLowDateTime||born.dwHighDateTime!=o->birth.dwHighDateTime||
       !QueryFullProcessImageNameW(o->handles[P2PT_PEER],0,path,&count)||count>=32768)return 0;
    path[count]=0;return _wcsicmp(path,o->image)==0;
}
int P2PT_Current(p2pt_owner *o) {
    if(!o||P2PT_FirstError(o)||o->held||o->closed||!o->handles[P2PT_STOP]||
       WaitForSingleObject(o->handles[P2PT_STOP],0)!=WAIT_TIMEOUT||!identity(o))return 0;
    return 1;
}
HANDLE P2PT_CancellationEvent(p2pt_owner *o){return o?o->handles[P2PT_STOP]:NULL;}
int P2PT_PeerIdentity(p2pt_owner *o,DWORD *pid,uint64_t *birth) {
    if(!pid||!birth||!P2PT_Current(o))return 0;
    *pid=o->pid;*birth=(uint64_t)o->birth.dwLowDateTime|(uint64_t)o->birth.dwHighDateTime<<32;return 1;
}
int P2PT_BindOriginalPeer(p2pt_owner *o,HANDLE original,DWORD pid,const wchar_t *image) {
    FILETIME born,ended,kernel,user;wchar_t path[32768];DWORD count=32768;int good=0;
    if(!o||!original||!pid||!image||!wcslen(image)||wcslen(image)>=32768)return 0;
    AcquireSRWLockExclusive(&lifecycle);
    if(!P2PT_Current(o)||o->handles[P2PT_THREAD]||o->start_in_progress||o->pending||o->io_claimed||
       o->handles[P2PT_PENDING_PEER])goto done;
    if(!DuplicateHandle(GetCurrentProcess(),original,GetCurrentProcess(),&o->handles[P2PT_PENDING_PEER],
        SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,0))goto done;
    if(GetProcessId(o->handles[P2PT_PENDING_PEER])!=pid||WaitForSingleObject(o->handles[P2PT_PENDING_PEER],0)!=WAIT_TIMEOUT||
       !GetProcessTimes(o->handles[P2PT_PENDING_PEER],&born,&ended,&kernel,&user)||
       !QueryFullProcessImageNameW(o->handles[P2PT_PENDING_PEER],0,path,&count)||count>=32768)goto done;
    path[count]=0;if(_wcsicmp(path,image))goto done;
    if(!close_one(o,P2PT_PEER))goto done;
    o->handles[P2PT_PEER]=o->handles[P2PT_PENDING_PEER];o->handles[P2PT_PENDING_PEER]=NULL;
    o->close_attempted&=~(1u<<P2PT_PEER);o->pid=pid;o->birth=born;wcscpy(o->image,image);good=1;
done:
    if(!good)first(o,GetLastError());
    ReleaseSRWLockExclusive(&lifecycle);return good;
}
void P2PT_RequestStop(p2pt_owner *o) {
    AcquireSRWLockShared(&lifecycle);
    if(o&&InterlockedCompareExchangePointer(&enrolled,NULL,NULL)==o&&
       o->handles[P2PT_STOP]&&!SetEvent(o->handles[P2PT_STOP]))hold(o,GetLastError());
    ReleaseSRWLockShared(&lifecycle);
}
p2pt_owner *P2PT_New(HANDLE original,DWORD pid,const wchar_t *image,uint64_t cleanup) {
    p2pt_owner *o;FILETIME ended,kernel,user;size_t length;
    if(!original||!pid||!image||!(length=wcslen(image))||length>=32768||
       !remaining(cleanup,1)||P2PT_RetirementBlocked()||
       InterlockedCompareExchangePointer(&enrolled,(PVOID)1,NULL)!=NULL)return NULL;
    o=(p2pt_owner *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*o));
    if(!o){InterlockedExchangePointer(&enrolled,NULL);return NULL;}
    InterlockedExchangePointer(&enrolled,o);o->pid=pid;o->cleanup_deadline=cleanup;
    memcpy(o->image,image,(length+1)*sizeof(wchar_t));
    if(!DuplicateHandle(GetCurrentProcess(),original,GetCurrentProcess(),&o->handles[P2PT_PEER],
        SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,0))goto fail;
    if(GetProcessId(o->handles[P2PT_PEER])!=pid||
       !GetProcessTimes(o->handles[P2PT_PEER],&o->birth,&ended,&kernel,&user))goto fail;
    o->handles[P2PT_STOP]=CreateEventW(NULL,TRUE,FALSE,NULL);if(!o->handles[P2PT_STOP])goto fail;
    o->handles[P2PT_EVENT]=CreateEventW(NULL,TRUE,FALSE,NULL);if(!o->handles[P2PT_EVENT])goto fail;
    if(!P2PT_Current(o))goto fail;
    return o;
fail:
    first(o,GetLastError());
    /* The exact owner remains reachable even when initialization fails. */
    P2PT_JoinClose(o,cleanup);return o;
}
static int pipe_name(const wchar_t *s) {
    static const wchar_t prefix[]=L"\\\\.\\pipe\\CoDOLProductionP2P-";size_t i,n=(sizeof(prefix)/sizeof(wchar_t))-1;
    if(!s||wcslen(s)!=n+64||wcsncmp(s,prefix,n))return 0;
    for(i=n;i<n+64;i++)if(!((s[i]>=L'0'&&s[i]<=L'9')||(s[i]>=L'a'&&s[i]<=L'f')))return 0;
    return 1;
}
int P2PT_Server(p2pt_owner *o,const wchar_t *name) {
    /* Current token SID and SYSTEM only; stack ACL is copied by the API. */
    union {void *alignment;unsigned char bytes[4096];} token;
    union {DWORD alignment;unsigned char bytes[512];} acl_store;
    union {void *alignment;unsigned char bytes[SECURITY_MAX_SID_SIZE];} system_sid;
    DWORD needed=0,system_size=sizeof(system_sid.bytes);TOKEN_USER *user=(TOKEN_USER *)token.bytes;
    SECURITY_DESCRIPTOR descriptor;SECURITY_ATTRIBUTES security;ACL *acl=(ACL *)acl_store.bytes;
    if(!P2PT_Current(o)||o->handles[P2PT_PIPE]||!pipe_name(name))return 0;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&o->handles[P2PT_TOKEN])||
       !GetTokenInformation(o->handles[P2PT_TOKEN],TokenUser,token.bytes,sizeof token.bytes,&needed)||
       !IsValidSid(user->User.Sid)||!CreateWellKnownSid(WinLocalSystemSid,NULL,system_sid.bytes,&system_size)||
       !InitializeAcl(acl,sizeof acl_store.bytes,ACL_REVISION)||
       !AddAccessAllowedAce(acl,ACL_REVISION,GENERIC_ALL,user->User.Sid)||
       !AddAccessAllowedAce(acl,ACL_REVISION,GENERIC_ALL,system_sid.bytes)||
       !InitializeSecurityDescriptor(&descriptor,SECURITY_DESCRIPTOR_REVISION)||
       !SetSecurityDescriptorDacl(&descriptor,TRUE,acl,FALSE))goto fail;
    if(!close_one(o,P2PT_TOKEN))return 0;
    security.nLength=sizeof security;security.lpSecurityDescriptor=&descriptor;security.bInheritHandle=FALSE;
    o->handles[P2PT_PIPE]=CreateNamedPipeW(name,PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,&security);
    if(o->handles[P2PT_PIPE]==INVALID_HANDLE_VALUE){o->handles[P2PT_PIPE]=NULL;goto fail;}
    return 1;
fail:first(o,GetLastError());return 0;
}
static int pipe_peer(p2pt_owner *o,int server) {
    typedef BOOL (WINAPI *GetPid)(HANDLE,PULONG);ULONG pid=0;
    GetPid get=(GetPid)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
        server?"GetNamedPipeClientProcessId":"GetNamedPipeServerProcessId");
    return get&&P2PT_Current(o)&&get(o->handles[P2PT_PIPE],&pid)&&pid==o->pid;
}
static int wait_operation(p2pt_owner *o,BOOL immediate,DWORD *done,uint64_t deadline) {
    DWORD error=immediate?ERROR_SUCCESS:GetLastError(),failure=ERROR_TIMEOUT;BOOL okay=immediate;
    if(!immediate&&error!=ERROR_IO_PENDING){first(o,error);return 0;}
    if(!immediate) {
        HANDLE waits[3]={o->handles[P2PT_STOP],o->handles[P2PT_PEER],o->handles[P2PT_EVENT]};
        InterlockedExchange(&o->pending,1);
        while(P2PT_Current(o)&&remaining(deadline,1)) {
            DWORD ms=remaining(deadline,50),selected;
            if(!ms)break;selected=WaitForMultipleObjects(3,waits,FALSE,ms);
            if(selected==WAIT_OBJECT_0+2) {
                /* Cancellation/identity take precedence over completion. */
                if(P2PT_Current(o)&&remaining(deadline,1)) {
                    okay=GetOverlappedResult(o->handles[P2PT_PIPE],&o->operation,done,FALSE);
                    if(!okay)failure=GetLastError();
                } else failure=ERROR_OPERATION_ABORTED;
                break;
            }
            if(selected!=WAIT_TIMEOUT){failure=selected==WAIT_FAILED?GetLastError():ERROR_OPERATION_ABORTED;break;}
        }
        if(!okay) {
            uint64_t now=P2PT_Now(),drain_deadline;
            first(o,failure);
            if(!CancelIoEx(o->handles[P2PT_PIPE],&o->operation)&&GetLastError()!=ERROR_NOT_FOUND)first(o,GetLastError());
            if(!now||now>UINT64_MAX-2000){hold(o,ERROR_TIMEOUT);return 0;}
            drain_deadline=now+2000;
            if(drain_deadline>o->cleanup_deadline)drain_deadline=o->cleanup_deadline;
            /* Kernel owns operation/event/buffer until THIS event is terminal.
             * A bounded unknown drain retains them; it never frees stack IO. */
            for(;;) {
                DWORD ms=remaining(drain_deadline,50),selected;
                if(!ms){hold(o,ERROR_TIMEOUT);return 0;}
                selected=WaitForSingleObject(o->handles[P2PT_EVENT],ms);
                if(selected==WAIT_OBJECT_0){
                    DWORD ignored=0,terminal_error;
                    if(GetOverlappedResult(o->handles[P2PT_PIPE],&o->operation,&ignored,FALSE))break;
                    terminal_error=GetLastError();
                    if(terminal_error==ERROR_OPERATION_ABORTED||terminal_error==ERROR_BROKEN_PIPE||
                       terminal_error==ERROR_NO_DATA||terminal_error==ERROR_PIPE_NOT_CONNECTED||terminal_error==ERROR_HANDLE_EOF)break;
                    hold(o,terminal_error);return 0;
                }
                if(selected!=WAIT_TIMEOUT){hold(o,GetLastError());return 0;}
            }
        }
        InterlockedExchange(&o->pending,0);
    }
    if(!okay||!P2PT_Current(o)||!remaining(deadline,1)){first(o,ERROR_OPERATION_ABORTED);return 0;}
    return 1;
}
int P2PT_Connect(p2pt_owner *o,uint64_t deadline) {
    BOOL result;DWORD unused=0,error;
    if(!P2PT_Current(o)||!o->handles[P2PT_PIPE]||!remaining(deadline,1))return 0;
    ZeroMemory(&o->operation,sizeof o->operation);o->operation.hEvent=o->handles[P2PT_EVENT];
    if(!ResetEvent(o->handles[P2PT_EVENT])){first(o,GetLastError());return 0;}
    result=ConnectNamedPipe(o->handles[P2PT_PIPE],&o->operation);error=result?0:GetLastError();
    if(!result&&error==ERROR_PIPE_CONNECTED)result=TRUE;
    else SetLastError(error);
    return wait_operation(o,result,&unused,deadline)&&pipe_peer(o,1);
}
int P2PT_Client(p2pt_owner *o,const wchar_t *name,uint64_t deadline) {
    if(!P2PT_Current(o)||o->handles[P2PT_PIPE]||!pipe_name(name))return 0;
    for(;;) {
        DWORD ms=remaining(deadline,50),e;if(!ms||!P2PT_Current(o))return 0;
        if(WaitNamedPipeW(name,ms))break;
        e=GetLastError();if(e!=ERROR_PIPE_BUSY&&e!=ERROR_SEM_TIMEOUT){first(o,e);return 0;}
    }
    o->handles[P2PT_PIPE]=CreateFileW(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,NULL);
    if(o->handles[P2PT_PIPE]==INVALID_HANDLE_VALUE){o->handles[P2PT_PIPE]=NULL;first(o,GetLastError());return 0;}
    return remaining(deadline,1)&&pipe_peer(o,0);
}
static int transfer(p2pt_owner *o,void *data,DWORD length,uint64_t deadline,int writing) {
    unsigned char *cursor=(unsigned char *)data;int okay=0;
    if(!o||!data||!length||InterlockedCompareExchange(&o->io_claimed,1,0)!=0)return 0;
    while(length) {
        DWORD size=length<sizeof o->buffer?length:sizeof o->buffer,count=0;BOOL immediate;
        if(!P2PT_Current(o)||!remaining(deadline,1)||o->pending)goto done;
        ZeroMemory(&o->operation,sizeof o->operation);o->operation.hEvent=o->handles[P2PT_EVENT];
        if(!ResetEvent(o->handles[P2PT_EVENT])){first(o,GetLastError());goto done;}
        if(writing)memcpy(o->buffer,cursor,size);
        immediate=writing?WriteFile(o->handles[P2PT_PIPE],o->buffer,size,&count,&o->operation):
                           ReadFile(o->handles[P2PT_PIPE],o->buffer,size,&count,&o->operation);
        if(!wait_operation(o,immediate,&count,deadline)||!count||count>size)goto done;
        if(!writing)memcpy(cursor,o->buffer,count);
        cursor+=count;length-=count;
    }
    okay=1;
done:
    if(!okay)first(o,ERROR_OPERATION_ABORTED);
    if(!o->pending)SecureZeroMemory(o->buffer,sizeof o->buffer);
    InterlockedExchange(&o->io_claimed,0);return okay;
}
int P2PT_Read(p2pt_owner *o,void *p,DWORD n,uint64_t d){return transfer(o,p,n,d,0);}
int P2PT_Write(p2pt_owner *o,const void *p,DWORD n,uint64_t d){return transfer(o,(void *)p,n,d,1);}
int P2PT_Start(p2pt_owner *o,p2pt_work work,void *context) {
    if(!o||InterlockedCompareExchange(&o->start_in_progress,1,0)!=0)return 0;
    if(!P2PT_Current(o)||!work||o->handles[P2PT_THREAD]){InterlockedExchange(&o->start_in_progress,0);return 0;}
    o->handles[P2PT_THREAD]=CreateThread(NULL,0,work,context,0,NULL);
    if(!o->handles[P2PT_THREAD])first(o,GetLastError());
    InterlockedExchange(&o->start_in_progress,0);
    return o->handles[P2PT_THREAD]!=NULL;
}
int P2PT_JoinClose(p2pt_owner *o,uint64_t deadline) {
    unsigned i;int okay=1;if(!o)return 1;
    P2PT_RequestStop(o);
    while(InterlockedCompareExchange(&o->start_in_progress,0,0)) {
        if(!remaining(deadline,1)){hold(o,ERROR_TIMEOUT);return 0;}
        Sleep(1);
    }
    if(o->handles[P2PT_THREAD]) {
        for(;;){DWORD ms=remaining(deadline,50),v=WaitForSingleObject(o->handles[P2PT_THREAD],0);
            if(v==WAIT_OBJECT_0)break;if(v!=WAIT_TIMEOUT||!ms){hold(o,ERROR_TIMEOUT);return 0;}
            v=WaitForSingleObject(o->handles[P2PT_THREAD],ms);if(v==WAIT_OBJECT_0)break;
            if(v!=WAIT_TIMEOUT){hold(o,GetLastError());return 0;}}
    }
    if(o->pending&&!o->io_claimed&&WaitForSingleObject(o->handles[P2PT_EVENT],0)==WAIT_OBJECT_0) {
        DWORD ignored=0,error=0;
        if(GetOverlappedResult(o->handles[P2PT_PIPE],&o->operation,&ignored,FALSE))InterlockedExchange(&o->pending,0);
        else {
            error=GetLastError();
            if(error==ERROR_OPERATION_ABORTED||error==ERROR_BROKEN_PIPE||error==ERROR_NO_DATA||
               error==ERROR_PIPE_NOT_CONNECTED||error==ERROR_HANDLE_EOF)InterlockedExchange(&o->pending,0);
        }
        if(!o->pending)SecureZeroMemory(o->buffer,sizeof o->buffer);
    }
    if(o->pending||o->io_claimed){hold(o,ERROR_IO_INCOMPLETE);return 0;}
    AcquireSRWLockExclusive(&lifecycle);
    for(i=0;i<P2PT_HANDLES;i++)if(!close_one(o,i))okay=0;
    if(okay){InterlockedExchange(&o->closed,1);InterlockedExchange(&o->held,0);}
    ReleaseSRWLockExclusive(&lifecycle);
    return okay;
}
int P2PT_Destroy(p2pt_owner **owner) {
    p2pt_owner *o;
    if(!owner||!(o=*owner))return 1;
    AcquireSRWLockExclusive(&lifecycle);
    if(!o->closed||o->held||o->pending||o->io_claimed||o->start_in_progress||o->free_attempted||
       InterlockedCompareExchangePointer(&enrolled,(PVOID)1,o)!=o){ReleaseSRWLockExclusive(&lifecycle);return 0;}
    o->free_attempted=1;
    if(!HeapFree(GetProcessHeap(),0,o)){DWORD error=GetLastError();InterlockedExchangePointer(&enrolled,o);hold(o,error);ReleaseSRWLockExclusive(&lifecycle);return 0;}
    InterlockedExchangePointer(&enrolled,NULL);*owner=NULL;ReleaseSRWLockExclusive(&lifecycle);return 1;
}

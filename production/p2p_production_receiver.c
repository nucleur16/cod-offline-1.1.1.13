/* Production original-parent receiver. The retained Launcher is the verifier;
 * trusted-byte Host APIs below are NOT cryptographic or endpoint verifiers.
 * Build this as a separate translation unit: original QA bridge is untouched. */
#include "p2p_production_receiver.h"
#include "../shared/p2p_win_pipe.h"
#include "p2p_production_wire.h"
#include "p2p_production_handoff_wire.h"
#include <wincrypt.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>

/* Reuse exact existing parsers/policy without duplicate QA linkage. */
#define P2PStartupMakeRequest P2PRWireMakeRequest
#define P2PStartupMakeRequestV2 P2PRWireMakeRequestV2
#define P2PStartupRequestProtocol P2PRWireRequestProtocol
#define P2PStartupParseEnvelope P2PRWireParseEnvelope
#define P2PStartupMakeAck P2PRWireMakeAck
#define P2PStartupMakeAckV2 P2PRWireMakeAckV2
#include "p2p_production_startup_wire.c"
#define P2PACaptureBootstrap P2PRCaptureBootstrap
#define P2PAMakeHello P2PRMakeHello
#define P2PAParseControl P2PRParseControl
#define P2PAInitPolicy P2PRInitPolicy
#define P2PAObserve P2PRObserve
#define P2PAPrepare P2PRPrepare
#define P2PACommit P2PRCommit
#define P2PAMakeAck P2PRMakeAck
#define P2PAParseQuery P2PRParseQuery
#define P2PAMakeQueryReply P2PRMakeQueryReply
#include "p2p_production_active_wire.c"
#include "p2p_production_guest.c"

enum { R_NEW,R_FETCH,R_STARTUP_STAGED,R_STARTUP_OWNED,R_STARTUP_DONE,R_CONTROL,
       R_CONTROL_STAGED,R_CONTROL_OWNED,R_CONTROL_DONE,R_DEAD,R_RETIRED };
static struct {
    volatile LONG claimed,phase,denied,held,first,stopping,revoking;
    DWORD owner_thread;
    HANDLE inherited,image_file,host_file;
    unsigned inherited_close,image_close,host_close,parent_adopted;
    HCRYPTPROV crypto;HCRYPTHASH hash;
    unsigned crypto_close,hash_close;
    p2pt_owner *io;
    p2pr_selection selected;
    wchar_t pipe[128],parent_path[32768];
    p2pp_scope scope;
    unsigned char request[P2P_STARTUP_V2_MAX_REQUEST];size_t request_size;
    unsigned char authority[P2PP_HEADER+P2PP_MAX_BODY];
    unsigned char *packet;unsigned packet_size,packet_free_attempted,full_installed;
    p2p_startup_view_t startup;
    unsigned char guest_bound[P2PR_GUEST_SIZE];volatile LONG guest_committed;
    uint64_t guest_deadline,guest_control_sequence,guest_hard_wall,guest_hard_deadline;
    unsigned char guest_active_id[16];unsigned guest_active;
    unsigned handoff_committed,release_committed;uint64_t handoff_deadline,handoff_expires,release_deadline,release_expires;
    unsigned char initial_id[16],handoff_id[32],release_id[32],handoff_ack[192],envelope_sha[32];
    unsigned char launcher_hash[32],client_hash[32];
    p2pa_bootstrap_t bootstrap;p2pa_policy_t policy;
    SRWLOCK lock;
    unsigned char hello[P2PA_HELLO_SIZE],control[P2PA_MAX_CONTROL];
    unsigned char installed[656],evidence[1536];unsigned installed_size,evidence_size;
    p2pa_control_t command;p2pa_query_t query;
    unsigned kind,request_kind,upgrade_length,valid_revocation,effective_role,connected_evidence,probe_kind;
    unsigned char socket_witness[112];
    int scripts_result,principals_result,options_result,result;
    uint64_t native_deadline,native_observed,work_deadline,cleanup_deadline,staged_deadline,last_wall,last_mono;
    unsigned clock_seen;
} receiver;
static void r_first(DWORD e){InterlockedCompareExchange(&receiver.first,(LONG)(e?e:ERROR_ACCESS_DENIED),0);}
static void r_deny(DWORD e){r_first(e);InterlockedExchange(&receiver.denied,1);}
static void r_hold(DWORD e){r_deny(e);InterlockedExchange(&receiver.held,1);}
DWORD P2PR_FirstError(void){return (DWORD)InterlockedCompareExchange(&receiver.first,0,0);}
static uint64_t r_u64(const unsigned char *p){return (uint64_t)p2ps_le32(p)|(uint64_t)p2ps_le32(p+4)<<32;}
static void r_put64(unsigned char *p,uint64_t n){unsigned i;for(i=0;i<8;i++)p[i]=(unsigned char)(n>>(i*8));}
static int r_clock(uint64_t *wall,uint64_t *mono) {
    FILETIME ft;ULARGE_INTEGER n;GetSystemTimeAsFileTime(&ft);n.LowPart=ft.dwLowDateTime;n.HighPart=ft.dwHighDateTime;
    if(n.QuadPart<116444736000000000ull)return 0;
    *wall=(n.QuadPart-116444736000000000ull)/10000;*mono=P2PT_Now();return *wall&&*mono;
}
static int r_close_handle(HANDLE *h,unsigned *attempted) {
    if(!*h||*h==INVALID_HANDLE_VALUE)return 1;if(*attempted)return 0;*attempted=1;
    if(!CloseHandle(*h)){r_hold(GetLastError());return 0;}*h=NULL;return 1;
}
static int r_hash_close(void) {
    if(!receiver.hash)return 1;if(receiver.hash_close)return 0;receiver.hash_close=1;
    if(!CryptDestroyHash(receiver.hash)){r_hold(GetLastError());return 0;}
    receiver.hash=0;receiver.hash_close=0;return 1;
}
static int r_sha(const void *data,size_t size,unsigned char out[32]) {
    DWORD n=32;int good=0;
    if(size>0xffffffffu||receiver.held||receiver.hash)return 0;
    if(!CryptCreateHash(receiver.crypto,CALG_SHA_256,0,0,&receiver.hash))goto done;
    if(!CryptHashData(receiver.hash,(const BYTE *)data,(DWORD)size,0)||
       !CryptGetHashParam(receiver.hash,HP_HASHVAL,out,&n,0)||n!=32)goto done;
    good=1;
done:
    if(!good)r_first(GetLastError());
    if(!r_hash_close())good=0;return good;
}
static int r_protected_path(const wchar_t *p) {
    static const wchar_t token[]=L"codolpresence";size_t i,j;
    for(i=0;p[i];i++){for(j=0;token[j]&&p[i+j]&&towlower(p[i+j])==token[j];j++){}if(!token[j])return 1;}return 0;
}
static int r_policy(const p2pr_release_policy *policy) {
    MEMORY_BASIC_INFORMATION m;HMODULE own=NULL;uintptr_t at=(uintptr_t)policy,end;
    if(!policy||at>UINTPTR_MAX-sizeof(*policy))return 0;end=at+sizeof(*policy);
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)(void *)P2PR_Begin,&own)||!VirtualQuery(policy,&m,sizeof m)||m.AllocationBase!=own||
        m.Type!=MEM_IMAGE||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
        !(m.Protect&(PAGE_READONLY|PAGE_EXECUTE_READ))||end>(uintptr_t)m.BaseAddress+m.RegionSize)return 0;
    return policy->format==1&&p2ps_nonzero(policy->launcher_sha256,32)&&
        p2ps_nonzero(policy->client_image_sha256,32)&&p2ps_nonzero(policy->issuer_ed25519,32)&&p2ps_nonzero(policy->host_sha256,32);
}
static int r_parent_hash(const wchar_t *path,const unsigned char expected[32],int retain) {
    BY_HANDLE_FILE_INFORMATION before,after;unsigned char buffer[16384],digest[32];
    uint64_t total=0;DWORD n=0,bytes=32;int okay=0;
    if(r_protected_path(path))return 0; /* BEFORE ANY filename operation */
    if(receiver.image_file||receiver.held)return 0;
    receiver.image_close=0; /* previous original file is conclusively closed */
    receiver.image_file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(receiver.image_file==INVALID_HANDLE_VALUE){receiver.image_file=NULL;goto done;}
    if(GetFileType(receiver.image_file)!=FILE_TYPE_DISK||!GetFileInformationByHandle(receiver.image_file,&before)||
       (before.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||before.nNumberOfLinks!=1||
       before.nFileSizeHigh||!before.nFileSizeLow||before.nFileSizeLow>512u*1024u*1024u||
       !CryptCreateHash(receiver.crypto,CALG_SHA_256,0,0,&receiver.hash))goto done;
    for(;;) {
        if(!ReadFile(receiver.image_file,buffer,sizeof buffer,&n,NULL))goto done;
        if(!n)break;total+=n;
        if(total>before.nFileSizeLow||!CryptHashData(receiver.hash,buffer,n,0))goto done;
    }
    if(total!=before.nFileSizeLow||!GetFileInformationByHandle(receiver.image_file,&after)||
       before.dwFileAttributes!=after.dwFileAttributes||before.dwVolumeSerialNumber!=after.dwVolumeSerialNumber||
       before.nFileIndexHigh!=after.nFileIndexHigh||before.nFileIndexLow!=after.nFileIndexLow||
       before.nNumberOfLinks!=after.nNumberOfLinks||before.nFileSizeHigh!=after.nFileSizeHigh||before.nFileSizeLow!=after.nFileSizeLow||
       CompareFileTime(&before.ftCreationTime,&after.ftCreationTime)||CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)||
       !CryptGetHashParam(receiver.hash,HP_HASHVAL,digest,&bytes,0)||
       bytes!=32||memcmp(digest,expected,32))goto done;
    okay=1;
done:
    if(!okay)r_first(GetLastError());
    SecureZeroMemory(buffer,sizeof buffer);SecureZeroMemory(digest,sizeof digest);
    if(!r_hash_close())okay=0;
    if(okay&&retain&&!receiver.host_file){receiver.host_file=receiver.image_file;receiver.image_file=NULL;}
    else if(!r_close_handle(&receiver.image_file,&receiver.image_close))okay=0;
    return okay;
}
static int r_permission(void) {
    uint64_t wall,mono;int okay;
    if(receiver.denied||receiver.held||!P2PT_Current(receiver.io))return 0;
    AcquireSRWLockExclusive(&receiver.lock);
    okay=!receiver.policy.dead&&r_clock(&wall,&mono)&&mono<receiver.cleanup_deadline&&
        (!receiver.clock_seen||(wall>=receiver.last_wall&&mono>=receiver.last_mono))&&
        (receiver.policy.active?P2PRObserve(&receiver.policy,wall,mono):
            receiver.guest_active?mono<receiver.guest_deadline:mono<receiver.work_deadline);
    if(okay){receiver.clock_seen=1;receiver.last_wall=wall;receiver.last_mono=mono;}
    if(okay&&receiver.effective_role==2&&receiver.guest_committed) {
        p2pp_scope scope=receiver.scope;
        if(receiver.guest_active)scope.original_deadline=receiver.guest_hard_deadline;
        okay=mono<receiver.guest_deadline&&(!receiver.guest_active||(wall<receiver.guest_hard_wall&&mono<receiver.guest_hard_deadline))&&
            P2PRGuestParse(receiver.guest_bound,sizeof receiver.guest_bound,
                &scope,receiver.selected.uid,receiver.selected.generation,wall,mono);
    }
    ReleaseSRWLockExclusive(&receiver.lock);return okay;
}
int P2PR_CurrentPermission(void){int good;if(receiver.revoking)return 0;good=r_permission();if(!good)r_deny(ERROR_ACCESS_DENIED);return good;}
unsigned P2PR_AuthorityScope(void) {
    unsigned scope;if(receiver.revoking||!r_permission())return P2PR_NO_AUTHORITY;
    AcquireSRWLockShared(&receiver.lock);
    scope=(receiver.policy.active||receiver.guest_active)?P2PR_ACTIVE:receiver.full_installed?P2PR_INSTALLED:
        (receiver.effective_role==1&&receiver.scripts_result==1&&receiver.principals_result==1&&receiver.options_result==1)?
        P2PR_HOST_INIT_ONLY:P2PR_NO_AUTHORITY;
    ReleaseSRWLockShared(&receiver.lock);return scope;
}
static int r_wait_phase(LONG done) {
    while(InterlockedCompareExchange(&receiver.phase,0,0)!=done) {
        if(!r_permission())return 0;Sleep(2);
    }
    return r_permission();
}
static int r_wait_control(void) {
    int revoke=receiver.request_kind==0&&receiver.command.operation==P2PA_REVOKE;
    for(;;) {
        LONG phase=InterlockedCompareExchange(&receiver.phase,0,0);
        if(receiver.staged_deadline&&P2PT_Now()>=receiver.staged_deadline)return 0;
        if(phase==R_CONTROL_DONE)return revoke?P2PT_Current(receiver.io):r_permission();
        if(revoke) {
            uint64_t now=P2PT_Now(),deadline;
            AcquireSRWLockShared(&receiver.lock);deadline=receiver.policy.active?receiver.policy.deadline:receiver.work_deadline;ReleaseSRWLockShared(&receiver.lock);
            if(!now||now>=deadline||!P2PT_Current(receiver.io))return 0;
        } else if(!r_permission())return 0;
        Sleep(2);
    }
}
static int r_read_packet(unsigned char *out,DWORD cap,DWORD *length,uint64_t deadline) {
    DWORD n=0;
    if(!P2PT_Read(receiver.io,&n,4,deadline)||n<8||n>cap||!P2PT_Read(receiver.io,out,n,deadline))return 0;
    *length=n;return 1;
}
static int r_query(const unsigned char *p,DWORD n,const char *magic) {
    return n==80&&!memcmp(p,magic,8)&&p2ps_le32(p+8)==80&&!p2ps_le32(p+12)&&
        p2ps_nonzero(p+16,32)&&!memcmp(p+48,receiver.scope.request_sha256,32);
}
static int r_control_stage(unsigned kind) {
    receiver.request_kind=kind;receiver.result=0;
    if(!r_permission())return 0;
    InterlockedExchange(&receiver.phase,R_CONTROL_STAGED);
    return r_wait_control()&&receiver.result==1;
}
static int r_probe_packet(const unsigned char *p,DWORD n,unsigned role,unsigned *kind) {
    uint64_t now=P2PT_Now(),deadline;unsigned cap;
    *kind=0;
    if(!memcmp(p,"CP2PPR01",8))*kind=1;
    else if(!memcmp(p,"CP2PAR01",8))*kind=2;
    else if(!memcmp(p,"CP2PIR01",8))*kind=3;
    else return 0;
    if(n!=(role==2?120u:104u)||p2ps_le32(p+8)!=n||p2ps_le32(p+12)||
       !p2ps_nonzero(p+16,4)||p[16]>=224||p[16]==0||!p2ps_le32(p+20)||p2ps_le32(p+20)>65535)return 0;
    cap=*kind==1?10000:5000;deadline=r_u64(p+24);
    if(!now||deadline<=now||deadline-now>cap||
       memcmp(p+32,*kind==1?"CP2PING1":*kind==2?"CP2PAIN1":"CP2PIIN1",8)||!p2ps_nonzero(p+40,32))return 0;
    if(role==2&&(!p2ps_le32(p+108)||p2ps_le32(p+108)>65535||!r_u64(p+112)))return 0;
    return 1;
}
static int r_probe_reply(unsigned char *ack,uint64_t deadline) {
    memset(ack,0,48);memcpy(ack,receiver.probe_kind==1?"CP2PPA01":receiver.probe_kind==2?"CP2PAA01":"CP2PIA02",8);
    p2ps_put32(ack+8,receiver.result==1?1:0);memcpy(ack+16,receiver.control+40,32);
    return P2PT_Write(receiver.io,ack,48,deadline);
}
static DWORD WINAPI r_worker(void *unused) {
    DWORD n;unsigned char hello[152+P2P_STARTUP_V2_MAX_REQUEST],ack[P2PA_ACK_SIZE],nonce[32];
    p2pp_frame permit;uint64_t deadline,probe_authority_deadline=0;int okay=0,probe_authorized=0;(void)unused;
    if(!P2PT_Client(receiver.io,receiver.pipe,receiver.work_deadline))goto done;
    memset(hello,0,sizeof hello);memcpy(hello,"CP2PHL03",8);
    p2ps_put32(hello+8,(uint32_t)(152+receiver.request_size));p2ps_put32(hello+12,receiver.scope.child_pid);
    r_put64(hello+16,receiver.scope.child_birth);p2ps_put32(hello+24,receiver.scope.parent_pid);
    r_put64(hello+32,receiver.scope.parent_birth);memcpy(hello+40,receiver.scope.nonce,32);
    memcpy(hello+72,receiver.scope.request_sha256,32);p2ps_put32(hello+104,(uint32_t)receiver.request_size);
    r_put64(hello+112,receiver.selected.uid);r_put64(hello+120,receiver.selected.generation);
    p2ps_put32(hello+128,receiver.selected.role);r_put64(hello+136,receiver.work_deadline);
    r_put64(hello+144,receiver.cleanup_deadline);
    memcpy(hello+152,receiver.request,receiver.request_size);n=(DWORD)(152+receiver.request_size);
    if(!P2PT_Write(receiver.io,&n,4,receiver.work_deadline)||!P2PT_Write(receiver.io,hello,n,receiver.work_deadline))goto done;
    /* No default Host: the bound context is a distinct first frame and is
     * committed once on the original C1 owner, before any installer. */
    if(!r_read_packet(receiver.authority,sizeof receiver.authority,&n,receiver.work_deadline)||
       !P2PP_Parse(receiver.authority,n,&receiver.scope,P2PT_Now(),&permit)||permit.kind!=P2PP_CONTEXT||
       !r_sha(permit.body,permit.length,nonce))goto done;
    receiver.scope.sequence=permit.sequence;
    receiver.staged_deadline=permit.deadline;
    if(!r_read_packet(receiver.control,sizeof receiver.control,&n,receiver.work_deadline)||n!=96||
       memcmp(receiver.control,"CP2PRO01",8)||p2ps_le32(receiver.control+8)!=96||
       (p2ps_le32(receiver.control+12)!=1&&p2ps_le32(receiver.control+12)!=2)||
       (receiver.selected.role&&receiver.selected.role!=p2ps_le32(receiver.control+12))||
       r_u64(receiver.control+16)!=receiver.selected.uid||r_u64(receiver.control+24)!=receiver.selected.generation||
       memcmp(receiver.control+32,receiver.scope.request_sha256,32)||memcmp(receiver.control+64,nonce,32)||
       !r_control_stage(5))goto done;
    memset(ack,0,24);memcpy(ack,"CP2PRA01",8);p2ps_put32(ack+8,receiver.effective_role);r_put64(ack+16,receiver.scope.sequence);
    if(!P2PT_Write(receiver.io,ack,24,receiver.work_deadline))goto done;
    receiver.staged_deadline=0;
    InterlockedExchange(&receiver.phase,R_FETCH);
    for(;;) {
        if(!r_read_packet(receiver.authority,sizeof receiver.authority,&n,receiver.work_deadline))goto done;
        if(r_query(receiver.authority,n,"CP2PSQ01")) {
            memcpy(receiver.control,receiver.authority,n);
            if(!r_control_stage(6))goto done;
            n=112;if(!P2PT_Write(receiver.io,&n,4,receiver.work_deadline)||
                !P2PT_Write(receiver.io,receiver.socket_witness,n,receiver.work_deadline))goto done;
            InterlockedExchange(&receiver.phase,R_FETCH);continue;
        }
        if(!memcmp(receiver.authority,"CP2PIP03",8)) {
            if(!P2PP_Parse(receiver.authority,n,&receiver.scope,P2PT_Now(),&permit))goto done;
            receiver.scope.sequence=permit.sequence;receiver.kind=permit.kind;
            if(receiver.effective_role==2&&permit.kind==P2PP_CHALLENGE) {
                if(probe_authorized)goto done;probe_authorized=1;probe_authority_deadline=permit.deadline;continue;
            }
            if((receiver.effective_role==1&&permit.kind!=P2PP_CHALLENGE)||
               (receiver.effective_role==2&&permit.kind!=P2PP_INSTALL))goto done;
            if(permit.deadline<receiver.work_deadline)receiver.work_deadline=permit.deadline;
            break;
        }
        if(receiver.effective_role!=2||!probe_authorized||
           !r_probe_packet(receiver.authority,n,2,&receiver.probe_kind)||receiver.probe_kind!=1||
           r_u64(receiver.authority+24)>probe_authority_deadline)goto done;
        probe_authorized=0;memcpy(receiver.control,receiver.authority,n);
        receiver.staged_deadline=r_u64(receiver.control+24);
        if(!r_control_stage(7)||!r_probe_reply(ack,receiver.work_deadline))goto done;
        receiver.staged_deadline=0;
        InterlockedExchange(&receiver.phase,R_FETCH);
    }
    if(!r_permission()||!P2PT_Read(receiver.io,&n,4,receiver.work_deadline))goto done;
    if(receiver.effective_role==2) {if(n!=P2PR_GUEST_SIZE)goto done;}
    else if(n<192+48+112+352||n>P2P_STARTUP_V2_MAX_ENVELOPE)goto done;
    receiver.packet=(unsigned char *)HeapAlloc(GetProcessHeap(),0,n);receiver.packet_size=n;
    if(!receiver.packet||!P2PT_Read(receiver.io,receiver.packet,n,receiver.work_deadline))goto done;
    if(receiver.effective_role==2) {
        uint64_t wall,mono,expires;
        if(!r_clock(&wall,&mono)||!P2PRGuestParse(receiver.packet,n,&receiver.scope,receiver.selected.uid,
            receiver.selected.generation,wall,mono))goto done;
        expires=r_u64(receiver.packet+48);if(expires-wall>UINT64_MAX-mono)goto done;
        receiver.guest_deadline=mono+expires-wall;
        if(receiver.guest_deadline>receiver.work_deadline)receiver.guest_deadline=receiver.work_deadline;
    } else {
        if(!P2PRWireParseEnvelope(receiver.request,receiver.request_size,receiver.packet,n,r_sha,&receiver.startup)||
           receiver.startup.protocol!=3||r_u64(receiver.startup.principals+64)!=receiver.selected.uid||
           p2ps_le32(receiver.startup.principals+72)!=1||!P2PRCaptureBootstrap(&receiver.startup,&receiver.bootstrap))goto done;
        P2PRInitPolicy(&receiver.policy,&receiver.bootstrap);
    }
    if(!r_permission())goto done;InterlockedExchange(&receiver.phase,R_STARTUP_STAGED);
    if(!r_wait_phase(R_STARTUP_DONE))goto done;
    if(receiver.effective_role==2) {
        memset(ack,0,48);memcpy(ack,"CP2PGA01",8);p2ps_put32(ack+8,receiver.result==1?1:0);memcpy(ack+16,receiver.scope.nonce,32);
        if(!P2PT_Write(receiver.io,ack,48,receiver.work_deadline)||receiver.result!=1)goto done;
    } else {
        if(!P2PRWireMakeAckV2(&receiver.startup,receiver.scripts_result,receiver.principals_result,receiver.options_result,ack)||
           !P2PT_Write(receiver.io,ack,P2P_STARTUP_ACK_SIZE,receiver.work_deadline)||receiver.result!=1||
           !CryptGenRandom(receiver.crypto,32,nonce)||!P2PRMakeHello(&receiver.bootstrap,nonce,receiver.hello))goto done;
        n=P2PA_HELLO_SIZE;
        if(!P2PT_Write(receiver.io,&n,4,receiver.work_deadline)||!P2PT_Write(receiver.io,receiver.hello,n,receiver.work_deadline))goto done;
    }
    InterlockedExchange(&receiver.phase,R_CONTROL);
    while(r_permission()) {
        uint64_t wall,mono;
        AcquireSRWLockExclusive(&receiver.lock);
        deadline=receiver.policy.active?receiver.policy.deadline:receiver.guest_active?receiver.guest_deadline:receiver.work_deadline;
        ReleaseSRWLockExclusive(&receiver.lock);
        if(!r_read_packet(receiver.authority,sizeof receiver.authority,&n,deadline))break;
        receiver.request_kind=0;receiver.connected_evidence=0;
        receiver.staged_deadline=0;
        if(n==192&&!memcmp(receiver.authority,"CP2PIQ02",8)) {
            if(!receiver.handoff_committed||receiver.release_committed||!receiver.full_installed||
               !P2PH_AckQuery(receiver.authority,n,receiver.handoff_ack,P2PT_Now()))break;
            memcpy(receiver.control,receiver.authority,n);receiver.request_kind=12;
        } else if(n>=P2PP_HEADER&&!memcmp(receiver.authority,"CP2PIP03",8)&&p2ps_le32(receiver.authority+8)==P2PP_RELEASE) {
            if(!receiver.handoff_committed||receiver.release_committed||!P2PP_Parse(receiver.authority,n,&receiver.scope,P2PT_Now(),&permit)||
               !r_sha(permit.body,permit.length,receiver.envelope_sha))break;
            receiver.scope.sequence=permit.sequence;receiver.staged_deadline=permit.deadline;
            if(!r_read_packet(receiver.control,sizeof receiver.control,&n,deadline)||!r_clock(&wall,&mono)||
               !P2PH_Release(receiver.control,n,&receiver.scope,receiver.selected.uid,receiver.selected.generation,receiver.effective_role,
                    receiver.initial_id,receiver.handoff_id,receiver.envelope_sha,permit.deadline,wall,mono))break;
            receiver.request_kind=11;
        } else if(n>=P2PP_HEADER&&!memcmp(receiver.authority,"CP2PIP03",8)&&p2ps_le32(receiver.authority+8)==P2PP_HANDOFF) {
            if(!receiver.full_installed||receiver.handoff_committed||!P2PP_Parse(receiver.authority,n,&receiver.scope,P2PT_Now(),&permit)||
               !r_sha(permit.body,permit.length,receiver.envelope_sha))break;
            receiver.scope.sequence=permit.sequence;receiver.staged_deadline=permit.deadline;
            if(!r_read_packet(receiver.control,sizeof receiver.control,&n,deadline)||!r_clock(&wall,&mono)||
               !P2PH_Install(receiver.control,n,&receiver.scope,receiver.selected.uid,receiver.selected.generation,receiver.effective_role,
                    receiver.envelope_sha,permit.deadline,wall,mono))break;
            receiver.request_kind=10;
        } else if(receiver.effective_role==1&&receiver.full_installed&&r_query(receiver.authority,n,"CP2PSQ02")) {
            memcpy(receiver.control,receiver.authority,n);receiver.request_kind=9;
        } else if(r_query(receiver.authority,n,"CP2PEQ01")||r_query(receiver.authority,n,"CP2PEC02")) {
            receiver.connected_evidence=!memcmp(receiver.authority,"CP2PEC02",8);
            memcpy(receiver.control,receiver.authority,n);receiver.request_kind=4;
        } else if(r_probe_packet(receiver.authority,n,receiver.effective_role,&receiver.probe_kind)) {
            memcpy(receiver.control,receiver.authority,n);receiver.request_kind=receiver.effective_role==2?7:2;
            receiver.staged_deadline=r_u64(receiver.control+24);
        } else if(receiver.effective_role==2) {
            if(n!=352||memcmp(receiver.authority,"CP2PGU01",8)||p2ps_le32(receiver.authority+8)!=352||
               p2ps_le32(receiver.authority+12)||p2ps_le32(receiver.authority+20)||
               (p2ps_le32(receiver.authority+16)!=1&&p2ps_le32(receiver.authority+16)!=2))break;
            memcpy(receiver.control,receiver.authority,n);receiver.request_kind=8;
        } else if(!memcmp(receiver.authority,"CP2PIP03",8)) {
            if(receiver.full_installed||!P2PP_Parse(receiver.authority,n,&receiver.scope,P2PT_Now(),&permit)||permit.kind!=P2PP_INSTALL)break;
            receiver.scope.sequence=permit.sequence;
            if(!r_read_packet(receiver.control,656,&n,deadline)||!p2pa_bundle(receiver.control,n)||n<144||
               memcmp(receiver.control,receiver.bootstrap.principals,40)||r_u64(receiver.control+40)!=2||
               memcmp(receiver.control+64,receiver.bootstrap.principals+64,8))break;
            receiver.upgrade_length=n;receiver.request_kind=3;
        } else if(n<=P2PA_MAX_CONTROL&&!memcmp(receiver.authority,"CPDQv3\0\0",8)) {
            memcpy(receiver.control,receiver.authority,n);
            if(!P2PRParseQuery(&receiver.bootstrap,receiver.hello,receiver.control,n,r_sha,&receiver.query))break;
            receiver.request_kind=1;
        } else {
            if(!receiver.full_installed||n>P2PA_MAX_CONTROL)break;
            memcpy(receiver.control,receiver.authority,n);
            if(!P2PRParseControl(&receiver.bootstrap,receiver.hello,receiver.control,n,r_sha,&receiver.command))break;
            AcquireSRWLockExclusive(&receiver.lock);okay=r_clock(&wall,&mono)&&P2PRPrepare(&receiver.policy,&receiver.command,wall,mono);
            ReleaseSRWLockExclusive(&receiver.lock);if(!okay)break;
            if(receiver.command.operation==P2PA_REVOKE)InterlockedExchange(&receiver.revoking,1);
        }
        if(!r_control_stage(receiver.request_kind))break;
        if(receiver.request_kind==1) {
            if(!P2PRMakeQueryReply(&receiver.query,receiver.result,receiver.native_deadline,receiver.native_observed,ack)||!P2PT_Write(receiver.io,ack,P2PA_QUERY_REPLY,deadline))break;
        } else if(receiver.request_kind==10||receiver.request_kind==12) {
            if(!P2PT_Write(receiver.io,receiver.handoff_ack,192,deadline))break;
        } else if(receiver.request_kind==9) {
            n=112;if(!P2PT_Write(receiver.io,&n,4,deadline)||!P2PT_Write(receiver.io,receiver.socket_witness,n,deadline))break;
        } else if(receiver.request_kind==4) {
            if(!P2PT_Write(receiver.io,&receiver.evidence_size,4,deadline)||!P2PT_Write(receiver.io,receiver.evidence,receiver.evidence_size,deadline))break;
        } else if(receiver.request_kind==0) {
            if(!P2PRMakeAck(&receiver.command,receiver.hello+16,receiver.result,receiver.policy.native_sequence,ack)||!P2PT_Write(receiver.io,ack,P2PA_ACK_SIZE,deadline))break;
            if(receiver.command.operation==P2PA_REVOKE){receiver.valid_revocation=1;break;}
        } else if(receiver.request_kind==2||receiver.request_kind==7) {
            if(!r_probe_reply(ack,deadline))break;
        } else {
            memset(ack,0,24);memcpy(ack,receiver.request_kind==11?"CP2PIRK1":receiver.request_kind==8?"CP2GUA01":"CP2PIA01",8);p2ps_put32(ack+8,1);
            r_put64(ack+16,receiver.request_kind==11?receiver.selected.generation:receiver.request_kind==8?receiver.guest_control_sequence:receiver.scope.sequence);
            if(!P2PT_Write(receiver.io,ack,24,deadline))break;
            if(receiver.request_kind==8&&p2ps_le32(receiver.control+16)==2){receiver.valid_revocation=1;break;}
            if(receiver.request_kind==3) {
                if(!CryptGenRandom(receiver.crypto,32,nonce)||!P2PRMakeHello(&receiver.bootstrap,nonce,receiver.hello))break;
                n=P2PA_HELLO_SIZE;if(!P2PT_Write(receiver.io,&n,4,deadline)||!P2PT_Write(receiver.io,receiver.hello,n,deadline))break;
            }
        }
        receiver.staged_deadline=0;InterlockedExchange(&receiver.phase,R_CONTROL);
    }
done:
    if(!receiver.stopping&&!receiver.valid_revocation)r_first(P2PT_FirstError(receiver.io));
    InterlockedExchange(&receiver.denied,1);SecureZeroMemory(nonce,sizeof nonce);SecureZeroMemory(ack,sizeof ack);
    for(;;) {LONG phase=InterlockedCompareExchange(&receiver.phase,0,0);
        if(phase==R_STARTUP_OWNED||phase==R_CONTROL_OWNED)break;
        if(InterlockedCompareExchange(&receiver.phase,R_DEAD,phase)==phase)break;}
    return 0;
}
int P2PR_Begin(HANDLE inherited,const wchar_t *image,const wchar_t *pipe,const wchar_t *host_image,const p2pr_release_policy *policy,
               const p2pr_selection *selection,uint64_t work,uint64_t cleanup) {
    FILETIME born,ended,kernel,user;DWORD flags,parent_pid,size=32768;wchar_t actual[32768];uint64_t now=P2PT_Now();
    if(InterlockedCompareExchange(&receiver.claimed,1,0)!=0)return 0;
    receiver.inherited=inherited;receiver.image_file=NULL;receiver.lock=(SRWLOCK)SRWLOCK_INIT;
    receiver.owner_thread=GetCurrentThreadId();receiver.work_deadline=work;receiver.cleanup_deadline=cleanup;
    if(!r_policy(policy)||!inherited||!selection||!selection->uid||!selection->generation||
       !memchr(selection->map,0,sizeof selection->map)||!memchr(selection->gametype,0,sizeof selection->gametype)||
       !memchr(selection->category,0,sizeof selection->category)||
       selection->role>2||!now||work<=now||work-now>30000||cleanup<15000||work>cleanup-15000||
       !image||!pipe||!host_image||wcslen(image)>=32768||wcslen(pipe)>=128||
       !GetHandleInformation(inherited,&flags)||!(flags&HANDLE_FLAG_INHERIT)||
       !(parent_pid=GetProcessId(inherited))||parent_pid==GetCurrentProcessId()||WaitForSingleObject(inherited,0)!=WAIT_TIMEOUT||
       !GetProcessTimes(inherited,&born,&ended,&kernel,&user)||
       !QueryFullProcessImageNameW(inherited,0,actual,&size)||size>=32768)goto fail;
    actual[size]=0;if(_wcsicmp(image,actual))goto fail;
    if(!CryptAcquireContextW(&receiver.crypto,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)||
       !r_parent_hash(image,policy->launcher_sha256,0))goto fail;
    receiver.parent_adopted=1;
    memcpy(receiver.launcher_hash,policy->launcher_sha256,32);memcpy(receiver.client_hash,policy->client_image_sha256,32);
    receiver.scope.parent_pid=parent_pid;receiver.scope.parent_birth=(uint64_t)born.dwLowDateTime|(uint64_t)born.dwHighDateTime<<32;
    receiver.scope.child_pid=GetCurrentProcessId();
    if(!GetProcessTimes(GetCurrentProcess(),&born,&ended,&kernel,&user))goto fail;
    receiver.scope.child_birth=(uint64_t)born.dwLowDateTime|(uint64_t)born.dwHighDateTime<<32;
    size=32768;
    if(!QueryFullProcessImageNameW(GetCurrentProcess(),0,actual,&size)||size>=32768)goto fail;
    actual[size]=0;if(!r_parent_hash(actual,policy->client_image_sha256,0)||!r_parent_hash(host_image,policy->host_sha256,1))goto fail;
    receiver.scope.original_deadline=work;
    receiver.selected=*selection;wcscpy(receiver.pipe,pipe);wcscpy(receiver.parent_path,image);
    if(!CryptGenRandom(receiver.crypto,32,receiver.scope.nonce)||
       !P2PRWireMakeRequestV2(selection->map,selection->gametype,selection->category,selection->difficulty_present,
          selection->difficulty,receiver.scope.nonce,receiver.request,sizeof receiver.request,&receiver.request_size)||
       !r_sha(receiver.request,receiver.request_size,receiver.scope.request_sha256))goto fail;
    receiver.io=P2PT_New(inherited,parent_pid,image,cleanup);
    if(!receiver.io||P2PT_FirstError(receiver.io)||!P2PT_Current(receiver.io))goto fail;
    InterlockedExchange(&receiver.phase,R_FETCH);
    if(!P2PT_Start(receiver.io,r_worker,NULL))goto fail;
    return 1;
fail:r_deny(GetLastError());P2PR_Retire(cleanup);return 0;
}
static int r_same_selection(const p2pr_selection *a,const p2pr_selection *b) {
    if(!memchr(a->map,0,sizeof a->map)||!memchr(a->gametype,0,sizeof a->gametype)||
       !memchr(a->category,0,sizeof a->category))return 0;
    return !strcmp(a->map,b->map)&&!strcmp(a->gametype,b->gametype)&&!strcmp(a->category,b->category)&&
        a->difficulty_present==b->difficulty_present&&a->difficulty==b->difficulty&&a->uid==b->uid&&
        a->generation==b->generation&&(a->role==b->role||(!b->role&&a->role==receiver.effective_role));
}
static int r_evidence(const p2pr_selection *actual,const p2pr_native_api *api) {
    static const unsigned char empty=0;
    p2pr_selection before,after;unsigned char native[16]={0};
    const unsigned char *scope,*options=NULL,*manifest;unsigned scope_length,options_length=0;
    uint64_t wall,start=P2PT_Now(),finished,deadline,observed;
    receiver.evidence_size=0;
    if(!receiver.full_installed||!api->read_selection||api->read_selection(&before)!=1||
       !r_same_selection(&before,actual)||!r_same_selection(&before,&receiver.selected)||!r_permission())return 0;
    if(receiver.effective_role==1) {
        int (__cdecl *observer)(const void *,unsigned,void *,unsigned)=receiver.connected_evidence?api->observe_connected:api->observe;
        if(!receiver.installed_size||!observer||!api->observe_options||!api->current||api->current()!=1||
           observer(receiver.installed,receiver.installed_size,native,sizeof native)!=1||
           api->observe_options(receiver.startup.options,receiver.startup.options_length)!=1||api->current()!=1)return 0;
        scope=receiver.installed;scope_length=receiver.installed_size;
        options=receiver.startup.options;options_length=receiver.startup.options_length;
        manifest=receiver.startup.scripts+16;deadline=r_u64(native);observed=r_u64(native+8);
        if(deadline>(receiver.policy.active?receiver.policy.deadline:receiver.work_deadline))deadline=receiver.policy.active?receiver.policy.deadline:receiver.work_deadline;
    } else {
        p2pp_scope current_scope=receiver.scope;
        if(receiver.guest_active)current_scope.original_deadline=receiver.guest_hard_deadline;
        if(!receiver.guest_committed||!r_clock(&wall,&finished)||
           !P2PRGuestParse(receiver.guest_bound,sizeof receiver.guest_bound,&current_scope,
               receiver.selected.uid,receiver.selected.generation,wall,finished))return 0;
        scope=receiver.guest_bound;scope_length=sizeof receiver.guest_bound;manifest=receiver.guest_bound+176;
        deadline=receiver.guest_deadline;observed=finished;
    }
    if(api->read_selection(&after)!=1||!r_same_selection(&after,&before)||!r_permission())return 0;
    finished=P2PT_Now();
    if(!start||!finished||observed<start||observed>finished||deadline<=finished||
       328u+scope_length+options_length>sizeof receiver.evidence)return 0;
    memset(receiver.evidence,0,328);memcpy(receiver.evidence,receiver.connected_evidence?"CP2PER02":"CP2PER01",8);
    p2ps_put32(receiver.evidence+8,328+scope_length+options_length);p2ps_put32(receiver.evidence+12,receiver.effective_role);
    memcpy(receiver.evidence+16,receiver.control+16,32);memcpy(receiver.evidence+48,receiver.scope.nonce,32);
    memcpy(receiver.evidence+80,receiver.scope.request_sha256,32);p2ps_put32(receiver.evidence+112,receiver.scope.parent_pid);
    p2ps_put32(receiver.evidence+116,receiver.scope.child_pid);r_put64(receiver.evidence+120,receiver.scope.parent_birth);
    r_put64(receiver.evidence+128,receiver.scope.child_birth);p2ps_put32(receiver.evidence+136,GetCurrentThreadId());
    r_put64(receiver.evidence+144,deadline);r_put64(receiver.evidence+152,observed);
    p2ps_put32(receiver.evidence+160,scope_length);p2ps_put32(receiver.evidence+164,options_length);
    if(!r_sha(scope,scope_length,receiver.evidence+168)||!r_sha(options?options:&empty,options_length,receiver.evidence+200))return 0;
    memcpy(receiver.evidence+232,manifest,32);memcpy(receiver.evidence+264,receiver.launcher_hash,32);
    memcpy(receiver.evidence+296,receiver.client_hash,32);memcpy(receiver.evidence+328,scope,scope_length);
    if(options_length)memcpy(receiver.evidence+328+scope_length,options,options_length);
    if(!r_permission())return 0;
    receiver.evidence_size=328+scope_length+options_length;return 1;
}
static int r_guest_control(uint64_t wall,uint64_t mono) {
    const unsigned char *p=receiver.control,*next=p+96;unsigned op=p2ps_le32(p+16);
    uint64_t sequence=r_u64(p+24),hard=r_u64(p+80),deadline=r_u64(p+88),expires;
    p2pp_scope scope=receiver.scope;
    if(!receiver.guest_committed||!receiver.full_installed||receiver.guest_control_sequence==UINT64_MAX||
       sequence!=receiver.guest_control_sequence+1||!p2ps_nonzero(p+32,16)||!p2ps_nonzero(p+48,32))return 0;
    if(receiver.guest_active&&(hard!=receiver.guest_hard_wall||memcmp(p+32,receiver.guest_active_id,16)))return 0;
    if(op==2) {
        if(!receiver.guest_active||deadline)return 0;
        receiver.guest_control_sequence=sequence;InterlockedExchange(&receiver.revoking,1);return 1;
    }
    if(op!=1||hard<=wall||hard-wall>UINT64_MAX-mono||deadline<=mono||deadline-mono>30000)return 0;
    if(!receiver.guest_active) {
        receiver.guest_hard_deadline=mono+hard-wall;
        if(receiver.guest_hard_deadline>receiver.cleanup_deadline)receiver.guest_hard_deadline=receiver.cleanup_deadline;
    }
    if(deadline>receiver.guest_hard_deadline||memcmp(next,receiver.guest_bound,40)||
       memcmp(next+56,receiver.guest_bound+56,200))return 0;
    scope.original_deadline=receiver.guest_hard_deadline;
    if(!P2PRGuestParse(next,256,&scope,receiver.selected.uid,receiver.selected.generation,wall,mono))return 0;
    expires=r_u64(next+48);if(expires>hard||expires-wall>UINT64_MAX-mono||deadline>mono+expires-wall)return 0;
    memcpy(receiver.guest_bound,next,256);memcpy(receiver.guest_active_id,p+32,16);
    receiver.guest_control_sequence=sequence;receiver.guest_hard_wall=hard;
    receiver.guest_deadline=deadline;receiver.guest_active=1;return 1;
}
static int r_handoff_ack(const p2pr_native_api *api) {
    unsigned char native[16]={0},digest[32];const unsigned char *scope;unsigned length;
    uint64_t start=P2PT_Now(),deadline,observed,finished;
    if(receiver.effective_role==1) {
        if(!api->observe||api->observe(receiver.installed,receiver.installed_size,native,sizeof native)!=1)return 0;
        scope=receiver.installed;length=receiver.installed_size;deadline=r_u64(native);observed=r_u64(native+8);
    } else {scope=receiver.guest_bound;length=256;deadline=receiver.guest_deadline;observed=P2PT_Now();}
    finished=P2PT_Now();if(!start||!finished||observed<start||observed>finished||deadline<=finished||!r_permission())return 0;
    if(deadline>receiver.work_deadline)deadline=receiver.work_deadline;
    if(!r_sha(scope,length,digest))return 0;
    return P2PH_Ack(&receiver.scope,receiver.selected.uid,receiver.selected.generation,receiver.effective_role,
        receiver.initial_id,receiver.handoff_id,digest,deadline,observed,receiver.handoff_ack)&&r_permission();
}
int P2PR_GuestEndpoint(unsigned char ipv4[4],unsigned *port,uint64_t *generation) {
    if(!ipv4||!port||!generation||receiver.effective_role!=2||!receiver.guest_committed||!receiver.handoff_committed||
       !P2PR_CurrentPermission())return 0;
    AcquireSRWLockShared(&receiver.lock);
    memcpy(ipv4,receiver.guest_bound+240,4);*port=(unsigned)receiver.guest_bound[244]|(unsigned)receiver.guest_bound[245]<<8;
    *generation=receiver.selected.generation;
    ReleaseSRWLockShared(&receiver.lock);return P2PR_CurrentPermission();
}
unsigned P2PR_EffectiveRole(void){return receiver.effective_role;}
int P2PR_InitialHandoffCurrent(const p2pr_selection *actual) {
    uint64_t wall,mono;
    return actual&&receiver.handoff_committed&&receiver.release_committed&&!receiver.revoking&&r_same_selection(actual,&receiver.selected)&&
        r_clock(&wall,&mono)&&wall<receiver.release_expires&&mono<receiver.release_deadline&&r_permission();
}
int P2PR_OwnerTick(const p2pr_selection *actual,const p2pr_native_api *api) {
    LONG phase;int good=0;uint64_t wall,mono,offered=0;unsigned char observed[16]={0};
    if(GetCurrentThreadId()!=receiver.owner_thread||!actual||!api)return 0;
    if(!r_same_selection(actual,&receiver.selected)||!r_permission()) {
        r_deny(ERROR_ACCESS_DENIED);if(api->quarantine)api->quarantine();return 0;
    }
    if(receiver.staged_deadline&&P2PT_Now()>=receiver.staged_deadline) {
        r_deny(ERROR_TIMEOUT);if(api->quarantine)api->quarantine();return 0;
    }
    phase=InterlockedCompareExchange(&receiver.phase,R_STARTUP_OWNED,R_STARTUP_STAGED);
    if(phase==R_STARTUP_STAGED) {
        if(receiver.effective_role==1&&(!api->install_boundary||api->install_boundary()!=1)) {
            InterlockedExchange(&receiver.phase,R_STARTUP_STAGED);return r_permission();
        }
        if(receiver.effective_role==2) {
            uint64_t current_wall,current_mono;
            p2pr_selection current;
            good=api->read_selection&&api->read_selection(&current)==1&&r_same_selection(actual,&current)&&
                r_clock(&current_wall,&current_mono)&&P2PRGuestParse(receiver.packet,receiver.packet_size,&receiver.scope,
                    actual->uid,actual->generation,current_wall,current_mono)&&r_permission();
            if(good) {
                AcquireSRWLockExclusive(&receiver.lock);
                memcpy(receiver.guest_bound,receiver.packet,sizeof receiver.guest_bound);
                InterlockedExchange(&receiver.guest_committed,1);receiver.full_installed=1;
                ReleaseSRWLockExclusive(&receiver.lock);
            }
            receiver.result=good?1:0;
        } else {
            if(api->scripts&&api->principals&&api->options) {
                receiver.scripts_result=api->scripts(receiver.startup.scripts,receiver.startup.scripts_length);
                if(receiver.scripts_result==1&&r_permission())receiver.principals_result=api->principals(receiver.startup.principals,receiver.startup.principals_length);
                if(receiver.principals_result==1&&r_permission())receiver.options_result=api->options(receiver.startup.options,receiver.startup.options_length);
            }
            good=receiver.scripts_result==1&&receiver.principals_result==1&&receiver.options_result==1&&r_permission();
            receiver.result=good?1:0;
        }
        if(!good){r_deny(ERROR_ACCESS_DENIED);if(api->quarantine)api->quarantine();}
        InterlockedExchange(&receiver.phase,R_STARTUP_DONE);return good;
    }
    phase=InterlockedCompareExchange(&receiver.phase,R_CONTROL_OWNED,R_CONTROL_STAGED);
    if(phase!=R_CONTROL_STAGED)return r_permission();
    if(receiver.effective_role==1&&receiver.request_kind!=5&&receiver.request_kind!=6&&
       (!api->running||api->running()!=1)) {
        InterlockedExchange(&receiver.phase,R_CONTROL_STAGED);return r_permission();
    }
    if(receiver.request_kind==5) {
        unsigned role=p2ps_le32(receiver.control+12);
        good=!receiver.effective_role&&api->commit_role&&api->commit_role(role,actual->uid,actual->generation)==1&&r_permission();
        if(good)receiver.effective_role=role;
    } else if(receiver.request_kind==6) {
        good=api->socket_witness?api->socket_witness(receiver.control,80,receiver.scope.nonce,actual->generation,
            receiver.socket_witness,sizeof receiver.socket_witness):-1;
        if(!good){InterlockedExchange(&receiver.phase,R_CONTROL_STAGED);return r_permission();}
        good=good==1&&r_permission();
    } else if(receiver.request_kind==7) {
        good=api->guest_probe?api->guest_probe(receiver.control+16,p2ps_le32(receiver.control+20),receiver.control+32,72,
            r_u64(receiver.control+24),receiver.control+104,p2ps_le32(receiver.control+108),r_u64(receiver.control+112),actual->generation):-1;
        if(!good){InterlockedExchange(&receiver.phase,R_CONTROL_STAGED);return r_permission();}
        good=good==1&&r_permission();
    } else if(receiver.request_kind==8) {
        AcquireSRWLockExclusive(&receiver.lock);
        good=r_clock(&wall,&mono)&&r_guest_control(wall,mono);
        ReleaseSRWLockExclusive(&receiver.lock);good=good&&r_permission();
    } else if(receiver.request_kind==12) {
        good=receiver.handoff_committed&&!receiver.release_committed&&receiver.full_installed&&r_clock(&wall,&mono)&&
            P2PH_AckQuery(receiver.control,192,receiver.handoff_ack,mono)&&
            (receiver.effective_role==2?P2PRGuestParse(receiver.guest_bound,256,&receiver.scope,receiver.selected.uid,
                receiver.selected.generation,wall,mono):(api->current&&api->current()==1&&api->observe_options&&
                api->observe_options(receiver.startup.options,receiver.startup.options_length)==1))&&r_permission();
        if(good)good=r_handoff_ack(api)&&!memcmp(receiver.handoff_ack+8,receiver.control+8,176)&&
            r_u64(receiver.handoff_ack+184)>=mono&&r_permission();
    } else if(receiver.request_kind==11) {
        good=receiver.handoff_committed&&!receiver.release_committed&&r_clock(&wall,&mono)&&
            P2PH_Release(receiver.control,224,&receiver.scope,receiver.selected.uid,receiver.selected.generation,receiver.effective_role,
                receiver.initial_id,receiver.handoff_id,receiver.envelope_sha,receiver.staged_deadline,wall,mono)&&
            receiver.staged_deadline<=receiver.work_deadline&&
            (receiver.effective_role==2||(api->current&&api->current()==1&&api->observe_options&&
                api->observe_options(receiver.startup.options,receiver.startup.options_length)==1))&&r_permission();
        if(good) {
            memcpy(receiver.release_id,receiver.control+144,32);receiver.release_deadline=r_u64(receiver.control+176);
            receiver.release_expires=r_u64(receiver.control+184);receiver.release_committed=1;
        }
    } else if(receiver.request_kind==10) {
        good=!receiver.handoff_committed&&receiver.full_installed&&r_clock(&wall,&mono)&&
            P2PH_Install(receiver.control,receiver.effective_role==2?448:192,&receiver.scope,receiver.selected.uid,receiver.selected.generation,
                receiver.effective_role,receiver.envelope_sha,receiver.staged_deadline,wall,mono)&&receiver.staged_deadline<=receiver.work_deadline&&
            (receiver.effective_role==2||(api->current&&api->current()==1&&api->observe_options&&
                api->observe_options(receiver.startup.options,receiver.startup.options_length)==1))&&r_permission();
        if(good&&receiver.effective_role==2) {
            const unsigned char *next=receiver.control+192;
            good=!memcmp(next,receiver.guest_bound,240)&&!memcmp(next+246,receiver.guest_bound+246,10)&&
                P2PRGuestParse(next,256,&receiver.scope,receiver.selected.uid,receiver.selected.generation,wall,mono);
            if(good){AcquireSRWLockExclusive(&receiver.lock);memcpy(receiver.guest_bound,next,256);ReleaseSRWLockExclusive(&receiver.lock);}
        }
        if(good) {
            memcpy(receiver.initial_id,receiver.control+96,16);memcpy(receiver.handoff_id,receiver.control+112,32);
            receiver.handoff_deadline=r_u64(receiver.control+144);receiver.handoff_expires=r_u64(receiver.control+152);
            receiver.handoff_committed=1;
            good=r_handoff_ack(api);
        }
    } else if(receiver.request_kind==9) {
        unsigned char sample[32]={0};
        good=receiver.effective_role==1&&receiver.full_installed&&api->observe_server_socket&&api->current&&api->current()==1&&
            api->observe_server_socket(sample,sizeof sample)==1&&r_permission();
        if(good) {
            memset(receiver.socket_witness,0,sizeof receiver.socket_witness);memcpy(receiver.socket_witness,"CP2PSW02",8);
            p2ps_put32(receiver.socket_witness+8,112);memcpy(receiver.socket_witness+16,receiver.control+16,32);
            memcpy(receiver.socket_witness+48,receiver.scope.nonce,32);memcpy(receiver.socket_witness+80,sample,32);
            good=p2ps_le32(sample+4)&&p2ps_le32(sample+4)<=65535&&r_u64(sample+8)&&r_u64(sample+16)&&
                p2ps_le32(sample+24)==receiver.owner_thread&&!p2ps_le32(sample+28)&&r_u64(sample+8)<=P2PT_Now();
        }
    } else if(receiver.request_kind==4) {
        good=r_evidence(actual,api);
        if(!good&&receiver.connected_evidence&&receiver.effective_role==1&&r_permission()) {
            /* Waiting for actual all-principal DATA sessions never creates a
             * new deadline or a positive observation. Original scope bounds it. */
            InterlockedExchange(&receiver.phase,R_CONTROL_STAGED);return 1;
        }
    } else if(receiver.request_kind==2) {
        int (__cdecl *send)(const unsigned char[4],unsigned,const void *,unsigned,uint64_t)=
            receiver.probe_kind==1?api->send_probe:receiver.probe_kind==2?api->send_active_probe:api->send_initial_probe;
        good=send&&api->current&&api->current()==1&&send(receiver.control+16,p2ps_le32(receiver.control+20),
            receiver.control+32,72,r_u64(receiver.control+24))==1&&r_permission();
    } else if(receiver.request_kind==3) {
        unsigned count=p2ps_le32(receiver.control+72),i;int host_unchanged=0;
        p2p_startup_view_t next=receiver.startup;
        for(i=0;i<count;i++)if(r_u64(receiver.control+80+i*32)==receiver.selected.uid)
            host_unchanged=!memcmp(receiver.control+80+i*32,receiver.bootstrap.principals+80,32);
        good=host_unchanged&&api->principals&&api->current&&api->observe_options&&
            api->observe_options(receiver.startup.options,receiver.startup.options_length)==1&&
            api->principals(receiver.control,receiver.upgrade_length)==1&&api->current()==1&&r_permission();
        if(good) {
            next.principals=receiver.control;next.principals_length=receiver.upgrade_length;
            AcquireSRWLockExclusive(&receiver.lock);
            good=P2PRCaptureBootstrap(&next,&receiver.bootstrap);
            if(good){
                P2PRInitPolicy(&receiver.policy,&receiver.bootstrap);receiver.full_installed=1;
                memcpy(receiver.installed,receiver.control,receiver.upgrade_length);receiver.installed_size=receiver.upgrade_length;
            }
            ReleaseSRWLockExclusive(&receiver.lock);
        }
    } else if(receiver.request_kind==1) {
        receiver.native_deadline=receiver.native_observed=0;
        if(api->observe&&api->observe(receiver.query.principals,receiver.query.length,observed,sizeof observed)==1&&r_permission()) {
            receiver.native_deadline=r_u64(observed);receiver.native_observed=r_u64(observed+8);
            if(receiver.native_deadline>(receiver.policy.active?receiver.policy.deadline:receiver.work_deadline))receiver.native_deadline=receiver.policy.active?receiver.policy.deadline:receiver.work_deadline;
            good=receiver.native_deadline>receiver.native_observed;
        }
    } else if(receiver.command.operation==P2PA_REVOKE) {
        if(api->quarantine)api->quarantine();good=api->current&&api->current()==0;
        AcquireSRWLockExclusive(&receiver.lock);receiver.policy.dead=1;receiver.policy.sequence=receiver.command.sequence;ReleaseSRWLockExclusive(&receiver.lock);
    } else {
        AcquireSRWLockExclusive(&receiver.lock);
        good=r_clock(&wall,&mono)&&P2PRPrepare(&receiver.policy,&receiver.command,wall,mono);
        if(good) {
            if(receiver.policy.offered_hard_deadline>receiver.cleanup_deadline)receiver.policy.offered_hard_deadline=receiver.cleanup_deadline;
            if(receiver.policy.offered_deadline>receiver.cleanup_deadline)receiver.policy.offered_deadline=receiver.cleanup_deadline;
            offered=receiver.policy.offered_deadline;
        }
        ReleaseSRWLockExclusive(&receiver.lock);
        if(good)good=api->active&&api->current&&api->current()==1&&api->active(receiver.command.principals,receiver.command.length,offered)==1&&r_permission();
        AcquireSRWLockExclusive(&receiver.lock);good=r_clock(&wall,&mono)&&P2PRCommit(&receiver.policy,&receiver.command,good,wall,mono);ReleaseSRWLockExclusive(&receiver.lock);
        if(good) {
            memcpy(receiver.installed,receiver.command.principals,receiver.command.length);
            receiver.installed_size=receiver.command.length;
        }
    }
    SecureZeroMemory(observed,sizeof observed);receiver.result=good?1:0;
    if(!good){r_deny(ERROR_ACCESS_DENIED);if(api->quarantine)api->quarantine();}
    InterlockedExchange(&receiver.phase,R_CONTROL_DONE);return good;
}
void P2PR_RequestStop(void){InterlockedExchange(&receiver.stopping,1);InterlockedExchange(&receiver.denied,1);P2PT_RequestStop(receiver.io);}
int P2PR_Retire(uint64_t deadline) {
    LONG phase;int okay=1;
    if(GetCurrentThreadId()!=receiver.owner_thread){r_hold(ERROR_INVALID_THREAD_ID);return 0;}
    P2PR_RequestStop();
    if(receiver.io&&!P2PT_JoinClose(receiver.io,deadline)){r_hold(P2PT_FirstError(receiver.io));return 0;}
    phase=InterlockedCompareExchange(&receiver.phase,0,0);
    if(phase==R_STARTUP_OWNED||phase==R_CONTROL_OWNED){r_hold(ERROR_BUSY);return 0;}
    if(receiver.packet) {
        if(receiver.packet_free_attempted){r_hold(ERROR_BUSY);return 0;}
        receiver.packet_free_attempted=1;
        SecureZeroMemory(receiver.packet,receiver.packet_size);
        if(HeapFree(GetProcessHeap(),0,receiver.packet)){receiver.packet=NULL;receiver.packet_size=0;}
        else {r_hold(GetLastError());okay=0;}
    }
    if(!r_hash_close())okay=0;
    if(!r_close_handle(&receiver.host_file,&receiver.host_close))okay=0;
    if(receiver.crypto&&!receiver.crypto_close) {
        receiver.crypto_close=1;
        if(CryptReleaseContext(receiver.crypto,0))receiver.crypto=0;else {r_hold(GetLastError());okay=0;}
    }
    if(receiver.crypto)okay=0;
    if(!r_close_handle(&receiver.image_file,&receiver.image_close))okay=0;
    if(receiver.inherited&&!receiver.parent_adopted){r_hold(ERROR_ACCESS_DENIED);okay=0;}
    else if(!r_close_handle(&receiver.inherited,&receiver.inherited_close))okay=0;
    if(receiver.io&&!P2PT_Destroy(&receiver.io))okay=0;
    if(okay&&!receiver.packet&&!receiver.hash&&!receiver.crypto&&!receiver.image_file&&!receiver.inherited&&!receiver.io) {
        InterlockedExchange(&receiver.held,0);InterlockedExchange(&receiver.phase,R_RETIRED);return 1;
    }
    return 0;
}

/* Included after hostgate.c. The only probe send is nested in the original
 * normal bdSocket::sendTo -> A531C0 -> HostGateSendTo call, while its real
 * object/cell remains borrowed. No socket HANDLE is stored in this state. */
typedef unsigned char (__attribute__((thiscall)) *ProdCreateFn)(void *,int,int,int);
typedef int (__attribute__((thiscall)) *ProdSendFn)(void *,void *,const void *,unsigned);
typedef unsigned char (__attribute__((thiscall)) *ProdCloseFn)(void *);
static unsigned char __attribute__((thiscall,noinline)) ProdSocketCreate(void *,int,int,int);
static unsigned char __attribute__((thiscall,noinline)) ProdSocketClose(void *);
static int __attribute__((thiscall,noinline)) ProdSocketSend(void *,void *,const void *,unsigned);
static struct {
    CRITICAL_SECTION lock;DWORD owner;void *object,*borrow_object;
    uint64_t epoch,generation;int ready,in_borrow,dead,job,result;
    unsigned char query[80],nonce[32],witness[112],probe[72],raw_bound[4];
    unsigned char destination[4];unsigned port,local_port;uint64_t deadline,expected_epoch;
} g_prodSocket;
typedef int (WINAPI *ProdCloseSocketFn)(SOCKET);
static ProdCloseSocketFn g_prodCloseSocket;
typedef int (WINAPI *ProdIoctlSocketFn)(SOCKET,long,u_long *);
static ProdIoctlSocketFn g_prodIoctlSocket;
static int WINAPI ProdIoctlSocket(SOCKET socket,long command,u_long *argument) {
    int result,error,valid;u_long requested=0;EnterCriticalSection(&g_prodSocket.lock);
    valid=argument&&Readable(argument,sizeof *argument);if(valid)requested=*argument;
    result=g_prodIoctlSocket(socket,command,argument);error=WSAGetLastError();
    if(command==FIONBIO&&g_prodSocket.object&&Readable(g_prodSocket.object,8)&&
       *(SOCKET *)((BYTE *)g_prodSocket.object+4)==socket&&
       (result==SOCKET_ERROR||!valid||requested!=1||!Readable(argument,sizeof *argument)||*argument!=1)) {
        g_prodSocket.dead=1;if(g_prodSocket.job)g_prodSocket.result=-1;
    }
    LeaveCriticalSection(&g_prodSocket.lock);WSASetLastError(error);return result;
}
static int WINAPI ProdCloseSocket(SOCKET socket) {
    int result,owned=0,error;EnterCriticalSection(&g_prodSocket.lock);
    /* A53580 is also called directly by destructors, so the shared import is
     * the actual lifetime boundary, not only the virtual close method. */
    if(g_prodSocket.object&&Readable(g_prodSocket.object,8)&&
       *(SOCKET *)((BYTE *)g_prodSocket.object+4)==socket) {
        owned=1;
        g_prodSocket.object=NULL;g_prodSocket.borrow_object=NULL;g_prodSocket.in_borrow=0;
        if(g_prodSocket.epoch==UINT64_MAX)g_prodSocket.dead=1;else ++g_prodSocket.epoch;
        if(g_prodSocket.job)g_prodSocket.result=-1;
    }
    result=g_prodCloseSocket(socket);error=WSAGetLastError();
    if(owned&&result==SOCKET_ERROR)g_prodSocket.dead=1;
    LeaveCriticalSection(&g_prodSocket.lock);WSASetLastError(error);return result;
}
static int ProdBytes(DWORD address,const unsigned char *bytes,unsigned size) {
    return Readable((void *)(DWORD_PTR)address,size)&&!memcmp((void *)(DWORD_PTR)address,bytes,size);
}
static int ProdSocketCode(void) {
    static const unsigned char send[]={0x55,0x8b,0xec,0x53,0x56,0x57,0x8b,0x7d,0x08};
    static const unsigned char cell[]={0x8b,0x56,0x04};
    static const unsigned char call[]={0xe8,0x02,0x8f,0xf3,0xff};
    static const unsigned char ioct[]={0xff,0x15,0x1c,0x25,0xb4,0x00};
    static const unsigned char closecell[]={0xc7,0x06,0xff,0xff,0xff,0xff};
    static const unsigned char sendiat[]={0xff,0x15,0x38,0x25,0xb4,0x00};
    static const unsigned char args[]={0x6a,0x01,0x6a,0x01,0x6a,0x00};
    static const unsigned char nonblock[]={0x33,0xc9,0x38,0x4d,0x08,0x8d,0x55,0xfc,0x0f,0x94,0xc1,0x52,0x68,0x7e,0x66,0x04,0x80,0x56,0x89,0x4d,0xfc,0xff,0x15,0x1c,0x25,0xb4,0x00,0x85,0xc0,0x74,0x03,0x83,0xce,0xff,0x8b,0xc6};
    static const unsigned char publish[]={0x89,0x46,0x04,0x5e,0xb0,0x01,0x5b,0x5d,0xc2,0x0c,0x00};
    static const unsigned char sendargs[]={0x6a,0x00,0x51,0x56,0x57,0xff,0x15,0x38,0x25,0xb4,0x00};
    return ProdBytes(0xB1A280,send,sizeof send)&&ProdBytes(0xB1A2B0,cell,sizeof cell)&&
        ProdBytes(0xB1A2B9,call,sizeof call)&&ProdBytes(0xA53035,ioct,sizeof ioct)&&
        ProdBytes(0xA5359F,closecell,sizeof closecell)&&ProdBytes(0xA53229,sendiat,sizeof sendiat)&&
        ProdBytes(0xA4D884,args,sizeof args)&&ProdBytes(0xA4D9FD,args,sizeof args)&&
        ProdBytes(0xA53020,nonblock,sizeof nonblock)&&ProdBytes(0xB1A201,publish,sizeof publish)&&
        ProdBytes(0xA53224,sendargs,sizeof sendargs);
}
static int ProdObject(void *object) {
    return object&&Readable(object,8)&&*(DWORD *)object==0xC244E0&&
        Readable((void *)(DWORD_PTR)0xC244E0,28)&&ProdSocketCode()&&
        *(DWORD *)(DWORD_PTR)0xC244E4==(DWORD)(DWORD_PTR)ProdSocketCreate&&
        *(DWORD *)(DWORD_PTR)0xC244F0==(DWORD)(DWORD_PTR)ProdSocketSend&&
        *(DWORD *)(DWORD_PTR)0xC244F8==(DWORD)(DWORD_PTR)ProdSocketClose&&
        Readable((void *)(DWORD_PTR)0xB4251C,4)&&*(DWORD *)(DWORD_PTR)0xB4251C==(DWORD)(DWORD_PTR)ProdIoctlSocket&&
        Readable((void *)(DWORD_PTR)0xB42528,4)&&*(DWORD *)(DWORD_PTR)0xB42528==(DWORD)(DWORD_PTR)ProdCloseSocket&&
        Readable((void *)(DWORD_PTR)HG_SLOT_SENDTO,4)&&*(DWORD *)(DWORD_PTR)HG_SLOT_SENDTO==(DWORD)(DWORD_PTR)HostGateSendTo&&
        Readable((void *)(DWORD_PTR)HG_SLOT_RECVFROM,4)&&*(DWORD *)(DWORD_PTR)HG_SLOT_RECVFROM==(DWORD)(DWORD_PTR)HostGateRecvFrom;
}
static unsigned char __attribute__((thiscall,noinline)) ProdSocketCreate(void *object,int blocking,int option,int third) {
    void *caller=__builtin_return_address(0);int result,error;
    EnterCriticalSection(&g_prodSocket.lock);
    result=((ProdCreateFn)(DWORD_PTR)0xB1A1B0)(object,blocking,option,third);
    error=WSAGetLastError();
    if(P2PProductionMode()>0&&GetCurrentThreadId()==g_prodSocket.owner&&
       (caller==(void *)(DWORD_PTR)0xA4D88F||caller==(void *)(DWORD_PTR)0xA4DA0A)&&
       blocking==0&&option==1&&third==1&&result&&ProdObject(object)&&*(SOCKET *)((BYTE *)object+4)!=INVALID_SOCKET) {
        if(g_prodSocket.epoch==UINT64_MAX)g_prodSocket.dead=1;
        else {++g_prodSocket.epoch;g_prodSocket.object=object;}
    }
    LeaveCriticalSection(&g_prodSocket.lock);WSASetLastError(error);return result;
}
static unsigned char __attribute__((thiscall,noinline)) ProdSocketClose(void *object) {
    int result,error,owned=0;EnterCriticalSection(&g_prodSocket.lock);
    if(object==g_prodSocket.object) {
        owned=1;
        g_prodSocket.object=NULL;g_prodSocket.borrow_object=NULL;g_prodSocket.in_borrow=0;
        if(g_prodSocket.epoch==UINT64_MAX)g_prodSocket.dead=1;else ++g_prodSocket.epoch;
        if(g_prodSocket.job)g_prodSocket.result=-1;
    }
    result=((ProdCloseFn)(DWORD_PTR)0xB1A370)(object);
    if(owned&&!result)g_prodSocket.dead=1;
    error=WSAGetLastError();LeaveCriticalSection(&g_prodSocket.lock);WSASetLastError(error);return result;
}
static int __attribute__((thiscall,noinline)) ProdSocketSend(void *object,void *address,const void *bytes,unsigned length) {
    int result,error;EnterCriticalSection(&g_prodSocket.lock);
    if(g_prodSocket.object==object&&GetCurrentThreadId()==g_prodSocket.owner&&!g_prodSocket.dead&&
       ProdObject(object)&&*(SOCKET *)((BYTE *)object+4)!=INVALID_SOCKET&&!g_prodSocket.in_borrow) {
        g_prodSocket.in_borrow=1;g_prodSocket.borrow_object=object;
    }
    /* The engine's original method runs exactly once with the original args. */
    result=((ProdSendFn)(DWORD_PTR)0xB1A280)(object,address,bytes,length);
    error=WSAGetLastError();
    g_prodSocket.borrow_object=NULL;g_prodSocket.in_borrow=0;
    LeaveCriticalSection(&g_prodSocket.lock);WSASetLastError(error);return result;
}
static int P2PProductionInstallSocketHooks(void) {
    DWORD *v=(DWORD *)(DWORD_PTR)0xC244E0;
    if(P2PProductionMode()==0)return 1;
    if(P2PProductionMode()<0||!ProdSocketCode()||!Readable(v,28)||v[1]!=0xB1A1B0||v[4]!=0xB1A280||v[6]!=0xB1A370)return 0;
    InitializeCriticalSection(&g_prodSocket.lock);g_prodSocket.owner=GetCurrentThreadId();
    FARPROC *slot=(FARPROC *)(DWORD_PTR)0xB42528,*ioctl_slot=(FARPROC *)(DWORD_PTR)0xB4251C;HMODULE ws=GetModuleHandleA("wsock32.dll"),ws2=GetModuleHandleA("ws2_32.dll");
    FARPROC want=ws?GetProcAddress(ws,"closesocket"):NULL,want2=ws2?GetProcAddress(ws2,"closesocket"):NULL;
    if(!Readable(slot,4)||!*slot||(*slot!=want&&*slot!=want2))return 0;
    FARPROC want_ioctl=ws?GetProcAddress(ws,"ioctlsocket"):NULL,want_ioctl2=ws2?GetProcAddress(ws2,"ioctlsocket"):NULL;
    if(!Readable(ioctl_slot,4)||!*ioctl_slot||(*ioctl_slot!=want_ioctl&&*ioctl_slot!=want_ioctl2))return 0;
    g_prodIoctlSocket=(ProdIoctlSocketFn)(void *)*ioctl_slot;FARPROC ioctl_hook=(FARPROC)ProdIoctlSocket;WriteCode(ioctl_slot,&ioctl_hook,4);
    g_prodCloseSocket=(ProdCloseSocketFn)(void *)*slot;FARPROC hook=(FARPROC)ProdCloseSocket;WriteCode(slot,&hook,4);
    DWORD create=(DWORD)(DWORD_PTR)ProdSocketCreate,send=(DWORD)(DWORD_PTR)ProdSocketSend,close=(DWORD)(DWORD_PTR)ProdSocketClose;
    WriteCode(v+1,&create,4);WriteCode(v+4,&send,4);WriteCode(v+6,&close,4);
    g_prodSocket.ready=v[1]==create&&v[4]==send&&v[6]==close&&*slot==hook&&*ioctl_slot==ioctl_hook;
    return g_prodSocket.ready;
}
static void ProdPut32(unsigned char *p,unsigned n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
static void ProdPut64(unsigned char *p,uint64_t n){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(n>>(i*8));}
static int ProdTuple(SOCKET socket,struct sockaddr_in *address) {
    int size=sizeof(*address),type=0,tsize=sizeof(type);memset(address,0,sizeof(*address));
    return getsockopt(socket,SOL_SOCKET,SO_TYPE,(char *)&type,&tsize)==0&&type==SOCK_DGRAM&&
        getsockname(socket,(struct sockaddr *)address,&size)==0&&size==sizeof(*address)&&
        address->sin_family==AF_INET&&address->sin_port!=0;
}
static void P2PProductionBorrowSend(SOCKET socket) {
    struct sockaddr_in before,after,to;uint64_t epoch,now;int result=-1,error=WSAGetLastError();
    if(!g_prodSocket.ready||!g_prodSocket.job||g_prodSocket.result||!g_prodSocket.in_borrow||
       GetCurrentThreadId()!=g_prodSocket.owner||g_prodSocket.borrow_object!=g_prodSocket.object||
       !ProdObject(g_prodSocket.object)||*(SOCKET *)((BYTE *)g_prodSocket.object+4)!=socket||
       !P2PR_CurrentPermission()){WSASetLastError(error);return;}
    epoch=g_prodSocket.epoch;now=P2PT_Now();
    if(!now||!ProdTuple(socket,&before))goto done;
    if(g_prodSocket.job==1) {
        memset(g_prodSocket.witness,0,112);memcpy(g_prodSocket.witness,"CP2PSW01",8);ProdPut32(g_prodSocket.witness+8,112);
        memcpy(g_prodSocket.witness+16,g_prodSocket.query+16,32);memcpy(g_prodSocket.witness+48,g_prodSocket.nonce,32);
        memcpy(g_prodSocket.witness+80,&before.sin_addr,4);ProdPut32(g_prodSocket.witness+84,ntohs(before.sin_port));
        ProdPut64(g_prodSocket.witness+88,now);ProdPut64(g_prodSocket.witness+96,epoch);ProdPut32(g_prodSocket.witness+104,g_prodSocket.owner);
        result=1;
    } else {
        if(g_prodSocket.expected_epoch!=epoch||memcmp(g_prodSocket.raw_bound,&before.sin_addr,4)||
           g_prodSocket.local_port!=ntohs(before.sin_port)||now>=g_prodSocket.deadline)goto done;
        memset(&to,0,sizeof to);to.sin_family=AF_INET;memcpy(&to.sin_addr,g_prodSocket.destination,4);to.sin_port=htons((u_short)g_prodSocket.port);
        result=g_hgSendTo(socket,(const char *)g_prodSocket.probe,72,0,(struct sockaddr *)&to,sizeof to)==72?1:-1;
    }
    if(!ProdObject(g_prodSocket.object)||g_prodSocket.object!=g_prodSocket.borrow_object||
       *(SOCKET *)((BYTE *)g_prodSocket.object+4)!=socket||g_prodSocket.epoch!=epoch||
       !ProdTuple(socket,&after)||memcmp(&before,&after,sizeof before)||!P2PR_CurrentPermission())result=-1;
done:g_prodSocket.result=result;WSASetLastError(error);
}
static int __cdecl ProdWitness(const void *query,unsigned length,const unsigned char nonce[32],uint64_t generation,void *out,unsigned cap) {
    int result;
    if(!g_prodSocket.ready||GetCurrentThreadId()!=g_prodSocket.owner||length!=80||cap!=112||!generation)return -1;
    EnterCriticalSection(&g_prodSocket.lock);
    if(!g_prodSocket.job) {
        memcpy(g_prodSocket.query,query,80);memcpy(g_prodSocket.nonce,nonce,32);g_prodSocket.generation=generation;
        g_prodSocket.result=0;g_prodSocket.job=1;
    } else if(g_prodSocket.job!=1||g_prodSocket.generation!=generation||memcmp(g_prodSocket.query,query,80)||memcmp(g_prodSocket.nonce,nonce,32)) {
        LeaveCriticalSection(&g_prodSocket.lock);return -1;
    }
    result=g_prodSocket.result;
    if(result){if(result==1)memcpy(out,g_prodSocket.witness,112);g_prodSocket.job=0;g_prodSocket.result=0;}
    LeaveCriticalSection(&g_prodSocket.lock);return result;
}
static int __cdecl ProdGuestProbe(const unsigned char ip[4],unsigned port,const void *probe,unsigned length,uint64_t deadline,
    const unsigned char raw[4],unsigned local_port,uint64_t epoch,uint64_t generation) {
    int result;uint64_t now=P2PT_Now();
    if(!g_prodSocket.ready||GetCurrentThreadId()!=g_prodSocket.owner||length!=72||!port||port>65535||
       !local_port||local_port>65535||!epoch||!generation||!now||deadline<=now||deadline-now>10000)return -1;
    EnterCriticalSection(&g_prodSocket.lock);
    if(!g_prodSocket.job) {
        memcpy(g_prodSocket.destination,ip,4);memcpy(g_prodSocket.probe,probe,72);memcpy(g_prodSocket.raw_bound,raw,4);
        g_prodSocket.port=port;g_prodSocket.local_port=local_port;g_prodSocket.deadline=deadline;
        g_prodSocket.expected_epoch=epoch;g_prodSocket.generation=generation;g_prodSocket.result=0;g_prodSocket.job=2;
    } else if(g_prodSocket.job!=2||g_prodSocket.generation!=generation||g_prodSocket.deadline!=deadline||
        g_prodSocket.expected_epoch!=epoch||g_prodSocket.port!=port||g_prodSocket.local_port!=local_port||
        memcmp(g_prodSocket.destination,ip,4)||memcmp(g_prodSocket.probe,probe,72)||memcmp(g_prodSocket.raw_bound,raw,4)) {
        LeaveCriticalSection(&g_prodSocket.lock);return -1;
    }
    result=g_prodSocket.result;if(result){g_prodSocket.job=0;g_prodSocket.result=0;}
    LeaveCriticalSection(&g_prodSocket.lock);return result;
}

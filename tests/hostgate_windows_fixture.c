/* Executes the real hostgate, including installation, in a disposable x86 process.
 * The private import table belongs to this fixture; no game image is loaded. */
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../production_profile.h"

static int write_number, fail_write;
static BYTE *test_table;
#define HG_SLOT_RECVFROM ((DWORD_PTR)test_table+0x24E0)
#define HG_SLOT_SENDTO ((DWORD_PTR)test_table+0x2538)
#define HG_SLOT_GETHOSTBYNAME ((DWORD_PTR)test_table+0x2530)
static void Log(const char *format,...) { (void)format; }
static BOOL Readable(const void *p, SIZE_T n)
{
    MEMORY_BASIC_INFORMATION m;
    return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect & (PAGE_NOACCESS|PAGE_GUARD)) &&
        (const char *)p+n <= (const char *)m.BaseAddress+m.RegionSize;
}
static void WriteCode(void *dst,const void *src,SIZE_T n)
{
    if (++write_number!=fail_write) memcpy(dst,src,n);
}
static void P2PTrim(char *s)
{
    size_t n=strlen(s);
    while (n && (s[n-1]=='\r' || s[n-1]=='\n' || s[n-1]==' ')) s[--n]=0;
}
/* Do not resolve configured hosts or start a background thread in this fixture. */
static HANDLE TestThread(LPSECURITY_ATTRIBUTES a,SIZE_T b,LPTHREAD_START_ROUTINE c,
                         LPVOID d,DWORD e,LPDWORD f)
{ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return NULL; }
#define CreateThread TestThread
#include "../hostgate.c"
#undef CreateThread

static int receives,sends,consume,allow_at;
static int WINAPI FakeRecv(SOCKET s,char *buf,int len,int flags,struct sockaddr *from,int *size)
{
    (void)s;
    struct sockaddr_in a;
    memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(17760);
    a.sin_addr.s_addr=inet_addr(allow_at && receives>=allow_at ? "127.0.0.1":"192.0.2.55");
    ++receives;if (!(flags&MSG_PEEK)) ++consume;
    assert(*size >= (int)sizeof(a));memcpy(from,&a,sizeof(a));*size=sizeof(a);
    if (len) buf[0]='x';
    return len ? 1:0;
}
static int WINAPI FakeSend(SOCKET s,const char *buf,int len,int flags,const struct sockaddr *to,int size)
{ (void)s;(void)buf;(void)flags;(void)to;(void)size;++sends;return len; }
static int WINAPI ForeignRecv(SOCKET s,char *b,int n,int f,struct sockaddr *a,int *l)
{ (void)s;(void)b;(void)n;(void)f;(void)a;(void)l;return SOCKET_ERROR; }

static void InstallTest(const char *test)
{
    if (!strcmp(test,"unmapped")) { assert(!InstallHostGate());return; }
    test_table=VirtualAlloc(NULL,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(test_table);
    HMODULE w1=LoadLibraryA("wsock32.dll"),w2=LoadLibraryA("ws2_32.dll");
    assert(w1 && w2);
    FARPROC *r=(FARPROC *)HG_SLOT_RECVFROM,*s=(FARPROC *)HG_SLOT_SENDTO,*h=(FARPROC *)HG_SLOT_GETHOSTBYNAME;
    *r=GetProcAddress(w2,"recvfrom");*s=GetProcAddress(w1,"sendto");*h=GetProcAddress(w1,"gethostbyname");
    if (!strcmp(test,"missing")) *r=NULL;
    if (!strcmp(test,"foreign_recv")) *r=(FARPROC)ForeignRecv;
    if (!strcmp(test,"foreign_send")) *s=(FARPROC)ForeignRecv;
    if (!strcmp(test,"foreign_resolver")) *h=(FARPROC)ForeignRecv;
    if (!strncmp(test,"write",5)) fail_write=atoi(test+5);
    if (!strcmp(test,"good")) {
        assert(InstallHostGate());
        assert(*r==(FARPROC)HostGateRecvFrom && *s==(FARPROC)HostGateSendTo && *h==(FARPROC)HostGateGetHostByName);
#if CODOL_LOCAL_TESTING
        assert(g_hgMode==1);
#else
        assert(g_hgMode==2);
#endif
    } else assert(!InstallHostGate());
}
static void PacketTests(void)
{
    InitializeCriticalSection(&g_hgLock);
    g_hgRecvFrom=FakeRecv;g_hgSendTo=FakeSend;
    assert(HostGateOk(inet_addr("127.2.3.4")));
    assert(!HostGateOk(inet_addr("192.0.2.55")));
    HostGateVouch(inet_addr("192.0.2.56"));
    assert(HostGateOk(inet_addr("192.0.2.56")));
    struct sockaddr_in address;memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;address.sin_port=htons(17760);address.sin_addr.s_addr=inet_addr("192.0.2.55");
    assert(HostGateSendTo(INVALID_SOCKET,"x",1,0,(struct sockaddr *)&address,sizeof(address))==1 && sends==0);
    address.sin_addr.s_addr=inet_addr("127.0.0.1");
    assert(HostGateSendTo(INVALID_SOCKET,"x",1,0,(struct sockaddr *)&address,sizeof(address))==1 && sends==1);
    /* A real loopback socket supplies readable queue state to the drain loop. */
    SOCKET receiver=socket(AF_INET,SOCK_DGRAM,0),sender=socket(AF_INET,SOCK_DGRAM,0);
    assert(receiver!=INVALID_SOCKET && sender!=INVALID_SOCKET);
    address.sin_port=0;assert(!bind(receiver,(struct sockaddr *)&address,sizeof(address)));
    int alen=sizeof(address);assert(!getsockname(receiver,(struct sockaddr *)&address,&alen));
    assert(sendto(sender,"x",1,0,(struct sockaddr *)&address,sizeof(address))==1);
    fd_set rd;FD_ZERO(&rd);FD_SET(receiver,&rd);struct timeval tv={1,0};
    assert(select(0,&rd,NULL,NULL,&tv)==1);
    char buf[16];struct sockaddr_storage source;int size=sizeof(source);
    allow_at=1;
    assert(HostGateRecvFrom(receiver,buf,sizeof(buf),0,(struct sockaddr *)&source,&size)==1);
    assert(receives==2 && consume==2);
#if CODOL_DIAGNOSTICS
    assert(g_hgDropsIn==1);
#endif
    receives=consume=0;allow_at=2;size=sizeof(source);
    assert(HostGateRecvFrom(receiver,buf,sizeof(buf),MSG_PEEK,(struct sockaddr *)&source,&size)==1);
    assert(receives==3 && consume==1);
    receives=consume=0;allow_at=0;size=sizeof(source);
    assert(HostGateRecvFrom(receiver,buf,sizeof(buf),0,(struct sockaddr *)&source,&size)==SOCKET_ERROR);
    assert(WSAGetLastError()==WSAEWOULDBLOCK && receives==64);
    g_hgMode=1;receives=0;size=sizeof(source);
    assert(HostGateRecvFrom(receiver,buf,sizeof(buf),0,(struct sockaddr *)&source,&size)==1 && receives==1);
    closesocket(sender);closesocket(receiver);
}
int main(int argc,char **argv)
{
    WSADATA w;assert(!WSAStartup(MAKEWORD(2,2),&w));assert(argc>=2);
    if (!strcmp(argv[1],"packets")) PacketTests();else InstallTest(argv[1]);
    puts("hostgate fixture passed");WSACleanup();return 0;
}

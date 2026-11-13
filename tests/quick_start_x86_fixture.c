/* Offline x86 fixture. Windows boundaries are explicit deterministic doubles;
 * production hook, snapshot, policy and installation bodies are unchanged. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
__declspec(dllexport) DWORD qtest_now, qtest_protect_calls, qtest_fail_protect,
 qtest_flush_calls, qtest_fail_flush, qtest_freed, qtest_terminated;
__declspec(dllexport) BYTE qtest_arena[64];
void *memcpy(void *d,const void *s,size_t n) {BYTE *a=d;const BYTE *b=s;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
void *memset(void *d,int v,size_t n) {BYTE *a=d;for(size_t i=0;i<n;i++)a[i]=(BYTE)v;return d;}
int memcmp(const void *a,const void *b,size_t n) {const BYTE *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return x[i]-y[i];return 0;}
int strcmp(const char *a,const char *b) {while(*a && *a==*b){a++;b++;}return (BYTE)*a-(BYTE)*b;}
static BOOL Readable(const void *p,SIZE_T n) {
 uintptr_t v=(uintptr_t)p;
 return (v>=0x400000 && v+n<=0xd00000) ||
        (v>=0x6000000 && v+n<=0x8400000);
}
static void Log(const char *format,...) {(void)format;}
static void *TAlloc(void *a,SIZE_T s,DWORD t,DWORD p) {(void)a;(void)s;(void)t;(void)p;return qtest_arena;}
static BOOL TFree(void *a,SIZE_T s,DWORD t) {(void)a;(void)s;(void)t;qtest_freed++;return TRUE;}
static BOOL TProtect(void *a,SIZE_T s,DWORD p,DWORD *old) {
 (void)a;(void)s;(void)p;*old=PAGE_EXECUTE_READ;qtest_protect_calls++;
 return qtest_protect_calls!=qtest_fail_protect;
}
static BOOL TFlush(HANDLE h,const void *p,SIZE_T s) {(void)h;(void)p;(void)s;qtest_flush_calls++;return qtest_flush_calls!=qtest_fail_flush;}
static void TLock(CRITICAL_SECTION *s) {(void)s;}
static DWORD TTime(void) {return qtest_now;}
static HANDLE TProcess(void) {return (HANDLE)1;}
static BOOL TTerminate(HANDLE h,UINT c) {(void)h;(void)c;qtest_terminated++;return TRUE;}
#define VirtualAlloc TAlloc
#define VirtualFree TFree
#define VirtualProtect TProtect
#define FlushInstructionCache TFlush
#define InitializeCriticalSection TLock
#define DeleteCriticalSection TLock
#define EnterCriticalSection TLock
#define LeaveCriticalSection TLock
#define GetTickCount TTime
#define GetCurrentProcess TProcess
#define TerminateProcess TTerminate
#include "../src/rooms/quick_start_gate.c"
__declspec(dllexport) void QsgTestInstall(void) {PatchQuickStartGate();}
__declspec(dllexport) int QsgTestAllow(void *p) {return QsgAllow(p);}
__declspec(dllexport) void QsgTestTick(void *p) {QsgTick(p);}
__declspec(dllexport) unsigned QsgTestState(void) {return g_qsgPolicy.state;}
__declspec(dllexport) unsigned QsgTestBorn(void) {return g_qsgPolicy.born;}
__declspec(dllexport) void QsgTestBridge(void *p) {QsgBridgeHook((void *)0x1234,NULL,p);}
__declspec(dllexport) void QsgTestFrame(void *p) {QsgFrameHook(p,NULL);}
__declspec(dllexport) DWORD qtest_reentrant_calls;
static void __attribute__((thiscall,noinline)) TReenter(void *self,const void *params) {
 qtest_reentrant_calls++;
 if(qtest_reentrant_calls==1) {qtest_now+=1;QsgBridgeHook(self,NULL,params);}
}
__declspec(dllexport) void QsgTestReentrant(void) {g_qsgBridge=TReenter;QsgBridgeHook((void *)0x1234,NULL,NULL);}

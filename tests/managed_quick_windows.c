#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "local_multi_policy.h"
static BOOL Readable(const void *p,SIZE_T n) {
 MEMORY_BASIC_INFORMATION m;
 return VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
  !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) && (const BYTE *)p+n<=(const BYTE *)m.BaseAddress+m.RegionSize;
}
static void Log(const char *format,...) {(void)format;}
#include "managed_quick.c"
static unsigned checks,failures;
#define CHECK(x) do { checks++;if(!(x)){failures++;printf("FAIL line %u: %s\n",__LINE__,#x);} } while(0)
static unsigned calls;static void *seenRoom;static BYTE seen[0x48];
/* Reserve the exact native instruction addresses as an inert section of this
 * dedicated test image, before Windows chooses its process heap addresses. */
__attribute__((section(".qa"),used)) static BYTE testNative[0x20000];
static void __attribute__((thiscall,noinline)) Capture(void *room,const void *info) {
 calls++;seenRoom=room;memcpy(seen,info,sizeof(seen));
}
static void *map(DWORD address,SIZE_T size) {
 if(address>=0x700000 && (uintptr_t)address+size<=0x720000) {
  DWORD previous;CHECK((uintptr_t)testNative==0x700000);
  CHECK(VirtualProtect((void *)(uintptr_t)address,size,PAGE_EXECUTE_READWRITE,&previous));
  return (void *)(uintptr_t)address;
 }
 void *p=VirtualAlloc((void *)(uintptr_t)address,size,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
 if(p!=(void *)(uintptr_t)address) {
  MEMORY_BASIC_INFORMATION m;VirtualQuery((void *)(uintptr_t)address,&m,sizeof(m));
  printf("MAPFAIL %08lx error=%lu state=%lx allocation=%p size=%lu image=%p\n",(unsigned long)address,(unsigned long)GetLastError(),(unsigned long)m.State,m.AllocationBase,(unsigned long)m.RegionSize,GetModuleHandleA(NULL));
 }
 CHECK(p==(void *)(uintptr_t)address);return p;
}
static void *callerTarget;
static ManagedRoomInfoFn caller(DWORD returnAddress) {
 BYTE *at=(BYTE *)(uintptr_t)(returnAddress-9);
 const BYTE push[]={0xff,0x74,0x24,0x04};memcpy(at,push,4);at[4]=0xe8;
 LONG rel=(LONG)((uintptr_t)(callerTarget?callerTarget:(void *)ManagedRoomInfoHook)-returnAddress);memcpy(at+5,&rel,4);
 at[9]=0xc2;at[10]=4;at[11]=0;FlushInstructionCache(GetCurrentProcess(),at,12);
 return (ManagedRoomInfoFn)(void *)at;
}
int main(void) {
 unsigned char info[0x48],original[0x48];
 for(unsigned i=0;i<sizeof(info);i++)info[i]=(BYTE)(i+7);
 info[0x0c]=11;info[0x0d]=2;info[0x0e]=12;info[0x28]=1;info[0x29]=info[0x2a]=1;
 CHECK(CodolManagedQuickOpen(0x71b121,1,1,1,1,1,info));
 CHECK(CodolManagedQuickOpen(0x71b312,1,1,1,1,1,info));
 CHECK(!CodolManagedQuickOpen(0x71b121,1,1,1,1,1,NULL));
 CHECK(!CodolManagedQuickOpen(0x71b122,1,1,1,1,1,info));
 CHECK(!CodolManagedQuickOpen(0x71b121,0,1,1,1,1,info));
 CHECK(!CodolManagedQuickOpen(0x71b121,1,0,1,1,1,info));
 CHECK(!CodolManagedQuickOpen(0x71b121,1,1,0,1,1,info));
 CHECK(!CodolManagedQuickOpen(0x71b121,1,1,1,2,1,info));
 CHECK(!CodolManagedQuickOpen(0x71b121,1,1,1,1,0,info));
 const unsigned offsets[]={0xc,0xd,0xe,0x28};
 for(unsigned i=0;i<4;i++) { info[offsets[i]]++;CHECK(!CodolManagedQuickOpen(0x71b121,1,1,1,1,1,info));info[offsets[i]]--; }
 memcpy(original,info,sizeof(info));CodolManagedQuickNormalize(info);
 for(unsigned i=0;i<sizeof(info);i++)CHECK(info[i]==(i==0x29||i==0x2a?0:original[i]));
 CHECK(map(0x082d0000,0x20000)!=NULL);CHECK(map(0x06c40000,0x10000)!=NULL);CHECK(map(0x00710000,0x10000)!=NULL);
 if(failures)return 1;
 BYTE *room=(BYTE *)(uintptr_t)MANAGED_MAIN_ROOM_VA;room[0x44f8]=1;
 DWORD *controller=VirtualAlloc(NULL,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(controller!=NULL);
 controller[0]=MANAGED_QUICK_VTABLE;controller[0xec/4]=1;
 *(DWORD *)(uintptr_t)MANAGED_QUICK_PTR_VA=(DWORD)(uintptr_t)controller;
 const char *mode="playlist";memcpy(info,&mode,4);info[0x29]=info[0x2a]=1;memcpy(original,info,sizeof(info));
 g_managedRoomInfo=Capture;
 for(unsigned i=0;i<2;i++) {
  ManagedRoomInfoFn fn=caller(i?0x71b312:0x71b121);
  DWORD before,after;__asm__ volatile("movl %%esp,%0":"=r"(before));fn(room,info);__asm__ volatile("movl %%esp,%0":"=r"(after));
  CHECK(before==after);CHECK(seenRoom==room);CHECK(calls==i+1);CHECK(seen[0x29]==0 && seen[0x2a]==0);CHECK(memcmp(info,original,sizeof(info))==0);
  for(unsigned j=0;j<sizeof(info);j++)CHECK(seen[j]==(j==0x29||j==0x2a?0:info[j]));
 }
 ManagedRoomInfoFn yes=caller(0x71b121);
 room[0x44f8]=0;yes(room,info);CHECK(memcmp(seen,info,sizeof(info))==0);room[0x44f8]=1;
 controller[0xec/4]=2;yes(room,info);CHECK(memcmp(seen,info,sizeof(info))==0);controller[0xec/4]=1;
 controller[0]=0;yes(room,info);CHECK(memcmp(seen,info,sizeof(info))==0);controller[0]=MANAGED_QUICK_VTABLE;
 mode="zombies";memcpy(info,&mode,4);yes(room,info);CHECK(memcmp(seen,info,sizeof(info))==0);
 mode="playlist";memcpy(info,&mode,4);info[0xe]=4;yes(room,info);CHECK(memcmp(seen,info,sizeof(info))==0);info[0xe]=12;
 caller(0x71b130)(room,info);CHECK(memcmp(seen,info,sizeof(info))==0);
 CHECK(map(0x00700000,0x10000)!=NULL);
 BYTE *entry=(BYTE *)(uintptr_t)MANAGED_ROOM_INFO_VA;
 const BYTE prologue[]={0x55,0x8b,0xec,0x83,0xec,0x10};memcpy(entry,prologue,6);
 const BYTE epilogue[]={0x89,0xec,0x5d,0xe9};memcpy(entry+6,epilogue,4);
 LONG captureJump=(LONG)((uintptr_t)Capture-(uintptr_t)(entry+14));memcpy(entry+10,&captureJump,4);
 const BYTE accessor[]={0x8d,0x81,0x80,0x5a,0x01,0,0xc3};memcpy((void *)(uintptr_t)0x70ea40,accessor,7);
 const BYTE first[]={0x66,0xc7,0x44,0x24,0x41,1,1};memcpy((void *)(uintptr_t)0x71b109,first,7);
 const BYTE second[]={0xc6,0x44,0x24,0x41,1};memcpy((void *)(uintptr_t)0x71b2fc,second,5);
 const DWORD guards[]={MANAGED_ROOM_INFO_VA,0x70ea40,0x71b109,0x71b2fc};
 for(unsigned i=0;i<4;i++) {
  BYTE *at=(BYTE *)(uintptr_t)guards[i];*at^=1;g_managedRoomInfo=NULL;
  PatchManagedQuickJoin();CHECK(g_managedRoomInfo==NULL);*at^=1;CHECK(memcmp(entry,prologue,6)==0);
 }
 PatchManagedQuickJoin();CHECK(g_managedRoomInfo!=NULL);CHECK(entry[0]==0xe9 && entry[5]==0x90);
 MEMORY_BASIC_INFORMATION protect;VirtualQuery((void *)g_managedRoomInfo,&protect,sizeof(protect));CHECK(protect.Protect==PAGE_EXECUTE_READ);
 callerTarget=entry;
 for(unsigned i=0;i<2;i++) {
  ManagedRoomInfoFn fn=caller(i?0x71b312:0x71b121);DWORD before,after;
  __asm__ volatile("movl %%esp,%0":"=r"(before));fn(room,info);__asm__ volatile("movl %%esp,%0":"=r"(after));
  CHECK(before==after);CHECK(seenRoom==room);CHECK(seen[0x29]==0 && seen[0x2a]==0);CHECK(info[0x29]==1 && info[0x2a]==1);
 }
 caller(0x71b130)(room,info);CHECK(memcmp(seen,info,sizeof(info))==0);
 const char *valid="127.0.0.1\r\n23174\r\n127.0.0.1\r\n23174\r\n1002\r\n";
 CHECK(CodolLocalMultiRoute(valid,strlen(valid)));
 const char *actual="127.0.0.1\r\n23174\r\n127.0.0.1\r\n23174\r\n3ea\r\n";
 CHECK(CodolLocalMultiRoute(actual,strlen(actual)));
 const char *isolated="127.0.0.3\r\n3074\r\n127.0.0.3\r\n3074\r\n3ea\r\n";
 CHECK(CodolLocalMultiRoute(isolated,strlen(isolated)));
 const char *isolatedLF="127.0.0.3\n3074\n127.0.0.3\n3074\n3ea\n";
 CHECK(CodolLocalMultiRoute(isolatedLF,strlen(isolatedLF)));
 CHECK(CodolLocalTitle("3EA"));CHECK(!CodolLocalTitle("3ez"));CHECK(!CodolLocalTitle("000"));CHECK(!CodolLocalTitle("123456789"));
 const char *invalid[]={"localhost\n23174\n127.0.0.1\n23174\n1002\n","127.0.0.1\n23174\n8.8.8.8\n23174\n1002\n","127.0.0.1\n23174\n127.0.0.1\n23175\n1002\n","127.0.0.1\n0\n127.0.0.1\n0\n1002\n","127.0.0.1\n65536\n127.0.0.1\n65536\n1002\n","127.0.0.1.example\n23174\n127.0.0.1\n23174\n1002\n","127.0.0.1\n23174\n127.0.0.1\n23174\n1002\nextra\n"};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)CHECK(!CodolLocalMultiRoute(invalid[i],strlen(invalid[i])));
 const char *isolatedInvalid[]={
  "127.0.0.1\n3074\n127.0.0.3\n3074\n3ea\n",
  "127.0.0.3\n3074\n127.0.0.1\n3074\n3ea\n",
  "127.0.0.2\n3074\n127.0.0.2\n3074\n3ea\n",
  "127.0.0.4\n3074\n127.0.0.4\n3074\n3ea\n",
  "127.0.0.3.example\n3074\n127.0.0.3.example\n3074\n3ea\n",
  "127.0.0.30\n3074\n127.0.0.30\n3074\n3ea\n",
  "127.000.000.003\n3074\n127.000.000.003\n3074\n3ea\n",
  "2130706435\n3074\n2130706435\n3074\n3ea\n",
  "localhost\n3074\nlocalhost\n3074\n3ea\n",
  "0.0.0.0\n3074\n0.0.0.0\n3074\n3ea\n",
  "127.0.0.3\n3074\n127.0.0.3\n3075\n3ea\n",
  "127.0.0.3\n0\n127.0.0.3\n0\n3ea\n",
  "127.0.0.3\n3074\n127.0.0.3\n3074\n3ea\nextra\n"};
 for(unsigned i=0;i<sizeof(isolatedInvalid)/sizeof(isolatedInvalid[0]);i++)
  CHECK(!CodolLocalMultiRoute(isolatedInvalid[i],strlen(isolatedInvalid[i])));
 const wchar_t *args[]={L"game",L"+set",L"codol_local_test_multi",L"1",L"+set",L"codol_local_test_multi",L"0"};
 CHECK(CodolLocalMultiArgument(4,args));CHECK(!CodolLocalMultiArgument(7,args));CHECK(!CodolLocalMultiArgument(3,args));CHECK(!CodolLocalMultiArgument(1,args));
 printf("{\"checks\":%u,\"failures\":%u,\"actual_x86_windows\":true,\"live_client\":false}\n",checks,failures);
 return failures!=0;
}

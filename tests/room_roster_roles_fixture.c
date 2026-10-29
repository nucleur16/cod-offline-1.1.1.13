#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static BYTE test_room[0x15b68],test_cache[0x1000],test_info[0x20];
static uintptr_t denied;
static uint64_t identity;
static unsigned roles,prunes;
static unsigned saved_esp __attribute__((used)),flags_before __attribute__((used)),flags_at_prune __attribute__((used));
static unsigned wrong_abi __attribute__((used));
static void Log(const char *format,...){(void)format;}
static int Readable(const void *p,size_t n){return p&&n&&(uintptr_t)p!=denied;}
static void put(void *p,uint32_t v){memcpy(p,&v,4);}
static void uid(void *p,uint64_t v){memcpy(p,&v,8);}
static void test_role(BYTE *member,BYTE *room){
 (void)room;uint64_t m=0,h=0;memcpy(&m,member,8);memcpy(&h,test_cache+0x20,8);member[0x8b]=m==h;++roles;
}
#define CMR_ROOM ((uintptr_t)test_room)
#define CMR_IDENTITY() identity
#ifndef CMR_NATIVE_ROLE
#define CMR_ROLE(member,room) test_role(member,room)
#endif
#include "../room_roster_roles.c"
static void __attribute__((stdcall)) fake_prune(void *room){(void)room;++prunes;}
static void __attribute__((naked,used)) abi_prune(void){
 __asm__ __volatile__("pushfl\n\tpopl _flags_at_prune\n\t"
  "cmpl $0x1234abcd,%eax\n\tjne 1f\n\tcmpl $0x3456abcd,%ecx\n\tjne 1f\n\tcmpl $0x4567abcd,%edx\n\tjne 1f\n\tjmp 2f\n"
  "1: incl _wrong_abi\n"
  "2: incl _prunes\n\tret $4\n");
}
static int __attribute__((naked,used)) invoke(void *object){
 __asm__ __volatile__("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
  "movl $0x11223344,%ebx\n\tmovl $0x55667788,%esi\n\tmovl $0x66778899,%edi\n\tmovl %esp,_saved_esp\n\t"
  "xorl %eax,%eax\n\tcmpl $0,%eax\n\tpushfl\n\tpopl _flags_before\n\t"
  "movl $0x1234abcd,%eax\n\tmovl $0x3456abcd,%ecx\n\tmovl $0x4567abcd,%edx\n\t"
  "pushl 8(%ebp)\n\tcall _CmrMemberListHook\n\t"
  "cmpl _saved_esp,%esp\n\tjne 1f\n\tcmpl $0x11223344,%ebx\n\tjne 1f\n\t"
  "cmpl $0x55667788,%esi\n\tjne 1f\n\tcmpl $0x66778899,%edi\n\tjne 1f\n\tmovl $1,%eax\n\tjmp 2f\n"
  "1: xorl %eax,%eax\n"
  "2: popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret\n");
}
static void reset(void){
 memset(test_room,0,sizeof(test_room));memset(test_cache,0,sizeof(test_cache));memset(test_info,0,sizeof(test_info));
 identity=0xd11078486f3c7b38ULL;roles=prunes=wrong_abi=0;denied=0;
 put(test_room,0xc7d66c);put(test_room+0x10,(uintptr_t)test_cache);test_room[0x4501]=1;
 test_cache[0]=test_cache[1]=test_cache[2]=1;uid(test_cache+0x10,69429);put(test_cache+0x18,2);uid(test_cache+0x20,0xfe7e87218aea4005ULL);
 put(test_room+0x15a70,2);uid(test_room+0x13f20,0xfe7e87218aea4005ULL);uid(test_room+0x14090,identity);
 uid(test_cache+0x38,0xfe7e87218aea4005ULL);uid(test_cache+0x90,identity);
 uid(test_info+8,69429);uid(test_info+0x18,0xfe7e87218aea4005ULL);
 g_cmrPrune=(void *)fake_prune;
}
int main(void){
 reset();g_cmrPrune=(void *)abi_prune;
 if(!invoke(test_room)||roles!=2||prunes!=1||wrong_abi||flags_before!=flags_at_prune||test_room[0x13fab]!=1||test_room[0x1411b]!=0)return 1;
 reset();CmrAfterSdk(test_info);if(roles!=2||prunes!=1||test_room[0x13fab]!=1)return 2;
 for(unsigned test=0;test<16;test++){
  reset();void *object=test_room;
  switch(test){
   case 0:object=test_room+4;break;
   case 1:test_room[0x4501]=0;break;
   case 2:test_room[0x4503]=1;break;
   case 3:put(test_room,0);break;
   case 4:put(test_room+0x44fc,3);break;
   case 5:uid(test_room+0x15b20,1);break;
   case 6:test_cache[2]=0;break;
   case 7:uid(test_cache+0x10,0);break;
   case 8:uid(test_cache+0x20,0);break;
   case 9:identity=0;break;
   case 10:put(test_room+0x15a70,19);break;
   case 11:put(test_cache+0x18,1);break;
   case 12:uid(test_room+0x14090,0xfe7e87218aea4005ULL);break;
   case 13:uid(test_cache+0x90,42);break;
   case 14:denied=(uintptr_t)test_room;break;
   case 15:denied=(uintptr_t)test_cache;break;
  }
  BYTE before[sizeof(test_room)],cache[sizeof(test_cache)];memcpy(before,test_room,sizeof(before));memcpy(cache,test_cache,sizeof(cache));
  if(CmrRefresh(object)||roles||memcmp(before,test_room,sizeof(before))||memcmp(cache,test_cache,sizeof(cache)))return 10+test;
 }
 reset();uid(test_info+8,69430);CmrAfterSdk(test_info);if(roles||prunes)return 30;
 reset();uid(test_info+0x18,identity);CmrAfterSdk(test_info);if(roles||prunes)return 31;
 puts("PASS: derived roles, SDK completion, ret4/register/flags ABI and eighteen no-change boundaries");return 0;
}

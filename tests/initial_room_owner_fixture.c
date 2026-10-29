/* Actual compiled adapter and its nonstandard ESI/ret12 callback ABI. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static BYTE test_room[0x15b68],test_cache[0x28],test_info[0x20],test_member[0x10];
static uintptr_t denied;
static uint64_t identity;
static unsigned original_calls __attribute__((used));
static unsigned saved_esp __attribute__((used)),flags_before __attribute__((used)),flags_after __attribute__((used));
static void Log(const char *fmt,...){(void)fmt;}
static int Readable(const void *p,size_t n){return p && n && (uintptr_t)p!=denied;}
#define CIO_ROOM ((uintptr_t)test_room)
#define CIO_IDENTITY() identity
#include "../initial_room_owner.c"
static void __attribute__((naked,used)) original_callback(void)
{
 __asm__ __volatile__("incl _original_calls\n\tret $12\n");
}
static int __attribute__((naked,used)) invoke(void *object,const void *info,const void *member,unsigned joined)
{
 __asm__ __volatile__(
  "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
  "movl 8(%ebp),%esi\n\tmovl $0x11223344,%ebx\n\tmovl $0x55667788,%edi\n\t"
  "movl %esp,_saved_esp\n\txorl %eax,%eax\n\tcmpl $0,%eax\n\tmovl $0x1234abcd,%eax\n\t"
  "pushfl\n\tpopl _flags_before\n\tpushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\t"
  "call _CioCallbackHook\n\tpushfl\n\tpopl _flags_after\n\t"
  "cmpl $0x1234abcd,%eax\n\tjne 1f\n\tcmpl _saved_esp,%esp\n\tjne 1f\n\t"
  "cmpl $0x11223344,%ebx\n\tjne 1f\n\tcmpl $0x55667788,%edi\n\tjne 1f\n\t"
  "cmpl 8(%ebp),%esi\n\tjne 1f\n\tmovl $1,%eax\n\tjmp 2f\n"
  "1: xorl %eax,%eax\n"
  "2: popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret\n");
}
static void put(void *p,uint32_t value){memcpy(p,&value,4);}
static void uid(void *p,uint64_t value){memcpy(p,&value,8);}
static void reset(void)
{
 memset(test_room,0,sizeof(test_room));memset(test_cache,0,sizeof(test_cache));
 memset(test_info,0,sizeof(test_info));memset(test_member,0,sizeof(test_member));
 denied=0;identity=0xd11078486f3c7b38ULL;original_calls=0;
 put(test_room,0xc7d66c);put(test_room+0x10,(uintptr_t)test_cache);test_room[0x4501]=1;
 test_cache[0]=test_cache[1]=test_cache[2]=1;uid(test_cache+0x10,69422);
 uid(test_info+8,69422);uid(test_info+0x18,0xfe7e87218aea4005ULL);uid(test_member+8,identity);
 put(test_room+0x15a70,2);uid(test_room+0x13f20,identity);uid(test_room+0x14090,0xfe7e87218aea4005ULL);
 g_cioCallback=(void *)original_callback;
}
int main(void)
{
 static BYTE before[sizeof(test_room)],cache_before[sizeof(test_cache)];
 reset();memcpy(before,test_room,sizeof(before));memcpy(cache_before,test_cache,sizeof(cache_before));
 if(!invoke(test_room,test_info,test_member,1)||original_calls||memcmp(before,test_room,sizeof(before))||
    memcmp(cache_before,test_cache,sizeof(cache_before))||flags_before!=flags_after)return 1;
 for(unsigned test=0;test<19;test++) {
  reset();unsigned joined=1;void *object=test_room;
  switch(test){
   case 0:joined=0;break;
   case 1:joined=2;break;
   case 2:test_room[0x44f8]=1;break;
   case 3:test_room[0x4501]=0;break;
   case 4:test_room[0x4503]=1;break;
   case 5:put(test_room+0x44fc,4);break;
   case 6:uid(test_room+0x15b20,0xfe7e87218aea4005ULL);break;
   case 7:uid(test_cache+0x20,0xfe7e87218aea4005ULL);break;
   case 8:uid(test_info+8,69423);break;
   case 9:uid(test_info+0x18,identity);break;
   case 10:uid(test_member+8,0xfe7e87218aea4005ULL);break;
   case 11:test_cache[2]=0;break;
   case 12:identity=0;break;
   case 13:uid(test_room+0x14090,111);break;
   case 14:uid(test_room+0x14090,identity);break;
   case 15:put(test_room+0x15a70,19);break;
   case 16:denied=(uintptr_t)test_info;break;
   case 17:denied=(uintptr_t)test_cache;break;
   case 18:object=test_room+4;break;
  }
  memcpy(before,test_room,sizeof(before));memcpy(cache_before,test_cache,sizeof(cache_before));
  if(!invoke(object,test_info,test_member,joined)||original_calls!=1||memcmp(before,test_room,sizeof(before))||
     memcmp(cache_before,test_cache,sizeof(cache_before)))return 10+test;
 }
 puts("PASS: compiled initial-owner callback, ESI/ret12 ABI and flags; nineteen original-path boundaries; room and SDK cache read-only");
 return 0;
}

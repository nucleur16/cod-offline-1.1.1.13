/* Execute the shipping adapter. Transport is a captured-call boundary. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static BYTE room[0x15b68],cache[0x28];
static BYTE pool[4*0x3778];
static uint64_t identity=123;
static int state,events[8],count,failures;
static uintptr_t denied;
static void Log(const char *fmt,...){(void)fmt;}
static int Readable(const void *p,size_t n){return p && n && (uintptr_t)p!=denied;}
static void send_info(void *object,const uint32_t *message){
 if(object!=room || message[0]!=0xc7d8d8u || message[1]!=15 || message[2]!=(uintptr_t)(room+0x15a80))failures++;
 events[count++]=15;
}
#define CTP_ROOM ((uintptr_t)room)
#define CTP_POOL ((uintptr_t)pool)
#define CTP_IDENTITY() identity
#define CTP_STATE() state
#define CTP_TRANSPORT(r,i) send_info(r,i)
#include "../training_preview_publish.c"
static void __attribute__((thiscall)) original_ready(void *r,unsigned ready){
 if(r!=room || ready>255)failures++;
 events[count++]=16;
}
static void put(void *p,uint32_t v){memcpy(p,&v,4);}
static void uid(void *p,uint64_t v){memcpy(p,&v,8);}
static void reset(void){
 memset(room,0,sizeof(room));memset(cache,0,sizeof(cache));memset(pool,0,sizeof(pool));memset(events,0,sizeof(events));count=0;denied=0;state=0;identity=123;g_ctpBusy=0;
 for(unsigned slot=1;slot<=3;slot++){put(pool+slot*0x3778+0x3740,slot==1?1:9);pool[slot*0x3778+0x376d]=1;}
 put(room,0xc7d66c);put(room+0x10,(uintptr_t)cache);room[0x44f8]=room[0x4501]=1;
 put(room+0x15a80,(uintptr_t)"playlist");room[0x15a8c]=2;room[0x15a8d]=1;room[0x15aac]=3;room[0x15aad]=255;
 cache[0]=cache[1]=cache[2]=1;uid(cache+0x10,69421);uid(cache+0x20,identity);g_ctpReady=original_ready;
}
int main(void){
 BYTE before[sizeof(room)];unsigned cases=0;
 for(unsigned entry=0;entry<9;entry++){
  reset();room[0x15aac]=entry;memcpy(before,room,sizeof(room));CtpReadyHook(room,NULL,1);cases++;
  if(count!=2||events[0]!=15||events[1]!=16||memcmp(before,room,sizeof(room)))failures++;
 }
 /* Shipping catalogue disables Versus Training. Future activation of an
  * earlier pool row must not change which source blocks qualify. */
 for(unsigned earlier=0;earlier<2;earlier++)for(unsigned entry=0;entry<9;entry++){
  reset();pool[2*0x3778+0x376d]=0;
  if(earlier){put(pool+0x3740,1);pool[0x376d]=1;}
  room[0x15a8c]=1+earlier;room[0x15aac]=entry;memcpy(before,room,sizeof(room));
  CtpReadyHook(room,NULL,1);cases++;
  if(count!=2||events[0]!=15||events[1]!=16||memcmp(before,room,sizeof(room)))failures++;
 }
 reset();room[0x15a8c]=0;room[0x15aac]=0;memcpy(before,room,sizeof(room));
 CtpReadyHook(room,NULL,1);cases++;
 if(count!=2||events[0]!=15||events[1]!=16||memcmp(before,room,sizeof(room)))failures++;
 reset();room[0x15a8c]=1;room[0x15aac]=0;memcpy(before,room,sizeof(room));
 CtpReadyHook(room,NULL,1);cases++;
 if(count!=1||events[0]!=16||memcmp(before,room,sizeof(room)))failures++;
 for(unsigned bad=0;bad<4;bad++){
  reset();
  switch(bad){
   case 0:pool[3*0x3778+0x376d]=0;break;
   case 1:put(pool+3*0x3778+0x3740,0);break;
   case 2:put(pool+3*0x3778+0x3740,256);break;
   case 3:denied=(uintptr_t)(pool+0x3740);break;
  }
  memcpy(before,room,sizeof(room));CtpReadyHook(room,NULL,1);cases++;
  if(count!=1||events[0]!=16||memcmp(before,room,sizeof(room)))failures++;
 }
 for(unsigned bad=0;bad<17;bad++){
  reset();unsigned ready=1;
  switch(bad){
   case 0:room[0x44f8]=0;break;
   case 1:room[0x4501]=0;break;
   case 2:room[0x4503]=1;break;
   case 3:put(room+0x44fc,1);break;
   case 4:room[0x15aa8]=1;break;
   case 5:room[0x15aac]=255;break;
   case 6:room[0x15a8c]=0;break;
   case 7:room[0x15a8d]=2;break;
   case 8:put(room+0x15a80,(uintptr_t)"war");break;
   case 9:identity=0;break;
   case 10:uid(cache+0x20,456);break;
   case 11:cache[2]=0;break;
   case 12:state=9;break;
   case 13:ready=0;break;
   case 14:denied=(uintptr_t)cache;break;
   case 15:g_ctpBusy=1;break;
   case 16:uid(room+0x15b20,456);break;
  }
  memcpy(before,room,sizeof(room));CtpReadyHook(room,NULL,ready);cases++;
  if(count!=1||events[0]!=16||memcmp(before,room,sizeof(room)))failures++;
 }
 printf("RESULT %u %d\n",cases,failures);return failures?1:0;
}

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static BYTE test_room[0x4504],record[0x20],remote[0x40];
static uintptr_t denied;
static float test_elapsed;
static unsigned changes,original_calls;
static int Readable(const void *p,size_t n){return p && n && (uintptr_t)p!=denied;}
static void Log(const char *fmt,...){(void)fmt;}
static void timeout(void *p,float value){memcpy((BYTE *)p+0x10,&value,4);changes++;}
#define CQW_ROOM ((uintptr_t)test_room)
#define CQW_NOW() 1793227000ULL
#define CQW_ELAPSED(p) test_elapsed
#define CQW_TIMEOUT(p,v) timeout(p,v)
#include "client_queue_wait.c"
#define room test_room
#define elapsed test_elapsed
static const char marker[]="Q09ET0xfUVVFVUVfV0FJVF9WMQ==";
static const char data[]="<MSG P=1 T=1793227030 Q=69422 />Server queue 1/1 - estimated wait ~10 min.";
static void put(BYTE *p,uint32_t v){memcpy(p,&v,4);}
static void reset(void)
{
    memset(room,0,sizeof(room));memset(record,0,sizeof(record));memset(remote,0,sizeof(remote));
    put(room,0xc7d66c);put(room+0x10,(uintptr_t)record);put(record+0x10,69422);
    room[0x44f8]=room[0x4501]=1;
    put(room+0x18,(uintptr_t)remote);put(room+0x1c,3);room[0x20]=1;put(room+0x24,0x7143c0);
    put(remote,0xbdd530);put(remote+0x14,1);float limit=60;memcpy(remote+0x10,&limit,4);
    elapsed=45;denied=0;changes=original_calls=0;
}
static void __attribute__((thiscall)) original(void *self,uint64_t id,const char *name,const void *body,unsigned size)
{
    if (self==room && id==0 && name==NULL && body==data && size==sizeof(data)-1)original_calls++;
}
int main(void)
{
    (void)PatchCapacityQueueWait;
    BYTE before[sizeof(room)],r_before[sizeof(remote)];
    reset();memcpy(before,room,sizeof(room));
    if(CqwRenew(0,NULL,data,sizeof(data)-1)!=1 || changes!=1 || memcmp(before,room,sizeof(room)))return 1;
    float limit;memcpy(&limit,remote+0x10,4);if(limit!=90)return 2;
    g_cqwTicker=original;CqwTickerHook(room,NULL,0,NULL,data,sizeof(data)-1);
    if(original_calls!=1 || changes!=1)return 3;
    for(unsigned test=0;test<19;test++) {
        reset();const char *name=marker,*body=data;unsigned size=sizeof(data)-1;uint64_t id=69422;
        switch(test){
        case 0:body="<MSG P=1 T=1793227030 Q=69423 />wrong room";size=strlen(body);break;
        case 1:body="<MSG P=1 T=1793227030 />missing scope";size=strlen(body);break;case 2:room[0x44f8]=0;break;
        case 3:room[0x4501]=0;break;case 4:room[0x4503]=1;break;case 5:put(room,0);break;
        case 6:room[0x20]=0;break;case 7:put(room+0x1c,2);break;case 8:put(room+0x24,0x714270);break;
        case 9:put(remote,0);break;case 10:put(remote+0x14,4);break;case 11:elapsed=60;break;
        case 12:elapsed=-1;break;case 13:denied=(uintptr_t)remote;break;case 14:denied=(uintptr_t)record;break;
        case 15:body="<MSG P=1 T=1793227000 Q=69422 />expired";size=strlen(body);break;
        case 16:body="<MSG P=1 T=1793227031 Q=69422 />too long";size=strlen(body);break;
        case 17:body="<MSG P=2 T=1793227030 Q=69422 />unrelated";size=strlen(body);break;
        case 18:body="<MSG P=1 T=999999999999999999999 Q=69422 />overflow";size=strlen(body);break;
        }
        memcpy(before,room,sizeof(room));memcpy(r_before,remote,sizeof(remote));
        if(CqwRenew(id,name,body,size) || changes || memcmp(before,room,sizeof(room)) || memcmp(r_before,remote,sizeof(remote)))return 10+test;
    }
    reset();
    for(unsigned i=0;i<1000;i++) {
        elapsed=45+i*10;
        if(CqwRenew(69422,marker,data,sizeof(data)-1)!=1)return 40;
    }
    /* A stopped/expired heartbeat and a completed/cancelled task cannot renew. */
    elapsed+=46;
    if(CqwRenew(69422,marker,data,sizeof(data)-1))return 41;
    printf("PASS: real compiled adapter,19 negative authority cases,1000 renewals, original ticker ABI, stopped heartbeat expiry\n");
    return 0;
}

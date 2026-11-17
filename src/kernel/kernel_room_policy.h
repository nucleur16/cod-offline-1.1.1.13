#ifndef KERNEL_ROOM_POLICY_H
#define KERNEL_ROOM_POLICY_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* C ownership of the retained room-context publication policy. Fixed buffers,
 * no allocation, no log writer, no network or received command strings. */
typedef struct { int valid; uint32_t low,high; } KrpSample;
typedef struct {
    void *context;
    int (*available)(void *);
    int (*published)(void *,char *,size_t);
    KrpSample (*current)(void *);
    int (*queue)(void *,const char *,int,const KrpSample *);
} KrpBackend;
typedef struct {
    uint32_t next,serial,pending_since;
    int started,pending;
    char outstanding[64];
    KrpSample outstanding_sample;
} KrpPublisher;

static void KrpValue(const KrpPublisher *publisher,KrpSample sample,char value[64])
{
    value[0]=0;
    if(!sample.valid || !(sample.low||sample.high) || publisher->serial==UINT32_MAX)return;
    snprintf(value,64,"v1|%u|%08x%08x",publisher->serial+1,sample.high,sample.low);
}
static void KrpTick(KrpPublisher *publisher,uint32_t now,const KrpBackend *backend)
{
    char current[64]={0},value[64]={0};
    KrpSample sample;
    if(publisher->started && (int32_t)(now-publisher->next)<0)return;
    publisher->started=1;publisher->next=now+250;
    if(!backend->available(backend->context) ||
       !backend->published(backend->context,current,sizeof(current)))return;
    current[sizeof(current)-1]=0;
    if(publisher->pending) {
        if(strcmp(current,publisher->outstanding)) {
            int same;
            if((uint32_t)(now-publisher->pending_since)<2000)return;
            sample=backend->current(backend->context);
            same=publisher->outstanding[0] && sample.valid && publisher->outstanding_sample.valid &&
                 sample.low==publisher->outstanding_sample.low && sample.high==publisher->outstanding_sample.high;
            if(same)memcpy(value,publisher->outstanding,sizeof(value));
            else KrpValue(publisher,sample,value);
            if(backend->queue(backend->context,value,1,&sample)) {
                if(!same && value[0])++publisher->serial;
                memcpy(publisher->outstanding,value,sizeof(value));
                publisher->outstanding_sample=sample;publisher->pending_since=now;
            }
            return;
        }
        publisher->pending=0;
    }
    sample=backend->current(backend->context);KrpValue(publisher,sample,value);
    if(!value[0] && !current[0])return;
    if(backend->queue(backend->context,value,0,&sample)) {
        if(value[0])++publisher->serial;
        memcpy(publisher->outstanding,value,sizeof(value));
        publisher->outstanding_sample=sample;publisher->pending=1;publisher->pending_since=now;
    }
}

typedef struct {
    void *context;
    int (*read)(void *,uint32_t,void *,size_t);
    int (*in_room)(void *,uint32_t,uint32_t);
    int (*add)(void *,const char *);
    int (*lock)(void *);
    void (*unlock)(void *);
    KrpSample (*current)(void *);
} KrpAccess;

static KrpSample KrpReadSample(const KrpAccess *access)
{
    const uint32_t object=0x82d5470u;
    KrpSample empty={0,0,0},result={1,0,0};
    uint32_t table=0,function=0,membership=0,record=0;
    uint32_t same_table=0,same_function=0,same_membership=0,same_record=0,same_low=0,same_high=0;
    uint8_t active=0,same_active=0;
#define KRP_READ(address,value) access->read(access->context,(address),&(value),sizeof(value))
    if(!KRP_READ(object,table) || !table || !KRP_READ(table+0x44,function) || function!=0x70e9f0u ||
       !KRP_READ(table+0x40,membership) || membership!=0x70e9b0u ||
       !KRP_READ(object+0x10,record) || !record || !KRP_READ(record+1,active) || !active ||
       !access->in_room(access->context,object,function) || !KRP_READ(record+0x10,result.low) ||
       !KRP_READ(record+0x14,result.high) || !(result.low||result.high))return empty;
    if(!KRP_READ(object,same_table) || same_table!=table ||
       !KRP_READ(table+0x44,same_function) || same_function!=function ||
       !KRP_READ(table+0x40,same_membership) || same_membership!=membership ||
       !KRP_READ(object+0x10,same_record) || same_record!=record ||
       !KRP_READ(record+1,same_active) || !same_active ||
       !KRP_READ(record+0x10,same_low) || same_low!=result.low ||
       !KRP_READ(record+0x14,same_high) || same_high!=result.high ||
       !access->in_room(access->context,object,function))return empty;
#undef KRP_READ
    return result;
}
static int KrpQueue(const KrpAccess *access,const char *value,int require_empty)
{
    char command[128];uint32_t buffer=0,capacity=0,used=0;
    int length=snprintf(command,sizeof(command),"set ui_quickNativeRoomContext \"%s\"\n",value);
    if(length<0 || (size_t)length>=sizeof(command))return 0;
    if(!access->read(access->context,0x25094dcu,&buffer,sizeof(buffer)) ||
       !access->read(access->context,0x25094e0u,&capacity,sizeof(capacity)) ||
       !access->read(access->context,0x25094e4u,&used,sizeof(used)) ||
       !buffer || capacity<1024 || capacity>1048576 || used>capacity ||
       (uint32_t)length+1>=capacity-used || (require_empty && used))return 0;
    return access->add(access->context,command);
}
static int KrpRecover(const KrpAccess *access,const char *value,KrpSample expected)
{
    int result=0;
    KrpSample fresh;
    if(!access->lock(access->context))return 0;
    fresh=access->current(access->context);
    if(fresh.valid==expected.valid && (!fresh.valid || (fresh.low==expected.low && fresh.high==expected.high)))
        result=KrpQueue(access,value,1);
    access->unlock(access->context);
    return result;
}
#endif

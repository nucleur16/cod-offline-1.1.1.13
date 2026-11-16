/* Offline differential proof against an explicitly supplied retained proxy
 * source. Nothing is installed or executed as a Windows process. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#define NATIVE_ROOM_FIXTURE
#define sprintf_s(buffer,...) snprintf(buffer,sizeof(buffer),__VA_ARGS__)
#include LEGACY_ROOM_PUBLISHER_HEADER
#include LEGACY_ROOM_SAMPLE_HEADER
#include LEGACY_ROOM_QUEUE_HEADER
#ifndef KRP_POLICY_HEADER
#define KRP_POLICY_HEADER "../src/kernel/kernel_room_policy.h"
#endif
#include KRP_POLICY_HEADER

static unsigned checks;
#define CHECK(value) do {++checks;if(!(value)){fprintf(stderr,"line %d: %s\n",__LINE__,#value);return 1;}}while(0)
namespace NativeRoomContext {
struct PublisherFixture {
    static void Serial(Publisher& p,uint32_t value){p.serial_=value;}
    static bool Same(const Publisher& p,const KrpPublisher& q){return p.next_==q.next && p.serial_==q.serial &&
        p.pendingSince_==q.pending_since && p.started_==bool(q.started) && p.pending_==bool(q.pending) &&
        p.outstanding_==q.outstanding && p.outstandingSample_.valid==bool(q.outstanding_sample.valid) &&
        p.outstandingSample_.low==q.outstanding_sample.low && p.outstandingSample_.high==q.outstanding_sample.high;}
};
}
struct IO : NativeRoomContext::Backend {
    bool available=true,readable=true,queueable=true,recoverable=true;
    unsigned available_calls=0,published_calls=0,sample_calls=0,recovery_calls=0;
    std::string actual;NativeRoomContext::Sample sample={true,0x1234567,0x89abcdef};
    std::vector<std::string> queued;
    bool Available(){++available_calls;return available;}
    bool Published(std::string& value){++published_calls;value=actual;return readable;}
    NativeRoomContext::Sample Current(){++sample_calls;return sample;}
    bool Queue(const std::string& value){if(!queueable)return false;queued.push_back(value);return true;}
    bool QueueRecovery(const std::string& value,const NativeRoomContext::Sample&){++recovery_calls;return recoverable&&Queue(value);}
};
static int available(void* p){return static_cast<IO*>(p)->Available();}
static int published(void* p,char* value,size_t size){std::string s;bool ok=static_cast<IO*>(p)->Published(s);if(s.size()>=size)return 0;memcpy(value,s.c_str(),s.size()+1);return ok;}
static KrpSample current(void* p){auto s=static_cast<IO*>(p)->Current();return {s.valid,s.low,s.high};}
static int queue(void* p,const char* text,int recovery,const KrpSample* expected){IO* io=static_cast<IO*>(p);return recovery?io->QueueRecovery(text,{bool(expected->valid),expected->low,expected->high}):io->Queue(text);}

struct Memory {
    std::map<uint32_t,uint32_t> cells;
    unsigned reads=0,fail_at=0,drift_at=0,in_room_calls=0,fail_room_at=0,locks=0,unlocks=0,adds=0;
    uint32_t drift_address=0,drift_value=0;
    bool can_lock=true,can_add=true;
    NativeRoomContext::Sample fresh={true,0x1234,0x5678};
    std::vector<std::string> commands;
    Memory(){cells={{0x82d5470,0x1000},{0x1044,0x70e9f0},{0x1040,0x70e9b0},{0x82d5480,0x2000},
                    {0x2001,1},{0x2010,0x1234},{0x2014,0x5678},
                    {0x25094dc,0x3000},{0x25094e0,65536},{0x25094e4,0}};}
    bool Bytes(uint32_t address,void* out,size_t size){
        ++reads;if(reads==drift_at)cells[drift_address]=drift_value;
        if(reads==fail_at || !cells.count(address) || size>4)return false;
        uint32_t value=cells[address];memcpy(out,&value,size);return true;
    }
    template<class T>bool Read(uint32_t at,T& value){return Bytes(at,&value,sizeof(value));}
    bool InRoom(uint32_t object,uint32_t function){++in_room_calls;return object==0x82d5470 && function==0x70e9f0 && in_room_calls!=fail_room_at;}
    bool Add(const char* text){++adds;if(!can_add)return false;commands.emplace_back(text);return true;}
    bool LockForRecovery(){++locks;return can_lock;}
    void UnlockRecovery(){++unlocks;}
    NativeRoomContext::Sample Current(){return fresh;}
};
static int read_memory(void* p,uint32_t at,void* out,size_t size){return static_cast<Memory*>(p)->Bytes(at,out,size);}
static int in_room(void* p,uint32_t object,uint32_t function){return static_cast<Memory*>(p)->InRoom(object,function);}
static int add(void* p,const char* text){return static_cast<Memory*>(p)->Add(text);}
static int lock(void* p){return static_cast<Memory*>(p)->LockForRecovery();}
static void unlock(void* p){static_cast<Memory*>(p)->UnlockRecovery();}
static KrpSample fresh(void* p){auto s=static_cast<Memory*>(p)->fresh;return {s.valid,s.low,s.high};}
static KrpAccess access(Memory& m){return {&m,read_memory,in_room,add,lock,unlock,fresh};}
static bool same_sample(NativeRoomContext::Sample a,KrpSample b){return a.valid==bool(b.valid)&&a.low==b.low&&a.high==b.high;}

static int sample_and_queue(){
    for(unsigned fail=0;fail<=14;++fail){Memory a,b;a.fail_at=b.fail_at=fail;auto io=access(b);CHECK(same_sample(NativeRoomContext::ReadSample(a),KrpReadSample(&io)));CHECK(a.reads==b.reads);}
    for(unsigned room=1;room<=2;++room){Memory a,b;a.fail_room_at=b.fail_room_at=room;auto io=access(b);CHECK(same_sample(NativeRoomContext::ReadSample(a),KrpReadSample(&io)));}
    for(uint32_t at:{0x82d5470u,0x1044u,0x1040u,0x82d5480u,0x2001u,0x2010u,0x2014u}){
        Memory a,b;a.drift_at=b.drift_at=8;a.drift_address=b.drift_address=at;a.drift_value=b.drift_value=0;auto io=access(b);
        CHECK(same_sample(NativeRoomContext::ReadSample(a),KrpReadSample(&io)));
    }
    for(uint32_t capacity:{0u,1023u,1024u,65536u,1048576u,1048577u})for(uint32_t used:{0u,1u,980u,1024u,65536u,UINT32_MAX})for(bool empty:{false,true}){
        Memory a,b;a.cells[0x25094e0]=b.cells[0x25094e0]=capacity;a.cells[0x25094e4]=b.cells[0x25094e4]=used;auto io=access(b);
        CHECK(NativeRoomContext::QueueCommand(a,"v1|1|0000567800001234",empty)==bool(KrpQueue(&io,"v1|1|0000567800001234",empty)));
        CHECK(a.commands==b.commands);CHECK(a.adds==b.adds);
    }
    for(bool held:{false,true})for(bool valid:{false,true})for(bool same:{false,true})for(bool empty:{false,true}){
        Memory a,b;a.can_lock=b.can_lock=held;a.fresh.valid=b.fresh.valid=valid;
        if(!same)a.fresh.low=b.fresh.low=99;
        if(!empty)a.cells[0x25094e4]=b.cells[0x25094e4]=1;
        auto io=access(b);NativeRoomContext::Sample expected={true,0x1234,0x5678};KrpSample e={1,0x1234,0x5678};
        CHECK(NativeRoomContext::QueueRecoveryCommand(a,"v1|1|0000567800001234",expected)==bool(KrpRecover(&io,"v1|1|0000567800001234",e)));
        CHECK(a.locks==b.locks && a.unlocks==b.unlocks && a.commands==b.commands);
    }
    return 0;
}
static uint32_t random32(uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(){
    CHECK(sample_and_queue()==0);
    const uint32_t intervals[]={0,1,249,250,251,1999,2000,2001,10000,0x7fffffffu};
    unsigned transitions=0;
    for(unsigned run=0;run<64;++run){
        NativeRoomContext::Publisher old;KrpPublisher port={};IO a,b;KrpBackend backend={&b,available,published,current,queue};
        uint32_t rng=0xc0d04500u+run,now=random32(rng);
        for(unsigned step=0;step<512;++step){
            uint32_t choice=random32(rng);now+=intervals[choice%10];
            a.available=b.available=(choice&7)!=0;a.readable=b.readable=(choice&31)!=0;
            a.queueable=b.queueable=(choice&3)!=0;a.recoverable=b.recoverable=(choice&15)!=0;
            if(choice&64)a.sample=b.sample={bool(choice&128),random32(rng),random32(rng)};
            if((choice&3)==0&&!a.queued.empty())a.actual=b.actual=a.queued.back();
            if((choice&127)==0)a.actual=b.actual="";
            if(step==300){NativeRoomContext::PublisherFixture::Serial(old,UINT32_MAX-1);port.serial=UINT32_MAX-1;}
            old.Tick(now,a);KrpTick(&port,now,&backend);++transitions;
            CHECK(a.queued==b.queued);
            CHECK(a.available_calls==b.available_calls && a.published_calls==b.published_calls && a.sample_calls==b.sample_calls && a.recovery_calls==b.recovery_calls);
            CHECK(NativeRoomContext::PublisherFixture::Same(old,port));
        }
    }
    printf("{\"passed\":true,\"transitions\":%u,\"checks\":%u,\"Windows_executed\":false}\n",transitions,checks);
    return 0;
}

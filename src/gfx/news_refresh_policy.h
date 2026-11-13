#ifndef CODOL_NEWS_REFRESH_POLICY_H
#define CODOL_NEWS_REFRESH_POLICY_H
#include <stdint.h>
#include <stddef.h>

/* Exact a5a7 frontend manager. No node or buffer pointer survives nr_frame().
 * +4 is the native request-kind discriminator: only 1 takes the binary path;
 * every other value takes the text path (5BCDA9). The extension kind below
 * identifies a real text refresh in this manager itself. Its constructor clears
 * +4, so reusing an allocation for a new frontend cannot inherit this state.
 * Native response callbacks change +8, never +4. No AS/data value is fabricated.
 */
#define NR_KIND 0x4E525458u
#define NR_INTERVAL_MS 60000u
#define NR_MAX_NODES 64u
#define NR_MAX_URL 2048u
enum { NR_KIND_OFF=4, NR_STATE=8, NR_CATEGORY=12, NR_HTTP=16,
       NR_HEAD=0x478, NR_CURRENT=0x480, NR_RECEIVE=0x484, NR_RECEIVE_SIZE=0x48c,
       NR_URL=0, NR_NEXT=4, NR_PAYLOAD=8, NR_PAYLOAD_SIZE=12,
       NR_PAYLOAD_TYPE=16, NR_NODE_CATEGORY=20, NR_HTTP_STATUS=0x28,
       NR_HTTP_PENDING=0x2c };
typedef struct NRPort {
    void *ctx;
    int (*readable)(void *, uint32_t, uint32_t);
    uint32_t (*read32)(void *, uint32_t);
    void (*write32)(void *, uint32_t, uint32_t);
    unsigned (*read8)(void *, uint32_t);
    void (*step)(void *); /* unmodified 5BC9E0, on its existing main-thread call */
    void (*release)(void *, uint32_t); /* exact same allocator as 5BC4E0 */
} NRPort;
typedef struct NRClock { uint32_t manager, due; int scheduled; } NRClock;
typedef struct NRResult { unsigned started, completed, replaced, failed; } NRResult;
static uint32_t nr_r(NRPort *p,uint32_t a){return p->read32(p->ctx,a);}
static void nr_w(NRPort *p,uint32_t a,uint32_t v){p->write32(p->ctx,a,v);}
static int nr_url_text(NRPort *p,uint32_t a) {
    uint32_t n; unsigned x[4];
    if(!a)return 0;
    for(n=0;n<NR_MAX_URL;n++){
        if(!p->readable(p->ctx,a+n,1))return 0;
        if(!p->read8(p->ctx,a+n))break;
    }
    if(n<4||n==NR_MAX_URL)return 0;
    for(unsigned i=0;i<4;i++){
        x[i]=p->read8(p->ctx,a+n-4+i);
        if(x[i]>='A'&&x[i]<='Z')x[i]+='a'-'A';
    }
    return !(x[0]=='.'&&((x[1]=='p'&&x[2]=='n'&&x[3]=='g')||
                                      (x[1]=='t'&&x[2]=='g'&&x[3]=='a')));
}
/* Validate the complete retained queue before touching any node. The last text
 * node of category 1 is the same winner 5BCB39 selects when it publishes adnews.
 * A cycle/oversized queue, unknown kind or invalid URL is left unmodified. */
static uint32_t nr_find(NRPort *p,uint32_t m) {
    uint32_t at=nr_r(p,m+NR_HEAD), seen[NR_MAX_NODES], winner=0; unsigned count=0;
    while(at){
        if(count==NR_MAX_NODES||!p->readable(p->ctx,at,0x1c))return 0;
        for(unsigned i=0;i<count;i++)if(seen[i]==at)return 0;
        seen[count++]=at;
        if(nr_r(p,at+NR_PAYLOAD_TYPE)>2)return 0;
        if(nr_r(p,at+NR_NODE_CATEGORY)==1&&
           nr_r(p,at+NR_PAYLOAD_TYPE)!=1&&nr_url_text(p,nr_r(p,at+NR_URL)))winner=at;
        at=nr_r(p,at+NR_NEXT);
    }
    return winner;
}
/* A completed text consume and its normal publication are synchronous here.
 * Restore next BEFORE publication (which traverses the whole queue, including
 * retained image nodes). Retire the overwritten text allocation only afterward.
 * Existing image bytes, IDs, URL strings and receive storage are never freed.
 */
static void nr_consume(NRPort *p,uint32_t m,uint32_t node,NRResult *out){
    uint32_t next=nr_r(p,node+NR_NEXT), old=nr_r(p,node+NR_PAYLOAD), fresh;
    nr_w(p,node+NR_NEXT,0);
    p->step(p->ctx);
    nr_w(p,node+NR_NEXT,next);
    fresh=nr_r(p,node+NR_PAYLOAD);
    if(nr_r(p,m+NR_STATE)==6)p->step(p->ctx);
    if(fresh&&fresh!=old){
        if(old){
#ifndef NR_SKIP_FREE
            p->release(p->ctx,old);
#endif
        }
        out->replaced++;
    }else out->failed++;
    nr_w(p,m+NR_KIND_OFF,2); /* ordinary text-kind after the real transaction */
    out->completed++;
}
static NRResult nr_frame(NRPort *p,NRClock *clock,uint32_t m,uint32_t now){
    NRResult out={0,0,0,0}; uint32_t state,node=0;
#ifdef NR_DISABLE_REFRESH
    p->step(p->ctx);return out;
#endif
    if(!m||!p->readable(p->ctx,m,0x498)){
        clock->manager=0;clock->scheduled=0;p->step(p->ctx);return out;
    }
    state=nr_r(p,m+NR_STATE);
    if(clock->manager!=m){clock->manager=m;clock->scheduled=0;}
    if(nr_r(p,m+NR_KIND_OFF)==NR_KIND){
        node=nr_find(p,m);
        if(!node||nr_r(p,m+NR_CURRENT)!=node||nr_r(p,m+NR_CATEGORY)!=1||
           state<2||state>5){p->step(p->ctx);return out;}
        if(state==4||state==5){
            /* 1C is the original HTTP worker's pending-task field. Native
             * 5BC8DB refuses starts until it is zero; wait for the same boundary
             * before retiring any text buffer, even if status is already final. */
            if(nr_r(p,m+NR_HTTP_PENDING)){
                nr_w(p,m+NR_STATE,3);p->step(p->ctx);return out;
            }
            nr_consume(p,m,node,&out);return out;
        }
        p->step(p->ctx);return out;
    }
    if(state!=7){p->step(p->ctx);return out;}
    if(!clock->scheduled){clock->due=now+NR_INTERVAL_MS;clock->scheduled=1;}
    if((int32_t)(now-clock->due)<0||nr_r(p,m+NR_CURRENT)||
       nr_r(p,m+NR_HTTP_PENDING)){p->step(p->ctx);return out;}
    clock->due=now+NR_INTERVAL_MS;
    node=nr_find(p,m);
    if(!node){p->step(p->ctx);return out;}
    nr_w(p,m+NR_CURRENT,node);nr_w(p,m+NR_STATE,2);
    p->step(p->ctx); /* real native URL classification and HTTP start */
    nr_w(p,m+NR_KIND_OFF,NR_KIND);out.started++;
    /* Native normally sets state3 from its first BODY callback (5BC7A7).
     * A zero-body response/failure has no such callback. Follow the real HTTP
     * status instead, preventing repeated starts or a permanently stalled queue.
     * 1=running, 2=complete, 3/4=failure, 5=setup incomplete (A3EA56).
     * This records request progress; it never fabricates a response/result. */
    state=nr_r(p,m+NR_STATE);
    if(state==2){
#ifndef NR_NO_PENDING_PROMOTION
        uint32_t status=nr_r(p,m+NR_HTTP_STATUS);
        if(nr_r(p,m+NR_HTTP_PENDING)||status==1)nr_w(p,m+NR_STATE,3);
        else nr_w(p,m+NR_STATE,status==2?4:5);
#endif
    }
    return out;
}
#endif

/* Execute the same production CAS helpers with actual competing threads.
 * This tests inventory ownership, not Windows kernel I/O or native teardown. */
#include "../p2p_startup_ownership.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
static p2ps_atomic_value cas(volatile p2ps_atomic_value *where,p2ps_atomic_value value,p2ps_atomic_value expected)
{ __atomic_compare_exchange_n(where,&expected,value,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return expected; }
typedef struct {
    volatile p2ps_atomic_value phase,claim;
    pthread_barrier_t go,done;
    unsigned *packet;
    int claimed,reclaimed;
} trial;
static void *fiber(void *raw)
{
    trial *t=raw;pthread_barrier_wait(&t->go);
    t->claimed=P2PSClaimForFiber(&t->phase,cas);
    if(t->claimed)assert(*t->packet==0x12345678);
    pthread_barrier_wait(&t->done);
    if(t->claimed)__atomic_store_n(&t->phase,P2PS_FIBER_DONE,__ATOMIC_SEQ_CST);
    return NULL;
}
static void *worker(void *raw)
{
    trial *t=raw;pthread_barrier_wait(&t->go);
    p2ps_atomic_value observed=P2PSCancelOrObserve(&t->phase,cas);
    t->reclaimed=P2PSWorkerMayReclaim(observed);
    if(t->reclaimed){free(t->packet);t->packet=NULL;}
    pthread_barrier_wait(&t->done);return NULL;
}
int main(void)
{
    volatile p2ps_atomic_value phase=P2PS_FETCHED,claim=0;
    assert(P2PSReserveContext(&claim,cas));assert(!P2PSReserveContext(&claim,cas));
    /* Cancellation wins: no subsequent fiber may read the inventory. */
    assert(P2PSWorkerMayReclaim(P2PSCancelOrObserve(&phase,cas)));
    assert(phase==P2PS_CANCELLED&&!P2PSClaimForFiber(&phase,cas));
    /* Fiber wins: timeout must retain even after its eventual DONE. */
    phase=P2PS_FETCHED;assert(P2PSClaimForFiber(&phase,cas));
    p2ps_atomic_value observed=P2PSCancelOrObserve(&phase,cas);
    assert(!P2PSWorkerMayReclaim(observed));phase=P2PS_FIBER_DONE;
    assert(!P2PSWorkerMayReclaim(observed));
    assert(P2PSWorkerMayReclaim(P2PSCancelOrObserve(&phase,cas)));
    assert(!P2PSWorkerMayReclaim(99));
    for(unsigned i=0;i<128;++i){
        trial t={.phase=P2PS_FETCHED};pthread_t a,b;
        t.packet=malloc(sizeof(*t.packet));assert(t.packet);*t.packet=0x12345678;
        assert(!pthread_barrier_init(&t.go,NULL,2));assert(!pthread_barrier_init(&t.done,NULL,2));
        assert(!pthread_create(&a,NULL,fiber,&t));assert(!pthread_create(&b,NULL,worker,&t));
        assert(!pthread_join(a,NULL));assert(!pthread_join(b,NULL));
        assert(t.claimed!=t.reclaimed);
        if(t.claimed){assert(t.phase==P2PS_FIBER_DONE);assert(*t.packet==0x12345678);free(t.packet);}
        pthread_barrier_destroy(&t.go);pthread_barrier_destroy(&t.done);
    }
    puts("PASS: production atomic ownership helpers, both orderings, one-shot reservation and128 competing fiber/worker races");return 0;
}

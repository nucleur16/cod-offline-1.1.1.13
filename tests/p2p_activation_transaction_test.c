/* Future native fixture for Root-admitted hosted compiler only. NOT run by author.
 * Exercises the ACTUAL portable transaction header, with RAM pointers/callbacks.
 * No Windows APIs, DLL/EXE load, sockets, threads or C1 memory. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/p2p/p2p_activation_transaction.h"
typedef struct {
    int slots[3], callbacks[5], count, chain, next;
    int handler, prepare_count, acquire_count, release_count, writes, tick_adds, worker;
    int failure, foreign_slot, foreign_callback, fail_remove;
} Fixture;
static void init(Fixture *f) {
    memset(f,0,sizeof(*f));f->slots[0]=100;f->slots[1]=200;f->slots[2]=300;
    f->callbacks[0]=7;f->callbacks[1]=8;f->callbacks[2]=9;f->count=3;f->chain=43;f->next=52;
}
static int prepare(void *p) { Fixture*f=p;++f->prepare_count;return f->failure!=1; }
static int handler(void *p,int add) {
    Fixture*f=p;
    if(add){++f->acquire_count;if(f->failure==2)return 0;f->handler=1;return 1;}
    ++f->release_count;if(f->fail_remove)return 0;f->handler=0;return 1;
}
static int slot(void *p,unsigned n,int add) {
    Fixture*f=p;int original=100*(int)(n+1),hook=original+1;
    if(add){if(f->slots[n]!=original)return 0;f->slots[n]=hook;++f->writes;return f->failure!=(int)n+3;}
    if(f->foreign_slot && n==1){f->slots[n]=999;return 0;}
    if(f->slots[n]==original)return 1;
    if(f->slots[n]!=hook)return 0;
    f->slots[n]=original;return 1;
}
static int tick(void *p,int add) {
    Fixture*f=p;
    if(f->chain!=43 || f->next!=52)return 0;
    if(add){f->callbacks[f->count++]=10;++f->tick_adds;if(f->foreign_callback)f->callbacks[0]=999;return f->failure!=6;}
    if(f->callbacks[0]!=7 || f->callbacks[1]!=8 || f->callbacks[2]!=9)return 0;
    if(f->count==3)return 1;
    if(f->count!=4 || f->callbacks[3]!=10)return 0;
    f->count=3;f->callbacks[3]=0;return 1;
}
static int current(void *p) {
    Fixture*f=p;
    return f->failure!=7 && f->handler && f->chain==43 && f->next==52 && f->count==4 &&
      f->callbacks[0]==7 && f->callbacks[1]==8 && f->callbacks[2]==9 && f->callbacks[3]==10 &&
      f->slots[0]==101 && f->slots[1]==201 && f->slots[2]==301;
}
static PiActivationOps ops(Fixture*f){PiActivationOps a={f,prepare,handler,slot,tick,current};return a;}
static int host(Fixture*f,PiActivation*a,PiActivationOps*o,int main_thread,int guest,int frozen_inprocess,int current_attempt) {
    if(!main_thread || guest || !frozen_inprocess || !current_attempt)return 0;
    if(!PiActivationEnsure(a,o))return 0;
    assert(a->state==PIA_READY && current(f));++f->worker;return 1;
}
int main(void) {
    unsigned cases=0;
    {Fixture f;PiActivation a={0};init(&f);PiActivationOps o=ops(&f);
     assert(host(&f,&a,&o,1,0,1,1));assert(f.worker==1 && f.prepare_count==1 && f.tick_adds==1);
     assert(f.chain==43 && f.next==52 && f.callbacks[0]==7 && f.callbacks[1]==8 && f.callbacks[2]==9);
     assert(PiActivationEnsure(&a,&o));assert(f.prepare_count==1 && f.acquire_count==1 && f.tick_adds==1);++cases;}
    for(int failure=1;failure<=7;++failure) {
     Fixture f;PiActivation a={0};init(&f);f.failure=failure;PiActivationOps o=ops(&f);
     assert(!host(&f,&a,&o,1,0,1,1));assert(!f.worker && a.state==PIA_FAILED && a.failure_stage==failure);
     assert(f.slots[0]==100 && f.slots[1]==200 && f.slots[2]==300 && f.count==3 && !f.handler && !a.quarantine);
     int acquire=f.acquire_count,writes=f.writes;assert(!PiActivationEnsure(&a,&o));assert(f.acquire_count==acquire && f.writes==writes);++cases;
    }
    for(int guard=0;guard<4;++guard) {
     Fixture f;PiActivation a={0};init(&f);PiActivationOps o=ops(&f);
     assert(!host(&f,&a,&o,guard!=0,guard==1,guard!=2,guard!=3));
     assert(!f.worker && !f.prepare_count && !f.acquire_count && !f.writes && !f.tick_adds);++cases;
    }
    {Fixture f;PiActivation a={0};init(&f);f.failure=7;f.foreign_slot=1;PiActivationOps o=ops(&f);
     assert(!host(&f,&a,&o,1,0,1,1));assert(a.quarantine && !f.worker && f.slots[1]==999 && f.slots[0]==100 && f.slots[2]==300 && !f.handler);++cases;}
    {Fixture f;PiActivation a={0};init(&f);f.foreign_callback=1;PiActivationOps o=ops(&f);
     assert(!host(&f,&a,&o,1,0,1,1));assert(a.quarantine && !f.worker && f.callbacks[0]==999 && f.count==4 && f.slots[0]==100 && f.slots[2]==300);++cases;}
    {Fixture f;PiActivation a={0};init(&f);f.failure=6;f.fail_remove=1;PiActivationOps o=ops(&f);
     assert(!host(&f,&a,&o,1,0,1,1));assert(a.quarantine && f.handler && !f.worker && f.release_count==1);
     assert(!PiActivationEnsure(&a,&o));assert(f.release_count==1);++cases;}
    {Fixture f;PiActivation a={0};init(&f);PiActivationOps o=ops(&f);assert(host(&f,&a,&o,1,0,1,1));
     f.chain=999;assert(!PiActivationCurrent(&a,&o));assert(a.quarantine && a.state==PIA_FAILED && f.chain==999 && f.handler);++cases;}
    printf("{\"actual_portable_core_cases\":%u,\"Windows_API_or_game\":false}\n",cases);return 0;
}

#include <stdio.h>
#include "../src/rooms/quick_start_policy.h"
static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static QsgSnapshot sample(void) {
 QsgSnapshot s;memset(&s,0,sizeof(s));
 s.managed=s.valid=s.host=s.all_ready=s.phase_known=1;s.phase=3;
 s.identity.controller=0x6000000;s.identity.room=69421;s.identity.count=2;
 s.identity.members[0]=11;s.identity.members[1]=22;
 memcpy(s.identity.mode,"playlist",9);s.identity.next_entry=0;
 return s;
}
int main(void) {
 QsgPolicy p;QsgSnapshot s=sample();memset(&p,0,sizeof(p));
 QsgObserve(&p,&s,0);CHECK(!QsgConsume(&p,&s,1));CHECK(p.state==4);
 CHECK(QsgGrant(&p,&s,2));CHECK(!QsgGrant(&p,&s,3));CHECK(p.born==2);
 s.phase=4;QsgObserve(&p,&s,1000);CHECK(p.state==1);
 s.phase=5;CHECK(QsgConsume(&p,&s,10000));CHECK(!QsgConsume(&p,&s,10001));
 s.identity.next_entry=1;CHECK(!QsgGrant(&p,&s,10002));
 s.phase=6;QsgObserve(&p,&s,10003);CHECK(p.state==0);
 s.phase=3;CHECK(QsgGrant(&p,&s,10004));s.all_ready=0;QsgObserve(&p,&s,10005);
 s.all_ready=1;s.phase=5;CHECK(!QsgConsume(&p,&s,10006));CHECK(!QsgGrant(&p,&s,10007));
 s.phase=0;QsgObserve(&p,&s,10008);s.phase=4;CHECK(QsgGrant(&p,&s,10009));
 s.phase=5;CHECK(!QsgConsume(&p,&s,40009));CHECK(p.state==3);
 memset(&p,0,sizeof(p));s=sample();CHECK(QsgGrant(&p,&s,0xfffffff0u));s.phase=5;
 CHECK(QsgConsume(&p,&s,100));
 memset(&p,0,sizeof(p));s=sample();QsgObserve(&p,&s,0);s.managed=0;
 CHECK(!QsgConsume(&p,&s,1));CHECK(p.state==3);
 s.phase=0;QsgObserve(&p,&s,2);s.phase=5;CHECK(QsgConsume(&p,&s,3));
 memset(&p,0,sizeof(p));s=sample();CHECK(QsgGrant(&p,&s,0));s.phase=4;QsgObserve(&p,&s,1);
 s.phase=3;QsgObserve(&p,&s,2);s.phase=5;CHECK(!QsgConsume(&p,&s,3));
 memset(&p,0,sizeof(p));s=sample();CHECK(QsgGrant(&p,&s,0));s.identity.room++;
 s.phase=5;CHECK(!QsgConsume(&p,&s,1));
 memset(&p,0,sizeof(p));s=sample();CHECK(QsgGrant(&p,&s,0));
 QsgSnapshot missing;memset(&missing,0,sizeof(missing));QsgObserve(&p,&missing,1);
 s.phase=5;CHECK(!QsgGrant(&p,&s,2));CHECK(!QsgConsume(&p,&s,3));
 /* Real repeated-round capture: AS start intent arrives while native is
  * still in phase1, before its ordinary enter-room progression to4 then5. */
 memset(&p,0,sizeof(p));s=sample();s.phase=1;
 CHECK(QsgGrant(&p,&s,100));QsgObserve(&p,&s,101);CHECK(p.state==1);
 CHECK(!QsgGrant(&p,&s,102));CHECK(p.born==100);
 s.phase=2;QsgObserve(&p,&s,103);s.phase=3;QsgObserve(&p,&s,104);s.phase=4;QsgObserve(&p,&s,105);
 s.phase=5;CHECK(QsgConsume(&p,&s,105));CHECK(!QsgConsume(&p,&s,106));
 memset(&p,0,sizeof(p));s=sample();s.phase=1;QsgObserve(&p,&s,100);
 s.phase=5;CHECK(!QsgConsume(&p,&s,101));
 memset(&p,0,sizeof(p));s=sample();s.phase=1;CHECK(QsgGrant(&p,&s,100));
 s.identity.room++;QsgObserve(&p,&s,101);CHECK(p.state==3);
 CHECK(!QsgGrant(&p,&s,102));s.phase=5;CHECK(!QsgConsume(&p,&s,103));
 memset(&p,0,sizeof(p));s=sample();s.phase=1;CHECK(QsgGrant(&p,&s,100));
 QsgObserve(&p,&s,120100);CHECK(p.state==3);s.phase=5;CHECK(!QsgConsume(&p,&s,120101));
 printf("Quick start policy: %u checks passed\n",checks);return 0;
}

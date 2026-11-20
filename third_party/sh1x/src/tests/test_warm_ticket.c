/* Real bounded admission-file parsing/replacement. SPDX-License-Identifier: AGPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "admission.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static char path[256];
static unsigned checks;
#define CHECK(name,condition) do { ++checks; if (!(condition)) { fprintf(stderr,"FAIL %s:%d\n",name,__LINE__);abort(); } } while(0)
static const char *prep="0123456789abcdef0123456789abcdef";
static const char *lease="11111111111111111111111111111111";
static const char *other="22222222222222222222222222222222";

static void publish(const char *p,const char *l,const char *uid,long long expiry,const char *tail)
{
    char temporary[280];
    FILE *f;
    snprintf(temporary,sizeof(temporary),"%s.new",path);
    f=fopen(temporary,"wb");assert(f);
    fprintf(f,"warm 27733 %s %s %s %lld%s\n",p,l,uid,expiry,tail);
    assert(!fclose(f));assert(!rename(temporary,path));
}

int main(void)
{
    long long now=(long long)time(NULL);
    snprintf(path,sizeof(path),"/tmp/codol-warm-ticket-%ld",(long)getpid());
    assert(!setenv("CODOL_ADMISSION_PATH",path,1));
    CHECK("missing file denies",!AdmissionWarmTicket(27733,prep,lease,7));
    publish(prep,lease,"0000000000000007",now+60,"");
    CHECK("current process lease and uid accepted",AdmissionWarmTicket(27733,prep,lease,7));
    CHECK("wrong process denied",!AdmissionWarmTicket(27733,other,lease,7));
    CHECK("wrong lease denied",!AdmissionWarmTicket(27733,prep,other,7));
    CHECK("wrong port denied",!AdmissionWarmTicket(27734,prep,lease,7));
    CHECK("wrong uid denied",!AdmissionWarmTicket(27733,prep,lease,8));
    publish(prep,other,"0000000000000007",now+60,"");
    CHECK("atomic same-size same-second replacement revokes old lease",!AdmissionWarmTicket(27733,prep,lease,7));
    CHECK("replacement lease accepted",AdmissionWarmTicket(27733,prep,other,7));
    publish(prep,lease,"0000000000000007",now-1,"");
    CHECK("expired denied",!AdmissionWarmTicket(27733,prep,lease,7));
    publish(prep,lease,"0000000000000007",now+3600,"");
    CHECK("unbounded lifetime denied",!AdmissionWarmTicket(27733,prep,lease,7));
    publish(prep,lease,"0000000000000007",now+60," extra");
    CHECK("trailing fields denied",!AdmissionWarmTicket(27733,prep,lease,7));
    publish("bad",lease,"0000000000000007",now+60,"");
    CHECK("malformed preparation denied",!AdmissionWarmTicket(27733,prep,lease,7));
    publish(prep,lease,"0000000000000000",now+60,"");
    CHECK("anonymous uid denied",!AdmissionWarmTicket(27733,prep,lease,0));
    publish(prep,lease,"0000000000000007",now+60,"");
    CHECK("restored roster accepted",AdmissionWarmTicket(27733,prep,lease,7));
    assert(!unlink(path));
    CHECK("file removal immediately revokes",!AdmissionWarmTicket(27733,prep,lease,7));
    {
        char central[280]; uint8_t keys[2][16]; FILE *f;
        snprintf(central,sizeof(central),"%s.central",path);
        f=fopen(central,"wb");assert(f);
        fprintf(f,"127.0.0.1 00112233445566778899aabbccddeeff 0000000000000007 %lld Host\n",now+300);
        assert(!fclose(f));
        assert(!setenv("CODOL_ADMISSION_PATH",central,1));
        assert(!setenv("CODOL_WARM_ADMISSION_PATH",path,1));
        publish(prep,lease,"0000000000000008",now+60,"");
        CHECK("private assignment accepted",AdmissionWarmTicket(27733,prep,lease,8));
        CHECK("private assignment does not grant host seat",!AdmissionWarmTicket(27733,prep,lease,7));
        CHECK("central credentials survive private assignment",AdmissionCandidates(NULL,keys,2)==1 && keys[0][1]==0x11);
        CHECK("central reload does not replace private assignment",AdmissionWarmTicket(27733,prep,lease,8));
        assert(!unlink(path));
        CHECK("missing private file denies despite central credential",!AdmissionWarmTicket(27733,prep,lease,7));
        CHECK("private removal preserves central credentials",AdmissionCandidates(NULL,keys,2)==1);
        assert(!unlink(central));
        CHECK("missing central file never becomes private credential",AdmissionCandidates(NULL,keys,2)==0);
    }
    printf("PASS %u real warm-ticket roster checks\n",checks);
    return 0;
}

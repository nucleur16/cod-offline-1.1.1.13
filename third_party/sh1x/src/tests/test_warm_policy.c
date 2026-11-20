/* Prepared-state ownership failures; no game engine. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "warm_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(name,condition) do { ++checks; if (!(condition)) { fprintf(stderr,"FAIL %s:%d\n",name,__LINE__); abort(); } } while(0)
static const char *prep="0123456789abcdef0123456789abcdef";
static const char *lease="11111111111111111111111111111111";
static const char *other="22222222222222222222222222222222";
static const char *digest="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static const char *changed="1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

static void prepared(warm_policy_t *s)
{
    WarmPolicyInit(s,prep);
    WarmPolicyBeginMap(s);
    WarmPolicyMapReady(s,digest);
    CHECK("ready",s->phase==WARM_READY);
}

int main(void)
{
    warm_policy_t s;
    unsigned i;
    WarmPolicyInit(&s,"");
    CHECK("default leaves existing servers alone",s.phase==WARM_OFF &&
        WarmPolicyAdmit(&s) && !WarmPolicyFrame(&s,NULL,3));
    WarmPolicyInit(&s,"bad");
    CHECK("bad preparation fails closed",s.phase==WARM_INVALID && !WarmPolicyAdmit(&s));
    WarmPolicyInit(&s,"00000000000000000000000000000000");
    CHECK("zero preparation rejected",s.phase==WARM_INVALID);
    WarmPolicyInit(&s,prep);
    CHECK("loading is not admission",!WarmPolicyAdmit(&s));
    CHECK("cannot activate before ready",!WarmPolicyActivate(&s,prep,lease,digest,0));
    WarmPolicyMapReady(&s,digest);
    CHECK("missing real map begin invalidates",s.phase==WARM_INVALID);
    prepared(&s);
    for (i=0;i<30000;++i)
        if (!WarmPolicyFrame(&s,digest,0)) abort();
    CHECK("thirty thousand idle polls stay unassigned",s.phase==WARM_READY && !s.lease[0]);
    CHECK("ready cannot admit",!WarmPolicyAdmit(&s));
    CHECK("wrong process cannot claim",!WarmPolicyActivate(&s,other,lease,digest,0));
    CHECK("wrong selection cannot claim",!WarmPolicyActivate(&s,prep,lease,changed,0));
    CHECK("malformed lease cannot claim",!WarmPolicyActivate(&s,prep,"bad",digest,0));
    CHECK("refusals preserve preparation",s.phase==WARM_READY && !s.lease[0]);
    CHECK("first assignment accepted",WarmPolicyActivate(&s,prep,lease,digest,0)==1);
    CHECK("activated admission still needs external ticket",WarmPolicyAdmit(&s));
    CHECK("duplicate same assignment accepted",WarmPolicyActivate(&s,prep,lease,digest,2)==2);
    CHECK("duplicate cannot change lease",!WarmPolicyActivate(&s,prep,other,digest,0));
    CHECK("resume discards wall interval once",WarmPolicyFrame(&s,digest,1));
    CHECK("simulation resumes next frame",!WarmPolicyFrame(&s,digest,1));
    CHECK("later retry does not pause again",WarmPolicyActivate(&s,prep,lease,digest,1)==2 &&
        !WarmPolicyFrame(&s,digest,1));
    WarmPolicyBeginMap(&s);WarmPolicyMapReady(&s,changed);
    CHECK("ordinary active rotation never reparks",s.phase==WARM_ACTIVE &&
        !WarmPolicyFrame(&s,changed,0) && !strcmp(s.lease,lease));
    prepared(&s);WarmPolicyBeginMap(&s);WarmPolicyMapReady(&s,digest);
    CHECK("unclaimed map reload cannot reuse preparation",s.phase==WARM_INVALID &&
        !WarmPolicyActivate(&s,prep,lease,digest,0));
    prepared(&s);
    CHECK("changed unclaimed settings invalidate",WarmPolicyFrame(&s,changed,0) &&
        s.phase==WARM_INVALID && !WarmPolicyAdmit(&s));
    prepared(&s);
    CHECK("unknown selection invalidates",WarmPolicyFrame(&s,NULL,0) && s.phase==WARM_INVALID);
    prepared(&s);
    CHECK("unexpected connected human is preserved",!WarmPolicyFrame(&s,digest,1) &&
        s.phase==WARM_INVALID && !WarmPolicyAdmit(&s));
    prepared(&s);
    CHECK("occupied activation is refused",!WarmPolicyActivate(&s,prep,lease,digest,1) &&
        s.phase==WARM_INVALID);
    printf("PASS %u warm ownership/freeze checks\n",checks);
    return 0;
}

#include "../src/p2p/p2p_match_options.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    P2PRoomContext o;
    const char *campaign[]={"coop_prolog_mp_crash_intro","coop","coop"};
    assert(!P2PParseRoomContext(3,campaign,&o));
    assert(!strcmp(o.map,"mp_crash_intro") && !strcmp(o.session,campaign[0]));
    const char *survival[]={"coop_survival_mp_fav_tropical","coop","survival"};
    assert(!P2PParseRoomContext(3,survival,&o));
    assert(!strcmp(o.map,"mp_fav_tropical") && !strcmp(o.session,"mp_fav_tropical") && !strcmp(o.gametype,"survival"));
    survival[0]="mp_cross_fire";survival[1]="survival";
    assert(!P2PParseRoomContext(3,survival,&o) && !strcmp(o.session,"mp_cross_fire"));
    const char *mp[]={"mp_killhouse_sh","tdm","mp"};
    assert(!P2PParseRoomContext(3,mp,&o) && !strcmp(o.gametype,"war"));
    const char *zm[]={"zm_factory_sh","zm","zm"};
    assert(!P2PParseRoomContext(3,zm,&o) && !strcmp(o.map,"zm_factory_sh"));
    mp[0]="mp_rust;quit";assert(P2PParseRoomContext(3,mp,&o));
    mp[0]="coop_spec_mp_rust";assert(P2PParseRoomContext(3,mp,&o));
    campaign[0]="coop_survival_mp_fav_tropical";assert(P2PParseRoomContext(3,campaign,&o));
    zm[0]="mp_rust";assert(P2PParseRoomContext(3,zm,&o));
    assert(P2PParseRoomContext(2,mp,&o));
    /* V2 (startup protocols 2 and 3): the same context plus "<present> <tier>". */
    const char *mp2[]={"mp_killhouse_sh","tdm","mp","0","-1"};
    assert(!P2PParseRoomContextV2(5,mp2,&o) && !strcmp(o.gametype,"war") && !o.difficultyPresent && o.difficulty==-1);
    mp2[3]="1";mp2[4]="2";assert(P2PParseRoomContextV2(5,mp2,&o)); /* an MP league never becomes a tier */
    const char *zm2[]={"zm_factory_sh","zm","zm","1","0"};
    assert(!P2PParseRoomContextV2(5,zm2,&o) && o.difficultyPresent && o.difficulty==0);
    zm2[4]="3";assert(!P2PParseRoomContextV2(5,zm2,&o) && o.difficulty==3 && !strcmp(o.map,"zm_factory_sh"));
    zm2[4]="4";assert(P2PParseRoomContextV2(5,zm2,&o));
    zm2[4]="03";assert(P2PParseRoomContextV2(5,zm2,&o));
    zm2[4]="";assert(P2PParseRoomContextV2(5,zm2,&o));
    zm2[3]="0";zm2[4]="-1";assert(P2PParseRoomContextV2(5,zm2,&o)); /* Zombies needs its tier */
    zm2[3]="1";zm2[4]="-1";assert(P2PParseRoomContextV2(5,zm2,&o));
    zm2[3]="2";zm2[4]="1";assert(P2PParseRoomContextV2(5,zm2,&o));
    const char *coop2[]={"coop_prolog_mp_crash_intro","coop","coop","1","2"};
    assert(!P2PParseRoomContextV2(5,coop2,&o) && o.difficultyPresent && o.difficulty==2 && !strcmp(o.map,"mp_crash_intro"));
    coop2[3]="0";coop2[4]="-1";assert(!P2PParseRoomContextV2(5,coop2,&o) && !o.difficultyPresent);
    coop2[0]="mp_rust;quit";assert(P2PParseRoomContextV2(5,coop2,&o)); /* the V1 refusals still apply */
    assert(P2PParseRoomContextV2(3,campaign,&o) && P2PParseRoomContextV2(4,zm2,&o));
    o.difficulty=7;zm2[3]="1";zm2[4]="9";assert(P2PParseRoomContextV2(5,zm2,&o) && o.difficulty==7); /* a refusal leaves *out alone */
    puts("PASS: MP/Zombies/Campaign context, direct MP Survival identity, V2 tier rules and invalid input rejection");
    return 0;
}

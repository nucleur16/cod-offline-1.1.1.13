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
    puts("PASS: MP/Zombies/Campaign context, direct MP Survival identity and invalid input rejection");
    return 0;
}

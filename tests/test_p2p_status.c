#include "../p2p_status.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const char *ready="sh1x_status map=mp_fav_tropical session_map=mp_fav_tropical gametype=survival type=survival running=1 parked=0 humans=0 bots=0";
    const char *mission="sh1x_status map=mp_killhouse_sh session_map=coop_training_mp_killhouse_sh gametype=coop type=coop running=1 parked=0";
    assert(P2PStatusMatches(ready,"mp_fav_tropical","mp_fav_tropical","survival","survival"));
    assert(P2PStatusMatches(ready,"mp_fav_tropical","","survival","survival"));
    assert(P2PStatusMatches(mission,"mp_killhouse_sh","coop_training_mp_killhouse_sh","coop","coop"));
    assert(!P2PStatusMatches(mission,"mp_killhouse_sh","coop_spec_mp_killhouse_sh","coop","coop"));
    assert(!P2PStatusMatches(ready,"mp_cross_fire","mp_cross_fire","survival","survival"));
    assert(!P2PStatusMatches(ready,"mp_fav_tropical","coop_survival_mp_fav_tropical","survival","survival"));
    assert(!P2PStatusMatches(ready,"mp_fav_tropical","mp_fav_tropical","war","survival"));
    assert(!P2PStatusMatches(ready,"mp_fav_tropical","mp_fav_tropical","survival","mp"));
    assert(!P2PStatusMatches("map=mp_rust session_map=mp_rust gametype=war type=mp running=0 parked=0","mp_rust","mp_rust","war","mp"));
    assert(!P2PStatusMatches("map=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=1","mp_rust","mp_rust","war","mp"));
    assert(!P2PStatusMatches("map=mp_rust session_map=mp_rust gametype=war type=mp parked=0","mp_rust","mp_rust","war","mp"));
    assert(!P2PStatusMatches("map=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=0 map=mp_rust","mp_rust","mp_rust","war","mp"));
    assert(!P2PStatusMatches("othermap=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=0","mp_rust","mp_rust","war","mp"));
    assert(!P2PStatusMatches("map=mp_rust_extra session_map=mp_rust gametype=war type=mp running=1 parked=0","mp_rust","mp_rust","war","mp"));
    assert(!P2PStatusMatches(NULL,"mp_rust","mp_rust","war","mp"));
    puts("PASS: exact running match readiness, Survival MP identity, mission distinction and malformed status rejection");
    return 0;
}

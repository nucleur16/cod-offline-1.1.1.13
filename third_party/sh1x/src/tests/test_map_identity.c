#include "sv_state.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    ResetServerConfigstrings();
    assert(!ServerMapIdentityMatches("mp_killhouse_sh","mp"));
    assert(!SetServerMapIdentity("../mp_killhouse_sh","mp"));
    assert(!SetServerMapIdentity("mp_killhouse_sh;quit","mp"));
    assert(!SetServerMapIdentity("MP_KILLHOUSE_SH","mp"));
    assert(!SetServerMapIdentity("mp_killhouse_sh","tdm"));
    assert(SetServerMapIdentity("mp_killhouse_sh","mp"));
    assert(SetServerMapIdentity("mp_killhouse_sh","mp"));
    assert(!SetServerMapIdentity("mp_cargoship_sh","mp"));
    assert(SetServerInheritedConfigstrings("mp",0,NULL));
    assert(ServerMapIdentityMatches("mp_killhouse_sh","mp"));
    assert(!ServerMapIdentityMatches("mp_cargoship_sh","mp"));
    assert(!ServerMapIdentityMatches("mp_killhouse_sh","coop"));
    ResetServerConfigstrings();
    assert(SetServerMapIdentity("mp_cargoship_sh","mp"));
    assert(SetServerInheritedConfigstrings("mp",0,NULL));
    assert(ServerMapIdentityMatches("mp_cargoship_sh","mp"));
    puts("PASS: mounted map/configstring identity is atomic and map-scoped");
    return 0;
}

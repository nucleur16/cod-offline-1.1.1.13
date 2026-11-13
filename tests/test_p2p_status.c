#include "../src/p2p/p2p_status.h"
#include <stdio.h>

/* One row per case: the status line, the requested match and the expected verdict.
 * (A table, not one assert per case: the assert form of this file compiled, under every optimisation level,
 * to a program that Windows Defender's heuristics refused to run as "Trojan:Win32/CobaltStrike.UTA!MTB".) */
static const char READY[]="sh1x_status map=mp_fav_tropical session_map=mp_fav_tropical gametype=survival type=survival running=1 parked=0 humans=0 bots=0";
static const char MISSION[]="sh1x_status map=mp_killhouse_sh session_map=coop_training_mp_killhouse_sh gametype=coop type=coop running=1 parked=0";
static const struct {
    const char *line, *map, *session, *gametype, *category;
    int matches;
} CASES[]={
    {READY,"mp_fav_tropical","mp_fav_tropical","survival","survival",1},
    {READY,"mp_fav_tropical","","survival","survival",1},
    {MISSION,"mp_killhouse_sh","coop_training_mp_killhouse_sh","coop","coop",1},
    {MISSION,"mp_killhouse_sh","coop_spec_mp_killhouse_sh","coop","coop",0},
    {READY,"mp_cross_fire","mp_cross_fire","survival","survival",0},
    {READY,"mp_fav_tropical","coop_survival_mp_fav_tropical","survival","survival",0},
    {READY,"mp_fav_tropical","mp_fav_tropical","war","survival",0},
    {READY,"mp_fav_tropical","mp_fav_tropical","survival","mp",0},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=0 parked=0","mp_rust","mp_rust","war","mp",0},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=1","mp_rust","mp_rust","war","mp",0},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp parked=0","mp_rust","mp_rust","war","mp",0},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=0 map=mp_rust","mp_rust","mp_rust","war","mp",0},
    {"othermap=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=0","mp_rust","mp_rust","war","mp",0},
    {"map=mp_rust_extra session_map=mp_rust gametype=war type=mp running=1 parked=0","mp_rust","mp_rust","war","mp",0},
    {NULL,"mp_rust","mp_rust","war","mp",0},
};

int main(void)
{
    unsigned i, failed=0;
    for (i=0; i<sizeof CASES/sizeof *CASES; ++i)
        if (!!P2PStatusMatches(CASES[i].line,CASES[i].map,CASES[i].session,CASES[i].gametype,CASES[i].category)!=CASES[i].matches) {
            printf("FAIL: case %u expected %d\n",i,CASES[i].matches);
            ++failed;
        }
    if (failed) return 1;
    puts("PASS: exact running match readiness, Survival MP identity, mission distinction and malformed status rejection");
    return 0;
}

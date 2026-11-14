#include "../src/p2p/p2p_status.h"
#include <stdio.h>

/* One row per case: the status line, the requested match and the expected verdict.
 * (A table, not one assert per case: the assert form of this file compiled, under every optimisation level,
 * to a program that Windows Defender's heuristics refused to run as "Trojan:Win32/CobaltStrike.UTA!MTB".) */
static const char READY[]="sh1x_status map=mp_fav_tropical session_map=mp_fav_tropical gametype=survival type=survival running=1 parked=0 humans=0 bots=0";
static const char MISSION[]="sh1x_status map=mp_killhouse_sh session_map=coop_training_mp_killhouse_sh gametype=coop type=coop running=1 parked=0";
/* After rcon `map mp_rust` on a Killhouse room: codol_sessionMap keeps the room's spelling. */
static const char MOVED[]="sh1x_status map=mp_rust session_map=mp_killhouse_sh gametype=war type=mp running=1 ended=0 humans=1 bots=4 maxclients=16 parked=0 quick=0";
static const char MOVED_DM[]="sh1x_status map=mp_backlot_sh session_map=mp_killhouse_sh gametype=dm type=mp running=1 ended=0 humans=1 bots=4 parked=0";
static const char LOADING[]="sh1x_status map=none session_map=none gametype=war type=mp running=0 ended=0 humans=0 bots=0 maxclients=0 parked=0";
static const char PARKED[]="sh1x_status map=none session_map=none gametype=coop type=coop running=0 ended=0 humans=0 bots=0 maxclients=0 parked=1";
static const struct {
    const char *line, *map, *session, *gametype, *category;
    int matches, kind;
} CASES[]={
    {READY,"mp_fav_tropical","mp_fav_tropical","survival","survival",1,P2P_STATUS_EXACT},
    {READY,"mp_fav_tropical","","survival","survival",1,P2P_STATUS_EXACT},
    {MISSION,"mp_killhouse_sh","coop_training_mp_killhouse_sh","coop","coop",1,P2P_STATUS_EXACT},
    {MISSION,"mp_killhouse_sh","coop_spec_mp_killhouse_sh","coop","coop",0,P2P_STATUS_LIVE},
    {READY,"mp_cross_fire","mp_cross_fire","survival","survival",0,P2P_STATUS_LIVE},
    {READY,"mp_fav_tropical","coop_survival_mp_fav_tropical","survival","survival",0,P2P_STATUS_LIVE},
    {READY,"mp_fav_tropical","mp_fav_tropical","war","survival",0,P2P_STATUS_LIVE},
    {READY,"mp_fav_tropical","mp_fav_tropical","survival","mp",0,P2P_STATUS_LIVE},
    /* a map change on a ready host: not the requested match, but a live one the session follows */
    {MOVED,"mp_killhouse_sh","mp_killhouse_sh","war","mp",0,P2P_STATUS_LIVE},
    {MOVED,"mp_rust","mp_killhouse_sh","war","mp",1,P2P_STATUS_EXACT},
    {MOVED_DM,"mp_killhouse_sh","mp_killhouse_sh","war","mp",0,P2P_STATUS_LIVE},
    /* answering, but no level: never a live match (a load under way, a failed load, a parked mission) */
    {LOADING,"mp_killhouse_sh","mp_killhouse_sh","war","mp",0,P2P_STATUS_IDLE},
    {PARKED,"mp_killhouse_sh","coop_training_mp_killhouse_sh","coop","coop",0,P2P_STATUS_IDLE},
    {"map=none session_map=none gametype=war type=mp running=1 parked=0","mp_rust","mp_rust","war","mp",0,P2P_STATUS_IDLE},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=0 parked=0","mp_rust","mp_rust","war","mp",0,P2P_STATUS_IDLE},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=1","mp_rust","mp_rust","war","mp",0,P2P_STATUS_IDLE},
    /* malformed or ambiguous: not an answer at all */
    {"map=mp_rust session_map=mp_rust gametype=war type=mp parked=0","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=1","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=yes parked=0","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=0 map=mp_rust","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"map=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=0 map=mp_crash","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"othermap=mp_rust session_map=mp_rust gametype=war type=mp running=1 parked=0","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"map= session_map=mp_rust gametype=war type=mp running=1 parked=0","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"map=mp_rust session_map=mp_rust type=mp running=1 parked=0","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"map=mp_rust_extra session_map=mp_rust gametype=war type=mp running=1 parked=0","mp_rust","mp_rust","war","mp",0,P2P_STATUS_LIVE},
    {"Bad rcon password.","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {"","mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
    {NULL,"mp_rust","mp_rust","war","mp",0,P2P_STATUS_BAD},
};

int main(void)
{
    unsigned i, failed=0;
    char ident[160];
    for (i=0; i<sizeof CASES/sizeof *CASES; ++i) {
        int kind=P2PStatusClassify(CASES[i].line,CASES[i].map,CASES[i].session,CASES[i].gametype,CASES[i].category);
        if (!!P2PStatusMatches(CASES[i].line,CASES[i].map,CASES[i].session,CASES[i].gametype,CASES[i].category)!=CASES[i].matches) {
            printf("FAIL: case %u expected match %d\n",i,CASES[i].matches);
            ++failed;
        }
        if (kind!=CASES[i].kind) {
            printf("FAIL: case %u classified %d, expected %d\n",i,kind,CASES[i].kind);
            ++failed;
        }
    }
    P2PStatusIdentity(MOVED,ident,sizeof(ident));
    if (strcmp(ident,"mp_rust mp_killhouse_sh war mp")) { printf("FAIL: identity \"%s\"\n",ident); ++failed; }
    P2PStatusIdentity(MOVED_DM,ident,sizeof(ident));
    if (strcmp(ident,"mp_backlot_sh mp_killhouse_sh dm mp")) { printf("FAIL: identity \"%s\"\n",ident); ++failed; }
    P2PStatusIdentity(NULL,ident,sizeof(ident));
    if (strcmp(ident,"? ? ? ?")) { printf("FAIL: identity \"%s\"\n",ident); ++failed; }
    P2PStatusIdentity(MOVED,ident,8);
    if (strlen(ident)>=8) { printf("FAIL: identity overflow \"%s\"\n",ident); ++failed; }
    if (failed) return 1;
    puts("PASS: exact running match readiness, Survival MP identity, mission distinction, malformed status rejection, "
         "map-change following (live vs idle vs bad status) and match identity");
    return 0;
}

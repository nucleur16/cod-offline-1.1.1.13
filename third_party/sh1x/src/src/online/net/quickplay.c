/* Continuous Quick Play lifecycle. SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * A worker freezes an installed MP rotation for one lease. Players already
 * connected follow the ordinary loadingnewmap/gamestate path. A new connect
 * additionally needs a short-lived ticket for THIS process lease and map
 * epoch, checked on the game thread after credential authentication. A master
 * status poll alone cannot authorize a connection across a map-change race.
 */
#include "quickplay.h"
#include "admission.h"
#include "warm_standby.h"
#include "match_report.h"
#include "connect.h"
#include "server.h"
#include "cvar.h"
#include "g_shared.h"
#include <limits.h>
#include <string.h>

static unsigned epoch;
static int loading;
/* Game VM/gclient are rebuilt on every full map load. Keep authenticated
 * session assignments beside native connection identities, not in gclient. */
static struct {
    uint64_t uid;
    party_assignment_t assignment;
} parties[MAX_CLIENTS];
static char party_session[33];

static int valid_session(const char *s)
{
    unsigned i;
    if (!s || strlen(s) != 32) return 0;
    for (i=0;i<32;++i)
        if (!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f'))) return 0;
    return 1;
}

const char *QuickPlaySession(void)
{
    const char *s=Cvar_GetVariantString("quickSession");
    return valid_session(s) ? s : "-";
}

int QuickPlayEnabled(void)
{
    return Cvar_VariableBooleanValue("sv_quickPlay") &&
        Cvar_VariableIntegerValue("sv_playlistId")==120 &&
        !strcmp(Cvar_GetVariantString("sv_zoneType"),"mp") &&
        valid_session(Cvar_GetVariantString("quickSession"));
}

int QuickPlayPartiesEnabled(void)
{
    return QuickPlayEnabled() && Cvar_VariableBooleanValue("quickParties");
}

static void party_scope(void)
{
    const char *session=QuickPlaySession();
    if (!QuickPlayPartiesEnabled() || strcmp(party_session,session)) {
        memset(parties,0,sizeof(parties));
        party_session[0]=0;
        if (QuickPlayPartiesEnabled()) memcpy(party_session,session,33);
    }
}

void QuickPlayClearClient(unsigned client)
{
    if (client<MAX_CLIENTS) memset(&parties[client],0,sizeof(parties[client]));
}

unsigned QuickPlayClientTeam(unsigned client)
{
    party_scope();
    if (client>=MAX_CLIENTS || !QuickPlayPartiesEnabled()) return 0;
    if (!parties[client].uid ||
        parties[client].uid!=ClientConnectIdentity(client) ||
        svs.clients[client].netchan.remoteAddress.type==NA_BOT) {
        QuickPlayClearClient(client);
        return 0;
    }
    return parties[client].assignment.team;
}

const char *QuickPlayClientGroup(unsigned client)
{
    return QuickPlayClientTeam(client) ? parties[client].assignment.group : "";
}

unsigned QuickPlayEpoch(void)
{ return QuickPlayEnabled() ? epoch : 0; }

void QuickPlayBeginMap(void)
{
    /* Called before shutdown/loadingnewmap, outside SV_ClearServer. A round
     * map_restart keeps its epoch; a full map spawn always invalidates tickets.
     * Saturation fails closed instead of making ancient epoch-1 tickets live. */
    loading=1;
    party_scope();
    if (epoch<INT_MAX) ++epoch;
}

void QuickPlayMapReady(void) { loading=0; }

int QuickPlayJoinable(void)
{
    return QuickPlayEnabled() && epoch && epoch<INT_MAX && !loading &&
        !WarmStandbyUnassigned() &&
        com_sv_running && com_sv_running->integer && sv.state==SS_GAME &&
        !onExitLevelExecuted && !Cvar_VariableIntegerValue("scr_gameended") &&
        !MatchReportTransitionHeld();
}

int QuickPlayAdmit(uint64_t uid)
{
    party_assignment_t assignment;
    if (!Cvar_VariableBooleanValue("sv_quickPlay")) return 1;
    return QuickPlayJoinable() && uid &&
        AdmissionQuickTicket(Cvar_VariableIntegerValue("net_port"),
            QuickPlaySession(),epoch,uid) &&
        (!QuickPlayPartiesEnabled() ||
         AdmissionQuickParty(Cvar_VariableIntegerValue("net_port"),
            QuickPlaySession(),uid,&assignment));
}

int QuickPlayBindClient(unsigned client, uint64_t uid)
{
    party_assignment_t assignment;
    unsigned i,side_count=0;
    party_scope();
    if (client>=MAX_CLIENTS) return 0;
    QuickPlayClearClient(client);
    if (!QuickPlayPartiesEnabled()) return 1;
    if (!uid || uid!=ClientConnectIdentity(client) ||
        svs.clients[client].netchan.remoteAddress.type==NA_BOT ||
        !QuickPlayAdmit(uid) ||
        !AdmissionQuickParty(Cvar_VariableIntegerValue("net_port"),
            QuickPlaySession(),uid,&assignment)) return 0;
    for (i=0;i<MAX_CLIENTS;++i) {
        unsigned side=QuickPlayClientTeam(i);
        if (!side) continue;
        /* Prevent a late roster refresh from splitting an already admitted
         * group or changing its UID's group. The master publishes immutable
         * assignments for the lifetime of this Quick session. */
        if (parties[i].uid==uid) return 0;
        if (!strcmp(parties[i].assignment.group,assignment.group) && side!=assignment.team) return 0;
        if (side==assignment.team) ++side_count;
    }
    if (side_count>=6) return 0;
    parties[client].uid=uid;
    parties[client].assignment=assignment;
    return 1;
}

int QuickPlayBotSeat(void)
{
    int i, first=Cvar_VariableIntegerValue("sv_privateClients");
    if (!QuickPlayJoinable() || !sv_maxclients || first<0 ||
        sv_maxclients->integer<1 || sv_maxclients->integer>MAX_CLIENTS) return -1;
    for (i=sv_maxclients->integer-1;i>=first;--i)
        if (i<MAX_CLIENTS && svs.clients[i].state>=CS_CONNECTED &&
            svs.clients[i].netchan.remoteAddress.type==NA_BOT &&
            MatchReportCanReuseSlot((unsigned)i)) return i;
    return -1;
}

static int training_human_limit(void)
{
    int playlist;
    /* These are the existing native-written launch fields, not a new flag
     * without a producer: training_policy.py plus launch-match.sh and
     * match_server_control.prepare all select this exact profile. */
    if (Cvar_VariableBooleanValue("sv_quickPlay") || !sv_maxclients ||
        strcmp(Cvar_GetVariantString("sv_zoneType"),"mp") ||
        sv_maxclients->integer!=12 ||
        Cvar_VariableIntegerValue("scr_botfill")!=12 ||
        Cvar_VariableIntegerValue("scr_botmax")!=11 ||
        Cvar_VariableIntegerValue("scr_bots")!=0) return 0;
    playlist=Cvar_VariableIntegerValue("sv_playlistId");
    if (playlist==1) return 1;
    if (playlist==3 || (playlist==0 &&
        !strcmp(Cvar_GetVariantString("g_gametype"),"combat"))) return 3;
    return 0;
}

int TrainingAdmit(uint64_t uid, int reconnect_slot)
{
    uint64_t humans[MAX_CLIENTS];
    int i,j,count=0,limit=training_human_limit();
    if (!limit) return 1; /* Not this explicit Training profile. */
    if (!uid || loading || !com_sv_running || !com_sv_running->integer ||
        sv.state!=SS_GAME || onExitLevelExecuted ||
        Cvar_VariableIntegerValue("scr_gameended") ||
        MatchReportTransitionHeld()) return 0;
    for (i=0;i<sv_maxclients->integer;++i) {
        uint64_t current;
        if (svs.clients[i].state<CS_CONNECTED) continue;
        if (svs.clients[i].state>CS_ACTIVE) return 0;
        if (svs.clients[i].netchan.remoteAddress.type==NA_BOT) continue;
        current=ClientConnectIdentity((unsigned)i);
        if (!current) return 0;
        /* Only the exact existing UID/slot is a reconnect, never an arbitrary
         * private/free slot selected earlier by DirectConnect. */
        if (i==reconnect_slot && current==uid) continue;
        if (current==uid) return 0;
        for (j=0;j<count;++j) if (humans[j]==current) return 0;
        humans[count++]=current;
    }
    /* Includes CONNECTED/PRIMED arrivals, before their first player snapshot.
     * This guard also runs when a public/private seat is already free. */
    return count<limit;
}

int TrainingBotSeat(uint64_t uid)
{
    int i,active=0,first=Cvar_VariableIntegerValue("sv_privateClients");
    if (!training_human_limit() || !TrainingAdmit(uid,-1) ||
        first<0 || first>=sv_maxclients->integer) return -1;
    for (i=0;i<sv_maxclients->integer;++i)
        if (svs.clients[i].state==CS_ACTIVE &&
            svs.clients[i].netchan.remoteAddress.type!=NA_BOT &&
            ClientConnectIdentity((unsigned)i)) active=1;
    if (!active) return -1;
    for (i=sv_maxclients->integer-1;i>=first;--i)
        if (svs.clients[i].state>=CS_CONNECTED &&
            svs.clients[i].netchan.remoteAddress.type==NA_BOT &&
            MatchReportCanReuseSlot((unsigned)i)) return i;
    return -1;
}

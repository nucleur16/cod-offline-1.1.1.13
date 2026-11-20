#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "persistent_events.h"
#endif
/* sh1x_status: one-line match state for the lobby's instance manager.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The lobby polls every match server over loopback RCON and decides from this
 * line where a player who presses Ready goes (join the running map, re-point an
 * empty server, or start a new instance). One line, key=value, no spaces in
 * values:
 *
 *   sh1x_status map=mp_rust session_map=mp_rust gametype=dm type=mp
 *               running=1 humans=2 bots=10 maxclients=18
 *               uids=1a4e399b1594b01e,9d93949b116047cb
 *
 * humans counts connected, non-bot clients (CS_CONNECTED and up, so a player who
 * is still loading is already counted); uids lists their connect identities.
 *
 * A server with no level loaded (a map load that ended in ERR_DROP, which
 * leaves the process answering rcon but not getchallenge) reports map=none,
 * running=0: the mapname dvar still names the old level, and reporting it let
 * the lobby answer "already on <map>" and send players to a dead server, where
 * they sat on "awaiting connection" until EXE_ERR_SERVER_TIMEOUT. With map=none
 * the lobby's placement sees a different map and dispatches a fresh `map`.
 *
 * A coop server whose mission has ended and whose players went back to the
 * lobby (CoopExitLevelParks) loads nothing further: it reports the same
 * map=none, running=0, plus parked=1, so the lobby's next Ready re-points it
 * with a fresh `map` -- even for the same mission, whose finished level must
 * not be rejoined. parked is 0 otherwise and is cleared by the next level load
 * (SV_ClearServer). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "server.h"
#include "cvar.h"
#include "cmd.h"
#include "qcommon_io.h"
#include "connect.h"
#include "g_shared.h"
#include "quickplay.h"
#include "warm_standby.h"

static const char *token(const char *name, const char *fallback)
{
    cvar_t *v = Cvar_FindVar(name);
    const char *s = v && v->string[0] ? v->string : fallback;
    return strchr(s, ' ') ? fallback : s;
}

static int match_parked;

void MatchStatusSetParked(int parked)
{
    match_parked = parked ? 1 : 0;
}

int MatchStatusParked(void)
{
    return match_parked;
}

void MatchStatus_f(void)
{
    char uids[18 * 64 + 1];
    size_t used = 0;
    int i, humans = 0, bots = 0, max = sv_maxclients ? sv_maxclients->integer : 0;
    int running = com_sv_running && com_sv_running->integer && sv.state == SS_GAME &&
                  !match_parked;
    const char *map = running ? token("mapname", "none") : "none";
    const char *session_map = running ? token("sv_sessionMap", map) : "none";
    uids[0] = 0;
    if (!running) max = 0;
    for (i = 0; i < max; ++i) {
        client_t *cl = &svs.clients[i];
        if (cl->state < CS_CONNECTED) continue;
        if (cl->netchan.remoteAddress.type == NA_BOT) { ++bots; continue; }
        ++humans;
        if (used + 18 < sizeof(uids)) {
            uint64_t uid = ClientConnectIdentity((unsigned)i);
            used += (size_t)snprintf(uids + used, sizeof(uids) - used, "%s%08x%08x", used ? "," : "",
                                     (unsigned)(uid >> 32), (unsigned)uid);
        }
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "sh1x_status map=%s session_map=%s gametype=%s type=%s running=%d ended=%d humans=%d bots=%d maxclients=%d parked=%d quick=%d quick_session=%s epoch=%u joinable=%d quick_parties=%d uids=%s %s %s %s\n",
        map, session_map,
        token("g_gametype", "none"), token("sv_zoneType", "mp"),
        running, running && onExitLevelExecuted, humans, bots, max, match_parked,
        QuickPlayEnabled(),QuickPlaySession(),QuickPlayEpoch(),
        QuickPlayJoinable(),QuickPlayPartiesEnabled(),
        uids[0] ? uids : "-",WarmStandbyStatus(),PersistentEventsStatus(),PersistentRuntimeStatus());
}

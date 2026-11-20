#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* A match server that lost its level loads one again by itself.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Com_Error(ERR_DROP / ERR_DISCONNECT / ERR_SERVERDISCONNECT) ends in
 * SV_Shutdown, which frees the level and clears com_sv_running, and a failed
 * map load ends the same way. The retained Quake loop then has nothing to do:
 * SV_Frame slept 20 ms and returned, SV_PacketEvent dropped every packet, and
 * the process sat there unreachable -- no getinfo, no rcon, so the lobby
 * could neither see that the server was down nor send it a `map`.
 *
 * MEASURED, local 17733 2026-10-23 14:44 and 15:06: an end-of-match script
 * runtime error (_spawnlogic.gsc, player.wasTI undefined for a top-players
 * actor) under developer 1 became Com_Error(4) -> "Server Shutdown: Server
 * disconnected", and the server stayed down until it was restarted by hand.
 *
 * Two parts:
 *   - SV_PacketEvent answers rcon while no level runs (sv_main.c), so
 *     sh1x_status reports running=0 map=none and the lobby can dispatch;
 *   - this file reloads the last level that loaded (category, session map,
 *     gametype and map, dispatched the way the lobby does it) after
 *     recoverDelay ms, then retries with a growing delay if that load
 *     fails too. A `map` from the lobby in the meantime wins: a level that
 *     loads disarms the retry.
 *
 * `killserver` is an operator's deliberate stop and is left alone. */
#include <stdio.h>
#include <string.h>
#include "q_shared.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "cvar.h"
#include "cmd.h"
#include "server.h"
#include "sys_net.h"
#include "sys_main.h"
#include "server_recovery.h"

static struct {
    int valid;           /* a level has loaded since the process started */
    int operatorStop;    /* killserver: stay down */
    int downSince;       /* Sys_Milliseconds when the idle state was first seen; 0 = up */
    int nextAttempt;     /* Sys_Milliseconds of the next reload */
    int attempts;
    char map[MAX_QPATH];
    char session[MAX_QPATH];
    char category[MAX_QPATH];
    char gametype[MAX_QPATH];
} recovery;

static cvar_t *recoverDelay;

static void copy_dvar(char *out, size_t size, const char *name, const char *fallback)
{
    cvar_t *v = Cvar_FindVar(name);
    const char *s = v && v->string && v->string[0] ? v->string : fallback;
    Q_strncpyz(out, s ? s : "", size);
}

static int has_unsafe_char(const char *s)
{
    for (; *s; ++s)
        if (*s == ';' || *s == '"' || *s == '\n' || *s == '\r' || *s == ' ')
            return 1;
    return 0;
}

void ServerRecoveryLevelLoaded(void)
{
    if (!recoverDelay)
        recoverDelay = Cvar_RegisterInt("recoverDelay", 2000, 0, 600000, 0,
            "Milliseconds a dedicated server that lost its level (an error shut it down or a map load failed) "
            "waits before it reloads the last level by itself; 0 = never");
    if (recovery.downSince)
        Com_Printf(CON_CHANNEL_SERVER, "CoDOL recovery: a level is running again after %d ms down (%d reload attempt(s))\n",
            Sys_Milliseconds() - recovery.downSince, recovery.attempts);
    copy_dvar(recovery.map, sizeof(recovery.map), "mapname", "");
    copy_dvar(recovery.session, sizeof(recovery.session), "sv_sessionMap", recovery.map);
    copy_dvar(recovery.category, sizeof(recovery.category), "sv_zoneType", "mp");
    copy_dvar(recovery.gametype, sizeof(recovery.gametype), "g_gametype", "dm");
    recovery.valid = recovery.map[0] && !has_unsafe_char(recovery.map) && !has_unsafe_char(recovery.session) &&
                     !has_unsafe_char(recovery.category) && !has_unsafe_char(recovery.gametype);
    recovery.operatorStop = 0;
    recovery.downSince = 0;
    recovery.attempts = 0;
}

void ServerRecoveryOperatorStop(void)
{
    recovery.operatorStop = 1;
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL recovery: killserver - staying down until the next map command\n");
}

static int retry_delay(int attempts)
{
    int base = recoverDelay ? recoverDelay->integer : 2000;
    int delay = base;
    /* 2 s, 4 s, 8 s ... capped at a minute, so a level that cannot load does
     * not spin the loader while the lobby gets its chance to send another. */
    while (attempts-- > 0 && delay < 60000)
        delay *= 2;
    return delay > 60000 ? 60000 : delay;
}

void ServerRecoveryIdleFrame(void)
{
    int now;

    /* Wait for packets instead of sleeping blind: rcon is answered while no
     * level runs (SV_PacketEvent), and NET_Sleep returns as soon as one is
     * there. Com_Frame then reads it. */
    NET_Sleep(20000);
    /* NET_Sleep dispatches what arrived, so a lobby `map` may have loaded a
     * level inside that call. */
    if (com_sv_running && com_sv_running->integer)
        return;

    /* A resident supervisor owns recovery/reconciliation; preserve UDP pumping
     * above, but never reload a previous room from this legacy retry path. */
    if (PersistentRuntimeEnabled()) return;

    if (!recovery.valid || recovery.operatorStop || !com_dedicated || !com_dedicated->integer ||
        !recoverDelay || recoverDelay->integer <= 0)
        return;

    now = Sys_Milliseconds();
    if (!recovery.downSince) {
        recovery.downSince = now ? now : 1;
        recovery.nextAttempt = now + retry_delay(0);
        Com_Printf(CON_CHANNEL_SERVER,
            "CoDOL recovery: no level is running; reloading %s (%s %s, session %s) in %d ms unless a map command comes first\n",
            recovery.map, recovery.category, recovery.gametype, recovery.session, retry_delay(0));
        return;
    }
    if (now - recovery.nextAttempt < 0)
        return;

    ++recovery.attempts;
    recovery.nextAttempt = now + retry_delay(recovery.attempts);
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL recovery: reload attempt %d: %s %s %s (session %s); next try in %d ms if it fails\n",
        recovery.attempts, recovery.category, recovery.gametype, recovery.map, recovery.session,
        retry_delay(recovery.attempts));
    /* The lobby's own dispatch order (sv_main.c SV_DiscardRefusedMatchRequest). */
    Cbuf_AddText(va("set fs_game %s\nset sv_sessionMap %s\nset sv_zoneType %s\nset g_gametype %s\nmap %s\n",
        recovery.category, recovery.session, recovery.category, recovery.gametype, recovery.map));
}

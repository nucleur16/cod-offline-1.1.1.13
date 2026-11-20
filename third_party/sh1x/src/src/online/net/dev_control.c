/* Operator-only development controls for the authenticated local panel.
 * These are server console commands; the lobby sends them over loopback RCON.
 * No client-issued command is registered here. SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "server.h"
#include "g_shared.h"
#include "cmd.h"
#include "qcommon_io.h"
#include "connect.h"
#include "sentient.h"
#include "dev_cheats.h"

_Static_assert(offsetof(gentity_t, flags) == 0x180,
               "development damage and target flags must match retained assembly");

static int dev_slot(const char *value)
{
    char *end;
    long slot;
    if (!value || !*value) return -1;
    slot = strtol(value, &end, 10);
    if (*end || slot < 0 || !sv_maxclients || slot >= sv_maxclients->integer) return -1;
    return (int)slot;
}

void DevClients_f(void)
{
    int i;
    if (!com_sv_running || !com_sv_running->integer || sv.state != SS_GAME) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_clients unavailable: no active match\n");
        return;
    }
    for (i = 0; i < sv_maxclients->integer; ++i) {
        client_t *cl = &svs.clients[i];
        uint64_t uid;
        char name[sizeof(cl->name)];
        size_t j;
        if (cl->state < CS_CONNECTED) continue;
        uid = ClientConnectIdentity((unsigned)i);
        memcpy(name, cl->name, sizeof(name));
        name[sizeof(name) - 1] = 0;
        for (j = 0; j < sizeof(name) - 1 && name[j]; ++j)
            if ((unsigned char)name[j] < 32 || name[j] == '=') name[j] = '_';
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "dev_client slot=%d bot=%d active=%d uid=%08x%08x noclip=%d ufo=%d god=%d demigod=%d notarget=%d name=%s\n",
            i, cl->netchan.remoteAddress.type == NA_BOT ? 1 : 0,
            cl->state == CS_ACTIVE ? 1 : 0,
            (unsigned)(uid >> 32), (unsigned)uid,
            g_entities[i].client ? !!g_entities[i].client->noclip : 0,
            g_entities[i].client ? !!g_entities[i].client->ufo : 0,
            !!(g_entities[i].flags & 1u), !!(g_entities[i].flags & 2u),
            !!(g_entities[i].flags & 4u), name);
    }
}

void DevPlayer_f(void)
{
    int slot, current = 0, desired;
    unsigned flag = 0;
    const char *action, *mode;
    gentity_t *ent;
    if (Cmd_Argc() != 4) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "usage: dev_player <slot> <noclip|ufo|god|demigod|notarget> <on|off|toggle>\n");
        return;
    }
    slot = dev_slot(Cmd_Argv(1));
    if (slot < 0 || !com_sv_running || !com_sv_running->integer || sv.state != SS_GAME ||
        svs.clients[slot].state != CS_ACTIVE ||
        svs.clients[slot].netchan.remoteAddress.type == NA_BOT) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_player refused: no active human in slot\n");
        return;
    }
    ent = &g_entities[slot];
    if (!ent->client) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_player refused: player has no game client\n");
        return;
    }
    action = Cmd_Argv(2);
    mode = Cmd_Argv(3);
    if (!strcmp(action, "noclip")) current = ent->client->noclip ? 1 : 0;
    else if (!strcmp(action, "ufo")) current = ent->client->ufo ? 1 : 0;
    else if (!strcmp(action, "god")) { flag = 1u; current = !!(ent->flags & flag); }
    else if (!strcmp(action, "demigod")) { flag = 2u; current = !!(ent->flags & flag); }
    else if (!strcmp(action, "notarget")) { flag = 4u; current = !!(ent->flags & flag); }
    else {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_player refused: unknown action\n");
        return;
    }
    if (!strcmp(mode, "on")) desired = 1;
    else if (!strcmp(mode, "off")) desired = 0;
    else if (!strcmp(mode, "toggle")) desired = !current;
    else {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_player refused: unknown mode\n");
        return;
    }
    if (desired && ent->health <= 0) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_player refused: player is not alive\n");
        return;
    }
    if (desired && (!g_cheats || !g_cheats->boolean)) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_player refused: enable sv_cheats first\n");
        return;
    }
    /* Share the canonical taint/target state with native cheat commands. */
    if (desired) CheatsTainted();
    if (flag) {
        if (desired) ent->flags |= flag;
        else ent->flags &= ~flag;
        if (flag == 4u) Sentient_SetDontTargetForEntity(ent, desired);
    } else if (!strcmp(action, "noclip")) ent->client->noclip = desired;
    else ent->client->ufo = desired;
    Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_player slot=%d action=%s enabled=%d\n",
               slot, action, desired);
}

/* "dev_cheats on|off": the switch half of dev_cheats (player/dev_cheats.c prints the state). */
void DevCheatsSwitch_f(void)
{
    const char *mode;
    if (Cmd_Argc() != 2 || !sv_cheats || !com_sv_running ||
        !com_sv_running->integer || sv.state != SS_GAME) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,
                   "usage: dev_cheats <on|off> (active match required)\n");
        return;
    }
    mode = Cmd_Argv(1);
    if (!strcmp(mode, "on")) { Cvar_SetBool(sv_cheats, qtrue); CheatsTainted(); }
    else if (!strcmp(mode, "off")) Cvar_SetBool(sv_cheats, qfalse);
    else {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_cheats refused: unknown mode\n");
        return;
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_cheats enabled=%d\n",
               sv_cheats->boolean ? 1 : 0);
}

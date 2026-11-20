/* SPDX-License-Identifier: AGPL-3.0-or-later
 * C1 field orders: SetFOGoal/SetFOProgress/SetFOReward/SetFOStatus and their
 * playerState publication. Evidence: field_orders.h. */
#ifdef OL_SERVER
#include "field_orders.h"
#include "playerstate_ext.h"
#include "scr_vm_functions.h"
#include "g_shared.h"
#include "ol_bot.h"
#include "radar_state.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include <stdio.h>
#include <string.h>

enum { FO_GOAL, FO_PROGRESS, FO_REWARD, FO_STATUS };

/* C1 0x518e80 and its three siblings: a player entity ("entity %i is not a
 * player" otherwise), exactly two parameters (the USAGE error otherwise), then
 * store (Scr_GetInt(1) & 0xff) into slot Scr_GetInt(0). C1 never bounds the
 * index - SetFOGoal(2, x) writes ps+0x1bc, the first progress word - so an
 * out-of-range index is a script error here instead of a neighbour write. */
static void set_word(scr_entref_t ref, int which, const char *usage)
{
    ps_ext_t *ext;
    int index, value;
    uint8_t *words;
    if (ref.classnum || ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client) {
        Scr_ObjectError("field order method requires a player");
        return;
    }
    if (Scr_GetNumParam() != 2) {
        Scr_Error(usage);
        return;
    }
    index = Scr_GetInt(0);
    value = Scr_GetInt(1);
    if (index < 0 || index >= FIELD_ORDER_SLOTS) {
        Scr_ParamError(0, "field order index must be 0 or 1");
        return;
    }
    ext = PlayerStateExt(&g_entities[ref.entnum].client->ps);
    if (!ext) {
        Scr_Error("field order state unavailable");
        return;
    }
    switch (which) {
    case FO_GOAL: words = ext->field_orders.goal; break;
    case FO_PROGRESS: words = ext->field_orders.progress; break;
    case FO_REWARD: words = ext->field_orders.reward; break;
    default: words = ext->field_orders.status; break;
    }
    words[index] = (uint8_t)(value & 0xff);
}

void Scr_SetFOGoal(scr_entref_t ref)
{ set_word(ref, FO_GOAL, "USAGE: SetFOGoal( <index>, <value> )\n"); }
void Scr_SetFOProgress(scr_entref_t ref)
{ set_word(ref, FO_PROGRESS, "USAGE: SetFOProgress( <index>, <value> )"); }
void Scr_SetFOReward(scr_entref_t ref)
{ set_word(ref, FO_REWARD, "USAGE: SetFOReward( <index>, <value> )"); }
void Scr_SetFOStatus(scr_entref_t ref)
{ set_word(ref, FO_STATUS, "USAGE: SetFOStatus( <index>, <value> )"); }

void ResetClientFieldOrders(unsigned client)
{
    ps_ext_t *ext;
    if (client >= MAX_CLIENTS || !level.clients) return;
    ext = PlayerStateExt(&level.clients[client].ps);
    if (ext) memset(&ext->field_orders, 0, sizeof(ext->field_orders));
}

int OverlayFieldOrderBlock(player_fields_t *wire, const field_orders_t *s)
{
    player_fields_t next;
    unsigned i, found = 0;
    if (!wire || !s) return 0;
    next = *wire;
    for (i = 0; i < PLAYER_FIELDS; ++i) {
        const player_field_t *f = OL_PlayerField(i);
        unsigned slot;
        if (!f) return 0;
        if (f->offset < 0x1b4 || f->offset > 0x1d0 || (f->offset & 3)) continue;
        slot = ((unsigned)f->offset - 0x1b4) / 4;   /* 0..7 */
        switch (slot >> 1) {
        case FO_GOAL: next.field[i] = s->goal[slot & 1]; break;
        case FO_PROGRESS: next.field[i] = s->progress[slot & 1]; break;
        case FO_REWARD: next.field[i] = s->reward[slot & 1]; break;
        /* C1 keeps the whole byte but transmits two bits (0x1cc/0x1d0); the
         * only reader compares it with 0. */
        default: next.field[i] = s->status[slot & 1] & 3u; break;
        }
        ++found;
    }
    if (found != 8) return 0;
    *wire = next;
    return 1;
}

int OverlayFieldOrders(player_fields_t *wire, const playerState_t *ps)
{
    const ps_ext_t *ext;
    if (!wire || !ps || !(ext = PlayerStateExtConst(ps))) return 0;
    return OverlayFieldOrderBlock(wire, &ext->field_orders);
}

/* A player's own orders follow them through every state they are shown: the
 * death cam and the killcam replay the killer's archived playerState, and a
 * bot killer has no orders, so the HUD bar (getFOPercent eases toward
 * progress/target while the goal stays) drained to 0 on every death and
 * refilled on respawn. Only a spectator draws the followed player's. */
/* ol_debugTrace 1: what a human's field-order HUD panel can see, one line
 * whenever it changes. The panel (patch_common_mp field_orders) draws slot i
 * while getFOGoal(i) > 0 && getFOPercent(i) < 1 && progress < target, under
 * weaponbar_hd's statement (!ui_active, !flashbanged, !isempjammed, ...).
 * The words are the ones published to this recipient; the gates the server
 * owns are the shellshock flag (C1 flashbanged reads the shellshock clock),
 * the EMP jam and the session state. The client-side gates (ui_active,
 * fo_table, the ui_show_fo stamp) are not visible here. */
static void trace_publish(unsigned recipient, const field_orders_t *s, const playerState_t *shown)
{
    static char last[MAX_CLIENTS][64];
    static int lines;
    char now[64];
    const gclient_t *client;
    if (!OL_TRACE_ON() || recipient >= MAX_CLIENTS || !level.clients || lines >= 4000 ||
        ClientIsBotClient(recipient)) return;
    client = &level.clients[recipient];
    snprintf(now, sizeof(now), "%u/%u %u/%u %u/%u %d %d %d %d %d", s->goal[0], s->goal[1],
             s->progress[0], s->progress[1], s->status[0], s->status[1],
             (client->ps.pm_flags & 0x8000) != 0, client->ps.shellshockDuration,
             PlayerStateEMPJammed(&client->ps), (int)client->sess.sessionState,
             shown != &client->ps);
    if (!strcmp(now, last[recipient])) return;
    memcpy(last[recipient], now, sizeof(now));
    ++lines;
    ((void)0);
}

/* The live client whose playerState is `ps`, or -1. */
static int live_client_of(const playerState_t *ps)
{
    uintptr_t p = (uintptr_t)ps, base;
    if (!ps || !level.clients) return -1;
    base = (uintptr_t)level.clients;
    if (p < base || p >= base + (uintptr_t)MAX_CLIENTS * sizeof(gclient_t)) return -1;
    if ((p - base) % sizeof(gclient_t)) return -1;
    return (int)((p - base) / sizeof(gclient_t));
}

/* SpectatorClientEndFrame copies the followed player's archived playerState
 * and its whole extension over the follower's live storage. The killcam takes
 * that path too (_killcam.gsc: sessionstate "spectator", forcespectatorclient
 * the killer), so a plain Apply replaced the victim's field orders with the
 * killer's - none, for a bot - and nothing put them back: a respawn keeps the
 * words, and the script publishes them only when it assigns the next order.
 * After any death with a killcam the panel lost its goal row and stopped
 * updating until that order ended. A follower that is not on the spectator
 * team keeps its own orders; a real spectator still takes the followed
 * player's (OverlayFieldOrdersFor). */
int PlayerStateExtApplyFollow(const playerState_t *dst, const ps_ext_t *value)
{
    field_orders_t own;
    ps_ext_t *ext;
    int client = live_client_of(dst), keep = 0;
    if (client >= 0 && level.clients[client].sess.cs.team != TEAM_SPECTATOR &&
        (ext = PlayerStateExt(dst)) != NULL) {
        own = ext->field_orders;
        keep = 1;
    }
    if (!PlayerStateExtApply(dst, value)) return 0;
    if (keep && (ext = PlayerStateExt(dst)) != NULL) ext->field_orders = own;
    return 1;
}

int OverlayFieldOrdersFor(player_fields_t *wire, const playerState_t *ps, unsigned recipient)
{
    const ps_ext_t *ext;
    if (recipient < MAX_CLIENTS && level.clients &&
        level.clients[recipient].sess.cs.team != TEAM_SPECTATOR &&
        (ext = PlayerStateExtConst(&level.clients[recipient].ps))) {
        trace_publish(recipient, &ext->field_orders, ps);
        return OverlayFieldOrderBlock(wire, &ext->field_orders);
    }
    return OverlayFieldOrders(wire, ps);
}
#endif

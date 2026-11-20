/* CoDOL usercmd fire-stream probe (developer diagnostic).
 *
 *   fireprobe <slot> <weapon> [key value ...]
 *   fireprobe stop
 *
 * Takes over a bot slot and feeds it C1-encoded usermove packets through the
 * real SV_UserMove, the way a lagged C1 client would: one command per client
 * frame, one packet every few frames carrying the new commands plus the
 * previous packets' (cl_packetdup), each packet delayed by a jittered one-way
 * latency, some dropped, out-of-order ones discarded as netchan does. The
 * commands carry the C1 wire word, so the server takes exactly the decode,
 * duplicate filter, acceptance, C1 pmove and weapon path a human's input
 * takes. The trigger is clicked (or held) on a schedule and every round the
 * weapon fires is attributed to the click that owns it: a semi-automatic
 * weapon must fire exactly one round per click.
 *
 * Keys (defaults in brackets):
 *   clicks   number of trigger presses                     [40]
 *   press    press length, ms; "a-b" picks per click        [40-260]
 *   gap      release before the next press, ms              [450-700]
 *   lat      one-way latency, ms                            [110]
 *   jitter   +/- latency jitter, ms                         [25]
 *   loss     packet loss, percent                           [10]
 *   frame    client frame, ms (C1 com_maxfps)               [8]
 *   packet   client packet interval, ms (cl_maxpackets)     [33]
 *   dup      previous packets repeated in each packet       [1]
 *   stall    percent of frames whose serverTime does not advance (C1 holds
 *            its clock while snapshots are late), then catches up      [0]
 *   ads      1: hold ADS (0x800) throughout; 2: only while pressing    [0]
 *   move     direction bits (1 fwd, 4 right, ...), mirrored every other
 *            press so the bot strafes back and forth                    [0]
 *   back     percent of packets after which C1's clock steps back 40 ms [0]
 *   hold     command bits held throughout (0x20000000 = ADS held, not
 *            toggled; 0x800 = ADS)                                      [0]
 *   pulse    command bits pressed for plen ms every pperiod ms (2 sprint,
 *            4 melee/zoom, 0x80000 night vision)              [0, 400, 60]
 *   trig     trigger bits; two bits alternate click by click, so
 *            0x100001 fires an akimbo pair hand by hand                 [1]
 *   seed     random seed                                    [1]
 *   setstance  every this many ms of server time, force the stance the way
 *            the setStance builtin does (ForceStanceEvent), cycling
 *            prone/stand/crouch; 0 = never                              [0]
 *
 * The report also runs a model of C1's event replay (model_report below): the
 * client's predicted event queue rebuilt per client frame from the snapshots
 * it would hold, compared the way CG_CheckPlayerstateEvents 0x45d680 compares
 * it, so a shot C1 would play twice is counted. Use frame == packet for it.
 *
 * The bot is given the weapon, a full magazine before every press and a large
 * reserve. Run it with the bot alone (scr_bots 1, scr_botequipment
 * 0, no score/time limit). Developer use only; it never touches a network
 * client. SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "bg_public.h"
#include "bg.h"
#include "qcommon.h"
#include "cmd.h"
#include "msg.h"
#include "xassets/weapondef.h"
#include "ol_msg.h"
#include "usercmd.h"
#include "game.h"
#include "history.h"
#include "sv_state.h"
#include "weapon_give.h"
#include "weapon_bits.h"
#include "sv_weapons.h"
#include "ol_events.h"
#include "weapon_state.h"
#include "variable_zoom.h"
#include "weapon_native.h"
#include "pmove_c1.h"
#include "pmove_c1_breath.h"
#include "weapon_compose.h"
#include "fire_probe.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

#define PROBE_MAX_CMDS 32768
#define PROBE_MAX_PACKETS 8192
#define PROBE_MAX_CLICKS 256
#define PROBE_PACKET_CMDS 64

typedef struct {
    int seq, arrival, first, count; /* commands [first, first+count) */
} probe_packet_t;

typedef struct {
    int active, slot, weapon, clip_index, clip_size;
    int clicks, press_lo, press_hi, gap_lo, gap_hi, lat, jitter, loss;
    int frame, packet, dup, stall, ads, move, back;
    unsigned rng;
    /* Client timeline, in server milliseconds. */
    int start, wall, next_frame, next_packet, client_time, frame_index;
    int last_packet_end;       /* first command not yet in any packet */
    int prev_firsts[8];        /* first command of the last packets, for dup */
    int click, click_down_at, click_up_at, next_click_at, done_at;
    int press_ms[PROBE_MAX_CLICKS];
    int shots[PROBE_MAX_CLICKS], events[PROBE_MAX_CLICKS];
    int first_shot_ms[PROBE_MAX_CLICKS], last_shot_ms[PROBE_MAX_CLICKS];
    int packet_seq, last_delivered_seq, sent, delivered, dropped, stale;
    int ncmds, npackets;
    ol_usercmd_t cmds[PROBE_MAX_CMDS];
    probe_packet_t packets[PROBE_MAX_PACKETS];
    uint16_t angles[2];
    unsigned wire_weapon, wire_alt;
    int last_clip, last_event_seq, executed_shots;
    /* Event-replay model (see model_report): what the server executed per
     * command, what C1 would have predicted for it, and the snapshots. */
    unsigned char cmd_executed[PROBE_MAX_CMDS], cmd_c1_foot[PROBE_MAX_CMDS];
    unsigned char cmd_nev[PROBE_MAX_CMDS], cmd_ev[PROBE_MAX_CMDS][8];
    /* What C1's own rules need per executed command (state before it runs,
     * and the sight after its ADS lerp, which is where PM_UpdateVariableZoom
     * reads it). */
    unsigned char cmd_breath_pre[PROBE_MAX_CMDS], cmd_weapon_live[PROBE_MAX_CMDS];
    /* PmoveSingle's own input filter: a dead, frozen (0x800 keeps 0x1300)
     * or just-respawned (0x400 keeps 0x1301) player's command carries no
     * sprint or night-vision bit, on either side. */
    unsigned char cmd_sprint_live[PROBE_MAX_CMDS];
    unsigned char cmd_scoped[PROBE_MAX_CMDS];
    float cmd_posfrac[PROBE_MAX_CMDS], cmd_fov[PROBE_MAX_CMDS][3];
    unsigned char snap_nv[PROBE_MAX_PACKETS * 2], snap_zlevel[PROBE_MAX_PACKETS * 2];
    unsigned char snap_zlatch[PROBE_MAX_PACKETS * 2];
    /* Extra buttons: held throughout, pulsed on a period, and the trigger
     * mask (two bits alternate per click, e.g. 0x100001 for akimbo). */
    unsigned hold, pulse, trig;
    int pperiod, plen;
    /* Accounting. */
    int ev_count[256], invisible[256], invisible_total;
    int shots_hand[2], entity_right, entity_left, entity_seq;
    int sprint_edges, sprint_resets, melee_holds, melee_resets, nv_edges, nv_events;
    int zoom_c1, zoom_server, zoom_disagree;
    int cmd_click[PROBE_MAX_CMDS];
    int stream[PROBE_MAX_CMDS * 2], nstream;     /* server primary queue, in order */
    int snap_arrival[PROBE_MAX_PACKETS * 2], snap_time[PROBE_MAX_PACKETS * 2];
    int snap_count[PROBE_MAX_PACKETS * 2], nsnaps, model_seq;
    int server_feet, c1_feet, ads_cmds, moving_cmds;
    /* Stance and landing coverage: forced-stance calls, events that entered
     * the predicted queue outside any command (C1 can never predict those),
     * and the stance/landing events the commands themselves produced. */
    int setstance, next_stance_at, stance_calls;
    /* Dive schedule (key "dive"): sprint forward, then crouch with the dive
     * bit on the command the sprint breaks, once per period. */
    int dive, divehold, yaw;
    /* "pos x,y,z": start the schedule from this origin (an open floor, so
     * the bot is not wedged against a wall). */
    int has_pos, has_ayaw, dive_lines;
    float ayaw; /* "ayaw": absolute view yaw instead of the spawn yaw + yaw */
    float pos[3];
    /* Weapon swap (key "swap"): a second weapon the command selects from
     * every trigger release to the next, so each change begins while the
     * shot's kick time still runs (PM_BeginWeaponChange's quick path). */
    int swap, swapped;
    /* "swaptake 1": the weapon being left is taken from the inventory the
     * moment the command switches (a script takeWeapon), so the change takes
     * PM_BeginWeaponChange's quick path; it is given back once the change ran. */
    int swaptake, take_pending, taken;
    unsigned wire_swap;
    /* Event-rate accounting: the most events one command queued, and the
     * snapshots whose predicted queue advanced by more than C1's four slots
     * since the previous one (CG_CheckPlayerstateEvents can play only the
     * newest four; the parity logger's "events: N new" line). */
    int cmd_max_events, cmd_max_at, snap_max_events, snap_over4, snap_last_count;
    int between_total, between_ev[256];
    int cmd_stance, cmd_invalid, cmd_land, cmd_land_pain, cmd_jump;
} probe_t;

static probe_t *probe;

static unsigned rnd(probe_t *p)
{
    p->rng = p->rng * 1103515245u + 12345u;
    return (p->rng >> 8) & 0xffffffu;
}
static int range(probe_t *p, int lo, int hi)
{
    return hi <= lo ? lo : lo + (int)(rnd(p) % (unsigned)(hi - lo + 1));
}
static void parse_range(const char *text, int *lo, int *hi)
{
    const char *dash = strchr(text, '-');
    *lo = atoi(text);
    *hi = dash ? atoi(dash + 1) : *lo;
    if(*hi < *lo) *hi = *lo;
}

/* The full-delta arm of C1 MSG_WriteDeltaUsercmdKey 0x59fa30: every field is
 * sent, so the decoded command does not depend on the delta base. This is the
 * exact inverse of ReadUsercmd's expected == 5 branch. */
static void write_command(ol_msg_t *m, uint32_t key, const ol_usercmd_t *c)
{
    uint32_t k;
    WriteBits(m, 0, 1);                  /* absolute serverTime */
    WriteLong(m, c->serverTime);
    WriteBits(m, 1u ^ (key & 1u), 1);   /* changed */
    WriteBits(m, 1u ^ (key & 1u), 1);   /* full delta */
    WriteBits(m, (c->buttons & 1u) ^ (key & 1u), 1);
    WriteBits(m, 1, 1); WriteShort(m, (uint16_t)(c->angles[0] ^ key));
    WriteBits(m, 1, 1); WriteShort(m, (uint16_t)(c->angles[1] ^ key));
    WriteBits(m, 1, 1); WriteBits(m, (c->direction ^ key) & 15u, 4);
    k = key ^ c->serverTime;
    WriteBits(m, 1, 1); WriteShort(m, (uint16_t)(c->angles[2] ^ k));
    WriteBits(m, 1, 1); WriteBits(m, ((c->buttons >> 1) ^ k) & 0x1fffffffu, 29);
    WriteBits(m, 1, 1); WriteBits(m, (c->weapon ^ k) & 1023u, 10);
    WriteBits(m, 1, 1); WriteBits(m, (c->offHandIndex ^ k) & 1023u, 10);
    WriteBits(m, 1, 1); WriteBits(m, (c->altModeWeapon ^ k) & 1023u, 10);
    /* The melee button carries the charge target (ReadUsercmd). */
    if(c->buttons & 4u) {
        WriteBits(m, 1, 1); WriteShort(m, (uint16_t)(c->meleeChargeYaw ^ k));
        WriteBits(m, 1, 1); WriteBits(m, (c->meleeChargeDist ^ k) & 255u, 8);
    }
    WriteBits(m, 5, 3);
}

static gentity_t *probe_entity(probe_t *p)
{
    gentity_t *ent = SV_GentityNum(p->slot);
    return ent && ent->client ? ent : NULL;
}

static void refill(probe_t *p, playerState_t *ps)
{
    struct WeaponDef *def = BG_GetWeaponDef(p->weapon);
    if(!def) return;
    if(def->iClipIndex >= 0 && def->iClipIndex < 128) ps->ammoclip[def->iClipIndex] = def->iClipSize;
    if(def->iAmmoIndex >= 0 && def->iAmmoIndex < 128) ps->ammo[def->iAmmoIndex] = 900;
    /* An akimbo pair's left gun has its own magazine in the hand bundle. */
    if(WeaponIsDualWield((unsigned)p->weapon))
        WeaponStateSetLeftClip(ps, (unsigned)p->weapon, def->iClipSize);
    p->last_clip = def->iClipIndex >= 0 && def->iClipIndex < 128 ? ps->ammoclip[def->iClipIndex] : 0;
}

/* Which click a round fired at server time `now` belongs to: the latest click
 * pressed before it (in client time, shifted by the one-way latency). */
static int owner(probe_t *p)
{
    return p->click > 0 ? p->click - 1 : 0;
}

static void count_shots(probe_t *p, playerState_t *ps)
{
    struct WeaponDef *def = BG_GetWeaponDef(p->weapon);
    int clip, seq, i, c = owner(p);
    if(!def || def->iClipIndex < 0 || def->iClipIndex >= 128 || c >= PROBE_MAX_CLICKS) return;
    clip = ps->ammoclip[def->iClipIndex];
    if(clip < p->last_clip) {
        int n = p->last_clip - clip;
        if(!p->shots[c]) p->first_shot_ms[c] = ps->commandTime;
        p->last_shot_ms[c] = ps->commandTime;
        p->shots[c] += n;
        p->executed_shots += n;
    }
    p->last_clip = clip;
    seq = ps->eventSequence;
    for(i = p->last_event_seq; i != seq && ((seq - i) & 255) <= 4; i = (i + 1) & 255) {
        int ev = ps->events[i & 3];
        if(ev == EV_FIRE_WEAPON || ev == EV_FIRE_WEAPON_LASTSHOT) p->events[c] += 1;
    }
    p->last_event_seq = seq;
}

/* ---- event-replay model ---------------------------------------------------
 *
 * C1 replays a predicted event when a snapshot renumbers it: CG_Transition
 * PlayerState 0x45d780 hands CG_CheckPlayerstateEvents 0x45d680 the new
 * predicted playerState and a summary of the previous frame's (taken by
 * 0x45d800), and for each of the four newest sequence numbers i it plays
 * events[i & 3] when
 *   i >= old sequence, or
 *   old sequence - 4 < i < old sequence and the event in that slot changed.
 * So an event C1 predicts that the server does not send (or the reverse)
 * shifts every newer predicted event down a slot when the snapshot covering it
 * arrives, and a shot fired within that round trip is played again.
 *
 * The model assumes C1's prediction produces exactly the server's events for
 * the same commands except footsteps, which it derives from the executed
 * command the way C1 0x413b10 / 0x4132c0 / 0x413210 would in multiplayer: a
 * bob crossing ((new+64)^(old+64)) & 0x80 by a standing player on the ground.
 * It needs one command per packet (frame == packet) to attribute events. */
static int is_footstep(int ev) { return ev >= EV_FOOTSTEP_SPRINT && ev <= EV_FOOTSTEP_PRONE; }
static int is_fire(int ev) { return ev == EV_FIRE_WEAPON || ev == EV_FIRE_WEAPON_LASTSHOT; }

/* Events the server queued outside command execution (none expected). */
static void model_between(probe_t *p, const playerState_t *ps)
{
    int seq = ps->eventSequence & 255;
    while(p->model_seq != seq && p->nstream < PROBE_MAX_CMDS * 2) {
        int ev = ps->events[p->model_seq & 3];
        p->stream[p->nstream++] = ev;
        ++p->between_total;
        ++p->between_ev[ev & 255];
        p->model_seq = (p->model_seq + 1) & 255;
    }
}
static int is_stance_force(int ev)
{ return ev == EV_STANCE_FORCE_STAND || ev == EV_STANCE_FORCE_CROUCH || ev == EV_STANCE_FORCE_PRONE; }
static int is_landing(int ev)
{ return ev >= EV_LANDING_DEFAULT && ev <= EV_LANDING_PAINTEDMETAL; }
static int is_landing_pain(int ev)
{ return ev >= EV_LANDING_PAIN_DEFAULT && ev <= EV_LANDING_PAIN_PAINTEDMETAL; }


/* Accounting for every event the server queued in the predicted playerState:
 * which types, and which go out as an empty slot (no wire event or a
 * parameter the mapper refuses) -- the slots that shift C1's comparison. */
static void account_event(probe_t *p, const playerState_t *ps, unsigned slot, int ev, int parm)
{
    uint32_t out;
    unsigned hand = 0, last = 0;
    ++p->ev_count[ev & 255];
    if(is_fire(ev)) {
        if(WeaponStateShotRecord(ps, slot, &hand, &last)) ++p->shots_hand[hand & 1];
        return; /* published through the shot-record mapping, zero parameter */
    }
    if(EventToWire((unsigned)ev) < 0 ||
       EventParameterToWire((unsigned)ev, (uint32_t)parm, &out) != 1) {
        ++p->invisible[ev & 255];
        ++p->invisible_total;
    }
}

static void model_executed(probe_t *p, const playerState_t *ps, const probe_packet_t *pk,
                           int before_time, int before_bob, int breath_pre)
{
    int i, idx = -1, seq = ps->eventSequence & 255, bob = ps->bobCycle & 255;
    const struct WeaponDef *def;
    if(ps->commandTime == before_time) { model_between(p, ps); return; }
    /* The newest command of the packet that the server accepted. */
    for(i = pk->first + pk->count - 1; i >= pk->first; --i)
        if((int)p->cmds[i].serverTime <= ps->commandTime) { idx = i; break; }
    if(idx < 0 || p->cmd_executed[idx]) { model_between(p, ps); return; }
    p->cmd_executed[idx] = 1;
    if(ps->pm_flags & 0x40) ++p->ads_cmds;
    if(ps->velocity[0] != 0.0f || ps->velocity[1] != 0.0f) ++p->moving_cmds;
    {
        /* Every command the packet carried ran in SV_UserMove; this is what
         * that packet added. */
        int queued = (seq - p->model_seq) & 255;
        if(queued > p->cmd_max_events) { p->cmd_max_events = queued; p->cmd_max_at = idx; }
        /* A dive schedule's commands, one line each (ol_debugTrace 1). */
        if(p->dive && OL_TRACE_ON() && p->dive_lines < 1500) {
            int dive_end = 0;
            ++p->dive_lines;
            PlayerStateDiveEndTimeRead(ps, &dive_end);
            Com_Printf(CON_CHANNEL_DONT_FILTER,
                "fireprobe dive cmd %d t%u b 0x%x d%u: pmf 0x%08x vel (%.0f %.0f %.0f) gnd %d vh %d "
                "ws %d jt %d sprint %d/%d up %d de %d org (%.0f %.0f %.0f); queued %d [%d %d %d %d]\n",
                idx, p->cmds[idx].serverTime, p->cmds[idx].buttons, p->cmds[idx].direction,
                (unsigned)ps->pm_flags, ps->velocity[0], ps->velocity[1], ps->velocity[2],
                ps->groundEntityNum, ps->viewHeightTarget, ps->weaponstate, ps->jumpTime,
                ps->sprintState.lastSprintStart, ps->sprintState.lastSprintEnd,
                ps->sprintState.sprintButtonUpRequired, dive_end,
                ps->origin[0], ps->origin[1], ps->origin[2], queued,
                ps->events[(seq - 4) & 3], ps->events[(seq - 3) & 3], ps->events[(seq - 2) & 3],
                ps->events[(seq - 1) & 3]);
        }
    }
    while(p->model_seq != seq && p->nstream < PROBE_MAX_CMDS * 2) {
        unsigned slot = (unsigned)p->model_seq & 3u;
        int ev = ps->events[slot];
        p->stream[p->nstream++] = ev;
        if(p->cmd_nev[idx] < 8) p->cmd_ev[idx][p->cmd_nev[idx]++] = (unsigned char)ev;
        if(is_footstep(ev)) ++p->server_feet;
        if(is_stance_force(ev)) ++p->cmd_stance;
        if(ev == EV_STANCE_INVALID) ++p->cmd_invalid;
        if(is_landing(ev)) ++p->cmd_land;
        if(is_landing_pain(ev)) ++p->cmd_land_pain;
        if(ev == EV_JUMP) ++p->cmd_jump;
        account_event(p, ps, slot, ev, ps->eventParms[slot]);
        p->model_seq = (p->model_seq + 1) & 255;
    }
    p->cmd_breath_pre[idx] = (unsigned char)(breath_pre & 1);
    p->cmd_sprint_live[idx] = (unsigned char)((breath_pre & 2) ? 1 : 0);
    p->cmd_weapon_live[idx] = (unsigned char)(ps->pm_type < 8 && !(ps->pm_flags & 0x400) &&
                                              !(ps->eFlags & 0xc00));
    p->cmd_posfrac[idx] = ps->fWeaponPosFrac;
    p->cmd_scoped[idx] = (unsigned char)(WeaponScopeOverlayC1((unsigned)ps->weapon) ? 1 : 0);
    def = BG_GetWeaponDef(ps->weapon);
    if(!def || !GetNativeWeaponZoomFov(def, p->cmd_fov[idx]))
        memset(p->cmd_fov[idx], 0, sizeof(p->cmd_fov[idx]));
    /* A stop resets bobCycle to 0 without a footstep (C1 PM_Footsteps idle arm),
     * so only a moving player's crossing counts. */
    if((((bob + 64) ^ (before_bob + 64)) & 0x80) && ps->groundEntityNum != 2047 &&
       ps->velocity[0] * ps->velocity[0] + ps->velocity[1] * ps->velocity[1] >= 1.0f &&
       ps->viewHeightTarget != 22 && ps->viewHeightTarget != 40 && ps->viewHeightTarget != 11) {
        p->cmd_c1_foot[idx] = 1;
        ++p->c1_feet;
    }
}

static void model_snapshot(probe_t *p, const playerState_t *ps)
{
    uint32_t level = 0, latch = 0;
    if(p->nsnaps >= PROBE_MAX_PACKETS * 2) return;
    model_between(p, ps);
    p->snap_arrival[p->nsnaps] = svs.time + p->lat + range(p, -p->jitter, p->jitter);
    p->snap_time[p->nsnaps] = ps->commandTime;
    p->snap_count[p->nsnaps] = p->nstream;
    if(p->nsnaps) {
        int burst = p->nstream - p->snap_last_count;
        if(burst > p->snap_max_events) p->snap_max_events = burst;
        if(burst > 4) ++p->snap_over4;
    }
    p->snap_last_count = p->nstream;
    /* The two wire words C1 rebuilds its own rules from: weapFlags 0x40 (night
     * vision, published) and the zoom level/latch netfields. */
    p->snap_nv[p->nsnaps] = (unsigned char)((ps->weapFlags & 0x40) ? 1 : 0);
    VariableZoomWire(ps->clientNum, &level, &latch);
    p->snap_zlevel[p->nsnaps] = (unsigned char)level;
    p->snap_zlatch[p->nsnaps] = (unsigned char)latch;
    ++p->nsnaps;
}

/* C1's rule-derived events for command j, predicted on top of a snapshot.
 * Everything else C1 predicts is taken to equal what the server queued. */
static int sprint_edge(const probe_t *p, int j)
{
    unsigned now = p->cmds[j].buttons, before = j > 0 ? p->cmds[j - 1].buttons : 0u;
    return p->cmd_sprint_live[j] && (now & 0x20000000u) && (now & 2u) && !(before & 2u);
}
static int nv_edge(const probe_t *p, int j)
{
    unsigned now = p->cmds[j].buttons, before = j > 0 ? p->cmds[j - 1].buttons : 0u;
    return p->cmd_sprint_live[j] && (now & 0x80000u) && !(before & 0x80000u);
}
static int has_ev(const probe_t *p, int j, int ev)
{
    int k;
    for(k = 0; k < p->cmd_nev[j]; ++k) if(p->cmd_ev[j][k] == ev) return 1;
    return 0;
}

static void model_report(probe_t *p)
{
    static int pred[PROBE_MAX_CMDS * 2];
    int old_seq = -1, old_slots[4] = {0, 0, 0, 0}, plays[PROBE_MAX_CLICKS];
    int i, j, k, best, doubled = 0, fired = 0, renumbered = 0;
    memset(plays, 0, sizeof(plays));
    for(i = 0; i < p->ncmds; ++i) {
        int w = p->start + 500 + i * p->frame, n, ts, base, slots[4], nv;
        variable_zoom_t zoom;
        /* Newest snapshot the client holds at this frame. */
        best = -1;
        for(k = 0; k < p->nsnaps; ++k)
            if(p->snap_arrival[k] <= w && (best < 0 || p->snap_time[k] > p->snap_time[best] ||
               (p->snap_time[k] == p->snap_time[best] && p->snap_count[k] > p->snap_count[best]))) best = k;
        if(best < 0) continue;
        ts = p->snap_time[best];
        base = p->snap_count[best];
        nv = p->snap_nv[best];
        memset(&zoom, 0, sizeof(zoom));
        zoom.level = p->snap_zlevel[best];
        zoom.latch = p->snap_zlatch[best];
        zoom.time = 0; /* ps+0x5c8 is not a netfield */
        n = 0;
        for(k = 0; k < base; ++k) pred[n++] = p->stream[k];
        /* The commands C1 still predicts on top of that snapshot, each one
         * with C1's own rules for the events this pass is about. */
        for(j = 0; j <= i; ++j) {
            int sprint_reset, melee_reset, zoom_event, nv_event = 0;
            if((int)p->cmds[j].serverTime <= ts || !p->cmd_executed[j]) continue;
            sprint_reset = sprint_edge(p, j) && !p->cmd_breath_pre[j];
            melee_reset = has_ev(p, j, EV_MELEE_SWIPE) && (p->cmds[j].buttons & 0x20000000u);
            if(nv_edge(p, j) && p->cmd_weapon_live[j]) {
                nv_event = nv ? EV_NIGHTVISION_REMOVE : EV_NIGHTVISION_WEAR;
                nv = !nv;
            }
            zoom_event = VariableZoomStep(&zoom, p->cmd_scoped[j], p->cmd_posfrac[j],
                p->cmd_fov[j], (int32_t)p->cmds[j].serverTime, p->cmds[j].buttons);
            if(sprint_reset) pred[n++] = EV_RESET_ADS;
            /* C1 decides WHETHER a step plays by its own rule (cmd_c1_foot);
             * the TYPE comes from the selector the server runs too (C1
             * 0x411040, pmove_c1_ground.c), so take the server's type
             * when both step on this command. A fixed WALK here counted every
             * running step as a renumbered replay. */
            if(p->cmd_c1_foot[j]) {
                int foot = EV_FOOTSTEP_WALK;
                for(k = 0; k < p->cmd_nev[j]; ++k) if(is_footstep(p->cmd_ev[j][k])) foot = p->cmd_ev[j][k];
                pred[n++] = foot;
            }
            for(k = 0; k < p->cmd_nev[j]; ++k) {
                int ev = p->cmd_ev[j][k];
                if(is_footstep(ev) || ev == EV_NIGHTVISION_WEAR || ev == EV_NIGHTVISION_REMOVE ||
                   ev == EV_SCOPE_ZOOM) continue;
                /* The server's own copy of a rule-derived reset is replaced by
                 * C1's (one per rule), so a missing one shows as a shift. */
                if(ev == EV_RESET_ADS && (sprint_edge(p, j) || melee_reset)) continue;
                if(ev == EV_MELEE_SWIPE && melee_reset) pred[n++] = EV_RESET_ADS;
                pred[n++] = ev;
            }
            if(zoom_event) pred[n++] = EV_SCOPE_ZOOM;
            if(nv_event) pred[n++] = nv_event;
            if(n > PROBE_MAX_CMDS * 2 - 16) break;
        }
        for(k = 0; k < 4; ++k) slots[k] = 0;
        for(k = n - 4; k < n; ++k) if(k >= 0) slots[k & 3] = pred[k];
        if(old_seq >= 0) {
            for(k = n - 4; k < n; ++k) {
                int ev, play;
                if(k < 0) continue;
                ev = slots[k & 3];
                play = k >= old_seq || (old_seq - k < 4 && ev != old_slots[k & 3]);
                if(play && k < old_seq) ++renumbered;
                if(play && is_fire(ev)) {
                    int c = -1, m;
                    /* The click this frame belongs to (the latest one pressed). */
                    for(m = i; m >= 0 && c < 0; --m) c = p->cmd_click[m];
                    if(c >= 0 && c < PROBE_MAX_CLICKS) ++plays[c];
                }
            }
        }
        old_seq = n;
        memcpy(old_slots, slots, sizeof(slots));
    }
    for(i = 0; i < p->click && i < PROBE_MAX_CLICKS; ++i) {
        if(plays[i]) ++fired;
        if(plays[i] > 1) {
            ++doubled;
            Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe model: click %d plays its shot %d times\n",
                       i, plays[i]);
        }
    }
    /* Variable zoom, per command: what C1 decides when it predicts the
     * command for the last time -- from the newest snapshot older than it,
     * with the published level and latch and the unsent debounce time at 0 --
     * against what the server queued. */
    p->zoom_c1 = p->zoom_server = p->zoom_disagree = 0;
    for(j = 0; j < p->ncmds; ++j) {
        variable_zoom_t zoom;
        int base = -1, m, c1 = 0, server;
        if(!p->cmd_executed[j]) continue;
        for(k = 0; k < p->nsnaps; ++k)
            if(p->snap_time[k] < (int)p->cmds[j].serverTime &&
               (base < 0 || p->snap_time[k] >= p->snap_time[base])) base = k;
        if(base < 0) continue;
        memset(&zoom, 0, sizeof(zoom));
        zoom.level = p->snap_zlevel[base];
        zoom.latch = p->snap_zlatch[base];
        for(m = 0; m <= j; ++m) {
            int e;
            if(!p->cmd_executed[m] || (int)p->cmds[m].serverTime <= p->snap_time[base]) continue;
            e = VariableZoomStep(&zoom, p->cmd_scoped[m], p->cmd_posfrac[m], p->cmd_fov[m],
                                       (int32_t)p->cmds[m].serverTime, p->cmds[m].buttons);
            if(m == j) c1 = e;
        }
        server = has_ev(p, j, EV_SCOPE_ZOOM);
        p->zoom_c1 += c1; p->zoom_server += server;
        if(c1 != server) ++p->zoom_disagree;
    }
    /* Footstep disagreements, command by command, with the command's whole
     * event list and the previous command's (landing and jump frames are
     * where the model's bob rule is least certain). */
    if(OL_TRACE_ON()) {
        int shown = 0;
        for(j = 0; j < p->ncmds && shown < 20; ++j) {
            int server_foot = 0;
            if(!p->cmd_executed[j]) continue;
            for(k = 0; k < p->cmd_nev[j]; ++k) if(is_footstep(p->cmd_ev[j][k])) server_foot = 1;
            if(server_foot == p->cmd_c1_foot[j]) continue;
            ++shown;
            Com_Printf(CON_CHANNEL_DONT_FILTER,
                "fireprobe foot cmd %d t%u buttons 0x%x: server %d model %d; events %d %d %d %d (prev %d %d %d)\n",
                j, p->cmds[j].serverTime, p->cmds[j].buttons, server_foot, p->cmd_c1_foot[j],
                p->cmd_nev[j] > 0 ? p->cmd_ev[j][0] : -1, p->cmd_nev[j] > 1 ? p->cmd_ev[j][1] : -1,
                p->cmd_nev[j] > 2 ? p->cmd_ev[j][2] : -1, p->cmd_nev[j] > 3 ? p->cmd_ev[j][3] : -1,
                j > 0 && p->cmd_nev[j-1] > 0 ? p->cmd_ev[j-1][0] : -1,
                j > 0 && p->cmd_nev[j-1] > 1 ? p->cmd_ev[j-1][1] : -1,
                j > 0 && p->cmd_nev[j-1] > 2 ? p->cmd_ev[j-1][2] : -1);
        }
    }
    /* Per-command agreement for the rule-derived events. */
    p->sprint_edges = p->sprint_resets = p->melee_holds = p->melee_resets = 0;
    p->nv_edges = p->nv_events = 0;
    for(j = 0; j < p->ncmds; ++j) {
        if(!p->cmd_executed[j]) continue;
        if(sprint_edge(p, j) && !p->cmd_breath_pre[j]) {
            if(OL_TRACE_ON())
                Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe edge cmd %d t%u: %d events %d %d %d\n", j,
                    p->cmds[j].serverTime, p->cmd_nev[j], p->cmd_ev[j][0], p->cmd_ev[j][1], p->cmd_ev[j][2]);
            ++p->sprint_edges;
            if(has_ev(p, j, EV_RESET_ADS)) ++p->sprint_resets;
        }
        if(has_ev(p, j, EV_MELEE_SWIPE) && (p->cmds[j].buttons & 0x20000000u)) {
            ++p->melee_holds;
            for(k = 1; k < p->cmd_nev[j]; ++k)
                if(p->cmd_ev[j][k] == EV_MELEE_SWIPE && p->cmd_ev[j][k - 1] == EV_RESET_ADS)
                    { ++p->melee_resets; break; }
        }
        if(nv_edge(p, j) && p->cmd_weapon_live[j]) {
            ++p->nv_edges;
            if(has_ev(p, j, EV_NIGHTVISION_WEAR) || has_ev(p, j, EV_NIGHTVISION_REMOVE)) ++p->nv_events;
        }
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "fireprobe model: footsteps server %d, C1 %d (commands %d moving, %d aiming); "
        "%d of %d clicks played by C1, %d played more than once; %d renumbered replays\n",
        p->server_feet, p->c1_feet, p->moving_cmds, p->ads_cmds, fired, p->click, doubled, renumbered);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "fireprobe model: sprint-out-of-sight %d/%d reset, melee-out-of-sight %d/%d reset, "
        "night vision %d/%d queued, scope zoom server %d, melee swipes %d, ADS resets %d; "
        "empty (unmapped) slots %d\n",
        p->sprint_resets, p->sprint_edges, p->melee_resets, p->melee_holds, p->nv_events, p->nv_edges,
        p->ev_count[EV_SCOPE_ZOOM & 255], p->ev_count[EV_MELEE_SWIPE], p->ev_count[EV_RESET_ADS],
        p->invisible_total);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "fireprobe model: scope zoom C1 %d server %d, commands that disagree %d\n",
        p->zoom_c1, p->zoom_server, p->zoom_disagree);
    for(i = 0; i < 256; ++i) if(p->invisible[i])
        Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe model: event %d went out empty %d times\n",
                   i, p->invisible[i]);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "fireprobe model: shots hand0 %d hand1 %d; entity fire events right %d left %d\n",
        p->shots_hand[0], p->shots_hand[1], p->entity_right, p->entity_left);
    {
        int stance_between = p->between_ev[EV_STANCE_FORCE_STAND] + p->between_ev[EV_STANCE_FORCE_CROUCH] +
                             p->between_ev[EV_STANCE_FORCE_PRONE];
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "fireprobe model: setStance calls %d; predicted-queue events queued outside commands %d "
            "(forced stance %d); from commands: forced stance %d, stance invalid %d, jumps %d, "
            "landings %d, painful landings %d\n",
            p->stance_calls, p->between_total, stance_between, p->cmd_stance, p->cmd_invalid,
            p->cmd_jump, p->cmd_land, p->cmd_land_pain);
        for(i = 0; i < 256; ++i) if(p->between_ev[i])
            Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe model: event %d queued outside commands %d times\n",
                       i, p->between_ev[i]);
    }
    /* Event rate: what the parity logger reports as "N new playerState events
     * in one frame" when a snapshot advances the queue past C1's four slots. */
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "fireprobe events: most from one packet %d (cmd %d t%u, events %d %d %d %d %d %d %d %d); "
        "most between snapshots %d; snapshots over four %d of %d\n",
        p->cmd_max_events, p->cmd_max_at, p->cmds[p->cmd_max_at].serverTime,
        p->cmd_ev[p->cmd_max_at][0], p->cmd_ev[p->cmd_max_at][1], p->cmd_ev[p->cmd_max_at][2],
        p->cmd_ev[p->cmd_max_at][3], p->cmd_ev[p->cmd_max_at][4], p->cmd_ev[p->cmd_max_at][5],
        p->cmd_ev[p->cmd_max_at][6], p->cmd_ev[p->cmd_max_at][7],
        p->snap_max_events, p->snap_over4, p->nsnaps);
    /* Weapon changes: C1 announces a change (event 0x1c) only when it drops
     * the weapon (put-away or alternate), never on the quick path. */
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "fireprobe events: weapon changes announced %d, put away %d, alternate %d, raised %d (first %d)\n",
        p->ev_count[EV_WEAPON_SWITCH_STARTED & 255], p->ev_count[EV_PUTAWAY_WEAPON],
        p->ev_count[EV_WEAPON_ALT], p->ev_count[EV_RAISE_WEAPON], p->ev_count[EV_FIRST_RAISE_WEAPON]);
    for(i = 0; i < 256; ++i) if(p->ev_count[i])
        Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe events: event %d queued by commands %d times\n",
                   i, p->ev_count[i]);
}

/* The shooter's entity queue, read once per server frame after the previous
 * frame's BG_PlayerStateToEntityState. */
static void account_entity(probe_t *p, const gentity_t *ent)
{
    int seq = ent->s.eventSequence & 255;
    while(p->entity_seq != seq) {
        int ev;
        /* Declared vec4_t, written as integer words (G_AddEvent). */
        memcpy(&ev, &ent->s.events[p->entity_seq & 3], sizeof(ev));
        if(ev == EV_FIRE_WEAPON || ev == EV_FIRE_WEAPON_LASTSHOT) ++p->entity_right;
        else if(ev == EV_FIRE_WEAPON_LEFT || ev == EV_FIRE_WEAPON_LASTSHOT_LEFT) ++p->entity_left;
        p->entity_seq = (p->entity_seq + 1) & 255;
    }
}

static void deliver(probe_t *p, client_t *cl, const probe_packet_t *pk)
{
    byte buffer[16384];
    ol_msg_t m;
    msg_t msg;
    uint32_t key;
    int i;
    float origin[3];
    playerState_t *ps = SV_GameClientNum(p->slot);
    if(pk->seq <= p->last_delivered_seq) { ++p->stale; return; }
    p->last_delivered_seq = pk->seq;
    key = UsercmdKey(cl->messageAcknowledge, sv.checksumFeed,
        (const byte *)cl->reliableCommands[
            cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS - 1)].command, 32);
    MsgWriteInit(&m, buffer, sizeof(buffer));
    WriteByte(&m, (uint32_t)pk->count);
    for(i = 0; i < pk->count; ++i) write_command(&m, key, &p->cmds[pk->first + i]);
    memcpy(origin, ps->origin, sizeof(origin));
    for(i = 0; i < 3; ++i) { uint32_t w; memcpy(&w, origin + i, 4); WriteLong(&m, w); }
    WriteLong(&m, p->cmds[pk->first + pk->count - 1].serverTime);
    if(m.failed) { Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe: packet overflow\n"); return; }
    memset(&msg, 0, sizeof(msg));
    msg.data = buffer; msg.maxsize = (int)sizeof(buffer); msg.cursize = (int)m.size;
    msg.readonly = qtrue;
    model_between(p, ps);
    {
        int before_time = ps->commandTime, before_bob = ps->bobCycle & 255;
        int breath_pre = CanHoldBreathC1(ps);
        int sprint_live = ps->pm_type < 8 && !(ps->pm_flags & 0xc00);
        SV_UserMove(cl, &msg, qtrue);
        model_executed(p, ps, pk, before_time, before_bob, breath_pre | (sprint_live ? 2 : 0));
    }
    if(msg.overflowed) Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe: usermove rejected (seq %d)\n", pk->seq);
    ++p->delivered;
    count_shots(p, ps);
}

static void build_packet(probe_t *p)
{
    probe_packet_t *pk;
    int first, i, k;
    if(p->last_packet_end >= p->ncmds || p->npackets >= PROBE_MAX_PACKETS) return;
    first = p->last_packet_end;
    for(k = 0; k < p->dup && k < 8; ++k)
        if(p->prev_firsts[k] >= 0 && p->prev_firsts[k] < first) first = p->prev_firsts[k];
    if(p->ncmds - first > PROBE_PACKET_CMDS) first = p->ncmds - PROBE_PACKET_CMDS;
    for(k = 7; k > 0; --k) p->prev_firsts[k] = p->prev_firsts[k - 1];
    p->prev_firsts[0] = p->last_packet_end;
    pk = &p->packets[p->npackets++];
    pk->seq = ++p->packet_seq;
    pk->first = first;
    pk->count = p->ncmds - first;
    p->last_packet_end = p->ncmds;
    ++p->sent;
    if((int)(rnd(p) % 100u) < p->loss) { pk->arrival = INT32_MAX; ++p->dropped; return; }
    pk->arrival = p->wall + p->lat + range(p, -p->jitter, p->jitter);
    (void)i;
}

/* The client side: frames and packets up to server time `now`. */
static void simulate_client(probe_t *p, int now)
{
    while(p->next_frame <= now && p->ncmds < PROBE_MAX_CMDS) {
        ol_usercmd_t *c;
        int down;
        p->wall = p->next_frame;
        /* Click schedule, in client wall time. */
        if(p->click < p->clicks && p->click < PROBE_MAX_CLICKS && !p->click_down_at &&
           p->wall >= p->next_click_at) {
            p->press_ms[p->click] = range(p, p->press_lo, p->press_hi);
            p->click_down_at = p->wall;
            p->click_up_at = p->wall + p->press_ms[p->click];
            ++p->click;
        }
        down = p->click_down_at && p->wall < p->click_up_at;
        if(p->click_down_at && p->wall >= p->click_up_at) {
            if(p->swap) { p->swapped = !p->swapped; if(p->swaptake) p->take_pending = 1; }
            p->click_down_at = 0;
            p->next_click_at = p->wall + range(p, p->gap_lo, p->gap_hi);
            if(p->click >= p->clicks) p->done_at = p->wall + 1500;
        }
        /* C1's command clock trails the server by the latency plus its
         * interpolation; a stalled frame repeats the previous millisecond. */
        if(!(p->stall && (int)(rnd(p) % 100u) < p->stall)) {
            int target = p->wall - p->lat - 50;
            /* After a backward step the clock rejoins gradually. */
            p->client_time = p->client_time + p->frame < target ? p->client_time + p->frame + 2 : target;
        }
        p->cmd_click[p->ncmds] = down ? p->click - 1 : -1;
        c = &p->cmds[p->ncmds++];
        memset(c, 0, sizeof(*c));
        c->serverTime = (uint32_t)p->client_time;
        {
            unsigned t = p->trig ? p->trig : 1u, bit = t;
            /* Two trigger bits alternate click by click (akimbo hands). */
            if((t & (t - 1)) && p->click > 0) {
                unsigned low = t & (0u - t);
                bit = ((p->click - 1) & 1) ? (t & ~low) : low;
            }
            c->buttons = down ? bit : 0u;
        }
        c->buttons |= p->hold;
        if(p->pulse && p->pperiod > 0 && ((p->wall - p->start) % p->pperiod) < p->plen)
            c->buttons |= p->pulse;
        if(p->ads == 1 || (p->ads == 2 && down)) c->buttons |= 0x800u;
        /* Alternate the direction every press so the bot strafes back and
         * forth instead of pinning itself against a wall. */
        if(p->click & 1) c->direction = (uint8_t)(p->move & 15);
        else c->direction = (uint8_t)(((p->move & 1) << 1) | ((p->move & 2) >> 1) |
                                      ((p->move & 4) << 1) | ((p->move & 8) >> 1));
        c->angles[0] = p->angles[0]; c->angles[1] = p->angles[1];
        c->weapon = (uint16_t)(p->swapped ? p->wire_swap : p->wire_weapon);
        c->altModeWeapon = (uint16_t)p->wire_alt;
        if(p->dive > 0) {
            /* Sprint forward for 60% of the period, then one crouch+dive
             * command (C1 reads 0x800000 on the command the sprint ends,
             * PM_CanDive 0x408ee2), then walk forward. */
            int phase = (p->wall - p->start) % p->dive, launch = p->dive * 6 / 10;
            c->direction = 1;
            /* divehold 1 keeps sprint held through the dive, 2 also holds
             * crouch after it, 3 holds crouch and the dive bit.
             * 4 and 5 send what C1 itself sends: CL_StanceButtons 0x48fb00
             * turns stance state 3 (set by a crouch/prone key while
             * sprinting, 0x48fac3) into prone 0x100 plus the dive bit, and
             * keeps it on every command until another stance key; 5 also
             * keeps sprint held. */
            if(p->divehold >= 4) {
                if(phase < launch || p->divehold >= 5) c->buttons |= 2u;
                if(phase >= launch) c->buttons |= 0x800100u;
                /* 6: jump (0x400) four frames before the dive input, so
                 * the dive arrives while the player is in the air: C1 starts
                 * a dive only from PM_WalkMove (0x410957), never airborne. */
                if(p->divehold == 6 && phase >= launch - 4 * p->frame &&
                   phase < launch - 3 * p->frame) c->buttons |= 0x400u;
            } else {
                if(phase < launch || p->divehold) c->buttons |= 2u;
                if(phase >= launch && phase < launch + p->frame) c->buttons |= 0x800200u;
                else if(p->divehold > 1 && phase >= launch)
                    c->buttons |= p->divehold > 2 ? 0x800200u : 0x200u;
            }
        }
        ++p->frame_index;
        p->next_frame += p->frame;
        if(p->wall >= p->next_packet) {
            build_packet(p);
            p->next_packet += p->packet;
            /* CL_AdjustTimeDelta's fast adjust can move C1's command clock
             * backwards; the server then skips commands until it passes the
             * last one it ran. */
            if(p->back && (int)(rnd(p) % 100u) < p->back) p->client_time -= 40;
        }
    }
}

static void report(probe_t *p)
{
    struct WeaponDef *def = BG_GetWeaponDef(p->weapon);
    int i, single = 0, multi = 0, none = 0;
    /* Both hands of an akimbo pair: count fire events, since the left gun's
     * rounds come out of the hand bundle, not ps->ammoclip. */
    if(WeaponIsDualWield((unsigned)p->weapon))
        for(i = 0; i < p->click && i < PROBE_MAX_CLICKS; ++i) p->shots[i] = p->events[i];
    for(i = 0; i < p->click && i < PROBE_MAX_CLICKS; ++i) {
        if(p->shots[i] == 1) ++single; else if(p->shots[i] == 0) ++none; else ++multi;
        if(p->shots[i] != 1)
            Com_Printf(CON_CHANNEL_DONT_FILTER,
                "fireprobe: click %d press %d ms -> %d rounds (%d fire events), first %d last %d\n",
                i, p->press_ms[i], p->shots[i], p->events[i], p->first_shot_ms[i], p->last_shot_ms[i]);
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "fireprobe: %s fireType %d fireTime %d: %d clicks, %d single, %d multi, %d none, %d rounds; "
        "packets sent %d dropped %d delivered %d stale %d; lat %d jitter %d loss %d dup %d stall %d\n",
        def ? def->szInternalName : "?", def ? (int)def->fireType : -1, def ? def->iFireTime : -1,
        p->click, single, multi, none, p->executed_shots, p->sent, p->dropped, p->delivered, p->stale,
        p->lat, p->jitter, p->loss, p->dup, p->stall, p->ads, p->move, p->back);
    model_report(p);
}

int FireProbeFrame(void *client)
{
    client_t *cl = (client_t *)client;
    probe_t *p = probe;
    gentity_t *ent;
    playerState_t *ps;
    int i, now, progressed;
    if(!p || !p->active || !cl || (int)(cl - svs.clients) != p->slot) return 0;
    if(cl->state != CS_ACTIVE || !(ent = probe_entity(p))) return 1;
    ps = &ent->client->ps;
    now = svs.time;
    if(!p->start) {
        if(ent->client->sess.sessionState != SESS_STATE_PLAYING || ent->health <= 0) return 1;
        p->start = now;
        p->wall = p->next_frame = p->next_packet = p->next_click_at = now + 500;
        p->client_time = now - p->lat - 50;
        GivePlayerWeapon(ps, p->weapon, 0);
        /* "swap current": the weapon the bot spawned holding. */
        if(p->swap < 0) p->swap = ps->weapon != p->weapon ? ps->weapon : 0;
        if(p->has_pos) {
            memcpy(ps->origin, p->pos, sizeof(p->pos));
            ps->velocity[0] = ps->velocity[1] = ps->velocity[2] = 0.0f;
        }
        ps->weapon = p->weapon;
        ps->weaponstate = 0; ps->weaponTime = 0; ps->weaponDelay = 0; ps->weaponShotCount = 0;
        RefreshServerWeaponMappings();
        p->wire_weapon = (unsigned)WeaponToWire((unsigned)p->weapon);
        p->wire_alt = p->wire_weapon;
        if(p->swap > 0) {
            GivePlayerWeapon(ps, p->swap, 0);
            RefreshServerWeaponMappings();
            p->wire_swap = (unsigned)WeaponToWire((unsigned)p->swap);
        }
        p->angles[0] = (uint16_t)ANGLE2SHORT(ps->viewangles[0] - ps->delta_angles[0]);
        p->angles[1] = (uint16_t)ANGLE2SHORT((p->has_ayaw ? p->ayaw : ps->viewangles[1]) + (float)p->yaw -
                                             ps->delta_angles[1]);
        p->last_event_seq = ps->eventSequence;
        p->model_seq = ps->eventSequence & 255;
        p->entity_seq = ent->s.eventSequence & 255;
        refill(p, ps);
        Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe: slot %d weapon %d (wire %u) started\n",
                   p->slot, p->weapon, p->wire_weapon);
    }
    /* A lone bot with equipment off takes no damage; nothing to hold here. */
    if(p->start) account_entity(p, ent);
    if(p->take_pending && ps->weapon) {
        p->taken = ps->weapon;
        WeaponBitsOwnedSet(ps, (unsigned)p->taken, 0);
        p->take_pending = 0;
    } else if(p->taken && ps->weapon != p->taken) {
        WeaponBitsOwnedSet(ps, (unsigned)p->taken, 1);
        p->taken = 0;
    }
    simulate_client(p, now);
    /* Refill between clicks, once the weapon is idle and nothing is in flight
     * for the released trigger. */
    if(!p->click_down_at && p->wall - p->click_up_at > 2 * (p->lat + p->jitter) + 250 &&
       ps->weaponstate == 0 && ps->weaponTime <= 1) refill(p, ps);
    /* Deliver what has arrived, oldest first. */
    do {
        int best = -1;
        progressed = 0;
        for(i = 0; i < p->npackets; ++i)
            if(p->packets[i].arrival <= now &&
               (best < 0 || p->packets[i].arrival < p->packets[best].arrival)) best = i;
        if(best >= 0) {
            probe_packet_t pk = p->packets[best];
            p->packets[best].arrival = INT32_MAX;
            deliver(p, cl, &pk);
            progressed = 1;
        }
    } while(progressed);
    /* A script forcing the stance between commands, as setStance does
     * (Final Stand revive, laststand). */
    if(p->setstance > 0 && now >= p->next_stance_at) {
        static const int cycle[3] = { EV_STANCE_FORCE_PRONE, EV_STANCE_FORCE_STAND, EV_STANCE_FORCE_CROUCH };
        extern void ForceStanceEvent(gentity_t *ent, int event);
        model_between(p, ps);
        ForceStanceEvent(ent, cycle[p->stance_calls % 3]);
        ++p->stance_calls;
        p->next_stance_at = now + p->setstance;
    }
    /* The snapshot this server frame sends carries the state after these. A
     * bot is never sent one, so stand in for SV_WriteSnapshotToClient's
     * per-snapshot step: the variable-zoom debounce restarts from zero,
     * because C1 replays a snapshot with ps+0x5c8 = 0. */
    model_snapshot(p, ps);
    if(C1EventParity()) VariableZoomSnapshotSent((unsigned)p->slot);
    if(p->done_at && now >= p->done_at) {
        report(p);
        p->active = 0;
    } else if(p->ncmds >= PROBE_MAX_CMDS || p->npackets >= PROBE_MAX_PACKETS) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe: buffer full, stopping early\n");
        report(p);
        p->active = 0;
    }
    return 1;
}

static void FireProbe_f(void)
{
    probe_t *p;
    int i, slot;
    if(Cmd_Argc() >= 2 && !strcmp(Cmd_Argv(1), "stop")) {
        if(probe && probe->active) { report(probe); probe->active = 0; }
        return;
    }
    if(Cmd_Argc() < 3) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "usage: fireprobe <bot slot> <weapon> [clicks N] [press a-b] [gap a-b] [lat ms] "
            "[jitter ms] [loss %%] [frame ms] [packet ms] [dup N] [stall %%] [seed N] | stop\n");
        return;
    }
    slot = atoi(Cmd_Argv(1));
    if(slot < 0 || slot >= sv_maxclients->integer ||
       svs.clients[slot].netchan.remoteAddress.type != NA_BOT) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe: slot %d is not a bot\n", slot);
        return;
    }
    if(!probe && !(probe = (probe_t *)malloc(sizeof(*probe)))) return;
    p = probe;
    memset(p, 0, sizeof(*p));
    p->slot = slot;
    if(!strncmp(Cmd_Argv(2), "akimbo:", 7) || !strncmp(Cmd_Argv(2), "with:", 5)) {
        /* "akimbo:<base>" or "with:<base>:<item>": the composed
         * "*a <base> 0 <item> <variant>" (weapon_compose.h), for
         * example the rof_akimbo pair or a scope_vzoom sight. */
        char base[64], item_name[64], composed[96];
        const char *arg = Cmd_Argv(2), *bname, *group, *colon;
        unsigned b, item = 0, variant = 0;
        p->weapon = 0;
        if(arg[0] == 'a') { Q_strncpyz(base, arg + 7, sizeof(base)); Q_strncpyz(item_name, "rof_akimbo", sizeof(item_name)); }
        else {
            colon = strchr(arg + 5, ':');
            Q_strncpyz(base, arg + 5, sizeof(base));
            if(colon && (size_t)(colon - (arg + 5)) < sizeof(base)) base[colon - (arg + 5)] = 0;
            Q_strncpyz(item_name, colon ? colon + 1 : "", sizeof(item_name));
        }
        for(b = 1; b < 1024; ++b)
            if(ComposedWeaponBaseName(b, &bname) == 1 && bname && !strcmp(bname, base)) break;
        if(b < 1024 && ComposedAttachmentItemIndex(item_name, &item, &group) == 1) {
            if(ComposedAttachmentVariant(base, item_name, &variant) != 1) variant = 0;
            snprintf(composed, sizeof(composed), "*a %u 0 %u %u", b, item, variant);
            p->weapon = G_GetWeaponIndexForName(composed);
            Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe: %s -> weapon %d%c", composed, p->weapon, 10);
        } else if(b < 1024) {
            /* List the attachment items whose name contains the request. */
            unsigned q; const char *iname;
            for(q = 0; q < 400; ++q)
                if(ComposedAttachmentItemName(q, &iname) == 1 && iname &&
                   (strstr(iname, item_name) || !strcmp(item_name, "list")))
                    Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe: item %u %s%c", q, iname, 10);
        }
    } else
    p->weapon = G_GetWeaponIndexForName(Cmd_Argv(2));
    if(p->weapon <= 0 || !BG_GetWeaponDef(p->weapon)) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "fireprobe: unknown weapon %s\n", Cmd_Argv(2));
        return;
    }
    p->clicks = 40; p->press_lo = 40; p->press_hi = 260; p->gap_lo = 450; p->gap_hi = 700;
    p->lat = 110; p->jitter = 25; p->loss = 10; p->frame = 8; p->packet = 33; p->dup = 1;
    p->stall = 0; p->rng = 1; p->pperiod = 400; p->plen = 60; p->trig = 1;
    for(i = 3; i + 1 < Cmd_Argc(); i += 2) {
        const char *k = Cmd_Argv(i), *v = Cmd_Argv(i + 1);
        if(!strcmp(k, "clicks")) p->clicks = atoi(v);
        else if(!strcmp(k, "press")) parse_range(v, &p->press_lo, &p->press_hi);
        else if(!strcmp(k, "gap")) parse_range(v, &p->gap_lo, &p->gap_hi);
        else if(!strcmp(k, "lat")) p->lat = atoi(v);
        else if(!strcmp(k, "jitter")) p->jitter = atoi(v);
        else if(!strcmp(k, "loss")) p->loss = atoi(v);
        else if(!strcmp(k, "frame")) p->frame = atoi(v) > 0 ? atoi(v) : 8;
        else if(!strcmp(k, "packet")) p->packet = atoi(v) > 0 ? atoi(v) : 33;
        else if(!strcmp(k, "dup")) p->dup = atoi(v);
        else if(!strcmp(k, "stall")) p->stall = atoi(v);
        else if(!strcmp(k, "seed")) p->rng = (unsigned)atoi(v);
        else if(!strcmp(k, "ads")) p->ads = atoi(v);
        else if(!strcmp(k, "move")) p->move = atoi(v);
        else if(!strcmp(k, "back")) p->back = atoi(v);
        else if(!strcmp(k, "hold")) p->hold = (unsigned)strtoul(v, NULL, 0);
        else if(!strcmp(k, "pulse")) p->pulse = (unsigned)strtoul(v, NULL, 0);
        else if(!strcmp(k, "pperiod")) p->pperiod = atoi(v);
        else if(!strcmp(k, "plen")) p->plen = atoi(v);
        else if(!strcmp(k, "trig")) p->trig = (unsigned)strtoul(v, NULL, 0);
        else if(!strcmp(k, "setstance")) p->setstance = atoi(v);
        else if(!strcmp(k, "dive")) p->dive = atoi(v);
        else if(!strcmp(k, "divehold")) p->divehold = atoi(v);
        else if(!strcmp(k, "yaw")) p->yaw = atoi(v);
        else if(!strcmp(k, "ayaw")) { p->has_ayaw = 1; p->ayaw = (float)atof(v); }
        else if(!strcmp(k, "pos"))
            p->has_pos = sscanf(v, "%f,%f,%f", &p->pos[0], &p->pos[1], &p->pos[2]) == 3;
        else if(!strcmp(k, "swaptake")) p->swaptake = atoi(v);
        else if(!strcmp(k, "swap")) p->swap = strcmp(v, "current") ? G_GetWeaponIndexForName(v) : -1;
    }
    if(p->clicks > PROBE_MAX_CLICKS) p->clicks = PROBE_MAX_CLICKS;
    for(i = 0; i < 8; ++i) p->prev_firsts[i] = -1;
    p->active = 1;
}

int FireProbeSlot(void)
{
    return probe && probe->active ? probe->slot : -1;
}

void RegisterFireProbe(void)
{
    Cmd_AddCommand("fireprobe", FireProbe_f);
}


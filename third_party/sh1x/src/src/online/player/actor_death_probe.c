/* CoDOL dying-actor collision probe (developer diagnostic, developer 1 only).
 *
 * corpseprobe list
 *     every actor and actor corpse: health, eType, r.contents, clipmask.
 * corpseprobe <entnum>
 *     contents, then sweeps through the body's bounds along x and y: a player
 *     hull with the player clipmask (0x2810011, ClientSpawn), an actor hull
 *     with the actor clipmask (0x2820015, SP_actor), each reporting whether
 *     it stopped on this entity; then bullet lines (0x2806831, Bullet_Fire)
 *     at several heights, listing those that hit it and their hit location.
 * corpseprobe <entnum> kill
 *     G_Damage from the world for more than the body's health, as a bullet,
 *     so the death path runs exactly as from a weapon.
 * corpseprobe place <client> <x> <y> <z>
 *     teleports a client to an absolute position (stepped across a body by
 *     a driver script, a walk the zm test client cannot be steered through).
 * corpseprobe killall
 *     the same kill for every live actor (corpse-capacity soaks).
 * corpseprobe <entnum> walk <client>
 *     a test client walks (real usercmds and Pmove) from 48 units on -x of
 *     the body to 48 units on +x, straight through it.
 * corpseprobe <entnum> press <client>
 *     teleports client <client> onto the body's origin, so its hull overlaps
 *     the dying actor or corpse; a following "list" shows whether the body
 *     moved (the pushable-corpse repro, 2026-10-13).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "cm_public.h"
#include "qcommon.h"
#include "cmd.h"
#include <stdlib.h>
#include <string.h>
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"
#include "sv_bots.h"

extern void G_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
    const float *dir, const float *point, int damage, int dflags, int mod, int weapon,
    int hitLoc, unsigned int modelIndex, unsigned int partName, int timeOffset);
extern char bulletPriorityMap[];

#define PROBE_MOD_RIFLE_BULLET 2

static int probe_is_actor(const gentity_t *ent)
{
    return ent->r.inuse && (ent->s.eType == ET_ACTOR || ent->s.eType == ET_ACTOR_CORPSE);
}

static const char *probe_sweep(char *buf, size_t size, const gentity_t *ent, int axis, const float *mins, const float *maxs, int mask)
{
    trace_t tr;
    vec3_t start, end;
    int i;
    for (i = 0; i < 3; ++i) start[i] = end[i] = (ent->r.absmin[i] + ent->r.absmax[i]) * 0.5f;
    start[2] = end[2] = ent->r.absmin[2] + 1.0f;
    start[axis] -= 96.0f; end[axis] += 96.0f;
    G_TraceCapsule(&tr, start, mins, maxs, end, ENTITYNUM_NONE, mask);
    if (tr.fraction >= 1.0f) return "passes through";
    if (tr.hitType == TRACE_HITTYPE_ENTITY && tr.hitId == ent->s.number) {
        Com_sprintf(buf, size, "BLOCKED by it at %.2f", tr.fraction);
        return buf;
    }
    Com_sprintf(buf, size, "stops on other (type %d id %d) at %.2f", tr.hitType, tr.hitId, tr.fraction);
    return buf;
}

/* Bullet lines at several heights above the origin: a dying or fallen body
 * is not where its standing bounds are.  Lists the heights that stop on it
 * and the hit location each resolves to. */
static const char *probe_bullets(const gentity_t *ent, int axis)
{
    static char buf[160];
    static const float heights[] = {2.0f, 6.0f, 12.0f, 20.0f, 30.0f, 45.0f, 60.0f};
    trace_t tr;
    vec3_t start, end;
    int i, h, len = 0, hits = 0;
    buf[0] = 0;
    for (h = 0; h < (int)(sizeof(heights) / sizeof(heights[0])); ++h) {
        for (i = 0; i < 3; ++i) start[i] = end[i] = (ent->r.absmin[i] + ent->r.absmax[i]) * 0.5f;
        start[2] = end[2] = ent->r.currentOrigin[2] + heights[h];
        start[axis] -= 96.0f; end[axis] += 96.0f;
        G_LocationalTrace(&tr, start, end, ENTITYNUM_NONE, 0x2806831, bulletPriorityMap);
        if (tr.fraction < 1.0f && tr.hitType == TRACE_HITTYPE_ENTITY && tr.hitId == ent->s.number) {
            len += Com_sprintf(buf + len, sizeof(buf) - len, "%s+%.0f(loc %d)", hits ? " " : "", heights[h], tr.partGroup);
            ++hits;
        }
    }
    return hits ? buf : "none";
}

static void probe_detail(const gentity_t *ent)
{
    static const float playerMins[3] = {-15.0f, -15.0f, 0.0f}, playerMaxs[3] = {15.0f, 15.0f, 70.0f};
    static const float actorMins[3] = {-15.0f, -15.0f, 0.0f}, actorMaxs[3] = {15.0f, 15.0f, 72.0f};
    char player[64], actor[64];
    int axis;
    Com_Printf(CON_CHANNEL_DONT_FILTER, "corpseprobe %d time %d: eType %d health %d takedamage %d contents 0x%x clipmask 0x%x at %.0f %.0f %.0f\n",
        ent->s.number, level.time, ent->s.eType, ent->health, ent->takedamage, ent->r.contents, ent->clipmask,
        ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
    for (axis = 0; axis < 2; ++axis) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "  %c: player hull %s; actor hull %s; bullet lines on it: %s\n", axis ? 'y' : 'x',
            probe_sweep(player, sizeof(player), ent, axis, playerMins, playerMaxs, 0x2810011),
            probe_sweep(actor, sizeof(actor), ent, axis, actorMins, actorMaxs, 0x2820015),
            probe_bullets(ent, axis));
    }
}

static void CorpseProbe_f(void)
{
    int n;
    gentity_t *ent;
    if (!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "corpseprobe needs developer 1\n"); return;
    }
    if (Cmd_Argc() < 2) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "usage: corpseprobe list | killall | place <client> <x> <y> <z> | <entnum> [kill | press <client> | walk <client>]\n"); return;
    }
    if (!strcmp(Cmd_Argv(1), "list")) {
        int live = 0, dying = 0, corpses = 0;
        for (n = 0; n < MAX_GENTITIES; ++n) {
            ent = &g_entities[n];
            if (!probe_is_actor(ent)) continue;
            if (ent->s.eType == ET_ACTOR_CORPSE) ++corpses;
            else if (ent->health <= 0) ++dying;
            else ++live;
            Com_Printf(CON_CHANNEL_DONT_FILTER, "  %4d eType %d health %4d takedamage %d contents 0x%08x clipmask 0x%08x at %.1f %.1f %.1f\n",
                n, ent->s.eType, ent->health, ent->takedamage, ent->r.contents, ent->clipmask,
                ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
        }
        for (n = 0; n < level.maxclients; ++n)
            if (g_entities[n].r.inuse && g_entities[n].client)
                Com_Printf(CON_CHANNEL_DONT_FILTER, "  client %d at %.1f %.1f %.1f contents 0x%x clipmask 0x%x vel %.0f %.0f\n", n,
                    g_entities[n].r.currentOrigin[0], g_entities[n].r.currentOrigin[1], g_entities[n].r.currentOrigin[2],
                    g_entities[n].r.contents, g_entities[n].clipmask,
                    g_entities[n].client->ps.velocity[0], g_entities[n].client->ps.velocity[1]);
        {
            int inuse = 0;
            for (n = 0; n < MAX_GENTITIES; ++n) if (g_entities[n].r.inuse) ++inuse;
            Com_Printf(CON_CHANNEL_DONT_FILTER, "corpseprobe time %d: %d live actors, %d dying, %d actor corpses; %d entities in use (num_entities %d)\n",
                level.time, live, dying, corpses, inuse, level.num_entities);
        }
        return;
    }
    if (!strcmp(Cmd_Argv(1), "place") && Cmd_Argc() > 5) {
        int c = atoi(Cmd_Argv(2));
        float origin[3], angles[3] = {0.0f, 0.0f, 0.0f};
        if (c < 0 || c >= MAX_CLIENTS || !g_entities[c].r.inuse || !g_entities[c].client) {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "  client %d is not in use\n", c); return;
        }
        origin[0] = (float)atof(Cmd_Argv(3)); origin[1] = (float)atof(Cmd_Argv(4)); origin[2] = (float)atof(Cmd_Argv(5));
        TeleportPlayer(&g_entities[c], origin, angles);
        return;
    }
    if (!strcmp(Cmd_Argv(1), "killall")) {
        int killed = 0;
        for (n = 0; n < MAX_GENTITIES; ++n) {
            float point[3], dir[3] = {1.0f, 0.0f, 0.0f};
            ent = &g_entities[n];
            if (!ent->r.inuse || ent->s.eType != ET_ACTOR || ent->health <= 0 || !ent->takedamage) continue;
            point[0] = ent->r.currentOrigin[0]; point[1] = ent->r.currentOrigin[1]; point[2] = ent->r.currentOrigin[2] + 40.0f;
            G_Damage(ent, &g_entities[ENTITYNUM_WORLD], &g_entities[ENTITYNUM_WORLD], dir, point,
                ent->health + 1000, 0, PROBE_MOD_RIFLE_BULLET, -1, 0, 0u, 0u, 0);
            ++killed;
        }
        Com_Printf(CON_CHANNEL_DONT_FILTER, "corpseprobe killall: killed %d actors at time %d\n", killed, level.time);
        return;
    }
    n = atoi(Cmd_Argv(1));
    if (n < 0 || n >= MAX_GENTITIES || !probe_is_actor(&g_entities[n])) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "entity %d is not an actor\n", n); return;
    }
    ent = &g_entities[n];
    if (Cmd_Argc() > 2 && !strcmp(Cmd_Argv(2), "kill")) {
        float point[3], dir[3] = {1.0f, 0.0f, 0.0f};
        if (!ent->takedamage) { Com_Printf(CON_CHANNEL_DONT_FILTER, "  entity %d does not take damage\n", n); return; }
        point[0] = ent->r.currentOrigin[0]; point[1] = ent->r.currentOrigin[1]; point[2] = ent->r.currentOrigin[2] + 40.0f;
        G_Damage(ent, &g_entities[ENTITYNUM_WORLD], &g_entities[ENTITYNUM_WORLD], dir, point,
            ent->health + 1000, 0, PROBE_MOD_RIFLE_BULLET, -1, 0, 0u, 0u, 0);
        Com_Printf(CON_CHANNEL_DONT_FILTER, "  killed %d at time %d\n", n, level.time);
    }
    if (Cmd_Argc() > 3 && !strcmp(Cmd_Argv(2), "press")) {
        int c = atoi(Cmd_Argv(3));
        float origin[3], angles[3] = {0.0f, 0.0f, 0.0f};
        if (c < 0 || c >= MAX_CLIENTS || !g_entities[c].r.inuse || !g_entities[c].client) {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "  client %d is not in use\n", c); return;
        }
        origin[0] = ent->r.currentOrigin[0]; origin[1] = ent->r.currentOrigin[1]; origin[2] = ent->r.currentOrigin[2] + 1.0f;
        TeleportPlayer(&g_entities[c], origin, angles);
        Com_Printf(CON_CHANNEL_DONT_FILTER, "  pressed client %d onto %d at %.1f %.1f %.1f (time %d)\n",
            c, n, origin[0], origin[1], origin[2], level.time);
    }
    if (Cmd_Argc() > 3 && !strcmp(Cmd_Argv(2), "walk")) {
        /* A test client 48 units on -x of the body walks (real usercmds,
         * Pmove) to 48 units on +x of it, straight through it. */
        int c = atoi(Cmd_Argv(3));
        float origin[3], angles[3] = {0.0f, 0.0f, 0.0f};
        if (c < 0 || c >= MAX_CLIENTS || !g_entities[c].r.inuse || !g_entities[c].client) {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "  client %d is not in use\n", c); return;
        }
        origin[0] = ent->r.currentOrigin[0] - 48.0f; origin[1] = ent->r.currentOrigin[1]; origin[2] = ent->r.currentOrigin[2] + 1.0f;
        TeleportPlayer(&g_entities[c], origin, angles);
        g_botai[c].moveTo[0] = ent->r.currentOrigin[0] + 48.0f;
        g_botai[c].moveTo[1] = ent->r.currentOrigin[1];
        g_botai[c].doMove = 1;
        Com_Printf(CON_CHANNEL_DONT_FILTER, "  client %d walks through %d from %.1f to %.1f (y %.1f, time %d)\n",
            c, n, origin[0], g_botai[c].moveTo[0], origin[1], level.time);
    }
    probe_detail(ent);
}

void RegisterCorpseProbe(void)
{
    Cmd_AddCommand("corpseprobe", CorpseProbe_f);
}

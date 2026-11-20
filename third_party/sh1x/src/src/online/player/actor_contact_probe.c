/* CoDOL actor-contact stuck probe (developer diagnostic, developer 1 only).
 *
 * Written for "dogs ... sometimes get stuck on each other" (2026-10-02).
 * Every live actor's movement frame is sampled from ActorRunFrame.
 * Over each 2-second window an actor that wanted to make progress (a route
 * leg to walk, or an enemy it is not yet biting) and moved under 16 units
 * without closing on its enemy is "stuck".  If another live actor stands
 * within 40 units of it (capsule radius 15: 30 is touching) the event is an
 * ACTOR-CONTACT stuck and is logged with both actors' positions, contents
 * and clipmasks and with what each relevant trace hits:
 *   stand     the actor's own hull where it stands (clipmask, no passable)
 *   ahead     a 16-unit sweep along its movement direction, as the clip
 *             world sees it, and again with the other actors passable
 *             (what ActorPhysicsMove sees)
 *   enemy     the dog attack-range line (0x2820011) to its enemy
 *
 * actorcontact on | off   start / stop sampling (on also resets)
 * actorcontact report     tallies since on
 * actorcontact reset      zero the tallies
 * actorcontact pile | cross | follow ...   staged contact, see probe_stage
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "cm_public.h"
#include "qcommon.h"
#include "cmd.h"
#include "cscr_stringlist.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "actor_contact_probe.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

extern void ActorsPassable(gentity_t *self, int passable);

#define PROBE_WINDOW_MS 2000
#define PROBE_MIN_PROGRESS 16.0f
#define PROBE_CONTACT 40.0f

typedef struct {
    int useCount, windowStart, lastSample, frames, wantFrames, stuckWindows;
    int turnFrames, turnEntries, lastTurning;
    int routeFlips;
    float origin[3], enemyDistance;
    int physAccepted, physRejected, physBlocked, deltaFails;
} probe_actor_t;

static probe_actor_t probeActors[MAX_GENTITIES];
static int probeOn, probeStartTime;
static int windows, contactStuck, contactEpisodes, otherStuck, overlapStuck;
static int contactBySpecies[2], otherBySpecies[2];
/* Route flips: an actor switching between its waypoint route and the
 * straight leg to its goal (either way).  flipWindows counts actor-windows
 * with 4 or more flips in 2 s, i.e. an actor oscillating. */
static int routeFlips, flipWindows, worstFlips, actorFrames;
static unsigned char lastRouteMode[MAX_GENTITIES]; /* 0 unknown, 1 straight, 2 route */

int ActorContactProbeActive(void)
{
    return probeOn && com_developer && com_developer->integer;
}

static void probe_reset(void)
{
    memset(probeActors, 0, sizeof(probeActors));
    windows = contactStuck = contactEpisodes = otherStuck = overlapStuck = 0;
    routeFlips = flipWindows = worstFlips = actorFrames = 0;
    memset(lastRouteMode, 0, sizeof(lastRouteMode));
    memset(contactBySpecies, 0, sizeof(contactBySpecies));
    memset(otherBySpecies, 0, sizeof(otherBySpecies));
    probeStartTime = level.time;
}

static const char *probe_hit(char *buf, size_t size, const trace_t *tr)
{
    if (tr->fraction >= 1.0f && !tr->startsolid && !tr->allsolid) return "clear";
    if (tr->hitType == TRACE_HITTYPE_ENTITY && tr->hitId < MAX_GENTITIES) {
        const gentity_t *hit = &g_entities[tr->hitId];
        Com_sprintf(buf, size, "%s ent %d %s eType %d contents 0x%x at %.2f",
            tr->startsolid ? "STARTSOLID in" : "hit", tr->hitId,
            hit->classname ? SL_ConvertToString(hit->classname) : "?", hit->s.eType,
            hit->r.contents, tr->fraction);
    } else
        Com_sprintf(buf, size, "%s %s at %.2f normal z %.2f", tr->startsolid ? "STARTSOLID in" : "hit",
            tr->hitType == TRACE_HITTYPE_NONE ? "world" : "non-entity", tr->fraction, tr->normal[2]);
    return buf;
}

static int probe_species(const char *species)
{
    return species && !strcmp(species, "dog") ? 0 : 1;
}

static void probe_event(gentity_t *e, const actor_contact_sample_t *s, probe_actor_t *p,
    float moved, gentity_t *other, float d, float dz)
{
    trace_t stand, ahead, aheadPassable, enemyLine;
    char b1[128], b2[128], b3[128], b4[128];
    const float *o = e->r.currentOrigin;
    float end[3];
    gentity_t *enemy = s->enemyNum >= 0 && s->enemyNum < MAX_GENTITIES ? &g_entities[s->enemyNum] : NULL;
    static const float attackMins[3] = {-15, -15, 18}, actorMaxs[3] = {15, 15, 48};
    end[0] = o[0] + s->dir[0]*16.0f; end[1] = o[1] + s->dir[1]*16.0f; end[2] = o[2];
    G_TraceCapsule(&stand, o, e->r.mins, e->r.maxs, o, e->s.number, e->clipmask);
    G_TraceCapsule(&ahead, o, e->r.mins, e->r.maxs, end, e->s.number, e->clipmask);
    ActorsPassable(e, 1);
    G_TraceCapsule(&aheadPassable, o, e->r.mins, e->r.maxs, end, e->s.number, e->clipmask);
    ActorsPassable(e, 0);
    memset(&enemyLine, 0, sizeof(enemyLine));
    enemyLine.fraction = 1.0f;
    if (enemy && enemy->r.inuse) {
        /* as dog_combat.c sweeps it: the actor itself out of the way */
        int contents = e->r.contents;
        e->r.contents = 0;
        G_TraceCapsule(&enemyLine, o, attackMins, actorMaxs, enemy->r.currentOrigin, enemy->s.number, 0x2820011);
        e->r.contents = contents;
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "CoDOL actorcontact: STUCK ent %d (%s) at (%.0f %.0f %.0f) moved %.1f in %d ms, wanted %d/%d frames; "
        "route %d attacking %d turning %d (%d frames, %d entries; facing %.0f dir yaw %.0f route yaw %.0f) enemy %d dist %.0f->%.0f melee %.0f goalSet %d inRange %d goal (%.0f %.0f %.0f) "
        "dir (%.2f %.2f) raw (%.2f %.2f) steer %.2f; phys ok %d rej %d blocked %d deltaFail %d; "
        "contact ent %d (%s) at (%.0f %.0f %.0f) d %.1f dz %.1f contents 0x%x/0x%x clipmask 0x%x/0x%x; "
        "stand %s; ahead %s; ahead passable %s; enemy line %s\n",
        e->s.number, s->species ? s->species : "?", o[0], o[1], o[2], moved, level.time - p->windowStart,
        p->wantFrames, p->frames, s->route, s->attacking, s->turning, p->turnFrames, p->turnEntries,
        e->r.currentAngles[1], atan2f(s->dir[1], s->dir[0]) * 57.2957795f,
        atan2f(s->rawDir[1], s->rawDir[0]) * 57.2957795f, s->enemyNum, p->enemyDistance,
        enemy ? sqrtf((float)Vec3DistanceSq(o, enemy->r.currentOrigin)) : -1.0f, s->meleeDistance,
        s->goalSet, s->inRange, s->goal[0], s->goal[1], s->goal[2], s->dir[0], s->dir[1],
        s->rawDir[0], s->rawDir[1], s->steer, p->physAccepted, p->physRejected, p->physBlocked, p->deltaFails,
        other->s.number, other->classname ? SL_ConvertToString(other->classname) : "?",
        other->r.currentOrigin[0], other->r.currentOrigin[1], other->r.currentOrigin[2], d, dz,
        e->r.contents, other->r.contents, e->clipmask, other->clipmask,
        probe_hit(b1, sizeof(b1), &stand), probe_hit(b2, sizeof(b2), &ahead),
        probe_hit(b3, sizeof(b3), &aheadPassable), enemy ? probe_hit(b4, sizeof(b4), &enemyLine) : "no enemy");
}

static void probe_open(probe_actor_t *p, const gentity_t *e, float enemyDistance, int stuckWindows)
{
    memset(p, 0, sizeof(*p));
    p->stuckWindows = stuckWindows;
    p->useCount = e->useCount;
    p->windowStart = p->lastSample = level.time;
    memcpy(p->origin, e->r.currentOrigin, 12);
    p->enemyDistance = enemyDistance;
}

void ActorContactProbeSample(gentity_t *e, const actor_contact_sample_t *s)
{
    probe_actor_t *p;
    int want, n;
    gentity_t *enemy;
    float enemyDistance = -1.0f;
    if (!ActorContactProbeActive() || !e || !s || e->s.number < 0 || e->s.number >= MAX_GENTITIES) return;
    p = &probeActors[e->s.number];
    enemy = s->enemyNum >= 0 && s->enemyNum < MAX_GENTITIES && g_entities[s->enemyNum].r.inuse ?
        &g_entities[s->enemyNum] : NULL;
    if (enemy) enemyDistance = sqrtf((float)Vec3DistanceSq(e->r.currentOrigin, enemy->r.currentOrigin));
    /* A new actor in the slot, a gap (traversal, scripted clip) or a level
     * restart opens a fresh window. */
    if (p->useCount != e->useCount || !p->windowStart || level.time - p->lastSample > 250 ||
        level.time < p->windowStart)
        probe_open(p, e, enemyDistance, 0);
    if (s->route == 1) {
        int mode = s->routeMode ? 2 : 1;
        if (lastRouteMode[e->s.number] && lastRouteMode[e->s.number] != mode) { ++p->routeFlips; ++routeFlips; }
        lastRouteMode[e->s.number] = (unsigned char)mode;
    }
    p->lastSample = level.time;
    ++p->frames;
    ++actorFrames;
    want = s->route == 1 || (enemy && !s->attacking && enemyDistance > s->meleeDistance + 24.0f);
    if (want) ++p->wantFrames;
    if (s->turning) { ++p->turnFrames; if (!p->lastTurning) ++p->turnEntries; }
    p->lastTurning = s->turning;
    p->physAccepted += s->physAccepted; p->physRejected += s->physRejected;
    p->physBlocked += s->physBlocked; p->deltaFails += s->deltaFails;
    if (level.time - p->windowStart < PROBE_WINDOW_MS) return;
    {
        float dx = e->r.currentOrigin[0] - p->origin[0], dy = e->r.currentOrigin[1] - p->origin[1];
        float moved = sqrtf(dx*dx + dy*dy);
        int closed = enemy && p->enemyDistance >= 0 && p->enemyDistance - enemyDistance >= PROBE_MIN_PROGRESS;
        int stuckWindows = 0;
        ++windows;
        if (p->routeFlips > worstFlips) worstFlips = p->routeFlips;
        if (p->routeFlips >= 4) {
            ++flipWindows;
            Com_Printf(CON_CHANNEL_DONT_FILTER, "CoDOL actorcontact: ROUTEFLIP ent %d (%s) at (%.0f %.0f %.0f) %d flips in %d ms goal (%.0f %.0f %.0f)\n",
                e->s.number, s->species ? s->species : "?", e->r.currentOrigin[0], e->r.currentOrigin[1], e->r.currentOrigin[2],
                p->routeFlips, level.time - p->windowStart, s->goal[0], s->goal[1], s->goal[2]);
        }
        if (p->wantFrames * 5 >= p->frames * 4 && moved < PROBE_MIN_PROGRESS && !closed) {
            gentity_t *nearest = NULL;
            float best = PROBE_CONTACT, bestDz = 0;
            for (n = 0; n < MAX_GENTITIES; ++n) {
                gentity_t *o = &g_entities[n];
                float ox, oy, oz, d;
                if (o == e || !o->r.inuse || o->s.eType != ET_ACTOR || o->health <= 0) continue;
                ox = o->r.currentOrigin[0] - e->r.currentOrigin[0];
                oy = o->r.currentOrigin[1] - e->r.currentOrigin[1];
                oz = o->r.currentOrigin[2] - e->r.currentOrigin[2];
                if (fabsf(oz) > 48.0f) continue;
                d = sqrtf(ox*ox + oy*oy);
                if (d < best) { best = d; bestDz = oz; nearest = o; }
            }
            if (nearest) {
                ++contactStuck; ++contactBySpecies[probe_species(s->species)];
                if (best < 30.0f) ++overlapStuck;
                if (!p->stuckWindows) ++contactEpisodes;
                stuckWindows = p->stuckWindows + 1;
                probe_event(e, s, p, moved, nearest, best, bestDz);
            } else {
                ++otherStuck; ++otherBySpecies[probe_species(s->species)];
                Com_Printf(CON_CHANNEL_DONT_FILTER,
                    "CoDOL actorcontact: stuck (no actor within %.0f) ent %d (%s) at (%.0f %.0f %.0f) moved %.1f route %d attacking %d enemy %d dist %.0f phys rej %d blocked %d\n",
                    PROBE_CONTACT, e->s.number, s->species ? s->species : "?", e->r.currentOrigin[0],
                    e->r.currentOrigin[1], e->r.currentOrigin[2], moved, s->route, s->attacking, s->enemyNum,
                    enemyDistance, p->physRejected, p->physBlocked);
            }
        }
        probe_open(p, e, enemyDistance, stuckWindows);
    }
}

static void probe_report(void)
{
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "CoDOL actorcontact: report after %d ms: %d actor-windows, %d ACTOR-CONTACT stuck (%d overlapping, %d episodes; dog %d, other %d), %d stuck with no actor near (dog %d, other %d); "
        "route flips %d (%.2f per actor-second), %d oscillating windows (>=4 flips/2 s), worst %d flips/2 s\n",
        level.time - probeStartTime, windows, contactStuck, overlapStuck, contactEpisodes,
        contactBySpecies[0], contactBySpecies[1], otherStuck, otherBySpecies[0], otherBySpecies[1],
        routeFlips, actorFrames ? routeFlips * 20.0f / actorFrames : 0.0f, flipWindows, worstFlips);
}

/* Staged contact (developer only).  Every live actor is placed and driven
 * through ActorDevDrive (ignoreall, no enemy, goal radius 16):
 *   pile x y z [r]        on a ring of radius r (120) around the point, all
 *                         sent to the point itself
 *   cross x y z yaw [h]   pairs facing each other h (160) either side of the
 *                         point along yaw, each sent to the other's start;
 *                         pairs 36 apart across the heading
 *   follow x y z yaw      a file 24 apart behind the point, sent 480 ahead to
 *                         goals 32 apart across the heading
 * A placement is dropped to the floor and skipped when the hull is in solid. */
extern int ActorDevDrive(gentity_t *entity, const float *origin, const float *goal, float radius);

static int probe_floor(gentity_t *e, const float *point, float *out)
{
    trace_t tr;
    float top[3], bottom[3];
    top[0] = bottom[0] = point[0]; top[1] = bottom[1] = point[1];
    top[2] = point[2] + 48.0f; bottom[2] = point[2] - 160.0f;
    ActorsPassable(e, 1);
    G_TraceCapsule(&tr, top, e->r.mins, e->r.maxs, bottom, e->s.number, e->clipmask);
    ActorsPassable(e, 0);
    if (tr.startsolid || tr.allsolid || tr.fraction >= 1.0f) return 0;
    out[0] = top[0]; out[1] = top[1]; out[2] = top[2] + (bottom[2] - top[2]) * tr.fraction + 1.0f;
    return 1;
}

static void probe_stage(const char *verb)
{
    gentity_t *list[64];
    int count = 0, placed = 0, n, i;
    float p[3], yaw = 0, extra = 0, fwd[2], side[2];
    if (Cmd_Argc() < 5) { Com_Printf(CON_CHANNEL_DONT_FILTER, "usage: actorcontact %s x y z ...\n", verb); return; }
    p[0] = (float)atof(Cmd_Argv(2)); p[1] = (float)atof(Cmd_Argv(3)); p[2] = (float)atof(Cmd_Argv(4));
    if (Cmd_Argc() > 5) yaw = (float)atof(Cmd_Argv(5));
    if (Cmd_Argc() > 6) extra = (float)atof(Cmd_Argv(6));
    fwd[0] = cosf(yaw * 0.0174532925f); fwd[1] = sinf(yaw * 0.0174532925f);
    side[0] = -fwd[1]; side[1] = fwd[0];
    for (n = 0; n < MAX_GENTITIES && count < 64; ++n)
        if (g_entities[n].r.inuse && g_entities[n].s.eType == ET_ACTOR && g_entities[n].health > 0)
            list[count++] = &g_entities[n];
    for (i = 0; i < count; ++i) {
        gentity_t *e = list[i];
        float at[3], goal[3], floor[3];
        if (!strcmp(verb, "pile")) {
            float r = Cmd_Argc() > 5 ? yaw : 120.0f, angle = 6.2831853f * i / (count ? count : 1);
            at[0] = p[0] + cosf(angle) * r; at[1] = p[1] + sinf(angle) * r; at[2] = p[2];
            memcpy(goal, p, 12);
        } else if (!strcmp(verb, "cross")) {
            float h = extra > 0 ? extra : 160.0f, lateral = (float)((i / 2) - count / 4) * 36.0f;
            float sign = (i & 1) ? 1.0f : -1.0f;
            at[0] = p[0] + fwd[0] * h * sign + side[0] * lateral;
            at[1] = p[1] + fwd[1] * h * sign + side[1] * lateral;
            goal[0] = p[0] - fwd[0] * h * sign + side[0] * lateral;
            goal[1] = p[1] - fwd[1] * h * sign + side[1] * lateral;
            at[2] = goal[2] = p[2];
        } else {
            at[0] = p[0] - fwd[0] * 24.0f * i; at[1] = p[1] - fwd[1] * 24.0f * i; at[2] = p[2];
            float lateral = (float)(i - count / 2) * 32.0f;
            goal[0] = p[0] + fwd[0] * 480.0f + side[0] * lateral;
            goal[1] = p[1] + fwd[1] * 480.0f + side[1] * lateral; goal[2] = p[2];
        }
        if (!probe_floor(e, at, floor)) continue;
        if (ActorDevDrive(e, floor, goal, 16.0f)) ++placed;
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER, "CoDOL actorcontact: %s at (%.0f %.0f %.0f): %d of %d live actors placed\n",
        verb, p[0], p[1], p[2], placed, count);
}

static void ActorContact_f(void)
{
    const char *verb = Cmd_Argc() > 1 ? Cmd_Argv(1) : "report";
    if (!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "actorcontact needs developer 1\n"); return;
    }
    if (!strcmp(verb, "on")) { probe_reset(); probeOn = 1; Com_Printf(CON_CHANNEL_DONT_FILTER, "CoDOL actorcontact: on\n"); }
    else if (!strcmp(verb, "off")) { probe_report(); probeOn = 0; }
    else if (!strcmp(verb, "reset")) probe_reset();
    else if (!strcmp(verb, "report")) probe_report();
    else if (!strcmp(verb, "pile") || !strcmp(verb, "cross") || !strcmp(verb, "follow")) probe_stage(verb);
    else Com_Printf(CON_CHANNEL_DONT_FILTER, "usage: actorcontact on | off | report | reset | pile | cross | follow\n");
}

void RegisterActorContactProbe(void)
{
    Cmd_AddCommand("actorcontact", ActorContact_f);
}

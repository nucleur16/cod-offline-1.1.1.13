#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Coop actor cover nodes: the base level's own IW4 cover node set, and
 * IW4's choice of cover for a soldier who has an enemy.
 *
 * WHAT THE LEVELS CARRY (MEASURED 2026-10-14, the entity lumps of the base
 * zones, %TEMP%\codol_zones): the coop levels are IW4 SP-noded --
 *   mp_estate_tropical  214 node_cover_left, 189 _right, 51 _stand,
 *                       51 _crouch, 13 _crouch_window, 1 _prone,
 *                       32/34/6 node_concealment_stand/crouch/prone
 *   mp_fav_tropical     74/72 left/right, 11 crouch, 51 crouch_window,
 *                       1 stand, 25+1 concealment
 *   mp_crash_tropical   67/91 left/right, 77 crouch, 12 window, 19 stand,
 *                       1 prone, 15 concealment
 *   mp_killhouse_sh     6 node_cover_crouch (the rest is path and guard)
 * and every cover node is also a node of the coop actors' route graph
 * (bot_nav.c builds it from the same records in a coop match), so a
 * soldier routes onto the node itself.
 *
 * THE CHOICE (IW4 code's FindBestCoverNode / the actor's claimed node,
 * reconstructed from what animscripts\cover_behavior.gsc and corner.gsc
 * expect of it -- a node in front of the enemy, unclaimed, reachable):
 *   - a cover type (not path, guard, exposed or a traversal);
 *   - free: no other live actor claims it, and none claims a node within
 *     32 units of it (two soldiers never share a corner);
 *   - on the actor's graph component, inside the search radius;
 *   - between the engagement minimum and maximum from the enemy;
 *   - facing him: the enemy's yaw from the node's forward inside the
 *     node's arc.  IW4's corner scripts step out only when the enemy's
 *     GetYawToOrigin (positive to the RIGHT, IW4 utility GetYawToSpot) is
 *     under 14 for a Cover Left (corner.gsc getCornerMode: lean -40..0, A/B
 *     below 14) and over -12 for a Cover Right, i.e. a left corner covers
 *     the left side and a right corner the right; walls and concealment
 *     face forward;
 *   - a wall (stand, crouch, window) must actually hide the soldier from
 *     the enemy's eye at its hide height; a corner or concealment that does
 *     is preferred;
 *   - preferred again when the soldier can shoot the enemy from its
 *     step-out / pop-up point (cover_behavior attackVisibleEnemy, else he
 *     only suppresses);
 *   - cost: path length proxy (the straight distance), plus any ground
 *     given up toward the enemy, plus how far off the node's axis he is.
 * Traces are spent only on the twelve cheapest candidates.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "server.h"
#include "g_shared.h"
#include "bot_nav.h"
#include "actor_cover.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

const char *PathNodeValue(unsigned index, const char *key);

#define COVER_MAX BOT_NODES
#define COVER_CANDIDATES 12
/* CONTENTS_SOLID: what hides a soldier and what his shot must clear. */
#define COVER_SIGHT_MASK 0x1

typedef struct {
    float origin[3], yaw;
    unsigned char type;
    short record, nav;
    int claimant;   /* entity number + 1, 0 free */
} cover_node_t;

static cover_node_t covers[COVER_MAX];
static int cover_count, cover_generation = -1;

static const struct { const char *classname; unsigned char type; } node_types[] = {
    {"node_cover_left", COVER_LEFT}, {"node_cover_right", COVER_RIGHT},
    {"node_cover_stand", COVER_STAND}, {"node_cover_crouch", COVER_CROUCH},
    {"node_cover_crouch_window", COVER_CROUCH_WINDOW}, {"node_cover_prone", COVER_PRONE},
    {"node_concealment_stand", COVER_CONCEAL_STAND}, {"node_concealment_crouch", COVER_CONCEAL_CROUCH},
    {"node_concealment_prone", COVER_CONCEAL_PRONE},
};
static unsigned char cover_type_of(const char *classname)
{
    unsigned i;
    if (!classname) return COVER_NONE;
    for (i = 0; i < sizeof(node_types)/sizeof(*node_types); ++i)
        if (!Q_stricmp(classname, node_types[i].classname)) return node_types[i].type;
    return COVER_NONE;
}

static void cover_build(void)
{
    int n = BotNavCount(), i;
    unsigned per[COVER_TYPES];
    memset(per, 0, sizeof(per));
    cover_count = 0;
    cover_generation = BotNavGeneration();
    for (i = 0; i < n && cover_count < COVER_MAX; ++i) {
        int record = BotNavRecord(i);
        const char *classname, *angles;
        const float *point;
        float a[3] = {0, 0, 0};
        cover_node_t *c;
        unsigned char type;
        if (record < 0) continue;
        classname = PathNodeValue((unsigned)record, "classname");
        type = cover_type_of(classname);
        if (type == COVER_NONE) continue;
        point = BotNavPoint(i);
        if (!point) continue;
        angles = PathNodeValue((unsigned)record, "angles");
        if (angles && sscanf(angles, "%f %f %f", &a[0], &a[1], &a[2]) != 3) a[1] = 0;
        if (!isfinite(a[1])) a[1] = 0;
        c = &covers[cover_count++];
        memcpy(c->origin, point, sizeof(c->origin));
        c->yaw = a[1];
        c->type = type;
        c->record = (short)record;
        c->nav = (short)i;
        c->claimant = 0;
        ++per[type];
    }
    if (cover_count)
        Com_Printf(CON_CHANNEL_SERVER,
            "CoDOL cover: %d cover nodes on the route graph (left %u right %u stand %u crouch %u window %u prone %u conceal %u/%u/%u)\n",
            cover_count, per[COVER_LEFT], per[COVER_RIGHT], per[COVER_STAND],
            per[COVER_CROUCH], per[COVER_CROUCH_WINDOW], per[COVER_PRONE],
            per[COVER_CONCEAL_STAND], per[COVER_CONCEAL_CROUCH], per[COVER_CONCEAL_PRONE]);
}
static int cover_ready(void)
{
    if (cover_generation != BotNavGeneration()) cover_build();
    return cover_count;
}
static const cover_node_t *cover_at(int cover)
{
    if (cover < 0 || cover >= cover_ready()) return NULL;
    return &covers[cover];
}

int CoverCount(void) { return cover_ready(); }
int CoverType(int cover) { const cover_node_t *c = cover_at(cover); return c ? c->type : COVER_NONE; }
int CoverRecord(int cover) { const cover_node_t *c = cover_at(cover); return c ? c->record : -1; }
int CoverNavNode(int cover) { const cover_node_t *c = cover_at(cover); return c ? c->nav : -1; }
float CoverYaw(int cover) { const cover_node_t *c = cover_at(cover); return c ? c->yaw : 0.0f; }
int CoverPoint(int cover, float *origin)
{
    const cover_node_t *c = cover_at(cover);
    if (!c || !origin) return 0;
    memcpy(origin, c->origin, sizeof(c->origin));
    return 1;
}
int CoverForRecord(int record)
{
    int i, n = cover_ready();
    for (i = 0; i < n; ++i) if (covers[i].record == record) return i;
    return -1;
}

/* A claimant that is gone (freed, dead) no longer holds the node. */
static int claimant_alive(int claimant)
{
    gentity_t *e;
    if (claimant <= 0 || claimant > MAX_GENTITIES) return 0;
    e = &g_entities[claimant - 1];
    return e->r.inuse && e->health > 0;
}
int CoverClaimant(int cover)
{
    const cover_node_t *c = cover_at(cover);
    return c && claimant_alive(c->claimant) ? c->claimant - 1 : -1;
}
int CoverClaim(int cover, int entnum)
{
    cover_node_t *c = (cover_node_t *)cover_at(cover);
    if (!c || entnum < 0) return 0;
    if (c->claimant && c->claimant != entnum + 1 && claimant_alive(c->claimant)) return 0;
    c->claimant = entnum + 1;
    return 1;
}
void CoverRelease(int cover, int entnum)
{
    cover_node_t *c = (cover_node_t *)cover_at(cover);
    if (c && c->claimant == entnum + 1) c->claimant = 0;
}

static float angle_clamp180(float a)
{
    a = fmodf(a, 360.0f);
    if (a > 180.0f) a -= 360.0f;
    if (a < -180.0f) a += 360.0f;
    return a;
}
/* The enemy's yaw from the node's forward, counter-clockwise positive (to
 * the node's LEFT).  IW4's GetYawToSpot is the negation. */
static float yaw_to(const cover_node_t *c, const float *point)
{
    float dx = point[0] - c->origin[0], dy = point[1] - c->origin[1];
    return angle_clamp180(atan2f(dy, dx) * 57.295779513f - c->yaw);
}
/* Whether the node's arc covers that yaw (the slack widens the arc for a
 * node already held, so a soldier is not flipped out by a step). */
static int in_arc(int type, float delta, float slack)
{
    switch (type) {
    case COVER_LEFT:  return delta >= -20.0f - slack && delta <= 80.0f + slack;
    case COVER_RIGHT: return delta >= -80.0f - slack && delta <= 20.0f + slack;
    default:                return fabsf(delta) <= 65.0f + slack;
    }
}
static int is_wall(int type)
{
    return type == COVER_STAND || type == COVER_CROUCH || type == COVER_CROUCH_WINDOW;
}
/* Where a soldier at the node is while hidden, and where he shoots from
 * (corner.gsc getPredictedPathMidpoint: 36 units out to the corner's side;
 * cover_wall pop_up: up over the wall). */
static void hide_point(const cover_node_t *c, float *out)
{
    float h = 40.0f;
    if (c->type == COVER_CROUCH || c->type == COVER_CROUCH_WINDOW ||
        c->type == COVER_CONCEAL_CROUCH) h = 28.0f;
    else if (c->type == COVER_PRONE || c->type == COVER_CONCEAL_PRONE) h = 12.0f;
    out[0] = c->origin[0]; out[1] = c->origin[1]; out[2] = c->origin[2] + h;
}
static void fire_point(const cover_node_t *c, float *out)
{
    float yaw = c->yaw * 0.01745329252f, side = 0.0f, h = 60.0f;
    if (c->type == COVER_LEFT) side = 36.0f;          /* out to the left */
    else if (c->type == COVER_RIGHT) side = -36.0f;   /* out to the right */
    else if (c->type == COVER_CROUCH || c->type == COVER_CROUCH_WINDOW) h = 46.0f;
    else if (c->type == COVER_CONCEAL_CROUCH) h = 40.0f;
    else if (c->type == COVER_PRONE || c->type == COVER_CONCEAL_PRONE) h = 14.0f;
    out[0] = c->origin[0] - sinf(yaw) * side;
    out[1] = c->origin[1] + cosf(yaw) * side;
    out[2] = c->origin[2] + h;
}
static int sight_clear(const float *a, const float *b)
{
    trace_t tr;
    static const float zero[3] = {0, 0, 0};
    G_TraceCapsule(&tr, a, zero, zero, b, ENTITYNUM_NONE, COVER_SIGHT_MASK);
    return !tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f;
}
static float dist2d(const float *a, const float *b)
{
    float x = a[0] - b[0], y = a[1] - b[1];
    return sqrtf(x * x + y * y);
}

int CoverFind(int entnum, const float *origin, const float *enemyFeet, const float *enemyEye,
    float minDist, float maxDist, float searchRadius, int component,
    const int *avoid, int avoidCount, const char **why, int *flags)
{
    int n = cover_ready(), i, k, best = -1, cand[COVER_CANDIDATES], candCount = 0, bestFlags = 0;
    float candScore[COVER_CANDIDATES], bestScore = 0;
    float actorEnemy = dist2d(origin, enemyFeet);
    unsigned rejected[6] = {0, 0, 0, 0, 0, 0};
    const char *reason = "no cover nodes on this level";
    if (why) *why = reason;
    if (flags) *flags = 0;
    if (!n) return -1;
    if (minDist < 64.0f) minDist = 64.0f;
    /* The nodes other live actors hold, once (at most one per actor). */
    int held[64], heldCount = 0;
    for (i = 0; i < n && heldCount < 64; ++i)
        if (covers[i].claimant && covers[i].claimant != entnum + 1 && claimant_alive(covers[i].claimant))
            held[heldCount++] = i;
    for (i = 0; i < n; ++i) {
        const cover_node_t *c = &covers[i];
        float d, de, delta, score;
        int skip = 0, j;
        if (c->type == COVER_PRONE || c->type == COVER_CONCEAL_PRONE) continue;
        if (c->claimant && c->claimant != entnum + 1 && claimant_alive(c->claimant)) { ++rejected[0]; continue; }
        d = dist2d(origin, c->origin);
        if (d > searchRadius || fabsf(c->origin[2] - origin[2]) > 160.0f) { ++rejected[1]; continue; }
        de = dist2d(c->origin, enemyFeet);
        if (de < minDist || de > maxDist) { ++rejected[2]; continue; }
        delta = yaw_to(c, enemyFeet);
        if (!in_arc(c->type, delta, 0.0f)) { ++rejected[3]; continue; }
        if (component >= 0 && BotNavComponent(c->nav) != component) { ++rejected[4]; continue; }
        for (j = 0; j < avoidCount && !skip; ++j) if (avoid[j] == i) skip = 1;
        /* Two soldiers never share a corner: a node within 32 of a node
         * someone else holds is taken too. */
        for (j = 0; j < heldCount && !skip; ++j) {
            const cover_node_t *o = &covers[held[j]];
            if (dist2d(o->origin, c->origin) < 32.0f && fabsf(o->origin[2] - c->origin[2]) < 48.0f) skip = 1;
        }
        if (skip) { ++rejected[5]; continue; }
        /* The arc's centre for a corner is 30 degrees to its open side. */
        if (c->type == COVER_LEFT) delta -= 30.0f;
        else if (c->type == COVER_RIGHT) delta += 30.0f;
        score = d + 0.75f * fmaxf(0.0f, de - actorEnemy) + 2.0f * fabsf(delta);
        /* Too near the enemy's own spot is no place to fight from. */
        if (de < minDist * 1.5f) score += minDist * 1.5f - de;
        /* Keep the cheapest few. */
        if (candCount < COVER_CANDIDATES) k = candCount++;
        else if (score >= candScore[COVER_CANDIDATES - 1]) continue;
        else k = COVER_CANDIDATES - 1;
        while (k > 0 && candScore[k - 1] > score) { cand[k] = cand[k - 1]; candScore[k] = candScore[k - 1]; --k; }
        cand[k] = i; candScore[k] = score;
    }
    if (!candCount) {
        static const char *const names[6] = {"claimed", "out of reach", "outside the engagement band",
            "facing away from the enemy", "off the actor's graph component", "given up or beside a held node"};
        unsigned most = 0;
        for (k = 1; k < 6; ++k) if (rejected[k] > rejected[most]) most = (unsigned)k;
        if (why) *why = rejected[most] ? names[most] : reason;
        return -1;
    }
    for (k = 0; k < candCount; ++k) {
        const cover_node_t *c = &covers[cand[k]];
        float hp[3], fp[3], score = candScore[k];
        int hidden, shootable;
        hide_point(c, hp);
        hidden = !sight_clear(enemyEye, hp);
        if (is_wall(c->type) && !hidden) continue;   /* the wall does not stand between them */
        fire_point(c, fp);
        shootable = sight_clear(fp, enemyEye);
        if (hidden) score -= 150.0f;
        if (shootable) score -= 600.0f;
        if (best < 0 || score < bestScore) {
            best = cand[k]; bestScore = score;
            bestFlags = (hidden ? COVER_FOUND_HIDDEN : 0) | (shootable ? COVER_FOUND_SHOOTABLE : 0);
        }
    }
    if (flags) *flags = bestFlags;
    if (best < 0 && why) *why = "no candidate wall hides him from the enemy";
    return best;
}

int CoverStillValid(int cover, const float *enemyFeet, const float *enemyEye, const char **why)
{
    const cover_node_t *c = cover_at(cover);
    float hp[3];
    if (!c) { if (why) *why = "no node"; return 0; }
    if (!in_arc(c->type, yaw_to(c, enemyFeet), 25.0f)) { if (why) *why = "flanked: the enemy is outside the node's arc"; return 0; }
    if (is_wall(c->type)) {
        hide_point(c, hp);
        if (sight_clear(enemyEye, hp)) { if (why) *why = "exposed: the enemy sees over or round the wall"; return 0; }
    }
    return 1;
}
#endif

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

void PersistentCoverClaimsReset(void) {
    /* Fixed POD map-cache records, not heap/tree/VM ownership. Only invoked
     * by resident cleanup after full entity shutdown and known zero humans. */
    memset(covers,0,sizeof(covers));cover_count=0;cover_generation=-1;
}

int PersistentCoverClaimsEmpty(void) {return resident_static_zero(covers,sizeof(covers)) && !cover_count && cover_generation==-1;}
#endif

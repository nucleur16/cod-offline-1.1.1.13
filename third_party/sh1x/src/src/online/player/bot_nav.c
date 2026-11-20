#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Native directed waypoint graph for CoDOL combat training.
 * CSV data: alpha0022/resources/waypoints. No navmesh offsets imported. */
#ifdef OL_SERVER
#include "bot_nav.h"
#include "filesystem.h"
#include "q_shared.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <ctype.h>
#define LINKS 48   /* CSV cap; T5 has no per-node cap, the map graph counts overflow as "link cap" */
/* blocker[i] is T5's pathlink_s::disconnectCount for links[i]: how many
 * entities' DisconnectPaths currently cut it (0 = open).  Which entities they
 * are is kept in cut_records below.  record / negotiationEnd are the
 * map-graph fields declared in the header (-1 on CSV graphs). */
typedef struct { float point[3]; unsigned short links[LINKS]; unsigned short blocker[LINKS]; unsigned char count,stance; short component; short record,negotiationEnd; bot_annotation_t annotation; } node_t;
#include "sys_ondemand.h"
SH1X_ONDEMAND(, node_t, bot_nodes, [BOT_NODES]); /* sys_ondemand.h: allocated when the server starts */
#define nodes (*sh1x_od_bot_nodes)
static int count;
/* Non-zero while the graph is the level's own (built from node_* records)
 * rather than a waypoint CSV, so the GSC loadbotwaypoints call that follows
 * map load keeps it instead of reloading the CSV over it. */
static int map_graph;
int BotNavIsMapGraph(void){return count>0&&map_graph;}
/* Bumped whenever the graph is replaced (CSV load, map build), so per-graph
 * caches built over it (actor_cover.c) know to rebuild. */
static int nav_generation;
int BotNavGeneration(void){return nav_generation;}
static uint64_t topology_revision = 1;
static void nav_changed(void) { if (!++topology_revision) ++topology_revision; }
uint64_t BotNavTopologyRevision(void) { return topology_revision; }
static bot_route_stats_t route_stats;
static float distance(const float *a,const float *b)
{float x=a[0]-b[0],y=a[1]-b[1],z=a[2]-b[2];return sqrtf(x*x+y*y+z*z);}
static char *line(char **text)
{
    char *start=*text,*p=start;
    if(!p || !*p)return NULL;
    while(*p && *p!='\n' && *p!='\r')++p;
    if(*p){*p++=0;while(*p=='\n'||*p=='\r')++p;}
    *text=p;return start;
}
static char *field(char **text)
{char *start=*text,*p=start;if(!p)return NULL;while(*p&&*p!=',')++p;if(*p){*p++=0;*text=p;}else *text=NULL;return start;}
static int integer(char **p,long *value)
{char *end;while(isspace((unsigned char)**p))++*p;*value=strtol(*p,&end,10);if(end==*p)return 0;*p=end;return 1;}
static int vector(char *part,float out[3])
{
    int i;char *end;
    if(!part||!*part)return 0;
    for(i=0;i<3;++i){out[i]=strtof(part,&end);if(end==part||!isfinite(out[i]))return -1;part=end;}
    while(isspace((unsigned char)*part))++part;
    return *part?-1:1;
}
/* Undirected component labels over the loaded links (union-find), so a
 * failed route can say whether its two ends were ever connected. */
static int nav_find(short *parent,int i){while(parent[i]!=i)i=parent[i]=parent[parent[i]];return i;}
static void nav_label_components(void)
{
    static short parent[BOT_NODES],label[BOT_NODES];int i,j,next=0;
    for(i=0;i<count;++i){parent[i]=(short)i;label[i]=-1;}
    for(i=0;i<count;++i)for(j=0;j<nodes[i].count;++j)parent[nav_find(parent,i)]=nav_find(parent,nodes[i].links[j]);
    for(i=0;i<count;++i){int root=nav_find(parent,i);if(label[root]<0)label[root]=(short)next++;nodes[i].component=label[root];}
}
int BotNavLoad(const char *path)
{
    void *buffer=NULL;char *cursor,*row,*part,*columns,*end;long total,v;int size,i,j;
    node_t *pending;
    count=0; ++nav_generation; map_graph=0; nav_changed(); /* Missing/invalid map must never reuse the previous map's graph. */
    if(!path || strstr(path,"..") || strchr(path,':') || *path=='/' || *path=='\\')return 0;
    size=FS_ReadFile(path,&buffer);
    if(size<=0 || size>4*1024*1024){if(buffer)FS_FreeFile(buffer);return 0;}
    pending=calloc(BOT_NODES,sizeof(*pending));
    if(!pending){FS_FreeFile(buffer);return 0;}
    cursor=buffer;row=line(&cursor);end=row;
    if(!row || !integer(&end,&total)||total<1||total>BOT_NODES)goto bad;
    while(isspace((unsigned char)*end))++end;if(*end)goto bad;
    for(i=0;i<total;++i){
        pending[i].record=pending[i].negotiationEnd=-1;
        row=line(&cursor);if(!row)goto bad;columns=row;part=field(&columns);
        for(j=0;j<3;++j){pending[i].point[j]=strtof(part,&end);if(end==part||!isfinite(pending[i].point[j]))goto bad;part=end;}
        while(isspace((unsigned char)*part))++part;if(*part)goto bad;
        part=field(&columns);if(!part)goto bad;
        while(*part){while(isspace((unsigned char)*part))++part;if(!*part)break;
            if(!integer(&part,&v)||v<0||v>=total||pending[i].count==LINKS)goto bad;
            pending[i].links[pending[i].count++]=(unsigned short)v;
        }
        part=field(&columns);if(!part)goto bad;
        if(!strcmp(part,"stand")||!strcmp(part,"claymore")||!strcmp(part,"grenade")||!strcmp(part,"javelin"))pending[i].stance=BOT_STAND;
        else if(!strcmp(part,"crouch"))pending[i].stance=BOT_CROUCH;
        else if(!strcmp(part,"prone"))pending[i].stance=BOT_PRONE;
        else if(!strcmp(part,"climb"))pending[i].stance=BOT_CLIMB;
        else goto bad;
        if(!strcmp(part,"claymore"))pending[i].annotation.tactic=BOT_TACTIC_CLAYMORE;
        else if(!strcmp(part,"grenade"))pending[i].annotation.tactic=BOT_TACTIC_GRENADE;
        else if(!strcmp(part,"javelin"))pending[i].annotation.tactic=BOT_TACTIC_JAVELIN;
        j=vector(field(&columns),pending[i].annotation.angles);if(j<0)goto bad;pending[i].annotation.hasAngles=(j==1);
        j=vector(field(&columns),pending[i].annotation.target);if(j<0)goto bad;pending[i].annotation.hasTarget=(j==1);
        /* Empty trailing fields are allowed; unknown nonempty data is not
         * silently discarded or misread as a graph link. */
        while((part=field(&columns))!=NULL){while(isspace((unsigned char)*part))++part;if(*part)goto bad;}
    }
    while((row=line(&cursor))!=NULL){while(isspace((unsigned char)*row))++row;if(*row)goto bad;}
    memcpy(nodes,pending,(size_t)total*sizeof(*nodes));count=(int)total;
    nav_label_components();
    free(pending);FS_FreeFile(buffer);return count;
bad:free(pending);FS_FreeFile(buffer);return 0;
}
/* The waypoint graph is per-map native state, so the map lifecycle loads it.
 *
 * MEASURED 2026-09-21: the only caller of BotNavLoad -- the sole writer
 * of `count` -- was the `loadbotwaypoints` builtin, called from
 * maps/mp/gametypes/_codol_bots::init(), which only maps/mp/gametypes/_gamelogic
 * threads (:1508).  The zm gametype never runs _gamelogic on purpose
 * (zm/maps/mp/gametypes/zm.gsc calls _callbacksetup + _zombiemode::init and
 * nothing else), so on a zm map nothing ever wrote `count` and this
 * file-static kept the PREVIOUS map's nodes.  bin/home/main/qconsole.log,
 * 2026-09-21 00:00: zm_prototype_sh zombies planned "route node 170 -> 48:
 * 9 nodes" over mp_rust's 192-node graph, walked at coordinates that mean
 * nothing in that map, wedged ("stuck ... -> repath", "no clear nav node
 * within 256 to snap to") and never reached the player.  It also suppressed
 * the actor mover's straight-line fallback, which only engages when
 * BotNavCount() is 0.
 *
 * Loading here gives every category the graph its own map ships and an empty
 * one where there is none (no zm/coop map has a waypoint CSV, and CoDOL ships
 * no pathdata asset class at all -- docs/zone_assets).  Returning 0 is a
 * normal outcome, not a failure. */
int BotNavLoadForMap(const char *mapname)
{
    char path[MAX_QPATH];
    if(!mapname || !*mapname){count=0;nav_changed();return 0;}
    Com_sprintf(path,sizeof(path),"waypoints/%s_wp.csv",mapname);
    return BotNavLoad(path);
}
int BotNavCount(void){return count;}

/* ---------------------------------------------------------------------------
 * The map's own AI graph.
 *
 * MEASURED 2026-09-21 (obj/zm-mapents/zm_prototype_sh-mapents.txt, parsed out
 * of the col_map_mp entity lump): zm_prototype_sh carries 691 node_pathnode
 * plus 12 node_negotiation_begin / 12 node_negotiation_end entities -- the IW
 * AI node set -- and mp_killhouse_sh carries 660 + node_guard/node_cover_*.
 * The compiled link table is NOT in the zone: replay_game_world_mp
 * (load_blocks.c) zeroes GameWorldMp bytes 4..71 (the PathData slot)
 * without consuming any stream bytes, which is only possible because every
 * CoDOL map ships it empty.  That matches the engine: C1.exe carries the
 * runtime connector's strings ("PATH_MAX_NODES (%i) exceeded.  Nodecount: %d",
 * "Pathnode (%s) at (%g %g %g) has no connections leaving it"), i.e. CoDOL
 * connects its nodes at map load exactly like T5 Path_ConnectPaths
 * (OpenBLOPS Game/Server/game/pathnode_load_obj.cpp:1517), which links every
 * ordered pair that Path_CanLinkNodes accepts (:1380):
 *   - |dz| <= 128 and horizontal distance <= 256,
 *   - an actor capsule (+-15.5 xy, actorMins/Maxs z) walked from A toward B
 *     in 4-unit wishes under gravity, failing on any hit, on a ground drop
 *     of more than 32, or on more than 96 steps,
 *   - and |z_end - z_B| <= 18 on arrival;
 *   - a negotiation begin node links only to its own end node (dist 15).
 * The walk below is that rule on this server's capsule trace: 8-unit steps
 * (half the iteration budget), an 18-unit step-up before each step and a
 * 32-unit drop test after it.  Dynamic blocking entities are irrelevant here
 * because it runs from GScr_LoadScripts, before any entity is spawned --
 * T5 zeroes their contents for the duration for the same reason.
 *
 * This is the "fix maps, don't pin" answer for zm/coop: the map is the
 * authority for its own navigation.  The hand-authored CSV still wins on the
 * mp maps that have one (the bots were tuned on it), so this only runs when
 * the CSV load left the graph empty.
 * ------------------------------------------------------------------------ */
#include "server.h"
#include "entity.h"
#include "qcommon_io.h"
#include "g_shared.h"
#include "g_public.h"
#include "sys_main.h"
const char *ServerZoneType(void);
int ZoneTypeRunsSoldiers(const char *zone_type);
const char *ServerMapName(void);
unsigned PathNodeCount(void);
const char *PathNodeValue(unsigned index, const char *key);
int PathNodeOrigin(unsigned index, float *origin);
/* T5 actor hull (OpenBLOPS actor.h:452): actorMins {-15,-15,0}, actorMaxs
 * {15,15,48}.  Path_CanLinkNodes widens xy by 0.5.  The 72-unit PLAYER
 * height this port first used is 24 taller than any zombie the graph is
 * for, and clipped door lintels and stair ceilings for nothing. */
#define MAP_LINK_MAX_XY 256.0f         /* T5: fDistSqrd <= 65536 */
#define MAP_LINK_MAX_DZ 128.0f
#define MAP_LINK_STEP 8.0f
#define MAP_LINK_STEP_UP 18.0f
#define MAP_LINK_MAX_DROP 32.0f
#define MAP_LINK_ARRIVE 4.0f           /* T5: fDistSqrd <= 16 */
#define MAP_LINK_ARRIVE_DZ 18.0f       /* T5: |z_end - z_to| <= 18 */
#define MAP_LINK_FLAT 4.0f             /* fast path: floor within +-4 of a.z along the line */
#define MAP_LINK_WALKABLE_NORMAL 0.7f /* same ground threshold as actor physics */
#define MAP_LINK_TRACE_MASK 0x2820011  /* T5 Path_CanLinkNodes phys.iTraceMask */
#define MAP_DROP_TRACE_MASK 0x820011   /* T5 node_droptofloor */
/* The same mask with monsterclip (0x20000) swapped for playerclip (0x10000):
 * an mp level graph built for the player bots walks its links as a player. */
#define MAP_PLAYER_TRACE_MASK 0x2810011
#define MAP_PLAYER_JUMP 44.0f          /* feet raised for the jump-over test */
static int map_link_mask = MAP_LINK_TRACE_MASK;
#define MAP_DROP_DEPTH 256.0f
#define MAP_GRID 256.0f
#define MAP_GRID_HASH 4096
#define MAP_BAD_PRINT 6
static const float map_actor_mins[3] = {-15.5f, -15.5f, 0.0f};
/* Height 48 is T5's actor; an mp level graph built for the player bots
 * (preferMapGraph) walks with the player's standing height instead
 * (C1 playerMaxs z 70): an overhang an actor passes under stopped the bots.
 * MEASURED 2026-10-22, mp_storm_spring (226 -444)->(175 -475): 17 stuck
 * reports in 3 minutes on the 48-tall graph. */
#define MAP_ACTOR_HEIGHT 48.0f
#define MAP_PLAYER_HEIGHT 70.0f
static float map_actor_maxs[3] = {15.5f, 15.5f, MAP_ACTOR_HEIGHT};
/* Rejection census, one counter per exit of the link test, printed at the end
 * of the build so ONE boot says why a pair did not link.  The forward-hit
 * counters split by what the capsule hit: world solid (CONTENTS_SOLID),
 * monsterclip (0x20000 without solid), anything else (glass, the runtime
 * bits). */
enum {
    MAP_R_DZ, MAP_R_XY, MAP_R_COINCIDENT, MAP_R_FWD_STARTSOLID, MAP_R_FWD_HIT_SOLID,
    MAP_R_FWD_HIT_MONSTERCLIP, MAP_R_FWD_HIT_OTHER, MAP_R_NO_FLOOR, MAP_R_DOWN_STARTSOLID,
    MAP_R_STEP_BUDGET, MAP_R_ARRIVE_DZ, MAP_R_LINK_CAP, MAP_R_LINK, MAP_R_COUNT
};
static const char *const map_reason_names[MAP_R_COUNT] = {
    "dz>128", "xy>256", "coincident", "fwd startsolid", "fwd hit solid", "fwd hit monsterclip",
    "fwd hit other", "no walkable floor within drop", "down startsolid", "step budget", "arrival dz>18",
    "link cap", "link"
};
static unsigned map_hist[MAP_R_COUNT];
static unsigned map_fast_path;
static int map_hit_reason(const trace_t *tr)
{
    if (tr->cflags & 0x1) return MAP_R_FWD_HIT_SOLID;
    if (tr->cflags & 0x20000) return MAP_R_FWD_HIT_MONSTERCLIP;
    return MAP_R_FWD_HIT_OTHER;
}
/* Floor z under (x,y) for the actor hull, searching from z+up down to z-down.
 * Returns 0 when nothing is found (or the start is in solid: *solid=1). */
static int map_floor_at(float x, float y, float z, float up, float down, float *floor, int *solid)
{
    float start[3], end[3];
    trace_t tr;
    start[0] = end[0] = x; start[1] = end[1] = y; start[2] = z + up; end[2] = z - down;
    G_TraceCapsule(&tr, start, map_actor_mins, map_actor_maxs, end, ENTITYNUM_NONE, map_link_mask);
    *solid = tr.startsolid || tr.allsolid;
    if (*solid || tr.fraction >= 1.0f || tr.normal[2] < MAP_LINK_WALKABLE_NORMAL) return 0;
    *floor = start[2] + (end[2] - start[2]) * tr.fraction;
    return 1;
}
/* The T5 rule on this server's capsule trace: 8-unit steps, an 18-unit
 * step-up before each step and a 32-unit drop test after it, arrive within
 * 18 in z.  Before the step walk, a flat-ground fast path: when the floor
 * sampled at every step (8 units) along the line stays within +-4 of a.z and
 * b is at that height, the step walk's capsule is the same constant-height
 * capsule every step, so ONE full-length sweep decides the forward traces.
 * The floor is sampled at the step walk's own spacing, not coarser: a hole
 * between two samples whose hulls both rest on its rims is a hole the step
 * walk (and T5's 4-unit Path_CanLinkNodes physics) falls into.  MEASURED
 * 2026-10-23, zm_factory_sh: sampled every 32 units, nodes (-2086 -1423)
 * and (-1885 -1560) linked across the corner of a 156-deep pit; zombies from
 * spawner (-2068 -1413 219) walked the link, fell to z 35 under their window
 * (-1534 -1541 226) and stuck there.
 * Returns a MAP_R_* reason. */
static int map_link_walk(const float *from, const float *to)
{
    float pos[3], dir[2], len, start[3], end[3], down[3], floor, t;
    trace_t tr;
    int steps, solid, flat = fabsf(to[2] - from[2]) <= MAP_LINK_FLAT;
    dir[0] = to[0] - from[0]; dir[1] = to[1] - from[1];
    len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1]);
    if (len < 1.0f) return MAP_R_COINCIDENT;
    dir[0] /= len; dir[1] /= len;
    for (t = MAP_LINK_STEP; flat && t < len; t += MAP_LINK_STEP) {
        if (!map_floor_at(from[0] + dir[0]*t, from[1] + dir[1]*t, from[2], MAP_LINK_STEP_UP, MAP_LINK_MAX_DROP, &floor, &solid)
            || fabsf(floor - from[2]) > MAP_LINK_FLAT) flat = 0;
    }
    if (flat) {
        start[0] = from[0]; start[1] = from[1]; start[2] = from[2] + MAP_LINK_STEP_UP;
        end[0] = to[0]; end[1] = to[1]; end[2] = start[2];
        G_TraceCapsule(&tr, start, map_actor_mins, map_actor_maxs, end, ENTITYNUM_NONE, map_link_mask);
        if (tr.startsolid || tr.allsolid) return MAP_R_FWD_STARTSOLID;
        if (tr.fraction < 1.0f) return map_hit_reason(&tr);
        ++map_fast_path;
        return MAP_R_LINK;
    }
    pos[0] = from[0]; pos[1] = from[1]; pos[2] = from[2];
    for (steps = 0; steps < 96; ++steps) {
        float dx = to[0] - pos[0], dy = to[1] - pos[1], remain = sqrtf(dx*dx + dy*dy), adv;
        if (remain <= MAP_LINK_ARRIVE) break;
        adv = remain < MAP_LINK_STEP ? remain : MAP_LINK_STEP;
        start[0] = pos[0]; start[1] = pos[1]; start[2] = pos[2] + MAP_LINK_STEP_UP;
        end[0] = start[0] + dir[0]*adv; end[1] = start[1] + dir[1]*adv; end[2] = start[2];
        G_TraceCapsule(&tr, start, map_actor_mins, map_actor_maxs, end, ENTITYNUM_NONE, map_link_mask);
        if (tr.startsolid || tr.allsolid) return MAP_R_FWD_STARTSOLID;
        if (tr.fraction < 1.0f) return map_hit_reason(&tr);
        down[0] = end[0]; down[1] = end[1]; down[2] = end[2] - MAP_LINK_STEP_UP - MAP_LINK_MAX_DROP;
        G_TraceCapsule(&tr, end, map_actor_mins, map_actor_maxs, down, ENTITYNUM_NONE, map_link_mask);
        if (tr.startsolid || tr.allsolid) return MAP_R_DOWN_STARTSOLID;
        /* A near-vertical face can be the first hit of this downward sweep.
         * Actor physics refuses it as ground, so it cannot support a nav
         * edge even when the trace returned a fraction below one. */
        if (tr.fraction >= 1.0f || tr.normal[2] < MAP_LINK_WALKABLE_NORMAL)
            return MAP_R_NO_FLOOR;
        pos[0] = end[0]; pos[1] = end[1];
        pos[2] = end[2] + (down[2] - end[2]) * tr.fraction;
    }
    if (steps >= 96) return MAP_R_STEP_BUDGET;
    return fabsf(pos[2] - to[2]) <= MAP_LINK_ARRIVE_DZ ? MAP_R_LINK : MAP_R_ARRIVE_DZ;
}
/* A level traversal (node_negotiation_begin -> _end) is an actor
 * animation: wall_over_40, mantle_over_36 through a window, often across a
 * playerclip brush the actor ignores.  A player bot takes one only when its
 * standing hull, feet raised by a jump, sweeps from the begin node to the end
 * node clear of the player mask.  MEASURED 2026-10-22: mp_terminal_cls's
 * wall_over_40 pairs at (2384 3106)..(2356 3252) held bots on the wall top
 * for 170 of 256 diagnostic samples near them. */
static int map_player_can_jump(const float *from, const float *to)
{
    float start[3], end[3];
    trace_t tr;
    start[0] = from[0]; start[1] = from[1]; end[0] = to[0]; end[1] = to[1];
    start[2] = end[2] = (from[2] > to[2] ? from[2] : to[2]) + MAP_PLAYER_JUMP;
    G_TraceCapsule(&tr, start, map_actor_mins, map_actor_maxs, end, ENTITYNUM_NONE, map_link_mask);
    return !tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f;
}
/* T5 node_droptofloor (OpenBLOPS pathnode.cpp:327), run for every node
 * before linking (G_SpawnPathnodes, :513): from origin+1 down 256 with the
 * actor hull cut to 30 tall, mask 0x820011; in solid or floating -> the
 * node becomes NODE_BADNODE and is never linked; otherwise the origin moves
 * to the landing and a stationary full-height probe must be clear.
 *
 * MEASURED 2026-09-21 (tools/path_link_census.py, point trace): the
 * authored node origins float 7..78 units above their floor on
 * zm_prototype_sh (mode 28), 2..80+ on zm_asylum_sh, 18..46 on
 * zm_zombie_training -- not one node within 7 of its floor.  C1.exe carries
 * this pass too ("is floating" 0x8794fc, "has no connections leaving it"
 * 0x8794dc).  Without it the walk started 28 above the floor, the drop
 * test failed for every node more than 32 up, and the arrival test failed
 * for the rest because the walker's z had snapped to the floor while the
 * target's had not: 620 of 715 nodes isolated. */
static int map_drop_to_floor(float *origin, int *reason)
{
    static const float drop_mins[3] = {-15.0f, -15.0f, 0.0f};
    static const float drop_maxs[3] = {15.0f, 15.0f, 30.0f};
    static const float probe_mins[3] = {-15.0f, -15.0f, 0.0f};
    static const float probe_maxs[3] = {15.0f, 15.0f, 48.0f};
    float start[3], end[3];
    trace_t tr;
    start[0] = end[0] = origin[0]; start[1] = end[1] = origin[1];
    start[2] = origin[2] + 1.0f; end[2] = origin[2] - (MAP_DROP_DEPTH - 1.0f);
    G_TraceCapsule(&tr, start, drop_mins, drop_maxs, end, ENTITYNUM_NONE, MAP_DROP_TRACE_MASK);
    if (tr.startsolid || tr.allsolid) { *reason = 0; return 0; }
    if (tr.fraction >= 1.0f) { *reason = 1; return 0; }
    end[2] = start[2] + (end[2] - start[2]) * tr.fraction;
    G_TraceCapsule(&tr, end, probe_mins, probe_maxs, end, ENTITYNUM_NONE, MAP_DROP_TRACE_MASK);
    if (tr.startsolid || tr.allsolid) { *reason = 2; return 0; }
    origin[2] = end[2];
    return 1;
}
/* Union-find over the links, treated as undirected, for the load-time
 * reachability census. */
static int map_find(int *parent, int i) { while (parent[i] != i) i = parent[i] = parent[parent[i]]; return i; }
static int map_grid_cell(float v) { return (int)floorf(v / MAP_GRID); }
static unsigned map_grid_hash(int cx, int cy) { return ((unsigned)cx * 73856093u ^ (unsigned)cy * 19349663u) & (MAP_GRID_HASH - 1); }
static const char *map_spawn_value(const SpawnVar *record, const char *key)
{
    int i;
    for (i = 0; i < record->numSpawnVars; ++i)
        if (!Q_stricmp(record->spawnVars[i].key, key)) return record->spawnVars[i].value;
    return NULL;
}
/* Which mp maps route their bots over the level's own node graph instead of
 * the hand-authored waypoint CSV.
 *
 * MEASURED 2026-10-22 (12 bots, 3-minute war runs, CSV vs level graph; the
 * per-map table is in waypoints/mapgraph.txt): on mp_abandon_sh the level
 * graph is 1731 nodes against the CSV's 387, stuck reports fell from 114 to
 * 31 and kills rose from 34 to 44.  The maps that
 * passed are listed in waypoints/mapgraph.txt (one map per line, '#'
 * comments), shipped in the server pack beside the CSVs, so the switch is
 * data and reversible per map.  preferMapGraph 0 keeps every CSV, 1
 * (default) prefers the level graph on the listed maps, 2 on every mp map
 * with node_* records (the measurement mode).  The CSV stays the fallback:
 * a level graph that is fragmented or leaves spawn points off its main
 * component is discarded and the CSV reloaded. */
static int map_graph_listed(const char *map)
{
    void *buffer = NULL;
    char *p, *line_start;
    int size, found = 0;
    size_t len;
    if (!map || !*map) return 0;
    size = FS_ReadFile("waypoints/mapgraph.txt", &buffer);
    if (size <= 0 || !buffer) { if (buffer) FS_FreeFile(buffer); return 0; }
    len = strlen(map);
    for (p = buffer; *p && !found; ) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        line_start = p;
        while (*p && *p != '\n' && *p != '\r') ++p;
        if (*line_start != '#' && (size_t)(p - line_start) >= len && !strncmp(line_start, map, len)) {
            char c = line_start[len];
            if (!c || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '#') found = 1;
        }
    }
    FS_FreeFile(buffer);
    return found;
}
static int map_graph_preferred(const char *map)
{
    static cvar_t *prefer;
    if (!prefer)
        prefer = Cvar_RegisterInt("preferMapGraph", 1, 0, 2, 0,
            "mp bots route over the level's own node_* graph instead of the waypoint CSV: 0 never, 1 maps listed in waypoints/mapgraph.txt, 2 every mp map");
    if (prefer->integer >= 2) return 1;
    if (prefer->integer == 1) return map_graph_listed(map);
    return 0;
}
/* The CSV graph the level graph replaced, kept for the graft below. */
static node_t *csv_copy;
static int csv_copy_count;
static void csv_copy_free(void) { free(csv_copy); csv_copy = NULL; csv_copy_count = 0; }
static int graft_link(int a, int b)
{
    int k;
    for (k = 0; k < nodes[a].count; ++k) if (nodes[a].links[k] == b) return 1;
    if (nodes[a].count >= LINKS) return 0;
    nodes[a].links[nodes[a].count++] = (unsigned short)b;
    return 1;
}
/* The level graph has no tactic annotations (claymore, grenade and javelin
 * spots with their angles) and no player ladders: its traversals are actor
 * negotiation pairs.  Carry every annotated or climb node of the CSV over as
 * a node of its own, keep the CSV links among them (a ladder's bottom-top
 * pair), and connect each to the nearest map nodes the link walk accepts
 * both ways (else, for a ladder end on a narrow ledge, the nearest map node
 * within 128). */
static void graft_csv_nodes(unsigned map_used)
{
    static short remap[BOT_NODES];
    int i, j, grafted = 0, tactics = 0, climbs = 0, attached = 0, forced = 0, unattached = 0;
    if (!csv_copy || !csv_copy_count) return;
    for (i = 0; i < csv_copy_count; ++i) {
        const node_t *c = &csv_copy[i];
        remap[i] = -1;
        if (c->annotation.tactic == BOT_TACTIC_NONE && c->stance != BOT_CLIMB) continue;
        if (count >= BOT_NODES) continue;
        nodes[count] = *c;
        nodes[count].count = 0;
        memset(nodes[count].blocker, 0, sizeof(nodes[count].blocker));
        nodes[count].record = nodes[count].negotiationEnd = -1;
        remap[i] = (short)count++;
        ++grafted;
        if (c->stance == BOT_CLIMB) ++climbs; else ++tactics;
    }
    for (i = 0; i < csv_copy_count; ++i) {
        if (remap[i] < 0) continue;
        for (j = 0; j < csv_copy[i].count; ++j)
            if (remap[csv_copy[i].links[j]] >= 0) graft_link(remap[i], remap[csv_copy[i].links[j]]);
    }
    for (i = 0; i < csv_copy_count; ++i) {
        int a = remap[i], m, best[4] = {-1, -1, -1, -1}, n = 0, nearest = -1, linked = 0;
        float bestd[4] = {0, 0, 0, 0}, nearestd = 128.0f;
        if (a < 0) continue;
        for (m = 0; m < (int)map_used; ++m) {
            float d = distance(nodes[a].point, nodes[m].point), dz = nodes[m].point[2] - nodes[a].point[2];
            if (d < nearestd) { nearestd = d; nearest = m; }
            if (d > 192.0f || dz < -MAP_LINK_MAX_DZ || dz > MAP_LINK_MAX_DZ) continue;
            if (n < 4 || d < bestd[3]) {
                int k = n < 4 ? n++ : 3;
                while (k > 0 && bestd[k-1] > d) { best[k] = best[k-1]; bestd[k] = bestd[k-1]; --k; }
                best[k] = m; bestd[k] = d;
            }
        }
        for (j = 0; j < n; ++j) {
            if (map_link_walk(nodes[a].point, nodes[best[j]].point) != MAP_R_LINK ||
                map_link_walk(nodes[best[j]].point, nodes[a].point) != MAP_R_LINK) continue;
            graft_link(a, best[j]); graft_link(best[j], a); ++linked;
        }
        if (linked) ++attached;
        else if (nearest >= 0) { graft_link(a, nearest); graft_link(nearest, a); ++forced; }
        else ++unattached;
    }
    if (grafted) {
        ++nav_generation;
        nav_label_components();
    }
    Com_Printf(CON_CHANNEL_SERVER,
        "CoDOL waypoint graph: grafted %d waypoint-CSV nodes onto the level graph (%d tactic spots, %d ladder nodes): %d linked by walk, %d to the nearest node, %d unattached\n",
        grafted, tactics, climbs, attached, forced, unattached);
}
/* The objective entities the running gametype's script looks up by
 * targetname (maps/mp/gametypes/<gt>.gsc).  Empty for gametypes without
 * fixed objectives (war, dm, conf, infect, ...).  dd.gsc takes the sd
 * bombzones on a map with no dd_bombzone (mp_mideast), and arena.gsc dom's
 * flags on a map with no flag_arena; `has_dd` and `has_arena` say whether
 * the entity string carries its own. */
static int map_gametype_objective(const char *gametype, const char *tn, int has_dd, int has_arena)
{
    static const struct { const char *gametype, *targetname; } table[] = {
        {"dom", "flag_primary"}, {"dom", "flag_secondary"},
        {"sd", "bombzone"}, {"sd", "sd_bomb_pickup_trig"},
        {"sab", "sab_bomb_pickup_trig"}, {"sab", "sab_bomb_allies"}, {"sab", "sab_bomb_axis"},
        {"ctf", "ctf_zone_allies"}, {"ctf", "ctf_zone_axis"},
        {"oneflag", "ctf_zone_allies"}, {"oneflag", "ctf_zone_axis"},
        {"dd", "dd_bombzone"}, {"koth", "hq_hardpoint"}, {"gtnw", "gtnw_zone"}, {"arena", "flag_arena"}
    };
    unsigned i;
    if (!gametype || !tn) return 0;
    if (!strcmp(gametype, "dd") && !has_dd && !strcmp(tn, "bombzone")) return 1;
    if (!strcmp(gametype, "arena") && !has_arena && !strcmp(tn, "flag_primary")) return 1;
    for (i = 0; i < sizeof(table) / sizeof(*table); ++i)
        if (!strcmp(gametype, table[i].gametype) && !strcmp(tn, table[i].targetname)) return 1;
    return 0;
}
static int csv_copy_covers(const float *point)
{
    int i;
    for (i = 0; i < csv_copy_count; ++i) if (distance(point, csv_copy[i].point) <= 256.0f) return 1;
    return 0;
}
/* Spawn and objective coverage of the level graph.
 *
 * Spawns: one whose nearest node is more than 256 away or off the main
 * component leaves the bots born there with no route; under 95% covered
 * fails.  A spawn more than 2048 from every node is a placeholder parked
 * outside the level (mp_cha_quad's zone keeps its 16 ctf/dd/sab/sd spawns
 * ~11000 units off the map), counted apart and not gated.
 *
 * Objectives, for the running gametype only (map_gametype_objective):
 * each must have a main-component node within 256.  The level graph fails
 * when the waypoint CSV covers more of them than it does.  An objective
 * neither graph reaches is a placeholder too (the mp_mideast zone parks its
 * sd, sab and hq objectives at z -816, 1200+ units under the level; the
 * mp_cha_quad zone parks every objective ~11000 off the map, CSV
 * included): falling back gains nothing there, so it is reported and the
 * level graph stays.  Both maps now run with maps/mp/<map>.d3dbsp.ents
 * (map_ents_override.c), which this pass reads, since it parses the
 * entity string the override replaced. */
static int map_spawn_coverage(int main_label, unsigned *spawns_out, unsigned *covered_out)
{
    SpawnVar parsed;
    unsigned spawns = 0, covered = 0, printed = 0, objectives = 0, obj_map = 0, obj_csv = 0, parked = 0;
    cvar_t *gametype_var = Cvar_FindVar("g_gametype");
    const char *gametype = gametype_var && gametype_var->string ? gametype_var->string : "";
    const char *text = CM_EntityString();
    int has_dd = text && strstr(text, "\"targetname\" \"dd_bombzone\"") != NULL;
    int has_arena = text && strstr(text, "\"targetname\" \"flag_arena\"") != NULL;
    G_ResetEntityParsePoint();
    while (G_ParseSpawnVars(&parsed)) {
        const char *cls = map_spawn_value(&parsed, "classname"), *origin = map_spawn_value(&parsed, "origin");
        const char *tn = map_spawn_value(&parsed, "targetname");
        float point[3];
        int node, is_spawn, is_objective, near_main;
        if (!cls || !origin || sscanf(origin, "%f %f %f", &point[0], &point[1], &point[2]) != 3) continue;
        is_spawn = !strncmp(cls, "mp_", 3) && strstr(cls, "_spawn") != NULL;
        is_objective = map_gametype_objective(gametype, tn, has_dd, has_arena);
        if (!is_spawn && !is_objective) continue;
        node = BotNavNearest(point);
        if (is_spawn && (node < 0 || distance(point, nodes[node].point) > 2048.0f)) { ++parked; continue; }
        near_main = node >= 0 && distance(point, nodes[node].point) <= 256.0f && nodes[node].component == main_label;
        if (is_spawn) { ++spawns; if (near_main) ++covered; }
        else {
            int by_csv = csv_copy_covers(point);
            ++objectives; obj_map += near_main; obj_csv += by_csv;
            if (!near_main && printed++ < MAP_BAD_PRINT)
                Com_PrintWarning(CON_CHANNEL_SERVER, "CoDOL waypoint graph: %s objective %s at (%s) has no main-component node within 256 (nearest %.0f away; the CSV %s it)\n",
                    gametype, tn, origin, node >= 0 ? distance(point, nodes[node].point) : -1.0f, by_csv ? "reaches" : "does not reach");
            continue;
        }
        if (!near_main && printed++ < MAP_BAD_PRINT)
            Com_PrintWarning(CON_CHANNEL_SERVER, "CoDOL waypoint graph: %s at (%s) has no main-component node within 256 (nearest %d, %.0f away)\n",
                cls, origin, node, node >= 0 ? distance(point, nodes[node].point) : -1.0f);
    }
    G_ResetEntityParsePoint();
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL waypoint graph: spawn coverage %u of %u spawn points (%u parked off the level); %s objectives: level graph reaches %u of %u, the CSV %u\n",
        covered, spawns, parked, gametype, obj_map, objectives, obj_csv);
    *spawns_out = spawns; *covered_out = covered;
    if (obj_csv > obj_map) {
        Com_Printf(CON_CHANNEL_SERVER, "CoDOL waypoint graph: the CSV reaches more %s objectives than the level graph\n", gametype);
        return 0;
    }
    return spawns == 0 || covered * 100 >= spawns * 95;
}
void BotNavBuildFromMapNodesIfEmpty(void)
{
    unsigned total = PathNodeCount(), i, j, used = 0, links = 0, negotiation = 0, negotiation_dropped = 0, isolated = 0, sinks = 0;
    unsigned bad[3] = {0, 0, 0}, printed = 0, components = 0, main_component = 0, main_size = 0, off_main = 0;
    int replacedCsv = 0, mpPrefer = 0;
    int *index, *head, *next, *cx, *cy, *parent, *entering;
    int start_ms = Sys_Milliseconds();
    map_actor_maxs[2] = MAP_ACTOR_HEIGHT;
    map_link_mask = MAP_LINK_TRACE_MASK;
    if (!total) return;                /* the map has no nodes */
    if (count > 0) {
        /* A CSV graph is loaded.  It stays for mp (the bots were tuned on
         * it) but not for coop: the coop actors are IW4 SP soldiers, and
         * the base levels carry IW4's own SP node set for them
         * (mp_estate_tropical: 2484 node_* records -- 1194 path nodes, 403
         * cover left/right, 115 cover stand/crouch, 72 concealment and 19
         * negotiation pairs with their traversal animscripts), where the
         * hand-authored player CSV has 447 points, no cover and no
         * traversals.  coopMapGraph 0 keeps the CSV for coop too. */
        static cvar_t *coop_map_graph;
        const char *zone = ServerZoneType();
        if (!coop_map_graph)
            coop_map_graph = Cvar_RegisterBool("coopMapGraph", qtrue, 0,
                "Coop actors route over the level's own node_* graph (cover nodes, traversals) instead of the bot waypoint CSV");
        if (ZoneTypeRunsSoldiers(zone) && coop_map_graph->boolean) {
            Com_Printf(CON_CHANNEL_SERVER, "CoDOL waypoint graph: coop match: replacing the %d-point waypoint CSV with the level's %u node_* records\n",
                count, total);
        } else if (zone && !strcmp(zone, "mp") && map_graph_preferred(ServerMapName())) {
            Com_Printf(CON_CHANNEL_SERVER, "CoDOL waypoint graph: mp match: trying the level's %u node_* records over the %d-point waypoint CSV (preferMapGraph)\n",
                total, count);
            csv_copy_free();
            csv_copy = malloc((size_t)count * sizeof(*csv_copy));
            if (csv_copy) { memcpy(csv_copy, nodes, (size_t)count * sizeof(*csv_copy)); csv_copy_count = count; }
            mpPrefer = 1;
            map_actor_maxs[2] = MAP_PLAYER_HEIGHT;
            map_link_mask = MAP_PLAYER_TRACE_MASK;
        } else return;
        count = 0;
        replacedCsv = 1;
    }
    index = calloc(total, sizeof(*index));
    head = malloc(MAP_GRID_HASH * sizeof(*head));
    next = calloc(total, sizeof(*next));
    cx = calloc(total, sizeof(*cx));
    cy = calloc(total, sizeof(*cy));
    parent = calloc(total, sizeof(*parent));
    entering = calloc(total, sizeof(*entering));
    if (!index || !head || !next || !cx || !cy || !parent || !entering) {
        free(index); free(head); free(next); free(cx); free(cy); free(parent); free(entering);
        return;
    }
    memset(nodes, 0, sizeof(nodes));
    memset(map_hist, 0, sizeof(map_hist));
    map_fast_path = 0;
    for (i = 0; i < MAP_GRID_HASH; ++i) head[i] = -1;
    for (i = 0; i < total; ++i) {
        const char *cls = PathNodeValue(i, "classname"), *flags = PathNodeValue(i, "spawnflags");
        int reason;
        index[i] = -1;
        if (!cls || strncmp(cls, "node_", 5)) continue;
        if (flags && (strtol(flags, NULL, 10) & 1)) continue;         /* T5: DONT_LINK */
        if (used >= BOT_NODES) break;
        if (!PathNodeOrigin(i, nodes[used].point)) continue;
        if (!map_drop_to_floor(nodes[used].point, &reason)) {
            static const char *const why[3] = {"is in solid", "is floating", "lands in solid"};
            ++bad[reason];
            if (printed++ < MAP_BAD_PRINT)
                Com_PrintError(CON_CHANNEL_SERVER, "ERROR: Pathnode (%s) at (%g %g %g) %s\n", cls,
                    nodes[used].point[0], nodes[used].point[1], nodes[used].point[2], why[reason]);
            continue;
        }
        nodes[used].stance = strcmp(cls, "node_cover_crouch") == 0 || strcmp(cls, "node_cover_prone") == 0
            ? BOT_CROUCH : BOT_STAND;
        nodes[used].record = (short)i;
        nodes[used].negotiationEnd = -1;
        cx[used] = map_grid_cell(nodes[used].point[0]);
        cy[used] = map_grid_cell(nodes[used].point[1]);
        j = map_grid_hash(cx[used], cy[used]);
        next[used] = head[j]; head[j] = (int)used;
        parent[used] = (int)used;
        index[i] = (int)used++;
    }
    ((void)0);
    for (i = 0; i < total; ++i) {
        const char *cls, *target;
        int a = index[i], dcx, dcy;
        if (a < 0) continue;
        cls = PathNodeValue(i, "classname");
        if (cls && !strcmp(cls, "node_negotiation_begin")) {
            /* T5 Path_IsNegotiationLink: begin -> its targeted end, unconditionally
             * (dist 15).  Geometric links are still attempted below, as T5 does. */
            target = PathNodeValue(i, "target");
            for (j = 0; target && j < total; ++j) {
                const char *tn = index[j] >= 0 ? PathNodeValue(j, "targetname") : NULL;
                if (tn && !strcmp(tn, target) && nodes[a].count < LINKS) {
                    if (mpPrefer && !map_player_can_jump(nodes[a].point, nodes[index[j]].point)) { ++negotiation_dropped; continue; }
                    nodes[a].links[nodes[a].count++] = (unsigned short)index[j];
                    nodes[a].negotiationEnd = (short)index[j];
                    nodes[a].stance = nodes[index[j]].stance = BOT_NEGOTIATE;   /* T5 traversal link: every actor may take it */
                    ++entering[index[j]];
                    parent[map_find(parent, a)] = map_find(parent, index[j]);
                    ++links; ++negotiation;
                }
            }
        }
        /* Every other node in the 3x3 grid neighbourhood is a candidate; the
         * T5 dz/xy gates then the walk decide.  Nodes outside the
         * neighbourhood are more than 256 apart on some axis and fail xy. */
        for (dcy = -1; dcy <= 1; ++dcy) for (dcx = -1; dcx <= 1; ++dcx) {
            int b;
            for (b = head[map_grid_hash(cx[a] + dcx, cy[a] + dcy)]; b >= 0; b = next[b]) {
                float dx, dy, dz;
                int r, k, dup = 0;
                if (cx[b] != cx[a] + dcx || cy[b] != cy[a] + dcy || b == a) continue;
                dx = nodes[b].point[0] - nodes[a].point[0];
                dy = nodes[b].point[1] - nodes[a].point[1];
                dz = nodes[b].point[2] - nodes[a].point[2];
                if (dz < -MAP_LINK_MAX_DZ || dz > MAP_LINK_MAX_DZ) { ++map_hist[MAP_R_DZ]; continue; }
                if (dx*dx + dy*dy > MAP_LINK_MAX_XY*MAP_LINK_MAX_XY) { ++map_hist[MAP_R_XY]; continue; }
                for (k = 0; k < nodes[a].count; ++k) if (nodes[a].links[k] == b) dup = 1;
                if (dup) continue;                                   /* the negotiation link above */
                r = map_link_walk(nodes[a].point, nodes[b].point);
                if (r == MAP_R_LINK && nodes[a].count >= LINKS) r = MAP_R_LINK_CAP;
                ++map_hist[r];
                if (r != MAP_R_LINK) continue;
                nodes[a].links[nodes[a].count++] = (unsigned short)b;
                ++entering[b];
                parent[map_find(parent, a)] = map_find(parent, b);
                ++links;
            }
        }
        if (!nodes[a].count) {
            ++isolated;
            if (isolated <= MAP_BAD_PRINT)
                Com_PrintError(CON_CHANNEL_SERVER, "ERROR: Pathnode (%s) at (%g %g %g) has no connections leaving it\n",
                    cls ? cls : "?", nodes[a].point[0], nodes[a].point[1], nodes[a].point[2]);
        }
    }
    count = (int)used;
    ++nav_generation;
    nav_changed();
    nav_label_components();
    /* Component census: the largest component is "main"; every spawner and
     * exterior goal must reach it or the zombies it feeds can never arrive. */
    for (i = 0; i < used; ++i) {
        if (!entering[i]) ++sinks;
        if (map_find(parent, (int)i) == (int)i) ++components;
    }
    for (i = 0; i < used; ++i) {
        unsigned size = 0;
        if (map_find(parent, (int)i) != (int)i) continue;
        for (j = 0; j < used; ++j) if (map_find(parent, (int)j) == (int)i) ++size;
        if (size > main_size) { main_size = size; main_component = i; }
    }
    off_main = used - main_size;
    ((void)0);
    ((void)0);
    for (i = 0; i < MAP_R_COUNT; ++i)
        if (map_hist[i]) ((void)0);
    ((void)0);
    /* A level whose own graph falls apart (mp_killhouse_sh: 13 components,
     * the main one 243 of 732 nodes -- its SP nodes were laid for scripted
     * scenes, not for fighting across the map) keeps the waypoint CSV it was
     * playing on. */
    map_graph = 1;
    if (replacedCsv && main_size * 10 < used * 8 && ServerMapName()) {
        const char *map = ServerMapName();
        Com_Printf(CON_CHANNEL_SERVER,
            "CoDOL waypoint graph: the level's graph is fragmented (main component %u of %u nodes); keeping the waypoint CSV\n",
            main_size, used);
        free(index); free(head); free(next); free(cx); free(cy); free(parent); free(entering);
        csv_copy_free();
        BotNavLoadForMap(map);
        return;
    }
    if (mpPrefer) {
        unsigned spawns, covered;
        const char *map = ServerMapName();
        int main_label = used ? nodes[main_component].component : -1;
        if (!map_spawn_coverage(main_label, &spawns, &covered) && map) {
            Com_Printf(CON_CHANNEL_SERVER,
                "CoDOL waypoint graph: the level's graph misses spawn points or objectives (%u of %u spawns covered); keeping the waypoint CSV\n",
                covered, spawns);
            free(index); free(head); free(next); free(cx); free(cy); free(parent); free(entering);
            csv_copy_free();
            BotNavLoadForMap(map);
            return;
        }
        graft_csv_nodes(used);
        csv_copy_free();
        map_actor_maxs[2] = MAP_ACTOR_HEIGHT;
        map_link_mask = MAP_LINK_TRACE_MASK;
        Com_Printf(CON_CHANNEL_SERVER, "CoDOL waypoint graph: %s routes its bots over the level graph (%d nodes)\n",
            map ? map : "?", count);
        free(index); free(head); free(next); free(cx); free(cy); free(parent); free(entering);
        return;
    }
    if (used) {
        /* zombie_spawner structs name their node by target; exterior_goal
         * structs sit outside the window, so the nearest node stands in. */
        SpawnVar parsed;
        unsigned spawners = 0, goals = 0, spawners_off = 0, goals_off = 0;
        G_ResetEntityParsePoint();
        while (G_ParseSpawnVars(&parsed)) {
            const char *cls = map_spawn_value(&parsed, "classname"), *note, *tn, *target, *origin;
            float point[3];
            int node = -1, is_spawner, is_goal;
            if (!cls || strcmp(cls, "script_struct")) continue;
            note = map_spawn_value(&parsed, "script_noteworthy"); tn = map_spawn_value(&parsed, "targetname");
            is_spawner = note && !strcmp(note, "zombie_spawner");
            is_goal = tn && !strcmp(tn, "exterior_goal");
            if (!is_spawner && !is_goal) continue;
            target = map_spawn_value(&parsed, "target");
            for (j = 0; target && j < total && node < 0; ++j) {
                const char *ntn = index[j] >= 0 ? PathNodeValue(j, "targetname") : NULL;
                if (ntn && !strcmp(ntn, target)) node = index[j];
            }
            origin = map_spawn_value(&parsed, "origin");
            if (node < 0 && origin && sscanf(origin, "%f %f %f", &point[0], &point[1], &point[2]) == 3)
                node = BotNavNearest(point);
            if (is_spawner) ++spawners; else ++goals;
            if (node < 0 || map_find(parent, node) != map_find(parent, (int)main_component)) {
                if (is_spawner) ++spawners_off; else ++goals_off;
                if (spawners_off + goals_off <= MAP_BAD_PRINT)
                    Com_PrintError(CON_CHANNEL_SERVER, "CoDOL waypoint graph: %s at (%s) -> node %d is off the main component\n",
                        is_spawner ? "zombie_spawner" : "exterior_goal", origin ? origin : "?", node);
            }
        }
        G_ResetEntityParsePoint();
        ((void)0);
    }
    free(index); free(head); free(next); free(cx); free(cy); free(parent); free(entering);
}
const float *BotNavPoint(int node){return node>=0&&node<count?nodes[node].point:NULL;}
int BotNavStance(int node){return node>=0&&node<count?nodes[node].stance:BOT_STAND;}
int BotNavComponent(int node){return node>=0&&node<count?nodes[node].component:-1;}
const bot_route_stats_t *BotNavLastRouteStats(void){return &route_stats;}
/* The closed set of the last search (BotNavRouteFiltered). */
static unsigned char route_closed[BOT_NODES];
int BotNavLastSearchReached(int node)
{
    if(route_stats.stop!=1||node<0||node>=count)return -1;
    return route_closed[node]!=0;
}
int BotNavNearest(const float *point)
{int i,best=-1;float d,minimum=FLT_MAX;for(i=0;i<count;++i){d=distance(point,nodes[i].point);if(d<minimum){minimum=d;best=i;}}return best;}
const bot_annotation_t *BotNavAnnotation(int node)
{return node>=0&&node<count?&nodes[node].annotation:NULL;}
/* A* over the graph.  The open set is a binary heap keyed on f = g + h
 * (lazy deletion: a node pushed again with a better f leaves its stale entry
 * behind, skipped when popped closed).  The former linear scan for the open
 * node with the lowest f cost O(n) per expansion, O(n^2) per route: about 6
 * million comparisons for a long route over the 2484-node mp_estate_tropical
 * graph the coop actors route on, several of them in one frame when a squad
 * picks cover.  Same costs, same heuristic, same filter and same result
 * contract; equal-f ties may resolve to a different equal-cost path. */
typedef struct { float f; unsigned short node; } nav_heap_t;
static void nav_heap_push(nav_heap_t *heap,int *size,float f,int node)
{
    int i=(*size)++;
    while(i>0){int parent=(i-1)>>1;if(heap[parent].f<=f)break;heap[i]=heap[parent];i=parent;}
    heap[i].f=f;heap[i].node=(unsigned short)node;
}
SH1X_ONDEMAND(, nav_heap_t, bot_route_heap, [BOT_NODES*LINKS+1]); /* the A* open set (sys_ondemand.h) */
static nav_heap_t nav_heap_pop(nav_heap_t *heap,int *size)
{
    nav_heap_t top=heap[0],last=heap[--*size];int i=0,n=*size;
    for(;;){int c=2*i+1;if(c>=n)break;if(c+1<n&&heap[c+1].f<heap[c].f)++c;if(heap[c].f>=last.f)break;heap[i]=heap[c];i=c;}
    if(n>0)heap[i]=last;
    return top;
}
int BotNavRouteFiltered(int from,int to,unsigned short *path,int capacity,bot_edge_filter_t allow,void *context)
{
    /* Static: 4096 nodes of g, parent, closed, the reverse path and a heap
     * with room for every link push would be ~1.7 MB of stack.  The planner
     * is only ever entered from the server frame (never re-entrantly). */
    static float cost[BOT_NODES];
    static short parent[BOT_NODES];unsigned char *closed=route_closed;
    static unsigned short reverse[BOT_NODES];
    nav_heap_t *heap=*sh1x_od_bot_route_heap;   /* one push per relaxed link at most (sys_ondemand.h) */
    int i,j,current,next,length=0,heapSize=0;
    const int heapCapacity=(int)(sizeof(*sh1x_od_bot_route_heap)/sizeof(nav_heap_t));
    memset(&route_stats,0,sizeof(route_stats));
    if(from<0||to<0||from>=count||to>=count||!path||capacity<1){route_stats.stop=2;return 0;}
    for(i=0;i<count;++i){cost[i]=FLT_MAX;parent[i]=-1;closed[i]=0;}
    cost[from]=0;nav_heap_push(heap,&heapSize,distance(nodes[from].point,nodes[to].point),from);
    while(heapSize>0){
        current=nav_heap_pop(heap,&heapSize).node;
        if(closed[current])continue;
        if(current==to){for(j=to;j!=-1;j=parent[j]){if(length>=capacity){route_stats.stop=3;return 0;}reverse[length++]=(unsigned short)j;}for(j=0;j<length;++j)path[j]=reverse[length-j-1];return length;}
        closed[current]=1;++route_stats.expanded;
        for(j=0;j<nodes[current].count;++j){float candidate;next=nodes[current].links[j];if(closed[next])continue;
            if(nodes[current].blocker[j])continue;   /* cut by DisconnectPaths (a barrier clip, a door) */
            if(allow && !allow(current,next,context)){++route_stats.filtered;continue;}
            candidate=cost[current]+distance(nodes[current].point,nodes[next].point);
            if(candidate<cost[next]){
                parent[next]=(short)current;cost[next]=candidate;
                /* heap overflow: not an exhausted frontier (closed is partial) */
                if(heapSize>=heapCapacity){route_stats.stop=3;return 0;}
                nav_heap_push(heap,&heapSize,candidate+distance(nodes[next].point,nodes[to].point),next);
            }
        }
    }
    route_stats.stop=1;
    return 0;
}
int BotNavRoute(int from,int to,unsigned short *path,int capacity)
{return BotNavRouteFiltered(from,to,path,capacity,NULL,NULL);}
/* Directed, bounded flood: the same blocked links and caller stance/link
 * filter as RouteFiltered, without one A* search per player every frame. */
int BotNavReachableNodes(int from, uint32_t *bits, unsigned words,
    bot_edge_filter_t allow, void *context)
{
    unsigned short queue[BOT_NODES];
    unsigned head = 0, tail = 0;
    if (!bits || words < (unsigned)(count + 31) / 32) return 0;
    memset(bits, 0, words * sizeof(*bits));
    if (from < 0 || from >= count) return 0;
    bits[(unsigned)from >> 5] |= UINT32_C(1) << (from & 31);
    queue[tail++] = (unsigned short)from;
    while (head < tail) {
        int current = queue[head++];
        for (int i = 0; i < nodes[current].count; ++i) {
            int next = nodes[current].links[i];
            uint32_t bit = UINT32_C(1) << (next & 31);
            if ((bits[(unsigned)next >> 5] & bit) || nodes[current].blocker[i] ||
                (allow && !allow(current, next, context))) continue;
            bits[(unsigned)next >> 5] |= bit;
            queue[tail++] = (unsigned short)next;
        }
    }
    return (int)tail;
}
int BotNavNegotiationEnd(int node){return node>=0&&node<count?nodes[node].negotiationEnd:-1;}
int BotNavRecord(int node){return node>=0&&node<count?nodes[node].record:-1;}
/* T5 Path_DisconnectPathsForEntity keeps, per entity, the list of links it
 * cut (gentity_s::disconnectedLinks over g_path.pathLinkInfoArray) and, per
 * link, how many entities cut it (pathlink_s::disconnectCount): a link is
 * open again only when every entity that cut it has connected its paths
 * (Path_ConnectPath_0 reconnects at a count of zero).  This kept a single
 * owner per link, the first cutter, until 2026-10-21: a link crossing two
 * blockers was reopened by whichever of them connected first, and a link
 * the second blocker crossed was never cut by it at all. */
typedef struct { unsigned short entity,node; unsigned char link; } nav_cut_t;
#define NAV_CUT_CAP 65536
SH1X_ONDEMAND(, nav_cut_t, bot_cut_records, [NAV_CUT_CAP]); /* sys_ondemand.h: allocated when the server starts */
#define cut_records (*sh1x_od_bot_cut_records)
static int cut_count,cut_generation=-1;
static void nav_cuts_current(void)
{
    /* A new graph (map change, CSV load) starts with every link open. */
    if(cut_generation!=nav_generation){cut_count=0;cut_generation=nav_generation;}
}
int BotNavLinkBlocked(int from,int to)
{
    int j,k;
    if(from<0||from>=count)return 0;
    nav_cuts_current();
    for(j=0;j<nodes[from].count;++j)if(nodes[from].links[j]==to){
        if(!nodes[from].blocker[j])return 0;
        for(k=0;k<cut_count;++k)if(cut_records[k].node==from&&cut_records[k].link==j)return cut_records[k].entity+1;
        return 1;
    }
    return 0;
}
/* Segment a->b, swept as the actor hull (T5 actorMins/Maxs {-15,-15,0}..
 * {15,15,48}, the hull the links were walked with), against the box:
 * slab test on the box grown by the hull.  Node points sit on the floor,
 * so the hull's height is what lets a floor-level link meet a clip brush
 * whose bottom is above the floor (the zm window clips start ~25 up). */
static int nav_segment_crosses_box(const float *a,const float *b,const float *mins,const float *maxs)
{
    float lo[3],hi[3],t0=0,t1=1;int i;
    for(i=0;i<3;++i){lo[i]=mins[i]-(i==2?48.0f:15.0f);hi[i]=maxs[i]+(i==2?0.0f:15.0f);}
    for(i=0;i<3;++i){
        float d=b[i]-a[i];
        if(fabsf(d)<0.0001f){if(a[i]<lo[i]||a[i]>hi[i])return 0;continue;}
        {float ta=(lo[i]-a[i])/d,tb=(hi[i]-a[i])/d;if(ta>tb){float x=ta;ta=tb;tb=x;}
         if(ta>t0)t0=ta;if(tb<t1)t1=tb;if(t0>t1)return 0;}
    }
    return 1;
}
/* T5 Path_DisconnectPathsForEntity (OpenBLOPS pathnode.cpp:3182): a link
 * near the entity is cut only when the capsule {-15,-15,18}..{15,15,48}
 * swept along it hits the ENTITY'S OWN brushes (SV_SightTraceCapsuleToEntity,
 * mask 0x2820011), not its bounding box.  The box test alone cut every link
 * through a diagonal window's AABB: zm_zombie_training's window at
 * (565 1335 272) faces 225 degrees, its clip's box (497 1266)-(567 1336)
 * swallowed all 37 links into the negotiation begin node from the yard, and
 * every zombie from that yard found no path to its own window. */
static int nav_link_hits_entity(int entnum,const float *a,const float *b)
{
    static const float cmins[3]={-15.0f,-15.0f,18.0f},cmaxs[3]={15.0f,15.0f,48.0f};
    gentity_t *ent=&g_entities[entnum];
    trace_t tr;
    /* An entity with none of the traced contents (a script_model chunk) is
     * judged by its box, as before. */
    if(!(ent->r.contents&0x2820011))return 1;
    /* CM_TransformedBoxTrace only lowers the fraction it is given (the
     * retained SV_ClipMoveToEntity passes its running trace); SV_ClipToEntity
     * zeroes it first and so reports a hit for every trace. */
    memset(&tr,0,sizeof(tr));
    tr.fraction=1.0f;
    CM_TransformedBoxTrace(&tr,a,b,cmins,cmaxs,SV_ClipHandleForEntity(ent),0x2820011,ent->r.currentOrigin,
        ent->r.bmodel?ent->r.currentAngles:vec3_origin);
    return tr.startsolid||tr.allsolid||tr.fraction<1.0f;
}
int BotNavConnectPaths(int entnum)
{
    int k,kept=0,changed=0;
    if(entnum<0||entnum>=65534)return 0;
    nav_cuts_current();
    for(k=0;k<cut_count;++k){
        nav_cut_t r=cut_records[k];
        if(r.entity!=(unsigned short)entnum){cut_records[kept++]=r;continue;}
        if(r.node<count&&r.link<nodes[r.node].count&&nodes[r.node].blocker[r.link]){
            --nodes[r.node].blocker[r.link];++changed;
        }
    }
    cut_count=kept;
    if (changed) nav_changed();
    return changed;
}
int BotNavDisconnectPaths(int entnum,const float *mins,const float *maxs)
{
    int i,j,changed=0;
    if(entnum<0||entnum>=65534||!mins||!maxs)return 0;
    /* T5 :3206: an entity's earlier cut is released first, so calling it
     * again (the zm barrier repair loop cuts its window every board) moves
     * the cut rather than stacking it. */
    BotNavConnectPaths(entnum);
    for(i=0;i<count;++i)for(j=0;j<nodes[i].count;++j){
        if(!nav_segment_crosses_box(nodes[i].point,nodes[nodes[i].links[j]].point,mins,maxs))continue;
        if(entnum<ENTITYNUM_WORLD&&!nav_link_hits_entity(entnum,nodes[i].point,nodes[nodes[i].links[j]].point))continue;
        if(cut_count>=NAV_CUT_CAP||nodes[i].blocker[j]==0xffff)continue;
        cut_records[cut_count].entity=(unsigned short)entnum;
        cut_records[cut_count].node=(unsigned short)i;
        cut_records[cut_count].link=(unsigned char)j;
        ++cut_count;
        ++nodes[i].blocker[j];++changed;
    }
    if (changed) nav_changed();
    return changed;
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}
int PersistentTailNavCanReset(void){return nav_generation>=0&&nav_generation<INT_MAX&&csv_copy_count>=0&&csv_copy_count<=BOT_NODES&&((csv_copy==NULL)==(csv_copy_count==0));}
int PersistentTailNavEmpty(void){return !count&&!map_graph&&!csv_copy&&!csv_copy_count&&!cut_count&&cut_generation==nav_generation&&
 map_link_mask==MAP_LINK_TRACE_MASK&&map_actor_maxs[0]==15.5f&&map_actor_maxs[1]==15.5f&&map_actor_maxs[2]==MAP_ACTOR_HEIGHT&&
 resident_tail_zero(nodes,sizeof(nodes))&&resident_tail_zero(map_hist,sizeof(map_hist))&&!map_fast_path&&
 resident_tail_zero(&route_stats,sizeof(route_stats))&&resident_tail_zero(route_closed,sizeof(route_closed))&&resident_tail_zero(cut_records,sizeof(cut_records));}
void PersistentTailNavReset(void){csv_copy_free();memset(nodes,0,sizeof(nodes));count=map_graph=0;
 memset(map_hist,0,sizeof(map_hist));map_fast_path=0;memset(&route_stats,0,sizeof(route_stats));memset(route_closed,0,sizeof(route_closed));
 memset(cut_records,0,sizeof(cut_records));cut_count=0;++nav_generation;cut_generation=nav_generation;
 map_actor_maxs[0]=map_actor_maxs[1]=15.5f;map_actor_maxs[2]=MAP_ACTOR_HEIGHT;map_link_mask=MAP_LINK_TRACE_MASK;}
#endif

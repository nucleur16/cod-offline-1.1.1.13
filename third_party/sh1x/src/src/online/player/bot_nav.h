#ifndef BOT_NAV_H
#define BOT_NAV_H
/* Co-op mp_estate_tropical has2484 authored nodes; preserve canonical4096 capacity. */
#define BOT_NODES 4096
#include <stdint.h>
/* CSV stance/negotiation types; tactical-placement rows remain navigable.
 * BOT_CLIMB is the hand-authored CSV's player mantle (bots only);
 * BOT_NEGOTIATE is a map node_negotiation_begin/_end pair, the link
 * T5 Path_IsNegotiationLink gives every actor (a window sill, a low wall). */
enum { BOT_STAND, BOT_CROUCH, BOT_PRONE, BOT_CLIMB, BOT_NEGOTIATE };
/* What the last BotNavRouteFiltered did, for the caller's failure line:
 * nodes taken off the open set, links the caller's filter rejected, and why
 * it stopped (0 found, 1 frontier exhausted, 2 bad node index, 3 path longer
 * than the caller's capacity). */
typedef struct { int expanded, filtered, stop; } bot_route_stats_t;
const bot_route_stats_t *BotNavLastRouteStats(void);
/* After a BotNavRouteFiltered that exhausted its frontier (stats stop
 * 1), whether that search reached `node`: an exhausted search closes every
 * node its start reaches over the links its filter allows, so this answers
 * "is there a route from the same start, under the same filter, to node"
 * without another full search.  -1 when the last search did not exhaust its
 * frontier.  Valid only until the next search or graph change; callers use
 * it within one planning step. */
int BotNavLastSearchReached(int node);
/* Undirected connected component of a node (0-based label); -1 off the graph. */
int BotNavComponent(int node);
enum { BOT_TACTIC_NONE,BOT_TACTIC_CLAYMORE,BOT_TACTIC_GRENADE,BOT_TACTIC_JAVELIN };
typedef struct { float angles[3],target[3]; unsigned char hasAngles,hasTarget,tactic; } bot_annotation_t;
const bot_annotation_t *BotNavAnnotation(int node);
typedef int (*bot_edge_filter_t)(int from,int to,void *context);
int BotNavRouteFiltered(int from,int to,unsigned short *path,int capacity,bot_edge_filter_t allow,void *context);
/* Changes on graph replacement and actual door/barrier link cuts/restores.
 * Static component labels do not represent those dynamic changes. */
uint64_t BotNavTopologyRevision(void);
int BotNavReachableNodes(int from, uint32_t *bits, unsigned words,
    bot_edge_filter_t allow, void *context);
int BotNavLoad(const char *path);
/* Per-map load: "waypoints/<mapname>_wp.csv", or an empty graph when the
 * map ships none.  Owned by the map lifecycle, not by one category's GSC. */
int BotNavLoadForMap(const char *mapname);
int BotNavCount(void);
/* Non-zero when the loaded graph is the level's own node graph (zm/coop, or
 * an mp map that prefers it over its CSV: preferMapGraph). */
int BotNavIsMapGraph(void);
/* Changes whenever the graph is replaced (a new map, a CSV load). */
int BotNavGeneration(void);
const float *BotNavPoint(int node);
int BotNavStance(int node);
int BotNavNearest(const float *point);
int BotNavRoute(int from,int to,unsigned short *path,int capacity);
/* Map-graph negotiation pairs (T5 node_negotiation_begin -> _end).  For a
 * begin node: the index of its end node, else -1.  Record is the index of
 * the node's entity record in script_path_nodes (its "animscript",
 * "angles" keys); -1 for CSV nodes. */
int BotNavNegotiationEnd(int node);
int BotNavRecord(int node);
/* T5 Path_DisconnectPaths / Path_ConnectPaths, per entity: every link whose
 * segment (swept as the actor hull) crosses [mins,maxs] is cut and tagged
 * with the entity; ConnectPaths(entity) restores exactly the links that
 * entity cut, wherever the entity is now (the zm clip brush is moved 10000
 * units down by disable_trigger before connectpaths is called).  Both return
 * the number of links changed.  A cut link is skipped by the planner. */
int BotNavDisconnectPaths(int entnum,const float *mins,const float *maxs);
int BotNavConnectPaths(int entnum);
/* Non-zero when the directed link from -> to is currently cut (its cutter's
 * entity number + 1), 0 when open or absent. */
int BotNavLinkBlocked(int from,int to);
#endif

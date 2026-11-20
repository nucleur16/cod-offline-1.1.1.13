/* Coop actor cover nodes (actor_cover.c).  No structs cross this
 * header: every query is by cover index.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ACTOR_COVER_H
#define ACTOR_COVER_H
#ifdef __cplusplus
extern "C" {
#endif
/* IW4 node types that hold a soldier (animscripts\init_move_transitions
 * anim.approach_types): the corner pair, the wall pair and the window,
 * prone, and the three concealment types. */
enum {
    COVER_NONE = 0,
    COVER_LEFT, COVER_RIGHT, COVER_STAND, COVER_CROUCH,
    COVER_CROUCH_WINDOW, COVER_PRONE,
    COVER_CONCEAL_STAND, COVER_CONCEAL_CROUCH, COVER_CONCEAL_PRONE,
    COVER_TYPES
};
/* Cover nodes of the current graph (built on first use per graph). */
int CoverCount(void);
int CoverType(int cover);
/* The node_* record (script_path_nodes.c index, the script's pathnode
 * object) and the graph node it is. */
int CoverRecord(int cover);
int CoverNavNode(int cover);
/* The node dropped to the floor (the graph's point), and its yaw. */
int CoverPoint(int cover, float *origin);
float CoverYaw(int cover);
/* The cover index of a node record, else -1. */
int CoverForRecord(int record);
/* Claims: one live actor per node.  Claim answers 1 when the node is now
 * the entity's (a claimant that died or left is replaced). */
int CoverClaimant(int cover);
int CoverClaim(int cover, int entnum);
void CoverRelease(int cover, int entnum);
/* IW4 FindBestCoverNode for a soldier at origin against an enemy standing at
 * enemyFeet with his eye at enemyEye: a free node on the actor's graph
 * component inside searchRadius, between minDist and maxDist from the enemy,
 * facing him, that hides the soldier from him (wall types) and preferably
 * lets him shoot from it.  avoid lists cover indices the actor gave up on.
 * Returns the cover index or -1; *why names the last rejection. */
int CoverFind(int entnum, const float *origin, const float *enemyFeet, const float *enemyEye,
    float minDist, float maxDist, float searchRadius, int component,
    const int *avoid, int avoidCount, const char **why, int *flags);
/* CoverFind's *flags: the node hides him from the enemy, and he can
 * shoot the enemy from its firing position. */
#define COVER_FOUND_HIDDEN 1
#define COVER_FOUND_SHOOTABLE 2
/* Is the node still worth holding against this enemy (in its arc and, for a
 * wall, still hiding the soldier)?  0 with *why when not. */
int CoverStillValid(int cover, const float *enemyFeet, const float *enemyEye, const char **why);
/* IW4's node.type string for a node_* classname ("Cover Left", "Path", ...)
 * (script_path_nodes.c). */
const char *PathNodeTypeName(const char *classname);
#ifdef __cplusplus
}
#endif
#endif

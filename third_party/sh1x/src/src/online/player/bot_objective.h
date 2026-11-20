#ifndef BOT_OBJECTIVE_H
#define BOT_OBJECTIVE_H
/* Objective play for combat-training bots. The objective choice lives in
 * maps/mp/gametypes/_codol_bot_objective.gsc; native code supplies what script
 * cannot see: the bot's current threat (T5 GetThreat) and waypoint-graph
 * points around an objective for patrolling it.
 * SPDX-License-Identifier: AGPL-3.0-or-later */

/* Engine-free picker (unit tested): a uniformly random node whose 2D distance
 * from anchor lies in [minRadius,maxRadius], whose height differs by at most
 * maxHeight, that is a standing node, and that lies in wantComponent (when
 * wantComponent >= 0). Returns -1 when no node qualifies. */
typedef const float *(*bot_objective_point_fn)(int node);
typedef int (*bot_objective_int_fn)(int node);
int BotObjectivePickNear(int count,bot_objective_point_fn point,
    bot_objective_int_fn component,bot_objective_int_fn stance,
    const float *anchor,float minRadius,float maxRadius,float maxHeight,
    int wantComponent,unsigned *seed);

#ifdef OL_SERVER
/* Implemented in ol_bot.c: the entity number of the enemy this combat
 * bot has seen within maxAgeMs, else -1. */
int BotThreat(int slot,int maxAgeMs);
/* Registers getthreat and codol_botnavpointnear. */
void RegisterBotObjectiveMethods(void);
#endif
#endif

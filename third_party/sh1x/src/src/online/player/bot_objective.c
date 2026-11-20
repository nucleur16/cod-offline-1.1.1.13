/* Script-facing natives for combat-training bot objective play
 * (maps/mp/gametypes/_codol_bot_objective.gsc).
 *
 *   <bot> getThreat()  OpenBLOPS T5 GetThreat: the enemy entity the native
 *                      bot AI is engaging (seen in the last 3 s), else
 *                      undefined. T5 _bot.gsc bot_is_idle() uses it so that
 *                      script errands never pull a bot out of a firefight.
 *   codol_botNavPointNear( <origin>, <maxRadius> [, <minRadius>] )
 *                      a random standing waypoint within maxRadius (2D) of
 *                      origin, reachable from the waypoint nearest origin,
 *                      or undefined. Used to patrol around a held objective
 *                      instead of standing on one spot.
 *
 * Native usercmd gameplay only: no teleporting, no scripted damage.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "bot_objective.h"
#include "bot_nav.h"
#include "server.h"
#include "g_shared.h"
#include "scr_vm.h"
#include "scr_vm_functions.h"
#include <math.h>
#include <string.h>

static unsigned objectiveSeed=0x9e3779b9u;

static int nav_stance(int node){return BotNavStance(node);}
static int nav_component(int node){return BotNavComponent(node);}

static void get_threat(scr_entref_t ref)
{
    gentity_t *ent,*enemy;int slot;
    if(ref.classnum||ref.entnum>=MAX_CLIENTS){Scr_ObjectError("not a player");return;}
    ent=&g_entities[ref.entnum];
    if(!ent->r.inuse||!ent->client||svs.clients[ref.entnum].netchan.remoteAddress.type!=NA_BOT){Scr_ObjectError("player is not a bot");return;}
    slot=BotThreat(ref.entnum,3000);
    if(slot<0||slot>=MAX_CLIENTS)return;
    enemy=&g_entities[slot];
    if(!enemy->r.inuse||!enemy->client||enemy->health<=0)return;
    Scr_AddEntity(enemy);
}

static void nav_point_near(void)
{
    float anchor[3],maxRadius,minRadius=0;int n,node,from,component=-1;const float *p;
    n=Scr_GetNumParam();
    if(n<2||n>3){Scr_Error("usage: codol_botNavPointNear( <origin>, <maxRadius> [, <minRadius>] )");return;}
    Scr_GetVector(0,anchor);maxRadius=Scr_GetFloat(1);
    if(n>2)minRadius=Scr_GetFloat(2);
    if(!isfinite(anchor[0])||!isfinite(anchor[1])||!isfinite(anchor[2])){Scr_ParamError(0,"origin must be finite");return;}
    if(!isfinite(maxRadius)||maxRadius<0||maxRadius>8192){Scr_ParamError(1,"radius must be 0..8192");return;}
    if(!isfinite(minRadius)||minRadius<0||minRadius>maxRadius){Scr_ParamError(2,"minimum radius must be 0..radius");return;}
    if(BotNavCount()<=0)return;
    /* Stay on the part of the graph that actually reaches the objective. */
    from=BotNavNearest(anchor);
    if(from>=0)component=BotNavComponent(from);
    objectiveSeed^=(unsigned)svs.time;
    node=BotObjectivePickNear(BotNavCount(),BotNavPoint,nav_component,nav_stance,
        anchor,minRadius,maxRadius,128,component,&objectiveSeed);
    if(node<0)return;
    p=BotNavPoint(node);
    if(p)Scr_AddVector(p);
}

void RegisterBotObjectiveMethods(void)
{
    Scr_AddMethod("getthreat",get_threat,0);
    Scr_AddFunction("codol_botnavpointnear",nav_point_near,0);
}
#endif

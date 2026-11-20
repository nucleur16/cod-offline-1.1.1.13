/* Engine-free part of combat-training bot objective play: choose a patrol
 * point on the waypoint graph around an objective (a flag, a bomb site, a
 * carrier being escorted). Kept free of engine headers so it is unit tested.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "bot_objective.h"
#include <math.h>
#include <stddef.h>

/* The stance value of a standing node (BOT_STAND in bot_nav.h). */
#define PICK_STAND 0

int BotObjectivePickNear(int count,bot_objective_point_fn point,
    bot_objective_int_fn component,bot_objective_int_fn stance,
    const float *anchor,float minRadius,float maxRadius,float maxHeight,
    int wantComponent,unsigned *seed)
{
    int i,seen=0,chosen=-1;
    float min2,max2;
    if(count<=0||!point||!anchor||!seed||!(maxRadius>=0)||!(minRadius>=0)||minRadius>maxRadius||!(maxHeight>=0))
        return -1;
    min2=minRadius*minRadius;max2=maxRadius*maxRadius;
    /* Reservoir sampling: one pass, every qualifying node equally likely. */
    for(i=0;i<count;++i){
        const float *p=point(i);float dx,dy,d2;
        if(!p)continue;
        dx=p[0]-anchor[0];dy=p[1]-anchor[1];d2=dx*dx+dy*dy;
        if(d2<min2||d2>max2||fabsf(p[2]-anchor[2])>maxHeight)continue;
        if(stance && stance(i)!=PICK_STAND)continue;
        if(wantComponent>=0 && component && component(i)!=wantComponent)continue;
        ++seen;
        *seed=*seed*1664525u+1013904223u;
        if((*seed>>8)%(unsigned)seen==0)chosen=i;
    }
    return chosen;
}

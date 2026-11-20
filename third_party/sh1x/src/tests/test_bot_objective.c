/* Unit test for the engine-free bot objective patrol-point picker.
 * Build: gcc -m32 -O1 -g -Isrc/online/player tests/test_bot_objective.c
 *        src/online/player/bot_objective_pick.c -lm
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "bot_objective.h"
#include <stdio.h>
#include <string.h>

static float pts[8][3]={
    {0,0,0},        /* 0: on the anchor */
    {100,0,0},      /* 1: in ring */
    {0,150,10},     /* 2: in ring */
    {300,0,0},      /* 3: too far */
    {0,-120,400},   /* 4: too high */
    {-110,0,0},     /* 5: crouch node */
    {80,80,0},      /* 6: other component */
    {-90,-90,-20},  /* 7: in ring */
};
static int stances[8]={0,0,0,0,0,1,0,0};
static int comps[8]={0,0,0,0,0,0,1,0};
static const float *point(int i){return i>=0&&i<8?pts[i]:NULL;}
static int stance(int i){return stances[i];}
static int comp(int i){return comps[i];}
static int failures;
#define CHECK(c) do{if(!(c)){printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c);++failures;}}while(0)

int main(void)
{
    float anchor[3]={0,0,0};unsigned seed=1;int i,hits[8];
    memset(hits,0,sizeof(hits));
    for(i=0;i<4000;++i){
        int n=BotObjectivePickNear(8,point,comp,stance,anchor,50,200,128,0,&seed);
        CHECK(n>=0&&n<8);
        if(n>=0&&n<8)hits[n]++;
    }
    /* Only 1, 2 and 7 qualify, and each is reachable by the sampler. */
    CHECK(hits[0]==0);CHECK(hits[3]==0);CHECK(hits[4]==0);CHECK(hits[5]==0);CHECK(hits[6]==0);
    CHECK(hits[1]>900);CHECK(hits[2]>900);CHECK(hits[7]>900);
    /* Without a component filter node 6 joins. */
    memset(hits,0,sizeof(hits));
    for(i=0;i<4000;++i){int n=BotObjectivePickNear(8,point,comp,stance,anchor,50,200,128,-1,&seed);if(n>=0)hits[n]++;}
    CHECK(hits[6]>500);
    /* Zero minimum radius admits the anchor node. */
    memset(hits,0,sizeof(hits));
    for(i=0;i<4000;++i){int n=BotObjectivePickNear(8,point,comp,stance,anchor,0,200,128,0,&seed);if(n>=0)hits[n]++;}
    CHECK(hits[0]>500);
    /* Nothing in range, bad arguments. */
    CHECK(BotObjectivePickNear(8,point,comp,stance,anchor,400,500,128,0,&seed)==-1);
    CHECK(BotObjectivePickNear(0,point,comp,stance,anchor,0,200,128,0,&seed)==-1);
    CHECK(BotObjectivePickNear(8,point,comp,stance,anchor,300,200,128,0,&seed)==-1);
    CHECK(BotObjectivePickNear(8,NULL,comp,stance,anchor,0,200,128,0,&seed)==-1);
    CHECK(BotObjectivePickNear(8,point,comp,stance,NULL,0,200,128,0,&seed)==-1);
    /* Null stance/component callbacks disable those filters. */
    CHECK(BotObjectivePickNear(8,point,NULL,NULL,anchor,105,115,128,0,&seed)==5);
    if(failures){printf("test_bot_objective: %d failure(s)\n",failures);return 1;}
    printf("test_bot_objective: ok\n");
    return 0;
}

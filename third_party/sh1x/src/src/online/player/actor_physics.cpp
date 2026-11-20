#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Native actor capsule slide/step/gravity. T5 actor_physics.cpp supplies the
 * behavioral reference; no actor_s offsets, player commands or fake proxies.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "actor_physics.h"
#include "server.h"
#include "g_shared.h"
#include <math.h>
#include <string.h>
static float dot(const float *a,const float *b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
extern "C" void ActorsPassable(gentity_s *self,int passable);
#define OVERCLIP 1.001f /* Q3 PM_SlideMove: clip slightly past the plane */
static void sweep(gentity_s *e,trace_t *t,const float *a,const float *b) {
    G_TraceCapsule(t,a,e->r.mins,e->r.maxs,b,e->s.number,e->clipmask);
}
/* A steep-face landing can persist for many 50-ms animation wishes even
 * though the actor has a clear authored step path a few units farther on.
 * Keep the exceptional look-ahead bounded to a sustained blocked actor. */
static int stepBlockedFrames[MAX_GENTITIES];
static int stepFallbackTime[MAX_GENTITIES];
static int slide(gentity_s *e,float *pos,float *vel,float dt) {
    float planes[5][3]; int count=0;
    for(int bump=0;bump<4;++bump) {
        float end[3];for(int i=0;i<3;++i)end[i]=pos[i]+vel[i]*dt;
        trace_t t;sweep(e,&t,pos,end);
        if(t.allsolid || t.startsolid) {memset(vel,0,12);return 0;}
        for(int i=0;i<3;++i)pos[i]+=(end[i]-pos[i])*t.fraction;
        if(t.fraction==1)return 1;
        dt*=1-t.fraction;
        if(count==5){memset(vel,0,12);return 1;}
        {
            int dup;
            for(dup=0;dup<count;++dup)if(dot(t.normal,planes[dup])>0.99f)break;
            if(dup<count){for(int k=0;k<3;++k)vel[k]+=t.normal[k];continue;}
        }
        memcpy(planes[count++],t.normal,12);
        float original[3];memcpy(original,vel,12);
        int i;
        for(i=0;i<count;++i) {
            float into=dot(original,planes[i]);
            if(into>=0)continue;
            into*=OVERCLIP;
            for(int k=0;k<3;++k)vel[k]=original[k]-planes[i][k]*into;
            int j;for(j=0;j<count;++j)if(j!=i && dot(vel,planes[j])<-.001f)break;
            if(j==count)break;
        }
        if(i==count && count>=2) {
            float crease[3]={planes[0][1]*planes[1][2]-planes[0][2]*planes[1][1],
                planes[0][2]*planes[1][0]-planes[0][0]*planes[1][2],
                planes[0][0]*planes[1][1]-planes[0][1]*planes[1][0]};
            float len=dot(crease,crease);
            if(len<.000001f){memset(vel,0,12);return 1;}
            float along=dot(original,crease)/len;
            for(int k=0;k<3;++k)vel[k]=crease[k]*along;
            for(int j=2;j<count;++j)if(dot(vel,planes[j])<-.001f){memset(vel,0,12);return 1;}
        }
    }
    return 1;
}
/* The nearest clear capsule position for an actor that is embedded at
 * `start`: straight up by up to the step height first, then sideways.
 *
 * MEASURED 2026-09-21 (docs/ZOMBIE_WINDOW_20260921.md part 2): every
 * zm_prototype_sh yard wedge (1400 -7xx 8), (1414 -1098 8), (844 -843 8) sat
 * on the same shape -- a raised slab whose top has a 1.4-2 unit groove right
 * behind its lip.  The capsule settles into the groove, its footprint
 * overlaps the lip, and from then on every sweep starts solid.  A lift of up
 * to the step height clears it.
 *
 * MEASURED 2026-10-05, zm_asylum_sh: zombies on the outside stair runs ended
 * moves embedded in the curved yellow wire pipes (material
 * codo_wire_pipe_yellow at (-72 -177 58), (-75 -131 60), (-204 218 61)), the
 * pipe above the step height, so no lift cleared it; every move was refused
 * for two seconds until the stuck detector placed the zombie on a node 25
 * units away (5-9 times per soak).  Sideways, the nearest clear point within
 * 16 units: a capsule (30 wide) that far into a face is still on its own
 * side of it, and the smallest radius is tried first.  Only reached from an
 * embedded start; a normal move never searches. */
static int clearSpot(gentity_s *e,const float *start,float step,float *out) {
    static const float dirs[8][2]={{1,0},{.7071f,.7071f},{0,1},{-.7071f,.7071f},
        {-1,0},{-.7071f,-.7071f},{0,-1},{.7071f,-.7071f}};
    memcpy(out,start,12);
    for(float lift=2.0f;lift<=step;lift+=2.0f) {
        out[2]=start[2]+lift;
        trace_t probe;sweep(e,&probe,out,out);
        if(!probe.startsolid && !probe.allsolid)return 1;
    }
    for(float radius=2.0f;radius<=16.0f;radius+=2.0f)
        for(int k=0;k<8;++k) {
            out[0]=start[0]+dirs[k][0]*radius;
            out[1]=start[1]+dirs[k][1]*radius;
            out[2]=start[2]+1.0f;
            trace_t probe;sweep(e,&probe,out,out);
            if(!probe.startsolid && !probe.allsolid)return 1;
        }
    memcpy(out,start,12);
    return 0;
}
static int physicsMove(gentity_s *e,actor_physics_t *state,
    const float *wish,float dt,float gravity,float step) {
    if(!e || !e->r.inuse || !e->clipmask || !state || !wish || !isfinite(dt) ||
       dt<=0 || dt>.25f || !isfinite(gravity) || gravity<0 || !isfinite(step) || step<0)return 0;
    for(int i=0;i<3;++i)if(!isfinite(wish[i]))return 0;
    float start[3],pos[3],vel[3],down[3];memcpy(start,e->r.currentOrigin,12);memcpy(pos,start,12);
    memcpy(down,start,12);down[2]-=.25f;
    trace_t ground;sweep(e,&ground,start,down);
    float groundFraction, groundNormalZ;
    int stepTried=0, stepUpClear=0, stepSlideClear=0, stepLandingWalkable=0, stepChosen=0;
    float stepUpFraction=-1.0f, stepLandingFraction=-1.0f, stepLandingNormalZ=0.0f;
    float stepRaisedXY=-1.0f, stepBaseXY=-1.0f;
    state->blocked=0;
    int unstuck=0;
    if(ground.startsolid || ground.allsolid) {
        /* Embedded: move from the nearest clear spot (clearSpot); an actor
         * clear nowhere near is genuinely stuck and the move is refused. */
        float lifted[3];
        unstuck=clearSpot(e,start,step,lifted);
        if(!unstuck)return 0;
        memcpy(start,lifted,12);memcpy(pos,start,12);++state->unstuck;
        memcpy(down,start,12);down[2]-=.25f;sweep(e,&ground,start,down);
    }
    groundFraction=ground.fraction;groundNormalZ=ground.normal[2];
    /* An actor lifted out of the floor was standing on it. */
    int grounded=unstuck || (!ground.startsolid && !ground.allsolid && ground.fraction<1 && ground.normal[2]>=.7f);
    /* Step logic also for an actor whose ground trace hit a lip or a wall
     * (normal below .7) but has floor within a step below it. */
    int nearGround=grounded;
    if(!nearGround && step>0) {
        float below[3];memcpy(below,start,12);below[2]-=step;
        trace_t t;sweep(e,&t,start,below);
        nearGround=!t.startsolid && !t.allsolid && t.fraction<1;
    }
    vel[0]=wish[0]/dt;vel[1]=wish[1]/dt;
    vel[2]=grounded?fmaxf(0,wish[2]/dt):state->velocity[2]+wish[2]/dt;
    float finalZ=vel[2]-gravity*dt;
    if(!grounded)vel[2]=(vel[2]+finalZ)*.5f;
    float startVel[3];memcpy(startVel,vel,12);
    if(!slide(e,pos,vel,dt)) {
        /* The ground probe was clear but the move's own sweep started solid:
         * the same embedding, seen by a sweep in another direction (the
         * asylum pipes report clear straight down).  Step clear and retry
         * once rather than refusing every move until the stuck detector. */
        float clear[3];
        if(unstuck || !clearSpot(e,start,step,clear))return 0;
        memcpy(start,clear,12);memcpy(pos,start,12);++state->unstuck;
        unstuck=1;grounded=nearGround=1;
        memcpy(vel,startVel,12);
        if(vel[2]<0)vel[2]=0;
        memcpy(startVel,vel,12);
        if(!slide(e,pos,vel,dt))return 0;
    }
    if(nearGround && step>0) {
        stepTried=1;
        float up[3];memcpy(up,start,12);up[2]+=step;
        trace_t t;sweep(e,&t,start,up);
        stepUpFraction=t.fraction;
        if(!t.startsolid && !t.allsolid && t.fraction>0) {
            stepUpClear=1;
            float raised[3],raisedVel[3];memcpy(raised,start,12);raised[2]+=step*t.fraction;
            memcpy(raisedVel,startVel,12);
            if(slide(e,raised,raisedVel,dt)) {
                stepSlideClear=1;
                /* Include a small landing margin below the original floor.
                 * On level ground the raised sweep ends exactly step units
                 * above the floor.  A step-unit drop only touches it at
                 * fraction 1, which the ground test below treats as no hit.
                 * The ordinary slide then stays pinned to the floor's
                 * fraction-0 contact even though the raised path is clear. */
                const float landingDrop=step+0.25f;
                memcpy(down,raised,12);down[2]-=landingDrop;
                sweep(e,&t,raised,down);
                stepLandingFraction=t.fraction;stepLandingNormalZ=t.normal[2];
                if(!t.startsolid && !t.allsolid && t.fraction<1 && t.normal[2]>=.7f) {
                    stepLandingWalkable=1;
                    raised[2]-=landingDrop*t.fraction;
                    float a=(raised[0]-start[0])*(raised[0]-start[0])+(raised[1]-start[1])*(raised[1]-start[1]);
                    float b=(pos[0]-start[0])*(pos[0]-start[0])+(pos[1]-start[1])*(pos[1]-start[1]);
                    stepRaisedXY=sqrtf(a);stepBaseXY=sqrtf(b);
                    if(a>b){memcpy(pos,raised,12);memcpy(vel,raisedVel,12);stepChosen=1;}
                }
            }
        }
    }
    int fallbackLanded=0;
    float wishedXY=wish[0]*wish[0]+wish[1]*wish[1];
    float movedXY=(pos[0]-start[0])*(pos[0]-start[0])+
        (pos[1]-start[1])*(pos[1]-start[1]);
    int steepBlocked=stepTried && stepUpClear && stepSlideClear &&
        stepLandingFraction<1.0f && stepLandingNormalZ<0.7f &&
        wishedXY>0.0025f && movedXY<0.25f*wishedXY;
    if(e->s.number>=0 && e->s.number<MAX_GENTITIES) {
        int entnum=e->s.number;
        if(steepBlocked) {
            if(stepBlockedFrames[entnum]<3) ++stepBlockedFrames[entnum];
        } else stepBlockedFrames[entnum]=0;
        if(steepBlocked && stepBlockedFrames[entnum]>=3 && step<=18.0f &&
            (!stepFallbackTime[entnum] || level.time<stepFallbackTime[entnum] ||
             level.time-stepFallbackTime[entnum]>=1000)) {
            stepFallbackTime[entnum]=level.time; /* also rate-limit failed probes */
            float wishLength=sqrtf(wishedXY);
            float lift=step*stepUpFraction;
            float raisedStart[3]={start[0],start[1],start[2]+lift};
            for(float reach=2.0f;reach<=16.0f;reach+=2.0f) {
                if(reach<=wishLength) continue;
                float raisedEnd[3]={
                    raisedStart[0]+wish[0]/wishLength*reach,
                    raisedStart[1]+wish[1]/wishLength*reach,
                    raisedStart[2]
                };
                trace_t forward,land,clear;
                sweep(e,&forward,raisedStart,raisedEnd);
                /* Farther samples cannot be clear if the elevated capsule
                 * already hit a solid wall on the same straight segment. */
                if(forward.startsolid || forward.allsolid || forward.fraction<1.0f) break;
                float landEnd[3]={raisedEnd[0],raisedEnd[1],raisedEnd[2]-lift-0.25f};
                sweep(e,&land,raisedEnd,landEnd);
                if(land.startsolid || land.allsolid || land.fraction>=1.0f ||
                    land.normal[2]<0.7f) continue;
                float candidate[3]={raisedEnd[0],raisedEnd[1],
                    raisedEnd[2]-(lift+0.25f)*land.fraction};
                sweep(e,&clear,candidate,candidate);
                if(clear.startsolid || clear.allsolid) continue;
                memcpy(pos,candidate,sizeof(candidate));
                vel[0]=startVel[0];vel[1]=startVel[1];vel[2]=0;
                fallbackLanded=1;
                stepBlockedFrames[entnum]=0;
                ((void)0);
                break;
            }
        }
    }
    /* An actor that started on the ground stays on it: walking over a stair
     * nosing, down a flight or across a slope crest, the floor drops away
     * by up to a step in one frame, and with only the 0.25 ground probe the
     * actor left the floor and fell under gravity, then stepped up again on
     * the next riser -- a hop on every stair.  Q3/T5 StepSlideMove step the
     * mover back down by the step height; do the same, onto walkable floor
     * only (a ledge higher than a step is still a fall). */
    if(grounded && !fallbackLanded && step>0) {
        float snapEnd[3]={pos[0],pos[1],pos[2]-step-.25f};
        trace_t snap;sweep(e,&snap,pos,snapEnd);
        if(!snap.startsolid && !snap.allsolid && snap.fraction>0 && snap.fraction<1 && snap.normal[2]>=.7f)
            pos[2]+=(snapEnd[2]-pos[2])*snap.fraction;
    }
    /* A grounded actor never leaves the floor upward: the slide clips the
     * horizontal walk along a slope or a riser bevel into an upward velocity,
     * and stored as state->velocity it was carried into the next airborne
     * frame (a slope crest, a stair top) as a launch.  Off the ground the
     * same clip against a face too steep to stand on (normal z under .7)
     * turned a running dog's horizontal wish into climbing speed that grew
     * frame on frame (MEASURED 2026-10-05, mp_rust dogs: vertical velocity
     * up to 546 u/s, rises of 15 units in one frame); the actor keeps the
     * height it slid up this frame but no upward speed the slide made. */
    if(grounded && vel[2]>0)vel[2]=0;
    else if(vel[2]>0 && vel[2]>startVel[2])vel[2]=fmaxf(startVel[2],0);
    memcpy(down,pos,12);down[2]-=.25f;sweep(e,&ground,pos,down);
    state->grounded=fallbackLanded || (!ground.startsolid && !ground.allsolid &&
        ground.fraction<1 && ground.normal[2]>=.7f);
    if(fallbackLanded || (state->grounded && vel[2]<=0))vel[2]=0;
    else if(!grounded && vel[2]<0)vel[2]=finalZ;
    memcpy(state->velocity,vel,12);
    {
        float wl=wish[0]*wish[0]+wish[1]*wish[1];
        float gx=pos[0]-start[0],gy=pos[1]-start[1];
        state->blocked=wl>0.0025f && gx*gx+gy*gy<0.25f*wl;
    }
    /* One actual step-branch verdict per actor per five seconds while it is
     * throttled.  The 16-unit wedge probes are only geometric leads; root
     * motion requests much shorter per-frame moves, so this records whether
     * the real up/slide/landing candidate existed and why it was selected. */
    if(state->blocked && step>0 && e->s.number>=0 && e->s.number<MAX_GENTITIES) {
        static int lastDiag[MAX_GENTITIES];
        int *last=&lastDiag[e->s.number];
        if(!*last || level.time<*last || level.time-*last>=5000) {
            *last=level.time;
            ((void)0);
        }
    }
    G_SetOrigin(e,pos);SV_LinkEntity(e);
    return 1;
}
int ActorPhysicsMove(gentity_s *e,actor_physics_t *state,
    const float *wish,float dt,float gravity,float step) {
    ActorsPassable(e,1);
    int moved=physicsMove(e,state,wish,dt,gravity,step);
    ActorsPassable(e,0);
    return moved;
}
static int physicsMoveMode(gentity_s *e,actor_physics_t *state,
    const float *wish,float dt,float gravity,float step,int mode);
int ActorPhysicsMoveMode(gentity_s *e,actor_physics_t *state,
    const float *wish,float dt,float gravity,float step,int mode)
{
    ActorsPassable(e,1);
    int moved=physicsMoveMode(e,state,wish,dt,gravity,step,mode);
    ActorsPassable(e,0);
    return moved;
}
static int physicsMoveMode(gentity_s *e,actor_physics_t *state,
    const float *wish,float dt,float gravity,float step,int mode)
{
    if(!e || !e->r.inuse || !state || !wish || !isfinite(dt) || dt<=0 || dt>.25f)return 0;
    for(int i=0;i<3;++i)if(!isfinite(wish[i]))return 0;
    if(mode==0)return physicsMove(e,state,wish,dt,gravity,step);
    float pos[3],vel[3];memcpy(pos,e->r.currentOrigin,12);
    state->blocked=0;
    for(int i=0;i<3;++i)vel[i]=wish[i]/dt;
    if(mode==1) { // T5 AIPHYS_NOCLIP consumes authored animation displacement.
        for(int i=0;i<3;++i)pos[i]+=wish[i];
    } else if(mode==2) { // T5 AIPHYS_NOGRAVITY: capsule slide, no gravity/step.
        if(!e->clipmask || !slide(e,pos,vel,dt))return 0;
    } else if(mode==3) {
        // T5 AIPhys_ZOnlyPhysicsMove resets incoming Z velocity, then calls
        // StepSlideMove(gravity=1,zonly=1). It still collides horizontally;
        // zonly suppresses pre-slide ground clipping, not wall collision.
        float previousZ=state->velocity[2];
        state->velocity[2]=0;
        int moved=physicsMove(e,state,wish,dt,gravity,step);
        if(!moved)state->velocity[2]=previousZ;
        return moved;
    } else return 0;
    memcpy(state->velocity,vel,12);
    if(mode!=3)state->grounded=0;
    G_SetOrigin(e,pos);SV_LinkEntity(e);
    return 1;
}

#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailActorStepEmpty(void){return resident_tail_zero(stepBlockedFrames,sizeof(stepBlockedFrames))&&resident_tail_zero(stepFallbackTime,sizeof(stepFallbackTime));}
void PersistentTailActorStepReset(void){memset(stepBlockedFrames,0,sizeof(stepBlockedFrames));memset(stepFallbackTime,0,sizeof(stepFallbackTime));}
#endif

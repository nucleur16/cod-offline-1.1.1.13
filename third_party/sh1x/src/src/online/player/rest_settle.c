#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* A resting player takes the spot C1's own prediction settles on.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * THE SYMPTOM
 * -----------
 * "A very slight jitter while standing still; it goes away after moving a
 * little, and comes back every time the player moves around a good bit and
 * stops."  The view of a player standing on uneven ground shakes by a few
 * tenths of a unit, several times a second, for as long as he stands there.
 *
 * WHAT THE CAPTURES SAY
 * ---------------------
 * C1 reports, in every usermove packet, the origin its own prediction
 * produced for its newest command (sv_client.c, predictedOrigin).  The rov
 * gate (ReadOriginVelocityFlag, src/sv_snapshot.c) republishes origin,
 * velocity, bobCycle and the sway phase with a zero delta base whenever that
 * report is more than 0.1 units from the server's origin for the same command
 * time -- C1's own MSG_WriteDeltaPlayerstate rule (0x5a619b..0x5a6238).  Each
 * such snapshot rebases the client's prediction on the server's origin; if
 * the two differ, C1 draws the difference as a prediction-error correction.
 *
 * In the stored live captures every stationary frame with an error is the
 * same event.  bin/home/main/qconsole.puddle.log (mp_storm_spring, a real C1,
 * player c12), from the stop at command time 98146 to 112703:
 *   - the server origin does not move at all (`srv 0.0000` on every row) and
 *     its velocity is exactly zero (`spd 0.00<-0.00`, `dir 0x0`);
 *   - C1 reports an origin 0.32-0.41 units away (0.8-0.9 on some rows); on
 *     the 341 tilted idle rows its offset points DOWNHILL of the server (cos
 *     to the ground normal's horizontal part: median 0.95, above 0.9 on 98.5%
 *     of them), and it is 0.02-0.04 units higher;
 *   - the gate forces 44-52 of every 60 snapshots (`CoDOL rov: ... toofar 51`)
 *     for the whole fifteen seconds, and the error never converges: each
 *     forced snapshot puts C1 back on the server's spot and its next
 *     prediction leaves it again.
 * On flat ground the same stationary rows are essentially exact (stepinstr,
 * terrprobe, walk: 0-1 rows over 0.1 each).  Replaying the bounds below over
 * the puddle capture settles that stop 278 ms after it began instead of
 * forcing all 335 of its reports.  So on a slope C1's movement code does not rest where the
 * server rests: this server runs CoD4's collision against CoDOL terrain
 * (commit 9ca97ffe; docs/UNEVEN_GROUND_FIX_SWEEP.md), and the hull's
 * contact with a tilted mesh is not C1's.  Moving a little "fixes" it only
 * because the player then stops somewhere the two happen to agree.
 *
 * THE FIX
 * -------
 * The server cannot make C1 rest on the server's spot, but it can rest on
 * C1's.  When a player is completely at rest on both sides of a packet
 * (velocity zero before and after, no movement input, on the ground, normal
 * movement mode, not linked, not mantling/on a ladder/frozen), and C1 has
 * reported the same nearby rest spot twice in a row for the exact command
 * time the server has just run, the server takes that spot: the hull must
 * sweep there clear of solid and stand on walkable ground within the pmove's
 * 0.25-unit ground reach, so the server rests there too.  From then on the
 * two origins agree, the gate stops forcing, and nothing moves.
 *
 * Bounds, so the client cannot steer a resting player with it: at most 1 unit
 * horizontally and 0.5 vertically per adoption, two consistent reports, and
 * at most 2 units in total until the player moves by his own input again.
 * Anything outside that keeps the old behaviour (the gate forces the
 * server's origin).
 *
 *   settleRestingPrediction (default 1): 0 restores the old behaviour,
 *   for bisecting.  Grep "CoDOL rest settle:" with ol_debugTrace 1.
 */
#include "q_shared.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "cvar.h"
#include "g_shared.h"
#include "entity.h"
#include "rest_settle.h"
#include <math.h>
#include <string.h>
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

extern qboolean PlayerIsLinked(int clientNum);

#define SETTLE_PM_NORMAL        0
#define SETTLE_PMF_MANTLE       0x00000004u
#define SETTLE_PMF_LADDER       0x00000008u
#define SETTLE_PMF_FROZEN       0x00000800u
#define SETTLE_PMF_MELEE_CHARGE 0x00010000u
#define SETTLE_PMF_REMOTE       0x00080000u
#define SETTLE_PMF_DIVE         0x40000000u
#define SETTLE_PMF_BLOCKING     (SETTLE_PMF_MANTLE|SETTLE_PMF_LADDER|SETTLE_PMF_FROZEN| \
                                 SETTLE_PMF_MELEE_CHARGE|SETTLE_PMF_REMOTE|SETTLE_PMF_DIVE)
#define SETTLE_DEFAULT_MASK     0x02810011
/* C1's own suppression tolerance (0.1 units, squared). */
#define SETTLE_TOLERANCE2       0.01f
#define SETTLE_MAX_XY           1.0f
#define SETTLE_MAX_Z            0.5f
#define SETTLE_CONSISTENT       0.05f
#define SETTLE_BUDGET           2.0f
/* PM_GroundTrace's own reach: the spot must keep the server walking. */
#define SETTLE_GROUND_PROBE     0.25f

typedef struct {
    float last_pred[3];     /* previous at-rest report */
    float last_srv[3];      /* server origin when it was made */
    int last_time;
    int have_last;
    float used;             /* distance adopted since the player last moved */
} settle_state_t;
static settle_state_t settle[MAX_CLIENTS];
static cvar_t *settleRestingPrediction;

void RegisterRestSettle(void)
{
    if(settleRestingPrediction) return;
    settleRestingPrediction=Cvar_RegisterBool("settleRestingPrediction",qtrue,0,
        "A player at rest takes the nearby rest spot C1's prediction reports, so a "
        "slope does not make the view shake while standing still (0: always force "
        "the server's origin)");
}

void RestSettleResetClient(int clientNum)
{
    if(clientNum>=0 && clientNum<MAX_CLIENTS) memset(&settle[clientNum],0,sizeof(settle[clientNum]));
}

static int vec_finite(const float *v)
{
    return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

static int at_rest(const playerState_t *ps)
{
    return ps->velocity[0]==0.0f && ps->velocity[1]==0.0f && ps->velocity[2]==0.0f;
}

static const char *ineligible(const gentity_t *ent,const playerState_t *ps,int clientNum)
{
    if(!ent->r.inuse || !ent->client) return "no player";
    if(ent->client->sess.sessionState!=SESS_STATE_PLAYING) return "not playing";
    if(ent->health<=0) return "dead";
    if(ps->pm_type!=SETTLE_PM_NORMAL) return "pm_type";
    if(ps->groundEntityNum==ENTITYNUM_NONE) return "airborne";
    if((unsigned)ps->pm_flags&SETTLE_PMF_BLOCKING) return "pm_flags";
    if(ent->tagInfo || PlayerIsLinked(clientNum)) return "linked";
    return NULL;
}

int RestSettleAfterUsercmds(int clientNum, const float *predicted,
                                  int predictedTime, int input_idle,
                                  int was_still)
{
    settle_state_t *st;
    gentity_t *ent;
    playerState_t *ps;
    trace_t tr;
    float d[3],dxy,dist,below[3];
    int mask;
    const char *why;

    RegisterRestSettle();
    if(clientNum<0 || clientNum>=MAX_CLIENTS || !predicted) return 0;
    st=&settle[clientNum];
    if(!settleRestingPrediction->boolean) { st->have_last=0; return 0; }
    ent=&g_entities[clientNum];
    if(!ent->client) { st->have_last=0; return 0; }
    ps=&ent->client->ps;
    /* Moving by his own input (or being moved) ends the rest period. */
    if(!input_idle || !was_still || !at_rest(ps)) {
        st->have_last=0; st->used=0.0f; return 0;
    }
    if((why=ineligible(ent,ps,clientNum))!=NULL) { st->have_last=0; return 0; }
    /* The report must belong to the command the server has just run. */
    if(predictedTime<=0 || predictedTime!=ps->commandTime || !vec_finite(predicted)) {
        st->have_last=0; return 0;
    }
    d[0]=predicted[0]-ps->origin[0];
    d[1]=predicted[1]-ps->origin[1];
    d[2]=predicted[2]-ps->origin[2];
    dist=d[0]*d[0]+d[1]*d[1]+d[2]*d[2];
    if(dist<=SETTLE_TOLERANCE2) { st->have_last=0; return 0; }
    dxy=sqrtf(d[0]*d[0]+d[1]*d[1]);
    dist=sqrtf(dist);
    if(dxy>SETTLE_MAX_XY || fabsf(d[2])>SETTLE_MAX_Z) { st->have_last=0; return 0; }
    /* Two consecutive reports of the same rest spot, with the server still
     * where it was for the first: C1 is resting there, not passing through. */
    if(!st->have_last || predictedTime==st->last_time ||
       fabsf(predicted[0]-st->last_pred[0])>SETTLE_CONSISTENT ||
       fabsf(predicted[1]-st->last_pred[1])>SETTLE_CONSISTENT ||
       fabsf(predicted[2]-st->last_pred[2])>SETTLE_CONSISTENT ||
       ps->origin[0]!=st->last_srv[0] || ps->origin[1]!=st->last_srv[1] ||
       ps->origin[2]!=st->last_srv[2]) {
        memcpy(st->last_pred,predicted,sizeof(st->last_pred));
        memcpy(st->last_srv,ps->origin,sizeof(st->last_srv));
        st->last_time=predictedTime;
        st->have_last=1;
        return 0;
    }
    if(st->used+dist>SETTLE_BUDGET) return 0;
    mask=ent->clipmask ? ent->clipmask : SETTLE_DEFAULT_MASK;
    /* The hull must get there through clear space ... */
    G_TraceCapsule(&tr,ps->origin,ent->r.mins,ent->r.maxs,predicted,clientNum,mask);
    if(tr.startsolid || tr.allsolid || tr.fraction<1.0f) {
        if(OL_TRACE_ON())
            ((void)0);
        st->have_last=0;
        return 0;
    }
    /* ... and stand on something there. */
    below[0]=predicted[0]; below[1]=predicted[1]; below[2]=predicted[2]-SETTLE_GROUND_PROBE;
    G_TraceCapsule(&tr,predicted,ent->r.mins,ent->r.maxs,below,clientNum,mask);
    if(tr.startsolid || tr.allsolid || tr.fraction>=1.0f || !tr.walkable) {
        if(OL_TRACE_ON())
            ((void)0);
        st->have_last=0;
        return 0;
    }
    if(OL_TRACE_ON())
        ((void)0);
    memcpy(ps->origin,predicted,sizeof(ps->origin));
    memcpy(ent->r.currentOrigin,predicted,sizeof(ent->r.currentOrigin));
    memcpy(ent->s.lerp.pos.trBase,predicted,sizeof(ent->s.lerp.pos.trBase));
    SV_LinkEntity(ent);
    st->used+=dist;
    st->have_last=0;
    return 1;
}

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailRestEmpty(void){return resident_tail_zero(settle,sizeof(settle));}
void PersistentTailRestReset(void){for(unsigned i=0;i<MAX_CLIENTS;++i)RestSettleResetClient(i);}
#endif

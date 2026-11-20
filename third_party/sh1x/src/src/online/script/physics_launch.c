#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Server rigid-body launch for script models (IW4 PhysicsLaunchServer).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C1's own method (C1.exe 0x569209..0x5694d1, string "physicsLaunchServer
 * valid only for script models" at 0xc5c530) accepts only eType 6, evaluates
 * both trajectories, then sets pos AND apos to trType 1 (TR_INTERPOLATE)
 * with time, duration and delta zero (0x5693bc..0x56944d), creates a server
 * physics body from the model's PhysPreset (0x529e20 -> 0x658800, falling
 * back to TR_GRAVITY only for a model without a preset, 0x529e81) and adds
 * the script's force at the script's point to it (0x659370). The body is
 * simulated on the server and the entity is published every frame as an
 * interpolated pose; C1's client lerps position and angles between two
 * snapshots for type 1 (entity lerp 0x443337/0x443354) and simulates
 * nothing itself. The crate therefore tumbles, bounces and settles exactly
 * where the server says.
 *
 * This server has no ODE world, so the body is a box rigid body of the
 * entity's own bounds: gravity, the launch impulse applied at its point,
 * contacts sampled at the 26 points of the box's 3x3x3 lattice (corners,
 * edge midpoints, face centres) against the world,
 * static models and solid entities (including other crates), sequential
 * impulses with restitution and Coulomb friction, then rest detection and
 * "physics_finished". The pose is published as C1 publishes it. */
#ifdef OL_SERVER
#include "script_combat_builtins.h"
#include "xmodel_native.h"
#include "xassets/phys.h"
#include "scr_vm_functions.h"
#include "g_shared.h"
#include "entity.h"
#include "server.h"
#include "cscr_stringlist.h"
#include "bg_public.h"
#include "q_math.h"
#include "cvar.h"
#include "qcommon_io.h"
#include <math.h>
#include <string.h>

extern void G_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
    const float *dir, const float *point, int damage, int dflags, int mod, int weapon,
    int hitLoc, unsigned int modelIndex, unsigned int partName, int timeOffset);

#define PL_MAX_CHILDREN 8
#define PL_SAMPLES 26               /* the 3x3x3 lattice of the box minus its centre */
#define PL_MAX_CONTACTS PL_SAMPLES
#define PL_GRAVITY 800.0f          /* g_gravity default; C1 BG gravity is 800 */
#define PL_SUBSTEP 0.008f          /* seconds; sv_fps 60 gives two per frame */
#define PL_MAX_FRAME 0.1f          /* clamp a hitch so the body cannot jump */
#define PL_ITERATIONS 8
#define PL_SLOP 0.25f              /* allowed penetration, units */
#define PL_BOUNCE_MIN_SPEED 60.0f  /* below this an impact does not bounce */
/* rad/s. The airdrop's lever arm runs from the world origin, thousands of
 * units, so its few units of force would spin a crate several turns a
 * second; bounded, it turns a few tens of degrees on the way down and
 * lands on its base or tips over an edge. */
#define PL_LAUNCH_MAX_SPIN 0.3f
#define PL_MAX_SPIN 10.0f          /* rad/s, numerical safety */
/* Friction may change a contact's velocity by at most this much per
 * substep. Resting and sliding loads are far below it; the impact of a crate
 * landing at 1500 u/s is not, and unbounded friction there turned the
 * fall into a 400 u/s sideways tumble (MEASURED on mp_rust) instead of the
 * thud and settle expected of a heavy crate. */
#define PL_FRICTION_IMPULSE_SPEED 150.0f
#define PL_MAX_SPEED 4000.0f
#define PL_REST_SPEED 4.0f
#define PL_REST_SPIN 0.12f
#define PL_REST_MS 300
#define PL_TIMEOUT_MS 30000
#define PL_ANCHOR_DIST 3.0f
#define PL_ANCHOR_COS 0.9986f    /* 3 degrees */
#define PL_ANCHOR_MS 1000
#define PL_CRUSH_SPEED 150.0f
#define PL_IMPACT_SPEED 200.0f
#define PL_IMPACT_SPIN 3.0f
/* Item mask the retained G_RunItem used: solid, glass, missile clip and
 * item clip. The crate's cloned collision carries missile and item clip
 * (MEASURED contents 0x80336c0), so crates land on each other; its player,
 * vehicle and monster clip bits are NOT added, as those also name invisible
 * world clip brushes. Players are not obstacles: a falling crate crushes
 * them. */
#define PL_MASK 0x491
#define PL_CONTENTS_BODY 0x2000000
#define PL_MOD_CRUSH 9             /* retained modNames index (g_combat_mp.cpp) */

#pragma pack(push, 8)
typedef struct {
    float r[3];      /* sample offset from the centre of mass, world axes */
    float n[3];
    float depth;
    float jn;        /* accumulated normal impulse */
    float jt[3];     /* accumulated friction impulse */
    float bias;      /* restitution target normal speed */
} pl_contact_t;

typedef struct {
    int active;
    int useCount;
    int launchTime, lastTime, restStartTime;
    int anchorTime, anchorTouched;   /* rocking-in-place rest detection */
    float anchorCom[3], anchorUp[3];
    int carrier, carrierUseCount;   /* entity the body starts inside, ignored */
    int mask;
    float com[3], vel[3], avel[3];  /* world space */
    float axis[3][3];               /* forward, left, up (AnglesToAxis rows) */
    float center[3], half[3];       /* box in model space */
    float invMass, invInertia[3];   /* principal, model space */
    float bounce, friction;
    float published[3];             /* last published origin */
    int childCount;
    unsigned short childNumber[PL_MAX_CHILDREN];
    int childUseCount[PL_MAX_CHILDREN];
    int childContents[PL_MAX_CHILDREN];
} physics_launch_t;
#pragma pack(pop)

#include "sys_ondemand.h"
SH1X_ONDEMAND(, physics_launch_t, physics_launches, [MAX_GENTITIES]); /* sys_ondemand.h: allocated when the server starts */
#define physics_launches (*sh1x_od_physics_launches)

void ResetPhysicsLaunches(void)
{
    memset(physics_launches,0,sizeof(physics_launches));
}

/* G_RunFrameForEntity asks this before its item and mover paths. */
int PhysicsLaunchOwnsEntity(const gentity_t *ent)
{
    const physics_launch_t *b;
    if(!ent || ent->s.number<0 || ent->s.number>=MAX_GENTITIES) return 0;
    b=&physics_launches[ent->s.number];
    return b->active && b->useCount==ent->useCount;
}

/* ---- small vector helpers ------------------------------------------- */

static void to_world(const physics_launch_t *b, const float local[3], float out[3])
{
    unsigned i;
    for(i=0;i<3;++i) out[i]=local[0]*b->axis[0][i]+local[1]*b->axis[1][i]+local[2]*b->axis[2][i];
}
/* I^-1 x in world axes: rotate into the body, scale, rotate back. */
static void inv_inertia(const physics_launch_t *b, const float x[3], float out[3])
{
    float l[3]; unsigned i;
    for(i=0;i<3;++i) l[i]=DotProduct(x,b->axis[i])*b->invInertia[i];
    to_world(b,l,out);
}
static float effective_mass(const physics_launch_t *b, const float r[3], const float d[3])
{
    float rd[3],t[3],u[3];
    CrossProduct(r,d,rd); inv_inertia(b,rd,t); CrossProduct(t,r,u);
    return b->invMass+DotProduct(d,u);
}
static void apply_impulse(physics_launch_t *b, const float r[3], const float j[3])
{
    float rj[3],dw[3];
    VectorMA(b->vel,b->invMass,j,b->vel);
    CrossProduct(r,j,rj); inv_inertia(b,rj,dw);
    VectorAdd(b->avel,dw,b->avel);
}
static void point_velocity(const physics_launch_t *b, const float r[3], float out[3])
{
    float wr[3];
    CrossProduct(b->avel,r,wr); VectorAdd(b->vel,wr,out);
}
static void rotate_axes(physics_launch_t *b, const float w[3], float dt)
{
    float angle=VectorLength(w)*dt,k[3],c,s;
    unsigned i;
    if(angle<1e-6f) return;
    VectorScale(w,dt/angle,k);
    c=cosf(angle); s=sinf(angle);
    for(i=0;i<3;++i) {
        float *v=b->axis[i],kv[3],kd=DotProduct(k,v),out[3];
        unsigned j;
        CrossProduct(k,v,kv);
        for(j=0;j<3;++j) out[j]=v[j]*c+kv[j]*s+k[j]*kd*(1.0f-c);
        VectorCopy(out,v);
    }
    /* Gram-Schmidt keeps the frame orthonormal and right handed. */
    Vec3Normalize(b->axis[0]);
    {
        float d=DotProduct(b->axis[0],b->axis[1]);
        VectorMA(b->axis[1],-d,b->axis[0],b->axis[1]);
        Vec3Normalize(b->axis[1]);
        CrossProduct(b->axis[0],b->axis[1],b->axis[2]);
    }
}
static void clamp_length(float v[3], float limit)
{
    float l=VectorLength(v);
    if(l>limit) VectorScale(v,limit/l,v);
}

/* ---- traces ----------------------------------------------------------- */

static void pl_trace(const physics_launch_t *b, int self, trace_t *tr, const float start[3], const float end[3])
{
    static const float zero[3]={0,0,0};
    IgnoreEntParams ignore;
    ignore.baseEntity=self;
    ignore.parentEntity=b->carrier;
    ignore.ignoreSelf=1;
    ignore.ignoreParent=b->carrier>=0;
    ignore.ignoreSiblings=0;
    ignore.ignoreChildren=1;
    /* Point traces honour ignoreParent (SV_PointTraceToEntity), which keeps
     * the carrier out without borrowing the entity's owner handle; static
     * models are included so a crate lands on a parked car or a crate prop. */
    SV_Trace(tr,start,zero,zero,end,&ignore,b->mask,0,NULL,1);
}

static int carrier_alive(physics_launch_t *b)
{
    if(b->carrier<0) return 0;
    if(!g_entities[b->carrier].r.inuse || g_entities[b->carrier].useCount!=b->carrierUseCount) {
        b->carrier=-1; return 0;
    }
    return 1;
}

/* ---- riders ------------------------------------------------------------ */

/* What rides on the object: the crate's team-coloured
 * com_plasticcase_friendly/_enemy models are linkTo'd to it
 * (_airdrop.gsc deleteOnOwnerDeath) and share its bounds exactly. Traces do
 * not skip an entity's tag children, so a solid rider would be a contact
 * inside the body itself. Clear the contents of each rider while the object
 * flies and give them back when it lands. */
static void clear_children(physics_launch_t *b, gentity_t *ent)
{
    gentity_t *child;
    int i,known;
    for(child=ent->tagChildren;child && child->tagInfo;child=child->tagInfo->next) {
        if(!child->r.contents) continue;
        for(known=0,i=0;i<b->childCount;++i)
            if(b->childNumber[i]==child->s.number && b->childUseCount[i]==child->useCount) known=1;
        if(known) continue;
        if(b->childCount>=PL_MAX_CHILDREN) break;
        b->childNumber[b->childCount]=(unsigned short)child->s.number;
        b->childUseCount[b->childCount]=child->useCount;
        b->childContents[b->childCount]=child->r.contents;
        ++b->childCount;
        child->r.contents=0;
        SV_LinkEntity(child);
    }
}
static void restore_children(physics_launch_t *b)
{
    int i;
    for(i=0;i<b->childCount;++i) {
        gentity_t *child=&g_entities[b->childNumber[i]];
        if(child->r.inuse && child->useCount==b->childUseCount[i] && !child->r.contents) {
            child->r.contents=b->childContents[i];
            SV_LinkEntity(child);
        }
    }
    b->childCount=0;
}

/* ---- publication ---------------------------------------------------------- */

static void body_origin(const physics_launch_t *b, float origin[3])
{
    float c[3];
    to_world(b,b->center,c);
    VectorSubtract(b->com,c,origin);
}

/* C1 0x5693a5..0x56944d: both tracks type 1, time/duration/delta zero, the
 * evaluated pose in trBase. The retained G_RunMover evaluates type 1 as its
 * base, which is also r.currentOrigin, so it moves nothing on its own. */
static void publish(physics_launch_t *b, gentity_t *ent)
{
    float origin[3],angles[3];
    body_origin(b,origin);
    AxisToAngles(b->axis,angles);
    ent->s.lerp.pos.trType=TR_INTERPOLATE;
    ent->s.lerp.pos.trTime=0;
    ent->s.lerp.pos.trDuration=0;
    VectorCopy(origin,ent->s.lerp.pos.trBase);
    VectorClear(ent->s.lerp.pos.trDelta);
    ent->s.lerp.apos.trType=TR_INTERPOLATE;
    ent->s.lerp.apos.trTime=0;
    ent->s.lerp.apos.trDuration=0;
    VectorCopy(angles,ent->s.lerp.apos.trBase);
    VectorClear(ent->s.lerp.apos.trDelta);
    VectorCopy(origin,ent->r.currentOrigin);
    VectorCopy(angles,ent->r.currentAngles);
    VectorCopy(origin,b->published);
    SV_LinkEntity(ent);
}

static unsigned gather_contacts(physics_launch_t *b, int self, pl_contact_t *contacts);

static void notify_finished(gentity_t *ent)
{
    unsigned notify=SL_GetString("physics_finished",0);
    if(!notify) return;
    Scr_Notify(ent,(unsigned short)notify,0);
    SL_RemoveRefToString(notify);
}

static void settle(physics_launch_t *b, gentity_t *ent, int time)
{
    float origin[3],angles[3];
    body_origin(b,origin);
    AxisToAngles(b->axis,angles);
    if(TRACE_VERBOSE()) {
        pl_contact_t contacts[PL_MAX_CONTACTS];
        unsigned count=gather_contacts(b,ent->s.number,contacts),k;
        float deepest=0;
        for(k=0;k<count;++k) if(contacts[k].depth>deepest) deepest=contacts[k].depth;
        ((void)0);
    }
    b->active=0;
    restore_children(b);
    G_SetOrigin(ent,origin);
    G_SetAngle(ent,angles);
    SV_LinkEntity(ent);
    if(TRACE_VERBOSE())
        ((void)0);
    notify_finished(ent);
}

/* ---- simulation ------------------------------------------------------------ */

/* Corners, edge midpoints and face centres: a crate coming down on a
 * railing or a roof ridge between its corners still finds it. */
static signed char sample_sign[PL_SAMPLES][3];
static void init_samples(void)
{
    int x,y,z; unsigned n=0;
    if(sample_sign[0][0]|sample_sign[0][1]|sample_sign[0][2]) return;
    for(x=-1;x<=1;++x) for(y=-1;y<=1;++y) for(z=-1;z<=1;++z) {
        if(!x && !y && !z) continue;
        sample_sign[n][0]=(signed char)x; sample_sign[n][1]=(signed char)y; sample_sign[n][2]=(signed char)z; ++n;
    }
}

/* Contacts: trace from the centre of mass (inside the box, so free space
 * whenever the body is not already embedded) out to each sample point; a
 * blocked trace is a sample that has crossed a surface, its depth measured
 * along the surface normal. */
static unsigned gather_contacts(physics_launch_t *b, int self, pl_contact_t *contacts)
{
    unsigned i,count=0;
    for(i=0;i<PL_SAMPLES;++i) {
        float local[3],r[3],p[3],hit[3],d[3];
        trace_t tr;
        unsigned k;
        for(k=0;k<3;++k) local[k]=(float)sample_sign[i][k]*b->half[k];
        to_world(b,local,r);
        VectorAdd(b->com,r,p);
        pl_trace(b,self,&tr,b->com,p);
        if(tr.startsolid || tr.allsolid || tr.fraction>=1.0f) continue;
        for(k=0;k<3;++k) hit[k]=b->com[k]+tr.fraction*(p[k]-b->com[k]);
        VectorSubtract(hit,p,d);
        VectorCopy(r,contacts[count].r);
        VectorCopy(tr.normal,contacts[count].n);
        contacts[count].depth=DotProduct(d,tr.normal);
        if(contacts[count].depth<0) contacts[count].depth=0;
        contacts[count].jn=0;
        VectorClear(contacts[count].jt);
        ++count;
    }
    return count;
}

static void solve_contacts(physics_launch_t *b, pl_contact_t *c, unsigned count)
{
    unsigned i,it;
    float correction[3]={0,0,0};
    for(i=0;i<count;++i) {
        float u[3],vn;
        point_velocity(b,c[i].r,u);
        vn=DotProduct(u,c[i].n);
        c[i].bias=vn<-PL_BOUNCE_MIN_SPEED ? -b->bounce*vn : 0.0f;
    }
    for(it=0;it<PL_ITERATIONS;++it) {
        for(i=0;i<count;++i) {
            float u[3],vn,k,dj,jn,j[3],ut[3],lt;
            point_velocity(b,c[i].r,u);
            vn=DotProduct(u,c[i].n);
            k=effective_mass(b,c[i].r,c[i].n);
            if(k<=0) continue;
            dj=(c[i].bias-vn)/k;
            jn=c[i].jn+dj; if(jn<0) jn=0;
            dj=jn-c[i].jn; c[i].jn=jn;
            VectorScale(c[i].n,dj,j);
            apply_impulse(b,c[i].r,j);
            /* Coulomb friction, accumulated as a vector in the contact plane
             * and bounded by mu times the normal impulse. */
            point_velocity(b,c[i].r,u);
            vn=DotProduct(u,c[i].n);
            VectorMA(u,-vn,c[i].n,ut);
            lt=VectorLength(ut);
            if(lt>1e-4f) {
                float t[3],kt,jt[3],old[3],len;
                float limit=b->friction*(c[i].jn<PL_FRICTION_IMPULSE_SPEED/b->invMass ? c[i].jn : PL_FRICTION_IMPULSE_SPEED/b->invMass);
                VectorScale(ut,1.0f/lt,t);
                kt=effective_mass(b,c[i].r,t);
                if(kt<=0) continue;
                VectorCopy(c[i].jt,old);
                VectorMA(c[i].jt,-lt/kt,t,jt);
                len=VectorLength(jt);
                if(len>limit) VectorScale(jt,limit/len,jt);
                VectorCopy(jt,c[i].jt);
                VectorSubtract(jt,old,jt);
                apply_impulse(b,c[i].r,jt);
            }
        }
    }
    /* Push the body out of what it sank into, deepest contact first. */
    for(;;) {
        unsigned best=count; float bestDepth=PL_SLOP;
        for(i=0;i<count;++i) {
            float d=c[i].depth-DotProduct(correction,c[i].n);
            if(d>bestDepth) { bestDepth=d; best=i; }
        }
        if(best==count) break;
        VectorMA(correction,bestDepth-PL_SLOP*0.5f,c[best].n,correction);
        c[best].depth=-1e9f;
    }
    VectorAdd(b->com,correction,b->com);
}

/* One substep. Returns nonzero when the body touched something. */
static int step(physics_launch_t *b, int self, float dt)
{
    float oldCom[3],oldAxis[3][3];
    pl_contact_t contacts[PL_MAX_CONTACTS];
    unsigned count;
    trace_t tr;
    int touched=0;

    VectorCopy(b->com,oldCom);
    memcpy(oldAxis,b->axis,sizeof(oldAxis));
    b->vel[2]-=PL_GRAVITY*dt;
    clamp_length(b->vel,PL_MAX_SPEED);
    clamp_length(b->avel,PL_MAX_SPIN);
    VectorMA(b->com,dt,b->vel,b->com);
    rotate_axes(b,b->avel,dt);

    /* The centre itself must not tunnel: a fast fall moves it further in one
     * substep than a thin floor is thick. */
    pl_trace(b,self,&tr,oldCom,b->com);
    if(tr.startsolid || tr.allsolid) {
        /* Embedded: hold the last free pose and stop. */
        VectorCopy(oldCom,b->com);
        memcpy(b->axis,oldAxis,sizeof(oldAxis));
        VectorClear(b->vel);
        VectorScale(b->avel,0.5f,b->avel);
        return 1;
    }
    if(tr.fraction<1.0f) {
        float vn;
        unsigned k;
        for(k=0;k<3;++k) b->com[k]=oldCom[k]+tr.fraction*(b->com[k]-oldCom[k]);
        VectorMA(b->com,0.25f,tr.normal,b->com);
        vn=DotProduct(b->vel,tr.normal);
        if(vn<0) VectorMA(b->vel,-(1.0f+(vn<-PL_BOUNCE_MIN_SPEED ? b->bounce : 0.0f))*vn,tr.normal,b->vel);
        touched=1;
    }

    count=gather_contacts(b,self,contacts);
    if(count) {
        float approach=0;
        unsigned i;
        for(i=0;i<count;++i) {
            float u[3],vn;
            point_velocity(b,contacts[i].r,u);
            vn=-DotProduct(u,contacts[i].n);
            if(vn>approach) approach=vn;
        }
        solve_contacts(b,contacts,count);
        /* A hard landing on one edge or corner: the point-contact impulse
         * of a rigid box arriving at 1500 u/s spins it at 50 rad/s, so it
         * rotated through flat within a substep, slammed its other edge and
         * flipped over (MEASURED, and reproduced offline against a plane).
         * The real crate and IW4's soft-contact solver spread that impact
         * over both edges. Keep the spin it may take from an impact to
         * what rights it onto the face it came down on. */
        if(approach>PL_IMPACT_SPEED) clamp_length(b->avel,PL_IMPACT_SPIN);
        /* Rolling resistance: a box tipping on its edges loses spin to the
         * ground, so it settles instead of rocking. */
        VectorScale(b->avel,1.0f-2.0f*dt,b->avel);
        touched=1;
    } else {
        /* Body damping in the air, the IW physics defaults (linear 0.03,
         * angular 0.5 per second): the tumble slows as the crate falls. */
        VectorScale(b->vel,1.0f-0.03f*dt,b->vel);
        VectorScale(b->avel,1.0f-0.5f*dt,b->avel);
    }
    return touched;
}

/* IW4 MP crush: a falling body that comes down on a living player kills
 * them as a blocked mover does (G_MoverPush -> MOD_CRUSH, inflictor and
 * attacker the mover). _damage.gsc turns it into the care-package kill,
 * including the teammate and owner cases (attackerIsNPC for script_model). */
static void box_extent(const physics_launch_t *b, float extent[3])
{
    unsigned k;
    for(k=0;k<3;++k)
        extent[k]=fabsf(b->axis[0][k])*b->half[0]+fabsf(b->axis[1][k])*b->half[1]+fabsf(b->axis[2][k])*b->half[2];
}

static void crush_players(physics_launch_t *b, gentity_t *ent)
{
    int i;
    static const float down[3]={0,0,-1};
    float extent[3],lo[3],hi[3];
    if(b->vel[2]>-PL_CRUSH_SPEED) return;
    /* The box's own world bounds: r.absmin/absmax of a rotated brush model
     * are a bounding sphere, twenty units taller than the crate. */
    box_extent(b,extent);
    VectorSubtract(b->com,extent,lo);
    VectorAdd(b->com,extent,hi);
    for(i=0;i<level.maxclients;++i) {
        gentity_t *player=&g_entities[i];
        float point[3];
        if(!player->r.inuse || !player->client || player->health<=0 || !player->takedamage ||
           !(player->r.contents & PL_CONTENTS_BODY)) continue;
        if(player->r.absmin[0]>hi[0]-2 || player->r.absmax[0]<lo[0]+2 ||
           player->r.absmin[1]>hi[1]-2 || player->r.absmax[1]<lo[1]+2 ||
           player->r.absmin[2]>hi[2] || player->r.absmax[2]<lo[2]) continue;
        /* Only from above: the body's centre over the player's waist. */
        if(b->com[2]<player->r.currentOrigin[2]+30.0f) continue;
        VectorCopy(player->r.currentOrigin,point);
        point[2]=player->r.absmax[2];
        if(TRACE_VERBOSE())
            ((void)0);
        G_Damage(player,ent,ent,down,point,100000,0,PL_MOD_CRUSH,0,0,0,0,0);
    }
}

void RunPhysicsLaunchFrame(int time)
{
    unsigned i;
    for(i=0;i<MAX_GENTITIES;++i) {
        physics_launch_t *b=&physics_launches[i];
        gentity_t *ent=&g_entities[i];
        float frame,left;
        int touched=0;
        if(!b->active) continue;
        /* Freed/reused, re-linked, or moved by script (moveTo, setOrigin,
         * a new trajectory): the launch is over without a notification, as
         * the entity is no longer the body that was launched. */
        if(!ent->r.inuse || ent->useCount!=b->useCount || ent->tagInfo ||
           ent->s.lerp.pos.trType!=TR_INTERPOLATE ||
           Vec3DistanceSq(ent->r.currentOrigin,b->published)>0.01) {
            if(TRACE_VERBOSE())
                ((void)0);
            if(ent->r.inuse && ent->useCount==b->useCount) restore_children(b);
            else b->childCount=0;
            b->active=0;
            continue;
        }
        frame=(float)(time-b->lastTime)*0.001f;
        b->lastTime=time;
        if(frame<=0) continue;
        if(frame>PL_MAX_FRAME) frame=PL_MAX_FRAME;
        clear_children(b,ent);
        carrier_alive(b);
        for(left=frame;left>1e-5f;) {
            float dt=left>PL_SUBSTEP ? PL_SUBSTEP : left;
            if(left-dt<PL_SUBSTEP*0.25f) dt=left;
            touched|=step(b,(int)i,dt);
            left-=dt;
        }
        publish(b,ent);
        crush_players(b,ent);
        if(TRACE_VERBOSE())
            ((void)0);
        if(touched && VectorLength(b->vel)<PL_REST_SPEED && VectorLength(b->avel)<PL_REST_SPIN) {
            if(!b->restStartTime) b->restStartTime=time;
        } else {
            b->restStartTime=0;
        }
        /* A body rocking on a thin support it keeps sinking into and being
         * pushed back out of never meets the speed test; one that has stayed
         * within a few units and degrees for a second, touching, is at rest. */
        b->anchorTouched|=touched;
        if(Vec3DistanceSq(b->com,b->anchorCom)>PL_ANCHOR_DIST*PL_ANCHOR_DIST ||
           DotProduct(b->axis[2],b->anchorUp)<PL_ANCHOR_COS) {
            VectorCopy(b->com,b->anchorCom);
            VectorCopy(b->axis[2],b->anchorUp);
            b->anchorTime=time;
            b->anchorTouched=touched;
        }
        if((b->restStartTime && time-b->restStartTime>=PL_REST_MS) ||
           (b->anchorTouched && time-b->anchorTime>=PL_ANCHOR_MS) ||
           time-b->launchTime>=PL_TIMEOUT_MS)
            settle(b,ent,time);
    }
}

/* Mass, bounce and friction from the model's PhysPreset, as C1's body takes
 * them (0x529e74 reads XModel+0x160). Defaults are a wooden crate's. */
static void load_preset(physics_launch_t *b, gentity_t *ent, float *mass)
{
    const XModel *model=NULL;
    const struct PhysPreset *preset;
    unsigned name=ent->model ? G_ModelName(ent->model) : 0;
    *mass=10.0f; b->bounce=0.2f; b->friction=0.6f;
    if(!name) return;
    if(LoadEnginePhysicsXModel(SL_ConvertToString(name),&model)!=1 || !model || !model->physPreset) return;
    preset=(const struct PhysPreset *)model->physPreset;
    if(isfinite(preset->mass) && preset->mass>0.01f) *mass=preset->mass;
    if(isfinite(preset->bounce) && preset->bounce>=0) b->bounce=preset->bounce>0.9f ? 0.9f : preset->bounce;
    if(isfinite(preset->friction) && preset->friction>=0) b->friction=preset->friction>2.0f ? 2.0f : preset->friction;
}

/* IW4 <script_model> PhysicsLaunchServer(<contact point>, <force>)
 * (_airdrop.gsc: the crate released from the little bird or C-130, with
 * point (0,0,0) and a force of a few units; af_chase_code.gsc: boat debris).
 * The force is an impulse at the point: the body takes force/mass of linear
 * velocity and (point - centre) x force of angular impulse, so the airdrop's
 * world-origin point gives the crate the slow tumble IW4 crates fall with. */
void Scr_PhysicsLaunchServer(scr_entref_t ref)
{
    gentity_t *ent=VM_GetGEntityForEntRef(ref);
    physics_launch_t *b;
    float point[3],force[3],lever[3],torque[3],dims[3],mass,c[3];
    unsigned number,k;
    if(!ent) return;
    number=(unsigned)ent->s.number;
    if(ent->classname!=scr_const.script_model || ent->s.eType!=ET_SCRIPTMOVER) {
        Scr_ObjectError("physicsLaunchServer valid only for script models");
        return;
    }
    if(Scr_GetNumParam()!=2) {
        Scr_Error("Usage: <script_model> PhysicsLaunchServer( <contact point>, <force> )");
        return;
    }
    if(ent->tagInfo) {
        Scr_ObjectError(va("Entity %i is linked; unlink it before PhysicsLaunchServer",number));
        return;
    }
    Scr_GetVector(0,point);
    Scr_GetVector(1,force);

    init_samples();
    b=&physics_launches[number];
    if(b->active) restore_children(b);
    memset(b,0,sizeof(*b));
    b->useCount=ent->useCount;
    b->launchTime=b->lastTime=level.time;
    b->carrier=-1;

    /* Pose from the evaluated trajectories (C1 0x56938f/0x5693a0). */
    {
        float origin[3],angles[3];
        BG_EvaluateTrajectory(&ent->s.lerp.pos,level.time,origin);
        BG_EvaluateTrajectory(&ent->s.lerp.apos,level.time,angles);
        if(ent->s.lerp.pos.trType==TR_STATIONARY || ent->s.lerp.pos.trType==TR_INTERPOLATE)
            VectorCopy(ent->r.currentOrigin,origin);
        if(ent->s.lerp.apos.trType==TR_STATIONARY || ent->s.lerp.apos.trType==TR_INTERPOLATE)
            VectorCopy(ent->r.currentAngles,angles);
        AnglesToAxis(angles,b->axis);
        for(k=0;k<3;++k) {
            float lo=ent->r.mins[k],hi=ent->r.maxs[k];
            /* A model without collision bounds still needs a body. */
            if(hi-lo<4.0f) { lo=-4.0f; hi=4.0f; }
            b->center[k]=0.5f*(lo+hi);
            b->half[k]=0.5f*(hi-lo);
            dims[k]=hi-lo;
        }
        to_world(b,b->center,c);
        VectorAdd(origin,c,b->com);
    }

    load_preset(b,ent,&mass);
    b->invMass=1.0f/mass;
    /* Solid box inertia, I = m/12 (b^2 + c^2). */
    b->invInertia[0]=12.0f/(mass*(dims[1]*dims[1]+dims[2]*dims[2]));
    b->invInertia[1]=12.0f/(mass*(dims[0]*dims[0]+dims[2]*dims[2]));
    b->invInertia[2]=12.0f/(mass*(dims[0]*dims[0]+dims[1]*dims[1]));
    b->mask=PL_MASK;

    /* Launch impulse. C1 starts the body at rest (trDelta zeroed at
     * 0x5693cf unless the entity keeps its velocity) and adds the force. */
    VectorScale(force,b->invMass,b->vel);
    VectorSubtract(point,b->com,lever);
    CrossProduct(lever,force,torque);
    inv_inertia(b,torque,b->avel);
    clamp_length(b->avel,PL_LAUNCH_MAX_SPIN);
    clamp_length(b->vel,PL_MAX_SPEED);

    /* A released object starts inside the thing that carried it: the
     * airdrop crate is unlinked from the little bird's tag_ground (and the
     * C-130's) and launched in place, inside the vehicle's bounds. IW4's
     * physics body does not collide with its carrier; ignore the entity the
     * box starts inside for the whole flight. World overlap is left alone:
     * that is a genuinely bad release point. */
    clear_children(b,ent);
    {
        trace_t tr;
        float mins[3],maxs[3],extent[3];
        int hit;
        box_extent(b,extent);
        for(k=0;k<3;++k) { mins[k]=-extent[k]; maxs[k]=extent[k]; }
        G_TraceCapsule(&tr,b->com,mins,maxs,b->com,(int)number,b->mask);
        if(tr.startsolid || tr.allsolid) {
            hit=Trace_GetEntityHitId(&tr);
            if(hit>=0 && hit<ENTITYNUM_WORLD && hit!=(int)number && g_entities[hit].r.inuse) {
                b->carrier=hit;
                b->carrierUseCount=g_entities[hit].useCount;
            }
        }
    }
    /* The retained item physics must not also move it. */
    ent->physicsObject=0;
    ent->s.groundEntityNum=ENTITYNUM_NONE;
    b->active=1;
    VectorCopy(b->com,b->anchorCom);
    VectorCopy(b->axis[2],b->anchorUp);
    b->anchorTime=level.time;
    publish(b,ent);
    if(TRACE_VERBOSE())
        ((void)0);
}
#endif

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentPhysicsLaunchesEmpty(void){return resident_entity_zero(physics_launches,sizeof(physics_launches));}
#endif

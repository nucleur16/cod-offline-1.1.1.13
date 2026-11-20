#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Player unstuck: the safety net for a player pinned where the pmove cannot
 * move him.
 *
 * Seen live 2026-10-05 on zm_asylum_sh: nucleur threw every grenade from the
 * same eye position (334.125 509.875 286.379) from 22:36:23 until he left at
 * 22:38:00, through a last stand and a revive.  The spot is the 226 floor
 * beside the Speed Cola electric trap: a 24-unit slot between a 0.5-unit trim
 * (y 516.5, top 226.5) and a 3-unit trim (y 492), narrower than the 30-unit
 * player hull.  The capsule's round bottom sinks into the slot and rests on
 * the trims' chamfered edges at z 226.377 (setpos reproduces it
 * exactly: origin 226.377 + eye 60 = 286.377).  The ground trace reads those
 * edges as a 0.673 slope, so the player has no ground (no walking, no jump),
 * the gravity sweep is blocked, and every horizontal sweep meets a trim 0.12
 * above the hull's bottom that an airborne pmove does not step over.  The
 * pmove's own correct_allsolid (C1 PM_CorrectAllSolid) only acts on an
 * allsolid ground trace and only tries one-unit nudges, so nothing frees him.
 * The same applies to a hull that ends up in solid (a brush made solid again
 * around a player, an entity spawned over him, a teleport into a wall).
 *
 *   unstuck <clientnum> [force]
 *       Server console / rcon (authenticated, so no developer gate).  Prints
 *       what the player's hull overlaps and moves him to the nearest spot
 *       where a standing hull is clear and rests on walkable floor: a small
 *       offset first, then the nearest waypoint-graph nodes, then spawn
 *       points.  Without "force" a player who is neither in solid nor held
 *       up without ground is left where he is.
 *
 *   autoUnstuck (dvar, default 1): 0 off, 1 zm matches, 2 every match.
 *   autoUnstuckMs (default 3000): a living player who is pinned (see
 *       PlayerUnstuckFrame) and asks to move for this long is moved
 *       free, and it is logged.  autoUnstuckInput 0 drops the "asks
 *       to move" condition (idle players and bots too).
 *
 *   developer > 0 only: solidprobe <x y z>, sweepprobe
 *       <x1 y1 z1 x2 y2 z2> [crouch], floorprobe <x y z> and
 *       setpos <client x y z> map a reported spot and reproduce it.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "qcommon.h"
#include "cmd.h"
#include "cvar.h"
#include "cm_public.h"
#include "cm_local.h"
#include "g_shared.h"
#include "g_public.h"
#include "entity.h"
#include "cscr_stringlist.h"
#include "bot_nav.h"
#include "sv_state.h"
#include "pmove_c1_cmdscale.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

#define UNSTUCK_CONTENTS_BODY   0x02000000
#define UNSTUCK_DEFAULT_MASK    0x02810011
#define UNSTUCK_PM_NOCLIP       2
#define UNSTUCK_PM_UFO          3
#define UNSTUCK_PM_LASTSTAND    7
#define UNSTUCK_PMF_MANTLE      0x00000004
#define UNSTUCK_PMF_LADDER      0x00000008
#define UNSTUCK_PMF_FROZEN      0x00000800
#define UNSTUCK_PMF_REMOTE      0x00080000
#define UNSTUCK_NAV_CANDIDATES  48

static cvar_t *autoUnstuck;
static cvar_t *autoUnstuckMs;
static cvar_t *autoUnstuckInput;

static const float stand_mins[3]={-15.0f,-15.0f,0.0f};
static const float stand_maxs[3]={15.0f,15.0f,70.0f};
static const float crouch_maxs[3]={15.0f,15.0f,50.0f};

typedef struct {
    int since;          /* level.time the current in-solid spell began, 0 = clear */
    float spot[3];      /* where it began; moving more than an inch resets it */
    int lastCheck;
    int checks;         /* tests in this spell, and how many saw movement input */
    int inputChecks;
    int solid;          /* last test: hull in solid (else held up without ground) */
    int rescues;
} unstuck_state_t;
static unstuck_state_t unstuck_state[MAX_CLIENTS];

/* Players pass through each other and through actors' hit boxes in the
 * pmove only as far as CONTENTS_BODY goes; a crowd is never "stuck in
 * solid", so bodies are left out of every test here. */
static int unstuck_mask(const gentity_t *ent)
{
    int mask=ent && ent->clipmask ? ent->clipmask : UNSTUCK_DEFAULT_MASK;
    return mask & ~UNSTUCK_CONTENTS_BODY;
}

static int hull_in_solid(const gentity_t *ent,const float *pos,const float *mins,const float *maxs,trace_t *out)
{
    trace_t tr;
    G_TraceCapsule(&tr,pos,mins,maxs,pos,ent ? ent->s.number : ENTITYNUM_NONE,unstuck_mask(ent));
    if(out) *out=tr;
    return tr.startsolid || tr.allsolid;
}

static const char *ent_string(unsigned handle)
{
    return handle ? SL_ConvertToString(handle) : "";
}

/* What a hull at pos overlaps: the world brushes, and every linked entity
 * with contents in the mask whose box reaches it. */
static void report_overlap(const gentity_t *self,const float *pos,const float *mins,const float *maxs)
{
    trace_t tr;
    int mask=unstuck_mask(self),n,found=0;
    float absmin[3],absmax[3];
    hull_in_solid(self,pos,mins,maxs,&tr);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "  hull (%.0f %.0f %.0f)-(%.0f %.0f %.0f) at (%.3f %.3f %.3f) mask 0x%x: startsolid %d allsolid %d hitType %d id %d contents 0x%x surf 0x%x material %s\n",
        mins[0],mins[1],mins[2],maxs[0],maxs[1],maxs[2],pos[0],pos[1],pos[2],mask,
        tr.startsolid,tr.allsolid,(int)tr.hitType,tr.hitId,tr.cflags,tr.sflags,tr.material ? tr.material : "-");
    CM_BoxTrace(&tr,pos,pos,mins,maxs,0,mask);
    Com_Printf(CON_CHANNEL_DONT_FILTER,"  world brushes only: startsolid %d allsolid %d contents 0x%x material %s\n",
        tr.startsolid,tr.allsolid,tr.cflags,tr.material ? tr.material : "-");
    for(n=0;n<3;++n) { absmin[n]=pos[n]+mins[n]; absmax[n]=pos[n]+maxs[n]; }
    for(n=0;n<ENTITYNUM_WORLD;++n) {
        gentity_t *ent=&g_entities[n];
        trace_t et;
        if(!ent->r.inuse || !ent->r.linked || ent==self || !(ent->r.contents & mask)) continue;
        if(ent->r.absmin[0]>absmax[0] || ent->r.absmin[1]>absmax[1] || ent->r.absmin[2]>absmax[2] ||
           ent->r.absmax[0]<absmin[0] || ent->r.absmax[1]<absmin[1] || ent->r.absmax[2]<absmin[2]) continue;
        SV_ClipToEntity(&et,pos,mins,maxs,pos,n,mask,1);
        ++found;
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "  ent %d %s '%s' model %d solid %d contents 0x%x box (%.0f %.0f %.0f)-(%.0f %.0f %.0f)%s: %s\n",
            n,ent_string(ent->classname),ent_string(ent->targetname),ent->s.index,ent->s.solid,ent->r.contents,
            ent->r.absmin[0],ent->r.absmin[1],ent->r.absmin[2],ent->r.absmax[0],ent->r.absmax[1],ent->r.absmax[2],
            (ent->r.svFlags & SVF_NOCLIENT) ? " hidden" : "",
            et.startsolid || et.allsolid ? "OVERLAPS" : "box only");
    }
    if(!found) Com_Printf(CON_CHANNEL_DONT_FILTER,"  no solid entity reaches the hull\n");
    /* The world brushes whose bounds reach the hull (bounds only: a brush is
     * its planes, so a listed brush need not overlap). */
    found=0;
    for(n=0;n<(int)cm.numBrushes;++n) {
        const cbrush_t *b=&cm.brushes[n];
        if(!(b->contents & mask)) continue;
        if(b->mins[0]>absmax[0] || b->mins[1]>absmax[1] || b->mins[2]>absmax[2] ||
           b->maxs[0]<absmin[0] || b->maxs[1]<absmin[1] || b->maxs[2]<absmin[2]) continue;
        if(++found>12) continue;
        Com_Printf(CON_CHANNEL_DONT_FILTER,"  brush %d bounds (%.3f %.3f %.3f)-(%.3f %.3f %.3f) contents 0x%x sides %u\n",
            n,b->mins[0],b->mins[1],b->mins[2],b->maxs[0],b->maxs[1],b->maxs[2],(unsigned)b->contents,b->numsides);
    }
    if(found>12) Com_Printf(CON_CHANNEL_DONT_FILTER,"  ... %d brushes in all\n",found);
}

/* A standing (else crouching) hull is clear at pos and a floor is within
 * 64 units below; out is pos settled onto that floor. */
static int spot_usable(const gentity_t *ent,const float *pos,float *out)
{
    trace_t tr;
    float end[3];
    const float *maxs=stand_maxs;
    if(hull_in_solid(ent,pos,stand_mins,stand_maxs,NULL)) {
        if(hull_in_solid(ent,pos,stand_mins,crouch_maxs,NULL)) return 0;
        maxs=crouch_maxs;
    }
    end[0]=pos[0]; end[1]=pos[1]; end[2]=pos[2]-64.0f;
    G_TraceCapsule(&tr,pos,stand_mins,maxs,end,ent->s.number,unstuck_mask(ent));
    if(tr.startsolid || tr.allsolid || tr.fraction>=1.0f || tr.normal[2]<0.7f) return 0;
    out[0]=pos[0]; out[1]=pos[1];
    out[2]=pos[2]+(end[2]-pos[2])*tr.fraction+0.125f;
    return 1;
}

static float dist2(const float *a,const float *b)
{
    float x=a[0]-b[0],y=a[1]-b[1],z=a[2]-b[2];
    return x*x+y*y+z*z;
}

/* A clear spot: offsets of up to 12 units first (a hull whose centre is in a
 * wall could otherwise come out on its far side), then waypoint-graph nodes
 * nearest first (always inside the playable space), then spawn entities. */
static int find_clear_spot(const gentity_t *ent,float *out,const char **how)
{
    static const float radii[]={0.0f,2.0f,4.0f,8.0f,12.0f};
    static const float lifts[]={0.0f,4.0f,10.0f,18.0f};
    const float *o=ent->client->ps.origin;
    float probe[3];
    unsigned r,l,d;
    int count,i,best[UNSTUCK_NAV_CANDIDATES],nbest=0;
    float bestd[UNSTUCK_NAV_CANDIDATES];

    for(r=0;r<sizeof(radii)/sizeof(radii[0]);++r)
        for(l=0;l<sizeof(lifts)/sizeof(lifts[0]);++l)
            for(d=0;d<(r ? 16u : 1u);++d) {
                float yaw=(float)d*(float)(M_PI/8.0);
                probe[0]=o[0]+radii[r]*cosf(yaw);
                probe[1]=o[1]+radii[r]*sinf(yaw);
                probe[2]=o[2]+lifts[l];
                if(spot_usable(ent,probe,out)) { *how="nearby offset"; return 1; }
            }

    count=BotNavCount();
    for(i=0;i<count;++i) {
        const float *p=BotNavPoint(i);
        float dd;
        int k;
        if(!p) continue;
        /* Height counts twice over: the node one floor down through the
         * ceiling is not the nearest. */
        dd=dist2(p,o)+3.0f*(p[2]-o[2])*(p[2]-o[2]);
        if(nbest==UNSTUCK_NAV_CANDIDATES && dd>=bestd[nbest-1]) continue;
        k=nbest<UNSTUCK_NAV_CANDIDATES ? nbest++ : nbest-1;
        while(k>0 && bestd[k-1]>dd) { bestd[k]=bestd[k-1]; best[k]=best[k-1]; --k; }
        bestd[k]=dd; best[k]=i;
    }
    /* Nodes in sight of the player's chest first (not through a wall),
     * then any. */
    for(r=0;r<2;++r)
        for(i=0;i<nbest;++i) {
            const float *p=BotNavPoint(best[i]);
            if(!r) {
                trace_t tr;
                static const float zero[3]={0.0f,0.0f,0.0f};
                float a[3]={o[0],o[1],o[2]+40.0f},b[3]={p[0],p[1],p[2]+40.0f};
                CM_BoxTrace(&tr,a,b,zero,zero,0,UNSTUCK_DEFAULT_MASK & ~UNSTUCK_CONTENTS_BODY);
                if(tr.startsolid || tr.fraction<1.0f) continue;
            }
            for(l=0;l<2;++l) {
                probe[0]=p[0]; probe[1]=p[1]; probe[2]=p[2]+(l ? 18.0f : 1.0f);
                if(spot_usable(ent,probe,out)) { *how=r ? "waypoint node" : "waypoint node in sight"; return 1; }
            }
        }

    for(i=0;i<ENTITYNUM_WORLD;++i) {
        const gentity_t *s=&g_entities[i];
        const char *cls;
        if(!s->r.inuse || !s->classname) continue;
        cls=SL_ConvertToString(s->classname);
        if(!cls) continue;
        if(Q_stricmpn(cls,"info_player",11) && (Q_stricmpn(cls,"mp_",3) || !strstr(cls,"spawn"))) continue;
        probe[0]=s->r.currentOrigin[0]; probe[1]=s->r.currentOrigin[1]; probe[2]=s->r.currentOrigin[2]+1.0f;
        if(spot_usable(ent,probe,out)) { *how="spawn point"; return 1; }
    }
    return 0;
}

/* No ground under him, and not on a ladder, mantling, frozen or remote
 * controlling (all held up on purpose). */
static int held_up(const playerState_t *ps)
{
    return ps->groundEntityNum==ENTITYNUM_NONE &&
        !(ps->pm_flags & (UNSTUCK_PMF_MANTLE|UNSTUCK_PMF_LADDER|UNSTUCK_PMF_FROZEN|UNSTUCK_PMF_REMOTE));
}

static int eligible(const gentity_t *ent,const char **why)
{
    const gclient_t *cl=ent->client;
    if(!ent->r.inuse || !cl) { *why="no player"; return 0; }
    if(cl->sess.sessionState!=SESS_STATE_PLAYING) { *why="not playing"; return 0; }
    if(ent->health<=0) { *why="dead"; return 0; }
    if(cl->ps.pm_type==UNSTUCK_PM_NOCLIP || cl->ps.pm_type==UNSTUCK_PM_UFO) { *why="noclip"; return 0; }
    if(cl->ps.pm_type!=0 && cl->ps.pm_type!=UNSTUCK_PM_LASTSTAND) { *why="pm_type not normal"; return 0; }
    if(ent->tagInfo) { *why="linked to an entity"; return 0; }
    *why="";
    return 1;
}

/* Moves the player to a clear spot; returns 1 when he was moved. */
static int rescue(gentity_t *ent,const char *reason)
{
    float spot[3],from[3],angles[3];
    const char *how="";
    int slot=ent->s.number;
    memcpy(from,ent->client->ps.origin,sizeof(from));
    if(!find_clear_spot(ent,spot,&how)) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"CoDOL unstuck: client %d '%s' at (%.3f %.3f %.3f) (%s): no clear spot found\n",
            slot,svs.clients[slot].name,from[0],from[1],from[2],reason);
        return 0;
    }
    memcpy(angles,ent->client->ps.viewangles,sizeof(angles));
    TeleportPlayer(ent,spot,angles);
    ent->client->ps.velocity[0]=ent->client->ps.velocity[1]=ent->client->ps.velocity[2]=0.0f;
    if(slot>=0 && slot<MAX_CLIENTS) { unstuck_state[slot].since=0; ++unstuck_state[slot].rescues; }
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "CoDOL unstuck: client %d '%s' moved from (%.3f %.3f %.3f) to (%.3f %.3f %.3f) via %s, %.0f units (%s)\n",
        slot,svs.clients[slot].name,from[0],from[1],from[2],spot[0],spot[1],spot[2],how,sqrtf(dist2(from,spot)),reason);
    return 1;
}

static void Unstuck_f(void)
{
    gentity_t *ent;
    const char *why;
    int slot,force,solid,hovering;
    if(Cmd_Argc()<2) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"usage: unstuck <clientnum> [force]\n");
        return;
    }
    if(sv.state!=SS_GAME) { Com_Printf(CON_CHANNEL_DONT_FILTER,"unstuck: no game running\n"); return; }
    slot=atoi(Cmd_Argv(1));
    force=Cmd_Argc()>2 && !Q_stricmp(Cmd_Argv(2),"force");
    if(slot<0 || slot>=sv_maxclients->integer || slot>=MAX_CLIENTS || svs.clients[slot].state<CS_ACTIVE) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"unstuck: client %d is not in the game\n",slot);
        return;
    }
    ent=&g_entities[slot];
    if(!eligible(ent,&why)) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"unstuck: client %d '%s' left alone: %s\n",slot,svs.clients[slot].name,why);
        return;
    }
    solid=hull_in_solid(ent,ent->client->ps.origin,ent->r.mins,ent->r.maxs,NULL);
    hovering=held_up(&ent->client->ps);
    Com_Printf(CON_CHANNEL_DONT_FILTER,"unstuck: client %d '%s' at (%.3f %.3f %.3f) pm_type %d pmf 0x%x ground %d: %s\n",
        slot,svs.clients[slot].name,ent->client->ps.origin[0],ent->client->ps.origin[1],ent->client->ps.origin[2],
        ent->client->ps.pm_type,ent->client->ps.pm_flags,ent->client->ps.groundEntityNum,
        solid ? "IN SOLID" : hovering ? "clear but without ground" : "clear");
    solid|=hovering;
    report_overlap(ent,ent->client->ps.origin,ent->r.mins,ent->r.maxs);
    if(!solid && !force) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"unstuck: not pinned; add 'force' to move him anyway\n");
        return;
    }
    if(!solid && force) {
        /* A wedged-but-clear player: the nearby offsets would leave him in
         * the same pocket, so go straight to the graph. */
        float spot[3],from[3],angles[3];
        int count=BotNavCount(),i,best=-1;
        float bestd=0.0f;
        memcpy(from,ent->client->ps.origin,sizeof(from));
        for(i=0;i<count;++i) {
            const float *p=BotNavPoint(i);
            float dd,probe[3];
            if(!p) continue;
            dd=dist2(p,from);
            if(dd<48.0f*48.0f || (best>=0 && dd>=bestd)) continue;
            probe[0]=p[0]; probe[1]=p[1]; probe[2]=p[2]+1.0f;
            if(!spot_usable(ent,probe,spot)) continue;
            best=i; bestd=dd;
        }
        if(best<0) { rescue(ent,"rcon force"); return; }
        {
            const float *p=BotNavPoint(best);
            float probe[3]={p[0],p[1],p[2]+1.0f};
            spot_usable(ent,probe,spot);
        }
        memcpy(angles,ent->client->ps.viewangles,sizeof(angles));
        TeleportPlayer(ent,spot,angles);
        ent->client->ps.velocity[0]=ent->client->ps.velocity[1]=ent->client->ps.velocity[2]=0.0f;
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "CoDOL unstuck: client %d '%s' moved from (%.3f %.3f %.3f) to (%.3f %.3f %.3f) via waypoint node %d, %.0f units (rcon force)\n",
            slot,svs.clients[slot].name,from[0],from[1],from[2],spot[0],spot[1],spot[2],best,sqrtf(bestd));
        return;
    }
    rescue(ent,"rcon");
}

static void SolidProbe_f(void)
{
    float pos[3];
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"solidprobe needs developer 1\n"); return;
    }
    if(sv.state!=SS_GAME || Cmd_Argc()<4) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"usage (in a game): solidprobe <x> <y> <z>\n"); return;
    }
    pos[0]=(float)atof(Cmd_Argv(1)); pos[1]=(float)atof(Cmd_Argv(2)); pos[2]=(float)atof(Cmd_Argv(3));
    Com_Printf(CON_CHANNEL_DONT_FILTER,"solidprobe (%.3f %.3f %.3f) contents 0x%x:\n",
        pos[0],pos[1],pos[2],CM_PointContents(pos,0));
    report_overlap(NULL,pos,stand_mins,stand_maxs);
    report_overlap(NULL,pos,stand_mins,crouch_maxs);
    {
        /* Point contents around the point, 4-unit grid, +y up the page:
         * '#' solid for a player, 'o' the probed point. */
        static const float heights[]={2.0f,30.0f,60.0f};
        unsigned h;
        int x,y;
        for(h=0;h<3;++h) {
            Com_Printf(CON_CHANNEL_DONT_FILTER,"  z %+.0f, x %.0f..%.0f:\n",heights[h],pos[0]-32.0f,pos[0]+32.0f);
            for(y=8;y>=-8;--y) {
                char row[20];
                for(x=-8;x<=8;++x) {
                    float p[3]={pos[0]+4.0f*x,pos[1]+4.0f*y,pos[2]+heights[h]};
                    row[x+8]=(CM_PointContents(p,0) & UNSTUCK_DEFAULT_MASK & ~UNSTUCK_CONTENTS_BODY) ? '#' : (x||y) ? '.' : 'o';
                }
                row[17]='\0';
                Com_Printf(CON_CHANNEL_DONT_FILTER,"   y %+4.0f %s\n",pos[1]+4.0f*y,row);
            }
        }
    }
}

/* sweepprobe <x1> <y1> <z1> <x2> <y2> <z2> [crouch]: a standing (or
 * crouching) player capsule swept from one point to the other, as the pmove
 * traces it.  developer > 0. */
static void SweepProbe_f(void)
{
    float a[3],b[3],end[3];
    const float *maxs;
    trace_t tr;
    int i;
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"sweepprobe needs developer 1\n"); return;
    }
    if(sv.state!=SS_GAME || Cmd_Argc()<7) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"usage (in a game): sweepprobe <x1> <y1> <z1> <x2> <y2> <z2> [crouch]\n"); return;
    }
    for(i=0;i<3;++i) { a[i]=(float)atof(Cmd_Argv(1+i)); b[i]=(float)atof(Cmd_Argv(4+i)); }
    maxs=Cmd_Argc()>7 ? crouch_maxs : stand_maxs;
    G_TraceCapsule(&tr,a,stand_mins,maxs,b,ENTITYNUM_NONE,UNSTUCK_DEFAULT_MASK & ~UNSTUCK_CONTENTS_BODY);
    for(i=0;i<3;++i) end[i]=a[i]+(b[i]-a[i])*tr.fraction;
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "sweepprobe (%.3f %.3f %.3f) -> (%.3f %.3f %.3f) maxs z %.0f: fraction %.4f startsolid %d allsolid %d end (%.3f %.3f %.3f) normal (%.3f %.3f %.3f) contents 0x%x; end in solid %d\n",
        a[0],a[1],a[2],b[0],b[1],b[2],maxs[2],tr.fraction,tr.startsolid,tr.allsolid,end[0],end[1],end[2],
        tr.normal[0],tr.normal[1],tr.normal[2],tr.cflags,hull_in_solid(NULL,end,stand_mins,maxs,NULL));
}

/* setpos <clientnum> <x> <y> <z>: puts a player at an exact origin,
 * solid or not, to reproduce a stuck report.  developer > 0. */
static void SetPos_f(void)
{
    float pos[3],angles[3];
    int slot;
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"setpos needs developer 1\n"); return;
    }
    if(sv.state!=SS_GAME || Cmd_Argc()<5) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"usage (in a game): setpos <clientnum> <x> <y> <z>\n"); return;
    }
    slot=atoi(Cmd_Argv(1));
    if(slot<0 || slot>=sv_maxclients->integer || slot>=MAX_CLIENTS || svs.clients[slot].state<CS_ACTIVE ||
       !g_entities[slot].client) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"setpos: client %d is not in the game\n",slot); return;
    }
    pos[0]=(float)atof(Cmd_Argv(2)); pos[1]=(float)atof(Cmd_Argv(3)); pos[2]=(float)atof(Cmd_Argv(4));
    memcpy(angles,g_entities[slot].client->ps.viewangles,sizeof(angles));
    TeleportPlayer(&g_entities[slot],pos,angles);
    Com_Printf(CON_CHANNEL_DONT_FILTER,"setpos: client %d at (%.3f %.3f %.3f)\n",slot,pos[0],pos[1],pos[2]);
}

/* floorprobe <x> <y> <z>: a 1-unit box dropped from z+24 to z-8 on
 * a 4-unit grid around (x y), printing the surface it lands on (height above
 * z, slope, material), to map the floor a player stands on.  developer > 0. */
static void FloorProbe_f(void)
{
    static const float mins[3]={-0.5f,-0.5f,-0.5f},maxs[3]={0.5f,0.5f,0.5f};
    float pos[3];
    int x,y;
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"floorprobe needs developer 1\n"); return;
    }
    if(sv.state!=SS_GAME || Cmd_Argc()<4) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"usage (in a game): floorprobe <x> <y> <z>\n"); return;
    }
    pos[0]=(float)atof(Cmd_Argv(1)); pos[1]=(float)atof(Cmd_Argv(2)); pos[2]=(float)atof(Cmd_Argv(3));
    for(y=5;y>=-5;--y)
        for(x=-5;x<=5;++x) {
            trace_t tr;
            float a[3]={pos[0]+4.0f*x,pos[1]+4.0f*y,pos[2]+24.0f},b[3]={a[0],a[1],pos[2]-8.0f};
            CM_BoxTrace(&tr,a,b,mins,maxs,0,UNSTUCK_DEFAULT_MASK & ~UNSTUCK_CONTENTS_BODY);
            if(tr.startsolid) { Com_Printf(CON_CHANNEL_DONT_FILTER,"  (%.0f %.0f) start in solid\n",a[0],a[1]); continue; }
            if(tr.fraction>=1.0f) { Com_Printf(CON_CHANNEL_DONT_FILTER,"  (%.0f %.0f) no floor\n",a[0],a[1]); continue; }
            Com_Printf(CON_CHANNEL_DONT_FILTER,"  (%.0f %.0f) floor %+.3f normal (%.3f %.3f %.3f) contents 0x%x surf 0x%x %s\n",
                a[0],a[1],a[2]+(b[2]-a[2])*tr.fraction-0.5f-pos[2],tr.normal[0],tr.normal[1],tr.normal[2],
                tr.cflags,tr.sflags,tr.material ? tr.material : "-");
        }
}

/* C1 PM_FoliageSounds 0x413b60's standing test box (C1FoliageBox),
 * traced from the origin to itself against contents 2 (foliage). */
static int foliage_test(const float *origin)
{
    trace_t tr;
    float mins[3],maxs[3];
    C1FoliageBox(stand_mins,stand_maxs,mins,maxs);
    memset(&tr,0,sizeof(tr));
    G_TraceCapsule(&tr,origin,mins,maxs,origin,ENTITYNUM_NONE,2);
    return tr.startsolid;
}

/* foliageprobe [x y z]: how much of the clip map carries contents 2
 * (foliage), and whether the C1 foliage-sound box finds it -- at the given
 * origin, or at the bottom centre of the first foliage brushes.  developer > 0. */
static void FoliageProbe_f(void)
{
    unsigned i,brushes=0,materials=0,shown=0,hits=0,tested=0;
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"foliageprobe needs developer 1\n"); return;
    }
    if(sv.state!=SS_GAME) {
        Com_Printf(CON_CHANNEL_DONT_FILTER,"usage (in a game): foliageprobe [x y z]\n"); return;
    }
    if(Cmd_Argc()>=7) {
        /* x0 y0 x1 y1 z step: every grid point whose box finds foliage. */
        float x0=(float)atof(Cmd_Argv(1)),y0=(float)atof(Cmd_Argv(2)),x1=(float)atof(Cmd_Argv(3));
        float y1=(float)atof(Cmd_Argv(4)),z=(float)atof(Cmd_Argv(5)),step=(float)atof(Cmd_Argv(6)),x,y;
        unsigned found=0,total=0;
        if(!(step>=1.0f)) step=8.0f;
        for(y=y0;y<=y1;y+=step) for(x=x0;x<=x1;x+=step) {
            float p[3]={x,y,z};
            ++total;
            if(!foliage_test(p)) continue;
            if(++found<=40) Com_Printf(CON_CHANNEL_DONT_FILTER,"  foliage at (%.1f %.1f %.1f)\n",x,y,z);
        }
        Com_Printf(CON_CHANNEL_DONT_FILTER,"foliageprobe grid: %u of %u points\n",found,total);
        return;
    }
    if(Cmd_Argc()>=4) {
        float pos[3]={(float)atof(Cmd_Argv(1)),(float)atof(Cmd_Argv(2)),(float)atof(Cmd_Argv(3))};
        float r=Cmd_Argc()>=5 ? (float)atof(Cmd_Argv(4)) : 0.0f;
        Com_Printf(CON_CHANNEL_DONT_FILTER,"foliageprobe (%.3f %.3f %.3f): startsolid %d point contents 0x%x\n",
            pos[0],pos[1],pos[2],foliage_test(pos),CM_PointContents(pos,0));
        if(r>0.0f) {
            int e;
            for(i=0;i<cm.numStaticModels;++i) {
                const cStaticModel_t *sm=&cm.staticModelList[i];
                if(sm->absmax[0]<pos[0]-r || sm->absmin[0]>pos[0]+r ||
                   sm->absmax[1]<pos[1]-r || sm->absmin[1]>pos[1]+r ||
                   sm->absmax[2]<pos[2]-r || sm->absmin[2]>pos[2]+r) continue;
                Com_Printf(CON_CHANNEL_DONT_FILTER,"  static model %u '%s' at (%.1f %.1f %.1f) bounds (%.1f %.1f %.1f)-(%.1f %.1f %.1f)\n",
                    i,sm->xmodel && *(const char *const *)sm->xmodel ? *(const char *const *)sm->xmodel : "?",
                    sm->origin[0],sm->origin[1],sm->origin[2],sm->absmin[0],sm->absmin[1],sm->absmin[2],
                    sm->absmax[0],sm->absmax[1],sm->absmax[2]);
            }
            for(e=0;e<level.num_entities;++e) {
                const gentity_t *g=&g_entities[e];
                if(!g->r.inuse || !g->r.linked) continue;
                if(g->r.absmax[0]<pos[0]-r || g->r.absmin[0]>pos[0]+r ||
                   g->r.absmax[1]<pos[1]-r || g->r.absmin[1]>pos[1]+r ||
                   g->r.absmax[2]<pos[2]-r || g->r.absmin[2]>pos[2]+r) continue;
                Com_Printf(CON_CHANNEL_DONT_FILTER,"  entity %d eType %d contents 0x%x classname %s bounds (%.1f %.1f %.1f)-(%.1f %.1f %.1f)\n",
                    e,g->s.eType,(unsigned)g->r.contents,g->classname ? SL_ConvertToString(g->classname) : "?",
                    g->r.absmin[0],g->r.absmin[1],g->r.absmin[2],g->r.absmax[0],g->r.absmax[1],g->r.absmax[2]);
            }
        }
        return;
    }
    for(i=0;i<cm.numMaterials;++i) if(cm.materials[i].contentFlags&2) {
        ++materials;
        if(materials<=8) Com_Printf(CON_CHANNEL_DONT_FILTER,"  material %u '%s' contents 0x%x surface 0x%x\n",
            i,cm.materials[i].material,cm.materials[i].contentFlags,cm.materials[i].surfaceFlags);
    }
    for(i=0;i<cm.numBrushes;++i) {
        const cbrush_t *b=&cm.brushes[i];
        float p[3];
        if(!(b->contents&2)) continue;
        ++brushes;
        p[0]=(b->mins[0]+b->maxs[0])*0.5f; p[1]=(b->mins[1]+b->maxs[1])*0.5f; p[2]=b->mins[2]+1.0f;
        if(tested<256) { ++tested; if(foliage_test(p)) ++hits; }
        if(shown<8) {
            ++shown;
            Com_Printf(CON_CHANNEL_DONT_FILTER,"  brush %u contents 0x%x mins (%.1f %.1f %.1f) maxs (%.1f %.1f %.1f): test at (%.1f %.1f %.1f) startsolid %d\n",
                i,(unsigned)b->contents,b->mins[0],b->mins[1],b->mins[2],b->maxs[0],b->maxs[1],b->maxs[2],
                p[0],p[1],p[2],foliage_test(p));
        }
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,"foliageprobe: %u of %u brushes and %u of %u materials carry contents 2; the C1 foliage box found %u of the first %u brushes\n",
        brushes,(unsigned)cm.numBrushes,materials,cm.numMaterials,hits,tested);
    {
        unsigned trees=0,treehits=0,treetested=0,treeshown=0;
        for(i=0;i<(unsigned)cm.aabbTreeCount;++i) {
            const CollisionAabbTree_t *t=&cm.aabbTrees[i];
            float p[3];
            if(t->childCount || t->materialIndex>=cm.numMaterials ||
               !(cm.materials[t->materialIndex].contentFlags&2)) continue;
            ++trees;
            /* Put the box's middle (z + 31.5) on the leaf's centre. */
            p[0]=t->origin[0]; p[1]=t->origin[1]; p[2]=t->origin[2]-31.5f;
            if(treetested<512) { ++treetested; if(foliage_test(p)) ++treehits; }
            if(treeshown<6) {
                ++treeshown;
                Com_Printf(CON_CHANNEL_DONT_FILTER,"  tri leaf %u '%s' centre (%.1f %.1f %.1f) half (%.1f %.1f %.1f): startsolid %d\n",
                    i,cm.materials[t->materialIndex].material,t->origin[0],t->origin[1],t->origin[2],
                    t->halfSize[0],t->halfSize[1],t->halfSize[2],foliage_test(p));
            }
        }
        Com_Printf(CON_CHANNEL_DONT_FILTER,"foliageprobe: %u triangle leaves carry contents 2; the C1 foliage box found %u of the first %u\n",
            trees,treehits,treetested);
    }
}

/* Can the player get anywhere from here?  The capsule swept 8 units in eight
 * horizontal directions; free when any sweep covers at least 4 of them.  A
 * player in a corner can always back out of it, so this is what separates
 * "pushing into a wall" from "pinned". */
static int hull_mobile(const gentity_t *ent)
{
    const float *o=ent->client->ps.origin;
    unsigned d;
    for(d=0;d<8;++d) {
        trace_t tr;
        float yaw=(float)d*(float)(M_PI/4.0),end[3];
        end[0]=o[0]+8.0f*cosf(yaw); end[1]=o[1]+8.0f*sinf(yaw); end[2]=o[2];
        G_TraceCapsule(&tr,o,ent->r.mins,ent->r.maxs,end,ent->s.number,unstuck_mask(ent));
        if(!tr.allsolid && tr.fraction>=0.5f) return 1;
    }
    return 0;
}

static int wants_to_move(int slot)
{
    const usercmd_t *cmd=&svs.clients[slot].lastUsercmd;
    return cmd->forwardmove || cmd->rightmove || (cmd->buttons & KEY_MASK_JUMP);
}

/* Once per server frame, from SV_Frame while a game runs.  Each living,
 * standing (not downed) player is tested four times a second.  A rescue
 * needs all of, for autoUnstuckMs:
 *   - either the hull position-tests in solid (players, actors excluded) and
 *     no horizontal sweep gets halfway out (hull_mobile), or the player has
 *     no ground and is not on a ladder or mantling (held up in the air),
 *   - the origin has not moved more than an inch,
 *   - the player asked to move on at least half of the tests.
 * zm_asylum_sh 2026-10-05 is the case it was written for: on the 226 floor
 * south of the Speed Cola trap, the 24-unit slot between a 0.5-unit trim
 * (y 516.5, top 226.5) and a 3-unit trim (y 492) is narrower than the
 * 30-unit hull.  The capsule's round bottom sinks into it and comes to rest
 * on the two trims' chamfered edges at z 226.377 (reproduced with
 * setpos; the live eye height 286.379 is that origin plus 60): the
 * ground trace reads them as a 0.673 slope, so groundEntityNum stays NONE,
 * there is no walking and no jump, the gravity sweep is blocked and every
 * horizontal sweep meets a trim 0.12 above the hull's bottom that an
 * airborne pmove does not step over. */
void PlayerUnstuckFrame(void)
{
    int slot,limit,now=level.time,hold;
    const char *zone;
    if(!autoUnstuck || !autoUnstuck->integer) return;
    if(autoUnstuck->integer==1) {
        zone=ServerZoneType();
        if(!zone || strcmp(zone,"zm")) return;
    }
    hold=autoUnstuckMs ? autoUnstuckMs->integer : 3000;
    limit=sv_maxclients->integer<MAX_CLIENTS ? sv_maxclients->integer : MAX_CLIENTS;
    for(slot=0;slot<limit;++slot) {
        unstuck_state_t *st=&unstuck_state[slot];
        gentity_t *ent=&g_entities[slot];
        const char *why;
        if(svs.clients[slot].state<CS_ACTIVE || !eligible(ent,&why) ||
           ent->client->ps.pm_type==UNSTUCK_PM_LASTSTAND) { st->since=0; continue; }
        if(st->lastCheck && now-st->lastCheck>=0 && now-st->lastCheck<250) continue;
        st->lastCheck=now ? now : 1;
        {
            const playerState_t *ps=&ent->client->ps;
            int solid=hull_in_solid(ent,ps->origin,ent->r.mins,ent->r.maxs,NULL);
            /* Airborne but held up: no ground, yet gravity has not moved
             * him for the whole spell.  Ladders, mantles, frozen and
             * remote-controlling players are held up on purpose. */
            int hovering=held_up(ps);
            if(!(solid && !hull_mobile(ent)) && !hovering) { st->since=0; continue; }
            st->solid=solid;
        }
        if(!st->since || now<st->since || dist2(st->spot,ent->client->ps.origin)>1.0f) {
            st->since=now ? now : 1;
            st->checks=st->inputChecks=0;
            memcpy(st->spot,ent->client->ps.origin,sizeof(st->spot));
        }
        ++st->checks;
        if(wants_to_move(slot)) ++st->inputChecks;
        if(now-st->since<hold) continue;
        if(autoUnstuckInput->integer && st->inputChecks*2<st->checks) continue;
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "CoDOL unstuck: client %d '%s' pinned (%s) at (%.3f %.3f %.3f) for %d ms, moving on %d of %d tests (pm_type %d pmf 0x%x ground %d):\n",
            slot,svs.clients[slot].name,st->solid ? "hull in solid" : "held up without ground",
            st->spot[0],st->spot[1],st->spot[2],now-st->since,st->inputChecks,st->checks,
            ent->client->ps.pm_type,ent->client->ps.pm_flags,ent->client->ps.groundEntityNum);
        report_overlap(ent,ent->client->ps.origin,ent->r.mins,ent->r.maxs);
        if(!rescue(ent,"automatic")) { st->since=now ? now : 1; st->checks=st->inputChecks=0; }
    }
}

void RegisterPlayerUnstuck(void)
{
    autoUnstuck=Cvar_RegisterInt("autoUnstuck",1,0,2,0,
        "Move a living player pinned where he cannot move (hull in solid, or held up without ground): 0 off, 1 zm matches, 2 every match");
    autoUnstuckMs=Cvar_RegisterInt("autoUnstuckMs",3000,500,60000,0,
        "How long a pinned player must stay motionless before autoUnstuck moves him");
    autoUnstuckInput=Cvar_RegisterBool("autoUnstuckInput",qtrue,0,
        "autoUnstuck only moves a pinned player who is pressing to move (0: also an idle one)");
    Cmd_AddCommand("unstuck",Unstuck_f);
    Cmd_AddCommand("solidprobe",SolidProbe_f);
    Cmd_AddCommand("sweepprobe",SweepProbe_f);
    Cmd_AddCommand("floorprobe",FloorProbe_f);
    Cmd_AddCommand("setpos",SetPos_f);
    Cmd_AddCommand("foliageprobe",FoliageProbe_f);
}

/* For the psev trace: "eType/owner/classname" of a cursor-hint entity, so an
 * offhand press that C1 and the server resolve differently (C1
 * PM_Weapon_CheckForOffHand 0x41cc5d: a hint entity forces OffHandInit) names
 * what the server's hint was. */
void TraceHintEntity(int entnum, char *out, unsigned size)
{
    const gentity_t *g;
    if(!out || !size) return;
    if(entnum<0 || entnum>=MAX_GENTITIES || entnum>=level.num_entities) { snprintf(out,size,"-"); return; }
    g=&g_entities[entnum];
    snprintf(out,size,"%d/%d/%s",g->s.eType,g->r.ownerNum.number ? g->r.ownerNum.number-1 : -1,
        g->classname ? SL_ConvertToString(g->classname) : "?");
}

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailUnstuckEmpty(void){return resident_tail_zero(unstuck_state,sizeof(unstuck_state));}
void PersistentTailUnstuckReset(void){memset(unstuck_state,0,sizeof(unstuck_state));}
#endif

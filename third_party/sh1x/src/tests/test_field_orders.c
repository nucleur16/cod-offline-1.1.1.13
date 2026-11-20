/* C1 field orders: the SetFO* methods store C1's bytes into the captured
 * playerState extension, the snapshot overlay publishes them on wire fields
 * 0x1b4..0x1d0, and the archive record carries them.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "g_shared.h"
#include "server.h"
#include "playerstate_ext.h"
#include "field_orders.h"
#include "qcommon.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
static gclient_t clients[2];
static client_t snapshots[1];
const void *PlayerStateExtLiveBase(unsigned *n){*n=2;return clients;}
const void *PlayerStateExtSnapshotBase(unsigned *n){*n=1;return snapshots;}
const void *PlayerStateExtArchiveBase(unsigned *n){*n=0;return NULL;}
void Com_PrintWarning(conChannel_t c,const char *f,...){(void)c;(void)f;}
/* The publish trace (ol_debugTrace) stays off here. */
cvar_t *ol_debugTrace;
void QDECL Com_Printf(conChannel_t c,const char *f,...){(void)c;(void)f;}
int ClientIsBotClient(unsigned c){(void)c;return 0;}
int PlayerStateEMPJammed(const playerState_t *ps){(void)ps;return 0;}
qboolean Assert_MyHandler(const char *e,const char *f,int l,const char *fn,const char *fmt,...)
{(void)fmt;fprintf(stderr,"%s %s:%d %s\n",e,f,l,fn);abort();}

/* A two-argument script call. */
static int params[4], param_count, errors;
int Scr_GetNumParam(void){return param_count;}
int Scr_GetInt(unsigned int i){assert((int)i<param_count);return params[i];}
void Scr_Error(const char *s){(void)s;++errors;}
void Scr_ObjectError(const char *s){(void)s;++errors;}
void Scr_ParamError(int i,const char *s){(void)i;(void)s;++errors;}

static void call(void (*method)(scr_entref_t),unsigned entnum,int argc,int a,int b)
{
    scr_entref_t ref;
    memset(&ref,0,sizeof(ref));
    ref.entnum=(uint16_t)entnum;
    param_count=argc;params[0]=a;params[1]=b;
    method(ref);
}

static uint32_t wire_value(const player_fields_t *w,unsigned offset)
{
    unsigned i;
    for(i=0;i<PLAYER_FIELDS;++i)
        if(OL_PlayerField(i)->offset==offset) return w->field[i];
    assert(!"wire field missing");
    return 0;
}

static void roundtrip(const ps_ext_t *state,const ps_ext_t *base,ps_ext_t *out)
{
    byte data[8192];msg_t msg;int size;
    MSG_Init(&msg,data,sizeof(data));
    PlayerStateExtWrite(&msg,state,base);
    assert(!msg.overflowed);size=msg.cursize;
    MSG_Init(&msg,data,sizeof(data));msg.cursize=size;
    PlayerStateExtRead(&msg,out,base);
    assert(!msg.overflowed);
}

int main(void)
{
    playerState_t *ps=&clients[0].ps;
    const field_orders_t *fo;
    player_fields_t wire;
    ps_ext_t captured,decoded,reset={0};

    level.clients=clients;
    g_entities[0].client=&clients[0];
    fo=&PlayerStateExtConst(ps)->field_orders;

    /* C1 0x518e80..0x519224: slot 0 goal row 3, progress 2, reward 50, slot 1
     * status 1; the stored value is the low byte of the integer. */
    call(Scr_SetFOGoal,0,2,0,3);
    call(Scr_SetFOProgress,0,2,0,2);
    call(Scr_SetFOReward,0,2,0,50);
    call(Scr_SetFOStatus,0,2,1,1);
    call(Scr_SetFOGoal,0,2,1,0x105);
    assert(!errors);
    assert(fo->goal[0]==3 && fo->progress[0]==2 && fo->reward[0]==50 && fo->status[1]==1);
    assert(fo->goal[1]==5);

    /* Rejected: a non-player, a wrong argument count, an index C1 would have
     * written past the slot array with. Nothing changes. */
    call(Scr_SetFOGoal,1,2,0,9);assert(errors==1);
    call(Scr_SetFOGoal,0,1,0,9);assert(errors==2);
    call(Scr_SetFOGoal,0,2,2,9);assert(errors==3);
    call(Scr_SetFOGoal,0,2,-1,9);assert(errors==4);
    assert(fo->goal[0]==3 && fo->goal[1]==5 && fo->progress[0]==2);

    /* Publication onto C1's wire fields; status keeps its two wire bits. */
    call(Scr_SetFOStatus,0,2,0,6);
    memset(&wire,0,sizeof(wire));
    assert(OverlayFieldOrders(&wire,ps));
    assert(wire_value(&wire,0x1b4)==3 && wire_value(&wire,0x1b8)==5);
    assert(wire_value(&wire,0x1bc)==2 && wire_value(&wire,0x1c0)==0);
    assert(wire_value(&wire,0x1c4)==50 && wire_value(&wire,0x1c8)==0);
    assert(wire_value(&wire,0x1cc)==2 && wire_value(&wire,0x1d0)==1);

    /* The archive record: full, delta, and back to none. */
    captured=*PlayerStateExtConst(ps);
    roundtrip(&captured,NULL,&decoded);
    assert(!memcmp(&decoded.field_orders,&captured.field_orders,sizeof(captured.field_orders)));
    call(Scr_SetFOProgress,0,2,0,7);
    roundtrip(PlayerStateExtConst(ps),&captured,&decoded);
    assert(decoded.field_orders.progress[0]==7 && decoded.field_orders.goal[1]==5);
    roundtrip(&reset,&captured,&decoded);
    assert(!decoded.field_orders.goal[0] && !decoded.field_orders.reward[0]);

    /* A copy (snapshot capture, spectator follow) carries the orders; a
     * connection reset clears only the live client. */
    {
        playerState_t *snapshot=&snapshots[0].frames[0].ps;
        *snapshot=*ps;
        assert(PlayerStateExtCopy(snapshot,ps));
        ResetClientFieldOrders(0);
        assert(!fo->goal[0] && !fo->progress[0] && !fo->status[1]);
        assert(PlayerStateExtConst(snapshot)->field_orders.goal[0]==3);
    }
    /* While dead, the shown state is the killer's (client 1, no orders): the
     * recipient still gets its own, so the HUD bar does not drain. A
     * spectator gets the followed player's. */
    {
        call(Scr_SetFOGoal,0,2,0,4);
        call(Scr_SetFOProgress,0,2,0,3);
        clients[0].sess.cs.team=TEAM_RED;
        memset(&wire,0,sizeof(wire));
        assert(OverlayFieldOrdersFor(&wire,&clients[1].ps,0));
        assert(wire_value(&wire,0x1b4)==4 && wire_value(&wire,0x1bc)==3);
        clients[0].sess.cs.team=TEAM_SPECTATOR;
        memset(&wire,0,sizeof(wire));
        assert(OverlayFieldOrdersFor(&wire,&clients[1].ps,0));
        assert(wire_value(&wire,0x1b4)==0 && wire_value(&wire,0x1bc)==0);
    }
    /* The killcam is SpectatorClientEndFrame's follow copy: the killer's
     * archived extension (a bot: no orders) goes over the victim's live one.
     * A victim on a team keeps its own orders through it and still takes the
     * rest of the archived state; a spectator takes the followed player's. */
    {
        ps_ext_t killer;
        call(Scr_SetFOGoal,0,2,0,6);
        call(Scr_SetFOProgress,0,2,0,1);
        call(Scr_SetFOReward,0,2,0,50);
        killer=*PlayerStateExtConst(&clients[1].ps);
        memset(&killer.field_orders,0,sizeof(killer.field_orders));
        killer.stun_time=1234;
        clients[0].sess.cs.team=TEAM_BLUE;
        assert(PlayerStateExtApplyFollow(ps,&killer));
        assert(fo->goal[0]==6 && fo->progress[0]==1 && fo->reward[0]==50);
        assert(PlayerStateExtConst(ps)->stun_time==1234);
        /* Respawned: the next kill moves the bar of the same order. */
        call(Scr_SetFOProgress,0,2,0,2);
        memset(&wire,0,sizeof(wire));
        assert(OverlayFieldOrdersFor(&wire,ps,0));
        assert(wire_value(&wire,0x1b4)==6 && wire_value(&wire,0x1bc)==2);
        /* A spectator following the killer draws the killer's (none). */
        clients[0].sess.cs.team=TEAM_SPECTATOR;
        assert(PlayerStateExtApplyFollow(ps,&killer));
        assert(!fo->goal[0] && !fo->progress[0]);
        /* Untracked storage: refused, as Apply refuses it. */
        {
            playerState_t loose;
            memset(&loose,0,sizeof(loose));
            assert(!PlayerStateExtApplyFollow(&loose,&killer));
        }
    }
    puts("PASS: field order setters, wire publication, archive record and reset");
    return 0;
}

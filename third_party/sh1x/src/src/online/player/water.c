#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Wading water: T5's player water level, speed scale and stance/sprint locks.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * REFERENCE (OpenBLOPS, T5 bg_pmove.cpp): PM_SetWaterLevel samples
 * CM_GetWaterHeight(origin, 200, -200) every command and PM_GetWaterLevel
 * buckets the depth of the water over the feet:
 *     >= 60 -> 5, >= 54 -> 4, >= 45 -> 3, >= 24 -> 2, >= 3 -> 1, else 0.
 * The level then drives, all inside the engine (no script on Shi No Numa
 * does any of it; zombie_cod5_sumpf.gsc only sets phys_buoyancy):
 *   PM_CmdScale         scale *= 1 - (1 - 0.6) * waterlevel / 3
 *   PlayerProneAllowed  no prone at waterlevel >= 1
 *   PM_CheckDuck        a prone player at waterlevel >= 1 is raised to
 *                       crouch (stand at >= 3); a crouched player at >= 2
 *                       stands and a crouch request at >= 2 is refused
 *   PM_ShouldStopSprint / PM_CanSprint   no sprint at waterlevel >= 2
 * Level 3 and deeper is T5's swim move (PM_SwimMove). C1 has no swim move,
 * so the walk scale is held at its level-3 value, 0.6, below that.
 *
 * C1 (and so this server) has no water volumes or water level in its
 * playerstate. The effects are therefore published through the fields C1's
 * own pmove already predicts from: ps+254 moveSpeedScaleMultiplier and the
 * pm_flags deny-crouch / deny-prone / no-sprint-start bits that allowCrouch,
 * allowProne and allowSprint use (pmove_c1.c stance_deny_c1 rewrites a
 * denied prone/crouch to the nearest permitted stance, which is T5's raise).
 *
 * WATER SURFACE. T5's CM_GetWaterHeight reads its water simulation surfaces;
 * C1's maps carry none (measured on zm_sumpf_sh: CONTENTS_WATER at no point
 * of the swamp, the bullet trace lands on "mud" at the swamp floor). A level
 * script declares its water plane with codol_setLevelWaterHeight(z); the
 * plane belongs to the map that set it. */
#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "server.h"
#include "g_shared.h"
#include "player.h"
#include "sv_state.h"
#include "water.h"
#include <math.h>
#include <string.h>

#define WATER_PMF_DENY_CROUCH 0x01000000u
#define WATER_PMF_DENY_PRONE  0x02000000u
#define WATER_PMF_NO_SPRINT   0x00020000u   /* C1 allowSprint(false) bit */
#define WATER_PMF_SPRINTING   0x00004000u
#define WATER_PM_LASTSTAND    7

static struct {
    int set;
    float height;
    char map[64];
} level_water;

static struct {
    unsigned owned;     /* pm_flags bits the water set and still holds */
    int level;
    float scale;
} players[MAX_CLIENTS];

float WaterHeight(const float *pos)
{
    const char *map=ServerMapName();
    (void)pos;
    if(!level_water.set || !map || strcmp(map,level_water.map)) return WATER_NONE;
    return level_water.height;
}

/* T5 PM_GetWaterLevel. */
int WaterLevelForDepth(float depth)
{
    if(depth>=60.0f) return 5;
    if(depth>=54.0f) return 4;
    if(depth>=45.0f) return 3;
    if(depth>=24.0f) return 2;
    return depth>=3.0f;
}

static void release(unsigned client,playerState_t *ps)
{
    ps->pm_flags&=~(int)players[client].owned;
    players[client].owned=0;
    players[client].level=0;
    players[client].scale=1.0f;
}

float UpdatePlayerWater(unsigned client,playerState_t *ps)
{
    float height,scale;
    unsigned want=0,flags,add;
    int wl,live;
    if(client>=MAX_CLIENTS || !ps) return 1.0f;
    /* Something else (spawn, a script allow*(true)) cleared a held bit: the
     * water no longer holds it and sets it afresh below if still wanted. */
    players[client].owned&=(unsigned)ps->pm_flags;
    live=level.clients && client<(unsigned)level.maxclients &&
         level.clients[client].sess.sessionState==SESS_STATE_PLAYING &&
         (ps->pm_type==0 || ps->pm_type==WATER_PM_LASTSTAND);
    height=live ? WaterHeight(ps->origin) : WATER_NONE;
    if(height==WATER_NONE) { release(client,ps); return 1.0f; }
    wl=WaterLevelForDepth(height-ps->origin[2]);
    if(wl>=1) want|=WATER_PMF_DENY_PRONE;
    if(wl>=2) want|=WATER_PMF_DENY_CROUCH|WATER_PMF_NO_SPRINT;
    flags=(unsigned)ps->pm_flags;
    /* Release what is no longer wanted, take what is wanted and free. A bit a
     * script set first stays the script's. */
    flags&=~(players[client].owned&~want);
    players[client].owned&=want;
    add=want&~flags;
    flags|=add;
    players[client].owned|=add;
    /* T5 PM_ShouldStopSprint: deep water ends a sprint in progress; C1's
     * no-sprint bit only refuses a new one (pmove_c1.c update_sprint). */
    if(wl>=2 && (flags&WATER_PMF_SPRINTING)) {
        flags&=~WATER_PMF_SPRINTING;
        ps->sprintState.sprintDelay=0;
        ps->sprintState.lastSprintEnd=ps->commandTime;
        ps->sprintState.sprintButtonUpRequired=1;
    }
    ps->pm_flags=(int)flags;
    scale=1.0f-(1.0f-0.6f)*(float)(wl<3 ? wl : 3)/3.0f;
    players[client].level=wl;
    players[client].scale=scale;
    return scale;
}

void WaterYieldBits(playerState_t *ps,unsigned bits)
{
    unsigned client;
    if(!ps || !level.clients) return;
    client=(unsigned)(((gclient_t *)ps)-level.clients);
    if(client>=MAX_CLIENTS || client>=(unsigned)level.maxclients ||
       &level.clients[client].ps!=ps) return;
    players[client].owned&=~bits;
}

int PlayerWaterLevel(unsigned client,float *scale)
{
    if(client>=MAX_CLIENTS) { if(scale) *scale=1.0f; return 0; }
    if(scale) *scale=players[client].level ? players[client].scale : 1.0f;
    return players[client].level;
}

/* codol_setLevelWaterHeight( <z> ) / codol_setLevelWaterHeight(): declare
 * (or remove) the current map's water plane. */
void Scr_SetLevelWaterHeight(void)
{
    const char *map=ServerMapName();
    float z;
    if(Scr_GetNumParam()==0) { level_water.set=0; return; }
    if(Scr_GetNumParam()!=1) { Scr_Error("Usage: codol_setLevelWaterHeight( <height> )"); return; }
    z=Scr_GetFloat(0);
    if(!isfinite(z)) { Scr_ParamError(0,"water height must be finite"); return; }
    if(!map) { Scr_Error("codol_setLevelWaterHeight: no map is loaded"); return; }
    level_water.height=z;
    strncpy(level_water.map,map,sizeof(level_water.map)-1);
    level_water.map[sizeof(level_water.map)-1]=0;
    level_water.set=1;
}

/* T5 GScr_GetWaterHeight: getWaterHeight( <position> ). */
void Scr_GetWaterHeight(void)
{
    vec3_t pos;
    if(Scr_GetNumParam()!=1) { Scr_Error("Usage: getWaterHeight( <position> )"); return; }
    Scr_GetVector(0,pos);
    Scr_AddFloat(WaterHeight(pos));
}

/* T5 GScr_DepthInWater: <entity> depthInWater(), the water over the entity's
 * origin, never negative. */
void Scr_DepthInWater(scr_entref_t ref)
{
    gentity_t *ent;
    float depth;
    if(ref.classnum || ref.entnum>=MAX_GENTITIES || !g_entities[ref.entnum].r.inuse) {
        Scr_ObjectError("depthInWater requires an entity"); return;
    }
    ent=&g_entities[ref.entnum];
    depth=WaterHeight(ent->r.currentOrigin)-ent->r.currentOrigin[2];
    Scr_AddFloat(depth<0.0f ? 0.0f : depth);
}
#endif

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

void PersistentWaterReset(void) {
    /* Only called after VM/entities are gone; never touch dead playerstates.
     * The multiplier's neutral value is one, not zero. */
    memset(&level_water,0,sizeof(level_water));
    memset(players,0,sizeof(players));
    for(unsigned i=0;i<MAX_CLIENTS;++i)players[i].scale=1.0f;
}

int PersistentWaterEmpty(void){
    if(!resident_entity_zero(&level_water,sizeof(level_water)))return 0;
    for(unsigned i=0;i<MAX_CLIENTS;++i)
        if(players[i].owned||players[i].level||players[i].scale!=1.0f)return 0;
    return 1;}
#endif

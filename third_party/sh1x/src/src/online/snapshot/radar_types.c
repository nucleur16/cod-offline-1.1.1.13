#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* SPDX-License-Identifier: AGPL-3.0-or-later
 * C1 ps224/228 spyplane/satellite types; T5 authoritative input precedence.
 * Script inputs retain integers. Only the captured wire types use two bits. */
#ifdef OL_SERVER
#include "radar_types.h"
#include "playerstate_ext.h"
#include "scr_vm_functions.h"
#include "cscr_stringlist.h"
#include "g_shared.h"
#include <string.h>
static int client_types[MAX_CLIENTS][3];
static int team_types[TEAM_NUM_TEAMS][2];
int GetClientRadarType(unsigned client,unsigned satellite)
{ return client<MAX_CLIENTS && satellite<3 ? client_types[client][satellite]:0; }
void SetClientRadarType(unsigned client,unsigned satellite,int value)
{ if(client<MAX_CLIENTS && satellite<3)client_types[client][satellite]=value; }
void ResetClientRadarTypes(unsigned client)
{
    ps_ext_t *ext;
    if(client>=MAX_CLIENTS)return;
    memset(client_types[client],0,sizeof(client_types[client]));
    /* C1 ClientSpawn50f31b initializes client+35a8 to1. Scripts may disable
     * it independently of whether the currently held weapon has a sensor. */
    client_types[client][2]=1;
    if(level.clients && (ext=PlayerStateExt(&level.clients[client].ps)))
        memset(&ext->radar_types,0,sizeof(ext->radar_types));
}
void ResetAllRadarTypes(void)
{
    unsigned client;
    memset(client_types,0,sizeof(client_types));
    for(client=0;client<MAX_CLIENTS;++client)client_types[client][2]=1;
    memset(team_types,0,sizeof(team_types));
}
static int team_arg(void)
{
    unsigned short s=(unsigned short)Scr_GetConstString(0);
    if(s==scr_const.allies)return TEAM_BLUE;
    if(s==scr_const.axis)return TEAM_RED;
    if(s==scr_const.none)return TEAM_FREE;
    Scr_ParamError(0,"team must be allies, axis, or none");return -1;
}
static void set_team(unsigned kind)
{
    int team;
    if(Scr_GetNumParam()!=2){Scr_Error("team radar setter requires team and type");return;}
    team=team_arg();if(team>=0)team_types[team][kind]=Scr_GetInt(1);
}
static void get_team(unsigned kind)
{
    int team;
    if(Scr_GetNumParam()!=1){Scr_Error("team radar getter requires team");return;}
    team=team_arg();if(team>=0)Scr_AddInt(team_types[team][kind]);
}
void Scr_SetTeamSpyplane(void){set_team(0);}
void Scr_GetTeamSpyplane(void){get_team(0);}
void Scr_SetTeamSatellite(void){set_team(1);}
void Scr_GetTeamSatellite(void){get_team(1);}
void PublishClientRadarTypes(unsigned client)
{
    gclient_t *player;ps_ext_t *ext;int team,spy,sat;
    if(client>=MAX_CLIENTS || !(player=g_entities[client].client))return;
    if(!(ext=PlayerStateExt(&player->ps)))return;
    team=(int)player->sess.cs.team;
    spy=(unsigned)team<TEAM_NUM_TEAMS?team_types[team][0]:0;
    sat=(unsigned)team<TEAM_NUM_TEAMS?team_types[team][1]:0;
    /* OpenBLOPS ClientEndFrame: nonzero individual input overrides team type.
     * It is neither boolean OR nor maximum of the two type values. */
    if(client_types[client][0])spy=client_types[client][0];
    if(client_types[client][1])sat=client_types[client][1];
    /* Existing IW4-script hasRadar/setTeamRadar requests a normal spyplane.
     * This explicit script compatibility input never aliases satellite. */
    if(!spy && player->ps.radarEnabled)spy=1;
    ext->radar_types.spyplane=(uint8_t)((unsigned)spy&3u);
    ext->radar_types.satellite=(uint8_t)((unsigned)sat&3u);
    ext->radar_types.motion_tracker=client_types[client][2]!=0;
}
int OverlayRadarTypes(player_fields_t *wire,const playerState_t *ps)
{
    const ps_ext_t *ext;player_fields_t next;unsigned i,found=0;
    if(!wire || !ps || !(ext=PlayerStateExtConst(ps)))return 0;
    if(ext->radar_types.spyplane>3 || ext->radar_types.satellite>3 ||
       ext->radar_types.motion_tracker>1)return 0;
    next=*wire;
    for(i=0;i<PLAYER_FIELDS;++i){
        const player_field_t *f=OL_PlayerField(i);if(!f)return 0;
        if(f->offset==0x224){next.field[i]=ext->radar_types.spyplane;found|=1;}
        if(f->offset==0x228){next.field[i]=ext->radar_types.satellite;found|=2;}
        /* C1 451f1d/451f39 and45241c/452425 require both the composed
         * weapon's sensor capability and this independent player enable bit. */
        if(f->offset==0x22c){next.field[i]=ext->radar_types.motion_tracker;found|=4;}
    }
    if(found!=7)return 0;
    *wire=next;return 1;
}
#endif

#ifdef OL_SERVER
#include "persistent_room.h"
static int resident_room_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}
int PersistentRadarTypesEmpty(void) {for(unsigned i=0;i<MAX_CLIENTS;++i)
            if(client_types[i][0]||client_types[i][1]||client_types[i][2]!=1)return 0;
        return resident_room_zero(team_types,sizeof(team_types));}

#endif

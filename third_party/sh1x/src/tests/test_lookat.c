#include "g_shared.h"
#include "server.h"
#include "q_math.h"
#include "lookat.h"
#include "sentient.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
serverStatic_t svs;
static gclient_t viewer,target;
static trace_t hit;
static cvar_t red,friendly,colour,debug;
static int debug_lines;
cvar_t *g_redCrosshairs=&red;
static int actor_team=SENTIENT_TEAM_AXIS,has_team=1;
int Sentient_TeamForEntity(gentity_t *ent,int *team)
{
    if(ent==&g_entities[0]){*team=SENTIENT_TEAM_ALLIES;return 1;}
    if(!has_team)return 0;*team=actor_team;return 1;
}
void G_GetPlayerViewOrigin(playerState_t *ps,float *v){(void)ps;memset(v,0,12);}
void AngleVectors(const float *angles,float *f,float *r,float *u)
{(void)angles;(void)r;(void)u;f[0]=1;f[1]=f[2]=0;}
void G_TraceCapsule(trace_t *tr,const float *start,const float *mins,const float *maxs,
                    const float *end,int pass,int mask)
{(void)start;(void)mins;(void)maxs;(void)end;(void)pass;(void)mask;*tr=hit;}
cvar_t *Cvar_RegisterString(const char *name,const char *value,unsigned short flags,const char *desc)
{(void)name;(void)flags;(void)desc;colour.string=(char *)value;return &colour;}
cvar_t *Cvar_RegisterBool(const char *name,qboolean value,unsigned short flags,const char *desc)
{(void)name;(void)value;(void)flags;(void)desc;return !strcmp(name,"lookatDebug")?&debug:&friendly;}
void SV_GameSendServerCommand(int client,int type,const char *cmd){(void)client;(void)type;(void)cmd;}
char *va(const char *format,...){static char b[256];va_list ap;va_start(ap,format);vsnprintf(b,sizeof(b),format,ap);va_end(ap);return b;}
void Com_Printf(conChannel_t channel,const char *format,...){(void)channel;(void)format;debug_lines++;}
int main(void)
{
    gentity_t *actor=&g_entities[100];
    level.maxclients=2;red.boolean=1;friendly.boolean=1;
    g_entities[0].r.inuse=1;g_entities[0].health=100;g_entities[0].client=&viewer;
    viewer.sess.sessionState=SESS_STATE_PLAYING;viewer.sess.cs.team=TEAM_BLUE;
    actor->r.inuse=1;actor->health=100;actor->s.eType=ET_ACTOR;
    hit.fraction=.1f;hit.hitType=TRACE_HITTYPE_ENTITY;hit.hitId=100;
    UpdateLookAtEntity(0);assert(viewer.ps.weapFlags&WEAP_CROSSHAIR_ENEMY);
    /* Incapacitated soldiers remain live actors with their actual remaining
     * health: even one HP stays hostile until the real lethal hit. */
    actor->health=1;UpdateLookAtEntity(0);
    assert(viewer.ps.weapFlags&WEAP_CROSSHAIR_ENEMY);
    actor->health=100;
    actor_team=SENTIENT_TEAM_ALLIES;
    UpdateLookAtEntity(0);assert(viewer.ps.weapFlags&WEAP_CROSSHAIR_FRIENDLY);
    actor_team=0;UpdateLookAtEntity(0);assert(!(viewer.ps.weapFlags&0x18));
    viewer.sess.cs.team=TEAM_FREE;actor_team=SENTIENT_TEAM_FOUR;
    UpdateLookAtEntity(0);assert(viewer.ps.weapFlags&WEAP_CROSSHAIR_ENEMY);
    viewer.sess.cs.team=TEAM_BLUE;
    actor_team=1;actor->health=0;UpdateLookAtEntity(0);assert(!(viewer.ps.weapFlags&0x18));
    actor->health=100;actor->r.svFlags=SVF_NOCLIENT;
    UpdateLookAtEntity(0);assert(!(viewer.ps.weapFlags&0x18));
    actor->r.svFlags=0;actor->s.eType=ET_ACTOR_CORPSE;
    UpdateLookAtEntity(0);assert(!(viewer.ps.weapFlags&0x18));
    actor->s.eType=ET_ACTOR;has_team=0;
    UpdateLookAtEntity(0);assert(!(viewer.ps.weapFlags&0x18));
    has_team=1;red.boolean=0;
    UpdateLookAtEntity(0);assert(!(viewer.ps.weapFlags&0x18));
    red.boolean=1;hit.hitId=1;
    g_entities[1].r.inuse=1;g_entities[1].health=100;g_entities[1].client=&target;
    target.sess.sessionState=SESS_STATE_PLAYING;target.sess.cs.team=TEAM_RED;
    UpdateLookAtEntity(0);assert(viewer.ps.weapFlags&WEAP_CROSSHAIR_ENEMY);
    hit.fraction=1;UpdateLookAtEntity(0);assert(!(viewer.ps.weapFlags&0x18));
    assert(debug_lines==0); /* Diagnostic is genuinely default-off. */
    debug.boolean=1;level.time=1000;svs.clients[0].netchan.remoteAddress.type=NA_IP;
    UpdateLookAtEntity(0);assert(debug_lines==1);
    UpdateLookAtEntity(0);assert(debug_lines==1);
    level.time=2000;UpdateLookAtEntity(0);assert(debug_lines==2);
    svs.clients[0].netchan.remoteAddress.type=NA_BOT;
    level.time=3000;UpdateLookAtEntity(0);assert(debug_lines==2);
    puts("PASS: actor sentient friend/foe targeting, dead/hidden/neutral/corpse rejection, player targeting and target loss");
    return 0;
}

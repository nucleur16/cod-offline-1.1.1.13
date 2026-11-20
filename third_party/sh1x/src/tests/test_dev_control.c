#include "server.h"
#include "g_shared.h"
#include "qcommon.h"
#include "cmd.h"
#include "qcommon_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
server_t sv;
serverStatic_t svs;
gentity_t g_entities[MAX_GENTITIES];
static gclient_t client;
static cvar_t running,limit,cheats;
cvar_t *com_sv_running=&running,*sv_maxclients=&limit,*g_cheats=&cheats;
static int taint_calls,target_state,target_calls;
int CheatsTainted(void){++taint_calls;return 1;}
int Sentient_SetDontTargetForEntity(gentity_t *ent,int on){assert(ent==&g_entities[0]);target_state=on;++target_calls;return 1;}
static char *args[]={"dev_player","0","god","on"};
int Cmd_Argc(void){return 4;}
char *Cmd_Argv(int i){return args[i];}
void Com_Printf(conChannel_t c,const char *fmt,...){(void)c;assert(fmt);}
void DevPlayer_f(void);
int main(void)
{
 running.integer=1;limit.integer=2;sv.state=SS_GAME;
 svs.clients[0].state=CS_ACTIVE;svs.clients[0].netchan.remoteAddress.type=NA_IP;g_entities[0].client=&client;g_entities[0].health=100;
 DevPlayer_f();assert(!g_entities[0].flags); /* cheats off */
 cheats.boolean=1;DevPlayer_f();assert(g_entities[0].flags==1);
 args[2]="notarget";DevPlayer_f();assert(g_entities[0].flags==5 && target_calls==1 && target_state && taint_calls==2);
 args[2]="noclip";DevPlayer_f();assert(client.noclip);
 args[3]="toggle";DevPlayer_f();assert(!client.noclip);
 args[2]="god";args[3]="off";cheats.boolean=0;g_entities[0].health=0;
 DevPlayer_f();assert(g_entities[0].flags==4); /* disabling remains possible */
 args[3]="on";cheats.boolean=1;DevPlayer_f();assert(g_entities[0].flags==4);
 g_entities[0].health=100;svs.clients[0].netchan.remoteAddress.type=NA_BOT;
 DevPlayer_f();assert(g_entities[0].flags==4); /* operator player controls human only */
 svs.clients[0].netchan.remoteAddress.type=NA_IP;svs.clients[0].state=CS_CONNECTED;
 DevPlayer_f();assert(g_entities[0].flags==4);
 svs.clients[0].state=CS_ACTIVE;args[1]="2";DevPlayer_f();assert(g_entities[0].flags==4);
 args[1]="0;quit";DevPlayer_f();assert(g_entities[0].flags==4);
 args[1]="0";args[2]="arbitrary";DevPlayer_f();assert(g_entities[0].flags==4);
 args[2]="god";args[3]="invalid";DevPlayer_f();assert(g_entities[0].flags==4);
 args[2]="notarget";args[3]="off";DevPlayer_f();assert(!g_entities[0].flags && target_calls==2 && !target_state);
 puts("PASS: actual operator player controls validate active human/slot/verb/mode/cheats and support safe disabling");
}

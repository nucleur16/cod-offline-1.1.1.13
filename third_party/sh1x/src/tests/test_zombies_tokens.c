/* Production token state machine; engine/VM/account transport are explicit doubles. */
#include "server.h"
#include "g_shared.h"
#include "revive_token.h"
#include "connect.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
serverStatic_t svs;
server_t sv;
level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
static gclient_t player;
#define clients (svs.clients)
static cvar_t maxclients, running;
cvar_t *sv_maxclients=&maxclients, *com_sv_running=&running;
static uint64_t uid=0x1234, profile_uid=0x1234;
static int laststand=1, result, requests, notifications, errors;
static const char *args[3];
uint64_t ClientConnectIdentity(unsigned slot){assert(!slot);return uid;}
const uint8_t *ClientSecureProfile(unsigned slot,uint64_t *trusted){static uint8_t p;assert(!slot);*trusted=profile_uid;return &p;}
qboolean LastStand_IsActive(const gentity_t *e){assert(e==g_entities);return laststand;}
void Scr_AddInt(int n){result=n;}
void Scr_ObjectError(const char *s){assert(s);++errors;}
int Cmd_Argc(void){return 3;}
char *Cmd_Argv(int i){return (char *)args[i];}
void SV_GameSendServerCommand(int slot,int kind,const char *s){assert(!slot&&kind==1&&!strcmp(s,"9 consume token"));++requests;}
unsigned SL_GetString(const char *s,int type){assert(!strcmp(s,"revive_token_consumed")&&!type);return 7;}
void SL_RemoveRefToString(unsigned n){assert(n==7);}
void Scr_Notify(gentity_t *e,unsigned short n,unsigned count){assert(e==g_entities&&n==7&&!count);++notifications;}
void Com_Printf(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
static void inventory(int n){static char count[16];snprintf(count,sizeof(count),"%d",n);args[0]="token_inventory";args[1]="0000000000001234";args[2]=count;TokenInventory_f();}
static void receipt(const char *id){args[0]="token_consumed";args[1]="0000000000001234";args[2]=id;TokenConsumed_f();}
int main(void)
{
 scr_entref_t ref={0,0};maxclients.integer=1;running.integer=1;sv.state=SS_GAME;
 clients[0].state=CS_ACTIVE;clients[0].netchan.remoteAddress.type=NA_IP;
 g_entities[0].client=&player;g_entities[0].r.inuse=1;g_entities[0].health=100;level.time=1000;
 inventory(3);Scr_GetReviveTokenCount(ref);assert(result==3);
 Scr_ArmReviveToken(ref);assert(result==1);Scr_RequestReviveTokenSpend(ref);assert(result==1&&requests==1);
 Scr_RequestReviveTokenSpend(ref);assert(!result&&requests==1);
 receipt("0000000000000001");assert(notifications==1);receipt("0000000000000001");assert(notifications==1);
 Scr_GetReviveTokenCount(ref);assert(result==2);
 /* A stale receipt cannot authorize a new down; a fresh request and receipt can. */
 Scr_ArmReviveToken(ref);Scr_RequestReviveTokenSpend(ref);receipt("0000000000000001");assert(notifications==1);
 receipt("0000000000000002");assert(notifications==2);
 Scr_ArmReviveToken(ref);Scr_RequestReviveTokenSpend(ref);level.time+=60001;
 receipt("0000000000000003");assert(notifications==2);
 level.time=70000;Scr_ArmReviveToken(ref);Scr_ClearReviveToken(ref);Scr_RequestReviveTokenSpend(ref);assert(!result);
 laststand=0;Scr_ArmReviveToken(ref);assert(!result);laststand=1;
 profile_uid=99;Scr_GetReviveTokenCount(ref);assert(!result);Scr_ArmReviveToken(ref);assert(!result);profile_uid=uid;
 clients[0].netchan.remoteAddress.type=NA_BOT;Scr_ArmReviveToken(ref);assert(!result);clients[0].netchan.remoteAddress.type=NA_IP;
 ResetReviveTokenClient(0);Scr_GetReviveTokenCount(ref);assert(!result);
 ref.classnum=1;Scr_ArmReviveToken(ref);assert(errors==1);
 puts("PASS production revive token ownership, request/receipt, bounded duplicate, timeout, cancellation, bot/account and reset guards");
}

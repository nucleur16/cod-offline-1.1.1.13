/* Execute the real new upstream recovery state machine. Test doubles are
 * explicit: dvar lookup/registration, clock, packet sleep, logging and command
 * buffer. This does not execute a server, RCON, a VM, or a level loader. */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "/src/src/codol/net/codol_server_recovery.c"

static cvar_t running, dedicated, delay, map, session, category, gametype;
cvar_t *com_sv_running = &running;
cvar_t *com_dedicated = &dedicated;
static unsigned now;
static unsigned sleeps, commands;
static int load_on_sleep;
static char last_command[2048];
static unsigned checks;
static void check(int ok,const char *what){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",what);exit(1);}}
unsigned Sys_Milliseconds(void){return now;}
__optimize3 __regparm1 qboolean NET_Sleep(unsigned usec){check(usec==20000,"idle packet wait retained");++sleeps;if(load_on_sleep)running.integer=1;return qtrue;}
void QDECL Com_Printf(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
void Q_strncpyz(char *out,const char *in,int size){check(size>0,"bounded dvar copy");snprintf(out,(size_t)size,"%s",in);}
char *va(const char *fmt,...){static char buffer[2048];va_list args;va_start(args,fmt);vsnprintf(buffer,sizeof(buffer),fmt,args);va_end(args);return buffer;}
void Cbuf_AddText(const char *text){++commands;snprintf(last_command,sizeof(last_command),"%s",text);}
cvar_t *Cvar_FindVar(const char *name){
 if(!strcmp(name,"mapname"))return &map;
 if(!strcmp(name,"codol_sessionMap"))return &session;
 if(!strcmp(name,"codol_zoneType"))return &category;
 if(!strcmp(name,"g_gametype"))return &gametype;
 return NULL;
}
cvar_t *Cvar_RegisterInt(const char *name,int value,int minimum,int maximum,unsigned short flags,const char *description){
 check(!strcmp(name,"codol_recoverDelay")&&value==2000&&minimum==0&&maximum==600000&&flags==0&&description[0],"upstream recovery dvar contract");
 delay.integer=value;return &delay;
}
static void reset(void){
 memset(&recovery,0,sizeof(recovery));codol_recoverDelay=NULL;
 running.integer=0;dedicated.integer=1;load_on_sleep=0;now=100;sleeps=commands=0;last_command[0]=0;
 map.string="zm_prototype_sh";session.string="zm_prototype_sh";category.string="zm";gametype.string="zombietron";
}
static void tick(unsigned time){now=time;CODOL_ServerRecoveryIdleFrame();}
int main(void){
 reset();tick(100000);check(!commands&&!recovery.valid,"never loaded server does not invent a map");
 reset();CODOL_ServerRecoveryLevelLoaded();check(recovery.valid&&!recovery.operatorStop&&!recovery.downSince&&!recovery.attempts,"level loaded arms clean recovery");
 tick(100);check(recovery.nextAttempt==2100&&!commands,"first error schedules two-second retry");
 tick(2099);check(!commands,"no early reload");tick(2100);check(commands==1&&recovery.attempts==1&&recovery.nextAttempt==6100,"first retry schedules four-second backoff");
 check(!strcmp(last_command,"set fs_game zm\nset codol_sessionMap zm_prototype_sh\nset codol_zoneType zm\nset g_gametype zombietron\nmap zm_prototype_sh\n"),"same remembered category/session/gametype/map dispatch");
 tick(6099);check(commands==1,"no early second reload");tick(6100);check(commands==2&&recovery.nextAttempt==14100,"second retry doubles backoff");
 check(retry_delay(4)==32000&&retry_delay(5)==60000&&retry_delay(30)==60000,"retry backoff capped at sixty seconds");
 CODOL_ServerRecoveryOperatorStop();tick(100000);check(commands==2&&recovery.operatorStop,"operator killserver prevents recovery");
 CODOL_ServerRecoveryLevelLoaded();check(!recovery.operatorStop&&!recovery.attempts&&!recovery.downSince,"new explicit map re-arms after operator stop");
 tick(100000);load_on_sleep=1;tick(102000);check(commands==2&&running.integer,"lobby map loaded while waiting wins over recovery");
 reset();CODOL_ServerRecoveryLevelLoaded();dedicated.integer=0;tick(100);tick(10000);check(!commands&&!recovery.downSince,"non-dedicated server does not reload");
 reset();CODOL_ServerRecoveryLevelLoaded();delay.integer=0;tick(100);tick(10000);check(!commands&&!recovery.downSince,"zero recovery delay disables automatic reload");
 const char *bad[]={"zm_bad;quit","zm_bad\nquit","zm_bad\rquit","zm_bad\"","zm bad"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i){reset();map.string=(char*)bad[i];CODOL_ServerRecoveryLevelLoaded();tick(100000);check(!recovery.valid&&!commands,"unsafe remembered map cannot dispatch commands");}
 reset();map.string="mp_killhouse_sh";session.string="";category.string="";gametype.string="";CODOL_ServerRecoveryLevelLoaded();tick(100);tick(2100);
 check(!strcmp(last_command,"set fs_game mp\nset codol_sessionMap mp_killhouse_sh\nset codol_zoneType mp\nset g_gametype dm\nmap mp_killhouse_sh\n"),"missing optional dvars use upstream defaults");
 printf("server recovery: %u checks PASS (explicit engine/clock/command doubles)\n",checks);return 0;
}

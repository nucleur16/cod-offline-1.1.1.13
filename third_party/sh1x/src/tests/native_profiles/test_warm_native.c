/* Actual warm adapter/policy with bounded engine providers, not a booted server.
 * Hash provider uses retained mbedTLS SHA256. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "warm_policy.h"
#include "warm_standby.h"
#include "server.h"
#include "cvar.h"
#include "cmd.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "sec_crypto.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

server_t sv;
serverStatic_t svs;
static cvar_t running,maximum,preparation;
cvar_t *com_sv_running=&running,*sv_maxclients=&maximum;
unsigned long long com_uFrameTime;
int SEC_HASH_SHA256=1;
static xcommand_t command;
static char *args[4],message[512],difficulty[]="0";
static int ticket,sleeps,last_sleep;
static unsigned checks;
static const char *prep="0123456789abcdef0123456789abcdef";
static const char *lease="11111111111111111111111111111111";
#define CHECK(name,condition) do { ++checks; if (!(condition)) { fprintf(stderr,"FAIL %s:%d\n",name,__LINE__);abort(); } } while(0)

extern int TestWarmFrameGate(void);
extern unsigned long long test_clock_stamp_us;

char *Cvar_GetVariantString(const char *name)
{
    if (!strcmp(name,"mapname") || !strcmp(name,"sv_sessionMap")) return "mp_rust";
    if (!strcmp(name,"sv_zoneType")) return "mp";
    if (!strcmp(name,"g_gametype")) return "dm";
    if (!strcmp(name,"g_difficulty")) return difficulty;
    return "";
}
cvar_t *Cvar_RegisterString(const char *n,const char *v,unsigned short flags,const char *help)
{
    (void)v;(void)help;
    CHECK("immutable preparation setting",!strcmp(n,"warmPreparation") && flags==CVAR_INIT);
    preparation.string=(char *)prep;
    return &preparation;
}
int Cvar_VariableIntegerValue(const char *name)
{ assert(!strcmp(name,"net_port"));return 27733; }
qboolean Sec_HashMemory(int algorithm,void *input,size_t length,void *output,unsigned long *size,qboolean binary)
{
    unsigned i;unsigned char digest[32];char *text=output;
    assert(algorithm==SEC_HASH_SHA256 && !binary && *size>=65);
    assert(!mbedtls_sha256_ret(input,length,digest,0));
    for (i=0;i<32;++i) snprintf(text+i*2,3,"%02x",digest[i]);
    *size=64;return qtrue;
}
qboolean Cmd_AddCommand(const char *name,xcommand_t fn)
{ assert(!strcmp(name,"sh1x_warm_activate"));command=fn;return qtrue; }
int Cmd_Argc(void) { return 4; }
char *Cmd_Argv(int n) { assert(n>=0 && n<4);return args[n]; }
void Com_Printf(conChannel_t channel,const char *format,...)
{ va_list a;(void)channel;va_start(a,format);vsnprintf(message,sizeof(message),format,a);va_end(a); }
int AdmissionWarmTicket(unsigned port,const char *p,const char *l,uint64_t uid)
{ return ticket && port==27733 && !strcmp(p,prep) && !strcmp(l,lease) && uid==7; }
__regparm1 qboolean NET_Sleep(unsigned int usec) { ++sleeps;last_sleep=(int)usec;return qfalse; }

int main(int argc,char **argv)
{
    char digest[65];const char *p;int mode=argc>1 ? atoi(argv[1]) : 0;
    CHECK("disabled adapter never holds/admission unchanged",!WarmStandbyFrame() && WarmStandbyAdmit(0));
    running.integer=1;maximum.integer=4;sv.state=SS_GAME;
    WarmStandbyRegister();CHECK("registered real command",command!=NULL);
    WarmStandbyBeginMap();
    CHECK("loading refuses humans",!WarmStandbyAdmit(7));
    WarmStandbyMapReady();
    CHECK("prepared is not assigned",WarmStandbyUnassigned());
    p=strstr(WarmStandbyStatus(),"warm_selection=");assert(p);
    memcpy(digest,p+15,64);digest[64]=0;assert(WarmHex(digest,64));
    sv.timeResidual=900000;svs.timeResidual=999;svs.time=1234;com_uFrameTime=987654321;
    CHECK("actual frame boundary freezes",!TestWarmFrameGate());
    CHECK("residual and stamp reset, game clock unchanged",!sv.timeResidual && !svs.timeResidual &&
        svs.time==1234 && test_clock_stamp_us==com_uFrameTime && sleeps==1 && last_sleep==20000);
    args[0]="sh1x_warm_activate";args[1]=(char *)prep;args[2]=(char *)lease;args[3]=digest;
    if (mode==1) difficulty[0]='2';
    if (mode==2) { svs.clients[0].state=CS_PRIMED;svs.clients[0].netchan.remoteAddress.type=NA_IP; }
    if (mode==3) { WarmStandbyBeginMap();WarmStandbyMapReady(); }
    command();
    if (mode) {
        CHECK("stale changed occupied or reloaded assignment refused",strstr(message,"accepted=0") && !WarmStandbyAdmit(7));
        if (mode==2) CHECK("occupied server simulation preserved",TestWarmFrameGate());
        else CHECK("invalid empty preparation held",!TestWarmFrameGate());
    } else {
        CHECK("actual command assigns process",strstr(message,"accepted=1") && !WarmStandbyUnassigned());
        CHECK("assignment alone grants no admission",!WarmStandbyAdmit(7));
        ticket=1;
        CHECK("current authenticated uid also needs its ticket",WarmStandbyAdmit(7) && !WarmStandbyAdmit(8));
        sv.timeResidual=800000;svs.timeResidual=700;
        CHECK("resume barrier discards warm wall time",!TestWarmFrameGate() && !sv.timeResidual && !svs.timeResidual && last_sleep==0);
        CHECK("next ordinary frame proceeds",TestWarmFrameGate());
        svs.clients[0].state=CS_CONNECTED;svs.clients[0].netchan.remoteAddress.type=NA_IP;
        command();CHECK("lost ack retry preserves live game",strstr(message,"accepted=2") && TestWarmFrameGate());
        args[2]="22222222222222222222222222222222";command();
        CHECK("second lease cannot take world",strstr(message,"accepted=0") && WarmStandbyAdmit(7));
    }
    printf("PASS %u actual warm adapter/frame-gate checks mode%d\n",checks,mode);
    return 0;
}

/* Engine boundary for this warm-only fixture: resident mode stays disabled. */
int PersistentRuntimeFrame(void){return 0;}
int PersistentRuntimeEnabled(void){return 0;}
const char *PersistentEventsStatus(void){return "persistent_events=0";}
const char *PersistentRuntimeStatus(void){return "persistent=0";}

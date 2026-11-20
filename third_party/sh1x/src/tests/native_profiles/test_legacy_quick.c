/* Actual admission/status/lifecycle sources with a bounded engine test fixture.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "server.h"
#include "cvar.h"
#include "qcommon_io.h"
#include "connect.h"
#include "admission.h"
#include "quickplay.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

server_t sv;
serverStatic_t svs;
int onExitLevelExecuted;
static cvar_t running_var,max_var,map_var,session_var,mode_var,zone_var,roster_var,bool_var,warm_roster_var;
cvar_t *com_sv_running=&running_var,*sv_maxclients=&max_var;
static int enabled=1, playlist=120, port=27733, ended, held, reserved, blocked_slot=-1;
static char nonce[33]="0123456789abcdef0123456789abcdef", roster_path[256],status[4096];
static unsigned checks;
#define CHECK(label,expr) do { ++checks; if (!(expr)) { fprintf(stderr,"FAIL %s (line%d)\n",label,__LINE__); abort(); } } while(0)

cvar_t *Cvar_FindVar(const char *n)
{
    if (!strcmp(n,"mapname")) return &map_var;
    if (!strcmp(n,"sv_sessionMap")) return &session_var;
    if (!strcmp(n,"g_gametype")) return &mode_var;
    if (!strcmp(n,"sv_zoneType")) return &zone_var;
    return NULL;
}
cvar_t *Cvar_RegisterString(const char *n,const char *v,unsigned short f,const char *d)
{ (void)f;(void)d;
  if (!strcmp(n,"warmAdmissionPath")) { warm_roster_var.string=(char *)v; return &warm_roster_var; }
  assert(!strcmp(n,"admissionPath"));roster_var.string=roster_path;return &roster_var; }
cvar_t *Cvar_RegisterBool(const char *n,qboolean v,unsigned short f,const char *d)
{ (void)n;(void)v;(void)f;(void)d;return &bool_var; }
char *Cvar_GetVariantString(const char *n)
{ return !strcmp(n,"quickSession") ? nonce : !strcmp(n,"sv_zoneType") ? zone_var.string : ""; }
qboolean Cvar_VariableBooleanValue(const char *n) { return !strcmp(n,"sv_quickPlay") && enabled; }
int Cvar_VariableIntegerValue(const char *n)
{
    if (!strcmp(n,"sv_playlistId")) return playlist;
    if (!strcmp(n,"net_port")) return port;
    if (!strcmp(n,"scr_gameended")) return ended;
    if (!strcmp(n,"sv_privateClients")) return reserved;
    return 0;
}
int MatchReportTransitionHeld(void) { return held; }
int MatchReportCanReuseSlot(unsigned i) { return !held && (int)i!=blocked_slot; }
uint64_t ClientConnectIdentity(unsigned i) { return i==0 ? 1 : 0; }
void Com_Printf(conChannel_t c,const char *f,...)
{ va_list a;(void)c;va_start(a,f);vsnprintf(status,sizeof(status),f,a);va_end(a); }
void MatchStatus_f(void);

static void write_ticket(unsigned ep,const char *tail,long long expiry)
{
    char temporary[280];FILE *out;
    snprintf(temporary,sizeof(temporary),"%s.new",roster_path);
    out=fopen(temporary,"wb");assert(out);
    fprintf(out,"127.0.0.1 00112233445566778899aabbccddeeff 0000000000000001 %lld Local\n",expiry);
    fprintf(out,"quick 27733 %s %u 0000000000000001 %lld%s\n",nonce,ep,expiry,tail?tail:"");
    assert(!fclose(out));assert(!rename(temporary,roster_path));
}

int main(void)
{
    unsigned ep;FILE *f;uint8_t keys[3][16],ip[4]={127,0,0,1};
    long long later=(long long)time(NULL)+60;
    snprintf(roster_path,sizeof(roster_path),"/tmp/codol-quick-native-%ld.txt",(long)getpid());
    AdmissionInit();
    running_var.integer=1;max_var.integer=12;map_var.string="mp_killhouse_sh";
    session_var.string=map_var.string;mode_var.string="war";zone_var.string="mp";
    sv.state=SS_GAME;svs.clients[0].state=CS_ACTIVE;svs.clients[0].netchan.remoteAddress.type=NA_IP;
    enabled=0;CHECK("ordinary match needs no Quick ticket",QuickPlayAdmit(1));
    CHECK("ordinary status epoch0",QuickPlayEpoch()==0);enabled=1;
    CHECK("no spawned epoch is not joinable",!QuickPlayJoinable());
    QuickPlayBeginMap();ep=QuickPlayEpoch();CHECK("first epoch1",ep==1);
    CHECK("loading blocks connects",!QuickPlayJoinable());
    MatchStatus_f();CHECK("status reports loading closed",strstr(status,"quick=1 ")&&strstr(status,"epoch=1 joinable=0 "));
    QuickPlayMapReady();CHECK("ready epoch joinable",QuickPlayJoinable());
    CHECK("no grant rejects",!QuickPlayAdmit(1));
    write_ticket(ep,NULL,later);CHECK("current scoped grant accepts",QuickPlayAdmit(1));
    CHECK("grant is not a credential",AdmissionCandidates(ip,keys,3)==1&&keys[0][0]==0);
    CHECK("other uid rejects",!QuickPlayAdmit(2));
    port=27734;CHECK("other port rejects",!QuickPlayAdmit(1));port=27733;
    nonce[0]='f';CHECK("reused port/new lease nonce rejects",!QuickPlayAdmit(1));nonce[0]='0';
    ended=1;CHECK("postgame rejects",!QuickPlayAdmit(1));ended=0;
    ended=2;CHECK("round intermission rejects",!QuickPlayAdmit(1));ended=0;
    onExitLevelExecuted=1;CHECK("terminal native rejects",!QuickPlayAdmit(1));onExitLevelExecuted=0;
    held=1;CHECK("durability hold rejects",!QuickPlayAdmit(1));held=0;
    sv.state=SS_LOADING;CHECK("not running map rejects",!QuickPlayAdmit(1));sv.state=SS_GAME;
    QuickPlayBeginMap();CHECK("full map advances epoch",QuickPlayEpoch()==ep+1);
    CHECK("in-flight old ticket refused",!QuickPlayAdmit(1));
    QuickPlayMapReady();CHECK("old ticket still refused after ready",!QuickPlayAdmit(1));
    write_ticket(ep+1,NULL,later);CHECK("reissued ticket accepts",QuickPlayAdmit(1));
    CHECK("round restart retains epoch",QuickPlayEpoch()==ep+1);
    write_ticket(ep+1," extra",later);CHECK("extra field rejects",!QuickPlayAdmit(1));
    write_ticket(ep+1,NULL,time(NULL)-1);CHECK("expired rejects",!QuickPlayAdmit(1));
    write_ticket(ep+1,NULL,time(NULL)+121);CHECK("overlong TTL rejects",!QuickPlayAdmit(1));
    write_ticket(ep+1,NULL,later);CHECK("atomic refresh current accepts",QuickPlayAdmit(1));
    f=fopen(roster_path,"wb");assert(f);
    fprintf(f,"quick 27733 %s 4294967296 0000000000000001 %lld\n",nonce,later);
    fprintf(f,"quick 27733 %s -1 0000000000000001 %lld\n",nonce,later);
    fprintf(f,"quick 27733 %s 02 0000000000000001 %lld\n",nonce,later);
    fprintf(f,"quick 27733 %s 2 0000000000000000 %lld\n",nonce,later);
    fprintf(f,"quick 27733 %s 2 0000000000000001 9223372036854775808\n",nonce);
    fclose(f);CHECK("overflow/negative/noncanonical/zero UID reject",!QuickPlayAdmit(1));
    write_ticket(ep+1,NULL,later);
    playlist=8;CHECK("wrong playlist fail closed",!QuickPlayAdmit(1));playlist=120;
    zone_var.string="zm";CHECK("Zombies fail closed if misconfigured Quick",!QuickPlayAdmit(1));zone_var.string="mp";
    nonce[0]='A';CHECK("noncanonical nonce fail closed",!QuickPlayAdmit(1));nonce[0]='0';
    CHECK("no bot seat in human-only session",QuickPlayBotSeat()==-1);
    svs.clients[11].state=CS_ACTIVE;svs.clients[11].netchan.remoteAddress.type=NA_BOT;
    CHECK("full public bot seat offered",QuickPlayBotSeat()==11);
    svs.clients[10].state=CS_ACTIVE;svs.clients[10].netchan.remoteAddress.type=NA_BOT;
    blocked_slot=11;CHECK("unsettled bot seat skipped",QuickPlayBotSeat()==10);blocked_slot=-1;
    reserved=12;CHECK("reserved bots never removed",QuickPlayBotSeat()==-1);reserved=0;
    svs.clients[11].netchan.remoteAddress.type=NA_IP;
    CHECK("human never displaced",QuickPlayBotSeat()==10);
    svs.clients[10].state=CS_FREE;CHECK("no eligible bot means no displacement",QuickPlayBotSeat()==-1);
    enabled=0;CHECK("ordinary bot seats untouched",QuickPlayBotSeat()==-1);enabled=1;
    MatchStatus_f();CHECK("status nonce/epoch/joinable actual",strstr(status,"quick_session=0123456789abcdef0123456789abcdef epoch=2 joinable=1 "));
    unlink(roster_path);CHECK("removed roster invalidates old grants",!QuickPlayAdmit(1));
    printf("PASS: %u native Quick lifecycle/admission/status/bot-seat checks\n",checks);
    return 0;
}

/* Engine boundary for this warm-only fixture: resident mode stays disabled. */
int PersistentRuntimeFrame(void){return 0;}
int PersistentRuntimeEnabled(void){return 0;}
const char *PersistentEventsStatus(void){return "persistent_events=0";}
const char *PersistentRuntimeStatus(void){return "persistent=0";}

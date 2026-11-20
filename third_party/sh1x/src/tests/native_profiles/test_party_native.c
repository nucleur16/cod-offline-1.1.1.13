/* Actual production parser/cache/VM receiver/native assigned-team sources.
 * Engine fixture supplies only dvars, globals and script result/error sinks.
 * No sockets, actual clients, credentials, hooks or game services are used. */
#include "server.h"
#include "cvar.h"
#include "qcommon_io.h"
#include "admission.h"
#include "connect.h"
#include "quickplay.h"
#include "script_quick_party.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

server_t sv;
serverStatic_t svs;
gentity_t g_entities[MAX_GENTITIES];
int onExitLevelExecuted;
static cvar_t running_var,max_var,map_var,mode_var,zone_var,roster_var,bool_var;
cvar_t *com_sv_running=&running_var,*sv_maxclients=&max_var;
static int enabled=1,parties_enabled=1,playlist=120,port=27743,ended,held;
static char nonce[33]="0123456789abcdef0123456789abcdef";
static char path[256],status[4096];
static const char group_a[]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char group_b[]="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
static unsigned checks;
static int args,script_result=-1,script_results,script_errors;
#define CHECK(label,expr) do { ++checks; if (!(expr)) { fprintf(stderr,"FAIL %s line%d\n",label,__LINE__); abort(); } } while(0)

cvar_t *Cvar_FindVar(const char *n)
{
    if (!strcmp(n,"mapname") || !strcmp(n,"sv_sessionMap")) return &map_var;
    if (!strcmp(n,"g_gametype")) return &mode_var;
    if (!strcmp(n,"sv_zoneType")) return &zone_var;
    return NULL;
}
cvar_t *Cvar_RegisterString(const char *n,const char *v,unsigned short f,const char *d)
{ (void)n;(void)v;(void)f;(void)d;roster_var.string=path;return &roster_var; }
cvar_t *Cvar_RegisterBool(const char *n,qboolean v,unsigned short f,const char *d)
{ (void)n;(void)v;(void)f;(void)d;return &bool_var; }
char *Cvar_GetVariantString(const char *n)
{
    if (!strcmp(n,"quickSession")) return nonce;
    if (!strcmp(n,"sv_zoneType")) return zone_var.string;
    if (!strcmp(n,"g_gametype")) return mode_var.string;
    return "";
}
qboolean Cvar_VariableBooleanValue(const char *n)
{
    if (!strcmp(n,"sv_quickPlay")) return enabled;
    if (!strcmp(n,"quickParties")) return parties_enabled;
    return 0;
}
int Cvar_VariableIntegerValue(const char *n)
{
    if (!strcmp(n,"sv_playlistId")) return playlist;
    if (!strcmp(n,"net_port")) return port;
    if (!strcmp(n,"scr_gameended")) return ended;
    return 0;
}
int MatchReportTransitionHeld(void) { return held; }
int MatchReportCanReuseSlot(unsigned i) { (void)i;return 1; }
void Com_Printf(conChannel_t c,const char *f,...)
{ va_list a;(void)c;va_start(a,f);vsnprintf(status,sizeof(status),f,a);va_end(a); }
int Scr_GetNumParam(void) { return args; }
void Scr_AddInt(int value) { script_result=value;++script_results; }
void Scr_Error(const char *error) { (void)error;++script_errors; }
void Scr_ObjectError(const char *error) { (void)error;++script_errors; }
void MatchStatus_f(void);
int SV_GetAssignedTeam(int clientNum);

static FILE *begin_roster(int quick)
{
    char temporary[280];FILE *out;unsigned i;
    snprintf(temporary,sizeof(temporary),"%s.new",path);
    out=fopen(temporary,"wb");assert(out);
    if (quick)
        for (i=1;i<=20;++i)
            fprintf(out,"quick 27743 %s %u %016llx %lld\n",nonce,QuickPlayEpoch(),
                    (unsigned long long)i,(long long)time(NULL)+60);
    return out;
}
static void finish_roster(FILE *out)
{
    char temporary[280];assert(!fclose(out));
    snprintf(temporary,sizeof(temporary),"%s.new",path);assert(!rename(temporary,path));
}
static void row(FILE *out,unsigned uid,const char *group,unsigned team,long long expiry)
{
    fprintf(out,"party 27743 %s %016llx %s %u %lld\n",nonce,
            (unsigned long long)uid,group,team,expiry);
}
static void one_row(unsigned uid,const char *group,unsigned team,long long expiry,int quick)
{ FILE *out=begin_roster(quick);row(out,uid,group,team,expiry);finish_roster(out); }
static void identity(unsigned slot,uint64_t uid)
{ svs.clients[slot].netchan.remoteAddress.type=NA_IP;SetClientConnectIdentity(slot,uid); }

int main(void)
{
    FILE *out;unsigned i;char line[512];party_assignment_t assignment;
    long long later=(long long)time(NULL)+60;
    scr_entref_t ref={0};
    snprintf(path,sizeof(path),"/tmp/codol-party-native-%ld.txt",(long)getpid());
    AdmissionInit();running_var.integer=1;max_var.integer=12;
    sv.state=SS_GAME;zone_var.string="mp";mode_var.string="war";map_var.string="mp_killhouse_sh";
    QuickPlayBeginMap();QuickPlayMapReady();identity(0,1);
    CHECK("party capability active only in valid Quick",QuickPlayPartiesEnabled());
    CHECK("missing party allocation rejects even valid Quick ticket",(out=begin_roster(1),finish_roster(out),!QuickPlayAdmit(1)));
    one_row(1,group_a,1,later,0);CHECK("party row cannot replace Quick epoch ticket",!QuickPlayAdmit(1));
    one_row(1,group_a,1,later,1);CHECK("trusted UID and party row accepts",QuickPlayAdmit(1));
    CHECK("parser returns exact group and native axis side",AdmissionQuickParty(27743,nonce,1,&assignment)&&assignment.team==1&&!strcmp(assignment.group,group_a));
    CHECK("wrong UID cannot borrow group",!QuickPlayAdmit(2));
    CHECK("wrong authenticated slot UID cannot bind",!QuickPlayBindClient(0,2));
    CHECK("valid authenticated UID binds",QuickPlayBindClient(0,1));
    identity(3,4);svs.clients[3].netchan.remoteAddress.type=NA_BOT;
    one_row(4,group_b,2,later,1);CHECK("bot/unpublished source cannot bind",!QuickPlayBindClient(3,4));
    identity(3,0);one_row(1,group_a,1,later,1);
    CHECK("native team1 means axis",SV_GetAssignedTeam(0)==TEAM_RED&&QuickPlayClientTeam(0)==1);
    CHECK("group cached as exact master ID",!strcmp(QuickPlayClientGroup(0),group_a));
    port=27744;CHECK("party/Quick rows are exact-port scoped",!QuickPlayAdmit(1));port=27743;
    CHECK("invalid slot cannot bind",!QuickPlayBindClient(MAX_CLIENTS,1));
    CHECK("invalid native slot has no assigned side",SV_GetAssignedTeam(-1)==0&&SV_GetAssignedTeam(MAX_CLIENTS)==0);
    one_row(1,group_a,1,time(NULL)-1,1);CHECK("expired row rejects new admission",!QuickPlayAdmit(1));
    CHECK("connected allocation survives ticket expiry",QuickPlayClientTeam(0)==1);
    one_row(1,group_a,1,time(NULL)+121,1);CHECK("overlong TTL rejects",!QuickPlayAdmit(1));
    one_row(1,group_a,1,later,1);
    out=begin_roster(1);row(out,1,group_a,1,later);row(out,1,group_b,1,later);finish_roster(out);
    CHECK("conflicting UID groups fail closed",!QuickPlayAdmit(1));
    out=begin_roster(1);row(out,1,group_a,1,later);row(out,2,group_a,2,later);finish_roster(out);
    CHECK("one group cannot straddle sides",!QuickPlayAdmit(1)&&!QuickPlayAdmit(2));
    out=begin_roster(1);row(out,1,group_a,1,later);row(out,1,group_a,1,later);finish_roster(out);
    CHECK("identical duplicate rows remain unambiguous",QuickPlayAdmit(1));
    one_row(2,group_a,2,later,1);identity(1,2);
    CHECK("later roster cannot split admitted group",!QuickPlayBindClient(1,2)&&QuickPlayClientTeam(0)==1);
    one_row(2,group_a,1,later,1);CHECK("same group same side late join binds",QuickPlayBindClient(1,2));
    one_row(3,group_b,2,later,1);identity(2,3);CHECK("opposing cohort binds allies",QuickPlayBindClient(2,3)&&SV_GetAssignedTeam(2)==TEAM_BLUE);

    MatchStatus_f();CHECK("status explicitly advertises party allocation",strstr(status,"quick_parties=1 "));
    unlink(path);QuickPlayBeginMap();CHECK("map loading closes new admissions",!QuickPlayAdmit(1));
    CHECK("full game VM map reset preserves cached cohort",QuickPlayClientTeam(0)==1&&QuickPlayClientTeam(1)==1&&QuickPlayClientTeam(2)==2);
    QuickPlayMapReady();CHECK("missing/expired roster does not eject retained human",QuickPlayClientTeam(0)==1);
    mode_var.string="dm";CHECK("FFA native session team ignores assigned side",SV_GetAssignedTeam(0)==TEAM_FREE);
    CHECK("FFA keeps cohort for next team map",QuickPlayClientTeam(0)==1);
    mode_var.string="dom";QuickPlayBeginMap();QuickPlayMapReady();CHECK("next team map restores native allocated side",SV_GetAssignedTeam(0)==TEAM_RED);
    g_entities[0].r.inuse=1;g_entities[0].client=(gclient_t*)1;
    Scr_QuickPartyTeam(ref);CHECK("actual player method returns trusted side",script_results==1&&script_result==1&&!script_errors);
    args=1;Scr_QuickPartyTeam(ref);CHECK("actual method rejects arguments",script_errors==1&&script_results==1);args=0;
    ref.classnum=1;Scr_QuickPartyTeam(ref);CHECK("actual method rejects nonentity receiver",script_errors==2&&script_results==1);ref.classnum=0;
    ref.entnum=MAX_CLIENTS;Scr_QuickPartyTeam(ref);CHECK("actual method rejects out-of-range player",script_errors==3);ref.entnum=0;
    g_entities[0].r.inuse=0;Scr_QuickPartyTeam(ref);CHECK("actual method rejects inactive player",script_errors==4);g_entities[0].r.inuse=1;
    g_entities[0].client=NULL;Scr_QuickPartyTeam(ref);CHECK("actual method rejects nonplayer entity",script_errors==5);g_entities[0].client=(gclient_t*)1;

    QuickPlayClearClient(0);SetClientConnectIdentity(0,0);
    CHECK("disconnect clears group and team",!QuickPlayClientTeam(0)&&!QuickPlayClientGroup(0)[0]);
    identity(0,1);CHECK("same UID reconnect needs new allocation bind",!QuickPlayClientTeam(0));
    one_row(1,group_a,1,later,1);CHECK("new current-epoch grant rebinds",QuickPlayBindClient(0,1));
    identity(0,9);CHECK("unexpected slot UID drift invalidates cache",!QuickPlayClientTeam(0));
    identity(0,1);CHECK("returning UID does not resurrect stale cache",!QuickPlayClientTeam(0));
    one_row(1,group_a,1,later,1);CHECK("fresh bind restores only validated allocation",QuickPlayBindClient(0,1));
    svs.clients[0].netchan.remoteAddress.type=NA_BOT;CHECK("bot reuse clears party assignment",!QuickPlayClientTeam(0));identity(0,1);
    one_row(1,group_a,1,later,1);CHECK("new human can bind after bot reuse",QuickPlayBindClient(0,1));
    nonce[0]='f';CHECK("lease session change clears allocation",!QuickPlayClientTeam(0)&&!QuickPlayClientTeam(1));
    nonce[0]='0';CHECK("old session does not resurrect cache",!QuickPlayClientTeam(0));

    /* Strict parser bounds: every invalid record has an otherwise current
     * Quick ticket, so failure cannot be attributed to a missing ticket. */
    const char *bad[] = {
        "party 1023 %s 0000000000000001 %s 1 %lld\n",
        "party 65536 %s 0000000000000001 %s 1 %lld\n",
        "party 027743 %s 0000000000000001 %s 1 %lld\n",
        "party 27743 %s 0000000000000000 %s 1 %lld\n",
        "party 27743 %s 000000000000001 %s 1 %lld\n",
        "party 27743 %s g000000000000001 %s 1 %lld\n",
        "party 27743 %s 0000000000000001 %s 0 %lld\n",
        "party 27743 %s 0000000000000001 %s 3 %lld\n",
        "party 27743 %s 0000000000000001 %s 9 %lld\n",
        "party 27743 %s 0000000000000001 %s 01 %lld\n",
        "party 27743 %s 0000000000000001 %s -1 %lld\n",
        "party 27743 %s 0000000000000001 %s 1 %lld extra\n",
        "party 27743 %s 0000000000000001 %s 1 9223372036854775808\n",
        "party 27743 %s 0000000000000001 %s 1 -1\n",
        "party 27743 %s 0000000000000001 %s 1 00\n"
    };
    for(i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        out=begin_roster(1);fprintf(out,bad[i],nonce,group_a,later);finish_roster(out);
        snprintf(line,sizeof(line),"invalid party parser row %u",i);CHECK(line,!QuickPlayAdmit(1));
    }
    one_row(1,"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",1,later,1);CHECK("noncanonical uppercase group rejects",!QuickPlayAdmit(1));
    one_row(1,"00000000000000000000000000000000",1,later,1);CHECK("zero group rejects",!QuickPlayAdmit(1));
    one_row(1,"aaaa",1,later,1);CHECK("short group rejects",!QuickPlayAdmit(1));
    out=begin_roster(1);fprintf(out,"party 27743 0123456789abcdeF0123456789abcdef 0000000000000001 %s 1 %lld\n",group_a,later);finish_roster(out);
    CHECK("noncanonical session rejects",!QuickPlayAdmit(1));
    out=begin_roster(1);fprintf(out,"party 27743 1123456789abcdef0123456789abcdef 0000000000000001 %s 1 %lld\n",group_a,later);finish_roster(out);
    CHECK("other valid session rejects",!QuickPlayAdmit(1));
    out=begin_roster(1);for(i=1;i<=257;++i)row(out,i,group_a,1,later);finish_roster(out);
    CHECK("256th party row fits bound",AdmissionQuickParty(27743,nonce,256,&assignment));
    CHECK("257th party row not admitted",!AdmissionQuickParty(27743,nonce,257,&assignment));
    CHECK("failed query clears output",!assignment.team&&!assignment.group[0]);
    out=begin_roster(1);for(i=0;i<70000;++i)fputc('x',out);finish_roster(out);CHECK("oversized roster fails closed",!QuickPlayAdmit(1));

    for(i=0;i<MAX_CLIENTS;++i) { QuickPlayClearClient(i);SetClientConnectIdentity(i,0); }
    out=begin_roster(1);for(i=1;i<=7;++i)row(out,i,group_a,1,later);finish_roster(out);
    for(i=0;i<6;++i) {identity(i,i+1);CHECK("six-person cohort fits one fixed bin",QuickPlayBindClient(i,i+1));}
    identity(6,7);CHECK("seventh human in fixed side rejected",!QuickPlayBindClient(6,7));
    one_row(7,group_b,2,later,1);CHECK("remaining opposing side accepts",QuickPlayBindClient(6,7));
    parties_enabled=0;CHECK("old Quick clears party cache and capability",!QuickPlayPartiesEnabled()&&!QuickPlayClientTeam(0));
    out=begin_roster(1);finish_roster(out);CHECK("old Quick needs no party row",QuickPlayAdmit(1));
    CHECK("old Quick native assignment unchanged",SV_GetAssignedTeam(0)==0);
    MatchStatus_f();CHECK("status distinguishes old Quick",strstr(status,"quick_parties=0 "));
    enabled=0;parties_enabled=1;CHECK("ordinary rooms unaffected by party flag",QuickPlayAdmit(1)&&!QuickPlayPartiesEnabled());
    unlink(path);
    printf("PASS: %u actual native party parser/cache/assigned-team/VM/lifecycle checks\n",checks);
    return 0;
}

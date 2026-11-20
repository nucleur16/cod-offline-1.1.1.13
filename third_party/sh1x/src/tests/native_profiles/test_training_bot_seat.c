/* Actual Training admission/helper and extracted DirectConnect allocation.
 * No fabricated population: the real native client array and connect-identity
 * cache are populated through their normal types/API. Engine/network effects
 * outside this bounded contour are explicit fixtures. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "server.h"
#include "cvar.h"
#include "connect.h"
#include "quickplay.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

server_t sv;
serverStatic_t svs;
int onExitLevelExecuted;
static cvar_t running,maxclients,privateclients;
cvar_t *com_sv_running=&running,*sv_maxclients=&maxclients,*sv_privateClients=&privateclients;
static int quick,playlist,fill,botmax,fixed,reserved,ended,held,blocked=-1;
static int drops,rejected,defer_drop,chosen=-1;
static char *zone="mp",*gametype="combat";
static unsigned checks;
#define CHECK(label,value) do { ++checks; if (!(value)) { fprintf(stderr,"FAIL %s line%d\n",label,__LINE__);abort(); } } while(0)

char *Cvar_GetVariantString(const char *name)
{
    if (!strcmp(name,"sv_zoneType")) return zone;
    if (!strcmp(name,"g_gametype")) return gametype;
    if (!strcmp(name,"quickSession")) return "0123456789abcdef0123456789abcdef";
    return "";
}
qboolean Cvar_VariableBooleanValue(const char *name)
{ return !strcmp(name,"sv_quickPlay") && quick; }
int Cvar_VariableIntegerValue(const char *name)
{
    if (!strcmp(name,"sv_playlistId")) return playlist;
    if (!strcmp(name,"scr_botfill")) return fill;
    if (!strcmp(name,"scr_botmax")) return botmax;
    if (!strcmp(name,"scr_bots")) return fixed;
    if (!strcmp(name,"sv_privateClients")) return reserved;
    if (!strcmp(name,"scr_gameended")) return ended;
    return 0;
}
int MatchReportTransitionHeld(void) { return held; }
int MatchReportCanReuseSlot(unsigned slot) { return !held && (int)slot!=blocked; }
void NET_OutOfBandPrint(netsrc_t sock,netadr_t *from,const char *format,...)
{ (void)sock;(void)from;(void)format;++rejected; }
void SV_DropClient(client_t *client,const char *reason)
{
    CHECK("only an actual native bot is dropped",client->netchan.remoteAddress.type==NA_BOT);
    CHECK("actual silent drop reason",!strcmp(reason,"silent"));
    ++drops;
    if (!defer_drop) client->state=CS_FREE;
}

/* prepare_focused_tests.py extracts this block from the actual current
 * SV_DirectConnect, with source-order/verified-UID assertions before compiling.
 * It includes the real free-slot branch and synchronous bot-drop CS_FREE test. */
static void attempt(uint64_t ol_verified_uid,int existing)
{
    int i=0,j;
    privateclients.integer=reserved;
    netadr_t address={0},*from=&address;
    client_t *cl,*newcl=existing>=0 ? &svs.clients[existing] : NULL;
#include "training-connect.inc"
    chosen=newcl ? (int)(newcl-svs.clients) : -1;
}

static void human(int slot,uint64_t uid,clientConnectState_t state)
{
    svs.clients[slot].state=state;
    svs.clients[slot].netchan.remoteAddress.type=NA_IP;
    SetClientConnectIdentity((unsigned)slot,uid);
}
static void reset(void)
{
    int i;
    memset(&svs,0,sizeof(svs));memset(&sv,0,sizeof(sv));
    for (i=0;i<MAX_CLIENTS;++i) SetClientConnectIdentity((unsigned)i,0);
    running.integer=1;maxclients.integer=12;quick=0;playlist=3;
    fill=12;botmax=11;fixed=reserved=ended=held=onExitLevelExecuted=0;
    blocked=-1;drops=rejected=defer_drop=0;chosen=-1;zone="mp";gametype="combat";
    sv.state=SS_GAME;QuickPlayMapReady();
    human(0,1001,CS_ACTIVE);
    for (i=1;i<12;++i) { svs.clients[i].state=CS_ACTIVE;svs.clients[i].netchan.remoteAddress.type=NA_BOT; }
}
static void count(int humans,int bots)
{
    int i,h=0,b=0;
    for (i=0;i<12;++i) if (svs.clients[i].state>=CS_CONNECTED) {
        if (svs.clients[i].netchan.remoteAddress.type==NA_BOT) ++b;else ++h;
    }
    CHECK("real native human count",h==humans);CHECK("real native bot count",b==bots);
    CHECK("real native total remains twelve",h+b==12);
}
static void denied(uint64_t uid)
{
    int before=drops;chosen=-1;rejected=0;attempt(uid,-1);
    CHECK("admission denied before seating",rejected>0 && chosen==-1);
    CHECK("denial drops no bot",drops==before);
}
int main(void)
{
    int seat;
    reset();count(1,11);attempt(1002,-1);seat=chosen;
    CHECK("second authenticated human replaces actual bot",seat==11 && drops==1 && !rejected);
    human(seat,1002,CS_CONNECTED);count(2,10);
    chosen=-1;attempt(1003,-1);seat=chosen;
    CHECK("third pending connected human reserves a real seat",seat==10 && drops==2 && !rejected);
    human(seat,1003,CS_PRIMED);count(3,9);denied(1004);count(3,9);
    svs.clients[9].state=CS_FREE;denied(1004); /* Cap also applies to FREE path. */
    rejected=0;chosen=-1;attempt(1003,10);
    CHECK("same verified UID can reconnect to its own seat",chosen==10 && !rejected && drops==2);
    rejected=0;chosen=-1;attempt(1003,9);
    CHECK("arbitrary free reconnect slot cannot hide duplicate UID",rejected && chosen==-1);
    reset();playlist=1;denied(1002); /* Boot Camp remains solo. */
    reset();playlist=0;attempt(1002,-1);CHECK("manual combat exact profile admits second",chosen==11);
    reset();playlist=8;CHECK("ordinary manual profile cannot evict",TrainingBotSeat(1002)==-1);
    reset();playlist=0;gametype="war";CHECK("ordinary manual war cannot evict",TrainingBotSeat(1002)==-1);
    reset();quick=1;CHECK("malformed Quick cannot fall through",TrainingBotSeat(1002)==-1);
    reset();zone="zm";CHECK("Zombies cannot evict",TrainingBotSeat(1002)==-1);
    reset();zone="coop";CHECK("Coop cannot evict",TrainingBotSeat(1002)==-1);
    reset();maxclients.integer=18;CHECK("wrong native geometry cannot evict",TrainingBotSeat(1002)==-1);
    reset();fill=0;CHECK("wrong fill cannot evict",TrainingBotSeat(1002)==-1);
    reset();botmax=6;CHECK("legacy fixed bot profile cannot evict",TrainingBotSeat(1002)==-1);
    reset();fixed=6;CHECK("fixed bots cannot evict",TrainingBotSeat(1002)==-1);
    reset();denied(0); /* Missing verified native transaction UID. */
    reset();human(1,0,CS_CONNECTED);denied(1002);
    reset();human(1,1001,CS_CONNECTED);denied(1002);
    reset();denied(1001);
    reset();held=1;denied(1002);
    reset();ended=1;denied(1002);
    reset();onExitLevelExecuted=1;denied(1002);
    reset();running.integer=0;denied(1002);
    reset();sv.state=SS_LOADING;denied(1002);
    reset();QuickPlayBeginMap();denied(1002);
    reset();reserved=12;CHECK("reserved bots cannot be evicted",TrainingBotSeat(1002)==-1);
    reset();reserved=-1;CHECK("invalid reservation rejects",TrainingBotSeat(1002)==-1);
    reset();blocked=11;CHECK("pending receipt bot skipped",TrainingBotSeat(1002)==10);
    reset();human(0,1001,CS_CONNECTED);CHECK("no active human cannot displace warm bots",TrainingBotSeat(1002)==-1);
    reset();memset(&svs,0,sizeof(svs));attempt(1001,-1);
    CHECK("first genuine human can use a free running-world seat",chosen==0 && !drops && !rejected);
    reset();playlist=8;svs.clients[2].state=CS_FREE;attempt(1002,-1);
    CHECK("ordinary manual free-seat path is preserved",chosen==2 && !drops && !rejected);
    reset();human(1,1002,CS_CONNECTED);human(2,1003,CS_PRIMED);
    svs.clients[3].state=CS_FREE;reserved=4;attempt(1004,3);
    CHECK("reserved free seat cannot bypass Training human cap",rejected && chosen==-1 && !drops);
    reset();defer_drop=1;attempt(1002,-1);
    CHECK("actual caller requires synchronous CS_FREE",chosen==-1 && drops==1);
    reset();quick=1;playlist=120;QuickPlayBeginMap();QuickPlayMapReady();
    CHECK("original Quick bot selector still works",QuickPlayBotSeat()==11);
    printf("PASS: %u actual Training helper/DirectConnect allocation checks\n",checks);
    return 0;
}

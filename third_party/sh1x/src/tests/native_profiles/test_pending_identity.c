/* Tests compile the actual sv_client.c helpers and packet dispatcher with the
 * actual native structs, UID store and address comparer. Extracted quick-gate
 * bodies are unchanged; only their I/O/drop/report boundaries use named doubles.
 * This is not a full authenticated network/C1 acceptance test. */
#include "/src/src/sv_client.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

serverStatic_t svs;
cvar_t *sv_maxclients;
static cvar_t maximum;
static int completed, dropped, rejected, reusable, cases;
static client_t *selected;
void QDECL Com_PrintError(conChannel_t channel, const char *fmt, ...) { abort(); }
static void FixturePrint(int channel, const char *format, ...) { (void)channel; (void)format; }
static const char *FixtureAddress(netadr_t *from) { (void)from; return "fixture"; }
static void FixtureReject(netsrc_t sock, netadr_t *from, const char *format, ...)
{ (void)sock; (void)from; (void)format; ++rejected; }
static int FixtureReuse(unsigned slot) { assert(slot < 4); return reusable; }
static void FixtureDrop(client_t *cl, const char *reason)
{ assert(!strcmp(reason,"silent")); ++dropped; cl->state=CS_ZOMBIE; SetClientConnectIdentity((unsigned)(cl-svs.clients),0); }
#define NET_OutOfBandPrint FixtureReject
#define NET_AdrToString FixtureAddress
#define Com_DPrintf FixturePrint
#define Com_Printf FixturePrint
#define SV_DropClient FixtureDrop
#define MatchReportCanReuseSlot FixtureReuse
static void run_donor(netadr_t *from, unsigned qport, uint64_t ol_verified_uid)
{
    int n; client_t *cl, *newcl=NULL; (void)ol_verified_uid;
#include "/recipe/donor-gate.inc"
    selected=newcl; completed=1;
}
static void run_candidate(netadr_t *from, unsigned qport, uint64_t ol_verified_uid)
{
    int n; client_t *cl, *newcl=NULL;
#include "/recipe/candidate-gate.inc"
    selected=newcl; completed=1;
}
#undef NET_OutOfBandPrint
#undef NET_AdrToString
#undef Com_DPrintf
#undef Com_Printf
#undef SV_DropClient
#undef MatchReportCanReuseSlot

static netadr_t address(unsigned port, unsigned host)
{
    netadr_t a={0}; a.type=NA_IP; a.ip[0]=172; a.ip[1]=17; a.ip[3]=(byte)host;
    a.port=(unsigned short)port; return a;
}
static void reset(void)
{
    unsigned i; memset(svs.clients,0,4*sizeof(*svs.clients));
    for(i=0;i<4;++i) { SV_ForgetConnectIdentity(i); SetClientConnectIdentity(i,0); }
    completed=dropped=rejected=0; selected=NULL; reusable=1; svs.time=100;
}
static void put(unsigned slot, int state, uint64_t uid, unsigned port, unsigned qport, int verified)
{
    client_t *cl=svs.clients+slot; cl->state=state; cl->netchan.remoteAddress=address(port,1);
    cl->netchan.qport=(unsigned short)qport; cl->lastConnectTime=10;
    SetClientConnectIdentity(slot,uid);
    if(verified) SV_RememberConnectIdentity(slot,cl,uid);
    if(state==CS_ZOMBIE) SetClientConnectIdentity(slot,0);
}
#define CHECK(x) do { assert(x); ++cases; } while(0)
int main(void)
{
    netadr_t from=address(46485,1), other=address(46485,2); int state; client_t *cl;
    maximum.integer=4; sv_maxclients=&maximum;
    CHECK(__alignof__(verified_connection_t)>=4);
    CHECK(offsetof(verified_connection_t,address)%__alignof__(netadr_t)==0);
    CHECK(sizeof(verified_connection_t)%__alignof__(netadr_t)==0);
    printf("native_client_size=%u address_align=%u marker_size=%u marker_align=%u marker_address_offset=%u\n",
        (unsigned)sizeof(client_t),(unsigned)__alignof__(netadr_t),(unsigned)sizeof(verified_connection_t),
        (unsigned)__alignof__(verified_connection_t),(unsigned)offsetof(verified_connection_t,address));
    /* Exact donor counterexample: one pending verified account blocks another. */
    reset(); put(0,CS_CONNECTED,111,46484,1000,1); run_donor(&from,1001,222);
    CHECK(!completed && rejected==1 && dropped==0);
    reset(); put(0,CS_CONNECTED,111,46484,1000,1); run_candidate(&from,1001,222);
    CHECK(completed && rejected==0 && dropped==0 && !selected && svs.clients[0].state==CS_CONNECTED);
    /* Three verified real slots can remain pending without excluding account4. */
    reset(); put(0,CS_CONNECTED,111,46484,1000,1); put(1,CS_PRIMED,222,46486,1002,1);
    put(2,CS_ACTIVE,333,46487,1003,1); run_candidate(&from,1001,444);
    CHECK(completed && !dropped && !rejected && ClientConnectIdentity(2)==333);
    /* Proven tombstones remove only a different account's base-IP lock. */
    reset(); put(0,CS_ZOMBIE,111,46484,1000,1); run_candidate(&from,1001,222);
    CHECK(completed && !rejected && !dropped && ClientConnectIdentity(0)==0);
    reset(); put(0,CS_ZOMBIE,111,46484,1000,0); run_candidate(&from,1001,222);
    CHECK(!completed && rejected==1 && !dropped);
    /* Claimed UID alone, incoming or existing, cannot opt out of flood guards. */
    reset(); put(0,CS_CONNECTED,111,46484,1000,1); run_candidate(&from,1001,0);
    CHECK(!completed && rejected==1 && !dropped);
    reset(); put(0,CS_CONNECTED,111,46484,1000,0); run_candidate(&from,1001,222);
    CHECK(!completed && rejected==1 && !dropped);
    /* Same user on another transport is a duplicate in every occupied state. */
    for(state=CS_ZOMBIE;state<=CS_ACTIVE;++state) {
        reset(); put(0,state,111,46484,1000,1); run_candidate(&from,1001,111);
        CHECK(!completed && rejected==1 && !dropped);
        reset(); put(0,state,111,46484,1000,1); run_candidate(&other,1001,111);
        CHECK(!completed && rejected==1 && !dropped);
    }
    /* Do not displace another verified user sharing qport or the UDP endpoint. */
    for(state=CS_ZOMBIE;state<=CS_ACTIVE;++state) {
        reset(); put(0,state,111,46484,1000,1); run_candidate(&from,1000,222);
        CHECK(!completed && rejected==1 && !dropped);
        reset(); put(0,state,111,46485,1000,1); run_candidate(&from,1001,222);
        CHECK(!completed && rejected==1 && !dropped);
    }
    reset(); put(0,CS_ACTIVE,111,46484,1000,1); run_candidate(&from,65536u+1000,222);
    CHECK(!completed && rejected==1 && !dropped);
    /* The old same-user reconnect path still honors the persistence barrier. */
    reset(); put(0,CS_CONNECTED,111,46484,1000,1); run_candidate(&from,1000,111);
    CHECK(!completed && !dropped); /* legacy pending reject is deliberately silent */
    reset(); put(0,CS_ACTIVE,111,46484,1000,1); reusable=0; run_candidate(&from,1000,111);
    CHECK(!completed && rejected==1 && !dropped);
    reset(); put(0,CS_ACTIVE,111,46484,1000,1); run_candidate(&from,1000,111);
    CHECK(completed && dropped==1 && selected==svs.clients);
    /* Even a later duplicate must be checked before the first slot is reused. */
    reset(); put(0,CS_ACTIVE,111,46485,1001,1); put(1,CS_ACTIVE,111,46486,1002,1);
    run_candidate(&from,1001,111); CHECK(!completed && rejected==1 && !dropped);
    /* Unrelated address / legacy pending cases keep their original result. */
    reset(); put(0,CS_CONNECTED,111,46484,1000,1); run_candidate(&other,1001,222);
    CHECK(completed && !dropped && !rejected);
    reset(); put(0,CS_CONNECTED,111,46484,1000,0); run_candidate(&from,1000,0);
    CHECK(!completed && !dropped);
    /* Record invalidation must fail closed after every meaningful slot change. */
    reset(); put(0,CS_ACTIVE,111,46484,1000,1); cl=svs.clients;
    CHECK(SV_VerifiedConnectIdentity(0,cl)==111);
    cl->state=CS_CONNECTED; CHECK(SV_VerifiedConnectIdentity(0,cl)==111); /* map change */
    cl->state=CS_FREE; CHECK(!SV_VerifiedConnectIdentity(0,cl)); cl->state=CS_ACTIVE;
    SetClientConnectIdentity(0,222); CHECK(!SV_VerifiedConnectIdentity(0,cl));
    SetClientConnectIdentity(0,111); ++cl->lastConnectTime; CHECK(!SV_VerifiedConnectIdentity(0,cl));
    --cl->lastConnectTime; ++cl->netchan.qport; CHECK(!SV_VerifiedConnectIdentity(0,cl));
    --cl->netchan.qport; ++cl->netchan.remoteAddress.ip[3]; CHECK(!SV_VerifiedConnectIdentity(0,cl));
    --cl->netchan.remoteAddress.ip[3]; cl->netchan.remoteAddress.type=NA_BOT;
    CHECK(!SV_VerifiedConnectIdentity(0,cl)); cl->netchan.remoteAddress.type=NA_IP;
    SV_ForgetConnectIdentity(0); CHECK(!SV_VerifiedConnectIdentity(0,cl));
    SV_RememberConnectIdentity(0,cl,0); CHECK(!SV_VerifiedConnectIdentity(0,cl));
    SV_RememberConnectIdentity(0,cl,222); CHECK(!SV_VerifiedConnectIdentity(0,cl));
    CHECK(!SV_VerifiedConnectIdentity(MAX_CLIENTS,cl));
    CHECK(!SV_VerifiedConnectIdentity(MAX_CLIENTS,NULL));
    CHECK(SV_ConnectIdentityPolicy(MAX_CLIENTS,NULL,&from,1000,0)==CONNECT_LEGACY);
    CHECK(SV_ConnectIdentityPolicy(MAX_CLIENTS,NULL,&from,1000,111)==CONNECT_LEGACY);
    SV_ForgetConnectIdentity(MAX_CLIENTS); SV_RememberConnectIdentity(MAX_CLIENTS,cl,111);
    /* Execute actual native packet demux: remapped UDP port preserves proof. */
    reset(); put(0,CS_ACTIVE,111,46484,1000,1); put(1,CS_CONNECTED,222,46486,1002,1);
    CHECK(SV_ReadPackets(&from,1000)==svs.clients);
    CHECK(svs.clients[0].netchan.remoteAddress.port==from.port);
    CHECK(SV_VerifiedConnectIdentity(0,svs.clients)==111);
    CHECK(SV_ReadPackets(&from,1002)==svs.clients+1);
    CHECK(!SV_ReadPackets(&from,1001));
    /* Dropped verified slot never leaks its tombstone through the UID API. */
    svs.clients[0].state=CS_ZOMBIE; SetClientConnectIdentity(0,0);
    CHECK(SV_VerifiedConnectIdentity(0,svs.clients)==111 && !ClientConnectIdentity(0));
    SetClientConnectIdentity(0,111); CHECK(!SV_VerifiedConnectIdentity(0,svs.clients));
    puts("PENDING_IDENTITY_ACTUAL_NATIVE_PASS"); printf("assertions=%d\n",cases); return 0;
}

#include "server.h"
#include "clientstate_ext.h"
#include "qcommon.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
serverStatic_t svs;
svsHeader_t svsHeader;
qboolean Assert_MyHandler(const char *e,const char *f,int l,const char *fn,const char *fmt,...)
{(void)fmt;fprintf(stderr,"%s %s:%d %s\n",e,f,l,fn);abort();}
/* The live world-gun children (player_worldweapon.c), as the test sets them. */
static unsigned live_gun[2];
unsigned PlayerWorldWeaponEntity(unsigned client,unsigned hand)
{ (void)client; return hand<2 ? live_gun[hand] : 0; }
static unsigned field(const client_state_t *s,unsigned offset)
{
    unsigned i;for(i=0;i<CLIENT_FIELDS;++i)
        if(ClientField(i)->offset==offset)return s->field[i];
    abort();
}
int main(void)
{
    client_appearance_t a={0},b={0},bad;
    client_state_t out={0};
    byte data[4096];msg_t msg;int size;unsigned i;
    svs.numSnapshotClients=2;
    svsHeader.numSnapshotClients=2;
    svsHeader.snapshotClients=svs.snapshotClients;
    svs.snapshotClients[0].clientIndex=0;
    svs.snapshotClients[1].clientIndex=0;
    svsHeader.nextSnapshotClients=1;
    a.model=99;
    for(i=0;i<CLIENT_ATTACHMENTS;++i)a.attach_model[i]=100+i;
    assert(PublishClientAppearance(0,&a));
    assert(CapturePublishedClientAbilities(0,0));
    b.model=222;b.attach_model[0]=223;
    assert(PublishClientAppearance(0,&b));
    assert(OverlayCapturedClientAbilities(0,&out));
    assert(field(&out,0x14)==99 && field(&out,0x5c)==115);
    bad=b;bad.attach_model[2]=224; /* Hole before a present part. */
    assert(!PublishClientAppearance(0,&bad));
    assert(CapturePublishedClientAbilities(1,0));
    svsHeader.nextSnapshotClients=2;
    assert(OverlayCapturedClientAbilities(1,&out));
    assert(field(&out,0x14)==222 && field(&out,0x20)==223 && !field(&out,0x5c));
    /* Archive old A, change live to B, decode A and recapture its own set. */
    assert(PublishClientAppearance(0,&a));
    svs.cachedSnapshotClients[0].cs.clientIndex=0;
    MSG_Init(&msg,data,sizeof(data));
    WriteArchivedClientAbilities(&msg,0,&svs.cachedSnapshotClients[0].cs);
    assert(!msg.overflowed);size=msg.cursize;
    assert(PublishClientAppearance(0,&b));
    MSG_Init(&msg,data,sizeof(data));msg.cursize=size;
    ReadArchivedClientAbilities(&msg,&svs.cachedSnapshotClients[0].cs);
    assert(!msg.overflowed);
    assert(CaptureArchivedClientAbilities(2,&svs.cachedSnapshotClients[0].cs));
    svsHeader.nextSnapshotClients=3;
    assert(OverlayCapturedClientAbilities(2,&out));
    assert(field(&out,0x14)==99 && field(&out,0x5c)==115);
    ClearPublishedClientAbilities(0);
    assert(OverlayCapturedClientAbilities(2,&out));
    assert(field(&out,0x5c)==115);
    /* An explicitly empty appearance clears all old parts. */
    memset(&b,0,sizeof(b));assert(PublishClientAppearance(0,&b));
    assert(CapturePublishedClientAbilities(3,0));
    svsHeader.nextSnapshotClients=4;
    assert(OverlayCapturedClientAbilities(3,&out));
    assert(!field(&out,0x14) && !field(&out,0x5c));
    /* Script attachments (the CTF carry flag) follow the character parts
     * with their tag; a full character keeps all sixteen of its own. */
    {
        unsigned models[2]={300,301},tags[2]={3,5},bad_tags[1]={64};
        b.model=222;b.attach_model[0]=223;b.attach_model[1]=224;
        assert(PublishClientAppearance(0,&b));
        assert(PublishClientScriptAttachments(0,models,tags,2));
        assert(!PublishClientScriptAttachments(0,models,bad_tags,1));
        assert(CapturePublishedClientAbilities(4,0));
        svsHeader.nextSnapshotClients=5;
        assert(OverlayCapturedClientAbilities(4,&out));
        assert(field(&out,0x20)==223 && field(&out,0x24)==224 && field(&out,0x28)==300 &&
               field(&out,0x2c)==301 && !field(&out,0x30));
        assert(!field(&out,0x60) && !field(&out,0x64) && field(&out,0x68)==3 && field(&out,0x6c)==5);
        /* The killcam archive carries the same merged set. */
        svs.cachedSnapshotClients[0].cs.clientIndex=0;
        MSG_Init(&msg,data,sizeof(data));
        WriteArchivedClientAbilities(&msg,0,&svs.cachedSnapshotClients[0].cs);
        assert(!msg.overflowed);size=msg.cursize;
        assert(PublishClientScriptAttachments(0,NULL,NULL,0));
        MSG_Init(&msg,data,sizeof(data));msg.cursize=size;
        ReadArchivedClientAbilities(&msg,&svs.cachedSnapshotClients[0].cs);
        assert(!msg.overflowed);
        assert(CaptureArchivedClientAbilities(5,&svs.cachedSnapshotClients[0].cs));
        svsHeader.nextSnapshotClients=6;
        assert(OverlayCapturedClientAbilities(5,&out));
        assert(field(&out,0x28)==300 && field(&out,0x6c)==5);
        /* Dropped: the live set no longer has it. */
        assert(CapturePublishedClientAbilities(6,0));
        svsHeader.nextSnapshotClients=7;
        assert(OverlayCapturedClientAbilities(6,&out));
        assert(field(&out,0x24)==224 && !field(&out,0x28) && !field(&out,0x68));
        /* No room after a full sixteen-part character. */
        assert(PublishClientAppearance(0,&a));
        assert(PublishClientScriptAttachments(0,models,tags,2));
        assert(CapturePublishedClientAbilities(7,0));
        svsHeader.nextSnapshotClients=8;
        assert(OverlayCapturedClientAbilities(7,&out));
        assert(field(&out,0x5c)==115 && !field(&out,0x9c));
    }
    /* clientState +0x12c/+0x130 carry the world guns of the capture's own
     * frame, not the live ones: a killcam frame must name the children its
     * archived entity set holds. */
    live_gun[0]=40;live_gun[1]=41;
    assert(CapturePublishedClientAbilities(8,0));
    live_gun[0]=50;live_gun[1]=0;
    svsHeader.nextSnapshotClients=9;
    assert(OverlayCapturedClientAbilities(8,&out));
    assert(field(&out,0x12c)==40 && field(&out,0x130)==41);
    live_gun[0]=60;live_gun[1]=61;
    svs.cachedSnapshotClients[0].cs.clientIndex=0;
    MSG_Init(&msg,data,sizeof(data));
    WriteArchivedClientAbilities(&msg,0,&svs.cachedSnapshotClients[0].cs);
    assert(!msg.overflowed);size=msg.cursize;
    live_gun[0]=0;live_gun[1]=0;
    MSG_Init(&msg,data,sizeof(data));msg.cursize=size;
    ReadArchivedClientAbilities(&msg,&svs.cachedSnapshotClients[0].cs);
    assert(!msg.overflowed);
    assert(CaptureArchivedClientAbilities(9,&svs.cachedSnapshotClients[0].cs));
    svsHeader.nextSnapshotClients=10;
    assert(OverlayCapturedClientAbilities(9,&out));
    assert(field(&out,0x12c)==60 && field(&out,0x130)==61);
    /* A number C1 cannot hold is refused at the archive. */
    live_gun[0]=2047;
    MSG_Init(&msg,data,sizeof(data));
    WriteArchivedClientAbilities(&msg,0,&svs.cachedSnapshotClients[0].cs);
    assert(msg.overflowed);
    live_gun[0]=0;
    /* C1 HUD enable bit belongs to the captured session, including archives. */
    assert(EnableClientStatusColour(0));
    assert(!EnableClientStatusColour(MAX_CLIENTS));
    assert(CapturePublishedClientAbilities(10,0));
    svsHeader.nextSnapshotClients=11;
    assert(OverlayCapturedClientAbilities(10,&out));
    assert(field(&out,0x134)==1);
    MSG_Init(&msg,data,sizeof(data));
    WriteArchivedClientAbilities(&msg,0,&svs.cachedSnapshotClients[0].cs);
    assert(!msg.overflowed);size=msg.cursize;
    ClearPublishedClientAbilities(0);
    MSG_Init(&msg,data,sizeof(data));msg.cursize=size;
    ReadArchivedClientAbilities(&msg,&svs.cachedSnapshotClients[0].cs);
    assert(!msg.overflowed);
    assert(CaptureArchivedClientAbilities(11,&svs.cachedSnapshotClients[0].cs));
    svsHeader.nextSnapshotClients=12;
    assert(OverlayCapturedClientAbilities(11,&out));assert(field(&out,0x134)==1);
    assert(CapturePublishedClientAbilities(12,0));svsHeader.nextSnapshotClients=13;
    assert(OverlayCapturedClientAbilities(12,&out));assert(!field(&out,0x134));
    {
        extern int PersistentClientAbilitiesEmpty(void);
        ClearCapturedClientAbilities();assert(PersistentClientAbilitiesEmpty());
        assert(EnableClientStatusColour(0));assert(!PersistentClientAbilitiesEmpty());
        ClearPublishedClientAbilities(0);assert(PersistentClientAbilitiesEmpty());
    }
    puts("PASS: sixteen-part appearance capture, replacement, archive ownership, clear, script attachments and frame-owned world guns");
    return 0;
}

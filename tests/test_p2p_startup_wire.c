/* Actual portable C framing with an independent Python-produced fixture.
 * No Windows process, native host or remote service is launched. */
#include "../p2p_startup_wire.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks;assert(x); } while(0)
static int hash_result=1;
static int sha256(const void *data,size_t length,unsigned char out[32])
{ return mbedtls_sha256_ret(data,length,out,0)==0?hash_result:0; }
static unsigned char *load(const char *path,size_t maximum,size_t *size)
{
    FILE *f=fopen(path,"rb");long n;unsigned char *data;
    CHECK(f!=NULL);CHECK(!fseek(f,0,SEEK_END));n=ftell(f);CHECK(n>=0&&(unsigned long)n<=maximum);
    CHECK(!fseek(f,0,SEEK_SET));data=malloc((size_t)n+1);CHECK(data!=NULL);
    CHECK(fread(data,1,(size_t)n,f)==(size_t)n);CHECK(!fclose(f));*size=(size_t)n;return data;
}
static void refused(const void *q,size_t qn,const void *p,size_t pn)
{
    p2p_startup_view_t view;memset(&view,0x5a,sizeof(view));
    CHECK(!P2PStartupParseEnvelope(q,qn,p,pn,sha256,&view));
    CHECK(!view.valid&&!view.scripts&&!view.principals&&!view.scripts_length&&!view.principals_length);
}
int main(int argc,char **argv)
{
    size_t qn,pn,an,n;unsigned char *q,*p,*expected,*bad,request[P2P_STARTUP_MAX_REQUEST],ack[76];
    p2p_startup_view_t view;char map[65],mode[65],category[65];size_t a,b,c;
    CHECK(argc==5);
    q=load(argv[1],P2P_STARTUP_MAX_REQUEST,&qn);p=load(argv[2],P2P_STARTUP_MAX_ENVELOPE,&pn);
    expected=load(argv[3],76,&an);CHECK(an==76);
    a=(unsigned)q[48]+((unsigned)q[49]<<8);b=(unsigned)q[50]+((unsigned)q[51]<<8);c=(unsigned)q[52]+((unsigned)q[53]<<8);
    CHECK(a<=64&&b<=64&&c<=64&&qn==56+a+b+c);
    memcpy(map,q+56,a);map[a]=0;memcpy(mode,q+56+a,b);mode[b]=0;memcpy(category,q+56+a+b,c);category[c]=0;
    CHECK(P2PStartupMakeRequest(map,mode,category,q+16,request,sizeof(request),&n)&&n==qn&&!memcmp(q,request,n));
    CHECK(P2PStartupParseEnvelope(q,qn,p,pn,sha256,&view));
    CHECK(view.valid==1&&view.scripts==p+152&&view.principals==p+152+view.scripts_length);
    CHECK(P2PStartupMakeAck(&view,1,1,ack)&&!memcmp(ack,expected,76));
    FILE *reply=fopen(argv[4],"wb");CHECK(reply!=NULL);CHECK(fwrite(ack,1,76,reply)==76);CHECK(!fclose(reply));
    CHECK(P2PStartupMakeAck(&view,0,1,ack)&&ack[72]==0&&ack[73]==0&&ack[74]==0&&ack[75]==0);
    CHECK(P2PStartupMakeAck(&view,2,1,ack)&&ack[72]==0);
    CHECK(P2PStartupMakeAck(&view,1,-7,ack)&&ack[72]==249&&ack[73]==255&&ack[74]==255&&ack[75]==255);
    CHECK(!P2PStartupMakeAck(NULL,1,1,ack));view.valid=0;CHECK(!P2PStartupMakeAck(&view,1,1,ack));
    /* Byte-for-byte independent framing, all truncation positions and trailing data. */
    for(n=0;n<qn;++n)refused(q,n,p,pn);
    CHECK(pn<1048576);for(n=0;n<pn;++n)refused(q,qn,p,n);
    bad=malloc(pn+1);CHECK(bad!=NULL);memcpy(bad,p,pn);bad[pn]=0;refused(q,qn,bad,pn+1);
    unsigned offsets[]={0,6,8,12,16,20,24,56,88,120,152};
    for(n=0;n<sizeof(offsets)/sizeof(offsets[0]);++n){
        memcpy(bad,p,pn);bad[offsets[n]]^=1;refused(q,qn,bad,pn);
    }
    memcpy(request,q,qn);request[54]=1;refused(request,qn,p,pn);
    memcpy(request,q,qn);memset(request+16,0,32);refused(request,qn,p,pn);
    hash_result=0;refused(q,qn,p,pn);hash_result=2;refused(q,qn,p,pn);hash_result=1;
    memset(request,0,sizeof(request));CHECK(!P2PStartupMakeRequest(map,mode,category,request,request,sizeof(request),&n)&&n==0);
    unsigned char nonce[32];memset(nonce,1,sizeof(nonce));
    memset(map,'a',64);map[64]=0;
    CHECK(P2PStartupMakeRequest(map,map,map,nonce,request,sizeof(request),&n)&&n==248);
    CHECK(!P2PStartupMakeRequest(map,map,map,nonce,request,247,&n)&&n==0);
    CHECK(!P2PStartupMakeRequest("1map","war","mp",nonce,request,sizeof(request),&n));
    CHECK(!P2PStartupMakeRequest("mp_killhouse;quit","war","mp",nonce,request,sizeof(request),&n));
    free(bad);free(expected);free(p);free(q);
    printf("PASS: %u actual C wire checks and independent request/envelope/ACK fixture\n",checks);return 0;
}

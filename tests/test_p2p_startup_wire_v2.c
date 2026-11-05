/* Actual portable codec, independent Python vectors. No native/Windows load. */
#include "../p2p_startup_wire.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks;assert(x); } while(0)
static int sha(const void *p,size_t n,unsigned char out[32])
{ return mbedtls_sha256_ret(p,n,out,0)==0?1:0; }
static unsigned char *load(const char *path,size_t *n)
{
    FILE *f=fopen(path,"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));long size=ftell(f);
    CHECK(size>0&&size<1048576);rewind(f);unsigned char *p=malloc((size_t)size+1);CHECK(p);
    CHECK(fread(p,1,(size_t)size,f)==(size_t)size);CHECK(!fclose(f));*n=(size_t)size;return p;
}
static unsigned u32(const unsigned char *p)
{ return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24; }
static void refuse(const void *q,size_t qn,const void *p,size_t pn)
{
    p2p_startup_view_t v;memset(&v,0x5a,sizeof(v));CHECK(!P2PStartupParseEnvelope(q,qn,p,pn,sha,&v));
    CHECK(!v.valid&&!v.scripts&&!v.principals&&!v.options&&!v.options_length&&!v.protocol);
}
static void rebind(const unsigned char *q,size_t qn,unsigned char *p)
{
    static const unsigned char domain[]="CODOL-P2P-STARTUP-V2";
    unsigned sn=u32(p+16),rn=u32(p+20);unsigned char in[sizeof(domain)+256+12+96];size_t at=0;
    CHECK(sha(p+192,sn,p+64));CHECK(sha(p+192+sn,rn,p+96));CHECK(sha(p+192+sn+rn,352,p+128));
    memcpy(in,domain,sizeof(domain));at=sizeof(domain);memcpy(in+at,q,qn);at+=qn;
    memcpy(in+at,p+16,12);at+=12;memcpy(in+at,p+64,96);at+=96;CHECK(sha(in,at,p+160));
}
int main(int argc,char **argv)
{
    CHECK(argc==5);size_t qn,pn,an,n;unsigned char *q=load(argv[1],&qn),*p=load(argv[2],&pn),*a=load(argv[3],&an);
    unsigned char request[256],ack[76],*bad=malloc(pn+1);CHECK(bad&&an==76);
    unsigned l0=q[48]|q[49]<<8,l1=q[50]|q[51]<<8,l2=q[52]|q[53]<<8;
    char map[65],gt[65],cat[65];CHECK(l0<=64&&l1<=64&&l2<=64&&qn==64+l0+l1+l2);
    memcpy(map,q+64,l0);map[l0]=0;memcpy(gt,q+64+l0,l1);gt[l1]=0;memcpy(cat,q+64+l0+l1,l2);cat[l2]=0;
    int difficulty=q[56]==255?-1:q[56];
    CHECK(P2PStartupMakeRequestV2(map,gt,cat,q[57],difficulty,q+16,request,sizeof(request),&n));
    CHECK(n==qn&&!memcmp(q,request,qn));p2p_startup_view_t v;
    CHECK(P2PStartupParseEnvelope(q,qn,p,pn,sha,&v));CHECK(v.valid==1&&v.protocol==2&&v.options_length==352);
    CHECK(v.scripts==p+192&&v.principals==p+192+v.scripts_length&&v.options==v.principals+v.principals_length);
    CHECK(P2PStartupMakeAckV2(&v,1,1,1,ack)&&!memcmp(ack,a,76));CHECK(!P2PStartupMakeAck(&v,1,1,ack));
    FILE *f=fopen(argv[4],"wb");CHECK(f);CHECK(fwrite(a,1,76,f)==76);CHECK(!fclose(f));
    for(int s=-1;s<=2;++s)for(int r=-1;r<=2;++r)for(int m=-1;m<=2;++m) {
        CHECK(P2PStartupMakeAckV2(&v,s,r,m,ack));CHECK(u32(ack+72)==(unsigned)(s==1&&r==1&&m==1));
    }
    for(n=0;n<qn;++n)refuse(q,n,p,pn);
    for(n=0;n<pn;++n)refuse(q,qn,p,n);
    memcpy(bad,p,pn);bad[pn]=0;refuse(q,qn,bad,pn+1);
    /* Every header byte and every request byte is independently bound. */
    for(n=0;n<192;++n){memcpy(bad,p,pn);bad[n]^=1;refuse(q,qn,bad,pn);}
    for(n=0;n<qn;++n){memcpy(request,q,qn);request[n]^=1;refuse(request,qn,p,pn);}
    size_t mo=192+v.scripts_length+v.principals_length;
    /* Recompute all public hashes, so these exercise structure/scope rather
     * than merely detecting a stale digest. */
    unsigned fields[]={0,8,12,48,80,96,104,112,172,240,304,336};
    for(n=0;n<sizeof(fields)/sizeof(fields[0]);++n){memcpy(bad,p,pn);bad[mo+fields[n]]^=1;rebind(q,qn,bad);refuse(q,qn,bad,pn);}
    unsigned ranges[]={120,124,128,132,136,140,144,148,152,156,160,164,168};
    for(n=0;n<sizeof(ranges)/sizeof(ranges[0]);++n){memcpy(bad,p,pn);memset(bad+mo+ranges[n],0xfe,4);rebind(q,qn,bad);refuse(q,qn,bad,pn);}
    memcpy(bad,p,pn);memset(bad+mo+16,0,32);rebind(q,qn,bad);refuse(q,qn,bad,pn);
    /* Zero padding and field-NUL boundaries, not substring comparisons. */
    unsigned slots[]={176,240,304,336},sizes[]={64,64,32,16};
    for(n=0;n<4;++n){memcpy(bad,p,pn);bad[mo+slots[n]+sizes[n]-1]='x';rebind(q,qn,bad);refuse(q,qn,bad,pn);}
    memcpy(bad,p,pn);bad[5]='1';refuse(q,qn,bad,pn); /* downgrade envelope */
    memcpy(request,q,qn);request[5]='1';refuse(request,qn,p,pn); /* downgrade request */
    CHECK(!P2PStartupMakeRequestV2(map,gt,cat,0,0,q+16,request,sizeof(request),&n)&&!n);
    CHECK(!P2PStartupMakeRequestV2(map,gt,cat,1,-1,q+16,request,sizeof(request),&n)&&!n);
    CHECK(!P2PStartupMakeRequestV2(map,gt,cat,1,4,q+16,request,sizeof(request),&n)&&!n);
    CHECK(!P2PStartupMakeRequestV2(map,gt,cat,2,0,q+16,request,sizeof(request),&n)&&!n);
    free(bad);free(q);free(p);free(a);printf("PASS %u v2 actual C codec checks\n",checks);return 0;
}

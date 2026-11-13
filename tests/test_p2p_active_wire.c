#include "../src/p2p/p2p_active_wire.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;assert(x);} while(0)
static int sha(const void *p,size_t n,unsigned char out[32])
{return mbedtls_sha256_ret(p,n,out,0)==0;}
static unsigned char *readfile(const char *name,size_t *n)
{FILE *f=fopen(name,"rb");long len;unsigned char *p;CHECK(f);CHECK(!fseek(f,0,SEEK_END));len=ftell(f);CHECK(len>=0&&len<4096);CHECK(!fseek(f,0,SEEK_SET));p=malloc(len+1);CHECK(p);CHECK(fread(p,1,len,f)==(size_t)len);CHECK(!fclose(f));*n=len;return p;}
int main(int argc,char **argv)
{
    unsigned char *scripts,*bundle,*nonce,*hello,*raw,*expected,*bad,actual[192],ack[96];
    size_t sn,bn,nn,hn,n,an,i; p2p_startup_view_t view; p2pa_bootstrap_t boot;
    p2pa_control_t command,variant; p2pa_policy_t policy,copy;
    CHECK(argc==10);scripts=readfile(argv[1],&sn);bundle=readfile(argv[2],&bn);nonce=readfile(argv[3],&nn);hello=readfile(argv[4],&hn);
    CHECK(nn==32&&hn==192);memset(&view,0,sizeof(view));view.valid=1;view.scripts=scripts;view.scripts_length=sn;
    view.principals=bundle;view.principals_length=bn;memset(view.nonce,'s',32);memset(view.binding,'b',32);
    CHECK(P2PACaptureBootstrap(&view,&boot));CHECK(P2PAMakeHello(&boot,nonce,actual));CHECK(!memcmp(actual,hello,192));
    /* Captured scope survives original startup packet reclamation. */
    memset(scripts,0,sn);memset(bundle,0,bn);free(scripts);free(bundle);
    P2PAInitPolicy(&policy,&boot);
    for(i=0;i<3;i++) {
        uint64_t wall=1001000+i*1000,mono=1000+i*1000;
        raw=readfile(argv[5+i],&n);expected=readfile(argv[8],&an);
        CHECK(P2PAParseControl(&boot,hello,raw,n,sha,&command));
        CHECK(P2PAPrepare(&policy,&command,wall,mono));
        if(i==0) {
            size_t k;bad=malloc(n);CHECK(bad);
            for(k=0;k<n;k++){memcpy(bad,raw,n);bad[k]^=1;CHECK(!P2PAParseControl(&boot,hello,bad,n,sha,&command));}
            for(k=0;k<n;k++)CHECK(!P2PAParseControl(&boot,hello,raw,k,sha,&command));
            free(bad);CHECK(P2PAParseControl(&boot,hello,raw,n,sha,&command));
            copy=policy;CHECK(!P2PACommit(&copy,&command,2,wall,mono));CHECK(copy.dead);
            copy=policy;CHECK(!P2PAPrepare(&copy,&command,1006001,mono));
            copy=policy;CHECK(P2PACommit(&copy,&command,1,wall,mono+5000));
            CHECK(copy.deadline==mono+command.expires-wall);
            CHECK(copy.hard_deadline==mono+command.hard_expires-wall);
            copy=policy;CHECK(!P2PACommit(&copy,&command,1,wall,mono+30000));CHECK(copy.dead);
            /* Pure policy cases operate on decoded typed metadata; they do
             * not substitute for signature/wire validation. */
            variant=command;variant.owner_deadline_low=(uint32_t)mono;
            P2PAInitPolicy(&copy,&boot);CHECK(!P2PAPrepare(&copy,&variant,wall,mono));
            variant.owner_deadline_low=(uint32_t)(mono+30001);
            P2PAInitPolicy(&copy,&boot);CHECK(!P2PAPrepare(&copy,&variant,wall,mono));
            variant.owner_deadline_low=(uint32_t)(mono-1);
            P2PAInitPolicy(&copy,&boot);CHECK(!P2PAPrepare(&copy,&variant,wall,mono));
            variant.owner_deadline_low=(uint32_t)(mono+1000);
            P2PAInitPolicy(&copy,&boot);CHECK(P2PAPrepare(&copy,&variant,wall,mono));
            CHECK(copy.offered_deadline==mono+1000);
            CHECK(P2PAPrepare(&copy,&variant,wall,mono+500));
            CHECK(P2PACommit(&copy,&variant,1,wall,mono+500));CHECK(copy.deadline==mono+1000);
            CHECK(!P2PAObserve(&copy,wall,mono+1000));
            P2PAInitPolicy(&copy,&boot);CHECK(P2PAPrepare(&copy,&variant,wall,mono));
            CHECK(!P2PACommit(&copy,&variant,1,wall,mono+1000));CHECK(copy.dead);
            /* A low32 value of zero is a valid future deadline across wrap. */
            variant.owner_deadline_low=0;
            P2PAInitPolicy(&copy,&boot);CHECK(P2PAPrepare(&copy,&variant,wall,UINT64_C(0x100000000)-100));
            CHECK(copy.offered_owner_deadline==UINT64_C(0x100000000));
            CHECK(P2PACommit(&copy,&variant,1,wall,UINT64_C(0x100000000)-1));
            CHECK(!P2PAObserve(&copy,wall,UINT64_C(0x100000000)));
            variant.expires=variant.hard_expires=wall+50;variant.issued=wall;
            variant.owner_deadline_low=49; /* +150 would overflow full uint64. */
            P2PAInitPolicy(&copy,&boot);CHECK(!P2PAPrepare(&copy,&variant,wall,UINT64_MAX-100));
            variant=command;variant.operation=P2PA_REVOKE;variant.owner_deadline_low=1;
            P2PAInitPolicy(&copy,&boot);CHECK(!P2PAPrepare(&copy,&variant,wall,mono));
        }
        CHECK(P2PACommit(&policy,&command,1,wall,mono));
        CHECK(P2PAMakeAck(&command,hello+16,1,policy.native_sequence,ack));
        if(i==0){CHECK(an==96);CHECK(!memcmp(ack,expected,96));FILE *f=fopen(argv[9],"wb");CHECK(f);CHECK(fwrite(ack,1,96,f)==96);CHECK(!fclose(f));}
        free(expected);free(raw);
        if(i==0){copy=policy;CHECK(!P2PAObserve(&copy,wall-1,mono+1));CHECK(!P2PAObserve(&copy,wall,mono+2));
            copy=policy;CHECK(!P2PAObserve(&copy,wall+1,mono-1));CHECK(!P2PAObserve(&copy,wall+2,mono));
            copy=policy;CHECK(!P2PAObserve(&copy,wall+1,policy.deadline));}
        if(i==1){copy=policy;command.sequence=policy.sequence;CHECK(!P2PAPrepare(&copy,&command,wall,mono));}
    }
    CHECK(policy.dead);free(nonce);free(hello);printf("PASS %u active codec/policy checks\n",checks);return 0;
}

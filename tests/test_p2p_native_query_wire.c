/* Independent Python bytes, actual startup/query codec and actual SHA256.
 * Reply times are supplied fixture values, not native observation evidence. */
#include "../p2p_active_wire.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;assert(x);}while(0)
static int sha(const void *p,size_t n,unsigned char out[32]){return !mbedtls_sha256_ret(p,n,out,0);}
static unsigned char *load(const char *name,size_t *length)
{FILE *f=fopen(name,"rb");long n;unsigned char *p;CHECK(f);CHECK(!fseek(f,0,SEEK_END));n=ftell(f);CHECK(n>=0&&n<4096);rewind(f);p=malloc(n+1);CHECK(p);CHECK(fread(p,1,n,f)==(size_t)n);fclose(f);*length=n;return p;}
int main(int argc,char **argv)
{
    size_t qn,en,hn,n,rn,i,k;unsigned char *request,*envelope,*hello,*raw,*expected,*bad,actual[192],reply[96];
    p2p_startup_view_t view;p2pa_bootstrap_t boot;p2pa_query_t query;p2pa_control_t control;
    CHECK(argc==11);request=load(argv[1],&qn);envelope=load(argv[2],&en);hello=load(argv[3],&hn);CHECK(hn==192);
    CHECK(P2PStartupParseEnvelope(request,qn,envelope,en,sha,&view));CHECK(P2PACaptureBootstrap(&view,&boot));
    memset(envelope,0,en);free(envelope);free(request);
    CHECK(P2PAMakeHello(&boot,hello+16,actual));CHECK(!memcmp(actual,hello,192));
    for(i=0;i<3;++i) {
        raw=load(argv[4+2*i],&n);expected=load(argv[5+2*i],&rn);CHECK(rn==96);
        CHECK(P2PAParseQuery(&boot,hello,raw,n,sha,&query));
        CHECK(!P2PAParseControl(&boot,hello,raw,n,sha,&control));
        CHECK(P2PAMakeQueryReply(&query,i==2?0:1,i==2?0:30100,i==0?100:i==1?20100:0,reply));
        CHECK(!memcmp(reply,expected,96));
        if(i==0) {
            FILE *f=fopen(argv[10],"wb");CHECK(f);CHECK(fwrite(reply,1,96,f)==96);fclose(f);
            bad=malloc(n);CHECK(bad);
            for(k=0;k<n;++k){memcpy(bad,raw,n);bad[k]^=1;CHECK(!P2PAParseQuery(&boot,hello,bad,n,sha,&query));}
            for(k=0;k<n;++k)CHECK(!P2PAParseQuery(&boot,hello,raw,k,sha,&query));
            free(bad);CHECK(P2PAParseQuery(&boot,hello,raw,n,sha,&query));
            CHECK(!P2PAMakeQueryReply(&query,1,100,100,reply));
            for(k=0;k<96;++k)CHECK(!reply[k]);
        }
        free(raw);free(expected);
    }
    free(hello);printf("PASS %u current-scope query codec checks; no installer/authority claim\n",checks);return 0;
}

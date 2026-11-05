#include "../p2p_active_wire.h"
#include "mbedtls/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int sha(const void*p,size_t n,unsigned char out[32]){return mbedtls_sha256_ret(p,n,out,0)==0;}
static unsigned char *load(const char*s,size_t*n){FILE*f=fopen(s,"rb");long l;unsigned char*p;assert(f);assert(!fseek(f,0,SEEK_END));l=ftell(f);assert(l>=0&&l<4096);rewind(f);p=malloc(l+1);assert(p);assert(fread(p,1,l,f)==(size_t)l);fclose(f);*n=l;return p;}
int main(int argc,char**argv)
{
 unsigned char*q,*p,*nonce,*hello,*control,*expected,out[192],ack[96];size_t qn,pn,nn,hn,cn,an;
 p2p_startup_view_t startup;p2pa_bootstrap_t boot;p2pa_control_t command;p2pa_policy_t policy;
 assert(argc==7);q=load(argv[1],&qn);p=load(argv[2],&pn);nonce=load(argv[3],&nn);
 hello=load(argv[4],&hn);control=load(argv[5],&cn);expected=load(argv[6],&an);assert(nn==32&&hn==192&&an==96);
 assert(P2PStartupParseEnvelope(q,qn,p,pn,sha,&startup));assert(P2PACaptureBootstrap(&startup,&boot));
 memset(p,0,pn);free(p);free(q);assert(P2PAMakeHello(&boot,nonce,out));assert(!memcmp(out,hello,192));
 assert(P2PAParseControl(&boot,hello,control,cn,sha,&command));P2PAInitPolicy(&policy,&boot);
 assert(P2PAPrepare(&policy,&command,1000000,100));assert(P2PACommit(&policy,&command,1,1000000,100));
 assert(P2PAMakeAck(&command,hello+16,1,policy.native_sequence,ack));assert(!memcmp(ack,expected,96));
 free(nonce);free(hello);free(control);free(expected);puts("PASS independent parent golden actual startup/control/ACK");return 0;
}

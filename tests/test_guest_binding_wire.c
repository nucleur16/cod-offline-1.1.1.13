/* Portable C contract, no OS/SDK/game IO. Hash callback is explicitly FAKE:
 * this checks framing/currentness fields, never signing/authentication. */
#include "../src/p2p/p2p_guest_binding_wire.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned char q[256],envelope[688];static size_t qn;static int hash_ok=1;
static void u32(unsigned char *p,uint32_t n){unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
static void u64(unsigned char *p,uint64_t n){u32(p,(uint32_t)n);u32(p+4,(uint32_t)(n>>32));}
static int fake_hash(const void *p,size_t n,unsigned char out[32]){(void)p;(void)n;memset(out,0xa5,32);return hash_ok;}
static void make(int protocol,int dispatch)
{
 unsigned char nonce[32],*p=envelope+80;memset(nonce,0x11,32);hash_ok=1;
 if(protocol==1)assert(P2PStartupMakeRequest("mp_rust","war","mp",nonce,q,sizeof(q),&qn));
 else assert(P2PStartupMakeRequestV2("mp_rust","war","mp",0,-1,nonce,q,sizeof(q),&qn));
 memset(envelope,0,sizeof(envelope));memcpy(envelope,"CPGEv1\0\0",8);u32(envelope+8,(uint32_t)dispatch);u32(envelope+12,sizeof(envelope));memcpy(envelope+16,nonce,32);memset(envelope+48,0xa5,32);
 memset(p,1,608);memcpy(p,"CPGBv1\0\0",8);u32(p+8,608);u32(p+12,0);u64(p+24,1);u64(p+32,2);memset(p+112,2,16);memset(p+152,2,24);
 u64(p+80,100000);u64(p+88,110000);memset(p+368,0xa5,32);memset(p+400,protocol==2?1:0,32);u32(p+464,1);u32(p+468,1);memcpy(p+512,"\x7f\0\0\1",4);p[516]=0x38;p[517]=0x4a;p[518]=p[519]=0;u32(p+568,1);u32(p+572,0);
}
static p2pg_view_t parsed(void){p2pg_view_t v;assert(P2PGuestBindingParse(q,qn,envelope,sizeof(envelope),fake_hash,&v));return v;}
static void refuse(void){p2pg_view_t v;memset(&v,0x7f,sizeof(v));assert(!P2PGuestBindingParse(q,qn,envelope,sizeof(envelope),fake_hash,&v));assert(v.valid==0);}
int main(void)
{
 p2pg_view_t v;unsigned char ack[76],command[88],queued[80];unsigned port;int protocol;
 for(protocol=1;protocol<=2;protocol++){
  make(protocol,0);v=parsed();assert(v.protocol==(unsigned)protocol&&v.connect_dispatch==0);assert(P2PGuestBindingAck(&v,1,ack));assert(!memcmp(ack,"CPGAv1\0\0",8));assert(!memcmp(ack+8,v.nonce,32));assert(!memcmp(ack+40,v.binding,32));assert(ack[72]==1);
  envelope[48]^=1;refuse();envelope[48]^=1;envelope[80+368]^=1;refuse();envelope[80+368]^=1;hash_ok=0;refuse();
 }
 make(1,0);u64(envelope+80+88,130001);refuse();make(1,0);u32(envelope+80+568,0);refuse();make(1,0);u32(envelope+80+572,1);refuse();make(1,0);envelope[80+512]=126;refuse();
 make(1,1);v=parsed();memset(command,0,sizeof(command));memcpy(command,"CPGDv1\0\0",8);memcpy(command+8,v.nonce,32);memcpy(command+40,v.binding,32);u32(command+72,20000);u64(command+76,110000);
 assert(P2PGuestConnectParse(&v,command,sizeof(command),&port)&&port==20000);assert(P2PGuestConnectQueued(&v,port,1,queued));assert(!memcmp(queued,"CPGQv1\0\0",8));assert(!memcmp(queued+8,v.nonce,32));assert(!memcmp(queued+40,v.binding,32));assert(queued[76]==1);
 v.connect_dispatch=0;port=999;assert(!P2PGuestConnectParse(&v,command,sizeof(command),&port)&&port==0);v.connect_dispatch=1;
 command[8]^=1;assert(!P2PGuestConnectParse(&v,command,sizeof(command),&port));command[8]^=1;command[40]^=1;assert(!P2PGuestConnectParse(&v,command,sizeof(command),&port));command[40]^=1;
 u64(command+76,110001);assert(!P2PGuestConnectParse(&v,command,sizeof(command),&port));u64(command+76,110000);u32(command+84,1);assert(!P2PGuestConnectParse(&v,command,sizeof(command),&port));u32(command+84,0);
 u32(command+72,1023);assert(!P2PGuestConnectParse(&v,command,sizeof(command),&port));u32(command+72,65536);assert(!P2PGuestConnectParse(&v,command,sizeof(command),&port));u32(command+72,19000);assert(!P2PGuestConnectParse(&v,command,sizeof(command),&port));
 u32(command+72,1024);assert(P2PGuestConnectParse(&v,command,sizeof(command),&port)&&port==1024);u32(command+72,65535);assert(P2PGuestConnectParse(&v,command,sizeof(command),&port)&&port==65535);
 assert(!P2PGuestConnectQueued(&v,20000,2,queued));puts("Guest binding framing/queued-only fake-hash checks passed");return 0;
}

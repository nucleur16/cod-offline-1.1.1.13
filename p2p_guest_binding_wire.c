#include "p2p_guest_binding_wire.h"
#include <string.h>
static uint16_t p2pg_u16(const unsigned char *p)
{ return (uint16_t)((unsigned)p[0]|(unsigned)p[1]<<8); }
static uint32_t p2pg_u32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint64_t p2pg_u64(const unsigned char *p)
{ return (uint64_t)p2pg_u32(p)|(uint64_t)p2pg_u32(p+4)<<32; }
static void p2pg_put32(unsigned char *p,uint32_t v)
{ unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8)); }
static int p2pg_nonzero(const unsigned char *p,size_t n)
{ unsigned v=0;size_t i;for(i=0;i<n;++i)v|=p[i];return v!=0; }
static int p2pg_same(const unsigned char *a,const unsigned char *b,size_t n)
{ unsigned v=0;size_t i;for(i=0;i<n;++i)v|=a[i]^b[i];return v==0; }
static int p2pg_packet(const unsigned char *p)
{
    static const unsigned u64s[]={16,24,32,56,64,72,80,88,472};
    static const unsigned blocks[][2]={{40,16},{96,16},{112,16},{128,24},{152,24},
        {176,32},{208,32},{240,32},{272,32},{304,32},{336,32},{368,32},
        {432,32},{480,32},{520,32},{552,16},{576,32}};
    unsigned i;uint64_t issued,expires;uint16_t port;
    if(memcmp(p,"CPGBv1\0\0",8)||p2pg_u32(p+8)!=P2PG_PACKET_SIZE||p2pg_u32(p+12)||p2pg_u16(p+518)||p2pg_u32(p+572)||
       !p2pg_u32(p+568)||p2pg_u32(p+568)==0xffffffffu)return 0;
    for(i=0;i<sizeof(u64s)/sizeof(u64s[0]);++i)if(!p2pg_u64(p+u64s[i]))return 0;
    for(i=0;i<sizeof(blocks)/sizeof(blocks[0]);++i)if(!p2pg_nonzero(p+blocks[i][0],blocks[i][1]))return 0;
    if(p2pg_u64(p+24)==p2pg_u64(p+32)||p2pg_same(p+96,p+112,16)||p2pg_same(p+128,p+152,24))return 0;
    issued=p2pg_u64(p+80);expires=p2pg_u64(p+88);
    if(expires<=issued||expires-issued>30000u||!p2pg_u32(p+464)||p2pg_u32(p+464)>8192u||
        p2pg_u32(p+468)>64u*1024u*1024u||memcmp(p+512,"\x7f\0\0\1",4))return 0;
    port=p2pg_u16(p+516);return port&&port!=17760u&&port!=17761u;
}
int P2PGuestBindingParse(const void *request,size_t qn,const void *envelope,size_t n,
    p2p_startup_sha256_fn sha,p2pg_view_t *output)
{
    static const unsigned char domain[]="CODOL-P2P-GUEST-BINDING-V1";
    const unsigned char *q=request,*p=envelope,*body;
    unsigned char qhash[32],binding[32],input[sizeof(domain)+4+256+P2PG_PACKET_SIZE];
    size_t at=0;int protocol;p2pg_view_t result;
    if(output)memset(output,0,sizeof(*output));
    if(!output||!q||!p||!sha||qn>256u||n!=P2PG_ENVELOPE_SIZE)return 0;
    protocol=P2PStartupRequestProtocol(q,qn);if(protocol!=1&&protocol!=2)return 0;
    if(memcmp(p,"CPGEv1\0\0",8)||p2pg_u32(p+8)>1u||p2pg_u32(p+12)!=n||!p2pg_same(p+16,q+16,32))return 0;
    body=p+P2PG_ENVELOPE_HEADER;
    if(!p2pg_packet(body)||sha(q,qn,qhash)!=1||!p2pg_same(qhash,body+368,32)||
        (p2pg_nonzero(body+400,32)!=(protocol==2)))return 0;
    memcpy(input,domain,sizeof(domain));at+=sizeof(domain);
    p2pg_put32(input+at,(uint32_t)qn);at+=4;memcpy(input+at,q,qn);at+=qn;
    memcpy(input+at,body,P2PG_PACKET_SIZE);at+=P2PG_PACKET_SIZE;
    if(sha(input,at,binding)!=1||!p2pg_same(binding,p+48,32))return 0;
    memset(&result,0,sizeof(result));memcpy(result.packet,body,P2PG_PACKET_SIZE);
    memcpy(result.nonce,q+16,32);memcpy(result.binding,binding,32);
    result.protocol=(unsigned)protocol;result.valid=1;result.connect_dispatch=p2pg_u32(p+8);*output=result;return 1;
}
int P2PGuestBindingAck(const p2pg_view_t *view,int bound_result,unsigned char out[P2PG_ACK_SIZE])
{
    if(!view||!out||view->valid!=1||(view->protocol!=1&&view->protocol!=2)||
        (bound_result!=0&&bound_result!=1)||!p2pg_nonzero(view->nonce,32)||!p2pg_nonzero(view->binding,32))return 0;
    memcpy(out,"CPGAv1\0\0",8);memcpy(out+8,view->nonce,32);memcpy(out+40,view->binding,32);
    p2pg_put32(out+72,(uint32_t)bound_result);return 1;
}

/* Explicit optional local command request: it never grants readiness/ingress. */
int P2PGuestConnectParse(const p2pg_view_t *v,const unsigned char *p,size_t n,unsigned *port)
{
    unsigned x;if(port)*port=0;
    if(!v||!p||!port||v->valid!=1||v->connect_dispatch!=1||n!=P2PG_DISPATCH_SIZE||
       memcmp(p,"CPGDv1\0\0",8)||!p2pg_same(p+8,v->nonce,32)||!p2pg_same(p+40,v->binding,32)||
       p2pg_u64(p+76)!=p2pg_u64(v->packet+88)||p2pg_u32(p+84))return 0;
    x=p2pg_u32(p+72);if(x<1024u||x>65535u||x==p2pg_u16(v->packet+516))return 0;
    *port=x;return 1;
}
int P2PGuestConnectQueued(const p2pg_view_t *v,unsigned port,int queued,unsigned char out[P2PG_QUEUED_SIZE])
{
    if(!v||!out||v->valid!=1||v->connect_dispatch!=1||port<1024u||port>65535u||(queued!=0&&queued!=1))return 0;
    memcpy(out,"CPGQv1\0\0",8);memcpy(out+8,v->nonce,32);memcpy(out+40,v->binding,32);
    p2pg_put32(out+72,port);p2pg_put32(out+76,(uint32_t)queued);return 1;
}

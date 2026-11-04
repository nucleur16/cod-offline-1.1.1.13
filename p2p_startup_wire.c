#include "p2p_startup_wire.h"
#include <string.h>

static uint16_t p2ps_le16(const unsigned char *p)
{ return (uint16_t)((unsigned)p[0]|(unsigned)p[1]<<8); }
static uint32_t p2ps_le32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void p2ps_put16(unsigned char *p,uint16_t value)
{ p[0]=(unsigned char)value;p[1]=(unsigned char)(value>>8); }
static void p2ps_put32(unsigned char *p,uint32_t value)
{ unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i)); }
static int p2ps_nonzero(const unsigned char *p,size_t size)
{ unsigned n=0;size_t i;for(i=0;i<size;++i)n|=p[i];return n!=0; }
static int p2ps_equal(const unsigned char *a,const unsigned char *b,size_t size)
{ unsigned n=0;size_t i;for(i=0;i<size;++i)n|=(unsigned)(a[i]^b[i]);return n==0; }
static int p2ps_identifier(const unsigned char *p,size_t size)
{
    size_t i;if(!size||size>64||p[0]<'a'||p[0]>'z')return 0;
    for(i=1;i<size;++i)if(!((p[i]>='a'&&p[i]<='z')||
        (p[i]>='0'&&p[i]<='9')||p[i]=='_'))return 0;
    return 1;
}
static size_t p2ps_identifier_length(const char *text)
{
    size_t n;if(!text)return 0;
    for(n=0;n<=64&&text[n];++n){}
    return p2ps_identifier((const unsigned char *)text,n)?n:0;
}
static int p2ps_request_valid(const unsigned char *p,size_t length)
{
    size_t a,b,c;
    if(!p||length<P2P_STARTUP_REQUEST_HEADER||length>P2P_STARTUP_MAX_REQUEST||
        memcmp(p,"CPSQv1\0\0",8)||p2ps_le32(p+8)||p2ps_le32(p+12)!=length||
        !p2ps_nonzero(p+16,32)||p2ps_le16(p+54))return 0;
    a=p2ps_le16(p+48);b=p2ps_le16(p+50);c=p2ps_le16(p+52);
    return a<=64&&b<=64&&c<=64&&length==56+a+b+c&&
        p2ps_identifier(p+56,a)&&p2ps_identifier(p+56+a,b)&&p2ps_identifier(p+56+a+b,c);
}
int P2PStartupMakeRequest(const char *map,const char *gametype,const char *category,
    const unsigned char nonce[32],void *output,size_t capacity,size_t *length)
{
    unsigned char *p=output;
    size_t a=p2ps_identifier_length(map),b=p2ps_identifier_length(gametype),c=p2ps_identifier_length(category),total;
    if(length)*length=0;
    if(!length||!p||!nonce||!p2ps_nonzero(nonce,32)||!a||!b||!c)return 0;
    total=P2P_STARTUP_REQUEST_HEADER+a+b+c;if(total>capacity)return 0;
    memset(p,0,P2P_STARTUP_REQUEST_HEADER);memcpy(p,"CPSQv1\0\0",8);
    p2ps_put32(p+12,(uint32_t)total);memcpy(p+16,nonce,32);
    p2ps_put16(p+48,(uint16_t)a);p2ps_put16(p+50,(uint16_t)b);p2ps_put16(p+52,(uint16_t)c);
    memcpy(p+56,map,a);memcpy(p+56+a,gametype,b);memcpy(p+56+a+b,category,c);
    *length=total;return 1;
}
int P2PStartupParseEnvelope(const void *request,size_t request_length,
    const void *envelope,size_t envelope_length,p2p_startup_sha256_fn sha256,
    p2p_startup_view_t *output)
{
    static const unsigned char domain[]="CODOL-P2P-STARTUP-V1";
    const unsigned char *q=request,*p=envelope,*scripts,*principals;
    unsigned char script_hash[32],principal_hash[32],binding_hash[32];
    unsigned char binding_input[sizeof(domain)+P2P_STARTUP_MAX_REQUEST+8+64];
    uint32_t script_length,principal_length,count;size_t at=0;
    p2p_startup_view_t result;
    if(!output)return 0;
    memset(output,0,sizeof(*output));
    if(!p2ps_request_valid(q,request_length)||!p||!sha256||
        envelope_length<P2P_STARTUP_ENVELOPE_HEADER||envelope_length>P2P_STARTUP_MAX_ENVELOPE||
        memcmp(p,"CPSEv1\0\0",8)||p2ps_le32(p+8)||p2ps_le32(p+12)!=envelope_length||
        !p2ps_equal(p+24,q+16,32))return 0;
    script_length=p2ps_le32(p+16);principal_length=p2ps_le32(p+20);
    if(script_length<48||script_length>P2P_STARTUP_MAX_CVSP||principal_length<112||
        principal_length>P2P_STARTUP_MAX_PRINCIPALS||
        envelope_length!=P2P_STARTUP_ENVELOPE_HEADER+(size_t)script_length+principal_length)return 0;
    scripts=p+P2P_STARTUP_ENVELOPE_HEADER;principals=scripts+script_length;
    if(memcmp(scripts,"CVSPv1\0\0",8)||memcmp(principals,"CPRBv1\0\0",8))return 0;
    count=p2ps_le32(principals+72);
    if(!count||count>18||principal_length!=80+32*count||p2ps_le32(principals+76))return 0;
    if(sha256(scripts,script_length,script_hash)!=1||sha256(principals,principal_length,principal_hash)!=1||
        !p2ps_equal(script_hash,p+56,32)||!p2ps_equal(principal_hash,p+88,32))return 0;
    memcpy(binding_input+at,domain,sizeof(domain));at+=sizeof(domain);
    memcpy(binding_input+at,q,request_length);at+=request_length;
    p2ps_put32(binding_input+at,script_length);at+=4;
    p2ps_put32(binding_input+at,principal_length);at+=4;
    memcpy(binding_input+at,script_hash,32);at+=32;memcpy(binding_input+at,principal_hash,32);at+=32;
    if(sha256(binding_input,at,binding_hash)!=1||!p2ps_equal(binding_hash,p+120,32))return 0;
    memset(&result,0,sizeof(result));
    result.scripts=scripts;result.principals=principals;
    result.scripts_length=script_length;result.principals_length=principal_length;
    memcpy(result.nonce,p+24,32);memcpy(result.binding,binding_hash,32);result.valid=1;
    *output=result;return 1;
}
int P2PStartupMakeAck(const p2p_startup_view_t *view,int scripts_result,int principals_result,
    unsigned char output[P2P_STARTUP_ACK_SIZE])
{
    int32_t result;
    if(!view||view->valid!=1||!output)return 0;
    result=scripts_result==1?(int32_t)principals_result:0;
    memset(output,0,P2P_STARTUP_ACK_SIZE);memcpy(output,"CPSAv1\0\0",8);
    memcpy(output+8,view->nonce,32);memcpy(output+40,view->binding,32);
    p2ps_put32(output+72,(uint32_t)result);return 1;
}

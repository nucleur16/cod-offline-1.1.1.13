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
static int p2ps_v2_intent(int present,int difficulty)
{ return (present==0&&difficulty==-1)||(present==1&&difficulty>=0&&difficulty<=3); }
int P2PStartupMakeRequestV2(const char *map,const char *gametype,const char *category,
    int present,int difficulty,const unsigned char nonce[32],void *output,size_t capacity,size_t *length)
{
    unsigned char *p=output;
    size_t a=p2ps_identifier_length(map),b=p2ps_identifier_length(gametype),c=p2ps_identifier_length(category),total;
    if(length)*length=0;
    if(!length||!p||!nonce||!p2ps_nonzero(nonce,32)||!a||!b||!c||!p2ps_v2_intent(present,difficulty))return 0;
    total=P2P_STARTUP_V2_REQUEST_HEADER+a+b+c;if(total>capacity)return 0;
    memset(p,0,P2P_STARTUP_V2_REQUEST_HEADER);memcpy(p,"CPSQv2\0\0",8);
    p2ps_put32(p+12,(uint32_t)total);memcpy(p+16,nonce,32);
    p2ps_put16(p+48,(uint16_t)a);p2ps_put16(p+50,(uint16_t)b);p2ps_put16(p+52,(uint16_t)c);
    p[56]=(unsigned char)difficulty;p[57]=(unsigned char)present;
    memcpy(p+64,map,a);memcpy(p+64+a,gametype,b);memcpy(p+64+a+b,category,c);
    *length=total;return 1;
}
static int p2ps_v2_request_valid(const unsigned char *p,size_t n)
{
    size_t a,b,c,i;int difficulty;
    if(!p||n<64||n>P2P_STARTUP_V2_MAX_REQUEST||memcmp(p,"CPSQv2\0\0",8)||
       p2ps_le32(p+8)||p2ps_le32(p+12)!=n||!p2ps_nonzero(p+16,32)||p2ps_le16(p+54))return 0;
    difficulty=p[56]==255?-1:(int)p[56];
    if(!p2ps_v2_intent(p[57],difficulty))return 0;
    for(i=58;i<64;++i)if(p[i])return 0;
    a=p2ps_le16(p+48);b=p2ps_le16(p+50);c=p2ps_le16(p+52);
    return a<=64&&b<=64&&c<=64&&n==64+a+b+c&&
        p2ps_identifier(p+64,a)&&p2ps_identifier(p+64+a,b)&&p2ps_identifier(p+64+a+b,c);
}
static int p2ps_fixed_identifier(const unsigned char *p,size_t n)
{
    size_t end=0,i;while(end<n&&p[end])++end;
    if(end==n||!p2ps_identifier(p,end))return 0;
    for(i=end;i<n;++i)if(p[i])return 0;
    return 1;
}
static int p2ps_request_field(const unsigned char *q,size_t offset,size_t length,const unsigned char *field,size_t capacity)
{ return length<capacity&&!memcmp(q+offset,field,length)&&field[length]==0; }
static int p2ps_v2_options(const unsigned char *q,const unsigned char *s,const unsigned char *r,const unsigned char *m)
{
    size_t a=p2ps_le16(q+48),b=p2ps_le16(q+50),c=p2ps_le16(q+52);
    uint32_t difficulty=p2ps_le32(m+120),seats=p2ps_le32(m+144),humans=p2ps_le32(m+148);
    const char *base,*category;
    if(memcmp(m,"CPMOv1\0\0",8)||p2ps_le32(m+8)!=352||p2ps_le32(m+12)||
       !p2ps_nonzero(m+16,32)||!p2ps_nonzero(m+48,32)||!p2ps_nonzero(m+80,16)||
       !p2ps_nonzero(m+96,8)||!p2ps_nonzero(m+104,8)||!p2ps_nonzero(m+112,8)||
       memcmp(m+48,s+16,32)||memcmp(m+80,r+8,16)||memcmp(m+96,r+24,24)||
       (difficulty!=0xffffffffu&&difficulty>3)||p2ps_le32(m+124)>1||p2ps_le32(m+128)>2||
       p2ps_le32(m+132)>65535||p2ps_le32(m+136)>400||p2ps_le32(m+140)>400||
       !seats||seats>18||!humans||humans>seats||p2ps_le32(m+152)>18||p2ps_le32(m+156)>18||
       p2ps_le32(m+160)>18||p2ps_le32(m+164)>2||p2ps_le32(m+168)>1||p2ps_le32(m+172)||
       !p2ps_fixed_identifier(m+176,64)||!p2ps_fixed_identifier(m+240,64)||
       !p2ps_fixed_identifier(m+304,32)||!p2ps_fixed_identifier(m+336,16)||
       !p2ps_request_field(q,64,a,m+240,64)||!p2ps_request_field(q,64+a,b,m+304,32)||
       !p2ps_request_field(q,64+a+b,c,m+336,16))return 0;
    category=(const char *)m+336;base=(const char *)m+240;
    if(strcmp(category,"mp")&&strcmp(category,"zm")&&strcmp(category,"coop")&&strcmp(category,"survival"))return 0;
    if(!strncmp(base,"coop_",5)) { base=strstr(base,"_mp_");if(!base)return 0;++base; }
    if(strcmp(base,(const char *)m+176))return 0;
    /* ZM has a real selected tier; finite0 must never become a default1.
     * Other categories can have a raw UI tier but no effective native tier.
     * The trusted SDK, not this byte parser, authenticates that distinction. */
    if(!strcmp(category,"zm")&&
       (q[57]!=1||difficulty!=(uint32_t)q[56]))return 0;
    if(!strcmp(category,"mp")&&
       ((p2ps_le32(m+124)&&(q[57]!=1||difficulty!=(uint32_t)q[56]||difficulty>2))||
        (!p2ps_le32(m+124)&&(q[57]||q[56]!=255))))return 0;
    return 1;
}
static int p2ps_parse_v2(const unsigned char *q,size_t qn,const unsigned char *p,size_t n,
    p2p_startup_sha256_fn sha256,p2p_startup_view_t *output)
{
    static const unsigned char domain[]="CODOL-P2P-STARTUP-V2";
    unsigned char sh[32],rh[32],mh[32],binding[32],input[sizeof(domain)+P2P_STARTUP_V2_MAX_REQUEST+12+96];
    const unsigned char *scripts,*principals,*options;uint32_t sn,rn,mn,count;size_t at;
    p2p_startup_view_t result;
    if(!p2ps_v2_request_valid(q,qn)||!p||!sha256||n<192||n>P2P_STARTUP_V2_MAX_ENVELOPE||
       memcmp(p,"CPSEv2\0\0",8)||p2ps_le32(p+8)||p2ps_le32(p+12)!=n||p2ps_le32(p+28)||
       !p2ps_equal(p+32,q+16,32))return 0;
    sn=p2ps_le32(p+16);rn=p2ps_le32(p+20);mn=p2ps_le32(p+24);
    if(sn<48||sn>P2P_STARTUP_MAX_CVSP||rn<112||rn>P2P_STARTUP_MAX_PRINCIPALS||mn!=352||
       n!=192+(size_t)sn+rn+mn)return 0;
    scripts=p+192;principals=scripts+sn;options=principals+rn;
    if(memcmp(scripts,"CVSPv1\0\0",8)||memcmp(principals,"CPRBv1\0\0",8))return 0;
    count=p2ps_le32(principals+72);
    if(!count||count>18||rn!=80+32*count||p2ps_le32(principals+76)||
       !p2ps_v2_options(q,scripts,principals,options))return 0;
    if(sha256(scripts,sn,sh)!=1||sha256(principals,rn,rh)!=1||sha256(options,mn,mh)!=1||
       !p2ps_equal(sh,p+64,32)||!p2ps_equal(rh,p+96,32)||!p2ps_equal(mh,p+128,32))return 0;
    memcpy(input,domain,sizeof(domain));at=sizeof(domain);memcpy(input+at,q,qn);at+=qn;
    p2ps_put32(input+at,sn);p2ps_put32(input+at+4,rn);p2ps_put32(input+at+8,mn);at+=12;
    memcpy(input+at,sh,32);memcpy(input+at+32,rh,32);memcpy(input+at+64,mh,32);at+=96;
    if(sha256(input,at,binding)!=1||!p2ps_equal(binding,p+160,32))return 0;
    memset(&result,0,sizeof(result));result.scripts=scripts;result.principals=principals;result.options=options;
    result.scripts_length=sn;result.principals_length=rn;result.options_length=mn;result.protocol=2;result.valid=1;
    memcpy(result.nonce,p+32,32);memcpy(result.binding,binding,32);*output=result;return 1;
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
    if(q&&request_length>=8&&!memcmp(q,"CPSQv2\0\0",8))
        return p2ps_parse_v2(q,request_length,p,envelope_length,sha256,output);
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
    result.protocol=1;
    memcpy(result.nonce,p+24,32);memcpy(result.binding,binding_hash,32);result.valid=1;
    *output=result;return 1;
}
int P2PStartupMakeAck(const p2p_startup_view_t *view,int scripts_result,int principals_result,
    unsigned char output[P2P_STARTUP_ACK_SIZE])
{
    int32_t result;
    if(!view||view->valid!=1||view->protocol==2||!output)return 0;
    result=scripts_result==1?(int32_t)principals_result:0;
    memset(output,0,P2P_STARTUP_ACK_SIZE);memcpy(output,"CPSAv1\0\0",8);
    memcpy(output+8,view->nonce,32);memcpy(output+40,view->binding,32);
    p2ps_put32(output+72,(uint32_t)result);return 1;
}
int P2PStartupMakeAckV2(const p2p_startup_view_t *view,int scripts,int principals,int options,
    unsigned char output[P2P_STARTUP_ACK_SIZE])
{
    if(!view||view->valid!=1||view->protocol!=2||!output)return 0;
    memset(output,0,P2P_STARTUP_ACK_SIZE);memcpy(output,"CPSAv2\0\0",8);
    memcpy(output+8,view->nonce,32);memcpy(output+40,view->binding,32);
    p2ps_put32(output+72,scripts==1&&principals==1&&options==1?1:0);return 1;
}

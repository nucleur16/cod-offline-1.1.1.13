/* Pure transport/binding checks, used by the actual Windows receiver. These
 * functions do not verify signatures, install bytes or assert Ready/ACTIVE. */
#ifndef CODOL_P2P_PRODUCTION_HANDOFF_WIRE_H
#define CODOL_P2P_PRODUCTION_HANDOFF_WIRE_H
#include "p2p_production_guest.h"
#include <string.h>
static uint32_t p2ph_u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t p2ph_u64(const unsigned char *p){return p2ph_u32(p)|(uint64_t)p2ph_u32(p+4)<<32;}
static void p2ph_put32(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
static void p2ph_put64(unsigned char *p,uint64_t n){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(n>>(i*8));}
static int p2ph_nonzero(const unsigned char *p,size_t n){unsigned v=0;while(n--)v|=*p++;return v!=0;}
static int P2PH_Install(const unsigned char *p,size_t n,const p2pp_scope *s,uint64_t uid,uint64_t generation,unsigned role,
    const unsigned char envelope_sha[32],uint64_t frame_deadline,uint64_t wall,uint64_t mono){
    uint64_t expires;
    if(!p||!s||!envelope_sha||(role!=1&&role!=2)||n!=(role==2?448u:192u)||
       memcmp(p,"CP2PIH01",8)||p2ph_u32(p+8)!=n||p2ph_u32(p+12)!=role||!uid||!generation||
       p2ph_u64(p+16)!=uid||p2ph_u64(p+24)!=generation||memcmp(p+32,s->request_sha256,32)||
       memcmp(p+64,s->nonce,32)||!p2ph_nonzero(p+96,16)||!p2ph_nonzero(p+112,32)||
       !wall||!mono||frame_deadline<=mono||frame_deadline>s->original_deadline||p2ph_u64(p+144)!=frame_deadline||
       memcmp(p+160,envelope_sha,32))return 0;
    expires=p2ph_u64(p+152);if(expires<=wall||expires-wall>5000)return 0;
    return role==1||P2PRGuestParse(p+192,256,s,uid,generation,wall,mono);
}
static int P2PH_Release(const unsigned char *p,size_t n,const p2pp_scope *s,uint64_t uid,uint64_t generation,unsigned role,
    const unsigned char initial[16],const unsigned char handoff[32],const unsigned char envelope_sha[32],
    uint64_t frame_deadline,uint64_t wall,uint64_t mono){
    uint64_t expires;
    if(!p||!s||!initial||!handoff||!envelope_sha||n!=224||(role!=1&&role!=2)||memcmp(p,"CP2PIRL1",8)||
       p2ph_u32(p+8)!=224||p2ph_u32(p+12)!=role||!uid||!generation||p2ph_u64(p+16)!=uid||p2ph_u64(p+24)!=generation||
       memcmp(p+32,s->request_sha256,32)||memcmp(p+64,s->nonce,32)||memcmp(p+96,initial,16)||memcmp(p+112,handoff,32)||
       !p2ph_nonzero(p+144,32)||!wall||!mono||frame_deadline<=mono||frame_deadline>s->original_deadline||
       p2ph_u64(p+176)!=frame_deadline||memcmp(p+192,envelope_sha,32))return 0;
    expires=p2ph_u64(p+184);return expires>wall&&expires-wall<=5000;
}
/* A read-only refresh names exactly the last ACK. It never carries install,
 * release, signature or deadline authority; currentness is checked by the owner. */
static int P2PH_AckQuery(const unsigned char *p,size_t n,const unsigned char actual_ack[192],uint64_t now){
    return p&&actual_ack&&now&&n==192&&!memcmp(p,"CP2PIQ02",8)&&!memcmp(actual_ack,"CP2PIK02",8)&&
        p2ph_u32(p+8)==192&&!memcmp(p+8,actual_ack+8,184)&&p2ph_u64(p+184)&&
        p2ph_u64(p+184)<=now&&p2ph_u64(p+176)>now;
}
static int P2PH_Ack(const p2pp_scope *s,uint64_t uid,uint64_t generation,unsigned role,const unsigned char initial[16],
    const unsigned char handoff[32],const unsigned char actual_scope_sha[32],uint64_t native_deadline,uint64_t observed,unsigned char out[192]){
    if(!s||!uid||!generation||(role!=1&&role!=2)||!initial||!handoff||!actual_scope_sha||!out||
       !p2ph_nonzero(initial,16)||!p2ph_nonzero(handoff,32)||!p2ph_nonzero(actual_scope_sha,32)||
       !observed||native_deadline<=observed||native_deadline>s->original_deadline)return 0;
    memset(out,0,192);memcpy(out,"CP2PIK02",8);p2ph_put32(out+8,192);p2ph_put32(out+12,role);p2ph_put64(out+16,uid);
    p2ph_put64(out+24,generation);memcpy(out+32,s->nonce,32);memcpy(out+64,s->request_sha256,32);memcpy(out+96,initial,16);
    memcpy(out+112,handoff,32);memcpy(out+144,actual_scope_sha,32);p2ph_put64(out+176,native_deadline);p2ph_put64(out+184,observed);return 1;
}
#endif

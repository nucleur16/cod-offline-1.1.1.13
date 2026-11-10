#include "p2p_production_guest.h"
#include <string.h>
static uint32_t pg_u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t pg_u64(const unsigned char *p){return pg_u32(p)|(uint64_t)pg_u32(p+4)<<32;}
static int pg_nonzero(const unsigned char *p,size_t n){unsigned a=0;while(n--)a|=*p++;return a!=0;}
int P2PRGuestParse(const unsigned char *p,size_t n,const p2pp_scope *s,uint64_t uid,
                   uint64_t generation,uint64_t wall,uint64_t mono) {
    uint64_t issued,expires;unsigned port;size_t i;
    if(!p||!s||n!=P2PR_GUEST_SIZE||memcmp(p,"CP2PGB02",8)||pg_u32(p+8)!=n||pg_u32(p+12)||
       !uid||!generation||pg_u64(p+16)!=uid||!pg_u64(p+24)||pg_u64(p+24)==uid||
       pg_u64(p+32)!=generation||pg_u32(p+56)!=s->parent_pid||pg_u32(p+60)!=s->child_pid||
       pg_u64(p+64)!=s->parent_birth||pg_u64(p+72)!=s->child_birth||
       memcmp(p+80,s->nonce,32)||memcmp(p+112,s->request_sha256,32)||
       !pg_nonzero(p+144,16)||!pg_nonzero(p+160,16)||!pg_nonzero(p+176,32)||!pg_nonzero(p+208,32)||
       !pg_u64(p+248)||!wall||!mono||mono>=s->original_deadline)return 0;
    issued=pg_u64(p+40);expires=pg_u64(p+48);port=(unsigned)p[244]|(unsigned)p[245]<<8;
    for(i=246;i<248;i++)if(p[i])return 0;
    return issued&&issued<=wall&&expires>wall&&expires>issued&&expires-issued<=30000&&port&&
        p[240]>0&&p[240]<224&&pg_u32(p+240)!=0&&pg_u32(p+240)!=0xffffffffu;
}

#include "p2p_production_wire.h"
#include <string.h>
static uint32_t p2pp_u32(const unsigned char *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint64_t p2pp_u64(const unsigned char *p) {
    return (uint64_t)p2pp_u32(p)|((uint64_t)p2pp_u32(p+4)<<32);
}
static int p2pp_nonzero(const unsigned char *p,size_t n) {
    unsigned char any=0;size_t i;for(i=0;i<n;i++)any|=p[i];return any!=0;
}
int P2PP_Parse(const void *packet,size_t length,const p2pp_scope *s,
               uint64_t now,p2pp_frame *out) {
    const unsigned char *p=packet;uint32_t kind,n;uint64_t sequence,deadline;
    if(out)memset(out,0,sizeof(*out));
    if(!out||!p||!s||length<P2PP_HEADER||length>P2PP_HEADER+P2PP_MAX_BODY||
       memcmp(p,"CP2PIP03",8)||!s->parent_pid||!s->child_pid||
       s->parent_pid==s->child_pid||!s->parent_birth||!s->child_birth||
       !s->original_deadline||now>=s->original_deadline||s->sequence==UINT64_MAX||
       !p2pp_nonzero(s->nonce,32)||!p2pp_nonzero(s->request_sha256,32))return 0;
    kind=p2pp_u32(p+8);n=p2pp_u32(p+12);sequence=p2pp_u64(p+16);
    deadline=p2pp_u64(p+112);
    if(kind<P2PP_CHALLENGE||kind>P2PP_RELEASE||!n||n>P2PP_MAX_BODY||
       length!=P2PP_HEADER+(size_t)n||sequence!=s->sequence+1||
       p2pp_u32(p+24)!=s->parent_pid||p2pp_u32(p+28)!=s->child_pid||
       p2pp_u64(p+32)!=s->parent_birth||p2pp_u64(p+40)!=s->child_birth||
       memcmp(p+48,s->nonce,32)||memcmp(p+80,s->request_sha256,32)||
       deadline<=now||deadline>s->original_deadline)return 0;
    out->kind=kind;out->length=n;out->sequence=sequence;
    out->deadline=deadline;out->body=p+P2PP_HEADER;return 1;
}

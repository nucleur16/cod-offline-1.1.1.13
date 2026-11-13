#include "p2p_active_wire.h"
#include <string.h>

static uint32_t p2pa_u32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint64_t p2pa_u64(const unsigned char *p)
{ return p2pa_u32(p)|((uint64_t)p2pa_u32(p+4)<<32); }
static void p2pa_put32(unsigned char *p,uint32_t n)
{ unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i)); }
static void p2pa_put64(unsigned char *p,uint64_t n)
{ p2pa_put32(p,(uint32_t)n);p2pa_put32(p+4,(uint32_t)(n>>32)); }
static int p2pa_nonzero(const unsigned char *p,size_t n)
{ unsigned char v=0;while(n--)v|=*p++;return v!=0; }
static int p2pa_bundle(const unsigned char *p,unsigned n)
{
    unsigned count;
    if(!p||n<112||n>656||memcmp(p,"CPRBv1\0\0",8)||!p2pa_nonzero(p+8,16))return 0;
    count=p2pa_u32(p+72);
    return count>=1&&count<=18&&n==80+count*32&&!p2pa_u32(p+76)&&
        p2pa_u64(p+24)&&p2pa_u64(p+32)&&p2pa_u64(p+40)&&p2pa_u64(p+48)&&
        p2pa_u64(p+56)>p2pa_u64(p+48)&&p2pa_u64(p+56)-p2pa_u64(p+48)<=30000&&p2pa_u64(p+64);
}
int P2PACaptureBootstrap(const p2p_startup_view_t *view,p2pa_bootstrap_t *out)
{
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!view||view->valid!=1||view->scripts_length<48||!view->scripts||
       !p2pa_bundle(view->principals,view->principals_length)||
       !p2pa_nonzero(view->nonce,32)||!p2pa_nonzero(view->binding,32)||
       !p2pa_nonzero(view->scripts+16,32))return 0;
    memcpy(out->nonce,view->nonce,32);memcpy(out->binding,view->binding,32);
    memcpy(out->manifest,view->scripts+16,32);memcpy(out->principals,view->principals,view->principals_length);
    out->length=view->principals_length;out->valid=1;return 1;
}
int P2PAMakeHello(const p2pa_bootstrap_t *boot,const unsigned char nonce[32],unsigned char out[P2PA_HELLO_SIZE])
{
    if(!boot||boot->valid!=1||!nonce||!out||!p2pa_nonzero(nonce,32)||!p2pa_bundle(boot->principals,boot->length))return 0;
    memset(out,0,P2PA_HELLO_SIZE);memcpy(out,"CPAHv1\0\0",8);p2pa_put32(out+12,P2PA_HELLO_SIZE);
    memcpy(out+16,nonce,32);memcpy(out+48,boot->nonce,32);memcpy(out+80,boot->binding,32);
    memcpy(out+112,boot->manifest,32);memcpy(out+144,boot->principals+8,16);
    memcpy(out+160,boot->principals+24,16);memcpy(out+176,boot->principals+64,8);
    memcpy(out+184,boot->principals+40,8);return 1;
}
int P2PAParseControl(const p2pa_bootstrap_t *boot,const unsigned char hello[P2PA_HELLO_SIZE],
    const void *raw,size_t n,p2p_startup_sha256_fn sha,p2pa_control_t *out)
{
    static const unsigned char domain[]="CODOL-P2P-ACTIVE-CONTROL-V2";
    const unsigned char *p=(const unsigned char *)raw,*bundle;
    unsigned op,len;unsigned char hash[32],input[sizeof(domain)+P2PA_HELLO_SIZE+240+656];size_t at;
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!boot||boot->valid!=1||!hello||!p||!sha||n<P2PA_CONTROL_HEADER||n>P2PA_MAX_CONTROL||
       memcmp(p,"CPACv2\0\0",8)||p2pa_u32(p+12)!=n||
       memcmp(p+24,hello+16,32)||memcmp(p+56,boot->binding,32)||memcmp(p+88,boot->manifest,32))return 0;
    op=p2pa_u32(p+8);len=p2pa_u32(p+200);bundle=p+P2PA_CONTROL_HEADER;
    if(!p2pa_u64(p+16)||!p2pa_nonzero(p+120,16)||!p2pa_nonzero(p+136,32)||
       !p2pa_u64(p+168)||!p2pa_u64(p+176)||!p2pa_nonzero(p+184,16)||n!=P2PA_CONTROL_HEADER+len||
       (op!=P2PA_UPDATE&&op!=P2PA_REVOKE)||(op==P2PA_REVOKE&&(len||p2pa_u32(p+204)))||
       (op==P2PA_UPDATE&&(!p2pa_bundle(bundle,len)||len!=boot->length||
        memcmp(bundle,boot->principals,40)||memcmp(bundle+64,boot->principals+64,len-64)||
        p2pa_u64(bundle+56)>p2pa_u64(p+168))))return 0;
    if(!sha(bundle,len,hash)||memcmp(hash,p+208,32))return 0;
    memcpy(input,domain,sizeof(domain));at=sizeof(domain);memcpy(input+at,hello,P2PA_HELLO_SIZE);at+=P2PA_HELLO_SIZE;
    memcpy(input+at,p,240);at+=240;if(len)memcpy(input+at,bundle,len);at+=len;
    if(!sha(input,at,hash)||memcmp(hash,p+240,32))return 0;
    out->operation=op;out->length=len;out->sequence=p2pa_u64(p+16);
    out->owner_deadline_low=p2pa_u32(p+204);
    out->hard_expires=p2pa_u64(p+168);out->path_epoch=p2pa_u64(p+176);
    memcpy(out->binding,p+240,32);memcpy(out->active_id,p+120,16);memcpy(out->handoff_id,p+136,32);memcpy(out->path_id,p+184,16);
    if(op==P2PA_UPDATE){out->principals=bundle;out->native_sequence=p2pa_u64(bundle+40);out->issued=p2pa_u64(bundle+48);out->expires=p2pa_u64(bundle+56);}
    return 1;
}
void P2PAInitPolicy(p2pa_policy_t *policy,const p2pa_bootstrap_t *boot)
{
    if(!policy)return;
    memset(policy,0,sizeof(*policy));
    if(!boot||boot->valid!=1||!p2pa_bundle(boot->principals,boot->length)){policy->dead=1;return;}
    policy->native_sequence=p2pa_u64(boot->principals+40);
}
int P2PAObserve(p2pa_policy_t *policy,uint64_t wall,uint64_t mono)
{
    if(!policy)return 0;
    if(policy->dead||!wall||(policy->clock_seen&&(wall<policy->last_wall||mono<policy->last_mono))||
       (policy->active&&(wall>=policy->expires||wall>=policy->hard_expires||
          mono>=policy->deadline||mono>=policy->hard_deadline))) {policy->dead=1;return 0;}
    policy->clock_seen=1;policy->last_wall=wall;policy->last_mono=mono;return 1;
}
int P2PAPrepare(p2pa_policy_t *policy,const p2pa_control_t *control,uint64_t wall,uint64_t mono)
{
    uint32_t delta;uint64_t owner_deadline;
    if(!policy||!control||!P2PAObserve(policy,wall,mono)||policy->sequence==UINT64_MAX||
       control->sequence!=policy->sequence+1)return 0;
    if(policy->active&&(control->hard_expires!=policy->hard_expires||memcmp(control->active_id,policy->active_id,16)||
       memcmp(control->handoff_id,policy->handoff_id,32)))return 0;
    if(control->operation==P2PA_REVOKE)return control->owner_deadline_low==0;
    if(!(control->operation==P2PA_UPDATE&&policy->native_sequence!=UINT64_MAX&&
        control->native_sequence==policy->native_sequence+1&&control->expires>wall&&
        control->hard_expires>=control->expires&&control->issued<=wall+((wall<=UINT64_MAX-2000)?2000:0)&&
        (wall<control->issued||wall-control->issued<=5000)&&control->hard_expires-wall<=UINT64_MAX-mono))return 0;
    /* The retained SDK's global QPC cap survives IPC latency even when UTC is
     * flat. Low32 is a bounded future delta, including a valid zero at wrap. */
    delta=(uint32_t)(control->owner_deadline_low-(uint32_t)mono);
    if(!delta||delta>30000u||mono>UINT64_MAX-delta)return 0;
    owner_deadline=mono+delta;
    /* Capture the earliest offered budgets before native installation. Rechecks
     * may observe a flat wall clock while monotonic time advances; they must
     * not turn elapsed install/ACK time into additional authorization. */
    if(!policy->offered_sequence) {
        policy->offered_sequence=control->sequence;policy->offered_native=control->native_sequence;
        policy->offered_expires=control->expires;policy->offered_hard=control->hard_expires;
        policy->offered_owner_deadline=owner_deadline;
        policy->offered_deadline=mono+control->expires-wall;
        policy->offered_hard_deadline=policy->active?policy->hard_deadline:mono+control->hard_expires-wall;
        if(policy->offered_deadline>policy->offered_hard_deadline)policy->offered_deadline=policy->offered_hard_deadline;
        if(policy->offered_deadline>owner_deadline)policy->offered_deadline=owner_deadline;
        memcpy(policy->offered_binding,control->binding,32);
    }
    return policy->offered_sequence==control->sequence&&policy->offered_native==control->native_sequence&&
        policy->offered_expires==control->expires&&policy->offered_hard==control->hard_expires&&
        policy->offered_owner_deadline==owner_deadline&&
        !memcmp(policy->offered_binding,control->binding,32)&&mono<policy->offered_deadline&&mono<policy->offered_hard_deadline;
}
int P2PACommit(p2pa_policy_t *policy,const p2pa_control_t *control,int result,uint64_t wall,uint64_t mono)
{
    if(result!=1||!P2PAPrepare(policy,control,wall,mono)){if(policy)policy->dead=1;return 0;}
    policy->sequence=control->sequence;
    if(control->operation==P2PA_REVOKE){policy->dead=1;return 1;}
    policy->active=1;policy->native_sequence=control->native_sequence;policy->expires=control->expires;
    policy->deadline=policy->offered_deadline;policy->hard_deadline=policy->offered_hard_deadline;
    policy->hard_expires=control->hard_expires;policy->offered_sequence=0;
    memcpy(policy->active_id,control->active_id,16);memcpy(policy->handoff_id,control->handoff_id,32);return 1;
}
int P2PAMakeAck(const p2pa_control_t *control,const unsigned char nonce[32],int result,uint64_t native_seq,unsigned char out[P2PA_ACK_SIZE])
{
    if(!control||!nonce||!out||!p2pa_nonzero(nonce,32)||!control->sequence||!native_seq)return 0;
    memset(out,0,P2PA_ACK_SIZE);memcpy(out,"CPAKv2\0\0",8);p2pa_put64(out+8,control->sequence);
    memcpy(out+16,nonce,32);memcpy(out+48,control->binding,32);p2pa_put32(out+80,result==1?1:0);
    p2pa_put32(out+84,control->operation);p2pa_put64(out+88,native_seq);return 1;
}
int P2PAParseQuery(const p2pa_bootstrap_t *boot,const unsigned char hello[P2PA_HELLO_SIZE],
    const void *raw,size_t n,p2p_startup_sha256_fn sha,p2pa_query_t *out)
{
    static const unsigned char domain[]="CODOL-P2P-NATIVE-OBSERVE-V1";
    const unsigned char *p=raw,*bundle;unsigned length;size_t at;
    unsigned char hash[32],input[sizeof(domain)+P2PA_HELLO_SIZE+112+656];
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!boot||boot->valid!=1||!hello||!p||!sha||n<P2PA_QUERY_HEADER+112||n>P2PA_MAX_QUERY||
       memcmp(p,"CPAQv1\0\0",8)||p2pa_u32(p+8)!=n||!p2pa_nonzero(p+16,32)||
       memcmp(p+48,hello+16,32))return 0;
    length=p2pa_u32(p+12);bundle=p+P2PA_QUERY_HEADER;
    if(n!=P2PA_QUERY_HEADER+length||!p2pa_bundle(bundle,length)||length!=boot->length||
       memcmp(bundle,boot->principals,40)||memcmp(bundle+64,boot->principals+64,length-64))return 0;
    if(!sha(bundle,length,hash)||memcmp(hash,p+80,32))return 0;
    memcpy(input,domain,sizeof(domain));at=sizeof(domain);
    memcpy(input+at,hello,P2PA_HELLO_SIZE);at+=P2PA_HELLO_SIZE;
    memcpy(input+at,p,112);at+=112;memcpy(input+at,bundle,length);at+=length;
    if(!sha(input,at,hash)||memcmp(hash,p+112,32))return 0;
    out->principals=bundle;out->length=length;
    memcpy(out->nonce,p+16,32);memcpy(out->binding,p+112,32);return 1;
}
int P2PAMakeQueryReply(const p2pa_query_t *query,int result,uint64_t deadline,uint64_t observed,
    unsigned char out[P2PA_QUERY_REPLY])
{
    if(!out)return 0;
    memset(out,0,P2PA_QUERY_REPLY);
    if(!query||!p2pa_nonzero(query->nonce,32)||!p2pa_nonzero(query->binding,32)||
       (result==1&&deadline<=observed))return 0;
    memcpy(out,"CPARv1\0\0",8);memcpy(out+8,query->nonce,32);memcpy(out+40,query->binding,32);
    p2pa_put32(out+72,result==1?1:0);
    if(result==1){p2pa_put64(out+80,deadline);p2pa_put64(out+88,observed);}
    return 1;
}

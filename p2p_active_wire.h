/* Private retained-SDK active-control v2. Parsing is not authorization. */
#ifndef CODOL_P2P_ACTIVE_WIRE_H
#define CODOL_P2P_ACTIVE_WIRE_H
#include "p2p_startup_wire.h"
#define P2PA_HELLO_SIZE 192u
#define P2PA_CONTROL_HEADER 272u
#define P2PA_MAX_CONTROL 928u
#define P2PA_ACK_SIZE 96u
#define P2PA_QUERY_HEADER 144u
#define P2PA_MAX_QUERY 800u
#define P2PA_QUERY_REPLY 96u
#define P2PA_UPDATE 1u
#define P2PA_REVOKE 2u
typedef struct {
    unsigned char nonce[32],binding[32],manifest[32],principals[656];
    unsigned length,valid;
} p2pa_bootstrap_t;
typedef struct {
    unsigned operation,length;
    uint32_t owner_deadline_low;
    uint64_t sequence,native_sequence,issued,expires,hard_expires,path_epoch;
    unsigned char binding[32],active_id[16],handoff_id[32],path_id[16];
    const unsigned char *principals;
} p2pa_control_t;
typedef struct {
    const unsigned char *principals;
    unsigned length;
    unsigned char nonce[32],binding[32];
} p2pa_query_t;
typedef struct {
    unsigned active,dead,clock_seen;
    uint64_t sequence,native_sequence,last_wall,last_mono,expires,deadline,hard_expires,hard_deadline;
    uint64_t offered_sequence,offered_native,offered_expires,offered_hard,offered_deadline,offered_hard_deadline,offered_owner_deadline;
    unsigned char offered_binding[32];
    unsigned char active_id[16],handoff_id[32];
} p2pa_policy_t;
int P2PACaptureBootstrap(const p2p_startup_view_t *,p2pa_bootstrap_t *);
int P2PAMakeHello(const p2pa_bootstrap_t *,const unsigned char[32],unsigned char[P2PA_HELLO_SIZE]);
int P2PAParseControl(const p2pa_bootstrap_t *,const unsigned char[P2PA_HELLO_SIZE],
    const void *,size_t,p2p_startup_sha256_fn,p2pa_control_t *);
void P2PAInitPolicy(p2pa_policy_t *,const p2pa_bootstrap_t *);
/* Caller serializes policy access. Native calls never run under that lock. */
int P2PAObserve(p2pa_policy_t *,uint64_t wall,uint64_t mono);
int P2PAPrepare(p2pa_policy_t *,const p2pa_control_t *,uint64_t wall,uint64_t mono);
int P2PACommit(p2pa_policy_t *,const p2pa_control_t *,int native_result,uint64_t wall,uint64_t mono);
int P2PAMakeAck(const p2pa_control_t *,const unsigned char[32],int,uint64_t,unsigned char[P2PA_ACK_SIZE]);
int P2PAParseQuery(const p2pa_bootstrap_t *,const unsigned char[P2PA_HELLO_SIZE],
    const void *,size_t,p2p_startup_sha256_fn,p2pa_query_t *);
int P2PAMakeQueryReply(const p2pa_query_t *,int,uint64_t,uint64_t,unsigned char[P2PA_QUERY_REPLY]);
#endif

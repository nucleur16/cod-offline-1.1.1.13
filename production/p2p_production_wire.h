/* Transport validation only. This does not verify an issuer or grant authority. */
#ifndef CODOL_P2P_PRODUCTION_WIRE_H
#define CODOL_P2P_PRODUCTION_WIRE_H
#include <stdint.h>
#include <stddef.h>
#define P2PP_HEADER 120u
#define P2PP_MAX_BODY 65536u
enum { P2PP_CHALLENGE=1, P2PP_INSTALL=2, P2PP_REVOKE=3, P2PP_CONTEXT=4, P2PP_HANDOFF=5, P2PP_RELEASE=6 };
typedef struct {
    uint32_t parent_pid,child_pid;
    uint64_t parent_birth,child_birth,sequence,original_deadline;
    unsigned char nonce[32],request_sha256[32];
} p2pp_scope;
typedef struct {
    uint32_t kind,length;
    uint64_t sequence,deadline;
    const unsigned char *body;
} p2pp_frame;
/* All identity fields are captured from original retained HANDLEs by the
 * Windows owner, never from a PID reopen or this untrusted message. No parser
 * state is advanced: caller commits sequence only after cryptographic/schema
 * and live original-parent checks, with a final original-clock check. */
int P2PP_Parse(const void *packet,size_t length,const p2pp_scope *scope,
               uint64_t now,p2pp_frame *out);
#endif

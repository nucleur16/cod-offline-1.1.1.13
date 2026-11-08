/* Guest binding-only bytes. Parsing is NOT service/OS authentication.
 * The retained SDK validates the signed own Guest proof; the game thread must
 * additionally match its current local UID, selected attempt and live parent. */
#ifndef CODOL_P2P_GUEST_BINDING_WIRE_H
#define CODOL_P2P_GUEST_BINDING_WIRE_H
#include <stddef.h>
#include <stdint.h>
#include "p2p_startup_wire.h"
#define P2PG_PACKET_SIZE 608u
#define P2PG_ENVELOPE_HEADER 80u
#define P2PG_ENVELOPE_SIZE 688u
#define P2PG_ACK_SIZE 76u
#define P2PG_DISPATCH_SIZE 88u
#define P2PG_QUEUED_SIZE 80u
typedef struct {
    unsigned char packet[P2PG_PACKET_SIZE],nonce[32],binding[32];
    unsigned protocol,valid,connect_dispatch;
} p2pg_view_t;
int P2PGuestBindingParse(const void *,size_t,const void *,size_t,p2p_startup_sha256_fn,p2pg_view_t *);
int P2PGuestBindingAck(const p2pg_view_t *,int,unsigned char[P2PG_ACK_SIZE]);
int P2PGuestConnectParse(const p2pg_view_t *,const unsigned char *,size_t,unsigned *);
int P2PGuestConnectQueued(const p2pg_view_t *,unsigned,int,unsigned char[P2PG_QUEUED_SIZE]);
#endif

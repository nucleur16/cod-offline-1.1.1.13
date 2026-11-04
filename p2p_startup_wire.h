/* Private, versioned SDK startup framing. Parsing is not authentication.
 * The caller must retain the owned SDK/process/channel and install on the
 * native game-thread fiber; no packet or PID alone authorizes a principal. */
#ifndef CODOL_P2P_STARTUP_WIRE_H
#define CODOL_P2P_STARTUP_WIRE_H
#include <stddef.h>
#include <stdint.h>

#define P2P_STARTUP_REQUEST_HEADER 56u
#define P2P_STARTUP_MAX_REQUEST 248u
#define P2P_STARTUP_ENVELOPE_HEADER 152u
#define P2P_STARTUP_MAX_CVSP 69140528u
#define P2P_STARTUP_MAX_PRINCIPALS 656u
#define P2P_STARTUP_MAX_ENVELOPE 69141336u
#define P2P_STARTUP_ACK_SIZE 76u

typedef int (*p2p_startup_sha256_fn)(const void *,size_t,unsigned char[32]);
typedef struct {
    const unsigned char *scripts,*principals;
    uint32_t scripts_length,principals_length;
    unsigned char nonce[32],binding[32];
    unsigned valid;
} p2p_startup_view_t;

int P2PStartupMakeRequest(const char *map,const char *gametype,const char *category,
    const unsigned char nonce[32],void *output,size_t capacity,size_t *length);
int P2PStartupParseEnvelope(const void *request,size_t request_length,
    const void *envelope,size_t envelope_length,p2p_startup_sha256_fn sha256,
    p2p_startup_view_t *output);
/* Only two literal native installer results of 1 encode success. This helper
 * does not call either installer and never represents native/DW Ready. */
int P2PStartupMakeAck(const p2p_startup_view_t *,int scripts_result,int principals_result,
    unsigned char output[P2P_STARTUP_ACK_SIZE]);

#endif

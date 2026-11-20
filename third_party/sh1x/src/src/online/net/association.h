/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ASSOCIATION_H
#define ASSOCIATION_H
#include <stddef.h>
#include <stdint.h>
#pragma pack(push, 8)
typedef struct {
    uint8_t shared_x[28], key[24];
    uint32_t receive_high, receive_window, send_next;
    uint16_t local_tag, peer_tag;
    unsigned active, received, send_exhausted;
} association_t;
#pragma pack(pop)

/* Called only after the owner validates the handshake/security ID and computes
 * ECDH. A fresh association clears all record state; duplicate COOKIE_ECHO must
 * be answered from the owner's cached handshake, without resetting this state. */
int AssociationStart(association_t *, const uint8_t shared_x[28],
                           const uint8_t security_key[16], uint16_t local_tag,
                           uint16_t peer_tag);
void AssociationClear(association_t *);
/* Consumes a counter upon successful encryption, before socket submission.
 * Retrying a failed socket send must encrypt again with a fresh counter. */
size_t AssociationSeal(association_t *, uint8_t *, size_t,
                             const uint8_t *, size_t);
/* Optional candidate security key comes from the trusted lobby session owner.
 * Adopt it only if the record authenticates with the derived candidate key.
 * Replay, tag and MAC failures leave output and association unchanged. */
int AssociationOpen(association_t *, uint8_t *, size_t, size_t *,
                          const uint8_t *, size_t, const uint8_t candidate[16]);
/* Diagnostic result contains no key material or plaintext. */
int AssociationOpenDetailed(association_t *, uint8_t *, size_t, size_t *,
                          const uint8_t *, size_t, const uint8_t candidate[16],
                          const char **reason);
#endif

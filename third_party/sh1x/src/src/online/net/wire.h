/* CoDOL 1.1.1.13 wire primitives. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef WIRE_H
#define WIRE_H

#include <stddef.h>
#include <stdint.h>

#define OL_PROTOCOL 187
#define PORT 17733
#define OL_FRAGMENT_SIZE 1162u
#define MAX_FRAGMENTS 128u
#define MAX_MESSAGE (OL_FRAGMENT_SIZE * MAX_FRAGMENTS)
#define SEQUENCE_MASK UINT32_C(0x1fffffff)
#define OL_FRAGMENT_BIT UINT32_C(0x80000000)
#define FORCE_ACK_BIT UINT32_C(0x40000000)
#define ACK_BIT UINT32_C(0x20000000)

/* Separate wire state: do not change IW3 player/entity layouts used by ASM. */
typedef struct {
    uint32_t sequence;
    uint32_t received[4];
    uint32_t length;
    uint16_t stride;
    uint8_t last;
    uint8_t active;
} fragments_t;

enum { WIRE_INVALID = -1, WIRE_WAIT = 0,
       WIRE_COMPLETE = 1, WIRE_ACK = 2 };

/* Returns packet length, or zero on invalid input/insufficient capacity.
 * Input is the complete already-XORed message, without its sequence prefix.
 * qport < 0 selects server->client; otherwise a client qport is emitted. */
size_t WritePacket(uint8_t *out, size_t capacity, uint32_t sequence,
                        int qport, const uint8_t *message, size_t length,
                        unsigned index);

/* Receive one fragment header + payload (sequence and qport already removed).
 * State/buffer are caller-owned and reset to zero when a channel is opened.
 * Rejects malformed/conflicting fragments before modifying state or buffer. */
int ReadFragment(fragments_t *state, uint8_t *buffer,
                       size_t capacity, uint32_t sequence, const uint8_t *data,
                       size_t length);

/* Symmetric CoD netchan XOR, starting at the first encoded body byte. */
void OL_Xor(uint8_t *data, size_t length, uint8_t key,
               const uint8_t *command, size_t command_length);

/* bdSequenceNumber::set; IV counter is 32 bits, wire counter only 16. */
uint32_t ExtendCounter(uint16_t wire, uint32_t last);

#endif

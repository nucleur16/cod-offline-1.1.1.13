/* C1 glass snapshot section. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef GLASS_SNAPSHOT_H
#define GLASS_SNAPSHOT_H
#include "ol_msg.h"
#include <stddef.h>
#include <stdint.h>
#pragma pack(push, 8)
#define GLASS_MAX_PIECES 1024u
typedef struct {
    uint8_t state; /* 0 intact, 1 weakened, 2 shattered, 3 removed */
    uint8_t direction; /* 255: no localized impact; otherwise DirToByte */
    uint8_t impact_x, impact_y; /* local plane coordinates, six bits each */
} glass_wire_piece_t;
/* Immutable captured arrays. Missing base sends full state; equal arrays send
 * the zero gate. Counts cannot exceed C1's fixed 1024-piece client store. */
int WriteGlass(ol_msg_t *, const glass_wire_piece_t *to,
    size_t count, const glass_wire_piece_t *base, size_t base_count);
#pragma pack(pop)
#endif

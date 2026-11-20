/* XAnimParts from a C1 record. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef C1_XANIM_H
#define C1_XANIM_H
#include "assets.h"
#pragma pack(push, 8) /* server.h leaves pack(1) behind (codol-header-packing-hazard) */
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint8_t *data;                     /* the inline stream C1's loader would read */
    size_t size;
    script_string_t *strings;    /* its local script-string table, by text */
    unsigned string_count;
} c1_xanim_stream_t;
int SerializeC1XAnim(const uint8_t *record,const char *(*text)(void *,unsigned),void *context,
    c1_xanim_stream_t *out);
void FreeC1XAnimStream(c1_xanim_stream_t *);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

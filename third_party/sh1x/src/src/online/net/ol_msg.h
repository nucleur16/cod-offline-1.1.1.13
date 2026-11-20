/* CoDOL's interleaved byte/bit message stream. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef MSG_H
#define MSG_H
#include <stddef.h>
#include <stdint.h>

/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    uint8_t *data;
    size_t size, capacity, cursor, bit;
    int failed;
} ol_msg_t;

void MsgReadInit(ol_msg_t *m, const void *data, size_t size);
void MsgWriteInit(ol_msg_t *m, void *data, size_t capacity);
uint32_t ReadBits(ol_msg_t *m, unsigned bits);
uint32_t ReadByte(ol_msg_t *m);
uint32_t ReadShort(ol_msg_t *m);
uint32_t ReadLong(ol_msg_t *m);
int OL_ReadData(ol_msg_t *m, void *out, size_t size);
void WriteBits(ol_msg_t *m, uint32_t value, unsigned bits);
void WriteByte(ol_msg_t *m, uint32_t value);
void WriteShort(ol_msg_t *m, uint32_t value);
void WriteLong(ol_msg_t *m, uint32_t value);
void OL_WriteData(ol_msg_t *m, const void *data, size_t size);
void OL_WriteString(ol_msg_t *m, const char *text);
#pragma pack(pop)
#endif

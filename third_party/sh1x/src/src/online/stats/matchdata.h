#ifndef MATCHDATA_H
#define MATCHDATA_H
#include "structured_schema.h"
#include "ol_msg.h"
#pragma pack(push, 8)
#define CLIENT_MATCHDATA_CAPACITY 1024
typedef struct {
    char name[64];
    structured_schema_t schema;
    uint8_t record[CLIENT_MATCHDATA_CAPACITY];
} client_matchdata_t;
/* Native per-server record; reset at SV_ClearServer before another map starts.
 * Schema arrays are owned; strings/maps borrow mounted archive lifetimes. */
void ResetClientMatchData(void);
/* Idempotent for the same ASCII-case-insensitive name. Different selections
 * require server reset. Failed loads preserve all state. */
int SelectClientMatchData(const char *name);
client_matchdata_t *ClientMatchData(void);
void RequestClientMatchData(unsigned client);
void AcknowledgeClientMatchData(unsigned client);
int ClientMatchDataDue(unsigned client,uint32_t time);
void ClientMatchDataSent(unsigned client,uint32_t time);
/* Append opcode 4/name/NUL/full record before the enclosing message EOF.
 * Whole-byte fields preserve any pending bit pool. Invalid input/capacity
 * sets message.failed without changing size, bit cursor or payload bytes. */
int WriteClientMatchDataSection(ol_msg_t *,const char *name,
    const uint8_t *record,size_t record_size);
#pragma pack(pop)
#endif

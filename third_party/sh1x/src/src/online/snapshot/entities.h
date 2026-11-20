#ifndef ENTITIES_H
#define ENTITIES_H
#include "delta.h"
#define ENTITY_FIELDS 66
#define OL_ENTITY_SENTINEL 2047
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct { uint16_t offset; int16_t bits; uint8_t hint; } entity_field_t;
typedef struct {
    uint16_t number, type;
    uint32_t field[ENTITY_FIELDS]; /* field[0] comes from type */
} entity_state_t;
const entity_field_t *EntityField(unsigned type, unsigned index);
const char *ReachEntityFailure(void);
int ReachEntity(entity_state_t *held, const entity_state_t *base,
                      const entity_state_t *requested, const delta_context_t *context);
/* Sorted complete states. Missing old entities are removed, unchanged ones
 * inherited, absent fields explicitly zeroed. Baseline: pass no from entities. */
int WriteEntities(ol_msg_t *msg, const entity_state_t *from, size_t from_count,
                        const entity_state_t *to, size_t to_count,
                        delta_context_t *context);
#pragma pack(pop)
#endif

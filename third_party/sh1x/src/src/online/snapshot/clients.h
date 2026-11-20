#ifndef CLIENTS_H
#define CLIENTS_H
#include "entities.h"
#define CLIENT_FIELDS 77
#define CLIENT_ATTACHMENTS 16
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    unsigned number;
    uint32_t field[CLIENT_FIELDS];
} client_state_t;
/* CoDOL composition uses its own model IDs and storage, never the six-entry
 * attachment arrays embedded in the retained IW3 ABI. */
typedef struct {
    unsigned model;
    unsigned attach_model[CLIENT_ATTACHMENTS];
    unsigned attach_tag[CLIENT_ATTACHMENTS];
} client_appearance_t;
enum { ABILITY_MASK_WORDS=4, ABILITY_VALUES=23 };
/* C1 ability-modifier state, distinct from retained IW3 perk bits. Mask IDs
 * use 0x80000000 >> (id & 31); values use the modifier table's slot mapping.
 * Values must already include the table defaults and modifier operations. */
typedef struct {
    uint32_t masks[ABILITY_MASK_WORDS];
    float values[ABILITY_VALUES];
} client_abilities_t;
/* Snapshot-owned copy at clientState +c0..128. No pointers to live state
 * escape, so a captured frame retains the values that existed at capture.
 * Does not translate perk names, infer defaults or populate gameplay state. */
int OverlayClientAbilities(client_state_t *,const client_abilities_t *);
/* Replace the entire appearance, clearing removed parts. Models must be
 * contiguous; an empty base is legal in C1. Other client fields are preserved.
 * Inputs are already resolved/published configstring IDs. Atomic on failure. */
int OverlayClientAppearance(client_state_t *,const client_appearance_t *);
/* clientState +0x12c/+0x130: the right/left world-gun entity numbers (C1
 * gclient+0x3594/+0x3598), 0 for none, each below 2047. Atomic on failure. */
int OverlayClientWorldWeapons(client_state_t *,const unsigned entities[2]);
const entity_field_t *ClientField(unsigned index);
int ReachClient(client_state_t *held, const client_state_t *base,
                      const client_state_t *requested, const delta_context_t *context);
int WriteClients(ol_msg_t *msg, const client_state_t *from, size_t from_count,
                       const client_state_t *to, size_t to_count,
                       delta_context_t *context);
#pragma pack(pop)
#endif

#ifndef ACTORS_H
#define ACTORS_H
#include "entities.h"
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    unsigned number;
    /* Canonical CoDOL bytes, not an IW3 actor. Fields at 0x44/45/46 overlap;
     * one shared byte image ensures their transmitted words agree. */
    uint8_t data[0x5c];
    /* Native archive metadata; never part of the canonical wire byte image.
     * Preserve alternate model/team/owner with the same historic actor, so a
     * per-recipient model choice uses the archived actor and not a live one. */
    unsigned enemyModel, actorTeam;
    int ownerClient;
} actor_state_t;
const entity_field_t *ActorField(unsigned index);
int WriteActors(ol_msg_t *msg, const actor_state_t *from, size_t from_count,
                      const actor_state_t *to, size_t to_count,
                      delta_context_t *context);
#pragma pack(pop)
#endif

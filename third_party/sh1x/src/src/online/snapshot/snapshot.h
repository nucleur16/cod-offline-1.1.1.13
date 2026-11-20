#ifndef SNAPSHOT_H
#define SNAPSHOT_H
#include "playerstate.h"
#include "entities.h"
#include "clients.h"
#include "actors.h"
#include "animcmd.h"
#include "glass_snapshot.h"
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    uint32_t sequence;
    const player_fields_t *player;
    const hud_state_t *hud;
    const entity_state_t *entities;
    size_t entity_count;
    const client_state_t *clients;
    size_t client_count;
    const actor_state_t *actors;
    size_t actor_count;
    const glass_wire_piece_t *glass;
    size_t glass_count;
} snapshot_state_t;
/* Serializes one svc_snapshot + EOF. The transport's reliable ACK and commands
 * precede this body. Caller owns states, selects an acknowledged base, and must
 * record the quantized client-held state only after a successful send. */
const char *WriteSnapshotFailure(void);
int OL_WriteSnapshot(ol_msg_t *msg, const snapshot_state_t *to,
                        const snapshot_state_t *base,
                        const player_array_updates_t *updates,
                        const animcmd_t *commands, size_t command_count,
                        unsigned flags, int read_origin_velocity,
                        delta_context_t *context);
/* As above, with the wire deltaNum selected independently from the effective
 * serialization base. C1 deltaNum 0/flags 0 uses its pinned baseline. */
int WriteSnapshotDeltaNumber(ol_msg_t *msg, const snapshot_state_t *to,
                        const snapshot_state_t *base, uint32_t delta_number,
                        const player_array_updates_t *updates,
                        const animcmd_t *commands, size_t command_count,
                        unsigned flags, int read_origin_velocity,
                        delta_context_t *context);
#pragma pack(pop)
#endif

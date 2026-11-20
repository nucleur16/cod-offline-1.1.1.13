#ifndef FRAME_BUILDER_H
#define FRAME_BUILDER_H
#include "server.h"
#include "sv_frame.h"
#include "sv_entities.h"
#include "snapshot.h"
typedef struct native_frame_s native_frame_t;
/* Diagnostic for the last synchronous frame-build attempt; not wire state. */
extern const char *frame_failure;
/* Step of the player/HUD overlay that rejected the frame, for the trace. */
extern const char *player_failure;
/* Why the last client snapshot write overflowed, for the recovery warning. */
extern const char *snapshot_failure;
#pragma pack(push, 8)
typedef struct {
    const player_fields_t *player_seed;
    const player_array_updates_t *array_seed;
    const engine_player_bindings_t *player;
    snapshot_entity_resolver_t entity_resolve;
    void *entity_context;
    snapshot_client_resolver_t client_resolve;
    void *client_context;
    const actor_state_t *actors;
    size_t actor_count;
    const animcmd_t *commands;
    size_t command_count;
} native_frame_bindings_t;
#pragma pack(pop)
/* Replace NULL/prior owned frame only after every section succeeds. Neither
 * event counter is staged here: both are properties of the states being
 * serialized. Asset publication and read-only resolvers must precede/obey this
 * transaction. Uses the native frame's exact ring spans and sequence-matched
 * HUD. Requires explicit seeds, including unmapped fields. This builds semantic
 * state; wire reachability is checked by serialization. */
int BuildNativeFrame(native_frame_t **,unsigned,uint32_t,
    const clientSnapshot_t *,const native_frame_bindings_t *);
void FreeNativeFrame(native_frame_t *);
/* Own one assembled frame per recipient. A failed rebuild invalidates the
 * previous frame, including retries using the same outgoing sequence. Ammo
 * bindings are resolved from source->ps and the server's native weapon registry;
 * supplied ammo bindings and event parameters are replaced by native semantic
 * resolution, while other bindings are kept. Unsupported events reject. */
int BuildClientNativeFrame(unsigned,uint32_t,const clientSnapshot_t *,
    const native_frame_bindings_t *);
const native_frame_t *ClientNativeFrame(unsigned,uint32_t);
void ClearClientNativeFrame(unsigned);
/* Current gameplay only; archived frames require historical metadata. */
int BuildGameplayFrame(unsigned,uint32_t,const clientSnapshot_t *);
int ProbeEntityWire(const entityState_t *,unsigned,entity_state_t *);
/* The server's event parameter resolution, shared by both event queues: the
 * generic per-event mapping plus the compound slots (EV_PLAY_FX_ON_TAG through
 * ServerTaggedEffectToWire). Fails rather than publishing an unmapped
 * slot; count is at most four. */
int ServerResolveEventParameters(const uint32_t *events,const uint32_t *parameters,
                                       uint32_t *out,unsigned count);
const snapshot_state_t *NativeFrameState(const native_frame_t *);
const player_array_updates_t *NativeFrameUpdates(const native_frame_t *);
const animcmd_t *NativeFrameCommands(const native_frame_t *,size_t *);
#endif

#ifndef SV_FRAME_H
#define SV_FRAME_H
#include "engine_state.h"
/* One current capture per recipient, not ACK history. Capture before losing
 * native HUD identity; the serializer's owned history retains sent frames.
 * Failure invalidates the previous capture so it cannot be reused by retry. */
int CaptureClientSnapshotHud(unsigned,uint32_t,const hudElemState_t *);
int CopyClientSnapshotHud(unsigned,uint32_t,hud_state_t *);
void ClearClientSnapshotHud(unsigned);
void InvalidateClientSnapshotHud(unsigned);
/* Compose the exact native frame's player fields with its sequence-matched HUD.
 * Caller still supplies the complete seed, weapon/ammo/event bindings and clock.
 * Missing capture or any conversion failure preserves all four outputs. */
int OverlayClientSnapshotPlayer(unsigned,uint32_t,player_fields_t *,
    player_array_updates_t *,hud_state_t *,
    const playerState_t *,const engine_player_bindings_t *);
#endif

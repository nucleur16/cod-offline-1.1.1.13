/* Value codecs shared by CoDOL entity/player/client/actor snapshot fields. */
#ifndef DELTA_H
#define DELTA_H
#include "ol_msg.h"
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    uint32_t server_time;
    int32_t position_base[3];
    int clamp_positions;
    unsigned clamped;
    /* Replay-only: C1 ROV=1 accepts authoritative changed -87 angles.
     * Live contexts remain zero and retain their existing send predicate. */
    int replay_view_angles;
} delta_context_t;
/* C1 wide position codec uses FISTP(center + 2^-30), nearest/even.
 * Only position_base changes; nonfinite/out-of-int32 centers reject atomically. */
int SetDeltaPositionBase(delta_context_t *,const float center[3]);
/* Values are raw native 32-bit words: float bits for float fields, integer bits
 * for integers. Special codecs determine their type; default negative widths
 * carry signed integers. No engine structure is reinterpreted as a CoDOL struct. */
int WriteDeltaValue(ol_msg_t *msg, int bits, uint32_t from, uint32_t to,
                          delta_context_t *context);
/* Value the client holds after this encoder's quantization. Output is untouched
 * on failure. Context counters are not changed by this read-only calculation. */
int ReachDeltaValue(int bits, uint32_t from, uint32_t to,
                          const delta_context_t *context, uint32_t *held);
/* A finite position (codecs -83..-81, -92..-90) outside the 16-bit window
 * around context.position_base is reached as the nearest window edge rather
 * than refused. Process-wide count of such clamps and the latest one, for the
 * server's rate-limited report. */
unsigned DeltaReachClamps(void);
const char *DeltaReachClampNote(void);
#pragma pack(pop)
#endif

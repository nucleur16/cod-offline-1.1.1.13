#ifndef HISTORY_H
#define HISTORY_H
#include "snapshot.h"
typedef struct history_s history_t;
history_t *CreateHistory(void);
void OL_ClearHistory(history_t *history);
void DestroyHistory(history_t *history);
/* Commit only after successful transport acceptance. State must describe the
 * quantized client-held values, not unquantized engine input. Makes deep copies;
 * failure leaves history unchanged. Sequences must strictly increase. */
int CommitHistory(history_t *history, const snapshot_state_t *held);
/* Prepare client-decoded values against an exact retained base (zero for a
 * full snapshot), then commit atomically. Call after successful serialization
 * and transport acceptance, using the same context and rov as the writer. */
int CommitReachedHistory(history_t *history, const snapshot_state_t *requested,
                               uint32_t base_sequence, int read_origin_velocity,
                               const delta_context_t *context);
/* Allocate/quantize before sending. One pending snapshot per history; it is
 * invisible to ACK lookup. Finish performs no allocation and requires the exact
 * attempted sequence. accepted=0 discards it. Clear/Destroy also discard it. */
const char *HistoryFailure(void);
/* An entity whose fields C1 cannot be sent is left out of the prepared frame
 * (and so out of what the writer sends) instead of refusing the snapshot.
 * Process-wide count of such omissions and the latest one, for reporting. */
unsigned HistoryOmittedEntities(void);
const char *HistoryOmittedNote(void);
int PrepareHistory(history_t *history, const snapshot_state_t *requested,
                         uint32_t base_sequence, int read_origin_velocity,
                         const delta_context_t *context);
/* Prepare a SNAPFLAG_BASELINE frame. Its reached state is copied into the
 * separately pinned no-delta baseline only if transport accepts the frame. */
int PrepareBaselineHistory(history_t *history, const snapshot_state_t *requested,
                                 int read_origin_velocity,
                                 const delta_context_t *context);
int FinishHistory(history_t *history, uint32_t sequence, int accepted);
/* Exact acknowledged sequence only. No guessing a base for an ACK that names
 * another kind of packet. Returned pointer lasts until overwrite/reset/destroy. */
const snapshot_state_t *FindHistoryBase(const history_t *history,
                                                  uint32_t acknowledged, uint32_t next_sequence);
/* Zero until the first successfully transmitted frame is committed. */
uint32_t HistoryLatestSequence(const history_t *history);
/* The frame CODOL_Prepare*History just staged: the requested state reached as
 * the client will hold it (quantized). NULL when nothing is pending. */
const snapshot_state_t *HistoryPendingState(const history_t *history);
/* C1's persistent noDeltaSnapshot, used when deltaNum is zero without flag 8.
 * This state remains available after its ordinary 32-frame ring slot expires. */
const snapshot_state_t *HistoryBaseline(const history_t *history);
/* The newest committed frame with floor < sequence <= acknowledged: what the
 * client holds as cl.snap after acknowledging that message. NULL when no
 * such frame is still in the 32-frame ring. */
const snapshot_state_t *HistoryReceivedState(const history_t *history,
                                                        uint32_t acknowledged, uint32_t floor);
/* The newest server message the client reports having received
 * (client_t.messageAcknowledge, read from every client packet). Unlike the
 * delta ACK it is reported with a no-delta request too. Zero until set. */
void HistoryNoteReceived(history_t *history, uint32_t received);
uint32_t HistoryReceived(const history_t *history);
/* 1 when the committed frame at this sequence was transmitted as a
 * SNAPFLAG_BASELINE (flag 8) frame. */
int HistoryIsBaselineFrame(const history_t *history, uint32_t sequence);
#endif

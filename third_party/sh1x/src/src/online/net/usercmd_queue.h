/* Native C1 command acceptance and execution cursors.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef USERCMD_QUEUE_H
#define USERCMD_QUEUE_H

#include "usercmd.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    C1_CMD_ACCEPT_INVALID = -3,
    C1_CMD_ACCEPT_FULL = -2,
    C1_CMD_ACCEPT_ORDER = -1,
    C1_CMD_ACCEPT_IGNORED = 0,
    C1_CMD_ACCEPTED = 1
} c1_cmd_accept_t;

typedef enum {
    C1_CMD_EXEC_INVALID = -1,
    C1_CMD_EXEC_EMPTY = 0,
    C1_CMD_EXEC_READY = 1,
    C1_CMD_EXEC_FUTURE = 2
} c1_cmd_exec_t;

typedef struct {
    size_t accepted;
    size_t ignored;
} c1_cmd_batch_result_t;

/* The owner supplies storage so queue depth is an integration decision based
 * on measured C1 traffic rather than a hard-coded timing assumption here. */
typedef struct {
    ol_usercmd_t *storage;
    size_t capacity;
    size_t head;
    size_t count;
    ol_usercmd_t accepted_cmd;
    ol_usercmd_t executed_cmd;
    ol_usercmd_t previous_executed_cmd;
    unsigned has_accepted;
    unsigned has_executed;
    unsigned has_previous_executed;
} c1_cmd_queue_t;

int C1CmdQueueInit(c1_cmd_queue_t *queue,
                         ol_usercmd_t *storage, size_t capacity);
void C1CmdQueueReset(c1_cmd_queue_t *queue);

/* Seed both cursors from authoritative C1 state during enter-world/reset.
 * The baseline is a cursor only and is not scheduled for execution. */
int C1CmdQueueSeed(c1_cmd_queue_t *queue,
                         const ol_usercmd_t *baseline);

/* Commands must advance serverTime in signed modular order. Stale/repeated
 * commands are ignored. A full queue rejects new work without changing the
 * accepted cursor. Batch acceptance is atomic for the new suffix. */
c1_cmd_accept_t C1CmdQueueAccept(
    c1_cmd_queue_t *queue, const ol_usercmd_t *command);
c1_cmd_accept_t C1CmdQueueAcceptBatch(
    c1_cmd_queue_t *queue, const ol_usercmd_t *commands, size_t count,
    c1_cmd_batch_result_t *result);

/* max_lead_ms is supplied by the native C1 scheduler after that policy is
 * verified. Begin never mutates cursors. Commit advances the executed cursor
 * only after native ClientThink/Pmove successfully consumes the exact head. */
c1_cmd_exec_t C1CmdQueueBeginExecute(
    const c1_cmd_queue_t *queue, uint32_t server_time,
    uint32_t max_lead_ms, ol_usercmd_t *command);
int C1CmdQueueCommitExecuted(c1_cmd_queue_t *queue,
                                   uint32_t server_time);

size_t C1CmdQueuePending(const c1_cmd_queue_t *queue);
const ol_usercmd_t *C1CmdQueueAccepted(
    const c1_cmd_queue_t *queue);
const ol_usercmd_t *C1CmdQueueExecuted(
    const c1_cmd_queue_t *queue);
const ol_usercmd_t *C1CmdQueuePreviousExecuted(
    const c1_cmd_queue_t *queue);

#ifdef __cplusplus
}
#endif
#endif

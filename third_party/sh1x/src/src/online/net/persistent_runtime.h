/* Native main-thread adapter, isolated prototype. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_RUNTIME_H
#define PERSISTENT_RUNTIME_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void PersistentRuntimeRegister(void);
void PersistentRuntimeWorldBegin(void);
/* Called inside the ordinary full map change, after its VM shutdown. */
void PersistentRuntimeBeforeMap(void);
void PersistentRuntimeMapReady(void);
void PersistentRuntimeEnded(void);
void PersistentRuntimeUnexpectedShutdown(void);
const char *PersistentRuntimeReportScope(void);
const char *PersistentRuntimeReportEnvelope(void);
void PersistentRuntimeReportPublished(const char *frozen_scope,const char *path,
    const void *payload,size_t bytes);
int PersistentRuntimeEnabled(void);
void PersistentRuntimeQuarantine(void);
const char *PersistentRuntimeRequest(void);
int PersistentRuntimeMapAllowed(const char *map);
int PersistentRuntimeFrame(void);
/* Read-only common-clock gate; unknown occupancy and present humans run. */
int PersistentRuntimeClockHeld(void);
int PersistentRuntimeAdmit(uint64_t uid);
/* Retained GScr_MapRestart carrier and actual SV_MapRestart callbacks.
 * A queued command is intent only; only the completed native callback acks it. */
void PersistentRuntimeRoundQueue(const char *text);
int PersistentRuntimeRoundAllowed(int fast_restart);
void PersistentRuntimeRoundBegin(void);
void PersistentRuntimeRoundComplete(void);
const char *PersistentRuntimeStatus(void);
#ifdef __cplusplus
}
#endif
#endif

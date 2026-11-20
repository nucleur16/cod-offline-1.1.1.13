/* Default-off native lifecycle observer. No assignment/recycling authority.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_EVENTS_H
#define PERSISTENT_EVENTS_H
#include <stdint.h>
#include "persistent_policy.h"
enum persistent_event_kind {
    PE_WORLD_BEGIN=1, PE_CLEAN_STEP, PE_MAP_READY,
    PE_MATCH_ENDED, PE_REPORT_PENDING, PE_RESET_BEGIN
};
#pragma pack(push,8)
typedef struct {
    uint64_t sequence,world;
    unsigned kind,cleanup_mask;
} persistent_event_t;
typedef struct {
    uint64_t world,sequence,last_ended,cleanup_epoch;
    unsigned steps,ready;
} persistent_event_snapshot_t;
#pragma pack(pop)
void PersistentEventsEnable(int enabled);
void PersistentEventWorldBegin(void);
void PersistentEventStep(unsigned mask);
void PersistentEventMapReady(void);
void PersistentEventMatchEnded(void);
void PersistentEventReportBarrier(int durable);
/* Clear earlier map-load observations exactly once for this reset. Retry does
 * not erase new cleanup steps; a different world/epoch cannot borrow proof. */
int PersistentEventsBeginReset(uint64_t expected_world,uint64_t epoch);
int PersistentEventsSnapshot(persistent_event_snapshot_t *);
/* 1=record,0=no newer record,-1=cursor lost; reconcile actual native state. */
int PersistentEventsRead(uint64_t after,persistent_event_t *out);
const char *PersistentEventsStatus(void);
#endif

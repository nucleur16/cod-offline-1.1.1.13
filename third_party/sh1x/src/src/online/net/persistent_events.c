/* Event ownership is the native main thread. Engine callbacks record facts;
 * this observer never makes a slot Idle, opens admission or resets a world.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_events.h"
#include <stdio.h>
#include <string.h>

#define RECORDS 64u
static int enabled,ready,report_pending;
static uint64_t world,sequence,last_ended_world,cleanup_epoch;
static unsigned steps;
static persistent_event_t records[RECORDS];

static void record(unsigned kind)
{
    persistent_event_t *r;
    if (!enabled || !world) return;
    if (sequence==UINT64_MAX) {enabled=0;return;}
    ++sequence;r=&records[(sequence-1)%RECORDS];
    r->sequence=sequence;r->world=world;r->kind=kind;r->cleanup_mask=steps;
}

void PersistentEventsEnable(int value)
{
    /* Startup-only caller. Repeating it must not erase an event cursor. */
    if (enabled || sequence) return;
    enabled=value==1;world=0;ready=0;steps=0;last_ended_world=0;report_pending=0;
    cleanup_epoch=0;memset(records,0,sizeof(records));
}

void PersistentEventWorldBegin(void)
{
    if (!enabled) return;
    if (world==UINT64_MAX) {enabled=0;return;}
    ++world;steps=0;ready=0;report_pending=0;cleanup_epoch=0;record(PE_WORLD_BEGIN);
}
int PersistentEventsBeginReset(uint64_t expected,uint64_t epoch)
{
    if(!enabled||!world||world!=expected||!epoch)return 0;
    if(cleanup_epoch)return cleanup_epoch==epoch?2:0;
    steps=0;ready=0;report_pending=0;cleanup_epoch=epoch;record(PE_RESET_BEGIN);
    return 1;
}
int PersistentEventsSnapshot(persistent_event_snapshot_t *out)
{
    if(!enabled||!out)return 0;
    out->world=world;out->sequence=sequence;out->last_ended=last_ended_world;
    out->cleanup_epoch=cleanup_epoch;out->steps=steps;out->ready=(unsigned)ready;
    return 1;
}

void PersistentEventStep(unsigned mask)
{
    if (!enabled || !world || !mask || (mask & ~PS_ALL_CLEAN)) return;
    if ((steps & mask)==mask) return;
    steps|=mask;record(PE_CLEAN_STEP);
}

void PersistentEventMapReady(void)
{
    if (!enabled || !world || ready) return;
    ready=1;record(PE_MAP_READY);
}

void PersistentEventMatchEnded(void)
{
    if (!enabled || !world || last_ended_world==world) return;
    last_ended_world=world;record(PE_MATCH_ENDED);
}

void PersistentEventReportBarrier(int durable)
{
    if (!enabled || !world) return;
    if (durable==1) {report_pending=0;PersistentEventStep(PS_REPORTS);}
    else {
        /* A new pending report invalidates the earlier observation. */
        steps&=~PS_REPORTS;
        if (!report_pending) {report_pending=1;record(PE_REPORT_PENDING);}
    }
}

int PersistentEventsRead(uint64_t after,persistent_event_t *out)
{
    if (!enabled || !out || after>=sequence) return 0;
    if (sequence-after>RECORDS) return -1;
    *out=records[after%RECORDS];
    return out->sequence==after+1 ? 1 : -1;
}

const char *PersistentEventsStatus(void)
{
    static char text[256];
    snprintf(text,sizeof(text),"persistent_events=%d persistent_world=%llu persistent_seq=%llu "
        "persistent_steps=%u persistent_missing=%u persistent_world_ready=%d persistent_last_ended=%llu persistent_cleanup_epoch=%llu",
        enabled,(unsigned long long)world,(unsigned long long)sequence,steps,
        PS_ALL_CLEAN & ~steps,ready,(unsigned long long)last_ended_world,(unsigned long long)cleanup_epoch);
    return text;
}

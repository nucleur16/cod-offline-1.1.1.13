/* Match-generation report receipts; real spool owns immutable payloads.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_REPORTS_H
#define PERSISTENT_REPORTS_H
#include "persistent_assignment.h"
#define PR_RECEIPTS 1024u
#pragma pack(push,8)
typedef struct {
    char scope[65],chain[65],sealed[65];
    unsigned count;
    struct { char path[65],payload[65]; } receipts[PR_RECEIPTS];
} persistent_reports_t;
#pragma pack(pop)
/* No rebinding until the old sealed ledger has been explicitly retired. */
int PersistentReportsBegin(persistent_reports_t *,const char *boot,
    const char *lease,uint64_t epoch,const char *request,pa_hash_t);
/* ONLY call after the actual writer's file+directory durable acknowledgement.
 * Frozen queue scope must be supplied, not the current room inferred later.
 * 1=new receipt,2=exact retry,0=scope/conflict/capacity/hash failure. */
int PersistentReportsPublished(persistent_reports_t *,const char *scope,
    const char *path,const void *payload,size_t size,pa_hash_t);
/* Barrier must come from actual report PrepareReset after known-zero humans.
 * Successful publication receipts alone don't mean all rows were drained. */
int PersistentReportsSeal(persistent_reports_t *,int barrier_ready,
    pa_hash_t);
int PersistentReportsRetire(persistent_reports_t *,const char *scope,
    const char *sealed_digest);
#endif

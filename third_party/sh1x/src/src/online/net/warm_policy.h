/* Single-use prepared-match ownership. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef WARM_POLICY_H
#define WARM_POLICY_H

enum warm_phase { WARM_OFF, WARM_LOADING,
    WARM_READY, WARM_ACTIVE, WARM_INVALID };

/* server.h leaves packing changed; keep this ABI identical in every caller. */
#pragma pack(push, 8)
typedef struct {
    enum warm_phase phase;
    char preparation[33], lease[33], selection[65];
    unsigned loads;
    int resume_barrier;
} warm_policy_t;
#pragma pack(pop)

int WarmHex(const char *value, unsigned length);
void WarmPolicyInit(warm_policy_t *state, const char *preparation);
void WarmPolicyBeginMap(warm_policy_t *state);
void WarmPolicyMapReady(warm_policy_t *state, const char *selection);
/* Returns1 on first assignment,2 for its exact idempotent retry,0 on refusal. */
int WarmPolicyActivate(warm_policy_t *state, const char *preparation,
    const char *lease, const char *selection, unsigned humans);
/* Changes while unassigned invalidate it. Active games may rotate normally. */
int WarmPolicyFrame(warm_policy_t *state, const char *selection,
    unsigned humans);
int WarmPolicyAdmit(const warm_policy_t *state);
#endif

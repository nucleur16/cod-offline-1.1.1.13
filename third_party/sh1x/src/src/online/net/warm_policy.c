/* Single-use prepared-match ownership. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "warm_policy.h"
#include <string.h>

int WarmHex(const char *value, unsigned length)
{
    unsigned i, nonzero=0;
    if (!value || strlen(value)!=length) return 0;
    for (i=0;i<length;++i) {
        unsigned char c=(unsigned char)value[i];
        if (!((c>='0' && c<='9') || (c>='a' && c<='f'))) return 0;
        nonzero|=c!='0';
    }
    return nonzero!=0;
}

void WarmPolicyInit(warm_policy_t *state, const char *preparation)
{
    memset(state,0,sizeof(*state));
    if (!preparation || !*preparation) return;
    state->phase=WARM_INVALID;
    if (!WarmHex(preparation,32)) return;
    memcpy(state->preparation,preparation,33);
    state->phase=WARM_LOADING;
}

void WarmPolicyBeginMap(warm_policy_t *state)
{
    if (state->phase==WARM_OFF || state->phase==WARM_ACTIVE) return;
    if (state->phase!=WARM_LOADING || state->loads) {
        state->phase=WARM_INVALID;
        return;
    }
    ++state->loads;
}

void WarmPolicyMapReady(warm_policy_t *state, const char *selection)
{
    if (state->phase!=WARM_LOADING) return;
    if (state->loads!=1 || !WarmHex(selection,64)) {
        state->phase=WARM_INVALID;
        return;
    }
    memcpy(state->selection,selection,65);
    state->phase=WARM_READY;
}

int WarmPolicyActivate(warm_policy_t *state, const char *preparation,
    const char *lease, const char *selection, unsigned humans)
{
    if (!WarmHex(preparation,32) || !WarmHex(lease,32) ||
        !WarmHex(selection,64) || strcmp(state->preparation,preparation) ||
        strcmp(state->selection,selection)) return 0;
    if (state->phase==WARM_ACTIVE)
        return !strcmp(state->lease,lease) ? 2 : 0;
    if (state->phase!=WARM_READY) return 0;
    if (humans) { state->phase=WARM_INVALID; return 0; }
    memcpy(state->lease,lease,33);
    state->phase=WARM_ACTIVE;
    state->resume_barrier=1;
    return 1;
}

int WarmPolicyFrame(warm_policy_t *state, const char *selection,
    unsigned humans)
{
    if (state->phase==WARM_OFF) return 0;
    if (state->phase==WARM_ACTIVE) {
        if (!state->resume_barrier) return 0;
        state->resume_barrier=0;
        return 1; /* discard the final warm wall-clock interval, exactly once */
    }
    if (humans) {
        state->phase=WARM_INVALID;
        return 0; /* never freeze an unexpectedly occupied server */
    }
    if (state->phase==WARM_READY &&
        (!selection || strcmp(state->selection,selection)))
        state->phase=WARM_INVALID;
    return 1;
}

int WarmPolicyAdmit(const warm_policy_t *state)
{ return state->phase==WARM_OFF || state->phase==WARM_ACTIVE; }

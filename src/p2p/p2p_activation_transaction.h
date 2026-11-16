/* Private fixed Pi activation transaction. No OS calls, allocation or retries.
 * Slot operations own exactly Sleep, SleepEx and PeekMessageA. Tick operation
 * owns only one LAST callback; it never rewrites the Com_EventLoop call chain. */
#ifndef P2P_ACTIVATION_TRANSACTION_H
#define P2P_ACTIVATION_TRANSACTION_H
enum { PIA_COLD, PIA_INSTALLING, PIA_READY, PIA_FAILED };
typedef struct {
    int state, failure_stage, quarantine;
    unsigned slots_attempted;
    int handler_attempted, tick_attempted;
} PiActivation;
typedef struct {
    void *context;
    int (*prepare)(void *);
    int (*handler)(void *, int);
    int (*slot)(void *, unsigned, int);
    int (*tick)(void *, int);
    int (*current)(void *);
} PiActivationOps;

static int PiActivationCurrent(PiActivation *a, const PiActivationOps *ops)
{
    if (__atomic_load_n(&a->state, __ATOMIC_ACQUIRE) != PIA_READY) return 0;
    if (!ops->current(ops->context)) {
        __atomic_store_n(&a->failure_stage, 7, __ATOMIC_RELAXED);
        __atomic_store_n(&a->quarantine, 1, __ATOMIC_RELAXED);
        __atomic_store_n(&a->state, PIA_FAILED, __ATOMIC_RELEASE); /* effects retained; no retry */
        return 0;
    }
    return 1;
}

static int PiActivationEnsure(PiActivation *a, const PiActivationOps *ops)
{
    int ok = 1;
    if (__atomic_load_n(&a->state, __ATOMIC_ACQUIRE) == PIA_READY) return PiActivationCurrent(a, ops);
    if (__atomic_load_n(&a->state, __ATOMIC_ACQUIRE) != PIA_COLD) return 0;
    __atomic_store_n(&a->state, PIA_INSTALLING, __ATOMIC_RELEASE); /* spent BEFORE acquisition */
    a->failure_stage = 1;
    if (!ops->prepare(ops->context)) goto failed;
    a->failure_stage = 2;
    a->handler_attempted = 1;
    if (!ops->handler(ops->context, 1)) goto failed;
    for (unsigned i = 0; i < 3; ++i) {
        a->failure_stage = 3 + (int)i;
        a->slots_attempted = i + 1; /* operation can fail after effect */
        if (!ops->slot(ops->context, i, 1)) goto failed;
    }
    a->failure_stage = 6;
    a->tick_attempted = 1;
    if (!ops->tick(ops->context, 1)) goto failed;
    a->failure_stage = 7;
    if (!ops->current(ops->context)) goto failed;
    a->failure_stage = 0;
    __atomic_store_n(&a->state, PIA_READY, __ATOMIC_RELEASE);
    return 1;
failed:
    /* Attempt ALL owned rollback branches even if an earlier one refuses.
     * Each operation must CAS/verify its OWN preimage or hook, never foreign. */
    if (a->tick_attempted && !ops->tick(ops->context, 0)) ok = 0;
    for (unsigned i = a->slots_attempted; i > 0; --i)
        if (!ops->slot(ops->context, i - 1, 0)) ok = 0;
    if (a->handler_attempted && !ops->handler(ops->context, 0)) ok = 0;
    a->quarantine = !ok;
    __atomic_store_n(&a->state, PIA_FAILED, __ATOMIC_RELEASE);
    return 0;
}
#endif

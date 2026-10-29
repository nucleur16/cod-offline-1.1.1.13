#ifndef K33_P2P_INTENT_HEARTBEAT_H
#define K33_P2P_INTENT_HEARTBEAT_H
/* UI-thread state only. A transient atomic-replace failure is retried at 2 Hz;
 * it does not become a permanent host-start failure. Unsigned clocks wrap. */
typedef struct { unsigned lastAttempt; int failed; } P2PIntentHeartbeat;
static int P2PIntentHeartbeatDue(const P2PIntentHeartbeat *state, unsigned now,
                                 unsigned lastSuccess, int contextChanged)
{
    return (contextChanged || state->failed || now-lastSuccess>=2000u) &&
           (!state->failed || now-state->lastAttempt>=500u);
}
static void P2PIntentHeartbeatResult(P2PIntentHeartbeat *state, unsigned now, int success)
{
    state->lastAttempt=now;
    state->failed=!success;
}
#endif

#include "../p2p_intent_heartbeat.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
int main(void)
{
    P2PIntentHeartbeat state={0};
    assert(P2PIntentHeartbeatDue(&state,1000,900,1));
    P2PIntentHeartbeatResult(&state,1000,0);
    assert(state.failed);
    assert(!P2PIntentHeartbeatDue(&state,1001,900,0));
    assert(!P2PIntentHeartbeatDue(&state,1499,900,1));
    assert(P2PIntentHeartbeatDue(&state,1500,900,0));
    P2PIntentHeartbeatResult(&state,1500,1);
    assert(!state.failed);
    assert(!P2PIntentHeartbeatDue(&state,3499,1500,0));
    assert(P2PIntentHeartbeatDue(&state,3500,1500,0));
    assert(P2PIntentHeartbeatDue(&state,1501,1500,1));
    P2PIntentHeartbeatResult(&state,UINT_MAX-255u,0);
    assert(!P2PIntentHeartbeatDue(&state,243u,0,1));
    assert(P2PIntentHeartbeatDue(&state,244u,0,0));
    puts("PASS: transient intent error recovery, 2 Hz failure retries and clock wrap");
    return 0;
}

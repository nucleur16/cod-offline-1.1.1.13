#include "../src/online/player/actor_threat_limits.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    /* Actual wave 2: four easy infantry already target the sole player.
     * Both newly spawned dogs must still acquire that player. */
    assert(ActorCandidateWithinAttackerLimit("survival",0,4,2,32));
    assert(ActorCandidateWithinAttackerLimit("survival",0,5,2,32));
    for(int count=0;count<32;++count)
        assert(ActorCandidateWithinAttackerLimit("survival",0,count,2,32));
    assert(!ActorCandidateWithinAttackerLimit("survival",0,32,2,32));
    /* Ordinary call-ins preserve the T5 configured cap, including zero. */
    for(int count=0;count<8;++count) {
        assert(ActorCandidateWithinAttackerLimit("mp",0,count,2,32)==(count<2));
        assert(ActorCandidateWithinAttackerLimit("coop",0,count,2,32)==(count<2));
        assert(ActorCandidateWithinAttackerLimit(NULL,0,count,2,32)==(count<2));
        assert(ActorCandidateWithinAttackerLimit("mp",1,count,0,32));
    }
    assert(!ActorCandidateWithinAttackerLimit("mp",0,0,0,32));
    assert(ActorCandidateWithinAttackerLimit("survival",1,32,2,32));
    puts("PASS: mixed Survival infantry cannot starve native dog target admission; MP cap/current-target behavior preserved");
    return 0;
}

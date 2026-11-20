/* Never discard native ownership to manufacture a cleanup acknowledgement.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_ai.h"
int PersistentAiReleased(void) {
    return PersistentActorsReleased() && PersistentSentientsReleased() &&
        PersistentTurretsReleased();
}
int PersistentAiEmpty(void) {
    return PersistentActorStaticsEmpty() && PersistentSentientStaticsEmpty() &&
        PersistentTurretStaticsEmpty();
}
int PersistentAiReset(void) {
    if(!PersistentAiReleased())return 0;
    /* All release preconditions precede every destructive reset. The sentient
     * owner also resets actor slots through its ordinary level-init helper. */
    return PersistentSentientStaticsReset() && PersistentActorStaticsReset() &&
        PersistentTurretStaticsReset() && PersistentAiEmpty();
}

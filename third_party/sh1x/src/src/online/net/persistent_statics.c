/* Post-full-VM reset supplements. Existing SV_ClearServer owns the ordinary
 * gameplay reset path; these are the remaining room-time/cache records.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_statics.h"
#include "persistent_build.h"
void PersistentInputOwnersReset(void) {
    PersistentPmoveReset();
    PersistentDogClaimsReset();
#if RESIDENT_HAS_COVER
    PersistentCoverClaimsReset();
#endif
}
int PersistentInputOwnersEmpty(void) {
    return PersistentRadarInputEmpty() && PersistentTeamRadarEmpty() &&
        PersistentSprintInputEmpty() && PersistentWeaponInputEmpty() &&
        PersistentScoreInputEmpty() && PersistentSpectateDefaultsEmpty() &&
        PersistentHudIconsEmpty() && PersistentPlayerVisionEmpty() &&
        PersistentPlayerUseEmpty() && PersistentUseHintsEmpty() &&
        PersistentWeaponMapsEmpty() && PersistentGameInputEmpty() &&
        PersistentCharactersEmpty() && PersistentClientAbilitiesEmpty() &&
        PersistentPmoveInputEmpty() && PersistentWeaponStatesEmpty() &&
        PersistentDogClaimsEmpty()
#if RESIDENT_HAS_COVER
        && PersistentCoverClaimsEmpty()
#endif
        ;
}

/* Specific resident native owner checks, not complete MATCH_STATICS proof.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_STATICS_H
#define PERSISTENT_STATICS_H
#ifdef __cplusplus
extern "C" {
#endif
int PersistentRadarInputEmpty(void);
int PersistentTeamRadarEmpty(void);
int PersistentSprintInputEmpty(void);
int PersistentWeaponInputEmpty(void);
int PersistentScoreInputEmpty(void);
int PersistentSpectateDefaultsEmpty(void);
int PersistentHudIconsEmpty(void);
int PersistentPlayerVisionEmpty(void);
int PersistentPlayerUseEmpty(void);
int PersistentUseHintsEmpty(void);
int PersistentWeaponMapsEmpty(void);
int PersistentGameInputEmpty(void);
int PersistentCharactersEmpty(void);
int PersistentClientAbilitiesEmpty(void);
int PersistentPmoveInputEmpty(void);
int PersistentWeaponStatesEmpty(void);
int PersistentDogClaimsEmpty(void);
int PersistentCoverClaimsEmpty(void);
void PersistentPmoveReset(void);
void PersistentDogClaimsReset(void);
void PersistentCoverClaimsReset(void);
void PersistentInputOwnersReset(void);
int PersistentInputOwnersEmpty(void);
#ifdef __cplusplus
}
#endif
#endif

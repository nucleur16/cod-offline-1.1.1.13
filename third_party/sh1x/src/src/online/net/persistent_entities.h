/* Concrete entity/player owners, not blanket MATCH_STATICS authority.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_ENTITIES_H
#define PERSISTENT_ENTITIES_H
#ifdef __cplusplus
extern "C" {
#endif
int PersistentBombsEmpty(void);
int PersistentKnivesEmpty(void);
int PersistentBoltsEmpty(void);
int PersistentWorldWeaponsEmpty(void);
int PersistentSpawnersEmpty(void);
int PersistentSpawnInfluencesEmpty(void);
int PersistentMissilePositionsEmpty(void);
int PersistentPhysicsLaunchesEmpty(void);
int PersistentGlassEmpty(void);
int PersistentModelAnimationsEmpty(void);
int PersistentScriptModelsEmpty(void);
int PersistentScriptModelResourcesReleased(void);
int PersistentRootMotionEmpty(void);
int PersistentMapShadowKnown(void);
int PersistentMapShadowSettled(void);
int PersistentPerkSourcesEmpty(void);
int PersistentZoomEmpty(void);
int PersistentReviveTokensEmpty(void);
int PersistentWaterEmpty(void);
void PersistentWaterReset(void);
int PersistentHudHelpEmpty(void);
void PersistentHudHelpReset(void);
int PersistentEntityOwnersReset(void);
int PersistentEntityOwnersEmpty(void);
#ifdef __cplusplus
}
#endif
#endif

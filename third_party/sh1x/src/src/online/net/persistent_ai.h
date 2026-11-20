/* Resident reset owner checks. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_AI_H
#define PERSISTENT_AI_H
#ifdef __cplusplus
extern "C" {
#endif
int PersistentActorsReleased(void);
int PersistentSentientsReleased(void);
int PersistentTurretsReleased(void);
int PersistentActorStaticsEmpty(void);
int PersistentSentientStaticsEmpty(void);
int PersistentTurretStaticsEmpty(void);
int PersistentActorStaticsReset(void);
int PersistentSentientStaticsReset(void);
int PersistentTurretStaticsReset(void);
int PersistentAiReleased(void);
int PersistentAiEmpty(void);
int PersistentAiReset(void);
#ifdef __cplusplus
}
#endif
#endif

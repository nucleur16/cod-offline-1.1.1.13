/* Read-only private native owner checks. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_CLEAN_STATE_H
#define PERSISTENT_CLEAN_STATE_H
int PersistentSecureProfilesEmpty(void);
int PersistentPublicProfilesEmpty(void);
int PersistentProfileUpdatesEmpty(void);
int PersistentScriptProfilesEmpty(void);
int PersistentClientMatchDataEmpty(void);
int PersistentServerMatchDataEmpty(void);
int PersistentSnapshotHistoriesEmpty(void);
int PersistentHudCapturesEmpty(void);
int PersistentNativeFramesEmpty(void);
int PersistentActorCapturesEmpty(void);
int PersistentActorDeliveryEmpty(void);
int PersistentLateCorpsesEmpty(void);
int PersistentDogPosesEmpty(void);
int PersistentActorJournalEmpty(void);
int PersistentPlayerCapturesEmpty(void);
int PersistentEntityCapturesEmpty(void);
int PersistentSlowMotionEmpty(void);
int PersistentCommandNotificationsEmpty(void);
int PersistentDeferredTransitionsEmpty(void);
#endif

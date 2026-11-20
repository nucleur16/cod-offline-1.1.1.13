/* Specific room-control owners, not complete MATCH_STATICS proof.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_ROOM_H
#define PERSISTENT_ROOM_H
#ifdef __cplusplus
extern "C" {
#endif
int PersistentRadarTypesEmpty(void);
int PersistentPlayerBuiltinsEmpty(void);
int PersistentPlayerLinksEmpty(void);
int PersistentRemoteDriversEmpty(void);
int PersistentVehicleControlsEmpty(void);
int PersistentRemoteMissilesEmpty(void);
int PersistentSoundOwnersEmpty(void);
int PersistentVehicleControlsReset(void);
int PersistentRoomOwnersReset(void);
int PersistentRoomOwnersEmpty(void);
#ifdef __cplusplus
}
#endif
#endif

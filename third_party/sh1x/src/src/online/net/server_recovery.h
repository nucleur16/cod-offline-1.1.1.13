/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SERVER_RECOVERY_H
#define SERVER_RECOVERY_H

/* A dedicated match server that lost its level must load one again by itself.
 * See server_recovery.c. */

/* SV_SpawnServer finished: remember what the lobby/rotation loaded and reset
 * the retry state. */
void ServerRecoveryLevelLoaded(void);
/* `killserver`: an operator stopped the level on purpose; stay down until the
 * next `map`. */
void ServerRecoveryOperatorStop(void);
/* SV_Frame with no level running: sleeps while serving rcon, and reloads the
 * last level once its retry time comes. */
void ServerRecoveryIdleFrame(void);

#endif

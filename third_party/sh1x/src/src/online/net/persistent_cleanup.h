/* Private main-thread empty-world teardown. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_CLEANUP_H
#define PERSISTENT_CLEANUP_H
#include <stdint.h>
/* 1: destructive teardown finished, 0: safely waiting, -1: quarantine.
 * This is NOT all-clean/Idle authority. Individual producers remain required. */
int PersistentCleanupEmpty(uint64_t world, uint64_t epoch);
/* First failed cleanup producer; diagnostic only, never reset authority. */
unsigned PersistentCleanupFailure(void);
/* Implemented beside the actual private gameInitialized owner in sv_game.c. */
int PersistentGameVMStopped(void);
int PersistentScriptQueuesEmpty(void);
#endif

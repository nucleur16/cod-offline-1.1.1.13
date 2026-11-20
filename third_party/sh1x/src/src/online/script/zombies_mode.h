/* Local Endless prototype contracts. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ZOMBIES_MODE_H
#define ZOMBIES_MODE_H
#include <string.h>

/* Endless is a mode, not a Veteran reward tier. Preserve explicit operator
 * league overrides in the caller; this changes only its default. */
static inline int ZombiesDefaultLeague(const char *zone, int difficulty)
{
    return zone && !strcmp(zone, "zm") && difficulty == 3 ? 1 : difficulty;
}
#endif

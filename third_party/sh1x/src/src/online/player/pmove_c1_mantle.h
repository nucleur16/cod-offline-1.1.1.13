/* Native C1 mantle state machine. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PMOVE_C1_MANTLE_H
#define PMOVE_C1_MANTLE_H

#include "pmove_c1.h"

#ifdef __cplusplus
extern "C" {
#endif

int PM_CheckMantleC1(pmove_c1_t *move);
void PM_MantleMoveC1(pmove_c1_t *move);

#ifdef __cplusplus
}
#endif
#endif

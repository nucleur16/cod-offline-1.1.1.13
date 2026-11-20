/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SPECTATE_DEFAULTS_H
#define SPECTATE_DEFAULTS_H
#include "scr_vm.h"
#ifdef __cplusplus
extern "C" {
#endif
void Scr_SetSpectateDefaults(scr_entref_t ref);
void GetSpectateDefaults(unsigned client, float *origin, float *angles);
void ResetSpectateDefaults(unsigned client);
void ResetAllSpectateDefaults(void);
#ifdef __cplusplus
}
#endif
#endif

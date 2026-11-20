/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SCRIPT_LIGHT_H
#define SCRIPT_LIGHT_H
#include "scr_vm.h"
#ifdef __cplusplus
extern "C" {
#endif
void Scr_SetLightIntensity(scr_entref_t ref);
void Scr_GetLightIntensity(scr_entref_t ref);
void Scr_SetLightRadius(scr_entref_t ref);
void Scr_GetLightRadius(scr_entref_t ref);
void Scr_SetLightExponent(scr_entref_t ref);
void Scr_GetLightExponent(scr_entref_t ref);
void Scr_SetLightColor(scr_entref_t ref);
void Scr_GetLightColor(scr_entref_t ref);
void Scr_GetLightFovInner(scr_entref_t ref);
void Scr_GetLightFovOuter(scr_entref_t ref);
void Scr_SetLightFov(scr_entref_t ref);
#ifdef __cplusplus
}
#endif
#endif

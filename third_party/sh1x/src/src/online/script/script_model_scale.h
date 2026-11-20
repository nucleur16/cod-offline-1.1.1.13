/* CoDOL per-entity model scale: <entity> setModelScale(<scale>).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SCRIPT_MODEL_SCALE_H
#define SCRIPT_MODEL_SCALE_H
#ifdef OL_SERVER
#include "scr_vm.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* The scale C1 draws entity <num> at (entityState +0x54), 1.0 by default. */
float EntityModelScale(int num);
/* G_FreeEntity: a recycled entity number starts at normal size. */
void EntityModelScaleFreed(int num);
/* Native counterpart of setModelScale, same range (0.01..100). */
void SetEntityModelScale(int num, float scale);
void Scr_SetModelScale(scr_entref_t ref);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
#endif

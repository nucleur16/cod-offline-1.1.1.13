/* PhysPreset and PhysCollmap from C1 records. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef C1_PHYSICS_H
#define C1_PHYSICS_H
#include "physics.h"
#pragma pack(push, 8) /* server.h leaves pack(1) behind (codol-header-packing-hazard) */
#ifdef __cplusplus
extern "C" {
#endif
/* The inline stream C1's loader would read, owned by out->data (free). 1 ok. */
int SerializeC1PhysicsPreset(const uint8_t *record,asset_t *out);
int SerializeC1PhysicsMap(const uint8_t *record,asset_t *out);
/* 0 when equal, else the first differing part. */
int ComparePhysicsMaps(const physics_map_t *,const physics_map_t *);
int ComparePhysicsPresets(const physics_preset_t *,const physics_preset_t *);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

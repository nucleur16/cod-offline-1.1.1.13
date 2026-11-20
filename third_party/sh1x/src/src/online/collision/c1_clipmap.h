/* clipMap_t from a C1 record. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef C1_CLIPMAP_H
#define C1_CLIPMAP_H
#include "collision.h"
#pragma pack(push, 8) /* server.h leaves pack(1) behind (codol-header-packing-hazard) */
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint8_t *data;              /* the inline stream C1's loader would read (a type-15 asset) */
    size_t size;
    const uint8_t *planes;      /* the record's planes, which the stream reaches as a pooled array */
    uint32_t plane_count;
    const char **model_names;   /* static model i's XModel name (C1ClipMapModelName) */
    uint32_t model_count;
} c1_clipmap_stream_t;
int SerializeC1ClipMap(const uint8_t *record,c1_clipmap_stream_t *out);
void FreeC1ClipMapStream(c1_clipmap_stream_t *);
/* The model name a static model's synthetic reference stands for. */
const char *C1ClipMapModelName(const c1_clipmap_stream_t *,uint32_t reference);
/* ReadCollisionMapResolved with the planes given instead of read from the graphics world. */
int ReadCollisionMapPlanes(const asset_t *collision,const uint8_t *planes,uint32_t plane_count,
    collision_name_resolver_t,collision_model_resolver_t,void *,collision_map_t *);
/* 0 when both maps hold the same collision, else a code naming the first differing part. */
int CompareCollisionMaps(const collision_map_t *,const collision_map_t *);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

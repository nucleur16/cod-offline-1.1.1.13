/* Borrowed C1 map data. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef GLASS_MAP_H
#define GLASS_MAP_H
#include "load_blocks.h"
#include <stdint.h>
#pragma pack(push, 8)
typedef struct {
    unsigned count;
    const uint8_t *server_pieces; /* GameWorldMp: count * 12 */
    const uint8_t *initial_pieces; /* FxWorld: count * 52 */
} glass_map_source_t;
/* 1 valid, 0 absent, -1 invalid. Borrowed spans last until archive unmount. */
int LoadMountedGlassMap(const char *, glass_map_source_t *);
int ReadGlassMap(const load_blocks_t *, const asset_t *,
    const load_blocks_t *, const asset_t *, glass_map_source_t *);
const char *GlassMapFailure(void);
#pragma pack(pop)
#endif

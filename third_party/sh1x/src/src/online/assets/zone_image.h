/* Zone images backed by the replay. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ZONE_IMAGE_H
#define ZONE_IMAGE_H
#include "assets.h"
#include "load_blocks.h"
#include <stddef.h>
#pragma pack(push, 8) /* server.h leaves pack(1) behind (codol-header-packing-hazard) */
#ifdef __cplusplus
extern "C" {
#endif
void *ZoneImageAllocate(size_t size);
void ZoneImageFree(void *data);
/* freeZoneImages: zone images are allocated as views of temporary files (ZoneImageAllocate). */
void SetFileBackedImages(int on);
/* After a zone's replay: make its image view read-only, record a CRC per page and trim it from the working set
 * (its pages are file-backed and read back from the file on touch). 1 released, 0 not a file view. */
int ZoneImageRelease(const zone_t *zone,const load_blocks_t *blocks);
/* Trims again the image pages readers touched since (after a map load), and the file-backed blocks. */
void ZoneImageTrim(void);
/* Diagnostics: module offsets of the callers, " +0x..." each. */
void CallerOffsets(char *out,size_t size,unsigned skip);
typedef struct {
    unsigned regions;
    unsigned long long checked_pages,modified_pages; /* ZoneImageCheck: pages checked, pages that differed */
    size_t released,committed; /* committed: image pages resident now */
} zone_image_stats_t;
void ZoneImageStats(zone_image_stats_t *out);
/* Integrity events: a refill or rebuild that differs from the page as released, or a page written in place. */
enum { ZONE_IMAGE_REFILL_DIFFERS=1, ZONE_IMAGE_WRITTEN=2, ZONE_IMAGE_REBUILD_DIFFERS=3 };
typedef struct { unsigned kind,asset_type,asset_index; size_t zone_size,offset; } zone_image_event_t;
/* Checks every released page against its CRC from the release (written: pages that differ; drifted: always 0). */
void ZoneImageCheck(unsigned *written,unsigned *drifted);
unsigned ZoneImageEvents(const zone_image_event_t **out);
/* Replayed block storage: temporary-file views for large blocks when enabled (fileBackedBlocks). */
void *BlockAllocate(size_t size);
void BlockFree(void *data);
void BlockTrim(void);
void SetFileBackedBlocks(int on);
size_t FileBackedBlockBytes(void);
/* Registry switch (freeZoneImages): record image maps and release images after their replay. */
void SetFreeZoneImages(int on);
/* Process private bytes and working set, and the C runtime heap's bytes in use and free (a heap walk). */
void ProcessMemory(size_t *private_bytes,size_t *working_set,size_t *heap_used,size_t *heap_free);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

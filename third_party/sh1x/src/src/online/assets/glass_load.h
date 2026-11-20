#ifndef GLASS_LOAD_H
#define GLASS_LOAD_H
#include "load_blocks.h"
#include "glass_groups.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Materialize a replayed GameWorldMp's named groups. Destination must be
 * zero-initialized or owned by the glass-group API. Failure preserves it;
 * successful output owns its names and IDs independently of the archive. */
int LoadGlassGroups(const load_blocks_t *,const asset_t *,
                         glass_groups_t *);
/* Named GameWorldMp lookup, with normal mounted-definition precedence.
 * 1=ready, 0=absent, -1=invalid; borrowed output lasts until archive unmount. */
int LoadMountedGlassGroups(const char *,const glass_groups_t **);
#ifdef __cplusplus
}
#endif
#endif

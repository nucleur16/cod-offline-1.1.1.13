/* Category-owned and explicitly shared permanent patch archives. */
#ifndef CATEGORY_PATCH_H
#define CATEGORY_PATCH_H
#include <stddef.h>
#include "q_shared.h"
int CategoryPatchZoneApplies(const char *category,const char *filename);
/* 1 when filename is a zone's old name and files[0..count) holds its new one (that one is mounted instead). */
int CategoryPatchZoneSuperseded(const char *filename,char *const *files,int count);
size_t AppendCategoryPatchZones(const char *zone_type,const char **paths,int *ranks,
    char (*storage)[MAX_OSPATH],size_t count,size_t capacity);
#endif

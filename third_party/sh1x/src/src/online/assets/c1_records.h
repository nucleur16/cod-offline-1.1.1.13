/* C1-layout rawfile and string-table records. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef C1_RECORDS_H
#define C1_RECORDS_H
#include "xassets/c1_assets.h"
#include "assets.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The record of a type 36 definition: payload bytes copied as serialized (compressed or plain plus NUL) into
 * *storage, which the caller owns. 1 built, 0 malformed. */
int BuildC1RawFile(const char *name,const asset_t *asset,C1RawFile *out,uint8_t **storage);
/* Retained views; out->buffer / out->values are malloc'd and owned by the caller. 1 built, 0 malformed. */
int C1RawFileView(const C1RawFile *record,engine_rawfile_t *out);
int C1StringTableView(const C1StringTable *record,engine_table_t *out);
/* The server's records (registry caches; borrowed until unmount). 1 found, 0 absent, -1 invalid. */
int LoadC1RawFile(const char *name,const C1RawFile **out);
int LoadC1StringTable(const char *name,const C1StringTable **out);
int EnumerateMountedNamed(unsigned type,int (*visit)(void *,const char *),void *context);
#ifdef __cplusplus
}
#endif
#endif

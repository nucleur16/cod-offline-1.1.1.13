/* C1's in-memory asset layouts that the server holds (feat/iw4-asset-layout, docs/IW4_ASSET_LAYOUT.md).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Each is the record C1's own loader leaves in memory, so an in-process host can hand the server C1's record
 * unchanged (stage 3). Retained IW3 consumers read views built from these records (c1_records.c). */
#ifndef C1_ASSETS_H
#define C1_ASSETS_H
#include <stdint.h>
#include <stddef.h>

/* RawFile, C1 loader 0x4c58a0 (16 bytes): name, compressed length, length, buffer. The buffer holds
 * compressedLen zlib bytes when compressedLen is nonzero, else len bytes plus a NUL. */
typedef struct C1RawFile
{
  const char *name;
  int compressedLen;
  int len;
  const char *buffer;
} C1RawFile;

/* StringTable, C1 loader 0x4c5b90 (16 bytes), cells of {string, hash} (8 bytes, hash StringTableHash). */
typedef struct C1StringTableCell
{
  const char *string;
  int hash;
} C1StringTableCell;

typedef struct C1StringTable
{
  const char *name;
  int columnCount;
  int rowCount;
  C1StringTableCell *values;
} C1StringTable;

#ifdef __cplusplus
static_assert(sizeof(C1RawFile) == 16 && sizeof(C1StringTable) == 16 && sizeof(C1StringTableCell) == 8, "C1 layouts");
#else
_Static_assert(sizeof(C1RawFile) == 16 && offsetof(C1RawFile, buffer) == 12, "C1 RawFile (0x4c58a0)");
_Static_assert(sizeof(C1StringTable) == 16 && offsetof(C1StringTable, values) == 12, "C1 StringTable (0x4c5b90)");
_Static_assert(sizeof(C1StringTableCell) == 8, "C1 StringTable cell");
#endif
#endif

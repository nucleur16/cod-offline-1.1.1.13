/* C1-layout records with real pointers. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef C1_MATERIALIZE_H
#define C1_MATERIALIZE_H
#include "load_blocks.h"
#include <stddef.h>
#pragma pack(push, 8) /* server.h leaves pack(1) behind (codol-header-packing-hazard) */
#ifdef __cplusplus
extern "C" {
#endif
/* The linked definition of (type, name) as C1 holds it in memory: real pointers, assets linked by name, script
 * strings converted through the hook. NULL when absent or when the zones were replayed without c1Fixups.
 * Valid until the mounted archive set changes. */
void *C1Record(unsigned type,const char *name);
/* Drops every record (the registry calls it when archives are mounted or unmounted). */
void C1MaterializeReset(void);
size_t C1MaterializedBytes(void);
/* The text of a script-string id in a record built without a hook. */
const char *C1RecordString(unsigned id);
/* Converts a zone script string to the holder's handle (the engine: SL_GetString). */
void SetC1ScriptStringHook(unsigned (*hook)(void *,const char *),void *context);
/* c1Readers (registry): 0 off, 1 converters read C1 records, 2 both and compare. */
void SetC1Readers(int mode);
/* Registry: the linked definition's header copy, and a zone script string by index. */
int FindC1Definition(unsigned type,const char *name,const load_blocks_t **blocks,uint32_t *header,
    uint32_t *length);
const char *BlocksScriptString(const load_blocks_t *blocks,unsigned index);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

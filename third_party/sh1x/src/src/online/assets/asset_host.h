/* Asset access for an in-process host. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ASSET_HOST_H
#define ASSET_HOST_H
#ifdef __cplusplus
extern "C" {
#endif
/* The server's record of a C1 asset type, in C1's memory layout, or NULL (absent, or the type is not converted). */
const void *HostFindAsset(unsigned type,const char *name);
/* Every mounted definition name of a converted type; -1 when the type is not converted. */
int HostEnumerateAssets(unsigned type,int (*visit)(void *,const char *),void *context);
/* C1-layout header copy of the linked definition (zones replayed with c1Fixups 1), or NULL. */
const void *FindC1Header(unsigned type,const char *name,unsigned *length);
/* What C1 stores in the pointer word at addr of such a copy (see asset_registry.c), or NULL. */
const void *ResolveC1Pointer(const void *addr,unsigned *kind,unsigned *size);
/* Offsets of the relocated pointer words in [addr, addr+length); -1 when addr is not replay memory. */
int C1PointersIn(const void *addr,unsigned length,unsigned *offsets,unsigned max);
/* Stage 3: the host's lookup into C1's asset database (type, name -> C1's record or NULL); NULL uninstalls. */
void SetC1AssetProvider(const void *(*find)(unsigned,const char *));
/* In-process host: C1's own zone loading (Sh1xHost_SetC1ZoneHost). await_zones makes C1 load the map's zones
 * (preloadmap) unless it holds them, and waits, yielding to C1, until it holds them all or the timeout passes:
 * 1 held. zone_held: whether C1 holds that zone now. */
typedef struct {
    int (*await_zones)(const char *map,const char *zone_type,unsigned timeout_ms);
    int (*zone_held)(const char *zone);
    /* the zone C1's linked (first) definition of (type, name) came from, NULL when C1 has none */
    const char *(*zone_of)(unsigned type,const char *name);
    /* the text of one of C1's script-string handles (the handles in C1's records), NULL for none */
    const char *(*script_string)(unsigned handle);
} c1_zone_host_t;
void SetC1ZoneHost(const c1_zone_host_t *host);
const c1_zone_host_t *C1ZoneHost(void);
/* How many records of a type were taken from C1 since the provider was installed. */
unsigned C1ProvidedCount(unsigned type);
#ifdef __cplusplus
}
#endif
#endif

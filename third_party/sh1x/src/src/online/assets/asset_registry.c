/* Native archive ownership for script source resolution.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "assets.h"
#include "script_source.h"
#include "structured_schema.h"
#include "script_profile_vm.h"
#include "collision.h"
#include "dynents.h"
#include "effects.h"
#include "effects_native.h"
#include "sound_native.h"
#include "physics_native.h"
#include "load_blocks.h"
#include "glass_load.h"
#include "glass_map.h"
#include "glass_groups.h"
#include "xsurface.h"
#include "stream_model.h"
#include "material_native.h"
#include "xmodel_native.h"
#include "xanim_native.h"
#include "weapon_native.h"
#include "weapon_compose.h"
#include "c1_records.h"
#include "c1_weapon.h"
#include "asset_host.h"
#include "c1_materialize.h"
#include "c1_xmodel.h"
#include "c1_xanim.h"
#include "c1_clipmap.h"
#include "c1_physics.h"
#include "zone_image.h"
#include "world.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct table_cache_s {
    stringtable_t source;
    engine_table_t engine;  /* retained view of c1 */
    C1StringTable c1;             /* the record as C1 holds it */
    int c1_borrowed;              /* c1.values are C1's own cells (in-process, a startup zone) */
    struct table_cache_s *next;
} table_cache_t;

typedef struct reference_table_s {
    table_cache_t table;
    char *root,*name;
    struct reference_table_s *next;
} reference_table_t;
static reference_table_t *reference_tables;

int LoadReferenceStringTable(const char *root,const char *name,const engine_table_t **out)
{
    reference_table_t *entry;char *data=NULL;size_t size,n,i;int status;
    if(!root || !*root)return 0;
    if(!name || !out)return -1;
    n=strlen(name);if(n<4 || strcmp(name+n-4,".csv"))return 0;
    for(entry=reference_tables;entry;entry=entry->next)
        if(!strcmp(root,entry->root) && !strcmp(name,entry->name)){*out=&entry->table.engine;return 1;}
    status=ReadScriptSource(root,NULL,name,&data,&size);if(status!=1)return status;
    entry=calloc(1,sizeof(*entry));if(!entry){free(data);return -1;}
    status=ReadCSVStringTable(data,size,&entry->table.source);free(data);
    if(!status)goto fail_reference;
    entry->root=strdup(root);entry->name=strdup(name);
    n=(size_t)entry->table.source.rows*entry->table.source.columns;
    entry->table.engine.values=calloc(n,sizeof(*entry->table.engine.values));
    if(!entry->root || !entry->name || !entry->table.engine.values)goto fail_reference;
    for(i=0;i<n;++i)entry->table.engine.values[i]=entry->table.source.cells[i].text;
    entry->table.engine.name=entry->name;
    entry->table.engine.rowCount=entry->table.source.rows;
    entry->table.engine.columnCount=entry->table.source.columns;
    entry->next=reference_tables;reference_tables=entry;*out=&entry->table.engine;return 1;
fail_reference:
    free(entry->root);free(entry->name);free(entry->table.engine.values);
    FreeStringTable(&entry->table.source);free(entry);return -1;
}

typedef struct rawfile_cache_s {
    engine_rawfile_t engine; /* retained view of c1: the decompressed text */
    C1RawFile c1;                  /* the record as C1 holds it */
    uint8_t *c1_storage;           /* its payload bytes */
    struct rawfile_cache_s *next;
} rawfile_cache_t;

/* A string-table name this archive was searched for and does not carry. An
 * archive's contents never change once mounted, so the answer stays true for
 * its whole life; without it every tableLookup walked all ~30k assets of the
 * archives in front of the one that has the table (~140 us each, dozens per
 * kill, 60-70 on a challenge tier). */
typedef struct table_miss_s {
    struct table_miss_s *next;
    char name[1];
} table_miss_t;

typedef struct dependency_index_entry_s {
    const char *name;
    const asset_t *asset;
} dependency_index_entry_t;

typedef struct archive_s {
    zone_t zone;
    load_blocks_t blocks;
    int prepared;
    dependency_index_entry_t *dependency_index;
    size_t dependency_index_capacity;
    int dependency_index_attempted;
    unsigned group;
    /* Position in C1's load order (CODOL_LinkRank*): the list is kept sorted
     * by it, earliest first, and lookups take the first definition. */
    int rank;
    script_string_t *script_strings;
    struct model_cache_s *models;
    table_cache_t *tables;
    table_miss_t *table_misses;
    rawfile_cache_t *rawfiles;
    struct material_cache_s *materials;
    struct physics_preset_cache_s *physics_presets;
    struct physics_map_cache_s *physics_maps;
    struct effect_cache_s *effects;
    struct sound_cache_s *sounds;
    struct common_world_cache_s *worlds;
    struct glass_world_cache_s *glass_worlds;
    struct xanim_strings_s *xanim_strings;
    struct weapon_tags_s *weapon_tags;
    native_weapon_core_t *null_weapon;
    struct native_weapon_cache_s *native_weapons;
    /* A zone C1 holds (in-process host, stage 3): no image, no replay. Lookups ask C1, restricted to this zone;
     * a lookup no record path serves mounts the server's own copy into this entry (c1_backed_mount). */
    char *c1_zone,*c1_path,*c1_key;
    char *zone_name; /* every map/startup archive: its zone basename (C1's zone names) */
    struct pool_blob_s *pool_blobs; /* buffers written from C1's records for this C1-backed zone */
    uint8_t *touched; /* sh1x_unused: a bit per directory asset some reader resolved or read */
    struct archive_s *next;
} archive_t;
typedef struct sound_cache_s {
    sound_alias_list_t source; /* first member: native lookups cast back */
    const uint8_t *definition;
    archive_t *archive;
    native_sound_t native;
    int native_ready;
    struct sound_cache_s *next;
} sound_cache_t;
typedef struct native_weapon_cache_s {
    const uint8_t *definition;
    native_weapon_core_t core;
    struct native_weapon_cache_s *next;
} native_weapon_cache_t;
typedef struct xanim_strings_s {
    const uint8_t *definition;
    xanim_handles_t handles;
    native_xanim_t native;
    xanim_string_acquire_t acquire;
    xanim_string_release_t release;
    void *context;
    struct xanim_strings_s *next;
} xanim_strings_t;
typedef struct weapon_tags_s {
    const uint8_t *definition;
    weapon_hide_tag_handles_t handles;
    xanim_string_acquire_t acquire;
    xanim_string_release_t release;
    void *context;
    struct weapon_tags_s *next;
} weapon_tags_t;
typedef struct common_world_cache_s {
    const uint8_t *definition;
    struct ComWorld world;
    struct common_world_cache_s *next;
} common_world_cache_t;
typedef struct glass_world_cache_s {
    const uint8_t *definition;
    glass_groups_t groups;
    struct glass_world_cache_s *next;
} glass_world_cache_t;
_Static_assert(sizeof(struct ComWorld)==16,"IW3 common world ABI");
_Static_assert(sizeof(struct ComPrimaryLight)==68,"IW3 primary light ABI");
_Static_assert(offsetof(struct ComPrimaryLight,defName)==64,"IW3 light name");
typedef struct material_cache_s {
    const uint8_t *definition;
    native_material_t native;
    struct material_cache_s *next;
} material_cache_t;
typedef struct physics_preset_cache_s {
    const uint8_t *definition;
    native_physics_preset_t native;
    struct physics_preset_cache_s *next;
} physics_preset_cache_t;
typedef struct physics_map_cache_s {
    physics_map_t source;
    uint8_t *c1_stream; /* the stream written from a C1 record that source views (c1Readers 1) */
    const uint8_t *definition;
    native_physics_map_t native;
    int native_ready;
    struct physics_map_cache_s *next;
} physics_map_cache_t;
typedef struct effect_cache_s {
    effect_source_t source;
    const uint8_t *definition;
    archive_t *archive;
    struct native_effect_cache_s *native;
    struct effect_cache_s *next;
} effect_cache_t;
typedef struct native_effect_cache_s {
    native_effect_t native;
    effect_cache_t *owner;
    unsigned generation;
    effect_model_resolver_t resolver;
    void *context;
    struct native_effect_cache_s *next;
    /* Private transaction state; bindings are patched before cache publication. */
    FxEffectDef placeholder;
    effect_bindings_t *bindings;
    struct native_effect_cache_s **children;
    int prepared;
} native_effect_cache_t;
typedef struct {
    native_effect_cache_t *pending;
    unsigned generation;
    effect_model_resolver_t resolver;
    void *context;
} effect_transaction_t;
typedef struct model_cache_s {
    mounted_model_skeleton_t model;
    asset_t source;
    mounted_model_geometry_t geometry;
    int geometry_ready;
    const uint8_t *c1_record,*c1_header; /* the C1 record the conversion read (c1Readers), else NULL; the
                                          * record itself is fetched again before use (C1MaterializeReset) */
    xmodel_materials_t materials;
    int materials_ready;
    archive_t *archive;
    unsigned generation; /* mount_serial when dependencies were resolved. */
    mounted_model_surfaces_t *surfaces[4];
    const uint8_t *surface_source[4]; /* the xmodelsurfs definition each built lod resolved to */
    stream_model_t streams[4];
    uint16_t *bone_handles;
    bone_string_acquire_t acquire;
    bone_string_release_t release;
    void *string_context;
    struct collision_model_cache_s *collision_model;
    /* built from C1's own XModel record (a zone C1 holds): c1_record is C1's, source views pool_header, a copy of
     * the record's header with its pointer words as serialized presence (-1 set, 0 null); no zone definition */
    int pool;
    uint8_t *pool_header;
    const uint8_t *pool_live;
    struct model_cache_s *next;
} model_cache_t;
typedef struct collision_model_cache_s {
    native_model_core_t core;
    native_model_mesh_t mesh;
    XModel physics_model; /* Borrows the core's owned arrays and archive dependencies. */
    int physics_ready;
} collision_model_cache_t;
static archive_t *archives;
static archive_t *retired_archives;
/* Bumped by every change to the mounted set; caches that resolved names
 * across archives record it (the list head no longer identifies a set: a
 * ranked mount can insert behind it). */
static unsigned mount_serial=1;
static int free_zone_images;
static int c1_backed_need(archive_t *entry,unsigned type,const char *name);
static void touch_asset(archive_t *a,const uint8_t *data);
static int c1_backed_need_map(archive_t *entry,const char *map);
/* freeZoneImages: zone images are read-only temporary-file views, trimmed after their replay (see
 * zone_image.c; no image map: the images no longer rebuild from the replay). */
void SetFreeZoneImages(int on) { free_zone_images=on!=0; SetFileBackedImages(on); SetCollectImageMap(0); }
/* Unranked mounts (tools, loadzones) keep the old meaning, each new
 * archive ahead of everything mounted before it. */
static int unranked_rank=LINK_RANK_UNRANKED;
static stream_model_loader_t stream_loader;
static void *stream_context;
int SetStreamModelLoader(stream_model_loader_t loader,void *context)
{
    if (archives && (loader!=stream_loader || context!=stream_context)) return 0;
    stream_loader=loader; stream_context=context; return 1;
}
typedef struct {
    archive_t *archive;
    collision_named_model_resolver_t model_resolver;
    void *model_context;
} collision_context_t;
static const char *collision_archive_string(void *context,uint32_t reference)
{
    const archive_t *archive=((collision_context_t *)context)->archive;
    return archive->prepared ? LoadBlockString(&archive->blocks,reference) : NULL;
}
static void *collision_archive_model(void *context,uint32_t reference)
{
    const collision_context_t *binding=context;
    const archive_t *archive=binding->archive;
    asset_t asset;
    const char *name;
    uint32_t ref;
    if (!archive->prepared || !binding->model_resolver ||
        !ResolveModelAlias(&archive->blocks,reference,&asset) || asset.size<360) return NULL;
    ref=(uint32_t)asset.data[0]|(uint32_t)asset.data[1]<<8|
        (uint32_t)asset.data[2]<<16|(uint32_t)asset.data[3]<<24;
    if (ref==UINT32_MAX) {
        name=(const char *)(asset.data+360);
        if (!memchr(name,0,asset.size-360)) return NULL;
    } else name=LoadBlockString(&archive->blocks,ref);
    if (!name) return NULL;
    if (*name==',') ++name;
    return *name ? binding->model_resolver(binding->model_context,name) : NULL;
}
int LoadMountedCollision(const char *name,collision_map_t *out)
{
    return LoadMountedCollisionModels(name,NULL,NULL,out);
}
static int c1_readers;
static void c1_reader_report(unsigned type,const char *name,int code);
typedef struct {
    const c1_clipmap_stream_t *stream;
    collision_named_model_resolver_t model_resolver;
    void *model_context;
} c1_clipmap_context_t;
static void *c1_clipmap_model(void *context,uint32_t reference)
{
    const c1_clipmap_context_t *c=context;
    const char *name=C1ClipMapModelName(c->stream,reference);
    if (!name || !c->model_resolver) return NULL;
    if (*name==',') ++name;
    return *name ? c->model_resolver(c->model_context,name) : NULL;
}
/* c1Readers: the same collision from the clipMap record C1 holds (c1_clipmap.c). 1 read, else 0. */
static unsigned c1_pool_builds[44]; /* tentative: defined with c1_backed_fallbacks below */
static const uint8_t *c1_pool_record(unsigned type,const char *name,struct archive_s **owner);
static int c1_collision_from(const uint8_t *record,collision_named_model_resolver_t resolver,void *context,
    collision_map_t *out)
{
    c1_clipmap_stream_t stream={0}; int ok=0;
    if (SerializeC1ClipMap(record,&stream)) {
        c1_clipmap_context_t binding={&stream,resolver,context};
        asset_t asset={15,stream.data,stream.size};
        ok=ReadCollisionMapPlanes(&asset,stream.planes,stream.plane_count,NULL,c1_clipmap_model,&binding,out);
    }
    FreeC1ClipMapStream(&stream);
    return ok;
}
static int c1_collision(const char *name,collision_named_model_resolver_t resolver,void *context,
    collision_map_t *out)
{
    char path[256]; const uint8_t *record=NULL; c1_clipmap_stream_t stream={0}; int ok=0;
    if (snprintf(path,sizeof(path),"maps/mp/%s.d3dbsp",name)>=(int)sizeof(path)) return 0;
    if (!(record=C1Record(15,path)) && !(record=C1Record(14,path))) return 0;
    if (SerializeC1ClipMap(record,&stream)) {
        c1_clipmap_context_t binding={&stream,resolver,context};
        asset_t asset={15,stream.data,stream.size};
        ok=ReadCollisionMapPlanes(&asset,stream.planes,stream.plane_count,NULL,c1_clipmap_model,&binding,out);
    }
    FreeC1ClipMapStream(&stream);
    return ok;
}
int LoadMountedCollisionModels(const char *name,collision_named_model_resolver_t resolver,
    void *context,collision_map_t *out)
{
    archive_t *entry;
    if (!name || !*name || !out) return -1;
    if (c1_readers==1) { /* the level is in a zone C1 holds: its clipMap from C1's own record, no mount */
        char path[256]; archive_t *owner=NULL; const uint8_t *record;
        if (snprintf(path,sizeof(path),"maps/mp/%s.d3dbsp",name)<(int)sizeof(path) &&
            ((record=c1_pool_record(15,path,&owner)) || (record=c1_pool_record(14,path,&owner)))) {
            if (c1_collision_from(record,resolver,context,out)) { ++c1_pool_builds[15]; return 1; }
            Com_PrintWarning(CON_CHANNEL_SYSTEM,"CoDOL collision %s: C1's clipMap record did not convert; mounting the server's copy\n",name);
        }
    }
    for (entry=archives;entry;entry=entry->next) {
        collision_context_t binding={entry,resolver,context};
        int status;
        if (!c1_backed_need_map(entry,name)) continue;
        status=LoadZoneCollisionResolved(&entry->zone,name,collision_archive_string,
            collision_archive_model,&binding,out);
        if (status==1) {
            asset_t collision,world;
            if (FindZoneCollisionPair(&entry->zone,name,&collision,&world)==1) {
                touch_asset(entry,collision.data); touch_asset(entry,world.data);
            }
        }
        if (status==1 && c1_readers) {
            collision_map_t c1={0};
            if (c1_collision(name,resolver,context,&c1)) {
                c1_reader_report(15,name,CompareCollisionMaps(out,&c1));
                if (c1_readers==1) { FreeCollisionMap(out); *out=c1; }
                else FreeCollisionMap(&c1);
            } else c1_reader_report(15,name,-1);
        }
        if (status) return status;
    }
    /* none: say what each mounted archive answered (a map whose collision pair no archive finds) */
    for (entry=archives;entry;entry=entry->next) {
        asset_t collision,world;
        int pair=FindZoneCollisionPair(&entry->zone,name,&collision,&world);
        if (entry->c1_zone) { Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL collision %s: archive rank %d is C1-backed (%s)\n",name,entry->rank,entry->c1_zone); continue; }
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL collision %s: archive rank %d, %u assets, %u bytes: pair %d\n",
                   name,entry->rank,entry->zone.asset_count,(unsigned)entry->zone.size,pair);
    }
    return 0;
}

/* Locate the one addon_map_ents (IW4 type 42) a coop playlist zone ships.
 *
 * The asset name is maps/mp/<playlist>.mapents for all nine (MEASURED
 * 2026-09-20, docs/ADDON_MAP_ENTS_20260920.md), and its name is inline
 * after the 36-byte header for all nine; a de-duplicated block-3 reference is
 * still resolved so a future patch zone that shares the string still matches.
 * Type 42 has no record_asset() entry, so this walks the zone directory the
 * way FindRawFileStatus does instead of using the dependency index.
 * 1 = loaded, 0 = no zone carries it, -1 = present but unreadable. */
static const char *addon_map_ents_name(const archive_t *archive, const asset_t *asset)
{
    const uint8_t *p; size_t available; uint32_t reference;
    if (!asset || asset->type != ASSET_ADDON_MAP_ENTS || asset->size < 36) return NULL;
    p = asset->data;
    reference = (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
    if (reference == UINT32_MAX) { p += 36; available = asset->size-36; }
    else {
        if (!archive->prepared) return NULL;
        return LoadBlockString(&archive->blocks, reference);
    }
    return memchr(p, 0, available) ? (const char *)p : NULL;
}
static int asset_name_equal(const char *, const char *);
static int c1_addon_map_entities(const char *,map_entities_t *);
static int compare_map_entities(const map_entities_t *,const map_entities_t *);
static int c1_glass_map(const char *,glass_map_source_t *);
static int c1_glass_groups(const char *,glass_groups_t *);
static const uint8_t *c1_record_any(unsigned type,const char *name);
static int pool_held(unsigned type,const char *name,archive_t **owner);
static int c1_addon_map_entities(const char *wanted,map_entities_t *out);
int LoadMountedAddonMapEntities(const char *name, map_entities_t *out)
{
    static const char prefix[] = "maps/mp/", suffix[] = ".mapents";
    char wanted[256];
    size_t length;
    archive_t *archive;
    if (!name || !*name || !out) return -1;
    length = strlen(name);
    if (length + sizeof(prefix) + sizeof(suffix) - 1 > sizeof(wanted)) return -1;
    memcpy(wanted, prefix, sizeof(prefix)-1);
    memcpy(wanted + sizeof(prefix)-1, name, length);
    memcpy(wanted + sizeof(prefix)-1 + length, suffix, sizeof(suffix));
    {   archive_t *owner;
        if (pool_held(ASSET_ADDON_MAP_ENTS, wanted, &owner) && c1_addon_map_entities(wanted, out)) return 1;
    }
    for (archive = archives; archive; archive = archive->next) {
        unsigned i;
        if (!c1_backed_need(archive, ASSET_ADDON_MAP_ENTS, wanted)) continue;
        /* The first definition wins, as C1 links it (DB_LinkXAssetEntry). */
        for (i = 1; i <= archive->zone.asset_count; ++i) {
            asset_t asset;
            const char *candidate;
            if (!ZoneAsset(&archive->zone, i-1, &asset)) return -1;
            if (asset.type != ASSET_ADDON_MAP_ENTS) continue;
            candidate = addon_map_ents_name(archive, &asset);
            if (!candidate || candidate[0] == ',' || !asset_name_equal(candidate, wanted)) continue;
            if (!ReadAddonMapEntities(asset.data, asset.size, out)) return -1;
            if (c1_readers) {
                map_entities_t c1 = {0};
                if (c1_addon_map_entities(wanted, &c1)) {
                    c1_reader_report(42, wanted, compare_map_entities(out, &c1));
                    if (c1_readers == 1) { FreeMapEntities(out); *out = c1; } else FreeMapEntities(&c1);
                } else c1_reader_report(42, wanted, -1);
            }
            return 1;
        }
    }
    return 0;
}

/* The level's clipMap record in a zone C1 holds (in-process, c1Readers 1), or NULL. */
static const uint8_t *pool_clipmap(const char *name)
{
    char path[256]; archive_t *owner=NULL; const uint8_t *record;
    if(c1_readers!=1 || snprintf(path,sizeof(path),"maps/mp/%s.d3dbsp",name)>=(int)sizeof(path)) return NULL;
    if(!(record=c1_pool_record(15,path,&owner))) record=c1_pool_record(14,path,&owner);
    return record;
}
/* C1's clipMap (256 bytes): dynEntCount[2] u16 at +168, dynEntDefList[2] at +172 (92-byte DynEntityDef, the
 * serialized layout: +32 XModel, +40 FxEffectDef, +44 PhysPreset, each a record that starts with its name). */
static const uint8_t *pool_dynamic_defs(const uint8_t *clipmap,unsigned category,unsigned *count)
{
    const uint8_t *defs; memcpy(&defs,clipmap+172+4*category,sizeof(defs));
    *count=clipmap[168+2*category]|(unsigned)clipmap[169+2*category]<<8;
    return *count ? defs : NULL;
}
static void *pool_blob(archive_t *a,size_t n);
static const char *archive_text(archive_t *a,const char *text)
{
    char *copy; size_t n;
    if(!text) return NULL;
    n=strlen(text)+1;
    if(!(copy=pool_blob(a,n))) return NULL;
    memcpy(copy,text,n); return copy;
}
static int pool_float(float *out,const uint8_t *p,unsigned count)
{
    unsigned i;
    for(i=0;i<count;++i) { memcpy(out+i,p+4*i,4); if(!isfinite(out[i])) return 0; }
    return 1;
}
int LoadMountedDynamicEntities(const char *name,dynamic_entities_t *out)
{
    archive_t *entry;
    if(!name || !*name || !out) return -1;
    {   /* the level in a zone C1 holds: the definitions from C1's clipMap record */
        const uint8_t *clipmap=pool_clipmap(name);
        if(clipmap) {
            dynamic_entities_t result={0}; unsigned category,i,count; const uint8_t *defs;
            for(category=0;category<2;++category) {
                if(!(defs=pool_dynamic_defs(clipmap,category,&count))) { if(count) goto pool_fail; continue; }
                result.count[category]=count;
                if(!(result.entities[category]=calloc(count,sizeof(*result.entities[category])))) goto pool_fail;
                for(i=0;i<count;++i) {
                    const uint8_t *p=defs+92*i,*a; dynamic_entity_t *e=result.entities[category]+i;
                    memcpy(&e->type,p,4);
                    memcpy(&a,p+32,sizeof(a)); e->model_reference=a ? UINT32_MAX : 0;
                    e->brush_model=p[36]|(unsigned)p[37]<<8; e->physics_brush_model=p[38]|(unsigned)p[39]<<8;
                    memcpy(&a,p+40,sizeof(a)); e->effect_reference=a ? UINT32_MAX : 0;
                    memcpy(&a,p+44,sizeof(a)); e->preset_reference=a ? UINT32_MAX : 0;
                    memcpy(&e->health,p+48,4); memcpy(&e->contents,p+88,4);
                    if(e->type>16 || !pool_float(e->quat,p+4,4) || !pool_float(e->origin,p+20,3) || !pool_float(e->mass,p+52,9))
                        goto pool_fail;
                }
            }
            FreeDynamicEntities(out); *out=result; return 1;
pool_fail:
            FreeDynamicEntities(&result);
            Com_PrintWarning(CON_CHANNEL_SYSTEM,"CoDOL collision %s: C1's dynamic entities did not convert; mounting the server's copy\n",name);
        }
    }
    for(entry=archives;entry;entry=entry->next) {
        asset_t collision,world;
        int status;
        if(!c1_backed_need_map(entry,name)) continue;
        status=FindZoneCollisionPair(&entry->zone,name,&collision,&world);
        if(status<0) return -1;
        if(status==1) return ReadDynamicEntities(&collision,&entry->blocks,out)?1:-1;
    }
    return 0;
}

int MountArchive(const char *path, const char *client_exe)
{
    return MountArchives(&path, 1, client_exe, NULL);
}

/* C1's asset linking (DB_LinkXAssetEntry 0x4cc390): a name keeps the FIRST
 * definition linked under it. A later zone's definition is chained behind it
 * and only takes over when that zone is ffotd_client (zone +0xA4, set at
 * 0x4cd639) or for the two types 0x4cc0a0 lets override (lightdef 22,
 * impactfx 31); a placeholder from a ",name" reference is always replaced.
 * The archive list is therefore kept in C1's load order, earliest first, and
 * every lookup walks it from the head and takes the first definition. An
 * archive's rank is its position in that order (CODOL_LINK_RANK_*, ascending;
 * an ffotd zone is ranked ahead of everything); equal ranks keep mount order. */
static void link_ranked(archive_t *entry)
{
    archive_t **link=&archives;
    while(*link && (*link)->rank<=entry->rank) link=&(*link)->next;
    entry->next=*link; *link=entry;
}
static char *dup_string(const char *s)
{
    char *c; size_t n;
    if(!s) return NULL;
    n=strlen(s)+1; if(!(c=malloc(n))) return NULL;
    memcpy(c,s,n); return c;
}
static unsigned c1_backed_fallbacks,c1_pool_builds[44];
/* Probe: find_dependency_definition stops at the first C1-backed archive that defines (type, name), without
 * mounting it, and leaves it in c1_probe_hit. */
static int c1_probe; static struct archive_s *c1_probe_hit;
/* Open and replay the server's own copy of a C1-backed zone into its entry (same rank, same place). */
static int c1_backed_mount(archive_t *entry,unsigned type,const char *name)
{
    unsigned failed=UINT32_MAX;
    if(!entry->c1_zone) return 1;
    {   /* which lookup needed it: the module offsets of the callers (record paths still to convert) */
        char where[160]={0};
        CallerOffsets(where,sizeof(where),2);
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL C1-backed %s: type %u '%s' has no record path; mounting the server's own copy (from%s)\n",
                   entry->c1_zone,type,name ? name : "",where);
    }
    /* in place: the zone image release registers &entry->blocks for its page refills */
    memset(&entry->zone,0,sizeof(entry->zone)); memset(&entry->blocks,0,sizeof(entry->blocks));
    if(!(entry->c1_key && *entry->c1_key ? OpenFastFile(&entry->zone,entry->c1_path,entry->c1_key) :
                                            OpenZoneFile(&entry->zone,entry->c1_path)) ||
       !ReplayZone(&entry->zone,&entry->blocks,&failed)) {
        CloseZone(&entry->zone); FreeLoadBlocks(&entry->blocks);
        memset(&entry->zone,0,sizeof(entry->zone)); memset(&entry->blocks,0,sizeof(entry->blocks));
        Com_PrintError(CON_CHANNEL_SYSTEM,"CoDOL C1-backed %s: the server's own copy did not mount (asset %u)\n",entry->c1_zone,failed);
        return 0;
    }
    BlockTrim();
    if(free_zone_images) ZoneImageRelease(&entry->zone,&entry->blocks);
    entry->prepared=1;
    entry->dependency_index_attempted=0;
    free(entry->c1_zone); free(entry->c1_path); free(entry->c1_key);
    entry->c1_zone=entry->c1_path=entry->c1_key=NULL;
    ++c1_backed_fallbacks;
    return 1;
}
/* Whether this C1-backed zone may define (type, name): C1's linked definition came from it, or from a zone the
 * server has no archive for (a level's _load zone links first and C1 keeps only the first definition, so the
 * level's own copy is then unseen). A zone the server holds otherwise answers for itself. */
static int c1_backed_find(const archive_t *entry,unsigned type,const char *name)
{
    const c1_zone_host_t *host=C1ZoneHost(); const char *zone; const archive_t *a;
    if(!entry->c1_zone || !host || !host->zone_of || !name || !*name) return 0;
    if(!(zone=host->zone_of(type,*name==',' ? name+1 : name))) return 0;
    if(asset_name_equal(zone,entry->c1_zone)) return 1;
    for(a=archives;a;a=a->next) if(a->zone_name && asset_name_equal(a->zone_name,zone)) return 0;
    return 1;
}
/* Whether a lookup of (type, name) has to look into this archive: 1 for a mounted one; for a C1-backed one, mount
 * the server's copy when C1 defines it there, or when C1 no longer holds the zone (C1 unloads the old level at a map
 * change before the server's next mount retires it); 0 to skip it. */
static int c1_backed_need(archive_t *entry,unsigned type,const char *name)
{
    const c1_zone_host_t *host=C1ZoneHost();
    if(!entry->c1_zone) return 1;
    if(!host || !host->zone_held || !host->zone_held(entry->c1_zone)) return c1_backed_mount(entry,type,"(zone no longer held by C1)");
    return c1_backed_find(entry,type,name) && c1_backed_mount(entry,type,name);
}
/* The same for a lookup by map name (collision, dynamic entities): C1's world asset (type 21) of that map. */
static int c1_backed_need_map(archive_t *entry,const char *map)
{
    char world[MAX_QPATH];
    if(!entry->c1_zone) return 1;
    snprintf(world,sizeof(world),"maps/mp/%s.d3dbsp",map);
    if(c1_backed_need(entry,21,world)) return 1;
    if(!entry->c1_zone) return 1; /* mounted because C1 let the zone go */
    snprintf(world,sizeof(world),"maps/%s.d3dbsp",map);
    return c1_backed_need(entry,21,world);
}
static int mount_archives(const char *const *paths,size_t count,const int *ranks,
                         const char *client_exe,size_t *failed_index,unsigned *failed_asset,int prepare,unsigned group)
{
    archive_t *pending = NULL;
    size_t i;
    if (failed_asset) *failed_asset=UINT32_MAX;
    if (!paths || !count) { if (failed_index) *failed_index = 0; return 0; }
    for (i = 0; i < count; ++i) {
        archive_t *entry = calloc(1, sizeof(*entry));
        if (!entry) goto fail;
        if (paths[i]) {
            const char *p=paths[i],*b=p,*q; size_t n;
            if (!strncmp(p,"c1:",3) && strchr(p,'|')) p=strchr(p,'|')+1;
            for (q=p;*q;++q) if (*q=='/' || *q=='\\') b=q+1;
            if (b<p) b=p;
            n=strlen(b); if (n>3 && asset_name_equal(b+n-3,".ff")) n-=3;
            if ((entry->zone_name=malloc(n+1))) { memcpy(entry->zone_name,b,n); entry->zone_name[n]=0; }
        }
        /* "c1:<zone>|<path>": a zone C1 holds; only remember where the server's own copy is */
        if (paths[i] && !strncmp(paths[i],"c1:",3) && strchr(paths[i],'|')) {
            const char *bar=strchr(paths[i],'|');
            entry->c1_zone=malloc((size_t)(bar-paths[i]-3)+1);
            if (!entry->c1_zone) { free(entry->zone_name); free(entry); goto fail; }
            memcpy(entry->c1_zone,paths[i]+3,(size_t)(bar-paths[i]-3)); entry->c1_zone[bar-paths[i]-3]=0;
            entry->c1_path=dup_string(bar+1); entry->c1_key=dup_string(client_exe);
            entry->group=group; entry->rank=ranks ? ranks[i] : unranked_rank-(int)i-1;
            entry->next=pending; pending=entry;
            continue;
        }
        if (!paths[i] || !(client_exe && *client_exe ?
            OpenFastFile(&entry->zone, paths[i], client_exe) :
            OpenZoneFile(&entry->zone, paths[i]))) {
            free(entry->zone_name); free(entry); goto fail;
        }
        if (prepare && !ReplayZone(&entry->zone,&entry->blocks,failed_asset)) {
            /* The type and offsets of the refused asset: the index alone is no
             * help with a zone a port tool just wrote. */
            if (failed_asset && *failed_asset<entry->zone.asset_count && entry->zone.data &&
                entry->zone.table_offset<=entry->zone.size &&
                entry->zone.asset_count<=(entry->zone.size-entry->zone.table_offset)/16) {
                const uint8_t *row=(const uint8_t *)entry->zone.data+entry->zone.table_offset+16*(size_t)*failed_asset;
                Com_PrintWarning(CON_CHANNEL_SYSTEM,
                    "CoDOL zone replay: %s asset %u of %u refused (type %u, header 0x%08X, stream 0x%08X, extent 0x%08X)\n",
                    paths[i],*failed_asset,entry->zone.asset_count,
                    (unsigned)(row[0]|row[1]<<8|row[2]<<16|(uint32_t)row[3]<<24),
                    (unsigned)(row[4]|row[5]<<8|row[6]<<16|(uint32_t)row[7]<<24),
                    (unsigned)(row[8]|row[9]<<8|row[10]<<16|(uint32_t)row[11]<<24),
                    (unsigned)(row[12]|row[13]<<8|row[14]<<16|(uint32_t)row[15]<<24));
            }
            CloseZone(&entry->zone); free(entry->zone_name); free(entry); goto fail;
        }
        /* freeZoneImages: the image view goes read-only and out of the working set once its zone is replayed */
        if (prepare) BlockTrim(); /* the replay's dirty block pages go to their files */
        if (prepare && free_zone_images) ZoneImageRelease(&entry->zone,&entry->blocks);
        entry->prepared=prepare;
        entry->group=group;
        /* Unranked input is ascending override priority (the last path wins),
         * as it always was: each archive goes ahead of everything before it. */
        entry->rank=ranks ? ranks[i] : unranked_rank-(int)i-1;
        entry->next = pending; pending = entry;
    }
    if(!ranks) unranked_rank-=(int)count;
    if(group) {
        archive_t **link=&archives;
        while(*link) {
            archive_t *entry=*link;
            if(entry->group==group) {
                *link=entry->next; entry->next=retired_archives; retired_archives=entry;
            } else link=&entry->next;
        }
    }
    /* A new archive can replace any named dependency used by a composed
     * weapon, even without replacing an existing group. Preserve old cores
     * for live users, but force subsequent names to resolve the new mount. */
    RetireComposedWeapons();
    while(pending) {
        archive_t *entry=pending; pending=entry->next;
        link_ranked(entry);
    }
    ++mount_serial; C1MaterializeReset();
    return 1;
fail:
    if (failed_index) *failed_index = i;
    while (pending) {
        archive_t *entry = pending; pending = entry->next;
        free(entry->dependency_index); free(entry->c1_zone); free(entry->zone_name); free(entry->c1_path); free(entry->c1_key);
        FreeLoadBlocks(&entry->blocks); CloseZone(&entry->zone); free(entry);
    }
    return 0;
}
int MountArchives(const char *const *paths,size_t count,const char *client_exe,size_t *failed_index)
{
    return mount_archives(paths,count,NULL,client_exe,failed_index,NULL,0,0);
}
int MountRuntimeArchives(const char *const *paths,size_t count,const char *client_exe,
                              size_t *failed_index,unsigned *failed_asset)
{
    return mount_archives(paths,count,NULL,client_exe,failed_index,failed_asset,1,0);
}
int ReplaceRuntimeArchives(const char *const *paths,size_t count,const char *client_exe,
                                unsigned group,size_t *failed_index,unsigned *failed_asset)
{
    if(!group) return 0;
    return mount_archives(paths,count,NULL,client_exe,failed_index,failed_asset,1,group);
}
int MountRankedArchives(const char *const *paths,const int *ranks,size_t count,const char *client_exe,
                              size_t *failed_index,unsigned *failed_asset)
{
    if(!ranks) return 0;
    return mount_archives(paths,count,ranks,client_exe,failed_index,failed_asset,1,0);
}
int ReplaceRankedArchives(const char *const *paths,const int *ranks,size_t count,const char *client_exe,
                                unsigned group,size_t *failed_index,unsigned *failed_asset)
{
    if(!group || !ranks) return 0;
    return mount_archives(paths,count,ranks,client_exe,failed_index,failed_asset,1,group);
}
/* How many archives of a mount group are currently live. A restart of the
 * same map can reuse them instead of re-opening, decrypting and replaying
 * every zone: mount_archives retires the old group rather than freeing it
 * (retired cores are still reachable from composed weapons), so remounting
 * unchanged content keeps a second full copy of the map resident. */
unsigned MountedArchiveCount(unsigned group)
{
    const archive_t *entry; unsigned count=0;
    for(entry=archives;entry;entry=entry->next)
        if(entry->group==group && entry->prepared) ++count;
    return count;
}
/* Move a mount group aside WITHOUT mounting a replacement, so a caller that
 * knows the outgoing asset category is finished can unload it before the next
 * one is opened instead of holding both resident. Same retirement the mount
 * path performs (archives whose data is still reachable from a composed weapon
 * must not be freed here), so ReleaseRetiredArchives, and every borrower
 * reset it lists, still have to follow. group 0 is the startup set and is
 * refused: nothing in it is category-specific. Returns how many were retired. */
unsigned RetireRuntimeArchiveGroup(unsigned group)
{
    archive_t **link=&archives; unsigned count=0;
    if(!group) return 0;
    while(*link) {
        archive_t *entry=*link;
        if(entry->group==group) {
            *link=entry->next; entry->next=retired_archives; retired_archives=entry; ++count;
        } else link=&entry->next;
    }
    if(count) { RetireComposedWeapons(); ++mount_serial; C1MaterializeReset(); }
    return count;
}
int MountedBlockViews(unsigned index,model_block_t views[8])
{
    archive_t *entry=archives;
    if (!views) return 0;
    while (entry && index) { entry=entry->next; --index; }
    if (!entry || !entry->prepared) return 0;
    LoadBlockViews(&entry->blocks,views); return 1;
}

int LoadScriptAssetStatus(const char *name, uint8_t **out, size_t *size)
{
    const engine_rawfile_t *rawfile;
    uint8_t *data;
    size_t length;
    int status;
    if (!name || !out || !size) return -1;
    /* Share archive precedence and reconstructed XString resolution with the
     * engine rawfile path. The standalone zone reader knows inline names only. */
    status = LoadEngineRawFile(name, &rawfile);
    if (status != 1) return status;
    if (!rawfile->buffer || rawfile->len < 0) return -1;
    length = (size_t)rawfile->len;
    /* This VM compiles source. Binary bytecode must be translated through
     * a separate loader, never silently truncated at an embedded NUL. */
    if (length > 16*1024*1024 || memchr(rawfile->buffer, 0, length)) return -1;
    data = malloc(length + 1);
    if (!data) return -1;
    memcpy(data, rawfile->buffer, length + 1);
    *out = data; *size = length; return 1;
}

int LoadScriptAsset(const char *name, uint8_t **out, size_t *size)
{
    return LoadScriptAssetStatus(name, out, size) == 1;
}

static int asset_name_equal(const char *a, const char *b)
{
    for (;;) {
        unsigned x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x == '\\') x = '/';
        if (y == '\\') y = '/';
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
        if (!x) return 1;
    }
}
static const char *dependency_asset_name(const archive_t *,const asset_t *);
/* c1Readers: 0 the converters parse the serialized zone image; 1 they read C1 records (C1Record, zones
 * replayed with c1Fixups 1); 2 both, compared field by field, the serialized result kept (a self-check).
 * Converted so far: xmodel. */
static unsigned c1_reader_count[44],c1_reader_mismatch[44];
void SetC1Readers(int mode) { c1_readers=mode; }
static const char *record_string(void *context,unsigned id) { (void)context; return C1RecordString(id); }
static char c1_reader_firsts[8][96];
static void c1_reader_report(unsigned type,const char *name,int code)
{
    ++c1_reader_count[type];
    if (code) {
        unsigned n=0,t;
        for (t=0;t<44;++t) n+=c1_reader_mismatch[t];
        if (n<8) snprintf(c1_reader_firsts[n],sizeof(c1_reader_firsts[n]),"type %u %s: field %d",type,name ? name : "?",code);
        ++c1_reader_mismatch[type];
    }
}
/* Whether a model cache built for an earlier mount still resolves as it did: every lod it built finds the same
 * xmodelsurfs definition now (a later archive can override one by name). A model of the startup set (group 0,
 * never retired) is then kept across mounts, as its native weapon, effect and table caches are, which hold its
 * XModel. Rebuilding it for every mount left the old copy in its archive's list for good: MEASURED 2026-10-28,
 * +11.5 MB of xmodel surfaces, streams and collision per mp/zm/coop/survival rotation cycle. */
static archive_t *find_dependency_definition(unsigned,const char *,asset_t *);
static const char *dependency_asset_name(const archive_t *,const asset_t *);
static int model_surface_definition(model_cache_t *cached,unsigned lod,const uint8_t **definition)
{
    xmodel_lods_t lods; asset_t assets[4]; const xmodel_lod_t *expected; const char *surface_name;
    *definition=NULL;
    if (!ReadXModelLods(&cached->source,&lods) || lod<lods.first || lod>=lods.count) return 0;
    expected=lods.lods+lod;
    if (!XModelInlineSurfaceAssets(&cached->source,assets)) return 0;
    if (!assets[lod].data) {
        if (!expected->surfaces_reference) return 1;
        if (!ResolveSurfaceAlias(&cached->archive->blocks,expected->surfaces_reference,assets+lod)) return 0;
    }
    surface_name=dependency_asset_name(cached->archive,assets+lod);
    if (surface_name && *surface_name && !find_dependency_definition(3,surface_name+(surface_name[0]==','),assets+lod) &&
        surface_name[0]==',') return 0;
    *definition=assets[lod].data;
    return 1;
}
static int model_cache_current(model_cache_t *cached)
{
    unsigned lod; const uint8_t *now;
    for (lod=0;lod<4;++lod) if (cached->surface_source[lod]) {
        if (!model_surface_definition(cached,lod,&now) || now!=cached->surface_source[lod]) return 0;
    }
    return 1;
}
static const uint8_t *rec_word_ptr(const uint8_t *record,unsigned offset)
{ const uint8_t *v; memcpy(&v,record+offset,sizeof(v)); return v; }
static void pool_presence(uint8_t *header,const uint8_t *record,unsigned offset)
{
    uint32_t v=rec_word_ptr(record,offset) ? UINT32_MAX : 0; memcpy(header+offset,&v,4);
}
static const char *c1_script_text(void *context,unsigned id);
/* The lod's XModelSurfs record in C1's model, or NULL. */
static const uint8_t *pool_lod_surfaces(const model_cache_t *cached,unsigned lod)
{ return lod<4 ? rec_word_ptr(cached->c1_record,0x48+56*lod+8) : NULL; }
/* A model cache from C1's own XModel record (0x168 bytes, loader 0x4bbb80): skeleton and bone names through C1's
 * script strings; geometry, materials, surfaces and physics are read from the record by the functions below. NULL
 * when the record does not convert (the caller mounts the server's copy): a lod whose surface count C1 rewrote at
 * load (measured on attachment models) does not match its XModelSurfs and is one such case. */
static model_cache_t *build_pool_model(archive_t *owner,const uint8_t *live)
{
    model_cache_t *cached; uint8_t *fixed,*header; unsigned i,index=0;
    xmodel_lods_t lods; asset_t h;
    /* C1 rewrites some header words when it links a model (load_blocks.c replay_model: the surface count +6,
     * each lod's surface count and index +4..+7 and its runtime words): the copy gets the serialized values back,
     * the counts from each lod's XModelSurfs and the indices running on from the first lod. Pointer words stay C1's. */
    if (!(fixed=malloc(2*0x168))) return NULL;
    memcpy(fixed,live,0x168); header=fixed+0x168;
    if (fixed[0x129]>4 || fixed[0x128]>fixed[0x129]) { free(fixed); return NULL; }
    for (i=0;i<4;++i) {
        uint8_t *lod=fixed+0x48+56*i; const uint8_t *surfs=rec_word_ptr(live,0x48+56*i+8);
        unsigned count=i>=fixed[0x128] && i<fixed[0x129] && surfs ? (surfs[8]|(unsigned)surfs[9]<<8) : 0;
        lod[4]=(uint8_t)count; lod[5]=(uint8_t)(count>>8);
        lod[6]=(uint8_t)index; lod[7]=(uint8_t)(index>>8);
        index+=count;
    }
    if (index>255) { free(fixed); return NULL; }
    fixed[6]=(uint8_t)index;
    memcpy(header,fixed,0x168);
    pool_presence(header,fixed,0);
    for (i=0;i<6;++i) pool_presence(header,fixed,0x2c+4*i);
    pool_presence(header,fixed,0x44);
    for (i=0;i<4;++i) { pool_presence(header,fixed,0x48+56*i+8); pool_presence(header,fixed,0x48+56*i+12); }
    pool_presence(header,fixed,0x12c); pool_presence(header,fixed,0x138);
    pool_presence(header,fixed,0x160); pool_presence(header,fixed,0x164);
    h=(asset_t){4,header,0x168};
    if (!ReadXModelLods(&h,&lods)) { free(fixed); return NULL; }
    if (!(cached=calloc(1,sizeof(*cached)))) { free(fixed); return NULL; }
    if (!ReadC1XModelSkeleton(fixed,header,c1_script_text,NULL,&cached->model.skeleton,&cached->model.bone_names) ||
        cached->model.bone_names.unresolved) {
        FreeXModelBoneNames(&cached->model.bone_names); FreeXModelSkeleton(&cached->model.skeleton);
        free(cached); free(fixed); return NULL;
    }
    /* c1_record: the corrected copy (its pointers are C1's); the identity of the C1 record is pool_live */
    cached->pool=1; cached->pool_header=fixed; cached->pool_live=live; cached->c1_record=fixed; cached->c1_header=header;
    cached->source=h; cached->archive=owner; cached->generation=mount_serial;
    for (i=0;i<4;++i) cached->surface_source[i]=rec_word_ptr(live,0x48+56*i+8);
    cached->next=owner->models; owner->models=cached;
    ++c1_pool_builds[4];
    return cached;
}
int LoadMountedXModelSkeleton(const char *name,const mounted_model_skeleton_t **out)
{
    archive_t *archive;
    if (!name || !*name || !out) return -1;
    if (c1_readers==1) { /* defined in a zone C1 holds: C1's own record, no mount */
        archive_t *owner=NULL; const uint8_t *record=c1_pool_record(4,name,&owner);
        if (record) {
            model_cache_t *cached;
            for (cached=owner->models;cached;cached=cached->next)
                if (cached->pool && cached->pool_live==record) { *out=&cached->model; return 1; }
            if ((cached=build_pool_model(owner,record))) { *out=&cached->model; return 1; }
        }
    }
    for (archive=archives;archive;archive=archive->next) {
        model_cache_t *cached;
        unsigned index,limit=archive->zone.asset_count;
        const asset_t *selected=NULL;
        for (cached=archive->models;cached;cached=cached->next)
            if (asset_name_equal(cached->model.skeleton.name,name) &&
                (cached->generation==mount_serial || (!archive->group && model_cache_current(cached)))) {
                cached->generation=mount_serial; *out=&cached->model; return 1;
            }
        if (!c1_backed_need(archive,4,name)) continue;
        limit=archive->zone.asset_count;
        if (archive->prepared) {
            size_t i;
            /* Models can be defined inside the graphics world or another
             * asset, without their own directory entry. C1's DB_LinkXAssetEntry
             * keeps the first definition it links (0x4cc390), so the lowest
             * source address wins even though aliases are sorted by reference. */
            for (i=0;i<archive->blocks.asset_alias_count;++i) {
                const asset_t *asset=&archive->blocks.asset_aliases[i].asset;
                const char *candidate;
                if (asset->type!=4) continue;
                candidate=dependency_asset_name(archive,asset);
                if (!candidate || *candidate==',' || !asset_name_equal(candidate,name)) continue;
                if (!selected || asset->data<selected->data) selected=asset;
            }
            if (!selected) continue;
            limit=1;
        }
        for (index=1;index<=limit;++index) {
            asset_t asset;
            xmodel_skeleton_t source={0},owned={0};
            model_block_t views[8];
            if (selected) asset=*selected;
            else if (!ZoneAsset(&archive->zone,index-1,&asset)) return -1;
            if (asset.type!=4) continue;
            if (!ReadXModelSkeleton(&asset,&source)) return -1;
            if (source.name && !asset_name_equal(source.name,name)) { FreeXModelSkeleton(&source); continue; }
            if (!archive->prepared) { FreeXModelSkeleton(&source); return -1; }
            LoadBlockViews(&archive->blocks,views);
            views[0]=(model_block_t){0}; /* discarded temporary headers */
            if (!RelocateXModelSkeleton(&source,views,&owned)) { FreeXModelSkeleton(&source); return -1; }
            FreeXModelSkeleton(&source);
            if (!owned.name || !asset_name_equal(owned.name,name)) { FreeXModelSkeleton(&owned); continue; }
            if (!archive->script_strings && !ReadScriptStrings(&archive->zone,&archive->script_strings)) {
                FreeXModelSkeleton(&owned); return -1;
            }
            cached=calloc(1,sizeof(*cached));
            if (!cached) { FreeXModelSkeleton(&owned); return -1; }
            touch_asset(archive,asset.data);
            cached->model.skeleton=owned;
            cached->source=asset;
            cached->archive=archive;
            cached->generation=mount_serial;
            if (!ResolveXModelBoneNames(&owned,archive->script_strings,archive->zone.string_count,&cached->model.bone_names) ||
                cached->model.bone_names.unresolved) {
                FreeXModelBoneNames(&cached->model.bone_names);
                FreeXModelSkeleton(&cached->model.skeleton); free(cached); return -1;
            }
            if (c1_readers) {
                const c1_header_t *h=C1Header(&archive->blocks,asset.data);
                const uint8_t *record=C1Record(4,owned.name);
                if (h && record) {
                    mounted_model_skeleton_t c1={{0}};
                    cached->c1_header=OL_BlockAddress(&archive->blocks,h->header);
                    if (cached->c1_header && ReadC1XModelSkeleton(record,cached->c1_header,record_string,NULL,
                                                                      &c1.skeleton,&c1.bone_names)) {
                        cached->c1_record=record;
                        c1_reader_report(4,owned.name,CompareXModelConversions(&cached->model,NULL,&c1,NULL));
                        if (c1_readers==1) {
                            FreeXModelBoneNames(&cached->model.bone_names);
                            FreeXModelSkeleton(&cached->model.skeleton);
                            cached->model=c1;
                        } else {
                            FreeXModelBoneNames(&c1.bone_names); FreeXModelSkeleton(&c1.skeleton);
                        }
                    } else c1_reader_report(4,owned.name,-1);
                }
            }
            cached->next=archive->models; archive->models=cached;
            *out=&cached->model; return 1;
        }
    }
    return 0;
}
int LoadMountedXModelGeometry(const char *name,const mounted_model_geometry_t **out)
{
    const mounted_model_skeleton_t *model;
    mounted_model_geometry_t pending={0};
    model_cache_t *cached;
    int status;
    if (!out) return -1;
    status=LoadMountedXModelSkeleton(name,&model);
    if (status!=1) return status;
    cached=(model_cache_t *)model;
    if (!cached->geometry_ready) {
        mounted_model_geometry_t c1={{0}};
        int from_record;
        /* records are dropped whenever the mounted set changes: take the current one (C1's own stays) */
        if (cached->c1_record && !cached->pool) cached->c1_record=C1Record(4,model->skeleton.name);
        if (cached->pool) {
            if (!ReadC1XModelGeometry(cached->c1_record,cached->c1_header,&pending)) return -1;
            cached->geometry=pending; cached->geometry_ready=1;
            *out=&cached->geometry; return 1;
        }
        from_record=cached->c1_record && ReadC1XModelGeometry(cached->c1_record,cached->c1_header,&c1);
        if (cached->c1_record && !from_record) c1_reader_report(4,model->skeleton.name,-2);
        if (from_record && c1_readers==1) pending=c1;
        else if (!ReadXModelCollision(&cached->source,&pending.collision) ||
            !ReadXModelBounds(&cached->source,&pending.bounds) ||
            !ReadXModelLods(&cached->source,&pending.lods)) {
            FreeXModelCollision(&pending.collision);
            FreeXModelBounds(&pending.bounds);
            if (from_record) { FreeXModelCollision(&c1.collision); FreeXModelBounds(&c1.bounds); }
            return -1;
        }
        if (from_record && c1_readers==2) {
            int code=CompareXModelConversions(model,&pending,model,&c1);
            if (code) c1_reader_report(4,model->skeleton.name,100+code);
            FreeXModelCollision(&c1.collision); FreeXModelBounds(&c1.bounds);
        }
        cached->geometry=pending; cached->geometry_ready=1;
    }
    *out=&cached->geometry; return 1;
}
int LoadMountedModelMaterials(const char *name,const xmodel_materials_t **out)
{
    const mounted_model_skeleton_t *model; model_cache_t *cached; int status;
    if (!out) return -1;
    status=LoadMountedXModelSkeleton(name,&model); if(status!=1) return status;
    cached=(model_cache_t *)model;
    if(!cached->materials_ready) {
        if(cached->pool) { /* C1's material pointers: the slot keeps the record, MaterialSource names it */
            const uint8_t *array=rec_word_ptr(cached->c1_record,0x44); unsigned i,count=cached->c1_record[6];
            xmodel_materials_t m={0};
            m.count=array ? count : 0;
            if(m.count && !(m.materials=calloc(m.count,sizeof(*m.materials)))) return -1;
            for(i=0;i<m.count;++i) {
                const uint8_t *material=rec_word_ptr(array,4*i);
                m.materials[i].reference=material ? UINT32_MAX : 0;
                m.materials[i].inline_asset=(asset_t){5,material,0};
            }
            cached->materials=m;
        } else if(!ReadXModelMaterials(&cached->source,&cached->materials)) return -1;
        cached->materials_ready=1;
    }
    *out=&cached->materials; return 1;
}
static archive_t *find_dependency_definition(unsigned,const char *,asset_t *);
/* Stage 3 (in-process host): C1's own records. The host installs a lookup into C1's asset database; the server takes
 * C1's record for an asset whose linked definition is in a zone C1 loads once at startup and never unloads (the
 * code_post_gfx set, rank STARTUP), so the record outlives every server use. Other zones (common_mp, the level)
 * stay the server's own: C1 loads them on its own schedule, which a hosted map change runs ahead of. */
static const void *(*c1_provider)(unsigned,const char *);
static unsigned c1_provided[44];
void SetC1AssetProvider(const void *(*find)(unsigned,const char *))
{
    c1_provider=find; memset(c1_provided,0,sizeof(c1_provided));
}
unsigned C1ProvidedCount(unsigned type) { return type<44 ? c1_provided[type] : 0; }
static int permanent_archive(const archive_t *archive)
{
    return archive && archive->rank>=LINK_RANK_STARTUP && archive->rank<LINK_RANK_COMMON;
}
static const void *provided_record(unsigned type,const char *name,const archive_t *defining)
{
    const void *record;
    if(!c1_provider || !permanent_archive(defining) || !name || !*name) return NULL;
    record=c1_provider(type,*name==',' ? name+1 : name);
    if(record && type<44) ++c1_provided[type];
    return record;
}
int LoadMountedModelMaterialSource(const char *name,unsigned index,mounted_model_material_t *out)
{
    const xmodel_materials_t *materials;
    const mounted_model_skeleton_t *model;
    const xmodel_material_t *slot;
    mounted_model_material_t result={0};
    archive_t *archive;
    int status;
    if (!out) return -1;
    status=LoadMountedModelMaterials(name,&materials); if (status!=1) return status;
    if (index>=materials->count) return -1;
    slot=materials->materials+index;
    if (LoadMountedXModelSkeleton(name,&model)!=1) return -1;
    archive=((const model_cache_t *)model)->archive;
    if (((const model_cache_t *)model)->pool) { /* by name: the engine material comes from C1's record too */
        const char *n=slot->inline_asset.data ? (const char *)rec_word_ptr(slot->inline_asset.data,0) : NULL;
        if (!n || !*n) return -1;
        result.name=*n==',' ? n+1 : n; result.asset=(asset_t){5,NULL,0};
        *out=result; return 1;
    }
    if (slot->inline_asset.data) result.asset=slot->inline_asset;
    else if (!ResolveMaterialAlias(&archive->blocks,slot->reference,&result.asset)) return -1;
    result.name=dependency_asset_name(archive,&result.asset);
    if (!result.name || !*result.name) return -1;
    /* Asset linking also redirects real definitions to the current override. */
    {   archive_t *owner;
        if (pool_held(5,result.name[0]==',' ? result.name+1 : result.name,&owner)) {
            if (result.name[0]==',') ++result.name;
            result.asset=(asset_t){5,NULL,0}; memset(result.blocks,0,sizeof(result.blocks));
            *out=result; return 1;
        }
    }
    archive=find_dependency_definition(5,result.name[0]==',' ? result.name+1 : result.name,&result.asset);
    if (!archive) return -1;
    result.name=dependency_asset_name(archive,&result.asset);
    LoadBlockViews(&archive->blocks,result.blocks);
    result.blocks[0]=(model_block_t){0};
    *out=result; return 1;
}
int LoadMountedModelMaterial(const char *name,unsigned index,asset_t *out)
{
    mounted_model_material_t source; int status;
    if (!out) return -1;
    status=LoadMountedModelMaterialSource(name,index,&source);
    if (status==1) *out=source.asset;
    return status;
}
static const char *dependency_asset_name(const archive_t *archive,const asset_t *asset)
{
    uint32_t ref; const uint8_t *p; size_t available,offset,header;
    if (!asset || (asset->type>2 && asset->type!=3 && asset->type!=4 && asset->type!=5 && asset->type!=11 && asset->type!=12 && asset->type!=13 && asset->type!=16 && asset->type!=18 && asset->type!=20 && asset->type!=28 && asset->type!=30 && asset->type!=39 && asset->type!=40 && asset->type!=41 && asset->type!=43 &&
        asset->type!=9 && asset->type!=10 && asset->type!=14 && asset->type!=15 && asset->type!=19 && asset->type!=21 && asset->type!=42)) return NULL;
    /* Sound headers: 12-byte alias list, 136-byte curve, 48-byte loaded sound;
     * all three carry their name reference in the leading word. */
    header=asset->type==0 ? 68 : asset->type==1 ? 72 : asset->type==2 ? 88 : asset->type==3 ? 48 : asset->type==4 ? 360 : asset->type==11 ? 12 : asset->type==12 ? 136 : asset->type==13 ? 48 : asset->type==16 ? 16 : asset->type==18 ? 84 : asset->type==20 ? 116 : asset->type==28 ? 0xbfc : asset->type==30 ? 32 : asset->type==39 ? 12 : asset->type==40 ? 112 : asset->type==41 ? 0x314 : asset->type==43 ? 12 :
        asset->type==9 ? 204 : asset->type==10 ? 36 : asset->type==14 || asset->type==15 ? 256 : asset->type==19 ? 44 : asset->type==21 ? 628 : asset->type==42 ? 36 : 96;
    if (asset->size<header) return NULL;
    /* GfxImage keeps its name at +32; C1 0x4b92d0 reads it right after the header, before the load definition */
    p=asset->data+(asset->type==10 ? 32 : 0); ref=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
    if (ref==UINT32_MAX) { p=asset->data+header; available=asset->size-header; } /* the inline name follows the header */
    else {
        if (!ref || ((ref-1)>>28)!=3) return NULL;
        offset=(ref-1)&0xfffffff;
        if (offset>=archive->blocks.written[3]) return NULL;
        p=archive->blocks.data[3]+offset; available=archive->blocks.written[3]-offset;
    }
    return memchr(p,0,available) ? (const char *)p : NULL;
}
/* Hash normalization must match asset_name_equal byte for byte: ASCII case
 * folding and slash equivalence only. Preserve punctuation and non-ASCII. */
static uint32_t dependency_name_hash(unsigned type,const char *name)
{
    uint32_t hash=2166136261u^type;
    unsigned c;
    while((c=(unsigned char)*name++)!=0) {
        if(c=='\\')c='/';
        if(c>='A' && c<='Z')c+=32;
        hash=(hash^c)*16777619u;
    }
    return hash;
}
/* Replayed blocks and aliases are immutable once the archive is published.
 * Index only complete archives; failure leaves the original scan available.
 * Nothing is shared between archive generations or built during zone replay. */
static int prepare_dependency_index(archive_t *archive)
{
    dependency_index_entry_t *index;
    size_t capacity=16,i,count=archive->blocks.asset_alias_count;
    if(archive->dependency_index)return 1;
    if(!archive->prepared || archive->dependency_index_attempted)return 0;
    archive->dependency_index_attempted=1;
    if(!count)return 0;
    if(count>(size_t)-1/2)return 0;
    while(capacity<count*2) {
        if(capacity>(size_t)-1/2)return 0;
        capacity*=2;
    }
    if(capacity>(size_t)-1/sizeof(*index))return 0;
    index=calloc(capacity,sizeof(*index));
    if(!index)return 0;
    for(i=0;i<count;++i) {
        const asset_t *asset=&archive->blocks.asset_aliases[i].asset;
        const char *name=dependency_asset_name(archive,asset);
        size_t slot;
        if(!name || !*name || name[0]==',')continue;
        slot=dependency_name_hash(asset->type,name)&(capacity-1);
        while(index[slot].asset) {
            if(index[slot].asset->type==asset->type &&
               asset_name_equal(index[slot].name,name))break;
            slot=(slot+1)&(capacity-1);
        }
        /* The first serialized definition wins, as C1 links it (the alias
         * array is sorted by reference, not source order). */
        if(!index[slot].asset || asset->data<index[slot].asset->data) {
            index[slot].name=name;index[slot].asset=asset;
        }
    }
    archive->dependency_index=index;
    archive->dependency_index_capacity=capacity;
    return 1;
}
/* sh1x_unused: mark the directory asset whose serialized bytes hold data (an inline definition marks the asset it
 * is inside). The directory tiles the image in order: asset k starts at its entry's offset + 0x28. */
static uint32_t dir_u32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void touch_asset(archive_t *a,const uint8_t *data)
{
    const zone_t *z; size_t at; uint32_t lo=0,hi;
    if(!a || !data || !(z=&a->zone)->data || data<z->data || data>=z->data+z->size || !z->asset_count) return;
    if(z->table_offset>z->size || z->asset_count>(z->size-z->table_offset)/16) return;
    if(!a->touched && !(a->touched=calloc((z->asset_count+7)/8,1))) return;
    at=(size_t)(data-z->data); hi=z->asset_count;
    while(hi-lo>1) { uint32_t mid=lo+(hi-lo)/2; if((size_t)dir_u32(z->data+z->table_offset+16*mid+4)+0x28<=at) lo=mid; else hi=mid; }
    a->touched[lo>>3]|=(uint8_t)(1u<<(lo&7));
}
/* Lightdefs (22) and impact effects (31) are the types C1 lets a later
 * definition override (0x4cc0a0 outside the lost-device reload): the LAST
 * definition in load order wins, in the last zone that has one. */
static archive_t *find_last_dependency_definition(unsigned type,const char *name,asset_t *out)
{
    archive_t *archive,*owner=NULL;
    for (archive=archives;archive;archive=archive->next) {
        const asset_t *winner=NULL; size_t i;
        if (!c1_backed_need(archive,type,name)) continue;
        for (i=0;i<archive->blocks.asset_alias_count;++i) {
            const asset_t *asset=&archive->blocks.asset_aliases[i].asset;
            const char *candidate;
            if (asset->type!=type) continue;
            candidate=dependency_asset_name(archive,asset);
            if (!candidate || candidate[0]==',' || !asset_name_equal(candidate,name)) continue;
            if (!winner || asset->data>winner->data) winner=asset;
        }
        if (winner) { *out=*winner; owner=archive; touch_asset(archive,winner->data); }
    }
    return owner;
}
/* DB_LinkXAssetEntry's leading comma denotes a reference, not an overriding
 * definition. Search real definitions in C1's load order and take the first,
 * as C1 links it (see link_ranked), including inline assets. */
static archive_t *find_dependency_definition(unsigned type,const char *name,asset_t *out)
{
    archive_t *archive;
    if (!name || !*name) return NULL;
    if (type==22 || type==31) return find_last_dependency_definition(type,name,out);
    for (archive=archives;archive;archive=archive->next) {
        const asset_t *winner=NULL; size_t i;
        if(archive->c1_zone) { /* C1 decides whether this zone defines it; a hit needs the server's copy here */
            if(!c1_backed_find(archive,type,name)) continue;
            if(c1_probe) { c1_probe_hit=archive; return NULL; } /* a record path asks first (c1_pool_record) */
            if(!c1_backed_mount(archive,type,name)) continue;
        }
        if(prepare_dependency_index(archive)) {
            size_t mask=archive->dependency_index_capacity-1;
            size_t slot=dependency_name_hash(type,name)&mask;
            const dependency_index_entry_t *entry;
            while((entry=&archive->dependency_index[slot])->asset) {
                if(entry->asset->type==type && asset_name_equal(entry->name,name)) {
                    *out=*entry->asset;touch_asset(archive,out->data);return archive;
                }
                slot=(slot+1)&mask;
            }
            continue;
        }
        for (i=0;i<archive->blocks.asset_alias_count;++i) {
            const asset_t *asset=&archive->blocks.asset_aliases[i].asset;
            const char *candidate;
            if (asset->type!=type) continue;
            candidate=dependency_asset_name(archive,asset);
            if (!candidate || candidate[0]==',' || !asset_name_equal(candidate,name)) continue;
            if (!winner || asset->data<winner->data) winner=asset;
        }
        if (winner) { *out=*winner; touch_asset(archive,out->data); return archive; }
    }
    return NULL;
}
/* C1's own record of (type, name) when the server's winning definition is in a zone C1 holds (C1-backed): a record
 * path reads it instead of mounting the server's copy. NULL otherwise (the server's definition answers), and while
 * C1's database links a zone (the provider gives nothing then). *owner is the C1-backed archive. */
static const uint8_t *c1_pool_record(unsigned type,const char *name,archive_t **owner)
{
    const c1_zone_host_t *host=C1ZoneHost(); asset_t d; archive_t *a; const uint8_t *r;
    if(!c1_provider || !host || !host->zone_held || !name || !*name) return NULL;
    c1_probe=1; c1_probe_hit=NULL;
    a=find_dependency_definition(type,name,&d);
    c1_probe=0;
    if(a || !c1_probe_hit || !host->zone_held(c1_probe_hit->c1_zone)) return NULL;
    if(!(r=c1_provider(type,*name==',' ? name+1 : name))) return NULL;
    *owner=c1_probe_hit; return r;
}
/* C1's own record of (type, name) when the server's winning definition is in a zone C1 holds (no mount), else NULL.
 * The materializer links such assets to C1's record instead of copying the server's (c1_materialize.c). */
const void *C1PoolRecord(unsigned type,const char *name)
{
    archive_t *owner=NULL; const uint8_t *r=c1_pool_record(type,name,&owner);
    if(r && type<44) ++c1_pool_builds[type];
    return r;
}
/* Owned copies of C1's rawfile and string-table records (a C1-backed zone: C1 unloads it at the next map change,
 * so nothing may keep pointing into its pools). One allocation each: the payload or the cells, then the strings. */
static int copy_c1_rawfile(const C1RawFile *from,C1RawFile *to,uint8_t **storage)
{
    size_t bytes,name; uint8_t *copy;
    if(!from || !from->name || !from->buffer || from->len<0 || from->compressedLen<0) return 0;
    bytes=from->compressedLen ? (size_t)from->compressedLen : (size_t)from->len+1;
    name=strlen(from->name)+1;
    if(!(copy=malloc(bytes+name))) return 0;
    memcpy(copy,from->buffer,bytes); memcpy(copy+bytes,from->name,name);
    *to=*from; to->buffer=(const char *)copy; to->name=(const char *)copy+bytes; *storage=copy;
    return 1;
}
static int copy_c1_table(const C1StringTable *from,C1StringTable *to)
{
    size_t count,i,bytes,at; char *text; void *block;
    if(!from || from->columnCount<0 || from->rowCount<0) return 0;
    count=(size_t)from->columnCount*(size_t)from->rowCount;
    if(count && !from->values) return 0;
    bytes=from->name ? strlen(from->name)+1 : 1;
    for(i=0;i<count;++i) bytes+=from->values[i].string ? strlen(from->values[i].string)+1 : 0;
    if(!(block=malloc(count*sizeof(*from->values)+bytes))) return 0;
    *to=*from; to->values=block; text=(char *)block+count*sizeof(*from->values); at=0;
    strcpy(text,from->name ? from->name : ""); to->name=text; at=strlen(text)+1;
    for(i=0;i<count;++i) {
        to->values[i].hash=from->values[i].hash; to->values[i].string=NULL;
        if(from->values[i].string) { strcpy(text+at,from->values[i].string); to->values[i].string=text+at; at+=strlen(text+at)+1; }
    }
    return 1;
}
static uint32_t world_u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static int weapon_string(const archive_t *archive,const asset_t *asset,size_t *pos,uint32_t ref,const char **out)
{
    const uint8_t *p,*end; size_t available,offset;
    if (!ref) { *out=NULL; return 1; }
    if (ref==UINT32_MAX) {
        if (*pos>asset->size) return 0;
        p=asset->data+*pos; available=asset->size-*pos;
    } else {
        if (((ref-1)>>28)!=3) return 0;
        offset=(ref-1)&0xfffffff;
        if (offset>=archive->blocks.written[3]) return 0;
        p=archive->blocks.data[3]+offset; available=archive->blocks.written[3]-offset;
    }
    end=memchr(p,0,available); if (!end) return 0;
    *out=(const char *)p;
    if (ref==UINT32_MAX) *pos+=(size_t)(end-p)+1;
    return 1;
}
static unsigned xanim_u16(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
static int xanim_take(const asset_t *asset,size_t *pos,size_t count,size_t stride,const uint8_t **out)
{
    if(*pos>asset->size || count>(asset->size-*pos)/stride) return 0;
    *out=asset->data+*pos; *pos+=count*stride; return 1;
}
/* The inline stream parse of one XAnimParts (C1 0x4b7a50 order), views borrowing asset. */
static int parse_xanim(archive_t *archive,mounted_xanim_t *io)
{
    static const unsigned offsets[]={4,6,8,32,10,12},widths[]={1,2,4,2,1,4};
    mounted_xanim_t result=*io; const uint8_t *h,*delta;
    size_t pos=88; unsigned i;
    if(result.asset.size<pos) return -1;
    h=result.asset.data;
    if(!weapon_string(archive,&result.asset,&pos,world_u32(h),&result.name)) return -1;
    result.bone_count=h[26]; result.notify_count=h[27]; result.index_width=xanim_u16(h+14)<256 ? 1 : 2;
    if(world_u32(h+48) && !xanim_take(&result.asset,&pos,result.bone_count,2,&result.bone_indices)) return -1;
    if(world_u32(h+80) && !xanim_take(&result.asset,&pos,result.notify_count,8,&result.notifies)) return -1;
    if(world_u32(h+84)) {
        if(!xanim_take(&result.asset,&pos,1,12,&delta)) return -1;
        for(i=0;i<3;++i) if(world_u32(delta+4*i)) {
            xanim_track_t *track=result.tracks+i;
            if(!xanim_take(&result.asset,&pos,1,4,&track->header)) return -1;
            track->count=xanim_u16(track->header);
            track->frame_stride=i==0 ? (track->header[2] ? 3 : 6) : i==1 ? 4 : 8;
            if(!track->count) {
                if(!xanim_take(&result.asset,&pos,1,i==0 ? 12 : i==1 ? 4 : 8,&track->frames)) return -1;
            } else {
                if(!xanim_take(&result.asset,&pos,1,i==0 ? 28 : 4,&track->metadata) ||
                   !xanim_take(&result.asset,&pos,track->count+1,result.index_width,&track->indices)) return -1;
                if(world_u32(track->metadata+(i==0 ? 24 : 0)) &&
                   !xanim_take(&result.asset,&pos,track->count+1,track->frame_stride,&track->frames)) return -1;
            }
        }
    }
    for(i=0;i<6;++i) {
        result.stream_counts[i]=i==3 ? world_u32(h+offsets[i]) : xanim_u16(h+offsets[i]);
        if(world_u32(h+52+4*i) && !xanim_take(&result.asset,&pos,result.stream_counts[i],widths[i],result.streams+i)) return -1;
    }
    result.index_count=world_u32(h+36);
    if(world_u32(h+76) && !xanim_take(&result.asset,&pos,result.index_count,result.index_width,&result.indices)) return -1;
    if(pos!=result.asset.size) return -1;
    *io=result; return 1;
}
/* Streams written from C1 records (c1Readers), one per definition (name and serialized source). They are
 * kept for the process: the handle and native caches key on the stream's address, so it must never be reused. */
typedef struct c1_xanim_s { char *name; const uint8_t *definition; c1_xanim_stream_t stream;
    const struct archive_s *pool; /* a C1-backed zone's: written from C1's own record, freed with that archive */
    struct c1_xanim_s *next; } c1_xanim_t;
static c1_xanim_t *c1_xanims;
static const c1_xanim_stream_t *c1_xanim_stream(const char *name,const uint8_t *definition)
{
    c1_xanim_t *c; const uint8_t *record;
    for(c=c1_xanims;c;c=c->next) if(c->definition==definition && asset_name_equal(c->name,name)) return &c->stream;
    if(!(record=C1Record(2,name)) || !(c=calloc(1,sizeof(*c)))) return NULL;
    if(!SerializeC1XAnim(record,record_string,NULL,&c->stream) || !(c->name=malloc(strlen(name)+1))) {
        FreeC1XAnimStream(&c->stream); free(c); return NULL;
    }
    strcpy(c->name,name); c->definition=definition; c->next=c1_xanims; c1_xanims=c;
    return &c->stream;
}
static const char *c1_script_text(void *context,unsigned id)
{
    const c1_zone_host_t *host=C1ZoneHost(); (void)context;
    return host && host->script_string ? host->script_string(id) : NULL;
}
/* The xanim definition a lookup resolves to. In a zone C1 holds (c1Readers 1): the stream written from C1's own
 * XAnimParts record (its script strings are C1's), owned by that C1-backed archive, and no mount; *pool is set and
 * def is the stream. Otherwise the server's definition. */
static archive_t *xanim_definition(const char *name,asset_t *def,const c1_xanim_stream_t **pool)
{
    archive_t *owner=NULL; const uint8_t *record; c1_xanim_t *c;
    *pool=NULL;
    if(c1_readers==1 && (record=c1_pool_record(2,name,&owner))) {
        for(c=c1_xanims;c;c=c->next) if(c->pool==owner && c->definition==record && asset_name_equal(c->name,name)) break;
        if(!c && (c=calloc(1,sizeof(*c)))) {
            if(SerializeC1XAnim(record,c1_script_text,NULL,&c->stream) && (c->name=malloc(strlen(name)+1))) {
                strcpy(c->name,name); c->definition=record; c->pool=owner; c->next=c1_xanims; c1_xanims=c;
                ++c1_pool_builds[2];
            } else { FreeC1XAnimStream(&c->stream); free(c); c=NULL; }
        }
        if(c) { *pool=&c->stream; *def=(asset_t){2,c->stream.data,c->stream.size}; return owner; }
    }
    return find_dependency_definition(2,name,def);
}
static int same_bytes(const uint8_t *a,const uint8_t *b,size_t n) { return (!a && !b) || (a && b && !memcmp(a,b,n)); }
static int xanim_string_equal(const mounted_xanim_t *a,const uint8_t *ia,const mounted_xanim_t *b,const uint8_t *ib)
{
    unsigned x=xanim_u16(ia),y=xanim_u16(ib);
    const char *ta=x<a->script_string_count ? a->script_strings[x].text : NULL,*tb=y<b->script_string_count ? b->script_strings[y].text : NULL;
    return ta && tb && !strcmp(ta,tb);
}
/* 0 when both parses describe the same animation, else the first differing part. */
static int compare_xanims(const mounted_xanim_t *a,const mounted_xanim_t *b)
{
    static const unsigned widths[]={1,2,4,2,1,4};
    unsigned i,j;
    if(!a->name || !b->name || strcmp(a->name,b->name)) return 1;
    if(a->bone_count!=b->bone_count || a->notify_count!=b->notify_count || a->index_width!=b->index_width ||
       a->index_count!=b->index_count || memcmp(a->stream_counts,b->stream_counts,sizeof(a->stream_counts))) return 2;
    if(memcmp(a->asset.data+4,b->asset.data+4,44)) return 3; /* the scalar header fields */
    for(i=0;i<a->bone_count;++i) if(!xanim_string_equal(a,a->bone_indices+2*i,b,b->bone_indices+2*i)) return 4;
    for(i=0;i<a->notify_count;++i)
        if(!xanim_string_equal(a,a->notifies+8*i,b,b->notifies+8*i) || memcmp(a->notifies+8*i+2,b->notifies+8*i+2,6)) return 5;
    for(i=0;i<6;++i) if(!same_bytes(a->streams[i],b->streams[i],(size_t)a->stream_counts[i]*widths[i])) return 10+i;
    if(!same_bytes(a->indices,b->indices,(size_t)a->index_count*a->index_width)) return 6;
    for(i=0;i<3;++i) {
        const xanim_track_t *x=a->tracks+i,*y=b->tracks+i;
        if(!x->header!=!y->header) return 20+i;
        if(!x->header) continue;
        if(x->count!=y->count || x->frame_stride!=y->frame_stride || memcmp(x->header,y->header,4)) return 30+i;
        if(!x->count) { if(!same_bytes(x->frames,y->frames,i==0 ? 12 : i==1 ? 4 : 8)) return 40+i; continue; }
        if(memcmp(x->metadata,y->metadata,i==0 ? 24 : 0)) return 50+i;
        if(!same_bytes(x->indices,y->indices,(size_t)(x->count+1)*a->index_width)) return 60+i;
        j=!x->frames==!y->frames && same_bytes(x->frames,y->frames,(size_t)(x->count+1)*x->frame_stride);
        if(!j) return 70+i;
    }
    return 0;
}
int LoadMountedXAnim(const char *name,mounted_xanim_t *out)
{
    mounted_xanim_t result={0}; archive_t *archive; int status; const c1_xanim_stream_t *pool;
    if(!name || !*name || !out) return -1;
    archive=xanim_definition(name,&result.asset,&pool); if(!archive) return 0;
    if(pool) {
        if((status=parse_xanim(archive,&result))!=1) return status;
        result.script_strings=pool->strings; result.script_string_count=pool->string_count;
        *out=result; return 1;
    }
    if((status=parse_xanim(archive,&result))!=1) return status;
    if(!archive->script_strings && !ReadScriptStrings(&archive->zone,&archive->script_strings)) return -1;
    result.script_strings=archive->script_strings; result.script_string_count=archive->zone.string_count;
    if(c1_readers) {
        const c1_xanim_stream_t *stream=c1_xanim_stream(result.name ? result.name : name,result.asset.data);
        mounted_xanim_t c1={0};
        if(stream) {
            c1.asset=(asset_t){2,stream->data,stream->size};
            if(parse_xanim(archive,&c1)==1) {
                c1.script_strings=stream->strings; c1.script_string_count=stream->string_count;
                c1_reader_report(2,result.name,compare_xanims(&result,&c1));
                if(c1_readers==1) result=c1;
            } else c1_reader_report(2,result.name,-2);
        } else c1_reader_report(2,result.name,-1);
    }
    *out=result; return 1;
}
int BindMountedXAnimHandles(const char *name,xanim_string_acquire_t acquire,
    xanim_string_release_t release,void *context,const xanim_handles_t **out)
{
    mounted_xanim_t source; asset_t definition; archive_t *archive;
    xanim_strings_t *cached; const char *names[510]; unsigned total,i,handle; int status;
    if(!out || !acquire || !release) return -1;
    status=LoadMountedXAnim(name,&source); if(status!=1) return status;
    { const c1_xanim_stream_t *pool; archive=xanim_definition(name,&definition,&pool); } if(!archive) return -1;
    for(cached=archive->xanim_strings;cached;cached=cached->next)
        if(cached->definition==source.asset.data && cached->acquire==acquire && cached->release==release && cached->context==context) {
            *out=&cached->handles; return 1;
        }
    if(source.bone_count>255 || source.notify_count>255 ||
        (source.bone_count && !source.bone_indices) || (source.notify_count && !source.notifies)) return -1;
    total=source.bone_count+source.notify_count;
    for(i=0;i<total;++i) {
        unsigned index=xanim_u16(i<source.bone_count ? source.bone_indices+2*i : source.notifies+8*(i-source.bone_count));
        if(!source.script_strings || index>=source.script_string_count || !source.script_strings[index].text) return -1;
        names[i]=source.script_strings[index].text;
    }
    cached=calloc(1,sizeof(*cached)); if(!cached) return -1;
    for(i=0;i<total;++i) {
        handle=acquire(context,names[i]);
        if(!handle || handle>UINT16_MAX) {
            if(handle) release(context,handle);
            while(i) { --i; release(context,i<source.bone_count ? cached->handles.bones[i] : cached->handles.notifies[i-source.bone_count]); }
            free(cached); return -1;
        }
        if(i<source.bone_count) cached->handles.bones[i]=(uint16_t)handle;
        else cached->handles.notifies[i-source.bone_count]=(uint16_t)handle;
    }
    cached->handles.bone_count=source.bone_count; cached->handles.notify_count=source.notify_count;
    cached->definition=source.asset.data; cached->acquire=acquire; cached->release=release; cached->context=context;
    cached->next=archive->xanim_strings; archive->xanim_strings=cached;
    *out=&cached->handles; return 1;
}
int LoadMountedNativeXAnim(const char *name,xanim_string_acquire_t acquire,
    xanim_string_release_t release,void *context,const struct XAnimParts **out)
{
    mounted_xanim_t source; asset_t definition; archive_t *archive;
    const xanim_handles_t *handles; xanim_strings_t *cached; int status;
    if(!out || !acquire || !release) return -1;
    status=LoadMountedXAnim(name,&source); if(status!=1) return status;
    status=BindMountedXAnimHandles(name,acquire,release,context,&handles);
    if(status!=1) return status;
    { const c1_xanim_stream_t *pool; archive=xanim_definition(name,&definition,&pool); } if(!archive) return -1;
    for(cached=archive->xanim_strings;cached;cached=cached->next) {
        if(&cached->handles!=handles) continue;
        if(!cached->native.parts && !BuildNativeXAnim(&source,handles,&cached->native)) return -1;
        *out=cached->native.parts; return 1;
    }
    return -1;
}
static int weapon_sound(const archive_t *,const asset_t *,size_t *,int,uint32_t,const char **);
static int vehicle_dependency(const archive_t *archive,const asset_t *vehicle,
    size_t *pos,unsigned type,uint32_t reference,asset_t *out,const char **name)
{
    asset_t source={0};
    archive_t *owner;
    const char *source_name;
    if(!reference) return 1;
    if(reference>=UINT32_MAX-1) {
        size_t i;
        if(*pos>=vehicle->size) return 0;
        for(i=0;i<archive->blocks.asset_alias_count;++i) {
            const asset_t *candidate=&archive->blocks.asset_aliases[i].asset;
            if(candidate->type==type && candidate->data==vehicle->data+*pos) {
                source=*candidate; break;
            }
        }
        if(!source.size || source.size>vehicle->size-*pos) return 0;
        *pos+=source.size;
    } else if(type==28) {
        if(!ResolveWeaponAlias(&archive->blocks,reference,&source)) return 0;
    } else if(type==5) {
        if(!ResolveMaterialAlias(&archive->blocks,reference,&source)) return 0;
    } else return 0;
    source_name=dependency_asset_name(archive,&source);
    if(!source_name || !*source_name) return 0;
    owner=find_dependency_definition(type,source_name[0]==',' ? source_name+1 : source_name,out);
    if(!owner) return 0;
    *name=dependency_asset_name(owner,out);
    return *name && **name;
}
int LoadMountedVehicle(const char *name,mounted_vehicle_t *out)
{
    mounted_vehicle_t result={0};
    archive_t *archive;
    uint32_t reference;
    unsigned i;
    static const unsigned sound_offsets[]={0x1e0,0x1e4,0x1e8,0x1ec,0x1f4,0x1fc,
        0x200,0x204,0x208,0x210,0x218,0x220,0x228,0x230};
    size_t pos=0x314;
    if(!name || !*name || !out) return -1;
    if(c1_readers==1) { /* defined in a zone C1 holds: from C1's VehicleDef record (0x314 bytes), no mount */
        archive_t *owner=NULL; const uint8_t *r=c1_pool_record(41,name,&owner);
        if(r) {
            uint8_t *header=pool_blob(owner,0x314); const uint8_t *p; unsigned k;
            #define VPTR(o) (memcpy(&p,r+(o),sizeof(p)),p)
            /* every name copied: C1 unloads the zone at the next map change */
            #define VSTR(o) archive_text(owner,(const char *)VPTR(o))
            #define VNAME(o) archive_text(owner,VPTR(o) ? *(const char *const *)p : NULL)
            #define VSOUND(o) archive_text(owner,VPTR(o) ? ((const snd_alias_list_t *)p)->aliasName : NULL)
            if(header) {
                static const unsigned pointers[]={0,8,0xac,0xb0,0xb4,0x190,0x194,0x1ac,0x1b0,0x1d0,0x1d4,0x238};
                memcpy(header,r,0x314);
                for(k=0;k<sizeof(pointers)/sizeof(pointers[0]);++k) { uint32_t v=VPTR(pointers[k]) ? UINT32_MAX : 0; memcpy(header+pointers[k],&v,4); }
                for(k=0;k<14;++k) { uint32_t v=VPTR(sound_offsets[k]) ? UINT32_MAX : 0; memcpy(header+sound_offsets[k],&v,4); }
                for(k=0;k<31;++k) { uint32_t v=VPTR(0x23c+4*k) ? UINT32_MAX : 0; memcpy(header+0x23c+4*k,&v,4); }
                result.asset=(asset_t){41,header,0x314};
                result.name=VSTR(0); result.prefix_strings[0]=VSTR(8); result.prefix_strings[1]=VSTR(0xac);
                result.physics_preset_name=VNAME(0xb0); result.physics_preset=(asset_t){0,NULL,0};
                result.physics_profile=VSTR(0xb4); result.string190=VSTR(0x190);
                result.weapon_name=VNAME(0x194); result.weapon=(asset_t){28,NULL,0};
                for(k=0;k<2;++k) result.sounds[k]=VSOUND(0x1ac+4*k);
                for(k=0;k<2;++k) { result.material_names[k]=VNAME(0x1d0+4*k); result.materials[k]=(asset_t){5,NULL,0}; }
                for(k=0;k<14;++k) result.sounds[2+k]=VSOUND(sound_offsets[k]);
                result.string238=VSTR(0x238);
                for(k=0;k<31;++k) result.sounds[16+k]=VSOUND(0x23c+4*k);
                if(result.weapon_name && *result.weapon_name==',') ++result.weapon_name;
                for(k=0;k<2;++k) if(result.material_names[k] && *result.material_names[k]==',') ++result.material_names[k];
                if(result.physics_preset_name && *result.physics_preset_name==',') ++result.physics_preset_name;
                result.dependency_offset=0x314;
                if(result.name && *result.name) { ++c1_pool_builds[41]; *out=result; return 1; }
            }
            #undef VPTR
            #undef VSTR
            #undef VNAME
            #undef VSOUND
        }
    }
    archive=find_dependency_definition(41,name,&result.asset);
    if(!archive) return 0;
    if(result.asset.size<pos) return -1;
    if(!weapon_string(archive,&result.asset,&pos,world_u32(result.asset.data),&result.name) ||
       !weapon_string(archive,&result.asset,&pos,world_u32(result.asset.data+8),&result.prefix_strings[0]) ||
       !weapon_string(archive,&result.asset,&pos,world_u32(result.asset.data+0xac),&result.prefix_strings[1]) ||
       !result.name || !*result.name) return -1;
    reference=world_u32(result.asset.data+0xb0);
    if(reference) {
        asset_t preset={0};
        const char *preset_name;
        archive_t *owner;
        if(reference>=UINT32_MAX-1) {
            size_t i;
            /* Replay retained the exact nested extent. Advance over the
             * original bytes before resolving a newer named definition. */
            if(pos>=result.asset.size) return -1;
            for(i=0;i<archive->blocks.asset_alias_count;++i) {
                const asset_t *candidate=&archive->blocks.asset_aliases[i].asset;
                if(candidate->type==0 && candidate->data==result.asset.data+pos) {
                    preset=*candidate; break;
                }
            }
            if(!preset.size || preset.size>result.asset.size-pos) return -1;
            pos+=preset.size;
        } else if(!ResolvePhysicsAlias(&archive->blocks,0,reference,&preset)) return -1;
        preset_name=dependency_asset_name(archive,&preset);
        if(!preset_name || !*preset_name) return -1;
        owner=find_dependency_definition(0,preset_name[0]==',' ? preset_name+1 : preset_name,
            &result.physics_preset);
        if(!owner) return -1;
        result.physics_preset_name=dependency_asset_name(owner,&result.physics_preset);
        if(!result.physics_preset_name || !*result.physics_preset_name) return -1;
    }
    if(!weapon_string(archive,&result.asset,&pos,world_u32(result.asset.data+0xb4),
        &result.physics_profile)) return -1;
    if(!weapon_string(archive,&result.asset,&pos,world_u32(result.asset.data+0x190),
        &result.string190) || !vehicle_dependency(archive,&result.asset,&pos,28,
        world_u32(result.asset.data+0x194),&result.weapon,&result.weapon_name)) return -1;
    for(i=0;i<2;++i)
        if(!weapon_sound(archive,&result.asset,&pos,0,world_u32(result.asset.data+0x1ac+4*i),
            &result.sounds[i])) return -1;
    for(i=0;i<2;++i)
        if(!vehicle_dependency(archive,&result.asset,&pos,5,world_u32(result.asset.data+0x1d0+4*i),
            &result.materials[i],&result.material_names[i])) return -1;
    for(i=0;i<14;++i)
        if(!weapon_sound(archive,&result.asset,&pos,0,world_u32(result.asset.data+sound_offsets[i]),
            &result.sounds[2+i])) return -1;
    if(!weapon_string(archive,&result.asset,&pos,world_u32(result.asset.data+0x238),
        &result.string238)) return -1;
    for(i=0;i<31;++i)
        if(!weapon_sound(archive,&result.asset,&pos,0,world_u32(result.asset.data+0x23c+4*i),
            &result.sounds[16+i])) return -1;
    if(pos!=result.asset.size) return -1;
    result.dependency_offset=pos;
    *out=result;
    return 1;
}
static int serial_weapon(const char *name,mounted_weapon_t *out)
{
    mounted_weapon_t result={0}; archive_t *archive; unsigned i;
    size_t pos=0xbfc;
    if (!name || !*name || !out) return -1;
    archive=find_dependency_definition(28,name,&result.asset); if (!archive) return 0;
    if (result.asset.size<pos) return -1;
    for (i=0;i<4;++i)
        if (!weapon_string(archive,&result.asset,&pos,world_u32(result.asset.data+4*i),result.strings+i)) return -1;
    if (!result.strings[0] || !*result.strings[0]) return -1;
    result.dependency_offset=pos;
    *out=result; return 1;
}
static int serial_weapon_hide_tags(const char *name,weapon_hide_tags_t *out)
{
    mounted_weapon_t weapon; weapon_hide_tags_t result={0};
    archive_t *archive; asset_t definition; unsigned i; int status;
    if(!out) return -1;
    status=serial_weapon(name,&weapon); if(status!=1) return status;
    archive=find_dependency_definition(28,name,&definition); if(!archive) return -1;
    /* C1 hide-tag consumer 0x422847/0x422852 selects def+3c. Serialized
     * script-string IDs belong to this zone, not the engine's SL table. */
    for(i=0;i<32;++i) {
        const uint8_t *p=weapon.asset.data+0x3c+2*i;
        unsigned index=(unsigned)p[0]|(unsigned)p[1]<<8;
        if(!index) continue;
        if(!archive->script_strings && !ReadScriptStrings(&archive->zone,&archive->script_strings)) return -1;
        if(index>=archive->zone.string_count || !archive->script_strings[index].text ||
           !*archive->script_strings[index].text) return -1;
        result.names[i]=archive->script_strings[index].text;
    }
    *out=result; return 1;
}
static const native_weapon_core_t *FindNativeWeaponQueryCore(const struct WeaponDef *definition)
{
    archive_t *archive; native_weapon_cache_t *cached; unsigned generation;
    if(!definition) return NULL;
    for(generation=0;generation<2;++generation)
        for(archive=generation ? retired_archives : archives;archive;archive=archive->next) {
            if(archive->null_weapon && definition==&archive->null_weapon->definition)
                return archive->null_weapon;
            for(cached=archive->native_weapons;cached;cached=cached->next)
                if(definition==&cached->core.definition) return &cached->core;
        }
    return ComposedWeaponCore(definition);
}
int GetNativeWeaponClass(const struct WeaponDef *definition,unsigned *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out || core->source_weapon_class>=12)return 0;
    *out=core->source_weapon_class;
    return 1;
}
int GetNativeBombWeapon(const struct WeaponDef *definition,bomb_weapon_t *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out)return 0;
    *out=core->bomb;
    return 1;
}
int GetNativeWeaponMissileStickiness(const struct WeaponDef *definition,
    missile_stickiness_t *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->missile_stickiness;
    return 1;
}
int GetNativeWeaponBounce(const struct WeaponDef *definition,unsigned surface,float out[2])
{
    const native_weapon_core_t *core;
    if(!out || surface>=WEAPON_BOUNCE_SURFACES) return 0;
    core=FindNativeWeaponQueryCore(definition);
    if(!core) return 0;
    out[0]=core->bounce.parallel[surface];
    out[1]=core->bounce.perpendicular[surface];
    return 1;
}
int GetNativeWeaponRolling(const struct WeaponDef *definition,int *enabled)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !enabled) return 0;
    *enabled=core->bounce.rolling;
    return 1;
}
int LoadMountedWeaponAttachment(const char *name,mounted_weapon_attachment_t *out)
{
    mounted_weapon_attachment_t result={0}; archive_t *archive;
    const uint8_t *h; size_t pos=12; uint32_t i;
    if(!name || !*name || !out) return -1;
    archive=NULL;
    if(c1_readers==1) { /* defined in a zone C1 holds: C1's WeaponAttachment written as the stream its loader reads */
        archive_t *owner=NULL; const uint8_t *r=c1_pool_record(43,name,&owner);
        if(r) {
            const char *n; const uint8_t *entries; uint32_t count=world_u32(r+4),k; size_t size=12,at;
            uint8_t *data; uint32_t m=UINT32_MAX,zero=0;
            memcpy(&n,r,sizeof(n)); memcpy(&entries,r+8,sizeof(entries));
            if(n && count<=4096 && (!count || entries)) {
                size+=strlen(n)+1;
                if(entries) {
                    size+=12*(size_t)count;
                    for(k=0;k<count;++k) {
                        uint32_t kind=world_u32(entries+12*k+4); const char *t;
                        if(!(kind<16 || kind>INT32_MAX || kind==39)) continue;
                        memcpy(&t,entries+12*k+8,sizeof(t)); if(t) size+=strlen(t)+1;
                    }
                }
                if((data=pool_blob(owner,size))) {
                    memcpy(data,&m,4); memcpy(data+4,&count,4); memcpy(data+8,entries ? &m : &zero,4); at=12;
                    memcpy(data+at,n,strlen(n)+1); at+=strlen(n)+1;
                    if(entries) {
                        size_t base=at; memcpy(data+at,entries,12*(size_t)count); at+=12*(size_t)count;
                        for(k=0;k<count;++k) {
                            uint32_t kind=world_u32(entries+12*k+4); const char *t;
                            if(!(kind<16 || kind>INT32_MAX || kind==39)) continue;
                            memcpy(&t,entries+12*k+8,sizeof(t));
                            memcpy(data+base+12*k+8,t ? &m : &zero,4);
                            if(t) { memcpy(data+at,t,strlen(t)+1); at+=strlen(t)+1; }
                        }
                    }
                    result.asset=(asset_t){43,data,size}; archive=owner; ++c1_pool_builds[43];
                }
            }
        }
    }
    if(!archive) { archive=find_dependency_definition(43,name,&result.asset); if(!archive) return 0; }
    if(result.asset.size<pos) return -1;
    h=result.asset.data;
    if(!weapon_string(archive,&result.asset,&pos,world_u32(h),&result.name)) return -1;
    if(!result.name || !*result.name) return -1;
    result.count=world_u32(h+4);
    /* C1 0x4c3e59 reads the entry array only when the pointer field is set;
     * 0x4c3d00 then walks exactly count twelve-byte entries and resolves the
     * value of every kind below 16, plus kind 39, as an XString. */
    if(!world_u32(h+8)) {
        if(result.count || pos!=result.asset.size) return -1;
        *out=result; return 1;
    }
    if(result.count>INT32_MAX || pos>result.asset.size ||
       (result.asset.size-pos)/12<result.count) return -1;
    result.entries=result.asset.data+pos; pos+=12*(size_t)result.count;
    if(result.count && !(result.texts=calloc(result.count,sizeof(*result.texts)))) return -1;
    for(i=0;i<result.count;++i) {
        uint32_t type=world_u32(result.entries+12*i+4);
        if(!(type<16 || type>INT32_MAX || type==39)) continue;
        if(!weapon_string(archive,&result.asset,&pos,world_u32(result.entries+12*i+8),result.texts+i)) {
            free((void *)result.texts); return -1;
        }
    }
    if(pos!=result.asset.size) { free((void *)result.texts); return -1; }
    *out=result; return 1;
}
void FreeMountedWeaponAttachment(mounted_weapon_attachment_t *attachment)
{
    if(!attachment) return;
    free((void *)attachment->texts);
    memset(attachment,0,sizeof(*attachment));
}
int GetNativeWeaponDetonateTrigger(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->detonate_trigger; return 1;
}
int GetNativeWeaponRequiresGround(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->requires_ground; return 1;
}
int GetNativeWeaponHideTags(const struct WeaponDef *definition,const weapon_hide_tag_handles_t **out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=&core->hide_tags; return 1;
}
int GetNativeWeaponSilenced(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->silenced!=0; return 1;
}
int GetNativeWeaponScriptMods(const struct WeaponDef *definition,const char **out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->script_mods ? core->script_mods : ""; return 1;
}
int GetNativeWeaponSpawnedGrenade(const struct WeaponDef *definition,const char **out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->spawned_grenade_name ? core->spawned_grenade_name : ""; return 1;
}
int GetNativeWeaponScriptFlag(const struct WeaponDef *definition,weapon_script_flag_t flag,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    switch(flag) {
    case WEAPON_SCRIPT_FLAG_THERMAL_SCOPE: *out=core->thermal_scope; return 1;
    case WEAPON_SCRIPT_FLAG_HPBT: *out=core->hpbt; return 1;
    case WEAPON_SCRIPT_FLAG_GPS: *out=core->gps; return 1;
    case WEAPON_SCRIPT_FLAG_ID_LOCK: *out=core->id_lock; return 1;
    }
    return 0;
}
int GetNativeWeaponHoldPin(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->hold_pin; return 1;
}
int GetNativeWeaponZoomFov(const struct WeaponDef *definition,float out[3])
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    memcpy(out,core->zoom_fov,sizeof core->zoom_fov); return 1;
}
int GetNativeWeaponOffhandCancel(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->offhand_cancel; return 1;
}
int GetNativeWeaponUnlimitedAmmo(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->unlimited_ammo; return 1;
}
int GetNativeWeaponNoAmmoPickup(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->no_ammo_pickup; return 1;
}
int GetNativeWeaponMeleeIgnoresKnifeRow(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->melee_ignores_knife_row; return 1;
}
int GetNativeWeaponSpinUp(const struct WeaponDef *definition,int *up_ms,int *down_ms)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !up_ms || !down_ms) return 0;
    *up_ms=core->timings.spin_up;
    *down_ms=core->timings.spin_down; return 1;
}
int GetNativeWeaponRechamberDualWield(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->timings.rechamber_variant; return 1;
}
int GetNativeWeaponDualWield(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->dual_wield; return 1;
}
int GetNativeWeaponRiotShield(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->riot_shield; return 1;
}
int GetNativeWeaponBulletExplosion(const struct WeaponDef *definition,float *damage_scale,
    float *radius_scale)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !core->bullet_explosive) return 0;
    if(damage_scale) *damage_scale=core->bullet_expl_damage_scale;
    if(radius_scale) *radius_scale=core->bullet_expl_radius_scale;
    return 1;
}
int GetNativeWeaponAdsReload(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->ads_reload; return 1;
}
int GetNativeWeaponReloadData(const struct WeaponDef *definition,int *dual_mag,
    weapon_timing_extensions_t *timings)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !dual_mag || !timings) return 0;
    *dual_mag=core->dual_mag;
    *timings=core->timings;
    return 1;
}
int GetNativeWeaponAnimationName(const struct WeaponDef *definition,unsigned slot,
    const char **out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out || slot>=144) return 0;
    *out=core->animations.names[slot];
    return 1;
}
int GetNativeWeaponInheritsPerks(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->inherits_perks; return 1;
}
int GetNativeWeaponLockWarningGate(const struct WeaponDef *definition,int *out)
{
    const native_weapon_core_t *core=FindNativeWeaponQueryCore(definition);
    if(!core || !out) return 0;
    *out=core->lock_warning_gate; return 1;
}
static const char *structured_string(void *context,uint32_t reference)
{ return LoadBlockString(&((archive_t *)context)->blocks,reference); }
int LoadMountedStructuredSchema(const char *name,structured_schema_t *out)
{
    asset_t asset; archive_t *archive;
    if(!name || !*name || !out)return -1;
    archive=find_dependency_definition(39,name,&asset);if(!archive)return 0;
    return ReadStructuredSchema(&asset,structured_string,archive,out) ? 1 : -1;
}
/* The weapon definition a lookup resolves to: in a zone C1 holds (c1Readers 1), C1's WeaponDef record and the
 * C1-backed archive its caches go with (no mount); otherwise the server's definition. */
static archive_t *weapon_definition(const char *name,asset_t *def)
{
    if(c1_readers==1) {
        archive_t *owner=NULL; const uint8_t *record=c1_pool_record(28,name,&owner);
        if(record) { *def=(asset_t){28,record,0xbfc}; return owner; }
    }
    return find_dependency_definition(28,name,def);
}
int LoadMountedNativeWeapon(const char *name,
    int (*build)(const char *,native_weapon_core_t *),const struct WeaponDef **out)
{
    asset_t definition; archive_t *archive;
    native_weapon_cache_t *cached;
    if(!name || !*name || !build || !out) return -1;
    archive=weapon_definition(name,&definition); if(!archive) return 0;
    for(cached=archive->native_weapons;cached;cached=cached->next)
        if(cached->definition==definition.data) { *out=&cached->core.definition; return 1; }
    cached=calloc(1,sizeof(*cached)); if(!cached) return -1;
    if(build(name,&cached->core)!=1) { FreeNativeWeaponCore(&cached->core); free(cached); return -1; }
    cached->definition=definition.data; cached->next=archive->native_weapons;
    archive->native_weapons=cached; *out=&cached->core.definition; return 1;
}
int LoadMountedDefaultWeapon(const char *name,
    int (*build)(const char *,native_weapon_core_t *),const struct WeaponDef **out)
{
    if(!name || !*name || !build || !out) return -1;
    if(!asset_name_equal(name,"defaultweapon_mp")) return 0;
    return LoadMountedNativeWeapon(name,build,out);
}
int LoadMountedNullWeapon(const char *name,const struct WeaponDef **out)
{
    mounted_weapon_t source; asset_t definition; archive_t *archive;
    weapon_animations_t animations; weapon_ammo_names_t ammo; weapon_tail_t tail;
    native_weapon_core_t *native; int status;
    if(!name || !*name || !out) return -1;
    if(!asset_name_equal(name,"none")) return 0;
    status=LoadMountedWeapon(name,&source); if(status!=1) return status;
    archive=weapon_definition(name,&definition); if(!archive) return -1;
    if(archive->null_weapon) { *out=&archive->null_weapon->definition; return 1; }
    if(LoadMountedWeaponAnimations(name,&animations)!=1 || LoadMountedWeaponAmmoNames(name,&ammo)!=1 ||
       LoadMountedWeaponTail(name,&tail)!=1) return -1;
    native=calloc(1,sizeof(*native)); if(!native) return -1;
    if(!BuildNativeNullWeapon(&source,&animations,&ammo,&tail,native)) { free(native); return -1; }
    archive->null_weapon=native; *out=&native->definition; return 1;
}
int BindMountedWeaponHideTags(const char *name,xanim_string_acquire_t acquire,
    xanim_string_release_t release,void *context,const weapon_hide_tag_handles_t **out)
{
    weapon_hide_tags_t source; asset_t definition; archive_t *archive;
    weapon_tags_t *cached; unsigned i,handle; int status;
    if(!out || !acquire || !release) return -1;
    /* Validate every archive string before acquiring any engine references. */
    status=LoadMountedWeaponHideTags(name,&source); if(status!=1) return status;
    archive=weapon_definition(name,&definition); if(!archive) return -1;
    for(cached=archive->weapon_tags;cached;cached=cached->next)
        if(cached->definition==definition.data && cached->acquire==acquire &&
           cached->release==release && cached->context==context) {
            *out=&cached->handles; return 1;
        }
    cached=calloc(1,sizeof(*cached)); if(!cached) return -1;
    for(i=0;i<32;++i) {
        if(!source.names[i]) continue;
        handle=acquire(context,source.names[i]);
        if(!handle || handle>UINT16_MAX) {
            if(handle) release(context,handle);
            while(i) { --i; if(cached->handles.tags[i]) release(context,cached->handles.tags[i]); }
            free(cached); return -1;
        }
        cached->handles.tags[i]=(uint16_t)handle;
    }
    cached->definition=definition.data; cached->acquire=acquire;
    cached->release=release; cached->context=context;
    cached->next=archive->weapon_tags; archive->weapon_tags=cached;
    *out=&cached->handles; return 1;
}
static int serial_weapon_model(const char *name,unsigned index,weapon_model_t *out)
{
    mounted_weapon_t weapon; weapon_model_t result={0};
    archive_t *archive; asset_t definition; size_t pos; unsigned i; int status;
    if (!out || index>=11) return -1;
    status=serial_weapon(name,&weapon); if (status!=1) return status;
    archive=find_dependency_definition(28,name,&definition); if (!archive) return -1;
    pos=weapon.dependency_offset;
    for (i=0;i<=index;++i) {
        uint32_t ref=world_u32(weapon.asset.data+0x10+4*i);
        asset_t model={0};
        if (ref>=UINT32_MAX-1) {
            size_t extent;
            if (pos>weapon.asset.size) return -1;
            model=(asset_t){4,weapon.asset.data+pos,weapon.asset.size-pos};
            if (!ReadXModelExtent(&model,&extent)) return -1;
            model.size=extent; pos+=extent;
        } else if (i==index) {
            if (!ref) return 0;
            if (!ResolveModelAlias(&archive->blocks,ref,&model)) return -1;
        }
        if (i!=index) continue;
        result.name=dependency_asset_name(archive,&model);
        if (!result.name || !*result.name) return -1;
        archive=find_dependency_definition(4,result.name[0]==',' ? result.name+1 : result.name,&result.asset);
        if (!archive) return -1;
        result.name=dependency_asset_name(archive,&result.asset);
        if (!result.name || !*result.name) return -1;
        *out=result; return 1;
    }
    return -1;
}
static int serial_weapon_animations(const char *name,weapon_animations_t *out)
{
    mounted_weapon_t weapon; weapon_animations_t result={0};
    archive_t *archive; asset_t definition; size_t pos; unsigned i; int status;
    if (!out) return -1;
    status=serial_weapon(name,&weapon); if (status!=1) return status;
    archive=find_dependency_definition(28,name,&definition); if (!archive) return -1;
    pos=weapon.dependency_offset;
    for (i=0;i<11;++i) if (world_u32(weapon.asset.data+0x10+4*i)>=UINT32_MAX-1) {
        asset_t model; size_t extent;
        if (pos>weapon.asset.size) return -1;
        model=(asset_t){4,weapon.asset.data+pos,weapon.asset.size-pos};
        if (!ReadXModelExtent(&model,&extent)) return -1;
        pos+=extent;
    }
    for (i=0;i<144;++i)
        if (!weapon_string(archive,&weapon.asset,&pos,world_u32(weapon.asset.data+0x7c+4*i),result.names+i)) return -1;
    result.dependency_offset=pos; *out=result; return 1;
}
static int serial_weapon_effects(const char *name,weapon_effects_t *out)
{
    mounted_weapon_t weapon; weapon_animations_t animations;
    weapon_effects_t result={0}; archive_t *archive,*owner;
    asset_t definition; size_t pos; unsigned i; int status;
    if (!out) return -1;
    status=serial_weapon(name,&weapon); if (status!=1) return status;
    status=serial_weapon_animations(name,&animations); if (status!=1) return status;
    archive=find_dependency_definition(28,name,&definition); if (!archive) return -1;
    pos=animations.dependency_offset;
    for (i=0;i<2;++i) {
        uint32_t ref=world_u32(weapon.asset.data+0x35c+4*i);
        asset_t effect={0}; const char *effect_name;
        if (!ref) continue;
        if (ref>=UINT32_MAX-1) {
            size_t j;
            if (pos>=weapon.asset.size) return -1;
            /* Complete replay records exact source extents, including nested
             * dependencies. Advance over the original before named override. */
            for (j=0;j<archive->blocks.asset_alias_count;++j) {
                const asset_t *candidate=&archive->blocks.asset_aliases[j].asset;
                if (candidate->type==30 && candidate->data==weapon.asset.data+pos) { effect=*candidate; break; }
            }
            if (!effect.size || effect.size>weapon.asset.size-pos) return -1;
            pos+=effect.size;
        } else if (!ResolveEffectAlias(&archive->blocks,ref,&effect)) return -1;
        effect_name=dependency_asset_name(archive,&effect);
        if (!effect_name || !*effect_name) return -1;
        owner=find_dependency_definition(30,effect_name[0]==',' ? effect_name+1 : effect_name,result.assets+i);
        if (!owner) return -1;
        result.names[i]=dependency_asset_name(owner,result.assets+i);
        if (!result.names[i] || !*result.names[i]) return -1;
    }
    result.dependency_offset=pos; *out=result; return 1;
}
static int weapon_sound(const archive_t *archive,const asset_t *asset,size_t *pos,
    int pooled,uint32_t ref,const char **out)
{
    uint32_t string;
    if(!ref) { *out=NULL; return 1; }
    if(ref==UINT32_MAX) {
        /* Serialized bytes have no padding; replayed block-3 wrappers align
         * to four bytes, including those following an odd-length name. */
        if(pooled) *pos=(*pos+3)&~(size_t)3;
        if(*pos>asset->size || asset->size-*pos<4) return 0;
        string=world_u32(asset->data+*pos); *pos+=4;
        return weapon_string(archive,asset,pos,string,out);
    } else {
        size_t offset=(ref-1)&0xfffffff,pooled_pos;
        asset_t pool={0,archive->blocks.data[3],archive->blocks.written[3]};
        if(((ref-1)>>28)!=3 || offset>pool.size || pool.size-offset<4) return 0;
        string=world_u32(pool.data+offset); pooled_pos=offset+4;
        return weapon_string(archive,&pool,&pooled_pos,string,out);
    }
}
static int serial_weapon_sounds(const char *name,weapon_sounds_t *out)
{
    mounted_weapon_t weapon; weapon_effects_t effects;
    weapon_sounds_t result={0}; archive_t *archive; asset_t definition;
    size_t pos; unsigned i; int status;
    if (!out) return -1;
    status=serial_weapon(name,&weapon); if(status!=1) return status;
    status=serial_weapon_effects(name,&effects); if(status!=1) return status;
    archive=find_dependency_definition(28,name,&definition); if(!archive) return -1;
    pos=effects.dependency_offset;
    for(i=0;i<53;++i)
        if(!weapon_sound(archive,&weapon.asset,&pos,0,world_u32(weapon.asset.data+0x364+4*i),result.names+i)) return -1;
    for(i=0;i<2;++i) {
        uint32_t ref=world_u32(weapon.asset.data+0x438+4*i);
        asset_t array=weapon.asset; size_t cursor=pos,start; unsigned j; int pooled=ref!=UINT32_MAX;
        if(!ref) continue;
        if(pooled) {
            if(((ref-1)>>28)!=3) return -1;
            array=(asset_t){0,archive->blocks.data[3],archive->blocks.written[3]}; cursor=(ref-1)&0xfffffff;
        }
        if(cursor>array.size || array.size-cursor<31*4) return -1;
        start=cursor; cursor+=31*4;
        for(j=0;j<31;++j)
            if(!weapon_sound(archive,&array,&cursor,pooled,world_u32(array.data+start+4*j),result.arrays[i]+j)) return -1;
        if(!pooled) pos=cursor;
    }
    result.dependency_offset=pos; *out=result; return 1;
}
static int serial_weapon_visuals(const char *name,weapon_visuals_t *out)
{
    static const unsigned dependencies[][3]={{0x440,4,30},{0x450,2,5},{0x52c,6,5},{0x548,1,5},{0x550,1,5}};
    mounted_weapon_t weapon; weapon_sounds_t sounds;
    weapon_visuals_t result={0}; archive_t *archive; asset_t definition;
    size_t pos; unsigned i,j,index=0; int status;
    if(!out) return -1;
    status=serial_weapon(name,&weapon); if(status!=1) return status;
    status=serial_weapon_sounds(name,&sounds); if(status!=1) return status;
    archive=find_dependency_definition(28,name,&definition); if(!archive) return -1;
    pos=sounds.dependency_offset;
    for(i=0;i<sizeof(dependencies)/sizeof(dependencies[0]);++i)
        for(j=0;j<dependencies[i][1];++j) {
            weapon_dependency_t *entry=result.entries+index++;
            archive_t *owner; const char *asset_name;
            uint32_t ref=world_u32(weapon.asset.data+dependencies[i][0]+4*j);
            asset_t asset={0}; size_t k;
            entry->offset=dependencies[i][0]+4*j; entry->kind=WEAPON_DEP_ASSET;
            /* Keep the slot's type even when an attachment is the first
             * definition to supply its asset name. */
            entry->asset.type=dependencies[i][2];
            if(!ref) continue;
            if(ref>=UINT32_MAX-1) {
                if(pos>=weapon.asset.size) return -1;
                for(k=0;k<archive->blocks.asset_alias_count;++k) {
                    const asset_t *candidate=&archive->blocks.asset_aliases[k].asset;
                    if(candidate->type==dependencies[i][2] && candidate->data==weapon.asset.data+pos) { asset=*candidate; break; }
                }
                if(!asset.size || asset.size>weapon.asset.size-pos) return -1;
                pos+=asset.size;
            } else if(!(dependencies[i][2]==30 ? ResolveEffectAlias(&archive->blocks,ref,&asset) :
                ResolveMaterialAlias(&archive->blocks,ref,&asset))) return -1;
            asset_name=dependency_asset_name(archive,&asset); if(!asset_name || !*asset_name) return -1;
            owner=find_dependency_definition(dependencies[i][2],asset_name[0]==',' ? asset_name+1 : asset_name,&entry->asset);
            if(!owner) return -1;
            entry->name=dependency_asset_name(owner,&entry->asset); if(!entry->name || !*entry->name) return -1;
        }
    result.dependency_offset=pos; *out=result; return 1;
}
static int serial_weapon_ammo_names(const char *name,weapon_ammo_names_t *out)
{
    mounted_weapon_t weapon; weapon_visuals_t visuals;
    weapon_ammo_names_t result={0}; archive_t *archive; asset_t definition;
    size_t pos; int status;
    if(!out) return -1;
    status=serial_weapon(name,&weapon); if(status!=1) return status;
    status=serial_weapon_visuals(name,&visuals); if(status!=1) return status;
    archive=find_dependency_definition(28,name,&definition); if(!archive) return -1;
    pos=visuals.dependency_offset;
    if(!weapon_string(archive,&weapon.asset,&pos,world_u32(weapon.asset.data+0x564),&result.ammo) ||
       !weapon_string(archive,&weapon.asset,&pos,world_u32(weapon.asset.data+0x56c),&result.clip) ||
       !weapon_string(archive,&weapon.asset,&pos,world_u32(weapon.asset.data+0x57c),&result.shared_cap)) return -1;
    result.dependency_offset=pos; *out=result; return 1;
}
static int serial_weapon_tail(const char *name,weapon_tail_t *out)
{
    enum { S=WEAPON_DEP_STRING,N=WEAPON_DEP_SOUND,A=WEAPON_DEP_ASSET,P=WEAPON_DEP_POINTS };
    static const unsigned fields[][4]={
        {0x684,4,A,5},{0x758,1,A,1},{0x7b0,1,A,4},
        {0x7b8,2,A,30},{0x7c0,2,N,0},{0x8cc,2,A,30},
        {0x8ec,1,A,30},{0x8f0,1,N,0},{0x9a0,1,S,0},
        {0x9a8,1,P,0xbb0},{0x9a4,1,S,0},{0x9ac,1,P,0xbb2},
        {0x9fc,2,S,0},{0xa20,2,S,0},{0xaa0,2,S,0},
        {0xaa8,1,A,40},{0xac4,1,N,0},{0xac8,1,A,30},
        {0xacc,1,S,0},{0xadc,9,N,0},{0xb00,2,N,0},
        {0xb48,6,N,0},{0xb78,1,S,0},{0xb84,2,S,0},
        {0xb90,3,A,5},{0xbb4,2,P,0xbb0}
    };
    mounted_weapon_t weapon; weapon_ammo_names_t ammo;
    weapon_tail_t result={0}; asset_t definition; archive_t *archive;
    size_t pos; unsigned i,j; int status;
    if(!out) return -1;
    status=serial_weapon(name,&weapon); if(status!=1) return status;
    status=serial_weapon_ammo_names(name,&ammo); if(status!=1) return status;
    archive=find_dependency_definition(28,name,&definition); if(!archive) return -1;
    pos=ammo.dependency_offset;
    for(i=0;i<sizeof(fields)/sizeof(fields[0]);++i) for(j=0;j<fields[i][1];++j) {
        weapon_dependency_t *entry; uint32_t ref;
        if(result.count>=64) return -1;
        entry=result.entries+result.count++; entry->offset=fields[i][0]+4*j; entry->kind=fields[i][2];
        if(entry->kind==A) entry->asset.type=fields[i][3];
        ref=world_u32(weapon.asset.data+entry->offset);
        if(!ref) continue;
        if(entry->kind==S) {
            if(!weapon_string(archive,&weapon.asset,&pos,ref,&entry->name)) return -1;
        } else if(entry->kind==N) {
            if(!weapon_sound(archive,&weapon.asset,&pos,0,ref,&entry->name)) return -1;
        } else if(entry->kind==P) {
            const uint8_t *base=weapon.asset.data; size_t size=weapon.asset.size,at=pos,k;
            entry->point_count=xanim_u16(weapon.asset.data+fields[i][3]+2*j);
            if(ref!=UINT32_MAX) {
                if(((ref-1)>>28)!=3) return -1;
                base=archive->blocks.data[3]; size=archive->blocks.written[3]; at=(ref-1)&0xfffffff;
            }
            if(at>size || entry->point_count>(size-at)/8) return -1;
            entry->points=base+at;
            for(k=0;k<(size_t)entry->point_count*2;++k) {
                uint32_t bits=world_u32(entry->points+4*k); float value; memcpy(&value,&bits,4);
                if(!isfinite(value)) return -1;
            }
            if(ref==UINT32_MAX) pos+=8*entry->point_count;
        } else {
            asset_t original={0}; archive_t *owner; const char *asset_name; size_t k;
            for(k=0;k<archive->blocks.asset_alias_count;++k) {
                const asset_alias_t *candidate=archive->blocks.asset_aliases+k;
                if(candidate->asset.type!=fields[i][3]) continue;
                if(ref>=UINT32_MAX-1 ? (pos<weapon.asset.size && candidate->asset.data==weapon.asset.data+pos) : candidate->reference==ref) {
                    original=candidate->asset; break;
                }
            }
            if(!original.data || !original.size) return -1;
            if(ref>=UINT32_MAX-1) { if(pos>weapon.asset.size || original.size>weapon.asset.size-pos) return -1; pos+=original.size; }
            asset_name=dependency_asset_name(archive,&original); if(!asset_name || !*asset_name) return -1;
            owner=find_dependency_definition(fields[i][3],asset_name[0]==',' ? asset_name+1 : asset_name,&entry->asset);
            if(!owner) return -1;
            entry->name=dependency_asset_name(owner,&entry->asset); if(!entry->name || !*entry->name) return -1;
        }
    }
    if(pos!=weapon.asset.size) return -1;
    result.dependency_offset=pos; *out=result; return 1;
}

/* ---- Weapons from C1 records (c1Readers, stage 2 step 3) --------------------------------------------------
 * The weapon's component views (strings, models, animations, effects, sounds, visuals, ammo names, the tail and the
 * hide tags) are read from the WeaponDef record C1 holds (0xbfc bytes, real pointers) instead of walking the
 * serialized stream. Asset slots name the linked record, whose definition is then found by name exactly as the
 * serialized readers do; the header view handed to the composition and native builders is the definition's header
 * copy (the serialized header words: presence, scalars, zone script-string indices). One build per definition,
 * kept with its archive. */
typedef struct c1_weapon_s {
    const uint8_t *definition;          /* the zone definition (identity) */
    mounted_weapon_t weapon;
    weapon_hide_tags_t tags;
    char *tag_storage;
    weapon_model_t models[11];
    int model_status[11];
    weapon_animations_t animations;
    weapon_effects_t effects;
    weapon_sounds_t sounds;
    weapon_visuals_t visuals;
    weapon_ammo_names_t ammo;
    weapon_tail_t tail;
    /* built from C1's own pool record (a C1-backed zone): every name and array copied, the header synthesized */
    const struct archive_s *pool;
    uint8_t *header;
    void **owned; size_t owned_count,owned_capacity;
    struct c1_weapon_s *next;
} c1_weapon_t;
static c1_weapon_t *c1_weapons;
static int c1_weapon_building;
static const uint8_t *rec_ptr(const uint8_t *record,unsigned offset)
{ const uint8_t *v; memcpy(&v,record+offset,sizeof(v)); return v; }
static unsigned rec_name_offset(unsigned type) { return type==10 ? 32 : type==27 ? 4 : 0; }
/* An asset slot: the linked record's name, then the winning definition by name (as the serialized readers).
 * 1 set, 0 null slot, -1 a slot the zone fills that has no linked record or definition. */
static int rec_asset(const uint8_t *record,const uint8_t *header,unsigned offset,unsigned type,
    const char **name,asset_t *asset)
{
    const uint8_t *p=rec_ptr(record,offset); const char *n; archive_t *owner;
    *name=NULL; memset(asset,0,sizeof(*asset)); asset->type=type;
    if(!p) return world_u32(header+offset) ? -1 : 0;
    n=(const char *)rec_ptr(p,rec_name_offset(type));
    if(!n || !*n) return -1;
    if(*n==',') ++n;
    if(c1_readers==1) { /* defined in a zone C1 holds: by name (its engine loader takes C1's record) */
        archive_t *pool=NULL;
        if(c1_pool_record(type,n,&pool)) { *name=n; asset->type=type; return 1; }
    }
    if(!(owner=find_dependency_definition(type,n,asset))) return -1;
    *name=dependency_asset_name(owner,asset);
    return *name && **name ? 1 : -1;
}
/* A sound slot: the snd_alias_list_t's name (a name-only list when the server mounts no such alias). */
static const char *rec_sound(const uint8_t *list) { return list ? (const char *)rec_ptr(list,0) : NULL; }
static void free_c1_weapon(c1_weapon_t *w)
{
    size_t i;
    if(!w) return;
    for(i=0;i<w->owned_count;++i) free(w->owned[i]);
    free(w->owned); free(w->header); free(w->tag_storage); free(w);
}
static void *pool_own(c1_weapon_t *w,void *p)
{
    if(!p) return NULL;
    if(w->owned_count==w->owned_capacity) {
        size_t n=w->owned_capacity ? 2*w->owned_capacity : 64; void **g=realloc(w->owned,n*sizeof(*g));
        if(!g) { free(p); return NULL; }
        w->owned=g; w->owned_capacity=n;
    }
    return w->owned[w->owned_count++]=p;
}
/* A copy of a C1 string, or NULL for none; *ok cleared when the copy fails. */
static const char *pool_text(c1_weapon_t *w,const char *s,int *ok)
{
    char *c; size_t n;
    if(!s) return NULL;
    n=strlen(s)+1;
    if(!(c=pool_own(w,malloc(n)))) { *ok=0; return NULL; }
    memcpy(c,s,n); return c;
}
/* The name of the asset C1's slot points at (every C1 header starts with its name, images at +32). */
static const char *pool_asset_name(c1_weapon_t *w,const uint8_t *record,unsigned offset,unsigned type,int *ok)
{
    const uint8_t *p=rec_ptr(record,offset); const char *n;
    if(!p) return NULL;
    n=(const char *)rec_ptr(p,rec_name_offset(type));
    if(n && *n==',') ++n;
    return pool_text(w,n && *n ? n : NULL,ok);
}
static const char *pool_sound(c1_weapon_t *w,const uint8_t *list,int *ok)
{ return list ? pool_text(w,(const char *)rec_ptr(list,0),ok) : NULL; }
unsigned C1WeaponSlotKind(unsigned offset);
/* c1_weapon_build from C1's own WeaponDef (loader 0x4c3c20, 0xbfc bytes, real pointers). The header view the
 * builders read is synthesized: the record's scalars, -1 in every pointer slot C1 filled (present), 0 where it is
 * null, the script-string halfwords and C1's runtime registrations zeroed (as serialized). Hide tags come from C1's
 * script strings; dependency slots carry names only (their engine loaders resolve them by name). */
static int c1_weapon_build_pool(const char *name,const uint8_t *record,c1_weapon_t *w)
{
    static const unsigned visual_slots[][3]={{0x440,4,30},{0x450,2,5},{0x52c,6,5},{0x548,1,5},{0x550,1,5}};
    enum { S=WEAPON_DEP_STRING,N=WEAPON_DEP_SOUND,A=WEAPON_DEP_ASSET,P=WEAPON_DEP_POINTS };
    static const unsigned tail_fields[][4]={
        {0x684,4,A,5},{0x758,1,A,1},{0x7b0,1,A,4},
        {0x7b8,2,A,30},{0x7c0,2,N,0},{0x8cc,2,A,30},
        {0x8ec,1,A,30},{0x8f0,1,N,0},{0x9a0,1,S,0},
        {0x9a8,1,P,0xbb0},{0x9a4,1,S,0},{0x9ac,1,P,0xbb2},
        {0x9fc,2,S,0},{0xa20,2,S,0},{0xaa0,2,S,0},
        {0xaa8,1,A,40},{0xac4,1,N,0},{0xac8,1,A,30},
        {0xacc,1,S,0},{0xadc,9,N,0},{0xb00,2,N,0},
        {0xb48,6,N,0},{0xb78,1,S,0},{0xb84,2,S,0},
        {0xb90,3,A,5},{0xbb4,2,P,0xbb0}
    };
    static const unsigned model_types[11]={4,4,4,4,4,4,4,4,4,4,4};
    const c1_zone_host_t *host=C1ZoneHost();
    unsigned i,j,index=0,o; int ok=1;
    if(!host || !host->script_string || !(w->header=malloc(0xbfc))) return -1;
    memcpy(w->header,record,0xbfc);
    for(o=0;o+4<=0xbfc;o+=4) if(C1WeaponSlotKind(o)) {
        uint32_t v=rec_ptr(record,o) ? UINT32_MAX : 0; memcpy(w->header+o,&v,4);
    }
    /* the script-string halfwords keep C1's handles: readers test them for presence only (the names come from
     * the views); C1's runtime registrations are zeroed (loaded data is 0 there) */
    memset(w->header+0x568,0,4); memset(w->header+0x570,0,4); memset(w->header+0x580,0,4);
    memset(w->header+0xb7c,0,4); /* the alternate weapon's registered index (C1 writes it at registration) */
    w->weapon.asset=(asset_t){28,w->header,0xbfc};
    w->weapon.dependency_offset=0xbfc;
    for(i=0;i<4;++i) w->weapon.strings[i]=pool_text(w,(const char *)rec_ptr(record,4*i),&ok);
    if(!w->weapon.strings[0] || !*w->weapon.strings[0]) return -1;
    for(i=0;i<32;++i) {
        unsigned id=record[0x3c+2*i]|(unsigned)record[0x3d+2*i]<<8; const char *t;
        if(!id) continue;
        if(!(t=host->script_string(id)) || !*t) return -1;
        w->tags.names[i]=pool_text(w,t,&ok);
    }
    for(i=0;i<11;++i) {
        w->models[i].name=pool_asset_name(w,record,0x10+4*i,model_types[i],&ok);
        w->models[i].asset.type=4;
        w->model_status[i]=w->models[i].name ? 1 : 0;
    }
    for(i=0;i<144;++i) w->animations.names[i]=pool_text(w,(const char *)rec_ptr(record,0x7c+4*i),&ok);
    for(i=0;i<2;++i) { w->effects.names[i]=pool_asset_name(w,record,0x35c+4*i,30,&ok); w->effects.assets[i].type=30; }
    for(i=0;i<53;++i) w->sounds.names[i]=pool_sound(w,rec_ptr(record,0x364+4*i),&ok);
    for(i=0;i<2;++i) {
        const uint8_t *array=rec_ptr(record,0x438+4*i);
        if(array) for(j=0;j<31;++j) w->sounds.arrays[i][j]=pool_sound(w,rec_ptr(array,4*j),&ok);
    }
    for(i=0;i<sizeof(visual_slots)/sizeof(visual_slots[0]);++i)
        for(j=0;j<visual_slots[i][1];++j) {
            weapon_dependency_t *e=w->visuals.entries+index++;
            e->offset=visual_slots[i][0]+4*j; e->kind=A; e->asset.type=visual_slots[i][2];
            e->name=pool_asset_name(w,record,e->offset,visual_slots[i][2],&ok);
        }
    w->ammo.ammo=pool_text(w,(const char *)rec_ptr(record,0x564),&ok);
    w->ammo.clip=pool_text(w,(const char *)rec_ptr(record,0x56c),&ok);
    w->ammo.shared_cap=pool_text(w,(const char *)rec_ptr(record,0x57c),&ok);
    for(i=0;i<sizeof(tail_fields)/sizeof(tail_fields[0]);++i) for(j=0;j<tail_fields[i][1];++j) {
        weapon_dependency_t *e;
        if(w->tail.count>=64) return -1;
        e=w->tail.entries+w->tail.count++; e->offset=tail_fields[i][0]+4*j; e->kind=tail_fields[i][2];
        switch(e->kind) {
        case S: e->name=pool_text(w,(const char *)rec_ptr(record,e->offset),&ok); break;
        case N: e->name=pool_sound(w,rec_ptr(record,e->offset),&ok); break;
        case A: e->asset.type=tail_fields[i][3]; e->name=pool_asset_name(w,record,e->offset,tail_fields[i][3],&ok); break;
        case P: {
            const uint8_t *points=rec_ptr(record,e->offset); unsigned count=xanim_u16(record+tail_fields[i][3]+2*j);
            void *copy;
            if(!points || !count) break;
            if(!(copy=pool_own(w,malloc((size_t)count*8)))) return -1;
            memcpy(copy,points,(size_t)count*8);
            e->points=copy; e->point_count=count;
            break;
        }
        }
    }
    (void)name;
    return ok ? 1 : -1;
}
static int c1_weapon_build(const char *name,c1_weapon_t *w)
{
    static const unsigned visual_slots[][3]={{0x440,4,30},{0x450,2,5},{0x52c,6,5},{0x548,1,5},{0x550,1,5}};
    enum { S=WEAPON_DEP_STRING,N=WEAPON_DEP_SOUND,A=WEAPON_DEP_ASSET,P=WEAPON_DEP_POINTS };
    static const unsigned tail_fields[][4]={
        {0x684,4,A,5},{0x758,1,A,1},{0x7b0,1,A,4},
        {0x7b8,2,A,30},{0x7c0,2,N,0},{0x8cc,2,A,30},
        {0x8ec,1,A,30},{0x8f0,1,N,0},{0x9a0,1,S,0},
        {0x9a8,1,P,0xbb0},{0x9a4,1,S,0},{0x9ac,1,P,0xbb2},
        {0x9fc,2,S,0},{0xa20,2,S,0},{0xaa0,2,S,0},
        {0xaa8,1,A,40},{0xac4,1,N,0},{0xac8,1,A,30},
        {0xacc,1,S,0},{0xadc,9,N,0},{0xb00,2,N,0},
        {0xb48,6,N,0},{0xb78,1,S,0},{0xb84,2,S,0},
        {0xb90,3,A,5},{0xbb4,2,P,0xbb0}
    };
    asset_t definition; archive_t *archive; const c1_header_t *h; const uint8_t *record,*header;
    unsigned i,j,index=0; size_t tag_size=0,pos=0;
    archive=find_dependency_definition(28,name,&definition); if(!archive) return 0;
    if(!(h=C1Header(&archive->blocks,definition.data)) || !(header=OL_BlockAddress(&archive->blocks,h->header)) ||
       h->length!=0xbfc || !(record=C1Record(28,name))) return -1;
    w->definition=definition.data;
    w->weapon.asset=(asset_t){28,header,0xbfc};
    for(i=0;i<4;++i) w->weapon.strings[i]=(const char *)rec_ptr(record,4*i);
    if(!w->weapon.strings[0] || !*w->weapon.strings[0]) return -1;
    w->weapon.dependency_offset=0xbfc;
    /* hide tags: the record's converted script strings, copied (the intern table goes with the records) */
    for(i=0;i<32;++i) { unsigned id=record[0x3c+2*i]|(unsigned)record[0x3d+2*i]<<8; const char *t=C1RecordString(id);
        if(id && (!t || !*t)) return -1;
        if(id) tag_size+=strlen(t)+1; }
    if(tag_size && !(w->tag_storage=malloc(tag_size))) return -1;
    for(i=0;i<32;++i) { unsigned id=record[0x3c+2*i]|(unsigned)record[0x3d+2*i]<<8; const char *t;
        if(!id) continue;
        t=C1RecordString(id); memcpy(w->tag_storage+pos,t,strlen(t)+1);
        w->tags.names[i]=w->tag_storage+pos; pos+=strlen(t)+1; }
    for(i=0;i<11;++i) {
        int status=rec_asset(record,header,0x10+4*i,4,&w->models[i].name,&w->models[i].asset);
        if(status<0) return -1;
        w->model_status[i]=status;
    }
    for(i=0;i<144;++i) w->animations.names[i]=(const char *)rec_ptr(record,0x7c+4*i);
    for(i=0;i<2;++i) if(rec_asset(record,header,0x35c+4*i,30,w->effects.names+i,w->effects.assets+i)<0) return -1;
    for(i=0;i<53;++i) w->sounds.names[i]=rec_sound(rec_ptr(record,0x364+4*i));
    for(i=0;i<2;++i) {
        const uint8_t *array=rec_ptr(record,0x438+4*i);
        if(!array) { if(world_u32(header+0x438+4*i)) return -1; continue; }
        for(j=0;j<31;++j) w->sounds.arrays[i][j]=rec_sound(rec_ptr(array,4*j));
    }
    for(i=0;i<sizeof(visual_slots)/sizeof(visual_slots[0]);++i)
        for(j=0;j<visual_slots[i][1];++j) {
            weapon_dependency_t *e=w->visuals.entries+index++;
            e->offset=visual_slots[i][0]+4*j; e->kind=A;
            if(rec_asset(record,header,e->offset,visual_slots[i][2],&e->name,&e->asset)<0) return -1;
        }
    w->ammo.ammo=(const char *)rec_ptr(record,0x564);
    w->ammo.clip=(const char *)rec_ptr(record,0x56c);
    w->ammo.shared_cap=(const char *)rec_ptr(record,0x57c);
    for(i=0;i<sizeof(tail_fields)/sizeof(tail_fields[0]);++i) for(j=0;j<tail_fields[i][1];++j) {
        weapon_dependency_t *e;
        if(w->tail.count>=64) return -1;
        e=w->tail.entries+w->tail.count++; e->offset=tail_fields[i][0]+4*j; e->kind=tail_fields[i][2];
        if(e->kind==A) e->asset.type=tail_fields[i][3];
        switch(e->kind) {
        case S: e->name=(const char *)rec_ptr(record,e->offset); break;
        case N: e->name=rec_sound(rec_ptr(record,e->offset)); break;
        case A: if(rec_asset(record,header,e->offset,tail_fields[i][3],&e->name,&e->asset)<0) return -1; break;
        case P: {
            const uint8_t *points=rec_ptr(record,e->offset);
            if(!points) { if(world_u32(header+e->offset)) return -1; break; }
            e->points=points; e->point_count=xanim_u16(record+tail_fields[i][3]+2*j);
            break;
        }
        }
    }
    return 1;
}
static int same_text(const char *a,const char *b,int caseless)
{ return (!a && !b) || (a && b && (caseless ? asset_name_equal(a,b) : !strcmp(a,b))); }
static int same_dependency(const weapon_dependency_t *a,const weapon_dependency_t *b)
{
    if(a->offset!=b->offset || a->kind!=b->kind) return 0;
    if(a->kind==WEAPON_DEP_POINTS)
        return a->point_count==b->point_count && (!a->point_count || (a->points && b->points && !memcmp(a->points,b->points,8*a->point_count)));
    if(!same_text(a->name,b->name,a->kind==WEAPON_DEP_SOUND)) return 0;
    return a->kind!=WEAPON_DEP_ASSET || (a->asset.data==b->asset.data && a->asset.size==b->asset.size);
}
static int serial_weapon(const char *,mounted_weapon_t *);
static int serial_weapon_hide_tags(const char *,weapon_hide_tags_t *);
static int serial_weapon_model(const char *,unsigned,weapon_model_t *);
static int serial_weapon_animations(const char *,weapon_animations_t *);
static int serial_weapon_effects(const char *,weapon_effects_t *);
static int serial_weapon_sounds(const char *,weapon_sounds_t *);
static int serial_weapon_visuals(const char *,weapon_visuals_t *);
static int serial_weapon_ammo_names(const char *,weapon_ammo_names_t *);
static int serial_weapon_tail(const char *,weapon_tail_t *);
/* 0 when the serialized readers give the same views, else the first differing component. */
static int c1_weapon_compare(const char *name,const c1_weapon_t *w)
{
    mounted_weapon_t weapon; weapon_hide_tags_t tags; weapon_model_t model;
    weapon_animations_t animations; weapon_effects_t effects; weapon_sounds_t sounds;
    weapon_visuals_t visuals; weapon_ammo_names_t ammo; weapon_tail_t tail; unsigned i,j;
    if(serial_weapon(name,&weapon)!=1 || memcmp(weapon.asset.data,w->weapon.asset.data,0xbfc)) return 1;
    for(i=0;i<4;++i) if(!same_text(weapon.strings[i],w->weapon.strings[i],0)) return 2;
    if(serial_weapon_hide_tags(name,&tags)!=1) return 3;
    for(i=0;i<32;++i) if(!same_text(tags.names[i],w->tags.names[i],0)) return 4;
    for(i=0;i<11;++i) {
        memset(&model,0,sizeof(model));
        if(serial_weapon_model(name,i,&model)!=w->model_status[i]) return 5;
        if(w->model_status[i]==1 && (!same_text(model.name,w->models[i].name,0) || model.asset.data!=w->models[i].asset.data)) return 6;
    }
    if(serial_weapon_animations(name,&animations)!=1) return 7;
    for(i=0;i<144;++i) if(!same_text(animations.names[i],w->animations.names[i],0)) return 8;
    if(serial_weapon_effects(name,&effects)!=1) return 9;
    for(i=0;i<2;++i) if(!same_text(effects.names[i],w->effects.names[i],0) || effects.assets[i].data!=w->effects.assets[i].data) return 10;
    if(serial_weapon_sounds(name,&sounds)!=1) return 11;
    for(i=0;i<53;++i) if(!same_text(sounds.names[i],w->sounds.names[i],1)) return 12;
    for(i=0;i<2;++i) for(j=0;j<31;++j) if(!same_text(sounds.arrays[i][j],w->sounds.arrays[i][j],1)) return 13;
    if(serial_weapon_visuals(name,&visuals)!=1) return 14;
    for(i=0;i<14;++i) if(!same_dependency(visuals.entries+i,w->visuals.entries+i)) return 15;
    if(serial_weapon_ammo_names(name,&ammo)!=1 || !same_text(ammo.ammo,w->ammo.ammo,0) ||
       !same_text(ammo.clip,w->ammo.clip,0) || !same_text(ammo.shared_cap,w->ammo.shared_cap,0)) return 16;
    if(serial_weapon_tail(name,&tail)!=1 || tail.count!=w->tail.count) return 17;
    for(i=0;i<tail.count;++i) if(!same_dependency(tail.entries+i,w->tail.entries+i)) return 1000+tail.entries[i].offset;
    return 0;
}
/* The record build of a weapon (c1Readers), built and (mode 2) compared once per definition; NULL when off,
 * absent or not buildable (the serialized readers then answer). */
static const c1_weapon_t *c1_weapon(const char *name)
{
    asset_t definition; c1_weapon_t *w; int status;
    if(!c1_readers || c1_weapon_building || !name || !*name) return NULL;
    {   /* the definition is in a zone C1 holds: C1's own record, no mount */
        archive_t *owner=NULL; const uint8_t *record=c1_pool_record(28,name,&owner);
        if(record) {
            for(w=c1_weapons;w;w=w->next) if(w->pool==owner && w->definition==record) return w->model_status[0]<0 ? NULL : w;
            if(!(w=calloc(1,sizeof(*w)))) return NULL;
            c1_weapon_building=1;
            status=c1_weapon_build_pool(name,record,w);
            c1_weapon_building=0;
            if(status==1) ++c1_pool_builds[28];
            else {
                c1_reader_report(28,name,-1);
                free_c1_weapon(w); return NULL; /* the server's copy answers (a lookup mounts it) */
            }
            w->definition=record; w->pool=owner; w->next=c1_weapons; c1_weapons=w;
            return w;
        }
    }
    if(!find_dependency_definition(28,name,&definition)) return NULL;
    for(w=c1_weapons;w;w=w->next) if(w->definition==definition.data) return w->model_status[0]<0 ? NULL : w;
    if(!(w=calloc(1,sizeof(*w)))) return NULL;
    c1_weapon_building=1;
    status=c1_weapon_build(name,w);
    if(status==1) c1_reader_report(28,name,c1_weapon_compare(name,w));
    else c1_reader_report(28,name,-1);
    c1_weapon_building=0;
    if(status!=1) { free(w->tag_storage); memset(w,0,sizeof(*w)); w->model_status[0]=-1; }
    w->definition=definition.data; w->next=c1_weapons; c1_weapons=w;
    return status==1 && c1_readers==1 ? w : NULL;
}
int LoadMountedWeapon(const char *name,mounted_weapon_t *out)
{
    const c1_weapon_t *w=c1_weapon(name);
    if(w && out) { *out=w->weapon; return 1; }
    return serial_weapon(name,out);
}
int LoadMountedWeaponHideTags(const char *name,weapon_hide_tags_t *out)
{
    const c1_weapon_t *w=c1_weapon(name);
    if(w && out) { *out=w->tags; return 1; }
    return serial_weapon_hide_tags(name,out);
}
int LoadMountedWeaponModel(const char *name,unsigned index,weapon_model_t *out)
{
    const c1_weapon_t *w=c1_weapon(name);
    if(w && out && index<11) { if(w->model_status[index]==1) *out=w->models[index]; return w->model_status[index]; }
    return serial_weapon_model(name,index,out);
}
#define C1_WEAPON_VIEW(fn,serial,type,field) \
int fn(const char *name,type *out) \
{ \
    const c1_weapon_t *w=c1_weapon(name); \
    if(w && out) { *out=w->field; return 1; } \
    return serial(name,out); \
}
C1_WEAPON_VIEW(LoadMountedWeaponAnimations,serial_weapon_animations,weapon_animations_t,animations)
C1_WEAPON_VIEW(LoadMountedWeaponEffects,serial_weapon_effects,weapon_effects_t,effects)
C1_WEAPON_VIEW(LoadMountedWeaponSounds,serial_weapon_sounds,weapon_sounds_t,sounds)
C1_WEAPON_VIEW(LoadMountedWeaponVisuals,serial_weapon_visuals,weapon_visuals_t,visuals)
C1_WEAPON_VIEW(LoadMountedWeaponAmmoNames,serial_weapon_ammo_names,weapon_ammo_names_t,ammo)
C1_WEAPON_VIEW(LoadMountedWeaponTail,serial_weapon_tail,weapon_tail_t,tail)
#undef C1_WEAPON_VIEW
static int world_string(const archive_t *archive,const asset_t *asset,size_t *pos,uint32_t ref,const char **out)
{
    const uint8_t *p,*end; size_t available,offset,length; char *copy;
    if (!ref) { *out=NULL; return 1; }
    if (ref==UINT32_MAX) {
        if (*pos>asset->size) return 0;
        p=asset->data+*pos; available=asset->size-*pos;
    } else {
        if (((ref-1)>>28)!=3) return 0;
        offset=(ref-1)&0xfffffff;
        if (offset>=archive->blocks.written[3]) return 0;
        p=archive->blocks.data[3]+offset; available=archive->blocks.written[3]-offset;
    }
    end=memchr(p,0,available); if (!end) return 0;
    length=(size_t)(end-p)+1; copy=malloc(length); if (!copy) return 0;
    memcpy(copy,p,length); if (ref==UINT32_MAX) *pos+=length;
    *out=copy; return 1;
}
static void free_common_world(common_world_cache_t *cached)
{
    unsigned i;
    if (cached->world.primaryLights) for (i=0;i<cached->world.primaryLightCount;++i)
        free((void *)cached->world.primaryLights[i].defName);
    free(cached->world.primaryLights); free((void *)cached->world.name); free(cached);
}
/* For readers of records without script strings: C1's own record when the definition is in a zone C1 holds
 * (c1Readers 1, no mount), else the materialized one. */
static const uint8_t *c1_record_any(unsigned type,const char *name)
{
    archive_t *owner=NULL; const uint8_t *r=c1_readers==1 ? c1_pool_record(type,name,&owner) : NULL;
    return r ? r : (const uint8_t *)C1Record(type,name);
}
static int c1_glass_map(const char *name,glass_map_source_t *out);
static int c1_glass_groups(const char *name,glass_groups_t *out);
static int pool_held(unsigned type,const char *name,archive_t **owner)
{ *owner=NULL; return c1_readers==1 && c1_pool_record(type,name,owner)!=NULL; }
int LoadMountedGlassMap(const char *name,glass_map_source_t *out)
{
    archive_t *ga,*fa;
    asset_t game,fx;
    if (!name || !*name || !out) return -1;
    {   /* a zone C1 holds: the pieces copied out of C1's records (C1 unloads the level at the next map change) */
        archive_t *owner; glass_map_source_t c1;
        if (pool_held(18,name,&owner) && c1_glass_map(name,&c1)) {
            uint8_t *copy=NULL;
            if (c1.count && !(copy=pool_blob(owner,64*(size_t)c1.count))) return -1;
            if (c1.count) {
                memcpy(copy,c1.server_pieces,12*(size_t)c1.count); memcpy(copy+12*(size_t)c1.count,c1.initial_pieces,52*(size_t)c1.count);
                c1.server_pieces=(void *)copy; c1.initial_pieces=(void *)(copy+12*(size_t)c1.count);
            }
            *out=c1; return 1;
        }
    }
    ga=find_dependency_definition(18,name,&game);
    if (!ga) return 0;
    fa=find_dependency_definition(20,name,&fx);
    if (!ReadGlassMap(&ga->blocks,&game,fa?&fa->blocks:NULL,
        fa?&fx:NULL,out)) {
        Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL glass map %s: %s (game size %u, fx size %u)\n",
            name,GlassMapFailure(),(unsigned)game.size,fa?(unsigned)fx.size:0u);
        return -1;
    }
    if (c1_readers) {
        glass_map_source_t c1;
        if (c1_glass_map(name,&c1)) {
            int code=c1.count!=out->count ? 1 : c1.count && (memcmp(c1.server_pieces,out->server_pieces,12*(size_t)c1.count) ||
                memcmp(c1.initial_pieces,out->initial_pieces,52*(size_t)c1.count)) ? 2 : 0;
            c1_reader_report(18,name,code);
            if (c1_readers==1) *out=c1;
        } else c1_reader_report(18,name,-1);
    }
    return 1;
}
int LoadMountedGlassGroups(const char *name,const glass_groups_t **out)
{
    archive_t *archive;
    asset_t asset;
    glass_world_cache_t *cached;
    if(!name || !*name || !out) return -1;
    {   archive_t *owner; const uint8_t *record;
        if(pool_held(18,name,&owner) && (record=c1_record_any(18,name))) {
            for(cached=owner->glass_worlds;cached;cached=cached->next)
                if(cached->definition==record) { *out=&cached->groups; return 1; }
            if((cached=calloc(1,sizeof(*cached)))) {
                if(c1_glass_groups(name,&cached->groups)) {
                    cached->definition=record; cached->next=owner->glass_worlds; owner->glass_worlds=cached;
                    *out=&cached->groups; return 1;
                }
                free(cached);
            }
        }
    }
    archive=find_dependency_definition(18,name,&asset);
    if(!archive) return 0;
    for(cached=archive->glass_worlds;cached;cached=cached->next)
        if(cached->definition==asset.data) { *out=&cached->groups; return 1; }
    cached=calloc(1,sizeof(*cached));
    if(!cached) return -1;
    if(!LoadGlassGroups(&archive->blocks,&asset,&cached->groups)) {
        free(cached); return -1;
    }
    if(c1_readers) {
        glass_groups_t c1={0};
        if(c1_glass_groups(name,&c1)) {
            c1_reader_report(20,name,CompareGlassGroups(&cached->groups,&c1));
            if(c1_readers==1) { FreeGlassGroups(&cached->groups); cached->groups=c1; } else FreeGlassGroups(&c1);
        } else c1_reader_report(20,name,-1);
    }
    cached->definition=asset.data;
    cached->next=archive->glass_worlds;
    archive->glass_worlds=cached;
    *out=&cached->groups;
    return 1;
}

/* ---- World assets from C1 records (c1Readers) --------------------------------------------------------------
 * ComWorld (primary lights), the glass map and glass groups (GameWorldMp, FxWorld) and addon MapEnts, from the
 * records C1 holds. */
static const uint8_t *w_ptr(const uint8_t *p,unsigned offset) { const uint8_t *v; memcpy(&v,p+offset,sizeof(v)); return v; }
static char *w_copy(const char *s)
{
    char *c; size_t n;
    if(!s) return NULL;
    n=strlen(s)+1; if(!(c=malloc(n))) return NULL;
    memcpy(c,s,n); return c;
}
static common_world_cache_t *c1_common_world(const char *name)
{
    const uint8_t *r=c1_record_any(16,name),*lights; common_world_cache_t *w; uint32_t count,i;
    if(!r || !(w=calloc(1,sizeof(*w)))) return NULL;
    count=world_u32(r+8); lights=w_ptr(r,12);
    w->world.name=w_copy((const char *)w_ptr(r,0)); w->world.isInUse=(int)world_u32(r+4); w->world.primaryLightCount=count;
    if(!w->world.name || (count && (!lights || !(w->world.primaryLights=calloc(count,sizeof(*w->world.primaryLights)))))) {
        free_common_world(w); return NULL;
    }
    for(i=0;i<count;++i) {
        struct ComPrimaryLight *light=w->world.primaryLights+i; const char *def=(const char *)w_ptr(lights+68*i,64);
        memcpy(light,lights+68*i,64);
        light->defName=def ? w_copy(def) : NULL;
        if(def && !light->defName) { free_common_world(w); return NULL; }
    }
    return w;
}
static int compare_common_worlds(const struct ComWorld *a,const struct ComWorld *b)
{
    unsigned i;
    if(!a->name || !b->name || strcmp(a->name,b->name) || a->isInUse!=b->isInUse || a->primaryLightCount!=b->primaryLightCount) return 1;
    for(i=0;i<a->primaryLightCount;++i) {
        const struct ComPrimaryLight *x=a->primaryLights+i,*y=b->primaryLights+i;
        if(memcmp(x,y,64)) return 2;
        if(!x->defName!=!y->defName || (x->defName && strcmp(x->defName,y->defName))) return 3;
    }
    return 0;
}
/* The glass map from the GameWorldMp and FxWorld records. 1 ok, 0 not from records. */
static int c1_glass_map(const char *name,glass_map_source_t *out)
{
    const uint8_t *game=c1_record_any(18,name),*fx,*glass;
    uint32_t count;
    if(!game) return 0;
    if(!(glass=w_ptr(game,80))) { *out=(glass_map_source_t){0}; return 1; }
    count=world_u32(glass+4);
    if(!count) { *out=(glass_map_source_t){0}; return 1; }
    if(!(fx=c1_record_any(20,name)) || world_u32(fx+24)!=count) return 0;
    *out=(glass_map_source_t){count,w_ptr(glass,0),w_ptr(fx,96)};
    return out->server_pieces && out->initial_pieces;
}
static int c1_glass_groups(const char *name,glass_groups_t *out)
{
    const uint8_t *game=c1_record_any(18,name),*glass,*groups; glass_group_view_t *views=NULL;
    uint32_t pieces=0,count=0,i,j; int ok=0;
    if(!game) return 0;
    if((glass=w_ptr(game,80))) { pieces=world_u32(glass+4); count=world_u32(glass+12); }
    groups=glass ? w_ptr(glass,16) : NULL;
    if(count && (!groups || !(views=calloc(count,sizeof(*views))))) return 0;
    for(i=0;i<count;++i) {
        const uint8_t *g=groups+12*i,*ids=w_ptr(g,8); unsigned n=xanim_u16(g+6); uint16_t *copy=NULL;
        if(n && (!ids || !(copy=malloc(2*(size_t)n)))) goto done;
        for(j=0;j<n;++j) copy[j]=(uint16_t)xanim_u16(ids+2*j);
        views[i]=(glass_group_view_t){(const char *)w_ptr(g,0),copy,(uint16_t)n};
    }
    ok=InstallGlassGroups(out,pieces,views,count);
done:
    for(i=0;i<count;++i) free((void *)views[i].pieces);
    free(views); return ok;
}
/* Addon MapEnts: the stream C1's loader reads (header 36, name, entity string, three trigger arrays). */
static int c1_addon_map_entities(const char *wanted,map_entities_t *out)
{
    static const unsigned strides[3]={8,32,20};
    const uint8_t *r=c1_record_any(42,wanted); uint8_t *s; size_t size,at; unsigned i; int ok;
    const char *name; const uint8_t *text;
    if(!r || !(name=(const char *)w_ptr(r,0)) || !(text=w_ptr(r,4))) return 0;
    size=36+strlen(name)+1+world_u32(r+8);
    for(i=0;i<3;++i) if(w_ptr(r,16+8*i)) size+=(size_t)world_u32(r+12+8*i)*strides[i];
    if(!(s=malloc(size))) return 0;
    memcpy(s,r,36); at=36;
    { uint32_t m=UINT32_MAX; memcpy(s,&m,4); memcpy(s+4,&m,4);
      for(i=0;i<3;++i) { uint32_t v=w_ptr(r,16+8*i) ? UINT32_MAX : 0; memcpy(s+16+8*i,&v,4); } }
    memcpy(s+at,name,strlen(name)+1); at+=strlen(name)+1;
    memcpy(s+at,text,world_u32(r+8)); at+=world_u32(r+8);
    for(i=0;i<3;++i) if(w_ptr(r,16+8*i)) {
        size_t n=(size_t)world_u32(r+12+8*i)*strides[i];
        memcpy(s+at,w_ptr(r,16+8*i),n); at+=n;
    }
    ok=ReadAddonMapEntities(s,size,out);
    free(s); return ok;
}
static int compare_map_entities(const map_entities_t *a,const map_entities_t *b)
{
    if(a->text_size!=b->text_size || (a->text_size && memcmp(a->text,b->text,a->text_size))) return 1;
    if(a->model_count!=b->model_count || a->hull_count!=b->hull_count || a->slab_count!=b->slab_count ||
       a->stage_count!=b->stage_count) return 2;
    if((a->model_count && memcmp(a->models,b->models,8*(size_t)a->model_count)) ||
       (a->hull_count && memcmp(a->hulls,b->hulls,32*(size_t)a->hull_count)) ||
       (a->slab_count && memcmp(a->slabs,b->slabs,20*(size_t)a->slab_count))) return 3;
    if(a->name && b->name && strcmp(a->name,b->name)) return 4;
    return 0;
}
int LoadEngineCommonWorld(const char *name,const struct ComWorld **out)
{
    archive_t *archive; asset_t asset; common_world_cache_t *cached;
    const uint8_t *lights; size_t pos=16; unsigned i,j; uint32_t count;
    if (!name || !*name || !out) return -1;
    {   archive_t *owner; const uint8_t *record;
        if (pool_held(16,name,&owner) && (record=c1_record_any(16,name))) {
            for (cached=owner->worlds;cached;cached=cached->next)
                if (cached->definition==record) { *out=&cached->world; return 1; }
            if ((cached=c1_common_world(name))) {
                cached->definition=record; cached->next=owner->worlds; owner->worlds=cached;
                *out=&cached->world; return 1;
            }
        }
    }
    archive=find_dependency_definition(16,name,&asset); if (!archive) return 0;
    for (cached=archive->worlds;cached;cached=cached->next)
        if (cached->definition==asset.data) { *out=&cached->world; return 1; }
    if (asset.size<16 || world_u32(asset.data+4)>1) return -1;
    count=world_u32(asset.data+8);
    if (count>ASSET_LIMIT/68 || (count && !world_u32(asset.data+12))) return -1;
    cached=calloc(1,sizeof(*cached)); if (!cached) return -1;
    if (!world_string(archive,&asset,&pos,world_u32(asset.data),&cached->world.name) ||
        !cached->world.name || !*cached->world.name) goto fail;
    if (count>(asset.size-pos)/68) goto fail;
    /* C1 4bff4c..4bff6e treats the light-array word as a presence flag:
     * every nonzero value loads a fresh aligned inline array. Unlike the
     * world/name strings, this word is not a packed block reference. The
     * replay layer consumes stream alignment; this asset view is contiguous. */
    lights=asset.data+pos; pos+=(size_t)count*68;
    cached->world.primaryLightCount=count;
    cached->world.isInUse=(int)world_u32(asset.data+4);
    if (count) {
        cached->world.primaryLights=calloc(count,sizeof(*cached->world.primaryLights));
        if (!cached->world.primaryLights) goto fail;
    }
    for (i=0;i<count;++i) {
        struct ComPrimaryLight *light=cached->world.primaryLights+i;
        const uint8_t *r=lights+68*i;
        for (j=4;j<64;j+=4) { float value; uint32_t bits=world_u32(r+j); memcpy(&value,&bits,4); if (!isfinite(value)) goto fail; }
        memcpy(light,r,64);
        if (!world_string(archive,&asset,&pos,world_u32(r+64),&light->defName)) goto fail;
    }
    if (pos!=asset.size) goto fail;
    if (c1_readers) {
        common_world_cache_t *c1=c1_common_world(cached->world.name);
        if (c1) {
            c1_reader_report(16,cached->world.name,compare_common_worlds(&cached->world,&c1->world));
            if (c1_readers==1) { free_common_world(cached); cached=c1; } else free_common_world(c1);
        } else c1_reader_report(16,cached->world.name,-1);
    }
    cached->definition=asset.data; cached->next=archive->worlds; archive->worlds=cached;
    *out=&cached->world; return 1;
fail:
    free_common_world(cached); return -1;
}

/* ---- Effects from C1 records (c1Readers) -------------------------------------------------------------------
 * The FxEffectDef record C1 holds gives the same source the serialized traversal does: the header scalars and name,
 * each 260-byte element (copied, with its extension, since records go when the mounted set changes), the sample
 * arrays and trail data where the replay placed them, effect and spawn-sound names, and visuals: strings, or a
 * model/material by the linked record's name (resolved by name in LoadMountedEffectVisual). */
static const uint8_t *fx_ptr(const uint8_t *p,unsigned offset) { const uint8_t *v; memcpy(&v,p+offset,sizeof(v)); return v; }
static const char *fx_name(const uint8_t *header,unsigned type)
{
    const char *n;
    if(!header) return NULL;
    n=(const char *)fx_ptr(header,type==10 ? 32 : 0);
    return n && *n==',' ? n+1 : n;
}
static int c1_effect_source(const char *name,effect_source_t *out)
{
    effect_source_t result={0}; const uint8_t *record,*elements; uint8_t *owned;
    size_t owned_size,at; unsigned i,j; uint64_t count=0;
    if(!(record=c1_record_any(30,name)) || !(result.prefix.name=(const char *)fx_ptr(record,0))) return 0;
    result.prefix.flags=world_u32(record+4); result.prefix.total_size=world_u32(record+8);
    { uint32_t life=world_u32(record+12); memcpy(&result.prefix.looping_life,&life,4); }
    for(i=0;i<3;++i) { result.prefix.counts[i]=world_u32(record+16+4*i); count+=result.prefix.counts[i]; }
    if(count>65536) return 0;
    result.prefix.count=(uint32_t)count;
    elements=fx_ptr(record,28);
    if(count && !elements) return 0;
    owned_size=260*(size_t)count;
    for(i=0;i<count;++i) if(fx_ptr(elements+260*i,244)) owned_size+=52;
    if(!(result.owned=owned=malloc(owned_size ? owned_size : 1))) return 0;
    memcpy(owned,elements,260*(size_t)count); at=260*(size_t)count;
    result.prefix.elements=count ? owned : NULL;
    if(count && !(result.elements=calloc(count,sizeof(*result.elements)))) goto fail;
    for(i=0;i<count;++i) {
        effect_element_t *e=result.elements+i; const uint8_t *r=owned+260*i,*visuals; unsigned type=r[176],n=r[177];
        e->record=r;
        if((e->velocity_samples=fx_ptr(r,180))) e->velocity_count=(unsigned)r[178]+1;
        if((e->visual_samples=fx_ptr(r,184))) e->visual_sample_count=(unsigned)r[179]+1;
        visuals=NULL;
        if(type==13) { if((visuals=fx_ptr(r,188))) e->visual_count=2*n; }
        else if(n<=1) { e->visual_count=1; visuals=r+188; }
        else if((visuals=fx_ptr(r,188))) e->visual_count=n;
        if(e->visual_count && !(e->visuals=calloc(e->visual_count,sizeof(*e->visuals)))) goto fail;
        for(j=0;j<e->visual_count;++j) {
            effect_visual_t *v=e->visuals+j; const uint8_t *p=fx_ptr(visuals,4*j);
            v->reference=world_u32(visuals+4*j);
            if(!p) continue;
            if(type==12) v->string=(const char *)p;
            else if(type==14) v->string=fx_name(p,30);
            else if(type!=10 && type!=11) v->name=fx_name(p,type==9 ? 4 : 5);
        }
        for(j=0;j<3;++j) e->effect_names[j]=fx_name(fx_ptr(r,216+4*j),30);
        if(fx_ptr(r,244)) {
            const uint8_t *x=fx_ptr(r,244);
            e->extension_size=type==5 ? 36 : type==8 ? 52 : 1;
            memcpy(owned+at,x,e->extension_size); e->extension=owned+at; at+=52;
            if(type==5) {
                e->trail_vertex_count=world_u32(x+20); e->trail_index_count=world_u32(x+28);
                e->trail_vertices=fx_ptr(x,24); e->trail_indices=fx_ptr(x,32);
            }
        }
        e->extra_name=(const char *)fx_ptr(r,256);
    }
    *out=result; return 1;
fail:
    FreeEffectSource(&result); return 0;
}
static const uint8_t *fx_visual_definition(archive_t *archive,const effect_element_t *e,unsigned index)
{
    const effect_visual_t *v=e->visuals+index; unsigned type=e->record[176]==9 ? 4 : 5;
    asset_t asset={0}; const char *n;
    if(v->name) n=v->name;
    else {
        if(!v->reference) return NULL;
        if(v->reference>=UINT32_MAX-1) asset=v->inline_asset;
        else if(!(type==4 ? ResolveModelAlias(&archive->blocks,v->reference,&asset) :
                  ResolveMaterialAlias(&archive->blocks,v->reference,&asset))) return NULL;
        n=dependency_asset_name(archive,&asset); if(!n) return NULL;
        if(*n==',') ++n;
    }
    return find_dependency_definition(type,n,&asset) ? asset.data : NULL;
}
static int c1_effect_compare(archive_t *archive,const effect_source_t *a,const effect_source_t *b)
{
    static const unsigned pointers[]={180,184,188,216,220,224,244,256};
    unsigned i,j,k;
    if(!same_text(a->prefix.name,b->prefix.name,0) || a->prefix.flags!=b->prefix.flags || a->prefix.total_size!=b->prefix.total_size ||
       a->prefix.looping_life!=b->prefix.looping_life || memcmp(a->prefix.counts,b->prefix.counts,sizeof(a->prefix.counts))) return 1;
    for(i=0;i<a->prefix.count;++i) {
        const effect_element_t *x=a->elements+i,*y=b->elements+i; unsigned type=x->record[176];
        for(k=0;k<260;k+=4) {
            int pointer=0; for(j=0;j<sizeof(pointers)/sizeof(pointers[0]);++j) pointer|=k==pointers[j];
            if(!pointer && memcmp(x->record+k,y->record+k,4)) return 2;
        }
        if(x->velocity_count!=y->velocity_count || (x->velocity_count && memcmp(x->velocity_samples,y->velocity_samples,96*x->velocity_count))) return 3;
        if(x->visual_sample_count!=y->visual_sample_count ||
           (x->visual_sample_count && memcmp(x->visual_samples,y->visual_samples,48*x->visual_sample_count))) return 4;
        if(x->visual_count!=y->visual_count) return 5;
        for(j=0;j<x->visual_count;++j) {
            if(type==12 || type==14) { if(!same_text(x->visuals[j].string,y->visuals[j].string,1)) return 6; }
            else if(type!=10 && type!=11 && fx_visual_definition(archive,x,j)!=fx_visual_definition(archive,y,j)) return 7;
        }
        for(j=0;j<3;++j) if(!same_text(x->effect_names[j],y->effect_names[j],1)) return 8;
        if(!same_text(x->extra_name,y->extra_name,0)) return 9;
        if(x->extension_size!=y->extension_size) return 10;
        if(x->extension_size) {
            for(k=0;k<x->extension_size;++k) if(!(type==5 && ((k>=24 && k<28) || (k>=32 && k<36))) && x->extension[k]!=y->extension[k]) return 11;
            if(x->trail_vertex_count!=y->trail_vertex_count || x->trail_index_count!=y->trail_index_count ||
               (x->trail_vertex_count && memcmp(x->trail_vertices,y->trail_vertices,20*x->trail_vertex_count)) ||
               (x->trail_index_count && memcmp(x->trail_indices,y->trail_indices,2*x->trail_index_count))) return 12;
        }
    }
    return 0;
}
int LoadMountedEffectSource(const char *name,const effect_source_t **out)
{
    archive_t *archive; asset_t asset; effect_cache_t *cached; model_block_t blocks[8];
    if(!name || !*name || *name==',' || !out) return -1;
    {   archive_t *owner; const uint8_t *record;
        if(pool_held(30,name,&owner) && (record=c1_record_any(30,name))) {
            for(cached=owner->effects;cached;cached=cached->next)
                if(cached->definition==record) { *out=&cached->source; return 1; }
            if((cached=calloc(1,sizeof(*cached)))) {
                if(c1_effect_source(name,&cached->source)) {
                    ++c1_pool_builds[30];
                    cached->definition=record; cached->archive=owner; cached->next=owner->effects; owner->effects=cached;
                    *out=&cached->source; return 1;
                }
                free(cached);
            }
        }
    }
    archive=find_dependency_definition(30,name,&asset); if(!archive) return 0;
    for(cached=archive->effects;cached;cached=cached->next)
        if(cached->definition==asset.data) { *out=&cached->source; return 1; }
    cached=calloc(1,sizeof(*cached)); if(!cached) return -1;
    LoadBlockViews(&archive->blocks,blocks); blocks[0]=(model_block_t){0};
    if(!ReadEffectSource(&asset,blocks,&cached->source)) { free(cached); return -1; }
    if(c1_readers) {
        effect_source_t c1={{0}};
        if(c1_effect_source(cached->source.prefix.name,&c1)) {
            c1_reader_report(30,cached->source.prefix.name,c1_effect_compare(archive,&cached->source,&c1));
            if(c1_readers==1) { FreeEffectSource(&cached->source); cached->source=c1; }
            else FreeEffectSource(&c1);
        } else c1_reader_report(30,cached->source.prefix.name,-1);
    }
    cached->definition=asset.data; cached->archive=archive; cached->next=archive->effects; archive->effects=cached;
    *out=&cached->source; return 1;
}
int LoadMountedEffectVisual(const char *name,unsigned element,unsigned index,effect_visual_source_t *out)
{
    const effect_source_t *source; const effect_visual_t *visual;
    effect_cache_t *cached; archive_t *archive; effect_visual_source_t result={0};
    unsigned type; int status;
    if(!out) return -1;
    status=LoadMountedEffectSource(name,&source); if(status!=1) return status;
    if(element>=source->prefix.count || index>=source->elements[element].visual_count) return -1;
    type=source->elements[element].record[176]; visual=source->elements[element].visuals+index;
    if(type==10 || type==11 || type==12 || type==14 || !visual->reference) return 0;
    type=type==9 ? 4 : 5; cached=(effect_cache_t *)source; archive=cached->archive;
    if(visual->name) { /* from a C1 record: the linked visual by name */
        archive_t *owner;
        if(pool_held(type,visual->name,&owner)) { /* its engine loader takes C1's record too */
            result.asset=(asset_t){type,NULL,0}; result.name=visual->name; *out=result; return 1;
        }
        archive=find_dependency_definition(type,visual->name,&result.asset);
        if(!archive) return -1;
        result.name=dependency_asset_name(archive,&result.asset);
        if(!result.name || !*result.name) return -1;
        LoadBlockViews(&archive->blocks,result.blocks); result.blocks[0]=(model_block_t){0};
        *out=result; return 1;
    }
    if(visual->reference>=UINT32_MAX-1) result.asset=visual->inline_asset;
    else {
        status=type==4 ? ResolveModelAlias(&archive->blocks,visual->reference,&result.asset) :
            ResolveMaterialAlias(&archive->blocks,visual->reference,&result.asset);
        if(!status) return -1;
    }
    if(!result.asset.data || result.asset.type!=type) return -1;
    result.name=dependency_asset_name(archive,&result.asset);
    if(!result.name || !*result.name) return -1;
    archive=find_dependency_definition(type,result.name[0]==',' ? result.name+1 : result.name,&result.asset);
    if(!archive) return -1;
    result.name=dependency_asset_name(archive,&result.asset);
    LoadBlockViews(&archive->blocks,result.blocks); result.blocks[0]=(model_block_t){0};
    *out=result; return 1;
}
static native_effect_cache_t *effect_node(effect_transaction_t *transaction,const char *name,int *status)
{
    const effect_source_t *source; effect_cache_t *owner; native_effect_cache_t *node;
    *status=LoadMountedEffectSource(name,&source); if(*status!=1) return NULL;
    owner=(effect_cache_t *)source;
    for(node=owner->native;node;node=node->next)
        if(node->generation==transaction->generation && node->resolver==transaction->resolver && node->context==transaction->context) return node;
    for(node=transaction->pending;node;node=node->next) if(node->owner==owner) return node;
    node=calloc(1,sizeof(*node)); if(!node) { *status=-1; return NULL; }
    node->owner=owner; node->generation=transaction->generation; node->resolver=transaction->resolver; node->context=transaction->context;
    node->next=transaction->pending; transaction->pending=node; return node;
}
static void free_effect_work(native_effect_cache_t *node)
{
    unsigned i;
    if(node->bindings) for(i=0;i<node->owner->source.prefix.count;++i) free((void *)node->bindings[i].visuals);
    free(node->bindings); free(node->children); node->bindings=NULL; node->children=NULL;
}
/* Why the last LoadMountedNativeEffect returned -1: the graph member
 * that failed and the check it failed. Diagnostic only; empty after success. */
static char effect_failure[256];
const char *NativeEffectFailureDetail(void) { return effect_failure; }
static const char *va_effect(const char *format,...)
{
    static char text[160]; va_list args;
    va_start(args,format); vsnprintf(text,sizeof(text),format,args); va_end(args);
    return text;
}
#define EFFECT_FAIL_DETAIL(effect,what) snprintf(effect_failure,sizeof(effect_failure),"%s: %s",(effect) ? (effect) : "?",(what))
int LoadMountedNativeEffect(const char *name,effect_model_resolver_t resolver,void *context,const FxEffectDef **out)
{
    effect_transaction_t transaction={NULL,mount_serial,resolver,context}; native_effect_cache_t *root,*node;
    unsigned i,j; int status;
    if(!out) return -1;
    effect_failure[0]=0;
    root=effect_node(&transaction,name,&status);
    if(!root) {
        if(status<0) EFFECT_FAIL_DETAIL(name,"source did not decode");
        return status;
    }
    for(;;) {
        const effect_source_t *source;
        for(node=transaction.pending;node && node->prepared;node=node->next) {}
        if(!node) break;
        source=&node->owner->source;
        if(source->prefix.count) {
            node->bindings=calloc(source->prefix.count,sizeof(*node->bindings));
            node->children=calloc(source->prefix.count,3*sizeof(*node->children));
            if(!node->bindings || !node->children) goto fail;
        }
        for(i=0;i<source->prefix.count;++i) {
            const effect_element_t *element=source->elements+i;
            unsigned type=element->record[176]; effect_bindings_t *binding=node->bindings+i;
            /* C1 knows element types 0..14; BuildNativeEffect decides
             * which of them this build retains natively. */
            if(type>14) {
                EFFECT_FAIL_DETAIL(source->prefix.name,va_effect("element %u has unsupported type %u",i,type)); goto fail;
            }
            if(type!=12) {
                const void **visuals=calloc(element->visual_count,sizeof(*visuals));
                if(element->visual_count && !visuals) goto fail;
                binding->visuals=visuals; binding->visual_count=element->visual_count;
                for(j=0;j<element->visual_count;++j) {
                    effect_visual_source_t visual;
                    if(type==14) {
                        const char *runner=element->visuals[j].string;
                        native_effect_cache_t *child;
                        if(!runner) continue;
                        child=effect_node(&transaction,runner,&status);
                        if(!child) { EFFECT_FAIL_DETAIL(runner,status ? "runner source did not decode" : "runner is in no mounted zone"); goto fail; }
                        visuals[j]=child->native.effect ? child->native.effect : &child->placeholder;
                        continue;
                    }
                    status=LoadMountedEffectVisual(source->prefix.name,i,j,&visual);
                    if(status<0) { EFFECT_FAIL_DETAIL(source->prefix.name,va_effect("element %u visual %u did not resolve",i,j)); goto fail; }
                    if(!status) continue;
                    if(visual.asset.type==5) {
                        const Material_t *material;
                        if(LoadEngineMaterial(visual.name,&material)!=1) { EFFECT_FAIL_DETAIL(source->prefix.name,va_effect("material %s did not load",visual.name)); goto fail; }
                        visuals[j]=material;
                    } else {
                        if(!resolver || !(visuals[j]=resolver(context,visual.name))) { EFFECT_FAIL_DETAIL(source->prefix.name,va_effect("model %s did not load",visual.name)); goto fail; }
                        if(mount_serial!=transaction.generation) goto fail;
                    }
                }
            }
            for(j=0;j<3;++j) if(element->effect_names[j]) {
                native_effect_cache_t *child=effect_node(&transaction,element->effect_names[j],&status);
                if(!child) { EFFECT_FAIL_DETAIL(element->effect_names[j],status ? "referenced effect did not decode" : "referenced effect is in no mounted zone"); goto fail; }
                node->children[3*i+j]=child;
                binding->effects[j]=child->native.effect ? child->native.effect : &child->placeholder;
            }
        }
        if(!BuildNativeEffect(source,node->bindings,&node->native)) {
            EFFECT_FAIL_DETAIL(source->prefix.name,NativeEffectBuildFailure()); goto fail;
        }
        node->prepared=1;
    }
    if(mount_serial!=transaction.generation) goto fail;
    /* Every graph member now owns its final storage. Remove all temporary
     * references before exposing any node, including self/mutual references. */
    for(node=transaction.pending;node;node=node->next)
        for(i=0;i<node->owner->source.prefix.count;++i) for(j=0;j<3;++j)
            if(node->children[3*i+j]) node->native.effect->elemDefs[i].effectRefs[j]=node->children[3*i+j]->native.effect;
    /* Runner visuals participate in the same graph transaction, including
     * cycles. Replace temporary handles only after every node is prepared. */
    for(node=transaction.pending;node;node=node->next)
        for(i=0;i<node->owner->source.prefix.count;++i) {
            struct FxElemDef *element=node->native.effect->elemDefs+i;
            unsigned count=node->owner->source.elements[i].visual_count;
            /* Source identity is authoritative across both native layouts. */
            if(node->owner->source.elements[i].record[176]!=14) continue;
            for(j=0;j<count;++j) {
                void **slot=count>1 ? (void **)element->visuals+j : &element->visuals;
                native_effect_cache_t *child;
                for(child=transaction.pending;child;child=child->next)
                    if(*slot==&child->placeholder) { *slot=child->native.effect; break; }
            }
        }
    while(transaction.pending) {
        node=transaction.pending; transaction.pending=node->next; free_effect_work(node);
        node->next=node->owner->native; node->owner->native=node;
    }
    *out=root->native.effect; return 1;
fail:
    while(transaction.pending) {
        node=transaction.pending; transaction.pending=node->next; free_effect_work(node);
        FreeNativeEffect(&node->native); free(node);
    }
    return -1;
}
/* Nested sound references keep the archive's own catalogue, then follow the
 * winning named definition exactly like model, material and effect links. */
static char sound_dep_detail[1024];
const char *SoundDependencyFailureDetail(void) { return sound_dep_detail; }
struct pool_blob_s { struct pool_blob_s *next; };
static void *pool_blob(archive_t *a,size_t n)
{
    struct pool_blob_s *b=calloc(1,sizeof(*b)+n);
    if(!b) return NULL;
    b->next=a->pool_blobs; a->pool_blobs=b; return b+1;
}
/* A SndCurve (136 bytes, name at +0) as the inline stream its loader reads, from C1's record. */
static int pool_sound_curve(archive_t *owner,const uint8_t *record,asset_t *out)
{
    const char *name; size_t n; uint8_t *data; uint32_t inline_word=UINT32_MAX;
    memcpy(&name,record,sizeof(name));
    if(!name) return 0;
    n=strlen(name)+1;
    if(!(data=pool_blob(owner,136+n))) return 0;
    memcpy(data,record,136); memcpy(data,&inline_word,4); memcpy(data+136,name,n);
    *out=(asset_t){12,data,136+n}; return 1;
}
static int sound_dependency(void *context,unsigned type,uint32_t reference,sound_view_t *out)
{
    archive_t *archive=context; sound_view_t result={0}; const char *name;
    if(!archive || !out || (type!=SOUND_SPAN && (type<11 || type>13))) return -1;
    if(type==SOUND_SPAN) {
        /* Deduplicated data pointer: hand back the serialized span the loader
         * replayed there. Its own archive owns those bytes until unmount. */
        if(!ResolveSoundSpan(&archive->blocks,reference,&result.asset,NULL)) return -1;
        LoadBlockViews(&archive->blocks,result.blocks);
        result.blocks[0]=(model_block_t){0};
        *out=result; return 1;
    }
    if(!ResolveSoundAlias(&archive->blocks,type,reference,&result.asset)) {
        /* Diagnostic for the engine error path: where the request fell among
         * this archive's recorded aliases, and every curve alias it holds. */
        size_t i,n=archive->blocks.asset_alias_count,used=0,index=0;
        const archive_t *walk;
        uint32_t below=0,above=0; unsigned below_type=0,above_type=0,curves=0;
        for(walk=archives;walk && walk!=archive;walk=walk->next) ++index;
        for(i=0;i<n;++i) {
            const asset_alias_t *a=archive->blocks.asset_aliases+i;
            if(a->reference<=reference && a->reference>=below) { below=a->reference; below_type=a->asset.type; }
            if(a->reference>reference && (!above || a->reference<above)) { above=a->reference; above_type=a->asset.type; }
        }
        used=(size_t)snprintf(sound_dep_detail,sizeof(sound_dep_detail),
            "archive #%u aliases %u: below 0x%08x/%u above 0x%08x/%u; curve aliases:",
            (unsigned)index,(unsigned)n,below,below_type,above,above_type);
        for(i=0;i<n && used<sizeof(sound_dep_detail)-16;++i) {
            const asset_alias_t *a=archive->blocks.asset_aliases+i;
            if(a->asset.type!=12) continue;
            ++curves;
            used+=(size_t)snprintf(sound_dep_detail+used,sizeof(sound_dep_detail)-used," 0x%08x",a->reference);
        }
        if(used<sizeof(sound_dep_detail)-16) used+=(size_t)snprintf(sound_dep_detail+used,sizeof(sound_dep_detail)-used," (%u)",curves);
        if(((reference-1)>>28)==3) {
            size_t offset=(reference-1)&0xfffffff;
            const load_blocks_t *b=&archive->blocks;
            if(offset+40<=b->written[3] && used<sizeof(sound_dep_detail)-120) {
                const uint8_t *p=b->data[3]+offset;
                used+=(size_t)snprintf(sound_dep_detail+used,sizeof(sound_dep_detail)-used,"; block3 written 0x%x words:",(unsigned)b->written[3]);
                for(i=0;i<10 && used<sizeof(sound_dep_detail)-12;++i)
                    used+=(size_t)snprintf(sound_dep_detail+used,sizeof(sound_dep_detail)-used," %08x",
                        (unsigned)(p[4*i]|p[4*i+1]<<8|p[4*i+2]<<16|(unsigned)p[4*i+3]<<24));
            }
            for(i=0;i<b->sound_record_count && used<sizeof(sound_dep_detail)-60;++i) {
                const sound_span_t *r=b->sound_records+i;
                if(((r->reference-1)>>28)!=3) continue;
                if(offset>=((r->reference-1)&0xfffffff) && offset<((r->reference-1)&0xfffffff)+r->size)
                    used+=(size_t)snprintf(sound_dep_detail+used,sizeof(sound_dep_detail)-used,"; inside span 0x%08x size %u count %u at +%u",
                        r->reference,r->size,r->count,(unsigned)(offset-((r->reference-1)&0xfffffff)));
            }
        }
        return -1;
    }
    name=dependency_asset_name(archive,&result.asset);
    if(!name || !*name) { snprintf(sound_dep_detail,sizeof(sound_dep_detail),"alias resolved but its name is empty"); return -1; }
    if(c1_readers==1 && (type==12 || type==13)) { /* linked into a zone C1 holds: no mount */
        archive_t *owner=NULL; const uint8_t *record=c1_pool_record(type,name[0]==',' ? name+1 : name,&owner);
        if(record) {
            /* a loaded sound keeps its stub: the server never reads sample data */
            if(type==13 || !pool_sound_curve(owner,record,&result.asset)) return 0;
            ++c1_pool_builds[12]; *out=result; return 1;
        }
    }
    archive=find_dependency_definition(type,name[0]==',' ? name+1 : name,&result.asset);
    if(!archive) { snprintf(sound_dep_detail,sizeof(sound_dep_detail),"definition '%s' not mounted",name); return 0; }
    LoadBlockViews(&archive->blocks,result.blocks);
    result.blocks[0]=(model_block_t){0};
    *out=result; return 1;
}
/* Inline curves and loaded sounds whose name starts with ',' are link stubs: C1 points the alias at the
 * definition of that name (DB_LinkXAssetEntry 0x4cc390). Measured: amb_creation_mp's ",$default" curve is the
 * two-knot $default curve in C1, not the stub's zero knots. A stub whose definition is not mounted stays as is. */
static void link_sound_stubs(sound_alias_list_t *list)
{
    unsigned i;
    for(i=0;i<list->count;++i) {
        sound_alias_t *a=list->aliases+i;
        sound_view_t view={0}; archive_t *owner;
        if(a->has_curve && a->curve.name && a->curve.name[0]==',' &&
            (owner=find_dependency_definition(12,a->curve.name+1,&view.asset))) {
            sound_curve_t curve;
            LoadBlockViews(&owner->blocks,view.blocks); view.blocks[0]=(model_block_t){0};
            if(ReadSoundCurveAsset(&view,&curve)) a->curve=curve;
        }
        if(a->has_file && a->file.has_loaded && a->file.loaded.name && a->file.loaded.name[0]==',' &&
            (owner=find_dependency_definition(13,a->file.loaded.name+1,&view.asset))) {
            loaded_sound_t sound;
            memset(&view,0,sizeof(view));
            find_dependency_definition(13,a->file.loaded.name+1,&view.asset);
            LoadBlockViews(&owner->blocks,view.blocks); view.blocks[0]=(model_block_t){0};
            if(ReadLoadedSoundAsset(&view,&sound)) a->file.loaded=sound;
        }
    }
}
int LoadMountedSoundAliasList(const char *name,const sound_alias_list_t **out)
{
    archive_t *archive; asset_t asset; sound_cache_t *cached; sound_view_t view={0};
    if(!name || !*name || *name==',' || !out) return -1;
    archive=find_dependency_definition(11,name,&asset); if(!archive) return 0;
    for(cached=archive->sounds;cached;cached=cached->next)
        if(cached->definition==asset.data) { *out=&cached->source; return 1; }
    cached=calloc(1,sizeof(*cached)); if(!cached) return -1;
    view.asset=asset; LoadBlockViews(&archive->blocks,view.blocks);
    view.blocks[0]=(model_block_t){0};
    if(!ReadSoundAliasList(&view,sound_dependency,archive,&cached->source)) { free(cached); return -1; }
    link_sound_stubs(&cached->source);
    cached->definition=asset.data; cached->archive=archive;
    cached->next=archive->sounds; archive->sounds=cached;
    *out=&cached->source; return 1;
}
/* Publication helper: every sound definition a mounted archive owns. Real
 * definitions only, so DB_LinkXAssetEntry style link references are skipped. */
int EnumerateMountedSoundAliases(int (*visit)(void *,const char *),void *context)
{
    archive_t *archive; int visited=0;
    if(!visit) return -1;
    for(archive=archives;archive;archive=archive->next) {
        size_t i;
        for(i=0;i<archive->blocks.asset_alias_count;++i) {
            const asset_t *asset=&archive->blocks.asset_aliases[i].asset;
            const char *name;
            if(asset->type!=11) continue;
            name=dependency_asset_name(archive,asset);
            if(!name) return -1;
            if(!*name || name[0]==',') continue;
            ++visited;
            if(!visit(context,name)) return visited;
        }
    }
    return visited;
}
int LoadEngineNativeSound(const char *name,const snd_alias_list_t **out)
{
    const sound_alias_list_t *source; sound_cache_t *cached; int status;
    if(!out) return -1;
    if(c1_readers==1 && name && *name) { /* defined in a zone C1 holds: an owned copy of C1's own list, no mount */
        archive_t *owner=NULL; const snd_alias_list_t *record=(const snd_alias_list_t *)c1_pool_record(11,name,&owner);
        if(record) {
            for(cached=owner->sounds;cached;cached=cached->next)
                if(cached->definition==(const uint8_t *)record) { *out=cached->native.list; return 1; }
            if((cached=calloc(1,sizeof(*cached)))) {
                if(CopyC1SoundAliasList(record,&cached->native)) {
                    ++c1_pool_builds[11];
                    cached->native_ready=1; cached->definition=(const uint8_t *)record; cached->archive=owner;
                    cached->next=owner->sounds; owner->sounds=cached;
                    *out=cached->native.list; return 1;
                }
                free(cached);
            }
        }
    }
    if(c1_provider && name && *name) {
        asset_t asset; const archive_t *defining=find_dependency_definition(11,name,&asset);
        const snd_alias_list_t *own=provided_record(11,name,defining);
        if(own) { *out=own; return 1; }
    }
    status=LoadMountedSoundAliasList(name,&source); if(status!=1) return status;
    cached=(sound_cache_t *)source;
    if(!cached->native_ready) {
        if(!BuildNativeSoundAliasList(&cached->source,&cached->native)) return -1;
        cached->native_ready=1;
    }
    *out=cached->native.list; return 1;
}
int LoadMountedDynamicDependency(const char *name,unsigned category,unsigned index,
    unsigned type,dynamic_dependency_t *out)
{
    archive_t *archive; unsigned offset;
    if(!name || !*name || category>1 || !out || (type!=0 && type!=4 && type!=30)) return -1;
    offset=type==4 ? 32 : type==30 ? 40 : 44;
    {   /* C1's clipMap record: the dependency by name (its engine loader takes C1's record too) */
        const uint8_t *clipmap=pool_clipmap(name);
        if(clipmap) {
            dynamic_dependency_t result={0}; unsigned count; const uint8_t *defs=pool_dynamic_defs(clipmap,category,&count),*a;
            const char *n;
            if(defs && index<count) {
                memcpy(&a,defs+92*index+offset,sizeof(a));
                if(!a) return 0;
                memcpy(&n,a,sizeof(n));
                if(n && *n) {
                    char path[256]; archive_t *owner=NULL;
                    if(snprintf(path,sizeof(path),"maps/mp/%s.d3dbsp",name)<(int)sizeof(path) &&
                       (c1_pool_record(15,path,&owner) || c1_pool_record(14,path,&owner)) && owner)
                        n=archive_text(owner,n);
                    if(!n) return -1;
                    result.name=*n==',' ? n+1 : n; result.asset=(asset_t){type,NULL,0}; *out=result; return 1;
                }
            }
        }
    }
    for(archive=archives;archive;archive=archive->next) {
        asset_t collision,world; dynamic_records_t records;
        dynamic_dependency_t result={0}; const uint8_t *p; uint32_t ref;
        int status;
        if(!c1_backed_need_map(archive,name)) continue;
        status=FindZoneCollisionPair(&archive->zone,name,&collision,&world);
        if(status<0) return -1;
        if(!status) continue;
        if(!archive->prepared || !ResolveDynamicRecords(&archive->blocks,&collision,category,&records) ||
            index>=records.count) return -1;
        p=records.records+92*index+offset;
        ref=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
        if(!ref) return 0;
        if(ref>=UINT32_MAX-1) ref=records.reference+92*index+offset;
        status=type==4 ? ResolveModelAlias(&archive->blocks,ref,&result.asset) :
            type==30 ? ResolveEffectAlias(&archive->blocks,ref,&result.asset) :
            ResolvePhysicsAlias(&archive->blocks,0,ref,&result.asset);
        if(!status) return -1;
        result.name=dependency_asset_name(archive,&result.asset);
        if(!result.name || !*result.name) return -1;
        archive=find_dependency_definition(type,result.name[0]==',' ? result.name+1 : result.name,&result.asset);
        if(!archive) return -1;
        result.name=dependency_asset_name(archive,&result.asset);
        LoadBlockViews(&archive->blocks,result.blocks); result.blocks[0]=(model_block_t){0};
        *out=result; return 1;
    }
    return 0;
}
int LoadMountedModelPhysics(const char *name,unsigned type,mounted_model_physics_t *out)
{
    const mounted_model_skeleton_t *model;
    const mounted_model_geometry_t *geometry;
    model_cache_t *cached; archive_t *archive;
    mounted_model_physics_t result={0}; asset_t inline_assets[2];
    uint32_t ref; int status;
    if (!out || type>1) return -1;
    status=LoadMountedXModelSkeleton(name,&model); if (status!=1) return status;
    status=LoadMountedXModelGeometry(name,&geometry); if (status!=1) return status;
    ref=type ? geometry->bounds.physics_map_reference : geometry->bounds.physics_preset_reference;
    if (!ref) return 0;
    cached=(model_cache_t *)model; archive=cached->archive;
    if (cached->pool) { /* by name: the engine preset/map come from C1's records too */
        const uint8_t *record=rec_word_ptr(cached->c1_record,type ? 0x164 : 0x160); const char *n;
        if (!record || !(n=(const char *)rec_word_ptr(record,0)) || !*n) return -1;
        result.name=*n==',' ? n+1 : n; result.asset=(asset_t){type,NULL,0};
        *out=result; return 1;
    }
    if (ref>=UINT32_MAX-1) {
        if (!XModelInlinePhysicsAssets(&cached->source,inline_assets)) return -1;
        result.asset=inline_assets[type];
    } else if (!ResolvePhysicsAlias(&archive->blocks,type,ref,&result.asset)) return -1;
    result.name=dependency_asset_name(archive,&result.asset);
    if (!result.name || !*result.name) return -1;
    archive=find_dependency_definition(type,result.name[0]==',' ? result.name+1 : result.name,&result.asset);
    if (!archive) return -1;
    result.name=dependency_asset_name(archive,&result.asset);
    LoadBlockViews(&archive->blocks,result.blocks); result.blocks[0]=(model_block_t){0};
    *out=result; return 1;
}
int LoadEngineMaterial(const char *name,const Material_t **out)
{
    mounted_model_material_t source={0}; archive_t *archive;
    material_cache_t *cached;
    if (!name || !*name || !out) return -1;
    if (*name==',') { ++name; if (!*name) return -1; }
    if (c1_readers==1) { /* defined in a zone C1 holds: from C1's own Material record, no mount */
        archive_t *owner=NULL; const uint8_t *record=c1_pool_record(5,name,&owner);
        if (record) {
            for (cached=owner->materials;cached;cached=cached->next)
                if (cached->definition==record) { *out=&cached->native.material; return 1; }
            if ((cached=calloc(1,sizeof(*cached)))) {
                if (BuildC1NativeMaterial(record,&cached->native)) {
                    ++c1_pool_builds[5];
                    cached->definition=record; cached->next=owner->materials; owner->materials=cached;
                    *out=&cached->native.material; return 1;
                }
                free(cached);
            }
        }
    }
    archive=find_dependency_definition(5,name,&source.asset);
    if (!archive) {
        /* A known CoDOL dependency must not silently become an IW3 default. */
        size_t i;
        for (archive=archives;archive;archive=archive->next)
            for (i=0;i<archive->blocks.asset_alias_count;++i) {
                const asset_t *asset=&archive->blocks.asset_aliases[i].asset;
                const char *candidate;
                if (asset->type!=5) continue;
                candidate=dependency_asset_name(archive,asset);
                if (candidate && candidate[0]==',' && asset_name_equal(candidate+1,name)) return -1;
            }
        return 0;
    }
    for (cached=archive->materials;cached;cached=cached->next)
        if (cached->definition==source.asset.data) { *out=&cached->native.material; return 1; }
    source.name=dependency_asset_name(archive,&source.asset);
    LoadBlockViews(&archive->blocks,source.blocks); source.blocks[0]=(model_block_t){0};
    cached=calloc(1,sizeof(*cached)); if (!cached) return -1;
    if (!BuildNativeMaterial(&source,&cached->native)) { free(cached); return -1; }
    C1ReplaceMaterial(&cached->native);
    cached->definition=source.asset.data; cached->next=archive->materials; archive->materials=cached;
    *out=&cached->native.material; return 1;
}
int LoadMountedPhysicsMap(const char *name,const physics_map_t **out)
{
    archive_t *archive; physics_map_cache_t *cached;
    mounted_model_physics_t source={0};
    if (!name || !*name || *name==',' || !out) return -1;
    if (c1_readers==1) { /* defined in a zone C1 holds: from C1's own PhysCollmap record, no mount */
        archive_t *owner=NULL; const uint8_t *record=c1_pool_record(1,name,&owner);
        if (record) {
            mounted_model_physics_t c1src={0};
            for (cached=owner->physics_maps;cached;cached=cached->next)
                if (cached->definition==record) { *out=&cached->source; return 1; }
            if ((cached=calloc(1,sizeof(*cached)))) {
                if (SerializeC1PhysicsMap(record,&c1src.asset) && ReadPhysicsMap(&c1src,&cached->source)) {
                    ++c1_pool_builds[1];
                    cached->c1_stream=(uint8_t *)c1src.asset.data;
                    cached->definition=record; cached->next=owner->physics_maps; owner->physics_maps=cached;
                    *out=&cached->source; return 1;
                }
                free((void *)c1src.asset.data); free(cached);
            }
        }
    }
    archive=find_dependency_definition(1,name,&source.asset);
    if (!archive) return 0;
    for (cached=archive->physics_maps;cached;cached=cached->next)
        if (cached->definition==source.asset.data) { *out=&cached->source; return 1; }
    LoadBlockViews(&archive->blocks,source.blocks); source.blocks[0]=(model_block_t){0};
    cached=calloc(1,sizeof(*cached)); if (!cached) return -1;
    if (!ReadPhysicsMap(&source,&cached->source)) { free(cached); return -1; }
    if (c1_readers) { /* the same map from the PhysCollmap record C1 holds */
        const uint8_t *record=C1Record(1,name); mounted_model_physics_t c1src={0}; physics_map_t c1={0};
        if (record && SerializeC1PhysicsMap(record,&c1src.asset) && ReadPhysicsMap(&c1src,&c1)) {
            c1_reader_report(1,name,ComparePhysicsMaps(&cached->source,&c1));
            if (c1_readers==1) { FreePhysicsMap(&cached->source); cached->source=c1; cached->c1_stream=(uint8_t *)c1src.asset.data; }
            else { FreePhysicsMap(&c1); free((void *)c1src.asset.data); }
        } else { c1_reader_report(1,name,-1); free((void *)c1src.asset.data); }
    }
    cached->definition=source.asset.data; cached->next=archive->physics_maps; archive->physics_maps=cached;
    *out=&cached->source; return 1;
}
int LoadEnginePhysicsMap(const char *name,const PhysGeomList_t **out)
{
    const physics_map_t *source; physics_map_cache_t *cached; int status;
    if (!out) return -1;
    status=LoadMountedPhysicsMap(name,&source); if (status!=1) return status;
    /* source is the first member of this loader's private cache object. */
    cached=(physics_map_cache_t *)source;
    if (!cached->native_ready) {
        if (!BuildNativePhysicsMap(source,&cached->native)) return -1;
        cached->native_ready=1;
    }
    *out=&cached->native.map; return 1;
}
int LoadEnginePhysicsPreset(const char *name,const struct PhysPreset **out)
{
    archive_t *archive; physics_preset_cache_t *cached;
    mounted_model_physics_t source={0};
    if(!name || !*name || *name==',' || !out) return -1;
    if(c1_readers==1) { /* defined in a zone C1 holds: from C1's own PhysPreset record, no mount */
        archive_t *owner=NULL; const uint8_t *record=c1_pool_record(0,name,&owner);
        if(record) {
            mounted_model_physics_t c1src={0};
            for(cached=owner->physics_presets;cached;cached=cached->next)
                if(cached->definition==record) { *out=&cached->native.preset; return 1; }
            if((cached=calloc(1,sizeof(*cached)))) {
                int ok=SerializeC1PhysicsPreset(record,&c1src.asset) && BuildNativePhysicsPreset(&c1src,&cached->native);
                free((void *)c1src.asset.data);
                if(ok) {
                    ++c1_pool_builds[0];
                    cached->definition=record; cached->next=owner->physics_presets; owner->physics_presets=cached;
                    *out=&cached->native.preset; return 1;
                }
                free(cached);
            }
        }
    }
    archive=find_dependency_definition(0,name,&source.asset);
    if(!archive) return 0;
    for(cached=archive->physics_presets;cached;cached=cached->next)
        if(cached->definition==source.asset.data) { *out=&cached->native.preset; return 1; }
    source.name=dependency_asset_name(archive,&source.asset);
    LoadBlockViews(&archive->blocks,source.blocks); source.blocks[0]=(model_block_t){0};
    cached=calloc(1,sizeof(*cached)); if(!cached) return -1;
    if(!BuildNativePhysicsPreset(&source,&cached->native)) { free(cached); return -1; }
    if(c1_readers) { /* the same preset from the PhysPreset record C1 holds; the native copies its strings */
        const uint8_t *record=C1Record(0,name); mounted_model_physics_t c1src={0};
        native_physics_preset_t c1={0};
        if(record && SerializeC1PhysicsPreset(record,&c1src.asset) && BuildNativePhysicsPreset(&c1src,&c1)) {
            c1_reader_report(0,name,ComparePhysicsPresets(&cached->native.source,&c1.source));
            if(c1_readers==1) { FreeNativePhysicsPreset(&cached->native); cached->native=c1; }
            else FreeNativePhysicsPreset(&c1);
        } else c1_reader_report(0,name,-1);
        free((void *)c1src.asset.data);
    }
    cached->definition=source.asset.data; cached->next=archive->physics_presets; archive->physics_presets=cached;
    *out=&cached->native.preset; return 1;
}
/* 0 when two surface sets hold the same geometry (pooled arrays resolved through blocks), else the part. */
static int compare_model_surfaces(const model_surfaces_t *a,const model_surfaces_t *b,const model_block_t blocks[8])
{
    static const unsigned pointers[]={16,28,32,44};
    unsigned i,j,k;
    if (a->count!=b->count) return 1;
    for (i=0;i<a->count;++i) {
        const surface_t *x=a->surfaces+i,*y=b->surfaces+i;
        surface_arrays_t ax={0},ay={0}; int ok;
        for (k=0;k<84;k+=4) {
            int pointer=0; for (j=0;j<4;++j) pointer|=k==pointers[j];
            if (!pointer && memcmp(x->header+k,y->header+k,4)) return 2;
        }
        if (x->rigid_count!=y->rigid_count) return 3;
        if (x->rigid_stream && y->rigid_stream &&
            (x->rigid_stream_size!=y->rigid_stream_size || memcmp(x->rigid_stream,y->rigid_stream,x->rigid_stream_size))) return 4;
        if (!ResolveSurfaceArrays(x,blocks,&ax) || !ResolveSurfaceArrays(y,blocks,&ay)) {
            FreeSurfaceArrays(&ax); FreeSurfaceArrays(&ay); return 5;
        }
        ok=1;
        for (j=0;j<3 && ok;++j) {
            size_t n=(size_t)ax.arrays[j].count*ax.arrays[j].stride;
            ok=ax.arrays[j].count==ay.arrays[j].count && ax.arrays[j].stride==ay.arrays[j].stride &&
               (!n || (ax.arrays[j].data && ay.arrays[j].data && !memcmp(ax.arrays[j].data,ay.arrays[j].data,n)));
        }
        FreeSurfaceArrays(&ax); FreeSurfaceArrays(&ay);
        if (!ok) return 6;
    }
    return 0;
}
int LoadMountedModelSurfaces(const char *name,unsigned lod,const mounted_model_surfaces_t **out)
{
    const mounted_model_skeleton_t *model;
    xmodel_lods_t lods;
    const xmodel_lod_t *expected;
    model_cache_t *cached;
    archive_t *surface_archive;
    const char *surface_name;
    asset_t assets[4];
    mounted_model_surfaces_t *pending;
    stream_model_t stream={0};
    int status;
    if (!out || lod>=4) return -1;
    status=LoadMountedXModelSkeleton(name,&model); if (status!=1) return status;
    cached=(model_cache_t *)model;
    if (cached->surfaces[lod]) { *out=cached->surfaces[lod]; return 1; }
    if (cached->pool) { /* the lod's XModelSurfs from C1's record, written as the inline stream its loader reads */
        const uint8_t *record=pool_lod_surfaces(cached,lod); asset_t stream={0};
        if (!ReadXModelLods(&cached->source,&lods)) return -1;
        if (lod<lods.first || lod>=lods.count) return 0;
        if (!record) return lods.lods[lod].surface_count ? -1 : 0;
        if (!(pending=calloc(1,sizeof(*pending)))) return -1;
        if (!SerializeC1ModelSurfaces(record,&stream) || !ReadModelSurfaces(&stream,&pending->source) ||
            pending->source.count!=lods.lods[lod].surface_count) {
            FreeModelSurfaces(&pending->source); free((void *)stream.data); free(pending); return -1;
        }
        /* the parse borrows the stream: keep it with the cache, as streamed models are */
        cached->streams[lod].storage[0]=(uint8_t *)stream.data;
        cached->surfaces[lod]=pending; *out=pending; return 1;
    }
    if (!ReadXModelLods(&cached->source,&lods)) return -1;
    if (lod<lods.first || lod>=lods.count) return 0;
    expected=lods.lods+lod;
    if ((unsigned)expected->surface_index+expected->surface_count>model->skeleton.surfaces) return -1;
    if (!XModelInlineSurfaceAssets(&cached->source,assets)) return -1;
    if (!assets[lod].data) {
        if (!expected->surfaces_reference) return expected->surface_count ? -1 : 0;
        if (!ResolveSurfaceAlias(&cached->archive->blocks,
            expected->surfaces_reference,assets+lod)) return -1;
    }
    surface_archive=cached->archive;
    surface_name=dependency_asset_name(surface_archive,assets+lod);
    if (surface_name && *surface_name) {
        archive_t *definition=find_dependency_definition(3,surface_name+(surface_name[0]==','),assets+lod);
        if (definition) {
            surface_archive=definition;
            surface_name=dependency_asset_name(surface_archive,assets+lod);
        } else if(surface_name[0]==',') return -1;
    }
    pending=calloc(1,sizeof(*pending)); if (!pending) return -1;
    if (!ReadModelSurfaces(assets+lod,&pending->source)) { free(pending); return -1; }
    if (c1_readers && surface_name && *surface_name) { /* the same surfaces from the XModelSurfs record C1 holds */
        const uint8_t *record=C1Record(3,surface_name+(surface_name[0]==','));
        asset_t c1asset={0}; model_surfaces_t c1={0};
        if (record && SerializeC1ModelSurfaces(record,&c1asset) && ReadModelSurfaces(&c1asset,&c1)) {
            model_block_t views[8];
            LoadBlockViews(&surface_archive->blocks,views); views[0]=(model_block_t){0};
            c1_reader_report(3,surface_name,compare_model_surfaces(&pending->source,&c1,views));
            if (c1_readers==1) {
                if (!c1.name) c1.name=pending->source.name;
                FreeModelSurfaces(&pending->source); pending->source=c1;
            } else FreeModelSurfaces(&c1);
        } else c1_reader_report(3,surface_name,-1);
        free((void *)c1asset.data);
    }
    /* A pooled name borrows its retained archive, as do the array block views. */
    if (!pending->source.name && surface_name) {
        pending->source.name=surface_name;
        if (pending->source.unresolved) --pending->source.unresolved;
    }
    /* Only empty named dependencies may be replaced by streamed geometry.
     * A nonempty count mismatch is malformed, not a request for another asset. */
    if (!pending->source.count && expected->surface_count && pending->source.name && stream_loader) {
        model_surfaces_t replacement={0};
        if (stream_loader(stream_context,pending->source.name,&stream)!=1 ||
            !ReadStreamModelSurfaces(&stream,&replacement) ||
            !asset_name_equal(replacement.name,pending->source.name) ||
            replacement.count!=expected->surface_count) {
            FreeModelSurfaces(&replacement); FreeStreamModel(&stream);
            FreeModelSurfaces(&pending->source); free(pending); return -1;
        }
        FreeModelSurfaces(&pending->source); pending->source=replacement;
    }
    if (pending->source.count!=expected->surface_count) {
        FreeModelSurfaces(&pending->source); free(pending); return -1;
    }
    if (stream.storage[0]) {
        memcpy(pending->blocks,stream.blocks,sizeof(pending->blocks)); cached->streams[lod]=stream;
    } else LoadBlockViews(&surface_archive->blocks,pending->blocks);
    pending->blocks[0]=(model_block_t){0};
    cached->surface_source[lod]=assets[lod].data;
    cached->surfaces[lod]=pending; *out=pending; return 1;
}
int BindMountedModelBoneHandles(const char *name,bone_string_acquire_t acquire,
    bone_string_release_t release,void *context,const uint16_t **out,unsigned *count)
{
    const mounted_model_skeleton_t *model;
    model_cache_t *cached;
    uint16_t *handles;
    unsigned i,handle;
    int status;
    if (!acquire || !release || !out || !count) return -1;
    status=LoadMountedXModelSkeleton(name,&model);
    if (status!=1) return status;
    /* model is the first member of the private cache allocation. */
    cached=(model_cache_t *)model;
    if (cached->acquire) {
        if (cached->acquire!=acquire || cached->release!=release || cached->string_context!=context) return -1;
        *out=cached->bone_handles; *count=model->bone_names.count; return 1;
    }
    handles=calloc(model->bone_names.count ? model->bone_names.count : 1,sizeof(*handles));
    if (!handles) return -1;
    for (i=0;i<model->bone_names.count;++i) {
        handle=acquire(context,model->bone_names.names[i].text);
        if (!handle || handle>UINT16_MAX) {
            if (handle) release(context,handle);
            while (i) release(context,handles[--i]);
            free(handles); return -1;
        }
        handles[i]=(uint16_t)handle;
    }
    cached->bone_handles=handles; cached->acquire=acquire; cached->release=release; cached->string_context=context;
    *out=handles; *count=model->bone_names.count; return 1;
}
int LoadMountedCollisionXModel(const char *name,bone_string_acquire_t acquire,
    bone_string_release_t release,void *context,const XModel **out)
{
    const mounted_model_skeleton_t *skeleton;
    const mounted_model_geometry_t *geometry;
    const xmodel_materials_t *slots;
    const uint16_t *handles; unsigned count,i;
    xMaterial_t *materials[256]={0};
    model_cache_t *cached; collision_model_cache_t *pending;
    int status;
    if (!out || !acquire || !release) return -1;
    status=LoadMountedXModelSkeleton(name,&skeleton); if(status!=1) return status;
    cached=(model_cache_t *)skeleton;
    if (cached->collision_model) {
        if (BindMountedModelBoneHandles(name,acquire,release,context,&handles,&count)!=1) return -1;
        *out=&cached->collision_model->core.model; return 1;
    }
    if (LoadMountedXModelGeometry(name,&geometry)!=1 ||
        LoadMountedModelMaterials(name,&slots)!=1 || slots->count>256) return -1;
    pending=calloc(1,sizeof(*pending)); if(!pending) return -1;
    if (LoadNativeModelMesh(name,&pending->mesh)!=1) goto fail;
    for(i=0;i<pending->mesh.count;++i) {
        unsigned slot=pending->mesh.material_indices[i];
        mounted_model_material_t source; const Material_t *material;
        if(slot>=slots->count) goto fail;
        if(materials[slot]) continue;
        if(LoadMountedModelMaterialSource(name,slot,&source)!=1 ||
            LoadEngineMaterial(source.name,&material)!=1) goto fail;
        materials[slot]=(xMaterial_t *)material;
    }
    if (BindMountedModelBoneHandles(name,acquire,release,context,&handles,&count)!=1 ||
        !BuildNativeModelCore(skeleton,geometry,handles,count,&pending->core) ||
        !BindNativeModelMesh(&pending->core,&pending->mesh,materials,slots->count)) goto fail;
    cached->collision_model=pending; *out=&pending->core.model;
    /* Without surface geometry (serverSurfaceGeometry 0) the native mesh copied every surface header it
     * keeps, so the parsed surfaces and the streamed model files they came from are done with: MEASURED
     * 2026-10-28 (heap tracker), 44-90 MB of the heap after a level load (ReadStreamModel,
     * ReadModelSurfaces). A later surface lookup parses them again. */
    if (!NativeSurfaceGeometry()) {
        for (i=0;i<4;++i) if (cached->surfaces[i]) {
            FreeModelSurfaces(&cached->surfaces[i]->source); free(cached->surfaces[i]); cached->surfaces[i]=NULL;
            FreeStreamModel(&cached->streams[i]); memset(&cached->streams[i],0,sizeof(cached->streams[i]));
        }
    }
    return 1;
fail:
    FreeNativeModelCore(&pending->core); FreeNativeModelMesh(&pending->mesh);
    free(pending); return -1;
}
int LoadMountedPhysicsXModel(const char *name,bone_string_acquire_t acquire,
    bone_string_release_t release,void *context,const XModel **out)
{
    const XModel *base; const mounted_model_skeleton_t *skeleton;
    const struct PhysPreset *preset=NULL; const PhysGeomList_t *map=NULL;
    mounted_model_physics_t dependency;
    collision_model_cache_t *cached; int status;
    if (!out) return -1;
    status=LoadMountedCollisionXModel(name,acquire,release,context,&base);
    if (status!=1) return status;
    if (LoadMountedXModelSkeleton(name,&skeleton)!=1) return -1;
    cached=((model_cache_t *)skeleton)->collision_model;
    if (!cached->physics_ready) {
        if (cached->core.physics_preset_reference &&
            (LoadMountedModelPhysics(name,0,&dependency)!=1 ||
             LoadEnginePhysicsPreset(dependency.name,&preset)!=1)) return -1;
        if (cached->core.physics_map_reference &&
            (LoadMountedModelPhysics(name,1,&dependency)!=1 ||
             LoadEnginePhysicsMap(dependency.name,&map)!=1)) return -1;
        /* Publish both dependencies together; static-only users keep their view. */
        cached->physics_model=*base;
        cached->physics_model.physPreset=(struct PhysPreset *)preset;
        cached->physics_model.physGeoms=(PhysGeomList_t *)map;
        cached->physics_ready=1;
    }
    *out=&cached->physics_model; return 1;
}
int MountedAssetDefined(unsigned type, const char *name)
{
    asset_t asset;
    return name && *name && find_dependency_definition(type, name, &asset) != NULL;
}
int LoadEngineRawFile(const char *name, const engine_rawfile_t **out)
{
    archive_t *archive;
    if (!name || !out) return -1;
    for (archive = archives; archive; archive = archive->next) {
        rawfile_cache_t *cached;
        unsigned index;
        for (cached = archive->rawfiles; cached; cached = cached->next)
            if (asset_name_equal(cached->engine.name, name)) { *out = &cached->engine; return 1; }
        if (archive->c1_zone && c1_provider && c1_backed_find(archive, ASSET_RAWFILE, name)) {
            /* C1's record, copied: no mount */
            const C1RawFile *own = c1_provider(ASSET_RAWFILE, *name == ',' ? name+1 : name);
            if (own && (cached = calloc(1, sizeof(*cached)))) {
                if (copy_c1_rawfile(own, &cached->c1, &cached->c1_storage) && C1RawFileView(&cached->c1, &cached->engine)) {
                    ++c1_pool_builds[ASSET_RAWFILE];
                    cached->next = archive->rawfiles; archive->rawfiles = cached;
                    *out = &cached->engine; return 1;
                }
                free(cached->c1_storage); free(cached);
            }
        }
        if (!c1_backed_need(archive, ASSET_RAWFILE, name)) continue;
        for (index = 1; index <= archive->zone.asset_count; ++index) {
            asset_t asset;
            const char *candidate;
            uint8_t *bytes;
            size_t length;
            if (!ZoneAsset(&archive->zone, index-1, &asset)) return -1;
            if (asset.type != ASSET_RAWFILE) continue;
            if (!asset.size) continue; /* directory reference, no definition */
            if (asset.size < 16) return -1;
            if (!world_u32(asset.data)) continue; /* unnamed rawfile */
            /* C1 4c58e7..4c592c: the leading word is an XString, either
             * inline (-1) or a packed block reference. Its payload remains
             * inline regardless of the nonzero buffer word (4c592e..4c5958). */
            candidate = world_u32(asset.data) == UINT32_MAX ?
                RawFileName(&asset) :
                LoadBlockString(&archive->blocks, world_u32(asset.data));
            if (!candidate) return -1;
            if (*candidate == ',' || !asset_name_equal(candidate, name)) continue;
            (void)bytes; (void)length;
            touch_asset(archive, asset.data);
            cached = calloc(1, sizeof(*cached));
            if (!cached) return -1;
            /* The C1 record (C1 0x4c58a0) is what the server holds; the retained view is built from it. In-process,
             * a startup zone's record is C1's own. */
            {
                const C1RawFile *own = provided_record(ASSET_RAWFILE, candidate, archive);
                if (own) cached->c1 = *own;
            }
            if ((!cached->c1.buffer && !BuildC1RawFile(candidate, &asset, &cached->c1, &cached->c1_storage)) ||
                !C1RawFileView(&cached->c1, &cached->engine)) {
                free(cached->c1_storage); free(cached); return -1;
            }
            cached->next = archive->rawfiles; archive->rawfiles = cached;
            *out = &cached->engine; return 1;
        }
    }
    return 0;
}

int LoadEngineStringTable(const char *name, const engine_table_t **out)
{
    archive_t *archive;
    if (!name || !out) return -1;
    for (archive = archives; archive; archive = archive->next) {
        table_cache_t *cached;
        unsigned index;
        table_miss_t *miss;
        for (cached = archive->tables; cached; cached = cached->next)
            if (asset_name_equal(cached->engine.name, name)) { *out = &cached->engine; return 1; }
        for (miss = archive->table_misses; miss; miss = miss->next)
            if (asset_name_equal(miss->name, name)) break;
        if (miss) continue;
        if (archive->c1_zone && c1_provider && c1_backed_find(archive, ASSET_STRINGTABLE, name)) {
            const C1StringTable *own = c1_provider(ASSET_STRINGTABLE, *name == ',' ? name+1 : name);
            if (own && (cached = calloc(1, sizeof(*cached)))) {
                if (copy_c1_table(own, &cached->c1) && C1StringTableView(&cached->c1, &cached->engine)) {
                    ++c1_pool_builds[ASSET_STRINGTABLE];
                    cached->next = archive->tables; archive->tables = cached;
                    *out = &cached->engine; return 1;
                }
                free(cached->c1.values); free(cached);
            }
        }
        if (!c1_backed_need(archive, ASSET_STRINGTABLE, name)) continue;
        for (index = 1; index <= archive->zone.asset_count; ++index) {
            asset_t asset;
            const char *candidate;
            size_t count, i;
            if (!ZoneAsset(&archive->zone, index-1, &asset)) return -1;
            if (asset.type != ASSET_STRINGTABLE) continue;
            if (!asset.size) continue;
            if (asset.size < 16) return -1;
            if (!world_u32(asset.data)) continue;
            /* C1 4c5bd5..4c5c1a uses the same inline/packed XString
             * representation as rawfiles. Do not miss a table override just
             * because its name was deduplicated into persistent block 3. */
            candidate = world_u32(asset.data) == UINT32_MAX ?
                StringTableName(&asset) :
                LoadBlockString(&archive->blocks, world_u32(asset.data));
            if (!candidate) return -1;
            if (*candidate == ',' || !asset_name_equal(candidate, name)) continue;
            touch_asset(archive, asset.data);
            cached = calloc(1, sizeof(*cached));
            if (!cached) return -1;
            {
                const C1StringTable *own = provided_record(ASSET_STRINGTABLE, candidate, archive);
                if (own) {
                    cached->c1 = *own; cached->c1_borrowed = 1; /* the cells stay C1's */
                    if (!C1StringTableView(own, &cached->engine)) { free(cached); return -1; }
                    cached->next = archive->tables; archive->tables = cached;
                    *out = &cached->engine; return 1;
                }
            }
            if (!ReadStringTable(&asset, &cached->source) ||
                cached->source.columns > INT_MAX || cached->source.rows > INT_MAX) goto fail;
            /* Inline names belong to source.storage; packed names borrow the
             * archive's replayed block, which outlives this cache entry. */
            if (!cached->source.name) cached->source.name = candidate;
            count = (size_t)cached->source.rows*cached->source.columns;
            if (count && !(cached->engine.values = calloc(count, sizeof(*cached->engine.values)))) goto fail;
            for (i = 0; i < count; ++i) {
                table_cell_t *cell=cached->source.cells+i;
                /* C1 4c5b41..4c5b43 leaves a zero cell pointer untouched.
                 * Retained table queries skip NULL search cells and return
                 * an empty string when selecting one; do not turn it into
                 * a searchable empty-string key or an invalid relocation. */
                if(!cell->text && cell->reference) {
                    cell->text=LoadBlockString(&archive->blocks,cell->reference);
                    if(!cell->text || StringTableHash(cell->text)!=cell->hash) goto fail;
                }
                cached->engine.values[i]=cell->text;
            }
            cached->source.unresolved=0;
            /* The C1 record (C1 0x4c5b90: {string, hash} cells), then the retained char* view from it. */
            free(cached->engine.values); cached->engine.values = NULL;
            cached->c1.name = cached->source.name;
            cached->c1.columnCount = (int)cached->source.columns;
            cached->c1.rowCount = (int)cached->source.rows;
            if (count && !(cached->c1.values = calloc(count, sizeof(*cached->c1.values)))) goto fail;
            for (i = 0; i < count; ++i) {
                cached->c1.values[i].string = cached->source.cells[i].text;
                cached->c1.values[i].hash = (int)cached->source.cells[i].hash;
            }
            if (!C1StringTableView(&cached->c1, &cached->engine)) goto fail;
            if (com_developer && com_developer->integer > 1 && cached->source.columns) {
                size_t row;
                ((void)0);
                for (row = 0; row < cached->source.rows && row < 64; ++row) {
                    const char *text = cached->engine.values[row*cached->source.columns];
                    ((void)0);
                }
                ((void)0);
                /* Small tables in full: every cell with its inline/pooled origin. */
                if (cached->source.rows <= 8) {
                    for (row = 0; row < cached->source.rows; ++row) {
                        size_t col;
                        ((void)0);
                        for (col = 0; col < cached->source.columns; ++col) {
                            const table_cell_t *cell = cached->source.cells + row*cached->source.columns + col;
                            const char *text = cached->engine.values[row*cached->source.columns+col];
                            ((void)0);
                        }
                        ((void)0);
                    }
                }
            }
            cached->next = archive->tables; archive->tables = cached;
            *out = &cached->engine; return 1;
fail:
            FreeStringTable(&cached->source); free(cached->engine.values); free(cached->c1.values); free(cached);
            return -1; /* a broken patch must not silently expose an older table */
        }
        /* Not in this archive: remember that, so the next lookup skips it. */
        {
            size_t length = strlen(name);
            miss = malloc(sizeof(*miss) + length);
            if (miss) {
                memcpy(miss->name, name, length + 1);
                miss->next = archive->table_misses; archive->table_misses = miss;
            }
        }
    }
    return 0;
}

/* Loose-file CSV overlays, keyed by (script root, name) and never by mounted
 * generation, so nothing about a group replacement drops them. The asset
 * category selects the script root (cscr_parser.cpp:584 passes
 * ServerZoneType() into the overlay lookup), so a category switch that
 * left these resident would answer the new category from the old category's
 * loose files for the rest of the process. */
void ResetReferenceStringTables(void)
{
    while(reference_tables) {
        reference_table_t *entry=reference_tables;reference_tables=entry->next;
        free(entry->root);free(entry->name);free(entry->table.engine.values);
        FreeStringTable(&entry->table.source);free(entry);
    }
}
/* Every cache an archive owns. Split out of UnmountArchives so a single
 * retired generation can be released without tearing down the startup group.
 * Callers must first drop everything that borrows from this archive: composed
 * weapon cores, weapon sound bindings, script profiles and model animations. */
static void free_archive(archive_t *entry)
{
    while(entry->pool_blobs) { struct pool_blob_s *b=entry->pool_blobs; entry->pool_blobs=b->next; free(b); }
    free(entry->touched);
    free(entry->zone_name); free(entry->c1_zone); free(entry->c1_path); free(entry->c1_key);
    {   /* weapon record builds of this archive's definitions */
        c1_weapon_t **link=&c1_weapons;
        while(*link) {
            c1_weapon_t *w=*link;
            if(w->pool==entry ||
               (entry->zone.data && w->definition>=entry->zone.data && w->definition<entry->zone.data+entry->zone.size)) {
                *link=w->next; free_c1_weapon(w);
            } else link=&w->next;
        }
    }
    {   /* xanim streams written for this archive's definitions (their caches go with the archive) */
        c1_xanim_t **link=&c1_xanims;
        while(*link) {
            c1_xanim_t *c=*link;
            if(c->pool==entry ||
               (entry->zone.data && c->definition>=entry->zone.data && c->definition<entry->zone.data+entry->zone.size)) {
                *link=c->next; FreeC1XAnimStream(&c->stream); free(c->name); free(c);
            } else link=&c->next;
        }
    }
    {
        while(entry->sounds) {
            sound_cache_t *sound=entry->sounds; entry->sounds=sound->next;
            FreeNativeSound(&sound->native);
            FreeSoundAliasList(&sound->source); free(sound);
        }
        while(entry->effects) {
            effect_cache_t *effect=entry->effects; entry->effects=effect->next;
            while(effect->native) {
                native_effect_cache_t *native=effect->native; effect->native=native->next;
                FreeNativeEffect(&native->native); free(native);
            }
            FreeEffectSource(&effect->source); free(effect);
        }
        while (entry->models) {
            model_cache_t *model=entry->models; entry->models=model->next;
            unsigned i;
            if(model->collision_model) {
                FreeNativeModelCore(&model->collision_model->core);
                FreeNativeModelMesh(&model->collision_model->mesh);
                free(model->collision_model);
            }
            /* Release a bone-name handle only while it still names this bone.
             * A retired archive can outlive the script string generation that
             * interned the name; releasing a recycled handle decrements a
             * stranger's refcount and, once that string is freed for real,
             * SL_FreeString walks its hash chain forever (2026-09-22 verrukt
             * hang: main thread parked in SL_RemoveRefToStringOfSize called
             * from free_archive). Leaking a dead handle costs nothing. */
            if (model->release) for (i=0;i<model->model.bone_names.count;++i)
                if (EngineStringHandleIs(model->bone_handles[i],model->model.bone_names.names[i].text))
                    model->release(model->string_context,model->bone_handles[i]);
            free(model->bone_handles);
            for (i=0;i<4;++i) if (model->surfaces[i]) {
                FreeModelSurfaces(&model->surfaces[i]->source); free(model->surfaces[i]);
                FreeStreamModel(&model->streams[i]);
            }
            FreeXModelCollision(&model->geometry.collision);
            FreeXModelMaterials(&model->materials);
            FreeXModelBounds(&model->geometry.bounds);
            FreeXModelBoneNames(&model->model.bone_names);
            FreeXModelSkeleton(&model->model.skeleton); free(model->pool_header); free(model);
        }
        while(entry->weapon_tags) {
            weapon_tags_t *tags=entry->weapon_tags; unsigned i;
            entry->weapon_tags=tags->next;
            /* No names are cached here to validate against; an interned tag
             * name is a few bytes, a stale release is a hang. Keep the refs. */
            (void)i;
            free(tags);
        }
        if(entry->null_weapon) { FreeNativeWeaponCore(entry->null_weapon); free(entry->null_weapon); }
        while(entry->native_weapons) {
            native_weapon_cache_t *weapon=entry->native_weapons;
            entry->native_weapons=weapon->next;
            FreeNativeWeaponCore(&weapon->core); free(weapon);
        }
        free(entry->script_strings);
        while(entry->glass_worlds) {
            glass_world_cache_t *world=entry->glass_worlds;
            entry->glass_worlds=world->next;
            FreeGlassGroups(&world->groups); free(world);
        }
        while (entry->worlds) {
            common_world_cache_t *world=entry->worlds; entry->worlds=world->next;
            free_common_world(world);
        }
        while (entry->physics_maps) {
            physics_map_cache_t *map=entry->physics_maps; entry->physics_maps=map->next;
            free(map->c1_stream);
            FreeNativePhysicsMap(&map->native); FreePhysicsMap(&map->source); free(map);
        }
        while(entry->physics_presets) {
            physics_preset_cache_t *preset=entry->physics_presets; entry->physics_presets=preset->next;
            FreeNativePhysicsPreset(&preset->native); free(preset);
        }
        while (entry->materials) {
            material_cache_t *material=entry->materials; entry->materials=material->next;
            FreeNativeMaterial(&material->native); free(material);
        }
        while (entry->rawfiles) {
            rawfile_cache_t *raw = entry->rawfiles; entry->rawfiles = raw->next;
            free((void *)raw->engine.buffer); free(raw->c1_storage); free(raw);
        }
        while (entry->tables) {
            table_cache_t *table = entry->tables; entry->tables = table->next;
            free(table->engine.values); if (!table->c1_borrowed) free(table->c1.values);
            FreeStringTable(&table->source); free(table);
        }
        while (entry->table_misses) {
            table_miss_t *miss = entry->table_misses; entry->table_misses = miss->next;
            free(miss);
        }
        while(entry->xanim_strings) {
            xanim_strings_t *strings=entry->xanim_strings; unsigned i;
            entry->xanim_strings=strings->next;
            FreeNativeXAnim(&strings->native);
            /* Same as weapon tags: no cached names to validate, keep the refs. */
            (void)i;
            free(strings);
        }
        free(entry->dependency_index);
        FreeLoadBlocks(&entry->blocks); CloseZone(&entry->zone); free(entry);
    }
}
/* Release the archives a previous ReplaceRuntimeArchives moved aside.
 * mount_archives only unlinks the outgoing group (it cannot free it: live
 * native code still holds WeaponDef pointers into it, which is what
 * FindNativeWeaponQueryCore walks the retired list for), so without this
 * every map or category change leaves a full extra copy of the gametype and
 * map zones resident for the rest of the process. Returns how many were freed.
 *
 * PRECONDITION: no borrower of a retired archive may still be live. The
 * borrowers are ResetComposedWeapons (retired cores and their base
 * definitions), ResetNativeWeaponSounds (alias lists), the engine sound
 * channel bindings, ResetScriptProfiles and ResetModelAnimations.
 * UnloadCategoryAssets is the entry point that sequences all of them. */
unsigned ReleaseRetiredArchives(void)
{
    unsigned count=0;
    while(retired_archives) {
        archive_t *entry=retired_archives; retired_archives=entry->next;
        free_archive(entry); ++count;
    }
    return count;
}
void UnmountArchives(void)
{
    /* Composed cores borrow image/archive strings and native assets, and own
     * SL references. Release them before any of those providers disappear. */
    ResetComposedWeapons();
    /* Cached profile definitions borrow names and offset maps from these
     * archives. Invalidate them before releasing the backing storage. */
    ResetScriptProfiles();
    /* Weapon sound bindings borrow archive-owned alias lists. */
    ResetNativeWeaponSounds(); ResetC1Weapons();
    ResetReferenceStringTables();
    ReleaseRetiredArchives();
    while (archives) {
        archive_t *entry=archives; archives=entry->next;
        free_archive(entry);
    }
    ++mount_serial; C1MaterializeReset();
    unranked_rank=LINK_RANK_UNRANKED;
}
/* A LocalizeEntry string: inline (0xffffffff, the next NUL-terminated string in
 * the entry's own stream bytes) or a de-duplicated reference into runtime
 * block 3 that the replay laid out. Only a prepared archive has that block. */
static const char *localize_string(const archive_t *archive,const uint8_t **cursor,const uint8_t *end,uint32_t ref)
{
    const char *s; size_t offset;
    if(ref==UINT32_MAX) {
        s=(const char *)*cursor;
        if(*cursor>=end || !memchr(*cursor,0,(size_t)(end-*cursor))) return NULL;
        *cursor+=strlen(s)+1; return s;
    }
    if(!archive->prepared || !ref || ((ref-1)>>28)!=3) return NULL;
    offset=(ref-1)&0xfffffff;
    if(offset>=archive->blocks.written[3]) return NULL;
    s=(const char *)archive->blocks.data[3]+offset;
    return memchr(s,0,archive->blocks.written[3]-offset) ? s : NULL;
}
static uint32_t localize_u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int EnumerateMountedLocalize(int (*visit)(void *,const char *,const char *),void *context)
{
    archive_t *archive; int visited=0;
    if(!visit) return -1;
    for(archive=archives;archive;archive=archive->next) {
        const zone_t *zone=&archive->zone; uint32_t k;
        if(zone->table_offset>zone->size || zone->asset_count>(zone->size-zone->table_offset)/16) continue;
        for(k=0;k<zone->asset_count;++k) {
            const uint8_t *entry=zone->data+zone->table_offset+16*k,*p,*cursor,*end;
            uint32_t type=localize_u32(entry),position=localize_u32(entry+4)+40,extent=localize_u32(entry+12);
            const char *value,*name;
            if(type!=27 || position>zone->size || extent<8 || extent>zone->size-position) continue;
            /* C1 LocalizeEntry 0x4c1ea0 (replay_localize): value, then name. */
            p=zone->data+position; end=p+extent; cursor=p+8;
            value=localize_string(archive,&cursor,end,localize_u32(p));
            name=localize_string(archive,&cursor,end,localize_u32(p+4));
            if(!value || !name || !*name) continue;
            ++visited;
            if(!visit(context,name,value)) return visited;
        }
    }
    return visited;
}

/* sh1x_mem (physicalmemory.cpp): what the mounted archives hold, for the C1-layout conversion plan. Per C1 asset
 * type, the bytes of its serialized records in the zone images; then the images and the replayed XZoneMemory
 * blocks, which together are the bulk of the server's asset memory. */
void AssetMemoryReport(void (*print)(const char *line))
{
    static const char *const names[44]={"physpreset","phys_collmap","xanim","xmodelsurfs","xmodel","material","pixelshader",
        "vertexshader","vertexdecl","techset","image","sound","sndcurve","loaded_sound","col_map_sp","col_map_mp",
        "com_map","game_map_sp","game_map_mp","map_ents","fx_map","gfx_map","lightdef","ui_map","font","menufile",
        "menu","localize","weapon","snddriverglobals","fx","impactfx","aitype","mptype","character","xmodelalias",
        "rawfile","stringtable","leaderboard","structureddata","tracer","vehicle","addon_map_ents","weaponattach"};
    unsigned long long per_type[44]={0}, counts[44]={0}, images=0, blocks=0, fixups=0, arena=0, headers=0;
    unsigned archive_count=0;
    char line[512];
    archive_t *a;
    for(a=archives;a;a=a->next){
        unsigned i,b;
        archive_count++;
        images+=a->zone.size;
        for(b=0;b<8;b++) blocks+=a->blocks.capacity[b];
        if(a->blocks.c1){
            fixups+=a->blocks.c1->fixup_count; headers+=a->blocks.c1->header_count;
            arena+=a->blocks.c1->arena_capacity+a->blocks.c1->fixup_capacity*sizeof(fixup_t)+
                a->blocks.c1->header_capacity*sizeof(c1_header_t);
        }
        for(i=0;i<a->zone.asset_count;i++){
            asset_t asset;
            if(ZoneAsset(&a->zone,i,&asset)&&asset.type<44){ per_type[asset.type]+=asset.size; counts[asset.type]++; }
        }
    }
    snprintf(line,sizeof(line),"sh1x_mem assets: %u archives, zone images %.2f MB, replayed blocks %.2f MB",archive_count,
             images/1048576.0,blocks/1048576.0);
    print(line);
    if(free_zone_images){
        zone_image_stats_t z; unsigned i,n; const zone_image_event_t *ev=NULL;
        ZoneImageStats(&z);
        snprintf(line,sizeof(line),"sh1x_mem assets: replayed blocks in temporary files %.2f MB",FileBackedBlockBytes()/1048576.0);
        print(line);
        snprintf(line,sizeof(line),"sh1x_mem assets: zone images in read-only temporary files %.2f MB in %u zones, %.2f MB resident; "
                 "integrity: %u pages checked, %u differed",z.released/1048576.0,z.regions,z.committed/1048576.0,
                 (unsigned)z.checked_pages,(unsigned)z.modified_pages);
        print(line);
        n=ZoneImageEvents(&ev);
        for(i=0;i<n;++i) {
            int rank=0;
            for(a=archives;a;a=a->next) if(a->zone.size==ev[i].zone_size) { rank=a->rank; break; }
            snprintf(line,sizeof(line),"sh1x_mem assets:   integrity event %u: zone rank %d (%u bytes) page 0x%x differs, asset %d type %d",
                     i,rank,(unsigned)ev[i].zone_size,(unsigned)ev[i].offset,(int)ev[i].asset_index,(int)ev[i].asset_type);
            print(line);
        }
    }
    if(c1_readers){
        unsigned k;
        snprintf(line,sizeof(line),"sh1x_mem assets: C1 readers mode %d: xmodel %u converted, %u differ; xanim %u, %u differ; "
                 "clipMap %u, %u differ; weapon %u, %u differ; fx %u, %u differ; physpreset %u, %u; collmap %u, %u; material %u, %u; "
                 "xmodelsurfs %u, %u; comworld %u, %u; glass map %u, %u; glass groups %u, %u; addon ents %u, %u",c1_readers,c1_reader_count[4],c1_reader_mismatch[4],c1_reader_count[2],
                 c1_reader_mismatch[2],c1_reader_count[15],c1_reader_mismatch[15],c1_reader_count[28],c1_reader_mismatch[28],
                 c1_reader_count[30],c1_reader_mismatch[30],c1_reader_count[0],c1_reader_mismatch[0],
                 c1_reader_count[1],c1_reader_mismatch[1],c1_reader_count[5],c1_reader_mismatch[5],
                 c1_reader_count[3],c1_reader_mismatch[3],c1_reader_count[16],c1_reader_mismatch[16],
                 c1_reader_count[18],c1_reader_mismatch[18],c1_reader_count[20],c1_reader_mismatch[20],
                 c1_reader_count[42],c1_reader_mismatch[42]);
        print(line);
        for(k=0;k<8 && c1_reader_firsts[k][0];++k) {
            snprintf(line,sizeof(line),"sh1x_mem assets: C1 reader differs: %s",c1_reader_firsts[k]); print(line);
        }
    }
    {
        unsigned backed=0; archive_t *e;
        for(e=archives;e;e=e->next) if(e->c1_zone) ++backed;
        if(backed || c1_backed_fallbacks) {
            snprintf(line,sizeof(line),"sh1x_mem assets: zones taken from C1: %u live, %u fell back to the server's copy; "
                     "C1 pool records read: %u weapon, %u rawfile, %u stringtable, %u clipMap, %u xanim, %u xmodel, %u material, %u physics, %u sound, %u curve, %u vehicle, %u fx, %u linked",backed,c1_backed_fallbacks,
                     c1_pool_builds[28],c1_pool_builds[36],c1_pool_builds[37],c1_pool_builds[15],c1_pool_builds[2],c1_pool_builds[4],
                     c1_pool_builds[5],c1_pool_builds[0]+c1_pool_builds[1],c1_pool_builds[11],c1_pool_builds[12],c1_pool_builds[41],c1_pool_builds[30],c1_pool_builds[5]+c1_pool_builds[6]+
                     c1_pool_builds[7]+c1_pool_builds[8]+c1_pool_builds[9]+c1_pool_builds[10]+c1_pool_builds[4]+c1_pool_builds[30]);
            print(line);
        }
    }
    if(c1_provider){
        snprintf(line,sizeof(line),"sh1x_mem assets: taken from C1: %u sound, %u rawfile, %u stringtable",
                 c1_provided[11],c1_provided[36],c1_provided[37]);
        print(line);
    }
    if(fixups||headers){
        snprintf(line,sizeof(line),"sh1x_mem assets: C1 fix-ups %llu, header copies %llu, fix-up state %.2f MB, records %.2f MB",
                 fixups,headers,arena/1048576.0,C1MaterializedBytes()/1048576.0);
        print(line);
    }
    for(unsigned t=0;t<44;t++) if(counts[t]){
        snprintf(line,sizeof(line),"sh1x_mem assets type %2u %-16s %6llu records %9.2f MB serialized",t,names[t],counts[t],per_type[t]/1048576.0);
        print(line);
    }
}

/* The server's C1-layout records (c1_records.h): looked up through the retained loaders, which build
 * and cache the record with its view. */
int LoadC1RawFile(const char *name,const C1RawFile **out)
{
    const engine_rawfile_t *view; int status;
    if(!out) return -1;
    status=LoadEngineRawFile(name,&view); if(status!=1) return status;
    *out=&((const rawfile_cache_t *)view)->c1; return 1;
}
int LoadC1StringTable(const char *name,const C1StringTable **out)
{
    const engine_table_t *view; int status;
    if(!out) return -1;
    status=LoadEngineStringTable(name,&view); if(status!=1) return status;
    /* Reference CSV tables (LoadReferenceStringTable) are not archive tables and have no C1 record. */
    *out=&((const table_cache_t *)((const char *)view-offsetof(table_cache_t,engine)))->c1; return 1;
}
/* Every mounted named definition of a rawfile (36) or string table (37), newest archive first, link
 * references excluded. */
int EnumerateMountedNamed(unsigned type,int (*visit)(void *,const char *),void *context)
{
    archive_t *archive; int visited=0;
    if(!visit || (type!=ASSET_RAWFILE && type!=ASSET_STRINGTABLE)) return -1;
    for(archive=archives;archive;archive=archive->next) {
        unsigned index;
        for(index=0;index<archive->zone.asset_count;++index) {
            asset_t asset; const char *name;
            if(!ZoneAsset(&archive->zone,index,&asset)) return -1;
            if(asset.type!=type || asset.size<16 || !world_u32(asset.data)) continue;
            name=world_u32(asset.data)==UINT32_MAX ?
                (type==ASSET_RAWFILE ? RawFileName(&asset) : StringTableName(&asset)) :
                LoadBlockString(&archive->blocks,world_u32(asset.data));
            if(!name || !*name || *name==',') continue;
            ++visited;
            if(!visit(context,name)) return visited;
        }
    }
    return visited;
}

unsigned AssetMountSerial(void) { return mount_serial; }
/* Every mounted named definition of an asset type the replay catalogues (asset aliases), newest archive first,
 * link references excluded. */
int EnumerateMountedType(unsigned type,int (*visit)(void *,const char *),void *context)
{
    archive_t *archive; int visited=0;
    if(!visit) return -1;
    for(archive=archives;archive;archive=archive->next) {
        size_t i;
        for(i=0;i<archive->blocks.asset_alias_count;++i) {
            const asset_t *asset=&archive->blocks.asset_aliases[i].asset;
            const char *name;
            if(asset->type!=type) continue;
            name=dependency_asset_name(archive,asset);
            if(!name || !*name || name[0]==',') continue;
            ++visited;
            if(!visit(context,name)) return visited;
        }
    }
    return visited;
}

/* C1-layout header copies (c1Fixups): the linked definition of (type,name) as C1 holds it after DB_AddXAsset,
 * with its pointer words still serialized; ResolveC1Pointer gives what each pointer word holds in C1. */
const void *FindC1Header(unsigned type,const char *name,unsigned *length)
{
    asset_t asset; archive_t *archive; const c1_header_t *h;
    if(!name || !*name) return NULL;
    c1_probe=1; c1_probe_hit=NULL; archive=find_dependency_definition(type,name,&asset); c1_probe=0;
    if(!archive) return NULL;
    if(!(h=C1Header(&archive->blocks,asset.data))) return NULL;
    if(length) *length=h->length;
    return OL_BlockAddress(&archive->blocks,h->header);
}
/* The pointer C1 stores in the word at addr (a word of a header copy or of the data it reaches): NULL when C1
 * stores none there. kind: FIXUP_DATA (size: bytes placed), FIXUP_ASSET | type << 8 (an asset header
 * copy), FIXUP_SOUND_NAME (the alias name C1 looks the snd_alias_list_t up by). Alias words are followed. */
const void *ResolveC1Pointer(const void *addr,unsigned *kind,unsigned *size)
{
    archive_t *archive;
    for(archive=archives;archive;archive=archive->next) {
        fixup_t f; unsigned hops;
        if(!FixupAt(&archive->blocks,addr,&f)) continue;
        for(hops=0;hops<4 && f.kind==FIXUP_ALIAS;++hops) {
            const uint8_t *word=OL_BlockAddress(&archive->blocks,f.target);
            if(!word || !FixupAt(&archive->blocks,word,&f)) return NULL;
        }
        if(f.kind==FIXUP_SOUND_NAME) {
            const uint8_t *word=OL_BlockAddress(&archive->blocks,f.target); fixup_t n;
            if(!word || !FixupAt(&archive->blocks,word,&n)) return NULL;
            if(kind) *kind=FIXUP_SOUND_NAME;
            if(size) *size=n.size;
            return OL_BlockAddress(&archive->blocks,n.target);
        }
        if(f.kind==FIXUP_ALIAS) return NULL;
        if(kind) *kind=f.kind;
        if(size) *size=f.size;
        return OL_BlockAddress(&archive->blocks,f.target);
    }
    return NULL;
}
/* Offsets (from addr) of the pointer words C1 relocates inside [addr, addr+length) of a header copy or the data it
 * reaches, ascending, at most max; returns how many there are (may exceed max), -1 when addr is not replay memory. */
int C1PointersIn(const void *addr,unsigned length,unsigned *offsets,unsigned max)
{
    const uint8_t *p=addr; archive_t *archive;
    for(archive=archives;archive;archive=archive->next) {
        const load_blocks_t *s=&archive->blocks; const c1_state_t *c=s->c1;
        uint32_t reference=0; size_t lo=0,hi; unsigned i,n=0;
        if(!c) continue;
        for(i=0;i<8 && !reference;++i)
            if(s->data[i] && p>=s->data[i] && p<s->data[i]+s->capacity[i]) reference=((uint32_t)i<<28)+(uint32_t)(p-s->data[i])+1;
        if(!reference && c->arena && p>=c->arena && p<c->arena+c->arena_size) reference=(8u<<28)+(uint32_t)(p-c->arena)+1;
        if(!reference) continue;
        hi=c->fixup_count;
        while(lo<hi) { size_t mid=lo+(hi-lo)/2; if(c->fixups[mid].slot<reference) lo=mid+1; else hi=mid; }
        for(;lo<c->fixup_count && c->fixups[lo].slot-reference<length && c->fixups[lo].slot>=reference;++lo) {
            if(n<max && offsets) offsets[n]=c->fixups[lo].slot-reference;
            ++n;
        }
        return (int)n;
    }
    return -1;
}
int FindC1Definition(unsigned type,const char *name,const load_blocks_t **blocks,uint32_t *header,
    uint32_t *length)
{
    asset_t asset; archive_t *archive; const c1_header_t *h;
    if(!name || !*name) return 0;
    /* a definition in a zone C1 holds has no server copy to materialize (and a lookup must not mount it) */
    c1_probe=1; c1_probe_hit=NULL; archive=find_dependency_definition(type,name,&asset); c1_probe=0;
    if(!archive) return 0;
    if(!(h=C1Header(&archive->blocks,asset.data))) return 0;
    if(blocks) *blocks=&archive->blocks;
    if(header) *header=h->header;
    if(length) *length=h->length;
    return 1;
}
const char *BlocksScriptString(const load_blocks_t *blocks,unsigned index)
{
    archive_t *archive;
    for(archive=archives;archive;archive=archive->next) if(&archive->blocks==blocks) {
        if(!archive->script_strings && !ReadScriptStrings(&archive->zone,&archive->script_strings)) return NULL;
        return index<archive->zone.string_count ? archive->script_strings[index].text : NULL;
    }
    return NULL;
}

void C1ReplaceMaterial(native_material_t *material)
{
    const uint8_t *record; native_material_t c1={0}; const char *name;
    if(!c1_readers || !material || !(name=material->material.info.name)) return;
    if(!(record=C1Record(5,name)) || !BuildC1NativeMaterial(record,&c1)) { c1_reader_report(5,name,-1); return; }
    c1_reader_report(5,name,CompareNativeMaterials(material,&c1));
    if(c1_readers==1) { FreeNativeMaterial(material); *material=c1; }
    else FreeNativeMaterial(&c1);
}

static c1_zone_host_t c1_zone_host;
static int c1_zone_host_set;
void SetC1ZoneHost(const c1_zone_host_t *host)
{
    if(host) { c1_zone_host=*host; c1_zone_host_set=1; } else c1_zone_host_set=0;
}
const c1_zone_host_t *C1ZoneHost(void) { return c1_zone_host_set ? &c1_zone_host : NULL; }

/* sh1x_unused: per mounted zone and asset type, the directory assets and their serialized bytes, against those some
 * reader resolved or read since the mount (touch_asset). Directory assets only: an inline definition counts as its
 * container. */
static const char *unused_type_name(unsigned t)
{
    static const char *const names[44]={"physpreset","phys_collmap","xanim","xmodelsurfs","xmodel","material",
        "pixelshader","vertexshader","vertexdecl","techset","image","sound","sndcurve","loaded_sound","col_map_sp",
        "col_map_mp","com_map","game_map_sp","game_map_mp","map_ents","fx_map","gfx_map","lightdef","ui_map","font",
        "menufile","menu","localize","weapon","snddriverglobals","fx","impactfx","aitype","mptype","character",
        "xmodelalias","rawfile","stringtable","leaderboard","structureddata","tracer","vehicle","addon_map_ents",
        "weaponattach"};
    return t<44 ? names[t] : "?";
}
void UnusedAssetReport(void (*print)(const char *line),int per_zone)
{
    unsigned long long total_count[44]={0},total_bytes[44]={0},used_count[44]={0},used_bytes[44]={0};
    unsigned long long all=0,all_used=0; archive_t *a; char line[300]; unsigned t;
    for(a=archives;a;a=a->next) {
        const zone_t *z=&a->zone; unsigned long long zc[44]={0},zb[44]={0},uc[44]={0},ub[44]={0},zall=0,zused=0; uint32_t k;
        if(!z->data || z->table_offset>z->size || z->asset_count>(z->size-z->table_offset)/16) continue;
        for(k=0;k<z->asset_count;++k) {
            const uint8_t *e=z->data+z->table_offset+16*k; uint32_t type=dir_u32(e),ext=dir_u32(e+12);
            int used=a->touched && (a->touched[k>>3]>>(k&7)&1);
            if(type>=44 || !ext) continue;
            ++zc[type]; zb[type]+=ext; zall+=ext;
            if(used) { ++uc[type]; ub[type]+=ext; zused+=ext; }
        }
        for(t=0;t<44;++t) { total_count[t]+=zc[t]; total_bytes[t]+=zb[t]; used_count[t]+=uc[t]; used_bytes[t]+=ub[t]; }
        all+=zall; all_used+=zused;
        if(per_zone) {
            snprintf(line,sizeof(line),"sh1x_unused zone %s (rank %d): %.2f MB serialized, %.2f MB touched (%.0f%%)",
                     a->zone_name ? a->zone_name : "?",a->rank,zall/1048576.0,zused/1048576.0,zall ? 100.0*zused/zall : 0.0);
            print(line);
            for(t=0;t<44;++t) if(zb[t]>=262144) {
                snprintf(line,sizeof(line),"sh1x_unused   %-16s %6llu assets %8.2f MB, touched %6llu %8.2f MB",unused_type_name(t),
                         zc[t],zb[t]/1048576.0,uc[t],ub[t]/1048576.0);
                print(line);
            }
        }
    }
    snprintf(line,sizeof(line),"sh1x_unused all zones: %.2f MB serialized, %.2f MB touched (%.0f%%)",all/1048576.0,
             all_used/1048576.0,all ? 100.0*all_used/all : 0.0);
    print(line);
    for(t=0;t<44;++t) if(total_count[t]) {
        snprintf(line,sizeof(line),"sh1x_unused type %-16s %6llu assets %8.2f MB, touched %6llu %8.2f MB, untouched %8.2f MB",
                 unused_type_name(t),total_count[t],total_bytes[t]/1048576.0,used_count[t],used_bytes[t]/1048576.0,
                 (total_bytes[t]-used_bytes[t])/1048576.0);
        print(line);
    }
}

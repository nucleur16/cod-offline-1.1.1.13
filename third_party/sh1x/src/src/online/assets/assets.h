/* Owned CoDOL archive images. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ASSETS_H
#define ASSETS_H
#include <stddef.h>
#include <stdint.h>
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
#define ASSET_MAP_ENTS 19
#define ASSET_RAWFILE 36
#define ASSET_STRINGTABLE 37
/* IW4 asset type 42. The nine coop zones ship exactly one each and no world
 * asset at all, so it is the only place their Spec Ops entities exist. NOTE
 * this is the CoDOL/IW4 zone numbering that load_blocks.c dispatches on,
 * NOT the IW3 XAssetType enum in src/xassets.h that g_assetNames, g_poolSize
 * and DB_XAssetPool are indexed by -- the two disagree from index 1 onwards. */
#define ASSET_ADDON_MAP_ENTS 42
#define ASSET_LIMIT (512u * 1024u * 1024u)

typedef struct {
    uint8_t *data;
    size_t size, table_offset;
    uint32_t asset_count, string_count, block_size[8];
} zone_t;

typedef struct {
    uint32_t type;
    const uint8_t *data;
    size_t size;
} asset_t;

/* Open copies/owns the zone image. Failed opens leave the output untouched. */
int OpenZoneImage(zone_t *out, const void *data, size_t size);
int OpenZoneFile(zone_t *out, const char *path);
void CloseZone(zone_t *zone);
int ZoneAsset(const zone_t *zone, unsigned index, asset_t *asset);
typedef struct {
    uint32_t reference;
    const char *text;
} script_string_t;
/* Read the zone-local script-string index table. The returned array is owned
 * by the caller (free); text points into the owned zone until CloseZone.
 * Preserve null and unresolved references as NULL, with their original value;
 * neither is interchangeable with an explicitly empty inline string.
 * Failed reads leave out untouched. An empty pool succeeds with *out=NULL. */
int ReadScriptStrings(const zone_t *zone, script_string_t **out);
typedef struct {
    uint32_t name_reference;
    const char *name;
    float origin[3];
    uint16_t trigger_index;
    uint8_t sun_index;
} map_stage_t;
typedef struct {
    uint8_t *storage;
    const char *name, *text;
    uint32_t name_reference;
    size_t text_size, consumed;
    uint32_t model_count, hull_count, slab_count, stage_count;
    const uint8_t *models, *hulls, *slabs;
    map_stage_t *stages;
} map_entities_t;
/* Read a MapEnts header at an already resolved stream position. The 44-byte
 * header precedes optional inline name and entity text; text_size includes its
 * NUL. Also reads trigger models (8 bytes), hulls (32), slabs (20), and stages.
 * A pooled name stays NULL with its reference preserved. Owns copied strings;
 * failure leaves out untouched. This function does not scan collision blobs. */
int ReadMapEntities(const void *data, size_t size, map_entities_t *out);
/* Same record with a 36-byte header and no stages: IW4 asset type 42, the only
 * asset the nine coop zones ship. MEASURED 2026-09-20: name pointer +0 (inline
 * for all nine), entityString +4 (always 0xFFFFFFFF), numEntityChars +8, then
 * trigger model/hull/slab count+pointer pairs to +35, and for every one of the
 * nine the decoded length equals the asset extent exactly. */
int ReadAddonMapEntities(const void *data, size_t size, map_entities_t *out);
/* Compose an addon overlay onto a base map's MapEnts: base entity text first,
 * then the addon's minus its leading worldspawn, with every addon "?N" trigger
 * reference biased by the base trigger model count and the addon's trigger
 * models/hulls/slabs appended (and their first/start indices rebased) so one
 * contiguous array serves both halves. The addon must carry no stages. Owns
 * its own storage; out is freed and replaced only on success. */
int MergeMapEntities(const map_entities_t *base,
                           const map_entities_t *addon,
                           map_entities_t *out);
void FreeMapEntities(map_entities_t *entities);
typedef struct { float normal[3],distance; } trigger_plane_t;
typedef struct {
    float mins[3],maxs[3];
    int32_t contents;
    unsigned plane_count;
    trigger_plane_t *planes;
} trigger_hull_t;
typedef struct {
    float mins[3],maxs[3];
    int32_t contents;
    unsigned hull_count;
    trigger_hull_t *hulls;
} trigger_model_t;
/* Decode one ?N trigger model into owned convex hulls. Each slab becomes two
 * outward planes; six axial planes retain each hull's box bounds. Coordinates
 * remain in the source trigger frame. Failure preserves out. No engine handle
 * is installed; this is geometry for native trigger collision integration. */
int ReadTriggerModel(const map_entities_t *,unsigned,trigger_model_t *);
void FreeTriggerModel(trigger_model_t *);
/* Walk the measured collision stream to its inline MapEnts record. Does not
 * search for text or use map-specific offsets; output is untouched on failure.
 * Inline static XModels still require their own loader and are rejected. */
int LocateCollisionMapEntities(const asset_t *asset, size_t *offset);
int ReadCollisionMapEntities(const asset_t *asset, map_entities_t *out);
enum collision_section {
    COLL_PLANES, COLL_STATIC_MODELS, COLL_MATERIALS,
    COLL_BRUSH_SIDES, COLL_BRUSH_EDGES, COLL_NODES,
    COLL_LEAVES, COLL_LEAF_BRUSHES, COLL_LEAF_BRUSH_NODES,
    COLL_LEAF_SURFACES, COLL_VERTICES, COLL_TRIANGLES,
    COLL_WALKABLE, COLL_BORDERS, COLL_PARTITIONS,
    COLL_AABBS, COLL_SUBMODELS, COLL_BRUSHES,
    COLL_BRUSH_BOUNDS, COLL_BRUSH_CONTENTS, COLL_STATIC_NODES,
    COLL_SECTIONS
};
typedef struct {
    size_t offset; /* asset-relative; SIZE_MAX for externally pooled planes */
    uint32_t count, stride, reference;
} collision_span_t;
typedef struct {
    collision_span_t sections[COLL_SECTIONS];
    size_t map_entities;
    uint32_t name_reference;
} collision_layout_t;
/* Validated stream spans, not native pointers or relocated engine geometry.
 * Requires the source asset to remain alive. Failure leaves out untouched. */
int ReadCollisionLayout(const asset_t *asset, collision_layout_t *out);
/* Returns inline name, or NULL for a deduplicated name requiring block fixups. */
const char *RawFileName(const asset_t *asset);
/* Caller frees the returned payload with free(). Always appends a sentinel NUL;
 * size excludes that sentinel, and compressed length must match exactly. */
int ReadRawFile(const asset_t *asset, uint8_t **out, size_t *size);
int FindRawFile(const zone_t *zone, const char *name,
                      uint8_t **out, size_t *size);
/* 1=loaded, 0=no matching inline name, -1=matching asset invalid/unreadable.
 * Outputs remain untouched unless loaded. Pooled names need relocation. */
int FindRawFileStatus(const zone_t *zone, const char *name,
                           uint8_t **out, size_t *size);
typedef struct {
    uint32_t reference, hash;
    /* NULL with reference 0 is an absent cell; NULL with a nonzero reference
     * needs relocation. Neither is an inline empty string. */
    const char *text;
} table_cell_t;
typedef struct {
    uint8_t *storage;
    const char *name;
    uint32_t name_reference, values_reference, columns, rows, unresolved;
    table_cell_t *cells;
} stringtable_t;
/* Owned structural reader; inline strings are hash-checked. Pooled references
 * stay explicit until block relocation resolves them. No hash-based guessing. */
int ReadStringTable(const asset_t *asset, stringtable_t *out);
uint32_t StringTableHash(const char *text);
const char *StringTableName(const asset_t *asset);
void FreeStringTable(stringtable_t *table);
const table_cell_t *StringTableCell(const stringtable_t *table,
                                               unsigned row, unsigned column);

/* IWff0100 signed-block and IWffu100 unsigned-stream containers. Key material
 * is read from the user's local C1.exe; no key is embedded or written to disk. */
int OpenFastFile(zone_t *out, const char *path, const char *client_exe);
/* Reason the last OpenFastFile returned 0 (static text). */
const char *FastFileOpenFailure(void);

/* Archive registry used by the native script loader. Mounts persist until
 * shutdown so returned data is never tied to a transient database buffer. */
int MountArchive(const char *path, const char *client_exe);
/* Atomic set: input order is ascending override priority (last wins).
 * All archives are validated before publication. Failure leaves existing
 * mounts/caches untouched; failed_index identifies the rejected input. */
int MountArchives(const char *const *paths, size_t count,
                        const char *client_exe, size_t *failed_index);
int LoadScriptAsset(const char *name, uint8_t **out, size_t *size);
int LoadScriptAssetStatus(const char *name, uint8_t **out, size_t *size);
/* Layout consumed by retained IW3 StringTable routines; strings/pointers live
 * until UnmountArchives. 1=ready, 0=not found, -1=present but not resolvable. */
typedef struct {
    const char *name;
    int columnCount, rowCount;
    const char **values;
} engine_table_t;
int LoadEngineStringTable(const char *name, const engine_table_t **out);
int ReadCSVStringTable(const char *data,size_t size,stringtable_t *out);
int LoadReferenceStringTable(const char *root,const char *name,const engine_table_t **out);
/* Retained IW3 RawFile layout. Decoded bytes may contain NULs; len is exact,
 * with an additional sentinel. Cache ownership/lifetime matches tables. */
typedef struct {
    const char *name;
    int len;
    const char *buffer;
} engine_rawfile_t;
int LoadEngineRawFile(const char *name, const engine_rawfile_t **out);
/* 1 when a mounted archive defines an asset of `type` named `name` (not a ',' stub) */
int MountedAssetDefined(unsigned type, const char *name);
/* Attaches the lobby-served secure profile for a connecting client (profilePath, userinfo xuid). */
int AutoBindClientSecureProfile(unsigned client,const char *userinfo);
struct ComWorld;
/* Owned cached world/lights/strings, retained until full unmount. 1/0/-1 for
 * ready/absent/invalid; output unchanged on failure. No global publication. */
int LoadEngineCommonWorld(const char *,const struct ComWorld **);
/* Replayed serialized weapon view, not an IW3 WeaponDef. The four leading
 * string references are resolved; all other pointer fields remain serialized.
 * Borrowed until full unmount, including retired groups. 1/0/-1 status; output
 * unchanged on failure. Never publish asset.data as a native weapon. */
typedef struct {
    asset_t asset;
    const char *strings[4];
    size_t dependency_offset; /* First inline model after leading strings. */
} mounted_weapon_t;
int LoadMountedWeapon(const char *,mounted_weapon_t *);
struct WeaponDef;
/* Complete native 'none' definition, archive-owned through full unmount.
 * Other names/absent root return 0; malformed none returns -1. Atomic output. */
int LoadMountedNullWeapon(const char *,const struct WeaponDef **);
typedef struct { const char *names[32]; } weapon_hide_tags_t;
/* Resolve source +0x3c's 32 script-string indices using the defining archive.
 * Zero is absent. Preserve all slots, including after gaps, for composition.
 * Names borrow archive/retired-group lifetime; no native handles are acquired.
 * 1=ready, 0=absent weapon, -1=invalid; output is unchanged on failure. */
int LoadMountedWeaponHideTags(const char *,weapon_hide_tags_t *);
typedef struct { const char *name; asset_t asset; } weapon_model_t;
/* Resolves one of the eleven leading model slots (source +0x10..+0x38),
 * including inline definitions, pooled aliases and newest named overrides.
 * 1=ready, 0=null slot/absent weapon, -1=invalid; output unchanged unless ready.
 * These are source slots; their native gun/hand/world roles need separate mapping. */
int LoadMountedWeaponModel(const char *,unsigned,weapon_model_t *);
typedef struct {
    const char *names[144];
    size_t dependency_offset; /* After names, before inline flash effects. */
} weapon_animations_t;
/* Resolves the complete serialized animation-name block after inline models.
 * Borrowed until full unmount. Preserves source slot order and null/empty names;
 * it does not map 144 source slots onto IW3's 33 animation slots or load XAnims.
 * 1=ready, 0=absent weapon, -1=invalid; failure leaves out unchanged. */
int LoadMountedWeaponAnimations(const char *,weapon_animations_t *);
typedef struct {
    const char *names[2];
    asset_t assets[2];
    size_t dependency_offset; /* After leading effects, before sound names. */
} weapon_effects_t;
/* Resolves source +0x35c/+0x360, including inline/pooled FX and named overrides.
 * Null slots stay null. Borrowed until full unmount. Source views, not native FX.
 * 1=ready, 0=absent weapon, -1=invalid; output unchanged unless ready. */
int LoadMountedWeaponEffects(const char *,weapon_effects_t *);
typedef struct {
    const char *names[53];
    const char *arrays[2][31];
    size_t dependency_offset; /* After both sound arrays, before further FX. */
} weapon_sounds_t;
/* Resolves indirect sound-name slots +0x364..+0x434 and arrays +0x438/+0x43c.
 * Borrowed source names,
 * not native sound aliases. 1/0/-1; failed calls preserve the whole output. */
int LoadMountedWeaponSounds(const char *,weapon_sounds_t *);
typedef struct {
    const char *ammo,*clip,*shared_cap;
    size_t dependency_offset;
} weapon_ammo_names_t;
/* Traverses intervening FX/materials and resolves +0x564/+0x56c/+0x57c.
 * Borrowed until full unmount; 1/0/-1, failure preserves out. No registration. */
int LoadMountedWeaponAmmoNames(const char *,weapon_ammo_names_t *);
enum { WEAPON_DEP_STRING, WEAPON_DEP_SOUND, WEAPON_DEP_ASSET, WEAPON_DEP_POINTS };
typedef struct {
    unsigned offset,kind;
    const char *name;
    asset_t asset;
    const uint8_t *points; /* Serialized little-endian float pairs. */
    unsigned point_count;
} weapon_dependency_t;
typedef struct {
    weapon_dependency_t entries[14]; /* Four FX, then ten materials. */
    size_t dependency_offset;
} weapon_visuals_t;
/* Resolves +0x440..+0x450 and the intervening material groups through +0x550.
 * Original offsets and null slots are retained. Named overrides apply after
 * advancing over inline source data. Borrowed lifetime; atomic 1/0/-1. */
int LoadMountedWeaponVisuals(const char *,weapon_visuals_t *);
typedef struct {
    weapon_dependency_t entries[64];
    unsigned count;
    size_t dependency_offset;
} weapon_tail_t;
/* Resolves the remaining dependencies after ammo names in loader order.
 * Includes null slots; offset identifies the original header field. Named
 * assets use the winning mounted definition. Borrowed through retired archive
 * lifetime; 1/0/-1, atomic output, exact final source extent required. */
int LoadMountedWeaponTail(const char *,weapon_tail_t *);
typedef struct {
    asset_t asset;
    const char *name;       /* Borrowed archive string. */
    const uint8_t *entries; /* count serialized {u32 offset,u32 type,u32 value}. */
    uint32_t count;
    const char **texts;     /* Owned array; borrowed strings, null where absent. */
} mounted_weapon_attachment_t;
/* Type 43 weaponattach definition in C1 loader order (0x4c3dd0, 0x4c3d00):
 * the twelve-byte header, its name, count twelve-byte entries, then one string
 * per entry whose kind carries one. Entry values stay serialized and are never
 * native pointers; strings borrow archive lifetime through retired groups and
 * the texts array is owned by the caller. Requires the exact source extent.
 * 1/0/-1, atomic output; the overrides themselves are applied elsewhere. */
int LoadMountedWeaponAttachment(const char *,mounted_weapon_attachment_t *);
void FreeMountedWeaponAttachment(mounted_weapon_attachment_t *);
typedef struct {
    const uint8_t *header,*metadata,*indices,*frames;
    unsigned count,frame_stride;
} xanim_track_t;
typedef struct {
    asset_t asset;
    const char *name;
    const uint8_t *bone_indices,*notifies,*streams[6],*indices;
    unsigned bone_count,notify_count,index_width;
    uint32_t stream_counts[6],index_count;
    xanim_track_t tracks[3]; /* translation, two-component quat, four-component quat */
    const script_string_t *script_strings;
    unsigned script_string_count;
} mounted_xanim_t;
/* Complete source traversal, not native XAnimParts. All views borrow archive
 * lifetime, including retired groups. Preserves all three source delta tracks.
 * 1=ready, 0=absent, -1=invalid; failed calls preserve out. */
int LoadMountedXAnim(const char *,mounted_xanim_t *);

typedef struct {
    asset_t asset;
    const char *name,*prefix_strings[2]; /* Header8 andac; semantics not assumed. */
    asset_t physics_preset; /* Zeroed for a null headerb0 dependency. */
    const char *physics_preset_name,*physics_profile;
    const char *string190,*string238;
    asset_t weapon,materials[2];
    const char *weapon_name,*material_names[2];
    const char *sounds[47]; /* 1ac/1b0, fourteen sparse fields, then23c[31]. */
    size_t dependency_offset;
} mounted_vehicle_t;
/* VehicleDef41 has a serialized0x314-byte header. Prefix strings occur at
 * header offsets0,8,ac, in that order (C1 replay4c4ec0). Header references
 * remain serialized, never native pointers. This is an archive-owned view;
 * Resolves the physics preset atb0 and profile string atb4; dependency_offset
 * follows the complete dependency stream. Resolves weapon/material definitions
 * and all47 sound-name slots in loader order. Native simulation is separate.
 * 1=found,0=absent,-1=invalid; failure preserves out. */
int LoadMountedVehicle(const char *,mounted_vehicle_t *);
typedef struct {
    uint16_t bones[255],notifies[255];
    unsigned bone_count,notify_count;
} xanim_handles_t;
typedef unsigned (*xanim_string_acquire_t)(void *,const char *);
typedef void (*xanim_string_release_t)(void *,unsigned);
typedef struct { uint16_t tags[32]; } weapon_hide_tag_handles_t;
/* Acquires one reference per populated slot, including duplicate names. Cached
 * by defining archive and callbacks/context until full unmount; callers must
 * keep the callback context alive that long. Zero slots stay zero. Does not
 * truncate to IW3 WeaponDef.hideTags[8]. Output is unchanged on failure.
 * Returns 1 ready, 0 absent, -1 invalid or acquisition failure. */
int BindMountedWeaponHideTags(const char *,xanim_string_acquire_t,
    xanim_string_release_t,void *,const weapon_hide_tag_handles_t **);
/* Native engine SL provider; returned references remain archive-owned. */
int LoadEngineWeaponHideTags(const char *,const weapon_hide_tag_handles_t **);
/* Owned SL references for an already-resolved (possibly composed) tag view.
 * out must be zeroed or a previous owned result. Acquires one reference per
 * populated slot, including duplicates. Failure rolls back new references
 * and preserves out; success releases its old references and replaces it.
 * Returns 1 ready, -1 invalid/acquisition failure. Release before SL shutdown.
 * Do not pass archive-borrowed handles to the release function. */
int AcquireEngineWeaponHideTags(const weapon_hide_tags_t *,weapon_hide_tag_handles_t *);
void ReleaseEngineWeaponHideTags(weapon_hide_tag_handles_t *);
/* Resolves all zone-local names before acquiring native script-string refs.
 * Cached per asset/provider; survives replacement until full unmount, which
 * releases every acquisition. Callbacks must not change mounted archives.
 * 1/0/-1; failure preserves out and rolls back acquired references. */
int BindMountedXAnimHandles(const char *,xanim_string_acquire_t,
    xanim_string_release_t,void *,const xanim_handles_t **);
int LoadEngineXAnimHandles(const char *,const xanim_handles_t **);
void UnmountArchives(void);
/* Free the archives a previous group replacement moved aside. Nothing else
 * frees them: mount_archives only unlinks the outgoing group because live
 * native code still holds WeaponDef pointers into it. Every borrower listed on
 * UnloadCategoryAssets must be reset first. Returns how many were freed. */
unsigned ReleaseRetiredArchives(void);
/* 1 when the engine script-string handle still reads exactly `name` (so a
 * cached reference to it is live), 0 for a dead or recycled handle. */
int EngineStringHandleIs(unsigned handle,const char *name);
/* Move a mount group aside without mounting a replacement, so the outgoing
 * asset category can be unloaded before the next one is opened. group 0 (the
 * startup set) is refused. Returns how many archives were retired. */
unsigned RetireRuntimeArchiveGroup(unsigned group);
/* SV_SetCategoryTeardown entry point, called BEFORE the new category
 * mounts: retires the live group-1 archives and then unloads everything that
 * belonged to `previous_category`. */
void UnloadGametypeAssets(const char *previous_category,const char *next_category);
/* SV_SetSessionMapInstaller entry point: the playlist/session spelling of
 * this match's map ("" or NULL when there is none, which is every mp and zm
 * entry). Consulted by the two-argument LoadMapArchives. */
void SetSessionMapName(const char *session_map);
const char *SessionMapName(void);
/* Drop the loose-file CSV overlay cache. Keyed by (script root, name) and not
 * by mounted generation, so a category switch must drop it explicitly. */
void ResetReferenceStringTables(void);
/* Forget the map/session/category the group-1 reuse fast path remembers, so
 * the next LoadMapArchives remounts even for the same name. Call it
 * whenever the mounted set is torn down behind that cache's back. */
void ForgetMountedMapArchives(void);
/* Retire every piece of native state that belongs to the asset category
 * `from_type` ("mp", "coop" or "zm"), then free the retired archives, so a
 * handoff into another category leaves nothing of the old one resident or
 * reachable. Safe to call with a NULL/unknown from_type (it then only reports
 * differently). Not a no-op: callers gate it on an actual category change. */
void UnloadCategoryAssets(const char *from_type);
void InitAssetCommands(void);
int LoadStartupArchives(void);
/* The asset category a map name PROVES, or NULL when the name proves nothing.
 * C1 0x612850 derives its own category from a case-insensitive prefix test on
 * the map/gametype it was handed -- "zm" (two characters), then "coop" (four),
 * everything else "mp" -- but only the first two are load-bearing: a coop
 * match's LEVEL zone is spelled mp_* (MEASURED: the coop playlist entry
 * coop_training_mp_killhouse_sh loads the level zone mp_killhouse_sh), so an
 * mp_* name is genuinely ambiguous between "mp" and "coop" and this returns
 * NULL for it. Callers that need a default may use "mp"; callers that need to
 * detect a contradiction must treat NULL as "no evidence", not as "mp". */
const char *ZoneTypeForMap(const char *map);
/* Mount the group-1 archives for one match.
 *   map          the LEVEL zone spelling, what the client DB_LoadXFile's last
 *                (mp_killhouse_sh, zm_zombie_training, mp_crash_tropical).
 *   session_map  the playlist/session spelling when it differs from `map`
 *                (coop_training_mp_killhouse_sh); NULL or equal to `map` when
 *                there is no separate spelling, which is every mp and zm entry.
 *   zone_type    the asset category carrier ("mp", "coop" or "zm"). This is
 *                the authority; the map name is only used to refuse a
 *                contradiction it can actually prove.
 * MEASURED (dw-capture.log:2232, the client's own ClientLoadTime telemetry):
 * a coop match loads BOTH spellings -- "2005 common_mp | 1324
 * common_gametype_coop | 1720 coop_training_mp_killhouse_sh | 4956
 * mp_killhouse_sh" -- and coop_training_mp_killhouse_sh.ff is 41,861,397 bytes
 * carrying 44 of its own weapon assets, so the session zone is real content,
 * not a loadscreen stub. */
int LoadMapArchivesEx(const char *map,const char *session_map,const char *zone_type);
/* Retained two-argument form: no separate session spelling. */
int LoadMapArchives(const char *map,const char *zone_type);
int BuildZonePath(const char *name,char *out,size_t capacity);

/* Bounded zlib stream helper shared with the fastfile reader. */
int OL_Inflate(const uint8_t *in, size_t input_size, uint8_t **out,
                  size_t *output_size, size_t exact_size, int padded);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

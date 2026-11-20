#ifndef LOAD_BLOCKS_H
#define LOAD_BLOCKS_H
#include "ol_xmodel.h"
#pragma pack(push, 8)
enum { LOAD_STACK=64 };
typedef struct {
    uint32_t reference;
    asset_t asset;
} asset_alias_t;
/* reference==0 catalogues an unaliased inline dependency for named lookup. */
typedef struct {
    const uint8_t *collision,*records;
    uint32_t category,count,reference;
} dynamic_records_t;
/* Serialized span of an alias array or sound file, keyed by the block 3
 * address its reconstruction received. count is the alias count for arrays. */
typedef struct {
    const uint8_t *source;
    uint32_t reference,size,count;
} sound_span_t;
/* GameWorldMp source ownership and named glass groups. group==UINT32_MAX
 * marks the world itself (also emitted when its glass pointer is null).
 * Other records preserve group order; IDs are serialized little-endian words.
 * name_reference is retained for pooled names, inline_name for inline names. */
typedef struct {
    const uint8_t *world,*glass,*ids;
    const char *inline_name;
    uint32_t group,name_reference;
    uint16_t count;
    /* UINT32_MAX world marker: 12-byte server records.
     * UINT32_MAX-1 FxWorld marker: 52-byte initial glass placements. */
    const uint8_t *pieces;
    uint32_t piece_count;
} glass_span_t;
/* C1 pointer fix-ups (feat/iw4-asset-layout). Each relocated pointer word C1's loader writes, recorded during
 * the apply pass of a complete-zone replay when collect_fixups is set: slot and target are encoded addresses
 * ((block << 28 | offset) + 1); block 8 is the header arena, where every asset header is copied when its
 * loader finishes (C1 copies a header out of the temporary block 0 into its pool, DB_AddXAsset). kind: */
enum { FIXUP_DATA=1,      /* target is data the loader placed (string, array, record) */
       FIXUP_ASSET=2,     /* target is an asset header (an inline asset, in the arena); kind bits 8..15: its type (0xff unknown) */
       FIXUP_ALIAS=3,     /* target is a block-3 word holding the asset pointer (DB_ConvertOffsetToAlias) */
       FIXUP_SOUND_NAME=4,/* target is a word holding a sound alias name; C1 stores the alias list */
       FIXUP_ASSET_NAME=7,/* target is a name; C1 stores the header of that type (bits 8..15) it finds by it */
       /* annotations, not pointers (target == slot, size: bytes): */
       FIXUP_SCRIPT_STRINGS=5, /* uint16 zone script-string indices C1 converts to its own handles */
       FIXUP_RUNTIME=6         /* bytes C1 writes while it loads or registers the asset */ };
typedef struct {
    uint32_t slot,target,size; /* size: bytes of the target the loader placed (DATA/ASSET), else 0 */
    uint32_t kind;
} fixup_t;
typedef struct {
    const uint8_t *source;   /* the asset's serialized start (asset_t.data) */
    uint32_t header,length;  /* arena reference and size of its C1 header copy */
    uint32_t type;
} c1_header_t;
typedef struct {
    const uint8_t *source;
    uint32_t length,reference;
} replay_chunk_t;
typedef struct {
    uint32_t target,size;   /* a placement: data the loader placed and a field points at (from its link) */
} c1_placement_t;
/* Where each serialized byte of a zone went (freeZoneImages): every read reservation of the complete-zone
 * replay copies source bytes verbatim, so the image is the chunks in source order. location is an encoded block
 * address, or (14 << 28 | offset) + 1 into shadow for block 0, which the loader reuses. */
typedef struct {
    uint32_t source,length,location;
} image_chunk_t;
typedef struct image_map_s {
    image_chunk_t *chunks;       /* ascending source */
    size_t count,capacity;
    uint8_t *shadow;                   /* block-0 reads, which later assets overwrite */
    size_t shadow_size,shadow_capacity;
    size_t lost;                       /* bytes not kept (location 0): loaded-sound payloads, which the server never reads */
    int failed,skip;                   /* skip: the reads in progress are not kept */
} image_map_t;
typedef struct c1_state_s {
    fixup_t *fixups;
    size_t fixup_count,fixup_capacity;
    replay_chunk_t *chunks;      /* read reservations of the current top-level asset pass */
    size_t chunk_count,chunk_capacity;
    uint8_t *arena;                    /* block 8: C1 header copies */
    size_t arena_size,arena_capacity;
    c1_header_t *headers;        /* sorted by source after the replay */
    size_t header_count,header_capacity;
    c1_placement_t *placements;  /* sorted by target, disjoint; built after the replay */
    size_t placement_count;
    int failed;                        /* an allocation failed: the fix-ups are incomplete and unused */
} c1_state_t;
typedef struct {
    uint8_t *data[8];
    size_t capacity[8],position[8],written[8];
    const uint8_t *source;
    size_t source_size,source_position;
    unsigned active,depth;
    struct { unsigned block; size_t entered; } stack[LOAD_STACK];
    asset_alias_t *asset_aliases;
    size_t asset_alias_count,asset_alias_capacity;
    int collect_asset_aliases; /* private to the complete-zone transaction */
    uint32_t directory_reference;
    dynamic_records_t *dynamic_records;
    size_t dynamic_record_count,dynamic_record_capacity;
    sound_span_t *sound_records;
    size_t sound_record_count,sound_record_capacity;
    glass_span_t *glass_records;
    size_t glass_record_count,glass_record_capacity;
    /* C1 pointer fix-ups of a complete-zone replay (c1_state_t, shared by every plan copy; NULL when not
     * collected). last_*: the most recent reservation and loaded asset header, for the field helpers. */
    struct c1_state_s *c1;
    struct image_map_s *image;   /* complete-zone replays with image collection, shared like c1 */
    uint32_t last_reference,last_length,last_header;
} load_blocks_t;
/* The C1 header copy of the asset whose serialized view starts at source, or NULL. Collected replays only. */
const c1_header_t *C1Header(const load_blocks_t *,const uint8_t *source);
/* Absolute address of an encoded reference (blocks 0..7, arena 8), or NULL. */
uint8_t *OL_BlockAddress(const load_blocks_t *,uint32_t reference);
/* The fix-up whose slot is at addr, if any. 1 found. */
int FixupAt(const load_blocks_t *,const void *addr,fixup_t *out);
/* The placement containing the encoded address reference, or NULL. */
const c1_placement_t *C1Placement(const load_blocks_t *,uint32_t reference);
/* The first fix-up whose slot is at or after reference, and how many follow below reference+length. */
const fixup_t *FixupsIn(const load_blocks_t *,uint32_t reference,uint32_t length,size_t *count);
/* Process-wide switch: record the image map in later complete-zone replays (freeZoneImages). */
void SetCollectImageMap(int);
/* Rebuilds [offset, offset+length) of the zone image from the replay; 1 when every byte is covered. Bytes the map
 * does not keep come from lost_source when given (verification), else are zero; *lost counts them. */
int ReconstructImage(const load_blocks_t *,size_t offset,uint8_t *dst,size_t length,
    const uint8_t *lost_source,size_t *lost);
/* Process-wide switch: collect C1 fix-ups in later complete-zone replays (c1Fixups). */
void SetCollectFixups(int);
/* Borrowed 92-byte dynamic-entity records from this collision asset's complete
 * replay. Owner identity prevents borrowing records from another zone.
 * Encoded references are not native pointers. Output unchanged on failure. */
int ResolveDynamicRecords(const load_blocks_t *,const asset_t *,unsigned,
    dynamic_records_t *);
/* Complete replay retains typed source views for material insertion slots,
 * directory entries and inline model slots. All inline material/surface/model assets
 * are also catalogued by source view for named lookup. Other asset families'
 * pointer-slot aliases are not captured yet. These are never native pointers.
 * 1=found, 0=absent/invalid reference. Output unchanged unless found. */
int ResolveMaterialAlias(const load_blocks_t *,uint32_t,asset_t *);
int ResolveWeaponAlias(const load_blocks_t *,uint32_t,asset_t *);
/* Type 3 surface assets use insertion slots and directory entries. */
int ResolveSurfaceAlias(const load_blocks_t *,uint32_t,asset_t *);
/* Type 4 XModels: directory entries, insertion slots, graphics-world
 * static-model draw-instance, dynamic-definition and effect-visual fields. */
int ResolveModelAlias(const load_blocks_t *,uint32_t,asset_t *);
/* Type 30 FX: directory entries, insertion slots and dynamic-definition fields. */
int ResolveEffectAlias(const load_blocks_t *,uint32_t,asset_t *);
/* Model/material aliases include inline FX single visuals, visual arrays and
 * both material pointers in decal pairs. Sound/runner strings and light
 * visuals are not treated as material pointers. */
/* Type 0 preset or type 1 collision map. Presets additionally catalogue
 * inline pointer fields in glass definitions and dynamic-entity definitions. */
int ResolvePhysicsAlias(const load_blocks_t *,unsigned type,uint32_t,asset_t *);
/* Type 11 alias lists, 12 curves and 13 loaded sounds, from directory entries,
 * weapon/effect pointer slots and inline nested sound-file references. */
int ResolveSoundAlias(const load_blocks_t *,unsigned type,uint32_t,asset_t *);
/* Serialized span behind a deduplicated alias array or sound file pointer.
 * Lets the reader replay loaded sounds and inline curves the loader wrote into
 * the rewound scratch block. 1=found, 0=absent; output unchanged if absent. */
int ResolveSoundSpan(const load_blocks_t *,uint32_t,asset_t *,unsigned *count);
/* Caller keeps source alive. Capacities describe loader memory, not file
 * extents. Individual failed operations leave state/output unchanged. */
int InitLoadBlocks(load_blocks_t *,const uint8_t *,size_t,const uint32_t [8],unsigned);
void FreeLoadBlocks(load_blocks_t *);
int PushLoadBlock(load_blocks_t *,unsigned);
int PopLoadBlock(load_blocks_t *);
/* Alignment affects memory only. Block 2 is zero-filled without reading input.
 * Returns the encoded block reference, never a serialized-file offset. */
int ReadLoadBlock(load_blocks_t *,size_t,unsigned,uint32_t *);
/* DB_InsertPointer reserves four aligned bytes in block 3, with no file read. */
int InsertLoadPointer(load_blocks_t *,uint32_t *);
void LoadBlockViews(const load_blocks_t *,model_block_t [8]);
/* Borrowed NUL-terminated XString in persistent block 3. Rejects sentinels,
 * scratch/runtime blocks and references beyond replayed bytes (including
 * zero-filled allocator capacity). NULL means unresolved/invalid. */
const char *LoadBlockString(const load_blocks_t *,uint32_t);
/* Caller has entered temporary block 0 for the asset reference. Replays the
 * header and skeletal prefix, then leaves block 3 pushed for remaining model
 * dependencies. Inline references become their reconstructed locations.
 * Failure preserves memory, cursors and output. Does not finish an XModel. */
int ReplayXModelSkeleton(load_blocks_t *,xmodel_skeleton_t *);
/* Type 0 PhysPreset or type 1 PhysCollmap. Caller has entered block 0.
 * Returns to block 0 with dependencies in block 3. Atomic on failure.
 * Copies serialized records; engine pointer patching is a later step. */
int ReplayPhysicsAsset(load_blocks_t *,unsigned,uint32_t *);
/* XModelSurfs: records/trees in block 3, vertices in 6, indices in 7.
 * Same caller/atomicity contract as physics replay; creates no GPU objects. */
int ReplayModelSurfaces(load_blocks_t *,uint32_t *);
/* Image starts in block 0; water starts in block 3. Nested image references
 * use temporary block 0 and preserve insertion slots for -2 references. */
int ReplayImage(load_blocks_t *,uint32_t *);
int ReplayWater(load_blocks_t *,uint32_t *);
int ReplayTechniqueSet(load_blocks_t *,uint32_t *);
int ReplayMaterial(load_blocks_t *,uint32_t *);
/* Complete XModel and inline dependencies. Starts/ends in block 0, with
 * validated serialized extent and an atomic block-memory preflight. */
int ReplayXModel(load_blocks_t *,uint32_t *);
int ReplayAnimation(load_blocks_t *,uint32_t *);
/* FX records and inline model/material dependencies; no runtime FX lookup. */
int ReplayEffect(load_blocks_t *,uint32_t *);
/* Types 11 alias list, 12 curve, 13 loaded sound; includes serialized audio
 * payloads without audio-device initialization or runtime pointer patching. */
int ReplaySoundAsset(load_blocks_t *,unsigned,uint32_t *);
/* GfxWorld header, skies, BSP, cell trees and portals only. Starts in block 0
 * and leaves block 3 entered for draw data. Atomic on failure; does not finish
 * the world or install runtime visibility. */
int ReplayWorldVisibility(load_blocks_t *,uint32_t *);
/* Extends the same prefix through draw data and the light grid, leaving block
 * 3 entered for world models. Includes visibility; starts in block 0. No GPU calls. */
int ReplayWorldDraw(load_blocks_t *,uint32_t *);
/* Extends the draw prefix through scene allocations, shadow geometry and
 * light regions. Leaves block 3 entered for static/dynamic DPVS data. */
int ReplayWorldScene(load_blocks_t *,uint32_t *);
/* Complete serialized GfxWorld with dependencies and runtime allocations.
 * Starts/ends in block 0; does not install engine world pointers. */
int ReplayWorld(load_blocks_t *,uint32_t *);
/* Entity text, trigger geometry and map stages. Serialized replay only;
 * ReadMapEntities separately validates and owns runtime-facing data. */
int ReplayMapEntities(load_blocks_t *,uint32_t *);
/* Complete clipMap serialized replay, including MapEnts and dynamic assets.
 * Starts/ends in block 0; owned collision conversion/installation is separate. */
int ReplayCollision(load_blocks_t *,uint32_t *);
/* Menu statement trees in block 3; bounded recursion, strings and nested
 * inline statements. Atomic on failure; does not evaluate or finish a menu. */
int ReplayMenuExpression(load_blocks_t *,uint32_t *);
/* Menu event sets and tagged conditional/local-variable payloads in block 3.
 * Reconstructs data with bounded recursion; never executes event scripts. */
int ReplayMenuEvents(load_blocks_t *,uint32_t *);
/* Initialize directory/string-pool replay from an opened zone. Zone must
 * outlive the stream. Declared capacities are rounded to allocator pages. */
int BeginZoneReplay(const zone_t *,load_blocks_t *);
/* Ordered dispatch: 1 replayed (including measured zero-payload no-load
 * families), 0 unsupported type, -1 invalid/cursor mismatch. Never skips an
 * unknown or nonempty unimplemented asset. Failure leaves memory/cursors
 * untouched. */
int ReplayZoneAsset(const zone_t *,unsigned,load_blocks_t *);
/* Complete transaction. Source zone outlives the result; failure leaves out
 * untouched. failed_asset is UINT32_MAX for initialization failure. */
int ReplayZone(const zone_t *,load_blocks_t *,unsigned *failed_asset);
/* Production mounts retain reconstructed blocks until unmount. Archive input
 * order and atomicity match MountArchives; no runtime pointer publication yet. */
int MountRuntimeArchives(const char *const *,size_t,const char *,size_t *,unsigned *);
/* Atomically replace a nonzero logical group after complete validation. Old
 * group assets stop participating in lookup. Previously borrowed objects remain
 * owned until UnmountArchives, including across subsequent group replacements. */
int ReplaceRuntimeArchives(const char *const *,size_t,const char *,unsigned,size_t *,unsigned *);
/* C1 load-order ranks. C1's DB_LinkXAssetEntry keeps the first definition of a
 * name, so the mounted set is ordered by when C1 loads each zone (ascending
 * rank, searched first) rather than by when the server mounted it. A patch_<x>
 * zone loads just ahead of <x>. Unranked mounts sit ahead of all ranked ones,
 * newest first. */
#define LINK_RANK_UNRANKED 0
#define LINK_RANK_FFOTD 100          /* ffotd_client: overrides what loaded before it */
#define LINK_RANK_PATCH_FOLDER 200   /* Kernel33 client_shipRetail/patch, ahead of stock */
#define LINK_RANK_STARTUP 1000       /* code_post_gfx set (C1 0x598482) */
#define LINK_RANK_COMMON 2000        /* common_mp set (level load, first) */
#define LINK_RANK_GAMETYPE 3000      /* common_gametype_<category> set */
#define LINK_RANK_SESSION 4000       /* co-op playlist zone */
#define LINK_RANK_LEVEL 5000         /* the level zone */
/* Ranked forms: ranks[i] belongs to paths[i]. */
int MountRankedArchives(const char *const *,const int *,size_t,const char *,size_t *,unsigned *);
int ReplaceRankedArchives(const char *const *,const int *,size_t,const char *,unsigned,size_t *,unsigned *);
/* Borrowed reconstructed views, in lookup order (first wins). Returns 0 for missing or
 * source-only mounts. Block 0 is scratch, never a persistent asset object. */
unsigned MountedArchiveCount(unsigned group);
int MountedBlockViews(unsigned,model_block_t [8]);
#pragma pack(pop)
#endif

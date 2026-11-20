#ifndef COLLISION_NATIVE_H
#define COLLISION_NATIVE_H
#include "collision.h"
#include "cm_local.h"
#include "dynents_native.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* Owned native brush/BSP data, including the extra brush, index and leaf-brush
 * node required by CM_InitBoxHull. brush_count excludes the extra box brush;
 * brush_node_count/index_count include their extra entries. No pointer borrows
 * decoded staging. This is not a complete clipMap or visibility installation. */
typedef struct {
    uint32_t plane_count,side_count,edge_count,node_count,brush_count;
    uint32_t brush_node_count,index_count;
    cplane_t *planes;
    cbrushside_t *sides;
    char *edges;
    cNode_t *nodes;
    cbrush_t *brushes;
    void *brush_storage;
    cLeafBrushNode_t *brush_nodes;
    uint16_t *indices;
    cmodel_t box_model;
} native_brushes_t;
/* Source must be decoded/validated collision staging. Validates/remaps all
 * borrowed pointer spans and preserves out on failure. out is zero or a prior
 * result; success frees/replaces it. Source may be freed after success. */
int BuildNativeCollisionBrushes(const collision_brushes_t *,
    const collision_brush_tree_t *,native_brushes_t *);
void FreeNativeCollisionBrushes(native_brushes_t *);
typedef struct {
    uint32_t vertex_count,triangle_count,walkable_words,border_count,partition_count,aabb_count;
    float (*vertices)[3];
    uint16_t (*triangles)[3];
    uint32_t *walkable;
    CollisionBorder_t *borders;
    CollisionPartition_t *partitions;
    CollisionAabbTree_t *aabbs;
} native_terrain_t;
/* Owned native terrain, with partition border pointers relocated. Source is
 * validated decoded staging; failure preserves out, success replaces it.
 * No visibility or engine-global collision state is installed. */
int BuildNativeCollisionTerrain(const collision_terrain_t *,native_terrain_t *);
void FreeNativeCollisionTerrain(native_terrain_t *);
typedef struct {
    uint32_t leaf_count,model_count;
    cLeaf_t *leaves;
    cmodel_t *models;
    uint32_t surface_count;
    uint32_t *surfaces;
} native_leaves_t;
/* Owned leaf/inline-model records. Validates references against the source
 * terrain/tree counts and keeps cluster values unchanged; no PVS is invented.
 * The temporary box handle is reserved and cannot become an inline model.
 * Failure preserves out; success frees/replaces a zero/prior result. */
int BuildNativeCollisionLeaves(const collision_leaves_t *,uint32_t,uint32_t,
    native_leaves_t *);
void FreeNativeCollisionLeaves(native_leaves_t *);
typedef struct {
    uint32_t material_count,model_count;
    dmaterial_t *materials;
    cStaticModel_t *models;
} native_collision_assets_t;
/* Owns native material and static-model records. XModels remain borrowed from
 * the resolver/archive and must outlive this result. Rejects unresolved data.
 * Atomic replacement; no global clipmap or spatial sectors are installed. */
int BuildNativeCollisionAssets(const collision_materials_t *,
    const collision_static_models_t *,native_collision_assets_t *);
void FreeNativeCollisionAssets(native_collision_assets_t *);
typedef struct {
    const char *name;
    uint32_t cluster_count,cluster_bytes;
    int vised;
    const uint8_t *visibility;
    size_t visibility_size;
    const int16_t *leaf_clusters;
    uint32_t leaf_count;
} clip_map_metadata_t;
typedef struct {
    clipMap_t map;
    native_brushes_t brushes;
    native_terrain_t terrain;
    native_leaves_t leaves;
    native_collision_assets_t assets;
    native_dynamic_entities_t dynamics;
    unsigned trigger_model_base,trigger_model_count;
} native_clip_map_t;
/* An mp match's maps/mp/<map>.d3dbsp.ents from the script roots, validated
 * against the zone's entity text (crc32) and trigger model count; NULL when
 * absent or refused.  Returns a malloc'd NUL-terminated string, *size with
 * the NUL (map_ents_override.c). */
char *LoadMapEntsOverride(const char *map,const char *zone_text,size_t zone_text_size,
    uint32_t trigger_models,size_t *size);
/* Append trigger hull brushes and inline collision models before publishing cm. */
int AppendNativeTriggers(const map_entities_t *,native_clip_map_t *);
int TriggerClipHandle(unsigned);
/* 1 when a clip handle is one of the server-only trigger models appended
 * above; C1's clipmap has no cmodel for it. */
int IsTriggerClipHandle(int handle);
/* Inline (brush) models the CLIENT's clipmap carries: cm.numSubModels minus
 * the appended trigger models.  The bound for any published brush index. */
unsigned ClientInlineModelCount(void);
/* Assembles owned native clipmap storage from validated staging and explicit
 * visibility metadata and the staged collision checksum. XModel/FX/preset bindings remain borrowed.
 * No checksum, leaf clusters or PVS are fabricated. Success replaces out;
 * the native fields do not borrow source buffers. Does not publish cm,
 * initialize thread data or activate physics. Callers must separately retain
 * source MapEnts extensions for CoDOL trigger/stage startup; those extensions
 * are not represented by IW3 MapEnts or consumed by this constructor. */
int BuildNativeClipMap(const collision_map_t *,const dynamic_entities_t *,
    const dynamic_binding_t *const [2],const clip_map_metadata_t *,native_clip_map_t *);
void FreeNativeClipMap(native_clip_map_t *);
/* Explicit conservative server policy for maps without translated PVS:
 * all leaves share cluster 0, with an all-visible row and vised=false.
 * This disables spatial visibility culling; it is not recovered CoDOL PVS.
 * Other construction/ownership requirements are identical to BuildNativeClipMap. */
int BuildUnculledNativeClipMap(const collision_map_t *,const dynamic_entities_t *,
    const dynamic_binding_t *const [2],const char *,native_clip_map_t *);
/* Engine dependency adapter: resolves this mounted map's dynamic models, FX
 * graphs and presets, then builds the explicit unculled native map above.
 * source must be the same mounted map's collision staging. Caller retains its
 * MapEnts extensions for startup. SL must be initialized. 1=ready, 0=absent,
 * -1=invalid/unresolved; failure preserves out. Does not install or activate cm. */
int BuildEngineClipMap(const char *,const collision_map_t *,native_clip_map_t *);
/* Installs a fully constructed maps/mp/<name>.d3dbsp in cm. Retains source
 * MapEnts extensions, but does not initialize threads or activate physics. */
int InstallEngineClipMap(const char *);
void ShutdownEngineClipMap(void);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

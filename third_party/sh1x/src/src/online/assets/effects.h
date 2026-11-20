#ifndef EFFECTS_H
#define EFFECTS_H
#include "ol_xmodel.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char *name;
    uint32_t flags,total_size;
    int32_t looping_life;
    uint32_t counts[3],count;
    const uint8_t *elements; /* contiguous 260-byte source records */
    size_t dependency_offset;
} effect_prefix_t;
/* Borrowed source header/elements from a replayed effect, not native FxElemDef.
 * Resolves inline/pooled names, checks counts and source spans, and preserves
 * out on failure. Nested samples/visuals/extensions follow dependency_offset
 * and still require decoding/relocation. Asset and block views must outlive out. */
int ReadEffectPrefix(const asset_t *,const model_block_t [8],effect_prefix_t *);
typedef struct {
    uint32_t reference;
    asset_t inline_asset; /* model/material; zero for pooled references */
    const char *string; /* sound/runner visual */
    const char *name;   /* from a C1 record: the model/material visual's name (resolved by name), else NULL */
} effect_visual_t;
typedef struct {
    const uint8_t *record,*velocity_samples,*visual_samples;
    unsigned velocity_count,visual_sample_count,visual_count;
    effect_visual_t *visuals;
    const char *effect_names[3],*extra_name; /* +256: C1 spawn-sound XString */
    const uint8_t *extension,*trail_vertices,*trail_indices;
    uint32_t extension_size,trail_vertex_count,trail_index_count;
} effect_element_t;
typedef struct {
    effect_prefix_t prefix;
    effect_element_t *elements;
    void *owned; /* from a C1 record: the element records and extensions, copied */
} effect_source_t;
/* Complete serialized traversal, including sample arrays, visual dependencies,
 * effect-name references and extension records. Owns descriptor arrays only;
 * payloads/strings borrow asset and blocks. No native pointers are fabricated.
 * out is zero/prior; success replaces it, failure preserves it. */
int ReadEffectSource(const asset_t *,const model_block_t [8],effect_source_t *);
void FreeEffectSource(effect_source_t *);
/* Canonical named definition, newest first. Cached source survives replacement
 * until full unmount. 1=decoded, 0=absent, -1=invalid; out unchanged unless ready. */
int LoadMountedEffectSource(const char *,const effect_source_t **);
typedef mounted_model_material_t effect_visual_source_t;
/* Resolve a material/model visual through the effect's owning archive, then
 * current named overrides. Decal visual indices address both materials.
 * Returns 0 for null/non-asset visuals or absent effect, -1 for invalid/missing
 * dependencies, 1 for a borrowed source (block 0 excluded). Not a native handle. */
int LoadMountedEffectVisual(const char *,unsigned element,unsigned visual,effect_visual_source_t *);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

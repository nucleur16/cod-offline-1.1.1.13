#ifndef EFFECTS_NATIVE_H
#define EFFECTS_NATIVE_H
#include "effects.h"
#include "xassets/fx.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    /* Decals flatten each two-material mark into adjacent entries. */
    const void *const *visuals;
    unsigned visual_count;
    struct FxEffectDef *effects[3];
} effect_bindings_t;
typedef struct {
    FxEffectDef *effect;
    void *storage;
    size_t size;
    uint32_t source_total_size;
} native_effect_t;
/* Owns a contiguous native definition, samples, visual arrays and strings.
 * Material/model/effect dependencies are real borrowed native objects.
 * The server build retains every C1 element type (0..14) in native C1
 * elements, including the owned spawn-sound string, trailing scalar bytes and
 * the +244 extension (type 5 trail with its vertex/index arrays, type 8 spark
 * fountain, one byte otherwise). The IW3-layout build keeps its narrower
 * set (0, 1, 3, 4, 6, 9, 10, 12, 13, 14, no extensions); anything else fails
 * explicitly. No DB publication.
 * out is zero/prior; failure preserves it, success replaces it. */
int BuildNativeEffect(const effect_source_t *,const effect_bindings_t *,native_effect_t *);
void FreeNativeEffect(native_effect_t *);
/* Why the last BuildNativeEffect refused its source (element, source
 * type, check). Diagnostic text only; borrowed, replaced by the next failure. */
const char *NativeEffectBuildFailure(void);
typedef const struct XModel *(*effect_model_resolver_t)(void *,const char *);
/* Atomically constructs the supported nested effect graph, including cycles.
 * Material handles use the native archive cache; model handles come from the
 * provider and must remain valid until unmount. Provider must not alter mounts.
 * Cached by mounted generation/provider; older pointers survive until unmount.
 * 1=ready, 0=absent root, -1=invalid/unresolved. Output unchanged unless ready.
 * This does not register the effect in the engine's global asset database. */
int LoadMountedNativeEffect(const char *,effect_model_resolver_t,void *,const FxEffectDef **);
/* The graph member and check behind the last -1 from the loader above.
 * Diagnostic text only; empty after a success. */
const char *NativeEffectFailureDetail(void);
/* Engine SL adapter with native physics-model dependencies; no activation. */
int LoadEngineNativeEffect(const char *,const FxEffectDef **);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

#ifndef PHYSICS_NATIVE_H
#define PHYSICS_NATIVE_H
#include "physics.h"
#include "xassets/phys.h"
#include "xassets/xmodel.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* Owns decoded strings and retains all extension bytes. Only IW3's common
 * parameters and cylinder fallback are translated; the additional CoDOL
 * simulation parameters are not yet applied. Not published to the engine. */
typedef struct {
    struct PhysPreset preset;
    physics_preset_t source;
} native_physics_preset_t;
int BuildNativePhysicsPreset(const mounted_model_physics_t *,native_physics_preset_t *);
void FreeNativePhysicsPreset(native_physics_preset_t *);
/* Newest mounted definition, cached until unmount (including retired groups).
 * 1=ready, 0=absent, -1=invalid. Output unchanged unless ready. */
int LoadEnginePhysicsPreset(const char *,const struct PhysPreset **);
typedef struct {
    PhysGeomList_t map;
    char *name;
    float source_bounds[6];
    void **brush_storage;
} native_physics_map_t;
/* Owned map: source type 1 box -> IW3 1, type 5 cylinder -> IW3 4,
 * and type 0 brush -> owned IW3 brush, planes, sides and adjacency. Retains source
 * mass, transforms, and map bounds; does not activate a physics body.
 * Initialize out to zero. Success frees/replaces it; failure preserves it. */
int BuildNativePhysicsMap(const physics_map_t *,native_physics_map_t *);
void FreeNativePhysicsMap(native_physics_map_t *);
int LoadEnginePhysicsMap(const char *,const PhysGeomList_t **);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

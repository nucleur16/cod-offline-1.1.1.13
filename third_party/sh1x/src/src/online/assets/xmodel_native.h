#ifndef XMODEL_NATIVE_H
#define XMODEL_NATIVE_H
#include "ol_xmodel.h"
#include "xsurface.h"
#include "xassets/xmodel.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* Construction state, not a publishable asset. Skeleton, strings and bone
 * bounds and collision triangle arrays are borrowed and must outlive this core.
 * Collision surface descriptors and any
 * bound material-handle array are owned; mesh geometry/material assets are borrowed.
 * Complete header metadata and material/LOD/physics binding before installing
 * model in the engine. Fields outside this core deliberately remain zero. */
typedef struct {
    XModel model;
    uint32_t physics_preset_reference,physics_map_reference;
} native_model_core_t;
int BuildNativeModelCore(const mounted_model_skeleton_t *,
    const mounted_model_geometry_t *,const uint16_t *,unsigned,
    native_model_core_t *);
void FreeNativeModelCore(native_model_core_t *);
/* Native SL system must be initialized. Returns 1/0/-1 for ready/absent/invalid.
 * Borrowed mounted data remains valid until archive unmount; free the core first. */
int LoadNativeModelCore(const char *,native_model_core_t *);
/* Cached dedicated collision model: native skeleton, triangle descriptors,
 * active LOD mesh and dedicated material handles. The owning model generation
 * retains all dependencies and bone-string references until full unmount,
 * including after map-group replacement. Output unchanged on failure.
 * 1=ready, 0=absent, -1=invalid/unresolved or changed string provider.
 * For static collision consumers only: general DB lookup uses the physics
 * view below. Never use this as a graphical asset. */
int LoadMountedCollisionXModel(const char *,bone_string_acquire_t,
    bone_string_release_t,void *,const XModel **);
/* Production SL adapter, called only after the engine string system starts. */
int LoadEngineCollisionXModel(const char *,const XModel **);
/* Separate cached model view with resolved native preset and primitive physics
 * map pointers. Shares skeleton/mesh ownership with the collision view. Same
 * lifetime, status and string-provider contract; missing or unsupported physics
 * dependencies fail atomically. Dedicated DB XModel lookup publishes this view;
 * it does not itself activate physics or provide graphical rendering resources. */
int LoadMountedPhysicsXModel(const char *,bone_string_acquire_t,
    bone_string_release_t,void *,const XModel **);
int LoadEnginePhysicsXModel(const char *,const XModel **);
typedef struct {
    unsigned count;
    XRigidVertList_t *lists;
} native_rigid_lists_t;
/* Owned conversion of inline rigid lists and trees. Pooled references require
 * relocation first and fail explicitly here; failure leaves output unchanged. */
int BuildNativeRigidLists(const surface_t *,native_rigid_lists_t *);
void FreeNativeRigidLists(native_rigid_lists_t *);
/* Resolves pooled lists/trees from retained block 3, applying loader alignment.
 * All returned data is owned; blocks may be released after success. */
int ResolveNativeRigidLists(const surface_t *,const model_block_t[8],native_rigid_lists_t *);
/* C1 surface/LOD part masks: eight words (256 bones), as in C1's DObj. */
#define SURFACE_PART_BIT_WORDS 8
typedef struct {
    XSurface surface;
    surface_arrays_t arrays;
    native_rigid_lists_t rigid;
    uint8_t source_header[84];
    uint32_t part_bits[SURFACE_PART_BIT_WORDS];
} native_surface_geometry_t;
/* Construction state: geometry, modes and part bits are native.
 * Remaining header metadata needs mapping before publication. Fully owned.
 * part_bits holds the full C1 mask; surface.partBits is its low 128 bits,
 * the width of the retained XSurface ABI (no dedicated-server consumer). */
int BuildNativeSurfaceGeometry(const surface_t *,const model_block_t[8],native_surface_geometry_t *);
/* serverSurfaceGeometry: whether native XSurfaces carry vertex/triangle data (see xsurface_native.c). */
void SetNativeSurfaceGeometry(int on);
int NativeSurfaceGeometry(void);
void FreeNativeSurfaceGeometry(native_surface_geometry_t *);
int LoadNativeModelSurfaceGeometry(const char *,unsigned,unsigned,native_surface_geometry_t *);
typedef struct {
    unsigned count;
    XSurface *surfaces;
    native_surface_geometry_t *owners;
    uint32_t part_bits[SURFACE_PART_BIT_WORDS];
} native_lod_geometry_t;
/* Contiguous native surface array backed by owned geometry, with cumulative
 * 16-bit triangle/vertex offsets and the union of surface bone masks, as in
 * native XModelLoad_640..680. Atomic construction;
 * model-level materials, LOD metadata and publication remain separate steps. */
int BuildNativeLodGeometry(const model_surfaces_t *,const model_block_t[8],native_lod_geometry_t *);
int LoadNativeModelLodGeometry(const char *,unsigned,native_lod_geometry_t *);
void FreeNativeLodGeometry(native_lod_geometry_t *);
typedef struct {
    unsigned count,lod_count;
    int collision_lod;
    XSurface *surfaces;
    uint16_t *material_indices;
    XModelLodInfo_t lods[4];
    native_lod_geometry_t owners[4];
} native_model_mesh_t;
/* Owned contiguous model geometry. Active source LODs are compacted while
 * preserving their original material indices; collision LOD is remapped.
 * Material handles and remaining model metadata are required before publication. */
int BuildNativeModelMesh(unsigned,const xmodel_lods_t *,
    const mounted_model_surfaces_t *const [4],native_model_mesh_t *);
int LoadNativeModelMesh(const char *,native_model_mesh_t *);
void FreeNativeModelMesh(native_model_mesh_t *);
/* Attach an assembled mesh using its original material-slot mapping. Mesh and
 * material assets must outlive the core. Atomic replacement; does not publish. */
int BindNativeModelMesh(native_model_core_t *,const native_model_mesh_t *,
    xMaterial_t *const *,unsigned);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

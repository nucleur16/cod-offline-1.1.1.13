#ifndef MATERIAL_NATIVE_H
#define MATERIAL_NATIVE_H
#include "ol_xmodel.h"
#include "xassets/material.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* Dedicated-engine material. Owns its name; renderer-only draw sorting,
 * techniques, texture/constant/state tables are absent, consistent with the
 * null client's disabled renderer. Not an asset for a graphical IW3 client. */
typedef struct {
    Material_t material;
    char *owned_name;
} native_material_t;
int BuildNativeMaterial(const mounted_model_material_t *,native_material_t *);
void FreeNativeMaterial(native_material_t *);
int LoadNativeModelMaterial(const char *,unsigned,native_material_t *);
/* Native asset lookup. Newest named definition wins; repeated lookups reuse
 * its owned cache entry. Borrowed until unmount. 1=ready, 0=absent, -1=invalid
 * or a known stub has no definition. Output stays unchanged on failure. */
int LoadEngineMaterial(const char *,const Material_t **);
int BuildC1NativeMaterial(const uint8_t *record,native_material_t *out);
int CompareNativeMaterials(const native_material_t *,const native_material_t *);
/* c1Readers (registry): check or replace a built material with the one from its C1 record. */
void C1ReplaceMaterial(native_material_t *material);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

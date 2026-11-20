/* CPU material identity for CoD4x's existing dedicated/null renderer. */
#include "material_native.h"
#include "asset_stream.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(Material_t)==80,"native material ABI");
_Static_assert(sizeof(MaterialInfo)==24,"native material info ABI");
_Static_assert(offsetof(MaterialInfo,surfaceTypeBits)==16,"native surface type bits");
_Static_assert(offsetof(Material_t,stateBitsEntry)==24,"native technique entries");
_Static_assert(offsetof(Material_t,techniqueSet)==64,"native technique pointer");

void FreeNativeMaterial(native_material_t *material)
{
    if (!material) return;
    free(material->owned_name); memset(material,0,sizeof(*material));
}
int BuildNativeMaterial(const mounted_model_material_t *source,native_material_t *out)
{
    native_material_t result={0}; size_t extent=0,length;
    const uint8_t *h;
    if (!source || !out || source->asset.type!=5 || !source->name || !*source->name ||
        source->name[0]==',' || !AdvanceMaterialStream(source->asset.data,source->asset.size,&extent) ||
        extent!=source->asset.size) return 0;
    length=strlen(source->name)+1;
    result.owned_name=malloc(length); if (!result.owned_name) return 0;
    memcpy(result.owned_name,source->name,length); h=source->asset.data;
    result.material.info.name=result.owned_name;
    /* Both formats have the 24-byte MaterialInfo prefix. DrawSurf is a
     * renderer-specific packed key and is deliberately not translated. */
    result.material.info.gameFlags=(char)h[4];
    result.material.info.sortKey=(char)h[5];
    result.material.info.textureAtlasRowCount=(char)h[6];
    result.material.info.textureAtlasColumnCount=(char)h[7];
    memcpy(&result.material.info.surfaceTypeBits,h+16,4);
    memset(result.material.stateBitsEntry,255,sizeof(result.material.stateBitsEntry));
    *out=result; return 1;
}
/* The same native material from the Material record C1 holds (c1Readers): its name and MaterialInfo bytes. */
int BuildC1NativeMaterial(const uint8_t *record,native_material_t *out)
{
    native_material_t result={0}; const char *name; size_t length;
    if (!record || !out) return 0;
    memcpy(&name,record,sizeof(name));
    if (!name || !*name) return 0;
    if (*name==',') ++name;
    length=strlen(name)+1;
    if (!(result.owned_name=malloc(length))) return 0;
    memcpy(result.owned_name,name,length);
    result.material.info.name=result.owned_name;
    result.material.info.gameFlags=(char)record[4];
    result.material.info.sortKey=(char)record[5];
    result.material.info.textureAtlasRowCount=(char)record[6];
    result.material.info.textureAtlasColumnCount=(char)record[7];
    memcpy(&result.material.info.surfaceTypeBits,record+16,4);
    memset(result.material.stateBitsEntry,255,sizeof(result.material.stateBitsEntry));
    *out=result; return 1;
}
int CompareNativeMaterials(const native_material_t *a,const native_material_t *b)
{
    const MaterialInfo *x=&a->material.info,*y=&b->material.info;
    if (!x->name || !y->name || strcmp(x->name,y->name)) return 1;
    if (x->gameFlags!=y->gameFlags || x->sortKey!=y->sortKey || x->textureAtlasRowCount!=y->textureAtlasRowCount ||
        x->textureAtlasColumnCount!=y->textureAtlasColumnCount || x->surfaceTypeBits!=y->surfaceTypeBits) return 2;
    return 0;
}
int LoadNativeModelMaterial(const char *name,unsigned index,native_material_t *out)
{
    mounted_model_material_t source; int status;
    if (!out) return -1;
    status=LoadMountedModelMaterialSource(name,index,&source);
    if (status!=1) return status;
    if (!BuildNativeMaterial(&source,out)) return -1;
    C1ReplaceMaterial(out);
    return 1;
}

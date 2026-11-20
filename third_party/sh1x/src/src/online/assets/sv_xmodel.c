/* Native engine ownership for converted model bone strings. */
#ifdef OL_SERVER
#include "q_shared.h"
#include "cscr_stringlist.h"
#include "ol_xmodel.h"
#include "xmodel_native.h"
#include "xanim_native.h"
#include "stream_model.h"
#include "collision.h"
#include "map_entity_policy.h"
#include "sv_state.h"
#include "effects_native.h"
#include "collision_native.h"
#include "physics_native.h"
#include "cm_public.h"
#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static native_clip_map_t installed_clip;
static collision_map_t installed_source;
int TriggerClipHandle(unsigned index)
{
    return index<installed_clip.trigger_model_count ? (int)(installed_clip.trigger_model_base+index) : -1;
}
/* AppendNativeTriggers places the map's trigger hulls AFTER the BSP's
 * own inline models, so cm.numSubModels on this server is larger than the
 * clipmap C1 loads: the client never has cmodels for "?N" triggers (C1's
 * G_SetModel 0x56ad17 resolves them through the map's trigger table, not
 * cm.cmodels).  These two answer which handles exist on the client. */
int IsTriggerClipHandle(int handle)
{
    return installed_clip.trigger_model_count && handle>=0 &&
           (unsigned)handle>=installed_clip.trigger_model_base &&
           (unsigned)handle-installed_clip.trigger_model_base<installed_clip.trigger_model_count;
}
unsigned ClientInlineModelCount(void)
{
    int all=CM_NumInlineModels();
    if(all<0) return 0;
    return installed_clip.trigger_model_count ? installed_clip.trigger_model_base : (unsigned)all;
}

void ShutdownEngineClipMap(void)
{
    FreeNativeClipMap(&installed_clip);
    FreeCollisionMap(&installed_source);
}

static int load_stream_model(void *context,const char *name,stream_model_t *out)
{
    char path[MAX_QPATH]; void *data=NULL; int length,status;
    (void)context;
    /* Streamed surface assets are basenames under models/, as in C1 0x4c9f20. */
    if (!name || !*name || strchr(name,'/') || strchr(name,'\\') || strchr(name,':') ||
        !strcmp(name,".") || !strcmp(name,"..") || strlen(name)+8>sizeof(path)) return -1;
    snprintf(path,sizeof(path),"models/%s",name);
    length=FS_ReadFile(path,&data);
    if (length<0) return 0;
    status=ReadStreamModel(data,(size_t)length,out) ? 1 : -1;
    if (data) FS_FreeFile(data);
    return status;
}
void InitEngineStreamModels(void)
{
    SetStreamModelLoader(load_stream_model,NULL);
}

static unsigned acquire_bone_string(void *context,const char *name)
{
    (void)context;
    return SL_GetString(name,0);
}
static void release_bone_string(void *context,unsigned handle)
{
    (void)context;
    SL_RemoveRefToString(handle);
}
int EngineStringHandleIs(unsigned handle,const char *name)
{
    const char *text;
    if(!handle || handle>UINT16_MAX || !name) return 0;
    text=SL_ConvertToString(handle);
    return text && !strcmp(text,name);
}
int LoadEngineModelBoneHandles(const char *name,const uint16_t **handles,unsigned *count)
{
    return BindMountedModelBoneHandles(name,acquire_bone_string,release_bone_string,NULL,handles,count);
}
int LoadEngineXAnimHandles(const char *name,const xanim_handles_t **out)
{
    return BindMountedXAnimHandles(name,acquire_bone_string,release_bone_string,NULL,out);
}
int LoadEngineWeaponHideTags(const char *name,const weapon_hide_tag_handles_t **out)
{
    return BindMountedWeaponHideTags(name,acquire_bone_string,release_bone_string,NULL,out);
}
void ReleaseEngineWeaponHideTags(weapon_hide_tag_handles_t *owned)
{
    unsigned i;
    if(!owned) return;
    for(i=0;i<32;++i) if(owned->tags[i])
        release_bone_string(NULL,owned->tags[i]);
    memset(owned,0,sizeof(*owned));
}
int AcquireEngineWeaponHideTags(const weapon_hide_tags_t *source,
    weapon_hide_tag_handles_t *out)
{
    weapon_hide_tag_handles_t result={{0}}; unsigned i,handle;
    if(!source || !out) return -1;
    for(i=0;i<32;++i) if(source->names[i] && !*source->names[i]) return -1;
    for(i=0;i<32;++i) {
        if(!source->names[i]) continue;
        handle=acquire_bone_string(NULL,source->names[i]);
        if(!handle || handle>UINT16_MAX) {
            if(handle) release_bone_string(NULL,handle);
            ReleaseEngineWeaponHideTags(&result); return -1;
        }
        result.tags[i]=(uint16_t)handle;
    }
    ReleaseEngineWeaponHideTags(out);
    *out=result; return 1;
}
int LoadEngineNativeXAnim(const char *name,const struct XAnimParts **out)
{
    return LoadMountedNativeXAnim(name,acquire_bone_string,release_bone_string,NULL,out);
}
int LoadNativeModelCore(const char *name,native_model_core_t *out)
{
    const mounted_model_skeleton_t *skeleton;
    const mounted_model_geometry_t *geometry;
    const uint16_t *handles;
    unsigned count;
    int status;
    if (!out) return -1;
    status=LoadMountedXModelSkeleton(name,&skeleton);
    if (status!=1) return status;
    status=LoadMountedXModelGeometry(name,&geometry);
    if (status!=1) return status;
    status=LoadEngineModelBoneHandles(name,&handles,&count);
    if (status!=1) return status;
    return BuildNativeModelCore(skeleton,geometry,handles,count,out) ? 1 : -1;
}
int LoadEngineCollisionXModel(const char *name,const XModel **out)
{
    return LoadMountedCollisionXModel(name,acquire_bone_string,release_bone_string,NULL,out);
}
int LoadEnginePhysicsXModel(const char *name,const XModel **out)
{
    return LoadMountedPhysicsXModel(name,acquire_bone_string,release_bone_string,NULL,out);
}
static const XModel *load_effect_model(void *context,const char *name)
{
    const XModel *model;
    (void)context;
    return LoadEnginePhysicsXModel(name,&model)==1 ? model : NULL;
}
int LoadEngineNativeEffect(const char *name,const FxEffectDef **out)
{
    return LoadMountedNativeEffect(name,load_effect_model,NULL,out);
}
static void *load_collision_model(void *context,const char *name)
{
    const XModel *model;
    int status;
    (void)context;
    status=LoadEngineCollisionXModel(name,&model);
    if(status!=1) Com_Printf(CON_CHANNEL_SERVER,"CoDOL collision model %s failed: %d\n",name,status);
    return status==1 ? (void *)model : NULL;
}
int LoadEngineCollisionMap(const char *name,collision_map_t *out)
{
    return LoadMountedCollisionModels(name,load_collision_model,NULL,out);
}
int BuildEngineClipMap(const char *name,const collision_map_t *source,native_clip_map_t *out)
{
    dynamic_entities_t dynamic={0}; dynamic_binding_t *bindings[2]={0};
    const dynamic_binding_t *views[2]; unsigned category,i; int status,result=-1;
    if (!name || !source || !out) return -1;
    status=LoadMountedDynamicEntities(name,&dynamic);
    if (status!=1) {
        Com_Printf(CON_CHANNEL_SERVER,"CoDOL collision %s: dynamic decode failed (%d)\n",name,status);
        return status;
    }
    for (category=0;category<2;++category) {
        if (dynamic.count[category]) {
            bindings[category]=calloc(dynamic.count[category],sizeof(*bindings[category]));
            if (!bindings[category]) goto done;
        }
        for (i=0;i<dynamic.count[category];++i) {
            const dynamic_entity_t *entity=dynamic.entities[category]+i;
            dynamic_binding_t *binding=bindings[category]+i;
            dynamic_dependency_t dependency;
            memset(&dependency,0,sizeof(dependency));
            if (entity->model_reference) {
                const XModel *model;
                if (LoadMountedDynamicDependency(name,category,i,4,&dependency)!=1 ||
                    LoadEnginePhysicsXModel(dependency.name,&model)!=1) {
                    Com_Printf(CON_CHANNEL_SERVER,"CoDOL collision %s: dynamic %u/%u model %s failed\n",name,category,i,dependency.name); goto done;
                }
                binding->model=(XModel *)model;
            }
            if (entity->effect_reference) {
                const FxEffectDef *effect=NULL;
                /* The destroy effect is cosmetic on a dedicated server: clients
                 * play it from their own zone, and the server keeps only the
                 * entity's collision. An effect the native decoder cannot build
                 * (MEASURED 2026-09-26: props/sack_coffee on mp_underpass) must
                 * not stop the whole map from loading -- degrade to no effect,
                 * as weapon muzzle flashes do, and say which. The dependency
                 * reference itself is still required to be well formed. */
                if (LoadMountedDynamicDependency(name,category,i,30,&dependency)!=1) {
                    Com_Printf(CON_CHANNEL_SERVER,"CoDOL collision %s: dynamic %u/%u effect reference unreadable\n",name,category,i); goto done;
                }
                if (LoadEngineNativeEffect(dependency.name,&effect)!=1 || !effect) {
                    Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL collision %s: dynamic %u/%u destroy effect %s could not be built; "
                        "keeping the entity without it\n",name,category,i,dependency.name);
                    effect=NULL;
                }
                binding->effect=(FxEffectDef *)effect;
            }
            if (entity->preset_reference) {
                const struct PhysPreset *preset;
                if (LoadMountedDynamicDependency(name,category,i,0,&dependency)!=1 ||
                    LoadEnginePhysicsPreset(dependency.name,&preset)!=1) {
                    Com_Printf(CON_CHANNEL_SERVER,"CoDOL collision %s: dynamic %u/%u preset %s failed\n",name,category,i,dependency.name); goto done;
                }
                binding->preset=(struct PhysPreset *)preset;
            }
        }
    }
    views[0]=bindings[0]; views[1]=bindings[1];
    result=BuildUnculledNativeClipMap(source,&dynamic,views,name,out) ? 1 : -1;
    if(result!=1) Com_Printf(CON_CHANNEL_SERVER,"CoDOL collision %s: resolved clipmap conversion failed\n",name);
done:
    free(bindings[0]); free(bindings[1]); FreeDynamicEntities(&dynamic); return result;
}
int InstallEngineClipMap(const char *path)
{
    static const char prefix[]="maps/mp/",suffix[]=".d3dbsp";
    char name[MAX_QPATH],*owned_name; size_t length,base_length;
    collision_map_t source={0}; native_clip_map_t native={0};
    if (!path) return -1;
    length=strlen(path);
    if (length<=sizeof(prefix)-1+sizeof(suffix)-1 ||
        strncmp(path,prefix,sizeof(prefix)-1) || strcmp(path+length-(sizeof(suffix)-1),suffix)) return -1;
    base_length=length-(sizeof(prefix)-1)-(sizeof(suffix)-1);
    if (base_length>=sizeof(name)) return -1;
    memcpy(name,path+sizeof(prefix)-1,base_length); name[base_length]=0;
    if (strchr(name,'/') || strchr(name,'\\') || strchr(name,':')) return -1;
    {
        int status=LoadEngineCollisionMap(name,&source);
        if(status!=1) {
            Com_Printf(CON_CHANNEL_SERVER,"CoDOL collision %s: source decode failed (%d)\n",name,status);
            goto fail;
        }
        /* A coop match has two spellings: `name` is the base mp_* level zone
         * that carries the world, and the session/playlist spelling names the
         * coop zone, whose ONLY asset is an addon_map_ents carrying the Spec
         * Ops entities (MEASURED, docs/ADDON_MAP_ENTS_20260920.md).
         * Overlay it before the clipmap is assembled, so the merged trigger
         * arrays reach AppendNativeTriggers as one contiguous range. */
        const char *session=SessionMapName();
        const char *category=ServerZoneType();
        if(MapEntityUsesCoopAddon(category,name,session)) {
            map_entities_t addon={0},merged={0};
            status=LoadMountedAddonMapEntities(session,&addon);
            if(status<0) {
                Com_PrintError(CON_CHANNEL_SERVER,
                    "CoDOL collision %s: addon entities maps/mp/%s.mapents are present but unreadable\n",name,session);
                goto fail;
            }
            if(status==0) {
                Com_PrintWarning(CON_CHANNEL_SERVER,
                    "CoDOL collision %s: no addon_map_ents maps/mp/%s.mapents in any mounted zone; "
                    "this match will only see the base multiplayer entities\n",name,session);
            } else {
                unsigned base_models=source.entities.model_count;
                size_t base_text=source.entities.text_size;
                if(!MergeMapEntities(&source.entities,&addon,&merged)) {
                    FreeMapEntities(&addon);
                    Com_PrintError(CON_CHANNEL_SERVER,
                        "CoDOL collision %s: addon entities maps/mp/%s.mapents could not be composed\n",name,session);
                    goto fail;
                }
                ((void)0);
                FreeMapEntities(&addon);
                FreeMapEntities(&source.entities);
                source.entities=merged;
            }
        }
        status=BuildEngineClipMap(name,&source,&native);
        if(status!=1) {
            Com_Printf(CON_CHANNEL_SERVER,"CoDOL collision %s: native assembly failed (%d)\n",name,status);
            goto fail;
        }
        /* maps/mp/<name>.d3dbsp.ents (map_ents_override.c): an MP-rules
         * match's corrected entity string, in place of the zone's, before
         * anything parses it.  The clipmap owns the string either way. */
        if(MapEntityUsesMpBase(category)) {
            size_t size=0;
            char *text=LoadMapEntsOverride(name,source.entities.text,source.entities.text_size,
                source.entities.model_count,&size);
            if(text) {
                free(native.map.mapEnts->entityString);
                native.map.mapEnts->entityString=text;
                native.map.mapEnts->numEntityChars=(int)size;
            }
        }
    }
    owned_name=malloc(length+1); if (!owned_name) goto fail;
    memcpy(owned_name,path,length+1); free((void *)native.map.name);
    native.map.name=owned_name; native.map.mapEnts->name=owned_name;
    ShutdownEngineClipMap(); installed_source=source; installed_clip=native;
    cm=installed_clip.map;
    /* CM_LoadMap initializes trace-thread data and marks cm in use next. */
    return 1;
fail:
    FreeNativeClipMap(&native); FreeCollisionMap(&source); return -1;
}
#endif

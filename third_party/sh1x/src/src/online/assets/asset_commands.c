#include "revive_token.h"
#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "persistent_events.h"
#endif
/* Native archive console integration. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "assets.h"
#include "category_patch.h"
#include "c1_records.h"
#include "client_crypto.h"
#include "match_report.h"
#include "structured_schema.h"
#include "ol_profile.h"
#include "profile_progress.h"
#include "character_variant.h"
#include "character_composition.h"
#include "configstrings.h"
#include "sv_state.h"
#include "connect.h"
#include "load_blocks.h"
#include "c1_materialize.h"
#include "asset_host.h"
#include "zone_image.h"
void LogMemoryCheckpoint(const char *what);
#include "stream_model.h"
#include "zone_path.h"
#include "collision.h"
#include "collision_native.h"
#include "dynents.h"
#include "effects.h"
#include "physics_native.h"
#include "xmodel_native.h"
#include "effects_native.h"
#include "material_native.h"
#include "weapon_native.h"
#include "weapon_compose.h"
#include "sv_weapons.h"
#include "xanim_native.h"
#include "model_animation.h"
#include "sound_native.h"
#include "script_profile_vm.h"
#include "sway_phase.h"
#include "pmove_c1_breath.h"
#include "cmd.h"
#include "qcommon_io.h"
#include "qcommon.h"
#include "qcommon_parsecmdline.h"
#include "filesystem.h"
#include "cvar.h"
#include "server.h"
#include "sys_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bounded source-graph audit; names borrow the mounted archives. */
static void audit_effect_graph(const char *root)
{
    const char *names[64]={root}; unsigned count=1,index,rows=0;
    for(index=0;index<count;++index) {
        const effect_source_t *source; unsigned i,j;
        int status=LoadMountedEffectSource(names[index],&source);
        if(status!=1) {
            Com_Printf(CON_CHANNEL_SYSTEM,"    effect source %s: status %d\n",names[index],status);
            continue;
        }
        for(i=0;i<source->prefix.count;++i) {
            const effect_element_t *element=source->elements+i;
            unsigned type=element->record[176];
            if(++rows>256) { Com_Printf(CON_CHANNEL_SYSTEM,"    effect audit truncated at 256 elements\n"); return; }
            Com_Printf(CON_CHANNEL_SYSTEM,"    effect %s element %u: type %u, visuals %u, extension %u bytes\n",
                names[index],i,type,element->visual_count,(unsigned)element->extension_size);
            for(j=0;j<3+(type==14 ? element->visual_count : 0);++j) {
                const char *child=j<3 ? element->effect_names[j] : element->visuals[j-3].string;
                unsigned k;
                if(!child || !*child) continue;
                for(k=0;k<count;++k) if(!strcmp(names[k],child)) break;
                if(k<count) continue;
                if(count==64) { Com_Printf(CON_CHANNEL_SYSTEM,"    effect audit truncated at 64 dependencies\n"); return; }
                names[count++]=child;
            }
        }
    }
}

typedef struct {
    unsigned presets,maps,failed,geometries,brushes,types[8],other_types;
    unsigned native_ready,native_failed;
    unsigned models_ready,models_failed;
} model_physics_audit_t;
static void audit_model_physics(const char *name,model_physics_audit_t *audit)
{
    const mounted_model_geometry_t *geometry;
    const XModel *native_model;
    if (LoadEnginePhysicsXModel(name,&native_model)==1) ++audit->models_ready;
    else ++audit->models_failed;
    if (LoadMountedXModelGeometry(name,&geometry)!=1) { ++audit->failed; return; }
    if (geometry->bounds.physics_preset_reference) ++audit->presets;
    if (geometry->bounds.physics_map_reference) {
        mounted_model_physics_t dependency; const physics_map_t *map; unsigned i;
        ++audit->maps;
        if (LoadMountedModelPhysics(name,1,&dependency)!=1 ||
            LoadMountedPhysicsMap(dependency.name,&map)!=1) { ++audit->failed; return; }
        { const PhysGeomList_t *native;
          if (LoadEnginePhysicsMap(dependency.name,&native)==1) ++audit->native_ready;
          else ++audit->native_failed; }
        audit->geometries+=map->count;
        for (i=0;i<map->count;++i) {
            const uint8_t *r=map->geometries[i].record;
            uint32_t type=(uint32_t)r[4]|(uint32_t)r[5]<<8|(uint32_t)r[6]<<16|(uint32_t)r[7]<<24;
            if (type<8) ++audit->types[type]; else ++audit->other_types;
            if (map->geometries[i].brush) ++audit->brushes;
        }
    }
}
static void CheckCollision_f(void)
{
    collision_map_t map={0}; native_brushes_t brushes={0};
    native_terrain_t terrain={0}; native_leaves_t leaves={0}; int status;
    native_collision_assets_t assets={0};
    if(Cmd_Argc()!=2) {
        Com_Printf(CON_CHANNEL_SYSTEM,"Usage: checkcollision <mounted map basename>\n"); return;
    }
    status=LoadEngineCollisionMap(Cmd_Argv(1),&map);
    Com_Printf(CON_CHANNEL_SYSTEM,
        "CoDOL collision check: %s status %d, %u/%u native models, %u unresolved materials (not installed)\n",
        Cmd_Argv(1),status,map.static_models.count-map.static_models.unresolved,
        map.static_models.count,map.materials.unresolved);
    if(status==1) {
        unsigned triggers_ready=0,triggers_failed=0;
        for(unsigned i=0;i<map.entities.model_count;++i) {
            trigger_model_t trigger={0};
            if(ReadTriggerModel(&map.entities,i,&trigger)) {
                ++triggers_ready;
                if(i==0) Com_Printf(CON_CHANNEL_SYSTEM,
                    "CoDOL trigger ?0: %u hulls, contents %x, bounds (%g %g %g)-(%g %g %g) (not installed)\n",
                    trigger.hull_count,(unsigned)trigger.contents,trigger.mins[0],trigger.mins[1],trigger.mins[2],
                    trigger.maxs[0],trigger.maxs[1],trigger.maxs[2]);
            } else ++triggers_failed;
            FreeTriggerModel(&trigger);
        }
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL trigger geometry: %u ready, %u failed (not installed)\n",triggers_ready,triggers_failed);
        native_clip_map_t native={0};
        int complete=BuildEngineClipMap(Cmd_Argv(1),&map,&native);
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL assembled native clipmap: status %d, %u/%u dynamic entities, %d leaves (not installed; all-visible policy)\n",
            complete,native.dynamics.count[0],native.dynamics.count[1],native.map.numLeafs);
        FreeNativeClipMap(&native);
        int ready=BuildNativeCollisionBrushes(&map.brushes,&map.brush_tree,&brushes);
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL collision checksum: %08x (serialized clipmap)\n",map.checksum);
        int assets_ready=BuildNativeCollisionAssets(&map.materials,&map.static_models,&assets);
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native collision asset check: status %d, %u materials, %u static models\n",
            assets_ready,assets.material_count,assets.model_count);
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native brush check: status %d, %u brushes, %u tree nodes including box hull\n",
            ready,brushes.brush_count,brushes.brush_node_count);
        ready=BuildNativeCollisionTerrain(&map.terrain,&terrain);
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native terrain check: status %d, %u triangles, %u partitions, %u AABB nodes\n",
            ready,terrain.triangle_count,terrain.partition_count,terrain.aabb_count);
        ready=BuildNativeCollisionLeaves(&map.leaves,map.terrain.aabb_count,map.brush_tree.node_count,&leaves);
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native leaf check: status %d, %u leaves, %u inline models, %u leaf surfaces (visibility pending)\n",
            ready,leaves.leaf_count,leaves.model_count,leaves.surface_count);
    }
    {
        dynamic_entities_t entities={0};
        int ready=LoadMountedDynamicEntities(Cmd_Argv(1),&entities);
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL dynamic definition check: status %d, %u/%u entities (source staging)\n",
            ready,entities.count[0],entities.count[1]);
        if(ready==1) {
            static const unsigned types[3]={4,30,0};
            unsigned category,index,k,resolved[3]={0},missing=0,native_presets=0,preset_failures=0;
            unsigned effect_models=0,effect_materials=0,effect_failures=0;
            unsigned native_effects=0,native_effect_failures=0;
            model_physics_audit_t model_physics={0};
            for(category=0;category<2;++category) for(index=0;index<entities.count[category];++index)
                for(k=0;k<3;++k) {
                    dynamic_dependency_t dependency;
                    int status=LoadMountedDynamicDependency(Cmd_Argv(1),category,index,types[k],&dependency);
                    if(status==1) ++resolved[k]; else if(status<0) ++missing;
                    if(status==1 && types[k]==4) audit_model_physics(dependency.name,&model_physics);
                    if(status==1 && types[k]==0) {
                        const struct PhysPreset *preset;
                        if(LoadEnginePhysicsPreset(dependency.name,&preset)==1) ++native_presets;
                        else ++preset_failures;
                    }
                    if(status==1 && types[k]==30) {
                        const effect_source_t *effect; unsigned element,visual;
                        const FxEffectDef *native_effect=NULL;
                        if(LoadEngineNativeEffect(dependency.name,&native_effect)==1) ++native_effects;
                        else {
                            ++native_effect_failures;
                            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL failed native dynamic effect: %s (entity %u:%u)\n",
                                dependency.name,category,index);
                        }
                        if(LoadMountedEffectSource(dependency.name,&effect)!=1) { ++effect_failures; continue; }
                        if(!native_effect) {
                            for(element=0;element<effect->prefix.count;++element) {
                                const effect_element_t *e=effect->elements+element;
                                Com_Printf(CON_CHANNEL_SYSTEM,"  element %u: type %u, extension %u bytes, extra name %s\n",
                                    element,e->record[176],(unsigned)e->extension_size,e->extra_name ? e->extra_name : "(none)");
                            }
                        }
                        for(element=0;element<effect->prefix.count;++element)
                            for(visual=0;visual<effect->elements[element].visual_count;++visual) {
                                effect_visual_source_t source;
                                int ready=LoadMountedEffectVisual(dependency.name,element,visual,&source);
                                if(ready<0) ++effect_failures;
                                else if(ready==1) {
                                    if(source.asset.type==4) { ++effect_models; audit_model_physics(source.name,&model_physics); }
                                    else ++effect_materials;
                                }
                            }
                    }
                }
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL dynamic dependencies: %u models, %u effects, %u presets, %u unresolved (source definitions)\n",
                resolved[0],resolved[1],resolved[2],missing);
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native dynamic presets: %u ready, %u failed (common physics fields)\n",
                native_presets,preset_failures);
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL effect visuals: %u models, %u materials, %u failed (source bindings)\n",
                effect_models,effect_materials,effect_failures);
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native dynamic effects: %u ready, %u failed (not activated)\n",
                native_effects,native_effect_failures);
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL dynamic/effect XModel physics audit: %u preset references, %u collision-map references, %u failed reads\n",
                model_physics.presets,model_physics.maps,model_physics.failed);
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL physics geometry audit: %u geometries, %u brushes; types 0..7: %u %u %u %u %u %u %u %u, other %u\n",
                model_physics.geometries,model_physics.brushes,model_physics.types[0],model_physics.types[1],
                model_physics.types[2],model_physics.types[3],model_physics.types[4],model_physics.types[5],
                model_physics.types[6],model_physics.types[7],model_physics.other_types);
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native primitive physics maps: %u ready, %u failed (not activated)\n",
                model_physics.native_ready,model_physics.native_failed);
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native physics XModels: %u ready, %u failed (not activated)\n",
                model_physics.models_ready,model_physics.models_failed);
        }
        FreeDynamicEntities(&entities);
    }
    FreeNativeCollisionLeaves(&leaves);
    FreeNativeCollisionAssets(&assets);
    FreeNativeCollisionTerrain(&terrain);
    FreeNativeCollisionBrushes(&brushes);
    FreeCollisionMap(&map);
}
int BuildZonePath(const char *name,char *out,size_t capacity)
{
    cvar_t *directory=Cvar_FindVar("zonePath");
    const char *language=SEH_GetLanguageName(SEH_GetCurrentLanguage());
    FILE *probe;
    if(!ZoneFilePath(fs_basepath->string,language,
           directory ? directory->string : NULL,name,out,capacity)) return 0;
    if((directory && *directory->string) || (probe=fopen(out,"rb"))==NULL) {
        /* loc_language comes from the localized_<lang>_*.iwd packs found under
         * fs_basepath/main and defaults to english when none are found. C1
         * ships its zones only in client_shipRetail/chinese, so fall back to
         * that folder when the derived one has no such zone. */
        if(directory && *directory->string) return 1;
        if(!ZoneFilePath(fs_basepath->string,"chinese",NULL,name,out,capacity)) return 0;
        if((probe=fopen(out,"rb"))!=NULL) { fclose(probe); return 1; }
        /* User maps (the client's Kernel33 usermaps.c): <base>/usermaps/<map>/<zone>.ff
         * for a zone no stock folder has; a map's loadscreen zone <map>_load sits in
         * <map>'s folder. Installed under client_shipRetail they fail C1's pure check. */
        {
            char folder[256],usermaps[1024],usermap[1024];
            size_t len=strlen(name);
            Q_strncpyz(folder,name,sizeof(folder));
            if(len>5 && !strcmp(name+len-5,"_load") && len-5<sizeof(folder)) folder[len-5]=0;
            Com_sprintf(usermaps,sizeof(usermaps),"%s/usermaps/%s",fs_basepath->string,folder);
            if(ZoneFilePath(NULL,NULL,usermaps,name,usermap,sizeof(usermap)) &&
               strlen(usermap)<capacity && (probe=fopen(usermap,"rb"))!=NULL) {
                fclose(probe);
                Q_strncpyz(out,usermap,capacity);
            }
        }
        return 1;
    }
    fclose(probe);
    return 1;
}
/* The client's shared Kernel33 patch zones (client_shipRetail\patch, the folder patchZonePath names), which
 * every client loads whatever the map: since 2026-10-26 Kernel33's consolidated common_extras (mode rules and
 * tables, the in-match movies, sounds and the usermaps' create-room data; it loads ahead of code_pre_gfx) and
 * common_postgfx_mp (field order menus, pad icons; it loads after code_post_gfx). The server mounts them so
 * it reads the same comaplist, cogametypestable, basemaps.arena, _gametypes.txt and animated_props anim tree as
 * the client instead of the stock ones (found by Kernel33's p2passetcheck, feat/iw4-asset-layout). Ranks follow
 * C1's first-definition order: common_extras ahead of every stock zone, common_postgfx_mp just after the
 * code_post_gfx set. An install from before the consolidation carries the parts as separate zones instead; the
 * ones with server data (000_party_modes, usermaps_patch) are mounted the same way. The locale zones
 * (*_locale) and the front-end movie zones (common_ui_mp, *_ui_*) are the client's business. An install from
 * before the zones dropped "codol" from their names (2026-11) has each one under its old name instead (the
 * second column below); the old file is mounted only when the new one is not there, as Kernel33 does. */
static size_t append_shared_patch_zones(const char **paths,int *ranks,char (*storage)[MAX_OSPATH],size_t count,
                                        size_t capacity)
{
    static const struct { const char *name; const char *old_name; int rank; } shared[]={
        {"common_extras","codol_common",LINK_RANK_PATCH_FOLDER}, /* LEGACY_ZONE */
        {"common_postgfx_mp","codol_common_postgfx_mp",LINK_RANK_STARTUP+5}, /* LEGACY_ZONE */
    };
    static const char *const legacy[]={"000_codol_party_modes","usermaps_patch"}; /* LEGACY_ZONE */
    cvar_t *folder=Cvar_FindVar("patchZonePath");
    char dir[MAX_OSPATH];
    size_t i; int have_common=0;
    if(folder && *folder->string) Q_strncpyz(dir,folder->string,sizeof(dir));
    else if(fs_basepath && *fs_basepath->string)
        Com_sprintf(dir,sizeof(dir),"%s/client_shipRetail/patch",fs_basepath->string);
    else return count;
    for(i=0;i<sizeof(shared)/sizeof(shared[0]) && count<capacity;++i) {
        const char *name=shared[i].name;
        Com_sprintf(storage[count],MAX_OSPATH,"%s/%s.ff",dir,name);
        if(!FS_FileExistsOSPath(storage[count])) {
            name=shared[i].old_name;
            Com_sprintf(storage[count],MAX_OSPATH,"%s/%s.ff",dir,name);
            if(!FS_FileExistsOSPath(storage[count])) continue;
        }
        if(i==0) have_common=1;
        paths[count]=storage[count]; ranks[count]=shared[i].rank; ++count;
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL shared patch zone: %s.ff mounted with the startup archives\n",name);
    }
    if(!have_common) for(i=0;i<sizeof(legacy)/sizeof(legacy[0]) && count<capacity;++i) {
        Com_sprintf(storage[count],MAX_OSPATH,"%s/%s.ff",dir,legacy[i]);
        if(!FS_FileExistsOSPath(storage[count])) continue;
        /* Kernel33 loads the folder alphabetically ahead of the stock zones. */
        paths[count]=storage[count]; ranks[count]=LINK_RANK_PATCH_FOLDER+(int)i; ++count;
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL shared patch zone: %s.ff mounted with the startup archives (pre-consolidation install)\n",legacy[i]);
    }
    return count;
}

int LoadStartupArchives(void)
{
    /* Ranked in C1's load order (first definition wins, see link_ranked):
     * the code_post_gfx set at 0x598482 -- code_post_gfx_default_mp, then each
     * patch zone just ahead of its base -- and, at the first level load, the
     * common_mp set in the same patch-first order (0x4ccd81..0x4ccddf). */
    static const char *const names[]={
        "code_post_gfx_default_mp", "code_post_gfx_mp", "patch_code_post_gfx_mp",
        "localized_code_post_gfx_mp", "patch_localized_code_post_gfx_mp",
        "common_mp", "patch_common_mp", "localized_common_mp", "patch_localized_common_mp",
        "ffotd_client"
    };
    static const int ranks[]={
        LINK_RANK_STARTUP+0, LINK_RANK_STARTUP+2, LINK_RANK_STARTUP+1,
        LINK_RANK_STARTUP+4, LINK_RANK_STARTUP+3,
        LINK_RANK_COMMON+1, LINK_RANK_COMMON+0, LINK_RANK_COMMON+3, LINK_RANK_COMMON+2,
        /* loaded after the front end, but its zone (+0xA4) overrides */
        LINK_RANK_FFOTD
    };
    char exe[MAX_OSPATH],storage[16][MAX_OSPATH];
    const char *paths[16],*key_path; cvar_t *client_exe,*infected_zone;
    int path_ranks[16];
    size_t i,count=0,failed=0; unsigned asset=UINT32_MAX;
    LogMemoryCheckpoint("before the startup archives");
    Com_StartupVariable("zonePath");
    Cvar_RegisterString("zonePath","",CVAR_INIT,"CoDOL fastfile directory; empty uses fs_basepath/client_shipRetail/<language>");
    Com_StartupVariable("patchZonePath");
    Cvar_RegisterString("patchZonePath","",CVAR_INIT,"The client's Kernel33 patch folder; empty uses fs_basepath/client_shipRetail/patch. Its <category>_*.ff zones are mounted with every map of that category");
    Com_StartupVariable("c1Fixups");
    SetCollectFixups(Cvar_RegisterBool("c1Fixups",qfalse,CVAR_INIT,
        "Record C1's pointer fix-ups and header copies while replaying zones (asset comparison and C1-layout records)")->boolean);
    Com_StartupVariable("freeZoneImages");
    SetFreeZoneImages(Cvar_RegisterBool("freeZoneImages",qtrue,CVAR_INIT,
        "Release each zone image once its replay rebuilds it exactly; pages refill from the replay when read")->boolean);
    Com_StartupVariable("fileBackedBlocks");
    SetFileBackedBlocks(Cvar_RegisterBool("fileBackedBlocks",qtrue,CVAR_INIT,
        "Keep large replayed zone blocks in temporary files (file-backed pages, not private memory)")->boolean);
    Com_StartupVariable("serverSurfaceGeometry");
    SetNativeSurfaceGeometry(Cvar_RegisterBool("serverSurfaceGeometry",qfalse,CVAR_INIT,
        "Native XModel surfaces carry vertices and triangles (the server never reads them; off saves the heap)")->boolean);
    Com_StartupVariable("c1Readers");
    SetC1Readers(Cvar_RegisterInt("c1Readers",0,0,2,CVAR_INIT,
        "Converters read C1 records (needs c1Fixups 1): 0 off, 1 on, 2 both and compare (self-check)")->integer);
    Com_StartupVariable("infectedModelZone");
    infected_zone=Cvar_RegisterString("infectedModelZone","",CVAR_INIT,
        "Optional cyborg appearance fastfile; empty auto-detects the client post-GFX model pack");
    Com_StartupVariable("profilePath");
    Cvar_RegisterString("profilePath","",0,"Directory holding the lobby's mpdata_s secure profiles (per-uid, shared, seed); empty attaches nothing");
    /* The same directory carries the match result back to the lobby, so install
     * the server match-record delivery sink now that the path is registered. */
    InstallMatchDataSink();
    client_exe=Cvar_FindVar("clientExe");
    key_path=client_exe ? client_exe->string : "";
    if(!*key_path) {
        if(!DefaultClientKeyPath(fs_basepath->string,exe,sizeof(exe))) return 0;
        key_path=exe;
    }
    for(i=0;i<10;++i) {
        if(!BuildZonePath(names[i],storage[count],sizeof(storage[count]))) return 0;
        /* ffotd_client is optional (the 1.1.1.13 install ships none) */
        if(i==9 && !FS_FileExistsOSPath(storage[count])) continue;
        paths[count]=storage[count]; path_ranks[count]=ranks[i]; ++count;
    }
    /* Preserve the tested permanent Infected model pack, with the same
     * priority class as the client's patch folder. The optional ffotd archive
     * occupies its own slot; it must not overwrite or overflow this path. */
    if (count >= sizeof(paths)/sizeof(paths[0])) return 0;
    if (*infected_zone->string) {
        if (strlen(infected_zone->string) >= sizeof(storage[count])) return 0;
        Q_strncpyz(storage[count],infected_zone->string,sizeof(storage[count]));
        paths[count]=storage[count]; path_ranks[count]=LINK_RANK_PATCH_FOLDER; ++count;
    } else {
        /* the zone's name, else (an install from before the zone names dropped "codol") its old one */
        static const char *const infected_names[]={"infected_cyborg_postgfx_mp","codol_infected_cyborg_postgfx_mp"}; /* LEGACY_ZONE */
        for (size_t k=0;k<sizeof(infected_names)/sizeof(infected_names[0]);++k) {
            int n=snprintf(storage[count],sizeof(storage[count]),
                "%s/client_shipRetail/patch/%s.ff",fs_basepath->string,infected_names[k]);
            if (n<0 || (size_t)n>=sizeof(storage[count])) return 0;
            if (FS_FileExistsOSPath(storage[count])) {
                paths[count]=storage[count]; path_ranks[count]=LINK_RANK_PATCH_FOLDER; ++count;
                break;
            }
        }
    }
    count=append_shared_patch_zones(paths,path_ranks,storage,count,sizeof(paths)/sizeof(paths[0]));
    if(!MountRankedArchives(paths,path_ranks,count,key_path,&failed,&asset)) {
        cvar_t *zone=Cvar_FindVar("zonePath");
        /* Overwhelmingly this is a bare launch: the executable was started
         * without the launcher, so fs_basepath is the exe directory and the
         * language is the default instead of the installation's. Name both,
         * and the launcher, so the message diagnoses itself rather than
         * printing an absolute path nobody can place. */
        Com_PrintError(CON_CHANNEL_SYSTEM,"CoDOL startup mount failed: %s (asset %u)\n",paths[failed],asset);
        Com_PrintError(CON_CHANNEL_SYSTEM,"  fs_basepath    %s\n",
            fs_basepath && fs_basepath->string ? fs_basepath->string : "<unset>");
        Com_PrintError(CON_CHANNEL_SYSTEM,"  language       %s\n",
            SEH_GetLanguageName(SEH_GetCurrentLanguage()));
        Com_PrintError(CON_CHANNEL_SYSTEM,"  zonePath %s\n",
            zone && *zone->string ? zone->string : "<empty; derived from fs_basepath/client_shipRetail/<language>>");
        Com_PrintError(CON_CHANNEL_SYSTEM,
            "  The CoDOL zones live under the game installation, not beside this\n"
            "  executable. Start the server through bin/START-SERVER.cmd, which\n"
            "  sets fs_basepath and the language, or pass them yourself.\n");
        return 0;
    }
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native startup archives mounted: %u (first: %s)\n",(unsigned)count,paths[0]);
    LogMemoryCheckpoint("startup archives mounted");
    return 1;
}

/* The group-1 archives currently mounted, so an unchanged match can reuse
 * them. Remounting re-opens, decrypts and replays every zone, so a map_restart
 * or a reload of the same map, which cannot need different zone content, takes
 * the fast path below instead. */
static char mounted_map[MAX_QPATH];
static char mounted_session[MAX_QPATH];
static char mounted_zone[16];
void ForgetMountedMapArchives(void)
{
    mounted_map[0]=0; mounted_session[0]=0; mounted_zone[0]=0;
}

/* Native weapon table tail, so the teardown below can assert that no live
 * WeaponDef points into the generation it is about to free.
 *
 * MEASURED: SV_SpawnServer calls SV_ShutdownGameProgs before it reaches the
 * mount, G_ShutdownGame calls Com_FreeWeaponInfoMemory (_g_main_mp.asm:1555),
 * and BG_ShutdownWeaponDefFiles ends by storing 0 into bg_lastParsedWeaponIndex
 * (_bg_weapons.asm, BG_ShutdownWeaponDefFiles_20). So the table is empty at
 * mount time and releasing composed definitions there is safe. The check keeps
 * it that way: if that ordering ever changes, this reports it and keeps the
 * old generation alive rather than handing the engine freed definitions. */
extern unsigned int bg_lastParsedWeaponIndex;
/* src/sv_main.c owns these; the asset layer installs itself into them. */
extern void SV_SetCategoryTeardown(void (*fn)(const char *previous,const char *next));
extern void SV_SetSessionMapInstaller(void (*fn)(const char *session_map));

/* Everything that must stop referring to the outgoing asset category before
 * its archives can be released.
 *
 * MEASURED caches that borrow from the mounted archives and are NOT keyed by
 * mounted generation, i.e. would otherwise answer a coop/zm match out of the
 * retired mp zones:
 *   - weapon sound bindings (sound_native.c:238, keyed by weapon NAME,
 *     holding snd_alias_list_t* into an archive's sound cache)
 *   - composed weapon cores and their base definitions
 *     (weapon_compose.c; RetireComposedWeapons only unlinks them)
 *   - the model-animation bank and client anim trees (model_animation.c,
 *     built from animlist rawfiles owned by an archive)
 *   - script profile definitions (borrow archive names and offset maps)
 *   - the loose-file CSV overlay cache (keyed by script root + name, and the
 *     script root is chosen from the asset category)
 * and finally the retired archives themselves, which nothing else ever frees.
 *
 * from_type is only used for the log line; it may be NULL. */
void UnloadCategoryAssets(const char *from_type)
{
    unsigned freed;
    /* Order matters: every borrower first, providers last. */
    ResetNativeWeaponSounds();
    ResetModelAnimations();
    ResetScriptProfiles();
    ResetReferenceStringTables();
    /* Both of these are keyed by a raw WeaponDef pointer into the composed
     * cores released below, and nothing else clears them. A recycled address
     * in the next category would false-match and serve the previous
     * category's sway curve / scope flag -- copied scalars, so a wrong value
     * rather than a crash, which is why it would go unnoticed. */
    SwayPhaseResetDefs();
    ResetBreathScopeCache();
    ForgetMountedMapArchives();
    if(bg_lastParsedWeaponIndex) {
        /* Not reachable through SV_SpawnServer; see the note on the extern. */
        Com_PrintWarning(CON_CHANNEL_SYSTEM,
            "CoDOL category unload (%s): %u native weapon definitions are still registered; "
            "keeping the retired archives resident rather than freeing definitions the engine holds\n",
            from_type ? from_type : "?",bg_lastParsedWeaponIndex+1);
        return;
    }
    ResetComposedWeapons();
    freed=ReleaseRetiredArchives();
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL category assets unloaded (%s): %u retired archives freed\n",
        from_type ? from_type : "?",freed);
    HeapCheck("after category asset unload");
}

/* SV_SetCategoryTeardown entry point: SV_SpawnServer calls this BEFORE the
 * new category's archives are mounted, which is the shape the user asked for --
 * the mp zones and scripts go away, then the coop/zm ones arrive, instead of
 * both being resident at once. Retire the live group-1 set first so the unload
 * has something to release; group 0 (code_post_gfx_mp, common_mp and their
 * localized/patch siblings) is deliberately untouched, see below. */
void UnloadGametypeAssets(const char *previous_category,const char *next_category)
{
    unsigned retired=RetireRuntimeArchiveGroup(1);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL gametype assets: %s -> %s, retiring %u mounted archives\n",
        previous_category ? previous_category : "?",next_category ? next_category : "?",retired);
    UnloadCategoryAssets(previous_category);
}

/* The playlist/session spelling of the map for this match, installed by
 * SV_SpawnServer through SV_SetSessionMapInstaller just before the mount.
 *
 * A co-op match has two spellings and the lobby sends both: `map` is the LEVEL
 * zone the client loads last and that goes on the wire (mp_killhouse_sh), and
 * sv_sessionMap is the playlist zone (coop_training_mp_killhouse_sh). For mp
 * and zm the two are identical and this stays empty. */
static char session_map_name[MAX_QPATH];
static int valid_zone_basename(const char *name)
{
    size_t i;
    if(!name || !*name || strlen(name)>=MAX_QPATH) return 0;
    for(i=0;name[i];++i)
        if((name[i]<'a' || name[i]>'z') && (name[i]<'0' || name[i]>'9') && name[i]!='_') return 0;
    return 1;
}
void SetSessionMapName(const char *session_map)
{
    if(!session_map || !*session_map) { session_map_name[0]=0; return; }
    if(!valid_zone_basename(session_map)) {
        Com_PrintWarning(CON_CHANNEL_SYSTEM,
            "CoDOL session map '%s' is not a zone basename; ignoring it\n",session_map);
        session_map_name[0]=0; return;
    }
    Q_strncpyz(session_map_name,session_map,sizeof(session_map_name));
}
const char *SessionMapName(void)
{
    return session_map_name;
}

/* The asset category a map name PROVES, or NULL when it proves nothing.
 *
 * C1 0x612850 derives the category from a case-insensitive prefix test -- "zm"
 * (two characters), then "coop" (four), everything else "mp" -- and applies it
 * to the map/gametype it was handed (reproduced in alpha0022/zone_crc.py:381-405).
 * Only the first two arms are evidence. A co-op match has TWO map spellings:
 * the playlist spelling coop_training_mp_killhouse_sh and the LEVEL zone
 * mp_killhouse_sh that the client actually loads, and the lobby sends the base
 * spelling. So an mp_* name is genuinely ambiguous between "mp" and "coop" and
 * this returns NULL for it -- treating that NULL as "mp" is what made every
 * co-op match refuse to mount. */
const char *ZoneTypeForMap(const char *map)
{
    if(!map || !*map) return NULL;
    if(!Q_stricmpn(map,"zm",2)) return "zm";
    if(!Q_stricmpn(map,"coop",4)) return "coop";
    return NULL;
}


int LoadMapArchivesEx(const char *map,const char *session_map,const char *zone_type)
{
    char names[6][MAX_QPATH],paths_storage[16][MAX_OSPATH],patch[MAX_QPATH],exe[MAX_OSPATH];
    char previous_zone[16];
    const char *paths[16],*level,*session,*key_path,*proven;
    cvar_t *client_exe; size_t roots,count=0,i,failed=0; unsigned asset=UINT32_MAX;
    int n,category_changed,ranks[16],root_rank[6];
    if(!map || !*map || !IsZoneType(zone_type)) return 0;
    if(session_map && !*session_map) session_map=NULL;
    /* Survival is its own category: common_gametype_survival and the
     * unchanged mp_* level, no session zone (docs/CODOL_SURVIVAL_PLAN.md).
     * A leftover coop_survival_* session spelling names no zone it mounts. */
    if(!strcmp(zone_type,"survival")) session_map=NULL;
    level=map; session=session_map;
    /* Accept either spelling in `map`. The lobby sends the base/level spelling
     * (match_server_control.plan() forwards selection["map"]), but a caller
     * that has only the playlist spelling must still work: the level zone is
     * the tail from the underscore that ends the co-op mode token. */
    if(!strncmp(map,"coop_",5)) {
        const char *tail=strchr(map+5,'_');
        if(!tail || !tail[1]) return 0;
        if(!session) session=map;
        level=tail+1;
    }
    if(session && !strcmp(session,level)) session=NULL;
    /* Refuse only a contradiction the names can actually prove. The category
     * carrier (fs_game -> sv_zoneType) is the authority; the map name is a
     * cross-check, not a source. A zm_* level is only ever a zm match, a coop_*
     * playlist entry is only ever a coop match, and every shipped zm level is
     * spelled zm_* -- but mp_killhouse_sh is a legitimate co-op level, so
     * "mp_* therefore mp" is NOT a proof and must not refuse. */
    proven=ZoneTypeForMap(level);
    if(!proven) proven=ZoneTypeForMap(session);
    if(proven ? strcmp(proven,zone_type)!=0 : !strcmp(zone_type,"zm")) {
        Com_PrintError(CON_CHANNEL_SYSTEM,
            "CoDOL map archives: level '%s' (session '%s') is a '%s' match but the category "
            "carrier says '%s'; mounting common_gametype_%s for it would give the client the "
            "wrong zones and fs_crc. Set the category from fs_game (or sv_zoneType) to "
            "match the map before the spawn.\n",
            level,session ? session : "-",proven ? proven : "mp",zone_type,zone_type);
        return 0;
    }
    /* In-process host: C1 loads this map's zones itself (the host player plays on this server); wait for them, so
     * the server mounts while C1 holds the same zones (stage 3: their records are C1's). */
    int c1_held=0;
    {
        const c1_zone_host_t *host=C1ZoneHost();
        if(host && host->await_zones) {
            int t0=Sys_Milliseconds(),held=host->await_zones(session ? session : level,zone_type,60000);
            c1_held=held && host->zone_held && host->zone_of;
            char common[MAX_QPATH];
            snprintf(common,sizeof(common),"common_gametype_%s",zone_type);
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL C1 zones for %s (%s): %s after %d ms; C1 holds %s %s, %s %s%s%s\n",
                session ? session : level,zone_type,held ? "held" : "NOT held",Sys_Milliseconds()-t0,
                level,host->zone_held && host->zone_held(level) ? "yes" : "no",common,
                host->zone_held && host->zone_held(common) ? "yes" : "no",
                session ? ", session " : "",session ? (host->zone_held && host->zone_held(session) ? "yes" : "no") : "");
        }
    }
    if(*mounted_map && !strcmp(mounted_map,level) && !strcmp(mounted_zone,zone_type) &&
       !strcmp(mounted_session,session ? session : "")) {
        unsigned live=MountedArchiveCount(1);
        if(live) {
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native map archives reused: %u (%s, %s)\n",live,level,zone_type);
            return 1;
        }
    }
    /* Ranked in C1's load order (first definition wins, see link_ranked in
     * asset_registry.c): after common_mp, the gametype set
     * (0x4ccf04 patch_common_gametype_<c>, 0x4ccf25 common_gametype_<c>, then
     * the localized pair), the co-op session zone, and last the level zone,
     * each patch_<x> loaded just ahead of <x> (0x4cd131 "patch_%s"). So the
     * gametype zones' definitions win over the level's, as on the client. */
    n=snprintf(names[0],sizeof(names[0]),"common_gametype_%s",zone_type);
    if(n<0 || (size_t)n>=sizeof(names[0])) return 0;
    root_rank[0]=LINK_RANK_GAMETYPE;
    n=snprintf(names[1],sizeof(names[1]),"localized_common_gametype_%s",zone_type);
    if(n<0 || (size_t)n>=sizeof(names[1])) return 0;
    root_rank[1]=LINK_RANK_GAMETYPE+10;
    n=snprintf(names[2],sizeof(names[2]),"%s",level);
    if(n<0 || (size_t)n>=sizeof(names[2])) return 0;
    root_rank[2]=LINK_RANK_LEVEL;
    roots=3;
    /* MEASURED (dw-capture.log:2232, the client's own ClientLoadTime line):
     *   "2005 common_mp| 1324 common_gametype_coop| 1720 coop_training_mp_killhouse_sh|
     *    4956 mp_killhouse_sh|NONE"
     * A co-op match loads BOTH the session zone and the level zone, and
     * coop_training_mp_killhouse_sh.ff is 41,861,397 bytes carrying 44 weapon
     * assets of its own (docs/zone_assets/ALL_weapon.txt), so it is mission
     * content, not the 24,853-byte loadscreen stub its _load sibling is. */
    if(session) {
        n=snprintf(names[3],sizeof(names[3]),"%s",session);
        if(n<0 || (size_t)n>=sizeof(names[3])) return 0;
        root_rank[3]=LINK_RANK_SESSION;
        roots=4;
    }
    /* Survival plays on the multiplayer chain with MP weapons and classes
     * (user direction 2026-10-27): the MP gametype set loads too, after the
     * survival set, so survival's definitions win where it carries a name
     * (the Kernel33 survival category loads them in this order). */
    if(!strcmp(zone_type,"survival")) {
        n=snprintf(names[roots],sizeof(names[roots]),"common_gametype_mp");
        if(n<0 || (size_t)n>=sizeof(names[roots])) return 0;
        root_rank[roots]=LINK_RANK_GAMETYPE+20;
        ++roots;
        n=snprintf(names[roots],sizeof(names[roots]),"localized_common_gametype_mp");
        if(n<0 || (size_t)n>=sizeof(names[roots])) return 0;
        root_rank[roots]=LINK_RANK_GAMETYPE+30;
        ++roots;
    }
    for(i=0;i<roots;++i) {
        if(!BuildZonePath(names[i],paths_storage[count],sizeof(paths_storage[count]))) return 0;
        paths[count]=paths_storage[count]; ranks[count]=root_rank[i]+1; ++count;
        n=snprintf(patch,sizeof(patch),"patch_%s",names[i]);
        if(n<0 || (size_t)n>=sizeof(patch) || !BuildZonePath(patch,paths_storage[count],sizeof(paths_storage[count]))) return 0;
        if(FS_FileExistsOSPath(paths_storage[count])) { paths[count]=paths_storage[count]; ranks[count]=root_rank[i]; ++count; }
    }
    count=AppendCategoryPatchZones(zone_type,paths,ranks,paths_storage,count,sizeof(paths)/sizeof(paths[0]));
    /* Zones C1 holds become C1-backed: "c1:<zone>|<path>" (the server's copy mounts only when a lookup needs it). */
    if(c1_held) {
        static char backed[16][MAX_OSPATH+MAX_QPATH+8];
        const c1_zone_host_t *host=C1ZoneHost();
        for(i=0;i<count;++i) {
            const char *base=strrchr(paths[i],'/'),*base2=strrchr(paths[i],'\\');
            char zone[MAX_QPATH]; size_t zl;
            if(base2 && (!base || base2>base)) base=base2;
            base=base ? base+1 : paths[i];
            Q_strncpyz(zone,base,sizeof(zone)); zl=strlen(zone);
            if(zl>3 && !Q_stricmp(zone+zl-3,".ff")) zone[zl-3]=0;
            if(!host->zone_held(zone)) continue;
            snprintf(backed[i],sizeof(backed[i]),"c1:%s|%s",zone,paths[i]);
            paths[i]=backed[i];
        }
    }
    client_exe=Cvar_FindVar("clientExe"); key_path=client_exe ? client_exe->string : "";
    if(!*key_path) {
        if(!DefaultClientKeyPath(fs_basepath->string,exe,sizeof(exe))) return 0;
        key_path=exe;
    }
    /* Captured before the mount: ReplaceRuntimeArchives moves the old
     * group aside, it does not free it, and it cannot fail after that point
     * (every zone is opened and replayed first). */
    Q_strncpyz(previous_zone,mounted_zone,sizeof(previous_zone));
    category_changed=*previous_zone && strcmp(previous_zone,zone_type)!=0;
    if(!ReplaceRankedArchives(paths,ranks,count,key_path,1,&failed,&asset)) {
        Com_PrintError(CON_CHANNEL_SYSTEM,"CoDOL map mount failed: %s (asset %u)\n",paths[failed],asset);
        return 0;
    }
    /* A same-category map change keeps the previous behaviour exactly: nothing
     * extra runs and the reuse fast path above is untouched. A category change
     * is the handoff the client makes too, and it must leave nothing of the old
     * category resident or reachable. */
    if(category_changed) UnloadCategoryAssets(previous_zone);
    HeapCheck("after map archive mount");
    Q_strncpyz(mounted_map,level,sizeof(mounted_map));
    Q_strncpyz(mounted_session,session ? session : "",sizeof(mounted_session));
    Q_strncpyz(mounted_zone,zone_type,sizeof(mounted_zone));
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native map archives mounted: %u (%s%s%s, %s)\n",
        (unsigned)count,level,session ? " + " : "",session ? session : "",zone_type);
    /* Name each one, so what a category mounts is checkable from the log
     * (survival: its own set, the MP gametype set and the level). */
    for(i=0;i<count;++i) {
        const char *base=strrchr(paths[i],'/'),*alt=strrchr(paths[i],'\\');
        if(alt && (!base || alt>base)) base=alt;
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native map archive %u: %s (rank %d)\n",
            (unsigned)i,base ? base+1 : paths[i],ranks[i]);
    }
    LogMemoryCheckpoint("map archives mounted");
    return 1;
}
int LoadMapArchives(const char *map,const char *zone_type)
{
    return LoadMapArchivesEx(map,session_map_name[0] ? session_map_name : NULL,zone_type);
}

void UnusedAssetReport(void (*print)(const char *line),int per_zone);
static void unused_print(const char *line) { Com_Printf(CON_CHANNEL_DONT_FILTER,"%s\n",line); }
/* sh1x_unused [zones]: mounted versus touched assets, per type (and per zone with "zones"). */
static void Unused_f(void)
{
    UnusedAssetReport(unused_print,Cmd_Argc()>1 && !strcmp(Cmd_Argv(1),"zones"));
}

/* A memory checkpoint line: process private bytes and working set, and the C runtime heap in use (sh1x_unused's
 * companion: where private memory goes between the mounts, the level load and the match). */
void LogMemoryCheckpoint(const char *what)
{
    size_t priv=0,ws=0,used=0,unused=0;
    ProcessMemory(&priv,&ws,&used,&unused);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL memory checkpoint %s: private %.1f MB, working set %.1f MB, heap in use %.1f MB (%.1f MB free)\n",
               what ? what : "",priv/1048576.0,ws/1048576.0,used/1048576.0,unused/1048576.0);
}
static void MemCheckpoint_f(void) { LogMemoryCheckpoint(Cmd_Argc()>1 ? Cmd_Argv(1) : "now"); }

/* sh1x_imagecheck: every zone image page (freeZoneImages) against its CRC from the release. */
static void ImageCheck_f(void)
{
    unsigned written=0,n,i; const zone_image_event_t *ev=NULL; zone_image_stats_t z;
    ZoneImageCheck(&written,NULL);
    ZoneImageStats(&z);
    Com_Printf(CON_CHANNEL_DONT_FILTER,"sh1x_imagecheck: %u zones, %u pages differ from the release (%u checked in all)\n",
               z.regions,written,(unsigned)z.checked_pages);
    n=ZoneImageEvents(&ev);
    for(i=0;i<n;++i)
        Com_Printf(CON_CHANNEL_DONT_FILTER,"sh1x_imagecheck:   zone of %u bytes, page 0x%x, asset %d type %d\n",(unsigned)ev[i].zone_size,
                   (unsigned)ev[i].offset,(int)ev[i].asset_index,(int)ev[i].asset_type);
}

/* sh1x_assetinfo <36|37> <name>: the size of the server's C1-layout rawfile or string table, so a probe can see
 * which zone's definition won (e.g. the shared patch zone's mp/comaplist.csv). */
static void AssetInfo_f(void)
{
    const char *name=Cmd_Argv(2); int type=atoi(Cmd_Argv(1));
    if(type==36) {
        const C1RawFile *r=NULL; int st=LoadC1RawFile(name,&r);
        Com_Printf(CON_CHANNEL_DONT_FILTER,"sh1x_assetinfo 36 %s: %s len %d compressed %d\n",name,st==1?"found":"absent",st==1?r->len:0,st==1?r->compressedLen:0);
    } else if(type==37) {
        const C1StringTable *t=NULL; int st=LoadC1StringTable(name,&t);
        Com_Printf(CON_CHANNEL_DONT_FILTER,"sh1x_assetinfo 37 %s: %s %d columns x %d rows\n",name,st==1?"found":"absent",st==1?t->columnCount:0,st==1?t->rowCount:0);
    } else Com_Printf(CON_CHANNEL_DONT_FILTER,"sh1x_assetinfo <36|37> <name>\n");
}

static void LoadZones_f(void)
{
    int count = Cmd_Argc(), i;
    const char **paths;
    const char *exe;
    size_t failed;
    unsigned asset;
    if (count < 3) {
        Com_Printf(CON_CHANNEL_SYSTEM, "loadzones <client exe|- for inflated zones> <base archive> ... <patch archive>\n");
        return;
    }
    paths = calloc((size_t)count-2, sizeof(*paths));
    if (!paths) { Com_PrintError(CON_CHANNEL_SYSTEM, "CoDOL archive set allocation failed\n"); return; }
    for (i = 2; i < count; ++i) paths[i-2] = Cmd_Argv(i);
    exe = Cmd_Argv(1);
    if (!strcmp(exe, "-")) exe = NULL;
    if (MountRuntimeArchives(paths, (size_t)count-2, exe, &failed, &asset))
        Com_Printf(CON_CHANNEL_SYSTEM, "CoDOL archive set mounted with loader blocks: %i archives\n", count-2);
    else if (asset==UINT32_MAX)
        Com_PrintError(CON_CHANNEL_SYSTEM, "CoDOL archive set unchanged; initialization failed: %s\n", paths[failed]);
    else
        Com_PrintError(CON_CHANNEL_SYSTEM, "CoDOL archive set unchanged; failed: %s, asset %u\n", paths[failed], asset);
    free(paths);
}

static void LoadZone_f(void)
{
    int count = Cmd_Argc();
    const char *path;
    unsigned asset;
    if (count != 2 && count != 3) {
        Com_Printf(CON_CHANNEL_SYSTEM, "loadzone <inflated.zone> OR <file.ff> <client exe>\n");
        return;
    }
    path=Cmd_Argv(1);
    if (MountRuntimeArchives(&path,1,count == 3 ? Cmd_Argv(2) : NULL,NULL,&asset))
        Com_Printf(CON_CHANNEL_SYSTEM, "CoDOL archive mounted with loader blocks: %s\n", path);
    else if (asset==UINT32_MAX)
        Com_PrintError(CON_CHANNEL_SYSTEM, "CoDOL archive initialization failed: %s\n", path);
    else
        Com_PrintError(CON_CHANNEL_SYSTEM, "CoDOL archive failed validation: %s, asset %u\n", path,asset);
}

static void CheckConstants_f(void)
{
    const engine_table_t *table=NULL; unsigned i; int status;
    if(Cmd_Argc()!=2) { Com_Printf(CON_CHANNEL_SYSTEM,"Usage: checkconstants <table name>\n"); return; }
    status=LoadEngineStringTable(Cmd_Argv(1),&table);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL constant table: %d, %d rows, %d columns\n",status,
        table ? table->rowCount : 0,table ? table->columnCount : 0);
    if(status==1 && table->columnCount==2) {
        configstrings_t inherited={0},overrides={0};
        int ready=ImportConstantConfigstrings(&inherited,(size_t)table->rowCount,table->values);
        int white=ready ? RegisterHudMaterial(&overrides,&inherited,"white") : -1;
        Com_Printf(CON_CHANNEL_SYSTEM,"  inherited bank: %d, %u entries; white material %d, %u overrides\n",
            ready,inherited.count,white,overrides.count);
        ClearConfigstrings(&overrides); ClearConfigstrings(&inherited);
        for(i=0;i<(unsigned)table->rowCount && i<8;++i)
            Com_Printf(CON_CHANNEL_SYSTEM,"  constant row: '%s' = '%s'\n",
                table->values[2*i] ? table->values[2*i] : "<null>",
                table->values[2*i+1] ? table->values[2*i+1] : "<null>");
    }
}
static void CheckXAnim_f(void)
{
    unsigned bones=0; int status;
    if(Cmd_Argc()!=2) { Com_Printf(CON_CHANNEL_SYSTEM,"Usage: checkxanim <name>\n"); return; }
    status=CheckEngineXAnim(Cmd_Argv(1),&bones);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL animation evaluation: %s, status %d, %u bones, %u finite samples (identity map; no client-pose comparison)\n",
        Cmd_Argv(1),status,bones,status==1 ? 5u : 0u);
}
static void CheckModel_f(void)
{
    const mounted_model_skeleton_t *skeleton=NULL;
    const mounted_model_geometry_t *geometry=NULL;
    const xmodel_materials_t *slots=NULL;
    native_model_mesh_t mesh={0}; native_model_core_t core={0};
    const XModel *model=NULL;
    if(Cmd_Argc()!=2) { Com_Printf(CON_CHANNEL_SYSTEM,"Usage: checkmodel <name>\n"); return; }
    const char *name=Cmd_Argv(1);
    int sk=LoadMountedXModelSkeleton(name,&skeleton);
    int ge=LoadMountedXModelGeometry(name,&geometry);
    int ma=LoadMountedModelMaterials(name,&slots);
    int me=LoadNativeModelMesh(name,&mesh);
    int co=LoadNativeModelCore(name,&core);
    int ready=LoadEnginePhysicsXModel(name,&model);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL model %s: skeleton %d, geometry %d, materials %d, mesh %d, core %d, complete %d\n",name,sk,ge,ma,me,co,ready);
    /* Bone names are what gettagorigin/linkto/playfxontag resolve against; a
     * script tag that is not here is a fatal script error on this model. */
    if(sk==1) {
        Com_Printf(CON_CHANNEL_SYSTEM,"  bones: %u (%u unresolved)\n",skeleton->bone_names.count,skeleton->bone_names.unresolved);
        for(unsigned i=0;i<skeleton->bone_names.count;++i)
            Com_Printf(CON_CHANNEL_SYSTEM,"  bone %u: %s\n",i,
                skeleton->bone_names.names[i].text ? skeleton->bone_names.names[i].text : "<unresolved>");
    }
    if(ma==1) for(unsigned i=0;i<slots->count;++i) {
        mounted_model_material_t source={0}; const Material_t *material=NULL;
        int found=LoadMountedModelMaterialSource(name,i,&source);
        int native=found==1 ? LoadEngineMaterial(source.name,&material) : 0;
        Com_Printf(CON_CHANNEL_SYSTEM,"  material %u: source %d, %s, native %d\n",i,found,found==1 ? source.name : "<missing>",native);
    }
    if(co==1) for(unsigned i=0;i<2;++i) {
        mounted_model_physics_t dependency={0}; const struct PhysPreset *preset=NULL; const PhysGeomList_t *map=NULL;
        unsigned ref=i ? core.physics_map_reference : core.physics_preset_reference;
        int found=ref ? LoadMountedModelPhysics(name,i,&dependency) : 0;
        int native=found==1 ? (i ? LoadEnginePhysicsMap(dependency.name,&map) : LoadEnginePhysicsPreset(dependency.name,&preset)) : 0;
        Com_Printf(CON_CHANNEL_SYSTEM,"  physics %u: reference %x, source %d, %s, native %d\n",i,ref,found,found==1 ? dependency.name : "<none>",native);
        if(i && found==1 && native!=1) {
            const physics_map_t *source=NULL;
            int decoded=LoadMountedPhysicsMap(dependency.name,&source);
            Com_Printf(CON_CHANNEL_SYSTEM,"    physics map decode: %d\n",decoded);
            if(decoded==1) for(unsigned g=0;g<source->count;++g) {
                const physics_geometry_t *shape=source->geometries+g;
                unsigned type=(unsigned)shape->record[4]|(unsigned)shape->record[5]<<8|(unsigned)shape->record[6]<<16|(unsigned)shape->record[7]<<24;
                Com_Printf(CON_CHANNEL_SYSTEM,"    geometry %u: type %u, brush %d, %u sides, %u adjacency bytes\n",g,type,shape->brush!=NULL,shape->side_count,shape->adjacency_size);
            }
        }
    }
    FreeNativeModelCore(&core); FreeNativeModelMesh(&mesh);
}
static void CheckWeapon_f(void)
{
    mounted_weapon_t weapon={0}; int status; unsigned i;
    if (Cmd_Argc()!=2) { Com_Printf(CON_CHANNEL_SYSTEM,"Usage: checkweapon <name>\n"); return; }
    status=LoadMountedWeapon(Cmd_Argv(1),&weapon);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL weapon source: %s, status %d, %u bytes (not a native WeaponDef)\n",
        Cmd_Argv(1),status,(unsigned)weapon.asset.size);
    if (status==1) for (i=0;i<4;++i)
        Com_Printf(CON_CHANNEL_SYSTEM,"  header string %u: %s\n",i,weapon.strings[i] ? weapon.strings[i] : "<null>");
    /* C1 rocket spawn 530e9d compares WeaponDef +0x8e0 (guidedMissileType). */
    if (status==1 && weapon.asset.size>=0x8e4) {
        uint32_t guided;
        memcpy(&guided,weapon.asset.data+0x8e0,sizeof(guided));
        Com_Printf(CON_CHANNEL_SYSTEM,"  source guidedMissileType (+0x8e0): %u\n",(unsigned)guided);
    }
    /* Lock-on bytes: maxSteeringAccel +0x8e4, projIgnitionDelay +0x8e8, the
     * warning gate +0xbbd (C1 0x41b9fb), requireLockonToFire +0xbbe (C1
     * 0x41b9ad) and noAdsWhenMagEmpty +0xbc0 (C1 BG_PlayerCanAds 0x4181ff). */
    if (status==1 && weapon.asset.size>=0xbc1) {
        float steering; int32_t ignition;
        memcpy(&steering,weapon.asset.data+0x8e4,sizeof(steering));
        memcpy(&ignition,weapon.asset.data+0x8e8,sizeof(ignition));
        Com_Printf(CON_CHANNEL_SYSTEM,"  source maxSteeringAccel %g, projIgnitionDelay %d, lock warning gate (+0xbbd) %u, requireLockonToFire (+0xbbe) %u, noAdsWhenMagEmpty (+0xbc0) %u\n",
            steering,(int)ignition,(unsigned)weapon.asset.data[0xbbd],(unsigned)weapon.asset.data[0xbbe],
            (unsigned)weapon.asset.data[0xbc0]);
    }
    /* Dropped-item bytes: inventoryType (5 = scavenger), avoidDropCleanup
     * (C1 ring picker 0x5286f9) and noAmmoPickup (scavenger check 0x40c5d0). */
    if (status==1 && weapon.asset.size>=0xbcf) {
        uint32_t inventory;
        memcpy(&inventory,weapon.asset.data+0x34c,sizeof(inventory));
        Com_Printf(CON_CHANNEL_SYSTEM,"  source inventoryType (+0x34c): %u, avoidDropCleanup (+0xbc1): %u, noAmmoPickup (+0xbce): %u\n",
            (unsigned)inventory,(unsigned)weapon.asset.data[0xbc1],(unsigned)weapon.asset.data[0xbce]);
    }
    if (status==1) {
        struct WeaponDef native={0}; weapon_timing_extensions_t extra={0};
        weapon_hide_tags_t hide_tags={0}; unsigned hide_count=0;
        int hide_status=LoadMountedWeaponHideTags(Cmd_Argv(1),&hide_tags);
        for(i=0;i<32;++i) if(hide_tags.names[i]) ++hide_count;
        Com_Printf(CON_CHANNEL_SYSTEM,"  source hide tags: %d, %u populated of 32 (archive strings)\n",hide_status,hide_count);
        weapon_visuals_t visuals={0}; unsigned visual_count=0;
        int visual_status=LoadMountedWeaponVisuals(Cmd_Argv(1),&visuals);
        if(visual_status==1) for(i=0;i<14;++i) if(visuals.entries[i].asset.data) ++visual_count;
        Com_Printf(CON_CHANNEL_SYSTEM,"  weapon middle visuals: %d, %u populated of 14, next byte %u\n",
            visual_status,visual_count,(unsigned)visuals.dependency_offset);
        weapon_tail_t tail={0}; int tail_status=LoadMountedWeaponTail(Cmd_Argv(1),&tail);
        native_weapon_graphs_t graphs={0};
        int graph_status=tail_status==1 && BindNativeWeaponGraphs(&tail,&native,&graphs);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native accuracy graphs: %d, active %d/%d, original %d/%d\n",graph_status,
            native.accuracyGraphKnotCount[0],native.accuracyGraphKnotCount[1],
            native.originalAccuracyGraphKnotCount[0],native.originalAccuracyGraphKnotCount[1]);
        Com_Printf(CON_CHANNEL_SYSTEM,"  weapon tail: %d, %u fields, final offset %u/%u\n",tail_status,tail.count,
            (unsigned)tail.dependency_offset,(unsigned)weapon.asset.size);
        weapon_animations_t animations={0}; unsigned populated=0,empty=0;
        int animation_status=LoadMountedWeaponAnimations(Cmd_Argv(1),&animations);
        if (animation_status==1) for (i=0;i<144;++i) {
            if (animations.names[i] && *animations.names[i]) ++populated;
            else if (animations.names[i]) ++empty;
        }
        Com_Printf(CON_CHANNEL_SYSTEM,"  animation names: status %d, %u populated, %u empty, next dependency byte %u (source slots)\n",
            animation_status,populated,empty,(unsigned)animations.dependency_offset);
        if(animation_status==1) {
            unsigned resolved=0,missing=0,invalid=0,bound=0,constructed=0;
            for(i=0;i<144;++i) if(animations.names[i] && *animations.names[i]) {
                mounted_xanim_t animation; int found=LoadMountedXAnim(animations.names[i],&animation);
                if(found==1) ++resolved; else if(found==0) ++missing; else ++invalid;
                if(found==1) {
                    const xanim_handles_t *handles;
                    if(LoadEngineXAnimHandles(animations.names[i],&handles)==1) {
                        native_xanim_t native={0}; ++bound;
                        if(BuildNativeXAnim(&animation,handles,&native)) ++constructed;
                        FreeNativeXAnim(&native);
                    }
                }
            }
            Com_Printf(CON_CHANNEL_SYSTEM,"  animation source assets: %u resolved, %u missing, %u invalid (not native XAnimParts)\n",resolved,missing,invalid);
            Com_Printf(CON_CHANNEL_SYSTEM,"  animation script-string bindings: %u ready\n",bound);
            Com_Printf(CON_CHANNEL_SYSTEM,"  native animation payloads: %u constructed (not published or evaluated)\n",constructed);
        }
        if (animation_status==1 && ApplyNativeWeaponAnimationNames(&animations,&native))
            Com_Printf(CON_CHANNEL_SYSTEM,"  shared animation roles mapped: 19; drop %s, raise %s (names only, not activated)\n",
                native.szXAnims[15] ? native.szXAnims[15] : "<null>",
                native.szXAnims[13] ? native.szXAnims[13] : "<null>");
        const XModel *bindings[11]={0};
        weapon_effects_t effects={0}; unsigned source_effects=0,native_effects=0;
        int effect_status=LoadMountedWeaponEffects(Cmd_Argv(1),&effects);
        if (effect_status==1) for (i=0;i<2;++i) if (effects.names[i]) {
            const FxEffectDef *effect;
            int ready;
            ++source_effects;
            ready=LoadEngineNativeEffect(effects.names[i],&effect);
            if (ready==1) ++native_effects;
            Com_Printf(CON_CHANNEL_SYSTEM,"  leading effect slot %u: %s, native status %d\n",i,effects.names[i],ready);
            if(ready!=1) audit_effect_graph(effects.names[i]);
        }
        Com_Printf(CON_CHANNEL_SYSTEM,"  leading effects: status %d, %u source, %u native; next dependency byte %u (not activated)\n",
            effect_status,source_effects,native_effects,(unsigned)effects.dependency_offset);
        unsigned models=0,native_models=0,failed=0;
        weapon_sounds_t sounds={0}; unsigned sound_count=0;
        int sound_status=LoadMountedWeaponSounds(Cmd_Argv(1),&sounds);
        if(sound_status==1) for(i=0;i<53;++i) if(sounds.names[i] && *sounds.names[i]) ++sound_count;
        Com_Printf(CON_CHANNEL_SYSTEM,"  leading sound names: status %d, %u populated; next dependency byte %u (not native aliases)\n",
            sound_status,sound_count,(unsigned)sounds.dependency_offset);
        if(sound_status==1) {
            unsigned array,counts[2]={0};
            for(array=0;array<2;++array) for(i=0;i<31;++i)
                if(sounds.arrays[array][i] && *sounds.arrays[array][i]) ++counts[array];
            Com_Printf(CON_CHANNEL_SYSTEM,"  sound arrays: %u/%u populated names\n",counts[0],counts[1]);
        }
        for (i=0;i<11;++i) {
            weapon_model_t model; const XModel *native_model;
            int found=LoadMountedWeaponModel(Cmd_Argv(1),i,&model);
            if (found==1) {
                ++models;
                Com_Printf(CON_CHANNEL_SYSTEM,"  leading model slot %u: %s\n",i,model.name);
                if(LoadEnginePhysicsXModel(model.name,&native_model)==1) { ++native_models; bindings[i]=native_model; } else ++failed;
            }
            else if (found<0) ++failed;
        }
        Com_Printf(CON_CHANNEL_SYSTEM,"  leading model slots: %u source, %u native, %u failed; native role binding %d (not activated)\n",
            models,native_models,failed,!failed && BindNativeWeaponModels(&weapon,bindings,&native));
        int ready=ApplyNativeWeaponTimings(&weapon,&native,&extra);
        int ammo=ApplyNativeWeaponAmmo(&weapon,&native);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native reload schedule: %d, segmented %d, add %d, start duration/add %d/%d; extra add/empty/empty-alt %d/%d/%d\n",
            ready && ammo,native.bSegmentedReload,native.iReloadAddTime,native.iReloadStartTime,native.iReloadStartAddTime,
            extra.reload_add_variant,extra.reload_empty_add,extra.reload_empty_add_variant);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native reload transfer caps: %d, ordinary/start %d/%d\n",
            ammo,native.iReloadAmmoAdd,native.iReloadStartAdd);
        int melee=ApplyNativeWeaponMelee(&weapon,&native);
        int damage=ApplyNativeWeaponDamage(&weapon,&native);
        int firemode=ApplyNativeWeaponFireMode(&weapon,&native);
        int ads=ApplyNativeWeaponAds(&weapon,&native);
        int movement=ApplyNativeWeaponMovement(&weapon,&native);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native movement scales: %d, hip/ADS/sprint %.9g/%.9g/%.9g\n",
            movement,native.moveSpeedScale,native.adsMoveSpeedScale,native.sprintDurationScale);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native ADS fields: %d, enabled/rechamber %d/%d, in/out rates %.9g/%.9g per ms\n",
            ads,native.aimDownSight,native.bRechamberWhileAds,native.fOOPosAnimLength[0],native.fOOPosAnimLength[1]);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native firing mode: %d, type %d, rechamber %d (base definition only)\n",
            firemode,(int)native.fireType,native.bBoltAction);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native damage fields: %d, base/min %d/%d, player %d/%d, falloff %g/%g (base definition only)\n",
            damage,native.damage,native.minDamage,native.playerDamage,native.minPlayerDamage,native.fMaxDamageRange,native.fMinDamageRange);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native melee fields: %d, damage %d, delay/time %d/%d, charge delay/time %d/%d (base definition only)\n",
            melee,native.iMeleeDamage,native.iMeleeDelay,native.iMeleeTime,native.meleeChargeDelay,native.meleeChargeTime);
        weapon_ammo_names_t ammo_names={0};
        int ammo_name_status=LoadMountedWeaponAmmoNames(Cmd_Argv(1),&ammo_names);
        Com_Printf(CON_CHANNEL_SYSTEM,"  ammo names: status %d; ammo '%s', clip '%s', shared cap '%s'; next dependency byte %u (indices unregistered)\n",
            ammo_name_status,ammo_names.ammo ? ammo_names.ammo : "<null>",ammo_names.clip ? ammo_names.clip : "<null>",
            ammo_names.shared_cap ? ammo_names.shared_cap : "<null>",(unsigned)ammo_names.dependency_offset);
        if(ammo_name_status==1 && ApplyNativeWeaponAmmoNames(&ammo_names,weapon.strings[0],&native))
            Com_Printf(CON_CHANNEL_SYSTEM,"  native ammo/clip keys: '%s' / '%s' (not registered)\n",native.szAmmoName,native.szClipName);
        int spread=ApplyNativeWeaponSpread(&weapon,&native);
        native_weapon_core_t core={0};
        int core_status=LoadEngineWeaponCore(Cmd_Argv(1),&core);
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  native sprint in/out: %d/%d ms; retained dive in/out: %d/%d ms\n",
            core.definition.sprintInTime,core.definition.sprintOutTime,core.timings.dive_in,core.timings.dive_out);
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  native recoil: reduced bullets ADS/hip %d/%d, gun pitch ADS %g/%g hip %g/%g\n",
            core.definition.adsGunKickReducedKickBullets,core.definition.hipGunKickReducedKickBullets,
            core.definition.fAdsGunKickPitchMin,core.definition.fAdsGunKickPitchMax,
            core.definition.fHipGunKickPitchMin,core.definition.fHipGunKickPitchMax);
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  native starting ammo/first raise: %d / %d ms\n",
            core.definition.iStartAmmo,core.definition.iFirstRaiseTime);
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  native shared ammo cap/clip-only: %d / %d (indices unregistered)\n",
            core.definition.iSharedAmmoCap,core.definition.bClipOnly);
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  native hit-location multipliers: head %g, upper torso %g, gun %g; retained shield %g\n",
            core.definition.locationDamageMultipliers[2],core.definition.locationDamageMultipliers[4],
            core.definition.locationDamageMultipliers[18],core.shield_damage_multiplier);
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  native weapon penetration/offhand: %d/%d\n",
            (int)core.definition.penetrateType,(int)core.definition.offhandClass);
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  native weapon type/inventory/class: %d/%d/%d\n",
            (int)core.definition.weapType,(int)core.definition.inventoryType,(int)core.definition.weapClass);
        unsigned core_anims=0;
        if(core_status==1) for(i=0;i<144;++i) if(core.animation_parts[i]) ++core_anims;
        Com_Printf(CON_CHANNEL_SYSTEM,"  composed native weapon core: %d (registration and remaining fields pending)\n",core_status);
        Com_Printf(CON_CHANNEL_SYSTEM,"  composed weapon native animations: %u populated slots of 144\n",core_anims);
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  composed weapon muzzle flashes: view %s, world %s\n",
            core.definition.viewFlashEffect ? core.definition.viewFlashEffect->name : "<null>",
            core.definition.worldFlashEffect ? core.definition.worldFlashEffect->name : "<null>");
        if(core_status==1) Com_Printf(CON_CHANNEL_SYSTEM,"  composed weapon brass effects: %s / %s; last shot %s / %s\n",
            core.definition.viewShellEjectEffect ? core.definition.viewShellEjectEffect->name : "<null>",
            core.definition.worldShellEjectEffect ? core.definition.worldShellEjectEffect->name : "<null>",
            core.definition.viewLastShotEjectEffect ? core.definition.viewLastShotEjectEffect->name : "<null>",
            core.definition.worldLastShotEjectEffect ? core.definition.worldLastShotEjectEffect->name : "<null>");
        FreeNativeWeaponCore(&core);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native spread fields: %d; standing %g/%g, ADS %g (not activated)\n",
            spread,native.fHipSpreadStandMin,native.hipSpreadStandMax,native.fAdsSpread);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native ammo fields: %d; maximum %d, clip %d, shots %d, fire time %d (indices unregistered)\n",
            ammo,native.iMaxAmmo,native.iClipSize,native.shotCount,native.iFireTime);
        Com_Printf(CON_CHANNEL_SYSTEM,"  native timing fields: %d; reload %d/%d, rechamber %d, drop/raise %d/%d; variants %d/%d/%d (not activated)\n",
            ready,native.iReloadTime,native.iReloadEmptyTime,native.iRechamberTime,native.iDropTime,native.iRaiseTime,
            extra.rechamber_variant,extra.reload_variant,extra.reload_empty_variant);
        FreeNativeWeaponGraphs(&graphs);
    }
}
/* dumplocalize [file]: every mounted LocalizeEntry as KEY<TAB>text, the
 * text as the zone's raw GBK bytes with \\ \t \n \r escaped, in mount order
 * (the first line for a name is the text C1 shows). Written under fs_homepath,
 * default codol_localize.tsv. tools/english/make_table.py reads it as
 * sources/native_localize.tsv: the native loader resolves the zones'
 * de-duplicated name references exactly, where the Python zone reader only
 * guesses them, and its guesses had paired most announcer subtitle names
 * (SUBTITLE_*_1MC_*) with another line's text. */
typedef struct { fileHandle_t file; int written; int failed; } localize_dump_t;
static int DumpLocalizeLine(void *context,const char *name,const char *value)
{
    localize_dump_t *dump=(localize_dump_t *)context;
    char line[4096]; size_t n=0; const char *c;
    for(c=name;*c && n<sizeof(line)-8;++c) line[n++]=*c;
    line[n++]='\t';
    for(c=value;*c && n<sizeof(line)-8;++c) {
        char e=*c=='\\' ? '\\' : *c=='\t' ? 't' : *c=='\n' ? 'n' : *c=='\r' ? 'r' : 0;
        if(e) { line[n++]='\\'; line[n++]=e; } else line[n++]=*c;
    }
    if(*c) { dump->failed=1; return 0; }
    line[n++]='\n';
    if(FS_Write(line,(int)n,dump->file)!=(int)n) { dump->failed=1; return 0; }
    ++dump->written; return 1;
}
static void DumpLocalize_f(void)
{
    const char *file=Cmd_Argc()>1 ? Cmd_Argv(1) : "codol_localize.tsv";
    localize_dump_t dump={0};
    const char *c;
    int visited;
    for(c=file;*c;++c)
        if(!((*c>='a' && *c<='z') || (*c>='A' && *c<='Z') || (*c>='0' && *c<='9') ||
             *c=='_' || *c=='-' || *c=='.')) break;
    if(!*file || *c || file[0]=='.') {
        Com_Printf(CON_CHANNEL_SYSTEM,"usage: dumplocalize [file name under fs_homepath]\n");
        return;
    }
    dump.file=FS_SV_FOpenFileWrite(file);
    if(!dump.file) { Com_Printf(CON_CHANNEL_SYSTEM,"dumplocalize: cannot write %s\n",file); return; }
    visited=EnumerateMountedLocalize(DumpLocalizeLine,&dump);
    FS_FCloseFile(dump.file);
    if(visited<0 || dump.failed)
        Com_Printf(CON_CHANNEL_SYSTEM,"dumplocalize: stopped after %d entries (write failed or text too long)\n",dump.written);
    else
        Com_Printf(CON_CHANNEL_SYSTEM,"dumplocalize: %d localize entries written to %s\n",dump.written,file);
}
static void CheckWeaponRegistry_f(void)
{
    const weapon_registry_t *registry;
    unsigned count=0; int ammo,clip;
    if(!RefreshServerWeaponRegistry()) {
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL weapon registry unavailable: %s\n",ServerWeaponRegistryError());
        return;
    }
    registry=ServerWeaponRegistry();
    while(GetWeaponKeys(registry,count,&ammo,&clip)) ++count;
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL weapon registry ready: %u definitions from published/inherited names\n",count);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL native weapon mapping: %s\n",
        RefreshServerWeaponMappings() ? "ready" : "unresolved native definitions");
}
static void CheckStructured_f(void)
{
    structured_schema_t schema={0}; unsigned i,j,k;
    int status=LoadMountedStructuredSchema(Cmd_Argv(1),&schema);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL structured schema status %d\n",status);
    if(status==1) for(i=0;i<schema.count;++i) {
        const structured_definition_t *d=schema.definitions+i;
        Com_Printf(CON_CHANNEL_SYSTEM,"  %s version %u size %u structs %u\n",schema.name,d->version,(unsigned)d->layout.record_size,d->struct_count);
        if(Cmd_Argc()>2) {
            structured_path_t path[32]; structured_field_t field;
            int count=Cmd_Argc()-2,valid=count<=32,n;
            for(n=0;valid && n<count;++n) {
                const char *arg=Cmd_Argv(n+2);
                path[n]=(structured_path_t){arg,0};
                if(arg[0]=='#') {
                    const char *digit=arg+1;uint64_t index=0;
                    if(!*digit)valid=0;
                    for(;valid && *digit;++digit) {
                        if(*digit<'0' || *digit>'9') {valid=0;break;}
                        index=index*10+(unsigned)(*digit-'0');
                        if(index>INT32_MAX)valid=0;
                    }
                    path[n]=(structured_path_t){NULL,(int32_t)index};
                }
            }
            if(valid && ResolveStructuredField(d,path,(size_t)count,&field))
                Com_Printf(CON_CHANNEL_SYSTEM,"  resolved type %u/%u logical %u mapped %u\n",
                    field.type.kind,field.type.index,field.logical_index,field.mapped_address);
            else Com_Printf(CON_CHANNEL_SYSTEM,"  structured path rejected\n");
            continue;
        }
        for(j=0;j<d->struct_count;++j) for(k=0;k<d->structs[j].count;++k) {
            const structured_property_t *p=d->structs[j].properties+k;
            Com_Printf(CON_CHANNEL_SYSTEM,"  struct %u field %s type %u/%u logical %u\n",j,p->name,p->type.kind,p->type.index,p->logical_index);
        }
    }
    FreeStructuredSchema(&schema);
}
static void CheckProfile_f(void)
{
    structured_schema_t schema={0};structured_path_t path;
    const char *digit=Cmd_Argv(1);unsigned client=0;uint32_t value;
    if(Cmd_Argc()!=4 || !*digit) {
        Com_Printf(CON_CHANNEL_SYSTEM,"usage: checkprofile <client> <schema> <root scalar field>\n");return;
    }
    for(;*digit;++digit) {
        if(*digit<'0' || *digit>'9' || client>3) {
            Com_Printf(CON_CHANNEL_SYSTEM,"invalid profile client\n");return;
        }
        client=client*10+(unsigned)(*digit-'0');
    }
    if(client>=32) {Com_Printf(CON_CHANNEL_SYSTEM,"invalid profile client\n");return;}
    path=(structured_path_t){Cmd_Argv(3),0};
    if(LoadMountedStructuredSchema(Cmd_Argv(2),&schema)==1 &&
       ClientProfileScalar(client,&schema,&path,1,&value))
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL profile client %u field %s bits %u\n",client,path.name,value);
    else Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL profile query rejected\n");
    FreeStructuredSchema(&schema);
}
/* Explicit operator-selected storage record. This diagnostic does not bind a
 * connection's claimed XUID to an account or expose profile data to GSC. */
static void CheckChallenge_f(void)
{
    structured_schema_t schema={0};const engine_table_t *table=NULL;
    uint64_t uid=0;const char *p;uint8_t *profile;int stage,status;
    if(Cmd_Argc()!=4 || !*(p=Cmd_Argv(2))) {
        Com_Printf(CON_CHANNEL_SYSTEM,"usage: checkchallenge <profile directory> <decimal uid> <challenge>\n");return;
    }
    for(;*p;++p) {
        unsigned digit=(unsigned char)*p-'0';
        if(digit>9 || uid>(UINT64_MAX-digit)/10) {
            Com_Printf(CON_CHANNEL_SYSTEM,"invalid profile uid\n");return;
        }
        uid=uid*10+digit;
    }
    profile=malloc(32768);if(!profile)return;
    status=ReadSecureProfile(Cmd_Argv(1),uid,1,NULL,profile,32768);
    if(status==1 && LoadMountedStructuredSchema("mp/playersecuredata.def",&schema)==1 &&
       LoadEngineStringTable("mp/allChallengesTable.csv",&table)==1 &&
       ProfileChallengeState(profile,32768,&schema,table,Cmd_Argv(3),&stage))
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL stored challenge %s IW4 stage %d\n",Cmd_Argv(3),stage);
    else Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL stored challenge query rejected (storage %d)\n",status);
    FreeStructuredSchema(&schema);free(profile);
}
static void CheckExperience_f(void)
{
    structured_schema_t schema={0};const engine_table_t *table=NULL;
    uint8_t *profile;int stored_rank,total,rank,status;
    if(Cmd_Argc()!=2) {
        Com_Printf(CON_CHANNEL_SYSTEM,"usage: checkexperience <profile directory containing mpdata_s>\n");return;
    }
    profile=malloc(32768);if(!profile)return;
    status=ReadSecureProfile(Cmd_Argv(1),0,0,NULL,profile,32768);
    if(status==1 && LoadMountedStructuredSchema("mp/playersecuredata.def",&schema)==1 &&
       LoadEngineStringTable("mp/rankTable.csv",&table)==1 &&
       ProfileExperience(profile,32768,&schema,table,&stored_rank,&total) &&
       RankForExperience(table,total,&rank))
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL stored rank %d total XP %d experience rank %d\n",stored_rank,total,rank);
    else Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL stored experience query rejected (storage %d)\n",status);
    FreeStructuredSchema(&schema);free(profile);
}
static void CheckUnlock_f(void)
{
    structured_schema_t schema={0};
    const engine_table_t *unlocks=NULL,*ranks=NULL,*challenges=NULL;
    uint8_t *profile;int status,unlocked; cvar_t *root;
    if(Cmd_Argc()!=3) {
        Com_Printf(CON_CHANNEL_SYSTEM,"usage: checkunlock <directory containing mpdata_s> <item>\n");return;
    }
    profile=malloc(32768);if(!profile)return;
    status=ReadSecureProfile(Cmd_Argv(1),0,0,NULL,profile,32768);
    if(status!=1 || LoadMountedStructuredSchema("mp/playersecuredata.def",&schema)!=1)goto reject;
    status=LoadEngineStringTable("mp/unlocktable.csv",&unlocks);
    root=Cvar_FindVar("scriptPath");
    if(!status && root)status=LoadReferenceStringTable(root->string,"mp/unlocktable.csv",&unlocks);
    if(status!=1 || LoadEngineStringTable("mp/rankTable.csv",&ranks)!=1 ||
       LoadEngineStringTable("mp/allChallengesTable.csv",&challenges)!=1 ||
       !ProfileItemUnlocked(profile,32768,&schema,unlocks,ranks,challenges,Cmd_Argv(2),&unlocked))goto reject;
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL stored item %s unlocked %d\n",Cmd_Argv(2),unlocked);
    goto done;
reject:
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL stored unlock query rejected\n");
done:
    FreeStructuredSchema(&schema);free(profile);
}
static void BindProfile_f(void)
{
    const char *p;unsigned client=0;uint64_t uid=0;uint8_t *profile;
    structured_schema_t schema={0};int ok=0;
    if(Cmd_Argc()!=4 || !*(p=Cmd_Argv(1)))goto usage;
    for(;*p;++p){if(*p<'0' || *p>'9' || client>3)goto usage;client=client*10+(*p-'0');}
    if(client>=32 || svs.clients[client].state<CS_CONNECTED)goto usage;
    p=Cmd_Argv(3);if(!*p)goto usage;
    for(;*p;++p){unsigned d=(unsigned char)*p-'0';if(d>9 || uid>(UINT64_MAX-d)/10)goto usage;uid=uid*10+d;}
    if(!uid)goto usage;
    profile=malloc(32768);if(!profile)return;
    if(ReadSecureProfile(Cmd_Argv(2),uid,1,NULL,profile,32768)==1 &&
       LoadMountedStructuredSchema("mp/playersecuredata.def",&schema)==1)
        ok=AttachClientSecureProfile(client,uid,profile,32768,&schema);
    if(ok) LoadClientProgress(client,uid);
    /* As the connect-time bind does (AutoBindClientSecureProfile): the
     * bound profile is what this slot arrives with, so the match report owes
     * the lobby whatever the scripts add on top. Without the row a test client
     * bound here could earn progress that no report ever carried. */
    if(ok) MatchReportClientAttached(client,uid);
    FreeStructuredSchema(&schema);free(profile);
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL secure profile binding client %u: %s\n",client,ok ? "ready" : "rejected");return;
usage:
    Com_Printf(CON_CHANNEL_SYSTEM,"usage: bindprofile <connected client 0..31> <profile directory> <nonzero decimal uid>\n");
}
static void CheckCharacterVariant_f(void)
{
    const engine_table_t *table=NULL;const char *value=NULL,*gender,*faction;
    unsigned g,f,row=0;int status;
    if(Cmd_Argc()!=7)goto usage;
    gender=Cmd_Argv(3);faction=Cmd_Argv(4);
    if(!strcmp(gender,"m"))g=0;else if(!strcmp(gender,"f"))g=1;else goto usage;
    if(!strcmp(faction,"tf"))f=0;else if(!strcmp(faction,"sc"))f=1;
    else if(!strcmp(faction,"zm"))f=2;else goto usage;
    status=LoadEngineStringTable(Cmd_Argv(1),&table);
    if(status!=1) {Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL character table unavailable (%d)\n",status);return;}
    status=CharacterVariant(table,Cmd_Argv(2),"model",g,f,Cmd_Argv(5),Cmd_Argv(6),&row,&value);
    if(status==1)Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL character variant %s: row %u model %s\n",Cmd_Argv(2),row,value);
    else Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL character variant %s: %s\n",Cmd_Argv(2),status ? "invalid" : "absent");
    return;
usage:
    Com_Printf(CON_CHANNEL_SYSTEM,"usage: checkcharvariant <table> <item> <m|f> <tf|sc|zm> <camo> <skin>\n");
}
static void CheckCharacter_f(void)
{
    structured_schema_t schema={0};const structured_definition_t *def=NULL;
    character_composition_t result;structured_field_t field;
    structured_path_t path[]={{"characterClasses",0},{NULL,0},{"slots",0},{NULL,4}};
    uint8_t *profile=NULL;char *end;unsigned long id;unsigned i,g;int ready=0;
    const char *faction,*gender;
    if(Cmd_Argc()!=6)goto usage;
    faction=Cmd_Argv(2);gender=Cmd_Argv(3);
    if(strcmp(faction,"tf") && strcmp(faction,"sc") && strcmp(faction,"zm"))goto usage;
    if(!strcmp(gender,"m"))g=0;else if(!strcmp(gender,"f"))g=1;else goto usage;
    if(strlen(Cmd_Argv(4))!=3)goto usage;
    id=strtoul(Cmd_Argv(1),&end,10);if(!*Cmd_Argv(1) || *end || !id || id>32767)goto usage;
    if(LoadMountedStructuredSchema("mp/playerdata.def",&schema)!=1)goto done;
    for(i=0;i<schema.count;++i)if(schema.definitions[i].version==220 && schema.definitions[i].checksum==0xe4fd5218)
        def=&schema.definitions[i];
    if(!def || !ResolveStructuredField(def,path,4,&field) || field.type.kind!=9)goto done;
    profile=calloc(32768,1);if(!profile)goto done;
    for(i=0;i<4;++i){profile[4+i]=(uint8_t)(def->version>>(8*i));profile[8+i]=(uint8_t)(def->checksum>>(8*i));}
    if(!StructuredWriteScalar(&def->layout,9,field.logical_index,profile+4,32764,(uint32_t)id))goto done;
    ready=ComposeProfileCharacter(profile,32768,&schema,faction,g,Cmd_Argv(4),Cmd_Argv(5),0,&result);
    if(ready)for(i=0;i<18;++i)
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL composed slot %u item %u model %s\n",i,result.items[i],result.models[i] ? result.models[i] : "<none>");
done:
    Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL synthetic character %lu faction %s name resolution %s\n",id,faction,ready ? "complete" : "rejected");
    free(profile);FreeStructuredSchema(&schema);return;
usage:
    Com_Printf(CON_CHANNEL_SYSTEM,"usage: checkcharacter <head item ID> <tf|sc|zm> <m|f> <camo> <skin> (synthetic profile; rig selector 0; names only)\n");
}
/* Explicit operator-only A/B probe, never a map-start override. These are
 * C1 renderer registration values versus the captured team lobby values. */
static void ShadowCompare_f(void)
{
    char *end; long client;
    const char *mode;
    if(Cmd_Argc()!=3) {
        Com_Printf(CON_CHANNEL_SERVER,"usage: shadowcompare <client> <defaults|lobby>\n");
        return;
    }
    client=strtol(Cmd_Argv(1),&end,10);
    mode=Cmd_Argv(2);
    if(!*Cmd_Argv(1) || *end || client<0 || client>=sv_maxclients->integer ||
       svs.clients[client].state!=CS_ACTIVE ||
       svs.clients[client].netchan.remoteAddress.type==NA_BOT ||
       (strcmp(mode,"defaults") && strcmp(mode,"lobby")))return;
    SV_GameSendServerCommand((int)client,1,
        !strcmp(mode,"defaults") ? "s 1611 \"1.5\"" : "s 1611 \"2\"");
    SV_GameSendServerCommand((int)client,1,
        !strcmp(mode,"defaults") ? "s 1612 \"4\"" : "s 1612 \"0.125\"");
    Com_Printf(CON_CHANNEL_SERVER,"CoDOL shadow comparison client %ld: %s (temporary client dvars)\n",client,mode);
}

/* Serialized CoDOL WeaponDef offsets whose value both sides of prediction
 * consume, with the C1 reader that proves the role. PMOVE marks the ones the
 * movement/bob integrators read every frame: if the server's copy of one of
 * these differs from the client's bg_weaponDefs entry, the two pmoves diverge
 * and the snapshot's forced-origin group (which also republishes ps->bobCycle)
 * yanks the view. Keep this list in step with src/online/weapon/
 * weapon_native.c, which is what actually converts these bytes. */
static const struct { unsigned offset; unsigned pmove; const char *name; } weapondef_offsets[]={
    {0x670,1,"moveSpeedScale (PM_CmdScale 0x41023f; bobCycle divisor 0x413734)"},
    {0x674,1,"adsMoveSpeedScale (PM_CmdScale 0x410266; bobCycle divisor 0x413749)"},
    {0x678,1,"sprintDurationScale (BG_GetMaxSprintTime 0x40e9e6)"},
    {0x694,1,"overlayReticle (BG_CanHoldBreath 0x421f00; scope-bob gate 0x424d90)"},
    {0x6ec,0,"adsSwayMaxAngle"},{0x6f0,0,"hipSwayMaxAngle"},
    {0x6f4,0,"adsSwaySpeed"},{0x6f8,0,"hipSwaySpeed"},
    {0x6fc,0,"swayCrouchScale"},{0x700,0,"swayProneScale"},
    {0x714,0,"adsSteadySwayDelay (BG_GetAdsSteadySwayAngle 0x422e20)"},
    {0x944,0,"zoomFov level 0 (view 0x4716ea)"},
    {0x948,1,"zoomFov level 1 (PM_UpdateVariableZoom 0x4184e2; non-zero = variable zoom)"},
    {0x94c,1,"zoomFov level 2 (PM_UpdateVariableZoom 0x4184e2)"},
    {0x718,0,"adsSteadySwayRamp (BG_GetAdsSteadySwayAngle 0x422e20)"},
    {0xa28,0,"adsInRate per ms (0x418798)"},{0xa2c,0,"adsOutRate per ms (0x4187af)"},
    {0xbc3,0,"crosshairColourGate"},{0xbc8,1,"aimDownSight (0x418655)"},
    {0xbc9,0,"bRechamberWhileAds (0x4186e1)"},{0xbcf,0,"adsFireOnly (0x4186fb)"}};
static const char *weapondef_offset_name(unsigned offset,unsigned *pmove)
{
    unsigned k;
    if(pmove) *pmove=0;
    for(k=0;k<sizeof weapondef_offsets/sizeof *weapondef_offsets;++k)
        if(weapondef_offsets[k].offset==offset) {
            if(pmove) *pmove=weapondef_offsets[k].pmove;
            return weapondef_offsets[k].name;
        }
    return "";
}
/* Every type-43 attachment override C1 will apply to one base weapon.
 * C1 BG_BuildCustomWeaponDef 0x00421590 copies the base definition
 * (rep movsd 0x004216c4, 0xbfc bytes) and then, for each attachment named in
 * the "*a <base> <flag> [<item> <variant>]*" configstring, resolves
 * bg_attachIdx[base<<8 | item] (0x00421833) and applies that definition's
 * count (+4) entries of {offset, type, value} (+8) through 0x004208e0:
 * kinds >= 0x10 other than 0x27 are a straight dword store to
 * weaponDef + offset (0x0042098d/0x00420990); lower kinds parse the string
 * value; 0x27 appends a hide tag. The offsets are CoDOL WeaponDef offsets and
 * belong to the shipped attachment asset, not to anything this server emits -
 * this command exists so the composed definition the CLIENT builds can be
 * predicted from the server's own mounted zone images. */
static void CheckAttachments_f(void)
{
    const char *weapon; unsigned item,printed=0,rows=0,pmove=0;
    if(Cmd_Argc()<2) {
        Com_Printf(CON_CHANNEL_SERVER,
            "Usage: checkattachments <base weapon> [item index ...]\n");
        return;
    }
    weapon=Cmd_Argv(1);
    for(item=0;item<256;++item) {
        mounted_weapon_attachment_t attachment; const char *asset=NULL;
        unsigned entry,selected=Cmd_Argc()>2 ? 0u : 1u,argument;
        for(argument=2;argument<(unsigned)Cmd_Argc();++argument)
            if((unsigned)atoi(Cmd_Argv((int)argument))==item) selected=1;
        if(!selected) continue;
        if(ComposedAttachmentAssetName(weapon,item,&asset)!=1 || !asset) continue;
        ++rows;
        if(LoadMountedWeaponAttachment(asset,&attachment)!=1) {
            Com_Printf(CON_CHANNEL_SERVER,"  item %3u -> %s: NOT MOUNTED\n",item,asset);
            continue;
        }
        Com_Printf(CON_CHANNEL_SERVER,"  item %3u -> %s (%u overrides)\n",
            item,asset,attachment.count);
        for(entry=0;entry<attachment.count;++entry) {
            uint32_t offset,type,value; float number; unsigned is_pmove; const char *label;
            memcpy(&offset,attachment.entries+12u*entry,4);
            memcpy(&type,attachment.entries+12u*entry+4u,4);
            memcpy(&value,attachment.entries+12u*entry+8u,4);
            memcpy(&number,&value,4);
            label=weapondef_offset_name(offset,&is_pmove);
            if(is_pmove) ++pmove;
            Com_Printf(CON_CHANNEL_SERVER,
                "      +0x%03x type %2u value %u (0x%08x, %.4f) %s%s%s\n",
                offset,type,value,value,number,
                attachment.texts && attachment.texts[entry] ? attachment.texts[entry] : "",
                label,is_pmove ? "  <== PMOVE INPUT" : "");
            ++printed;
        }
        FreeMountedWeaponAttachment(&attachment);
    }
    Com_Printf(CON_CHANNEL_SERVER,
        "CoDOL attachments for %s: %u legal rows, %u override entries, %u of them pmove inputs\n",
        weapon,rows,printed,pmove);
}

/* What BG_GetWeaponDef(ps->weapon) actually hands this server's pmove, for one
 * base or composed weapon name, beside the serialized bytes it was converted
 * from. The client runs the same two functions over its own bg_weaponDefs
 * entry, so this is the server half of the comparison tools/bob_probe.py
 * takes from the live C1 process. */
static void CheckPmoveWeapon_f(void)
{
    const struct WeaponDef *def=NULL,*base=NULL;
    const composed_weapon_t *record=NULL;
    mounted_weapon_t mounted; const char *name,*base_name;
    unsigned k; int composed,status;
    if(Cmd_Argc()<2) {
        Com_Printf(CON_CHANNEL_SERVER,
            "Usage: checkpmovedef <weapon name>            (e.g. cheytac_mp)\n"
            "       checkpmovedef \"*a 11 11 69 20 ...\"     (a composed name)\n"
            "Prints the +0x670/+0x674/+0x678/+0x694 this server's PM_CmdScale and\n"
            "ps->bobCycle divisor consume, and the mounted image they came from.\n");
        return;
    }
    name=Cmd_Argv(1);
    composed=name[0]=='*';
    status=composed ? LoadComposedWeapon(name,&def)
                    : LoadMountedNativeWeapon(name,LoadEngineWeaponCore,&def);
    if(status!=1 || !def) {
        Com_Printf(CON_CHANNEL_SERVER,"checkpmovedef: '%s' did not resolve (%d) %s\n",
            name,status,composed ? ComposedWeaponError() : "");
        return;
    }
    base=def;
    if(composed && ComposedWeaponBaseDefinition(def,&base)==1 && base) {
        int adjusted=0;
        ComposedWeaponAdjusted(def,&adjusted);
        ComposedWeaponRecord(def,&record);
        Com_Printf(CON_CHANNEL_SERVER,
            "composed '%s' over base '%s', attachment overrides applied: %s\n",
            def->szInternalName?def->szInternalName:"?",
            base->szInternalName?base->szInternalName:"?",adjusted?"yes":"NO");
    } else base=def;
    {
        /* Akimbo evidence: C1 dual wields a weapon whose definition byte
         * +0xbd5 is set (0x419d51); a rof_akimbo pair's weaponattach asset
         * (akimbo_de, akimbo_usp, ...) is what sets it on a composed pistol.
         * The client draws the second gun itself from that same definition. */
        int dual=0; const char *world=NULL,*view=NULL;
        if(!GetNativeWeaponDualWield(def,&dual)) dual=-1;
        if(composed) ComposedWeaponBaseArt(def,&world,&view);
        Com_Printf(CON_CHANNEL_SERVER,"  dual wield (+0xbd5) %d  recipe world model %s  view model %s\n",
            dual,world ? world : "-",view ? view : "-");
        /* The native effect bindings the definition carries (null = none or
         * unresolved; the loader warns once for the latter). */
        Com_Printf(CON_CHANNEL_SERVER,"  bound effects: explosion %s  view flash %s  world flash %s\n",
            def->projExplosionEffect && def->projExplosionEffect->name ? def->projExplosionEffect->name : "-",
            def->viewFlashEffect && def->viewFlashEffect->name ? def->viewFlashEffect->name : "-",
            def->worldFlashEffect && def->worldFlashEffect->name ? def->worldFlashEffect->name : "-");
        /* Bolt launchers (bolt_attachment.c): type 2 with explosion
         * kind 7 and a spawned grenade name at +0xb84. */
        {
            const char *spawned=NULL;
            if(!GetNativeWeaponSpawnedGrenade(def,&spawned)) spawned=NULL;
            Com_Printf(CON_CHANNEL_SERVER,"  weapType %d  projExplosion %d  damage %d  spawned grenade (+0xb84) '%s'\n",
                (int)def->weapType,(int)def->projExplosion,def->damage,spawned ? spawned : "?");
        }
    }
    /* The converted definition: exactly what BG_GetWeaponDef hands pmove. */
    Com_Printf(CON_CHANNEL_SERVER,
        "  converted WeaponDef: moveSpeedScale %.9g  adsMoveSpeedScale %.9g  sprintDurationScale %.9g  aimDownSight %d\n",
        def->moveSpeedScale,def->adsMoveSpeedScale,def->sprintDurationScale,def->aimDownSight);
    {
        /* The composed zoom FOVs the variable-zoom step cycles through. */
        float fov[3];
        if(GetNativeWeaponZoomFov(def,fov))
            Com_Printf(CON_CHANNEL_SERVER,"  composed zoomFov (+0x944..+0x94c): %.9g %.9g %.9g\n",
                fov[0],fov[1],fov[2]);
        else Com_Printf(CON_CHANNEL_SERVER,"  composed zoomFov: no native core\n");
    }
    /* The serialized base image those came from, straight out of the archive
     * find_dependency_definition selected. A composed name's own image is the
     * base image with this name's attachment entries applied, so the two lines
     * differ exactly by the overrides checkattachments lists. */
    base_name=base && base->szInternalName ? base->szInternalName : name;
    if(LoadMountedWeapon(base_name,&mounted)==1 && mounted.asset.data &&
       mounted.asset.size>=0xbfcu) {
        Com_Printf(CON_CHANNEL_SERVER,"  mounted image of '%s' (%u bytes):\n",
            base_name,(unsigned)mounted.asset.size);
        for(k=0;k<sizeof weapondef_offsets/sizeof *weapondef_offsets;++k) {
            unsigned offset=weapondef_offsets[k].offset; uint32_t word; float number;
            if(offset+4u>mounted.asset.size) continue;
            memcpy(&word,mounted.asset.data+offset,4);
            memcpy(&number,&word,4);
            Com_Printf(CON_CHANNEL_SERVER,"    +0x%03x = %.9g (u32 %u, byte %u) %s%s\n",
                offset,number,word,mounted.asset.data[offset],
                weapondef_offsets[k].name,
                weapondef_offsets[k].pmove ? "  <== PMOVE INPUT" : "");
        }
    } else Com_Printf(CON_CHANNEL_SERVER,"  mounted image of '%s': NOT READABLE\n",base_name);
    if(record) {
        unsigned a;
        Com_Printf(CON_CHANNEL_SERVER,"  attachment items (%u):",record->count);
        for(a=0;a<record->count && a<COMPOSED_ATTACHMENTS;++a) {
            const char *item=NULL,*asset=NULL;
            ComposedAttachmentItemName(record->items[a],&item);
            ComposedAttachmentAssetName(base_name,record->items[a],&asset);
            Com_Printf(CON_CHANNEL_SERVER," %u(%s->%s)",record->items[a],
                item?item:"?",asset?asset:"none");
        }
        Com_Printf(CON_CHANNEL_SERVER,"\n  run `checkattachments %s` for every override entry\n",base_name);
    }
}

void MatchStatus_f(void);
void DevClients_f(void);
void DevPlayer_f(void);
void InitAssetCommands(void)
{
    /* Installed rather than called by name, the same way the weapon resolvers
     * are: SV_SpawnServer owns the call sites and this translation unit owns
     * the behaviour. Without these the server still runs, it just keeps the
     * previous category's zones resident and never learns the coop playlist
     * spelling -- sv_main warns in both cases. */
    SV_SetCategoryTeardown(UnloadGametypeAssets);
    SV_SetSessionMapInstaller(SetSessionMapName);
    Cmd_AddCommand("shadowcompare", ShadowCompare_f);
    InitEngineStreamModels();
    Cmd_AddCommand("checkstructured", CheckStructured_f);
    Cmd_AddCommand("checkprofile", CheckProfile_f);
    Cmd_AddCommand("checkchallenge", CheckChallenge_f);
    Cmd_AddCommand("checkexperience", CheckExperience_f);
    Cmd_AddCommand("checkunlock", CheckUnlock_f);
    Cmd_AddCommand("bindprofile", BindProfile_f);
    Cmd_AddCommand("loadzone", LoadZone_f);
    Cmd_AddCommand("sh1x_assetinfo", AssetInfo_f);
    Cmd_AddCommand("sh1x_imagecheck", ImageCheck_f);
    Cmd_AddCommand("sh1x_unused", Unused_f);
    Cmd_AddCommand("token_consumed", TokenConsumed_f);
    Cmd_AddCommand("token_inventory", TokenInventory_f);
    Cmd_AddCommand("sh1x_memcheckpoint", MemCheckpoint_f);
    Cmd_AddCommand("loadzones", LoadZones_f);
    Cmd_AddCommand("checkcollision", CheckCollision_f);
    Cmd_AddCommand("checkweapon", CheckWeapon_f);
    Cmd_AddCommand("checkattachments", CheckAttachments_f);
    Cmd_AddCommand("checkpmovedef", CheckPmoveWeapon_f);
    Cmd_AddCommand("checkweaponregistry", CheckWeaponRegistry_f);
    Cmd_AddCommand("dumplocalize", DumpLocalize_f);
    Cmd_AddCommand("checkxanim", CheckXAnim_f);
    Cmd_AddCommand("checkmodel", CheckModel_f);
    Cmd_AddCommand("checkconstants", CheckConstants_f);
    /* One-line match state the lobby polls to place players (match_status.c). */
    Cmd_AddCommand("sh1x_status", MatchStatus_f);
    Cmd_AddCommand("dev_clients", DevClients_f);
    Cmd_AddCommand("dev_player", DevPlayer_f);
    /* dev_cheats (state, or on|off) is registered by RegisterDevCheatCommands below. */
    { extern void WarmStandbyRegister(void); WarmStandbyRegister(); }
    PersistentEventsEnable(Cvar_RegisterBool("persistentEvents",qfalse,CVAR_INIT,
        "Record resident-server lifecycle observations; does not enable persistent assignment")->boolean ? 1 : 0);
    Cmd_AddCommand("checkcharvariant", CheckCharacterVariant_f);
    Cmd_AddCommand("checkcharacter", CheckCharacter_f);
    { extern void RegisterHitProbe(void); RegisterHitProbe(); }
    { extern void RegisterFireProbe(void); RegisterFireProbe(); }
    { extern void RegisterDestructibleProbe(void); RegisterDestructibleProbe(); }
    { extern void RegisterExplosionProbe(void); RegisterExplosionProbe(); }
    { extern void RegisterEffectAudit(void); RegisterEffectAudit(); }
    { extern void RegisterBrushProbe(void); RegisterBrushProbe(); }
    { extern void RegisterPlayerUnstuck(void); RegisterPlayerUnstuck(); }
    { extern void RegisterDevCheatCommands(void); RegisterDevCheatCommands(); }
    { extern void RegisterRestSettle(void); RegisterRestSettle(); }
    { extern void RegisterGibProbe(void); RegisterGibProbe(); }
    { extern void RegisterScriptModelTreeProbe(void); RegisterScriptModelTreeProbe(); }
    { extern void RegisterCorpseProbe(void); RegisterCorpseProbe(); }
    { extern void RegisterActorContactProbe(void); RegisterActorContactProbe(); }
    { extern void RegisterModelSlotProbe(void); RegisterModelSlotProbe(); }
    { extern void WorldGunProbe_f(void); Cmd_AddCommand("worldgunprobe",WorldGunProbe_f); }
    { extern void RegisterLimitCommands(void); RegisterLimitCommands(); }
}
#endif

/* Automatic secure-profile attachment at connect. The lobby (dw-capture.py)
 * serves mp/playersecuredata (mpdata_s) per uid from its resources directory:
 * mpdata_s.<uid> when the player uploaded one, else the shared mpdata_s, else
 * the seed mpdata_s.ranklevel31.proposed. ReadSecureProfile walks that
 * same chain, so the server can attach the profile the client is playing with
 * from the same directory (profilePath), keyed by the initial connect
 * identity, which survives map resets and cannot change with userinfo. Scripts read secure fields through getPlayerData
 * from the first player-connect callback. */
int AutoBindClientSecureProfile(unsigned client,const char *userinfo)
{
    cvar_t *directory=Cvar_FindVar("profilePath");
    uint64_t uid=ClientConnectIdentity(client);uint8_t *profile;structured_schema_t schema={0};int ok=0,status,carried;
    (void)userinfo; /* Mutable C1 updates may omit or replace xuid. */
    if(!directory || !*directory->string || client>=32 || !uid)return 0;
    profile=malloc(32768);if(!profile)return 0;
    /* Still connected across a map change: keep playing with the profile this
     * client holds rather than the lobby file, which lags the match just
     * reported (see CarryClientSecureProfile). */
    carried=TakeCarriedSecureProfile(client,uid,profile,32768);
    status=carried ? 1 :
        ReadSecureProfile(directory->string,uid,uid!=0,"mpdata_s.ranklevel31.proposed",profile,32768);
    if(status==1 && LoadMountedStructuredSchema("mp/playersecuredata.def",&schema)==1)
        ok=AttachClientSecureProfile(client,uid,profile,32768,&schema);
    if(ok) LoadClientProgress(client,uid);
    /* The attached profile is the XP this player arrives with; whatever
     * _rank.gsc commits on top of it this match is what the lobby is owed. */
    if(ok) MatchReportClientAttached(client,uid);
    FreeStructuredSchema(&schema);free(profile);
    Com_Printf(CON_CHANNEL_SERVER,"CoDOL secure profile for client %u (uid %llu): %s\n",client,(unsigned long long)uid,
        ok ? (carried ? "attached (carried across map change)" : "attached from lobby file") : status==0 ? "no mpdata_s in profilePath" : status<0 ? "unreadable or invalid" : "schema/attach rejected");
    return ok;
}

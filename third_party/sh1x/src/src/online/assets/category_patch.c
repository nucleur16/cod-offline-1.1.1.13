/* C1 loads the two permanent MP weapon patches globally. Survival borrows the
 * MP gametype/weapon system and must read these same base tables/models.
 * Do not infer "all mp_*" from that dependency: level, UI and old category
 * aliases are not additional Survival archives. Preserve first-definition order.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "category_patch.h"
#include "assets.h"
#include "load_blocks.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "filesystem.h"
#include "cvar.h"
#include "sys_main.h"
#include <stdlib.h>
#include <string.h>

int CategoryPatchZoneApplies(const char *category,const char *filename)
{
    char prefix[16];size_t length,prefix_length;
    if(!category || !filename || !*category)return 0;
    length=strlen(filename);
    if(length<4 || Q_stricmp(filename+length-3,".ff"))return 0;
    /* Sys_ListFiles supplies basenames; reject non-basename callers too. */
    if(strchr(filename,'/') || strchr(filename,'\\'))return 0;
    Com_sprintf(prefix,sizeof(prefix),"%s_",category);
    prefix_length=strlen(prefix);
    if(length>prefix_length+3 && !Q_stricmpn(filename,prefix,prefix_length))return 1;
    if(Q_stricmp(category,"survival"))return 0;
    return !Q_stricmp(filename,"mp_weapons.ff") ||
           !Q_stricmp(filename,"mp_weapons_postgfx_mp.ff") ||
           !Q_stricmp(filename,"mp_codol_weapons.ff") || /* LEGACY_ZONE: an install from before the rename */
           !Q_stricmp(filename,"mp_codol_weapons_postgfx_mp.ff"); /* LEGACY_ZONE */
}

/* The weapon zones dropped "codol" from their names (2026-11). An install that has both the old and the new file
 * mounts only the new one, as Kernel33 loads only the new one (patch_locale_policy.h kPatchZoneMerged). */
static const char *const renamed_patch_zones[][2]={
    {"mp_codol_weapons.ff","mp_weapons.ff"}, /* LEGACY_ZONE */
    {"mp_codol_weapons_postgfx_mp.ff","mp_weapons_postgfx_mp.ff"}, /* LEGACY_ZONE */
};

int CategoryPatchZoneSuperseded(const char *filename,char *const *files,int count)
{
    size_t r;int i;
    for(r=0;r<sizeof(renamed_patch_zones)/sizeof(renamed_patch_zones[0]);++r) {
        if(Q_stricmp(filename,renamed_patch_zones[r][0]))continue;
        for(i=0;i<count;++i) if(!Q_stricmp(files[i],renamed_patch_zones[r][1]))return 1;
    }
    return 0;
}

static int patch_zone_name_compare(const void *a,const void *b)
{
    return Q_stricmp(*(const char *const *)a,*(const char *const *)b);
}

/* One sorted traversal for native category zones and borrowed weapon bases;
 * sorting each family separately could invert the client's link precedence.
 * Stock-name collision and bounded archive-array behavior are unchanged. */
size_t AppendCategoryPatchZones(const char *zone_type,const char **paths,int *ranks,
                                        char (*storage)[MAX_OSPATH],size_t count,size_t capacity)
{
    cvar_t *folder=Cvar_FindVar("patchZonePath");
    char dir[MAX_OSPATH],stock[MAX_OSPATH],name[MAX_QPATH];
    char **list; int n=0,i; size_t len;
    if(folder && *folder->string) Q_strncpyz(dir,folder->string,sizeof(dir));
    else if(fs_basepath && *fs_basepath->string)
        Com_sprintf(dir,sizeof(dir),"%s/client_shipRetail/patch",fs_basepath->string);
    else return count;
    list=Sys_ListFiles(dir,".ff",NULL,&n,qfalse);
    if(!list) return count;
    qsort(list,(size_t)n,sizeof(*list),patch_zone_name_compare);
    for(i=0;i<n;++i) {
        len=strlen(list[i]);
        if(!CategoryPatchZoneApplies(zone_type,list[i]) || len-3>=sizeof(name))
            continue;
        if(CategoryPatchZoneSuperseded(list[i],list,n)) {
            Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL patch folder: %s skipped, its renamed zone is installed\n",list[i]);
            continue;
        }
        memcpy(name,list[i],len-3); name[len-3]=0;
        /* Kernel33 skips a name a stock zone folder has; so does this */
        if(BuildZonePath(name,stock,sizeof(stock)) && FS_FileExistsOSPath(stock)) {
            Com_PrintWarning(CON_CHANNEL_SYSTEM,"CoDOL patch folder: %s skipped, a zone of that name is installed\n",list[i]);
            continue;
        }
        if(count>=capacity) {
            Com_PrintWarning(CON_CHANNEL_SYSTEM,"CoDOL patch folder: %s skipped, too many zones\n",list[i]);
            continue;
        }
        Com_sprintf(storage[count],MAX_OSPATH,"%s/%s",dir,list[i]);
        paths[count]=storage[count]; ranks[count]=LINK_RANK_PATCH_FOLDER+i; ++count;
        Com_Printf(CON_CHANNEL_SYSTEM,"CoDOL patch folder: %s mounted with the %s map archives\n",list[i],zone_type);
    }
    Sys_FreeFileList(list);
    return count;
}

#endif

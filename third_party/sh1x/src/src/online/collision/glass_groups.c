/* IW4 4149e0: first matching named group, ordered uint16 piece IDs.
 * Native ownership independent of archive lifetime. GameWorldMp loading must
 * install measured groups; an uninstalled registry is not an empty map.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "glass_groups.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; uint16_t *pieces,count; } owned_group_t;
void FreeGlassGroups(glass_groups_t *state)
{
    size_t i;
    owned_group_t *groups;
    if(!state) return;
    groups=state->groups;
    for(i=0;i<state->count;++i) {
        free(groups[i].name); free(groups[i].pieces);
    }
    free(groups);
    memset(state,0,sizeof(*state));
}
int InstallGlassGroups(glass_groups_t *state,uint32_t piece_count,
    const glass_group_view_t *source,size_t count)
{
    glass_groups_t staged={0};
    owned_group_t *groups;
    size_t i,j,length;
    if(!state || (count && !source) || count>SIZE_MAX/sizeof(*groups)) return 0;
    groups=count?calloc(count,sizeof(*groups)):NULL;
    if(count && !groups) return 0;
    staged.groups=groups; staged.count=count; staged.piece_count=piece_count;
    for(i=0;i<count;++i) {
        if(!source[i].name || (source[i].count && !source[i].pieces)) goto fail;
        length=strlen(source[i].name);
        if(length==SIZE_MAX || !(groups[i].name=malloc(length+1))) goto fail;
        memcpy(groups[i].name,source[i].name,length+1);
        groups[i].count=source[i].count;
        if(!groups[i].count) continue;
        groups[i].pieces=malloc((size_t)groups[i].count*sizeof(uint16_t));
        if(!groups[i].pieces) goto fail;
        for(j=0;j<groups[i].count;++j) {
            if(source[i].pieces[j]>=piece_count) goto fail;
            groups[i].pieces[j]=source[i].pieces[j];
        }
    }
    staged.ready=1;
    FreeGlassGroups(state);
    *state=staged;
    return 1;
fail:
    FreeGlassGroups(&staged);
    return 0;
}
int FindGlassGroup(const glass_groups_t *state,const char *name,
    const uint16_t **pieces,uint16_t *count)
{
    const owned_group_t *groups;
    size_t i;
    if(!state || !state->ready || !name || !pieces || !count) return 0;
    groups=state->groups;
    for(i=0;i<state->count;++i) if(!strcmp(groups[i].name,name)) {
        *pieces=groups[i].pieces; *count=groups[i].count; return 1;
    }
    *pieces=NULL; *count=0;
    return 1;
}

int CompareGlassGroups(const glass_groups_t *a,const glass_groups_t *b)
{
    const owned_group_t *x,*y; size_t i;
    if(!a || !b || a->ready!=b->ready || a->count!=b->count || a->piece_count!=b->piece_count) return 1;
    x=a->groups; y=b->groups;
    for(i=0;i<a->count;++i) {
        if(strcmp(x[i].name,y[i].name)) return 2;
        if(x[i].count!=y[i].count || (x[i].count && memcmp(x[i].pieces,y[i].pieces,2*(size_t)x[i].count))) return 3;
    }
    return 0;
}

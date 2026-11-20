#include "collision.h"
#include <string.h>
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int LoadZoneCollision(const zone_t *zone,const char *name,
                            collision_name_resolver_t resolver,void *context,
                            collision_map_t *out)
{
    return LoadZoneCollisionResolved(zone,name,resolver,NULL,context,out);
}
int LoadZoneCollisionResolved(const zone_t *zone,const char *name,
                            collision_name_resolver_t resolver,
                            collision_model_resolver_t model_resolver,void *context,
                            collision_map_t *out)
{
    asset_t world,collision;
    int status;
    if(!out) return -1;
    status=FindZoneCollisionPair(zone,name,&collision,&world);
    if(status!=1) return status;
    return ReadCollisionMapResolved(&collision,&world,resolver,model_resolver,context,out)?1:-1;
}
int FindZoneCollisionPair(const zone_t *zone,const char *name,
    asset_t *collision_out,asset_t *world_out)
{
    asset_t world={0},collision={0},asset;
    world_planes_t planes;
    uint32_t i;
    int found_world=0,found_collision=0;
    if (!zone || !name || !*name || !collision_out || !world_out) return -1;
    for (i=0;i<zone->asset_count;++i) {
        const char *base;
        if (!ZoneAsset(zone,i,&asset)) return -1;
        if (asset.type!=21 || !(base=WorldBaseName(&asset)) || strcmp(base,name)) continue;
        if (found_world) return -1;
        world=asset; found_world=1;
    }
    if (!found_world) return 0;
    if (!ReadWorldPlanes(&world,&planes) || !planes.name_reference) return -1;
    for (i=0;i<zone->asset_count;++i) {
        collision_layout_t layout;
        if (!ZoneAsset(zone,i,&asset)) return -1;
        if ((asset.type!=14 && asset.type!=15) || asset.size<4 || u32(asset.data)!=planes.name_reference) continue;
        if (!ReadCollisionLayout(&asset,&layout)) return -1;
        if (planes.name_reference==UINT32_MAX && strcmp(planes.name,(const char *)(asset.data+256))) continue;
        if (found_collision) return -1;
        collision=asset; found_collision=1;
    }
    if (!found_collision) return -1;
    *collision_out=collision; *world_out=world; return 1;
}

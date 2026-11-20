#include "collision.h"
#include <string.h>

void FreeCollisionMap(collision_map_t *map)
{
    if (!map) return;
    FreeCollisionMaterials(&map->materials);
    FreeCollisionBrushes(&map->brushes);
    FreeCollisionTerrain(&map->terrain);
    FreeCollisionLeaves(&map->leaves);
    FreeCollisionBrushTree(&map->brush_tree);
    FreeMapEntities(&map->entities);
    FreeCollisionStaticModels(&map->static_models);
    memset(map,0,sizeof(*map));
}
int ReadCollisionMap(const asset_t *collision,const asset_t *world,
                           collision_name_resolver_t resolver,void *context,
                           collision_map_t *out)
{
    return ReadCollisionMapResolved(collision,world,resolver,NULL,context,out);
}
int ReadCollisionMapResolved(const asset_t *collision,const asset_t *world,
                           collision_name_resolver_t resolver,
                           collision_model_resolver_t model_resolver,void *context,
                           collision_map_t *out)
{
    collision_map_t map={0};
    collision_layout_t layout;
    if (!out || !ReadCollisionLayout(collision,&layout)) return 0;
    /* C1's loader matches the IW4 clipMap layout through dynEntCollList.
     * The following uint32 at 0xcc is checksum in IW4x's declaration.
     * Preserve the archive value; do not substitute the configstring CRC. */
    map.checksum=(uint32_t)collision->data[204]|(uint32_t)collision->data[205]<<8|
        (uint32_t)collision->data[206]<<16|(uint32_t)collision->data[207]<<24;
    map.static_model_count=layout.sections[COLL_STATIC_MODELS].count;
    if (!ReadCollisionStaticModels(collision,model_resolver,context,&map.static_models) ||
        !ReadWorldBrushes(collision,world,&map.brushes) ||
        !ReadCollisionTerrain(collision,&map.terrain) ||
        !ReadCollisionLeaves(collision,&map.leaves) ||
        !ReadCollisionBrushTree(collision,&map.brush_tree) ||
        !ReadCollisionMaterials(collision,resolver,context,&map.materials) ||
        !ReadCollisionMapEntities(collision,&map.entities)) {
        FreeCollisionMap(&map); return 0;
    }
    FreeCollisionMap(out); *out=map; return 1;
}

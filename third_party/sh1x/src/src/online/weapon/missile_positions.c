#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#include "weapon_lock.h"
#include <string.h>
unsigned char missile_position_active[ENTITYNUM_WORLD];
unsigned char missile_lock_active[ENTITYNUM_WORLD];
float missile_positions[ENTITYNUM_WORLD][3];
void ResetMissilePosition(unsigned entity)
{
    if(entity>=ENTITYNUM_WORLD)return;
    missile_position_active[entity]=0;
    missile_lock_active[entity]=0;
    memset(missile_positions[entity],0,sizeof(missile_positions[entity]));
}
void ResetMissilePositions(void)
{
    memset(missile_lock_active,0,sizeof(missile_lock_active));
    memset(missile_position_active,0,sizeof(missile_position_active));
    memset(missile_positions,0,sizeof(missile_positions));
}
void SetMissileEntityTarget(unsigned entity,int locked)
{
    ResetMissilePosition(entity);
    if(entity<ENTITYNUM_WORLD) missile_lock_active[entity]=locked ? 1 : 0;
}

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentMissilePositionsEmpty(void){return
    resident_entity_zero(missile_position_active,sizeof(missile_position_active))&&
    resident_entity_zero(missile_lock_active,sizeof(missile_lock_active))&&
    resident_entity_zero(missile_positions,sizeof(missile_positions));}
#endif

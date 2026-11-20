#ifndef VEHICLE_SPAWNER_H
#define VEHICLE_SPAWNER_H
#include "entity.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* Server-only entity category, separate from IW3's ET_EVENTS=17. The vehicle
 * definition is loaded now; no live vehicle pool entry exists until spawned. */
typedef struct {
    int known;
    int vehicle_slot;
    int vehicle_info;
} vehicle_spawner_t;
int CreateVehicleSpawner(gentity_t *entity,const char *vehicle_name);
const vehicle_spawner_t *VehicleSpawner(const gentity_t *entity);
void ResetVehicleSpawner(unsigned entity_number);
void ResetVehicleSpawners(void);
void Scr_IsSpawner(void);
void Scr_VehicleGetSpawnerArray(void);
/* Draft: registration awaits native factory type/initialization parity. */
void Scr_SpawnVehicle(void);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

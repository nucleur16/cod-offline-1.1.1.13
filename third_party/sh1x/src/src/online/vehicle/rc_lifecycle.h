#ifndef RC_LIFECYCLE_H
#define RC_LIFECYCLE_H
#include "scr_vm.h"
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
void Scr_VehicleSetOwner(scr_entref_t);
void Scr_VehicleGetClientFlag(scr_entref_t);
void Scr_VehicleSetClientFlag(scr_entref_t);
void Scr_VehicleClearClientFlag(scr_entref_t);
int RCVehicleStunned(const struct gentity_s *);
/* An RC car's own blast (the car is radiusDamage's inflictor) measures its
 * targets from their origins, as T5 G_RadiusDamage does. */
int RCVehicleBlastFromOrigin(const struct gentity_s *inflictor);
struct gentity_s *RCVehicleOwner(const struct gentity_s *);
void RCEntityFreed(const struct gentity_s *);
void RCResetLifecycle(void);
#ifdef __cplusplus
}
#endif
#endif

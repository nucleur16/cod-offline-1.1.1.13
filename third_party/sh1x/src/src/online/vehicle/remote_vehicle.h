/* Native C1 remote vehicle ownership and driver-seat script lifecycle. */
#ifndef REMOTE_VEHICLE_H
#define REMOTE_VEHICLE_H
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
/* control_angles must come from the vehicle's actual driver/barrel transform.
 * Returns 1 on success; rejected ownership or unsupported definition returns 0. */
int RemoteVehicleEnter(struct gentity_s *player,struct gentity_s *vehicle,
                             const float control_angles[3]);
void RemoteVehicleExit(unsigned client);
int RemoteVehicleScriptExit(unsigned client);
struct gentity_s *RemoteVehicleSeatOccupant(struct gentity_s *,int seat);
void RemoteVehicleResetAll(void);
void RemoteVehicleEntityFreed(const struct gentity_s *entity);
void RemoteVehicleValidate(unsigned client);
/* Native-only type: C1 ps+1f8 has no entry in playerStateFields. */
int RemoteVehicleType(unsigned client);
#ifdef __cplusplus
}
#endif
#endif

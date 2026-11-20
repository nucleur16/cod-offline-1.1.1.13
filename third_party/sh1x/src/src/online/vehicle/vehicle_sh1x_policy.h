/* Explicit sh1x backend tuning; these values are not recovered C1 constants. */
#ifndef VEHICLE_SH1X_POLICY_H
#define VEHICLE_SH1X_POLICY_H
#include "vehicle_ground_frame.h"
/* server.h leaks a bare pack(1); keep one layout in every TU. */
#pragma pack(push,8)
typedef struct {
    float sag_fraction,damping_ratio,grip;
    float brake_g,handbrake_g,coast_g;
    float stability_inches; /* T5 wheel roll/pitch stability lever */
    /* T5 NitrousVehicle orientation constraint, per unit body mass. A
     * negative value means "use the vehicle asset's authored value". */
    vehicle_orientation_t orientation;
    /* Driving behaviour measured on real T5 (docs/RCXD_T5_PARITY.md).
     * reverse_scale: T5 NitrousVehicle m_reverse_scale on the reverse target
     * speed and motor force. auto_handbrake_*: T5 view-steered auto handbrake
     * (NitrousVehicleController::UpdateVehicleInputs: |camera - body yaw|
     * above the angle, speed above the minimum, body not turning the other
     * way). boost_speed/boost_accel: replace the asset's boost multipliers
     * when positive. jump: 0 = no jump (T5), 1 = the ADS hop. players: chassis
     * contacts against standing players and actors (T5 vehicles collide). */
    float reverse_scale,auto_handbrake_angle,auto_handbrake_min_speed;
    float boost_speed,boost_accel,max_steer,min_steer_scale,min_steer_speed;
    int jump,players;
} vehicle_sh1x_tuning_t;
#ifdef __cplusplus
extern "C" {
#endif
int ReadSh1xVehiclePolicy(vehicle_sh1x_tuning_t *,vehicle_frame_settings_t *);
/* Per-mass orientation strengths (policy, or the asset's authored value where
 * the policy says -1) times body mass. */
int ResolveSh1xVehicleOrientation(const vehicle_sh1x_tuning_t *,
    const vehicle_raw_t *,float mass,vehicle_orientation_t *);
int BuildSh1xVehicleSuspension(const vehicle_sh1x_tuning_t *,
    const vehicle_raw_t *,const vehicle_wheel_layout_t *,float mass,
    const float center_of_mass[3],float gravity,vehicle_frame_wheel_t out[6]);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

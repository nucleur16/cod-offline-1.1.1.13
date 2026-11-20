#ifndef VEHICLE_DRIVE_H
#define VEHICLE_DRIVE_H
#include "vehicle_raw.h"
#pragma pack(push,8)
#ifdef __cplusplus
extern "C" {
#endif
enum { VEHICLE_ACCEL_KNOTS=32 };
typedef struct { float speed_fraction,acceleration_fraction; } vehicle_accel_knot_t;
typedef struct {
    unsigned count;
    vehicle_accel_knot_t knots[VEHICLE_ACCEL_KNOTS];
    /* C1 per-definition graph record +144. Source units are mph/sec. */
    float maximum_acceleration;
} vehicle_acceleration_t;
typedef struct {
    /* Physical instance state, not retained vehicleDef speed aliases. */
    float forward_speed,forward_axis_z,max_speed,mass_scale;
    float forward_target,reverse_target,reverse_fraction,boost_acceleration;
    float previous_friction;
} vehicle_drive_input_t;
typedef struct {
    float motor_target,motor_force,longitudinal_friction;
    int reversing,wake_body;
} vehicle_drive_output_t;
/* C1 618d80 interpolation. Invalid/uncovered input fails without an output. */
int EvaluateVehicleAcceleration(const vehicle_acceleration_t *,float,float *);
/* C1 65d0c0 ordinary four-wheel motor/friction path. Contact-relative forward
 * velocity and mass_scale must come from the real physics instance. This does
 * not substitute velocity integration for a wheel/contact constraint solver. */
int ResolveGroundVehicleDrive(const vehicle_ground_settings_t *,
    const vehicle_acceleration_t *,const vehicle_drive_input_t *,
    vehicle_drive_output_t *);
/* C1 683d00..683daa applies this to each positive speed demand before boost. */
int ApplyVehicleHandbrakeTarget(float target,float speed,float *out);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

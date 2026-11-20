/* Native ground vehicle settings from shipped C1 VEHICLEFILE raw assets. */
#ifndef VEHICLE_RAW_H
#define VEHICLE_RAW_H
#include "vehicle_native.h"
#include <stddef.h>
#pragma pack(push,8)
#ifdef __cplusplus
extern "C" {
#endif
enum { VEHICLE_RAW_FIELDS=192 };
enum { GROUND_PHYSICS=1, GROUND_CONTROL=2,
       GROUND_CAMERA=4, GROUND_SOURCE_TEXT=8 };
typedef struct { char key[64],value[128]; } vehicle_raw_field_t;
typedef enum { AXLE_FRONT, AXLE_REAR, AXLE_ALL } vehicle_axle_t;
typedef struct {
    int remote_control,third_person_driver,boost_enabled,handbrake_enabled;
    vehicle_axle_t steering_axle,power_axle,braking_axle;
    float wheel_radius,suspension_travel_rear;
    float suspension_strength_front,suspension_strength_rear;
    float suspension_damping_front,suspension_damping_rear;
    float max_steering_angle,steering_lerp,min_steering_scale,min_steering_speed;
    float friction_braking,friction_coasting,friction_top_speed;
    float friction_side,friction_side_rear,velocity_dependent_slip;
    float roll_stability,roll_resistance,pitch_resistance,yaw_resistance;
    float upright_strength_pitch,upright_strength_roll,target_air_pitch;
    float air_yaw_torque,air_pitch_torque;
    float boost_speed_multiplier,boost_accel_multiplier,boost_duration,boost_recharge_time;
    float camera_range,camera_height,camera_height_min,camera_pitch,camera_pitch_min;
    float camera_min_pitch_clamp,camera_max_pitch_clamp,camera_spring_distance;
    float camera_spring_time,camera_handbrake_turn_rate,camera_fov;
    /* These distinct physics text settings retain source units. Do not
     * replace them with common.top_speed or assume all velocities are mph. */
    float max_velocity_source,reverse_speed_source;
} vehicle_ground_settings_t;
typedef struct {
    vehicle_native_t common;
    vehicle_ground_settings_t ground;
    /* Explicit coverage: binary physics/control decoding does not yet claim
     * camera reconstruction. Activation must require its needed domains. */
    unsigned decoded_domains;
    unsigned count;
    /* Preserve every source setting, including fields with no current native
     * consumer. No borrowed archive pointers and no silently dropped keys. */
    vehicle_raw_field_t fields[VEHICLE_RAW_FIELDS];
} vehicle_raw_t;
/* 1 success, -1 malformed/unsupported ground record. Output unchanged on error.
 * Common speed/acceleration fields are converted once from text mph to in/s;
 * named ground settings retain source units pending their own consumers. */
int ParseGroundVehicleFile(const char *name,const char *bytes,size_t size,vehicle_raw_t *out);
/* Mounted vehicles/<name> type36, 1 loaded, 0 absent, -1 unusable. */
int LoadGroundVehicleFile(const char *name,vehicle_raw_t *out);
/* Installed ground settings from binary VehicleDef or raw VEHICLEFILE, or
 * NULL for a non-ground/stale record. Inspect decoded_domains before use.
 * Borrowed until ResetNativeVehicles or record replacement;
 * callers must not retain this pointer across either lifecycle boundary. */
const vehicle_raw_t *NativeGroundVehicleDefinition(int info_index);
const char *GroundVehicleSourceValue(const vehicle_raw_t *,const char *key);
int GroundVehicleSourceNumber(const vehicle_raw_t *,const char *key,float *value);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

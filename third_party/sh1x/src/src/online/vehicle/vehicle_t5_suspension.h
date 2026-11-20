#ifndef VEHICLE_T5_SUSPENSION_H
#define VEHICLE_T5_SUSPENSION_H
#include "vehicle_ground_frame.h"
/* server.h leaks a bare pack(1); keep one layout in every TU. */
#pragma pack(push,8)
/* Named T5 physics parameters: these are NOT C1 VehiclePhysDef aliases.
 * No generic constructor defaults are substituted for missing RC tuning. */
typedef struct {
    float spring_k,damping_k,suspension_adjustment,hard_limit;
    float wheel_radius,forward_friction,side_friction,side_friction_max;
    float roll_stability,braking,power_braking,coasting;
} vehicle_t5_suspension_t;
#ifdef __cplusplus
extern "C" {
#endif
/* Converts T5's actual mass-normalized spring parameters to physical rows,
 * using actual chassis COM and model wheel tags. Output unchanged on failure.
 * Parameter source identity must be established by caller; neither this API
 * nor the runtime treats C1 raw/binary spring numbers as equivalent units. */
int BuildT5VehicleSuspension(const vehicle_t5_suspension_t *,
    const vehicle_wheel_layout_t *,float mass,const float model_center_of_mass[3],
    float gravity_multiplier,vehicle_frame_wheel_t out[6]);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

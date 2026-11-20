/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef VEHICLE_CONSTRAINTS_H
#define VEHICLE_CONSTRAINTS_H
/* server.h leaks a bare pack(1); keep one layout in every TU. */
#pragma pack(push,8)
#ifdef __cplusplus
extern "C" {
#endif
/* T5 wheel constraint inputs after actual contact/Jacobian construction.
 * Speeds are angular wheel radians/sec; force factors already include the
 * authored wheel share. No world/default coefficients are synthesized here. */
typedef struct {
    float seconds,position_error,spring,damping,radius;
    float desired_speed,acceleration,braking;
    float turning_speed_ratio,turning_acceleration_ratio;
    float side_friction,forward_friction,friction_max;
    int braking_state;
} vehicle_wheel_constraint_input_t;
typedef struct {
    float rhs,cfm,minimum,maximum;
} vehicle_constraint_row_t;
typedef struct {
    vehicle_constraint_row_t suspension,side,forward;
    float side_ratio,forward_ratio,friction_max;
    int has_forward,side_parent_ratio;
} vehicle_wheel_constraints_t;
int BuildVehicleWheelConstraints(const vehicle_wheel_constraint_input_t *,
    vehicle_wheel_constraints_t *);
/* Projects the two accumulated friction impulses together, not independent
 * Coulomb clamps. Returns success; 'clamped' mirrors T5 forward flag4. */
int ClampVehicleWheelFriction(const vehicle_wheel_constraints_t *,
    float suspension_impulse,float side_impulse,float forward_impulse,
    float *side_result,float *forward_result,int *clamped);
/* T5 Phys_RunToTime: at most100ms, at least1ms on a changed clock; no catch-up
 * loop. Caller commits time only after its complete prolog/solver/epilog pass. */
int VehicleFrameSeconds(int previous_time,int current_time,float *seconds);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

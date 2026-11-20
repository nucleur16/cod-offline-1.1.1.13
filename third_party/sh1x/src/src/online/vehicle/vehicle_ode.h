#ifndef VEHICLE_ODE_H
#define VEHICLE_ODE_H
#include "vehicle_raw.h"
#include "vehicle_constraints.h"
/* server.h leaks a bare pack(1); keep one layout in every TU. */
#pragma pack(push,8)
#ifdef __cplusplus
extern "C" {
#endif
struct vehicle_ode_world_s;
struct vehicle_ode_body_s;
typedef struct vehicle_ode_world_s vehicle_ode_world_t;
typedef struct vehicle_ode_body_s vehicle_ode_body_t;
/* Real retained ODE backend, with caller-owned world memory. No retained
 * PhysWorld index is borrowed; C1 world3 is not a valid retained array index.
 * Single server-thread ownership; engine Phys_Init is idempotent. */
vehicle_ode_world_t *CreateVehicleODEWorld(void);
/* Players and actors are not ODE geometry. The owner lists their absolute
 * boxes before each step; a body with a chassis box (entity-local, see
 * SetVehicleODEBodyBox) gets horizontal contacts against them, as a T5
 * vehicle is stopped by a standing player. */
enum { VEHICLE_ODE_OBSTACLES=64 };
typedef struct { float mins[3],maxs[3]; } vehicle_obstacle_t;
int SetVehicleODEObstacles(vehicle_ode_world_t *,const vehicle_obstacle_t *,unsigned count);
void DestroyVehicleODEWorld(vehicle_ode_world_t *);
/* Strict shipped preset/model geometry, owned until destruction. No model
 * bounds fallback. Model origin/axis use native game coordinates; velocity is
 * world inches/sec and angular velocity is world radians/sec. */
vehicle_ode_body_t *CreateVehicleODEBody(vehicle_ode_world_t *,
    const vehicle_raw_t *,const char *model_name,unsigned entity,unsigned generation,
    const float model_origin[3],const float model_axis[3][3],
    const float linear_velocity[3],const float angular_velocity[3]);
void DestroyVehicleODEBody(vehicle_ode_body_t *);
/* NULL mins/maxs clears the box. */
int SetVehicleODEBodyBox(vehicle_ode_body_t *,const float mins[3],const float maxs[3]);
int VehicleODEOwnsBody(const vehicle_ode_world_t *,const vehicle_ode_body_t *);
int VehicleODEMass(const vehicle_ode_body_t *,float *mass);
/* Diagnostics: chassis contact points generated for this body in the last pulse pass. */
unsigned VehicleODEFrameContacts(const vehicle_ode_body_t *);
/* Returns the actual installed physical COM, including the C1 roll-stability
 * adjustment. Constructor normalization must not re-read an unadjusted model. */
int VehicleODECenterOfMass(const vehicle_ode_body_t *,float center[3]);
/* Caller must validate entity/generation before publishing the returned state.
 * The renderer origin is recovered from center-of-mass transform, not directly
 * equated with ODE body's center. Output unchanged on invalid body/state. */
int ReadVehicleODEBody(const vehicle_ode_body_t *,unsigned entity,unsigned generation,
    float model_origin[3],float model_axis[3][3],float velocity[3],float angular_velocity[3]);
/* One caller-scheduled physical substep. These are explicit native solver
 * settings, not guessed C1 defaults. The owner must supply the actual C1
 * wheel/constraint force producer; a missing producer never advances the body. */
typedef struct {
    float gravity[3],erp,cfm,max_correcting_velocity,surface_layer;
    int iterations;
    float contact_erp,contact_cfm,friction_scale,bounce_velocity;
    float contact_max_bounce_velocity;
    float max_angular_velocity;
    float linear_drag,angular_drag;
} vehicle_ode_step_t;
typedef int (*vehicle_ode_force_fn)(vehicle_ode_body_t *,float seconds,void *);
int StepVehicleODEWorld(vehicle_ode_world_t *,float seconds,
    const vehicle_ode_step_t *,vehicle_ode_force_fn,void *);
/* Forces act on the real body at the real world-space contact point. */
int VehicleODEForceAtPoint(vehicle_ode_body_t *,const float force[3],const float point[3]);
/* Pure force through the centre of mass (T5 rigid_body::add_force), no torque. */
int VehicleODECentralForce(vehicle_ode_body_t *,const float force[3]);
/* Shared T5 pulse pass: chassis contacts and wheel rows solve against the
 * SAME body scratch, then commit once. Never follow it with the ODE world
 * step above. This is a replacement backend, not a pre-step force hack.
 * The callback resolves current contact geometry/control and adds wheel rows.
 * Position iterations/error thresholds are explicit caller-owned settings. */
int StepVehiclePulseWorld(vehicle_ode_world_t *,float seconds,
    const vehicle_ode_step_t *,int position_iterations,float velocity_error_squared,
    float position_error_squared,vehicle_ode_force_fn,void *);
/* Only valid in a pulse prepare callback. Contact point/normal are actual
 * current wheel hit, normal outward from the hit surface. NULL hitBody means
 * the static map. Center and suspension/side axes are world-space. */
int VehicleODEAddWheel(vehicle_ode_body_t *,vehicle_ode_body_t *hitBody,
    const float wheel_center[3],const float suspension_direction[3],const float side_axis[3],
    const float hit_point[3],const float hit_normal[3],float hard_limit,float stop_margin,
    float roll_stability,float pitch_stability,const vehicle_wheel_constraint_input_t *,
    float cached_force[4]);
/* T5 rigid_body_constraint_custom_orientation strengths, already scaled by
 * body mass (NitrousVehicle m_parameter * m_body_mass). T5 has one pitch/roll
 * resistance and one upright strength; C1's vehicle asset (and IW4's
 * VehiclePhysDef it descends from) authors pitch and roll separately
 * (rollResistance/pitchResistance/uprightStrengthPitch/uprightStrengthRoll),
 * so the rows take them separately. Zero disables a row. */
typedef struct {
    float pitch_resistance,roll_resistance,yaw_resistance;
    float upright_pitch,upright_roll;
} vehicle_orientation_t;
/* Only valid in a pulse prepare callback. The T5 orientation constraint
 * (rbc_def_custom.cpp:39) between the body and the static environment, as
 * NitrousVehicle installs it for every nitrous vehicle
 * (nitrous_vehicle.cpp:1888-1897): pitch/roll/yaw angular-velocity resistance
 * plus the upright restoring rows. wheels_airborne: no wheel touched ground
 * this pass; with no chassis contact either, T5 sets
 * m_no_orientation_correction and the upright rows only resist further tilt. */
int VehicleODEAddOrientation(vehicle_ode_body_t *,const vehicle_orientation_t *,
    int wheels_airborne);
typedef struct {
    float fraction,point[3],normal[3];
    vehicle_ode_body_t *body; /* NULL: static map/model */
    unsigned entity,generation;
    int surface_flags,starts_inside;
} vehicle_wheel_hit_t;
/* Actual suspension-segment query, during prepare. Tests native clipmap and
 * static-model triangles plus every other owned model's physical primitives.
 * fraction1 means no surface hit. No gameplay entity bounds stand-ins. */
int VehicleODEWheelRay(vehicle_ode_body_t *ignored,const float start[3],
    const float end[3],int contents_mask,vehicle_wheel_hit_t *);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

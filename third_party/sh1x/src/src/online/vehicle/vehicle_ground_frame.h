/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef VEHICLE_GROUND_FRAME_H
#define VEHICLE_GROUND_FRAME_H
#include "vehicle_ode.h"
#include "vehicle_wheels.h"
/* server.h leaks a bare pack(1); keep one layout in every TU. */
#pragma pack(push,8)
#ifdef __cplusplus
extern "C" {
#endif
/* Normalized physical wheel inputs. Centers are REST centers in model space,
 * not an implicit alias for C1 tag translations. The vehicle constructor must
 * resolve its authored travel/mass convention before attaching these records. */
typedef struct {
    float center[3],suspension[3],side[3];
    float hard_limit,stop_margin,roll_stability,pitch_stability;
    /* T5 has separate authored brake/coast damping factors. These are force
     * factors, not the dimensionless C1 tire-friction coefficients. */
    float regular_braking,power_braking,coasting_braking;
    vehicle_wheel_constraint_input_t constraint;
    int present;
} vehicle_frame_wheel_t;
typedef struct {
    unsigned hit_entity,hit_generation;
    int contacting,surface_flags;
    float point[3],normal[3],force[4];
} vehicle_frame_contact_t;
typedef struct {
    vehicle_ode_body_t *body;
    unsigned entity,generation;
    vehicle_frame_wheel_t wheels[VEHICLE_WHEELS];
    vehicle_frame_contact_t contacts[VEHICLE_WHEELS];
    float origin[3],axis[3][3],velocity[3],angular_velocity[3];
    /* Current controller steering (degrees, +left); published as the C1
     * front-wheel yaw word (entity +0x68). Not consumed by the solver. */
    float steering_degrees;
    /* T5 NitrousVehicleController::UpdateJump: requested world-up velocity
     * impulse (in/s) for the next physical step; the frame consumes it (zeroes
     * it) once applied, so a two-step clock advance fires it once. */
    float jump_velocity;
    /* T5 rigid_body_constraint_custom_orientation strengths, already scaled
     * by body mass (NitrousVehicle m_parameter * m_body_mass). All zero: no
     * orientation constraint (the pre-2026-09-27 behaviour). */
    vehicle_orientation_t orientation;
} vehicle_frame_body_t;
typedef struct {
    vehicle_ode_step_t solver;
    int position_iterations,contents_mask,minimum_step_milliseconds;
    float velocity_error_squared,position_error_squared;
} vehicle_frame_settings_t;
/* One complete shared physical frame. The records cover EVERY body in world;
 * there is no secondary per-entity step. Caller validates entity generations
 * before entry, then publishes only after success. Contact caches and public
 * poses remain unchanged on failure. No callbacks/scripts during the pass.
 * Lifetime: records may be removed only outside this synchronous function;
 * after entity reuse initialize the new record, never inherit old contacts. */
int RunGroundVehicleFrame(vehicle_ode_world_t *,float seconds,
    const vehicle_frame_settings_t *,vehicle_frame_body_t *,unsigned count);
typedef struct { float origin[3],axis[3][3]; } vehicle_frame_pose_t;
typedef struct {
    int initialized,previous_time,next_time;
    unsigned count,entity[8],generation[8];
    vehicle_frame_pose_t previous[8];
} vehicle_frame_clock_t;
/* C1 world clock algorithm (65b0b0): at most two physical steps with an
 * explicit caller-owned minimum, followed by previous/next pose interpolation.
 * World3's scheduler/minimum remains unresolved; do not read CCE1F8[3] as a
 * verified constant (adjacent data follows the client-world table).
 * Explicit reset required
 * after map-time rewind or body-set change. A failed second physical step
 * retains the first successful step and its clock; callers can retry, but must
 * not publish the requested frame until this returns success. */
int ResetGroundVehicleClock(vehicle_frame_clock_t *,int now,
    const vehicle_frame_body_t *,unsigned count);
int AdvanceGroundVehicleWorld(vehicle_ode_world_t *,int now,
    const vehicle_frame_settings_t *,vehicle_frame_clock_t *,
    vehicle_frame_body_t *,unsigned count,vehicle_frame_pose_t *presentation);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

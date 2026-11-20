#ifndef VEHICLE_CONTROLS_H
#define VEHICLE_CONTROLS_H
#include <stdint.h>
#include "vehicle_raw.h"
/* server.h leaks a bare pack(1); keep one layout in every TU. */
#pragma pack(push,8)
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float steering_degrees;
    /* C1 vehicle+0x230: an int32 level time, end of duration+recharge. */
    int boost_ready_time;
    /* T5 NitrousVehicleController::m_jump_button_pressed: set when a jump
     * fired, cleared when the button is released (holding it never re-fires). */
    int jump_pressed;
} vehicle_control_state_t;
/* The controller TU and the runtime TU (which includes server.h's leaked
 * pack(1)) once disagreed on this layout (an int64 deadline: offset 8 under
 * the MinGW default, 4 under pack(1)): the runtime's jump latch landed in the
 * high word of the boost deadline, so holding the jump button kept the C1
 * boost (2x speed, 3x accel, no over-speed friction) active indefinitely.
 * Every member is now 4 bytes, so no packing or ABI can move them. */
#ifdef __cplusplus
static_assert(sizeof(vehicle_control_state_t)==12,"RC control state layout");
#else
_Static_assert(sizeof(vehicle_control_state_t)==12,"RC control state layout");
#endif
typedef struct {
    float throttle,reverse_throttle,steering_degrees;
    float forward_speed_scale,reverse_speed_scale,boost_acceleration;
    /* C1 vehicle+22c merges explicit button and automatic turn braking;
     * +22d preserves automatic braking separately. Wheel constraints need both. */
    int boost_active,boost_ready,handbrake,automatic_handbrake;
} vehicle_controls_t;
/* C1 signed command axes, no IW3 usercmd translation. This decodes the actual
 * controller; physics applies its resulting demands later. view_angle_steering
 * must be resolved from C1 VehicleDef+2cc, not guessed from a raw field name.
 * current_speed is C1 vehicle speed, not a source maxVelocity assumption.
 * Output/state unchanged on unsupported or invalid data. */
int StepGroundVehicleControls(const vehicle_raw_t *definition,
    int forward,int right,uint32_t buttons,int view_angle_steering,
    float driver_yaw,float vehicle_yaw,int now,float frame_seconds,
    float current_speed,vehicle_control_state_t *state,
    vehicle_controls_t *out);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

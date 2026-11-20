/* RC-XD control state: the C1 boost (683cc0) and the T5 jump latch.
 *
 * Regression for "after jumping in the RC-XD it gained a lot of speed": the
 * runtime TU saw vehicle_control_state_t under server.h's leaked
 * pack(1) while vehicle_controls.cpp saw the default layout, so the
 * runtime's jump_pressed=1 (offset 12) was the controller's high word of the
 * then-int64 boost_ready_time (offset 8..15 under the MinGW ABI). Every jump
 * then held the boost (2x speed, 3x acceleration, no over-speed friction)
 * until the button was let go. This TU reproduces the runtime's packing on
 * purpose; the Linux i386 ABI aligns int64 to 4 and could not show the bug,
 * so build it with i686-w64-mingw32-g++ as well to cover the shipped ABI.
 *
 * rc_car_medium_mp (common_gametype_mp VEHICLEFILE): topSpeed 38 mph,
 * boostEnabled 1, boostSpeedMultiplier 2, boostAccelMultiplier 3,
 * boostDuration 2, boostRechargeTime 60, handbrakeEnabled 1.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#pragma pack(1)
#include "vehicle_controls.h"
#include "vehicle_drive.h"

static int failures;
#define CHECK(c) do{if(!(c)){std::printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c);++failures;}}while(0)

enum { BUTTON_JUMP=0x400, BUTTON_ADS=0x800 };

static vehicle_raw_t rc;
static void setup()
{
    std::memset(&rc,0,sizeof(rc));
    vehicle_ground_settings_t &g=rc.ground;
    g.remote_control=1;g.third_person_driver=1;g.boost_enabled=1;g.handbrake_enabled=1;
    g.max_steering_angle=30;g.steering_lerp=5;g.min_steering_scale=1;g.min_steering_speed=0;
    g.friction_braking=3;g.friction_coasting=1;g.friction_top_speed=0.3f;
    g.boost_speed_multiplier=2;g.boost_accel_multiplier=3;g.boost_duration=2;g.boost_recharge_time=60;
    rc.common.top_speed=38*17.6f;
}
static vehicle_controls_t step(vehicle_control_state_t &s,int now,unsigned buttons)
{
    vehicle_controls_t out;std::memset(&out,0,sizeof(out));
    /* Forward throttle, a little steer so the automatic handbrake stays off. */
    int ok=StepGroundVehicleControls(&rc,127,20,buttons,0,0,0,now,0.05f,300,&s,&out);
    CHECK(ok);
    /* The runtime's T5 UpdateJump latch, written exactly as demand() does. */
    s.jump_pressed=(buttons&BUTTON_ADS)?1:0;
    return out;
}

static void jump_does_not_boost()
{
    vehicle_control_state_t s;std::memset(&s,0,sizeof(s));
    int now=100000;
    vehicle_controls_t c=step(s,now,0);
    CHECK(!c.boost_active && c.boost_ready && std::fabs(c.forward_speed_scale-0.5f)<1e-6f);
    /* Hold the jump (ADS) for five seconds: never a boost. */
    for(int t=50;t<=5000;t+=50) {
        c=step(s,now+t,BUTTON_ADS);
        CHECK(!c.boost_active);
        CHECK(std::fabs(c.forward_speed_scale-0.5f)<1e-6f);
        CHECK(c.boost_acceleration==0);
    }
    CHECK(s.boost_ready_time==0);
}

static void boost_is_two_seconds_then_sixty_recharge()
{
    vehicle_control_state_t s;std::memset(&s,0,sizeof(s));
    const int start=200000;
    vehicle_controls_t c=step(s,start,BUTTON_JUMP|BUTTON_ADS);
    CHECK(c.boost_active && !c.boost_ready);
    CHECK(std::fabs(c.forward_speed_scale-1.0f)<1e-6f && c.boost_acceleration==3);
    CHECK(s.boost_ready_time==start+62000);
    c=step(s,start+1999,BUTTON_ADS);CHECK(c.boost_active);
    c=step(s,start+2000,BUTTON_ADS);CHECK(!c.boost_active && std::fabs(c.forward_speed_scale-0.5f)<1e-6f);
    /* Pressing again during the recharge does nothing. */
    c=step(s,start+30000,BUTTON_JUMP|BUTTON_ADS);CHECK(!c.boost_active && !c.boost_ready);
    c=step(s,start+61999,BUTTON_JUMP);CHECK(!c.boost_active && !c.boost_ready);
    CHECK(s.boost_ready_time==start+62000);
    c=step(s,start+62000,BUTTON_JUMP);CHECK(c.boost_active);
    CHECK(s.boost_ready_time==start+124000);
}

static void over_speed_has_no_motor_without_boost()
{
    vehicle_acceleration_t curve;std::memset(&curve,0,sizeof(curve));
    curve.count=2;curve.knots[0].speed_fraction=0;curve.knots[0].acceleration_fraction=1;
    curve.knots[1].speed_fraction=1;curve.knots[1].acceleration_fraction=0.2f;curve.maximum_acceleration=30;
    vehicle_drive_input_t in;std::memset(&in,0,sizeof(in));
    in.forward_speed=700;in.max_speed=38*17.6f;in.mass_scale=10;
    in.forward_target=in.max_speed*0.5f;
    vehicle_drive_output_t out;
    CHECK(ResolveGroundVehicleDrive(&rc.ground,&curve,&in,&out));
    CHECK(out.motor_force==0 && std::fabs(out.longitudinal_friction-0.3f)<1e-6f);
}

int main()
{
    setup();
    jump_does_not_boost();
    boost_is_two_seconds_then_sixty_recharge();
    over_speed_has_no_motor_without_boost();
    if(failures){std::printf("test_vehicle_controls: %d failure(s)\n",failures);return 1;}
    std::printf("test_vehicle_controls: ok\n");
    return 0;
}

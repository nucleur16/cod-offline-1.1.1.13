#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Staged server-thread RC physics ownership. Uses original T5 changed-clock
 * cadence explicitly; generic C1 client-world cadence is not an RC assumption. */
#ifdef OL_SERVER
#include "vehicle_ground_runtime.h"
#include "vehicle_ground_publish.h"
#include "vehicle_sh1x_policy.h"
#include "rc_lifecycle.h"
#include "vehicle_controls.h"
#include "vehicle_graph.h"
#include "remote_vehicle.h"
#include "weapon_input.h"
#include "vehicle_wheel_anim.h"
#include "g_shared.h"
#include "g_scr_vehicle.h"
#include "dobj.h"
#include "cscr_stringlist.h"
#include "q_math.h"
#include "qcommon.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
extern "C" scr_vehicle_s s_vehicles[8];
namespace {
struct ground_owner_t {
    vehicle_frame_body_t physics;
    vehicle_wheel_layout_t layout;
    vehicle_raw_t *definition;
    vehicle_acceleration_t graph;
    vehicle_control_state_t controls;
    vehicle_frame_wheel_t normalized[6];
    float friction;
    /* The sh1x RC policy this body was attached with (T5-measured driving
     * behaviour); has_tuning 0 keeps the plain C1 controller (tests). */
    vehicle_sh1x_tuning_t tuning;
    int has_tuning;
    /* Diagnostics: next periodic report time, last seen driver (0 none, client+1). */
    int report_time,driver_seen,input_missing_reported;
};
ground_owner_t owners[8];
vehicle_ode_world_t *world;
vehicle_frame_settings_t worldSettings;
int previousTime;
bool clockValid;
bool sameSettings(const vehicle_frame_settings_t &a,const vehicle_frame_settings_t &b) {
    for(int i=0;i<3;++i)if(a.solver.gravity[i]!=b.solver.gravity[i])return false;
#define SAME(member) if(a.solver.member!=b.solver.member)return false
    SAME(erp);SAME(cfm);SAME(max_correcting_velocity);SAME(surface_layer);SAME(iterations);
    SAME(contact_erp);SAME(contact_cfm);SAME(friction_scale);SAME(bounce_velocity);
    SAME(contact_max_bounce_velocity);SAME(max_angular_velocity);SAME(linear_drag);SAME(angular_drag);
#undef SAME
    return a.position_iterations==b.position_iterations&&a.contents_mask==b.contents_mask&&
        a.minimum_step_milliseconds==b.minimum_step_milliseconds&&
        a.velocity_error_squared==b.velocity_error_squared&&a.position_error_squared==b.position_error_squared;
}
int slot(const gentity_t *e) {
    if(!e||!e->r.inuse||e->s.number<0||e->s.number>=ENTITYNUM_MAX_NORMAL||
       e!=&g_entities[e->s.number]||e->s.eType!=ET_VEHICLE||!e->scr_vehicle)return -1;
    uintptr_t p=(uintptr_t)e->scr_vehicle,b=(uintptr_t)s_vehicles;
    if(p<b||p-b>=sizeof(s_vehicles)||(p-b)%sizeof(scr_vehicle_s)||
       e->scr_vehicle->entNum!=e->s.number)return -1;
    return (int)((p-b)/sizeof(scr_vehicle_s));
}
bool owned(const ground_owner_t &o,const gentity_t *e) {
    return o.physics.body&&o.physics.entity==(unsigned)e->s.number&&o.physics.generation==(unsigned)e->useCount;
}
void release(ground_owner_t &o) {
    DestroyVehicleODEBody(o.physics.body);std::free(o.definition);
    std::memset(&o,0,sizeof(o));
}
bool demand(ground_owner_t &o,int now,float seconds) {
    gentity_t *e=&g_entities[o.physics.entity];
    float origin[3],axis[3][3],velocity[3],angular[3],angles[3],mass;
    if(!ReadVehicleODEBody(o.physics.body,o.physics.entity,o.physics.generation,
        origin,axis,velocity,angular)||!VehicleODEMass(o.physics.body,&mass))return false;
    AxisToAngles(axis,angles);
    const float forwardSpeed=DotProduct(axis[0],velocity);
    gentity_t *driver=RemoteVehicleSeatOccupant(e,0);
    ol_usercmd_t command={};int forward=0,right=0;float yaw=angles[1];bool haveCommand=false;
    const int driverNow=driver&&driver->client?driver->s.number+1:0;
    if(driverNow!=o.driver_seen) {
        ((void)0);
        o.driver_seen=driverNow;o.input_missing_reported=0;
    }
    if(driverNow) {
        /* A driver whose last executed CoDOL command is not available (bot,
         * synthetic think, packet gap) drives with zero input; the physical
         * frame itself must not fail on input availability. */
        haveCommand=ExecutedUsercmd((unsigned)driver->s.number,&command)!=0;
        if(!haveCommand&&!o.input_missing_reported) {
            o.input_missing_reported=1;
            ((void)0);
        }
        if(haveCommand) {
            forward=(command.direction&1)?127:(command.direction&2)?-127:0;
            right=(command.direction&4)?127:(command.direction&8)?-127:0;
        }
        /* C1 683b50 (thirdPersonDriver) steers from the driver's ps yaw
         * (ps+0x150). Our remote enter sets the controls-linked pm bit, under
         * which the C1 pmove port no longer runs PM_UpdateViewAngles, so
         * ps.viewangles froze at the tag_player yaw and steering went dead
         * (live 2CF09DD5). Derive the yaw exactly as PM_UpdateViewAngles
         * would: command angles plus delta_angles (SetClientViewAngle at
         * enter seeded delta_angles so this starts at the car's yaw). */
        yaw=driver->client->ps.viewangles[1];
        if(haveCommand)
            yaw=AngleNormalize360((float)SHORT2ANGLE(command.angles[1])+driver->client->ps.delta_angles[1]);
    }
    /* Test lever: sh1x_rcTestThrottle forces the throttle of a driven car
     * (bots drive with zero input) so wall hits can be reproduced without a
     * human driver. 0 (default) leaves the driver's input alone. */
    {
        static cvar_t *testThrottle;
        if(!testThrottle)testThrottle=Cvar_RegisterFloat("sh1x_rcTestThrottle",0.0f,-1.0f,1.0f,0,
            "Test only: throttle forced onto every driven RC car (-1 reverse .. 1 forward); 0 = driver input");
        if(driverNow&&testThrottle->value!=0)forward=(int)(127.0f*testThrottle->value);
        /* Same for buttons (e.g. 0x800 jump, 0x400 boost), OR'd into the
         * driver's command so a bot-driven car can jump and boost. */
        static cvar_t *testButtons;
        if(!testButtons)testButtons=Cvar_RegisterInt("sh1x_rcTestButtons",0,0,0x7fffffff,0,
            "Test only: usercmd buttons OR'd into every driven RC car's input; 0 = driver input");
        if(driverNow&&testButtons->integer){command.buttons|=(unsigned)testButtons->integer;haveCommand=true;}
    }
    if(RCVehicleStunned(e)) {forward=right=0;command.buttons=0;yaw=angles[1];}
    vehicle_controls_t controls;
    const float speed=std::sqrt(DotProduct(velocity,velocity));
    if(!StepGroundVehicleControls(o.definition,forward,right,command.buttons,
        o.definition->ground.third_person_driver,yaw,angles[1],now,seconds,
        speed,&o.controls,&controls))return false;
    /* T5 view-steered auto handbrake (OpenBLOPS nitrous_vehicle_controller.cpp
     * UpdateVehicleInputs): driving forward with the camera more than
     * hand_brake_apply_delta (30) degrees off the body's yaw, above the
     * vehicle's minimum speed, locks the braking axle unless the body is
     * already turning the other way. Measured on real T5 (Plutonium,
     * mp_nuked): camera 30 deg off -> 210 u/s on a 53-unit circle; 45 deg ->
     * 130 u/s, 255 deg/s; 90 deg -> a 101 u/s, 473 deg/s spin. */
    bool t5Handbrake=false;
    if(o.has_tuning&&driverNow&&forward>0&&o.definition->ground.third_person_driver&&
       o.tuning.auto_handbrake_angle<180) {
        const float delta=(float)AngleDelta(yaw,angles[1]);
        if(std::fabs(delta)>o.tuning.auto_handbrake_angle&&speed>o.tuning.auto_handbrake_min_speed)
            t5Handbrake=!((delta<0&&angular[2]>0)||(delta>0&&angular[2]<0));
    }
    if(t5Handbrake){controls.handbrake=1;}
    if(o.has_tuning&&o.tuning.boost_speed>0&&controls.boost_active&&o.definition->ground.boost_speed_multiplier>0)
        controls.forward_speed_scale*=o.tuning.boost_speed/o.definition->ground.boost_speed_multiplier;
    /* T5 NitrousVehicleController::UpdateJump (OpenBLOPS
     * nitrous_vehicle_controller.cpp:1062): while the jump button is held and
     * at least two wheels were colliding in the previous update, add one
     * upward force and latch m_jump_button_pressed; release clears the latch.
     * C1's RC-XD binds it to the ADS button (usercmd 0x800, right mouse). The
     * BO1 rc_car vehicle file (m_jump_force) is not on disk, so the impulse is
     * a velocity sized for a ~32 in hop under C1 gravity 800 (sqrt(2*800*32)). */
    {
        /* T5's RC-XD has no hop (its jump key boosts; measured: no lift from
         * gostand, ADS, vehiclemoveup or vehiclespecialability), so the sh1x
         * policy leaves it off unless sh1x_rcJump is set. */
        const bool jumpHeld=haveCommand&&(command.buttons&0x800u)!=0&&(!o.has_tuning||o.tuning.jump);
        unsigned onGround=0;
        for(unsigned i=0;i<6;++i)if(o.physics.wheels[i].present&&o.physics.contacts[i].contacting)++onGround;
        o.physics.jump_velocity=0;
        if(jumpHeld) {
            if(onGround>=2&&!o.controls.jump_pressed) {
                o.physics.jump_velocity=226.0f;o.controls.jump_pressed=1;
                ((void)0);
            }
        } else o.controls.jump_pressed=0;
    }
    vehicle_wheel_layout_t steering;
    if(!SteerVehicleWheels(&o.layout,controls.steering_degrees,controls.handbrake,
        controls.automatic_handbrake,&steering))return false;
    vehicle_drive_input_t input={};
    input.forward_speed=forwardSpeed;input.forward_axis_z=axis[0][2];
    input.max_speed=o.definition->common.top_speed;input.mass_scale=mass;
    input.forward_target=controls.throttle*o.definition->common.top_speed;
    input.reverse_target=controls.reverse_throttle*o.definition->ground.reverse_speed_source*17.600000381469727f;
    /* T5 NitrousVehicle::update scales the reverse target speed and motor
     * force by m_reverse_scale (measured on the T5 RC-XD: 170 u/s backwards
     * against 330 forwards). */
    const float reverseScale=o.has_tuning?o.tuning.reverse_scale:1.0f;
    input.reverse_target*=reverseScale;
    if(controls.handbrake) {
        if(!ApplyVehicleHandbrakeTarget(input.forward_target,forwardSpeed,&input.forward_target)||
           !ApplyVehicleHandbrakeTarget(input.reverse_target,-forwardSpeed,&input.reverse_target))return false;
    }
    input.forward_target*=controls.forward_speed_scale;input.reverse_target*=controls.reverse_speed_scale;
    input.reverse_fraction=controls.reverse_throttle;input.boost_acceleration=controls.boost_acceleration;
    input.previous_friction=o.friction;
    vehicle_drive_output_t drive;
    if(!ResolveGroundVehicleDrive(&o.definition->ground,&o.graph,&input,&drive))return false;
    if(drive.reversing)drive.motor_force*=reverseScale;
    o.friction=drive.longitudinal_friction;
    o.physics.steering_degrees=controls.steering_degrees;
    for(unsigned i=0;i<6;++i) {
        vehicle_frame_wheel_t &wheel=o.physics.wheels[i];
        wheel=o.normalized[i];
        const vehicle_wheel_t &steer=steering.wheels[i];
        if(!wheel.present)continue;
        wheel.side[0]=-steer.forward[1];wheel.side[1]=steer.forward[0];wheel.side[2]=0;
        wheel.constraint.desired_speed=drive.motor_target/wheel.constraint.radius;
        // T5 NitrousVehicle::update applies the mass-scaled acceleration to
        // each powered wheel and floors an active factor at2. It does not
        // divide by wheel count; turning-radius normalization is separate.
        wheel.constraint.acceleration=(steer.flags&WHEEL_POWERED)&&drive.motor_target!=0?
            std::fmax(2.0f,drive.motor_force):0;
        wheel.constraint.turning_speed_ratio=steer.path_speed_scale;
        wheel.constraint.turning_acceleration_ratio=steer.motor_speed_scale;
        // Preserve the T5 gravity-normalized coefficient. C1 steering's
        // dimensionless change is a ratio, not a replacement physical unit.
        if(o.layout.wheels[i].side_friction>0)
            wheel.constraint.side_friction*=steer.side_friction/o.layout.wheels[i].side_friction;
        // C1 drive's longitudinal_friction is zero under forward throttle;
        // it is NOT T5's tire traction ellipse coefficient. Keep the latter
        // from the physical constructor, otherwise every driven wheel would
        // have zero forward traction precisely when accelerating.
        // C1's handbrake word is also set by its automatic turn-brake
        // (683c50: speed<=250 and steer==0 or |steer|>=10), which in C1 only
        // rescales the powered wheels (SteerVehicleWheels 6x/10x) and the
        // motor target. Feeding that automatic state into T5's braking_state
        // removes the motor row from every braking-axle wheel, so a car that
        // is standing still (steer exactly 0) or turning (|steer|>=10) can
        // never accelerate. Only the explicit C1 handbrake button is a brake.
        const bool explicitBrake=(controls.handbrake&&(command.buttons&0x100000u)!=0)||t5Handbrake;
        const bool powerBrake=explicitBrake&&(steer.flags&WHEEL_BRAKING);
        const bool regularBrake=controls.reverse_throttle>0&&!drive.reversing;
        const bool coast=drive.motor_target==0;
        wheel.constraint.braking_state=powerBrake||regularBrake||coast;
        wheel.constraint.braking=powerBrake?wheel.power_braking:
            regularBrake?wheel.regular_braking:coast?wheel.coasting_braking:0;
    }
    /* C1 spins the RC-XD's wheel meshes only through server animation
     * commands (vehicle_wheel_anim.h); drive them from this body speed. */
    VehicleWheelAnimFrame(e,forwardSpeed,now);
    
    return true;
}
}
extern "C" int GroundVehicleOwned(const gentity_t *e) {
    int index=slot(e);return index>=0&&owned(owners[index],e);
}
static int attach(gentity_t *e,const vehicle_frame_wheel_t supplied[6],
    const vehicle_t5_suspension_t *tuning,const vehicle_sh1x_tuning_t *sh1x,float gravity,
    const vehicle_frame_settings_t *settings) {
    int index=slot(e);if(index<0||((supplied!=NULL)+(tuning!=NULL)+(sh1x!=NULL))!=1||!settings||owners[index].physics.body)return 0;
    const vehicle_raw_t *d=NativeGroundVehicleDefinition(e->scr_vehicle->infoIdx);
    if(!d||!d->ground.remote_control||!d->common.physics_enabled)return 0;
    if(world&&!sameSettings(worldSettings,*settings))return 0;
    static const char *tags[6]={"tag_wheel_front_left","tag_wheel_front_right","tag_wheel_back_left",
        "tag_wheel_back_right","tag_wheel_middle_left","tag_wheel_middle_right"};
    float origins[6][3]={};unsigned present=0;
    for(unsigned i=0;i<6;++i) {
        unsigned tag=SL_GetString(tags[i],0);
        DObjAnimMat *matrix=G_DObjGetLocalTagMatrix(e,tag);
        if(matrix){std::memcpy(origins[i],matrix->trans,12);present|=1u<<i;}
        SL_RemoveRefToString(tag);
        if(supplied&&supplied[i].present!=((present&(1u<<i))!=0))return 0;
    }
    ground_owner_t next={};
    if(!BuildVehicleWheelLayout(d,origins,present,&next.layout)||
       LoadVehicleAccelerationGraph(d,&next.graph)!=1)return 0;
    next.definition=(vehicle_raw_t *)std::malloc(sizeof(*d));if(!next.definition)return 0;
    *next.definition=*d;
    const bool createdWorld=!world;
    if(createdWorld){world=CreateVehicleODEWorld();if(!world){std::free(next.definition);return 0;}}
    const auto discard=[&]() {
        release(next);
        if(createdWorld){DestroyVehicleODEWorld(world);world=NULL;}
        return 0;
    };
    float axis[3][3],zero[3]={};AnglesToAxis(e->r.currentAngles,axis);
    next.physics.entity=(unsigned)e->s.number;next.physics.generation=(unsigned)e->useCount;
    const char *model=SL_ConvertToString(G_ModelName(e->model));
    next.physics.body=CreateVehicleODEBody(world,d,model,next.physics.entity,next.physics.generation,
        e->r.currentOrigin,axis,zero,zero);
    if(!next.physics.body)return discard();
    vehicle_frame_wheel_t converted[6];
    const vehicle_frame_wheel_t *normalized=supplied;
    if(tuning||sh1x) {
        float mass,center[3];
        if(!VehicleODEMass(next.physics.body,&mass)||
           !VehicleODECenterOfMass(next.physics.body,center)||
           !(tuning?BuildT5VehicleSuspension(tuning,&next.layout,mass,center,gravity,converted):
              BuildSh1xVehicleSuspension(sh1x,d,&next.layout,mass,center,gravity,converted))) {
            return discard();
        }
        normalized=converted;
    }
    std::memcpy(next.physics.wheels,normalized,sizeof(next.physics.wheels));
    if(sh1x) {
        next.tuning=*sh1x;next.has_tuning=1;
        /* Boost overrides (the asset's 2x / 3x already match T5: 330 -> 660
         * u/s within 0.35 s of the press). A speed multiplier is applied while
         * boosting only (demand), because C1 also divides the normal top
         * speed by the asset's multiplier. */
        if(sh1x->boost_accel>0)next.definition->ground.boost_accel_multiplier=sh1x->boost_accel;
        /* T5 steers the wheels to the camera's yaw offset beyond the C1
         * asset's 30 degrees (measured: a 90-degree offset spins the car
         * nearly twice as fast as a 45-degree one). */
        if(sh1x->max_steer>0&&sh1x->max_steer<89)next.definition->ground.max_steering_angle=sh1x->max_steer;
        /* T5 understeers at speed (measured: a camera held 15 degrees off
         * turns the car at about 120 deg/s at 330 u/s); C1's own
         * speed-scaled steering (minSteeringScale/minSteeringSpeed) gives
         * that when the policy sets it. */
        if(sh1x->min_steer_scale>=0&&sh1x->min_steer_speed>0) {
            next.definition->ground.min_steering_scale=sh1x->min_steer_scale;
            next.definition->ground.min_steering_speed=sh1x->min_steer_speed;
        }
        if(sh1x->players) {
            /* Chassis box from the wheel tags: bumper to bumper over the
             * wheels, the track plus the tyre, tyre bottom to roof. */
            const float radius=d->ground.wheel_radius;
            float front=-1e9f,rear=1e9f,side=0;
            for(unsigned i=0;i<6;++i)if(present&(1u<<i)) {
                if(origins[i][0]>front)front=origins[i][0];
                if(origins[i][0]<rear)rear=origins[i][0];
                if(std::fabs(origins[i][1])>side)side=std::fabs(origins[i][1]);
            }
            const float mins[3]={rear-radius,-(side+radius*0.4f),-radius*0.5f};
            const float maxs[3]={front+radius,side+radius*0.4f,radius*1.5f};
            if(!SetVehicleODEBodyBox(next.physics.body,mins,maxs))return discard();
            Com_Printf(CON_CHANNEL_SERVER,"CoDOL rc: chassis box (%.1f %.1f %.1f)..(%.1f %.1f %.1f) for player contacts\n",
                mins[0],mins[1],mins[2],maxs[0],maxs[1],maxs[2]);
        }
    }
    std::memcpy(next.normalized,normalized,sizeof(next.normalized));
    if(sh1x) {
        /* T5 NitrousVehicle installs its orientation constraint with the
         * per-mass parameters times m_body_mass (nitrous_vehicle.cpp:1892). */
        float mass=0;
        if(!VehicleODEMass(next.physics.body,&mass)||
           !ResolveSh1xVehicleOrientation(sh1x,d,mass,&next.physics.orientation))return discard();
    }
    if(createdWorld)worldSettings=*settings;
    {
        float mass=0,center[3]={};
        VehicleODEMass(next.physics.body,&mass);VehicleODECenterOfMass(next.physics.body,center);
        const vehicle_frame_wheel_t &w0=normalized[0];
        ((void)0);
    }
    ((void)0);
    ((void)0);
    next.report_time=level.time;
    owners[index]=next;clockValid=false;
    /* C1 67b3a4..67b3b8 publishes this same authored distance*1000 for
     * client suspension traces. Owned vehicles bypass retained Think. */
    if(d->common.suspension_travel_known)
        e->s.time2=(int)(d->common.suspension_travel*1000.0f);
    /* The wheels themselves are animated, not simulated, on the client. A
     * missing tree is logged there and leaves the car drivable. */
    VehicleWheelAnimAttach(e,d->ground.wheel_radius);
    return 1;
}
extern "C" int AttachGroundVehicle(gentity_t *e,const vehicle_frame_wheel_t normalized[6],
    const vehicle_frame_settings_t *settings) {
    return attach(e,normalized,NULL,NULL,0,settings);
}
extern "C" int AttachT5GroundVehicle(gentity_t *e,const vehicle_t5_suspension_t *tuning,
    float gravity,const vehicle_frame_settings_t *settings) {
    return attach(e,NULL,tuning,NULL,gravity,settings);
}
extern "C" int AttachSpawnedRCVehicle(gentity_t *e) {
    int index=slot(e);if(index<0)return 0;
    const vehicle_raw_t *d=NativeGroundVehicleDefinition(e->scr_vehicle->infoIdx);
    if(!d||std::strcmp(d->common.name,"rc_car_medium_mp"))return 0;
    vehicle_sh1x_tuning_t tuning;vehicle_frame_settings_t settings;
    if(!ReadSh1xVehiclePolicy(&tuning,&settings))return -1;
    return attach(e,NULL,NULL,&tuning,-settings.solver.gravity[2],&settings)?1:-1;
}
static void releaseEmptyWorld() {
    for(unsigned i=0;i<8;++i)if(owners[i].physics.body)return;
    DestroyVehicleODEWorld(world);world=NULL;clockValid=false;
    std::memset(&worldSettings,0,sizeof(worldSettings));
}
extern "C" void GroundVehicleEntityFreed(const gentity_t *e) {
    if(!e)return;
    RCEntityFreed(e);
    VehicleWheelAnimEntityFreed(e);
    for(unsigned i=0;i<8;++i)if(owned(owners[i],e)){release(owners[i]);clockValid=false;}
    releaseEmptyWorld();
}
extern "C" void ResetGroundVehicleRuntime(void) {
    RCResetLifecycle();
    VehicleWheelAnimReset();
    for(unsigned i=0;i<8;++i)release(owners[i]);
    DestroyVehicleODEWorld(world);world=NULL;clockValid=false;
    std::memset(&worldSettings,0,sizeof(worldSettings));
}
extern "C" int RunGroundVehicles(int now) {
    if(!world)return 1;
    ground_owner_t pending[8];vehicle_frame_body_t bodies[8];unsigned indices[8],count=0;
    for(unsigned i=0;i<8;++i)if(owners[i].physics.body) {
        const unsigned entity=owners[i].physics.entity;
        if(entity>=ENTITYNUM_MAX_NORMAL||slot(&g_entities[entity])!=(int)i||!owned(owners[i],&g_entities[entity])) {
            release(owners[i]);clockValid=false;continue;
        }
        indices[count]=i;pending[count++]=owners[i];
    }
    if(!count){releaseEmptyWorld();return 1;}
    if(!clockValid||now<previousTime){previousTime=now;clockValid=true;return 1;}
    /* Standing players and actors near any car that collides with them. T5
     * vehicles are stopped by a player (measured: the RC-XD stops 18-20 units
     * from a standing player's origin and does not move or hurt him). The
     * player boxes are narrowed by sh1x_rcPlayerInset to that stop distance. */
    {
        static cvar_t *inset;
        if(!inset)inset=Cvar_RegisterFloat("sh1x_rcPlayerInset",7.0f,0.0f,14.0f,0,
            "Horizontal inset of a player's box for RC-XD contacts (T5 fit: the car stops 18-20 units from the player's origin)");
        vehicle_obstacle_t boxes[VEHICLE_ODE_OBSTACLES];unsigned boxCount=0;
        bool wanted=false;
        for(unsigned i=0;i<count;++i)if(pending[i].has_tuning&&pending[i].tuning.players)wanted=true;
        for(int n=0;wanted&&n<ENTITYNUM_MAX_NORMAL&&boxCount<VEHICLE_ODE_OBSTACLES;++n) {
            const gentity_t *p=&g_entities[n];
            if(!p->r.inuse||p->health<=0)continue;
            if(p->client) {
                if(n>=MAX_CLIENTS||p->client->sess.sessionState!=SESS_STATE_PLAYING)continue;
            } else if(p->handler!=ENT_HANDLER_ACTOR)continue;
            bool near=false;
            for(unsigned i=0;i<count&&!near;++i) {
                const gentity_t *car=&g_entities[pending[i].physics.entity];
                float d[3];VectorSubtract(p->r.currentOrigin,car->r.currentOrigin,d);
                near=DotProduct(d,d)<512.0f*512.0f;
            }
            if(!near)continue;
            vehicle_obstacle_t &b=boxes[boxCount];
            std::memcpy(b.mins,p->r.absmin,12);std::memcpy(b.maxs,p->r.absmax,12);
            for(int a=0;a<2;++a) {
                const float half=(b.maxs[a]-b.mins[a])*0.5f,cut=inset->value<half-1?inset->value:half-1;
                if(cut>0){b.mins[a]+=cut;b.maxs[a]-=cut;}
            }
            ++boxCount;
        }
        if(!SetVehicleODEObstacles(world,boxes,boxCount))return 0;
    }
    float seconds;if(!VehicleFrameSeconds(previousTime,now,&seconds))return 0;
    if(seconds==0)return 1;
    for(unsigned i=0;i<count;++i){if(!demand(pending[i],now,seconds))return 0;bodies[i]=pending[i].physics;}
    if(!RunGroundVehicleFrame(world,seconds,&worldSettings,bodies,count))return 0;
    vehicle_frame_pose_t poses[8];
    for(unsigned i=0;i<count;++i) {
        pending[i].physics=bodies[i];owners[indices[i]]=pending[i];
        std::memcpy(poses[i].origin,bodies[i].origin,12);std::memcpy(poses[i].axis,bodies[i].axis,36);
    }
    previousTime=now;
    return PublishGroundVehicleFrame(bodies,poses,count);
}
extern "C" void RunGroundVehiclesFrame(void) {
    if(!RunGroundVehicles(level.time))
        Com_Error(ERR_DROP,"Native ground vehicle physical frame failed; refusing stale pose publication");
}
#endif

#ifdef OL_SERVER
#include "persistent_world.h"
static int resident_world_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}
extern "C" int PersistentGroundWorldEmpty(void){
    return !world&&!clockValid&&resident_world_zero(owners,sizeof(owners)) &&
        resident_world_zero(&worldSettings,sizeof(worldSettings));}

extern "C" int PersistentGroundWorldCanReset(void) {
    unsigned bodies,owned_count=0;
    if(!PersistentPhysicsWorldQuiet(world,&bodies))return 0;
    for(unsigned i=0;i<8;++i)if(owners[i].physics.body) {
        if(!VehicleODEOwnsBody(world,owners[i].physics.body))return 0;
        for(unsigned j=0;j<i;++j)if(owners[j].physics.body==owners[i].physics.body)return 0;
        ++owned_count;
    }
    return owned_count==bodies;
}

#endif

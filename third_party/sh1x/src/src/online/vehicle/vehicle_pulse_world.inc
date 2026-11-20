/* Included only by vehicle_ode.cpp: the owned ODE geometry is the
 * collision backend; T5 pulse constraints replace its entire world step. */
enum { PULSE_BODIES=8,PULSE_CONTACTS=1024,PULSE_WHEELS=PULSE_BODIES*6,PULSE_ANGULAR=PULSE_BODIES*5 };
/* T5 rigid_body_constraint_custom_orientation request for one body. */
struct vehicle_pulse_orientation_t {
    vehicle_orientation_t strength;
    bool requested,no_orientation_correction;
};
struct vehicle_pulse_body_t {
    vehicle_ode_body_t *owner;
    ol_vehicle_pulse_body_t pulse;
    float position[3],axis[3][3],inertia[3][3],force[3],torque[3];
    float next_position[3],next_axis[3][3],next_linear[3],next_angular[3];
    unsigned next_tick;
    vehicle_pulse_orientation_t orientation;
};
/* T5 pulse_sum_angular against the static environment body (angular
 * velocity zero, so it has no node and no application term): objective is
 * ud . omega, application is world_inverse_inertia * ud. */
struct vehicle_pulse_angular_t {
    ol_vehicle_pulse_body_t *body;
    float direction[3],application[3];
    float rhs,cfm,minimum,maximum,denominator,impulse;
};
struct vehicle_pulse_wheel_t {
    vehicle_pulse_body_t *first,*second;
    float center[3],suspension[3],side[3],hit[3],normal[3];
    float hard_limit,stop_margin,roll,pitch,*cache,old_cache[4],next_cache[4];
    vehicle_wheel_constraint_input_t input;
    vehicle_wheel_constraints_t limits;
    vehicle_pulse_row_t stop,rows[3];
    float stop_bias;
    bool has_stop;
};
namespace {
float pdot(const float *a,const float *b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
void pcross(const float *a,const float *b,float *r) {
    r[0]=a[1]*b[2]-a[2]*b[1];r[1]=a[2]*b[0]-a[0]*b[2];r[2]=a[0]*b[1]-a[1]*b[0];
}
bool punit(const float *a) {return finite3(a)&&std::fabs(pdot(a,a)-1)<0.001f;}
bool invertTensor(const float m[3][3],float out[3][3]) {
    const double det=(double)m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])-
        (double)m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])+
        (double)m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
    if(!std::isfinite(det)||det<=0)return false;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)
        out[j][i]=(float)(((double)m[(i+1)%3][(j+1)%3]*m[(i+2)%3][(j+2)%3]-
            (double)m[(i+1)%3][(j+2)%3]*m[(i+2)%3][(j+1)%3])/det);
    return finite3(out[0])&&finite3(out[1])&&finite3(out[2]);
}
}
struct vehicle_pulse_context_t {
    vehicle_ode_world_t *owner;
    const vehicle_ode_step_t *settings;
    float seconds;
    unsigned body_count,contact_count,wheel_count;
    bool failed,preparing;
    vehicle_pulse_body_t bodies[PULSE_BODIES];
    vehicle_pulse_wheel_t wheels[PULSE_WHEELS];
    vehicle_pulse_contact_t contacts[PULSE_CONTACTS];
    unsigned angular_count;
    vehicle_pulse_angular_t angular[PULSE_ANGULAR];
};
namespace {
vehicle_pulse_body_t *pulseBody(vehicle_pulse_context_t &c,vehicle_ode_body_t *owner) {
    for(unsigned i=0;i<c.body_count;++i)if(c.bodies[i].owner==owner)return c.bodies+i;
    return NULL;
}
bool readPulseBody(vehicle_ode_body_t *owner,vehicle_pulse_body_t &b) {
    float origin[3],mass[17],local[3][3],inverse[3][3];
    if(!ReadVehicleODEBody(owner,owner->entity,owner->generation,origin,b.axis,b.pulse.linear,b.pulse.angular))return false;
    const float *position=dBodyGetPosition(owner->body);if(!finite3(position))return false;
    std::memcpy(b.position,position,12);b.owner=owner;
    dBodyGetMass(owner->body,mass);
    if(!std::isfinite(mass[0])||mass[0]<=0)return false;
    b.pulse.inverse_mass=1.0f/mass[0];
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)local[i][j]=mass[5+4*i+j];
    if(!invertTensor(local,inverse))return false;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j) {
        double inertia=0,inv=0;
        for(int x=0;x<3;++x)for(int y=0;y<3;++y) {
            inertia+=(double)b.axis[x][i]*local[x][y]*b.axis[y][j];
            inv+=(double)b.axis[x][i]*inverse[x][y]*b.axis[y][j];
        }
        b.inertia[i][j]=(float)inertia;b.pulse.world_inverse_inertia[i][j]=(float)inv;
    }
    return finite3(b.inertia[0])&&finite3(b.inertia[1])&&finite3(b.inertia[2])&&
        finite3(b.pulse.world_inverse_inertia[0])&&finite3(b.pulse.world_inverse_inertia[1])&&
        finite3(b.pulse.world_inverse_inertia[2]);
}
bool integrateForces(vehicle_pulse_body_t &b,const vehicle_ode_step_t &settings,float seconds) {
    /* T5 integrates force once before row prologs. Full tensor form preserves
     * model products of inertia rather than pretending the axes are principal. */
    float momentum[3],gyro[3],angular[3],forcedAngular[3];
    for(int i=0;i<3;++i)forcedAngular[i]=b.pulse.angular[i]+seconds*pdot(b.pulse.world_inverse_inertia[i],b.torque);
    for(int i=0;i<3;++i)momentum[i]=pdot(b.inertia[i],forcedAngular);
    const double previousEnergy=pdot(momentum,forcedAngular);
    pcross(momentum,forcedAngular,gyro);
    for(int i=0;i<3;++i) {
        b.pulse.linear[i]+=seconds*(settings.gravity[i]+b.force[i]*b.pulse.inverse_mass);
        angular[i]=forcedAngular[i]+seconds*pdot(b.pulse.world_inverse_inertia[i],gyro);
    }
    if(!finite3(b.pulse.linear)||!finite3(angular))return false;
    const double speed=std::sqrt((double)angular[0]*angular[0]+(double)angular[1]*angular[1]+(double)angular[2]*angular[2]);
    if(speed>settings.max_angular_velocity)for(int i=0;i<3;++i)angular[i]=(float)(angular[i]*settings.max_angular_velocity/speed);
    for(int i=0;i<3;++i)momentum[i]=pdot(b.inertia[i],angular);
    const double energy=pdot(momentum,angular);
    if(!std::isfinite(previousEnergy)||!std::isfinite(energy)||previousEnergy<0)return false;
    if(energy>previousEnergy&&energy>0.00001) {
        const double factor=std::sqrt(previousEnergy/energy);
        for(int i=0;i<3;++i)angular[i]=(float)(angular[i]*factor);
    }
    /* T5 drag belongs before constraints and caps at90% per step; applying it
     * after solving attenuates wheel impulses and changes traction. */
    const float linear=1-std::fmin(0.9f,settings.linear_drag*seconds);
    const float rotational=1-std::fmin(0.9f,settings.angular_drag*seconds);
    for(int i=0;i<3;++i){b.pulse.linear[i]*=linear;angular[i]*=rotational;}
    std::memcpy(b.pulse.angular,angular,12);return true;
}
bool prepareWheel(vehicle_pulse_context_t &c,vehicle_pulse_wheel_t &w) {
    float ra[3],rb[3]={},difference[3],normal[3],roll[3],pitch[3],axes[3][3];
    for(int i=0;i<3;++i) {
        ra[i]=w.center[i]+w.input.radius*w.suspension[i]-w.first->position[i];
        rb[i]=w.second?w.hit[i]-w.second->position[i]:w.hit[i];
        difference[i]=w.first->position[i]+ra[i]-w.hit[i];normal[i]=-w.normal[i];
        roll[i]=-w.roll*w.suspension[i];pitch[i]=-w.pitch*w.suspension[i];
    }
    const float displacement=pdot(difference,w.suspension);
    w.has_stop=displacement>=w.hard_limit-w.stop_margin;
    if(w.has_stop) {
        float lever[3],zero[3]={};
        for(int i=0;i<3;++i)lever[i]=ra[i]-w.hard_limit*w.suspension[i];
        const float pos=pdot(difference,normal)-w.hard_limit*pdot(w.suspension,normal);
        float bias=-pos/(c.seconds>0.016666668f?c.seconds:0.016666668f);
        if(bias<0)bias*=0.3f;
        if(bias< -170)bias=-170;
        w.stop_bias=bias<0?bias:0;
        vehicle_constraint_row_t limits={bias>=0?bias:0,0,-10000000,0};
        if(!PrepareVehiclePulse(&w.stop,&w.first->pulse,w.second?&w.second->pulse:NULL,
            normal,lever,rb,zero,&limits))return false;
    }
    w.input.seconds=c.seconds;w.input.position_error=pdot(normal,difference);
    for(int i=0;i<3;++i) {ra[i]-=normal[i]*w.input.position_error;rb[i]-=normal[i]*w.input.position_error;}
    /* Minimal rotation from suspension direction to contact normal, matching
     * T5 make_rotate(a,b). Side is perpendicular to suspension by construction. */
    const float cosine=pdot(w.suspension,normal);
    if(cosine<=-0.999999f)return false;
    float axis[3],firstCross[3],secondCross[3];
    pcross(w.suspension,normal,axis);pcross(axis,w.side,firstCross);pcross(axis,firstCross,secondCross);
    for(int i=0;i<3;++i) {axes[0][i]=normal[i];axes[1][i]=w.side[i]+firstCross[i]+secondCross[i]/(1+cosine);}
    pcross(axes[1],normal,axes[2]);for(int i=0;i<3;++i)axes[2][i]=-axes[2][i];
    return BuildVehicleWheelConstraints(&w.input,&w.limits)&&
        PrepareVehicleWheel(w.rows,&w.first->pulse,w.second?&w.second->pulse:NULL,
            axes,ra,rb,roll,pitch,&w.limits);
}
void pulseNear(void *opaque,void *first,void *second) {
    vehicle_pulse_context_t &c=*(vehicle_pulse_context_t *)opaque;if(c.failed)return;
    void *a=dGeomGetBody(first),*b=dGeomGetBody(second);if(a==b)return;
    vehicle_ode_body_t *ownerA=a?findBody(c.owner,a):NULL,*ownerB=b?findBody(c.owner,b):NULL;
    if((a&&!ownerA)||(b&&!ownerB)) {c.failed=true;return;}
    if(a&&b&&dAreConnectedExcluding(a,b,4))return;
    struct ContactList {vehicle_contact_geom_t points[128];unsigned count;};
    ContactList all={},reduced={};int count=dCollide(first,second,128,&all,sizeof(all.points[0]));
    if(count<0||count>128) {c.failed=true;return;}
    all.count=count;ContactList *list=&all;
    if(count>4) {Phys_ReduceContacts(a&&b?NULL:a?a:b,&all,&reduced);if(reduced.count>128){c.failed=true;return;}list=&reduced;}
    vehicle_pulse_body_t *bodyA=ownerA?pulseBody(c,ownerA):NULL,*bodyB=ownerB?pulseBody(c,ownerB):NULL;
    bool reverse=!bodyA;if(reverse) {bodyA=bodyB;bodyB=NULL;}
    if(!bodyA) {c.failed=true;return;}
    float friction=((ownerA?ownerA->friction:0)+(ownerB?ownerB->friction:0))*c.settings->friction_scale;
    float bounce=1;if(ownerA&&ownerA->bounce<bounce)bounce=ownerA->bounce;if(ownerB&&ownerB->bounce<bounce)bounce=ownerB->bounce;
    bodyA->owner->frame_contacts+=(unsigned)list->count;
    if(list->count&&!bodyA->owner->contact_reported) {
        const vehicle_contact_geom_t &g=list->points[0];
        bodyA->owner->contact_reported=1;
        ((void)0);
    }
    for(unsigned i=0;i<list->count;++i) {
        const vehicle_contact_geom_t &g=list->points[i];
        if(c.contact_count==PULSE_CONTACTS||!finite3(g.position)||!punit(g.normal)||!std::isfinite(g.depth)||g.depth<0){c.failed=true;return;}
        float axes[3][3],ra[3],rb[3],reference[3]={};
        for(int j=0;j<3;++j){axes[0][j]=reverse?g.normal[j]:-g.normal[j];ra[j]=g.position[j]-bodyA->position[j];rb[j]=bodyB?g.position[j]-bodyB->position[j]:g.position[j];}
        int least=0;if(std::fabs(axes[0][1])<std::fabs(axes[0][least]))least=1;if(std::fabs(axes[0][2])<std::fabs(axes[0][least]))least=2;
        reference[least]=1;pcross(axes[0],reference,axes[1]);const float length=std::sqrt(pdot(axes[1],axes[1]));
        for(int j=0;j<3;++j)axes[1][j]/=length;
        pcross(axes[0],axes[1],axes[2]);
        if(!PrepareVehicleContact(c.contacts+c.contact_count,&bodyA->pulse,bodyB?&bodyB->pulse:NULL,
            axes,ra,rb,g.depth,friction,bounce,c.settings->contact_max_bounce_velocity,
            pdot(c.settings->gravity,c.settings->gravity)*0.09f,c.seconds)){c.failed=true;return;}
        ++c.contact_count;
    }
}
/* Chassis against standing players/actors (absolute boxes). Sample points
 * on the body's entity-local chassis box that lie inside an obstacle box get
 * a contact pushing the body out horizontally along the shallower axis, so a
 * driven car is stopped by a player as a T5 vehicle is (measured: it stops
 * with its origin 18-20 units from the player's), and the player is not
 * moved. Same solver rows as the map contacts; the obstacle is static. */
void pulseObstacles(vehicle_pulse_context_t &c) {
    vehicle_ode_world_t *w=c.owner;
    if(!w->obstacle_count)return;
    static const float fx[5]={0,0.25f,0.5f,0.75f,1},fy[3]={0,0.5f,1},fz[2]={0,1};
    for(unsigned bi=0;bi<c.body_count&&!c.failed;++bi) {
        vehicle_pulse_body_t &b=c.bodies[bi];vehicle_ode_body_t *o=b.owner;
        if(!o->has_box)continue;
        float origin[3];
        for(int i=0;i<3;++i)origin[i]=b.position[i]-b.axis[0][i]*o->center[0]-b.axis[1][i]*o->center[1]-b.axis[2][i]*o->center[2];
        for(unsigned oi=0;oi<w->obstacle_count&&!c.failed;++oi) {
            const vehicle_obstacle_t &box=w->obstacles[oi];
            unsigned made=0;
            for(int ix=0;ix<5&&made<4;++ix)for(int iy=0;iy<3&&made<4;++iy)for(int iz=0;iz<2&&made<4;++iz) {
                const float local[3]={o->box_mins[0]+fx[ix]*(o->box_maxs[0]-o->box_mins[0]),
                    o->box_mins[1]+fy[iy]*(o->box_maxs[1]-o->box_mins[1]),
                    o->box_mins[2]+fz[iz]*(o->box_maxs[2]-o->box_mins[2])};
                float point[3];
                for(int i=0;i<3;++i)point[i]=origin[i]+b.axis[0][i]*local[0]+b.axis[1][i]*local[1]+b.axis[2][i]*local[2];
                if(point[0]<=box.mins[0]||point[0]>=box.maxs[0]||point[1]<=box.mins[1]||point[1]>=box.maxs[1]||
                   point[2]<=box.mins[2]||point[2]>=box.maxs[2])continue;
                const float dx0=point[0]-box.mins[0],dx1=box.maxs[0]-point[0];
                const float dy0=point[1]-box.mins[1],dy1=box.maxs[1]-point[1];
                const float px=dx0<dx1?dx0:dx1,py=dy0<dy1?dy0:dy1;
                float out[3]={0,0,0},depth;
                if(px<py){out[0]=dx0<dx1?-1.0f:1.0f;depth=px;}
                else{out[1]=dy0<dy1?-1.0f:1.0f;depth=py;}
                if(c.contact_count==PULSE_CONTACTS){c.failed=true;return;}
                /* Normal row axis points from the body into the obstacle; its
                 * impulse is clamped <= 0, pushing the body back out. */
                float axes[3][3],ra[3],rb[3];
                for(int i=0;i<3;++i){axes[0][i]=-out[i];axes[1][i]=i==2?1.0f:0.0f;ra[i]=point[i]-b.position[i];rb[i]=point[i];}
                pcross(axes[0],axes[1],axes[2]);
                if(!PrepareVehicleContact(c.contacts+c.contact_count,&b.pulse,NULL,axes,ra,rb,depth,
                    o->friction*c.settings->friction_scale,0,c.settings->contact_max_bounce_velocity,
                    pdot(c.settings->gravity,c.settings->gravity)*0.09f,c.seconds)){c.failed=true;return;}
                ++c.contact_count;++made;o->frame_contacts++;
            }
        }
    }
}
/* pulse_sum_angular::set + calc_abs (phys_constraint_solver_multithreaded.cpp
 * 1092/2406) for b2 = environment: the base angular velocity is folded into
 * the right-hand side the way PrepareVehiclePulse folds linear rows. */
bool addAngular(vehicle_pulse_context_t &c,vehicle_pulse_body_t &b,const float ud[3],
    float rhs,float cfm,float minimum,float maximum) {
    if(c.angular_count==PULSE_ANGULAR||!punit(ud)||!std::isfinite(rhs)||!std::isfinite(cfm)||cfm<0||
       !std::isfinite(minimum)||!std::isfinite(maximum)||minimum>maximum)return false;
    vehicle_pulse_angular_t &r=c.angular[c.angular_count];
    r.body=&b.pulse;std::memcpy(r.direction,ud,12);
    for(int i=0;i<3;++i)r.application[i]=pdot(b.pulse.world_inverse_inertia[i],ud);
    r.denominator=pdot(ud,r.application)+cfm;
    r.cfm=cfm;r.minimum=minimum;r.maximum=maximum;r.impulse=0;
    r.rhs=rhs-pdot(ud,b.pulse.angular);
    if(!finite3(r.application)||!std::isfinite(r.denominator)||r.denominator<=0.00001f||!std::isfinite(r.rhs))return false;
    ++c.angular_count;return true;
}
/* pulse_sum_angular::get_pos: -ud . (b1_r x b2_r). */
float angularPosition(const float ud[3],const float first[3],const float second[3]) {
    float crossed[3];pcross(first,second,crossed);return -pdot(ud,crossed);
}
/* rigid_body_constraint_custom_orientation::setup_constraint
 * (OpenBLOPS Engine/Core/tl/physics/rbc_def_custom.cpp:39). b1 is the vehicle,
 * b2 the static environment body (identity orientation). Rows, in T5 order:
 *   pitch/roll resistance: ud = b1.y and b1.x, cfm = 1/(10 k dt), rhs 0;
 *   yaw resistance:        ud = b1.z,           cfm = 1/(10 k dt), rhs 0;
 *   upright pitch:         ud = b1.y, pos from b1.z x env.z, limit
 *                          30 k dt |b1.x.z|, rhs -pos/dt;
 *   upright roll:          ud = b1.x, limit 100 k dt |b1.y.z|.
 * Upside down (b1.z.z < 0) both upright limits use full strength. With
 * m_no_orientation_correction (airborne) the upright rows keep a one-sided
 * limit and a zero target, resisting further tilt without righting. */
bool prepareOrientation(vehicle_pulse_context_t &c,vehicle_pulse_body_t &b) {
    const vehicle_pulse_orientation_t &o=b.orientation;
    if(!o.requested)return true;
    const float *forward=b.axis[0],*left=b.axis[1],*up=b.axis[2];
    static const float envUp[3]={0,0,1};
    const float dt=c.seconds;
    const vehicle_orientation_t &s=o.strength;
    if(s.pitch_resistance>0&&
       !addAngular(c,b,left,0,1.0f/(s.pitch_resistance*10.0f*dt),-10000000.0f,10000000.0f))return false;
    if(s.roll_resistance>0&&
       !addAngular(c,b,forward,0,1.0f/(s.roll_resistance*10.0f*dt),-10000000.0f,10000000.0f))return false;
    if(s.yaw_resistance>0&&
       !addAngular(c,b,up,0,1.0f/(s.yaw_resistance*10.0f*dt),-10000000.0f,10000000.0f))return false;
    float roll=std::fabs(left[2]),pitch=std::fabs(forward[2]);
    if(up[2]<0)pitch=roll=1.0f;
    float minimum,maximum,target;
    if(s.upright_pitch>0) {
        const float pitchLimit=s.upright_pitch*30.0f*dt*pitch;
        if(o.no_orientation_correction) {
            if(forward[2]>=0){minimum=0;maximum=pitchLimit;}else{minimum=-pitchLimit;maximum=0;}
            target=0;
        } else {
            minimum=-pitchLimit;maximum=pitchLimit;
            target=angularPosition(left,up,envUp)*(-1.0f/dt);
        }
        if(!addAngular(c,b,left,target,0,minimum,maximum))return false;
    }
    if(s.upright_roll>0) {
        float rollLimit=dt*(s.upright_roll*100.0f)*roll;
        if(o.no_orientation_correction) {
            rollLimit*=0.5f;
            if(left[2]<=0){minimum=0;maximum=rollLimit;}else{minimum=-rollLimit;maximum=0;}
            target=0;
        } else {
            minimum=-rollLimit;maximum=rollLimit;
            target=angularPosition(forward,up,envUp)*(-1.0f/dt);
        }
        if(!addAngular(c,b,forward,target,0,minimum,maximum))return false;
    }
    return true;
}
/* pulse_sum_angular::SOLVER_apply_relaxation. */
bool relaxAngular(vehicle_pulse_angular_t &r,float *maximum) {
    const float objective=pdot(r.direction,r.body->delta_angular);
    float next=r.impulse-(objective+r.cfm*r.impulse-r.rhs)/r.denominator;
    if(!std::isfinite(next))return false;
    next=next<r.minimum?r.minimum:next>r.maximum?r.maximum:next;
    const float delta=next-r.impulse;
    for(int i=0;i<3;++i)r.body->delta_angular[i]+=r.application[i]*delta;
    if(!finite3(r.body->delta_angular))return false;
    r.impulse=next;
    float e=delta*r.denominator;e*=e;if(e>*maximum)*maximum=e;
    return true;
}
bool relaxWorld(vehicle_pulse_context_t &c,int iterations,float threshold) {
    /* T5 solves hard-limit normals, angular rows, wheels, then contacts every
     * iteration (pulse_sum_constraint_solver relaxation loop). */
    for(int pass=0;pass<=iterations;++pass) {
        float error=0;
        for(unsigned i=0;i<c.wheel_count;++i)if(c.wheels[i].has_stop&&!RelaxVehiclePulse(&c.wheels[i].stop,&error))return false;
        for(unsigned i=0;i<c.angular_count;++i)if(!relaxAngular(c.angular[i],&error))return false;
        for(unsigned i=0;i<c.wheel_count;++i)if(!RelaxVehicleWheel(c.wheels[i].rows,&c.wheels[i].limits,&error))return false;
        for(unsigned i=0;i<c.contact_count;++i)if(!RelaxVehicleContact(c.contacts+i,&error))return false;
        if(error<=threshold)break;
    }
    return true;
}
bool preparePose(vehicle_pulse_body_t &b,float seconds) {
    float velocity[3],angular[3],rotationMatrix[3][3]={};
    for(int i=0;i<3;++i) {velocity[i]=b.pulse.linear[i]+b.pulse.delta_linear[i];angular[i]=b.pulse.angular[i]+b.pulse.delta_angular[i];}
    if(!finite3(velocity)||!finite3(angular))return false;
    const double speed=std::sqrt((double)angular[0]*angular[0]+(double)angular[1]*angular[1]+(double)angular[2]*angular[2]);
    const double angle=speed*seconds,cosine=std::cos(angle),sine=std::sin(angle);
    if(!std::isfinite(angle)||!std::isfinite(cosine)||!std::isfinite(sine))return false;
    float axis[3]={};if(speed>0)for(int i=0;i<3;++i)axis[i]=(float)(angular[i]/speed);
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)rotationMatrix[i][j]=(float)((i==j?cosine:0)+(1-cosine)*axis[i]*axis[j]);
    rotationMatrix[0][1]-=(float)(sine*axis[2]);rotationMatrix[0][2]+=(float)(sine*axis[1]);
    rotationMatrix[1][0]+=(float)(sine*axis[2]);rotationMatrix[1][2]-=(float)(sine*axis[0]);
    rotationMatrix[2][0]-=(float)(sine*axis[1]);rotationMatrix[2][1]+=(float)(sine*axis[0]);
    for(int i=0;i<3;++i) {
        b.next_position[i]=b.position[i]+seconds*velocity[i];
        b.next_linear[i]=velocity[i];b.next_angular[i]=angular[i];
        for(int j=0;j<3;++j)b.next_axis[i][j]=pdot(rotationMatrix[j],b.axis[i]);
    }
    b.next_tick=b.owner->rotation_tick+1;
    if(b.next_tick>5) {
        b.next_tick=0;
        float length=std::sqrt(pdot(b.next_axis[0],b.next_axis[0]));if(!std::isfinite(length)||length<=0)return false;
        for(int j=0;j<3;++j)b.next_axis[0][j]/=length;
        const float overlap=pdot(b.next_axis[0],b.next_axis[1]);
        for(int j=0;j<3;++j)b.next_axis[1][j]-=overlap*b.next_axis[0][j];
        length=std::sqrt(pdot(b.next_axis[1],b.next_axis[1]));if(!std::isfinite(length)||length<=0)return false;
        for(int j=0;j<3;++j)b.next_axis[1][j]/=length;
        pcross(b.next_axis[0],b.next_axis[1],b.next_axis[2]);
    }
    return finite3(b.next_position)&&finite3(b.next_linear)&&finite3(b.next_angular)&&rotation(b.next_axis);
}
}
static int pulseForceAtPoint(vehicle_pulse_context_t *c,vehicle_ode_body_t *owner,const float *force,const float *point) {
    if(!c||!c->preparing)return 0;
    vehicle_pulse_body_t *b=pulseBody(*c,owner);if(!b)return 0;
    float lever[3],torque[3];for(int i=0;i<3;++i)lever[i]=point[i]-b->position[i];pcross(lever,force,torque);
    for(int i=0;i<3;++i){b->force[i]+=force[i];b->torque[i]+=torque[i];}
    if(!finite3(b->force)||!finite3(b->torque)){c->failed=true;return 0;}return 1;
}
static int pulseCentralForce(vehicle_pulse_context_t *c,vehicle_ode_body_t *owner,const float *force) {
    if(!c||!c->preparing)return 0;
    vehicle_pulse_body_t *b=pulseBody(*c,owner);if(!b)return 0;
    for(int i=0;i<3;++i)b->force[i]+=force[i];
    if(!finite3(b->force)){c->failed=true;return 0;}return 1;
}
extern "C" int VehicleODEAddWheel(vehicle_ode_body_t *owner,vehicle_ode_body_t *hitBody,
    const float center[3],const float suspension[3],const float side[3],const float hit[3],const float normal[3],
    float hardLimit,float stopMargin,float roll,float pitch,const vehicle_wheel_constraint_input_t *input,float cache[4]) {
    vehicle_pulse_context_t *c=owner&&owner->owner?owner->owner->pulse:NULL;
    if(!c||!c->preparing||!input||!cache||owner==hitBody||!finite3(center)||!finite3(hit)||
       !punit(suspension)||!punit(side)||!punit(normal)||std::fabs(pdot(suspension,side))>0.001f||
       !std::isfinite(hardLimit)||hardLimit<0||!std::isfinite(stopMargin)||stopMargin<0||
       !std::isfinite(roll)||!std::isfinite(pitch))return 0;
    if(c->wheel_count==PULSE_WHEELS){c->failed=true;return 0;}
    for(unsigned i=0;i<c->wheel_count;++i)if(c->wheels[i].cache==cache){c->failed=true;return 0;}
    vehicle_pulse_wheel_t &w=c->wheels[c->wheel_count];w.first=pulseBody(*c,owner);w.second=hitBody?pulseBody(*c,hitBody):NULL;
    if(!w.first||(hitBody&&!w.second))return 0;
    for(int i=0;i<4;++i){if(!std::isfinite(cache[i]))return 0;w.old_cache[i]=cache[i];}
    std::memcpy(w.center,center,12);std::memcpy(w.suspension,suspension,12);std::memcpy(w.side,side,12);
    std::memcpy(w.hit,hit,12);std::memcpy(w.normal,normal,12);w.hard_limit=hardLimit;w.stop_margin=stopMargin;
    w.roll=roll;w.pitch=pitch;w.input=*input;w.cache=cache;
    ++c->wheel_count;return 1;
}
extern "C" int VehicleODEAddOrientation(vehicle_ode_body_t *owner,
    const vehicle_orientation_t *strength,int wheelsAirborne) {
    vehicle_pulse_context_t *c=owner&&owner->owner?owner->owner->pulse:NULL;
    if(!c||!c->preparing||!strength)return 0;
    const float values[]={strength->pitch_resistance,strength->roll_resistance,strength->yaw_resistance,
        strength->upright_pitch,strength->upright_roll};
    for(unsigned i=0;i<5;++i)if(!std::isfinite(values[i])||values[i]<0)return 0;
    vehicle_pulse_body_t *b=pulseBody(*c,owner);if(!b||b->orientation.requested)return 0;
    b->orientation.strength=*strength;b->orientation.requested=true;
    /* T5 _update_orientation_constraint: !m_num_colliding_wheels &&
     * !body->m_contact_count (the previous solve's chassis contacts). */
    b->orientation.no_orientation_correction=wheelsAirborne&&owner->previous_contacts==0;
    return 1;
}
extern "C" int StepVehiclePulseWorld(vehicle_ode_world_t *w,float seconds,
    const vehicle_ode_step_t *settings,int positionIterations,float velocityError,float positionError,
    vehicle_ode_force_fn prepare,void *opaque) {
    if(!w||w->stepping||!settings||!prepare||!validStep(*settings)||!std::isfinite(seconds)||seconds<=0||
       settings->max_angular_velocity<=0||positionIterations<0||!std::isfinite(velocityError)||velocityError<0||!std::isfinite(positionError)||positionError<0)return 0;
    if(!w->pulse_storage)w->pulse_storage=std::calloc(1,sizeof(vehicle_pulse_context_t));
    vehicle_pulse_context_t *c=(vehicle_pulse_context_t *)w->pulse_storage;if(!c)return 0;
    std::memset(c,0,sizeof(*c));
    c->owner=w;c->settings=settings;c->seconds=seconds;
    for(vehicle_ode_body_t *b=w->bodies;b;b=b->next) {
        b->previous_contacts=b->frame_contacts;b->frame_contacts=0;
        if(c->body_count==PULSE_BODIES||!readPulseBody(b,c->bodies[c->body_count]))return 0;
        ++c->body_count;
    }
    w->stepping=1;w->pulse=c;c->preparing=true;
    for(unsigned i=0;i<c->body_count&&!c->failed;++i)if(!prepare(c->bodies[i].owner,seconds,opaque))c->failed=true;
    c->preparing=false;
    for(unsigned i=0;i<c->body_count&&!c->failed;++i)if(!integrateForces(c->bodies[i],*settings,seconds))c->failed=true;
    for(unsigned i=0;i<c->wheel_count&&!c->failed;++i)if(!prepareWheel(*c,c->wheels[i]))c->failed=true;
    for(unsigned i=0;i<c->body_count&&!c->failed;++i)if(!prepareOrientation(*c,c->bodies[i]))c->failed=true;
    if(!c->failed) {
        dSpaceCollide(w->space,c,pulseNear);void *map=Phys_GetWorldGeom();if(!map)c->failed=true;
        for(vehicle_ode_body_t *b=w->bodies;b&&!c->failed;b=b->next)
            for(void *g=ODE_BodyGetFirstGeom(b->body);g&&!c->failed;g=dGeomGetBodyNext(g))pulseNear(c,g,map);
        if(!c->failed)pulseObstacles(*c);
    }
    for(unsigned i=0;i<c->wheel_count&&!c->failed;++i) {
        vehicle_pulse_wheel_t &wheel=c->wheels[i];
        if(wheel.has_stop&&!WarmVehiclePulse(&wheel.stop,wheel.old_cache[0],seconds))c->failed=true;
        if(!c->failed&&!WarmVehicleWheel(wheel.rows,&wheel.limits,wheel.old_cache+1,seconds))c->failed=true;
    }
    /* Chassis manifolds currently cold-start: contacts are rebuilt from real
     * ODE geometry every pass. Never reuse cache by list index across changes. */
    if(!c->failed&&!relaxWorld(*c,settings->iterations,velocityError))c->failed=true;
    for(unsigned i=0;i<c->wheel_count&&!c->failed;++i) {
        vehicle_pulse_wheel_t &wheel=c->wheels[i];
        wheel.next_cache[0]=wheel.has_stop?wheel.stop.impulse/seconds:0;
        for(int r=0;r<3;++r)wheel.next_cache[r+1]=(r<2||wheel.limits.has_forward)?wheel.rows[r].impulse/seconds:0;
        for(int r=0;r<4;++r)if(!std::isfinite(wheel.next_cache[r]))c->failed=true;
        if(wheel.has_stop)wheel.stop.rhs+=wheel.stop_bias;
    }
    for(unsigned i=0;i<c->contact_count;++i)c->contacts[i].rows[0].rhs+=c->contacts[i].position_bias;
    if(!c->failed&&!relaxWorld(*c,positionIterations,positionError))c->failed=true;
    for(unsigned i=0;i<c->body_count&&!c->failed;++i)
        if(!preparePose(c->bodies[i],seconds))c->failed=true;
    if(!c->failed) {
        /* All computations and cache validation succeeded before first native
         * mutation. ODE geometry setters have no script callbacks. */
        for(unsigned i=0;i<c->body_count;++i) {
            vehicle_pulse_body_t &b=c->bodies[i];float matrix[12]={};
            for(int x=0;x<3;++x)for(int y=0;y<3;++y)matrix[x*4+y]=b.next_axis[y][x];
            dBodySetPosition(b.owner->body,b.next_position[0],b.next_position[1],b.next_position[2]);dBodySetRotation(b.owner->body,matrix);
            dBodySetLinearVel(b.owner->body,b.next_linear[0],b.next_linear[1],b.next_linear[2]);
            dBodySetAngularVel(b.owner->body,b.next_angular[0],b.next_angular[1],b.next_angular[2]);ODE_PostStepBody(b.owner->body);
            b.owner->rotation_tick=b.next_tick;
        }
        for(unsigned i=0;i<c->wheel_count;++i)std::memcpy(c->wheels[i].cache,c->wheels[i].next_cache,16);
    }
    const bool success=!c->failed;w->pulse=NULL;w->stepping=0;return success;
}

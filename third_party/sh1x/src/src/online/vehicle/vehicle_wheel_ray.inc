/* Real model-physics ray query. The retained ODE custom world/brush/cylinder
 * classes lack a ray consumer, so intersect the owned decoded collision data
 * directly, and use native CM point traces for map/static-model geometry. */
namespace {
struct wheel_interval_t {
    double enter,leave;
    float enter_normal[3],leave_normal[3];
    bool outside;
};
bool wheelPlane(wheel_interval_t &interval,const float *normal,double distance,
    const float *start,const float *delta) {
    const double from=pdot(normal,start)-distance,rate=pdot(normal,delta);
    if(from>0)interval.outside=true;
    if(rate==0)return from<=0;
    const double time=-from/rate;
    if(rate<0) {
        if(time>interval.enter){interval.enter=time;std::memcpy(interval.enter_normal,normal,12);}
    } else if(time<interval.leave){interval.leave=time;std::memcpy(interval.leave_normal,normal,12);}
    return interval.enter<=interval.leave;
}
bool wheelBox(wheel_interval_t &interval,const float *mins,const float *maxs,
    const float *start,const float *delta) {
    for(int axis=0;axis<3;++axis) {
        float normal[3]={};normal[axis]=1;
        if(!wheelPlane(interval,normal,maxs[axis],start,delta))return false;
        normal[axis]=-1;if(!wheelPlane(interval,normal,-mins[axis],start,delta))return false;
    }
    return true;
}
bool wheelCylinder(wheel_interval_t &interval,float halfLength,float radius,
    const float *start,const float *delta) {
    float normal[3]={1,0,0};
    if(!wheelPlane(interval,normal,halfLength,start,delta))return false;
    normal[0]=-1;if(!wheelPlane(interval,normal,halfLength,start,delta))return false;
    const double a=(double)delta[1]*delta[1]+(double)delta[2]*delta[2];
    const double b=(double)start[1]*delta[1]+(double)start[2]*delta[2];
    const double c=(double)start[1]*start[1]+(double)start[2]*start[2]-(double)radius*radius;
    if(c>0)interval.outside=true;
    if(a==0)return c<=0;
    const double discriminant=b*b-a*c;if(discriminant<0)return false;
    const double root=std::sqrt(discriminant),entryRoot=(-b-root)/a,exitRoot=(-b+root)/a;
    if(entryRoot>interval.enter) {
        interval.enter=entryRoot;interval.enter_normal[0]=0;
        for(int i=1;i<3;++i)interval.enter_normal[i]=(float)((start[i]+entryRoot*delta[i])/radius);
    }
    if(exitRoot<interval.leave) {
        interval.leave=exitRoot;interval.leave_normal[0]=0;
        for(int i=1;i<3;++i)interval.leave_normal[i]=(float)((start[i]+exitRoot*delta[i])/radius);
    }
    return interval.enter<=interval.leave;
}
/* 1 hit,0 no hit,-1 malformed; exact convex collision geometry, not its AABB.
 * Brush min/max are six axial planes, followed by its authored bevel sides. */
int wheelShape(const PhysGeomInfo_t &shape,const float modelStart[3],const float modelDelta[3],
    int mask,float maximum,float *fraction,float modelNormal[3],int *inside) {
    float start[3],delta[3];wheel_interval_t interval={-1e30,1e30,{},{},false};
    bool overlaps=false;
    if(shape.brush) {
        const BrushWrapper_t &brush=*shape.brush;
        if(!(brush.contents&mask))return 0;
        if(!finite3(brush.mins)||!finite3(brush.maxs)||(brush.numsides&&!brush.sides))return -1;
        for(int i=0;i<3;++i)if(brush.mins[i]>brush.maxs[i])return -1;
        overlaps=wheelBox(interval,brush.mins,brush.maxs,modelStart,modelDelta);
        for(unsigned i=0;i<brush.numsides&&overlaps;++i) {
            const cplane_t *plane=brush.sides[i].plane;
            if(!plane||!finite3(plane->normal)||!std::isfinite(plane->dist))return -1;
            overlaps=wheelPlane(interval,plane->normal,plane->dist,modelStart,modelDelta);
        }
    } else {
        if(!rotation(shape.orientation)||!finite3(shape.offset)||!finite3(shape.halfLengths))return -1;
        float shifted[3];for(int i=0;i<3;++i)shifted[i]=modelStart[i]-shape.offset[i];
        for(int i=0;i<3;++i){start[i]=pdot(shape.orientation[i],shifted);delta[i]=pdot(shape.orientation[i],modelDelta);}
        if(shape.type==1) {
            float mins[3];for(int i=0;i<3;++i){if(shape.halfLengths[i]<=0)return -1;mins[i]=-shape.halfLengths[i];}
            overlaps=wheelBox(interval,mins,shape.halfLengths,start,delta);
        } else if(shape.type==4) {
            if(shape.halfLengths[0]<=0||shape.halfLengths[1]<=0)return -1;
            /* Phys_GetCylinderAABB consumes parameter+8 as HALF-length along
             * direction-1, matching owned constructor direction1/halfLengths.x. */
            overlaps=wheelCylinder(interval,shape.halfLengths[0],shape.halfLengths[1],start,delta);
        } else return -1;
    }
    if(!overlaps)return 0;
    const double time=interval.outside?interval.enter:interval.leave;
    if(time<0||time>=maximum||time>1)return 0;
    const float *normal=interval.outside?interval.enter_normal:interval.leave_normal;
    if(shape.brush)std::memcpy(modelNormal,normal,12);
    else for(int i=0;i<3;++i)modelNormal[i]=shape.orientation[0][i]*normal[0]+shape.orientation[1][i]*normal[1]+shape.orientation[2][i]*normal[2];
    const float length=std::sqrt(pdot(modelNormal,modelNormal));
    if(!std::isfinite(length)||length<=0.000001f)return 0; // T5 collision_epilog invalid-normal gate.
    for(int i=0;i<3;++i)modelNormal[i]/=length;
    *fraction=(float)time;*inside=!interval.outside;return 1;
}
}
extern "C" int VehicleODEWheelRay(vehicle_ode_body_t *ignored,const float start[3],
    const float end[3],int mask,vehicle_wheel_hit_t *out) {
    vehicle_pulse_context_t *c=ignored&&ignored->owner?ignored->owner->pulse:NULL;
    if(!c||!c->preparing||!out||!mask||!finite3(start)||!finite3(end))return 0;
    float delta[3];for(int i=0;i<3;++i)delta[i]=end[i]-start[i];
    if(!finite3(delta)||pdot(delta,delta)<=0)return 0;
    vehicle_wheel_hit_t hit={};hit.fraction=1;hit.entity=1023;
    trace_t trace={};const float zero[3]={};
    CM_BoxTrace(&trace,start,end,zero,zero,0,mask);
    CM_PointTraceStaticModels(&trace,start,end,mask);
    if(!std::isfinite(trace.fraction)||trace.fraction<0||trace.fraction>1)return 0;
    const float length=std::sqrt(pdot(trace.normal,trace.normal));
    hit.starts_inside=trace.startsolid||trace.allsolid;
    if(trace.fraction<1&&std::isfinite(length)&&length>0.000001f) {
        hit.fraction=trace.fraction;hit.surface_flags=trace.sflags;
        for(int i=0;i<3;++i)hit.normal[i]=trace.normal[i]/length;
    }
    for(unsigned bodyIndex=0;bodyIndex<c->body_count;++bodyIndex) {
        vehicle_pulse_body_t &body=c->bodies[bodyIndex];if(body.owner==ignored)continue;
        float relative[3],modelStart[3],modelDelta[3];
        for(int i=0;i<3;++i)relative[i]=start[i]-body.position[i];
        for(int i=0;i<3;++i){modelStart[i]=pdot(body.axis[i],relative)+body.owner->center[i];modelDelta[i]=pdot(body.axis[i],delta);}
        for(unsigned shape=0;shape<body.owner->geometry.map.count;++shape) {
            float fraction,normal[3];int inside=0;
            const int result=wheelShape(body.owner->geometry.map.geoms[shape],modelStart,modelDelta,mask,hit.fraction,&fraction,normal,&inside);
            if(result<0)return 0;
            if(!result)continue;
            hit.fraction=fraction;hit.starts_inside=inside;hit.body=body.owner;
            hit.entity=body.owner->entity;hit.generation=body.owner->generation;hit.surface_flags=0;
            for(int i=0;i<3;++i)hit.normal[i]=body.axis[0][i]*normal[0]+body.axis[1][i]*normal[1]+body.axis[2][i]*normal[2];
        }
    }
    for(int i=0;i<3;++i)hit.point[i]=start[i]+delta[i]*hit.fraction;
    if(!finite3(hit.point)||!finite3(hit.normal))return 0;
    *out=hit;return 1;
}

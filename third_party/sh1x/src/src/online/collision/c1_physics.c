/* PhysPreset and PhysCollmap from C1 records (feat/iw4-asset-layout, stage 2 step 3).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The physics readers (physics.c) parse the stream C1's loader reads. These write that stream from the record
 * C1 holds, every present pointer inline (-1) in loader order (replay_physics), so the same readers and native
 * builders run on a C1 record. */
#include "c1_physics.h"
#include <stdlib.h>
#include <string.h>

static const uint8_t *ptr(const uint8_t *p,unsigned offset) { const uint8_t *v; memcpy(&v,p+offset,sizeof(v)); return v; }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
typedef struct { uint8_t *data; size_t size,capacity; int failed; } out_t;
static size_t emit(out_t *o,const void *src,size_t n)
{
    size_t at=o->size;
    if(o->failed) return at;
    if(o->size+n>o->capacity) {
        size_t c=o->capacity ? o->capacity : 1024; uint8_t *g;
        while(c<o->size+n) c*=2;
        if(!(g=realloc(o->data,c))) { o->failed=1; return at; }
        o->data=g; o->capacity=c;
    }
    if(n) { if(src) memcpy(o->data+o->size,src,n); else memset(o->data+o->size,0,n); }
    o->size+=n; return at;
}
static void mark(out_t *o,size_t at,const void *present)
{ if(!o->failed) { uint32_t v=present ? UINT32_MAX : 0; memcpy(o->data+at,&v,4); } }

int SerializeC1PhysicsPreset(const uint8_t *record,asset_t *out)
{
    out_t o={0}; const char *name,*sound; size_t h;
    if(!record || !out || !(name=(const char *)ptr(record,0))) return 0;
    sound=(const char *)ptr(record,28);
    h=emit(&o,record,68); mark(&o,h,name); mark(&o,h+28,sound);
    emit(&o,name,strlen(name)+1);
    if(sound) emit(&o,sound,strlen(sound)+1);
    if(o.failed) { free(o.data); return 0; }
    *out=(asset_t){0,o.data,o.size}; return 1;
}

int SerializeC1PhysicsMap(const uint8_t *record,asset_t *out)
{
    out_t o={0}; const char *name; const uint8_t *geoms; size_t h,g; uint32_t count,i,j;
    if(!record || !out || !(name=(const char *)ptr(record,0))) return 0;
    count=u32(record+4); geoms=ptr(record,8);
    if(count && !geoms) return 0;
    h=emit(&o,record,72); mark(&o,h,name); mark(&o,h+8,count ? geoms : NULL);
    emit(&o,name,strlen(name)+1);
    g=emit(&o,geoms,68*(size_t)count);
    for(i=0;i<count;++i) {
        const uint8_t *brush=ptr(geoms+68*i,0),*sides,*adjacency,*planes; size_t b,s; unsigned n;
        mark(&o,g+68*i,brush);
        if(!brush) continue;
        n=brush[24]|(unsigned)brush[25]<<8;
        sides=ptr(brush,28); adjacency=ptr(brush,32); planes=ptr(brush,64);
        b=emit(&o,brush,68);
        mark(&o,b+28,sides); mark(&o,b+32,adjacency); mark(&o,b+64,planes);
        if(sides) {
            s=emit(&o,sides,8*(size_t)n);
            for(j=0;j<n;++j) {
                const uint8_t *plane=ptr(sides+8*j,0);
                mark(&o,s+8*j,plane);
                if(plane) emit(&o,plane,20);
            }
        }
        if(adjacency) emit(&o,adjacency,u32(brush+60));
        if(planes) emit(&o,planes,20*(size_t)n);
    }
    if(o.failed) { free(o.data); return 0; }
    *out=(asset_t){1,o.data,o.size}; return 1;
}

static int same_bytes_masked(const uint8_t *a,const uint8_t *b,size_t n,const unsigned *mask,unsigned masks)
{
    size_t i; unsigned k;
    for(i=0;i<n;i+=4) {
        int skip=0;
        for(k=0;k<masks;++k) skip|=i==mask[k];
        if(!skip && memcmp(a+i,b+i,n-i<4 ? n-i : 4)) return 0;
    }
    return 1;
}
int ComparePhysicsMaps(const physics_map_t *a,const physics_map_t *b)
{
    static const unsigned header_mask[]={0,8},geom_mask[]={0},brush_mask[]={28,32,64};
    uint32_t i,j;
    if(!a->name || !b->name || strcmp(a->name,b->name) || a->count!=b->count) return 1;
    if(!same_bytes_masked(a->header,b->header,72,header_mask,2)) return 2;
    for(i=0;i<a->count;++i) {
        const physics_geometry_t *x=a->geometries+i,*y=b->geometries+i;
        if(!same_bytes_masked(x->record,y->record,68,geom_mask,1)) return 3;
        if(!x->brush!=!y->brush) return 4;
        if(!x->brush) continue;
        if(!same_bytes_masked(x->brush,y->brush,68,brush_mask,3) || x->side_count!=y->side_count ||
           x->adjacency_size!=y->adjacency_size) return 5;
        if(!x->sides!=!y->sides || !x->adjacency!=!y->adjacency || !x->planes!=!y->planes) return 6;
        for(j=0;x->sides && j<x->side_count;++j) {
            if(memcmp(x->sides+8*j+4,y->sides+8*j+4,4)) return 7;
            if(!x->side_planes[j]!=!y->side_planes[j] || (x->side_planes[j] && memcmp(x->side_planes[j],y->side_planes[j],20))) return 8;
        }
        if(x->adjacency && memcmp(x->adjacency,y->adjacency,x->adjacency_size)) return 9;
        if(x->planes && memcmp(x->planes,y->planes,20*(size_t)x->side_count)) return 10;
    }
    return 0;
}
int ComparePhysicsPresets(const physics_preset_t *a,const physics_preset_t *b)
{
    if(!a->name || !b->name || strcmp(a->name,b->name)) return 1;
    if(!a->sound_prefix!=!b->sound_prefix || (a->sound_prefix && strcmp(a->sound_prefix,b->sound_prefix))) return 2;
    if(a->type!=b->type || a->mass!=b->mass || a->bounce!=b->bounce || a->friction!=b->friction ||
       a->bullet_force_scale!=b->bullet_force_scale || a->explosive_force_scale!=b->explosive_force_scale ||
       a->pieces_spread_fraction!=b->pieces_spread_fraction || a->pieces_upward_velocity!=b->pieces_upward_velocity ||
       memcmp(a->extended,b->extended,sizeof(a->extended))) return 3;
    return 0;
}

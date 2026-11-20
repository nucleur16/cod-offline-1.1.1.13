/* XModel conversion from a C1 record (feat/iw4-asset-layout, stage 2 step 3).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The retained IW3 model is built from mounted_model_skeleton_t / _geometry_t. These read them from the
 * XModel record C1 holds (0x168 bytes, real pointers: C1Record or C1's own pool) instead of parsing the
 * serialized zone image. The header layout is the serialized one (C1 0x4bbb80), so the scalar fields keep their
 * offsets; arrays are followed through their pointers. Reference fields that later steps still resolve through
 * the replay (lod surfaces, physics) come from the header copy, which holds the serialized words. */
#include "c1_xmodel.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static float f32(const uint8_t *p)
{ uint32_t bits=u32(p); float v; memcpy(&v,&bits,4); return v; }
static const uint8_t *ptr(const uint8_t *p)
{ const uint8_t *v; memcpy(&v,p,sizeof(v)); return v; }
static void put32(uint8_t *p,uint32_t value)
{ unsigned i; for (i=0;i<4;++i) p[i]=(uint8_t)(value>>(8*i)); }

int ReadC1XModelSkeleton(const uint8_t *record,const uint8_t *header_copy,
    const char *(*text)(void *,unsigned),void *context,
    xmodel_skeleton_t *skeleton,xmodel_bone_names_t *names)
{
    static const unsigned strides[6]={2,1,8,12,1,32};
    xmodel_skeleton_t result={0}; xmodel_bone_names_t bones={0};
    const char *name; size_t size=0x168,name_length,lengths[6],p,pos=0,text_size=0;
    unsigned i,count[6]; uint8_t *data; asset_t asset;
    if (!record || !header_copy || !text || !skeleton || !names || !(name=(const char *)ptr(record)) ||
        record[5]>record[4]) return 0;
    /* The same synthetic inline asset RelocateXModelSkeleton builds, from the record's arrays. */
    name_length=strlen(name)+1; size+=name_length;
    for (i=0;i<6;++i) {
        count[i]=i>=1 && i<=3 ? record[4]-record[5] : record[4];
        lengths[i]=(size_t)count[i]*strides[i];
        if (lengths[i] && !ptr(record+0x2c+4*i)) return 0;
        size+=lengths[i];
    }
    if (!(data=calloc(size,1))) return 0;
    memcpy(data,record,0x168);
    put32(data,UINT32_MAX); p=0x168; memcpy(data+p,name,name_length); p+=name_length;
    for (i=0;i<6;++i) {
        put32(data+0x2c+4*i,UINT32_MAX);
        if (lengths[i]) memcpy(data+p,ptr(record+0x2c+4*i),lengths[i]);
        p+=lengths[i];
    }
    asset=(asset_t){4,data,size};
    i=ReadXModelSkeleton(&asset,&result); free(data);
    if (!i) return 0;
    /* serialized references and the stream extent stay those of the zone's definition */
    result.name_reference=u32(header_copy); result.consumed=0;
    for (i=0;i<6;++i) result.arrays[i].reference=u32(header_copy+0x2c+4*i);
    /* bone names: the record holds converted script strings; the callback gives their text */
    bones.count=result.bones;
    for (i=0;i<bones.count;++i) {
        const uint8_t *h=result.arrays[MODEL_BONE_NAMES].data+2*i; const char *t=text(context,h[0]|(unsigned)h[1]<<8);
        if (t) text_size+=strlen(t)+1;
    }
    if (bones.count && !(bones.names=calloc(bones.count,sizeof(*bones.names)))) goto fail;
    if (text_size && !(bones.storage=malloc(text_size))) goto fail;
    for (i=0;i<bones.count;++i) {
        const uint8_t *h=result.arrays[MODEL_BONE_NAMES].data+2*i;
        xmodel_bone_name_t *n=bones.names+i; const char *t;
        n->index=(uint16_t)(h[0]|h[1]<<8); t=text(context,n->index);
        if (t) { size_t l=strlen(t)+1; memcpy(bones.storage+pos,t,l); n->text=bones.storage+pos; pos+=l; }
        else ++bones.unresolved;
    }
    *skeleton=result; *names=bones; return 1;
fail:
    FreeXModelBoneNames(&bones); FreeXModelSkeleton(&result); return 0;
}

static int half_bounds(const uint8_t *p,float *mins,float *maxs,float *center)
{
    unsigned j;
    for (j=0;j<3;++j) {
        float mid=f32(p+4*j),half=f32(p+12+4*j);
        if (!isfinite(mid) || !isfinite(half) || half<0) return 0;
        mins[j]=mid-half; maxs[j]=mid+half;
        if (!isfinite(mins[j]) || !isfinite(maxs[j])) return 0;
        if (center) center[j]=mid;
    }
    return 1;
}

int ReadC1XModelGeometry(const uint8_t *record,const uint8_t *header_copy,mounted_model_geometry_t *out)
{
    mounted_model_geometry_t g={0};
    asset_t header={4,header_copy,0x168};
    const uint8_t *surfs=ptr(record+0x12c),*infos=ptr(record+0x138);
    unsigned i,j;
    if (!record || !header_copy || !out || !ReadXModelLods(&header,&g.lods)) return 0;
    /* collision surfaces (44 bytes) and their 48-byte triangles */
    g.collision.count=u32(record+0x130);
    if (g.collision.count && (!surfs || g.collision.count>65536)) return 0;
    if (g.collision.count && !(g.collision.surfaces=calloc(g.collision.count,sizeof(*g.collision.surfaces)))) return 0;
    for (i=0;i<g.collision.count;++i) {
        const uint8_t *raw=surfs+44*i,*tris=ptr(raw);
        xmodel_collision_surface_t *s=g.collision.surfaces+i;
        uint32_t bone=u32(raw+32),flags,k;
        if (bone>=record[4]) goto fail;
        s->boneIdx=(int)bone;
        flags=u32(raw+36); memcpy(&s->contents,&flags,4);
        flags=u32(raw+40); memcpy(&s->surfFlags,&flags,4);
        for (j=0;j<3;++j) {
            float mid=f32(raw+8+4*j),half=f32(raw+20+4*j);
            if (!isfinite(mid) || !isfinite(half) || half<0) goto fail;
            s->mins[j]=mid-half; s->maxs[j]=mid+half;
        }
        s->triangle_count=u32(raw+4);
        if (s->triangle_count && (!tris || s->triangle_count>(1u<<24))) goto fail;
        if (s->triangle_count) {
            if (!(s->triangles=calloc(s->triangle_count,sizeof(*s->triangles)))) goto fail;
            for (k=0;k<s->triangle_count;++k) for (j=0;j<12;++j) {
                float value=f32(tris+48*k+4*j);
                if (!isfinite(value)) goto fail;
                s->triangles[k][j]=value;
            }
        }
    }
    /* bounds and the 28-byte bone infos (ReadXModelBounds' fields) */
    g.bounds.radius=f32(record+0x13c); g.bounds.contents=u32(record+0x134); g.bounds.flags=record[0x12b];
    if (!isfinite(g.bounds.radius) || g.bounds.radius<0 || !half_bounds(record+0x140,g.bounds.mins,g.bounds.maxs,NULL)) goto fail;
    g.bounds.bone_count=record[4]; g.bounds.bone_info_reference=u32(header_copy+0x138);
    if (infos && g.bounds.bone_count) {
        if (!(g.bounds.bones=calloc(g.bounds.bone_count,sizeof(*g.bounds.bones)))) goto fail;
        for (i=0;i<g.bounds.bone_count;++i) {
            xmodel_bone_info_t *b=g.bounds.bones+i; const uint8_t *raw=infos+28*i;
            if (!half_bounds(raw,b->bounds[0],b->bounds[1],b->offset)) goto fail;
            b->radiusSquared=f32(raw+24);
            if (!isfinite(b->radiusSquared) || b->radiusSquared<0) goto fail;
        }
    }
    g.bounds.physics_preset_reference=u32(header_copy+0x160);
    g.bounds.physics_map_reference=u32(header_copy+0x164);
    *out=g; return 1;
fail:
    FreeXModelCollision(&g.collision); FreeXModelBounds(&g.bounds); return 0;
}

/* Field-by-field equality of the two conversions, for c1Readers 2: 0 equal, else a code naming the field. */
int CompareXModelConversions(const mounted_model_skeleton_t *a,const mounted_model_geometry_t *ga,
    const mounted_model_skeleton_t *b,const mounted_model_geometry_t *gb)
{
    unsigned i,j;
    if (!a->skeleton.name || !b->skeleton.name || strcmp(a->skeleton.name,b->skeleton.name)) return 1;
    if (a->skeleton.bones!=b->skeleton.bones || a->skeleton.roots!=b->skeleton.roots ||
        a->skeleton.surfaces!=b->skeleton.surfaces) return 2;
    for (i=0;i<6;++i) {
        const model_array_t *x=a->skeleton.arrays+i,*y=b->skeleton.arrays+i;
        if (x->count!=y->count || x->stride!=y->stride || x->reference!=y->reference) return 10+i;
        if (i!=MODEL_BONE_NAMES && x->count && (!x->data || !y->data || memcmp(x->data,y->data,(size_t)x->count*x->stride))) return 20+i;
    }
    if (a->bone_names.count!=b->bone_names.count || a->bone_names.unresolved!=b->bone_names.unresolved) return 3;
    for (i=0;i<a->bone_names.count;++i) {
        const char *x=a->bone_names.names[i].text,*y=b->bone_names.names[i].text;
        if (!x!=!y || (x && strcmp(x,y))) return 4;
    }
    if (!ga || !gb) return 0;
    if (memcmp(&ga->lods,&gb->lods,sizeof(ga->lods))) return 5;
    if (ga->collision.count!=gb->collision.count) return 6;
    for (i=0;i<ga->collision.count;++i) {
        const xmodel_collision_surface_t *x=ga->collision.surfaces+i,*y=gb->collision.surfaces+i;
        if (memcmp(x->mins,y->mins,sizeof(x->mins)) || memcmp(x->maxs,y->maxs,sizeof(x->maxs)) || x->boneIdx!=y->boneIdx ||
            x->contents!=y->contents || x->surfFlags!=y->surfFlags || x->triangle_count!=y->triangle_count) return 7;
        for (j=0;j<x->triangle_count;++j) if (memcmp(x->triangles[j],y->triangles[j],sizeof(x->triangles[j]))) return 8;
    }
    if (memcmp(ga->bounds.mins,gb->bounds.mins,sizeof(ga->bounds.mins)) || memcmp(ga->bounds.maxs,gb->bounds.maxs,sizeof(ga->bounds.maxs)) ||
        ga->bounds.radius!=gb->bounds.radius || ga->bounds.bone_count!=gb->bounds.bone_count ||
        ga->bounds.bone_info_reference!=gb->bounds.bone_info_reference || ga->bounds.contents!=gb->bounds.contents ||
        ga->bounds.flags!=gb->bounds.flags || ga->bounds.physics_preset_reference!=gb->bounds.physics_preset_reference ||
        ga->bounds.physics_map_reference!=gb->bounds.physics_map_reference) return 9;
    if (ga->bounds.bones && gb->bounds.bones &&
        memcmp(ga->bounds.bones,gb->bounds.bones,ga->bounds.bone_count*sizeof(*ga->bounds.bones))) return 30;
    if (!ga->bounds.bones!=!gb->bounds.bones) return 31;
    return 0;
}

/* XModelSurfs: the stream C1's loader reads (replay_surfaces order: name; surfaces; per surface the blend weights,
 * vertices, rigid lists with their trees, triangles), every present pointer inline, from the record C1 holds. */
typedef struct { uint8_t *data; size_t size,capacity; int failed; } surfs_out_t;
static size_t surfs_emit(surfs_out_t *o,const void *src,size_t n)
{
    size_t at=o->size;
    if(o->failed) return at;
    if(o->size+n>o->capacity) {
        size_t c=o->capacity ? o->capacity : 65536; uint8_t *g;
        while(c<o->size+n) c*=2;
        if(!(g=realloc(o->data,c))) { o->failed=1; return at; }
        o->data=g; o->capacity=c;
    }
    if(n) { if(src) memcpy(o->data+o->size,src,n); else memset(o->data+o->size,0,n); }
    o->size+=n; return at;
}
static void surfs_mark(surfs_out_t *o,size_t at,const void *present)
{ if(!o->failed) put32(o->data+at,present ? UINT32_MAX : 0); }
int SerializeC1ModelSurfaces(const uint8_t *record,asset_t *out)
{
    surfs_out_t o={0}; const char *name=(const char *)ptr(record); const uint8_t *surfs=ptr(record+4);
    unsigned count=record[8]|(unsigned)record[9]<<8,i,j; size_t h,s;
    if(!record || !out || !name) return 0;
    h=surfs_emit(&o,record,48); surfs_mark(&o,h,name); surfs_mark(&o,h+4,surfs);
    surfs_emit(&o,name,strlen(name)+1);
    if(surfs) {
        s=surfs_emit(&o,surfs,84*(size_t)count);
        for(i=0;i<count;++i) {
            const uint8_t *x=surfs+84*i,*blend=ptr(x+28),*verts=ptr(x+32),*rigid=ptr(x+44),*tris=ptr(x+16);
            unsigned blend_count=0,rigid_count=u32(x+40); size_t r;
            for(j=0;j<4;++j) blend_count+=(x[20+2*j]|(unsigned)x[21+2*j]<<8)*(2*j+1);
            surfs_mark(&o,s+84*i+28,blend); surfs_mark(&o,s+84*i+32,verts);
            surfs_mark(&o,s+84*i+44,rigid); surfs_mark(&o,s+84*i+16,tris);
            if(blend) surfs_emit(&o,blend,2*(size_t)blend_count);
            if(verts) surfs_emit(&o,verts,32*(size_t)(x[4]|(unsigned)x[5]<<8));
            if(rigid) {
                r=surfs_emit(&o,rigid,12*(size_t)rigid_count);
                for(j=0;j<rigid_count;++j) {
                    const uint8_t *tree=ptr(rigid+12*j+8);
                    surfs_mark(&o,r+12*j+8,tree);
                    if(tree) {
                        size_t t=surfs_emit(&o,tree,40); const uint8_t *nodes=ptr(tree+28),*leafs=ptr(tree+36);
                        surfs_mark(&o,t+28,nodes); surfs_mark(&o,t+36,leafs);
                        if(nodes) surfs_emit(&o,nodes,16*(size_t)u32(tree+24));
                        if(leafs) surfs_emit(&o,leafs,2*(size_t)u32(tree+32));
                    }
                }
            }
            if(tris) surfs_emit(&o,tris,6*(size_t)(x[6]|(unsigned)x[7]<<8));
        }
    }
    if(o.failed) { free(o.data); return 0; }
    *out=(asset_t){3,o.data,o.size}; return 1;
}

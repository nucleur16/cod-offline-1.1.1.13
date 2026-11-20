/* clipMap_t from a C1 record (feat/iw4-asset-layout, stage 2 step 3).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The retained collision readers (codol_collision_*.c) parse a clipMap the way C1's loader reads it (0x4bf410): an
 * inline stream whose shared arrays are reached through zone-pooled references. This writes that stream from the
 * clipMap record C1 holds (256-byte header, real pointers):
 *  - every array the header names follows inline, in loader order;
 *  - a pointer into a shared array (brush sides and nodes into the planes, brushes into the sides and edges,
 *    leaf-brush nodes into the leaf brushes, partitions into the borders) becomes base + offset, the relation the
 *    readers check; one that points elsewhere follows inline, as C1's -1 does;
 *  - planes stay outside (the readers take them as an array, as they take the graphics world's);
 *  - static models carry a synthetic reference the caller resolves by the model's name;
 *  - the MapEnts follow inline after the static nodes.
 * The same readers then build the server's collision from a C1 record. */
#include "c1_clipmap.h"
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static unsigned u16(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
static const uint8_t *ptr(const uint8_t *p) { const uint8_t *v; memcpy(&v,p,sizeof(v)); return v; }

typedef struct { uint8_t *data; size_t size,capacity; int failed; } out_t;
static size_t emit(out_t *o,const void *src,size_t n)
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
static void put(out_t *o,size_t at,uint32_t v) { if(!o->failed) memcpy(o->data+at,&v,4); }
static void put_string(out_t *o,const char *s) { emit(o,s ? s : "",s ? strlen(s)+1 : 1); }

/* Synthetic pooled bases: block-3-style references far apart, so base + offset never collide or reach -2. */
enum { REF_PLANES=0x30000001u, REF_SIDES=0x34000001u, REF_EDGES=0x38000001u, REF_LEAF_BRUSHES=0x3c000001u,
       REF_BORDERS=0x40000001u, REF_MODELS=0x44000001u };

/* p inside [base, base+count*stride): the pooled reference ref+offset, else 0 (follows inline). */
static uint32_t pooled(const uint8_t *p,const uint8_t *base,size_t count,size_t stride,uint32_t ref)
{
    if(!p || !base || p<base || (size_t)(p-base)>=count*stride) return 0;
    return ref+(uint32_t)(p-base);
}

void FreeC1ClipMapStream(c1_clipmap_stream_t *s)
{
    if(!s) return;
    free(s->data); free(s->model_names); memset(s,0,sizeof(*s));
}

int SerializeC1ClipMap(const uint8_t *r,c1_clipmap_stream_t *out)
{
    c1_clipmap_stream_t result={0}; out_t o={0};
    size_t h,i,j,k,n;
    const uint8_t *planes=ptr(r+12),*sides=ptr(r+36),*edges=ptr(r+44),*leaf_brushes=ptr(r+76),*borders=ptr(r+112);
    uint32_t plane_count=u32(r+8),side_count=u32(r+32),edge_count=u32(r+40),leaf_brush_count=u32(r+72),
             border_count=u32(r+108),brush_count=u16(r+140);
    const char *name=(const char *)ptr(r);
    if(!r || !out || !name) return 0;
    h=emit(&o,r,256);
    /* every pointer word the readers look at: inline (-1), the planes pooled, MapEnts inline */
    {
        static const unsigned words[]={20,28,36,44,52,60,68,76,84,92,100,104,112,120,128,136,144,148,152,164};
        for(i=0;i<sizeof(words)/sizeof(words[0]);++i) put(&o,h+words[i],ptr(r+words[i]) ? UINT32_MAX : 0);
        put(&o,h,UINT32_MAX); put(&o,h+12,plane_count ? REF_PLANES : 0); put(&o,h+156,UINT32_MAX);
    }
    put_string(&o,name);
    /* static models: a synthetic reference per entry, resolved by the model's name */
    n=u32(r+16);
    if(n && !(result.model_names=calloc(n,sizeof(*result.model_names)))) goto fail;
    result.model_count=(uint32_t)n;
    for(i=0;i<n;++i) {
        const uint8_t *m=ptr(r+20)+76*i,*model=ptr(m);
        size_t at=emit(&o,m,76);
        result.model_names[i]=model ? (const char *)ptr(model) : NULL;
        put(&o,at,REF_MODELS+4*(uint32_t)i);
    }
    /* materials: 12-byte records, inline names after the array */
    n=u32(r+24);
    {
        size_t at=emit(&o,ptr(r+28),12*n);
        for(i=0;i<n;++i) put(&o,at+12*i,UINT32_MAX);
        for(i=0;i<n;++i) put_string(&o,(const char *)ptr(ptr(r+28)+12*i));
    }
    /* brush sides and nodes: plane pointers pooled into the planes */
    {
        size_t at=emit(&o,sides,8*(size_t)side_count);
        for(i=0;i<side_count;++i) put(&o,at+8*i,pooled(ptr(sides+8*i),planes,plane_count,20,REF_PLANES));
    }
    emit(&o,edges,edge_count);
    n=u32(r+48);
    {
        size_t at=emit(&o,ptr(r+52),8*n);
        for(i=0;i<n;++i) put(&o,at+8*i,pooled(ptr(ptr(r+52)+8*i),planes,plane_count,20,REF_PLANES));
    }
    emit(&o,ptr(r+60),40*(size_t)u32(r+56));
    emit(&o,leaf_brushes,2*(size_t)leaf_brush_count);
    /* leaf-brush nodes: a leaf's brush list pooled into the leaf brushes, else inline after the nodes */
    n=u32(r+64);
    {
        const uint8_t *nodes=ptr(r+68); size_t at=emit(&o,nodes,20*n);
        for(i=0;i<n;++i) {
            int count=(int16_t)u16(nodes+20*i+2);
            if(count>0) {
                uint32_t ref=pooled(ptr(nodes+20*i+8),leaf_brushes,leaf_brush_count,2,REF_LEAF_BRUSHES);
                put(&o,at+20*i+8,ref ? ref : UINT32_MAX);
            }
        }
        for(i=0;i<n;++i) {
            int count=(int16_t)u16(nodes+20*i+2);
            if(count>0 && !pooled(ptr(nodes+20*i+8),leaf_brushes,leaf_brush_count,2,REF_LEAF_BRUSHES))
                emit(&o,ptr(nodes+20*i+8),2*(size_t)count);
        }
    }
    emit(&o,ptr(r+84),4*(size_t)u32(r+80));
    emit(&o,ptr(r+92),12*(size_t)u32(r+88));
    emit(&o,ptr(r+100),6*(size_t)u32(r+96));
    emit(&o,ptr(r+104),4*(((size_t)u32(r+96)*3+31)/32));
    emit(&o,borders,28*(size_t)border_count);
    /* partitions: their border pooled into the borders, else one inline border */
    n=u32(r+116);
    {
        const uint8_t *parts=ptr(r+120); size_t at=emit(&o,parts,12*n);
        for(i=0;i<n;++i) {
            const uint8_t *b=ptr(parts+12*i+8);
            uint32_t ref=pooled(b,borders,border_count,28,REF_BORDERS);
            put(&o,at+12*i+8,b ? (ref ? ref : UINT32_MAX) : 0);
        }
        for(i=0;i<n;++i) {
            const uint8_t *b=ptr(parts+12*i+8);
            if(b && !pooled(b,borders,border_count,28,REF_BORDERS)) emit(&o,b,28);
        }
    }
    emit(&o,ptr(r+128),32*(size_t)u32(r+124));
    emit(&o,ptr(r+136),68*(size_t)u32(r+132));
    /* brushes: sides pooled into the brush sides and edges into the brush edges, else inline */
    {
        const uint8_t *brushes=ptr(r+144); size_t at=emit(&o,brushes,36*(size_t)brush_count);
        for(i=0;i<brush_count;++i) {
            const uint8_t *b=brushes+36*i,*bs=ptr(b+4),*be=ptr(b+8);
            uint32_t sref=pooled(bs,sides,side_count,8,REF_SIDES),eref=pooled(be,edges,edge_count,1,REF_EDGES);
            put(&o,at+36*i+4,bs ? (sref ? sref : UINT32_MAX) : 0);
            put(&o,at+36*i+8,be ? (eref ? eref : UINT32_MAX) : 0);
            if(bs && !sref) {
                size_t s=emit(&o,bs,8*(size_t)u16(b));
                for(j=0;j<u16(b);++j) put(&o,s+8*j,pooled(ptr(bs+8*j),planes,plane_count,20,REF_PLANES));
            }
            if(be && !eref) {
                unsigned count=0;
                for(k=0;k<6;++k) count+=b[30+k];
                emit(&o,be,count);
            }
        }
    }
    emit(&o,ptr(r+148),24*(size_t)brush_count);
    emit(&o,ptr(r+152),4*(size_t)brush_count);
    emit(&o,ptr(r+164),28*(size_t)u32(r+160));
    /* MapEnts (C1 0x4be8c0): header, name, entity string, three trigger arrays, stages with names */
    {
        const uint8_t *me=ptr(r+156);
        if(!me) goto fail;
        {
            static const unsigned strides[3]={8,32,20};
            size_t at=emit(&o,me,44); const uint8_t *stages=ptr(me+36);
            put(&o,at,UINT32_MAX); put(&o,at+4,ptr(me+4) ? UINT32_MAX : 0);
            for(i=0;i<3;++i) put(&o,at+16+8*i,ptr(me+16+8*i) ? UINT32_MAX : 0);
            put(&o,at+36,stages ? UINT32_MAX : 0);
            put_string(&o,(const char *)ptr(me));
            if(ptr(me+4)) emit(&o,ptr(me+4),u32(me+8));
            for(i=0;i<3;++i) if(ptr(me+16+8*i)) emit(&o,ptr(me+16+8*i),(size_t)u32(me+12+8*i)*strides[i]);
            if(stages) {
                size_t s=emit(&o,stages,20*(size_t)me[40]);
                for(i=0;i<me[40];++i) put(&o,s+20*i,UINT32_MAX);
                for(i=0;i<me[40];++i) put_string(&o,(const char *)ptr(stages+20*i));
            }
        }
    }
    if(o.failed) goto fail;
    result.data=o.data; result.size=o.size; result.planes=planes; result.plane_count=plane_count;
    *out=result; return 1;
fail:
    free(o.data); free(result.model_names); return 0;
}

const char *C1ClipMapModelName(const c1_clipmap_stream_t *s,uint32_t reference)
{
    uint32_t i;
    if(!s || reference<REF_MODELS || (reference-REF_MODELS)%4) return NULL;
    i=(reference-REF_MODELS)/4;
    return i<s->model_count ? s->model_names[i] : NULL;
}

int ReadCollisionMapPlanes(const asset_t *collision,const uint8_t *planes,uint32_t plane_count,
    collision_name_resolver_t resolver,collision_model_resolver_t model_resolver,void *context,
    collision_map_t *out)
{
    collision_map_t map={0};
    collision_layout_t layout;
    if (!out || !ReadCollisionLayout(collision,&layout)) return 0;
    map.checksum=u32(collision->data+204);
    map.static_model_count=layout.sections[COLL_STATIC_MODELS].count;
    if (!ReadCollisionStaticModels(collision,model_resolver,context,&map.static_models) ||
        !ReadCollisionBrushes(collision,planes,(size_t)plane_count*20,&map.brushes) ||
        !ReadCollisionTerrain(collision,&map.terrain) ||
        !ReadCollisionLeaves(collision,&map.leaves) ||
        !ReadCollisionBrushTree(collision,&map.brush_tree) ||
        !ReadCollisionMaterials(collision,resolver,context,&map.materials) ||
        !ReadCollisionMapEntities(collision,&map.entities)) {
        FreeCollisionMap(&map); return 0;
    }
    FreeCollisionMap(out); *out=map; return 1;
}

#define SAME(a,b,n) ((size_t)(n)==0 || ((a) && (b) && !memcmp((a),(b),(n))))
static long index_of(const void *p,const void *base,size_t stride)
{ return p ? (long)(((const uint8_t *)p-(const uint8_t *)base)/(long)stride) : -1; }
int CompareCollisionMaps(const collision_map_t *a,const collision_map_t *b)
{
    uint32_t i;
    const collision_brushes_t *x=&a->brushes,*y=&b->brushes;
    const collision_terrain_t *tx=&a->terrain,*ty=&b->terrain;
    if (a->checksum!=b->checksum || a->static_model_count!=b->static_model_count) return 1;
    /* materials */
    if (a->materials.count!=b->materials.count || a->materials.unresolved!=b->materials.unresolved ||
        !SAME(a->materials.materials,b->materials.materials,a->materials.count*sizeof(*a->materials.materials))) return 2;
    /* brushes: planes, sides, edges, brushes and nodes, pointers by index */
    if (x->plane_count!=y->plane_count || x->side_count!=y->side_count || x->edge_count!=y->edge_count ||
        x->brush_count!=y->brush_count || x->node_count!=y->node_count) return 3;
    if (!SAME(x->planes,y->planes,x->plane_count*sizeof(*x->planes)) || !SAME(x->edges,y->edges,x->edge_count)) return 4;
    for (i=0;i<x->side_count;++i) {
        const collision_side_t *s=x->sides+i,*t=y->sides+i;
        if (index_of(s->plane,x->planes,sizeof(*x->planes))!=index_of(t->plane,y->planes,sizeof(*y->planes)) ||
            s->materialNum!=t->materialNum || s->firstAdjacentSideOffset!=t->firstAdjacentSideOffset ||
            s->edgeCount!=t->edgeCount) return 5;
    }
    for (i=0;i<x->brush_count;++i) {
        const collision_brush_t *s=x->brushes+i,*t=y->brushes+i;
        if (memcmp(s->mins,t->mins,sizeof(s->mins)) || memcmp(s->maxs,t->maxs,sizeof(s->maxs)) || s->contents!=t->contents ||
            s->numsides!=t->numsides || index_of(s->sides,x->sides,sizeof(*x->sides))!=index_of(t->sides,y->sides,sizeof(*y->sides)) ||
            memcmp(s->axialMaterialNum,t->axialMaterialNum,sizeof(s->axialMaterialNum)) ||
            index_of(s->baseAdjacentSide,x->edges,1)!=index_of(t->baseAdjacentSide,y->edges,1) ||
            memcmp(s->firstAdjacentSideOffsets,t->firstAdjacentSideOffsets,sizeof(s->firstAdjacentSideOffsets)) ||
            memcmp(s->edgeCount,t->edgeCount,sizeof(s->edgeCount))) return 6;
    }
    for (i=0;i<x->node_count;++i)
        if (index_of(x->nodes[i].plane,x->planes,sizeof(*x->planes))!=index_of(y->nodes[i].plane,y->planes,sizeof(*y->planes)) ||
            memcmp(x->nodes[i].children,y->nodes[i].children,sizeof(x->nodes[i].children))) return 7;
    /* terrain */
    if (tx->vertex_count!=ty->vertex_count || tx->triangle_count!=ty->triangle_count || tx->walkable_words!=ty->walkable_words ||
        tx->border_count!=ty->border_count || tx->partition_count!=ty->partition_count || tx->aabb_count!=ty->aabb_count) return 8;
    if (!SAME(tx->vertices,ty->vertices,tx->vertex_count*sizeof(*tx->vertices)) ||
        !SAME(tx->triangles,ty->triangles,tx->triangle_count*sizeof(*tx->triangles)) ||
        !SAME(tx->walkable,ty->walkable,tx->walkable_words*sizeof(*tx->walkable)) ||
        !SAME(tx->borders,ty->borders,tx->border_count*sizeof(*tx->borders)) ||
        !SAME(tx->aabbs,ty->aabbs,tx->aabb_count*sizeof(*tx->aabbs))) return 9;
    for (i=0;i<tx->partition_count;++i) {
        const collision_partition_t *s=tx->partitions+i,*t=ty->partitions+i;
        if (s->triCount!=t->triCount || s->borderCount!=t->borderCount || s->vertBlock!=t->vertBlock || s->firstTri!=t->firstTri ||
            index_of(s->borders,tx->borders,sizeof(*tx->borders))!=index_of(t->borders,ty->borders,sizeof(*ty->borders))) return 10;
    }
    /* leaves, submodels, leaf surfaces */
    if (a->leaves.leaf_count!=b->leaves.leaf_count || a->leaves.submodel_count!=b->leaves.submodel_count ||
        a->leaves.surface_count!=b->leaves.surface_count ||
        !SAME(a->leaves.leaves,b->leaves.leaves,a->leaves.leaf_count*sizeof(*a->leaves.leaves)) ||
        !SAME(a->leaves.submodels,b->leaves.submodels,a->leaves.submodel_count*sizeof(*a->leaves.submodels)) ||
        !SAME(a->leaves.surfaces,b->leaves.surfaces,a->leaves.surface_count*sizeof(*a->leaves.surfaces))) return 11;
    /* brush tree: each leaf brush list by index into the tree indices */
    if (a->brush_tree.node_count!=b->brush_tree.node_count || a->brush_tree.index_count!=b->brush_tree.index_count ||
        a->brush_tree.pooled_count!=b->brush_tree.pooled_count ||
        !SAME(a->brush_tree.indices,b->brush_tree.indices,a->brush_tree.index_count*sizeof(*a->brush_tree.indices))) return 12;
    for (i=0;i<a->brush_tree.node_count;++i) {
        const collision_brush_node_t *s=a->brush_tree.nodes+i,*t=b->brush_tree.nodes+i;
        if (s->axis!=t->axis || s->leafBrushCount!=t->leafBrushCount || s->contents!=t->contents) return 13;
        if (s->leafBrushCount>0) {
            if (index_of(s->data.leaf.brushes,a->brush_tree.indices,2)!=index_of(t->data.leaf.brushes,b->brush_tree.indices,2)) return 14;
        } else if (memcmp(&s->data.children,&t->data.children,sizeof(s->data.children))) return 15;
    }
    /* entities */
    if (a->entities.text_size!=b->entities.text_size) return 16;
    if (!SAME(a->entities.text,b->entities.text,a->entities.text_size)) return 21;
    /* a pooled MapEnts name is not resolved by the serialized reader (name NULL); compare names both carry */
    if (a->entities.name && b->entities.name && strcmp(a->entities.name,b->entities.name)) return 22;
    if (a->entities.model_count!=b->entities.model_count || a->entities.hull_count!=b->entities.hull_count ||
        a->entities.slab_count!=b->entities.slab_count || a->entities.stage_count!=b->entities.stage_count) return 23;
    if (!SAME(a->entities.models,b->entities.models,a->entities.model_count*8) ||
        !SAME(a->entities.hulls,b->entities.hulls,a->entities.hull_count*32) ||
        !SAME(a->entities.slabs,b->entities.slabs,a->entities.slab_count*20)) return 17;
    for (i=0;i<a->entities.stage_count;++i) {
        const map_stage_t *s=a->entities.stages+i,*t=b->entities.stages+i;
        if (!s->name!=!t->name || (s->name && strcmp(s->name,t->name)) || memcmp(s->origin,t->origin,sizeof(s->origin)) ||
            s->trigger_index!=t->trigger_index || s->sun_index!=t->sun_index) return 18;
    }
    /* static models: transforms and the bound models */
    if (a->static_models.count!=b->static_models.count || a->static_models.unresolved!=b->static_models.unresolved) return 19;
    for (i=0;i<a->static_models.count;++i) {
        const collision_static_model_t *s=a->static_models.models+i,*t=b->static_models.models+i;
        if (s->writable!=t->writable || s->xmodel!=t->xmodel || memcmp(s->origin,t->origin,sizeof(s->origin)) ||
            memcmp(s->invScaledAxis,t->invScaledAxis,sizeof(s->invScaledAxis)) ||
            memcmp(s->absmin,t->absmin,sizeof(s->absmin)) || memcmp(s->absmax,t->absmax,sizeof(s->absmax))) return 20;
    }
    return 0;
}

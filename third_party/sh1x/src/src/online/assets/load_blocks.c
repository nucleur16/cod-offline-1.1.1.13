/* C1 Push/PopStreamPos 0x4cee10/0x4cee70, InsertPointer 0x4ceec0.
 * Block 0 rewinds on pop; block 2 loader branches zero-fill runtime memory. */
#include "load_blocks.h"
#include "zone_image.h"
#include <stdlib.h>
#include <string.h>
void FreeLoadBlocks(load_blocks_t *s)
{
    unsigned i;
    if (!s) return;
    for (i=0;i<8;++i) BlockFree(s->data[i]);
    free(s->asset_aliases);
    free(s->dynamic_records);
    free(s->sound_records);
    free(s->glass_records);
    if (s->image) { free(s->image->chunks); free(s->image->shadow); free(s->image); }
    if (s->c1) {
        free(s->c1->fixups); free(s->c1->chunks); free(s->c1->arena); free(s->c1->headers); free(s->c1->placements);
        free(s->c1);
    }
    memset(s,0,sizeof(*s));
}
int InitLoadBlocks(load_blocks_t *out,const uint8_t *source,size_t size,
    const uint32_t capacities[8],unsigned active)
{
    load_blocks_t s={0};
    size_t total=0;
    unsigned i;
    if (!out || !capacities || (!source && size) || size>ASSET_LIMIT || active>=8) return 0;
    for (i=0;i<8;++i) {
        if (capacities[i]>0x10000000u || capacities[i]>ASSET_LIMIT-total) return 0;
        total+=capacities[i];
    }
    for (i=0;i<8;++i) {
        s.capacity[i]=capacities[i];
        if (capacities[i] && !(s.data[i]=BlockAllocate(capacities[i]))) {
            FreeLoadBlocks(&s); return 0;
        }
    }
    s.source=source; s.source_size=size; s.active=active; *out=s; return 1;
}
int PushLoadBlock(load_blocks_t *s,unsigned block)
{
    if (!s || block>=8 || s->active>=8 || s->depth>=LOAD_STACK) return 0;
    s->stack[s->depth].block=s->active;
    s->stack[s->depth].entered=s->position[block];
    ++s->depth; s->active=block; return 1;
}
int PopLoadBlock(load_blocks_t *s)
{
    if (!s || !s->depth || s->depth>LOAD_STACK || s->active>=8) return 0;
    --s->depth;
    if (!s->active) s->position[0]=s->stack[s->depth].entered;
    s->active=s->stack[s->depth].block; return 1;
}
static void c1_chunk(load_blocks_t *,const uint8_t *,uint32_t,uint32_t);
static void image_chunk(load_blocks_t *s,unsigned block,size_t p,size_t length)
{
    image_map_t *m=s->image;
    uint32_t location;
    if (!m || m->failed || !length) return;
    if (m->skip) { location=0; m->lost+=length; }
    else if (block==0) {
        size_t need=m->shadow_size+length,n=m->shadow_capacity ? m->shadow_capacity : 65536;
        uint8_t *grown;
        while (n<need) n*=2;
        if (n!=m->shadow_capacity) {
            if (!(grown=realloc(m->shadow,n))) { m->failed=1; return; }
            m->shadow=grown; m->shadow_capacity=n;
        }
        if (m->shadow_size>=0x0fffffffu) { m->failed=1; return; }
        memcpy(m->shadow+m->shadow_size,s->source+s->source_position,length);
        location=(14u<<28)+(uint32_t)m->shadow_size+1; m->shadow_size+=length;
    } else location=((uint32_t)block<<28)+(uint32_t)p+1;
    if (m->count==m->capacity) {
        size_t n=m->capacity ? m->capacity*2 : 4096;
        image_chunk_t *grown=realloc(m->chunks,n*sizeof(*grown));
        if (!grown) { m->failed=1; return; }
        m->chunks=grown; m->capacity=n;
    }
    /* consecutive source and destination merge */
    if (m->count) {
        image_chunk_t *last=m->chunks+m->count-1;
        if (last->source+last->length==(uint32_t)s->source_position &&
            (location ? last->location+last->length==location : !last->location)) {
            last->length+=(uint32_t)length; return;
        }
    }
    m->chunks[m->count++]=(image_chunk_t){(uint32_t)s->source_position,(uint32_t)length,location};
}
static int reserve(load_blocks_t *s,unsigned block,size_t length,unsigned alignment,
    int read,int apply,uint32_t *reference)
{
    size_t p;
    if (!s || !reference || block>=8 || !alignment || alignment>128 || (alignment&(alignment-1))) return 0;
    if (s->position[block]>s->capacity[block] || s->capacity[block]>0x10000000u) return 0;
    p=(s->position[block]+alignment-1)&~(size_t)(alignment-1);
    if (p>=0x10000000u || p>s->capacity[block] || length>s->capacity[block]-p ||
        (length && !s->data[block])) return 0;
    if (read && block!=2 && (s->source_position>s->source_size ||
        length>s->source_size-s->source_position || (length && !s->source))) return 0;
    if (length && apply) {
        if (read && block!=2) memcpy(s->data[block]+p,s->source+s->source_position,length);
        else memset(s->data[block]+p,0,length);
    }
    if (apply && read && block!=2 && s->image) image_chunk(s,block,p,length);
    if (apply && s->c1) {
        s->last_reference=((uint32_t)block<<28)+(uint32_t)p+1; s->last_length=(uint32_t)length;
        if (read && block!=2) c1_chunk(s,s->source+s->source_position,(uint32_t)length,s->last_reference);
    }
    if (read && block!=2) s->source_position+=length;
    s->position[block]=p+length;
    if (s->written[block]<p+length) s->written[block]=p+length;
    *reference=((uint32_t)block<<28)+(uint32_t)p+1; return 1;
}
int ReadLoadBlock(load_blocks_t *s,size_t length,unsigned alignment,uint32_t *reference)
{ return s && reserve(s,s->active,length,alignment,1,1,reference); }
int InsertLoadPointer(load_blocks_t *s,uint32_t *reference)
{ return reserve(s,3,4,4,0,1,reference); }
void LoadBlockViews(const load_blocks_t *s,model_block_t views[8])
{
    unsigned i;
    if (!s || !views) return;
    for (i=0;i<8;++i) views[i]=(model_block_t){s->data[i],s->written[i]};
}
int ReplayXModelSkeleton(load_blocks_t *stream,xmodel_skeleton_t *out)
{
    static const unsigned alignments[6]={2,1,2,4,1,4};
    load_blocks_t plan;
    xmodel_skeleton_t model={0};
    asset_t asset;
    unsigned i,pass;
    uint32_t name_reference,array_references[6],ignored;
    size_t name_length;
    if (!stream || !out || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    asset=(asset_t){4,stream->source+stream->source_position,stream->source_size-stream->source_position};
    if (!ReadXModelSkeleton(&asset,&model)) return 0;
    name_length=model.name ? strlen(model.name)+1 : 0;
    /* First pass validates all cursor operations without writing block memory.
     * The second pass applies precisely the validated sequence. */
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        name_reference=model.name_reference;
        if (!reserve(&plan,0,360,4,1,pass,&ignored) || !PushLoadBlock(&plan,3)) goto fail;
        if (model.name_reference==UINT32_MAX &&
            !reserve(&plan,3,name_length,1,1,pass,&name_reference)) goto fail;
        for (i=0;i<6;++i) {
            model_array_t *a=model.arrays+i;
            array_references[i]=a->reference;
            if (a->reference==UINT32_MAX &&
                !reserve(&plan,3,(size_t)a->count*a->stride,alignments[i],1,pass,array_references+i)) goto fail;
        }
    }
    model.name_reference=name_reference;
    for (i=0;i<6;++i) model.arrays[i].reference=array_references[i];
    *stream=plan; *out=model; return 1;
fail:
    FreeXModelSkeleton(&model); return 0;
}
static uint32_t read32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static const uint8_t *replay_bytes(load_blocks_t *s,uint32_t count,unsigned stride,
    unsigned alignment,int apply)
{
    const uint8_t *start;
    uint32_t ignored;
    if (s->source_position>s->source_size || count>(s->source_size-s->source_position)/stride) return NULL;
    start=s->source+s->source_position;
    if (!reserve(s,s->active,(size_t)count*stride,alignment,1,apply,&ignored)) return NULL;
    return start;
}
static int replay_string(load_blocks_t *s,int apply)
{
    const uint8_t *end;
    uint32_t ignored;
    if (s->source_position>=s->source_size ||
        !(end=memchr(s->source+s->source_position,0,s->source_size-s->source_position))) return 0;
    return reserve(s,s->active,(size_t)(end-(s->source+s->source_position))+1,1,1,apply,&ignored);
}
static int replay_asset_slot(load_blocks_t *,uint32_t,uint32_t,uint32_t,int,
    int (*)(load_blocks_t *,int,uint32_t *),int);
static int replay_map_entities(load_blocks_t *,int,uint32_t *);
/* ---- C1 pointer fix-ups (feat/iw4-asset-layout) ------------------------------------------------------------
 * C1's loader relocates each pointer field as it reads it: -1 (inline) points at the data the loader places next,
 * -2 also stores that pointer in an inserted block-3 word, any other nonzero value is an encoded address (data) or,
 * for asset fields, the address of a word that holds the asset pointer (DB_ConvertOffsetToAlias). The replay
 * records each such field as a fixup_t, keyed by where the field lies in loader memory: every read
 * reservation copies source bytes verbatim, so a field read at source address X lies in the reservation whose
 * source range contains X (the chunk list of the current top-level asset). Asset headers are copied into the
 * arena (block 8) when their loader finishes, as C1 copies them into its pools, and the fix-ups inside the
 * header are moved with it. Nothing here writes a pointer into loader memory: readers that parse the replayed
 * blocks keep seeing serialized references. */
static int c1_grow(void **data,size_t *capacity,size_t need,size_t element)
{
    size_t n;
    void *p;
    if (need<=*capacity) return 1;
    n=*capacity ? *capacity : 1024;
    while (n<need) n*=2;
    if (!(p=realloc(*data,n*element))) return 0;
    *data=p; *capacity=n; return 1;
}
static void c1_chunk(load_blocks_t *s,const uint8_t *source,uint32_t length,uint32_t reference)
{
    c1_state_t *c=s->c1;
    if (!c || c->failed || !length) return;
    if (!c1_grow((void **)&c->chunks,&c->chunk_capacity,c->chunk_count+1,sizeof(*c->chunks))) { c->failed=1; return; }
    c->chunks[c->chunk_count++]=(replay_chunk_t){source,length,reference};
}
/* The loader-memory address of the 4-byte field read at source address field; 0 when it is not in a read
 * reservation of the current asset (a field the loader read into a register, never stored). */
static uint32_t c1_slot(const load_blocks_t *s,const uint8_t *field)
{
    const c1_state_t *c=s->c1;
    size_t i;
    if (!c) return 0;
    for (i=c->chunk_count;i-->0;) {
        const replay_chunk_t *k=c->chunks+i;
        if (field>=k->source && field<k->source+k->length) return k->reference+(uint32_t)(field-k->source);
    }
    return 0;
}
static void c1_fix(load_blocks_t *s,int apply,uint32_t slot,uint32_t target,uint32_t size,uint32_t kind)
{
    c1_state_t *c=s->c1;
    if (!apply || !c || c->failed || !slot || !target) return;
    if (!c1_grow((void **)&c->fixups,&c->fixup_capacity,c->fixup_count+1,sizeof(*c->fixups))) { c->failed=1; return; }
    c->fixups[c->fixup_count++]=(fixup_t){slot,target,size,kind};
}
/* A struct C1 loads in pieces (a fixed part, then its inline tail): the field at source address field points at
 * the whole, from start to the end of the last placement. */
static void c1_link_extent(load_blocks_t *s,int apply,uint32_t slot,uint32_t start)
{
    if (apply && s->c1 && start && s->last_reference>=start)
        c1_fix(s,apply,slot,start,s->last_reference+s->last_length-start,FIXUP_DATA);
}
/* The field at source address field points at the data the loader just placed (s->last_*). */
static void c1_link(load_blocks_t *s,int apply,const uint8_t *field)
{
    if (apply && s->c1) c1_fix(s,apply,c1_slot(s,field),s->last_reference,s->last_length,FIXUP_DATA);
}
/* A pooled (encoded) data reference held by the field. */
static void c1_pooled(load_blocks_t *s,int apply,const uint8_t *field)
{
    uint32_t v=read32(field);
    if (apply && s->c1 && v && v<UINT32_MAX-1) c1_fix(s,apply,c1_slot(s,field),v,0,FIXUP_DATA);
}
/* A range C1 writes itself while it loads (kind FIXUP_RUNTIME) or converts from the zone's script-string
 * indices to its own handles (FIXUP_SCRIPT_STRINGS), starting at source address field. */
static void c1_note(load_blocks_t *s,int apply,const uint8_t *field,uint32_t size,uint32_t kind)
{
    uint32_t slot;
    if (!apply || !s->c1 || !size || !(slot=c1_slot(s,field))) return;
    c1_fix(s,apply,slot,slot,size,kind);
}
/* The same for data just placed (s->last_reference). */
static void c1_note_last(load_blocks_t *s,int apply,uint32_t size,uint32_t kind)
{
    if (apply && s->c1 && size) c1_fix(s,apply,s->last_reference,s->last_reference,size,kind);
}
/* An XString field: -1 inline string, else a pooled string or null. */
static int string_field(load_blocks_t *s,const uint8_t *field,int apply)
{
    if (read32(field)==UINT32_MAX) {
        if (!replay_string(s,apply)) return 0;
        c1_link(s,apply,field);
    } else c1_pooled(s,apply,field);
    return 1;
}
/* A name C1 replaces with the header of the asset of that type it finds by it (FxEffectDefRef). */
static int asset_name_field(load_blocks_t *s,const uint8_t *field,unsigned type,int apply)
{
    uint32_t v=read32(field),slot=apply && s->c1 ? c1_slot(s,field) : 0;
    if (v==UINT32_MAX) {
        if (!replay_string(s,apply)) return 0;
        c1_fix(s,apply,slot,s->last_reference,s->last_length,FIXUP_ASSET_NAME|type<<8);
    } else if (v) c1_fix(s,apply,slot,v,0,FIXUP_ASSET_NAME|type<<8);
    return 1;
}
/* An array field read only when it is -1, as C1's Load_*Array do; pooled values are relocated. */
static int array_field(load_blocks_t *s,const uint8_t *field,uint32_t count,unsigned stride,
    unsigned alignment,int apply,const uint8_t **out)
{
    const uint8_t *p=NULL;
    if (read32(field)==UINT32_MAX) {
        if (!(p=replay_bytes(s,count,stride,alignment,apply))) return 0;
        c1_link(s,apply,field);
    } else c1_pooled(s,apply,field);
    if (out) *out=p;
    return 1;
}
/* An array field whose shipped value is 0 or -1 and that the replay reads whenever it is nonzero. */
static int present_field(load_blocks_t *s,const uint8_t *field,uint32_t count,unsigned stride,
    unsigned alignment,int apply,const uint8_t **out)
{
    const uint8_t *p=NULL;
    if (read32(field)) {
        if (!(p=replay_bytes(s,count,stride,alignment,apply))) return 0;
        c1_link(s,apply,field);
    }
    if (out) *out=p;
    return 1;
}
static int replay_physics(load_blocks_t *s,unsigned type,int apply,uint32_t *reference)
{
    const uint8_t *h,*geoms;
    unsigned i,count;
    h=s->source+s->source_position;
    if (!reserve(s,0,type ? 72 : 68,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (!type) {
        c1_note(s,apply,h+4,4,FIXUP_RUNTIME); /* PhysPreset type: C1 assigns it at load */
        if (!string_field(s,h+28,apply)) return 0;
    } else if (read32(h+8)) {
        count=read32(h+4);
        if (!(geoms=replay_bytes(s,count,68,4,apply))) return 0;
        c1_link(s,apply,h+8);
        for (i=0;i<count;++i) if (read32(geoms+68*i)) {
            const uint8_t *brush,*sides;
            unsigned j,n;
            if (!(brush=replay_bytes(s,1,68,4,apply))) return 0;
            c1_link(s,apply,geoms+68*i);
            n=brush[24]|(unsigned)brush[25]<<8;
            if (read32(brush+28)) {
                if (!(sides=replay_bytes(s,n,8,4,apply))) return 0;
                c1_link(s,apply,brush+28);
                for (j=0;j<n;++j)
                    if (!array_field(s,sides+8*j,1,20,4,apply,NULL)) return 0;
            }
            if (!present_field(s,brush+32,read32(brush+60),1,1,apply,NULL)) return 0;
            if (!array_field(s,brush+64,n,20,4,apply,NULL)) return 0;
        }
    }
    return PopLoadBlock(s);
}
int ReplayPhysicsAsset(load_blocks_t *stream,unsigned type,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || type>1 || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_physics(&plan,type,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
static unsigned read16(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
static int replay_surfaces(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*surfaces;
    unsigned i,count;
    if (!reserve(s,0,48,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!read32(h+4)) { /* lod surfaces C1 streams in (stream_model.c): pointer, count and part bits */
        c1_note(s,apply,h+4,6,FIXUP_RUNTIME); c1_note(s,apply,h+0x2c,4,FIXUP_RUNTIME);
    }
    if (!string_field(s,h,apply)) return 0;
    if (read32(h+4)) {
        count=read16(h+8);
        if (!(surfaces=replay_bytes(s,count,84,4,apply))) return 0;
        c1_link(s,apply,h+4);
        for (i=0;i<count;++i) {
            const uint8_t *surface=surfaces+84*i,*rigid;
            unsigned j,n,blend=0;
            c1_note(s,apply,surface+3,1,FIXUP_RUNTIME); /* XSurface zone handle: the loading zone's index */
            if (read32(surface+28)==UINT32_MAX) {
                for (j=0;j<4;++j) {
                    n=read16(surface+20+2*j); if (n>32767) return 0;
                    blend+=n*(2*j+1);
                }
                if (!replay_bytes(s,blend,2,2,apply)) return 0;
                c1_link(s,apply,surface+28);
            }
            if (!PushLoadBlock(s,6)) return 0;
            if (!array_field(s,surface+32,read16(surface+4),32,16,apply,NULL)) return 0;
            if (!PopLoadBlock(s)) return 0;
            if (read32(surface+44)==UINT32_MAX) {
                n=read32(surface+40);
                if (!(rigid=replay_bytes(s,n,12,4,apply))) return 0;
                c1_link(s,apply,surface+44);
                for (j=0;j<n;++j) if (read32(rigid+12*j+8)==UINT32_MAX) {
                    const uint8_t *tree=replay_bytes(s,1,40,4,apply);
                    if (!tree) return 0;
                    c1_link(s,apply,rigid+12*j+8);
                    if (!present_field(s,tree+28,read32(tree+24),16,16,apply,NULL)) return 0;
                    if (!present_field(s,tree+36,read32(tree+32),2,2,apply,NULL)) return 0;
                }
            }
            if (!PushLoadBlock(s,7)) return 0;
            if (!array_field(s,surface+16,read16(surface+6),6,16,apply,NULL)) return 0;
            if (!PopLoadBlock(s)) return 0;
        }
    }
    return PopLoadBlock(s);
}
int ReplayModelSurfaces(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_surfaces(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
/* Image 0x4b92d0/load-def 0x4b9250; water 0x4b95a0. */
static int replay_image(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*definition;
    if (!reserve(s,0,36,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h+32,apply)) return 0;
    if (read32(h)==UINT32_MAX) {
        if (!(definition=replay_bytes(s,1,16,4,apply))) return 0;
        c1_link(s,apply,h);
        if (!replay_bytes(s,read32(definition+12),1,1,apply)) return 0;
    }
    return PopLoadBlock(s);
}
static int replay_water(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    uint32_t m,n,image;
    uint64_t cells;
    if (!reserve(s,3,68,4,1,apply,reference)) return 0;
    m=read32(h+12); n=read32(h+16); cells=(uint64_t)m*n;
    if (m>INT32_MAX || n>INT32_MAX || cells>UINT32_MAX) return 0;
    if (!present_field(s,h+4,(uint32_t)cells,8,4,apply,NULL)) return 0;
    if (!present_field(s,h+8,(uint32_t)cells,4,4,apply,NULL)) return 0;
    image=read32(h+64);
    return replay_asset_slot(s,image,0,apply && s->c1 ? c1_slot(s,h+64) : 0,apply,replay_image,10);
}
static int replay_texture_dependency(load_blocks_t *stream,int water,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=(water ? 3u : 0u) || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!(water ? replay_water(&plan,pass,&result) : replay_image(&plan,pass,&result))) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
int ReplayImage(load_blocks_t *s,uint32_t *reference)
{ return replay_texture_dependency(s,0,reference); }
int ReplayWater(load_blocks_t *s,uint32_t *reference)
{ return replay_texture_dependency(s,1,reference); }
static int replay_shader(load_blocks_t *s,int declaration,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    if (!reserve(s,0,declaration ? 100 : 16,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (!declaration && !present_field(s,h+8,read16(h+12),4,4,apply,NULL)) return 0;
    return PopLoadBlock(s);
}
static int replay_shader_asset(load_blocks_t *s,int apply,uint32_t *reference)
{ return replay_shader(s,0,apply,reference); }
static int replay_declaration_asset(load_blocks_t *s,int apply,uint32_t *reference)
{ return replay_shader(s,1,apply,reference); }
/* type 8 vertex declaration, 7 vertex shader, 6 pixel shader. */
static int replay_shader_reference(load_blocks_t *s,const uint8_t *field,int type,int apply)
{
    return replay_asset_slot(s,read32(field),0,apply && s->c1 ? c1_slot(s,field) : 0,apply,
        type==8 ? replay_declaration_asset : replay_shader_asset,type);
}
/* C1 0x4ba130/0x4ba0c0/0x4b9ef0: technique names follow passes. */
static int replay_technique_set(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    unsigned i,j,k;
    if (!reserve(s,0,204,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    c1_note(s,apply,h+8,4,FIXUP_RUNTIME); /* remapped technique set */
    if (!string_field(s,h,apply)) return 0;
    for (i=0;i<48;++i) if (read32(h+12+4*i)!=UINT32_MAX) c1_pooled(s,apply,h+12+4*i);
    for (i=0;i<48;++i) if (read32(h+12+4*i)==UINT32_MAX) {
        const uint8_t *technique,*passes;
        unsigned count;
        uint32_t slot,start;
        if (!(technique=replay_bytes(s,1,8,4,apply))) return 0;
        slot=apply && s->c1 ? c1_slot(s,h+12+4*i) : 0; start=s->last_reference;
        count=read16(technique+6);
        if (!(passes=replay_bytes(s,count,20,1,apply))) return 0;
        c1_link_extent(s,apply,slot,start); /* MaterialTechnique: header and its inline passes */
        for (j=0;j<count;++j) {
            const uint8_t *pass=passes+20*j,*args;
            unsigned n;
            if (!replay_shader_reference(s,pass,8,apply) ||
                !replay_shader_reference(s,pass+4,7,apply) ||
                !replay_shader_reference(s,pass+8,6,apply)) return 0;
            if (read32(pass+16)) {
                n=pass[12]+pass[13]+pass[14];
                if (!(args=replay_bytes(s,n,8,4,apply))) return 0;
                c1_link(s,apply,pass+16);
                for (k=0;k<n;++k)
                    if ((read16(args+8*k)==1 || read16(args+8*k)==7) &&
                        !array_field(s,args+8*k+4,1,16,4,apply,NULL)) return 0;
            }
        }
        if (!string_field(s,technique,apply)) return 0;
    }
    return PopLoadBlock(s);
}
int ReplayTechniqueSet(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_technique_set(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
static int replay_material(load_blocks_t *,int,uint32_t *);
static int replay_model(load_blocks_t *,int,uint32_t *);
static int replay_effect(load_blocks_t *,int,uint32_t *);
static int replay_preset_asset(load_blocks_t *,int,uint32_t *);
static int replay_physics_map_asset(load_blocks_t *,int,uint32_t *);
static int replay_tracer(load_blocks_t *,int,uint32_t *);
static int replay_sound_alias(load_blocks_t *,int,uint32_t *);
static int replay_sound_curve(load_blocks_t *,int,uint32_t *);
static int replay_loaded_sound(load_blocks_t *,int,uint32_t *);
static int record_asset(load_blocks_t *s,unsigned type,uint32_t ref,size_t start,int apply)
{
    if (!s->collect_asset_aliases) return 1;
    if (apply) {
        if (s->asset_alias_count>=s->asset_alias_capacity) return 0;
        s->asset_aliases[s->asset_alias_count]=(asset_alias_t){
            ref,{type,s->source+start,s->source_position-start}};
    }
    ++s->asset_alias_count; return 1;
}
static int replay_weapon(load_blocks_t *,int,uint32_t *);
static int c1_loader_type(int (*loader)(load_blocks_t *,int,uint32_t *))
{
    return loader==replay_material ? 5 : loader==replay_model ? 4 : loader==replay_surfaces ? 3 :
        loader==replay_preset_asset ? 0 : loader==replay_physics_map_asset ? 1 : loader==replay_effect ? 30 : loader==replay_tracer ? 40 :
        loader==replay_sound_alias ? 11 : loader==replay_sound_curve ? 12 : loader==replay_loaded_sound ? 13 : loader==replay_weapon ? 28 :
        loader==replay_image ? 10 : loader==replay_technique_set ? 9 : -1;
}
/* Copy the header the loader just placed in block 0 into the arena (block 8), as C1 copies it into its pool,
 * and move the fix-ups recorded inside it (from index first on). Returns the header's final reference. */
static uint32_t c1_copy_header(load_blocks_t *s,uint32_t header,size_t first_fixup,size_t first_chunk,
    const uint8_t *source,int type,uint32_t *length)
{
    c1_state_t *c=s->c1;
    uint32_t L=0,A;
    size_t i,offset;
    *length=0;
    if (!c || c->failed || !header || ((header-1)>>28)!=0) return header;
    for (i=first_chunk;i<c->chunk_count;++i) if (c->chunks[i].reference==header) { L=c->chunks[i].length; break; }
    offset=(header-1)&0xfffffff;
    if (!L || offset+L>s->capacity[0]) return header;
    i=(c->arena_size+15)&~(size_t)15;
    if (i+L>0x0fffffffu || !c1_grow((void **)&c->arena,&c->arena_capacity,i+L,1)) { c->failed=1; return header; }
    memcpy(c->arena+i,s->data[0]+offset,L); c->arena_size=i+L;
    A=(8u<<28)+(uint32_t)i+1;
    for (i=first_fixup;i<c->fixup_count;++i) {
        fixup_t *f=c->fixups+i;
        if (f->slot>=header && f->slot<header+L) f->slot=f->slot-header+A;
        if ((f->kind==FIXUP_DATA || f->kind==FIXUP_SCRIPT_STRINGS || f->kind==FIXUP_RUNTIME) && f->target>=header && f->target<header+L)
            f->target=f->target-header+A;
    }
    if (!c1_grow((void **)&c->headers,&c->header_capacity,c->header_count+1,sizeof(*c->headers))) { c->failed=1; return A; }
    c->headers[c->header_count++]=(c1_header_t){source,A,L,type<0 ? UINT32_MAX : (uint32_t)type};
    *length=L; return A;
}
/* slot: the loader-memory address of the pointer word C1 relocates for this reference (0: none recorded);
 * type: the asset type, or -1 to derive it from the loader. */
static int replay_asset_slot(load_blocks_t *s,uint32_t reference,uint32_t field,uint32_t slot,int apply,
    int (*loader)(load_blocks_t *,int,uint32_t *),int type)
{
    uint32_t header=0,alias=reference==UINT32_MAX ? field : 0;
    size_t start=s->source_position,first_fixup=0,first_chunk=0;
    int catalog=c1_loader_type(loader);
    if (type<0) type=catalog>=0 ? catalog : loader==replay_technique_set ? 9 : loader==replay_image ? 10 :
        loader==replay_map_entities ? 19 : -1;
    if (!PushLoadBlock(s,0)) return 0;
    if (reference>=UINT32_MAX-1) {
        uint32_t inserted=0,length=0;
        if (reference==UINT32_MAX-1 && !reserve(s,3,4,4,0,apply,&alias)) return 0;
        inserted=reference==UINT32_MAX-1 ? alias : 0;
        if (apply && s->c1) { first_fixup=s->c1->fixup_count; first_chunk=s->c1->chunk_count; }
        if (!loader(s,apply,&header)) return 0;
        if (catalog>=0 && !record_asset(s,(unsigned)catalog,alias,start,apply)) return 0;
        if (apply && s->c1) {
            header=c1_copy_header(s,header,first_fixup,first_chunk,s->source+start,type,&length);
            s->last_header=header;
            c1_fix(s,apply,inserted,header,length,FIXUP_ASSET|(type>=0 ? (uint32_t)type<<8 : 0xff00u));
            c1_fix(s,apply,slot,header,length,FIXUP_ASSET|(type>=0 ? (uint32_t)type<<8 : 0xff00u));
        }
    } else if (reference) c1_fix(s,apply,slot,reference,0,FIXUP_ALIAS);
    return PopLoadBlock(s);
}
/* field is the block 3 address of the word holding the reference, or 0 when
 * that word lives in temporary memory. C1 only emits the insert-pointer marker
 * (-2) for temporary holders; a holder already in block 3 is itself the target
 * of later deduplicated pointers (DB_ConvertOffsetToAlias reads the linked
 * asset back out of that word), so an inline (-1) asset must be catalogued at
 * the holder's own address for those pointers to resolve. */
static int replay_field_asset_reference(load_blocks_t *s,uint32_t reference,uint32_t field,int apply,
    int (*loader)(load_blocks_t *,int,uint32_t *))
{ return replay_asset_slot(s,reference,field,field,apply,loader,-1); }
static int replay_asset_reference(load_blocks_t *s,uint32_t reference,int apply,
    int (*loader)(load_blocks_t *,int,uint32_t *))
{ return replay_asset_slot(s,reference,0,0,apply,loader,-1); }
/* An asset pointer field read from serialized memory at field. */
static int asset_field(load_blocks_t *s,const uint8_t *field,int apply,
    int (*loader)(load_blocks_t *,int,uint32_t *))
{ return replay_asset_slot(s,read32(field),0,apply && s->c1 ? c1_slot(s,field) : 0,apply,loader,-1); }
static int replay_material(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*textures;
    uint32_t ignored;
    unsigned i;
    if (!reserve(s,0,96,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    c1_note(s,apply,h+8,8,FIXUP_RUNTIME); /* GfxDrawSurf: C1 assigns sort and material indices at load */
    if (!asset_field(s,h+80,apply,replay_technique_set)) return 0;
    if (read32(h+84)==UINT32_MAX) {
        if (!(textures=replay_bytes(s,h[72],12,4,apply))) return 0;
        c1_link(s,apply,h+84);
        for (i=0;i<h[72];++i) {
            const uint8_t *texture=textures+12*i;
            uint32_t ref=read32(texture+8);
            if (texture[7]==11) {
                if (ref==UINT32_MAX) {
                    uint32_t slot=apply && s->c1 ? c1_slot(s,texture+8) : 0;
                    if (!replay_water(s,apply,&ignored)) return 0;
                    c1_fix(s,apply,slot,ignored,68,FIXUP_DATA);
                } else c1_pooled(s,apply,texture+8);
            } else if (!replay_asset_slot(s,ref,0,apply && s->c1 ? c1_slot(s,texture+8) : 0,apply,replay_image,10)) return 0;
        }
    }
    if (!array_field(s,h+88,h[73],32,16,apply,NULL)) return 0;
    if (!array_field(s,h+92,h[74],8,4,apply,NULL)) return 0;
    return PopLoadBlock(s);
}
int ReplayMaterial(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_material(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
static int replay_preset_asset(load_blocks_t *s,int apply,uint32_t *reference)
{ return replay_physics(s,0,apply,reference); }
static int replay_physics_map_asset(load_blocks_t *s,int apply,uint32_t *reference)
{ return replay_physics(s,1,apply,reference); }
static int replay_model(load_blocks_t *s,int apply,uint32_t *reference)
{
    static const unsigned strides[6]={2,1,8,12,1,32},alignments[6]={2,1,2,4,1,4};
    const uint8_t *h=s->source+s->source_position,*materials,*surfaces;
    unsigned i,count;
    if (!reserve(s,0,360,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    c1_note(s,apply,h+0x15d,1,FIXUP_RUNTIME); /* a flag byte C1 sets at load */
    c1_note(s,apply,h+6,1,FIXUP_RUNTIME); /* surface count: C1 clears it at load (measured: 0 in 6077 of 6618 models) */
    if (h[5]>h[4]) return 0;
    if (!string_field(s,h,apply)) return 0;
    for (i=0;i<6;++i) if (read32(h+44+4*i)==UINT32_MAX) {
        count=i>=1 && i<=3 ? h[4]-h[5] : h[4];
        if (!replay_bytes(s,count,strides[i],alignments[i],apply)) return 0;
        c1_link(s,apply,h+44+4*i);
        if (!i) c1_note_last(s,apply,count*2,FIXUP_SCRIPT_STRINGS); /* bone names */
    } else c1_pooled(s,apply,h+44+4*i);
    if (read32(h+68)) {
        uint32_t slots=0x30000001u+(uint32_t)((s->position[3]+3)&~(size_t)3);
        if (!(materials=replay_bytes(s,h[6],4,4,apply))) return 0;
        c1_link(s,apply,h+68);
        for (i=0;i<h[6];++i) {
            size_t start=s->source_position;
            if (!asset_field(s,materials+4*i,apply,replay_material)) return 0;
            if (read32(materials+4*i)>=UINT32_MAX-1 &&
                !record_asset(s,5,slots+4*i,start,apply)) return 0;
        }
    }
    for (i=0;i<4;++i) {
        const uint8_t *lod=h+72+56*i;
        /* written by C1 when it links the lod's surfaces (measured: +16..23, +26..27, +48..51 of each lod) */
        c1_note(s,apply,lod+16,8,FIXUP_RUNTIME);
        c1_note(s,apply,lod+4,4,FIXUP_RUNTIME);  /* surface count and index: C1 rewrites them at load (measured 2-5 -> 1 on 117 attachment models) */
        c1_note(s,apply,lod+26,2,FIXUP_RUNTIME);
        c1_note(s,apply,lod+48,4,FIXUP_RUNTIME);
        if (!asset_field(s,lod+8,apply,replay_surfaces)) return 0;
        if (!string_field(s,lod+12,apply)) return 0;
    }
    if (read32(h+0x12c)) {
        count=read32(h+0x130);
        if (!(surfaces=replay_bytes(s,count,44,4,apply))) return 0;
        c1_link(s,apply,h+0x12c);
        for (i=0;i<count;++i)
            if (!present_field(s,surfaces+44*i,read32(surfaces+44*i+4),48,4,apply,NULL)) return 0;
    }
    if (!present_field(s,h+0x138,h[4],28,4,apply,NULL)) return 0;
    if (!asset_field(s,h+0x160,apply,replay_preset_asset) ||
        !asset_field(s,h+0x164,apply,replay_physics_map_asset)) return 0;
    return PopLoadBlock(s);
}
int ReplayXModel(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    asset_t asset;
    size_t extent;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    asset=(asset_t){4,stream->source+stream->source_position,stream->source_size-stream->source_position};
    if (!ReadXModelExtent(&asset,&extent)) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_model(&plan,pass,&result) || plan.source_position-stream->source_position!=extent) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
/* C1 XAnimParts 0x4b7a50, delta parts 0x4b7690 and track loaders. */
static int replay_animation(load_blocks_t *s,int apply,uint32_t *reference)
{
    static const unsigned pointer_offsets[6]={52,56,60,64,68,72};
    static const unsigned count_offsets[6]={4,6,8,32,10,12};
    static const unsigned widths[6]={1,2,4,2,1,4};
    const uint8_t *h=s->source+s->source_position,*delta,*notify=NULL;
    unsigned i,index_width;
    if (!reserve(s,0,88,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (!present_field(s,h+48,h[26],2,2,apply,NULL)) return 0;
    if (read32(h+48)) c1_note_last(s,apply,h[26]*2u,FIXUP_SCRIPT_STRINGS); /* bone names */
    if (!present_field(s,h+80,h[27],8,4,apply,&notify)) return 0;
    if (notify) for (i=0;i<h[27];++i) c1_note(s,apply,notify+8*i,2,FIXUP_SCRIPT_STRINGS); /* notetrack names */
    index_width=read16(h+14)<256 ? 1 : 2;
    if (read32(h+84)) {
        if (!(delta=replay_bytes(s,1,12,4,apply))) return 0;
        c1_link(s,apply,h+84);
        for (i=0;i<3;++i) if (read32(delta+4*i)) {
            const uint8_t *track=replay_bytes(s,1,4,4,apply),*frames;
            unsigned count;
            uint32_t slot,start;
            if (!track) return 0;
            /* XAnimPartTrans / XAnimDeltaPartQuat: size, then the inline union and indices */
            slot=apply && s->c1 ? c1_slot(s,delta+4*i) : 0; start=s->last_reference;
            count=read16(track);
            if (!count) {
                if (!replay_bytes(s,1,i==0 ? 12 : i==1 ? 4 : 8,1,apply)) return 0;
                c1_link_extent(s,apply,slot,start);
            } else {
                if (!(frames=replay_bytes(s,1,i==0 ? 28 : 4,1,apply)) ||
                    !replay_bytes(s,count+1,index_width,1,apply)) return 0;
                c1_link_extent(s,apply,slot,start);
                if (read32(frames+(i==0 ? 24 : 0))) {
                    unsigned stride=i==0 ? (track[2] ? 3 : 6) : i==1 ? 4 : 8;
                    unsigned alignment=i==0 && track[2] ? 1 : 4;
                    if (!replay_bytes(s,count+1,stride,alignment,apply)) return 0;
                    c1_link(s,apply,frames+(i==0 ? 24 : 0));
                }
            }
        }
    }
    for (i=0;i<6;++i) if (read32(h+pointer_offsets[i])) {
        uint32_t count=i==3 ? read32(h+count_offsets[i]) : read16(h+count_offsets[i]);
        if (!replay_bytes(s,count,widths[i],widths[i],apply)) return 0;
        c1_link(s,apply,h+pointer_offsets[i]);
    }
    if (!present_field(s,h+76,read32(h+36),index_width,index_width,apply,NULL)) return 0;
    return PopLoadBlock(s);
}
int ReplayAnimation(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_animation(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
/* C1 FxEffectDef 0x4bd5e0; element/visual/extension loaders 0x4bd2b0,
 * 0x4bd050 and 0x4bd230. Effect references serialize names, not nested FX. */
static int replay_effect_visual(load_blocks_t *s,unsigned type,uint32_t ref,uint32_t slot,int apply)
{
    size_t start=s->source_position;
    if (type==12 || type==14) { /* a runner keeps the FxEffectDef C1 finds by name; a sound keeps its name (measured:
                                 * the C1 word is the string, not an alias list) */
        uint32_t kind=type==14 ? FIXUP_ASSET_NAME|30u<<8 : FIXUP_DATA;
        if (ref!=UINT32_MAX) { if (ref) c1_fix(s,apply,slot,ref,0,kind); return 1; }
        if (!replay_string(s,apply)) return 0;
        c1_fix(s,apply,slot,s->last_reference,s->last_length,kind); return 1;
    }
    if (type==10 || type==11) return 1;
    if(!replay_asset_slot(s,ref,0,slot,apply,type==9 ? replay_model : replay_material,-1)) return 0;
    return ref<UINT32_MAX-1 || record_asset(s,type==9 ? 4 : 5,slot,start,apply);
}
static int replay_effect(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*elements,*visuals,*trail;
    uint64_t count=0;
    unsigned i,j,type,n; uint32_t element_slots,visual_slots;
    if (!reserve(s,0,32,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    for (i=16;i<=24;i+=4) {
        uint32_t part=read32(h+i);
        if (part>INT32_MAX) return 0;
        count+=part;
    }
    if (count>INT32_MAX) return 0;
    if (read32(h+28)) {
        element_slots=0x30000001u+(uint32_t)((s->position[3]+3)&~(size_t)3);
        if (!(elements=replay_bytes(s,(uint32_t)count,260,4,apply))) return 0;
        c1_link(s,apply,h+28);
        for (i=0;i<count;++i) {
            const uint8_t *e=elements+260*(size_t)i;
            type=e[176]; n=e[177];
            if (!present_field(s,e+180,e[178]+1,96,4,apply,NULL)) return 0;
            if (!present_field(s,e+184,e[179]+1,48,4,apply,NULL)) return 0;
            if (type==13) {
                if (read32(e+188)) {
                    visual_slots=0x30000001u+(uint32_t)((s->position[3]+3)&~(size_t)3);
                    if (!(visuals=replay_bytes(s,n,8,4,apply))) return 0;
                    c1_link(s,apply,e+188);
                    for (j=0;j<2*n;++j)
                        if (!replay_effect_visual(s,0,read32(visuals+4*j),visual_slots+4*j,apply)) return 0;
                }
            } else if (n<=1) {
                if (!replay_effect_visual(s,type,read32(e+188),element_slots+260*i+188,apply)) return 0;
            } else if (read32(e+188)) {
                visual_slots=0x30000001u+(uint32_t)((s->position[3]+3)&~(size_t)3);
                if (!(visuals=replay_bytes(s,n,4,4,apply))) return 0;
                c1_link(s,apply,e+188);
                for (j=0;j<n;++j)
                    if (!replay_effect_visual(s,type,read32(visuals+4*j),visual_slots+4*j,apply)) return 0;
            }
            /* effectOnImpact / OnDeath / Emitted: names C1 resolves to the FxEffectDef (FxEffectDefRef) */
            for (j=216;j<=224;j+=4)
                if (!asset_name_field(s,e+j,30,apply)) return 0;
            if (read32(e+244)) {
                if (type==5) {
                    if (!(trail=replay_bytes(s,1,36,4,apply))) return 0;
                    c1_link(s,apply,e+244);
                    if (!present_field(s,trail+24,read32(trail+20),20,4,apply,NULL)) return 0;
                    if (!present_field(s,trail+32,read32(trail+28),2,2,apply,NULL)) return 0;
                } else {
                    if (!replay_bytes(s,1,type==8 ? 52 : 1,type==8 ? 4 : 1,apply)) return 0;
                    c1_link(s,apply,e+244);
                }
            }
            if (!string_field(s,e+256,apply)) return 0;
        }
    }
    return PopLoadBlock(s);
}
int ReplayEffect(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_effect(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
/* C1 LoadedSound 0x4b80b0 and sound data 0x4b7f30. Payload storage
 * temporarily enters block 0, even though the name resides in block 3. */
static int replay_loaded_sound(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    uint32_t ref,ignored;
    if (!reserve(s,0,48,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    /* the sound driver's handles C1 writes when it uploads the sample */
    c1_note(s,apply,h+8,4,FIXUP_RUNTIME);
    c1_note(s,apply,h+0x28,8,FIXUP_RUNTIME);
    if (!PushLoadBlock(s,0)) return 0;
    ref=read32(h+44);
    if (ref>=UINT32_MAX-1) {
        if (ref==UINT32_MAX-1 && !reserve(s,3,4,4,0,apply,&ignored)) return 0;
        /* the sample payload: the server never reads it, so the image map does not keep it */
        if (s->image) s->image->skip=1;
        if (!replay_bytes(s,read32(h+12),1,1,apply)) { if (s->image) s->image->skip=0; return 0; }
        if (s->image) s->image->skip=0;
    }
    return PopLoadBlock(s) && PopLoadBlock(s);
}
/* C1 SndCurve 0x4b82b0. */
static int replay_sound_curve(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    if (!reserve(s,0,136,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    return PopLoadBlock(s);
}
/* C1 snd_alias_list_t 0x4b8750, aliases 0x4b84b0, SoundFile 0x4b8240
 * and SpeakerMap 0x4b8420. Pooled array references consume no input. */
/* Reconstructed loaded sounds live in the rewound scratch block, so a
 * deduplicated pointer into replay memory cannot reach them. Record the
 * serialized span of every alias array and sound file next to the block 3
 * address its reconstruction received; the reader replays those spans from the
 * archive image, which stays owned for the archive's whole lifetime. */
static int record_sound_span(load_blocks_t *s,uint32_t reference,size_t start,uint32_t count,int apply)
{
    if (!s->collect_asset_aliases) return 1;
    if (apply) {
        if (s->sound_record_count>=s->sound_record_capacity) return 0;
        s->sound_records[s->sound_record_count]=(sound_span_t){
            s->source+start,reference,(uint32_t)(s->source_position-start),count};
    }
    ++s->sound_record_count; return 1;
}
static uint32_t block_three_reference(const load_blocks_t *s)
{ return 0x30000001u+(uint32_t)((s->position[3]+3)&~(size_t)3); }
static int replay_sound_alias(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*aliases,*file,*speakers;
    uint32_t count,i,j,array,record;
    size_t start,at;
    if (!reserve(s,0,12,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    count=read32(h+8);
    if (count>INT32_MAX) return 0;
    if (read32(h+4)==UINT32_MAX) {
        array=block_three_reference(s); start=s->source_position;
        if (!(aliases=replay_bytes(s,count,108,4,apply))) return 0;
        c1_link(s,apply,h+4);
        for (i=0;i<count;++i) {
            const uint8_t *a=aliases+(size_t)i*108;
            for (j=0;j<=16;j+=4)
                if (!string_field(s,a+j,apply)) return 0;
            if (read32(a+20)==UINT32_MAX) {
                record=block_three_reference(s); at=s->source_position;
                if (!(file=replay_bytes(s,1,12,4,apply))) return 0;
                c1_link(s,apply,a+20);
                if (file[0]==1) {
                    if (!replay_field_asset_reference(s,read32(file+4),record+4,apply,replay_loaded_sound)) return 0;
                } else {
                    for (j=4;j<=8;j+=4)
                        if (!string_field(s,file+j,apply)) return 0;
                }
                if (!record_sound_span(s,record,at,0,apply)) return 0;
            } else c1_pooled(s,apply,a+20);
            c1_note(s,apply,a+24,4,FIXUP_RUNTIME); /* the picker's last sequence (0x61ec9a) */
            if (!replay_field_asset_reference(s,read32(a+88),array+(uint32_t)i*108+88,apply,replay_sound_curve)) return 0;
            if (read32(a+104)==UINT32_MAX) {
                if (!(speakers=replay_bytes(s,1,408,4,apply))) return 0;
                c1_link(s,apply,a+104);
                if (!string_field(s,speakers+4,apply)) return 0;
            } else c1_pooled(s,apply,a+104);
        }
        if (!record_sound_span(s,array,start,count,apply)) return 0;
    }
    return PopLoadBlock(s);
}
int ReplaySoundAsset(load_blocks_t *stream,unsigned type,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    int (*loader)(load_blocks_t *,int,uint32_t *);
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    switch (type) {
    case 11: loader=replay_sound_alias; break;
    case 12: loader=replay_sound_curve; break;
    case 13: loader=replay_loaded_sound; break;
    default: return 0;
    }
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!loader(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
/* GfxWorld prefix through cells: C1 0x4c7fe0..0x4c81be. Keep block 3
 * entered for the following draw-data stage; this is not a complete asset. */
static int replay_world_visibility(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*skies,*counts=NULL,*trees,*nodes,*cells,*portals;
    uint32_t count,i,j,ignored;
    uint64_t runtime;
    if (!reserve(s,0,628,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    for (i=0;i<8;i+=4) if (!string_field(s,h+i,apply)) return 0;
    count=read32(h+20);
    if (count>INT32_MAX) return 0;
    if (read32(h+24)) {
        if (!(skies=replay_bytes(s,count,16,4,apply))) return 0;
        for (i=0;i<count;++i) {
            const uint8_t *sky=skies+(size_t)i*16;
            if (!present_field(s,sky+4,read32(sky),4,4,apply,NULL)) return 0;
            if (!asset_field(s,sky+8,apply,replay_image)) return 0;
        }
    }
    if (!array_field(s,h+56,read32(h+8),20,4,apply,NULL)) return 0;
    if (!present_field(s,h+60,read32(h+12),2,2,apply,NULL)) return 0;
    count=read32(h+52);
    if (count>INT32_MAX || !PushLoadBlock(s,2)) return 0;
    runtime=(uint64_t)count*2048;
    if (read32(h+64) && (runtime>SIZE_MAX || !reserve(s,2,(size_t)runtime,4,1,apply,&ignored))) return 0;
    if (!PopLoadBlock(s)) return 0;
    if (!present_field(s,h+68,count,4,4,apply,&counts)) return 0;
    if (read32(h+72)) {
        if (!(trees=replay_bytes(s,count,4,128,apply))) return 0;
        for (i=0;i<count;++i) if (read32(trees+4*i)) {
            uint32_t n;
            if (!counts || (n=read32(counts+4*i))>INT32_MAX ||
                !(nodes=replay_bytes(s,n,44,4,apply))) return 0;
            for (j=0;j<n;++j) {
                const uint8_t *node=nodes+(size_t)j*44;
                if (!array_field(s,node+36,read16(node+34),2,2,apply,NULL)) return 0;
            }
        }
    }
    if (read32(h+76)) {
        if (!(cells=replay_bytes(s,count,40,4,apply))) return 0;
        for (i=0;i<count;++i) {
            const uint8_t *cell=cells+(size_t)i*40;
            uint32_t n=read32(cell+24);
            if (read32(cell+28)) {
                if (n>INT32_MAX || !(portals=replay_bytes(s,n,60,4,apply))) return 0;
                for (j=0;j<n;++j) {
                    const uint8_t *portal=portals+(size_t)j*60;
                    if (!present_field(s,portal+28,portal[34],12,4,apply,NULL)) return 0;
                }
            }
            if (!present_field(s,cell+36,cell[32],1,1,apply,NULL)) return 0;
        }
    }
    return 1;
}
/* C1 GfxWorldDraw 0x4c7d20: reflection probes, lightmaps, vertices and
 * indices. GPU buffer creation is not part of serialized block replay. */
static int replay_world_draw(load_blocks_t *s,const uint8_t *d,int apply)
{
    const uint8_t *images;
    uint32_t count,i,j,ignored;
    uint64_t size;
    count=read32(d);
    if (count>INT32_MAX) return 0;
    if (read32(d+4)) {
        if (!(images=replay_bytes(s,count,4,4,apply))) return 0;
        for (i=0;i<count;++i)
            if (!asset_field(s,images+4*i,apply,replay_image)) return 0;
    }
    if (!present_field(s,d+8,count,12,4,apply,NULL)) return 0;
    if (!PushLoadBlock(s,2)) return 0;
    size=(uint64_t)count*4;
    if (read32(d+12) && (size>SIZE_MAX || !reserve(s,2,(size_t)size,4,1,apply,&ignored))) return 0;
    if (!PopLoadBlock(s)) return 0;
    count=read32(d+16);
    if (count>INT32_MAX) return 0;
    if (read32(d+20)) {
        if (!(images=replay_bytes(s,count,8,4,apply))) return 0;
        for (i=0;i<count;++i) for (j=0;j<2;++j)
            if (!asset_field(s,images+(size_t)i*8+4*j,apply,replay_image)) return 0;
    }
    for (i=24;i<=28;i+=4) {
        if (!PushLoadBlock(s,2)) return 0;
        size=(uint64_t)count*4;
        if (read32(d+i) && (size>SIZE_MAX || !reserve(s,2,(size_t)size,4,1,apply,&ignored))) return 0;
        if (!PopLoadBlock(s)) return 0;
    }
    for (i=32;i<=36;i+=4)
        if (!asset_field(s,d+i,apply,replay_image)) return 0;
    if (!present_field(s,d+44,read32(d+40),44,4,apply,NULL)) return 0;
    if (!present_field(s,d+56,read32(d+52),1,1,apply,NULL)) return 0;
    if (!present_field(s,d+68,read32(d+64),2,2,apply,NULL)) return 0;
    return 1;
}
/* C1 GfxLightGrid 0x4c6e60. Row offsets use the selected bounds axis. */
static int replay_world_light_grid(load_blocks_t *s,const uint8_t *g,int apply)
{
    if (read32(g+28)) {
        uint32_t axis=read32(g+20),minimum,maximum;
        if (axis>2) return 0;
        minimum=read16(g+8+2*axis); maximum=read16(g+14+2*axis);
        if (maximum<minimum || !replay_bytes(s,maximum-minimum+1,2,2,apply)) return 0;
    }
    if (!present_field(s,g+36,read32(g+32),1,1,apply,NULL)) return 0;
    if (!present_field(s,g+44,read32(g+40),4,4,apply,NULL)) return 0;
    if (!present_field(s,g+52,read32(g+48),168,4,apply,NULL)) return 0;
    return 1;
}
/* C1 0x4c81e3..0x4c86c8: world models/materials, scene allocations,
 * shadow geometry and light-region hulls. */
static int replay_world_scene(load_blocks_t *s,const uint8_t *h,int apply)
{
    const uint8_t *records,*hulls;
    uint64_t counts[8],cells=read32(h+52),words=(cells+31)/32,non_sun=0;
    static const unsigned strides[8]={4,4,6,4,32768,4,4,1};
    uint32_t i,j,n,lights=read32(h+32),sun=read32(h+28),ignored;
    if (!present_field(s,h+212,read32(h+208),60,4,apply,NULL)) return 0;
    if (read32(h+248)) {
        n=read32(h+244);
        if (n>INT32_MAX || !(records=replay_bytes(s,n,8,4,apply))) return 0;
        for (i=0;i<n;++i)
            if (!asset_field(s,records+(size_t)i*8,apply,replay_material)) return 0;
    }
    if (!asset_field(s,h+256,apply,replay_material) ||
        !asset_field(s,h+260,apply,replay_material) ||
        !asset_field(s,h+412,apply,replay_image)) return 0;
    if (read32(h+432) || read32(h+436) || read32(h+440)) {
        if (lights<=sun) return 0;
        non_sun=(uint64_t)lights-sun-1;
    }
    counts[0]=words*cells; counts[1]=words;
    counts[2]=read32(h+572); counts[3]=read32(h+576);
    counts[4]=non_sun; counts[5]=non_sun*counts[2]; counts[6]=non_sun*counts[3]; counts[7]=counts[2];
    for (i=0;i<8;++i) {
        if (!PushLoadBlock(s,2)) return 0;
        if (read32(h+416+4*i) && (counts[i]>SIZE_MAX/strides[i] ||
            !reserve(s,2,(size_t)counts[i]*strides[i],i==7 ? 1 : 4,1,apply,&ignored))) return 0;
        if (!PopLoadBlock(s)) return 0;
    }
    if (lights>INT32_MAX) return 0;
    if (read32(h+448)) {
        if (!(records=replay_bytes(s,lights,12,4,apply))) return 0;
        for (i=0;i<lights;++i) {
            const uint8_t *r=records+(size_t)i*12;
            if (!present_field(s,r+4,read16(r),2,2,apply,NULL)) return 0;
            if (!present_field(s,r+8,read16(r+2),2,2,apply,NULL)) return 0;
        }
    }
    if (read32(h+452)) {
        if (!(records=replay_bytes(s,lights,8,4,apply))) return 0;
        for (i=0;i<lights;++i) {
            const uint8_t *r=records+(size_t)i*8;
            if (read32(r+4)) {
                n=read32(r);
                if (n>INT32_MAX || !(hulls=replay_bytes(s,n,80,4,apply))) return 0;
                for (j=0;j<n;++j) {
                    const uint8_t *hull=hulls+(size_t)j*80;
                    if (!present_field(s,hull+76,read32(hull+72),20,4,apply,NULL)) return 0;
                }
            }
        }
    }
    return 1;
}
static int replay_world_zero(load_blocks_t *s,const uint8_t *field,uint64_t count,unsigned stride,unsigned alignment,int apply)
{
    uint32_t ignored,ref=read32(field);
    if (!PushLoadBlock(s,2)) return 0;
    if (ref && (count>SIZE_MAX/stride || !reserve(s,2,(size_t)count*stride,alignment,1,apply,&ignored))) return 0;
    if (ref) {
        c1_link(s,apply,field);
        c1_note_last(s,apply,(uint32_t)(count*stride),FIXUP_RUNTIME); /* block 2: zero-filled, the game's state */
    }
    return PopLoadBlock(s);
}
/* Static/dynamic DPVS: C1 0x4c7630 / 0x4c7290. */
static int replay_world_dpvs(load_blocks_t *s,const uint8_t *h,int apply)
{
    const uint8_t *p=h+456,*d=h+564,*records;
    uint32_t models=read32(p),surfaces=read32(h+16),i,j;
    uint64_t sorted=(uint64_t)read32(p+4)+read32(p+8);
    if (models>INT32_MAX || surfaces>INT32_MAX || sorted>UINT32_MAX) return 0;
    for (i=0;i<6;++i)
        if (!replay_world_zero(s,p+52+4*i,read32(p+(i<3 ? 0 : 4)),1,1,apply)) return 0;
    if (!present_field(s,p+76,(uint32_t)sorted,2,2,apply,NULL)) return 0;
    if (!present_field(s,p+80,models,36,4,apply,NULL)) return 0;
    if (read32(p+84)) {
        if (!(records=replay_bytes(s,surfaces,24,4,apply))) return 0;
        for (i=0;i<surfaces;++i)
            if (!asset_field(s,records+(size_t)i*24+16,apply,replay_material)) return 0;
    }
    if (!present_field(s,p+88,surfaces,24,4,apply,NULL)) return 0;
    if (read32(p+92)) {
        uint32_t slots=0x30000001u+(uint32_t)((s->position[3]+3)&~(size_t)3);
        if (!(records=replay_bytes(s,models,76,4,apply))) return 0;
        c1_link(s,apply,p+92);
        for (i=0;i<models;++i) {
            uint32_t ref=read32(records+(size_t)i*76+52);
            size_t start=s->source_position;
            if (!asset_field(s,records+(size_t)i*76+52,apply,replay_model)) return 0;
            if (ref>=UINT32_MAX-1 && !record_asset(s,4,slots+76*i+52,start,apply)) return 0;
        }
    }
    if (!replay_world_zero(s,p+96,surfaces,8,8,apply) ||
        !replay_world_zero(s,p+100,read32(p+48),4,128,apply)) return 0;
    for (i=0;i<2;++i)
        if (!replay_world_zero(s,d+16+4*i,(uint64_t)read32(h+52)*read32(d+4*i),4,4,apply)) return 0;
    /* The client interleaves both dynamic categories for each view. */
    for (i=0;i<3;++i) for (j=0;j<2;++j)
        if (!replay_world_zero(s,d+24+12*j+4*i,read32(d+4*j),32,16,apply)) return 0;
    return 1;
}
static int replay_world(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    if (!replay_world_visibility(s,apply,reference) || !replay_world_draw(s,h+80,apply) ||
        !replay_world_light_grid(s,h+152,apply) || !replay_world_scene(s,h,apply) ||
        !replay_world_dpvs(s,h,apply)) return 0;
    if (!present_field(s,h+620,read32(h+616),56,4,apply,NULL)) return 0;
    return PopLoadBlock(s);
}
int ReplayWorld(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_world(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
static int replay_world_prefix(load_blocks_t *stream,int draw,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_world_visibility(&plan,pass,&result)) return 0;
        if (draw && !replay_world_draw(&plan,stream->source+stream->source_position+80,pass)) return 0;
        if (draw && !replay_world_light_grid(&plan,stream->source+stream->source_position+152,pass)) return 0;
        if (draw>1 && !replay_world_scene(&plan,stream->source+stream->source_position,pass)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
int ReplayWorldVisibility(load_blocks_t *s,uint32_t *reference)
{ return replay_world_prefix(s,0,reference); }
int ReplayWorldDraw(load_blocks_t *s,uint32_t *reference)
{ return replay_world_prefix(s,1,reference); }
int ReplayWorldScene(load_blocks_t *s,uint32_t *reference)
{ return replay_world_prefix(s,2,reference); }
/* C1 FxWorld 0x4be220 and glass system 0x4bdb50. Runtime glass arrays
 * live in zero-filled block 2 and therefore consume no serialized bytes. */
static int record_glass_span(load_blocks_t *,int,glass_span_t);
static int replay_effect_world(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*g,*defs,*initial=NULL;
    uint64_t sizes[9],pieces,words;
    uint32_t ignored,count,i;
    if (!reserve(s,0,116,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    g=h+4; count=read32(g+8);
    if (count>INT32_MAX) return 0;
    /* FxGlassSystem time, previous time and an initialised flag: C1 writes them (measured on killhouse) */
    c1_note(s,apply,h+4,4,FIXUP_RUNTIME); c1_note(s,apply,h+8,4,FIXUP_RUNTIME);
    c1_note(s,apply,h+0x69,1,FIXUP_RUNTIME);
    if (read32(g+48)) {
        uint32_t slots=0x30000001u+(uint32_t)((s->position[3]+3)&~(size_t)3);
        if (!(defs=replay_bytes(s,count,36,4,apply))) return 0;
        for (i=0;i<count;++i) {
            const uint8_t *d=defs+(size_t)i*36;
            size_t start=s->source_position;
            if (!asset_field(s,d+32,apply,replay_preset_asset)) return 0;
            if (read32(d+32)>=UINT32_MAX-1 && !record_asset(s,0,slots+36*i+32,start,apply)) return 0;
            if (!asset_field(s,d+24,apply,replay_material) ||
                !asset_field(s,d+28,apply,replay_material)) return 0;
        }
    }
    pieces=read32(g+12); words=read32(g+16);
    sizes[0]=pieces*32; sizes[1]=pieces*32; sizes[2]=pieces*36;
    sizes[3]=(uint64_t)read32(g+36)*4; sizes[4]=words*4;
    sizes[5]=words*read32(g+24);
    if (sizes[5]>UINT64_MAX/4) return 0;
    sizes[5]*=4;
    sizes[6]=(pieces+15)&~UINT64_C(15); sizes[7]=pieces*12;
    sizes[8]=((pieces+3)&~UINT64_C(3))*4;
    for (i=0;i<9;++i) {
        if (!PushLoadBlock(s,2)) return 0;
        if (read32(g+52+4*i) && (sizes[i]>SIZE_MAX ||
            !reserve(s,2,(size_t)sizes[i],i==6 || i==8 ? 16 : 4,1,apply,&ignored))) return 0;
        if (!PopLoadBlock(s)) return 0;
    }
    if (!present_field(s,g+88,read32(g+20),2,2,apply,NULL)) return 0;
    if (!present_field(s,g+92,read32(g+20),52,4,apply,&initial)) return 0;
    if (!record_glass_span(s,apply,(glass_span_t){
        .world=h,.group=UINT32_MAX-1,.pieces=initial,.piece_count=read32(g+20)})) return 0;
    if (!present_field(s,g+96,read32(g+44),4,4,apply,NULL)) return 0;
    return PopLoadBlock(s);
}
/* C1 ComWorld 0x4bfec0 and primary lights 0x4bfe10. */
static int replay_common_world(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*lights;
    uint32_t count,i;
    if (!reserve(s,0,16,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    count=read32(h+8);
    if (count>INT32_MAX) return 0;
    if (read32(h+12)) {
        if (!(lights=replay_bytes(s,count,68,4,apply))) return 0;
        c1_link(s,apply,h+12);
        for (i=0;i<count;++i)
            if (!string_field(s,lights+(size_t)i*68+64,apply)) return 0;
    }
    return PopLoadBlock(s);
}
/* C1 GfxLightDef 0x4bab40: attenuation image is an asset reference. */
static int replay_light_definition(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    if (!reserve(s,0,16,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (!asset_field(s,h+4,apply,replay_image)) return 0;
    return PopLoadBlock(s);
}
/* C1 vehicle-track segments 0x4bc520, sectors 0x4bc410 and branch
 * references 0x4bc480. Branches use -1 inline records, not asset wrappers. */
static int replay_vehicle_segment(load_blocks_t *s,const uint8_t *r,unsigned depth,int apply)
{
    const uint8_t *sectors,*refs,*child;
    uint32_t n,i,j;
    if (depth>=LOAD_STACK) return 0;
    if (!string_field(s,r,apply)) return 0;
    if (read32(r+4)) {
        n=read32(r+8);
        if (n>INT32_MAX || !(sectors=replay_bytes(s,n,60,4,apply))) return 0;
        c1_link(s,apply,r+4);
        for (i=0;i<n;++i) {
            const uint8_t *sector=sectors+(size_t)i*60;
            if (!present_field(s,sector+52,read32(sector+56),12,4,apply,NULL)) return 0;
        }
    }
    for (j=12;j<=20;j+=8) if (read32(r+j)) {
        n=read32(r+j+4);
        if (n>INT32_MAX || !(refs=replay_bytes(s,n,4,4,apply))) return 0;
        c1_link(s,apply,r+j);
        for (i=0;i<n;++i) if (read32(refs+4*i)==UINT32_MAX) {
            if (!(child=replay_bytes(s,1,44,4,apply))) return 0;
            c1_link(s,apply,refs+4*i);
            if (!replay_vehicle_segment(s,child,depth+1,apply)) return 0;
        }
    }
    return 1;
}
static int record_glass_span(load_blocks_t *s,int apply,glass_span_t span)
{
    if(!s->collect_asset_aliases) return 1;
    if(apply) {
        if(s->glass_record_count>=s->glass_record_capacity) return 0;
        s->glass_records[s->glass_record_count]=span;
    }
    ++s->glass_record_count;
    return 1;
}
/* C1 GameWorldMp 0x4bca00 and glass data 0x4bc800. */
static int replay_game_world_mp(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*records,*glass;
    uint32_t count,i;
    if (!reserve(s,0,84,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (apply) memset(s->data[0]+((*reference-1)&0x0fffffffu)+4,0,68);
    if (read32(h+72)==UINT32_MAX) {
        count=read32(h+76);
        if (count>INT32_MAX || !(records=replay_bytes(s,count,44,4,apply))) return 0;
        c1_link(s,apply,h+72);
        for (i=0;i<count;++i)
            if (!replay_vehicle_segment(s,records+(size_t)i*44,0,apply)) return 0;
    }
    if (!read32(h+80)) {
        if(!record_glass_span(s,apply,(glass_span_t){h,NULL,NULL,NULL,UINT32_MAX,0,0})) return 0;
    } else {
        if (!(glass=replay_bytes(s,1,128,4,apply))) return 0;
        c1_link(s,apply,h+80);
        const uint8_t *pieces=NULL;
        if (!present_field(s,glass,read32(glass+4),12,4,apply,&pieces)) return 0;
        if(!record_glass_span(s,apply,(glass_span_t){
            .world=h,.glass=glass,.group=UINT32_MAX,
            .pieces=pieces,.piece_count=read32(glass+4)})) return 0;
        if (read32(glass+16)) {
            count=read32(glass+12);
            if (count>INT32_MAX || !(records=replay_bytes(s,count,12,4,apply))) return 0;
            c1_link(s,apply,glass+16);
            for (i=0;i<count;++i) {
                const uint8_t *r=records+(size_t)i*12;
                const char *name=read32(r)==UINT32_MAX?(const char *)(s->source+s->source_position):NULL;
                const uint8_t *ids=NULL;
                if (!string_field(s,r,apply)) return 0;
                if (!present_field(s,r+8,read16(r+6),2,2,apply,&ids)) return 0;
                if(!record_glass_span(s,apply,(glass_span_t){h,glass,ids,name,i,read32(r),read16(r+6)})) return 0;
            }
        }
    }
    return PopLoadBlock(s);
}
/* C1 MapEnts 0x4be8c0, triggers 0x4be6f0 and stages 0x4be820.
 * AddonMapEnts 0x4beae0 shares the prefix but has no stages. */
static int replay_map_entities_body(load_blocks_t *s,int apply,uint32_t *reference,int addon)
{
    const uint8_t *h=s->source+s->source_position,*stages;
    static const unsigned strides[3]={8,32,20};
    unsigned i;
    if (!reserve(s,0,addon ? 36 : 44,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (!present_field(s,h+4,read32(h+8),1,1,apply,NULL)) return 0;
    for (i=0;i<3;++i)
        if (!present_field(s,h+16+8*i,read32(h+12+8*i),strides[i],4,apply,NULL)) return 0;
    if (!addon && read32(h+36)) {
        if (!(stages=replay_bytes(s,h[40],20,4,apply))) return 0;
        c1_link(s,apply,h+36);
        for (i=0;i<h[40];++i)
            if (!string_field(s,stages+20*i,apply)) return 0;
    }
    return PopLoadBlock(s);
}
static int replay_map_entities(load_blocks_t *s,int apply,uint32_t *reference)
{
    return replay_map_entities_body(s,apply,reference,0);
}
static int replay_addon_map_entities(load_blocks_t *s,int apply,uint32_t *reference)
{
    return replay_map_entities_body(s,apply,reference,1);
}
int ReplayMapEntities(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_map_entities(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
static int replay_collision_planes(load_blocks_t *s,const uint8_t *field,uint32_t count,int apply)
{
    const uint8_t *records=replay_bytes(s,count,8,4,apply);
    uint32_t i;
    if (!records) return 0;
    c1_link(s,apply,field);
    for (i=0;i<count;++i)
        if (!array_field(s,records+(size_t)i*8,1,20,4,apply,NULL)) return 0;
    return 1;
}
/* C1 clipMap_t 0x4bf410. Count/pointer offsets preserve loader order,
 * including leaf brushes before leaf-brush nodes and MapEnts after static nodes. */
static int replay_collision(load_blocks_t *s,int apply,uint32_t *reference)
{
    static const unsigned fields[][5]={
        {16,20,76,4,1},{24,28,12,4,2},{32,36,8,4,3},{40,44,1,1,0},
        {48,52,8,4,3},{56,60,40,4,0},{72,76,2,2,0},{64,68,20,4,4},
        {80,84,4,4,0},{88,92,12,4,0},{96,100,6,2,0},{96,104,4,1,5},
        {108,112,28,4,0},{116,120,12,4,6},{124,128,32,16,0},{132,136,68,4,0},
        {140,144,36,128,7},{140,148,24,128,0},{140,152,4,4,0},{160,164,28,4,0}
    };
    const uint8_t *h=s->source+s->source_position,*records;
    uint32_t count,i,j,k;
    if (!reserve(s,0,256,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    c1_note(s,apply,h+4,4,FIXUP_RUNTIME); /* isInUse */
    if (!string_field(s,h,apply)) return 0;
    if (!array_field(s,h+12,read32(h+8),20,4,apply,NULL)) return 0;
    for (i=0;i<sizeof(fields)/sizeof(fields[0]);++i) if (read32(h+fields[i][1])) {
        unsigned offset=fields[i][0],kind=fields[i][4];
        count=offset==140 || offset==160 ? read16(h+offset) : read32(h+offset);
        if (kind==5) count=(uint32_t)(((uint64_t)count*3+31)/32);
        if (!(records=replay_bytes(s,count,fields[i][2],fields[i][3],apply))) return 0;
        c1_link(s,apply,h+fields[i][1]);
        if (kind==0 || kind==5) continue;
        for (j=0;j<count;++j) {
            const uint8_t *r=records+(size_t)j*fields[i][2];
            if (kind==1 && !asset_field(s,r,apply,replay_model)) return 0;
            if (kind==2 && !string_field(s,r,apply)) return 0;
            if (kind==3 && !array_field(s,r,1,20,4,apply,NULL)) return 0;
            if (kind==4 && read16(r+2)>0 && read16(r+2)<32768 && read32(r+8)==UINT32_MAX) {
                if (!replay_bytes(s,read16(r+2),2,2,apply)) return 0;
                c1_link(s,apply,r+8);
            }
            else if (kind==4 && read16(r+2)>0 && read16(r+2)<32768) c1_pooled(s,apply,r+8);
            if (kind==6 && !array_field(s,r+8,1,28,4,apply,NULL)) return 0;
            if (kind==7) {
                unsigned edges=0;
                if (read32(r+4)==UINT32_MAX) { if (!replay_collision_planes(s,r+4,read16(r),apply)) return 0; }
                else c1_pooled(s,apply,r+4);
                for (k=30;k<36;++k) edges+=r[k];
                if (!array_field(s,r+8,edges,1,1,apply,NULL)) return 0;
            }
        }
    }
    if (!asset_field(s,h+156,apply,replay_map_entities)) return 0;
    for (i=0;i<2;++i) if (read32(h+172+4*i)) {
        uint32_t slots=0x30000001u+(uint32_t)((s->position[3]+3)&~(size_t)3);
        count=read16(h+168+2*i);
        if (!(records=replay_bytes(s,count,92,4,apply))) return 0;
        c1_link(s,apply,h+172+4*i);
        if(s->collect_asset_aliases) {
            if(apply) {
                if(s->dynamic_record_count>=s->dynamic_record_capacity) return 0;
                s->dynamic_records[s->dynamic_record_count]=(dynamic_records_t){h,records,i,count,slots};
            }
            ++s->dynamic_record_count;
        }
        for (j=0;j<count;++j) {
            const uint8_t *r=records+(size_t)j*92;
            size_t start;
            start=s->source_position;
            if (!asset_field(s,r+32,apply,replay_model)) return 0;
            if (read32(r+32)>=UINT32_MAX-1 && !record_asset(s,4,slots+92*j+32,start,apply)) return 0;
            start=s->source_position;
            if (!asset_field(s,r+40,apply,replay_effect)) return 0;
            if (read32(r+40)>=UINT32_MAX-1 && !record_asset(s,30,slots+92*j+40,start,apply)) return 0;
            start=s->source_position;
            if (!asset_field(s,r+44,apply,replay_preset_asset)) return 0;
            if (read32(r+44)>=UINT32_MAX-1 && !record_asset(s,0,slots+92*j+44,start,apply)) return 0;
        }
    }
    for (i=0;i<3;++i) for (j=0;j<2;++j)
        if (!replay_world_zero(s,h+180+8*i+4*j,read16(h+168+2*j),i==0 ? 32 : i==1 ? 12 : 20,4,apply)) return 0;
    return PopLoadBlock(s);
}
int ReplayCollision(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=0 || !stream->source ||
        stream->source_position>stream->source_size || stream->depth>=LOAD_STACK) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_collision(&plan,pass,&result)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
/* C1 TracerDef 0x4c23d0: name followed by a material asset reference. */
static int replay_tracer(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    if (!reserve(s,0,112,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (!asset_field(s,h+4,apply,replay_material)) return 0;
    return PopLoadBlock(s);
}
/* C1 sound-name indirection 0x4b7160: the field holds the address of a word that holds the alias name (-1:
 * that word and the name follow); C1 stores the snd_alias_list_t it finds by that name. */
static int sound_name_field(load_blocks_t *s,const uint8_t *field,int apply)
{
    const uint8_t *name;
    uint32_t ref=read32(field),slot=apply && s->c1 ? c1_slot(s,field) : 0,word;
    if (ref!=UINT32_MAX) { if (ref) c1_fix(s,apply,slot,ref,0,FIXUP_SOUND_NAME); return 1; }
    if (!(name=replay_bytes(s,1,4,4,apply))) return 0;
    word=s->last_reference;
    if (!string_field(s,name,apply)) return 0;
    c1_fix(s,apply,slot,word,4,FIXUP_SOUND_NAME);
    return 1;
}
/* C1 WeaponDef 0x4c2630. Operations retain loader order, especially the
 * interleaved accuracy graph names and point arrays. Script-string indices
 * remain serialized indices until the zone's runtime string pool is bound. */
static int replay_weapon(load_blocks_t *s,int apply,uint32_t *reference)
{
    enum { STRING,MODEL,EFFECT,MATERIAL,SOUND,SOUND_ARRAY,POINTS,PHYSICS,TRACER };
    static const unsigned fields[][3]={
        {0,4,STRING},{0x10,11,MODEL},{0x7c,144,STRING},
        {0x35c,2,EFFECT},{0x364,53,SOUND},{0x438,2,SOUND_ARRAY},
        {0x440,4,EFFECT},{0x450,2,MATERIAL},{0x52c,6,MATERIAL},
        {0x548,1,MATERIAL},{0x550,1,MATERIAL},
        {0x564,1,STRING},{0x56c,1,STRING},{0x57c,1,STRING},
        {0x684,4,MATERIAL},{0x758,1,PHYSICS},{0x7b0,1,MODEL},
        {0x7b8,2,EFFECT},{0x7c0,2,SOUND},{0x8cc,2,EFFECT},
        {0x8ec,1,EFFECT},{0x8f0,1,SOUND},{0x9a0,1,STRING},
        {0x9a8,1,POINTS},{0x9a4,1,STRING},{0x9ac,1,POINTS},
        {0x9fc,2,STRING},{0xa20,2,STRING},{0xaa0,2,STRING},
        {0xaa8,1,TRACER},{0xac4,1,SOUND},{0xac8,1,EFFECT},
        {0xacc,1,STRING},{0xadc,9,SOUND},{0xb00,2,SOUND},
        {0xb48,6,SOUND},{0xb78,1,STRING},{0xb84,2,STRING},
        {0xb90,3,MATERIAL},{0xbb4,2,POINTS}
    };
    const uint8_t *h=s->source+s->source_position,*names;
    unsigned i,j,k,offset,kind;
    uint32_t ref;
    if (!reserve(s,0,0xbfc,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    /* hide tags and the four notetrack maps hold script strings; the ammo, clip and shared-ammo-cap indices are
     * C1's registrations once a match registers the weapon */
    c1_note(s,apply,h+0x3c,0x40,FIXUP_SCRIPT_STRINGS);
    c1_note(s,apply,h+0x2bc,0x80,FIXUP_SCRIPT_STRINGS);
    c1_note(s,apply,h+0x568,4,FIXUP_RUNTIME);
    c1_note(s,apply,h+0x570,4,FIXUP_RUNTIME);
    c1_note(s,apply,h+0x580,4,FIXUP_RUNTIME);
    /* +0xb7c: C1 writes it when a match registers the weapon, on grenade-launcher weapons only (measured
     * 2026-10-28, hosted: m16_gl_upgraded_zm 0x47, gl_m16_upgraded_zm 0x46, m16_grenadier_reddot_so 0x0d; 0 as
     * loaded) -- the registered index of the alternate (launcher) weapon */
    c1_note(s,apply,h+0xb7c,4,FIXUP_RUNTIME);
    for (i=0;i<sizeof(fields)/sizeof(fields[0]);++i) for (j=0;j<fields[i][1];++j) {
        offset=fields[i][0]+4*j; kind=fields[i][2]; ref=read32(h+offset);
        switch (kind) {
        case STRING: if (!string_field(s,h+offset,apply)) return 0; break;
        case MODEL: if (!asset_field(s,h+offset,apply,replay_model)) return 0; break;
        case EFFECT: if (!asset_field(s,h+offset,apply,replay_effect)) return 0; break;
        case MATERIAL: if (!asset_field(s,h+offset,apply,replay_material)) return 0; break;
        case PHYSICS: if (!asset_field(s,h+offset,apply,replay_physics_map_asset)) return 0; break;
        case TRACER: if (!asset_field(s,h+offset,apply,replay_tracer)) return 0; break;
        case SOUND: if (!sound_name_field(s,h+offset,apply)) return 0; break;
        case SOUND_ARRAY:
            if (ref!=UINT32_MAX) { c1_pooled(s,apply,h+offset); break; }
            if (!(names=replay_bytes(s,31,4,4,apply))) return 0;
            c1_link(s,apply,h+offset);
            for (k=0;k<31;++k) if (!sound_name_field(s,names+4*k,apply)) return 0;
            break;
        case POINTS:
            if (!array_field(s,h+offset,read16(h+
                (offset==0x9a8 || offset==0xbb4 ? 0xbb0 : 0xbb2)),8,4,apply,NULL)) return 0;
            break;
        }
    }
    return PopLoadBlock(s);
}
/* C1 menu Statement 0x4c0260, entries 0x4c00c0 and nested statement
 * pointers 0x4c01c0. Statements stay in block 3, including recursion. */
static int replay_menu_expression(load_blocks_t *s,int apply,uint32_t *reference,unsigned depth)
{
    const uint8_t *h=s->source+s->source_position,*entries;
    uint32_t count,i,child;
    if (depth>=LOAD_STACK || !reserve(s,3,20,4,1,apply,reference)) return 0;
    if (read32(h+4)) {
        count=read32(h);
        if (count>INT32_MAX || !(entries=replay_bytes(s,count,12,4,apply))) return 0;
        for (i=0;i<count;++i) {
            const uint8_t *r=entries+12*i;
            if (!read32(r) || read32(r+8)!=UINT32_MAX) continue;
            if (read32(r+4)==2 && !replay_string(s,apply)) return 0;
            if (read32(r+4)==3 && !replay_menu_expression(s,apply,&child,depth+1)) return 0;
        }
    }
    return 1;
}
int ReplayMenuExpression(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=3 || !stream->source ||
        stream->source_position>stream->source_size) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_menu_expression(&plan,pass,&result,0)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
/* C1 menu event sets 0x4c0630, handler pointers 0x4c0570 and tagged
 * payloads 0x4c04b0. Nonzero handler/set pointers are inline, unlike
 * statement pointers, for which only -1 is inline. */
static int replay_menu_events(load_blocks_t *s,int apply,uint32_t *reference,unsigned depth)
{
    const uint8_t *h=s->source+s->source_position,*refs,*event,*payload;
    uint32_t count,i,child;
    unsigned type;
    if (depth>=LOAD_STACK || !reserve(s,3,8,4,1,apply,reference)) return 0;
    if (!read32(h+4)) return 1;
    count=read32(h);
    if (count>INT32_MAX || !(refs=replay_bytes(s,count,4,4,apply))) return 0;
    for (i=0;i<count;++i) if (read32(refs+4*i)) {
        if (!(event=replay_bytes(s,1,8,4,apply))) return 0;
        type=event[4];
        if (type==0) {
            if (!string_field(s,event,apply)) return 0;
        } else if (read32(event)) {
            if (type==2) {
                if (!replay_menu_events(s,apply,&child,depth+1)) return 0;
            } else if (type==1 || (type>=3 && type<=6)) {
                if (!(payload=replay_bytes(s,1,8,4,apply))) return 0;
                if (type!=1 && !string_field(s,payload,apply)) return 0;
                if (read32(payload+4)==UINT32_MAX &&
                    !replay_menu_expression(s,apply,&child,depth+1)) return 0;
                if (type==1 && read32(payload) &&
                    !replay_menu_events(s,apply,&child,depth+1)) return 0;
            }
        }
    }
    return 1;
}
int ReplayMenuEvents(load_blocks_t *stream,uint32_t *reference)
{
    load_blocks_t plan;
    uint32_t result;
    unsigned pass;
    if (!stream || !reference || stream->active!=3 || !stream->source ||
        stream->source_position>stream->source_size) return 0;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        if (!replay_menu_events(&plan,pass,&result,0)) return 0;
    }
    *stream=plan; *reference=result; return 1;
}
/* C1 key handlers 0x4c0980/0x4c0a30 and window dependencies 0x4c08c0. */
static int replay_menu_keys(load_blocks_t *s,int apply)
{
    const uint8_t *key;
    uint32_t child;
    unsigned depth;
    for (depth=0;depth<LOAD_STACK;++depth) {
        if (!(key=replay_bytes(s,1,12,4,apply))) return 0;
        if (read32(key+4) && !replay_menu_events(s,apply,&child,depth+1)) return 0;
        if (!read32(key+8)) return 1;
    }
    return 0;
}
static int replay_menu_window(load_blocks_t *s,const uint8_t *h,int apply)
{
    if (!string_field(s,h,apply)) return 0;
    if (!string_field(s,h+44,apply)) return 0;
    return asset_field(s,h+160,apply,replay_material);
}
/* C1 itemDef 0x4c0c90 and type-specific data 0x4c0a80. */
static int replay_menu_item(load_blocks_t *s,int apply)
{
    const uint8_t *h,*payload,*expressions;
    uint32_t child,count,type;
    unsigned i;
    if (!(h=replay_bytes(s,1,380,4,apply)) || !replay_menu_window(s,h,apply)) return 0;
    if (!string_field(s,h+0xe4,apply)) return 0;
    for (i=0xf0;i<=0x10c;i+=4)
        if (read32(h+i) && !replay_menu_events(s,apply,&child,0)) return 0;
    for (i=0x110;i<=0x114;i+=4)
        if (!string_field(s,h+i,apply)) return 0;
    if (read32(h+0x118) && !replay_menu_keys(s,apply)) return 0;
    for (i=0x11c;i<=0x120;i+=4)
        if (!string_field(s,h+i,apply)) return 0;
    if (!asset_field(s,h+0x128,apply,replay_sound_alias)) return 0;
    type=read32(h+0xb8);
    if (read32(h+0x134)) switch (type) {
    case 6:
        if (!(payload=replay_bytes(s,1,324,4,apply))) return 0;
        if (read32(payload+288) && !replay_menu_events(s,apply,&child,0)) return 0;
        if (!asset_field(s,payload+320,apply,replay_material)) return 0;
        break;
    case 0: case 4: case 9: case 10: case 11: case 14: case 16: case 17: case 18: case 22: case 23:
        if (!replay_bytes(s,1,32,4,apply)) return 0;
        break;
    case 12:
        if (!(payload=replay_bytes(s,1,392,4,apply))) return 0;
        for (i=0;i<64;++i) if (!string_field(s,payload+4*i,apply)) return 0;
        break;
    case 13:
        if (!string_field(s,h+0x134,apply)) return 0;
        break;
    case 20: if (!replay_bytes(s,1,28,4,apply)) return 0; break;
    case 21: if (!replay_bytes(s,1,4,4,apply)) return 0; break;
    default: break;
    }
    if (read32(h+0x140)) {
        count=read32(h+0x13c);
        if (count>INT32_MAX || !(expressions=replay_bytes(s,count,8,4,apply))) return 0;
        for (i=0;i<count;++i)
            if (read32(expressions+8*i+4)==UINT32_MAX && !replay_menu_expression(s,apply,&child,0)) return 0;
    }
    for (i=0x144;i<=0x150;i+=4)
        if (read32(h+i)==UINT32_MAX && !replay_menu_expression(s,apply,&child,0)) return 0;
    return 1;
}
/* C1 menuDef 0x4c12b0, item pointers 0x4c1230, MenuList 0x4c18c0. */
static int replay_menu(load_blocks_t *s,int apply,uint32_t *reference)
{
    static const unsigned events[]={0xd0,0xd8,0xd4,0xdc};
    const uint8_t *h=s->source+s->source_position,*items;
    uint32_t child,count;
    unsigned i;
    if (!reserve(s,0,400,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!replay_menu_window(s,h,apply)) return 0;
    if (!string_field(s,h+0xa4,apply)) return 0;
    for (i=0;i<4;++i) if (read32(h+events[i]) && !replay_menu_events(s,apply,&child,0)) return 0;
    if (read32(h+0xe0) && !replay_menu_keys(s,apply)) return 0;
    if (read32(h+0xe4)==UINT32_MAX && !replay_menu_expression(s,apply,&child,0)) return 0;
    for (i=0xe8;i<=0xec;i+=4) if (!string_field(s,h+i,apply)) return 0;
    for (i=0x104;i<=0x118;i+=4)
        if (read32(h+i)==UINT32_MAX && !replay_menu_expression(s,apply,&child,0)) return 0;
    if (read32(h+0x11c)) {
        count=read32(h+0xb0);
        if (count>INT32_MAX || !(items=replay_bytes(s,count,4,4,apply))) return 0;
        for (i=0;i<count;++i) if (read32(items+4*i) && !replay_menu_item(s,apply)) return 0;
    }
    return PopLoadBlock(s);
}
static int replay_menu_list(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*menus;
    uint32_t count,i;
    if (!reserve(s,0,12,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (read32(h+8)) {
        count=read32(h+4);
        if (count>INT32_MAX || !(menus=replay_bytes(s,count,4,4,apply))) return 0;
        for (i=0;i<count;++i) if (!asset_field(s,menus+4*i,apply,replay_menu)) return 0;
    }
    return PopLoadBlock(s);
}
/* C1 StructuredDataDefSet 0x4c61c0, definitions 0x4c6150/0x4c5fd0,
 * enums 0x4c5e30 and structs 0x4c5f50. Type references remain serialized. */
static int replay_structured_data(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*defs,*records,*members;
    uint32_t count,n,m,i,j,k;
    unsigned kind;
    if (!reserve(s,0,12,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (read32(h+8)) {
        count=read32(h+4);
        if (count>INT32_MAX || !(defs=replay_bytes(s,count,60,4,apply))) return 0;
        for (i=0;i<count;++i) {
            const uint8_t *d=defs+60*i;
            for (kind=0;kind<2;++kind) if (read32(d+12+8*kind)) {
                n=read32(d+8+8*kind);
                if (n>INT32_MAX || !(records=replay_bytes(s,n,kind ? 20 : 12,4,apply))) return 0;
                for (j=0;j<n;++j) {
                    const uint8_t *r=records+j*(kind ? 20 : 12);
                    if (!read32(r+(kind ? 4 : 8))) continue;
                    m=read32(r);
                    if (m>INT32_MAX || !(members=replay_bytes(s,m,kind ? 24 : 8,4,apply))) return 0;
                    for (k=0;k<m;++k)
                        if (!string_field(s,members+k*(kind ? 24 : 8),apply)) return 0;
                }
            }
            for (j=24;j<=32;j+=8)
                if (!present_field(s,d+j+4,read32(d+j),16,4,apply,NULL)) return 0;
            if (!present_field(s,d+56,read32(d+52),4,4,apply,NULL)) return 0;
        }
    }
    return PopLoadBlock(s);
}
/* C1 LeaderboardDef 0x4c64d0 and columns 0x4c6460/0x4c63c0. */
static int replay_leaderboard(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*columns;
    uint32_t count,i;
    if (!reserve(s,0,24,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (read32(h+20)) {
        count=read32(h+8);
        if (count>INT32_MAX || !(columns=replay_bytes(s,count,32,4,apply))) return 0;
        for (i=0;i<count;++i) {
            if (!string_field(s,columns+32*i,apply)) return 0;
            if (!string_field(s,columns+32*i+16,apply)) return 0;
        }
    }
    return PopLoadBlock(s);
}
/* C1 Font_s 0x4c8c00 and 24-byte glyph array 0x4c8bc0. */
static int replay_font(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    if (!reserve(s,0,24,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (!asset_field(s,h+12,apply,replay_material) ||
        !asset_field(s,h+16,apply,replay_material)) return 0;
    if (!array_field(s,h+20,read32(h+8),24,4,apply,NULL)) return 0;
    return PopLoadBlock(s);
}
/* C1 WeaponAttach 0x4c3dd0, field records 0x4c3d00. Types below 16
 * (signed comparison) and type 39 carry strings; other values are scalar. */
static int replay_weapon_attachment(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*fields;
    uint32_t count,i,type;
    if (!reserve(s,0,12,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (read32(h+8)) {
        count=read32(h+4);
        if (count>INT32_MAX || !(fields=replay_bytes(s,count,12,4,apply))) return 0;
        for (i=0;i<count;++i) {
            type=read32(fields+12*i+4);
            if ((type<16 || type>INT32_MAX || type==39) &&
                !string_field(s,fields+12*i+8,apply)) return 0;
        }
    }
    return PopLoadBlock(s);
}
/* C1 VehicleDef 0x4c4ec0 and embedded vehicle physics 0x4c4e10.
 * Runtime script-string binding and vehicle simulation are separate. */
static int replay_vehicle(load_blocks_t *s,int apply,uint32_t *reference)
{
    static const unsigned names[]={0,8,0xac};
    static const unsigned sounds[]={0x1e0,0x1e4,0x1e8,0x1ec,0x1f4,0x1fc,
        0x200,0x204,0x208,0x210,0x218,0x220,0x228,0x230};
    const uint8_t *h=s->source+s->source_position;
    unsigned i;
    if (!reserve(s,0,0x314,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    for (i=0;i<3;++i) if (!string_field(s,h+names[i],apply)) return 0;
    if (!asset_field(s,h+0xb0,apply,replay_preset_asset)) return 0;
    if (!string_field(s,h+0xb4,apply)) return 0;
    if (!string_field(s,h+0x190,apply)) return 0;
    if (!asset_field(s,h+0x194,apply,replay_weapon)) return 0;
    for (i=0;i<2;++i) if (!sound_name_field(s,h+0x1ac+4*i,apply)) return 0;
    for (i=0;i<2;++i) if (!asset_field(s,h+0x1d0+4*i,apply,replay_material)) return 0;
    for (i=0;i<sizeof(sounds)/sizeof(sounds[0]);++i)
        if (!sound_name_field(s,h+sounds[i],apply)) return 0;
    if (!string_field(s,h+0x238,apply)) return 0;
    for (i=0;i<31;++i) if (!sound_name_field(s,h+0x23c+4*i,apply)) return 0;
    return PopLoadBlock(s);
}
/* C1 FxImpactTable 0x4c2180/0x4c20c0: 16 records, each with
 * 31 ordinary and four flesh effect references, in serialized order. */
static int replay_impact_table(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*effects;
    unsigned i;
    if (!reserve(s,0,8,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (read32(h+4)) {
        if (!(effects=replay_bytes(s,16,140,4,apply))) return 0;
        for (i=0;i<560;++i)
            if (!asset_field(s,effects+4*i,apply,replay_effect)) return 0;
    }
    return PopLoadBlock(s);
}
/* C1 LocalizeEntry 0x4c1ea0: value followed by key, both strings. */
static int replay_localize(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    if (!reserve(s,0,8,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    if (!string_field(s,h+4,apply)) return 0;
    return PopLoadBlock(s);
}
/* C1 rawfile 0x4c58a0 and StringTable 0x4c5b90. */
static int replay_rawfile(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position;
    uint32_t compressed,length;
    if (!reserve(s,0,16,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    compressed=read32(h+4); length=read32(h+8);
    if (compressed>INT32_MAX || length>INT32_MAX) return 0;
    if (!present_field(s,h+12,compressed ? compressed : length+1,1,1,apply,NULL)) return 0;
    return PopLoadBlock(s);
}
static int replay_string_table(load_blocks_t *s,int apply,uint32_t *reference)
{
    const uint8_t *h=s->source+s->source_position,*cells;
    uint32_t rows,columns,i;
    uint64_t count;
    if (!reserve(s,0,16,4,1,apply,reference) || !PushLoadBlock(s,3)) return 0;
    if (!string_field(s,h,apply)) return 0;
    columns=read32(h+4); rows=read32(h+8); count=(uint64_t)columns*rows;
    if (columns>INT32_MAX || rows>INT32_MAX || count>INT32_MAX) return 0;
    if (read32(h+12)) {
        if (!(cells=replay_bytes(s,(uint32_t)count,8,4,apply))) return 0;
        for (i=0;i<count;++i)
            if (!string_field(s,cells+8*i,apply)) return 0;
    }
    return PopLoadBlock(s);
}
/* C1 DB_LoadXFile 0x4b5fc3..0x4b616b. The XAssetList itself is read
 * externally and does not advance a block cursor. */
int BeginZoneReplay(const zone_t *zone,load_blocks_t *out)
{
    load_blocks_t s={0};
    uint32_t capacities[8];
    const uint8_t *strings;
    unsigned i;
    if (!zone || !out || !zone->data || zone->size<56 || zone->size>ASSET_LIMIT ||
        zone->table_offset>zone->size || zone->asset_count>(zone->size-zone->table_offset)/16) return 0;
    for (i=0;i<8;++i) {
        if (zone->block_size[i]>0x10000000u) return 0;
        capacities[i]=(zone->block_size[i]+4095u)&~4095u;
    }
    if (!InitLoadBlocks(&s,zone->data,zone->size,capacities,0)) return 0;
    s.source_position=56;
    if (!PushLoadBlock(&s,3)) goto fail;
    if (read32(zone->data+44)) {
        if (!(strings=replay_bytes(&s,zone->string_count,4,4,1))) goto fail;
        for (i=0;i<zone->string_count;++i)
            if (read32(strings+4*i)==UINT32_MAX && !replay_string(&s,1)) goto fail;
    } else if (zone->string_count) goto fail;
    if (s.source_position!=zone->table_offset) goto fail;
    if (read32(zone->data+52)) {
        s.directory_reference=0x30000001u+(uint32_t)((s.position[3]+3)&~(size_t)3);
        if (!replay_bytes(&s,zone->asset_count,16,4,1)) goto fail;
    } else if (zone->asset_count) goto fail;
    *out=s; return 1;
fail:
    FreeLoadBlocks(&s); return 0;
}
static int compare_asset_alias(const void *a,const void *b)
{
    uint32_t x=((const asset_alias_t *)a)->reference;
    uint32_t y=((const asset_alias_t *)b)->reference;
    return (x>y)-(x<y);
}
static int collect_fixups,collect_image;
void SetCollectImageMap(int on) { collect_image=on!=0; }
int ReconstructImage(const load_blocks_t *s,size_t offset,uint8_t *dst,size_t length,
    const uint8_t *lost_source,size_t *lost)
{
    const image_map_t *m;
    size_t lo=0,hi,covered=0;
    if (!s || !(m=s->image) || m->failed || !dst) return 0;
    hi=m->count;
    while (lo<hi) { size_t mid=lo+(hi-lo)/2; if ((size_t)m->chunks[mid].source+m->chunks[mid].length<=offset) lo=mid+1; else hi=mid; }
    for (;lo<m->count && m->chunks[lo].source<offset+length;++lo) {
        const image_chunk_t *c=m->chunks+lo;
        size_t from=c->source>offset ? c->source : offset,to=(size_t)c->source+c->length;
        uint32_t block=(c->location-1)>>28,at=(c->location-1)&0x0fffffffu;
        const uint8_t *src;
        if (to>offset+length) to=offset+length;
        if (from>=to) continue;
        if (!c->location) {
            if (lost_source) memcpy(dst+(from-offset),lost_source+(from-offset),to-from);
            else memset(dst+(from-offset),0,to-from);
            if (lost) *lost+=to-from;
            covered+=to-from; continue;
        }
        at+=(uint32_t)(from-c->source);
        if (block==14) { if (!m->shadow || at+(to-from)>m->shadow_size) return 0; src=m->shadow+at; }
        else { if (block>=8 || !s->data[block] || at+(to-from)>s->capacity[block]) return 0; src=s->data[block]+at; }
        memcpy(dst+(from-offset),src,to-from); covered+=to-from;
    }
    return covered==length;
}
void SetCollectFixups(int on) { collect_fixups=on!=0; }
static int compare_fixup(const void *a,const void *b)
{
    uint32_t x=((const fixup_t *)a)->slot,y=((const fixup_t *)b)->slot;
    return (x>y)-(x<y);
}
static int compare_header(const void *a,const void *b)
{
    uintptr_t x=(uintptr_t)((const c1_header_t *)a)->source,y=(uintptr_t)((const c1_header_t *)b)->source;
    return (x>y)-(x<y);
}
const c1_header_t *C1Header(const load_blocks_t *s,const uint8_t *source)
{
    size_t lo=0,hi;
    if (!s || !s->c1 || !source) return NULL;
    hi=s->c1->header_count;
    while (lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if ((uintptr_t)s->c1->headers[mid].source<(uintptr_t)source) lo=mid+1; else hi=mid;
    }
    return lo<s->c1->header_count && s->c1->headers[lo].source==source ? s->c1->headers+lo : NULL;
}
uint8_t *OL_BlockAddress(const load_blocks_t *s,uint32_t reference)
{
    uint32_t block,offset;
    if (!s || !reference) return NULL;
    block=(reference-1)>>28; offset=(reference-1)&0x0fffffffu;
    if (block<8) return s->data[block] && offset<s->capacity[block] ? s->data[block]+offset : NULL;
    if (block==8 && s->c1 && offset<s->c1->arena_size) return s->c1->arena+offset;
    return NULL;
}
int FixupAt(const load_blocks_t *s,const void *addr,fixup_t *out)
{
    const uint8_t *p=addr;
    uint32_t reference=0;
    size_t lo=0,hi;
    unsigned i;
    if (!s || !s->c1 || !p) return 0;
    for (i=0;i<8 && !reference;++i)
        if (s->data[i] && p>=s->data[i] && p<s->data[i]+s->capacity[i]) reference=((uint32_t)i<<28)+(uint32_t)(p-s->data[i])+1;
    if (!reference && s->c1->arena && p>=s->c1->arena && p<s->c1->arena+s->c1->arena_size)
        reference=(8u<<28)+(uint32_t)(p-s->c1->arena)+1;
    if (!reference) return 0;
    hi=s->c1->fixup_count;
    while (lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if (s->c1->fixups[mid].slot<reference) lo=mid+1; else hi=mid;
    }
    if (lo==s->c1->fixup_count || s->c1->fixups[lo].slot!=reference) return 0;
    if (out) *out=s->c1->fixups[lo];
    return 1;
}
static int compare_placement(const void *a,const void *b)
{
    uint32_t x=((const c1_placement_t *)a)->target,y=((const c1_placement_t *)b)->target;
    return (x>y)-(x<y);
}
static int compare_target(const void *a,const void *b)
{
    uint32_t x=((const fixup_t *)a)->target,y=((const fixup_t *)b)->target;
    return (x>y)-(x<y);
}
/* The placements: every sized data link, before pooled references are sized. Duplicates (two links to the same
 * data) collapse; a placement inside another (a nested reservation) is dropped. */
static int build_placements(c1_state_t *c)
{
    size_t i,n=0;
    c1_placement_t *p;
    for (i=0;i<c->fixup_count;++i) if (c->fixups[i].kind==FIXUP_DATA && c->fixups[i].size) ++n;
    if (!n) return 1;
    if (!(p=malloc(n*sizeof(*p)))) return 0;
    for (i=0,n=0;i<c->fixup_count;++i) if (c->fixups[i].kind==FIXUP_DATA && c->fixups[i].size)
        p[n++]=(c1_placement_t){c->fixups[i].target,c->fixups[i].size};
    qsort(p,n,sizeof(*p),compare_placement);
    {
        size_t w=0;
        for (i=0;i<n;++i) {
            if (w && p[i].target<p[w-1].target+p[w-1].size) {
                uint32_t end=p[i].target+p[i].size;
                if (end>p[w-1].target+p[w-1].size) p[w-1].size=end-p[w-1].target;
                continue;
            }
            p[w++]=p[i];
        }
        n=w;
    }
    c->placements=p; c->placement_count=n; return 1;
}
const c1_placement_t *C1Placement(const load_blocks_t *s,uint32_t reference)
{
    size_t lo=0,hi;
    if (!s || !s->c1 || !s->c1->placement_count) return NULL;
    hi=s->c1->placement_count;
    while (lo<hi) { size_t mid=lo+(hi-lo)/2; if (s->c1->placements[mid].target<=reference) lo=mid+1; else hi=mid; }
    if (!lo) return NULL;
    --lo;
    return reference-s->c1->placements[lo].target<s->c1->placements[lo].size ? s->c1->placements+lo : NULL;
}
const fixup_t *FixupsIn(const load_blocks_t *s,uint32_t reference,uint32_t length,size_t *count)
{
    size_t lo=0,hi,n=0;
    if (count) *count=0;
    if (!s || !s->c1) return NULL;
    hi=s->c1->fixup_count;
    while (lo<hi) { size_t mid=lo+(hi-lo)/2; if (s->c1->fixups[mid].slot<reference) lo=mid+1; else hi=mid; }
    while (lo+n<s->c1->fixup_count && s->c1->fixups[lo+n].slot-reference<length) ++n;
    if (count) *count=n;
    return s->c1->fixups+lo;
}
/* A pooled data reference points at data an earlier field placed: give it that placement's size (placements
 * are disjoint reservations). */
static void size_pooled_fixups(c1_state_t *c)
{
    fixup_t *sized;
    size_t i,n=0;
    for (i=0;i<c->fixup_count;++i) if (c->fixups[i].kind==FIXUP_DATA && c->fixups[i].size) ++n;
    if (!n || !(sized=malloc(n*sizeof(*sized)))) return;
    for (i=0,n=0;i<c->fixup_count;++i) if (c->fixups[i].kind==FIXUP_DATA && c->fixups[i].size) sized[n++]=c->fixups[i];
    qsort(sized,n,sizeof(*sized),compare_target);
    for (i=0;i<c->fixup_count;++i) {
        fixup_t *f=c->fixups+i;
        size_t lo=0,hi=n;
        if (f->kind!=FIXUP_DATA || f->size) continue;
        while (lo<hi) { size_t mid=lo+(hi-lo)/2; if (sized[mid].target<f->target) lo=mid+1; else hi=mid; }
        if (lo<n && sized[lo].target==f->target) f->size=sized[lo].size;
        /* inside an earlier placement (a brush side's plane in the plane array): at most 64 bytes of the rest,
         * so a comparison sees the element without walking the array once per reference */
        else if (lo && f->target-sized[lo-1].target<sized[lo-1].size) {
            uint32_t rest=sized[lo-1].size-(f->target-sized[lo-1].target);
            f->size=rest<64 ? rest : 64;
        }
    }
    free(sized);
}
int ReplayZone(const zone_t *zone,load_blocks_t *out,unsigned *failed_asset)
{
    load_blocks_t pending={0};
    unsigned i;
    if (failed_asset) *failed_asset=UINT32_MAX;
    if (!out || !BeginZoneReplay(zone,&pending)) return 0;
    pending.collect_asset_aliases=1;
    if (collect_fixups && !(pending.c1=calloc(1,sizeof(*pending.c1)))) { FreeLoadBlocks(&pending); return 0; }
    if (collect_image && !(pending.image=calloc(1,sizeof(*pending.image)))) { FreeLoadBlocks(&pending); return 0; }
    for (i=0;i<zone->asset_count;++i) if (ReplayZoneAsset(zone,i,&pending)!=1) {
        if (failed_asset) *failed_asset=i;
        FreeLoadBlocks(&pending); return 0;
    }
    if (pending.source_position!=zone->size) {
        if (failed_asset) *failed_asset=zone->asset_count;
        FreeLoadBlocks(&pending); return 0;
    }
    pending.collect_asset_aliases=0;
    if (pending.image && !pending.image->failed) { /* trim the doubling growth */
        image_map_t *m=pending.image; void *p;
        if (m->shadow_size && (p=realloc(m->shadow,m->shadow_size))) { m->shadow=p; m->shadow_capacity=m->shadow_size; }
        if (m->count && (p=realloc(m->chunks,m->count*sizeof(*m->chunks)))) { m->chunks=p; m->capacity=m->count; }
    }
    if (pending.c1) {
        c1_state_t *c=pending.c1;
        free(c->chunks); c->chunks=NULL; c->chunk_count=c->chunk_capacity=0;
        if (c->failed) {
            free(c->fixups); free(c->arena); free(c->headers); free(c); pending.c1=NULL;
        } else {
            if (c->fixup_count>1) qsort(c->fixups,c->fixup_count,sizeof(*c->fixups),compare_fixup);
            if (c->header_count>1) qsort(c->headers,c->header_count,sizeof(*c->headers),compare_header);
            if (!build_placements(c)) c->failed=1;
            size_pooled_fixups(c);
        }
    }
    if (pending.asset_alias_count>1) qsort(pending.asset_aliases,pending.asset_alias_count,
        sizeof(*pending.asset_aliases),compare_asset_alias);
    *out=pending; return 1;
}
int ReplayZoneAsset(const zone_t *zone,unsigned index,load_blocks_t *stream)
{
    load_blocks_t plan;
    const uint8_t *entry;
    uint32_t type,extent,ref;
    unsigned pass;
    int (*loader)(load_blocks_t *,int,uint32_t *);
    if (!zone || !stream || !zone->data || index>=zone->asset_count ||
        stream->source!=zone->data || stream->source_size!=zone->size || stream->active!=3 ||
        zone->table_offset>zone->size || zone->asset_count>(zone->size-zone->table_offset)/16) return -1;
    entry=zone->data+zone->table_offset+16*index;
    type=read32(entry); extent=read32(entry+12); ref=read32(entry+8);
    if ((size_t)read32(entry+4)+40!=stream->source_position ||
        stream->source_position>zone->size || extent>zone->size-stream->source_position) return -1;
    switch (type) {
    case 0: loader=replay_preset_asset; break;
    case 1: loader=replay_physics_map_asset; break;
    case 2: loader=replay_animation; break;
    case 3: loader=replay_surfaces; break;
    case 4: loader=replay_model; break;
    case 5: loader=replay_material; break;
    case 6: case 7: loader=replay_shader_asset; break;
    case 8: loader=replay_declaration_asset; break;
    case 9: loader=replay_technique_set; break;
    case 10: loader=replay_image; break;
    case 11: loader=replay_sound_alias; break;
    case 12: loader=replay_sound_curve; break;
    case 13: loader=replay_loaded_sound; break;
    case 14: case 15: loader=replay_collision; break;
    case 16: loader=replay_common_world; break;
    case 18: loader=replay_game_world_mp; break;
    case 19: loader=replay_map_entities; break;
    case 20: loader=replay_effect_world; break;
    case 21: loader=replay_world; break;
    case 22: loader=replay_light_definition; break;
    case 24: loader=replay_font; break;
    case 25: loader=replay_menu_list; break;
    case 26: loader=replay_menu; break;
    case 27: loader=replay_localize; break;
    case 28: loader=replay_weapon; break;
    /* C1 0x4c8ed0 dispatch has no load operation for these asset families.
     * They may still occur as zero-payload directory records.  Preserve that
     * no-op behavior, but never use it to skip bytes from an unknown layout. */
    case 23: /* ui_map */
    case 29: /* sound-driver globals */
    case 32: /* aitype */
    case 33: /* mptype */
    case 34: /* character */
    case 35: /* xmodelalias */
        return extent==0 ? 1 : -1;
    case 30: loader=replay_effect; break;
    case 31: loader=replay_impact_table; break;
    case 36: loader=replay_rawfile; break;
    case 37: loader=replay_string_table; break;
    case 38: loader=replay_leaderboard; break;
    case 39: loader=replay_structured_data; break;
    case 40: loader=replay_tracer; break;
    case 41: loader=replay_vehicle; break;
    case 42: loader=replay_addon_map_entities; break;
    case 43: loader=replay_weapon_attachment; break;
    default: return 0;
    }
    if (type==4) {
        asset_t asset={4,zone->data+stream->source_position,extent};
        size_t consumed;
        if (!ReadXModelExtent(&asset,&consumed) || consumed!=extent) return -1;
    }
    if (ref<UINT32_MAX-1) return -1;
    for (pass=0;pass<2;++pass) {
        plan=*stream;
        /* Restrict reads to this asset even when later asset bytes exist. */
        plan.source_size=stream->source_position+extent;
        if (plan.c1) plan.c1->chunk_count=0;
        if (!replay_asset_slot(&plan,ref,0,stream->directory_reference+16*index+8,pass,loader,(int)type) ||
            plan.source_position!=stream->source_position+extent) return -1;
        if ((type<=2 || type==3 || type==4 || type==5 || type==11 || type==12 || type==13 || type==16 || type==18 || type==20 || type==28 || type==30 || type==39 || type==40 || type==41 || type==43 ||
            type==9 || type==10 || type==14 || type==15 || type==19 || type==21 || type==42) && !record_asset(&plan,type,stream->directory_reference+16*index+8,
            stream->source_position,pass)) return -1;
        if (!pass && plan.asset_alias_count>stream->asset_alias_capacity) {
            size_t capacity=plan.asset_alias_count;
            asset_alias_t *aliases;
            if (capacity>ASSET_LIMIT/sizeof(*aliases)) return -1;
            aliases=realloc(stream->asset_aliases,capacity*sizeof(*aliases));
            if (!aliases) return -1;
            stream->asset_aliases=aliases; stream->asset_alias_capacity=capacity;
        }
        if (!pass && plan.sound_record_count>stream->sound_record_capacity) {
            size_t capacity=plan.sound_record_count;
            sound_span_t *records;
            if(capacity>ASSET_LIMIT/sizeof(*records)) return -1;
            records=realloc(stream->sound_records,capacity*sizeof(*records));
            if(!records) return -1;
            stream->sound_records=records; stream->sound_record_capacity=capacity;
        }
        if (!pass && plan.glass_record_count>stream->glass_record_capacity) {
            size_t capacity=plan.glass_record_count;
            glass_span_t *records;
            if(capacity>ASSET_LIMIT/sizeof(*records)) return -1;
            records=realloc(stream->glass_records,capacity*sizeof(*records));
            if(!records) return -1;
            stream->glass_records=records; stream->glass_record_capacity=capacity;
        }
        if (!pass && plan.dynamic_record_count>stream->dynamic_record_capacity) {
            size_t capacity=plan.dynamic_record_count;
            dynamic_records_t *records;
            if(capacity>ASSET_LIMIT/sizeof(*records)) return -1;
            records=realloc(stream->dynamic_records,capacity*sizeof(*records));
            if(!records) return -1;
            stream->dynamic_records=records; stream->dynamic_record_capacity=capacity;
        }
    }
    plan.source_size=stream->source_size; *stream=plan; return 1;
}
static int resolve_asset_alias(const load_blocks_t *s,unsigned type,uint32_t reference,asset_t *out)
{
    size_t lo=0,hi;
    if (!s || !out || !reference || ((reference-1)>>28)!=3) return 0;
    hi=s->asset_alias_count;
    while (lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if (s->asset_aliases[mid].reference<reference) lo=mid+1;
        else hi=mid;
    }
    if (lo==s->asset_alias_count || s->asset_aliases[lo].reference!=reference ||
        s->asset_aliases[lo].asset.type!=type) return 0;
    *out=s->asset_aliases[lo].asset; return 1;
}
const char *LoadBlockString(const load_blocks_t *s,uint32_t reference)
{
    size_t offset,available;
    const uint8_t *p;
    if (!s || !reference || ((reference-1)>>28)!=3 || !s->data[3] ||
        s->written[3]>s->capacity[3]) return NULL;
    offset=(reference-1)&0xfffffff;
    if (offset>=s->written[3]) return NULL;
    p=s->data[3]+offset; available=s->written[3]-offset;
    return memchr(p,0,available) ? (const char *)p : NULL;
}
int ResolveMaterialAlias(const load_blocks_t *s,uint32_t ref,asset_t *out)
{ return resolve_asset_alias(s,5,ref,out); }
int ResolveWeaponAlias(const load_blocks_t *s,uint32_t ref,asset_t *out)
{ return resolve_asset_alias(s,28,ref,out); }
int ResolveSurfaceAlias(const load_blocks_t *s,uint32_t ref,asset_t *out)
{ return resolve_asset_alias(s,3,ref,out); }
int ResolveModelAlias(const load_blocks_t *s,uint32_t ref,asset_t *out)
{ return resolve_asset_alias(s,4,ref,out); }
int ResolveEffectAlias(const load_blocks_t *s,uint32_t ref,asset_t *out)
{ return resolve_asset_alias(s,30,ref,out); }
int ResolvePhysicsAlias(const load_blocks_t *s,unsigned type,uint32_t ref,asset_t *out)
{ return type<=1 && resolve_asset_alias(s,type,ref,out); }
/* Types 11 alias list, 12 curve and 13 loaded sound use directory entries and
 * the alias/sound-file pointer slots recorded during the complete replay. */
int ResolveSoundAlias(const load_blocks_t *s,unsigned type,uint32_t ref,asset_t *out)
{ return type>=11 && type<=13 && resolve_asset_alias(s,type,ref,out); }
int ResolveSoundSpan(const load_blocks_t *s,uint32_t ref,asset_t *out,unsigned *count)
{
    size_t i;
    if (!s || !out || !ref) return 0;
    for (i=0;i<s->sound_record_count;++i) if (s->sound_records[i].reference==ref) {
        *out=(asset_t){11,s->sound_records[i].source,s->sound_records[i].size};
        if (count) *count=s->sound_records[i].count;
        return 1;
    }
    return 0;
}
int ResolveDynamicRecords(const load_blocks_t *s,const asset_t *collision,
    unsigned category,dynamic_records_t *out)
{
    size_t i;
    if(!s || !collision || !out || category>1 ||
        (collision->type!=14 && collision->type!=15) || !collision->data) return 0;
    for(i=0;i<s->dynamic_record_count;++i) {
        const dynamic_records_t *records=s->dynamic_records+i;
        if(records->collision==collision->data && records->category==category) {
            uintptr_t offset;
            if((uintptr_t)records->records<(uintptr_t)collision->data) return 0;
            offset=(uintptr_t)records->records-(uintptr_t)collision->data;
            if(offset>collision->size || records->count>(collision->size-offset)/92) return 0;
            *out=*records; return 1;
        }
    }
    return 0;
}

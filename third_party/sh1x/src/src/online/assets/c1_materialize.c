/* C1-layout records with real pointers (feat/iw4-asset-layout, stage 2 step 1).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * A complete-zone replay with c1Fixups 1 keeps each asset's header as C1 copies it into its pool, with the
 * pointer words still serialized, and a fix-up for every pointer word C1's loader relocates. This file turns such
 * a header into the record C1 holds: the header and every placement it reaches are copied once, and each pointer
 * word is written with the address of the copy it names, as C1 writes the address of loaded data.
 *  - Data pointers point into the copied placement that holds their target (pooled references included); a
 *    placement without pointer or converted words is not copied: the pointer reaches the replayed bytes, which
 *    live as long as their archive.
 *  - Asset pointers are linked by name, as DB_LinkXAssetEntry links them: the first definition in the server's
 *    link order, so a ",name" reference and a later duplicate both reach the linked definition.
 *  - Names C1 resolves (sound alias names, FxEffectDefRef) are linked the same way; an absent alias is NULL.
 *  - Script-string arrays are converted, as C1 converts the zone's indices to its own handles: through a hook
 *    (the engine's SL handles), else to ids of this file's intern table (C1RecordString).
 *  - Bytes C1 itself writes at load stay as the zone has them (zero).
 * Records live until the mounted archive set changes (C1MaterializeReset from the registry). */
#include "c1_materialize.h"
#include "asset_host.h"
#include <stdlib.h>
#include <string.h>

/* C1 records hold 32-bit pointers: only the 32-bit server builds them. */
typedef char c1_pointer_size_check[sizeof(void *)==4 ? 1 : -1];

typedef struct {
    const load_blocks_t *blocks;
    uint32_t base;         /* encoded address of the placement or header copy */
    uint8_t *copy;
} entry_t;
typedef struct alloc_s { struct alloc_s *next; } alloc_t;

static entry_t *table;
static size_t table_capacity,table_count;
static alloc_t *allocations;
static size_t allocated_bytes;
static unsigned (*script_string_hook)(void *,const char *);
static void *script_string_context;

/* Without a hook, script strings are interned here: ids 1..65535 over every zone, text by C1RecordString. */
static char **interned;
static size_t interned_count,interned_capacity;
static unsigned *intern_index; /* open addressing over interned ids */
static size_t intern_index_capacity;
static unsigned intern_hash(const char *text)
{
    unsigned h=2166136261u;
    while(*text) { unsigned char c=(unsigned char)*text++; if(c>='A' && c<='Z') c+=32; h=(h^c)*16777619u; }
    return h;
}
static unsigned intern(void *unused,const char *text)
{
    size_t i,length;
    (void)unused;
    if(!text) return 0;
    if((interned_count+1)*2>intern_index_capacity) {
        size_t n=intern_index_capacity ? intern_index_capacity*2 : 4096,j;
        unsigned *grown=calloc(n,sizeof(*grown));
        if(!grown) return 0;
        for(j=0;j<interned_count;++j) {
            for(i=intern_hash(interned[j])&(n-1);grown[i];i=(i+1)&(n-1));
            grown[i]=(unsigned)j+1;
        }
        free(intern_index); intern_index=grown; intern_index_capacity=n;
    }
    for(i=intern_hash(text)&(intern_index_capacity-1);intern_index[i];i=(i+1)&(intern_index_capacity-1))
        if(!strcmp(interned[intern_index[i]-1],text)) return intern_index[i];
    if(interned_count>=65535) return 0;
    if(interned_count==interned_capacity) {
        size_t n=interned_capacity ? interned_capacity*2 : 1024;
        char **grown=realloc(interned,n*sizeof(*grown));
        if(!grown) return 0;
        interned=grown; interned_capacity=n;
    }
    length=strlen(text)+1;
    if(!(interned[interned_count]=malloc(length))) return 0;
    memcpy(interned[interned_count],text,length);
    intern_index[i]=(unsigned)++interned_count;
    return (unsigned)interned_count;
}
const char *C1RecordString(unsigned id)
{
    return id && id<=interned_count ? interned[id-1] : NULL;
}
static void free_interned(void)
{
    size_t i;
    for(i=0;i<interned_count;++i) free(interned[i]);
    free(interned); free(intern_index);
    interned=NULL; intern_index=NULL; interned_count=interned_capacity=intern_index_capacity=0;
}
void SetC1ScriptStringHook(unsigned (*hook)(void *,const char *),void *context)
{
    script_string_hook=hook; script_string_context=context;
    C1MaterializeReset();
}

void C1MaterializeReset(void)
{
    while(allocations) { alloc_t *a=allocations; allocations=a->next; free(a); }
    free(table); table=NULL; table_capacity=table_count=0; allocated_bytes=0;
    free_interned();
}

size_t C1MaterializedBytes(void) { return allocated_bytes; }

static size_t slot_of(const load_blocks_t *b,uint32_t base)
{
    uintptr_t h=((uintptr_t)b>>4)*2654435761u ^ base*40503u;
    return (size_t)h&(table_capacity-1);
}
static uint8_t *find(const load_blocks_t *b,uint32_t base)
{
    size_t i;
    if(!table_capacity) return NULL;
    for(i=slot_of(b,base);table[i].copy;i=(i+1)&(table_capacity-1))
        if(table[i].blocks==b && table[i].base==base) return table[i].copy;
    return NULL;
}
static int insert(const load_blocks_t *b,uint32_t base,uint8_t *copy)
{
    size_t i;
    if((table_count+1)*2>table_capacity) {
        size_t n=table_capacity ? table_capacity*2 : 4096,j;
        entry_t *old=table,*grown=calloc(n,sizeof(*grown));
        if(!grown) return 0;
        table=grown; table_capacity=n;
        for(j=0;old && j<n/2;++j) if(old[j].copy) {
            for(i=slot_of(old[j].blocks,old[j].base);table[i].copy;i=(i+1)&(n-1));
            table[i]=old[j];
        }
        free(old);
    }
    for(i=slot_of(b,base);table[i].copy;i=(i+1)&(table_capacity-1));
    table[i]=(entry_t){b,base,copy}; ++table_count; return 1;
}
static uint8_t *allocate(size_t size)
{
    alloc_t *a=malloc(sizeof(alloc_t)+(size ? size : 1)+15);
    uint8_t *p;
    if(!a) return NULL;
    a->next=allocations; allocations=a; allocated_bytes+=size;
    p=(uint8_t *)(((uintptr_t)(a+1)+15)&~(uintptr_t)15);
    return p;
}

/* Pending copies whose pointer words are still serialized. */
typedef struct { const load_blocks_t *blocks; uint32_t base,length; uint8_t *copy; } work_t;
static work_t *work;
static size_t work_count,work_capacity;
static int push_work(const load_blocks_t *b,uint32_t base,uint32_t length,uint8_t *copy)
{
    if(work_count==work_capacity) {
        size_t n=work_capacity ? work_capacity*2 : 256;
        work_t *w=realloc(work,n*sizeof(*w));
        if(!w) return 0;
        work=w; work_capacity=n;
    }
    work[work_count++]=(work_t){b,base,length,copy}; return 1;
}

/* A copy of the region [base, base+length) of b, queued for its pointers; NULL when it cannot be copied. */
static uint8_t *copy_region(const load_blocks_t *b,uint32_t base,uint32_t length)
{
    uint8_t *copy=find(b,base); const uint8_t *src;
    if(copy) return copy;
    if(!(src=OL_BlockAddress(b,base)) || !(copy=allocate(length))) return NULL;
    memcpy(copy,src,length);
    if(!insert(b,base,copy) || !push_work(b,base,length,copy)) return NULL;
    return copy;
}
/* The copied address of data at encoded reference ref of b. */
static void *data_address(const load_blocks_t *b,uint32_t ref)
{
    const c1_placement_t *p=C1Placement(b,ref);
    uint8_t *copy;
    if(!p) {
        /* data no link sized (pointer-free pooled data such as an unreferenced string tail): the replayed bytes */
        return OL_BlockAddress(b,ref);
    }
    /* data holding no pointer or converted word (strings, vertices, planes...) is used where the replay placed it:
     * no copy, and it lives as long as its archive, as the serialized readers' borrowed views do */
    {
        size_t n=0;
        FixupsIn(b,p->target,p->size,&n);
        if(!n) return OL_BlockAddress(b,ref);
    }
    copy=copy_region(b,p->target,p->size);
    return copy ? copy+(ref-p->target) : NULL;
}

static unsigned name_offset(unsigned type) { return type==10 ? 32 : type==27 ? 4 : 0; }
/* The name of a header copy at encoded reference header (its name field's data pointer). */
static const char *header_name(const load_blocks_t *b,uint32_t header,unsigned type)
{
    fixup_t f; const uint8_t *h=OL_BlockAddress(b,header);
    if(!h || !FixupAt(b,h+name_offset(type),&f) || f.kind!=FIXUP_DATA) return NULL;
    return (const char *)OL_BlockAddress(b,f.target);
}
static void *record(unsigned type,const char *name);
/* The linked definition of an asset header the zone holds (by its name), or this copy when it has no name. */
const void *C1PoolRecord(unsigned type,const char *name);
static void *linked_asset(const load_blocks_t *b,uint32_t header,uint32_t length,unsigned type)
{
    const char *name=type<0xff ? header_name(b,header,type) : NULL;
    if(name) {
        /* defined in a zone C1 holds (in-process): C1's own record, which has every pointer already */
        const void *pool=C1PoolRecord(type,*name==',' ? name+1 : name);
        void *linked;
        if(pool) return (void *)pool;
        linked=record(type,*name==',' ? name+1 : name);
        if(linked) return linked;
        if(*name==',') return NULL; /* a reference nothing defines: C1 would load a default asset */
    }
    return copy_region(b,header,length);
}

static void patch(const work_t *w)
{
    size_t n,i;
    const fixup_t *f=FixupsIn(w->blocks,w->base,w->length,&n);
    for(i=0;i<n;++i) {
        uint32_t off=f[i].slot-w->base,kind=f[i].kind&0xff,type=(f[i].kind>>8)&0xff;
        void *value=NULL;
        switch(kind) {
        case FIXUP_DATA: value=data_address(w->blocks,f[i].target); break;
        case FIXUP_ASSET: value=linked_asset(w->blocks,f[i].target,f[i].size,type); break;
        case FIXUP_ALIAS: {
            fixup_t holder; const uint8_t *word=OL_BlockAddress(w->blocks,f[i].target);
            if(word && FixupAt(w->blocks,word,&holder) && (holder.kind&0xff)==FIXUP_ASSET)
                value=linked_asset(w->blocks,holder.target,holder.size,(holder.kind>>8)&0xff);
            break;
        }
        case FIXUP_SOUND_NAME: {
            fixup_t n2; const uint8_t *word=OL_BlockAddress(w->blocks,f[i].target); const char *name;
            if(word && FixupAt(w->blocks,word,&n2) && (name=(const char *)OL_BlockAddress(w->blocks,n2.target)) &&
                *name && !(value=record(11,name))) {
                /* an alias the server does not mount (sound zones a dedicated server skips): a name-only
                 * snd_alias_list_t {name, aliases NULL, count 0}, as the weapon record keeps it */
                /* the name stays the replayed string, which lives as long as its archive */
                uint8_t *stub=allocate(12);
                if(stub) { memset(stub,0,12); memcpy(stub,&name,4); value=stub; }
            }
            break;
        }
        case 7: { /* FIXUP_ASSET_NAME */
            const char *name=(const char *)OL_BlockAddress(w->blocks,f[i].target);
            if(name && *name && !(value=record(type,name))) {
                /* a name the server does not define: a name-only header (zeroed, the name at its field) */
                uint8_t *stub=allocate(64);
                if(stub) { memset(stub,0,64); memcpy(stub+name_offset(type),&name,4); value=stub; }
            }
            break;
        }
        case FIXUP_SCRIPT_STRINGS:
            {
                unsigned (*convert)(void *,const char *)=script_string_hook ? script_string_hook : intern;
                uint32_t k;
                for(k=0;k+1<f[i].size && off+k+2<=w->length;k+=2) {
                    uint16_t index; const char *text;
                    memcpy(&index,w->copy+off+k,2);
                    text=BlocksScriptString(w->blocks,index);
                    index=(uint16_t)(text ? convert(script_string_context,text) : 0);
                    memcpy(w->copy+off+k,&index,2);
                }
            }
            continue;
        default: continue; /* FIXUP_RUNTIME: left as loaded */
        }
        if(off+4<=w->length) memcpy(w->copy+off,&value,4);
    }
}

/* Each (type, name) record once; a record under construction is already in the table, so cycles terminate. */
static void *record(unsigned type,const char *name)
{
    const load_blocks_t *b; uint32_t header,length;
    if(!name || !*name || !FindC1Definition(type,name,&b,&header,&length)) return NULL;
    return copy_region(b,header,length);
}

void *C1Record(unsigned type,const char *name)
{
    void *result=record(type,name);
    /* patch everything this lookup copied (patching may copy more) */
    while(work_count) { work_t w=work[--work_count]; patch(&w); }
    return result;
}

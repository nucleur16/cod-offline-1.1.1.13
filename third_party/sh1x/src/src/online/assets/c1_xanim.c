/* XAnimParts from a C1 record (feat/iw4-asset-layout, stage 2 step 3).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The retained parser (LoadMountedXAnim) and native builder read an XAnimParts as C1's loader reads it
 * (0x4b7a50): an inline stream. This writes that stream from the record C1 holds (88-byte header, real pointers),
 * every present pointer as -1 followed by its data in loader order, so the same parser and builder run on a C1
 * record. Script strings (bone names, notetracks) are renumbered into a local table built from their text. */
#include "c1_xanim.h"
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
        size_t c=o->capacity ? o->capacity : 4096; uint8_t *g;
        while(c<o->size+n) c*=2;
        if(!(g=realloc(o->data,c))) { o->failed=1; return at; }
        o->data=g; o->capacity=c;
    }
    if(src) memcpy(o->data+o->size,src,n); else memset(o->data+o->size,0,n);
    o->size+=n; return at;
}
static void mark(out_t *o,size_t at,int present)
{
    if(!o->failed) { uint32_t v=present ? UINT32_MAX : 0; memcpy(o->data+at,&v,4); }
}

void FreeC1XAnimStream(c1_xanim_stream_t *s)
{
    unsigned i;
    if(!s) return;
    for(i=0;i<s->string_count;++i) free((char *)s->strings[i].text);
    free(s->data); free(s->strings); memset(s,0,sizeof(*s));
}

/* Local script-string index of text (1-based; 0 is the empty slot), adding it. */
static int local_string(c1_xanim_stream_t *s,const char *text,uint16_t *index)
{
    unsigned i;
    if(!text) return 0;
    for(i=1;i<s->string_count;++i) if(!strcmp(s->strings[i].text,text)) { *index=(uint16_t)i; return 1; }
    if(!s->string_count) {
        if(!(s->strings=calloc(512,sizeof(*s->strings)))) return 0;
        s->string_count=1;
    }
    if(s->string_count>=512) return 0;
    {   /* owned: the record's strings go when the records are dropped */
        size_t n=strlen(text)+1; char *copy=malloc(n);
        if(!copy) return 0;
        memcpy(copy,text,n); s->strings[s->string_count].text=copy;
    }
    *index=(uint16_t)s->string_count++; return 1;
}

int SerializeC1XAnim(const uint8_t *record,const char *(*text)(void *,unsigned),void *context,
    c1_xanim_stream_t *out)
{
    static const unsigned pointer_offsets[6]={52,56,60,64,68,72},count_offsets[6]={4,6,8,32,10,12},widths[6]={1,2,4,2,1,4};
    c1_xanim_stream_t result={0}; out_t o={0}; size_t header,delta_at=0; unsigned i,j,index_width;
    const char *name; const uint8_t *bones,*notifies,*delta;
    if(!record || !text || !out || !(name=(const char *)ptr(record))) return 0;
    index_width=u16(record+14)<256 ? 1 : 2;
    header=emit(&o,record,88);
    mark(&o,header,1); emit(&o,name,strlen(name)+1);
    bones=ptr(record+48); notifies=ptr(record+80); delta=ptr(record+84);
    mark(&o,header+48,bones!=NULL);
    if(bones) for(i=0;i<record[26];++i) {
        uint16_t index;
        if(!local_string(&result,text(context,u16(bones+2*i)),&index)) goto fail;
        emit(&o,&index,2);
    }
    mark(&o,header+80,notifies!=NULL);
    if(notifies) for(i=0;i<record[27];++i) {
        uint8_t entry[8]; uint16_t index;
        memcpy(entry,notifies+8*i,8);
        if(!local_string(&result,text(context,u16(entry)),&index)) goto fail;
        memcpy(entry,&index,2); emit(&o,entry,8);
    }
    mark(&o,header+84,delta!=NULL);
    if(delta) {
        delta_at=emit(&o,delta,12);
        for(i=0;i<3;++i) {
            const uint8_t *track=ptr(delta+4*i); size_t track_at; unsigned count;
            mark(&o,delta_at+4*i,track!=NULL);
            if(!track) continue;
            count=u16(track);
            track_at=emit(&o,track,4);
            (void)track_at;
            if(!count) emit(&o,track+4,i==0 ? 12 : i==1 ? 4 : 8);
            else {
                unsigned stride=i==0 ? (track[2] ? 3 : 6) : i==1 ? 4 : 8;
                size_t meta=emit(&o,track+4,i==0 ? 28 : 4),frame_word=meta+(i==0 ? 24 : 0);
                const uint8_t *frames=ptr(track+4+(i==0 ? 24 : 0));
                emit(&o,track+4+(i==0 ? 28 : 4),(size_t)(count+1)*index_width);
                mark(&o,frame_word,frames!=NULL);
                if(frames) emit(&o,frames,(size_t)(count+1)*stride);
            }
        }
    }
    for(i=0;i<6;++i) {
        const uint8_t *data=ptr(record+pointer_offsets[i]);
        uint32_t count=i==3 ? u32(record+count_offsets[i]) : u16(record+count_offsets[i]);
        mark(&o,header+pointer_offsets[i],data!=NULL);
        if(data) emit(&o,data,(size_t)count*widths[i]);
    }
    {
        const uint8_t *indices=ptr(record+76);
        mark(&o,header+76,indices!=NULL);
        if(indices) emit(&o,indices,(size_t)u32(record+36)*index_width);
    }
    if(o.failed) goto fail;
    (void)j;
    result.data=o.data; result.size=o.size;
    *out=result; return 1;
fail:
    free(o.data); FreeC1XAnimStream(&result); return 0;
}

/* C1-layout rawfile and string-table records and the retained views built from them.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * The server holds each rawfile and string table as C1 holds it (xassets/c1_assets.h). The retained IW3
 * consumers (the script parser, _com_stringtable.asm, the CoDOL table readers) read a view built from that
 * record: the decompressed text with its length, or the char* cell array. The same view functions serve a
 * record from C1's own pools (stage 3), so dedicated and in-process servers read assets one way. */
#include <stdlib.h>
#include <string.h>
#include "assets.h"
#include "c1_records.h"

static uint32_t c1_u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }

int BuildC1RawFile(const char *name,const asset_t *asset,C1RawFile *out,uint8_t **storage)
{
    uint32_t compressed,length; size_t start=16,bytes; uint8_t *copy;
    if(!name || !asset || asset->type!=ASSET_RAWFILE || asset->size<16 || !out || !storage) return 0;
    compressed=c1_u32(asset->data+4); length=c1_u32(asset->data+8);
    if(compressed>ASSET_LIMIT || length>ASSET_LIMIT) return 0;
    if(c1_u32(asset->data)==UINT32_MAX) {
        const char *inline_name=(const char *)asset->data+16;
        const void *end=memchr(inline_name,0,asset->size-16);
        if(!end) return 0;
        start+=(size_t)((const char *)end-inline_name)+1;
    }
    bytes=compressed ? compressed : (size_t)length+1;
    if(start>asset->size || bytes!=asset->size-start) return 0;
    if(!compressed && asset->data[asset->size-1]) return 0;
    if(!(copy=malloc(bytes))) return 0;
    memcpy(copy,asset->data+start,bytes);
    out->name=name; out->compressedLen=(int)compressed; out->len=(int)length; out->buffer=(const char *)copy;
    *storage=copy; return 1;
}

int C1RawFileView(const C1RawFile *record,engine_rawfile_t *out)
{
    uint8_t *text; size_t decoded;
    if(!record || !out || !record->buffer || record->len<0 || record->compressedLen<0) return 0;
    if(record->compressedLen) {
        if(!OL_Inflate((const uint8_t *)record->buffer,(size_t)record->compressedLen,&text,&decoded,
                          (size_t)record->len,0) || decoded!=(size_t)record->len) return 0;
    } else {
        if(!(text=malloc((size_t)record->len+1))) return 0;
        memcpy(text,record->buffer,(size_t)record->len+1);
    }
    out->name=record->name; out->len=record->len; out->buffer=(const char *)text;
    return 1;
}

int C1StringTableView(const C1StringTable *record,engine_table_t *out)
{
    size_t count,i; const char **values=NULL;
    if(!record || !out || record->columnCount<0 || record->rowCount<0) return 0;
    count=(size_t)record->columnCount*(size_t)record->rowCount;
    if(count && (!record->values || !(values=calloc(count,sizeof(*values))))) return 0;
    for(i=0;i<count;++i) values[i]=record->values[i].string;
    out->name=record->name; out->columnCount=record->columnCount; out->rowCount=record->rowCount; out->values=values;
    return 1;
}

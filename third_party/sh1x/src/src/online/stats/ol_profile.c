#include "ol_profile.h"
#include "ol_stats.h"
static uint32_t word(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int ProfileRecord(const uint8_t *profile,size_t size,const structured_schema_t *schema,
    const structured_definition_t **definition,const uint8_t **record,size_t *capacity)
{
    const structured_definition_t *found=NULL;uint32_t i;
    if(!profile || size!=STATS_SIZE || !schema || !schema->definitions || !definition || !record || !capacity)return 0;
    for(i=0;i<schema->count;++i) {
        const structured_definition_t *d=schema->definitions+i;
        if(d->version!=word(profile+4) || d->checksum!=word(profile+8))continue;
        if(found || d->layout.record_size<8 || d->layout.record_size>STATS_SIZE-4)return 0;
        found=d;
    }
    if(!found)return 0;
    *definition=found;*record=profile+4;*capacity=STATS_SIZE-4;return 1;
}
int ClientProfileRecord(unsigned client,const structured_schema_t *schema,
    const structured_definition_t **definition,const uint8_t **record,size_t *capacity)
{ return ProfileRecord(ClientStats(client),STATS_SIZE,schema,definition,record,capacity); }
int ClientProfileScalar(unsigned client,const structured_schema_t *schema,
    const structured_path_t *path,size_t count,uint32_t *out)
{
    const structured_definition_t *d;const uint8_t *record;size_t capacity;
    structured_field_t field;
    if(!ClientProfileRecord(client,schema,&d,&record,&capacity) ||
       !ResolveStructuredField(d,path,count,&field))return 0;
    return StructuredReadScalar(&d->layout,field.type.kind,field.logical_index,record,capacity,out);
}

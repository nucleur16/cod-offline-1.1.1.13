#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#include "matchdata.h"
#include <string.h>
static client_matchdata_t match;
static struct {unsigned pending;uint32_t last_sent;} delivery[32];
void RequestClientMatchData(unsigned client)
{ if(client<32 && match.name[0]) {delivery[client].pending=1;delivery[client].last_sent=0;} }
void AcknowledgeClientMatchData(unsigned client)
{ if(client<32)memset(&delivery[client],0,sizeof(delivery[client])); }
int ClientMatchDataDue(unsigned client,uint32_t time)
{ return client<32 && delivery[client].pending && (int32_t)(time-delivery[client].last_sent)>=1000; }
void ClientMatchDataSent(unsigned client,uint32_t time)
{ if(client<32 && delivery[client].pending)delivery[client].last_sent=time; }
static int same_name(const char *a,const char *b)
{
    for(;;++a,++b) {
        unsigned char x=*a,y=*b;
        if(x>='A' && x<='Z')x+='a'-'A';
        if(y>='A' && y<='Z')y+='a'-'A';
        if(x!=y)return 0;
        if(!x)return 1;
    }
}
void ResetClientMatchData(void)
{
    FreeStructuredSchema(&match.schema);
    memset(&match,0,sizeof(match));
    memset(delivery,0,sizeof(delivery));
}
int SelectClientMatchData(const char *name)
{
    structured_schema_t schema={0};const structured_definition_t *d;
    size_t n;unsigned i;
    if(!name || !*name)return 0;
    for(n=0;n<sizeof(match.name) && name[n];++n) {}
    if(n==sizeof(match.name))return 0;
    if(match.name[0])return same_name(name,match.name);
    if(LoadMountedStructuredSchema(name,&schema)!=1) {
        FreeStructuredSchema(&schema);return 0;
    }
    if(!schema.count || !schema.definitions)goto fail;
    d=schema.definitions;
    if(d->layout.record_size<8 || d->layout.record_size>sizeof(match.record))goto fail;
    memcpy(match.name,name,n+1);match.schema=schema;
    for(i=0;i<4;++i) {
        match.record[i]=(uint8_t)(d->version>>(8*i));
        match.record[4+i]=(uint8_t)(d->checksum>>(8*i));
    }
    return 1;
fail:
    FreeStructuredSchema(&schema);return 0;
}
client_matchdata_t *ClientMatchData(void)
{ return match.name[0] ? &match : NULL; }

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
int PersistentClientMatchDataEmpty(void) {return resident_storage_zero(&match,sizeof(match)) && resident_storage_zero(delivery,sizeof(delivery));}

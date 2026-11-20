#include "ol_profile.h"
#include "ol_stats.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put(uint8_t *p,uint32_t v)
{ unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8)); }
static void upload(unsigned client,const uint8_t *p,unsigned count)
{
    unsigned i;ResetClientStats(client);
    for(i=0;i<count;++i) {
        size_t n=STATS_SIZE-i*STATS_CHUNK;
        if(n>STATS_CHUNK)n=STATS_CHUNK;
        assert(AcceptClientStats(client,i,p+i*STATS_CHUNK,n));
    }
}
int main(void)
{
    uint8_t data[STATS_SIZE]={0},map[4]={8,0,0,0};uint32_t value=123;
    structured_property_t prop={"experience",{0,0},0,{0,0}};
    structured_struct_t structure={1,12,0,1,&prop};
    structured_definition_t defs[2]={{0}};
    structured_schema_t schema={"mp/playerdata.def",2,defs};
    structured_path_t path={"experience",0};
    const structured_definition_t *selected=NULL;const uint8_t *record=NULL;size_t capacity=99;
    defs[1].version=220;defs[1].checksum=0xe4fd5218;defs[1].root=(structured_type_t){5,0};
    defs[1].struct_count=1;defs[1].structs=&structure;
    defs[1].layout=(structured_layout_t){map,4,12};
    put(data+4,220);put(data+8,0xe4fd5218);put(data+12,457750);
    upload(0,data,26);
    assert(!ClientProfileScalar(0,&schema,&path,1,&value) && value==123);
    upload(0,data,27);
    assert(ClientProfileRecord(0,&schema,&selected,&record,&capacity));
    assert(selected==defs+1 && record==ClientStats(0)+4 && capacity==STATS_SIZE-4);
    assert(ClientProfileScalar(0,&schema,&path,1,&value) && value==457750);
    put(data+12,999);upload(1,data,27);
    assert(ClientProfileScalar(1,&schema,&path,1,&value) && value==999);
    assert(ClientProfileScalar(0,&schema,&path,1,&value) && value==457750);
    defs[0]=defs[1];assert(!ClientProfileRecord(0,&schema,&selected,&record,&capacity));
    assert(selected==defs+1 && record==ClientStats(0)+4 && capacity==STATS_SIZE-4);
    memset(defs,0,sizeof(defs[0]));defs[1].checksum++;
    assert(!ClientProfileScalar(0,&schema,&path,1,&value) && value==457750);
    defs[1].checksum--;defs[1].layout.record_size=STATS_SIZE;
    assert(!ClientProfileScalar(0,&schema,&path,1,&value));
    ResetClientStats(0);ResetClientStats(1);
    puts("PASS: profile schema selection, upload prefix, isolation and atomic failures");
}

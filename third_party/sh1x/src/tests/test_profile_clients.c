#include "ol_profile.h"
#include "ol_stats.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    uint8_t data[STATS_SIZE]={0};data[4]=11;data[8]=1;
    structured_definition_t def={0};def.version=11;def.checksum=1;def.layout.record_size=8;
    structured_schema_t schema={"secure",1,&def};uint64_t uid=0;
    assert(!ClientSecureProfile(0,&uid) && uid==0);
    assert(AttachClientSecureProfile(0,UINT64_MAX,data,sizeof(data),&schema));
    const uint8_t *owned=ClientSecureProfile(0,&uid);assert(owned!=data && uid==UINT64_MAX);
    data[12]=55;assert(owned[12]==0);
    assert(AttachClientSecureProfile(1,2,data,sizeof(data),&schema));
    assert(ClientSecureProfile(1,&uid)[12]==55 && uid==2);
    data[4]=12;assert(!AttachClientSecureProfile(0,3,data,sizeof(data),&schema));
    assert(ClientSecureProfile(0,&uid)==owned && uid==UINT64_MAX);
    data[4]=11;assert(!AttachClientSecureProfile(32,3,data,sizeof(data),&schema));
    assert(!AttachClientSecureProfile(0,0,data,sizeof(data),&schema));
    assert(AttachClientSecureProfile(0,3,data,sizeof(data),&schema));
    assert(ClientSecureProfile(0,&uid)[12]==55 && uid==3);
    DetachClientSecureProfile(0);assert(!ClientSecureProfile(0,NULL));
    assert(ClientSecureProfile(1,NULL));ResetClientSecureProfiles();
    assert(!ClientSecureProfile(1,NULL));ResetClientSecureProfiles();
    puts("PASS: secure profile ownership, isolation, atomic replacement and connection reset");
}

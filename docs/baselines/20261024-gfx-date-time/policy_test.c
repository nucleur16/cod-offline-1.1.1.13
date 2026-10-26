#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include "gfx_date_time_policy.h"
int main(void){
 unsigned checks=0;uint64_t ticks=0;int32_t offset=0;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"failed line %u\n",__LINE__);return 1;}}while(0)
 double bad[]={NAN,INFINITY,-INFINITY,1e20,-11644473600001.0,910692730085478.0,1.25};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)CHECK(!gdt_filetime(bad[i],&ticks));
 CHECK(gdt_filetime(0,&ticks)&&ticks==UINT64_C(116444736000000000));
 CHECK(gdt_filetime(-1,&ticks)&&ticks==UINT64_C(116444735999990000));
 CHECK(gdt_filetime(-11644473600000.,&ticks)&&ticks==0);
 CHECK(gdt_filetime(910692730085477.,&ticks)&&ticks<=INT64_MAX);
 CHECK(!gdt_offset(0,1,&offset));
 CHECK(!gdt_offset(0,UINT64_C(900000000000),&offset));
 CHECK(!gdt_offset(UINT64_MAX,0,&offset));
 CHECK(!gdt_offset(0,UINT64_MAX,&offset));
 CHECK(gdt_offset(UINT64_C(500000000000),UINT64_C(248000000000),&offset)&&offset==-25200000);
 CHECK(gdt_offset(0,UINT64_C(396000000000),&offset)&&offset==39600000);
 CHECK(gdt_offset(0,UINT64_C(207000000000),&offset)&&offset==20700000);
 printf("Date portable policy: %u checks PASS\n",checks);return 0;
}


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
static BOOL Readable(const void *p,SIZE_T n){(void)p;(void)n;return FALSE;}
static void Log(const char *fmt,...){(void)fmt;}
static void WriteCode(void *p,const void *s,SIZE_T n){(void)p;(void)s;(void)n;}
#include "gfx_date_time.c"
typedef DWORD (WINAPI *EnumFn)(DWORD,PDYNAMIC_TIME_ZONE_INFORMATION);
static DYNAMIC_TIME_ZONE_INFORMATION selected;
static DWORD WINAPI selected_zone(PDYNAMIC_TIME_ZONE_INFORMATION p){*p=selected;return TIME_ZONE_ID_UNKNOWN;}
typedef struct Case {const WCHAR *zone;double epoch;int32_t offset;} Case;
static Case cases[]={
    {L"Pacific Standard Time",1768480496000.0,-28800000},
    {L"Pacific Standard Time",1784118896000.0,-25200000},
    {L"Pacific Standard Time",1792799822000.0,-25200000},
    {L"Pacific Standard Time",1772963999000.0,-28800000},
    {L"Pacific Standard Time",1772964000000.0,-25200000},
    {L"Pacific Standard Time",1793523599000.0,-25200000},
    {L"Pacific Standard Time",1793523600000.0,-28800000},
    {L"Pacific Standard Time",1775314799000.0,-25200000},
    {L"Pacific Standard Time",1775314800000.0,-25200000},
    {L"Pacific Standard Time",1791041399000.0,-25200000},
    {L"Pacific Standard Time",1791041400000.0,-25200000},
    {L"Pacific Standard Time",1767195000000.0,-28800000},
    {L"Pacific Standard Time",1798759800000.0,-28800000},
    {L"Pacific Standard Time",1142856000000.0,-28800000},
    {L"Pacific Standard Time",1174392000000.0,-25200000},
    {L"Pacific Standard Time",-14642704000.0,-25200000},
    {L"Pacific Standard Time",0.0,-28800000},
    {L"UTC",1768480496000.0,0},
    {L"UTC",1784118896000.0,0},
    {L"UTC",1792799822000.0,0},
    {L"UTC",1772963999000.0,0},
    {L"UTC",1772964000000.0,0},
    {L"UTC",1793523599000.0,0},
    {L"UTC",1793523600000.0,0},
    {L"UTC",1775314799000.0,0},
    {L"UTC",1775314800000.0,0},
    {L"UTC",1791041399000.0,0},
    {L"UTC",1791041400000.0,0},
    {L"UTC",1767195000000.0,0},
    {L"UTC",1798759800000.0,0},
    {L"UTC",1142856000000.0,0},
    {L"UTC",1174392000000.0,0},
    {L"UTC",-14642704000.0,0},
    {L"UTC",0.0,0},
    {L"Lord Howe Standard Time",1768480496000.0,39600000},
    {L"Lord Howe Standard Time",1784118896000.0,37800000},
    {L"Lord Howe Standard Time",1792799822000.0,39600000},
    {L"Lord Howe Standard Time",1772963999000.0,39600000},
    {L"Lord Howe Standard Time",1772964000000.0,39600000},
    {L"Lord Howe Standard Time",1793523599000.0,39600000},
    {L"Lord Howe Standard Time",1793523600000.0,39600000},
    {L"Lord Howe Standard Time",1775314799000.0,39600000},
    {L"Lord Howe Standard Time",1775314800000.0,37800000},
    {L"Lord Howe Standard Time",1791041399000.0,37800000},
    {L"Lord Howe Standard Time",1791041400000.0,39600000},
    {L"Lord Howe Standard Time",1767195000000.0,39600000},
    {L"Lord Howe Standard Time",1798759800000.0,39600000},
    {L"Lord Howe Standard Time",1142856000000.0,39600000},
    {L"Lord Howe Standard Time",1174392000000.0,39600000},
    {L"AUS Eastern Standard Time",1768480496000.0,39600000},
    {L"AUS Eastern Standard Time",1784118896000.0,36000000},
    {L"AUS Eastern Standard Time",1792799822000.0,39600000},
    {L"AUS Eastern Standard Time",1772963999000.0,39600000},
    {L"AUS Eastern Standard Time",1772964000000.0,39600000},
    {L"AUS Eastern Standard Time",1793523599000.0,39600000},
    {L"AUS Eastern Standard Time",1793523600000.0,39600000},
    {L"AUS Eastern Standard Time",1775314799000.0,39600000},
    {L"AUS Eastern Standard Time",1775314800000.0,39600000},
    {L"AUS Eastern Standard Time",1791041399000.0,36000000},
    {L"AUS Eastern Standard Time",1791041400000.0,36000000},
    {L"AUS Eastern Standard Time",1767195000000.0,39600000},
    {L"AUS Eastern Standard Time",1798759800000.0,39600000},
    {L"AUS Eastern Standard Time",1142856000000.0,39600000},
    {L"AUS Eastern Standard Time",1174392000000.0,39600000},
    {L"India Standard Time",1768480496000.0,19800000},
    {L"India Standard Time",1784118896000.0,19800000},
    {L"India Standard Time",1792799822000.0,19800000},
    {L"India Standard Time",1772963999000.0,19800000},
    {L"India Standard Time",1772964000000.0,19800000},
    {L"India Standard Time",1793523599000.0,19800000},
    {L"India Standard Time",1793523600000.0,19800000},
    {L"India Standard Time",1775314799000.0,19800000},
    {L"India Standard Time",1775314800000.0,19800000},
    {L"India Standard Time",1791041399000.0,19800000},
    {L"India Standard Time",1791041400000.0,19800000},
    {L"India Standard Time",1767195000000.0,19800000},
    {L"India Standard Time",1798759800000.0,19800000},
    {L"India Standard Time",1142856000000.0,19800000},
    {L"India Standard Time",1174392000000.0,19800000},
    {L"Nepal Standard Time",1768480496000.0,20700000},
    {L"Nepal Standard Time",1784118896000.0,20700000},
    {L"Nepal Standard Time",1792799822000.0,20700000},
    {L"Nepal Standard Time",1772963999000.0,20700000},
    {L"Nepal Standard Time",1772964000000.0,20700000},
    {L"Nepal Standard Time",1793523599000.0,20700000},
    {L"Nepal Standard Time",1793523600000.0,20700000},
    {L"Nepal Standard Time",1775314799000.0,20700000},
    {L"Nepal Standard Time",1775314800000.0,20700000},
    {L"Nepal Standard Time",1791041399000.0,20700000},
    {L"Nepal Standard Time",1791041400000.0,20700000},
    {L"Nepal Standard Time",1767195000000.0,20700000},
    {L"Nepal Standard Time",1798759800000.0,20700000},
    {L"Nepal Standard Time",1142856000000.0,20700000},
    {L"Nepal Standard Time",1174392000000.0,20700000},
};
int main(void){
 HMODULE k=GetModuleHandleA("kernel32.dll");
 EnumFn enumerate=(EnumFn)(void*)GetProcAddress(LoadLibraryA("advapi32.dll"),"EnumDynamicTimeZoneInformation");
 g_gdtToLocal=(GDTToLocalFn)(void*)GetProcAddress(k,"SystemTimeToTzSpecificLocalTimeEx");
 g_gdtGetDynamic=selected_zone;
 if(!enumerate||!g_gdtToLocal){puts("required historical Windows timezone APIs unavailable");return 2;}
 unsigned checks=0,failures=0;
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  int found=0;for(DWORD index=0;index<512;index++){
   DWORD status=enumerate(index,&selected);if(status==ERROR_NO_MORE_ITEMS)break;
   if(status!=ERROR_SUCCESS)return 3;
   if(!wcscmp(cases[i].zone,selected.TimeZoneKeyName)){found=1;break;}
  }
  if(!found){wprintf(L"missing installed Windows zone %ls\n",cases[i].zone);return 4;}
  int32_t actual=0x12345678;
  int success=GfxDateTargetOffset(cases[i].epoch,&actual);checks++;
  if(!success||actual!=cases[i].offset){
   wprintf(L"rule mismatch case%u %ls epoch%.0f expected%ld actual%ld success%d\n",i,cases[i].zone,cases[i].epoch,(long)cases[i].offset,(long)actual,success);failures++;
  }
 }
 /* Disabled-DST snapshot is an explicit local fixture selection; the system
  * timezone and clock are never set. Its actual Windows rule evaluation is used. */
 for(DWORD index=0;index<512;index++){
  if(enumerate(index,&selected)!=ERROR_SUCCESS)return 5;
  if(!wcscmp(selected.TimeZoneKeyName,L"Pacific Standard Time"))break;
 }
 selected.DynamicDaylightTimeDisabled=TRUE;
 int32_t offset=0;checks++;
 if(!GfxDateTargetOffset(1784118896000.0,&offset)||offset!=-28800000){printf("disabled DST mismatch %ld\n",(long)offset);failures++;}
 uint64_t ticks;int32_t delta;
 double invalid[]={NAN,INFINITY,-INFINITY,1e20,-11644473600001.0,910692730085478.0,1.25};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){checks++;if(gdt_filetime(invalid[i],&ticks))failures++;}
 checks++;if(!gdt_filetime(0.0,&ticks)||ticks!=UINT64_C(116444736000000000))failures++;
 checks++;if(!gdt_filetime(-1.0,&ticks)||ticks!=UINT64_C(116444735999990000))failures++;
 checks++;if(gdt_offset(0,1,&delta))failures++;
 checks++;if(gdt_offset(0,UINT64_C(900000000000),&delta))failures++;
 g_gdtToLocal=NULL;checks++;if(GfxDateTargetOffset(0.0,&offset))failures++;
 printf("actual Windows target-date rules: %u checks, %u failures\n",checks,failures);
 return failures?1:0;
}

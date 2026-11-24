/* Execute the production GSC method at engine boundaries; no fabricated owner. */
#include "vehicle_compass.h"
#include "vehicle_native.h"
#include "g_shared.h"
#include "g_scr_vehicle.h"
#include "entitystate_ext.h"
#include "script_minimap.h"
#include <cassert>
#include <cstddef>
static_assert(sizeof(es_ext_t)==64,"probe archive stride");
static_assert(offsetof(es_ext_t,vehicle_compass_material)==10,"probe selector offset");
#include <cstring>
#include <cstdio>
#include <strings.h>
gentity_t g_entities[MAX_GENTITIES];
extern "C" { scr_vehicle_s s_vehicles[8]; }
static vehicle_native_t definition,other;
static const char *second="littlebird_mp";
static char variant[32]="enemy";
static int errors,calls,materialCalls,params=1;
static unsigned captured;
extern "C" int Scr_GetNumParam(void){return params;}
extern "C" char *Scr_GetString(unsigned int i){return i?(char*)second:variant;}
extern "C" void Scr_Error(const char*){++errors;}
extern "C" void Scr_ObjectError(const char*){++errors;}
extern "C" void Scr_ParamError(int,const char*){++errors;}
int Q_stricmp(const char *a,const char *b){return strcasecmp(a,b);}
extern "C" const vehicle_native_t *NativeVehicleDefinition(int info){assert(info==4||info==5);return info==5?&other:&definition;}
extern "C" int InstallNativeVehicleRecord(const char *name){return !strcmp(name,"littlebird_mp")?5:-1;}
extern "C" unsigned RegisterMinimapIcon(const char *name){++materialCalls;return !strcmp(name,"hud_minimap_pavelow_red")?7:!strcmp(name,"compass_objpoint_littlebird_busy")?9:8;}
extern "C" int EntityStateVehicleCompassCaptured(entityState_t *es,unsigned material){assert(es==&g_entities[100].s);++calls;captured=material;return 1;}
int main(){
 gentity_t &e=g_entities[100];e.r.inuse=1;e.s.number=100;e.s.eType=ET_HELICOPTER;e.scr_vehicle=&s_vehicles[0];e.scr_vehicle->entNum=100;e.scr_vehicle->infoIdx=4;
 strcpy(definition.compass_icon[0],"hud_minimap_pavelow_green");strcpy(definition.compass_icon[1],"hud_minimap_pavelow_red");
 scr_entref_t ref={};ref.entnum=100;
 Scr_VehicleSetCompassIcon(ref);assert(!errors&&captured==7&&calls==1&&materialCalls==1);
 strcpy(variant,"friendly");Scr_VehicleSetCompassIcon(ref);assert(!errors&&captured==8&&calls==2);
 strcpy(variant,"");Scr_VehicleSetCompassIcon(ref);assert(!errors&&captured==0&&calls==3&&materialCalls==2);
 strcpy(variant,"bogus");Scr_VehicleSetCompassIcon(ref);assert(errors==1&&calls==3);
 strcpy(variant,"enemy");definition.compass_icon[1][0]=0;Scr_VehicleSetCompassIcon(ref);assert(errors==2&&calls==3);
 e.s.eType=ET_PLAYER;Scr_VehicleSetCompassIcon(ref);assert(errors==3&&calls==3);
 e.s.eType=ET_VEHICLE;strcpy(definition.compass_icon[1],"hud_minimap_pavelow_red");Scr_VehicleSetCompassIcon(ref);assert(errors==3&&captured==7&&calls==4);
 strcpy(other.compass_icon[1],"compass_objpoint_littlebird_busy");params=2;
 Scr_VehicleSetCompassIcon(ref);assert(errors==3&&captured==9&&calls==5&&e.scr_vehicle->infoIdx==4);
 second="no_such_vehicle";Scr_VehicleSetCompassIcon(ref);assert(errors==4&&captured==9&&calls==5);
 params=3;Scr_VehicleSetCompassIcon(ref);assert(errors==5&&calls==5);params=1;
 assert(e.s.clientNum==0);puts("PASS: actual vehicle compass method selects mounted hostile/friendly material, clear restores automatic, rejects invalid receiver/variant/missing material, owner unchanged");
}

/* Real C1 vehicle compass override: 6811bf reads entity+78; 6811e9 resolves
 * its fifteen-slot minimap-icon selector before the controller-team icon fallback.
 * No synthetic controller player, objective or compass marker is generated. */
#ifdef OL_SERVER
#include "vehicle_compass.h"
#include "vehicle_native.h"
#include "entitystate_ext.h"
#include "script_minimap.h"
#include "g_shared.h"
#include "g_scr_vehicle.h"
#include "scr_vm_functions.h"
#include "qcommon.h"
#include <string.h>
extern "C" scr_vehicle_s s_vehicles[8];

void Scr_VehicleSetCompassIcon(scr_entref_t ref)
{
    if(ref.classnum || ref.entnum>=ENTITYNUM_MAX_NORMAL) {
        Scr_ObjectError("vehicle compass icon requires a vehicle entity"); return;
    }
    gentity_t *e=&g_entities[ref.entnum];
    if(!e->r.inuse || (e->s.eType!=ET_HELICOPTER && e->s.eType!=ET_VEHICLE) || !e->scr_vehicle) {
        Scr_ObjectError("vehicle compass icon requires a live native vehicle"); return;
    }
    int count=Scr_GetNumParam();
    if(count<1 || count>2) { Scr_Error("codol_vehicleSetCompassIcon(enemy|friendly|empty [, vehicle definition])"); return; }
    const char *variant=Scr_GetString(0);
    unsigned material=0;
    if(*variant) {
        int which=!Q_stricmp(variant,"enemy") ? 1 : !Q_stricmp(variant,"friendly") ? 0 : -1;
        if(which<0) { Scr_ParamError(0,"compass variant must be enemy, friendly, or empty to restore automatic selection"); return; }
        uintptr_t address=(uintptr_t)e->scr_vehicle,base=(uintptr_t)s_vehicles;
        if(address<base || address-base>=sizeof(s_vehicles) ||
           (address-base)%sizeof(scr_vehicle_s) || e->scr_vehicle->entNum!=e->s.number) {
            Scr_ObjectError("vehicle compass icon has no native vehicle instance"); return;
        }
        /* Another mounted definition's icon (the Survival little bird flies
         * on cobra_mp for its turret arc and shows littlebird_mp's icon);
         * precacheVehicle installed it, or it is installed here. */
        int record=e->scr_vehicle->infoIdx;
        if(count>1) {
            record=InstallNativeVehicleRecord(Scr_GetString(1));
            if(record<0) { Scr_ParamError(1,"unknown vehicle definition"); return; }
        }
        const vehicle_native_t *def=NativeVehicleDefinition(record);
        if(!def || !def->compass_icon[which][0]) {
            Scr_Error("vehicle definition has no requested compass material"); return;
        }
        unsigned index=RegisterMinimapIcon(def->compass_icon[which]);
        if(!index || index>15) {
            Scr_Error("vehicle compass material could not be registered"); return;
        }
        material=(unsigned)index;
    }
    if(!EntityStateVehicleCompassCaptured(&e->s,material))
        Scr_Error("vehicle compass captured state unavailable");
}
#endif

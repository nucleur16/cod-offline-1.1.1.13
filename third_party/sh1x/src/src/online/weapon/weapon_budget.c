/* See weapon_budget.h for why "fall back to the stock weapon" is not
 * free and what it used to cost. */
#ifdef OL_SERVER
#include "weapon_budget.h"
#include "sv_weapons.h"
#include "usercmd.h"
#include <string.h>
/* One composed name can append its own definition and a retained alternate. */
#define NATIVE_WEAPON_HEADROOM 2u
extern int BG_GetNumWeapons(void);
extern int BG_FindWeaponIndexForName(const char *name);
int NativeWeaponRoom(void)
{
    int count=BG_GetNumWeapons();
    return count>0 &&
        (unsigned)count+NATIVE_WEAPON_HEADROOM<=WEAPON_SLOTS;
}
int WeaponAlreadyLive(const char *name)
{
    int engine;
    if(!name || !*name || !strcmp(name,"none")) return 0;
    engine=BG_FindWeaponIndexForName(name);
    return engine>0 && engine<BG_GetNumWeapons() && WeaponToWire((unsigned)engine)>0;
}
const char *SafeStockWeapon(const char *name)
{
    if(!name || !*name || !strcmp(name,"none")) return "none";
    if(WeaponAlreadyLive(name)) return name;
    return NativeWeaponRoom() ? name:"none";
}
#endif

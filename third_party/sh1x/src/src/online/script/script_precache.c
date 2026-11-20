/* Native precacheItem with mounted-zone awareness. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "script_precache.h"
#include "scr_vm_functions.h"
#include "assets.h"
#include "weapon_native.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include <string.h>

void OL_Scr_PrecacheItem(void)
{
    const char *name;
    if (Scr_GetNumParam() != 1) {
        Scr_Error("Usage: precacheItem( <weapon name> )");
        return;
    }
    name = Scr_GetString(0);
    /* Composed weapons ('*' names) and every zone-defined weapon take the
     * retained path, which registers the definition and its dependencies. */
    if (name && *name && name[0] != '*' && strcmp(name, "none") && strcmp(name, "defaultweapon_mp")) {
        const struct WeaponDef *weapon = NULL;
        int status = LoadMountedNullWeapon(name, &weapon);
        if (status == 0) status = LoadMountedNativeWeapon(name, LoadEngineWeaponCore, &weapon);
        if (status == 0) {
            ((void)0);
            return;
        }
    }
    Scr_PrecacheItem();
}

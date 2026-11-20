/* Local developer capability; no client command can change the build or environment.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "localtest.h"
#include "cvar.h"
#include "scr_vm.h"
#include "scr_vm_functions.h"
#include "server.h"
#include "entity.h"
#include "g_shared.h"
#include <stdlib.h>
#include <stddef.h>

int LocalTestBuild(void)
{
#ifdef LOCAL_TEST_BUILD
    /* Kept in the ELF for package/promotion rejection before any remote operation. */
    static volatile const char marker[] = "CODOL_LOCAL_TEST_ONLY_V1";
    return marker[0] == 'C';
#else
    return 0;
#endif
}

int LocalTestAllowed(void)
{
#ifdef LOCAL_TEST_BUILD
    char enabled[32], zone[32], gametype[32];
    const cvar_t *dedicated = Cvar_FindVar("dedicated");
    if (!LocalTestBuild()) return 0;
    Cvar_VariableStringBuffer("localTesting", enabled, sizeof(enabled));
    Cvar_VariableStringBuffer("sv_zoneType", zone, sizeof(zone));
    Cvar_VariableStringBuffer("g_gametype", gametype, sizeof(gametype));
    /* dedicated is a CVAR_INIT enum. Cvar_GetInt excludes CVAR_ENUM;
     * the engine consumes dedicated->integer itself (common.c Com_Frame).
     * Display text is "dedicated internet server", not the argument "2". */
    return LocalTestPolicy(getenv("CODOL_LOCAL_TEST"), enabled, zone, gametype,
                                dedicated && dedicated->type == CVAR_ENUM && dedicated->integer == 2 ? "2" : "");
#else
    return 0;
#endif
}

void Scr_LocalTestAllowed(void)
{
    Scr_AddBool(LocalTestAllowed());
}
#endif

/* C1's existing player HUD status-colour enable producer.
 * This does not draw, tint, time or animate a HUD element.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "script_client_status_colour.h"
#include "scr_vm_functions.h"
#include "server.h"
#include "clientstate_ext.h"

void Scr_EnableClientStatusColour(scr_entref_t ref)
{
    gentity_t *ent;
    if(ref.classnum || ref.entnum>=MAX_GENTITIES) {
        Scr_ObjectError("not an entity"); return;
    }
    ent=&g_entities[ref.entnum];
    if(!ent->client || ref.entnum>=MAX_CLIENTS) {
        Scr_ObjectError("entity is not a player"); return;
    }
    /* The explicit compatibility API exposes only the observed no-argument
     * operation. C1's original handler itself reads no parameters. */
    if(Scr_GetNumParam()!=0) {
        Scr_Error("codol_enableClientStatusColour takes no arguments"); return;
    }
    if(!EnableClientStatusColour(ref.entnum))
        Scr_Error("player HUD status storage is unavailable");
}
#endif

/* Trusted Quick party side, source-registered player method.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C1 ClientScr_Get/SetSessionTeam use TEAM_RED=axis and TEAM_BLUE=allies.
 * This method exposes the allocation rather than arbitrary userinfo or the
 * mutable script team. It has no setter and is scoped to the authenticated
 * UID/current Quick session. Retained stock getassignedteam() is global and
 * has no receiver; do not guess a client argument for that unrelated ABI.
 */
#ifdef OL_SERVER
#include "script_quick_party.h"
#include "quickplay.h"
#include "scr_vm_functions.h"
#include "server.h"

void Scr_QuickPartyTeam(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum>=MAX_CLIENTS ||
        !g_entities[ref.entnum].r.inuse || !g_entities[ref.entnum].client) {
        Scr_ObjectError("codol_quickpartyteam requires a player");
        return;
    }
    if (Scr_GetNumParam()!=0) {
        Scr_Error("codol_quickpartyteam requires no arguments");
        return;
    }
    Scr_AddInt((int)QuickPlayClientTeam(ref.entnum));
}
#endif

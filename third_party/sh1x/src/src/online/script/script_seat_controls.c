/* Coop seat controls: the stance permissions a seated rider is held with, and
 * a movement-state line for the opening ride's trace. Kept apart from
 * script_weapon_controls.c, which unit tests link on its own.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "sv_state.h"
#include "water.h"
#include <string.h>

/* <player> allowStand / allowCrouch / allowProne( <bool> ): the IW4 stance
 * permissions. C1 keeps them as pm_flags 0x800000 / 0x1000000 / 0x2000000;
 * its stance code reads them (0x412882 on foot, 0x412e42 while linked: a
 * denied request is rewritten to the nearest permitted stance and the stance
 * event names why), and src/online/player/pmove_c1.c mirrors both
 * paths. pm_flags publish to C1 unchanged, so the client's prediction
 * refuses the same change. Coop (the opening ride seats its riders with
 * them) and zombies (T5's perk drink and Shi No Numa's water clamp the same
 * stances); nothing in the multiplayer scripts calls these. A deny bit a
 * script sets while the wading water (water.c) holds it becomes the
 * script's, so leaving the water does not lift the script's lock. */
#define PMF_DENY_STAND  0x00800000
#define PMF_DENY_CROUCH 0x01000000
#define PMF_DENY_PRONE  0x02000000
static void stance_permission(scr_entref_t ref,int mask,const char *name)
{
    playerState_t *ps;
    const char *zone=ServerZoneType();
    if(ref.classnum || ref.entnum>=MAX_GENTITIES || !g_entities[ref.entnum].client) {
        Scr_ObjectError(va("%s requires a player",name)); return;
    }
    if(!zone || (strcmp(zone,"coop") && strcmp(zone,"zm"))) {
        Scr_Error(va("%s is available in coop and zombies matches only",name)); return;
    }
    ps=&g_entities[ref.entnum].client->ps;
    if(Scr_GetInt(0)) ps->pm_flags&=~mask;
    else { ps->pm_flags|=mask; WaterYieldBits(ps,(unsigned)mask); }
}
void Scr_AllowStand(scr_entref_t ref) { stance_permission(ref,PMF_DENY_STAND,"allowStand"); }
void Scr_AllowCrouch(scr_entref_t ref) { stance_permission(ref,PMF_DENY_CROUCH,"allowCrouch"); }
void Scr_AllowProne(scr_entref_t ref) { stance_permission(ref,PMF_DENY_PRONE,"allowProne"); }

/* <player> codol_moveStateText(): "pm_type T pm_flags 0x.. eFlags 0x..
 * otherFlags 0x.. viewheight V/target" for the coop ride trace, so a user
 * report about a rider's stance or link can be read from the server log. */
void Scr_MoveStateText(scr_entref_t ref)
{
    playerState_t *ps;
    float water_scale;
    int water;
    if(ref.classnum || ref.entnum>=MAX_GENTITIES || !g_entities[ref.entnum].client) {
        Scr_ObjectError("codol_moveStateText requires a player"); return;
    }
    ps=&g_entities[ref.entnum].client->ps;
    water=PlayerWaterLevel((unsigned)ref.entnum,&water_scale);
    Scr_AddString(va("pm_type %d pm_flags 0x%x eFlags 0x%x otherFlags 0x%x viewheight %.0f/%d "
        "speedscale %.3f water %d/%.3f",
        ps->pm_type,(unsigned)ps->pm_flags,(unsigned)ps->eFlags,(unsigned)ps->otherFlags,
        ps->viewHeightCurrent,ps->viewHeightTarget,ps->moveSpeedScaleMultiplier,
        water,water_scale));
}
#endif

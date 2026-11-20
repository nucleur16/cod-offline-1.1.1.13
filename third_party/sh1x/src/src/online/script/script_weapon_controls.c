#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "weapon_flags.h"
static void control(scr_entref_t ref,unsigned mask,int disabled)
{
    playerState_t *ps;
    if(ref.classnum || ref.entnum>=MAX_GENTITIES || !g_entities[ref.entnum].client) {
        Scr_ObjectError("Player control requires a player"); return;
    }
    ps=&g_entities[ref.entnum].client->ps;
    if(disabled) ps->weapFlags|=mask;
    else ps->weapFlags&=~mask;
}
void Scr_EnableOffhandWeapons(scr_entref_t ref) { control(ref,WEAP_OFFHAND_DISABLED,0); }
void Scr_DisableOffhandWeapons(scr_entref_t ref) { control(ref,WEAP_OFFHAND_DISABLED,1); }
void Scr_EnableWeaponSwitch(scr_entref_t ref) { control(ref,WEAP_SWITCH_DISABLED,0); }
void Scr_DisableWeaponSwitch(scr_entref_t ref) { control(ref,WEAP_SWITCH_DISABLED,1); }
void Scr_EnableUsability(scr_entref_t ref) { control(ref,USE_DISABLED,0); }
void Scr_DisableUsability(scr_entref_t ref) { control(ref,USE_DISABLED,1); }
/* <player> allowMelee( <bool> ), T5's AllowMelee. C1 registers it as the
 * player method at 0x51b040: Scr_GetInt(0) true clears, false sets pm_flags
 * 0x8000000, and PM_Weapon's melee check 0x41c07a refuses a melee while that
 * bit is set. pm_flags publish to C1 unchanged, so the client's prediction
 * and the retained melee arm (_bg_weapons.asm PM_Weapon) read the same bit. */
#define PMF_NO_MELEE 0x8000000
void Scr_AllowMelee(scr_entref_t ref)
{
    playerState_t *ps;
    if(ref.classnum || ref.entnum>=MAX_GENTITIES || !g_entities[ref.entnum].client) {
        Scr_ObjectError("Player control requires a player"); return;
    }
    ps=&g_entities[ref.entnum].client->ps;
    if(Scr_GetInt(0)) ps->pm_flags&=~PMF_NO_MELEE;
    else ps->pm_flags|=PMF_NO_MELEE;
}
#endif

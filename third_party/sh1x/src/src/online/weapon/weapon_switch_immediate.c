/* player switchToWeaponImmediate( <weapon> ), C1's immediate weapon switch.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C1 registers two script methods over helper 0x511010 with its immediate
 * flag set (0x511240, 0x511292; switchToWeapon and its twin pass 0, 0x511190
 * and 0x5111e2). The helper resolves the name, refuses a weapon the player
 * does not hold (0x40ab60 returns no equipped record -> false), and with the
 * flag sets ps->weapon to 0 and weapFlags 0x10000 (0x5110b1..0x5110c7) before
 * it sends the same 'a <weapon>' select command switchToWeapon sends
 * (0x5110d1..0x5110f2; G_SelectWeaponIndex here) and returns true.
 *
 * Weapon 0 and weapFlags 0x10000 both send PM_BeginWeaponChange down its
 * quick arm (0x4197d4..0x419800: weaponstate 3/4, no drop, no weapon-change
 * announcement); the bit stays until the raise finishes, where 0x41a0c0
 * (reached from weaponstates 1 and 2, 0x41d82b) clears it. The retained
 * PM_BeginWeaponChange and PM_Weapon (_bg_weapons.asm) carry both halves.
 * IW4 has the same method. */
#ifdef OL_SERVER
#include "q_shared.h"
#include "g_shared.h"
#include "scr_vm.h"
#include "bg_public.h"
#include "weapon_bits.h"
#include "weapon_flags.h"

extern int __cdecl G_GetWeaponIndexForName(const char *name);
extern void __cdecl G_SelectWeaponIndex(int clientNum, int weapon);

void PlayerCmd_SwitchToWeaponImmediate(scr_entref_t entref)
{
    gentity_t *ent = VM_GetGEntityForNum(entref);
    const char *name;
    int weapon;
    if(!ent || !ent->client) {
        Scr_ObjectError("not a player entity");
        return;
    }
    name = Scr_GetString(0);
    weapon = G_GetWeaponIndexForName(name);
    if(weapon <= 0) {
        Scr_ParamError(0, va("unknown weapon %s", name));
        Scr_AddBool(qfalse);
        return;
    }
    if(!WeaponBitsOwned(&ent->client->ps, (unsigned)weapon)) {
        Scr_AddBool(qfalse);
        return;
    }
    ent->client->ps.weapon = 0;
    ent->client->ps.weapFlags |= (int)WEAP_IMMEDIATE_SWITCH;
    G_SelectWeaponIndex(ent->s.number, weapon);
    Scr_AddBool(qtrue);
}
#endif

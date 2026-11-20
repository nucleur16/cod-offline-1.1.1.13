#ifdef OL_SERVER
#include "playerstate_ext.h"
/* The whole-game input reset.
 *
 * It lives apart from game.c because it has to reach subsystems in the
 * player, script and vehicle areas. game.c owns the input state itself
 * and must stay linkable on its own, so it exposes ResetGameInputCore
 * and this file adds the rest on top. */
#include "server.h"
#include "game.h"
#include "spectate_defaults.h"
#include "weapon_input.h"
#include "script_usehint.h"
#include "clientstate_ext.h"
#include "scoreboard_stats.h"
#include "weapon_state.h"
#include "item_ammo.h"
#include "script_vision.h"
#include "script_playeruse.h"
#include "script_player_builtins.h"
#include "script_vehicle_builtins.h"
#include "hud_icon_overrides.h"
#include "field_orders.h"
#include "player_appearance.h"

void ResetGameInput(void)
{
    unsigned client;
    ResetGameInputCore();
    for(client=0;client<MAX_CLIENTS;++client) ResetWeaponInput(client);
    ResetWeaponStates();
    ResetWeaponWireIndexReports();
    ResetAllScoreStats();
    ResetAllSpectateDefaults();
    ClearCapturedClientAbilities();
    ResetClientCharacters();
    ForceUseHint_ResetAll();
    PlayerUseResetAll();
    ResetAllPlayerVisionSets();
    ResetPlayerBuiltinsAll();
    PlayerRemoteView_ResetAll();
    VehicleTurretControl_Reset();
    TurretMinimap_ResetAll();
    ResetAllHudIconOverrides();
}
void ResetClientInput(unsigned client)
{
    PlayerStateExtClearClientPoison(client);
    ResetClientInputCore(client);
    ResetWeaponInput(client);
    ResetScoreStats(client);
    ResetSpectateDefaults(client);
    ResetClientWeaponState(client);
    ClearPublishedClientAbilities(client);
    ClearClientCharacter(client);
    ForceUseHint_ResetClient((int)client);
    PlayerUseResetClient((int)client);
    /* A connecting or leaving slot drops the per-player vision records with the
     * rest of the client input state, so a reused slot cannot inherit them. */
    ResetPlayerVisionSets(client);
    ResetPlayerBuiltinsClient(client);
    PlayerRemoteView_ResetClient((int)client);
    VehicleTurretControl_ClientLeft((int)client);
    /* A fresh or leaving client slot: both halves of the weapon HUD icon
     * overrides go, the script-owned array and the copy last published into the
     * live playerState. In IW4 and C1 both halves are gclient memory that a new
     * connection starts zeroed. Captured and archived states keep theirs. */
    ResetClientHudIconOverrides(client);
    /* C1's field order words are gclient memory too: a new connection starts
     * with none. A respawn keeps them (the script owns the order for the
     * whole match and re-publishes nothing on spawn). */
    ResetClientFieldOrders(client);
}
#endif

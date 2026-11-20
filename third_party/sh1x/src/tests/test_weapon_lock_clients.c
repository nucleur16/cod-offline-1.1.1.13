#include "weapon_lock.h"
#include "game.h"
#include "qcommon_io.h"
#include "action_slots.h"
#include "clientstate_ext.h"
#include "hud_icon_overrides.h"
#include "item_ammo.h"
#include "killstreak_hud.h"
#include "lookat.h"
#include "perk_runtime.h"
#include "perks.h"
#include "player_appearance.h"
#include "pmove_c1.h"
#include "radar_types.h"
#include "scoreboard_stats.h"
#include "script_player_builtins.h"
#include "script_playeruse.h"
#include "script_usehint.h"
#include "script_vehicle_builtins.h"
#include "script_vision.h"
#include "spectate_defaults.h"
#include "weapon_input.h"
#include "weapon_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* This harness only uses the client-indexed live slots, so it binds no
 * playerState storage; the definitions keep the resolver linkable without
 * dragging in the server globals. */
const void *PlayerStateExtLiveBase(unsigned *count){if(count)*count=0;return NULL;}
const void *PlayerStateExtSnapshotBase(unsigned *count){if(count)*count=0;return NULL;}
const void *PlayerStateExtArchiveBase(unsigned *count){if(count)*count=0;return NULL;}
/* playerstate_ext.c reports untracked storage through this hook. */
void Com_PrintWarning(conChannel_t channel,const char *fmt,...){(void)channel;(void)fmt;}
/* The fixture links the production aggregate reset and its core. These
 * adapters are the unrelated reset owners; they have no access to the lock
 * under test. Header imports above keep their signatures compiler-checked. */
#define RESET_CLIENT(fn) void fn(unsigned client) { (void)client; }
#define RESET_SLOT(fn) void fn(int slot) { (void)slot; }
#define RESET_ALL(fn) void fn(void) {}
RESET_CLIENT(ResetPerkRuntime)
void ClearPerks(perk_state_t *perks) { if(perks) memset(perks, 0, sizeof(*perks)); }
RESET_CLIENT(ResetClientActionSlotState)
RESET_CLIENT(ResetClientKillstreakHud)
RESET_CLIENT(ResetLookAtClientState)
RESET_CLIENT(PmoveC1ResetClient)
RESET_ALL(PmoveC1ResetAll)
RESET_CLIENT(ResetWeaponInput)
RESET_ALL(ResetWeaponStates)
RESET_ALL(ResetWeaponWireIndexReports)
RESET_ALL(ResetAllScoreStats)
RESET_ALL(ResetAllSpectateDefaults)
RESET_ALL(ClearCapturedClientAbilities)
RESET_ALL(ResetClientCharacters)
RESET_ALL(ForceUseHint_ResetAll)
RESET_ALL(PlayerUseResetAll)
RESET_ALL(ResetAllPlayerVisionSets)
RESET_ALL(ResetPlayerBuiltinsAll)
RESET_ALL(PlayerRemoteView_ResetAll)
RESET_ALL(VehicleTurretControl_Reset)
RESET_ALL(TurretMinimap_ResetAll)
RESET_ALL(ResetAllHudIconOverrides)
RESET_CLIENT(ResetScoreStats)
RESET_CLIENT(ResetSpectateDefaults)
RESET_CLIENT(ResetClientWeaponState)
RESET_CLIENT(ClearPublishedClientAbilities)
RESET_CLIENT(ClearClientCharacter)
RESET_SLOT(ForceUseHint_ResetClient)
RESET_SLOT(PlayerUseResetClient)
RESET_CLIENT(ResetPlayerVisionSets)
RESET_CLIENT(ResetPlayerBuiltinsClient)
RESET_SLOT(PlayerRemoteView_ResetClient)
RESET_SLOT(VehicleTurretControl_ClientLeft)
RESET_CLIENT(ResetClientHudIconOverrides)
RESET_CLIENT(ResetClientFieldOrders)
RESET_CLIENT(ResetClientRadarTypes)
RESET_ALL(ResetAllRadarTypes)
#undef RESET_CLIENT
#undef RESET_SLOT
#undef RESET_ALL
int main(void)
{
    weapon_lock_t *first=ClientWeaponLock(0),*last=ClientWeaponLock(31);
    assert(first && last && first!=last && first->entity==2047 && last->entity==2047);
    assert(!ClientWeaponLock(32));
    assert(SetWeaponLock(first,10,NULL,1,NULL,0));
    assert(SetWeaponLock(last,11,NULL,1,NULL,1));
    ResetClientInput(0);
    assert(!first->flags && first->entity==2047 && last->entity==11 && last->flags==7);
    ResetGameInput();
    for(unsigned i=0;i<32;++i) {
        weapon_lock_t *s=ClientWeaponLock(i);
        assert(s && !s->flags && s->entity==2047);
    }
    puts("PASS: native client input reset and map reset clear independent weapon locks");
}

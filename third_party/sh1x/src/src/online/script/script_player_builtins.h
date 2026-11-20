/* Native CoDOL player builtins. SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Script surface (fifteen player methods and one function) plus the native
 * state carriers and wire producers they need:
 *
 *   <player> GetThirdPersonCrosshairOffset()          -> float
 *   <player> GetRestedTime()                          -> int seconds
 *   <player> IsUsingOnlineDataOffline()               -> int
 *   <player> PredictStreamPos( <origin> [, <angles>] )
 *   <player> PlayerForceDeathAnim( <inflictor>, <mod>, <weapon>, <hitloc>, <dir> ) -> int
 *   <player> StunPlayer( <seconds> )
 *   <player> ThermalVisionOn() / ThermalVisionOff()
 *   <player> ThermalVisionFOFOverlayOn() / ThermalVisionFOFOverlayOff()
 *   <player> SetCardIcon( <int> ) / SetCardTitle( <int> ) / SetCardNameplate( <int> )
 *   <player> DropScavengerBag( <weapon name> )        -> entity | undefined
 *   <player> KC_RegWeaponForFXRemoval( <weapon name> )
 *   PrecacheLeaderboards( <names> [, "replace"] )
 *
 * Provenance is in script_player_builtins.c, per builtin. Every method
 * validates its receiver the way IW4 0x18835d8-stride handlers and C1
 * 0x220cfb0-stride handlers do: a non-entity reference raises "not an entity"
 * and an entity without a client raises "entity %i is not a player".
 */
#ifndef SCRIPT_PLAYER_BUILTINS_H
#define SCRIPT_PLAYER_BUILTINS_H
#include "scr_vm.h"
#include "player.h"
#include "entity.h"
#include "playerstate.h"
#include "clients.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif

/* Script entry points. */
void Scr_GetThirdPersonCrosshairOffset(scr_entref_t ref);
void Scr_GetRestedTime(scr_entref_t ref);
void Scr_IsUsingOnlineDataOffline(scr_entref_t ref);
void Scr_PredictStreamPos(scr_entref_t ref);
void Scr_PlayerForceDeathAnim(scr_entref_t ref);
void Scr_StunPlayer(scr_entref_t ref);
void Scr_ThermalVisionOn(scr_entref_t ref);
void Scr_ThermalVisionOff(scr_entref_t ref);
void Scr_ThermalVisionFOFOverlayOn(scr_entref_t ref);
void Scr_ThermalVisionFOFOverlayOff(scr_entref_t ref);
void Scr_SetCardIcon(scr_entref_t ref);
void Scr_SetCardTitle(scr_entref_t ref);
void Scr_SetCardNameplate(scr_entref_t ref);
void Scr_SetCardBackground(scr_entref_t ref);
void Scr_DropScavengerBag(scr_entref_t ref);
void Scr_KC_RegWeaponForFXRemoval(scr_entref_t ref);
void Scr_PrecacheLeaderboards(void);
void Scr_SetRearViewRenderEnabled(scr_entref_t ref);
/* disableWeaponPickup / enableWeaponPickup (ps.weapFlags 0x100000) and the
 * Touch_Item weapon-path predicate (1 = pickup allowed). */
void Scr_DisableWeaponPickup(scr_entref_t ref);
void Scr_EnableWeaponPickup(scr_entref_t ref);
int WeaponPickupAllowed(const struct gentity_s *player);

/* Thermal vision: the two retained ps->otherFlags bits both references use
 * (IW4 0x5f03f0/0x5f04d0, C1 0x53c936/0x53ca76: ps+0x10 bits 8 and 0x10).
 * They travel with the playerState, survive the five-bit otherFlags archive
 * codec, and are cleared by ClientSpawn like every other playerState bit.
 * The overlay publishes them into C1's 26-bit flags word at wire offset 0x010,
 * read-modify-write of exactly these two bits; call it beside
 * OverlayEngineRadar in engine_state.c overlay_player. */
#define OTHERFLAG_THERMAL      0x8
#define OTHERFLAG_THERMAL_FOF  0x10
int OverlayEngineThermal(player_fields_t *wire, const playerState_t *ps);

/* Stun: C1 ps+0x3338 / IW4 ps+0x3118 stunTime, level.time + milliseconds,
 * carried in ps_ext_t (stun_time, stun_known) so archive and spectator
 * copies keep it. The wire field is C1 playerState 0x3338 (player_fields.inc
 * entry {0x3338,-97,0}). The gate is the C1/IW4 PM_Weapon test
 * "stunTime >= cmd.serverTime" (C1 0x41d720..0x41d72b, IW4 0x5782cd..0x5782db). */
int PlayerStunTime(const playerState_t *ps);
int PlayerStunned(const playerState_t *ps, int serverTime);
int OverlayEngineStun(player_fields_t *wire, const playerState_t *ps);

/* C1 session card table row indices; see authored HUD consumer evidence in
 * PLAYERCARD_CATALOGUE_20260915.md. All four fields are ten bits. */
#define CARD_WIRE_ICON       0x0ac
#define CARD_WIRE_TITLE      0x0b0
#define CARD_WIRE_BACKGROUND 0x0b4
#define CARD_WIRE_NAMEPLATE  0x0b8
#define CARD_INDEX_LIMIT    0x400
int OverlayClientCards(client_state_t *wire);
void ResetPlayerCards(unsigned client);
void ResetAllPlayerCards(void);

/* Scavenger bag: the touch handler of the bag entity. Fires "scavenger" on the
 * bag with the toucher as the parameter, then frees the bag, for a living
 * player who holds the scavenger ability; everyone else passes through. The
 * bag is dropped with the retained Drop_Item and re-handled with
 * ENT_HANDLER_SCAVENGER_BAG (see the parent notes in the .c). */
#define SCAVENGER_BAG_HANDLER 0x18
#define SCAVENGER_ABILITY_ID 2
void ScavengerBagTouch(gentity_t *bag, gentity_t *other, int touched);
/* Scavenger bag think (C1 0x5288e0): clears the owner and recomputes which
 * clients see the bag, then runs again 50 ms later. */
void ScavengerBagThink(gentity_t *bag);
int PlayerHasScavengerAbility(unsigned client);

/* Rested time: IW4 GetRestedTime is now - stats.lastPlayedTime, and
 * LiveStorage_EndGame (0x468830) stamps lastPlayedTime when a match ends.
 * The native ledger is keyed by the trusted profile uid; call the recorder
 * from the game-end path (G_ShutdownGame / ExitLevel) for every connected
 * client. Unknown uids read as "never played" and rest for zero seconds. */
void RecordPlayerLastPlayedTime(unsigned client);
void RecordAllPlayersLastPlayedTime(void);
int PlayerRestedSeconds(unsigned client);

/* IW4 IsUsingOnlineDataOffline reads a per-controller Live session byte
 * (0x321ae88 + 0xa6790*client) that nothing in iw4x.exe writes. The native
 * flag is owned by whoever attaches profiles: raise it when a client's
 * persistent data is online-format data that cannot be written back. */
void SetClientUsingOnlineDataOffline(unsigned client, int offline);
void ResetClientOnlineDataFlag(unsigned client);

/* Leaderboard precache ledger (C1 0x7279d0: up to 100 names, "replace"
 * restarts it). Reset with the level. */
#define LEADERBOARD_PRECACHE_LIMIT 100
unsigned PrecachedLeaderboardCount(void);
const char *PrecachedLeaderboardName(unsigned index);
void ResetPrecachedLeaderboards(void);

/* Session reset for one client or the whole game: cards, online-data flag. */
void ResetPlayerBuiltinsClient(unsigned client);
void ResetPlayerBuiltinsAll(void);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

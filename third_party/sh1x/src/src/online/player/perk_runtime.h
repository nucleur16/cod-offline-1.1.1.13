#ifndef PERK_RUNTIME_H
#define PERK_RUNTIME_H
#include "abilities.h"
#include "perks.h"
#include "player.h"
#ifdef __cplusplus
extern "C" {
#endif
void ResetPerkRuntime(unsigned client);
/* Commit both perk sources (the eight C1 slots and the server-only set of
 * Zombies no_slot perks) and republish the merged abilities. */
int CommitPerkState(unsigned client,const perk_state_t *selection,
                          const slotless_perks_t *slotless);
int GetSlotlessPerks(unsigned client,slotless_perks_t *out);
int GetScriptAbilities(unsigned client,client_abilities_t *out);
/* Selected-perk contribution only; script ability writes remain independent.
 * Returns zero for absent/non-numeric effects or a non-live player. */
int GetSelectedPerkValue(const playerState_t *ps,unsigned id,float *out);
int CommitScriptAbilities(unsigned client,const client_abilities_t *state,int written_value_slot);
void UpdatePerkMoveSpeed(playerState_t *ps,float base);
#ifdef __cplusplus
}
#endif
#endif

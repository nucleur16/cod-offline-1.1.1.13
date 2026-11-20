/* CoDOL player appearance: the player's own character, assembled for the wire.
 *
 * What this module owns
 * ---------------------
 * The step between "the profile says the player equipped these eighteen items"
 * (character_composition.c) and "the snapshot carries a base model, a
 * contiguous run of attachments and a first-person arm/hand pair". That means
 * three things the composer deliberately does not do:
 *
 *   1. ORDER. A composition is indexed by C1's *profile* slot numbering
 *      (fpa 0, boo 1, fja 2, han 3, hea 4, ves 5, heg 6, elp 7, knp 8,
 *      pat 9, eye 13 - CharacterSlotForGroup). That is not the order the
 *      parts are attached in. The engine's own default character is built from
 *      mp/botClassTable.csv through the column-name array at C1 .data:0xCCCAB4,
 *      { charFPA, charBOO, charFJA, charHAN, charVES, charHEA, charHEG,
 *      charELP, charKNP, charPAT }, and CG_NewClientInfo's registration loop
 *      takes element 0 as the DObj root and the rest as attachModelIndex[0..].
 *      So the root is the trousers, and the head is *not* first. Publishing the
 *      first non-empty slot as the base model produces exactly the failure this
 *      module exists to remove: a body that is a floating head.
 *
 *   2. AVAILABILITY. Both registration loops stop at the first empty
 *      attachment name, so the published run has to be contiguous, and a name
 *      whose XModel the client cannot load makes it discard the *whole* body
 *      and substitute its built-in default character. Unresolvable parts are
 *      therefore dropped here rather than published as holes.
 *
 *   3. FIRST PERSON. The weapon only supplies the *default* arms:
 *      CG_RegisterWeaponViewModels (0x0047C4A0) seeds weaponViewInfo[w][0] from
 *      WeaponCompleteDef+0x10 and only while that slot is still zero. The
 *      per-player override is the playerState pair read at 0x0046D80E /
 *      0x0046D822 and installed into every weapon of ps->weaponsEquipped[15] by
 *      CG_SetCustomizedViewmodel (0x0047CF30). Its two models are column 4
 *      ("vmchar") of the jacket and glove rows the same composition selected -
 *      the only two of the ten part tables that have one. The arm gates the
 *      pair: 0x0046D819 skips the whole override when the arm index is <= 0,
 *      and 0x0047C5D6 draws no viewmodel at all when the arm model is NULL, so
 *      hands are never published without arms.
 *
 * What it does not own
 * --------------------
 * The playerState fields themselves. Arms and hands are handed to
 * PlayerStateSetViewmodels, which owns their storage, their archive and
 * follow copies and their translation to C1's +0x144/+0x148; this module never
 * writes those offsets. Nor does it own the snapshot's client record: it
 * produces a client_appearance_t and the collector applies it.
 *
 * Indices are *native* CS_MODELS slots, the same namespace G_ModelIndex uses,
 * because that is the namespace both consumers translate from.
 */
#ifndef PLAYER_APPEARANCE_H
#define PLAYER_APPEARANCE_H
#include "character_composition.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* One DObj root plus C1's sixteen attachment slots. */
#define CHARACTER_PARTS 17
typedef struct {
    unsigned count;                          /* 0..17; element 0 is the DObj root. */
    const char *model[CHARACTER_PARTS]; /* Borrowed from the mounted tables. */
    const char *arms,*hands;                 /* vmchar names, NULL when the item has none. */
} character_parts_t;
/* Composition -> attachment order, dropping slots with no selected model so the
 * run stays contiguous. Element 0 is the trousers when the player has any, as
 * the engine's own column list requires; the remaining named groups follow in
 * that same measured order, then eyewear, then any further composition slot
 * that resolved to a model in ascending slot order. The material slot (pat)
 * never contributes an XModel. Returns 1 with a complete ordering (possibly
 * empty), 0 on invalid input or an outfit with more parts than C1 can carry.
 * This orders names only; it loads nothing and publishes nothing. */
int OrderCharacterParts(const character_composition_t *,character_parts_t *);
#ifdef OL_SERVER
#include "clients.h"
/* Register a mounted model after level startup. Used by profile appearance and
 * runtime-composed weapons; returns the native CS_MODELS slot or zero. */
unsigned RegisterRuntimeModel(const char *name);
/* Resolve, validate, publish and store one connected client's character.
 *
 * Reads the client's own uploaded public profile through the mounted
 * playerdata schema, composes it for the given visual context, drops parts
 * whose XModel is not mounted, registers the survivors as native model
 * configstrings, records the resulting indices for the snapshot collector and
 * hands the arm/hand pair to PlayerStateSetViewmodels for the client's
 * live playerState.
 *
 * faction is "tf", "sc" or "zm"; gender 0 male, 1 female; camo is the
 * three-character camouflage tag ("urb", "arc", "des", "woo" or "sss"). The
 * skin tone is not an argument: C1 derives it from the head item itself and so
 * does this. rig_selector comes from ChestRigSelector.
 *
 * Returns 1 when a character was published, 0 when nothing could be resolved -
 * in which case the client keeps whatever appearance it already had, because
 * replacing a working body with an empty one is strictly worse. */
/* Publish an explicit native composition; bot CSVs need no uploaded profile. */
int WeaponRigSelector(const char *weapon,unsigned *selector);
int PublishCharacterComposition(unsigned client,const character_composition_t *,const char *faction);
/* A gametype-owned third-person body/head override. Both models must be
 * mounted, resolved humanoid skeletons; every head root must bone-merge onto
 * the body and their total must fit C1's DObj. The normal profile/bot
 * composition must have been published first. The override preserves its
 * first-person arms/hands and remains active across profile refresh/respawn.
 * Set/clear publish atomically; failure leaves the active outfit unchanged.
 * Clear restores the latest normal composition, not the outfit from the
 * first override. Both stores are discarded on connection reuse/map reset. */
int SetClientAppearanceModels(unsigned client,const char *body,const char *head);
int ClearClientAppearanceModels(unsigned client);
/* Sends C1 "s cg_vo_prefix" for this character's faction and gender; without
 * it C1 never registers the hold-breath sounds. See the definition. */
void SendClientVoicePrefix(unsigned client,const char *faction,unsigned gender);
/* The cg_vo_prefix last sent to this client ("UK_M1_", "AB_F1_", ...), or
 * NULL before its character is published. */
const char *ClientVoicePrefix(unsigned client);
int SelectClientCharacter(unsigned client,const char *faction,unsigned gender,
    const char *camo,unsigned rig_selector);
/* mp/cyborgclasstable.csv row key (zm1..zm4) of the client's zombies character;
 * 1 with *key set, 0 when the profile names none. */
int ClientZombieCharacterKey(unsigned client,const char **key);
/* The same selection with the chest-rig selector taken from a weapon name, the
 * way C1 0x611ba0 does: load the weapon, read its class at +0x344 and map it
 * through ChestRigSelector. An unknown weapon or class selects rig 0. */
int SelectClientCharacterForWeapon(unsigned client,const char *faction,unsigned gender,
    const char *camo,const char *weapon);
/* Republish stored native indices after namespace refresh. This is a live
 * producer; captured snapshots never consult the live character store. */
int PublishStoredClientCharacter(unsigned client);
/* Reapply the stored customised arm/hand pair to a tracked playerState owned
 * by this client. This accepts the live state and tracked snapshot/archive
 * copies: PlayerStateSetViewmodels resolves the extension belonging to
 * the supplied address. Retained script methods can overwrite
 * ps.viewmodelIndex after loadout selection; C1 instead keeps the customised
 * pair in gclient and copies it into ps for every snapshot. Refuses
 * followed/spectator states whose ps.clientNum names another player. */
struct playerState_s;
int ApplyStoredClientViewmodels(unsigned client,struct playerState_s *ps);
/* Stored native indices, for diagnostics and for callers that publish the
 * appearance themselves. count includes the base model. */
int ClientCharacterModels(unsigned client,const unsigned **models,unsigned *count,
    unsigned *arms,unsigned *hands);
void ClearClientCharacter(unsigned client);
void ResetClientCharacters(void);
/* Resolve the player's tagged (script) attachments to wire IDs and publish
 * them after the character parts. Game frame only (ClientEndFrame). */
void UpdateClientScriptAttachments(int client);
#endif
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

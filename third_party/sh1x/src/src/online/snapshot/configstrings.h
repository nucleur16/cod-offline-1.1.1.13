/* CoDOL configstring namespace, separate from assembly-visible IW3 storage. */
#ifndef CONFIGSTRINGS_H
#define CONFIGSTRINGS_H
#include "ol_msg.h"
#pragma pack(push, 8)
#define OL_MAX_CONFIGSTRINGS 6519
/* C1 SV_SetConfigstring5d30e0 rejects strlen(value)+1 > 0x3f8.
 * Dynamic updates are one d command; C1 has no IW3 x/y/z reassembly. */
#define CONFIGSTRING_VALUE_CHARS 1015
/* C1's rebuilt pool includes its reserved empty byte, then each nonempty
 * effective value and its NUL (4875c8..487660). */
#define CONFIGSTRING_POOL_BYTES 131072
#define MAX_GAME_CLIENTS 18 /* C1 dvar 2485 domain, not table capacity. */
#define CS_SERVER_ID 3
/* C1 team scores. The reliable commands C (4699d5) and D (469a12) write
 * cg+0xBEB20 and cg+0xBEB24 live; these two configstrings hold the same
 * values for the win/lose banner (550960) and for a late joiner's gamestate.
 * CS 5 is teamScores[TEAM_RED] (axis), CS 6 is teamScores[TEAM_BLUE]
 * (allies), matching the C/D pairing in PlayerCmd_UpdateScores. */
#define CS_TEAM_SCORE1 5
#define CS_TEAM_SCORE2 6
#define CS_FOG 13
#define CS_AMBIENT 1300
#define CS_AC130_AMBIENT 1301
#define CS_NORTHYAW 1302
#define CS_MATCH_END_TIME 15
#define CS_SLOWMOTION 6132 /* C1 54a5d2; four-field parser466eb0. */
#define CS_MAP_CENTER 16
#define CS_MINIMAP 1303
#define CS_THERMAL_BODY_MATERIAL 1304 /* C1 4675bf ->43de20. */
#define CS_WEAPONS 5362
#define WEAPON_SLOTS 700 /* C1 CG_Init validates slots 1..699. */
#define OL_CS_MODELS 1315
#define MODEL_SLOTS 2048
#define OL_CS_TAGS 5299
#define OL_CS_SOUNDS 3395
#define CS_EFFECTS 3651
#define EFFECT_SLOTS 320 /* Sentinel zero, effect IDs 1..319. */
#define BYTE_EFFECT_SLOTS 256 /* ET_FX/ET_LOOP_FX carry an 8-bit selector. */
#define CS_TAGGED_EFFECTS 3971
#define TAGGED_EFFECT_SLOTS 256
#define CS_TEAM_THERMAL 6100
#define CS_MINIMAP_ICONS 6085 /* C1 precacheminimapicon54ff80:17c5..17d3. */
#define CS_STATUS_ICONS 6062 /* C1 dispatcher467534:17ae..17b5. */
#define CS_HEAD_ICONS 6070 /* C1 precache head54fdb0:17b6..17c4. */
#define MINIMAP_ICON_SLOTS 15
#define TEAM_THERMAL_SLOTS 32 /* All slots, including zero, are usable. */
#define OL_CS_MATERIALS 4275
#define OL_CS_MENUS 4243
#define CS_SHELLSHOCKS 4227
#define SHELLSHOCK_SLOTS 16
#define MENU_SLOTS 32
#define HUD_MATERIAL_SLOTS 256
#define OL_CS_LOCALIZEDSTRINGS 680
#define CS_HINTSTRINGS 584 /* C1 45629c: offsets base6d0d130 + 584*4. */
/* C1 CS 584..679 (its G_GetHintStringIndex 0x53c4d0 compares against 0x60,
 * G_RegisterWeapon 0x529add passes 0x60). Slots 0..31 are the retained
 * 0x115 block (it ends at CS_LOCALIZEDSTRINGS); 32..95 are CoDOL-owned. */
#define HINTSTRING_SLOTS 96
#define HINTSTRING_RETAINED 32
#define LOCALIZED_SLOTS 600
#define SOUND_SLOTS 257 /* Sentinel zero, playable indices 1..256. */
#define TAG_SLOTS 64
#define CS_PLAYERNAMES 1280
#define PLAYERNAME_SLOTS 20 /* Followed by ambience at1300/1301. */
/* C1 467560 routes 1305..1310 into the vision applier 466b80 with the type
 * index; its global builtins publish there (5504fd naked, 5505cd thermal,
 * 55067d pain). IW4 uses 45c..460 and IW3 used 824/825 for naked/night. */
#define CS_VISIONSET 1305
#define VISIONSET_SLOTS 6 /* Scripts drive the first five. */
#define CS_WEAPON_COUNT 6517
#define CS_DVAR_NAMES 24
#define CS_DVAR_VALUES 224
#define MAX_REPLICATED_DVARS 200

typedef struct {
    char *values[OL_MAX_CONFIGSTRINGS];
    size_t bytes;
    unsigned count;
} configstrings_t;

/* Zero initialize before use. NULL/unset differs from explicitly published empty:
 * an empty value clears a client constant; unset leaves it inherited. */
void ClearConfigstrings(configstrings_t *state);
int OL_SetConfigstring(configstrings_t *state, unsigned index, const char *value);
/* Validate the client-visible merged pool. Empty overrides clear inherited
 * values; NULL overrides inherit. bytes is optional and unchanged on failure. */
int ConfigstringPoolSize(const configstrings_t *overrides,
    const configstrings_t *inherited, size_t *bytes);
/* Replace the whole name block atomically. NULL entries and unused slots clear
 * inherited names; names are bounded to the client's 63-byte storage. */
int SetPlayerNames(configstrings_t *state, unsigned count, const char *const *names);
/* Produce a single reliable-command token; NULL means a disconnected slot. */
void PlayerNameToken(char output[64], const char *name);
const char *GetConfigstring(const configstrings_t *state, unsigned index);
/* An explicit override, including "", takes precedence over the inherited
 * fastfile value. This is the value C1 has after a gamestate. */
const char *GetEffectiveConfigstring(const configstrings_t *overrides,
    const configstrings_t *inherited, unsigned index);
/* Build a separate inherited bank from (decimal index,value) rows. Atomic
 * replacement; duplicate/out-of-range indices are invalid. Do not serialize
 * this bank as overrides: the client already owns these constant values. */
int ImportConstantConfigstrings(configstrings_t *,size_t rows,const char *const *cells);
/* Reuse effective values, then allocate only slots not occupied by constants.
 * An inherited match creates no override. Empty values return sentinel zero.
 * Keep the inherited bank immutable for the map lifetime. */
int RegisterHudMaterial(configstrings_t *overrides,const configstrings_t *inherited,const char *);
int RegisterLocalizedString(configstrings_t *overrides,const configstrings_t *inherited,const char *);
int RegisterEffectName(configstrings_t *overrides,const configstrings_t *inherited,const char *);
/* A high inherited match may be duplicated into a free low slot; never truncate. */
int RegisterByteEffectName(configstrings_t *overrides,const configstrings_t *inherited,const char *);
/* Startup-only caller: exchange a wide effect with an unpinned byte slot.
 * Preserves all effective names and known constant references atomically.
 * Caller must prevent gamestate delivery and remap native bindings. */
int ReserveByteEffectSlot(configstrings_t *,const configstrings_t *,
    unsigned effect,const unsigned char pinned[BYTE_EFFECT_SLOTS]);
int RegisterTaggedEffect(configstrings_t *overrides,const configstrings_t *inherited,unsigned effect,const char *tag);
/* Publish an already resolved asset name, reusing its existing slot. Empty
 * names return the reserved zero slot; invalid names/full ranges return -1.
 * These do not load or validate the existence of an asset. Slots remain stable
 * until the bank is reset; callers must not overwrite registered entries. */
int RegisterModelName(configstrings_t *state, const char *name);
int RegisterTagName(configstrings_t *state, const char *name);
int RegisterInheritedModelName(configstrings_t *state,
    const configstrings_t *inherited, const char *name);
int RegisterInheritedTagName(configstrings_t *state,
    const configstrings_t *inherited, const char *name);
/* Resolve a retained effect by name into the CoDOL effect bank and register
 * its thermal attachment. Returns thermal slot 0..31, or -1 without mutation.
 * The caller must enforce precache timing and validate the loaded effect. */
int RegisterTeamThermal(configstrings_t *overrides,
    const configstrings_t *inherited,const char *effect,const char *tag);
/* Measured checksums of the unmodified 1.1.1.13 constant tables, not fastfile
 * checksums. Custom tables require an explicit checksum in WriteGamestate. */
int ConstConfigChecksum(const char *zone_type, uint32_t *checksum);
/* Caller supplies names contiguously. Replaces the complete replicated block. */
int SetReplicatedDvars(configstrings_t *state, unsigned count,
                            const char *const *names, const char *const *values);
/* Both writers return zero on overflow; failed bytes must never be sent. */
int WriteConfigstrings(ol_msg_t *msg, const configstrings_t *state);
int WriteGamestate(ol_msg_t *msg, const configstrings_t *state,
                         const char *map, const char *zone_type, uint32_t const_crc,
                         uint32_t command_sequence, uint32_t client_number,
                         uint32_t checksum_feed);
#pragma pack(pop)
#endif

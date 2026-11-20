#ifndef OL_PROFILE_H
#define OL_PROFILE_H
#include "structured_schema.h"
#pragma pack(push, 8)
/* Lobby storage zlib stream: exactly 32768 decoded bytes and a valid CRC32
 * over bytes 4 onward. No file selection or account association is implied. */
int DecodeStoredProfile(const uint8_t *compressed,size_t size,uint8_t *out,size_t capacity);
/* Read-only lobby-compatible secure-profile selection. trusted_uid must come
 * from a trusted session owner, never directly from connect userinfo. Tries
 * mpdata_s.<decimal uid> when enabled, then mpdata_s, then optional seed basename.
 * Existing unreadable/invalid files fail without falling back. 1=loaded,
 * 0=absent, -1=invalid/I/O failure; output is preserved unless loaded. */
int ReadSecureProfile(const char *directory,uint64_t trusted_uid,int per_uid,
    const char *seed_basename,uint8_t *out,size_t capacity);
int ProfileRecord(const uint8_t *profile,size_t size,const structured_schema_t *,
    const structured_definition_t **definition,const uint8_t **record,size_t *capacity);
/* Select the unique definition matching the complete upload's version and
 * format checksum. The uploaded CRC occupies four bytes before the structured
 * record. Returns borrowed data; failure preserves every output. This does not
 * establish CRC authenticity or translate IW4 fields to CoDOL fields. */
int ClientProfileRecord(unsigned client,const structured_schema_t *,
    const structured_definition_t **definition,const uint8_t **record,size_t *capacity);
int ClientProfileScalar(unsigned client,const structured_schema_t *,
    const structured_path_t *,size_t count,uint32_t *out);
typedef struct {
    unsigned character_index;
    uint16_t items[20]; /* C1 cache order; use CharacterSlotForGroup.
                        * First 18 feed composition, not DObj attachment order. */
} profile_character_t;
/* C1 faction selector plus all twenty characterClasses[index].slots values.
 * Explicit public profile input; no file/account selection or item defaults.
 * Faction is tf/sc/zm. Missing/invalid schema or record preserves output. */
int ProfileCharacter(const uint8_t *,size_t,const structured_schema_t *,
    const char *faction,profile_character_t *out);
/* Explicit decoded secure profile + mounted CoDOL allChallengesTable. Resolves
 * a challenge name to column-4 ID, reads challenges[ID] as float progress,
 * and returns IW4's one-based next tier. Failure preserves out. No identity,
 * CRC validation or profile attachment is inferred by this read-only adapter. */
int ProfileChallengeState(const uint8_t *,size_t,const structured_schema_t *,
    const engine_table_t *,const char *challenge,int *out);
/* C1 total XP conversion, including rankVersion mismatch clamping. Returns
 * zero-based stored rank and total experience atomically; no profile writes. */
int ProfileExperience(const uint8_t *,size_t,const structured_schema_t *,
    const engine_table_t *,int *rank,int *experience);
/* Rank-table interval lookup for nonnegative total XP. Numeric rank keys are
 * used rather than physical CSV row indices. Above-max XP returns maxrank.
 * Missing/duplicate/non-contiguous thresholds fail without changing output. */
int RankForExperience(const engine_table_t *,int experience,int *rank);
/* Complete IW4 unlock-table query over explicit profile/table inputs. Unknown
 * item rows return unlocked, as IW4 does. Missing required profile semantics
 * fail and preserve output; unknown title/icon overrides permit true when
 * ordinary requirements prove access, but cannot establish a locked result. */
int ProfileItemUnlocked(const uint8_t *,size_t,const structured_schema_t *,
    const engine_table_t *unlocks,const engine_table_t *ranks,
    const engine_table_t *challenges,const char *item,int *unlocked);
/* Trusted server owner API. Caller validates storage CRC and chooses the
 * account; never call with an identity taken solely from connect userinfo.
 * Owned copy is replaced atomically, and cleared on connection/map reset. */
int AttachClientSecureProfile(unsigned client,uint64_t trusted_uid,const uint8_t *,size_t,
    const structured_schema_t *);
/* The view every secure-profile reader shares: the session copy a script edit
 * made, otherwise the attached profile. */
const uint8_t *ClientSecureProfile(unsigned client,uint64_t *trusted_uid);
/* The attached bytes themselves, ignoring session edits, for callers that must
 * see what was actually attached rather than what a script changed. */
const uint8_t *ClientSecureProfileAttached(unsigned client,uint64_t *trusted_uid);
/* Copy-on-write session view of the attached profile, owned here and dropped
 * on detach and on re-attach. Never uploaded, stored or re-authenticated. */
uint8_t *ClientSecureProfileWritable(unsigned client);
void DetachClientSecureProfile(unsigned client);
void ResetClientSecureProfiles(void);
/* Map change with the player still connected (map_rotate, map). SV_ClearServer
 * detaches every profile; without a carry the next attach re-reads the lobby's
 * file, which does not yet hold the match just played, and the player drops
 * back to the rank they joined with. Carry keeps the session view (what the
 * client itself holds, via the 'K' pushes) for that slot and uid; Take hands
 * it back exactly once, and only to the same uid; Drop forgets it. */
void CarryClientSecureProfile(unsigned client);
int TakeCarriedSecureProfile(unsigned client,uint64_t trusted_uid,uint8_t *out,size_t capacity);
void DropCarriedSecureProfile(unsigned client);
#pragma pack(pop)
#endif

/*
 * dw_profiles.h - the per-player career profile (store/player_profiles.py) and the rank table
 * arithmetic it needs (career/rank_service.py).
 *
 * players/<uid>.json is the ONE authority: `total_xp` is the only number that matters, the
 * (rank, experience) pair is derived from it and rendered into the "mpdata_s" storage file whenever
 * the lobby is about to serve it. MEASURED encoding of that blob: stored `rank` is ONE-BASED
 * (GetRank @0x0058EFE0 does `dec eax`), `experience` is XP *within* the rank
 * (LiveStorage_GetExperience @0x0058EEB0 adds the rank's minXP itself), rankVersion = 2.
 *
 * The functions other modules call without this header (EnsureBlob, PlayerLevel, ExportMatchBlob,
 * SetMirrorOwner) are declared in dw_state.h.
 */
#pragma once
#include "dw.h"
#include "dw_json.h"

namespace dw {

/* ---- mp/rankTable.csv (resource unlock_tables.proposed.json "rankTable"; 70 rows, maxrank 69) ---- */
int RankCount(Server &s);
/* total lifetime XP -> the ZERO-BASED rank index and the XP within it. Clamped inside the last rank
 * so the client's "XP to next" stays positive. -> false when the table is missing. */
bool XpToRank(Server &s, int64_t totalXp, int &rankIndex, int64_t &xpInRank);
/* The inverse, from the mpdata_s pair (one-based stored rank). */
int64_t RankToXp(Server &s, int64_t storedRank, int64_t experience);
int64_t RankXpToNext(Server &s, int rankIndex);
/* The ONE-BASED level the client displays for this lifetime XP. */
int LevelForXp(Server &s, int64_t totalXp);
/* GP for the displayed levels in (fromLevel, toLevel]: a fixed 200 x the level reached. */
int64_t RankupGp(int64_t fromLevel, int64_t toLevel);
/* Total lifetime XP of a stored mpdata_s blob. -> false when it is not a readable mpdata_s. */
bool XpFromBlob(Server &s, const Bytes &stored, int64_t &totalXp);

/* ---- the store ---- */
/* The profile for this uid: uid / total_xp / level normalised. A uid with no profile gets a blank
 * level-1 one (written at once) unless create is false. -> false when there is none. */
bool ProfileLoad(Server &s, uint64_t uid, Json &prof, bool create = true);
/* Normalises (format, level, updated, lifetime, history <= 50) and writes players/<uid>.json. */
bool ProfileSave(Server &s, Json &prof);
Json ProfileBlank(Server &s, uint64_t uid);
std::vector<uint64_t> ProfileListUids(Server &s);
/* The collectibles[] indices (story intel) the profile holds, sorted (progression.collectibles). */
std::vector<int> ProfileCollectibles(const Json &prof);

/* ---- the two mutators ---- */
struct RankupClaim { /* add_xp(rankup=...): filled with the GP owed for the levels this credit reached */
    int64_t gp;
    int from, to;
    RankupClaim() : gp(0), from(0), to(0) {}
};
/* Credit `amount` XP. `stats` = {kills,deaths,assists,matches} increments, `lifetime` = the match
 * report row's per-match secure-DDL deltas (open key set), `progression` = its progression object
 * (handed to ProgressionCredit of dw_career.cpp). Null Json = absent. -> a note, "" for a no-op. */
std::string ProfileAddXp(Server &s, uint64_t uid, int64_t amount, const std::string &reason, const std::string &name,
                         const Json &stats, const Json &lifetime, const Json &progression, RankupClaim *rankup);
/* Take a client-uploaded "mpdata_s" back into the profile: the higher XP wins, nobody is demoted. */
std::string AbsorbBlob(Server &s, uint64_t uid);
/* EnsureBlob (dw_state.h) with the shared-copy mirror switchable (ensure_blob(mirror=...)). */
std::string EnsureBlobEx(Server &s, uint64_t uid, bool mirror);
uint64_t MirrorOwner(Server &s);

} // namespace dw

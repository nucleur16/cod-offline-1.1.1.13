/*
 * dw_career.h - the career side: challenge / weapon-level progression (career/progression.py,
 * challenges.py, weapon_leveling.py over resources/progression_tables.json) and the match-report
 * write-back (career/rank_writeback.py).
 *
 * What other modules call (RankWritebackMaybeApply, ProgressionCredit, ProgressionMastery, ServiceTick,
 * CareerInit / CareerFree) is declared in dw_state.h / dw.h; this header is the rest of the module's
 * surface, used by dw_career.cpp itself and by the tests.
 *
 * THE MATCH REPORT (format 1), as the match server writes it into Config::matchProfile:
 *   match_report.json                         the legacy single slot
 *   match_report.q.<epoch>.<port>.<seq>.json  the spool, one file per report, folded in name order
 * (written as "<name>.tmp" and renamed, so a half-written report is never seen). After folding the
 * file is renamed in place to match_report.applied.<YYYYmmdd-HHMMSS>[-n].json; a byte-identical
 * re-drop becomes match_report.dup.<ts>.json, a malformed one match_report.bad.<ts>.json; every
 * outcome is appended to match_report.journal.log.
 *
 *   { "format": 1, "reason": "<text>", "created": <unix s>, "duration_s": <s>,
 *     "players": [ { "slot": <int>, "name": "<text>", "bot": <bool>,        required on every row
 *                    "uid": "<decimal u64>",              the row's account; absent on a bot row
 *                    "kills": n, "deaths": n, "assists": n,
 *                    "xp": { "total": n, "applied_live": n },
 *                    "lifetime": { "<secure-DDL member>": <per-match delta, int> ... },   open key set
 *                    "progression": {
 *                        "challenges":   { "<challenge ref or id>": amount },
 *                        "weapons":      { "<weapon name>": level-driving counter },
 *                        "weapon_uses":  { "<weapon name>": count },
 *                        "collectibles": { "<index 0..1023>": 1 },
 *                        "coop_mission_completed": "<coop playlist name>",
 *                        "coop_mission_failed":    "<coop playlist name>",
 *                        "gp": n, "gp_fixed": { "rankups": n } } } ... ] }
 */
#pragma once
#include <map>
#include <string>
#include <vector>
#include "dw.h"
#include "dw_json.h"

namespace dw {

/* One row of mp/allchallengestable.csv (challenges.Challenge). */
struct CareerChallenge {
    int64_t id;
    std::string ref;
    std::vector<double> targets;   /* ascending tier targets */
    std::vector<int64_t> rewards;  /* per tier */
    /* how many tier targets `progress` has reached, in order */
    int Completed(double progress) const;
};

/* One weapon's levelling rule (weapon_leveling.Weapon). */
struct CareerWeapon {
    std::string name;            /* table spelling */
    int64_t item;                /* marketplace item id; 0 = none */
    std::vector<int64_t> curve;  /* curve[n] = cumulative XP at which level n begins */
    int LevelForXp(int64_t xp) const;
    int MaxLevel() const { return (int)curve.size() - 1; }
};

/* false when resources/progression_tables.json is missing or lacks a table the Python loader needs. */
bool CareerTablesOk(Server &s);
/* Challenges.get: by (case-insensitive) ref or by numeric id text. NULL when unknown. */
const CareerChallenge *CareerChallengeGet(Server &s, const std::string &ref);
bool CareerChallengeDeactivated(Server &s, int64_t id);
/* WeaponLeveling.get: case-insensitive. */
const CareerWeapon *CareerWeaponGet(Server &s, const std::string &name);
/* WeaponLeveling.mastery_ref: "ch_<weapon>_mastery" when the tables have that challenge, else "". */
std::string CareerMasteryRef(Server &s, const std::string &weapon);
/* progression.weapon_levels: {lower-cased weapon: level}. */
std::map<std::string, int> CareerWeaponLevels(Server &s, const Json &prof);
/* progression.effective_progress: stored challenge progress with the mastery rows overwritten. */
void CareerEffectiveProgress(Server &s, const Json &prof, std::map<int64_t, double> &out);
/* progression.granted_flag_ids: sorted unlock-flag ids the profile has earned; empty without tables. */
std::vector<int64_t> CareerGrantedFlagIds(Server &s, const Json &prof);
/* progression.coop_mission_challenge: the storymission challenge ref of a coop playlist, or "". */
std::string CareerCoopMissionChallenge(Server &s, const std::string &mission);
/* progression.weapon_xp_by_item: {marketplace item id: weapon XP} for the inventory records. */
std::map<int64_t, int64_t> CareerWeaponXpByItem(Server &s, const Json &prof);

/* rank_writeback._validate: "" for a well-formed format-1 report, else what is wrong with it. */
std::string RwbValidate(const Json &rep);
/* The report id: the SHA-1 of the report's bytes, as the Python lobby's ledger keys it. */
std::string RwbReportId(const Bytes &raw);

} // namespace dw

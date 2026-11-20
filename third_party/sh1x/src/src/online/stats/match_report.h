#ifndef MATCH_REPORT_H
#define MATCH_REPORT_H
#include <stdint.h>
/* The match result this server reports back to the CoDOL lobby.
 *
 * The lobby (alpha0022 dw-capture.py) owns progression: players/<uid>.json and
 * resources/lobby.sqlite are the rank/XP authority, and the blob the client
 * actually reads, mpdata_s.<uid>, is RE-RENDERED from that profile every time
 * the client fetches it. The one channel between the two processes is a file in
 * the lobby's resources directory: the match server drops
 * resources/match_report.json, and rank_writeback.maybe_apply() consumes it on
 * the next storage GETFILE, folding each row's XP into that row's profile.
 * The server never writes mpdata_s itself - two writers on one blob is exactly
 * the lost-update race the lobby's own journal recorded.
 *
 * Format 1, the schema rank_writeback._validate() accepts, plus a per-row
 * "uid": the retired Python game server only ever had one human, so its reader
 * credited every row to the connection that happened to ask for storage. Each
 * row now names its own account.
 *
 * XP is not invented here. _rank.gsc runs the ordinary IW4 progression flow and
 * commits through setRankExperience, which normalises (rank, experience,
 * rankVersion) into the client's session secure profile. So the XP a player
 * earned this match is exactly
 *     ProfileExperience(session blob) - the same value at attach time,
 * and both halves are read through the recovered C1 conversion rather than
 * accumulated separately.
 *
 * Every report resets its own baselines, so consecutive reports partition the
 * match's XP instead of repeating it. Rows carry applied_live 0: unlike the
 * Python server this one runs no live save pump, so nothing has been persisted
 * behind the lobby's back. */

/* A client's secure profile has just been attached. Captures the baseline XP
 * and adopts any row the same uid left behind earlier in this match, so a
 * reconnect neither loses nor repeats its earnings. */
void MatchReportClientAttached(unsigned client, uint64_t uid);
/* A client is leaving. Freezes its row (counters and earned XP) so the next
 * report still carries it after the slot is reused or cleared. */
void MatchReportClientDropped(unsigned client);
/* Write this match's report into resources/: by default a spool file of its
 * own, match_report.q.<epoch>.<port>.<seq>.json, which the lobby folds oldest
 * first (a rotation writes several before anyone returns to the lobby);
 * matchReportSpool 0 writes the legacy single match_report.json.
 * reason is the free-text cause the lobby echoes back in its journal
 * ("gameended", "disconnect", "mapchange", ...).
 * Returns 2 immutable result retained/queued, 0 nothing owed (or explicitly
 * suppressed testing build), -1 serialization/path failure with rows retained.
 * No asynchronous admission is presented as an on-disk success. */
#define REPORT_PENDING 2
/* 2 means immutable bytes retained/queued, NOT yet durably acknowledged.
 * Main-thread pump only; worker never reads world/engine state. */
int WriteMatchReport(const char *reason);
void MatchReportFrame(void);
int MatchReportPrepareReset(void);
int MatchReportTransitionHeld(void);
int MatchReportCanReuseSlot(unsigned client);
/* Exit-only bounded drain; zero is explicit failure, never a durable claim. */
int MatchReportShutdown(unsigned timeout_ms);
/* Whether any human client is still connected. Used by the disconnect path to
 * decide whether the match is over. */
int MatchReportHumansConnected(void);
/* Credit `amount` weapon XP to the base weapon of `weapon` (a plain or
 * composed name) on this client's row; the next report carries it as
 * progression.weapons. 1 recorded, 0 no account row / nothing to credit. */
int MatchReportCreditWeaponXP(unsigned client, const char *weapon, int amount);
/* Record that this client picked up collectibles[index] (mp/playerdata.def;
 * the index is the mp/coCollectibles.csv row). The next report carries it as
 * progression.collectibles. 1 recorded (or already recorded this match), 0 no
 * account row / out of range / full. */
int MatchReportCollectible(unsigned client, unsigned index);
/* Record that this client finished the coop mission `mission` (its playlist
 * name, sv_sessionMap): completed != 0 for a completion, 0 for a failure.
 * The next report carries it as progression.coop_mission_completed or
 * progression.coop_mission_failed. A completion is not replaced by a later
 * failure of the same mission in the same match. 1 recorded, 0 no account row
 * or an unusable name. */
int MatchReportCoopMission(unsigned client, const char *mission, int completed);
/* Record this player's fixed-round Zombies outcome for the site leaderboard. */
int MatchReportZombiesResult(unsigned client, int extracted, int difficulty,
                                  int round, int round_goal, int downs,
                                  int revives, int headshots, int extraction_event);
/* Main-thread world state only. No submission/finality or reward ownership
 * change. Reset with the world after all older rows are durably drained. */
int MatchReportZombiesCheckpoint(int difficulty, int round, int round_goal);
/* Fixed GP award sources. A fixed award is credited at exactly its table
 * amount; no activity gp_scale applies (user rule 2026-10-13). */
#define GP_SOURCE_FIELD_ORDER 0   /* mp/cofieldorder*.csv FO_GP reward */
#define GP_SOURCE_CHALLENGE   1   /* mp/allChallengesTable.csv CP1..CP9 */
#define GP_SOURCE_RANKUP      2   /* a rank-up: RANKUP_GP_PER_LEVEL x level */
#define GP_SOURCES            3
/* A rank-up pays this many GP times the new displayed level (rank index + 1),
 * for every level crossed (user rule 2026-10-13): level 2 pays 400, a jump from
 * 4 to 6 pays 200*5 + 200*6. Fixed: no gp_scale or league factor. */
#define RANKUP_GP_PER_LEVEL   200
/* Credit `amount` fixed GP (account currency 2) from `source` to this
 * client's row; the next report carries the row's total as progression.gp
 * (the lobby deposits it) and the per-source split as progression.gp_fixed.
 * 1 recorded, 0 no account row / bad source / non-positive amount. */
int MatchReportCreditFixedGP(unsigned client, int source, int amount);
/* MatchReportCreditFixedGP(client, GP_SOURCE_FIELD_ORDER, amount). */
int MatchReportCreditGP(unsigned client, int amount);
/* "fieldorder"/"field_orders" (also NULL or ""), "challenge"/"challenges" and
 * "rankup"/"rankups" -> CODOL_GP_SOURCE_*, anything else -1. */
int MatchReportGPSource(const char *name);
/* GP for the displayed levels in (old_level, new_level] this row has not been
 * paid for yet: RANKUP_GP_PER_LEVEL x each level, credited to
 * GP_SOURCE_RANKUP. The row remembers the highest level it paid (and,
 * from attach, the level the profile already had), carried across a reconnect,
 * so a level is paid at most once per match whatever the caller passes.
 * -> GP credited now, 0 for nothing new / no account row. */
int MatchReportCreditRankUp(unsigned client, int old_level, int new_level);
/* Sum of RANKUP_GP_PER_LEVEL x L for L in (from, to]; 0 when to <= from.
 * Levels are clamped to 1..RANKUP_MAX_LEVEL. */
#define RANKUP_MAX_LEVEL      1000
int RankUpGP(int from_level, int to_level);
/* GP this client's live row holds now, before a report carries it: one
 * CODOL_GP_SOURCE_* source, or source -1 for the total the next report's
 * progression.gp deposits. 0 without an account row. */
int MatchReportGP(unsigned client, int source);
/* Whether this client's row already records collectibles[index] this match. */
int MatchReportHasCollectible(unsigned client, unsigned index);
/* Final teardown only after PrepareReset: return0 without clearing anything
 * while immutable persistence is pending/failed; return1 after durable ack. */
int ResetMatchReport(void);
/* Install the server match-record delivery sink so sendMatchData's prepared
 * record reaches the lobby's resources directory instead of being retained
 * with nowhere to go. Called once at startup. */
void InstallMatchDataSink(void);
#endif

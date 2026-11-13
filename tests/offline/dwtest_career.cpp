/*
 * dwtest --selftest-career: level / XP boundaries, the progression tables, report validation and the
 * match-report write-back (idempotence, also across a Store reopen), with no network. Needs
 * --resources <the lobby's resources>, --profile <an EMPTY scratch folder> and --match-profile
 * <an EMPTY scratch folder> BEFORE the flag.
 *
 * dwtest [--fixed-time T] [--differential] [--wb-background] [--wb-create <uid>]
 *        --writeback <resources> <profile-dir> <match-dir> <uid>
 * applies the reports waiting in <match-dir> as the connection `uid` (0 = none) and prints one JSON
 * object: the note, every profile, GP wallet, inflated mpdata_s and the retry state. The other half
 * of tests/offline/writeback_diff.py.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "../../offline/dw.h"
#include "../../offline/dw_career.h"
#include "../../offline/dw_playerdata.h"
#include "../../offline/dw_profiles.h"
#include "../../offline/dw_state.h"
#include "../../offline/dw_store.h"

using namespace dw;

#ifndef DW_HAVE_CAREER /* built with OFFLINE_SKIP=dw_career.cpp: nothing to test */
int DwCareerSelfTest(const Config &) { fprintf(stderr, "dwtest was built without dw_career.cpp\n"); return 2; }
int DwWritebackRun(const Config &, const char *, bool, const char *) { fprintf(stderr, "dwtest was built without dw_career.cpp\n"); return 2; }
#else

static int g_fail, g_checks;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

namespace {

struct QuietHost : Host {
    std::vector<std::string> lines;
    virtual void Log(const std::string &line) { lines.push_back(line); }
};

const uint64_t U1 = 9000000000000000041ULL, U2 = 9000000000000000042ULL, U3 = 9000000000000000043ULL;

std::string UidText(uint64_t uid) { return Fmt("%llu", (unsigned long long)uid); }

/* One human row; `extra` is spliced in as further members (", \"lifetime\": {...}"). */
std::string HumanRow(int slot, const char *name, uint64_t uid, int total, int kills, int deaths, int assists, const std::string &extra = "")
{
    return Fmt("{\"slot\": %d, \"name\": \"%s\", \"bot\": false, %s\"kills\": %d, \"deaths\": %d, \"assists\": %d, "
               "\"xp\": {\"total\": %d, \"applied_live\": 0}%s}",
               slot, name, uid ? ("\"uid\": \"" + UidText(uid) + "\", ").c_str() : "", kills, deaths, assists, total, extra.c_str());
}

std::string Report(const std::string &rows, const char *reason = "timelimit")
{
    return Fmt("{\"format\": 1, \"created\": 1790000000, \"reason\": \"%s\", \"duration_s\": 600.0, \"players\": [", reason) + rows +
           ", {\"slot\": 7, \"name\": \"bot0\", \"bot\": true, \"kills\": 2, \"deaths\": 10, \"assists\": 0}]}";
}

bool Drop(const Config &cfg, const std::string &text, const char *name = "match_report.json")
{
    return WriteFileAtomic(PathJoin(cfg.matchProfile, name), text);
}

int CountFiles(const Config &cfg, const char *prefix)
{
    std::vector<std::string> names = ListDir(cfg.matchProfile);
    int n = 0;
    for (size_t i = 0; i < names.size(); i++)
        if (!names[i].compare(0, strlen(prefix), prefix)) n++;
    return n;
}

int64_t Xp(Server &s, uint64_t uid)
{
    Json prof;
    return ProfileLoad(s, uid, prof, false) ? prof.get("total_xp").asInt() : -1;
}

Json Parsed(const char *text)
{
    Json j;
    if (!Json::Parse(text, j)) printf("  (test JSON does not parse: %s)\n", text);
    return j;
}

void Levels(Server &s)
{
    /* mp/rankTable.csv: 70 ranks; the first begins at 0 XP, every threshold is exact */
    CHECK(RankCount(s) == 70);
    CHECK(LevelForXp(s, 0) == 1 && LevelForXp(s, -5) == 1);
    int64_t second = RankToXp(s, 2, 0);
    CHECK(second > 0 && LevelForXp(s, second - 1) == 1 && LevelForXp(s, second) == 2);
    for (int level = 2; level <= 70; level++) {
        int64_t min = RankToXp(s, level, 0);
        int idx;
        int64_t in;
        if (LevelForXp(s, min) != level || LevelForXp(s, min - 1) != level - 1 || !XpToRank(s, min, idx, in) || idx != level - 1 || in != 0) {
            CHECK(!"rank threshold");
            break;
        }
    }
    g_checks++;
    /* the last rank: lifetime XP past the table clamps inside it, so "XP to next" stays positive */
    int64_t last = RankToXp(s, 70, 0);
    int idx;
    int64_t in;
    CHECK(LevelForXp(s, last) == 70 && LevelForXp(s, last - 1) == 69 && LevelForXp(s, last * 100) == 70);
    CHECK(XpToRank(s, last * 100, idx, in) && idx == 69 && in == RankXpToNext(s, 69) - 1);
    CHECK(RankToXp(s, 1, 0) == 0 && RankToXp(s, 0, 7) == 7 && RankToXp(s, 999, 0) == last);
    /* rank-up GP: 200 x every level reached */
    CHECK(RankupGp(1, 2) == 400 && RankupGp(4, 6) == 2200 && RankupGp(5, 5) == 0 && RankupGp(6, 3) == 0 && RankupGp(0, 2) == 400);
}

void Tables(Server &s)
{
    CHECK(CareerTablesOk(s));
    const CareerWeapon *ak = CareerWeaponGet(s, "AK47_mp");
    CHECK(ak && ak->name == "ak47_mp" && ak->item == 1009 && ak->MaxLevel() == 30);
    if (ak) {
        CHECK(ak->LevelForXp(0) == 0 && ak->LevelForXp(499) == 0 && ak->LevelForXp(500) == 1 && ak->LevelForXp(1599) == 1);
        CHECK(ak->LevelForXp(1600) == 2 && ak->LevelForXp(179599) == 29 && ak->LevelForXp(179600) == 30 && ak->LevelForXp(1000000000) == 30);
    }
    CHECK(!CareerWeaponGet(s, "no_such_weapon_mp"));
    CHECK(CareerMasteryRef(s, "ak47_mp") == "ch_ak47_mp_mastery" && CareerMasteryRef(s, "no_such_weapon_mp").empty());
    const CareerChallenge *g = CareerChallengeGet(s, " CH_Ghillie ");
    CHECK(g && g->id == 60 && g->targets.size() == 3 && g->targets[0] == 50 && g->targets[2] == 200);
    if (g) CHECK(g->Completed(0) == 0 && g->Completed(49.5) == 0 && g->Completed(50) == 1 && g->Completed(199.5) == 2 && g->Completed(10000) == 3);
    CHECK(CareerChallengeGet(s, "60") == g && !CareerChallengeGet(s, "ch_not_a_real_challenge") && !CareerChallengeGet(s, "--60"));
    CHECK(CareerCoopMissionChallenge(s, "coop_training_mp_killhouse_sh") == "so_coop_training_mp_killhouse_sh");
    CHECK(CareerCoopMissionChallenge(s, "coop_spec_mp_estate_tropical") == "so_coop_spec_mp_estate_tropical_2");
    CHECK(CareerCoopMissionChallenge(s, "coop_prolog_mp_crash_intro").empty() && CareerCoopMissionChallenge(s, "mp_crash").empty());

    /* the flag set: rank flags follow the level, 505 / 81 / 663 are never granted */
    Json p1 = Parsed("{\"level\": 1}"), p31 = Parsed("{\"level\": 31}");
    std::vector<int64_t> at1 = CareerGrantedFlagIds(s, p1), at31 = CareerGrantedFlagIds(s, p31);
    CHECK(std::binary_search(at1.begin(), at1.end(), 1) && !std::binary_search(at1.begin(), at1.end(), 31));
    CHECK(std::binary_search(at31.begin(), at31.end(), 31) && at31.size() > at1.size());
    Json big = Parsed("{\"level\": 70, \"challenges\": {}}");
    for (int id = 0; id < 1000; id++) big["challenges"][Fmt("%d", id)] = Json(1e9);
    std::vector<int64_t> all = CareerGrantedFlagIds(s, big);
    CHECK(all.size() > 600 && !std::binary_search(all.begin(), all.end(), 505) && !std::binary_search(all.begin(), all.end(), 81) &&
          !std::binary_search(all.begin(), all.end(), 663));

    /* progression.credit */
    Json prof = Parsed("{\"level\": 1}");
    std::string note = ProgressionCredit(s, prof, Parsed("{\"challenges\": {\"ch_ghillie\": 50.0, \"ch_nope\": 5}, \"weapons\": {\"AK47_mp\": 14000.0, "
                                                         "\"not_a_weapon\": 1}, \"weapon_uses\": {\"ak47_mp\": 3}}"));
    CHECK(prof.get("challenges").get("60").asReal() == 50.0 && prof.get("challenges").size() == 1);
    CHECK(prof.get("weapons").get("ak47_mp").asInt() == 14000 && prof.get("weapons").get("not_a_weapon").asInt() == 1);
    CHECK(prof.get("weapon_uses").get("ak47_mp").asInt() == 3);
    CHECK(note.find("1 challenge tier(s)") != std::string::npos && note.find("10 weapon level(s)") != std::string::npos &&
          note.find("unknown challenge 'ch_nope'") != std::string::npos && note.find("unknown weapon") != std::string::npos);
    std::map<int, double> mastery;
    ProgressionMastery(s, prof, mastery);
    const CareerChallenge *m = CareerChallengeGet(s, "ch_ak47_mp_mastery");
    CHECK(m && mastery.size() == 1 && mastery[(int)m->id] == 10.0 && !prof.get("challenges").has(Fmt("%lld", (long long)m->id)));
    CHECK(CareerWeaponXpByItem(s, prof).size() == 1 && CareerWeaponXpByItem(s, prof)[1009] == 14000);
    /* coop: a failure unlocks nothing, a completion completes challenge 800 once, a replay is only counted */
    Json cp = Parsed("{\"level\": 1}");
    size_t flags0 = CareerGrantedFlagIds(s, cp).size();
    ProgressionCredit(s, cp, Parsed("{\"coop_failed\": {\"coop_training_mp_killhouse_sh\": 1}}"));
    CHECK(CareerGrantedFlagIds(s, cp).size() == flags0 && cp.get("coop_missions").get("coop_training_mp_killhouse_sh").get("failed").asInt() == 1);
    note = ProgressionCredit(s, cp, Parsed("{\"coop_completed\": {\"coop_training_mp_killhouse_sh\": 1}}"));
    CHECK(cp.get("challenges").get("800").asReal() == 1.0 && CareerGrantedFlagIds(s, cp).size() == flags0 + 3);
    CHECK(note.find("3 unlock flag(s)") != std::string::npos);
    note = ProgressionCredit(s, cp, Parsed("{\"coop_completed\": {\"coop_training_mp_killhouse_sh\": 1}, \"collectibles\": {\"5\": 1, \"3\": 1, \"2000\": 1}}"));
    CHECK(cp.get("challenges").get("800").asReal() == 1.0 && cp.get("coop_missions").get("coop_training_mp_killhouse_sh").get("completed").asInt() == 2);
    CHECK(cp.get("collectibles").size() == 2 && cp.get("collectibles").at(0).asInt() == 3 && note.find("intel 3,5") != std::string::npos);
}

void Validation()
{
    CHECK(RwbReportId("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(RwbReportId("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(RwbValidate(Parsed("[1]")) == "not a JSON object");
    CHECK(RwbValidate(Parsed("{\"format\": 2, \"players\": [{}]}")).find("format") == 0);
    CHECK(RwbValidate(Parsed("{\"format\": 1}")) == "players is not a non-empty list");
    CHECK(RwbValidate(Parsed("{\"format\": 1, \"players\": []}")) == "players is not a non-empty list");
    CHECK(RwbValidate(Parsed("{\"format\": 1, \"players\": [5]}")) == "players[0] is not an object");
    CHECK(RwbValidate(Parsed("{\"format\": 1, \"players\": [{\"slot\": 0, \"name\": \"a\"}]}")) == "players[0] lacks 'bot'");
    const char *head = "{\"format\": 1, \"players\": [{\"slot\": 0, \"name\": \"a\", \"bot\": false";
    CHECK(RwbValidate(Parsed((std::string(head) + "}]}").c_str())).empty());                                   /* xp is optional */
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"uid\": \"12\"}]}").c_str())).empty());
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"uid\": \"../evil\"}]}").c_str())).empty());             /* a routing question, not a shape error */
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"uid\": 1.5}]}").c_str())) == "players[0].uid is not a string or int");
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"uid\": null}]}").c_str())) == "players[0].uid is not a string or int");
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"xp\": 5}]}").c_str())) == "players[0].xp is not an object");
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"xp\": {\"total\": \"12\"}}]}").c_str())).empty());        /* int("12") */
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"xp\": {\"total\": \"x\"}}]}").c_str())) == "players[0].xp.total is not an int");
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"xp\": {\"applied_live\": null}}]}").c_str())) == "players[0].xp.applied_live is not an int");
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"lifetime\": []}]}").c_str())) == "players[0].lifetime is not an object");
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"lifetime\": {\"kills\": 2, \"madeUp\": 9}}]}").c_str())).empty()); /* the key set is open */
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"lifetime\": {\"kills\": 2.5}}]}").c_str())) == "players[0].lifetime.kills is not an int");
    CHECK(RwbValidate(Parsed((std::string(head) + ", \"lifetime\": {\"kills\": true}}]}").c_str())) == "players[0].lifetime.kills is not an int");
    /* a bot row is not inspected past its three required members */
    CHECK(RwbValidate(Parsed("{\"format\": 1, \"players\": [{\"slot\": 0, \"name\": \"b\", \"bot\": true, \"xp\": 5, \"lifetime\": 7}]}")).empty());
}

} // namespace

int DwCareerSelfTest(const Config &base)
{
    Config cfg = base;
    if (cfg.profile.empty() || cfg.matchProfile.empty()) {
        fprintf(stderr, "--selftest-career needs --profile and --match-profile <empty scratch folders> before it\n");
        return 2;
    }
    if (!ListDir(cfg.profile).empty() || !ListDir(cfg.matchProfile).empty()) {
        fprintf(stderr, "--selftest-career: the profile / match-profile folder is not empty -- refusing to test in it\n");
        return 2;
    }
    cfg.fixedTime = 1790000000;
    cfg.differential = false;
    QuietHost host;
    const std::string A = Report(HumanRow(0, "edtest", U1, 2500, 9, 3, 2,
                                          ", \"lifetime\": {\"kills\": 9, \"score\": 1250, \"timePlayedTotal\": 540}, \"progression\": "
                                          "{\"challenges\": {\"ch_ghillie\": 50}, \"weapons\": {\"ak47_mp\": 1600}, \"gp\": 150}"));
    const std::string B = Report(HumanRow(0, "edtest", U1, 300, 1, 1, 0), "scorelimit");
    Bytes blobBefore, blobAfter, exported;
    {
        Server s(cfg, &host);
        printf("career: levels and tables\n");
        Levels(s);
        Tables(s);
        Validation();

        printf("career: write-back\n");
        CHECK(RankWritebackMaybeApply(s, U1, false).empty()); /* nothing waiting */
        Json prof;
        CHECK(ProfileLoad(s, U1, prof) && prof.get("level").asInt() == 1);
        EnsureBlob(s, U1);
        CHECK(GetStore(s).GetStorageFile(U1, "mpdata_s", blobBefore));
        CHECK(Drop(cfg, A));
        std::string note = RankWritebackMaybeApply(s, 0, true); /* the poller: the row names its account */
        CHECK(note.find(UidText(U1) + " +2500 XP level 1->2") != std::string::npos && note.find("RANK UP") != std::string::npos);
        CHECK(ProfileLoad(s, U1, prof, false) && prof.get("total_xp").asInt() == 2500 && prof.get("level").asInt() == 2);
        CHECK(prof.get("name").str() == "edtest" && prof.get("matches").asInt() == 1 && prof.get("kills").asInt() == 9 &&
              prof.get("deaths").asInt() == 3 && prof.get("assists").asInt() == 2);
        CHECK(prof.get("lifetime").get("score").asInt() == 1250 && prof.get("lifetime").get("timePlayedTotal").asInt() == 540);
        CHECK(prof.get("challenges").get("60").asReal() == 50.0 && prof.get("weapons").get("ak47_mp").asInt() == 1600);
        CHECK(prof.get("rankup_gp_level").asInt() == 2);
        CHECK(GetStore(s).WalletGet(U1, 1, -1) == 150 + 400); /* the server's GP plus level 2's rank-up GP */
        CHECK(note.find("+550 GP") != std::string::npos);
        /* the folded report changed what EnsureBlob renders, and the match server's copy follows */
        int64_t xp = -1;
        CHECK(GetStore(s).GetStorageFile(U1, "mpdata_s", blobAfter) && blobAfter != blobBefore && XpFromBlob(s, blobAfter, xp) && xp == 2500);
        PdBlob pd;
        double v = -1;
        CHECK(pd.Load(s, blobAfter) && pd.GetPath("rank", v) && v == 2 && pd.GetPath("experience", v) && v == 2500 - RankToXp(s, 2, 0));
        CHECK(pd.GetPath("kills", v) && v == 9 && pd.GetPath("score", v) && v == 1250 && pd.GetPath("challenges[60]", v) && v == 50.0);
        CHECK(EnsureBlob(s, U1).empty()); /* current */
        CHECK(ReadFile(PathJoin(cfg.matchProfile, "mpdata_s." + UidText(U1)), exported) && exported == blobAfter);
        CHECK(!FileExists(PathJoin(cfg.matchProfile, "match_report.json")) && CountFiles(cfg, "match_report.applied.") == 1);
        CHECK(FileExists(PathJoin(cfg.matchProfile, "match_report.journal.log")));
        CHECK(GetStore(s).HasReport(RwbReportId(A)));

        /* the same bytes again: archived as a duplicate, nothing credited */
        CHECK(Drop(cfg, A));
        note = RankWritebackMaybeApply(s, U1, false);
        CHECK(note.find("duplicate report") != std::string::npos && Xp(s, U1) == 2500 && CountFiles(cfg, "match_report.dup.") == 1);
        CHECK(ProfileLoad(s, U1, prof, false) && prof.get("matches").asInt() == 1 && GetStore(s).WalletGet(U1, 1, -1) == 550);
    }
    {
        Server s(cfg, &host); /* a restart: the retry state and the ledger come back from the store */
        CHECK(Drop(cfg, A));
        std::string note = RankWritebackMaybeApply(s, U1, false);
        CHECK(note.find("duplicate report") != std::string::npos && Xp(s, U1) == 2500 && CountFiles(cfg, "match_report.dup.") == 2);
        /* a newer report moves the retry state on; the ledger still refuses the old one */
        CHECK(Drop(cfg, B));
        note = RankWritebackMaybeApply(s, U1, false);
        CHECK(note.find("+300 XP") != std::string::npos && Xp(s, U1) == 2800);
        CHECK(Drop(cfg, A));
        note = RankWritebackMaybeApply(s, U1, false);
        CHECK(note.find("duplicate report") != std::string::npos && note.find("ledger") != std::string::npos && Xp(s, U1) == 2800);
        CHECK(CountFiles(cfg, "match_report.dup.") == 3 && GetStore(s).WalletGet(U1, 1, -1) == 550);
    }
    {
        Server s(cfg, &host);
        CHECK(Drop(cfg, A));
        CHECK(RankWritebackMaybeApply(s, U1, false).find("duplicate report") != std::string::npos && Xp(s, U1) == 2800);

        /* a malformed report is archived loudly and never applied */
        CHECK(Drop(cfg, "{\"format\": 1, \"players\": [{\"slot\": 0"));
        std::string note = RankWritebackMaybeApply(s, U1, false);
        CHECK(note.find("BAD REPORT") != std::string::npos && CountFiles(cfg, "match_report.bad.") == 1 && Xp(s, U1) == 2800);
        CHECK(Drop(cfg, "{\"format\": 3, \"players\": [{\"slot\": 0, \"name\": \"x\", \"bot\": false}]}"));
        CHECK(RankWritebackMaybeApply(s, U1, false).find("BAD REPORT") != std::string::npos && CountFiles(cfg, "match_report.bad.") == 2);

        /* a report whose rows name no account waits for a connection that has one */
        const std::string legacy = Report(HumanRow(0, "edtest", 0, 400, 2, 2, 0), "legacy");
        CHECK(Drop(cfg, legacy));
        CHECK(RankWritebackMaybeApply(s, 0, true).empty() && FileExists(PathJoin(cfg.matchProfile, "match_report.json")));
        CHECK(RankWritebackMaybeApply(s, U2, false).find(UidText(U2) + " +400 XP") != std::string::npos && Xp(s, U2) == 400 && Xp(s, U1) == 2800);
        /* ...and with no connection uid either, the shared blob takes the part the live pump did not persist */
        CHECK(Drop(cfg, Report("{\"slot\": 0, \"name\": \"x\", \"bot\": false, \"xp\": {\"total\": 900, \"applied_live\": 200}}", "shared")));
        note = RankWritebackMaybeApply(s, 0, false);
        Bytes shared;
        int64_t sharedXp = -1;
        CHECK(note.find("mpdata_s +700 XP") != std::string::npos && GetStore(s).GetStorageFile(0, "mpdata_s", shared) && XpFromBlob(s, shared, sharedXp));

        /* two humans: each profile takes its own row; the connection (a third account) takes nothing */
        CHECK(Drop(cfg, Report(HumanRow(0, "edtest", U1, 100, 1, 0, 0) + ", " + HumanRow(1, "other", U3, 700, 4, 7, 1), "two")));
        note = RankWritebackMaybeApply(s, U2, false);
        CHECK(Xp(s, U1) == 2900 && Xp(s, U3) == 700 && Xp(s, U2) == 400);
        Json prof;
        CHECK(ProfileLoad(s, U3, prof, false) && prof.get("name").str() == "other" && prof.get("kills").asInt() == 4 && prof.get("matches").asInt() == 1);
        CHECK(FileExists(PathJoin(cfg.matchProfile, "mpdata_s." + UidText(U3))));

        /* a zero-XP match is still a match: the lifetime members and the match counter move */
        CHECK(Drop(cfg, Report(HumanRow(0, "edtest", U3, 0, 0, 5, 0, ", \"lifetime\": {\"deaths\": 5, \"timePlayedTotal\": 60}"), "zero")));
        note = RankWritebackMaybeApply(s, 0, true);
        CHECK(ProfileLoad(s, U3, prof, false) && prof.get("total_xp").asInt() == 700 && prof.get("matches").asInt() == 2 &&
              prof.get("lifetime").get("deaths").asInt() == 5);
        /* ...and one with nothing at all is consumed without a credit */
        CHECK(Drop(cfg, Report(HumanRow(0, "edtest", U3, 0, 0, 0, 0), "empty")));
        CHECK(RankWritebackMaybeApply(s, 0, true).find("nothing to add") != std::string::npos);
        CHECK(ProfileLoad(s, U3, prof, false) && prof.get("matches").asInt() == 2);

        /* a progression row the tables refuse costs neither the credit nor a retry loop */
        CHECK(Drop(cfg, Report(HumanRow(0, "edtest", U3, 50, 0, 0, 0, ", \"progression\": {\"collectibles\": {\"abc\": 1, \"4\": 1}}"), "odd")));
        note = RankWritebackMaybeApply(s, 0, true);
        CHECK(note.find("RETAINED") == std::string::npos && note.find("unusable collectible") != std::string::npos && Xp(s, U3) == 750);
        CHECK(!FileExists(PathJoin(cfg.matchProfile, "match_report.json")));
        CHECK(ProfileLoad(s, U3, prof, false) && prof.get("collectibles").size() == 1 && prof.get("collectibles").at(0).asInt() == 4);

        /* the spool is folded oldest first by the once-a-second poller */
        CHECK(Drop(cfg, Report(HumanRow(0, "edtest", U3, 20, 0, 0, 0), "q2"), "match_report.q.1790000001.17760.2.json"));
        CHECK(Drop(cfg, Report(HumanRow(0, "edtest", U3, 10, 0, 0, 0), "q1"), "match_report.q.1790000001.17760.1.json"));
        CHECK(Drop(cfg, "half a report", "match_report.q.1790000001.17760.3.json.tmp"));
        host.lines.clear();
        s.Tick();
        CHECK(Xp(s, U3) == 780 && CountFiles(cfg, "match_report.q.") == 1); /* the in-flight .tmp is not a report */
        bool logged = false;
        for (size_t i = 0; i < host.lines.size(); i++)
            if (host.lines[i].find("match report poller") != std::string::npos && host.lines[i].find("q1") < host.lines[i].find("q2")) logged = true;
        CHECK(logged);
    }
    printf("%d checks, %d failed\n%s\n", g_checks, g_fail, g_fail ? "SELFTEST-CAREER FAIL" : "SELFTEST-CAREER PASS");
    return g_fail ? 1 : 0;
}

/* ---- dwtest --writeback ---- */

static std::string InflatedHex(const Bytes &stored)
{
    Bytes raw;
    if (stored.empty() || !Inflate(stored, raw)) return "";
    return Hex(raw.data(), raw.size());
}

int DwWritebackRun(const Config &base, const char *uidText, bool background, const char *createUid)
{
    Config cfg = base;
    QuietHost host;
    uint64_t uid = strtoull(uidText, 0, 10), fresh = strtoull(createUid, 0, 10);
    Server s(cfg, &host);
    Store &store = GetStore(s);
    Json prof;
    if (fresh) ProfileLoad(s, fresh, prof, true);
    Json out = Json::MakeObject();
    out["note"] = RankWritebackMaybeApply(s, uid, background);
    Json profiles = Json::MakeObject(), wallet = Json::MakeObject(), blobs = Json::MakeObject(), exported = Json::MakeObject();
    Json flags = Json::MakeObject(), weaponXp = Json::MakeObject();
    std::vector<uint64_t> uids = ProfileListUids(s);
    for (size_t i = 0; i < uids.size(); i++) {
        const std::string key = UidText(uids[i]);
        if (!ProfileLoad(s, uids[i], prof, false)) continue;
        profiles[key] = prof;
        wallet[key] = store.WalletHas(uids[i], 1) ? Json((long long)store.WalletGet(uids[i], 1)) : Json();
        Bytes blob, copy;
        blobs[key] = store.GetStorageFile(uids[i], "mpdata_s", blob) ? InflatedHex(blob) : "";
        if (ReadFile(PathJoin(cfg.matchProfile, "mpdata_s." + key), copy)) exported[key] = Json(copy == blob);
        Json ids = Json::MakeArray(), xp = Json::MakeObject();
        std::vector<int64_t> granted = CareerGrantedFlagIds(s, prof);
        for (size_t k = 0; k < granted.size(); k++) ids.push(Json((long long)granted[k]));
        std::map<int64_t, int64_t> byItem = CareerWeaponXpByItem(s, prof);
        for (std::map<int64_t, int64_t>::iterator it = byItem.begin(); it != byItem.end(); ++it)
            xp[Fmt("%lld", (long long)it->first)] = Json((long long)it->second);
        flags[key] = ids;
        weaponXp[key] = xp;
    }
    out["flags"] = flags;
    out["weapon_xp"] = weaponXp;
    out["profiles"] = profiles;
    out["wallet"] = wallet;
    out["blobs"] = blobs;
    out["exported_matches_store"] = exported;
    Bytes shared;
    out["shared"] = store.GetStorageFile(0, "mpdata_s", shared) ? InflatedHex(shared) : "";
    out["state"] = store.KvGet("rank_writeback.state");
    Json files = Json::MakeArray();
    std::vector<std::string> names = ListDir(cfg.matchProfile);
    std::sort(names.begin(), names.end());
    for (size_t i = 0; i < names.size(); i++) files.push(names[i]);
    out["files"] = files;
    printf("%s\n", out.dump(1).c_str());
    return 0;
}

/* ---- dwtest --career: what a players folder holds, read the way the lobby reads it at login ---- */

int DwCareerDump(const Config &base)
{
    Config cfg = base;
    cfg.matchProfile.clear(); /* read only: no report is folded, nothing but a missing account.txt is written */
    QuietHost host;
    Server s(cfg, &host);
    Store &store = GetStore(s);
    std::vector<uint64_t> uids = ProfileListUids(s);
    Json out = Json::MakeObject();
    for (size_t i = 0; i < uids.size(); i++) {
        Json prof, row = Json::MakeObject(), wallet = Json::MakeObject();
        if (!ProfileLoad(s, uids[i], prof, false)) continue;
        std::map<int, int64_t> bal = store.WalletBalances(uids[i]);
        for (std::map<int, int64_t>::iterator it = bal.begin(); it != bal.end(); ++it) wallet[Fmt("%d", it->first)] = Json((long long)it->second);
        int idx = 0;
        int64_t in = 0;
        XpToRank(s, prof.get("total_xp").asInt(), idx, in);
        row["folder"] = store.FolderOf(uids[i]);
        row["level"] = prof.get("level");
        row["total_xp"] = prof.get("total_xp");
        row["xp_into_rank"] = Json((long long)in);
        row["xp_to_next"] = Json((long long)(RankXpToNext(s, idx) - in));
        row["wallet"] = wallet;
        row["inventory_rows"] = Json((long long)store.InventoryList(uids[i], false).size());
        row["profile"] = prof;
        out[UidText(uids[i])] = row;
    }
    printf("%s\n", out.dump(1).c_str());
    return 0;
}

#endif /* DW_HAVE_CAREER */

/*
 * dwtest --selftest-store: the persistent store, the playerdata codec and the profile render, with
 * no network. Needs --resources <the lobby's resources folder> and --profile <an EMPTY scratch folder>
 * (and optionally --match-profile) BEFORE the flag:
 *
 *   dwtest.exe --resources R --profile $DWDIFF_ROOT/slot1/selftest/profile \
 *              --match-profile $DWDIFF_ROOT/slot1/selftest/match --selftest-store
 */
#include <stdio.h>
#include <string.h>
#include "../../src/offline/dw.h"
#include "../../src/offline/dw_playerdata.h"
#include "../../src/offline/dw_profiles.h"
#include "../../src/offline/dw_state.h"
#include "../../src/offline/dw_store.h"

using namespace dw;

/* MEASURED against the Python lobby (store/playerdata_editor.py, dw_marketplace._equipped_item_ids) on
 * the shipped resources: the CRC32 of each def's expanded layout text, of the level-1 seed's equipped
 * item id list and of mpdata.unlockall.proposed's buffer after the legacy character rows are moved. */
static const uint32_t kLayoutCrcPublic = 0xfaa6bba8u, kLayoutCrcSecure = 0x40fcee9eu, kEquippedCrc = 0xe2ce32f1u,
                      kUnlockallNormalisedCrc = 0x6ee8723du;

static int g_fail, g_checks;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static bool TmpLeft(const std::string &dir)
{
    std::vector<std::string> names = ListDir(dir);
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i].find(".tmp") != std::string::npos) return true;
        if (names[i].find('.') == std::string::npos && TmpLeft(PathJoin(dir, names[i]))) return true;
    }
    return false;
}

static void StoreWrites(Server &s)
{
    Store &st = GetStore(s);
    const uint64_t A = 9000000000000000001ULL, B = 4242, C = 77;

    /* the player's folder: the account that logs in takes the local profile name, once; anybody else
     * (and a name that is not a usable folder) is named by the uid */
    CHECK(st.FolderOf(A) == "9000000000000000001");
    st.TouchLogin(A, "", "");
    CHECK(st.FolderOf(A) == s.cfg.localName && FileExists(PathJoin(PathJoin(s.cfg.profile, s.cfg.localName), "account.txt")));
    st.TouchLogin(C, "", ""); /* the name is taken */
    CHECK(st.FolderOf(C) == "77" && FileExists(PathJoin(PathJoin(s.cfg.profile, "77"), "account.txt")));
    CHECK(!st.BindFolder(B, "..") && !st.BindFolder(B, "a/b") && !st.BindFolder(B, "con") && !st.BindFolder(B, "nul.txt") &&
          !st.BindFolder(B, ".hidden") && !st.BindFolder(B, "dot.") && !st.BindFolder(B, "") && !st.BindFolder(B, "has space"));
    CHECK(!st.BindFolder(B, s.cfg.localName) && !st.BindFolder(B, "TESTER_1") && !st.BindFolder(B, "77") && !st.BindFolder(B, "123"));
    CHECK(!st.BindFolder(A, "other")); /* a uid keeps its folder */
    CHECK(st.FolderOf(B) == "4242");

    /* wallet: credit, clamp to u32, debit, short debit, allowNegative, seed */
    CHECK(!st.WalletHas(A, 1));
    CHECK(st.WalletGet(A, 1, -7) == -7);
    CHECK(st.WalletCredit(A, 1, 500, 1000) == 1500); /* a missing row starts at the seed */
    CHECK(st.WalletCredit(A, 1, 250) == 1750);
    int64_t after = 0;
    CHECK(st.WalletDebit(A, 1, 750, 0, false, &after) && after == 1000);
    CHECK(!st.WalletDebit(A, 1, 1001, 0, false, &after) && after == 1000);
    CHECK(st.WalletGet(A, 1) == 1000);
    CHECK(st.WalletDebit(A, 2, 30, 10, true, &after) && after == -20);
    CHECK(st.WalletCredit(A, 3, 0x7FFFFFFFFFLL) == 0xFFFFFFFFLL);
    st.WalletSet(A, 4, -5);
    CHECK(st.WalletGet(A, 4, 99) == 0);
    CHECK(st.WalletBalances(A).size() == 4);
    CHECK(!st.WalletHas(B, 1));

    /* inventory: stacking, refresh, consume, expiry, shared inheritance, replace, revoke */
    CHECK(!st.InventoryExists(A));
    st.InventoryGrant(0, 700, 1, 0, "seed", 0, false);  /* shared rows */
    st.InventoryGrant(0, 100, 9, 0, "seed", 0, false);
    st.InventoryGrant(A, 100, 2, 5000, "grant", 11, true);
    st.InventoryGrant(A, 100, 3, 4000, "grant", 12, true);  /* stack: 5, the later expiry stays */
    InvRow r;
    CHECK(st.InventoryGet(A, 100, r) && r.quantity == 5 && r.expiresAt == 5000 && r.skuId == 12);
    st.InventoryGrant(A, 200, 1, 9000, "purchase", 0, false);
    st.InventoryGrant(A, 200, 1, 9500, "purchase", 0, false); /* no stack: own it, refresh the expiry */
    CHECK(st.InventoryGet(A, 200, r) && r.quantity == 1 && r.expiresAt == 9500);
    st.InventoryGrant(A, 300, 1, 100, "purchase", 0, false);
    CHECK(st.InventoryExists(A));
    std::vector<InvRow> all = st.InventoryList(A);
    CHECK(all.size() == 4 && all[0].itemId == 100 && all[0].quantity == 5 && all[3].itemId == 700); /* own row wins */
    CHECK(st.InventoryList(A, false).size() == 3);
    CHECK(st.InventoryList(A, true, 4999).size() == 3);  /* 300 expired at 100 */
    CHECK(st.InventoryList(A, true, 5000).size() == 2);  /* 100 expires at exactly 5000 */
    CHECK(st.InventoryList(B).size() == 2 && !st.InventoryExists(B));
    CHECK(st.InventoryConsume(A, 100, 2, true) == 3);
    CHECK(st.InventoryConsume(A, 100, 4, true) == -1);   /* short: nothing consumed */
    CHECK(st.InventoryConsume(A, 999, 1, true) == -1);
    CHECK(st.InventoryConsume(A, 100, 3, false) == 0 && st.InventoryGet(A, 100, r) && r.quantity == 0);
    CHECK(st.InventoryConsume(A, 200, 1, true) == 0 && !st.InventoryGet(A, 200, r));
    CHECK(st.InventoryRevoke(A, 300) && !st.InventoryRevoke(A, 300));
    std::vector<InvRow> rows(2);
    rows[0].itemId = 100; rows[0].quantity = 7; rows[0].expiresAt = 0; rows[0].acquiredAt = 55;
    rows[1].itemId = 400; rows[1].quantity = 1; rows[1].expiresAt = 77;
    st.InventoryReplace(A, rows, "purchase");
    CHECK(st.InventoryList(A, false).size() == 2);
    CHECK(st.InventoryGet(A, 100, r) && r.quantity == 7 && r.source == "grant" && r.acquiredAt == 55);
    CHECK(st.InventoryGet(A, 400, r) && r.source == "purchase" && r.acquiredAt == (double)s.Now());

    /* storage files: a player's mpdata / mpdata_s are files in the folder, everything else is memory */
    Bytes blob("\x78\x01\x00\xff binary \x00 data", 19), got;
    CHECK(!st.GetStorageFile(A, "mpdata", got));
    CHECK(st.PutStorageFile(A, "mpdata", blob, "upload"));
    CHECK(FileExists(PathJoin(PathJoin(s.cfg.profile, s.cfg.localName), "mpdata")));
    CHECK(st.PutStorageFile(0, "blacklist", "shared", "upload"));
    CHECK(st.PutStorageFile(0, "uploads/odd.bin", "x", "upload"));
    CHECK(st.GetStorageFile(A, "mpdata", got) && got == blob);
    CHECK(!st.GetStorageFile(A, "blacklist", got) && st.GetStorageFile(A, "blacklist", got, true) && got == "shared");
    CHECK(!st.GetStorageFile(B, "mpdata", got, true));
    CHECK(!st.PutStorageFile(A, "../escape", "x", "upload") && !st.PutStorageFile(A, "a:b", "x", "upload"));
    CHECK(st.PutStorageFile(A, "Recentmetlist", "this run only", "upload") && st.GetStorageFile(A, "Recentmetlist", got) && got == "this run only");
    CHECK(st.PutStorageFile(C, "mpdata_s", "not a profile", "upload") && FileExists(PathJoin(PathJoin(s.cfg.profile, "77"), "mpdata_s")));
    st.QuarantineStorageFile(C, "mpdata_s");
    CHECK(!st.GetStorageFile(C, "mpdata_s", got) && !FileExists(PathJoin(PathJoin(s.cfg.profile, "77"), "mpdata_s")) &&
          FileExists(PathJoin(PathJoin(s.cfg.profile, "77"), "mpdata_s.unreadable")));
    CHECK(st.PutStorageFile(C, "mpdata", "x", "upload") && st.DeleteStorageFile(C, "mpdata") && !st.DeleteStorageFile(C, "mpdata") &&
          !FileExists(PathJoin(PathJoin(s.cfg.profile, "77"), "mpdata")));

    /* the profile cache and its extras (account.txt career lines), kv, reports */
    Json prof = Json::MakeObject(), extras = Json::MakeObject(), back;
    prof["format"] = 1; prof["uid"] = "4242"; prof["total_xp"] = 12345; prof["name"] = "caf\xc3\xa9 \"q\"";
    extras["name"] = prof["name"];
    extras["big"] = Json(18446744073709551615ULL);
    extras["weapons"] = Json::MakeObject();
    extras["weapons"]["m4_mp"] = 5210;
    extras["collectibles"] = Json::MakeArray();
    extras["collectibles"].push(Json(3));
    CHECK(!st.ProfileRead(B, back) && st.ProfileExtras(B).members().empty());
    CHECK(st.ProfileWrite(B, prof, extras) && st.ProfileRead(B, back) && back == prof && st.ProfileExtras(B) == extras);
    st.KvSet("rank_writeback.state", prof);
    st.KvSet("n", Json(7));
    CHECK(st.KvGet("n").asInt() == 7 && st.KvGet("missing").isNull());
    Json rep = Json::MakeObject();
    rep["reason"] = "timelimit"; rep["duration_s"] = 600.5;
    CHECK(!st.HasReport("abc"));
    st.RecordMatchReport("abc", rep, false);
    st.RecordMatchReport("abc", Json::MakeObject(), true); /* idempotent on the id */
    Json cr = Json::MakeObject();
    cr["0"] = 900;
    st.MarkReportApplied("abc", cr);
    CHECK(st.HasReport("abc"));
}

static void StoreReads(Server &s)
{
    Store &st = GetStore(s);
    const uint64_t A = 9000000000000000001ULL, B = 4242;
    InvRow r;
    Bytes got;
    Json back;
    CHECK(st.WalletGet(A, 1) == 1000 && st.WalletGet(A, 2) == -20 && st.WalletGet(A, 3) == 0xFFFFFFFFLL && st.WalletGet(A, 4, 9) == 0);
    CHECK(st.InventoryList(A, false).size() == 2 && st.InventoryList(A).size() == 2); /* the shared rows were this run's */
    CHECK(st.InventoryGet(A, 100, r) && r.quantity == 7 && r.source == "grant" && r.acquiredAt == 55);
    CHECK(st.InventoryGet(A, 400, r) && r.expiresAt == 77 && r.source == "purchase");
    /* the folders are found again by the uid in their account.txt, before anybody logs in */
    CHECK(st.FolderOf(A) == s.cfg.localName && st.FolderOf(B) == "4242" && st.FolderOf(77) == "77");
    st.TouchLogin(A, "", "");
    CHECK(st.FolderOf(A) == s.cfg.localName);
    CHECK(st.GetStorageFile(A, "mpdata", got) && got == Bytes("\x78\x01\x00\xff binary \x00 data", 19));
    /* what is not a player's file did not outlive the run */
    CHECK(!st.GetStorageFile(0, "blacklist", got) && !st.GetStorageFile(0, "uploads/odd.bin", got) && !st.GetStorageFile(0, "mpdata", got));
    CHECK(!st.GetStorageFile(A, "Recentmetlist", got) && !st.GetStorageFile(77, "mpdata_s", got));
    CHECK(st.InventoryList(0, false).empty());
    /* the profile itself is memory only (dw_profiles.cpp rebuilds it from mpdata_s); its extras are kept */
    CHECK(!st.ProfileRead(B, back));
    Json ex = st.ProfileExtras(B);
    CHECK(ex.get("name").str() == "caf\xc3\xa9 \"q\"" && ex.get("big").asUInt() == 18446744073709551615ULL);
    CHECK(ex.get("weapons").get("m4_mp").asInt() == 5210 && ex.get("collectibles").size() == 1 && ex.get("collectibles").at(0).asInt() == 3);
    CHECK(st.ProfileUids().empty()); /* nobody has an mpdata_s yet */
    if (!s.cfg.matchProfile.empty()) {
        CHECK(st.KvGet("n").asInt() == 7 && st.KvGet("rank_writeback.state").get("uid").str() == "4242");
        CHECK(st.HasReport("abc") && !st.HasReport("abd"));
        CHECK(FileExists(PathJoin(s.cfg.matchProfile, "match_report.state.json")));
    }
    CHECK(!TmpLeft(s.cfg.profile));
    /* nothing but the players' folders, and in them nothing but the three files */
    std::vector<std::string> top = ListDir(s.cfg.profile);
    CHECK(top.size() == 3);
    std::vector<std::string> mine = ListDir(PathJoin(s.cfg.profile, s.cfg.localName));
    CHECK(mine.size() == 2); /* mpdata + account.txt */
    Bytes text;
    CHECK(ReadFile(PathJoin(PathJoin(s.cfg.profile, s.cfg.localName), "account.txt"), text));
    CHECK(text.find("uid = 9000000000000000001\n") != std::string::npos && text.find("wallet.1 = 1000\n") != std::string::npos &&
          text.find("item = 100 7 0 0 55 grant\n") != std::string::npos && text.find("{") == std::string::npos);
}

static void Codec(Server &s)
{
    static const struct { const char *file, *key; } seeds[] = {
        { "mpdata.level1.proposed", "mpdata" }, { "mpdata_s.level1.proposed", "mpdata_s" },
        { "mpdata.unlockall.proposed", "mpdata" }, { "mpdata_s.unlockall.proposed", "mpdata_s" },
        { "mpdata.default", "mpdata" }, { "mpdata_s.saferank.proposed", "mpdata_s" },
    };
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
        Bytes raw, again, inflated;
        PdBlob b;
        std::string err;
        if (!s.Resource(seeds[i].file, raw)) { printf("  (no %s in the resources, skipped)\n", seeds[i].file); continue; }
        CHECK(b.Load(s, raw, "", &err));
        if (!b.ok()) { printf("  %s: %s\n", seeds[i].file, err.c_str()); continue; }
        CHECK(b.def->key == seeds[i].key);
        CHECK(Inflate(raw, inflated) && inflated.size() == PD_BUF_SIZE);
        bool normalized = inflated != b.buf; /* a legacy eye-first character row was moved on load */
        CHECK(b.StoredCrc() == b.ComputedCrc());
        CHECK(b.Store(again));
        /* decode -> encode gives identical bytes (the seeds are zlib level 1 streams of a CRC-correct buffer) */
        if (!normalized) CHECK(again == raw);
        else if (!strcmp(seeds[i].file, "mpdata.unlockall.proposed")) CHECK(Crc32(b.buf.data(), b.buf.size()) == kUnlockallNormalisedCrc);
        printf("  %-30s %s v%u, %u B stored, %s\n", seeds[i].file, b.def->key.c_str(), b.def->version, (unsigned)raw.size(),
               normalized ? "legacy character rows normalised" : again == raw ? "re-encodes byte-identically" : "RE-ENCODE DIFFERS");
    }

    const PdDef *sd = PlayerDataDef(s, "mpdata_s"), *md = PlayerDataDef(s, "mpdata");
    CHECK(sd && md);
    if (!sd || !md) return;
    /* The whole expanded layout, as one CRC32 over "path|kind|byte|bit|member|prop\n" per leaf: compare
     * with the same CRC32 over playerdata_editor.load_defs()[key].fields of the Python lobby. */
    static const char *const kinds[] = { "int", "byte", "bool", "float", "short", "enum", "string" };
    const PdDef *both[2] = { md, sd };
    for (int d = 0; d < 2 && both[d]; d++) {
        std::string all;
        for (size_t i = 0; i < both[d]->fields.size(); i++) {
            const PdField &f = both[d]->fields[i];
            all += f.path + "|" + kinds[f.kind] + "|" + Fmt("%d|", f.byte) + (f.bit < 0 ? std::string("None") : Fmt("%d", f.bit)) + "|" +
                   f.member + "|" + f.prop + "\n";
        }
        printf("  %-8s %u leaves, layout text %u B crc32 %08x\n", both[d]->key.c_str(), (unsigned)both[d]->fields.size(), (unsigned)all.size(),
               (unsigned)Crc32(all.data(), all.size()));
        /* MEASURED against the Python lobby's playerdata_editor on the shipped playerdata_defs.proposed.json */
        if (both[d]->leafCount == (d ? 1680 : 3998)) CHECK(Crc32(all.data(), all.size()) == (d ? kLayoutCrcSecure : kLayoutCrcPublic));
    }
    CHECK(sd->version == 11 && sd->formatChecksum == 0x096B8F57u && sd->leafCount == 1680 && (int)sd->fields.size() == 1680);
    CHECK(md->version == 220 && md->formatChecksum == 0xE4FD5218u && md->leafCount == 3998);
    /* MEASURED offsets (playerdata_audit.md 4): rank @0x13EC, experience @0x1130, rankVersion @0x13F0 */
    CHECK(sd->Find("rank") && sd->Find("rank")->byte == 0x13EC && sd->Find("rank")->kind == PD_INT);
    CHECK(sd->Find("experience") && sd->Find("experience")->byte == 0x1130);
    CHECK(sd->Find("rankVersion") && sd->Find("rankVersion")->byte == 0x13F0);
    CHECK(sd->Find("challenges[999]") && sd->Find("challenges[999]")->byte == 0x1110 && sd->Find("challenges[0]")->kind == PD_FLOAT);
    CHECK(sd->Find("isTutor") && sd->Find("isTutor")->kind == PD_BOOL && sd->Find("isTutor")->byte == 0x1A0C && sd->Find("isTutor")->bit == 0);
    CHECK(sd->Find("kills") && sd->Find("kills")->byte == 0x114C && sd->Find("kills")->kind == PD_FLOAT); /* a recovered name */
    CHECK(sd->ArrayCount("challenges") == 1000 && sd->ArrayCount("rank") == -1);
    /* the four bool members of mpdata are BIT offsets (the standing trap) */
    CHECK(md->Find("collectibles[0]") && md->Find("collectibles[0]")->byte == 0x0F69 && md->Find("collectibles[0]")->bit == 0);
    CHECK(md->Find("collectibles[9]") && md->Find("collectibles[9]")->byte == 0x0F6A && md->Find("collectibles[9]")->bit == 1);
    CHECK(md->Find("characterClasses[0].slots[0]") && md->Find("characterClasses[0].slots[0]")->byte == PD_CHARACTER_CLASSES_OFF);
    CHECK(md->Find("weaponClasses[0].primaryBase") && md->Find("weaponClasses[0].primaryBase")->byte == 0x1511 + 27 * 2);
    CHECK(md->Find("zmCharacterIndex") && md->Find("zmCharacterIndex")->byte == PD_ZM_CHARACTER_INDEX_OFF);
    CHECK(!md->Find("rank") && !sd->Find("no.such[3].member"));

    /* set / get of rank and experience, CRC fix, container round trip */
    Bytes raw, out;
    PdBlob b, c;
    double v = -1;
    CHECK(s.Resource("mpdata_s.level1.proposed", raw) && b.Load(s, raw));
    if (!b.ok()) return;
    CHECK(b.GetPath("rank", v) && v == 1);
    CHECK(b.GetPath("experience", v) && v == 0);
    CHECK(b.SetPath("rank", 31) && b.SetPath("experience", 36649) && b.SetPath("challenges[60]", 120.5));
    CHECK(b.SetPath("isTutor", 1) && b.GetPath("isTutor", v) && v == 1 && b.SetPath("isTutor", 0));
    CHECK(!b.SetPath("rank", 4294967296.0) && !b.SetPath("nope", 1));
    CHECK(b.SetPath("rankVersion", 0xFFFFFFFFu) && b.GetPath("rankVersion", v) && v == -1 && b.SetPath("rankVersion", 2));
    CHECK(b.StoredCrc() != b.ComputedCrc());
    CHECK(b.Store(out) && b.StoredCrc() == b.ComputedCrc());
    CHECK(out.size() > 2 && out[0] == 0x78 && out[1] == 0x01); /* the client's own zlib header */
    CHECK(c.Load(s, out) && c.def->key == "mpdata_s" && c.buf == b.buf);
    CHECK(c.GetPath("rank", v) && v == 31 && c.GetPath("experience", v) && v == 36649 && c.GetPath("challenges[60]", v) && v == 120.5);
    CHECK(PdU32(c.buf, 0x13EC) == 31 && PdU32(c.buf, 0x1130) == 36649);
    int64_t xp = 0;
    CHECK(XpFromBlob(s, out, xp) && xp == 457750 + 36649); /* rank 31 = rankTable index 30 (csm1), minXP 457750 */
    CHECK(c.Blank(s, "mpdata") && PdIdentify(s, c.buf) == "mpdata" && c.StoredCrc() == c.ComputedCrc());
    CHECK(!c.Load(s, "not zlib") && !c.Load(s, raw.substr(0, raw.size() / 2)));

    /* weaponClasses item ids, the equipped set of the level-1 seed */
    CHECK(PdClassItemId("perks", 3004) == 3003 && PdClassItemId("perks", 1) == -1 && PdClassItemId("perks", 0) == -1);
    CHECK(PdClassItemId("primaryBase", 1001) == 1001 && PdClassItemId("primaryBase", -1) == -1 && PdClassItemId("primaryAttach", 0) == -1);
    CHECK(PdClassItemRaw("lethal", 1200) == 1201 && PdClassItemRaw("lethal", -1) == 1 && PdClassItemRaw("secondaryBase", -1) == -1);
    Bytes seed;
    if (s.Resource("mpdata.level1.proposed", seed)) {
        std::set<int> ids = PdEquippedItemIds(s, seed);
        CHECK(!ids.empty() && ids.count(3003) && ids.count(1200) && !ids.count(0)); /* classtable defaults: scavenger, frag */
        std::string text;
        for (std::set<int>::iterator it = ids.begin(); it != ids.end(); ++it) text += (text.empty() ? "" : ",") + Fmt("%d", *it);
        printf("  the level-1 mpdata seed references %u item ids (list crc32 %08x)\n", (unsigned)ids.size(), (unsigned)Crc32(text.data(), text.size()));
        CHECK(ids.size() == 49 && Crc32(text.data(), text.size()) == kEquippedCrc); /* MEASURED: the Python lobby's _equipped_item_ids set */
    }
    CHECK(PdItemNames(s).count(171) && PdItemNames(s).find(171)->second.name == "hea_zmcowboy");
}

static void Profiles(Server &s)
{
    const uint64_t U = 7700000000000000077ULL;
    Json prof;
    int idx;
    int64_t in;
    CHECK(RankCount(s) == 70);
    CHECK(XpToRank(s, 0, idx, in) && idx == 0 && in == 0);
    CHECK(XpToRank(s, 1999, idx, in) && idx == 0 && in == 1999);
    CHECK(XpToRank(s, 2000, idx, in) && idx == 1 && in == 0);
    CHECK(XpToRank(s, 457750, idx, in) && idx == 30 && in == 0);
    CHECK(XpToRank(s, 1000000000, idx, in) && idx == 69 && in == RankXpToNext(s, 69) - 1); /* clamped inside maxrank */
    CHECK(RankToXp(s, 31, 100) == 457850 && LevelForXp(s, 2000) == 2);
    CHECK(RankupGp(1, 2) == 400 && RankupGp(4, 6) == 200 * 5 + 200 * 6 && RankupGp(5, 5) == 0);

    CHECK(PlayerLevel(s, U) == 0 && !ProfileLoad(s, U, prof, false));
    CHECK(ProfileLoad(s, U, prof) && prof.get("level").asInt() == 1 && prof.get("seeded_from").str() == "level 1 (new account)");
    CHECK(prof.get("uid").str() == "7700000000000000077" && prof.get("format").asInt() == 1 && prof.get("lifetime").isObject());
    CHECK(PlayerLevel(s, U) == 1);

    /* the career file exists from the moment the profile does: creating the account wrote it */
    Bytes blob, seed, file;
    PdBlob b;
    double v = 0;
    const std::string dir = PathJoin(s.cfg.profile, "7700000000000000077");
    CHECK(ReadFile(PathJoin(dir, "mpdata_s"), file) && GetStore(s).GetStorageFile(U, "mpdata_s", blob) && file == blob);
    std::string note = EnsureBlob(s, U);
    CHECK(note.empty()); /* nothing left to render; this call only mirrors the shared copy */
    CHECK(GetStore(s).GetStorageFile(U, "mpdata_s", blob) && b.Load(s, blob) && b.def->key == "mpdata_s" && file == blob);
    if (!b.ok()) return;
    CHECK(b.GetPath("rank", v) && v == 1 && b.GetPath("experience", v) && v == 0 && b.GetPath("rankVersion", v) && v == 2);
    /* the profile is authoritative over every slot the challenge table defines: the seed's 999999.0 reads 0 */
    CHECK(s.Resource("mpdata_s.level1.proposed", seed) && blob != seed);
    CHECK(b.GetPath("challenges[60]", v) && v == 0 && b.GetPath("challegeUpdateTime", v) && v == (double)s.Now());
    CHECK(EnsureBlob(s, U).empty()); /* already current: the idle path */
    Bytes shared;
    CHECK(GetStore(s).GetStorageFile(0, "mpdata_s", shared) && shared == blob); /* the shared mirror */
    /* the one shared copy that is a file: beside the match reports, never in the players folder */
    if (!s.cfg.matchProfile.empty()) CHECK(ReadFile(PathJoin(s.cfg.matchProfile, "mpdata_s"), file) && file == shared);
    CHECK(!FileExists(PathJoin(s.cfg.profile, "mpdata_s")));

    Json stats = Json::MakeObject(), life = Json::MakeObject();
    stats["kills"] = 7; stats["matches"] = 1;
    life["kills"] = 7; life["deaths"] = 2; life["hits"] = 30; life["totalShots"] = 120; life["score"] = 700; life["futureMember"] = 5;
    life["kdRatio"] = 99999; life["bogus"] = "x";
    RankupClaim claim;
    note = ProfileAddXp(s, U, 4100, "match", "Tester", stats, life, Json(), &claim);
    CHECK(note.find("level 1->3") != std::string::npos && note.find("RANK UP") != std::string::npos);
    CHECK(claim.gp == 200 * 2 + 200 * 3 && claim.from == 2 && claim.to == 3);
    CHECK(ProfileLoad(s, U, prof, false) && prof.get("total_xp").asInt() == 4100 && prof.get("level").asInt() == 3);
    CHECK(prof.get("kills").asInt() == 7 && prof.get("matches").asInt() == 1 && prof.get("name").str() == "Tester");
    CHECK(prof.get("lifetime").get("kills").asInt() == 7 && prof.get("lifetime").get("futureMember").asInt() == 5);
    CHECK(!prof.get("lifetime").has("kdRatio") && !prof.get("lifetime").has("bogus"));
    CHECK(prof.get("history").size() == 1 && prof.get("rankup_gp_level").asInt() == 3);
    CHECK(PlayerLevel(s, U) == 3);
    ProfileAddXp(s, U, 10, "again", "", Json(), Json(), Json(), &claim);
    CHECK(claim.gp == 0); /* each level is paid once */

    /* the credit is in the file already (no render was asked for) */
    CHECK(ReadFile(PathJoin(dir, "mpdata_s"), file) && b.Load(s, file));
    CHECK(b.GetPath("rank", v) && v == 3 && b.GetPath("experience", v) && v == 110 && b.GetPath("kills", v) && v == 7);
    note = EnsureBlob(s, U);
    CHECK(note.empty());
    CHECK(GetStore(s).GetStorageFile(U, "mpdata_s", blob) && b.Load(s, blob) && blob == file);
    CHECK(b.GetPath("rank", v) && v == 3 && b.GetPath("experience", v) && v == 110);
    CHECK(b.GetPath("kills", v) && v == 7 && b.GetPath("deaths", v) && v == 2 && b.GetPath("score", v) && v == 700);
    CHECK(b.GetPath("accuracy", v) && v == 2500 && b.GetPath("kdRatio", v) && v == 3500); /* derived, never reported */
    int64_t xp = 0;
    CHECK(XpFromBlob(s, blob, xp) && xp == 4110);

    /* an uploaded mpdata_s with MORE xp is absorbed, one with less never demotes */
    CHECK(b.SetPath("rank", 5) && b.SetPath("experience", 0) && b.Store(blob) && GetStore(s).PutStorageFile(U, "mpdata_s", blob, "upload"));
    CHECK(!AbsorbBlob(s, U).empty() && PlayerLevel(s, U) == 5);
    CHECK(b.SetPath("rank", 2) && b.Store(blob) && GetStore(s).PutStorageFile(U, "mpdata_s", blob, "upload"));
    CHECK(AbsorbBlob(s, U).empty() && PlayerLevel(s, U) == 5);
    /* ... and the file is put right at once, not at the next render */
    CHECK(ReadFile(PathJoin(dir, "mpdata_s"), file) && XpFromBlob(s, file, xp) && LevelForXp(s, xp) == 5);
    /* an upload that is no profile at all does not stay the player's career file */
    CHECK(GetStore(s).PutStorageFile(U, "mpdata_s", "garbage", "upload") && AbsorbBlob(s, U).empty());
    CHECK(ReadFile(PathJoin(dir, "mpdata_s"), file) && XpFromBlob(s, file, xp) && LevelForXp(s, xp) == 5);
    EnsureBlob(s, U);
    CHECK(GetStore(s).GetStorageFile(U, "mpdata_s", blob) && XpFromBlob(s, blob, xp) && LevelForXp(s, xp) == 5);

    /* the mirror owner: another account may not re-point the shared blob */
    const uint64_t V = 8800000000000000088ULL;
    SetMirrorOwner(s, U);
    EnsureBlob(s, V);
    CHECK(GetStore(s).GetStorageFile(0, "mpdata_s", shared) && XpFromBlob(s, shared, xp) && LevelForXp(s, xp) == 5);
    SetMirrorOwner(s, 0);
    EnsureBlob(s, V);
    CHECK(GetStore(s).GetStorageFile(0, "mpdata_s", shared) && XpFromBlob(s, shared, xp) && xp == 0);

    if (!s.cfg.matchProfile.empty()) {
        Bytes file;
        note = ExportMatchBlob(s, U);
        CHECK(note.find("wrote") != std::string::npos);
        CHECK(ReadFile(PathJoin(s.cfg.matchProfile, "mpdata_s.7700000000000000077"), file) && GetStore(s).GetStorageFile(U, "mpdata_s", blob) && file == blob);
    }
    std::vector<int> col;
    prof["collectibles"] = Json::MakeArray();
    prof["collectibles"].push(Json(23)); prof["collectibles"].push(Json("3")); prof["collectibles"].push(Json(5000)); prof["collectibles"].push(Json(23));
    col = ProfileCollectibles(prof);
    CHECK(col.size() == 2 && col[0] == 3 && col[1] == 23);
}

struct QuietHost : Host {
    virtual void Log(const std::string &line) { if (line.find("playerdata:") == 0 || line.find("profiles:") == 0) printf("  %s\n", line.c_str()); }
};

int DwStoreSelfTest(const Config &base)
{
    Config cfg = base;
    if (cfg.profile.empty()) { fprintf(stderr, "--selftest-store needs --profile <an empty scratch folder> before it\n"); return 2; }
    if (!ListDir(cfg.profile).empty()) { fprintf(stderr, "--selftest-store: %s is not empty -- refusing to test in it\n", cfg.profile.c_str()); return 2; }
    cfg.fixedTime = 1790000000;
    cfg.localName = "Tester_1";
    QuietHost host;
    {
        Server s(cfg, &host);
        printf("store: writes\n");
        StoreWrites(s);
    }
    {
        Server s(cfg, &host); /* a fresh process image: everything must come back from disk */
        printf("store: reopen\n");
        StoreReads(s);
    }
    {
        Server s(cfg, &host);
        printf("playerdata codec\n");
        Codec(s);
        printf("profiles\n");
        Profiles(s);
    }
    const uint64_t U = 7700000000000000077ULL;
    const std::string dir = PathJoin(cfg.profile, "7700000000000000077");
    Json before;
    {
        /* A fresh process image: THE mpdata_s FILE IS THE CAREER. Everything the profile held comes back
         * from the blob and the account.txt career lines; only the credit history and the clock members
         * are not kept. */
        printf("profiles: reopen (the career is read back from mpdata_s)\n");
        Server s(cfg, &host);
        Json prof;
        Bytes file, text;
        CHECK(PlayerLevel(s, U) == 5);
        CHECK(ProfileLoad(s, U, prof, false) && prof.get("level").asInt() == 5 && prof.get("history").size() == 0);
        CHECK(prof.get("total_xp").asInt() == RankToXp(s, 5, 0) && prof.get("seeded_from").str() == "the mpdata_s file");
        CHECK(prof.get("lifetime").get("kills").asInt() == 7 && prof.get("lifetime").get("deaths").asInt() == 2 &&
              prof.get("lifetime").get("score").asInt() == 700 && prof.get("lifetime").get("totalShots").asInt() == 120);
        CHECK(prof.get("lifetime").get("futureMember").asInt() == 5 && !prof.get("lifetime").has("kdRatio")); /* an extra / a derived member */
        CHECK(prof.get("kills").asInt() == 7 && prof.get("matches").asInt() == 1 && prof.get("name").str() == "Tester" &&
              prof.get("rankup_gp_level").asInt() == 3); /* the absorbed upload paid no rank-up GP */
        CHECK(prof.get("challenges").members().empty() && prof.get("weapons").members().empty());
        /* the sidecar holds the extras only: nothing the blob has */
        CHECK(ReadFile(PathJoin(dir, "account.txt"), text));
        CHECK(text.find("career.kills = 7\n") != std::string::npos && text.find("career.lifetime = {\"futureMember\": 5}\n") != std::string::npos &&
              text.find("career.name = \"Tester\"\n") != std::string::npos && text.find("total_xp") == std::string::npos &&
              text.find("\"score\"") == std::string::npos && text.find("history") == std::string::npos);
        /* reading the career changes nothing on disk, and one more credit lands in the file at once */
        CHECK(ReadFile(PathJoin(dir, "mpdata_s"), file));
        Json ch = Json::MakeObject(), prog = Json::MakeObject();
        prof["challenges"]["60"] = 120.5;       /* a challenge-table slot: the blob */
        prof["challenges"]["99999"] = 4;        /* outside challenges[1000]: an extra */
        prof["collectibles"] = Json::MakeArray();
        prof["collectibles"].push(Json(3));
        prof["total_xp"] = Json((long long)(prof.get("total_xp").asInt() + 250));
        CHECK(ProfileSave(s, prof));
        Bytes after;
        PdBlob b;
        double v = 0;
        CHECK(ReadFile(PathJoin(dir, "mpdata_s"), after) && after != file && b.Load(s, after));
        CHECK(b.GetPath("experience", v) && v == 250 && b.GetPath("challenges[60]", v) && v == 120.5 && b.GetPath("kills", v) && v == 7);
        CHECK(ProfileLoad(s, U, before, false));
        CHECK(!TmpLeft(cfg.profile));
    }
    {
        Server s(cfg, &host);
        Json prof;
        CHECK(ProfileLoad(s, U, prof, false));
        /* identical but for the two clock members and where it came from */
        static const char *const same[] = { "uid", "format", "total_xp", "level", "kills", "deaths", "assists", "matches", "name", "lifetime",
                                            "challenges", "weapons", "weapon_uses", "collectibles", "rankup_gp_level" };
        for (size_t i = 0; i < sizeof(same) / sizeof(same[0]); i++) {
            bool eq = prof.get(same[i]) == before.get(same[i]);
            if (!eq) printf("  %s: %s != %s\n", same[i], prof.get(same[i]).dump().c_str(), before.get(same[i]).dump().c_str());
            CHECK(eq);
        }
        CHECK(prof.get("challenges").get("60").asReal() == 120.5 && prof.get("challenges").get("99999").asInt() == 4);
        std::vector<uint64_t> uids = ProfileListUids(s);
        CHECK(uids.size() == 2 && uids[0] == U); /* U and the mirror test's second account */
    }
    {
        /* account.txt gone: the career is still the blob (the extras are lost, the folder is found by its name) */
        printf("profiles: without account.txt, without mpdata_s\n");
        CHECK(RemoveFile(PathJoin(dir, "account.txt")));
        Server s(cfg, &host);
        Json prof;
        CHECK(ProfileLoad(s, U, prof, false) && prof.get("total_xp").asInt() == before.get("total_xp").asInt());
        CHECK(prof.get("lifetime").get("kills").asInt() == 7 && prof.get("kills").asInt() == 0 && !prof.has("collectibles"));
        CHECK(FileExists(PathJoin(dir, "account.txt"))); /* written again: the folder keeps its uid */
        GetStore(s).WalletSet(U, 1, 321);
        prof["weapons"]["m4_mp"] = 5210;
        CHECK(ProfileSave(s, prof));
    }
    {
        /* mpdata_s gone: a new career at level 1. The career lines of account.txt are void with it;
         * wallet and inventory are the account's, not the career's, and stay. */
        CHECK(RemoveFile(PathJoin(dir, "mpdata_s")));
        Server s(cfg, &host);
        Json prof;
        Bytes text;
        CHECK(PlayerLevel(s, U) == 0 && !ProfileLoad(s, U, prof, false));
        CHECK(ProfileLoad(s, U, prof) && prof.get("level").asInt() == 1 && prof.get("total_xp").asInt() == 0);
        CHECK(prof.get("weapons").members().empty() && prof.get("lifetime").members().empty() && prof.get("seeded_from").str() == "level 1 (new account)");
        CHECK(GetStore(s).WalletGet(U, 1) == 321);
        CHECK(FileExists(PathJoin(dir, "mpdata_s")) && ReadFile(PathJoin(dir, "account.txt"), text) && text.find("\ncareer.") == std::string::npos);
    }
    {
        /* a blob that is not a profile is moved aside, never overwritten in place */
        CHECK(WriteFileAtomic(PathJoin(dir, "mpdata_s"), "this is not a profile"));
        Server s(cfg, &host);
        Json prof;
        Bytes text;
        CHECK(!ProfileLoad(s, U, prof, false));
        CHECK(ProfileLoad(s, U, prof) && prof.get("level").asInt() == 1);
        CHECK(ReadFile(PathJoin(dir, "mpdata_s.unreadable"), text) && text == "this is not a profile");
        int64_t xp = -1;
        CHECK(ReadFile(PathJoin(dir, "mpdata_s"), text) && XpFromBlob(s, text, xp) && xp == 0);
        CHECK(!TmpLeft(cfg.profile));
    }
    {
        /* A career that has not played reads back as one that has not played: what the level-1 seed
         * itself carries in a lifetime member (objectives = 100000) is not the player's. A total that
         * happens to equal the seed's value is still the player's. */
        Server s(cfg, &host);
        Json prof;
        Bytes text;
        CHECK(ProfileLoad(s, U, prof, false) && prof.get("lifetime").members().empty() && prof.get("challenges").members().empty());
        prof["lifetime"]["objectives"] = 100000;
        prof["lifetime"]["kills"] = 3;
        CHECK(ProfileSave(s, prof) && ReadFile(PathJoin(dir, "account.txt"), text) &&
              text.find("career.lifetime = {\"objectives\": 100000}\n") != std::string::npos);
    }
    {
        Server s(cfg, &host);
        Json prof;
        CHECK(ProfileLoad(s, U, prof, false) && prof.get("lifetime").get("objectives").asInt() == 100000 &&
              prof.get("lifetime").get("kills").asInt() == 3 && prof.get("lifetime").members().size() == 2);
    }
    {
        /* The JSON store of the first offline builds is imported once: the career into the folder of
         * the name it was played under, wallet / inventory / mpdata with it; a marker ends it. */
        printf("legacy store import\n");
        Config lc = cfg;
        lc.profile = cfg.profile + "_imported";
        lc.legacyStore = cfg.profile + "_old";
        lc.localName = "Vet";
        const uint64_t L = 555;
        if (!ListDir(lc.profile).empty() || !ListDir(lc.legacyStore).empty()) { fprintf(stderr, "--selftest-store: %s / %s are not empty\n", lc.profile.c_str(), lc.legacyStore.c_str()); return 2; }
        CHECK(WriteFileAtomic(PathJoin(PathJoin(lc.legacyStore, "players"), "555.json"),
                              "{\"format\": 1, \"uid\": \"555\", \"name\": \"Vet\", \"total_xp\": 4100, \"kills\": 9, \"matches\": 4,"
                              " \"lifetime\": {\"kills\": 9, \"score\": 900}, \"weapons\": {\"m4_mp\": 5210}, \"rankup_gp_level\": 3,"
                              " \"history\": [{\"xp\": 4100}], \"created\": 1.0}"));
        CHECK(WriteFileAtomic(PathJoin(PathJoin(lc.legacyStore, "accounts"), "555.json"),
                              "{\"version\": 1, \"wallet\": {\"1\": 77}, \"inventory\": [{\"item_id\": 1201, \"quantity\": 3, \"expires_at\": 0,"
                              " \"acquired_at\": 5.0, \"source\": \"purchase\", \"sku_id\": 217}]}"));
        CHECK(WriteFileAtomic(PathJoin(PathJoin(PathJoin(lc.legacyStore, "storage"), "555"), "mpdata"), "old mpdata"));
        CHECK(WriteFileAtomic(PathJoin(PathJoin(lc.legacyStore, "players"), "notes.txt"), "not a profile"));
        for (int pass = 0; pass < 2; pass++) { /* the second start finds the marker and the same career */
            Server s(lc, &host);
            Json prof;
            Bytes got;
            InvRow r;
            CHECK(GetStore(s).FolderOf(L) == "Vet" && ProfileLoad(s, L, prof, false));
            CHECK(prof.get("level").asInt() == 3 && prof.get("total_xp").asInt() == 4100 && prof.get("kills").asInt() == 9 &&
                  prof.get("matches").asInt() == 4 && prof.get("name").str() == "Vet" && prof.get("rankup_gp_level").asInt() == 3);
            CHECK(prof.get("lifetime").get("kills").asInt() == 9 && prof.get("lifetime").get("score").asInt() == 900 &&
                  prof.get("weapons").get("m4_mp").asInt() == 5210);
            CHECK(GetStore(s).WalletGet(L, 1) == (pass ? 80 : 77) && GetStore(s).InventoryGet(L, 1201, r) && r.quantity == 3 && r.skuId == 217);
            CHECK(GetStore(s).GetStorageFile(L, "mpdata", got) && got == "old mpdata");
            CHECK(FileExists(PathJoin(lc.legacyStore, "IMPORTED.txt")) && FileExists(PathJoin(PathJoin(lc.profile, "Vet"), "mpdata_s")));
            CHECK(ListDir(lc.profile).size() == 1 && ListDir(PathJoin(lc.profile, "Vet")).size() == 3);
            if (pass == 0) GetStore(s).WalletSet(L, 1, 80); /* never imported over what the player has since done */
        }
        CHECK(!TmpLeft(lc.profile));
    }
    printf("%d checks, %d failed\n%s\n", g_checks, g_fail, g_fail ? "SELFTEST-STORE FAIL" : "SELFTEST-STORE PASS");
    return g_fail ? 1 : 0;
}

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
#include "../../offline/dw.h"
#include "../../offline/dw_playerdata.h"
#include "../../offline/dw_profiles.h"
#include "../../offline/dw_state.h"
#include "../../offline/dw_store.h"

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
    const uint64_t A = 9000000000000000001ULL, B = 4242;

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

    /* purchases, history, account */
    Json p = Json::MakeObject();
    p["sku_id"] = 31; p["item_id"] = 400; p["amount"] = 120;
    st.RecordPurchase(A, p);
    p["item_id"] = 401;
    st.RecordPurchase(A, p);
    CHECK(st.Purchases(A).size() == 2 && st.Purchases(A).at(0).get("item_id").asInt() == 401);
    st.AddHistory(A, 150, "match", 150, 1);
    CHECK(st.GetAccount(B).isNull());
    st.TouchLogin(A, "Tester", "");
    st.TouchLogin(A, "", "");
    CHECK(st.GetAccount(A).get("login_count").asInt() == 2 && st.GetAccount(A).get("gamertag").str() == "Tester");

    /* storage files */
    Bytes blob("\x78\x01\x00\xff binary \x00 data", 19), got;
    CHECK(!st.GetStorageFile(A, "mpdata", got));
    CHECK(st.PutStorageFile(A, "mpdata", blob, "upload"));
    CHECK(st.PutStorageFile(0, "blacklist", "shared", "upload"));
    CHECK(st.PutStorageFile(0, "uploads/odd.bin", "x", "upload"));
    CHECK(st.GetStorageFile(A, "mpdata", got) && got == blob);
    CHECK(!st.GetStorageFile(A, "blacklist", got) && st.GetStorageFile(A, "blacklist", got, true) && got == "shared");
    CHECK(!st.GetStorageFile(B, "mpdata", got, true));
    CHECK(!st.PutStorageFile(A, "../escape", "x", "upload") && !st.PutStorageFile(A, "a:b", "x", "upload"));
    CHECK(st.PutStorageFile(A, "Recentmetlist", "gone soon", "upload") && st.DeleteStorageFile(A, "Recentmetlist"));
    CHECK(!st.GetStorageFile(A, "Recentmetlist", got) && !st.DeleteStorageFile(A, "Recentmetlist"));

    /* profiles, kv, reports */
    Json prof = Json::MakeObject(), back;
    prof["format"] = 1; prof["uid"] = "4242"; prof["total_xp"] = 12345; prof["name"] = "caf\xc3\xa9 \"q\"";
    prof["big"] = Json(18446744073709551615ULL);
    CHECK(!st.ProfileRead(B, back));
    CHECK(st.ProfileWrite(B, prof) && st.ProfileRead(B, back) && back == prof);
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
    CHECK(st.InventoryList(A, false).size() == 2 && st.InventoryList(A).size() == 3);
    CHECK(st.InventoryGet(A, 100, r) && r.quantity == 7 && r.source == "grant" && r.acquiredAt == 55);
    CHECK(st.InventoryGet(A, 400, r) && r.expiresAt == 77 && r.source == "purchase");
    CHECK(st.Purchases(A).size() == 2 && st.Purchases(A).at(1).get("id").asInt() == 1);
    CHECK(st.GetAccount(A).get("login_count").asInt() == 2 && st.GetAccount(A).get("uid").asUInt() == A);
    CHECK(st.GetStorageFile(A, "mpdata", got) && got == Bytes("\x78\x01\x00\xff binary \x00 data", 19));
    CHECK(st.GetStorageFile(0, "blacklist", got) && got == "shared");
    CHECK(st.GetStorageFile(0, "uploads/odd.bin", got) && got == "x");
    CHECK(!st.GetStorageFile(A, "Recentmetlist", got));
    CHECK(st.ProfileRead(B, back) && back.get("total_xp").asInt() == 12345 && back.get("name").str() == "caf\xc3\xa9 \"q\"");
    CHECK(back.get("big").asUInt() == 18446744073709551615ULL);
    std::vector<uint64_t> uids = st.ProfileUids();
    CHECK(uids.size() == 1 && uids[0] == B);
    CHECK(st.KvGet("n").asInt() == 7 && st.KvGet("rank_writeback.state").get("uid").str() == "4242");
    CHECK(st.HasReport("abc") && !st.HasReport("abd"));
    CHECK(!TmpLeft(s.cfg.profile));
    /* the shared mpdata / Recentmetlist are transient across a restart (1-START-LOBBY.cmd), others stay */
    CHECK(st.PutStorageFile(0, "mpdata", "transient", "upload"));
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

    std::string note = EnsureBlob(s, U);
    Bytes blob, seed;
    PdBlob b;
    double v = 0;
    CHECK(note.find("rendered mpdata_s.7700000000000000077 level 1") != std::string::npos);
    CHECK(GetStore(s).GetStorageFile(U, "mpdata_s", blob) && b.Load(s, blob) && b.def->key == "mpdata_s");
    if (!b.ok()) return;
    CHECK(b.GetPath("rank", v) && v == 1 && b.GetPath("experience", v) && v == 0 && b.GetPath("rankVersion", v) && v == 2);
    /* the profile is authoritative over every slot the challenge table defines: the seed's 999999.0 reads 0 */
    CHECK(s.Resource("mpdata_s.level1.proposed", seed) && blob != seed);
    CHECK(b.GetPath("challenges[60]", v) && v == 0 && b.GetPath("challegeUpdateTime", v) && v == (double)s.Now());
    CHECK(EnsureBlob(s, U).empty()); /* already current: the idle path */
    Bytes shared;
    CHECK(GetStore(s).GetStorageFile(0, "mpdata_s", shared) && shared == blob); /* the shared mirror */

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

    note = EnsureBlob(s, U);
    CHECK(note.find("rendered mpdata_s.7700000000000000077 level 3 (4110 XP, was 0 XP)") != std::string::npos);
    CHECK(GetStore(s).GetStorageFile(U, "mpdata_s", blob) && b.Load(s, blob));
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
        Bytes got;
        CHECK(!GetStore(s).GetStorageFile(0, "mpdata", got) && GetStore(s).GetStorageFile(0, "blacklist", got));
        printf("playerdata codec\n");
        Codec(s);
        printf("profiles\n");
        Profiles(s);
    }
    {
        Server s(cfg, &host);
        Json prof;
        CHECK(ProfileLoad(s, 7700000000000000077ULL, prof, false) && prof.get("level").asInt() == 5 && prof.get("history").size() == 3);
        CHECK(!TmpLeft(cfg.profile));
    }
    printf("%d checks, %d failed\n%s\n", g_checks, g_fail, g_fail ? "SELFTEST-STORE FAIL" : "SELFTEST-STORE PASS");
    return g_fail ? 1 : 0;
}

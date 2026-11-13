/*
 * dwtest --selftest-market: the marketplace (service 80) against a temporary store and the pristine
 * resources, through the same entry point the dispatcher uses (dw::MarketplaceTask).
 *
 *   dwtest --resources <pristine resources> --profile <empty temp folder> --selftest-market
 *
 * Checks: the catalogue loads with the expected record counts; a purchase the wallet cannot cover is
 * refused (8007) and leaves wallet and inventory untouched; a funded purchase debits the wallet,
 * grants the item, shows up in getInventory and getPlayerData and survives a Store reopen; the
 * unlock-flag set and the weapon XP field of getPlayerData match the Python lobby for three profiles.
 * Linked into dwtest.exe by build_dwtest.sh; entry point DwMarketSelfTest (dwtest_main.cpp).
 */
#include <stdio.h>
#include "../../src/offline/dw.h"
#include "../../src/offline/dw_json.h"
#include "../../src/offline/dw_profiles.h"
#include "../../src/offline/dw_store.h"

namespace market_selftest {

enum {
    kUid = 4242,             /* a synthetic account: nothing in the resources knows it */
    kProducts = 745, kSkus = 1494, /* resources/marketplace_catalogue.json as shipped */
    kSku = 217,              /* product 109: item 1201 equ/semtex_mp in packs of 100, permanent, 1500 of currency 1 */
    kItem = 1201, kPack = 100, kPrice = 1500, kCurrency = 1,
    kErrFunds = 8007,
};

static int g_fail;

static void Check(bool cond, const char *what)
{
    printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) g_fail++;
}

static dw::Vals PurchaseArgs(unsigned sku)
{
    dw::Vals a; /* count, sku, count, quantity, Bool, count, UInt64, UInt32 (builder 0x00A37930) */
    a.push_back(dw::Val(dw::T_UINT32, 1)); a.push_back(dw::Val(dw::T_UINT32, sku));
    a.push_back(dw::Val(dw::T_UINT32, 1)); a.push_back(dw::Val(dw::T_UINT32, 1));
    a.push_back(dw::Val(dw::T_BOOL, 0));
    a.push_back(dw::Val(dw::T_UINT32, 1)); a.push_back(dw::Val(dw::T_UINT64, 0));
    a.push_back(dw::Val(dw::T_UINT32, 0));
    return a;
}

static dw::Reply Task(dw::Server &s, dw::Conn &c, int task, const dw::Vals &args = dw::Vals())
{
    return dw::MarketplaceTask(s, c, task, args);
}

/* The quantity an inventory reply carries for `item`, -1 when it has no such row.
 * Record: [10 u64 instance][8 u32 itemId][8 u32 quantity]... */
static long long QtyIn(const std::vector<dw::Bytes> &recs, unsigned item)
{
    for (size_t i = 0; i < recs.size(); i++) {
        dw::Vals v = dw::ParseValues(recs[i], 0xEE);
        if (v.size() >= 3 && v[1].tag == dw::T_UINT32 && v[1].u == item) return (long long)v[2].u;
    }
    return -1;
}

static long long Balance(dw::Server &s, dw::Conn &c, unsigned currency)
{
    dw::Vals a;
    a.push_back(dw::Val(dw::T_UINT32, 2));
    dw::Reply r = Task(s, c, 32, a);
    for (size_t i = 0; i < r.results.size(); i++) {
        dw::Vals v = dw::ParseValues(r.results[i], 0xEE);
        if (v.size() >= 2 && v[0].u == currency) return (long long)v[1].u;
    }
    return -1;
}

/* getPlayerData for a profile with this lifetime XP, challenge progress and weapon counters:
 * the unlock-flag set (count + CRC32 of the ids as LE u32) and the XP field of the m4_mp inventory
 * record (item 1001, owned through the purchased_items.txt seed) against the numbers the Python lobby
 * gives for the same profile: dw_marketplace.granted_flags(level_for_xp(xp), profile) and
 * progression.weapon_xp_by_item(profile). */
static void CheckProfile(dw::Server &s, unsigned uid, long long xp, const dw::Json &challenges, const dw::Json &weapons,
                         unsigned wantFlags, unsigned wantCrc, unsigned wantM4Xp, const char *what)
{
    dw::Json prof = dw::Json::MakeObject();
    prof["format"] = 1;
    prof["uid"] = dw::Fmt("%u", uid);
    prof["total_xp"] = dw::Json(xp);
    prof["challenges"] = challenges;
    prof["weapons"] = weapons;
    dw::ProfileSave(s, prof);
    dw::Conn &c = *s.Open();
    c.uid = uid;
    dw::Reply pd = Task(s, c, 30);
    s.Close(&c);
    /* [10 id][8 nCur] nCur x {10, 3, 8} [8 nItems] nItems x 8 values [10 id][8 nFlags] nFlags x 8 */
    dw::Vals v = pd.results.size() == 1 ? dw::ParseValues(pd.results[0], 0xEE) : dw::Vals();
    size_t items = v.size() > 8 ? (size_t)v[8].u : 0, at = 9 + 8 * items;
    unsigned m4 = 0;
    for (size_t i = 0; i < items && 9 + 8 * i + 3 < v.size(); i++)
        if (v[9 + 8 * i + 1].u == 1001) m4 = (unsigned)v[9 + 8 * i + 3].u;
    dw::Bytes ids;
    size_t n = at + 1 < v.size() ? (size_t)v[at + 1].u : 0;
    for (size_t i = 0; i < n && at + 2 + i < v.size(); i++)
        for (int k = 0; k < 4; k++) ids += (char)(v[at + 2 + i].u >> (8 * k));
    bool ok = n == wantFlags && at + 2 + n == v.size() && dw::Crc32(ids.data(), ids.size()) == wantCrc && m4 == wantM4Xp;
    if (!ok) printf("       got %u flags, crc 0x%08X, m4 xp %u\n", (unsigned)n, (unsigned)dw::Crc32(ids.data(), ids.size()), m4);
    Check(ok, what);
}

} // namespace market_selftest

using namespace market_selftest;

int DwMarketSelfTest(const dw::Config &cfg)
{
    dw::Host host; /* quiet: the base Host logs nowhere */
    if (cfg.resources.empty() || cfg.profile.empty()) {
        fprintf(stderr, "--selftest-market needs --resources <pristine resources> and --profile <temp store>\n");
        return 2;
    }
    g_fail = 0;
    std::vector<dw::Bytes> before, after;
    {
        dw::Server s(cfg, &host);
        dw::Conn &c = *s.Open();
        c.uid = kUid;

        printf("[1] catalogue\n");
        dw::Vals a;
        a.push_back(dw::Val(dw::T_UINT16, 2000));
        dw::Reply products = Task(s, c, 3, a);
        a[0].u = 4000;
        a.push_back(dw::Val(dw::T_BOOL, 0));
        dw::Reply skus = Task(s, c, 6, a);
        Check(products.err == 0 && products.results.size() == kProducts, "getProducts: 745 records");
        Check(skus.err == 0 && skus.results.size() == kSkus, "getSkus: 1494 records");
        Check(Task(s, c, 6, a).results == skus.results, "getSkus again: the same bytes (reply cache)");
        a[0].u = 10;
        Check(Task(s, c, 6, a).results.size() == 10, "getSkus honours the client's maxResults");

        printf("[2] a purchase the wallet cannot cover\n");
        dw::Store &st = dw::GetStore(s);
        st.WalletSet(kUid, kCurrency, kPrice - 1);
        before = Task(s, c, 10).results;
        Check(!before.empty() && QtyIn(before, kItem) < 0, "the starting inventory is not empty and lacks the item");
        size_t rowsBefore = st.InventoryList(kUid, false).size();
        dw::Reply refused = Task(s, c, 26, PurchaseArgs(kSku));
        Check(refused.err == kErrFunds && refused.results.empty(), "refused with 8007 BD_MARKETPLACE_INSUFFICIENT_FUNDS, no records");
        Check(Balance(s, c, kCurrency) == kPrice - 1, "the wallet is untouched");
        Check(Task(s, c, 10).results == before && st.InventoryList(kUid, false).size() == rowsBefore, "the inventory is untouched");

        printf("[3] a funded purchase\n");
        st.WalletSet(kUid, kCurrency, kPrice + 500);
        dw::Reply bought = Task(s, c, 26, PurchaseArgs(kSku));
        Check(bought.err == 0 && bought.results.empty(), "err 0, empty success");
        Check(Balance(s, c, kCurrency) == 500, "the wallet is debited by exactly the price");
        after = Task(s, c, 10).results;
        Check(after.size() == before.size() + 1 && QtyIn(after, kItem) == kPack, "getInventory serves the item with the pack size as its count");
        dw::Reply pd = Task(s, c, 30);
        Check(pd.err == 0 && pd.results.size() == 1, "getPlayerData: one record");
        dw::InvRow row;
        Check(st.InventoryGet(kUid, kItem, row) && row.quantity == kPack && row.expiresAt == 0 && row.skuId == kSku,
              "the store holds the grant: permanent, quantity 100, sku 217");
        s.Close(&c);
    }
    {
        printf("[4] after a Store reopen\n");
        dw::Server s(cfg, &host);
        dw::Conn &c = *s.Open();
        c.uid = kUid;
        Check(Balance(s, c, kCurrency) == 500, "the balance survived");
        Check(Task(s, c, 10).results == after, "getInventory answers the same bytes, the bought item included");
        dw::Reply again = Task(s, c, 26, PurchaseArgs(kSku));
        Check(again.err == kErrFunds && Balance(s, c, kCurrency) == 500, "and 500 does not buy a second pack");
        s.Close(&c);

        printf("[5] getPlayerData: unlock flags and weapon XP against the Python lobby's numbers\n");
        dw::Json none = dw::Json::MakeObject(), ch = dw::Json::MakeObject(), wp = dw::Json::MakeObject();
        CheckProfile(s, 4243, 0, none, none, 22, 0x494C6887u, 0, "0 XP -> level 1: 22 flags");
        ch["60"] = 120; ch["1"] = 55.5; ch["bogus"] = 3;
        wp["m4_mp"] = 30000; wp["AK47_MP"] = 2900; wp["nosuch_mp"] = 5;
        CheckProfile(s, 4244, 500000, ch, wp, 60, 0x41F3E7F2u, 30000, "500000 XP -> level 32, two challenges, two weapons: 60 flags, m4 XP 30000");
        ch = dw::Json::MakeObject(); wp = dw::Json::MakeObject();
        ch["60"] = 10000; wp["m4_mp"] = 179600;
        CheckProfile(s, 4245, 9000000, ch, wp, 99, 0x98153A59u, 179600, "9000000 XP -> level 70, a mastered weapon: 99 flags, m4 XP 179600");
    }
    printf("%s\n", g_fail ? "SELFTEST-MARKET FAIL" : "SELFTEST-MARKET PASS");
    return g_fail ? 1 : 0;
}

/*
 * dw_marketplace.cpp - Demonware MARKETPLACE, service 80 (demonware/dw_marketplace.py).
 *
 * Task map, MEASURED from C1.exe (every bdMarketplace builder calls initTaskBuffer 0x00A30A60 with
 * `push <task>; push 0x50`):
 *    1 deposit            2 getBalance (legacy arm, never sent)      32 getBalance (the arm this client takes)
 *    3 getProducts        6 getSkus                                  10 getInventory   13 getPlayersInventory
 *   12 consumeInventoryItem   26 purchaseSkus                             20 coupon list (answered empty)
 *   30 getPlayerData: ONE record carrying the wallet, the owned items and the unlock-flag set
 *
 * The catalogue is the client's own shipped tables (resources/marketplace_catalogue.json); the
 * per-player state (wallet, grants) lives in the Store, keyed by the connection's latched uid
 * (uid 0 = the shared bucket, as the Python lobby's '' / "shared").
 */
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "dw_marketplace.h"
#include "dw_playerdata.h" /* PdEquippedItemIds */
#include "dw_profiles.h"   /* ProfileLoad */

namespace dw {

namespace {

enum {
    CLIENT_PRODUCT_SLOTS = 2000, /* array 0x6D9DB80: 2000 x 0x258 */
    CLIENT_SKU_SLOTS = 4000,     /* array 0x6EFC9E0: 4000 x 0x16C */
    CLIENT_INV_SLOTS = 2000,     /* array 0x6ED16B8: 2000 x 0x30 */
    PD_MAX_CURRENCIES = 10,      /* bdMarketplacePlayerData::deserialize clamp 0x00A3ABE1 */
    PD_MAX_ENTITLEMENTS = 1000,  /* clamp 0x00A3AC69 */
    PD_MAX_FLAGS = 1000,         /* flag-set ctor 0x00A3B59A: 1000 dwords */
    UF_RANK_MAX = 70,
    MAX_BALANCE_RECORDS = 255,   /* ours: the wallet count is a client-supplied u32 (always 2) */
};
const char *const kHideFlag = "uf_item_hide"; /* id 663: the retail shelf-hide sentinel, never a requirement */
const int64_t kWalletMax = 0xFFFFFFFFLL;

/* ---- catalogue/perk_upgrades.PRO_BY_BASE: mp/coPerkStatsTable.csv column 1 -> column 8 ---- */
const char *const kProByBase[][2] = {
    { "specialty_bulletaccuracy", "specialty_holdbreath" },   { "specialty_marathon", "specialty_fastmantle" },
    { "specialty_fastreload", "specialty_quickswap" },        { "specialty_scavenger", "specialty_extraammo" },
    { "specialty_flakjacket", "specialty_flakjacket_pro" },   { "specialty_paint", "specialty_paint_pro" },
    { "specialty_lightweight", "specialty_fastsprintrecovery" }, { "specialty_hardline", "specialty_rollover" },
    { "specialty_coldblooded", "specialty_spygame" },         { "specialty_quickdraw", "specialty_fastoffhand" },
    { "specialty_stalker", "specialty_delaymine" },           { "specialty_heartbreaker", "specialty_quieter" },
    { "specialty_detectexplosive", "specialty_selectivehearing" }, { "specialty_pistoldeath", "specialty_laststandoffhand" },
    { "specialty_hacker", "specialty_hackerpro" },            { "specialty_marksman", "specialty_marksmanpro" },
    { "specialty_engineer", "specialty_engineerpro" },
};

/* ---- catalogue/zm_characters: the Zombies roster (mp/cyborgclasstable.csv, four characters) ---- */
/* core_grant_item_ids(): the four identities and all their parts, sorted. */
const int kReleasedUnsoldWeapons[] = { 1057 }; /* m14ebr_mp: released by mp_weapons, no sku (see LoadItemIds) */
const int kZmCoreGrant[] = { 107, 171, 177, 178, 179, 180, 181, 182, 183, 184, 186, 278, 279, 280,
                             281, 282, 283, 284, 409, 410, 411, 412, 413, 423 };
/* non_core_item_ids(): the zm heads the roster does not list (zmlubu, zmtightgirl, zmchao, zmhiphop). */
const int kZmNonCore[] = { 223, 230, 289, 316 };

std::string Strip(const std::string &t)
{
    size_t a = 0, b = t.size();
    while (a < b && strchr(" \t\r\n\f\v", t[a])) a++;
    while (b > a && strchr(" \t\r\n\f\v", t[b - 1])) b--;
    return t.substr(a, b - a);
}

std::string Lower(std::string t)
{
    for (size_t i = 0; i < t.size(); i++)
        if (t[i] >= 'A' && t[i] <= 'Z') t[i] = (char)(t[i] + 32);
    return t;
}

int64_t WalletClamp(int64_t v) { return v < 0 ? 0 : v > kWalletMax ? kWalletMax : v; }

MpPairs ReadPairs(const Json &arr)
{
    MpPairs out;
    for (size_t i = 0; i < arr.size(); i++) {
        const Json &p = arr.at(i);
        if (p.isArray() && p.size() >= 2) out.push_back(std::make_pair(p.at(0).asInt(), p.at(1).asInt()));
    }
    return out;
}

/* Python float(v) / int(v) on a JSON value, as far as a profile can hold one. */
bool JsonFloat(const Json &v, double &out)
{
    if (v.isNumber()) { out = v.asReal(); return true; }
    if (v.isBool()) { out = v.asBool() ? 1.0 : 0.0; return true; }
    if (v.isString()) {
        std::string t = Strip(v.asString());
        if (t.empty()) return false;
        char *end = NULL;
        out = strtod(t.c_str(), &end);
        return end && !*end;
    }
    return false;
}

bool JsonInt(const Json &v, int64_t &out)
{
    if (v.isInt()) { out = v.asInt(); return true; }
    if (v.isNumber()) { out = (int64_t)v.asReal(); return true; }
    if (v.isBool()) { out = v.asBool() ? 1 : 0; return true; }
    if (v.isString()) return MpPyInt(v.asString(), out);
    return false;
}

} // namespace

/* ---- text helpers ---- */

std::vector<std::string> MpSplitLines(const Bytes &text)
{
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        out.push_back(text.substr(pos, nl - pos));
        pos = nl + 1;
    }
    return out;
}

std::vector<std::string> MpSplitWs(const std::string &line)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && strchr(" \t\r\n\f\v", line[i])) i++;
        size_t b = i;
        while (i < line.size() && !strchr(" \t\r\n\f\v", line[i])) i++;
        if (i > b) out.push_back(line.substr(b, i - b));
    }
    return out;
}

bool MpIsDigits(const std::string &t)
{
    if (t.empty()) return false;
    for (size_t i = 0; i < t.size(); i++)
        if (t[i] < '0' || t[i] > '9') return false;
    return true;
}

bool MpPyInt(const std::string &text, int64_t &out)
{
    std::string t = Strip(text);
    size_t i = 0;
    bool neg = false;
    if (i < t.size() && (t[i] == '+' || t[i] == '-')) neg = t[i++] == '-';
    if (i >= t.size() || t.size() - i > 18) return false;
    int64_t v = 0;
    for (; i < t.size(); i++) {
        if (t[i] < '0' || t[i] > '9') return false;
        v = v * 10 + (t[i] - '0');
    }
    out = neg ? -v : v;
    return true;
}

/* ---- state ---- */

MarketState &Market(Server &s)
{
    if (!s.st->market) s.st->market = new MarketState();
    return *s.st->market;
}

void MarketInit(Server &s) { Market(s); }

void MarketFree(Server &s)
{
    delete s.st->market;
    s.st->market = NULL;
}

/* load_catalogue() + perk_upgrades._tables(): read once; any failure = no store, never a crash. */
void MpLoadCatalogue(Server &s, MarketState &m)
{
    if (m.catLoaded) return;
    m.catLoaded = true;
    Bytes text;
    Json doc;
    std::string err;
    if (!s.Resource("marketplace_catalogue.json", text)) { m.catError = "marketplace_catalogue.json not found"; return; }
    if (!Json::Parse(text, doc, &err) || !doc.isObject()) { m.catError = err.empty() ? "not a JSON object" : err; return; }

    const std::vector<Json> &prods = doc.get("products").items();
    m.products.reserve(prods.size());
    for (size_t i = 0; i < prods.size(); i++) {
        if (!prods[i].has("id")) continue;
        MpProduct p;
        p.id = prods[i].get("id").asInt();
        p.pairs = ReadPairs(prods[i].get("pairs"));
        const std::vector<Json> &fl = prods[i].get("flags").items();
        for (size_t k = 0; k < fl.size(); k++) p.flags.push_back(fl[k].asInt());
        m.productById[p.id] = m.products.size(); /* a repeated id: the last one wins, as a dict */
        if (!p.pairs.empty() && !m.productByItem.count(p.pairs[0].first)) m.productByItem[p.pairs[0].first] = m.products.size();
        m.products.push_back(p);
    }
    const std::vector<Json> &skus = doc.get("skus").items();
    m.skus.reserve(skus.size());
    for (size_t i = 0; i < skus.size(); i++) {
        const Json &j = skus[i];
        if (!j.has("id")) continue;
        MpSku k;
        k.id = j.get("id").asInt();
        k.hasProduct = j.has("product");
        k.product = j.get("product").asInt();
        k.prices = ReadPairs(j.get("prices"));
        k.f4e = j.has("f4e") ? j.get("f4e").asInt() : 1;
        k.f50 = j.get("f50").asInt();
        k.f54 = j.get("f54").asInt();
        k.f58 = j.get("f58").asInt();
        k.f4f = j.get("f4f").asInt();
        k.days = j.has("days") ? j.get("days").asInt() : k.f58; /* sku_days(): col6 -> +0x58, 0 == permanent */
        m.skuById[k.id] = m.skus.size();
        m.skus.push_back(k);
    }
    const Json::Members &names = doc.get("item_names").members();
    std::map<std::string, int64_t> perkByName;
    for (size_t i = 0; i < names.size(); i++) {
        const std::string &v = names[i].second.asString();
        m.itemNames[names[i].first] = v;
        /* _perk_ids_by_name: "per/<name>" rows */
        size_t slash = v.find('/');
        if (slash == std::string::npos) continue;
        std::string name = Lower(Strip(v.substr(slash + 1)));
        int64_t id;
        if (v.size() == slash + 1 || Lower(Strip(v.substr(0, slash))) != "per" || !MpPyInt(names[i].first, id)) continue;
        perkByName[name] = id;
        m.perkIds.insert(id);
    }
    for (size_t i = 0; i < sizeof(kProByBase) / sizeof(kProByBase[0]); i++) {
        std::map<std::string, int64_t>::iterator b = perkByName.find(kProByBase[i][0]), p = perkByName.find(kProByBase[i][1]);
        if (b != perkByName.end() && p != perkByName.end()) m.proBase[p->second] = b->second;
    }
    const std::vector<Json> &st = doc.get("stackable_items").items();
    for (size_t i = 0; i < st.size(); i++) m.stackable.insert(st[i].asInt());
    const std::vector<Json> &hid = doc.get("hidden_items").items();
    for (size_t i = 0; i < hid.size(); i++) m.hidden.insert(hid[i].asInt());
}

std::string MpItemName(const MarketState &m, int64_t itemId)
{
    std::string key = Fmt("%lld", (long long)itemId);
    std::map<std::string, std::string>::const_iterator it = m.itemNames.find(key);
    return it == m.itemNames.end() ? key : it->second;
}

/* perk_upgrades.unlock_flags: a product keeps its own flags; a Pro that C1 ships with none (3036,
 * 3037) falls back to its base perk's product flags. */
std::vector<int64_t> MpUnlockFlags(const MarketState &m, const MpProduct *product)
{
    std::vector<int64_t> none;
    if (!product) return none;
    if (!product->flags.empty()) return product->flags;
    if (product->pairs.empty()) return none;
    std::map<int64_t, int64_t>::const_iterator base = m.proBase.find(product->pairs[0].first);
    if (base == m.proBase.end()) return none;
    std::map<int64_t, size_t>::const_iterator bp = m.productByItem.find(base->second);
    return bp == m.productByItem.end() ? none : m.products[bp->second].flags;
}

/* ---- unlock flags ---- */

/* load_unlock_flags(): the client's own mp/unlockFlags.csv, 668 rows (resources/unlock_flags.json). */
static void LoadUnlockFlags(Server &s, MarketState &m)
{
    if (m.flagsLoaded) return;
    m.flagsLoaded = true;
    Bytes text;
    Json doc;
    if (!s.Resource("unlock_flags.json", text) || !Json::Parse(text, doc)) return;
    const std::vector<Json> &rows = doc.get("rows").items();
    for (size_t i = 0; i < rows.size(); i++) {
        if (!rows[i].has("name") || !rows[i].has("id")) { m.flagRows.clear(); return; } /* KeyError -> [] */
        m.flagRows.push_back(std::make_pair(rows[i].get("name").str(), rows[i].get("id").asInt()));
    }
    for (size_t i = 0; i < m.flagRows.size(); i++)
        if (m.flagRows[i].first == kHideFlag) { m.hideFlag = m.flagRows[i].second; break; }
}

/* The slice of resources/progression_tables.json the flag set and the weapon XP field read:
 * career/challenges.Challenges.__init__ and career/weapon_leveling.WeaponLeveling.__init__. */
static void LoadProgression(Server &s, MarketState &m)
{
    if (m.progLoaded) return;
    m.progLoaded = true;
    Bytes text;
    Json doc;
    if (!s.Resource("progression_tables.json", text) || !Json::Parse(text, doc) || !doc.isObject()) return;
    if (!doc.get("challenges").isArray() || !doc.get("unlock_flags").isArray() || !doc.get("weapons").isObject()) return;
    const std::vector<Json> &chs = doc.get("challenges").items();
    for (size_t i = 0; i < chs.size(); i++) {
        MpChallenge c;
        c.id = chs[i].get("id").asInt();
        const std::vector<Json> &tiers = chs[i].get("tiers").items();
        for (size_t k = 0; k < tiers.size(); k++) c.targets.push_back(tiers[k].get("target").asReal());
        std::string ref = chs[i].get("ref").str();
        m.chById[c.id] = c;
        m.chByRef[Lower(ref)] = c.id;
        m.chRefs.insert(ref);
    }
    const std::vector<Json> &fl = doc.get("unlock_flags").items();
    for (size_t i = 0; i < fl.size(); i++) {
        MpFlagRow f;
        f.name = fl[i].get("name").str();
        f.challenge = fl[i].get("challenge").str();
        f.id = fl[i].get("id").asInt();
        f.tier = fl[i].get("tier").asInt();
        m.progFlags.push_back(f);
    }
    const Json &curves = doc.get("weapon_curves");
    const Json::Members &ws = doc.get("weapons").members();
    for (size_t i = 0; i < ws.size(); i++) {
        MpWeapon w;
        w.name = ws[i].first;
        w.item = ws[i].second.get("item").asInt();
        const std::vector<Json> &cv = curves.get(ws[i].second.get("curve").str()).items();
        for (size_t k = 0; k < cv.size(); k++) w.curve.push_back(cv[k].asInt());
        if (w.curve.empty()) w.curve.push_back(0);
        m.weapons[Lower(w.name)] = w;
    }
    m.progOk = true;
}

/* Challenges.get(ref): by numeric id or by (case-insensitive) ref. */
static const MpChallenge *FindChallenge(const MarketState &m, const std::string &ref)
{
    std::string t = Strip(ref);
    int64_t id = 0;
    size_t d = t.find_first_not_of('-');
    if (d != std::string::npos && MpIsDigits(t.substr(d))) {
        if (!MpPyInt(t, id)) return NULL;
    } else {
        std::map<std::string, int64_t>::const_iterator r = m.chByRef.find(Lower(t));
        if (r == m.chByRef.end()) return NULL;
        id = r->second;
    }
    std::map<int64_t, MpChallenge>::const_iterator c = m.chById.find(id);
    return c == m.chById.end() ? NULL : &c->second;
}

/* progression.weapon_progress: {lower-cased weapon: counter}. -> false where Python would raise. */
static bool WeaponProgress(const Json &profile, std::map<std::string, int64_t> &out)
{
    const Json &raw = profile.get("weapons");
    if (!raw.truthy()) return true;
    if (!raw.isObject()) return false;
    const Json::Members &mm = raw.members();
    for (size_t i = 0; i < mm.size(); i++) {
        int64_t v;
        if (JsonInt(mm[i].second, v)) out[Lower(mm[i].first)] = v;
    }
    return true;
}

/* Weapon.level_for_xp: curve[n] is the cumulative XP at which level n begins. */
static int WeaponLevel(const MpWeapon &w, int64_t xp)
{
    int level = 0;
    if (xp <= 0) return 0;
    for (size_t n = 0; n < w.curve.size(); n++) {
        if (xp >= w.curve[n]) level = (int)n;
        else break;
    }
    return level;
}

/* progression.granted_flag_ids(profile): challenge flags from the stored progress (mastery rows
 * overwritten by the weapon levels they mirror), uf_rank_N at level >= N, the rest unconditional.
 * -> false where the Python function returns [] (no tables, an unreadable profile). */
static bool ProgressionFlagIds(Server &s, MarketState &m, const Json &profile, int64_t level, std::vector<int64_t> &out)
{
    LoadProgression(s, m);
    if (!m.progOk) return false;
    std::map<int64_t, double> progress;
    const Json &raw = profile.get("challenges");
    if (raw.truthy()) {
        if (!raw.isObject()) return false;
        const Json::Members &mm = raw.members();
        for (size_t i = 0; i < mm.size(); i++) {
            int64_t id;
            double v;
            if (MpPyInt(mm[i].first, id) && JsonFloat(mm[i].second, v)) progress[id] = v;
        }
    }
    std::map<std::string, int64_t> wp;
    if (!WeaponProgress(profile, wp)) return false;
    for (std::map<std::string, int64_t>::iterator it = wp.begin(); it != wp.end(); ++it) { /* mastery_progress */
        std::map<std::string, MpWeapon>::const_iterator w = m.weapons.find(it->first);
        if (w == m.weapons.end()) continue;
        std::string ref = "ch_" + w->second.name + "_mastery";
        if (!m.chRefs.count(ref)) continue;
        const MpChallenge *c = FindChallenge(m, ref);
        if (c) progress[c->id] = (double)WeaponLevel(w->second, it->second);
    }
    std::set<int64_t> got;
    for (size_t i = 0; i < m.progFlags.size(); i++) {
        const MpFlagRow &f = m.progFlags[i];
        if (!f.id || f.name == kHideFlag) continue;
        /* challenges.ALWAYS_GRANTED_FLAGS (Python 50fc5ef): cf_ch_zm_prototype_sh_1 (668) is the
         * notification un_zm_prototype_sh, which C1's Zombies button tests: set, it opens the room
         * list and room creator; unset, every click starts the first-play tutorial room. Nothing
         * credits that challenge, so it is granted with no progress. */
        if (f.name == "cf_ch_zm_prototype_sh_1") {
            got.insert(f.id);
            continue;
        }
        if (!f.challenge.empty()) {
            const MpChallenge *c = FindChallenge(m, f.challenge);
            if (!c) continue;
            std::map<int64_t, double>::iterator p = progress.find(c->id);
            double have = p == progress.end() ? 0.0 : p->second;
            int64_t done = 0; /* Challenge.completed: tiers reached, in order */
            for (size_t k = 0; k < c->targets.size() && have >= c->targets[k]; k++) done++;
            if (done >= (f.tier ? f.tier : 1)) got.insert(f.id);
            continue;
        }
        if (!f.name.compare(0, 8, "uf_rank_")) {
            int64_t need;
            if (MpPyInt(f.name.substr(8), need) && level >= need) got.insert(f.id);
            continue;
        }
        got.insert(f.id);
    }
    out.assign(got.begin(), got.end());
    return !out.empty();
}

/* granted_flags(level, profile): the task-30 flag set. `profile` may be null (no account). */
static std::vector<int64_t> GrantedFlags(Server &s, int64_t level, const Json &profile)
{
    MarketState &m = Market(s);
    std::vector<int64_t> ids;
    if (!ProgressionFlagIds(s, m, profile, level, ids)) {
        /* No progression tables: the rank gate off unlock_flags.json rather than an empty set, which
         * would zero every front-end unlock. */
        LoadUnlockFlags(s, m);
        std::set<int64_t> out;
        for (size_t i = 0; i < m.flagRows.size(); i++) {
            const std::string &name = m.flagRows[i].first;
            int64_t fid = m.flagRows[i].second;
            if (!name.compare(0, 8, "uf_rank_")) {
                if (1 <= fid && fid <= level) out.insert(fid);
            } else if (name != kHideFlag) {
                out.insert(fid);
            }
        }
        ids.assign(out.begin(), out.end());
    }
    if (ids.size() > PD_MAX_FLAGS) ids.resize(PD_MAX_FLAGS);
    return ids;
}

/* _player_profile(uid): the account's profile, never created here. -> false when there is none. */
static bool PlayerProfile(Server &s, uint64_t uid, Json &prof)
{
    if (uid && ProfileLoad(s, uid, prof, false) && prof.isObject()) return true;
    prof = Json();
    return false;
}

/* _player_level(uid): the 1-based level, 0 when there is no profile (satisfies no rank flag). */
static int64_t ProfileLevel(const Json &prof) { return prof.isObject() ? prof.get("level").asInt() : 0; }

std::vector<int64_t> MpGrantedFlagsFor(Server &s, uint64_t uid, int64_t *levelOut)
{
    Json prof;
    PlayerProfile(s, uid, prof);
    int64_t level = ProfileLevel(prof);
    if (levelOut) *levelOut = level;
    return GrantedFlags(s, level, prof);
}

/* ---- wallet ---- */

/* _wallet_db(): resources/wallet.json is the SEED, imported once per store: a (bucket, currency)
 * the store already knows is left alone, so a balance that has moved since is never rolled back. */
static void WalletSeed(Server &s)
{
    MarketState &m = Market(s);
    if (m.walletSeeded) return;
    m.walletSeeded = true;
    Bytes text;
    Json doc;
    if (!s.Resource("wallet.json", text) || !Json::Parse(text, doc) || !doc.isObject()) return;
    Store &st = GetStore(s);
    const Json::Members &buckets = doc.members();
    for (size_t i = 0; i < buckets.size(); i++) {
        if (!buckets[i].second.isObject()) continue;
        std::string key = Strip(buckets[i].first);
        uint64_t uid = 0;
        if (key != "shared" && !key.empty() && key != "None") {
            if (!MpIsDigits(key) || key.size() > 20) continue;
            uid = strtoull(key.c_str(), NULL, 10);
        }
        const Json::Members &curs = buckets[i].second.members();
        for (size_t k = 0; k < curs.size(); k++) {
            int64_t cur;
            if (!MpPyInt(curs[k].first, cur)) continue;
            if (!st.WalletHas(uid, (int)cur)) st.WalletSet(uid, (int)cur, curs[k].second.asInt());
        }
    }
}

/* A (uid, currency) pair the store has never seen reads 0 (CODOL_MARKETPLACE_BALANCE=0) and is not created. */
int64_t MpWalletGet(Server &s, uint64_t uid, int currency)
{
    WalletSeed(s);
    return WalletClamp(GetStore(s).WalletGet(uid, currency, 0));
}

int64_t MpWalletAdd(Server &s, uint64_t uid, int currency, int64_t amount)
{
    WalletSeed(s);
    return GetStore(s).WalletCredit(uid, currency, amount, 0);
}

void MpWalletSet(Server &s, uint64_t uid, int currency, int64_t amount)
{
    WalletSeed(s);
    GetStore(s).WalletSet(uid, currency, amount);
}

/* ---- grants ---- */

static MpOwnedRows RowsOf(const std::vector<InvRow> &inv)
{
    MpOwnedRows out;
    for (size_t i = 0; i < inv.size(); i++) {
        MpOwned r;
        r.expiry = inv[i].expiresAt;
        r.sku = inv[i].skuId;
        r.grant = (int64_t)inv[i].acquiredAt;
        r.qty = inv[i].quantity;
        out[inv[i].itemId] = r;
    }
    return out;
}

static std::vector<InvRow> InvOf(const MpOwnedRows &rows)
{
    std::vector<InvRow> out;
    for (MpOwnedRows::const_iterator it = rows.begin(); it != rows.end(); ++it) {
        InvRow r;
        r.itemId = it->first;
        r.quantity = it->second.qty;
        r.expiresAt = it->second.expiry;
        r.acquiredAt = (double)it->second.grant;
        r.skuId = it->second.sku;
        out.push_back(r);
    }
    return out;
}

/* _load_owned_file: the grants file, now only the seed. Five columns
 *     itemId  expiryEpoch(0=permanent)  skuId  grantEpoch  qty   # name
 * A legacy four-column line carries no count and an expiry that was a blanket 30-day artefact: both
 * are recovered from the sku it names (the pack size from the product, the term from the sku). */
static MpOwnedRows LoadOwnedFile(Server &s, uint64_t uid)
{
    MarketState &m = Market(s);
    MpLoadCatalogue(s, m);
    MpOwnedRows out;
    Bytes text;
    std::string own = Fmt("purchased_items.%llu.txt", (unsigned long long)uid);
    if (!(uid && s.ResourceExists(own) && s.Resource(own, text)) && !s.Resource("purchased_items.txt", text)) return out;
    std::vector<std::string> lines = MpSplitLines(text);
    for (size_t n = 0; n < lines.size(); n++) {
        std::vector<std::string> p = MpSplitWs(lines[n].substr(0, lines[n].find('#')));
        if (p.empty() || !MpIsDigits(p[0])) continue;
        int64_t v[5] = { 0, 0, 0, 0, 0 };
        bool have[5] = { false, false, false, false, false }, bad = false;
        for (size_t i = 0; i < 5 && i < p.size(); i++) {
            size_t d = p[i].find_first_not_of('-');
            if (d == std::string::npos || !MpIsDigits(p[i].substr(d))) continue;
            if (!MpPyInt(p[i], v[i])) { bad = true; break; } /* "--5": int() raises, the rest of the file is lost */
            have[i] = true;
        }
        if (bad) break;
        MpOwned r;
        int64_t iid = v[0];
        r.expiry = v[1];
        r.sku = v[2];
        r.grant = v[3];
        if (have[4]) {
            r.qty = std::max<int64_t>(0, v[4]);
        } else {
            std::map<int64_t, size_t>::iterator sk = m.skuById.find(r.sku);
            const MpSku *sku = sk == m.skuById.end() ? NULL : &m.skus[sk->second];
            r.qty = 1; /* _legacy_qty */
            if (m.stackable.count(iid) && sku) {
                std::map<int64_t, size_t>::iterator pr = m.productById.find(sku->hasProduct ? sku->product : -1);
                if (pr != m.productById.end()) {
                    const MpPairs &pairs = m.products[pr->second].pairs;
                    for (size_t k = 0; k < pairs.size(); k++)
                        if (pairs[k].first == iid) { r.qty = std::max<int64_t>(1, pairs[k].second ? pairs[k].second : 1); break; }
                }
            }
            if (m.stackable.count(iid)) r.expiry = 0; /* _legacy_expiry: counted, never clocked */
            else if (sku) r.expiry = sku->days <= 0 ? 0 : ((r.grant + sku->days * 86400) & 0xFFFFFFFFLL);
        }
        out[iid] = r;
    }
    return out;
}

/* load_owned(uid): the account's own rows; none -> the shared bucket's; none there either -> the
 * grants file is imported for this uid, once. */
MpOwnedRows MpLoadOwned(Server &s, uint64_t uid)
{
    Store &st = GetStore(s);
    std::vector<InvRow> inv = st.InventoryList(uid, false);
    if (inv.empty() && uid) inv = st.InventoryList(0, false);
    if (!inv.empty()) return RowsOf(inv);
    MpOwnedRows seeded = LoadOwnedFile(s, uid);
    if (!seeded.empty()) st.InventoryReplace(uid, InvOf(seeded), "seed");
    return seeded;
}

/* export_owned: the match server's copy of the PURCHASES (never the starter inventory); it gates the
 * Pro perks and cannot ask the lobby, so it reads owned_items[.<uid>].txt beside mpdata_s.<uid>.
 *     itemId  expiryEpoch(0=permanent)  qty   # name
 * Rewritten only when the text changes. */
void MpExportOwned(Server &s, uint64_t uid, const MpOwnedRows &rows)
{
    if (s.cfg.matchProfile.empty()) return;
    MarketState &m = Market(s);
    std::string text = "# owned_items -- purchases exported by the offline lobby for the match server\n"
                       "# columns: itemId  expiryEpoch(0=permanent)  qty   # name\n";
    for (MpOwnedRows::const_iterator it = rows.begin(); it != rows.end(); ++it) {
        std::string key = Fmt("%lld", (long long)it->first);
        std::map<std::string, std::string>::const_iterator nm = m.itemNames.find(key);
        int64_t exp = it->second.expiry < 0 ? 0xFFFFFFFFLL : (it->second.expiry & 0xFFFFFFFFLL);
        text += Fmt("%-6lld %-11lld %-5lld # %s\n", (long long)it->first, (long long)exp,
                    (long long)std::max<int64_t>(0, it->second.qty), nm == m.itemNames.end() ? "" : nm->second.c_str());
    }
    std::map<uint64_t, std::string>::iterator last = m.exported.find(uid);
    if (last != m.exported.end() && last->second == text) return;
    std::string name = uid ? Fmt("owned_items.%llu.txt", (unsigned long long)uid) : std::string("owned_items.txt");
    if (WriteFileAtomic(PathJoin(s.cfg.matchProfile, name), text)) m.exported[uid] = text;
}

/* save_owned: replace the account's whole grants table, then refresh the match server's copy. */
void MpSaveOwned(Server &s, uint64_t uid, const MpOwnedRows &rows)
{
    GetStore(s).InventoryReplace(uid, InvOf(rows), "purchase");
    MpExportOwned(s, uid, rows);
}

/* ---- starting inventory ---- */

static std::vector<int64_t> IdsFromResource(Server &s, const std::string &name)
{
    std::vector<int64_t> out;
    Bytes text;
    if (!s.Resource(name, text)) return out;
    std::vector<std::string> lines = MpSplitLines(text);
    for (size_t i = 0; i < lines.size(); i++) {
        std::vector<std::string> p = MpSplitWs(lines[i]);
        int64_t id;
        if (!p.empty() && MpIsDigits(p[0]) && MpPyInt(p[0], id)) out.push_back(id);
    }
    return out;
}

/* _equipped_item_ids: every item id the mpdata about to be served to this uid references (the
 * account's own upload, else the shared copy, else the level-1 seed). Empty when it cannot be decoded. */
static std::set<int64_t> EquippedItemIds(Server &s, uint64_t uid)
{
    std::set<int64_t> out;
    Store &st = GetStore(s);
    Bytes blob;
    if (!(uid && st.GetStorageFile(uid, "mpdata", blob, false)) && !st.GetStorageFile(0, "mpdata", blob, false) &&
        !s.Resource("mpdata.level1.proposed", blob))
        return out;
    std::set<int> ids = PdEquippedItemIds(s, blob);
    out.insert(ids.begin(), ids.end());
    return out;
}

/* _load_item_ids: which items a player owns. Policy, in order:
 *   items_owned.txt (an author override, verbatim), else items_cha_default.proposed (the curated
 *   appearance base) + every id the served mpdata has equipped, else the whole item table;
 *   + the purchases with a count left; - a Pro perk that was not bought; + the four Zombies
 *   characters complete, - the zm heads the roster does not list; - the shelf-hidden items
 *   (unless equipped: an equipped item missing from the inventory is dropped by the client). */
static std::vector<int64_t> LoadItemIds(Server &s, uint64_t uid, const MpOwnedRows &purchased, std::vector<int64_t> *exempted)
{
    MarketState &m = Market(s);
    MpLoadCatalogue(s, m);
    std::set<int64_t> equipped = EquippedItemIds(s, uid);
    std::vector<int64_t> ids;
    std::set<int64_t> have;
    if (s.ResourceExists("items_owned.txt")) {
        ids = IdsFromResource(s, "items_owned.txt");
    } else if (s.ResourceExists("items_cha_default.proposed")) {
        std::vector<int64_t> base = IdsFromResource(s, "items_cha_default.proposed");
        base.insert(base.end(), equipped.begin(), equipped.end());
        for (size_t i = 0; i < base.size(); i++)
            if (have.insert(base[i]).second) ids.push_back(base[i]);
    } else {
        ids = IdsFromResource(s, s.ResourceExists("items_cha_unlockall.proposed") ? "items_cha_unlockall.proposed" : "items_cha.txt");
    }
    have.clear();
    have.insert(ids.begin(), ids.end());
    /* A stackable item spent down to 0 is not folded in: the client's owned-row finder 0x0069D9E0
     * requires qty > 0, and dropping the row is what puts the consumable back on the shelf. */
    for (MpOwnedRows::const_iterator it = purchased.begin(); it != purchased.end(); ++it)
        if (it->second.qty > 0 && have.insert(it->first).second) ids.push_back(it->first);
    if (!m.proBase.empty()) { /* a Pro perk is owned only when it was bought */
        std::vector<int64_t> keep;
        for (size_t i = 0; i < ids.size(); i++) {
            MpOwnedRows::const_iterator p = purchased.find(ids[i]);
            if (!m.proBase.count(ids[i]) || (p != purchased.end() && p->second.qty > 0)) keep.push_back(ids[i]);
        }
        ids.swap(keep);
        have.clear();
        have.insert(ids.begin(), ids.end());
    }
    for (size_t i = 0; i < sizeof(kZmCoreGrant) / sizeof(kZmCoreGrant[0]); i++)
        if (have.insert(kZmCoreGrant[i]).second) ids.push_back(kZmCoreGrant[i]);
    /* The product only (not the Python lobby): a weapon the Kernel33 patch zone mp_weapons
     * releases but the client's store tables have no sku for cannot be bought, so nobody could ever
     * own it. The M14 EBR (m14ebr_mp, item 1057, product 1365 with no sku) is owned by every
     * offline profile. */
    if (!s.cfg.differential)
        for (size_t i = 0; i < sizeof(kReleasedUnsoldWeapons) / sizeof(kReleasedUnsoldWeapons[0]); i++)
            if (have.insert(kReleasedUnsoldWeapons[i]).second) ids.push_back(kReleasedUnsoldWeapons[i]);
    std::set<int64_t> drop(kZmNonCore, kZmNonCore + sizeof(kZmNonCore) / sizeof(kZmNonCore[0]));
    for (std::set<int64_t>::iterator h = m.hidden.begin(); h != m.hidden.end(); ++h) {
        if (!equipped.count(*h)) drop.insert(*h);
        else if (exempted) exempted->push_back(*h);
    }
    std::vector<int64_t> keep;
    for (size_t i = 0; i < ids.size(); i++)
        if (!drop.count(ids[i])) keep.push_back(ids[i]);
    return keep;
}

/* ---- records ---- */

/* bdMarketplaceInventoryItem, as 0x00A3B470 reads it (array 0x6ED16B8, stride 0x30):
 *   +0x08 UInt64 instance | +0x10 UInt32 itemId | +0x14 UInt32 quantity (the stack count on screen)
 *   +0x18 UInt32 xp (a weapon's level bar: digest 0x005BFDB0 -> 0x0069E0C0 -> rank table 0x0049F170)
 *   +0x1C UInt32 expiry (epoch seconds; 0 / -1 = permanent, 0x0069E030)
 *   +0x20 Int64 (ctor default "never") | +0x28 UInt16 | +0x2C UInt32 */
static Bytes InventoryRecord(int64_t itemId, uint64_t instance, int64_t qty, int64_t expiry, int64_t xp)
{
    return w_u64(instance) + w_u32((uint32_t)itemId) + w_u32((uint32_t)qty) + w_u32((uint32_t)std::max<int64_t>(0, xp)) +
           w_u32((uint32_t)expiry) + w_i64(0x7FFFFFFFFFFFFFFFLL) + w_u16(0) + w_u32(0);
}

/* item_expiry(): an item that was never bought is permanent unless resources/item_expiry_days.txt
 * (`<itemId> <days>`) gives it a term. */
static int64_t ItemExpiry(Server &s, MarketState &m, int64_t itemId, int64_t now)
{
    if (!m.expiryLoaded) {
        m.expiryLoaded = true;
        Bytes text;
        if (s.Resource("item_expiry_days.txt", text)) {
            std::vector<std::string> lines = MpSplitLines(text);
            for (size_t i = 0; i < lines.size(); i++) {
                std::vector<std::string> p = MpSplitWs(lines[i].substr(0, lines[i].find('#')));
                int64_t id, days;
                if (p.size() >= 2 && MpPyInt(p[0], id) && MpPyInt(p[1], days)) m.expiryDays[id] = days;
            }
        }
    }
    std::map<int64_t, int64_t>::iterator it = m.expiryDays.find(itemId);
    int64_t days = it == m.expiryDays.end() ? 0 : it->second;
    return days <= 0 ? 0 : now + days * 86400;
}

/* shipped_flags(): the product's own unlock flags, wire order, max 4 (the count is a byte at
 * product+0x244 and DWProductTable has columns 24..27 for them), minus the shelf-hide sentinel.
 * NOT filtered by level: the client's own gate (0x0069A780) decides, against the set task 30 delivers. */
static Bytes ProductRecord(const MarketState &m, const MpProduct &p)
{
    std::vector<int64_t> flags = MpUnlockFlags(m, &p), keep;
    for (size_t i = 0; i < flags.size() && keep.size() < 4; i++)
        if (flags[i] != m.hideFlag) keep.push_back(flags[i]);
    size_t npairs = std::min<size_t>(p.pairs.size(), 10);
    /* bdMarketplaceProduct::deserialize 0x00A3AF50 (getSize 0x258): productId, three blobs (empty in
     * the shipped tables), UInt16, UInt32, pair count + {itemId, quantity}, flag count + flag ids. */
    Bytes out = w_u32((uint32_t)p.id) + w_blob("") + w_blob("") + w_blob("") + w_u16(0) + w_u32(0) + w_u32((uint32_t)npairs);
    for (size_t i = 0; i < npairs; i++) out += w_u32((uint32_t)p.pairs[i].first) + w_u32((uint32_t)p.pairs[i].second);
    out += w_u32((uint32_t)keep.size());
    for (size_t i = 0; i < keep.size(); i++) out += w_u32((uint32_t)keep[i]);
    return out;
}

/* bdMarketplaceSku::deserialize 0x00A3A6C0 (getSize 0x16C), in READ order. The price element is
 * bdMarketplaceCurrency (0x00A3B810): UChar8 currencyId then UInt32 amount - a UInt32 id there
 * desyncs every record. */
static Bytes SkuRecord(const MpSku &k)
{
    size_t nprices = std::min<size_t>(k.prices.size(), 10);
    Bytes out = w_u32((uint32_t)k.id) + w_u32((uint32_t)k.product) + w_u8(1) + w_blob("") + w_u8((unsigned)k.f4e) +
                w_u32((uint32_t)k.f50) + w_u32((uint32_t)k.f54) + w_u32((uint32_t)k.f58) + w_blob("") + w_u32(0) + w_u16(0) +
                w_u32(0) + w_u8((unsigned)k.f4f) + w_u32((uint32_t)nprices);
    for (size_t i = 0; i < nprices; i++) out += w_u8((unsigned)k.prices[i].first) + w_u32((uint32_t)k.prices[i].second);
    return out;
}

/* ---- replies ---- */

/* getProducts (3) [UInt16 maxResults = 2000] and getSkus (6) [UInt16 4000, Bool]. The records are the
 * same bytes for every account, so they are built once per requested cap. */
static Reply CatalogueReply(Server &s, int task, const Vals &args)
{
    MarketState &m = Market(s);
    MpLoadCatalogue(s, m);
    LoadUnlockFlags(s, m);
    const char *kind = task == MP_GET_PRODUCTS ? "getProducts" : "getSkus";
    if (!m.catError.empty()) return Reply::Ok(Fmt("%s -- no catalogue (%s)", kind, m.catError.c_str()));
    int slots = task == MP_GET_PRODUCTS ? CLIENT_PRODUCT_SLOTS : CLIENT_SKU_SLOTS;
    const Val *reqMax = First(args, T_UINT16);
    int cap = reqMax ? std::min((int)reqMax->u, slots) : slots;
    std::pair<int, int> key(task, cap);
    std::map<std::pair<int, int>, std::vector<Bytes> >::iterator hit = m.replyCache.find(key);
    if (hit == m.replyCache.end()) {
        std::vector<Bytes> recs;
        if (task == MP_GET_PRODUCTS) {
            size_t n = std::min<size_t>(m.products.size(), (size_t)cap);
            recs.reserve(n);
            for (size_t i = 0; i < n; i++) recs.push_back(ProductRecord(m, m.products[i]));
        } else {
            size_t n = std::min<size_t>(m.skus.size(), (size_t)cap);
            recs.reserve(n);
            for (size_t i = 0; i < n; i++) recs.push_back(SkuRecord(m.skus[i]));
        }
        if (m.replyCache.size() > 16) m.replyCache.clear();
        hit = m.replyCache.insert(std::make_pair(key, recs)).first;
    }
    Reply r = Reply::Ok(Fmt("%s %u records (cap=%d)", kind, (unsigned)hit->second.size(), cap));
    r.results = hit->second;
    return r;
}

/* getInventory (10) / getPlayersInventory (13): [UInt16 maxResults] (+ a UInt64 playerId on 13, ignored). */
static Reply InventoryReply(Server &s, uint64_t uid, int task, const Vals &args)
{
    MarketState &m = Market(s);
    MpLoadCatalogue(s, m);
    const Val *reqMax = First(args, T_UINT16);
    size_t cap = reqMax ? std::min<size_t>((size_t)reqMax->u, CLIENT_INV_SLOTS) : (size_t)CLIENT_INV_SLOTS;
    MpOwnedRows owned = MpLoadOwned(s, uid);
    std::vector<int64_t> exempted;
    std::vector<int64_t> ids = LoadItemIds(s, uid, owned, &exempted);
    if (ids.empty()) return Reply::Ok("no starting inventory -- empty");
    if (ids.size() > cap) ids.resize(cap);
    int64_t now = s.Now();
    MpExportOwned(s, uid, owned);
    /* Field 4 is the only channel that gives a weapon a level in C1, so the profile's weapon XP rides
     * out on the inventory (progression.weapon_xp_by_item / WeaponLeveling.xp_by_item). */
    std::map<int64_t, int64_t> xpByItem;
    Json prof;
    std::map<std::string, int64_t> wp;
    if (PlayerProfile(s, uid, prof) && WeaponProgress(prof, wp)) {
        LoadProgression(s, m);
        for (std::map<std::string, int64_t>::iterator it = wp.begin(); it != wp.end(); ++it) {
            std::map<std::string, MpWeapon>::iterator w = m.weapons.find(it->first);
            if (w == m.weapons.end() || !w->second.item || it->second <= 0) continue;
            int64_t &slot = xpByItem[w->second.item];
            slot = std::max(slot, it->second);
        }
    }
    Reply r = Reply::Ok();
    r.results.reserve(ids.size());
    for (size_t n = 0; n < ids.size(); n++) {
        int64_t qty = 1, exp;
        MpOwnedRows::iterator row = owned.find(ids[n]);
        if (row != owned.end()) { /* quantity and expiry are what the purchase recorded */
            if (m.stackable.count(ids[n])) qty = std::max<int64_t>(1, row->second.qty);
            exp = row->second.expiry & 0xFFFFFFFFLL;
        } else {
            exp = ItemExpiry(s, m, ids[n], now);
        }
        std::map<int64_t, int64_t>::iterator xp = xpByItem.find(ids[n]);
        r.results.push_back(InventoryRecord(ids[n], n + 1, qty, exp, xp == xpByItem.end() ? 0 : xp->second));
    }
    r.note = Fmt("%s %u items (cap=%u, %u purchased rows, %u equipped-hidden kept)", task == MP_GET_INVENTORY ? "inventory" : "players-inventory",
                 (unsigned)r.results.size(), (unsigned)cap, (unsigned)owned.size(), (unsigned)exempted.size());
    return r;
}

/* getPlayerData (30): ONE bdMarketplacePlayerData (deserialize 0x00A3AB80, getSize 0xCC48):
 *   UInt64 playerId | UInt32 n + n x currency | UInt32 n + n x inventory item | the flag set
 * playerId = all-FF makes every sub-record read its OWN UInt64 id (otherwise it inherits the parent's
 * and reads nothing), which is what lets the task-10 inventory record be reused verbatim. The counts
 * must be exactly the number of records sent. Answering this empty zeroes the wallet, the owned-item
 * table and the unlock set on every front-end entry. */
static Reply PlayerDataReply(Server &s, uint64_t uid)
{
    /* Front-end entry is the fold point: the client's first request after a match is this one. */
    std::string fold = RankWritebackMaybeApply(s, uid, false);
    if (!fold.empty()) fold += " | ";
    if (uid) {
        std::string blob = EnsureBlob(s, uid);
        if (!blob.empty()) fold += blob + " | ";
    }
    int64_t level = 0;
    std::vector<int64_t> flags = MpGrantedFlagsFor(s, uid, &level);

    Bytes rec = w_u64(0xFFFFFFFFFFFFFFFFULL) + w_u32(2);
    int64_t bal[2];
    for (int cur = 1; cur <= 2; cur++) { /* element reader 0x00A3B220: UInt64 id, UChar8 currency, UInt32 amount */
        bal[cur - 1] = MpWalletGet(s, uid, cur);
        rec += w_u64(0) + w_u8((unsigned)cur) + w_u32((uint32_t)bal[cur - 1]);
    }
    Reply inv = InventoryReply(s, uid, MP_GET_INVENTORY, Vals());
    size_t items = std::min<size_t>(inv.results.size(), PD_MAX_ENTITLEMENTS);
    rec += w_u32((uint32_t)items);
    for (size_t i = 0; i < items; i++) rec += inv.results[i];
    rec += w_u64(0) + w_u32((uint32_t)flags.size()); /* reader 0x00A3B680: UInt64 id, UInt32 count, count x UInt32 */
    for (size_t i = 0; i < flags.size(); i++) rec += w_u32((uint32_t)flags[i]);

    Reply r = Reply::Ok(Fmt("%splayerData level=%lld wallet 1=%lld 2=%lld, %u entitlements, %u unlock flags, %u B", fold.c_str(),
                            (long long)level, (long long)bal[0], (long long)bal[1], (unsigned)items, (unsigned)flags.size(),
                            (unsigned)rec.size()));
    r.results.push_back(rec);
    return r;
}

/* bdMarketplaceCurrency (0x00A3B810): UChar8 currencyId, UInt32 amount. A zero id is dropped by the
 * client (0x004D1A94), so the ids are 1..n. */
static Reply BalanceReply(Server &s, uint64_t uid, uint64_t n, const char *what)
{
    Reply r = Reply::Ok(what);
    if (n > MAX_BALANCE_RECORDS) n = MAX_BALANCE_RECORDS;
    for (int cur = 1; cur <= (int)n; cur++) {
        int64_t bal = MpWalletGet(s, uid, cur);
        r.results.push_back(w_u8((unsigned)cur) + w_u32((uint32_t)bal));
        r.note += Fmt(" %d=%lld", cur, (long long)bal);
    }
    return r;
}

/* ==== purchaseSkus (80/26) and consumeInventoryItem (80/12): demonware/dw_marketplace_buy.py ====
 *
 * Request shapes, MEASURED from the builders in C1.exe:
 *   purchaseSkus 0x00A37930:  UInt32 count | count x UInt32 skuId | UInt32 count | count x UInt32 quantity |
 *                             Bool | UInt32 count | count x UInt64 | UInt32
 *     (the client fills the quantity array with a literal 1, 0x0069F465)
 *   consumeInventoryItem 0x00A38260:  UInt64 localUserId | UInt32 itemId | UInt32 count | Bool
 *
 * Where each number comes from:
 *   the AMOUNT is on the product: its (itemId, quantity) pairs, 100 on the grenade products;
 *   the TERM is on the sku: `days` (mp/coMarketplaceSkus.csv col6 -> +0x58), 0 == permanent;
 *   a STACKABLE item (mp/coMarketplaceItems.csv col9 == 1) is counted, never clocked.
 * The client reads an item as permanently owned only with qty > 0 (0x0069D9E0) and expiry 0 / -1
 * (0x0069E030), so a permanent grant is stored with expiry 0.
 *
 * The reply is empty success: the purchase reply is not what puts the item in the inventory (the
 * owned-item table is written only from the getInventory / getPlayerData records), so the job is to
 * debit the wallet, persist the grant and answer safely. A refusal is a real error code and leaves
 * the wallet untouched. The Python lobby's opt-in receipt record (CODOL_MP_PURCHASE_RECEIPT=1,
 * bdMarketplacePurchase 0x00A365A0) is not ported: it is off by default.
 */
namespace {

enum {
    MAX_PURCHASE_MULTIPLIER = 100, /* ours: a sanity ceiling, the client always sends 1 */
    MAX_STACK = 9999,              /* ours: keeps the UInt32 the client reads believable */
    SECONDS_PER_DAY = 86400,
};

struct Wanted { int64_t sku, mult; };

/* _purchase_request: only the tag-8 values matter (the u64 block is tag 10), so the flat stream is
 * [count, sku..., count, qty..., count, trailing]. An implausible leading count falls back to "every
 * tag-8 value", multiplier 1. */
std::vector<Wanted> PurchaseRequest(const Vals &args)
{
    std::vector<int64_t> u32s;
    for (size_t i = 0; i < args.size(); i++)
        if (args[i].tag == T_UINT32) u32s.push_back((int64_t)args[i].u);
    std::vector<Wanted> out;
    if (u32s.empty()) return out;
    size_t count = (size_t)u32s[0];
    if (!(u32s[0] >= 1 && u32s[0] <= 5 && u32s.size() >= 1 + count)) { /* 5 = the console argc cap (0xC4FF08) */
        for (size_t i = 0; i < u32s.size() && i < 5; i++) {
            Wanted w = { u32s[i], 1 };
            out.push_back(w);
        }
        return out;
    }
    size_t rest = 1 + count;
    bool haveQty = u32s.size() > rest && u32s[rest] == (int64_t)count && u32s.size() - rest >= 1 + count;
    for (size_t i = 0; i < count; i++) {
        Wanted w = { u32s[1 + i], haveQty ? u32s[rest + 1 + i] : 1 };
        if (w.mult < 1) w.mult = 1;
        if (w.mult > MAX_PURCHASE_MULTIPLIER) w.mult = MAX_PURCHASE_MULTIPLIER;
        out.push_back(w);
    }
    return out;
}

bool PermanentExpiry(int64_t exp) { return exp == 0 || exp == -1 || exp == 0xFFFFFFFFLL; }

/* owns_permanently: what the client reads as isPermanentHave. A stackable item never is. */
bool OwnsPermanently(const MarketState &m, const MpOwnedRows &rows, int64_t item)
{
    if (m.stackable.count(item)) return false;
    MpOwnedRows::const_iterator r = rows.find(item);
    return r != rows.end() && r->second.qty > 0 && PermanentExpiry(r->second.expiry);
}

/* perk_upgrades.owns: qty > 0 and permanent, or a rental that has not ended. */
bool Owns(const MpOwnedRows &rows, int64_t item, int64_t now)
{
    MpOwnedRows::const_iterator r = rows.find(item);
    return r != rows.end() && r->second.qty > 0 && (PermanentExpiry(r->second.expiry) || r->second.expiry > now);
}

/* extend_expiry: 0 days == permanent and overrides a running clock; a live rental extends from where
 * it ends, a dead or absent one restarts from now. */
int64_t ExtendExpiry(int64_t current, int64_t now, int64_t days)
{
    if (days <= 0) return 0;
    int64_t base = current > now ? current : now;
    return (base + days * SECONDS_PER_DAY) & 0xFFFFFFFFLL;
}

std::string JoinIds(const std::vector<int64_t> &ids)
{
    std::string out;
    for (size_t i = 0; i < ids.size(); i++) out += Fmt("%s%lld", i ? "," : "", (long long)ids[i]);
    return out;
}

/* _perk_gate (catalogue/perk_upgrades.py, THE RULE): a perk can be bought once its product's unlock
 * flags are earned, a Pro once its base perk is owned. Only `per` items are gated. -> 0 = let through. */
int PerkGate(const MarketState &m, const MpPairs &pairs, const MpOwnedRows &rows, int64_t now, const std::set<int64_t> &granted,
             const MpProduct *product, std::string &why)
{
    std::vector<int64_t> perks;
    for (size_t i = 0; i < pairs.size(); i++)
        if (m.perkIds.count(pairs[i].first)) perks.push_back(pairs[i].first);
    if (perks.empty()) return 0;
    for (size_t i = 0; i < perks.size(); i++) {
        std::map<int64_t, int64_t>::const_iterator base = m.proBase.find(perks[i]);
        if (base != m.proBase.end() && !Owns(rows, base->second, now)) {
            why = Fmt("Pro item %lld needs its base perk %lld owned first", (long long)perks[i], (long long)base->second);
            return BD_MARKETPLACE_MISSING_REQUIRED_ENTITLEMENT;
        }
    }
    std::vector<int64_t> need = MpUnlockFlags(m, product), missing;
    for (size_t i = 0; i < need.size(); i++)
        if (!granted.count(need[i])) missing.push_back(need[i]);
    if (!missing.empty()) {
        why = "perk still locked, unlock flag(s) [" + JoinIds(missing) + "] not earned";
        return BD_MARKETPLACE_PRECONDITION_REQUIRED;
    }
    return 0;
}

Reply PurchaseSkus(Server &s, uint64_t uid, const Vals &args)
{
    MarketState &m = Market(s);
    MpLoadCatalogue(s, m);
    if (!m.catError.empty()) return Reply::Ok("purchaseSkus -- no catalogue (" + m.catError + ") -- empty success");
    int64_t now = s.Now();
    std::vector<Wanted> wanted = PurchaseRequest(args);
    if (wanted.empty()) return Reply::Ok("purchaseSkus with no sku id in args -- empty success");

    /* The perk gate tests a perk's unlock flags against the very set task 30 serves this player, so
     * the store refuses exactly what it padlocks. */
    std::vector<int64_t> flagIds = MpGrantedFlagsFor(s, uid);
    std::set<int64_t> granted(flagIds.begin(), flagIds.end());

    MpOwnedRows rows = MpLoadOwned(s, uid);
    std::string shown;
    bool changed = false;
    int refused = 0, refusal = 0; /* the first refusal's error code */
    for (size_t w = 0; w < wanted.size(); w++) {
        int64_t skuId = wanted[w].sku, mult = wanted[w].mult;
        if (!shown.empty()) shown += "; ";
        std::map<int64_t, size_t>::iterator sk = m.skuById.find(skuId);
        if (sk == m.skuById.end()) {
            shown += Fmt("sku %lld NOT FOUND (no-op)", (long long)skuId);
            continue;
        }
        const MpSku &sku = m.skus[sk->second];
        std::map<int64_t, size_t>::iterator pr = m.productById.find(sku.hasProduct ? sku.product : -1);
        const MpProduct *prod = pr == m.productById.end() ? NULL : &m.products[pr->second];
        MpPairs pairs;
        if (prod) pairs.assign(prod->pairs.begin(), prod->pairs.begin() + (long)std::min<size_t>(prod->pairs.size(), 10));
        int64_t days = sku.days;

        /* ALREADY OWNED: a permanent sku whose every non-stackable item is already held permanently
         * is refused with the client's own code. A duration sku is always allowed (it extends the
         * clock) and so is a stackable item (the count goes up). */
        std::vector<int64_t> durables;
        for (size_t i = 0; i < pairs.size(); i++)
            if (!m.stackable.count(pairs[i].first)) durables.push_back(pairs[i].first);
        bool allOwned = days == 0 && !durables.empty();
        for (size_t i = 0; allOwned && i < durables.size(); i++) allOwned = OwnsPermanently(m, rows, durables[i]);
        if (allOwned) {
            refused++;
            if (!refusal) refusal = BD_MARKETPLACE_ITEM_MULTIPLE_PURCHASE_ERROR;
            shown += Fmt("sku %lld REFUSED 8010 already owned permanently: items[%s]", (long long)skuId, JoinIds(durables).c_str());
            continue;
        }

        std::string why;
        int gate = PerkGate(m, pairs, rows, now, granted, prod, why);
        if (gate) {
            refused++;
            if (!refusal) refusal = gate;
            shown += Fmt("sku %lld REFUSED %d %s", (long long)skuId, gate, why.c_str());
            continue;
        }

        /* FUNDS: a sku the wallet cannot cover is refused with 8007; nothing is charged or granted. */
        int64_t currency = sku.prices.empty() ? 0 : sku.prices[0].first, amount = sku.prices.empty() ? 0 : sku.prices[0].second;
        int64_t cost = amount * mult;
        if (currency) {
            int64_t bal = MpWalletGet(s, uid, (int)currency);
            if (cost > bal) {
                refused++;
                if (!refusal) refusal = BD_MARKETPLACE_INSUFFICIENT_FUNDS;
                shown += Fmt("sku %lld REFUSED 8007 insufficient funds: price %lld:%lld, balance %lld", (long long)skuId,
                             (long long)currency, (long long)cost, (long long)bal);
                continue;
            }
            MpWalletSet(s, uid, (int)currency, bal - cost);
        }

        /* GRANT: the product's pair quantity is the bundle size, the request multiplier scales it. A
         * stackable item accumulates a count and stays permanent; a durable gets qty 1 and the sku's
         * own term, extending whatever is already running. */
        shown += Fmt("sku %lld%s ->", (long long)skuId, mult == 1 ? "" : Fmt(" x%lld", (long long)mult).c_str());
        for (size_t i = 0; i < pairs.size(); i++) {
            int64_t item = pairs[i].first, add = std::max<int64_t>(1, pairs[i].second ? pairs[i].second : 1) * mult;
            MpOwnedRows::iterator cur = rows.find(item);
            MpOwned next;
            if (m.stackable.count(item)) {
                next.qty = std::min<int64_t>(MAX_STACK, (cur == rows.end() ? 0 : cur->second.qty) + add);
                next.expiry = 0;
                shown += Fmt(" %s x%lld (stack now %lld)", MpItemName(m, item).c_str(), (long long)add, (long long)next.qty);
            } else {
                next.qty = 1;
                next.expiry = ExtendExpiry(cur == rows.end() ? 0 : cur->second.expiry, now, days);
                shown += Fmt(" %s x%lld %s", MpItemName(m, item).c_str(), (long long)add,
                             next.expiry ? Fmt("until %lld", (long long)next.expiry).c_str() : "perm");
            }
            next.sku = skuId;
            next.grant = now;
            rows[item] = next;
            changed = true;
        }
        shown += Fmt("  price %lld:%lld term=%s", (long long)currency, (long long)cost,
                     days == 0 ? "permanent" : Fmt("%lldd", (long long)days).c_str());
    }
    if (changed) MpSaveOwned(s, uid, rows);
    std::string note = Fmt("purchaseSkus %u sku(s), %d refused, uid=%llu; %s", (unsigned)wanted.size(), refused,
                           (unsigned long long)uid, shown.c_str());
    /* Every sku refused -> the first refusal's own code, instead of charging. */
    if (refused && !changed) return Reply(refusal ? refusal : (int)BD_MARKETPLACE_ITEM_MULTIPLE_PURCHASE_ERROR, note);
    return Reply::Ok(note);
}

/* consumeInventoryItem: take units off the stack, floored at 0. The reply stays empty success (the
 * wrapper 0x004D3910 passes no result array). At 0 the row stops being served, which is how the
 * client reads "not owned". A non-stackable item is ACKed and left alone. */
Reply Consume(Server &s, uint64_t uid, const Vals &args)
{
    MarketState &m = Market(s);
    MpLoadCatalogue(s, m);
    std::vector<int64_t> u32s;
    for (size_t i = 0; i < args.size(); i++)
        if (args[i].tag == T_UINT32) u32s.push_back((int64_t)args[i].u);
    if (u32s.empty()) return Reply::Ok("consumeInventoryItem with no item id in args -- ACK");
    int64_t item = u32s[0], count = u32s.size() > 1 ? u32s[1] : 1;
    if (count < 1) count = 1;
    if (count > MAX_STACK) count = MAX_STACK;
    std::string nm = MpItemName(m, item);
    if (!m.stackable.count(item))
        return Reply::Ok(Fmt("consumeInventoryItem %lld (%s) x%lld -- not stackable, ACK, row untouched", (long long)item, nm.c_str(),
                             (long long)count));
    MpOwnedRows rows = MpLoadOwned(s, uid);
    MpOwnedRows::iterator row = rows.find(item);
    if (row == rows.end() || row->second.qty <= 0)
        return Reply::Ok(Fmt("consumeInventoryItem %lld (%s) x%lld -- no stack owned, ACK", (long long)item, nm.c_str(), (long long)count));
    int64_t was = row->second.qty;
    row->second.qty = std::max<int64_t>(0, was - count);
    MpSaveOwned(s, uid, rows);
    return Reply::Ok(Fmt("consumeInventoryItem %lld (%s) x%lld -- stack %lld -> %lld, uid=%llu", (long long)item, nm.c_str(),
                         (long long)count, (long long)was, (long long)row->second.qty, (unsigned long long)uid));
}

} // namespace

Reply MpPurchaseTask(Server &s, Conn &c, int task, const Vals &args)
{
    if (task == MP_PURCHASE_SKUS) return PurchaseSkus(s, c.uid, args);
    if (task == MP_CONSUME_INVENTORY_ITEM) return Consume(s, c.uid, args);
    return Reply();
}

Reply MarketplaceTask(Server &s, Conn &c, int task, const Vals &args)
{
    uint64_t uid = c.uid;
    if (task == MP_DEPOSIT) {
        const Val *cur = First(args, T_UCHAR8), *amount = First(args, T_UINT32);
        if (!cur) return Reply::Ok("deposit with no currency id");
        int64_t now = MpWalletAdd(s, uid, (int)cur->u, amount ? (int64_t)amount->u : 0);
        return Reply::Ok(Fmt("deposit currency=%d +%llu -> %lld", (int)cur->u, (unsigned long long)(amount ? amount->u : 0), (long long)now));
    }
    if (task == MP_GET_BALANCE) {
        const Val *n = First(args, T_UCHAR8);
        return BalanceReply(s, uid, n && n->u ? n->u : 2, "balance");
    }
    if (task == MP_GET_INVENTORY || task == MP_GET_PLAYERS_INVENTORY) return InventoryReply(s, uid, task, args);
    if (task == MP_PURCHASE_SKUS || task == MP_CONSUME_INVENTORY_ITEM) return MpPurchaseTask(s, c, task, args);
    if (task == MP_GET_BALANCE_V2) { /* the arm this client takes (0xA3714A `push 0x20`): [UInt32 2] */
        const Val *a = First(args, T_UINT32), *b = First(args, T_UCHAR8);
        return BalanceReply(s, uid, a && a->u ? a->u : b && b->u ? b->u : 2, "balance(task32)");
    }
    if (task == MP_GET_PLAYER_DATA) return PlayerDataReply(s, uid);
    if (task == MP_GET_PRODUCTS || task == MP_GET_SKUS) return CatalogueReply(s, task, args);
    if (task == MP_COUPON_LIST) return Reply::Ok("coupon list (maxResults u16) -- answered EMPTY");
    return Reply();
}

} // namespace dw

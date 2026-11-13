/*
 * dw_marketplace.h - the state and internal interface of dw_marketplace.cpp (service 80: catalogue,
 * inventory, wallet, getPlayerData, purchaseSkus / consumeInventoryItem).
 * Ports of demonware/dw_marketplace.py, demonware/dw_marketplace_buy.py, catalogue/perk_upgrades.py
 * and the pure read side of career/challenges.py + career/weapon_leveling.py that the unlock-flag
 * set and the weapon XP field need.
 */
#pragma once
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "dw.h"
#include "dw_json.h"
#include "dw_state.h"
#include "dw_store.h"

namespace dw {

enum {
    MP_DEPOSIT = 1, MP_GET_BALANCE = 2, MP_GET_PRODUCTS = 3, MP_GET_SKUS = 6, MP_GET_INVENTORY = 10,
    MP_CONSUME_INVENTORY_ITEM = 12, MP_GET_PLAYERS_INVENTORY = 13, MP_COUPON_LIST = 20, MP_PURCHASE_SKUS = 26,
    MP_GET_PLAYER_DATA = 30, MP_GET_BALANCE_V2 = 32,
    /* bdLobbyErrorCode, name table built by C1.exe 0x007230E0 */
    BD_MARKETPLACE_INSUFFICIENT_FUNDS = 8007,
    BD_MARKETPLACE_PRECONDITION_REQUIRED = 8009,          /* a perk whose unlock flags are not earned */
    BD_MARKETPLACE_ITEM_MULTIPLE_PURCHASE_ERROR = 8010,   /* already owned permanently */
    BD_MARKETPLACE_MISSING_REQUIRED_ENTITLEMENT = 8011,   /* a Pro perk without its base perk */
};

typedef std::vector<std::pair<int64_t, int64_t> > MpPairs;

struct MpProduct {
    int64_t id;
    MpPairs pairs;               /* (itemId, quantity), all of them; the wire takes the first 10 */
    std::vector<int64_t> flags;  /* unlock flag ids as shipped */
    MpProduct() : id(0) {}
};
struct MpSku {
    int64_t id, product, days, f4e, f50, f54, f58, f4f;
    bool hasProduct;
    MpPairs prices;              /* (currencyId, amount) */
    MpSku() : id(0), product(0), days(0), f4e(1), f50(0), f54(0), f58(0), f4f(0), hasProduct(false) {}
};

/* One row of a player's grants table (dw_marketplace_buy.load_owned: {'expiry','sku','grant','qty'}). */
struct MpOwned {
    int64_t expiry, sku, grant, qty;
    MpOwned() : expiry(0), sku(0), grant(0), qty(1) {}
};
typedef std::map<int64_t, MpOwned> MpOwnedRows;

struct MpFlagRow { std::string name, challenge; int64_t id, tier; };
struct MpChallenge { int64_t id; std::vector<double> targets; };
struct MpWeapon { std::string name; int64_t item; std::vector<int64_t> curve; };

struct MarketState {
    /* resources/marketplace_catalogue.json */
    bool catLoaded;
    std::string catError;        /* non-empty: no catalogue (every reply is then empty) */
    std::vector<MpProduct> products;
    std::vector<MpSku> skus;
    std::map<int64_t, size_t> productById, skuById;
    std::map<std::string, std::string> itemNames; /* "1201" -> "equ/semtex_mp" */
    std::set<int64_t> stackable, hidden;
    /* catalogue/perk_upgrades._tables */
    std::map<int64_t, int64_t> proBase;       /* Pro item id -> base perk item id */
    std::set<int64_t> perkIds;
    std::map<int64_t, size_t> productByItem;  /* first pair's item id -> first product selling it */
    /* resources/unlock_flags.json */
    bool flagsLoaded;
    std::vector<std::pair<std::string, int64_t> > flagRows;
    int64_t hideFlag;
    /* resources/progression_tables.json (challenges, unlock_flags, weapons, weapon_curves) */
    bool progLoaded, progOk;
    std::vector<MpFlagRow> progFlags;
    std::map<int64_t, MpChallenge> chById;
    std::map<std::string, int64_t> chByRef;   /* lower-cased ref -> id */
    std::set<std::string> chRefs;             /* refs as written */
    std::map<std::string, MpWeapon> weapons;  /* lower-cased name */
    /* resources/item_expiry_days.txt */
    bool expiryLoaded;
    std::map<int64_t, int64_t> expiryDays;
    /* getProducts / getSkus records: the same bytes for every account, keyed by (task, cap) */
    std::map<std::pair<int, int>, std::vector<Bytes> > replyCache;
    bool walletSeeded;
    std::map<uint64_t, std::string> exported; /* last owned_items text written per uid */
    MarketState()
        : catLoaded(false), flagsLoaded(false), hideFlag(663), progLoaded(false), progOk(false), expiryLoaded(false),
          walletSeeded(false) {}
};

MarketState &Market(Server &s);
void MpLoadCatalogue(Server &s, MarketState &m);
std::string MpItemName(const MarketState &m, int64_t itemId); /* the catalogue name, or the id as text */

/* wallet (dw_marketplace._wallet_get / _wallet_add / _WalletView) */
int64_t MpWalletGet(Server &s, uint64_t uid, int currency);
int64_t MpWalletAdd(Server &s, uint64_t uid, int currency, int64_t amount);
void MpWalletSet(Server &s, uint64_t uid, int currency, int64_t amount);

/* grants (dw_marketplace_buy.load_owned / save_owned / export_owned) */
MpOwnedRows MpLoadOwned(Server &s, uint64_t uid);
void MpSaveOwned(Server &s, uint64_t uid, const MpOwnedRows &rows);
void MpExportOwned(Server &s, uint64_t uid, const MpOwnedRows &rows);

/* perk_upgrades.unlock_flags: the flags a product is offered behind */
std::vector<int64_t> MpUnlockFlags(const MarketState &m, const MpProduct *product);
/* dw_marketplace.granted_flags(_player_level(uid), _player_profile(uid)) */
std::vector<int64_t> MpGrantedFlagsFor(Server &s, uint64_t uid, int64_t *levelOut = NULL);

/* dw_marketplace_buy.purchase_task (tasks 26 and 12) */
Reply MpPurchaseTask(Server &s, Conn &c, int task, const Vals &args);

/* text helpers */
std::vector<std::string> MpSplitLines(const Bytes &text);
std::vector<std::string> MpSplitWs(const std::string &line);
bool MpIsDigits(const std::string &t);
bool MpPyInt(const std::string &t, int64_t &out); /* Python int(str) for plain decimal text */

} // namespace dw

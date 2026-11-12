/*
 * dw_store.h - the offline lobby's persistent store (replaces the Python lobby's SQLite database,
 * store/lobby_db.py, with versioned JSON documents and atomic writes; docs/OFFLINE_LOBBY.md "Store").
 *
 * Layout under Config::profile:
 *   players/<uid>.json        the career profile, the SAME schema as the Python lobby's
 *                             players\<uid>.json (so an existing career imports by copying the file)
 *   accounts/<uid>.json       {"version":1, account, wallet, inventory, purchases, history}
 *   storage/<uid>/<name>      per-player storage files (mpdata, mpdata_s, Recentmetlist, blacklist)
 *   storage/_shared/<name>    the non-per-player copy
 *   kv.json, reports.json     small durable state (rank write-back bookkeeping, applied reports)
 *
 * Every write is temp-file + atomic replace (WriteFileAtomic). Method semantics follow the LobbyDB
 * method of the same name in store/lobby_db.py unless noted.
 */
#pragma once
#include "dw.h"
#include "dw_json.h"

namespace dw {

struct InvRow {
    int64_t itemId, quantity, expiresAt, skuId;
    double acquiredAt;
    std::string source;
    InvRow() : itemId(0), quantity(1), expiresAt(0), skuId(0), acquiredAt(0) {}
};

class Store {
public:
    Store(Server &server);
    ~Store();

    /* accounts */
    void TouchLogin(uint64_t uid, const std::string &gamertag, const std::string &addr);
    Json GetAccount(uint64_t uid);

    /* storage files; uid 0 = the shared copy */
    bool GetStorageFile(uint64_t uid, const std::string &name, Bytes &out, bool fallbackShared = false);
    bool PutStorageFile(uint64_t uid, const std::string &name, const Bytes &blob, const std::string &source);
    bool DeleteStorageFile(uint64_t uid, const std::string &name);

    /* career profiles: the players/<uid>.json document */
    bool ProfileRead(uint64_t uid, Json &out);
    bool ProfileWrite(uint64_t uid, const Json &prof);
    std::vector<uint64_t> ProfileUids();
    void AddHistory(uint64_t uid, int64_t xp, const std::string &reason, int64_t total, int64_t level);

    /* wallet */
    bool WalletHas(uint64_t uid, int currency);
    int64_t WalletGet(uint64_t uid, int currency, int64_t dflt = 0);
    std::map<int, int64_t> WalletBalances(uint64_t uid);
    int64_t WalletCredit(uint64_t uid, int currency, int64_t amount, int64_t seed = 0);
    /* -> false when the balance is short and allowNegative is false; *after = the new balance */
    bool WalletDebit(uint64_t uid, int currency, int64_t amount, int64_t seed, bool allowNegative, int64_t *after);
    void WalletSet(uint64_t uid, int currency, int64_t amount);

    /* inventory */
    bool InventoryExists(uint64_t uid); /* the account has an inventory of its own */
    std::vector<InvRow> InventoryList(uint64_t uid, bool includeShared = true);
    bool InventoryGet(uint64_t uid, int64_t itemId, InvRow &out);
    void InventoryGrant(uint64_t uid, int64_t itemId, int64_t quantity, int64_t expiresAt, const std::string &source,
                        int64_t skuId, bool stack);
    /* -> the remaining quantity, or -1 when the player does not hold the item */
    int64_t InventoryConsume(uint64_t uid, int64_t itemId, int64_t quantity, bool removeAtZero);
    void InventoryReplace(uint64_t uid, const std::vector<InvRow> &rows, const std::string &source);
    bool InventoryRevoke(uint64_t uid, int64_t itemId);

    /* purchases ledger */
    void RecordPurchase(uint64_t uid, const Json &row);
    Json Purchases(uint64_t uid);

    /* match reports */
    bool HasReport(const std::string &reportId);
    void RecordMatchReport(const std::string &reportId, const Json &report, bool applied);
    void MarkReportApplied(const std::string &reportId, const Json &credited);

    /* key/value */
    Json KvGet(const std::string &key);
    void KvSet(const std::string &key, const Json &value);

    struct Impl;
private:
    Impl *impl_;
};

/* The server's store (created by StateInit). */
Store &GetStore(Server &s);

} // namespace dw

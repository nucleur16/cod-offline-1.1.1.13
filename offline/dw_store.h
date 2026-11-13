/*
 * dw_store.h - what the offline lobby keeps on disk (docs/OFFLINE_LOBBY.md "Store").
 *
 * There is no database. A player is ONE FOLDER under Config::profile (the game's own players\
 * folder), holding the game's own profile blobs exactly as the client receives and uploads them:
 *
 *   <profile>/<name>/mpdata       the client-owned blob (classes, characters), as uploaded
 *   <profile>/<name>/mpdata_s     THE CAREER: rank, XP, lifetime stats, challenge progress. What the
 *                                 client is served at login is this file; a match write-back rewrites
 *                                 it; deleting it starts the career again at level 1.
 *   <profile>/<name>/account.txt  a few "key = value" lines for what neither blob has a field for:
 *                                 the uid the folder belongs to, the wallet, purchased inventory, and
 *                                 the career values mpdata_s cannot hold (weapon XP counters, ...)
 *
 * <name> is the player's profile name (Config::localName, for the account that logs in) or the
 * decimal uid. Everything else the Python lobby kept in SQLite is either not kept at all (login
 * records, purchase ledger, XP history) or lives in memory for the run (other storage files, the
 * shared copies). Two files that are not a player's are kept beside the match reports they belong
 * to, in Config::matchProfile: match_report.state.json, the applied-report ledger of the rank
 * write-back, and mpdata_s, the unkeyed rank blob (the Python lobby's resources\mpdata_s).
 *
 * The career PROFILE the modules work on is still the JSON value of store/player_profiles.py, but it
 * exists in memory only: dw_profiles.cpp builds it from mpdata_s (+ the account.txt career lines) on
 * first use and renders it back on every change. This class holds the cache and the files.
 *
 * Every write is temp-file + atomic replace (WriteFileAtomic). With an empty Config::profile nothing
 * is written at all.
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

    /* ---- the player's folder ---- */
    /* A player logged in. An account that has no folder yet takes Config::localName when that name is
     * free, and its account.txt is written so the folder is recognised next time. */
    void TouchLogin(uint64_t uid, const std::string &gamertag, const std::string &addr);
    /* Give `uid` the folder `name` when the uid has none and the name is usable and free. */
    bool BindFolder(uint64_t uid, const std::string &name);
    /* The folder name of this uid ("<decimal uid>" until one is bound). */
    std::string FolderOf(uint64_t uid);

    /* ---- storage files; uid 0 = the shared copy ----
     * A player's "mpdata" and "mpdata_s" are the files in the folder and the shared "mpdata_s" is a
     * file in Config::matchProfile; any other name lives in memory for the run. */
    bool GetStorageFile(uint64_t uid, const std::string &name, Bytes &out, bool fallbackShared = false);
    bool PutStorageFile(uint64_t uid, const std::string &name, const Bytes &blob, const std::string &source);
    bool DeleteStorageFile(uint64_t uid, const std::string &name);
    /* A player file that cannot be used: moved aside as "<name>.unreadable" (replacing an older one). */
    void QuarantineStorageFile(uint64_t uid, const std::string &name);

    /* ---- the career profile (dw_profiles.cpp owns its meaning) ---- */
    /* The profile built or written earlier in this run. -> false when there is none in memory. */
    bool ProfileRead(uint64_t uid, Json &out);
    /* Remember `prof`; `extras` = the members mpdata_s cannot hold, written as account.txt's career lines. */
    bool ProfileWrite(uint64_t uid, const Json &prof, const Json &extras);
    /* account.txt's career lines as an object (empty when there are none). */
    Json ProfileExtras(uint64_t uid);
    /* Every uid with a profile in memory or an mpdata_s on disk. */
    std::vector<uint64_t> ProfileUids();

    /* ---- wallet ---- */
    bool WalletHas(uint64_t uid, int currency);
    int64_t WalletGet(uint64_t uid, int currency, int64_t dflt = 0);
    std::map<int, int64_t> WalletBalances(uint64_t uid);
    int64_t WalletCredit(uint64_t uid, int currency, int64_t amount, int64_t seed = 0);
    /* -> false when the balance is short and allowNegative is false; *after = the new balance */
    bool WalletDebit(uint64_t uid, int currency, int64_t amount, int64_t seed, bool allowNegative, int64_t *after);
    void WalletSet(uint64_t uid, int currency, int64_t amount);

    /* ---- inventory ---- */
    bool InventoryExists(uint64_t uid); /* the account has rows of its own (else it inherits the shared ones) */
    /* Ordered by item id; a per-uid row wins over a shared one. now != 0 drops rows that have expired. */
    std::vector<InvRow> InventoryList(uint64_t uid, bool includeShared = true, int64_t now = 0);
    bool InventoryGet(uint64_t uid, int64_t itemId, InvRow &out);
    void InventoryGrant(uint64_t uid, int64_t itemId, int64_t quantity, int64_t expiresAt, const std::string &source,
                        int64_t skuId, bool stack);
    /* -> the remaining quantity, or -1 (nothing consumed) when the item is not held or the stack is short */
    int64_t InventoryConsume(uint64_t uid, int64_t itemId, int64_t quantity, bool removeAtZero);
    void InventoryReplace(uint64_t uid, const std::vector<InvRow> &rows, const std::string &source);
    bool InventoryRevoke(uint64_t uid, int64_t itemId);

    /* ---- the rank write-back's bookkeeping (<matchProfile>/match_report.state.json) ---- */
    bool HasReport(const std::string &reportId);
    void RecordMatchReport(const std::string &reportId, const Json &report, bool applied);
    void MarkReportApplied(const std::string &reportId, const Json &credited);
    Json KvGet(const std::string &key);
    void KvSet(const std::string &key, const Json &value);

    struct Impl;
private:
    Impl *impl_;
};

/* The server's store (created by StateInit). */
Store &GetStore(Server &s);

} // namespace dw

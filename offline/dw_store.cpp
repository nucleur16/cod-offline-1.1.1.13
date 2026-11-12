/*
 * dw_store.cpp - the persistent store (dw_store.h): JSON documents and plain files under
 * Config::profile, cached in memory and written through (WriteFileAtomic) on every mutation.
 * Method semantics are store/lobby_db.py's; there is no database, no writer thread and no lock -
 * the server is single-threaded, so every read-modify-write below is already atomic.
 *
 * With an empty Config::profile nothing is written: the store then lives in memory only.
 */
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "dw.h"
#include "dw_state.h"
#include "dw_store.h"

namespace dw {

namespace {

const int64_t kWalletMax = 0xFFFFFFFFLL; /* the client's balance field is a u32 (lobby_db.WALLET_MAX) */
const size_t kHistoryKeep = 500;         /* accounts/<uid>.json is rewritten whole, so it has to forget */
const size_t kPurchasesKeep = 2000;
const size_t kReportRawKeep = 100;       /* reports.json keeps every id, the raw report of the newest ones */

int64_t WalletClamp(int64_t v) { return v < 0 ? 0 : v > kWalletMax ? kWalletMax : v; }

std::string UidKey(uint64_t uid) { return uid ? Fmt("%llu", (unsigned long long)uid) : std::string("_shared"); }

/* A storage name becomes a path below storage/<uid>/: refuse anything that could leave it. */
bool SafeName(const std::string &name)
{
    if (name.empty() || name.size() > 200 || name[0] == '/' || name[0] == '.') return false;
    if (name.find("..") != std::string::npos) return false;
    for (size_t i = 0; i < name.size(); i++) {
        unsigned char c = (unsigned char)name[i];
        if (c < 0x20 || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false;
    }
    return true;
}

struct Account {
    bool exists;                 /* has an account row (lobby_db._ensure_account ran) */
    Json account;                /* {uid, username, gamertag, created_at, last_login, login_count, last_addr} */
    std::map<int, int64_t> wallet;
    std::map<int64_t, InvRow> inv;
    Json purchases, history;     /* arrays, oldest first */
    int64_t nextPurchaseId;
    Account() : exists(false), account(Json::MakeObject()), purchases(Json::MakeArray()), history(Json::MakeArray()), nextPurchaseId(1) {}
};

struct FileEntry { bool present; Bytes data; };
struct ProfEntry { bool present; Json doc; };

} // namespace

struct Store::Impl {
    Server &s;
    std::string root;
    std::map<uint64_t, Account> accounts;
    std::map<uint64_t, ProfEntry> profiles;
    std::map<std::pair<uint64_t, std::string>, FileEntry> files;
    bool kvLoaded, reportsLoaded;
    Json kv, reports; /* {key: value} / {reportId: {...}} */

    Impl(Server &server) : s(server), root(server.cfg.profile), kvLoaded(false), reportsLoaded(false) {}
    bool persist() const { return !root.empty(); }
    std::string path(const std::string &a, const std::string &b = "", const std::string &c = "") const
    {
        std::string p = PathJoin(root, a);
        if (!b.empty()) p = PathJoin(p, b);
        if (!c.empty()) p = PathJoin(p, c);
        return p;
    }

    /* ---- accounts/<uid>.json ---- */
    Account &acct(uint64_t uid)
    {
        std::map<uint64_t, Account>::iterator it = accounts.find(uid);
        if (it != accounts.end()) return it->second;
        Account &a = accounts[uid];
        Bytes text;
        Json doc;
        if (!persist() || !ReadFile(path("accounts", UidKey(uid) + ".json"), text) || !Json::Parse(text, doc) || !doc.isObject()) return a;
        a.exists = true;
        if (doc.get("account").isObject()) a.account = doc.get("account");
        const Json::Members &w = doc.get("wallet").members();
        for (size_t i = 0; i < w.size(); i++) a.wallet[atoi(w[i].first.c_str())] = w[i].second.asInt();
        const std::vector<Json> &inv = doc.get("inventory").items();
        for (size_t i = 0; i < inv.size(); i++) {
            InvRow r;
            r.itemId = inv[i].get("item_id").asInt();
            r.quantity = inv[i].get("quantity").asInt(1);
            r.expiresAt = inv[i].get("expires_at").asInt();
            r.acquiredAt = inv[i].get("acquired_at").asReal();
            r.source = inv[i].get("source").str();
            r.skuId = inv[i].get("sku_id").asInt();
            a.inv[r.itemId] = r;
        }
        if (doc.get("purchases").isArray()) a.purchases = doc.get("purchases");
        if (doc.get("history").isArray()) a.history = doc.get("history");
        for (size_t i = 0; i < a.purchases.size(); i++)
            a.nextPurchaseId = std::max(a.nextPurchaseId, a.purchases.at(i).get("id").asInt() + 1);
        a.nextPurchaseId = std::max(a.nextPurchaseId, doc.get("next_purchase_id").asInt(1));
        return a;
    }
    /* lobby_db._ensure_account: the row exists from the first write that names the uid. */
    Account &ensure(uint64_t uid)
    {
        Account &a = acct(uid);
        if (!a.exists) {
            a.exists = true;
            a.account["uid"] = uid ? Json((unsigned long long)uid) : Json("");
            if (!uid) a.account["username"] = "(shared)";
        }
        if (!a.account.has("created_at")) a.account["created_at"] = Json((long long)s.Now());
        return a;
    }
    bool save(uint64_t uid)
    {
        if (!persist()) return true;
        Account &a = acct(uid);
        Json doc = Json::MakeObject();
        doc["version"] = 1;
        doc["account"] = a.account;
        Json w = Json::MakeObject();
        for (std::map<int, int64_t>::iterator it = a.wallet.begin(); it != a.wallet.end(); ++it)
            w[Fmt("%d", it->first)] = Json((long long)it->second);
        doc["wallet"] = w;
        Json inv = Json::MakeArray();
        for (std::map<int64_t, InvRow>::iterator it = a.inv.begin(); it != a.inv.end(); ++it) {
            const InvRow &r = it->second;
            Json row = Json::MakeObject();
            row["item_id"] = Json((long long)r.itemId);
            row["quantity"] = Json((long long)r.quantity);
            row["expires_at"] = Json((long long)r.expiresAt);
            row["acquired_at"] = Json(r.acquiredAt);
            row["source"] = r.source;
            row["sku_id"] = Json((long long)r.skuId);
            inv.push(row);
        }
        doc["inventory"] = inv;
        doc["purchases"] = a.purchases;
        doc["next_purchase_id"] = Json((long long)a.nextPurchaseId);
        doc["history"] = a.history;
        return WriteFileAtomic(path("accounts", UidKey(uid) + ".json"), doc.dump(1));
    }

    /* ---- kv.json / reports.json ---- */
    static void loadDoc(const Impl &im, const char *file, const char *member, Json &out)
    {
        Bytes text;
        Json doc;
        out = Json::MakeObject();
        if (im.persist() && ReadFile(im.path(file), text) && Json::Parse(text, doc) && doc.get(member).isObject()) out = doc.get(member);
    }
    bool saveDoc(const char *file, const char *member, const Json &body)
    {
        if (!persist()) return true;
        Json doc = Json::MakeObject();
        doc["version"] = 1;
        doc[member] = body;
        return WriteFileAtomic(path(file), doc.dump(1));
    }
    Json &kvDoc() { if (!kvLoaded) { loadDoc(*this, "kv.json", "values", kv); kvLoaded = true; } return kv; }
    Json &repDoc() { if (!reportsLoaded) { loadDoc(*this, "reports.json", "reports", reports); reportsLoaded = true; } return reports; }
};

Store::Store(Server &server) : impl_(new Impl(server))
{
    if (impl_->persist()) MakeDirs(impl_->root);
}

Store::~Store() { delete impl_; }

/* ---- accounts ---- */

void Store::TouchLogin(uint64_t uid, const std::string &gamertag, const std::string &addr)
{
    Account &a = impl_->ensure(uid);
    a.account["last_login"] = Json((long long)impl_->s.Now());
    a.account["login_count"] = Json((long long)(a.account.get("login_count").asInt() + 1));
    if (!gamertag.empty()) a.account["gamertag"] = gamertag;
    if (!addr.empty()) a.account["last_addr"] = addr;
    impl_->save(uid);
}

Json Store::GetAccount(uint64_t uid)
{
    Account &a = impl_->acct(uid);
    return a.exists ? a.account : Json();
}

/* ---- storage files ---- */

bool Store::GetStorageFile(uint64_t uid, const std::string &name, Bytes &out, bool fallbackShared)
{
    out.clear();
    if (!SafeName(name)) return false;
    for (int pass = 0; pass < 2; pass++) {
        uint64_t u = pass ? 0 : uid;
        if (pass && (!fallbackShared || !uid)) break;
        std::pair<uint64_t, std::string> key(u, name);
        std::map<std::pair<uint64_t, std::string>, FileEntry>::iterator it = impl_->files.find(key);
        if (it == impl_->files.end()) {
            FileEntry e;
            e.present = impl_->persist() && FileExists(impl_->path("storage", UidKey(u), name)) &&
                        ReadFile(impl_->path("storage", UidKey(u), name), e.data);
            if (!e.present) e.data.clear();
            it = impl_->files.insert(std::make_pair(key, e)).first;
        }
        if (it->second.present) { out = it->second.data; return true; }
    }
    return false;
}

bool Store::PutStorageFile(uint64_t uid, const std::string &name, const Bytes &blob, const std::string &source)
{
    (void)source; /* lobby_db keeps size/crc/source columns beside the blob; nothing here reads them */
    if (!SafeName(name)) return false;
    if (impl_->persist() && !WriteFileAtomic(impl_->path("storage", UidKey(uid), name), blob)) return false;
    FileEntry &e = impl_->files[std::make_pair(uid, name)];
    e.present = true;
    e.data = blob;
    return true;
}

bool Store::DeleteStorageFile(uint64_t uid, const std::string &name)
{
    if (!SafeName(name)) return false;
    Bytes old;
    bool had = GetStorageFile(uid, name, old, false);
    if (impl_->persist()) RemoveFile(impl_->path("storage", UidKey(uid), name));
    FileEntry &e = impl_->files[std::make_pair(uid, name)];
    e.present = false;
    e.data.clear();
    return had;
}

/* ---- career profiles ---- */

bool Store::ProfileRead(uint64_t uid, Json &out)
{
    out = Json();
    if (!uid) return false;
    std::map<uint64_t, ProfEntry>::iterator it = impl_->profiles.find(uid);
    if (it == impl_->profiles.end()) {
        ProfEntry e;
        Bytes text;
        e.present = impl_->persist() && ReadFile(impl_->path("players", UidKey(uid) + ".json"), text) &&
                    Json::Parse(text, e.doc) && e.doc.isObject();
        if (!e.present) e.doc = Json();
        it = impl_->profiles.insert(std::make_pair(uid, e)).first;
    }
    if (!it->second.present) return false;
    out = it->second.doc;
    return true;
}

bool Store::ProfileWrite(uint64_t uid, const Json &prof)
{
    if (!uid || !prof.isObject()) return false;
    /* json.dumps(prof, indent=1, sort_keys=True, ensure_ascii=False): the Python mirror's exact form. */
    if (impl_->persist() && !WriteFileAtomic(impl_->path("players", UidKey(uid) + ".json"), prof.dump(1, true))) return false;
    ProfEntry &e = impl_->profiles[uid];
    e.present = true;
    e.doc = prof;
    return true;
}

std::vector<uint64_t> Store::ProfileUids()
{
    std::vector<uint64_t> out;
    if (impl_->persist()) {
        std::vector<std::string> names = ListDir(impl_->path("players"));
        for (size_t i = 0; i < names.size(); i++) {
            const std::string &n = names[i];
            if (n.size() <= 5 || n.size() > 29 || n.compare(n.size() - 5, 5, ".json")) continue;
            std::string stem = n.substr(0, n.size() - 5);
            if (stem.find_first_not_of("0123456789") != std::string::npos) continue;
            uint64_t uid = strtoull(stem.c_str(), 0, 10);
            if (uid) out.push_back(uid);
        }
    }
    for (std::map<uint64_t, ProfEntry>::iterator it = impl_->profiles.begin(); it != impl_->profiles.end(); ++it)
        if (it->second.present) out.push_back(it->first);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

void Store::AddHistory(uint64_t uid, int64_t xp, const std::string &reason, int64_t total, int64_t level)
{
    Account &a = impl_->ensure(uid);
    Json row = Json::MakeObject();
    row["ts"] = Json((long long)impl_->s.Now());
    row["xp"] = Json((long long)xp);
    row["reason"] = reason;
    row["total"] = Json((long long)total);
    row["level"] = Json((long long)level);
    std::vector<Json> &h = a.history.items();
    h.push_back(row);
    if (h.size() > kHistoryKeep) h.erase(h.begin(), h.begin() + (long)(h.size() - kHistoryKeep));
    impl_->save(uid);
}

/* ---- wallet ---- */

bool Store::WalletHas(uint64_t uid, int currency)
{
    Account &a = impl_->acct(uid);
    return a.wallet.find(currency) != a.wallet.end();
}

int64_t Store::WalletGet(uint64_t uid, int currency, int64_t dflt)
{
    Account &a = impl_->acct(uid);
    std::map<int, int64_t>::iterator it = a.wallet.find(currency);
    return it == a.wallet.end() ? dflt : it->second;
}

std::map<int, int64_t> Store::WalletBalances(uint64_t uid) { return impl_->acct(uid).wallet; }

/* lobby_db._wallet_row: a missing row is created holding `seed`. */
static int64_t &WalletRow(Account &a, int currency, int64_t seed)
{
    std::map<int, int64_t>::iterator it = a.wallet.find(currency);
    if (it == a.wallet.end()) it = a.wallet.insert(std::make_pair(currency, seed)).first;
    return it->second;
}

int64_t Store::WalletCredit(uint64_t uid, int currency, int64_t amount, int64_t seed)
{
    Account &a = impl_->ensure(uid);
    int64_t &bal = WalletRow(a, currency, seed);
    bal = WalletClamp(bal + amount);
    int64_t out = bal;
    impl_->save(uid);
    return out;
}

bool Store::WalletDebit(uint64_t uid, int currency, int64_t amount, int64_t seed, bool allowNegative, int64_t *after)
{
    Account &a = impl_->ensure(uid);
    int64_t &bal = WalletRow(a, currency, seed);
    bool ok = !(amount > bal && !allowNegative);
    if (ok) bal -= amount; /* not clamped, as lobby_db: allow_negative may take it below zero */
    if (after) *after = bal;
    impl_->save(uid); /* the seed row is written even when the debit is refused */
    return ok;
}

void Store::WalletSet(uint64_t uid, int currency, int64_t amount)
{
    Account &a = impl_->ensure(uid);
    a.wallet[currency] = WalletClamp(amount);
    impl_->save(uid);
}

/* ---- inventory ---- */

bool Store::InventoryExists(uint64_t uid) { return !impl_->acct(uid).inv.empty(); }

std::vector<InvRow> Store::InventoryList(uint64_t uid, bool includeShared, int64_t now)
{
    std::map<int64_t, InvRow> all;
    if (includeShared && uid) all = impl_->acct(0).inv; /* a per-uid row wins over a shared one */
    const std::map<int64_t, InvRow> &own = impl_->acct(uid).inv;
    for (std::map<int64_t, InvRow>::const_iterator it = own.begin(); it != own.end(); ++it) all[it->first] = it->second;
    std::vector<InvRow> out;
    for (std::map<int64_t, InvRow>::iterator it = all.begin(); it != all.end(); ++it)
        if (!now || !it->second.expiresAt || it->second.expiresAt > now) out.push_back(it->second);
    return out;
}

bool Store::InventoryGet(uint64_t uid, int64_t itemId, InvRow &out)
{
    Account &a = impl_->acct(uid);
    std::map<int64_t, InvRow>::iterator it = a.inv.find(itemId);
    if (it == a.inv.end()) return false;
    out = it->second;
    return true;
}

void Store::InventoryGrant(uint64_t uid, int64_t itemId, int64_t quantity, int64_t expiresAt, const std::string &source,
                           int64_t skuId, bool stack)
{
    Account &a = impl_->ensure(uid);
    std::map<int64_t, InvRow>::iterator it = a.inv.find(itemId);
    InvRow r;
    r.itemId = itemId;
    r.quantity = quantity;
    r.expiresAt = expiresAt;
    if (it != a.inv.end()) {
        if (stack) { /* consumables: add to the stack, keep the later expiry */
            r.quantity = it->second.quantity + quantity;
            r.expiresAt = std::max(it->second.expiresAt, expiresAt);
        } else {     /* "own it, refresh the rental expiry" */
            r.quantity = std::max(it->second.quantity, quantity);
        }
    }
    r.acquiredAt = (double)impl_->s.Now();
    r.source = source;
    r.skuId = skuId;
    a.inv[itemId] = r;
    impl_->save(uid);
}

int64_t Store::InventoryConsume(uint64_t uid, int64_t itemId, int64_t quantity, bool removeAtZero)
{
    Account &a = impl_->acct(uid);
    std::map<int64_t, InvRow>::iterator it = a.inv.find(itemId);
    if (it == a.inv.end() || it->second.quantity < quantity) return -1; /* nothing consumed */
    int64_t left = it->second.quantity - quantity;
    if (left <= 0 && removeAtZero) a.inv.erase(it);
    else it->second.quantity = left;
    impl_->save(uid);
    return left;
}

void Store::InventoryReplace(uint64_t uid, const std::vector<InvRow> &rows, const std::string &source)
{
    Account &a = impl_->ensure(uid);
    std::map<int64_t, InvRow> next;
    for (size_t i = 0; i < rows.size(); i++) {
        InvRow r = rows[i];
        std::map<int64_t, InvRow>::iterator old = a.inv.find(r.itemId);
        /* lobby_db.inventory_replace: a row that stays keeps its source, a new one takes `source` */
        r.source = old != a.inv.end() ? old->second.source : source;
        if (r.acquiredAt == 0) r.acquiredAt = (double)impl_->s.Now();
        next[r.itemId] = r;
    }
    a.inv.swap(next);
    impl_->save(uid);
}

bool Store::InventoryRevoke(uint64_t uid, int64_t itemId)
{
    Account &a = impl_->acct(uid);
    if (!a.inv.erase(itemId)) return false;
    impl_->save(uid);
    return true;
}

/* ---- purchases ledger ---- */

void Store::RecordPurchase(uint64_t uid, const Json &row)
{
    Account &a = impl_->ensure(uid);
    Json r = row.isObject() ? row : Json::MakeObject();
    r["id"] = Json((long long)a.nextPurchaseId++);
    if (!r.has("ts")) r["ts"] = Json((long long)impl_->s.Now());
    std::vector<Json> &p = a.purchases.items();
    p.push_back(r);
    if (p.size() > kPurchasesKeep) p.erase(p.begin(), p.begin() + (long)(p.size() - kPurchasesKeep));
    impl_->save(uid);
}

Json Store::Purchases(uint64_t uid)
{
    const std::vector<Json> &p = impl_->acct(uid).purchases.items();
    Json out = Json::MakeArray(); /* newest first, at most 200 (lobby_db.purchases) */
    for (size_t i = p.size(); i > 0 && out.size() < 200; i--) out.push(p[i - 1]);
    return out;
}

/* ---- match reports ---- */

bool Store::HasReport(const std::string &reportId) { return impl_->repDoc().has(reportId); }

void Store::RecordMatchReport(const std::string &reportId, const Json &report, bool applied)
{
    Json &reps = impl_->repDoc();
    if (reps.has(reportId)) return; /* idempotent on the id */
    int64_t now = impl_->s.Now();
    Json row = Json::MakeObject();
    row["created"] = report.get("created").truthy() ? report.get("created") : Json((long long)now);
    row["received_at"] = Json((long long)now);
    row["reason"] = report.get("reason").str();
    row["duration_s"] = Json(report.get("duration_s").asReal());
    row["applied"] = applied ? 1 : 0;
    row["applied_at"] = Json((long long)(applied ? now : 0));
    row["raw"] = report;
    reps[reportId] = row;
    Json::Members &m = reps.members();
    for (size_t i = 0; i + kReportRawKeep < m.size(); i++) m[i].second.erase("raw");
    impl_->saveDoc("reports.json", "reports", reps);
}

void Store::MarkReportApplied(const std::string &reportId, const Json &credited)
{
    Json &reps = impl_->repDoc();
    if (!reps.has(reportId)) return;
    Json &row = reps[reportId];
    row["applied"] = 1;
    row["applied_at"] = Json((long long)impl_->s.Now());
    if (credited.isObject()) row["credited"] = credited; /* {slot: xp credited} */
    impl_->saveDoc("reports.json", "reports", reps);
}

/* ---- key/value ---- */

Json Store::KvGet(const std::string &key) { return impl_->kvDoc().get(key); }

void Store::KvSet(const std::string &key, const Json &value)
{
    Json &kv = impl_->kvDoc();
    kv[key] = value;
    impl_->saveDoc("kv.json", "values", kv);
}

/* ---- lifecycle ---- */

void StoreInit(Server &s)
{
    Store *st = new Store(s);
    s.st->store = st;
    /* 1-START-LOBBY.cmd deletes resources\mpdata and resources\Recentmetlist on every boot: the
     * unkeyed copies are transient, a per-player one is never touched. */
    st->DeleteStorageFile(0, "mpdata");
    st->DeleteStorageFile(0, "Recentmetlist");
}

void StoreFree(Server &s)
{
    delete s.st->store;
    s.st->store = NULL;
}

Store &GetStore(Server &s)
{
    if (!s.st->store) s.st->store = new Store(s);
    return *s.st->store;
}

} // namespace dw

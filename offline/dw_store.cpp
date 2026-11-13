/*
 * dw_store.cpp - the store (dw_store.h): one folder per player under Config::profile holding the
 * game's own mpdata / mpdata_s blobs and a small account.txt; everything else in memory. Wallet and
 * inventory semantics are store/lobby_db.py's; there is no database, no writer thread and no lock -
 * the server is single-threaded, so every read-modify-write below is already atomic.
 *
 * With an empty Config::profile nothing is written: the store then lives in memory only.
 */
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <set>
#include "dw.h"
#include "dw_state.h"
#include "dw_store.h"

namespace dw {

namespace {

const int64_t kWalletMax = 0xFFFFFFFFLL; /* the client's balance field is a u32 (lobby_db.WALLET_MAX) */
const size_t kReportsKeep = 500;         /* match_report.state.json is rewritten whole, so it has to forget */
const char *const kAccountFile = "account.txt";
const char *const kStateFile = "match_report.state.json";

int64_t WalletClamp(int64_t v) { return v < 0 ? 0 : v > kWalletMax ? kWalletMax : v; }

std::string Lower(const std::string &s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); i++)
        if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] + 32);
    return out;
}

std::string Trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}

bool AllDigits(const std::string &s) { return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos; }

/* A storage name is a key and, for the two profile blobs, a file name: refuse anything path-like. */
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

/* A profile name that can be a folder: letters, digits, _ - . ; no leading or trailing dot, none of
 * the device names Windows reserves. */
bool SafeFolder(const std::string &name)
{
    if (name.empty() || name.size() > 31 || name[0] == '.' || name[name.size() - 1] == '.') return false;
    for (size_t i = 0; i < name.size(); i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    std::string stem = Lower(name.substr(0, name.find('.')));
    static const char *const reserved[] = { "con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8",
                                            "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };
    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
        if (stem == reserved[i]) return false;
    return true;
}

/* The two blobs that ARE the player's files. */
bool PlayerFile(const std::string &name) { return name == "mpdata" || name == "mpdata_s"; }
/* The one shared (unkeyed) copy that is a file: "mpdata_s" beside the match reports, where the
 * Python lobby keeps resources\mpdata_s - the rank blob of whoever played last, and the target of a
 * match report that names no account. */
bool SharedFile(const std::string &name) { return name == "mpdata_s"; }

struct Account {
    bool loaded;                 /* account.txt was looked for */
    bool exists;                 /* has a row of its own (lobby_db._ensure_account ran, or the file exists) */
    std::map<int, int64_t> wallet;
    std::map<int64_t, InvRow> inv;
    Json career;                 /* {member: value}: what mpdata_s cannot hold */
    std::string written;         /* the account.txt text last read or written */
    Account() : loaded(false), exists(false), career(Json::MakeObject()) {}
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
    bool indexed;
    std::map<uint64_t, std::string> folders; /* uid -> folder name, from the account.txt files and the logins of this run */
    std::set<std::string> taken;             /* lower-cased folder names that belong to a uid */
    bool stateLoaded;
    Json kv, reports; /* {key: value} / {reportId: {...}} */

    Impl(Server &server) : s(server), root(server.cfg.profile), indexed(false), stateLoaded(false), kv(Json::MakeObject()), reports(Json::MakeObject()) {}
    bool persist() const { return !root.empty(); }

    /* ---- folders ---- */
    static bool AccountUid(const std::string &text, uint64_t &uid)
    {
        size_t pos = 0;
        while (pos < text.size()) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string line = Trim(text.substr(pos, end - pos));
            pos = end + 1;
            size_t eq = line.find('=');
            if (line.empty() || line[0] == '#' || eq == std::string::npos || Trim(line.substr(0, eq)) != "uid") continue;
            std::string v = Trim(line.substr(eq + 1));
            if (!AllDigits(v) || v.size() > 20) return false;
            uid = strtoull(v.c_str(), NULL, 10);
            return uid != 0;
        }
        return false;
    }
    void index()
    {
        if (indexed) return;
        indexed = true;
        if (!persist()) return;
        std::vector<std::string> names = ListDir(root);
        std::sort(names.begin(), names.end());
        for (size_t i = 0; i < names.size(); i++) {
            Bytes text;
            uint64_t uid = 0;
            if (!SafeFolder(names[i]) || !ReadFile(PathJoin(PathJoin(root, names[i]), kAccountFile), text) || !AccountUid(text, uid)) continue;
            if (folders.count(uid)) continue; /* two folders claim one uid: the first (by name) is the player's */
            folders[uid] = names[i];
            taken.insert(Lower(names[i]));
        }
    }
    std::string folder(uint64_t uid)
    {
        index();
        std::map<uint64_t, std::string>::iterator it = folders.find(uid);
        return it != folders.end() ? it->second : Fmt("%llu", (unsigned long long)uid);
    }
    std::string path(uint64_t uid, const std::string &file) { return PathJoin(PathJoin(root, folder(uid)), file); }
    /* Where a storage file lives on disk, "" when it is kept in memory only. */
    std::string filePath(uint64_t uid, const std::string &name)
    {
        if (uid) return persist() && PlayerFile(name) ? path(uid, name) : std::string();
        return !s.cfg.matchProfile.empty() && SharedFile(name) ? PathJoin(s.cfg.matchProfile, name) : std::string();
    }
    /* What this run has in memory about `uid` was read from another folder: forget it. */
    void forget(uint64_t uid)
    {
        accounts.erase(uid);
        profiles.erase(uid);
        for (std::map<std::pair<uint64_t, std::string>, FileEntry>::iterator it = files.begin(); it != files.end();) {
            if (it->first.first == uid && PlayerFile(it->first.second)) files.erase(it++);
            else ++it;
        }
    }
    bool bind(uint64_t uid, const std::string &name)
    {
        index();
        if (!uid || folders.count(uid) || !SafeFolder(name) || taken.count(Lower(name))) return false;
        if (AllDigits(name) && name != Fmt("%llu", (unsigned long long)uid)) return false; /* a number names that uid's folder */
        if (persist() && FileExists(PathJoin(root, name))) return false;                   /* a file of the game's own */
        forget(uid);
        folders[uid] = name;
        taken.insert(Lower(name));
        return true;
    }

    /* ---- account.txt ---- */
    Account &acct(uint64_t uid)
    {
        Account &a = accounts[uid];
        if (a.loaded) return a;
        a.loaded = true;
        Bytes text;
        if (!uid || !persist() || !ReadFile(path(uid, kAccountFile), text)) return a;
        a.exists = true;
        a.written = text;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string line = Trim(text.substr(pos, end - pos));
            pos = end + 1;
            size_t eq = line.find('=');
            if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
            std::string key = Trim(line.substr(0, eq)), val = Trim(line.substr(eq + 1));
            if (!key.compare(0, 7, "wallet.") && AllDigits(key.substr(7)) && key.size() < 17) {
                a.wallet[atoi(key.c_str() + 7)] = strtoll(val.c_str(), NULL, 10);
            } else if (key == "item") { /* itemId quantity expiresAt skuId acquiredAt source */
                InvRow r;
                long long f[5] = { 0, 1, 0, 0, 0 };
                int used = 0;
                if (sscanf(val.c_str(), "%lld %lld %lld %lld %lld %n", &f[0], &f[1], &f[2], &f[3], &f[4], &used) < 5) continue;
                r.itemId = f[0]; r.quantity = f[1]; r.expiresAt = f[2]; r.skuId = f[3]; r.acquiredAt = (double)f[4];
                r.source = used > 0 && (size_t)used <= val.size() ? Trim(val.substr((size_t)used)) : "";
                a.inv[r.itemId] = r;
            } else if (!key.compare(0, 7, "career.") && key.size() > 7) {
                Json v;
                if (Json::Parse(val, v)) a.career[key.substr(7)] = v;
            }
        }
        return a;
    }
    /* lobby_db._ensure_account: the row exists from the first write that names the uid. */
    Account &ensure(uint64_t uid)
    {
        Account &a = acct(uid);
        a.exists = true;
        return a;
    }
    bool save(uint64_t uid)
    {
        if (!uid || !persist()) return true;
        Account &a = acct(uid);
        std::string text = "# Kernel33 offline profile (docs/OFFLINE_LOBBY.md, \"Store\"). The career - rank, XP, lifetime\n"
                           "# stats, challenge progress - is the game's own profile blob mpdata_s beside this file, and\n"
                           "# mpdata holds the classes and characters. This file is only what those blobs have no field\n"
                           "# for. wallet.<n> = balance of marketplace currency n (1 is GP); item = itemId quantity\n"
                           "# expiresAt skuId acquiredAt source; career.<member> = a career value mpdata_s cannot hold.\n";
        text += "version = 1\n";
        text += Fmt("uid = %llu\n", (unsigned long long)uid);
        for (std::map<int, int64_t>::iterator it = a.wallet.begin(); it != a.wallet.end(); ++it)
            text += Fmt("wallet.%d = %lld\n", it->first, (long long)it->second);
        for (std::map<int64_t, InvRow>::iterator it = a.inv.begin(); it != a.inv.end(); ++it) {
            const InvRow &r = it->second;
            std::string src = r.source;
            for (size_t i = 0; i < src.size(); i++)
                if ((unsigned char)src[i] < 0x20) src[i] = ' ';
            text += Fmt("item = %lld %lld %lld %lld %lld %s\n", (long long)r.itemId, (long long)r.quantity, (long long)r.expiresAt,
                        (long long)r.skuId, (long long)r.acquiredAt, src.c_str());
        }
        const Json::Members &cm = a.career.members();
        for (size_t i = 0; i < cm.size(); i++) text += "career." + cm[i].first + " = " + cm[i].second.dump() + "\n";
        if (text == a.written) return true;
        if (!WriteFileAtomic(path(uid, kAccountFile), text)) return false;
        a.written = text;
        return true;
    }

    /* ---- <matchProfile>/match_report.state.json ---- */
    std::string statePath() const { return s.cfg.matchProfile.empty() ? std::string() : PathJoin(s.cfg.matchProfile, kStateFile); }
    void loadState()
    {
        if (stateLoaded) return;
        stateLoaded = true;
        Bytes text;
        Json doc;
        std::string p = statePath();
        if (p.empty() || !ReadFile(p, text) || !Json::Parse(text, doc)) return;
        if (doc.get("values").isObject()) kv = doc.get("values");
        if (doc.get("reports").isObject()) reports = doc.get("reports");
    }
    bool saveState()
    {
        std::string p = statePath();
        if (p.empty()) return true;
        Json doc = Json::MakeObject();
        doc["version"] = 1;
        doc["values"] = kv;
        doc["reports"] = reports;
        return WriteFileAtomic(p, doc.dump(1));
    }
};

Store::Store(Server &server) : impl_(new Impl(server))
{
    if (impl_->persist()) MakeDirs(impl_->root);
}

Store::~Store() { delete impl_; }

/* ---- the player's folder ---- */

void Store::TouchLogin(uint64_t uid, const std::string &gamertag, const std::string &addr)
{
    (void)gamertag; (void)addr; /* lobby_db keeps a login record; nothing reads one here */
    if (!uid) return;
    impl_->index();
    if (!impl_->folders.count(uid)) {
        /* A folder named by the uid (an earlier run without a name) stays the player's folder. */
        std::string own = Fmt("%llu", (unsigned long long)uid);
        std::string dir = PathJoin(impl_->root, own);
        bool hasOwn = impl_->persist() && (FileExists(PathJoin(dir, "mpdata_s")) || FileExists(PathJoin(dir, "mpdata")) ||
                                           FileExists(PathJoin(dir, kAccountFile)));
        if (hasOwn || !impl_->bind(uid, impl_->s.cfg.localName)) impl_->bind(uid, own);
    }
    impl_->ensure(uid);
    impl_->save(uid);
}

bool Store::BindFolder(uint64_t uid, const std::string &name) { return impl_->bind(uid, name); }

std::string Store::FolderOf(uint64_t uid) { return impl_->folder(uid); }

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
            const std::string file = impl_->filePath(u, name);
            e.present = !file.empty() && FileExists(file) && ReadFile(file, e.data);
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
    const std::string file = impl_->filePath(uid, name);
    if (!file.empty()) {
        FileEntry *have = NULL;
        std::map<std::pair<uint64_t, std::string>, FileEntry>::iterator it = impl_->files.find(std::make_pair(uid, name));
        if (it != impl_->files.end() && it->second.present) have = &it->second;
        /* identical bytes are not written again */
        if (!(have && have->data == blob) && !WriteFileAtomic(file, blob)) return false;
    }
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
    const std::string file = impl_->filePath(uid, name);
    if (!file.empty()) RemoveFile(file);
    FileEntry &e = impl_->files[std::make_pair(uid, name)];
    e.present = false;
    e.data.clear();
    return had;
}

void Store::QuarantineStorageFile(uint64_t uid, const std::string &name)
{
    if (!SafeName(name)) return;
    if (uid && !impl_->filePath(uid, name).empty()) {
        std::string from = impl_->filePath(uid, name), to = from + ".unreadable";
        RemoveFile(to);
        if (rename(from.c_str(), to.c_str()) != 0) RemoveFile(from);
    }
    FileEntry &e = impl_->files[std::make_pair(uid, name)];
    e.present = false;
    e.data.clear();
}

/* ---- career profiles ---- */

bool Store::ProfileRead(uint64_t uid, Json &out)
{
    out = Json();
    if (!uid) return false;
    std::map<uint64_t, ProfEntry>::iterator it = impl_->profiles.find(uid);
    if (it == impl_->profiles.end() || !it->second.present) return false;
    out = it->second.doc;
    return true;
}

bool Store::ProfileWrite(uint64_t uid, const Json &prof, const Json &extras)
{
    if (!uid || !prof.isObject()) return false;
    Account &a = impl_->ensure(uid);
    a.career = extras.isObject() ? extras : Json::MakeObject();
    if (!impl_->save(uid)) return false;
    ProfEntry &e = impl_->profiles[uid];
    e.present = true;
    e.doc = prof;
    return true;
}

Json Store::ProfileExtras(uint64_t uid) { return impl_->acct(uid).career; }

std::vector<uint64_t> Store::ProfileUids()
{
    std::vector<uint64_t> out;
    if (impl_->persist()) {
        impl_->index();
        std::map<std::string, uint64_t> byFolder;
        for (std::map<uint64_t, std::string>::iterator it = impl_->folders.begin(); it != impl_->folders.end(); ++it)
            byFolder[Lower(it->second)] = it->first;
        std::vector<std::string> names = ListDir(impl_->root);
        for (size_t i = 0; i < names.size(); i++) {
            const std::string &n = names[i];
            if (!SafeFolder(n) || !FileExists(PathJoin(PathJoin(impl_->root, n), "mpdata_s"))) continue;
            std::map<std::string, uint64_t>::iterator b = byFolder.find(Lower(n));
            uint64_t uid = b != byFolder.end() ? b->second : (AllDigits(n) && n.size() <= 20 ? strtoull(n.c_str(), 0, 10) : 0);
            if (uid && impl_->folder(uid) == n) out.push_back(uid);
        }
    }
    for (std::map<uint64_t, ProfEntry>::iterator it = impl_->profiles.begin(); it != impl_->profiles.end(); ++it)
        if (it->second.present) out.push_back(it->first);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
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

/* ---- match reports ----
 * The ledger keeps the ids and what was credited; the report itself is the archived
 * match_report.applied.<time>.json beside this file. */

bool Store::HasReport(const std::string &reportId)
{
    impl_->loadState();
    return impl_->reports.has(reportId);
}

void Store::RecordMatchReport(const std::string &reportId, const Json &report, bool applied)
{
    impl_->loadState();
    Json &reps = impl_->reports;
    if (reps.has(reportId)) return; /* idempotent on the id */
    int64_t now = impl_->s.Now();
    Json row = Json::MakeObject();
    row["created"] = report.get("created").truthy() ? report.get("created") : Json((long long)now);
    row["received_at"] = Json((long long)now);
    row["reason"] = report.get("reason").str();
    row["applied"] = applied ? 1 : 0;
    row["applied_at"] = Json((long long)(applied ? now : 0));
    reps[reportId] = row;
    Json::Members &m = reps.members();
    if (m.size() > kReportsKeep) m.erase(m.begin(), m.begin() + (long)(m.size() - kReportsKeep));
    impl_->saveState();
}

void Store::MarkReportApplied(const std::string &reportId, const Json &credited)
{
    impl_->loadState();
    Json &reps = impl_->reports;
    if (!reps.has(reportId)) return;
    Json &row = reps[reportId];
    row["applied"] = 1;
    row["applied_at"] = Json((long long)impl_->s.Now());
    if (credited.isObject()) row["credited"] = credited; /* {slot: xp credited} */
    impl_->saveState();
}

/* ---- key/value ---- */

Json Store::KvGet(const std::string &key)
{
    impl_->loadState();
    return impl_->kv.get(key);
}

void Store::KvSet(const std::string &key, const Json &value)
{
    impl_->loadState();
    impl_->kv[key] = value;
    impl_->saveState();
}

/* ---- lifecycle ---- */

void StoreInit(Server &s)
{
    s.st->store = new Store(s);
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

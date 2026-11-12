/*
 * dw_profiles.cpp - per-player progression: players/<uid>.json and its render into "mpdata_s".
 * Port of store/player_profiles.py, the rank arithmetic and RankState of career/rank_service.py and
 * the blob side of career/progression.py (blob_updates, collectibles).
 *
 * Where the Python lobby keeps three copies of a rendered blob (resources\mpdata_s.<uid>, the SQLite
 * storage_files row and the shared resources\mpdata_s), the store holds two: the per-player
 * "mpdata_s" storage file and the shared one (uid 0), which is what a connection without a latched
 * uid is served.
 */
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "dw.h"
#include "dw_playerdata.h"
#include "dw_profiles.h"
#include "dw_state.h"
#include "dw_store.h"

namespace dw {

namespace {

const char *const kBlobName = "mpdata_s";
const char *const kSeedBlob = "mpdata_s.level1.proposed"; /* paths.MPDATA_S_SEED: rank 1 / 0 XP */
const char *const kNewAccount = "level 1 (new account)";
const int kFormat = 1;
const int64_t kI32Max = 0x7FFFFFFF;
const int kChallengeCount = 1000; /* challenges[1000] of float in mp/playersecuredata.def */

struct RankRow { int64_t minXP, xpToNext; };

} // namespace

struct ProfilesState {
    bool rankTried;
    std::vector<RankRow> ranks;
    int rankVersion;
    bool challengesTried;
    std::vector<int> challengeIds;   /* every id of mp/allchallengestable.csv */
    std::vector<char> isChallenge;   /* [kChallengeCount] */
    uint64_t mirrorOwner;
    ProfilesState() : rankTried(false), rankVersion(2), challengesTried(false), mirrorOwner(0) {}
};

void ProfilesInit(Server &s) { s.st->profiles = new ProfilesState(); }

void ProfilesFree(Server &s)
{
    delete s.st->profiles;
    s.st->profiles = NULL;
    PlayerDataFree(s);
}

static ProfilesState &State(Server &s)
{
    if (!s.st->profiles) s.st->profiles = new ProfilesState();
    return *s.st->profiles;
}

void SetMirrorOwner(Server &s, uint64_t uid) { State(s).mirrorOwner = uid; }
uint64_t MirrorOwner(Server &s) { return State(s).mirrorOwner; }

/* ---- the rank table (rank_service.rank_table / xp_to_rank / rank_to_xp) ---- */

static const std::vector<RankRow> &Ranks(Server &s)
{
    ProfilesState &st = State(s);
    if (st.rankTried) return st.ranks;
    st.rankTried = true;
    Bytes text;
    Json doc;
    if (s.Resource("unlock_tables.proposed.json", text) && Json::Parse(text, doc)) {
        const std::vector<Json> &rows = doc.get("rankTable").items();
        for (size_t i = 0; i < rows.size(); i++) {
            RankRow r;
            r.minXP = rows[i].get("minXP").asInt();
            r.xpToNext = rows[i].get("xpToNext").asInt();
            st.ranks.push_back(r);
        }
        st.rankVersion = (int)doc.get("rankTable_version").asInt(2);
    }
    if (st.ranks.empty()) s.Log("profiles: unlock_tables.proposed.json carries no rankTable -- rank cannot be rendered");
    return st.ranks;
}

int RankCount(Server &s) { return (int)Ranks(s).size(); }

bool XpToRank(Server &s, int64_t totalXp, int &rankIndex, int64_t &xpInRank)
{
    const std::vector<RankRow> &t = Ranks(s);
    rankIndex = 0;
    xpInRank = 0;
    if (t.empty()) return false;
    int64_t total = std::max<int64_t>(0, totalXp);
    /* clamp so the client's "XP to next" cannot go negative at maxrank */
    int64_t ceiling = t.back().minXP + t.back().xpToNext - 1;
    if (total > ceiling) total = ceiling;
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i].minXP <= total) rankIndex = (int)i;
        else break;
    }
    xpInRank = total - t[(size_t)rankIndex].minXP;
    return true;
}

int64_t RankToXp(Server &s, int64_t storedRank, int64_t experience)
{
    const std::vector<RankRow> &t = Ranks(s);
    if (t.empty()) return std::max<int64_t>(0, experience);
    int64_t idx = std::max<int64_t>(0, std::min<int64_t>((int64_t)t.size() - 1, storedRank - 1));
    return t[(size_t)idx].minXP + std::max<int64_t>(0, experience);
}

int64_t RankXpToNext(Server &s, int rankIndex)
{
    const std::vector<RankRow> &t = Ranks(s);
    return rankIndex >= 0 && (size_t)rankIndex < t.size() ? t[(size_t)rankIndex].xpToNext : 0;
}

int LevelForXp(Server &s, int64_t totalXp)
{
    int idx;
    int64_t in;
    XpToRank(s, totalXp, idx, in);
    return idx + 1;
}

/* RANK-UP GP: every rank-up pays a fixed 200 GP x the new displayed level, for every level crossed
 * (level 2 pays 400; 4 -> 6 pays 200*5 + 200*6). */
int64_t RankupGp(int64_t fromLevel, int64_t toLevel)
{
    int64_t lo = std::max<int64_t>(1, fromLevel), hi = toLevel;
    if (hi <= lo) return 0;
    int64_t sum = 0;
    for (int64_t l = lo + 1; l <= hi; l++) sum += l;
    return 200 * sum;
}

bool XpFromBlob(Server &s, const Bytes &stored, int64_t &totalXp)
{
    PdBlob blob;
    if (Ranks(s).empty() || !blob.Load(s, stored) || blob.def->key != kBlobName) return false;
    const PdField *rank = blob.def->Find("rank"), *xp = blob.def->Find("experience");
    if (!rank || !xp) return false;
    totalXp = RankToXp(s, blob.GetInt(*rank), blob.GetInt(*xp));
    return true;
}

/* RankState.save's edit: the rank triple for this lifetime XP. */
static bool SetRankFields(Server &s, PdBlob &blob, int64_t totalXp)
{
    int idx;
    int64_t in;
    if (!XpToRank(s, totalXp, idx, in)) return false;
    return blob.SetPath("rank", idx + 1) &&       /* the field is ONE-BASED */
           blob.SetPath("experience", (double)in) && /* XP *within* the rank */
           blob.SetPath("rankVersion", State(s).rankVersion);
}

/* ---- python-flavoured conversions on Json ---- */

/* int(v): -> false where Python raises (None, a non-numeric string, a container). */
static bool PyInt(const Json &v, int64_t &out)
{
    if (v.isBool() || v.isInt()) { out = v.asInt(); return true; }
    if (v.isNumber()) { double d = v.asReal(); if (d != d || d > 9e18 || d < -9e18) return false; out = (int64_t)d; return true; }
    if (v.isString()) {
        const char *p = v.asString().c_str();
        char *end = 0;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) return false;
        long long n = strtoll(p, &end, 10);
        if (end == p) return false;
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
        if (*end) return false;
        out = n;
        return true;
    }
    return false;
}

/* float(v) */
static bool PyFloat(const Json &v, double &out)
{
    if (v.isBool() || v.isNumber()) { out = v.asReal(); return true; }
    if (v.isString()) {
        const char *p = v.asString().c_str();
        char *end = 0;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) return false;
        double d = strtod(p, &end);
        if (end == p) return false;
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
        if (*end) return false;
        out = d;
        return true;
    }
    return false;
}

static int64_t IntOr(const Json &v, int64_t dflt)
{
    int64_t n;
    return PyInt(v, n) ? n : dflt;
}

/* ---- the store (load / save / blank / seed) ---- */

/* prof['lifetime']: a dict of str -> non-negative int; an unfamiliar member name is KEPT (the key
 * set is open on purpose), only unusable values are dropped. */
static void FixLifetime(Json &prof)
{
    Json clean = Json::MakeObject();
    const Json &lt = prof.get("lifetime");
    if (lt.isObject()) {
        const Json::Members &m = lt.members();
        for (size_t i = 0; i < m.size(); i++) {
            int64_t n;
            if (PyInt(m[i].second, n)) clean[m[i].first] = Json((long long)std::max<int64_t>(0, n));
        }
    }
    prof["lifetime"] = clean;
}

Json ProfileBlank(Server &s, uint64_t uid)
{
    double now = (double)s.Now();
    Json p = Json::MakeObject();
    p["format"] = kFormat;
    p["uid"] = Fmt("%llu", (unsigned long long)uid);
    p["name"] = "";
    p["total_xp"] = 0;
    p["level"] = 1;
    p["kills"] = 0;
    p["deaths"] = 0;
    p["assists"] = 0;
    p["matches"] = 0;
    p["lifetime"] = Json::MakeObject();    /* {DDL member name: running total}, open-keyed */
    p["challenges"] = Json::MakeObject();  /* {"<numeric challenge id>": progress} */
    p["weapons"] = Json::MakeObject();     /* {"<weapon name>": the level-driving counter} */
    p["weapon_uses"] = Json::MakeObject(); /* {"<weapon name>": count} */
    p["created"] = Json(now);
    p["updated"] = Json(now);
    p["seeded_from"] = "";
    p["history"] = Json::MakeArray();
    return p;
}

bool ProfileSave(Server &s, Json &prof)
{
    if (!prof.isObject()) return false;
    const std::string key = prof.get("uid").isString() ? prof.get("uid").asString() : Fmt("%llu", (unsigned long long)prof.get("uid").asUInt());
    if (key.empty() || key.size() > 24 || key.find_first_not_of("0123456789") != std::string::npos) return false;
    uint64_t uid = strtoull(key.c_str(), 0, 10);
    if (!uid) return false;
    prof["format"] = kFormat;
    int64_t xp = std::max<int64_t>(0, IntOr(prof.get("total_xp"), 0));
    prof["total_xp"] = Json((long long)xp);
    prof["level"] = LevelForXp(s, xp);
    prof["updated"] = Json((double)s.Now());
    FixLifetime(prof);
    if (prof.get("history").isArray() && prof.get("history").size() > 50) {
        std::vector<Json> &h = prof["history"].items();
        h.erase(h.begin(), h.begin() + (long)(h.size() - 50));
    }
    return GetStore(s).ProfileWrite(uid, prof);
}

bool ProfileLoad(Server &s, uint64_t uid, Json &prof, bool create)
{
    prof = Json();
    if (!uid) return false;
    Store &store = GetStore(s);
    Json d;
    if (store.ProfileRead(uid, d) && IntOr(d.get("format"), 0) == kFormat) {
        d["uid"] = Fmt("%llu", (unsigned long long)uid);
        int64_t xp = std::max<int64_t>(0, IntOr(d.get("total_xp"), 0));
        d["total_xp"] = Json((long long)xp);
        d["level"] = LevelForXp(s, xp);
        if (!d.get("history").isArray()) d["history"] = Json::MakeArray();
        FixLifetime(d);
        prof = d;
        return true;
    }
    if (!create) return false;
    /* A NEW ACCOUNT STARTS AT LEVEL 1. The one exception is a per-uid mpdata_s already in the store
     * (an operator's explicit choice for this uid), which is honoured. The shared blob and the
     * shipped seed are deliberately not read: the shared one describes whoever played last. */
    prof = ProfileBlank(s, uid);
    int64_t xp = 0;
    std::string how = kNewAccount;
    Bytes own;
    if (store.GetStorageFile(uid, kBlobName, own) && XpFromBlob(s, own, xp)) how = Fmt("%s.%llu", kBlobName, (unsigned long long)uid);
    else xp = 0;
    prof["total_xp"] = Json((long long)std::max<int64_t>(0, xp));
    prof["level"] = LevelForXp(s, xp);
    prof["seeded_from"] = how;
    ProfileSave(s, prof);
    return true;
}

std::vector<uint64_t> ProfileListUids(Server &s) { return GetStore(s).ProfileUids(); }

int PlayerLevel(Server &s, uint64_t uid)
{
    Json prof;
    if (!ProfileLoad(s, uid, prof, false)) return 0; /* 0 satisfies no rank flag */
    return (int)IntOr(prof.get("level"), 0);
}

std::vector<int> ProfileCollectibles(const Json &prof)
{
    std::vector<int> out;
    const std::vector<Json> &list = prof.get("collectibles").items();
    for (size_t i = 0; i < list.size(); i++) {
        int64_t n;
        if (PyInt(list[i], n) && n >= 0 && n < 1024) out.push_back((int)n);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

/* ---- LIFETIME STATS: the profile's counters and the secure-DDL members they render into ----
 *
 * The match server measures, per match, the delta of each member of mp/playersecuredata.def its GSC
 * maintains and reports it as the row's "lifetime" object; add_xp accumulates it. Rendering resolves
 * every member BY NAME through the def. Eleven of the names are not streamed in the def (the zone
 * carries a pointer, the name comes from the JSON's nameRecovered), so for exactly those the byte
 * offset is asserted too [MEASURED 2026-09-15 off patch_code_post_gfx_mp.ff, def v11 / 0x096B8F57]:
 * a missing member is a missing stat, a misplaced one is a corrupted career. */
static const char *const kLifetimeDdl[] = {
    "score", "kills", "deaths", "assists", "headshots", "suicides", "hits", "totalShots", "wins", "losses", "ties",
    "escapes", "objectives", "bombPlants", "bombDefuses", "confirmed", "denied",
    "winStreak", "killStreak", /* best-ever: the report carries the RISE, so plain addition is right */
    "timePlayed", "timePlayedOverAll", "timePlayedTotal",
};
static const struct { const char *name; int byte; } kLifetimeCheck[] = {
    { "assists", 0x0164 }, { "confirmed", 0x1114 }, { "deaths", 0x111C }, { "denied", 0x1128 }, { "headshots", 0x1134 },
    { "hits", 0x1138 }, { "killStreak", 0x1148 }, { "kills", 0x114C }, { "score", 0x13F4 }, { "suicides", 0x1514 },
    { "timePlayed", 0x151C },
};
/* DERIVED members: recomputed from the totals exactly as the client's scripts do (updatePersRatio:
 * a zero denominator becomes 1), never taken from a report. */
static const struct { const char *path, *num; int64_t scale; const char *den; } kLifetimeDerived[] = {
    { "accuracy", "hits", 10000, "totalShots" },
    { "kdRatio", "kills", 1000, "deaths" },
    { "winRatio", "wins", 1000, "losses" },
    { "scorePerMinute", "score", 60, "timePlayedTotal" },
};

/* -> the field for `path`, or NULL when it cannot be trusted (never "guess"). */
static const PdField *DdlField(const PdDef &def, const std::string &path)
{
    const PdField *f = def.Find(path);
    if (!f) return NULL;
    if (def.version == 11 && def.formatChecksum == 0x096B8F57u)
        for (size_t i = 0; i < sizeof(kLifetimeCheck) / sizeof(kLifetimeCheck[0]); i++)
            if (path == kLifetimeCheck[i].name && f->byte != kLifetimeCheck[i].byte) return NULL;
    return f;
}

/* Blob.get(f) == want, else Blob.set(f, want): -> 1 when the member was rewritten. */
static int Stamp(PdBlob &blob, const PdField &f, double v)
{
    double want = f.kind == PD_FLOAT ? v : (v < 0 ? -(double)(int64_t)(-v) : (double)(int64_t)v);
    if (blob.Get(f) == want) return 0;
    return blob.Set(f, want) ? 1 : 0; /* a member we cannot type-fit is skipped */
}

/* _render_lifetime on a loaded blob. -> how many members changed. */
static int RenderLifetime(PdBlob &blob, const Json &prof)
{
    const Json &tot = prof.get("lifetime");
    if (!tot.isObject() || tot.members().empty()) return 0; /* never played: do not stamp zeroes over the seed */
    int changed = 0;
    for (size_t i = 0; i < sizeof(kLifetimeDdl) / sizeof(kLifetimeDdl[0]); i++) {
        const char *key = kLifetimeDdl[i];
        int64_t n;
        const PdField *f;
        if (!tot.has(key) || !(f = DdlField(*blob.def, key)) || !PyInt(tot.get(key), n)) continue;
        changed += Stamp(blob, *f, (double)std::max<int64_t>(0, std::min<int64_t>(kI32Max, n)));
    }
    for (size_t i = 0; i < sizeof(kLifetimeDerived) / sizeof(kLifetimeDerived[0]); i++) {
        const PdField *f = DdlField(*blob.def, kLifetimeDerived[i].path);
        if (!f) continue;
        if (!tot.has(kLifetimeDerived[i].num) && !tot.has(kLifetimeDerived[i].den)) continue; /* nothing to derive it from */
        int64_t n = 0, d = 0;
        if (tot.has(kLifetimeDerived[i].num) && !PyInt(tot.get(kLifetimeDerived[i].num), n)) continue;
        if (tot.has(kLifetimeDerived[i].den) && !PyInt(tot.get(kLifetimeDerived[i].den), d)) continue;
        n = std::max<int64_t>(0, n);
        d = std::max<int64_t>(0, d);
        if (!d) d = 1;
        int64_t scale = kLifetimeDerived[i].scale;
        int64_t v = n > (int64_t)0x7FFFFFFFFFFFFFFFLL / scale ? kI32Max : (n * scale) / d;
        changed += Stamp(blob, *f, (double)std::max<int64_t>(0, std::min<int64_t>(kI32Max, v)));
    }
    return changed;
}

/* ---- PROGRESSION: challenges[1000] and challegeUpdateTime (progression.blob_updates) ---- */

static void LoadChallenges(Server &s, ProfilesState &st)
{
    st.challengesTried = true;
    st.isChallenge.assign((size_t)kChallengeCount, 0);
    Bytes text;
    Json doc;
    if (!s.Resource("progression_tables.json", text) || !Json::Parse(text, doc)) {
        s.Log("profiles: progression_tables.json is missing or unreadable -- challenge progress cannot be rendered");
        return;
    }
    const std::vector<Json> &rows = doc.get("challenges").items();
    for (size_t i = 0; i < rows.size(); i++) {
        int64_t id = rows[i].get("id").asInt(-1);
        if (id < 0 || id >= kChallengeCount) continue; /* never written past the declared array */
        st.challengeIds.push_back((int)id);
        st.isChallenge[(size_t)id] = 1;
    }
}

/* The two indexed arrays progression owns, with the byte offset their first and last element must
 * land on [MEASURED off mp/playersecuredata.def v11]. The blob has NO per-field clamping on the client
 * side, so nothing is written at all if either end of either array has moved. */
static bool ProgressionLayoutOk(const PdDef &def)
{
    static const struct { const char *path; int byte; int kind; } check[] = {
        { "challenges[0]", 0x0174, PD_FLOAT }, { "challenges[999]", 0x1110, PD_FLOAT },
        { "challegeUpdateTime", 0x0170, PD_INT }, /* the typo is the client's own member name */
        { "weaponStatistics[0].itemId", 0x1538, PD_INT }, { "weaponStatistics[0].usedTime", 0x1540, PD_INT },
        { "weaponStatistics[99].usedTime", 0x19E4, PD_INT },
    };
    for (size_t i = 0; i < sizeof(check) / sizeof(check[0]); i++) {
        const PdField *f = def.Find(check[i].path);
        if (!f || f->byte != check[i].byte || f->kind != check[i].kind) return false;
    }
    return true;
}

/* _render_progression on a loaded blob. -> members changed, -1 when the layout guard refused.
 *
 * THE PROFILE IS AUTHORITATIVE OVER EVERY SLOT THE CHALLENGE TABLE DEFINES, including the ones with
 * no progress, which must read 0: every seed blob ships all 1000 slots at 999999.0 (a QA "everything
 * done" profile). Slots whose index is not a challenge id are left alone unless the profile holds
 * progress for them. Nothing is written to weaponStatistics[100]: C1 never reads it. */
static int RenderProgression(Server &s, PdBlob &blob, const Json &prof)
{
    ProfilesState &st = State(s);
    if (!st.challengesTried) LoadChallenges(s, st);
    if (!ProgressionLayoutOk(*blob.def)) return -1;
    std::map<int, double> progress; /* effective_progress: stored, with the mastery rows overwritten */
    const Json::Members &stored = prof.get("challenges").members();
    for (size_t i = 0; i < stored.size(); i++) {
        int64_t id;
        double v;
        if (PyInt(Json(stored[i].first), id) && PyFloat(stored[i].second, v) && id > -2147483647 && id < 2147483647) progress[(int)id] = v;
    }
    ProgressionMastery(s, prof, progress);
    int changed = 0;
    for (size_t i = 0; i < st.challengeIds.size(); i++) {
        int id = st.challengeIds[i];
        const PdField *f = blob.def->Find(Fmt("challenges[%d]", id));
        std::map<int, double>::iterator it = progress.find(id);
        if (f) changed += Stamp(blob, *f, it == progress.end() ? 0.0 : it->second);
    }
    for (std::map<int, double>::iterator it = progress.begin(); it != progress.end(); ++it) {
        if (it->first < 0 || it->first >= kChallengeCount || it->second == 0 || st.isChallenge[(size_t)it->first]) continue;
        const PdField *f = blob.def->Find(Fmt("challenges[%d]", it->first));
        if (f) changed += Stamp(blob, *f, it->second);
    }
    const PdField *upd = blob.def->Find("challegeUpdateTime");
    if (upd) changed += Stamp(blob, *upd, (double)s.Now());
    return changed;
}

/* _render_lifetime + _render_progression over one stored blob; `stored` is replaced when a member
 * changed. -> the note suffix ("" when there was nothing to do). */
static std::string RenderStats(Server &s, Bytes &stored, const Json &prof)
{
    PdBlob blob;
    if (!blob.Load(s, stored) || blob.def->key != kBlobName) return "";
    std::string note;
    int lt = RenderLifetime(blob, prof);
    if (lt) note += Fmt(" (+%d stats)", lt);
    int pg = RenderProgression(s, blob, prof);
    if (pg < 0) note += " (progression NOT written: the secure DDL's challenges[] or weaponStatistics[] moved)";
    else if (pg) note += Fmt(" (+%d progression)", pg);
    if (lt > 0 || pg > 0) {
        Bytes out;
        if (!blob.Store(out)) return "";
        stored.swap(out);
    }
    return note;
}

/* ---- the shared (unkeyed) blob ----
 * The match server's live rank pump knows only the one unkeyed mpdata_s; it is kept in step with the
 * profile about to play. While a match is dispatched only its owner may re-point it (MC-4). */
static std::string MirrorShared(Server &s, int64_t want, const Bytes &src, uint64_t uid)
{
    ProfilesState &st = State(s);
    if (st.mirrorOwner && uid && uid != st.mirrorOwner) return ""; /* somebody else is playing */
    Store &store = GetStore(s);
    Bytes shared;
    bool have = store.GetStorageFile(0, kBlobName, shared);
    int64_t xp;
    if (have && XpFromBlob(s, shared, xp) && xp == want) return "";
    PdBlob blob;
    if (!blob.Load(s, have ? shared : src) || blob.def->key != kBlobName) return "";
    Bytes out;
    if (!SetRankFields(s, blob, want) || !blob.Store(out) || !store.PutStorageFile(0, kBlobName, out, "render")) return "";
    return " (+shared mirror)";
}

static void RenderSharedStats(Server &s, const Json &prof)
{
    Store &store = GetStore(s);
    Bytes shared;
    if (!store.GetStorageFile(0, kBlobName, shared)) return;
    Bytes before = shared;
    RenderStats(s, shared, prof);
    if (shared != before) store.PutStorageFile(0, kBlobName, shared, "render");
}

/* ---- ensure_blob ---- */

std::string EnsureBlobEx(Server &s, uint64_t uid, bool mirror)
{
    if (!uid) return "";
    Store &store = GetStore(s);
    Json prof;
    if (!ProfileLoad(s, uid, prof)) return "";
    const std::string key = Fmt("%llu", (unsigned long long)uid);
    int64_t want = IntOr(prof.get("total_xp"), 0), have = 0;
    Bytes dst;
    bool haveDst = store.GetStorageFile(uid, kBlobName, dst), haveXp = false;
    if (haveDst) {
        haveXp = XpFromBlob(s, dst, have);
        if (haveXp && have == want) {
            /* The rank triple is current. The lifetime members may still not be - a match that
             * produced no XP still moves kills/score/timePlayed - so the stats pass runs here too. */
            Bytes before = dst;
            std::string lt = RenderStats(s, dst, prof);
            if (dst != before) store.PutStorageFile(uid, kBlobName, dst, "render");
            if (mirror) {
                MirrorShared(s, want, dst, uid);
                RenderSharedStats(s, prof);
            }
            return lt.empty() ? "" : "player_profiles: refreshed mpdata_s." + key + " stats" + lt;
        }
    }
    /* Source blob for the render: the uid's own if it exists, else the shipped level-1 seed. NOT the
     * shared blob: every member the render does not overwrite would leak from the last player's career. */
    Bytes src;
    const char *srcName = haveDst ? kBlobName : kSeedBlob;
    if (haveDst) src = dst;
    PdBlob blob;
    if ((!haveDst && !s.Resource(kSeedBlob, src)) || Ranks(s).empty() || !blob.Load(s, src) || blob.def->key != kBlobName)
        return Fmt("player_profiles: cannot read %s -- mpdata_s.%s NOT rendered", srcName, key.c_str());
    Bytes out;
    if (!SetRankFields(s, blob, want) || !blob.Store(out))
        return "player_profiles: render of mpdata_s." + key + " FAILED -- the served profile is stale";
    /* The rank triple first, then the lifetime members (what the account page shows) and the
     * challenge progress, through the same DDL machinery, before the blob is stored. */
    std::string lt = RenderStats(s, out, prof);
    if (!store.PutStorageFile(uid, kBlobName, out, "render"))
        return "player_profiles: render of mpdata_s." + key + " FAILED -- the served profile is stale";
    std::string tail;
    if (mirror) {
        tail = MirrorShared(s, want, out, uid);
        RenderSharedStats(s, prof);
    }
    return Fmt("player_profiles: rendered mpdata_s.%s level %d (%lld XP, was %s)%s%s", key.c_str(), LevelForXp(s, want), (long long)want,
               !haveDst ? "absent" : haveXp ? Fmt("%lld XP", (long long)have).c_str() : "None", lt.c_str(), tail.c_str());
}

std::string EnsureBlob(Server &s, uint64_t uid) { return EnsureBlobEx(s, uid, true); }

std::string ExportMatchBlob(Server &s, uint64_t uid)
{
    if (!uid || s.cfg.matchProfile.empty()) return ""; /* nobody to export for / no match host folder */
    std::string note = EnsureBlob(s, uid);
    Bytes blob;
    if (!GetStore(s).GetStorageFile(uid, kBlobName, blob))
        return "match blob: no mpdata_s for " + Fmt("%llu", (unsigned long long)uid) + (note.empty() ? "" : " (" + note + ")");
    /* The name contract of player_profiles.blob_path: <BLOB_NAME>.<decimal uid>. */
    std::string path = PathJoin(s.cfg.matchProfile, Fmt("%s.%llu", kBlobName, (unsigned long long)uid));
    if (!WriteFileAtomic(path, blob)) return "match blob: WRITE FAILED " + path;
    return Fmt("match blob: wrote %s (%u B)", path.c_str(), (unsigned)blob.size());
}

/* ---- the two mutators ---- */

std::string ProfileAddXp(Server &s, uint64_t uid, int64_t amount, const std::string &reason, const std::string &name,
                         const Json &stats, const Json &lifetime, const Json &progression, RankupClaim *rankup)
{
    if (rankup) *rankup = RankupClaim();
    if (amount <= 0 && !stats.truthy() && !lifetime.truthy()) return "";
    Json prof;
    if (!ProfileLoad(s, uid, prof)) return Fmt("player_profiles: unusable uid %llu -- nothing credited", (unsigned long long)uid);
    int64_t wasLevel = IntOr(prof.get("level"), 1);
    /* The profile's "rankup_gp_level" is the highest level already paid for. A profile that predates
     * the mark takes the level it had before its first credit: nothing retroactive. */
    int64_t mark = 0;
    if (!PyInt(prof.get("rankup_gp_level"), mark) || mark < 1) mark = wasLevel;
    int64_t total = std::max<int64_t>(0, IntOr(prof.get("total_xp"), 0) + amount);
    int64_t level = LevelForXp(s, total);
    prof["total_xp"] = Json((long long)total);
    prof["level"] = Json((long long)level);
    if (rankup && level > mark) {
        rankup->gp = RankupGp(mark, level);
        rankup->from = (int)mark + 1;
        rankup->to = (int)level;
        mark = level;
    }
    prof["rankup_gp_level"] = Json((long long)mark);
    if (!name.empty()) prof["name"] = name;
    static const char *const counters[] = { "kills", "deaths", "assists", "matches" };
    for (int i = 0; i < 4; i++)
        if (stats.isObject() && stats.has(counters[i]))
            prof[counters[i]] = Json((long long)std::max<int64_t>(0, IntOr(prof.get(counters[i]), 0) + IntOr(stats.get(counters[i]), 0)));
    if (lifetime.isObject() && !lifetime.members().empty()) {
        Json lt = prof.get("lifetime").isObject() ? prof.get("lifetime") : Json::MakeObject();
        const Json::Members &m = lifetime.members();
        for (size_t i = 0; i < m.size(); i++) {
            int64_t d;
            if (!PyInt(m[i].second, d)) continue; /* an unusable value, never a refusal */
            d = std::max<int64_t>(0, d);
            if (!d) continue;                     /* the server only sends deltas > 0 */
            const std::string &k = m[i].first;
            if (k == "accuracy" || k == "kdRatio" || k == "winRatio" || k == "scorePerMinute") continue; /* recomputed on render */
            lt[k] = Json((long long)std::max<int64_t>(0, IntOr(lt.get(k), 0) + d));
        }
        prof["lifetime"] = lt;
    }
    std::string progNote;
    if (progression.isObject() && !progression.members().empty()) progNote = ProgressionCredit(s, prof, progression);
    if (amount) {
        Json row = Json::MakeObject();
        row["ts"] = Json((double)s.Now());
        row["xp"] = Json((long long)amount);
        row["reason"] = reason;
        row["total"] = Json((long long)total);
        row["level"] = Json((long long)level);
        if (!prof.get("history").isArray()) prof["history"] = Json::MakeArray();
        prof["history"].push(row);
    }
    if (!ProfileSave(s, prof))
        return Fmt("player_profiles: SAVE FAILED for %llu -- %lld XP NOT persisted", (unsigned long long)uid, (long long)amount);
    /* the credit as its own row: the profile keeps the last 50, the account history more */
    if (amount) GetStore(s).AddHistory(uid, amount, reason, total, level);
    int idx;
    int64_t inRank;
    XpToRank(s, total, idx, inRank);
    int64_t toNext = std::max<int64_t>(0, RankXpToNext(s, idx) - inRank);
    return Fmt("player_profiles: %llu +%lld XP level %lld->%lld (total %lld, %lld into rank, %lld to next)%s%s",
               (unsigned long long)uid, (long long)amount, (long long)wasLevel, (long long)level, (long long)total, (long long)inRank,
               (long long)toNext, level > wasLevel ? "  *** RANK UP ***" : "", progNote.c_str());
}

/* The 1.1.1.13 client is MEASURED never to upload mpdata_s (only mpdata), which is why rank is
 * server-authored. If that ever changes, an upload must not be overwritten by the next render. */
std::string AbsorbBlob(Server &s, uint64_t uid)
{
    if (!uid) return "";
    Bytes blob;
    int64_t have;
    if (!GetStore(s).GetStorageFile(uid, kBlobName, blob) || !XpFromBlob(s, blob, have)) return "";
    Json prof;
    if (!ProfileLoad(s, uid, prof)) return "";
    int64_t total = IntOr(prof.get("total_xp"), 0);
    if (have <= total) return "";
    return ProfileAddXp(s, uid, have - total, "client-uploaded mpdata_s", "", Json(), Json(), Json(), NULL);
}

} // namespace dw

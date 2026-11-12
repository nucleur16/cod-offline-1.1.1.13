/*
 * dw_career.cpp - the career side of the lobby.
 *
 *   progression   career/progression.py + challenges.py + weapon_leveling.py: challenge progress, weapon
 *                 levels, the mastery bridge, coop missions, story intel and the unlock-flag set, over
 *                 resources/progression_tables.json.
 *   write-back    career/rank_writeback.py: fold the match server's match_report.json (and its spool)
 *                 into players/<uid>.json, pay GP, re-render mpdata_s, archive the report. The report
 *                 schema and the file life cycle are in dw_career.h.
 *
 * Where the Python lobby looks in resources\ for reports, this looks in Config::matchProfile (the
 * match server's profile folder); the retry state (match_report.state.json) and the ledger live in the
 * Store (kv "rank_writeback.state", reports.json).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <set>
#include "dw.h"
#include "dw_career.h"
#include "dw_playerdata.h"
#include "dw_profiles.h"
#include "dw_state.h"
#include "dw_store.h"

namespace dw {

namespace {

const char *const kHideFlag = "uf_item_hide"; /* the retail shelf-hide sentinel: never granted */
const int kCollectiblesCount = 1024;          /* collectibles[1024] of bool in mp/playerdata.def */
const int kGpCurrency = 1;                    /* C1's account-info builder shows wallet currency 1 as GP */
const char *const kReportName = "match_report.json";
const char *const kSpoolPrefix = "match_report.q.";
const char *const kJournalName = "match_report.journal.log";
const char *const kStateKey = "rank_writeback.state";
const char *const kSharedBlob = "mpdata_s";
const char *const kSeedBlob = "mpdata_s.level1.proposed";

struct FlagRow {
    std::string name, challenge;
    int64_t id, tier;
};

std::string Lower(std::string s)
{
    for (size_t i = 0; i < s.size(); i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] + 32);
    return s;
}

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

std::string Strip(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && IsSpace(s[a])) a++;
    while (b > a && IsSpace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

bool IsDigits(const std::string &s)
{
    if (s.empty()) return false;
    for (size_t i = 0; i < s.size(); i++)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

bool Has(const std::string &text, const char *needle) { return text.find(needle) != std::string::npos; }

/* int(v): -> false where Python raises (None, a non-numeric string, a container). */
bool PyInt(const Json &v, int64_t &out)
{
    if (v.isBool() || v.isInt()) { out = v.asInt(); return true; }
    if (v.isNumber()) {
        double d = v.asReal();
        if (d != d || d > 9e18 || d < -9e18) return false;
        out = (int64_t)d;
        return true;
    }
    if (v.isString()) {
        const std::string t = Strip(v.asString());
        if (t.empty()) return false;
        char *end = 0;
        long long n = strtoll(t.c_str(), &end, 10);
        if (end == t.c_str() || *end) return false;
        out = n;
        return true;
    }
    return false;
}

/* float(v) */
bool PyFloat(const Json &v, double &out)
{
    if (v.isBool() || v.isNumber()) { out = v.asReal(); return true; }
    if (v.isString()) {
        const std::string t = Strip(v.asString());
        if (t.empty()) return false;
        char *end = 0;
        double d = strtod(t.c_str(), &end);
        if (end == t.c_str() || *end) return false;
        out = d;
        return true;
    }
    return false;
}

/* int(v or 0): a falsy value is 0; -> false where Python raises. */
bool PyIntOr0(const Json &v, int64_t &out)
{
    out = 0;
    return !v.truthy() || PyInt(v, out);
}

/* str(v) of a decoded JSON value, for notes and names. */
std::string PyStr(const Json &v)
{
    if (v.isString()) return v.asString();
    if (v.isNull()) return "None";
    if (v.isBool()) return v.asBool() ? "True" : "False";
    return v.dump();
}

std::string Quoted(const std::string &s) { return "'" + s + "'"; } /* %r of a str */

/* An insertion-ordered {key: value}, as the Python dicts these maps are ported from. */
template <class K, class V> struct Ordered {
    std::vector<std::pair<K, V> > rows;
    V *find(const K &k)
    {
        for (size_t i = 0; i < rows.size(); i++)
            if (rows[i].first == k) return &rows[i].second;
        return NULL;
    }
    V get(const K &k, V dflt)
    {
        V *p = find(k);
        return p ? *p : dflt;
    }
    void set(const K &k, V v)
    {
        V *p = find(k);
        if (p) *p = v;
        else rows.push_back(std::make_pair(k, v));
    }
};
typedef Ordered<int64_t, double> ChallengeMap;
typedef Ordered<std::string, int64_t> CounterMap;

} // namespace

/* ---- the tables (progression_tables.load, Challenges.__init__, WeaponLeveling.__init__) ---- */

struct CareerState {
    bool tried, ok;
    std::vector<CareerChallenge> challenges;  /* table order */
    std::map<std::string, size_t> byRef;      /* lower-cased ref -> index */
    std::map<int64_t, size_t> byId;
    std::set<std::string> refs;               /* exact spelling, for mastery_ref */
    std::set<int64_t> deactivated;
    std::vector<FlagRow> flags;               /* mp/unlockflags.csv, table order */
    std::map<std::string, CareerWeapon> weapons; /* lower-cased name */
    bool rankVersionTried;
    int rankVersion;
    bool polled;
    double lastPoll;
    CareerState() : tried(false), ok(false), rankVersionTried(false), rankVersion(2), polled(false), lastPoll(0) {}
};

void CareerInit(Server &s) { s.st->career = new CareerState(); }

void CareerFree(Server &s)
{
    delete s.st->career;
    s.st->career = NULL;
}

static CareerState &State(Server &s)
{
    if (!s.st->career) s.st->career = new CareerState();
    return *s.st->career;
}

static CareerState &Tables(Server &s)
{
    CareerState &c = State(s);
    if (c.tried) return c;
    c.tried = true;
    Bytes text;
    Json doc;
    if (!s.Resource("progression_tables.json", text) || !Json::Parse(text, doc) || !doc.get("challenges").isArray() ||
        !doc.get("unlock_flags").isArray() || !doc.get("weapons").isObject() || !doc.get("weapon_curves").isObject()) {
        s.Log("career: progression_tables.json is missing or unreadable -- challenge and weapon progress cannot be credited");
        return c;
    }
    const std::vector<Json> &chs = doc.get("challenges").items();
    for (size_t i = 0; i < chs.size(); i++) {
        CareerChallenge ch;
        ch.id = chs[i].get("id").asInt();
        ch.ref = chs[i].get("ref").str();
        const std::vector<Json> &tiers = chs[i].get("tiers").items();
        for (size_t k = 0; k < tiers.size(); k++) {
            ch.targets.push_back(tiers[k].get("target").asReal());
            ch.rewards.push_back(tiers[k].get("reward").asInt());
        }
        c.byRef[Lower(ch.ref)] = c.challenges.size(); /* a repeated ref / id: the later row wins */
        c.byId[ch.id] = c.challenges.size();
        c.refs.insert(ch.ref);
        c.challenges.push_back(ch);
    }
    const std::vector<Json> &off = doc.get("deactivated_ids").items();
    for (size_t i = 0; i < off.size(); i++) c.deactivated.insert(off[i].asInt());
    const std::vector<Json> &fl = doc.get("unlock_flags").items();
    for (size_t i = 0; i < fl.size(); i++) {
        FlagRow f;
        f.name = fl[i].get("name").str();
        f.challenge = fl[i].get("challenge").str();
        f.id = fl[i].get("id").asInt();
        f.tier = fl[i].get("tier").asInt();
        c.flags.push_back(f);
    }
    const Json &curves = doc.get("weapon_curves");
    const Json::Members &ws = doc.get("weapons").members();
    for (size_t i = 0; i < ws.size(); i++) {
        CareerWeapon w;
        w.name = ws[i].first;
        w.item = ws[i].second.get("item").asInt();
        const std::vector<Json> &cv = curves.get(ws[i].second.get("curve").str()).items();
        for (size_t k = 0; k < cv.size(); k++) w.curve.push_back(cv[k].asInt());
        if (w.curve.empty()) w.curve.push_back(0);
        c.weapons[Lower(w.name)] = w;
    }
    c.ok = true;
    return c;
}

bool CareerTablesOk(Server &s) { return Tables(s).ok; }

int CareerChallenge::Completed(double progress) const
{
    int n = 0;
    for (size_t i = 0; i < targets.size() && progress >= targets[i]; i++) n++;
    return n;
}

int CareerWeapon::LevelForXp(int64_t xp) const
{
    int level = 0;
    if (xp <= 0) return 0;
    for (size_t n = 0; n < curve.size(); n++) {
        if (xp >= curve[n]) level = (int)n;
        else break;
    }
    return level;
}

const CareerChallenge *CareerChallengeGet(Server &s, const std::string &ref)
{
    CareerState &c = Tables(s);
    const std::string text = Strip(ref);
    size_t d = text.find_first_not_of('-');
    if (d != std::string::npos && IsDigits(text.substr(d))) {
        /* int("--5") raises in Python and takes the whole credit with it; here it is an unknown ref */
        int64_t id;
        if (d > 1 || !PyInt(Json(text), id)) return NULL;
        std::map<int64_t, size_t>::const_iterator it = c.byId.find(id);
        return it == c.byId.end() ? NULL : &c.challenges[it->second];
    }
    std::map<std::string, size_t>::const_iterator it = c.byRef.find(Lower(text));
    return it == c.byRef.end() ? NULL : &c.challenges[it->second];
}

bool CareerChallengeDeactivated(Server &s, int64_t id) { return Tables(s).deactivated.count(id) != 0; }

const CareerWeapon *CareerWeaponGet(Server &s, const std::string &name)
{
    CareerState &c = Tables(s);
    std::map<std::string, CareerWeapon>::const_iterator it = c.weapons.find(Lower(name));
    return it == c.weapons.end() ? NULL : &it->second;
}

/* CoDOL's ch_<weapon>_mastery counts weapon LEVELS (its targets are 10, 20, 30, 31), not events. */
std::string CareerMasteryRef(Server &s, const std::string &weapon)
{
    const CareerWeapon *w = CareerWeaponGet(s, weapon);
    if (!w) return "";
    std::string ref = "ch_" + w->name + "_mastery";
    return Tables(s).refs.count(ref) ? ref : "";
}

/* ---- the profile's progression maps (progression.challenge_progress / weapon_progress / ...) ---- */

static void ChallengeProgress(const Json &prof, ChallengeMap &out)
{
    const Json::Members &m = prof.get("challenges").members();
    for (size_t i = 0; i < m.size(); i++) {
        int64_t id;
        double v;
        if (PyInt(Json(m[i].first), id) && PyFloat(m[i].second, v)) out.set(id, v); /* JSON keys are strings */
    }
}

static void CounterProgress(const Json &prof, const char *key, CounterMap &out)
{
    const Json::Members &m = prof.get(key).members();
    for (size_t i = 0; i < m.size(); i++) {
        int64_t v;
        if (PyInt(m[i].second, v)) out.set(Lower(m[i].first), v);
    }
}

std::map<std::string, int> CareerWeaponLevels(Server &s, const Json &prof)
{
    std::map<std::string, int> out;
    CounterMap wp;
    CounterProgress(prof, "weapons", wp);
    for (size_t i = 0; i < wp.rows.size(); i++) {
        const CareerWeapon *w = CareerWeaponGet(s, wp.rows[i].first);
        out[wp.rows[i].first] = w ? w->LevelForXp(wp.rows[i].second) : 0;
    }
    return out;
}

/* THE MASTERY BRIDGE: derived from the weapon counter on every read, never stored. */
static void Mastery(Server &s, const Json &prof, std::map<int64_t, double> &out)
{
    std::map<std::string, int> levels = CareerWeaponLevels(s, prof);
    for (std::map<std::string, int>::iterator it = levels.begin(); it != levels.end(); ++it) {
        std::string ref = CareerMasteryRef(s, it->first);
        if (ref.empty()) continue;
        const CareerChallenge *c = CareerChallengeGet(s, ref);
        if (c) out[c->id] = (double)it->second;
    }
}

void ProgressionMastery(Server &s, const Json &prof, std::map<int, double> &out)
{
    std::map<int64_t, double> m;
    Mastery(s, prof, m);
    for (std::map<int64_t, double>::iterator it = m.begin(); it != m.end(); ++it)
        if (it->first > -2147483647 && it->first < 2147483647) out[(int)it->first] = it->second;
}

void CareerEffectiveProgress(Server &s, const Json &prof, std::map<int64_t, double> &out)
{
    ChallengeMap stored;
    ChallengeProgress(prof, stored);
    for (size_t i = 0; i < stored.rows.size(); i++) out[stored.rows[i].first] = stored.rows[i].second;
    Mastery(s, prof, out);
}

/* Challenge flags: completed tiers >= the flag's tier. uf_rank_N: player level >= N. Everything else
 * is an unconditional front-end flag. Two shipped flags name a tier their challenge does not have
 * (505, 81); derived from progress they are simply never reached. */
std::vector<int64_t> CareerGrantedFlagIds(Server &s, const Json &prof)
{
    CareerState &c = Tables(s);
    std::vector<int64_t> ids;
    if (!c.ok) return ids;
    int64_t level = 0;
    if (!PyIntOr0(prof.get("level"), level)) level = 0;
    std::map<int64_t, double> progress;
    CareerEffectiveProgress(s, prof, progress);
    std::set<int64_t> got;
    for (size_t i = 0; i < c.flags.size(); i++) {
        const FlagRow &f = c.flags[i];
        if (!f.id || f.name == kHideFlag) continue;
        if (!f.challenge.empty()) {
            const CareerChallenge *ch = CareerChallengeGet(s, f.challenge);
            if (!ch) continue;
            std::map<int64_t, double>::iterator p = progress.find(ch->id);
            if (ch->Completed(p == progress.end() ? 0.0 : p->second) >= (f.tier ? f.tier : 1)) got.insert(f.id);
            continue;
        }
        if (!f.name.compare(0, 8, "uf_rank_")) {
            int64_t need;
            if (PyInt(Json(f.name.substr(8)), need) && level >= need) got.insert(f.id);
            continue;
        }
        got.insert(f.id);
    }
    ids.assign(got.begin(), got.end());
    return ids;
}

/* A mission's completion IS its storymission challenge: so_<mission> for a story mission,
 * so_<mission>_2 (the first difficulty) for a Spec Ops one; "" for the prologue or an unknown name. */
std::string CareerCoopMissionChallenge(Server &s, const std::string &mission)
{
    const std::string name = Lower(Strip(mission));
    if (name.compare(0, 5, "coop_")) return "";
    const CareerChallenge *c = CareerChallengeGet(s, "so_" + name);
    if (!c) c = CareerChallengeGet(s, "so_" + name + "_2");
    return c ? c->ref : "";
}

/* The ONLY channel that gives a weapon a level in C1: field 4 (+0x18) of its inventory record. */
std::map<int64_t, int64_t> CareerWeaponXpByItem(Server &s, const Json &prof)
{
    std::map<int64_t, int64_t> out;
    CounterMap wp;
    CounterProgress(prof, "weapons", wp);
    for (size_t i = 0; i < wp.rows.size(); i++) {
        const CareerWeapon *w = CareerWeaponGet(s, wp.rows[i].first);
        if (!w || !w->item || wp.rows[i].second <= 0) continue;
        int64_t &slot = out[w->item];
        slot = std::max(slot, wp.rows[i].second);
    }
    return out;
}

/* ---- progression.credit ---- */

namespace {

struct Advance {
    bool known, deactivated;
    int tiers;        /* how many tiers this credit completed */
    std::string ref;
    Advance() : known(false), deactivated(false), tiers(0) {}
};

/* Challenges.advance: add `amount` to one challenge. An unknown or deactivated ref is a no-op that
 * says so: one bad row in a match report must not cost the rest of it. */
Advance AdvanceChallenge(Server &s, ChallengeMap &stored, const std::string &ref, double amount)
{
    Advance r;
    r.ref = ref;
    const CareerChallenge *c = CareerChallengeGet(s, ref);
    if (!c) return r;
    r.known = true;
    r.ref = c->ref;
    if (CareerChallengeDeactivated(s, c->id)) {
        r.deactivated = true;
        return r;
    }
    double before = stored.get(c->id, 0.0), after = std::max(0.0, before + amount);
    stored.set(c->id, after);
    r.tiers = std::max(0, c->Completed(after) - c->Completed(before));
    return r;
}

} // namespace

std::string ProgressionCredit(Server &s, Json &prof, const Json &deltas)
{
    if (!Tables(s).ok) /* Python raises here and add_xp reports "[progression FAILED ...]", which makes the
                          write-back retry (and re-credit the XP of) the whole report: not reproduced */
        return "  [progression skipped: progression_tables.json is unreadable]";
    std::vector<int64_t> beforeFlags = CareerGrantedFlagIds(s, prof);

    ChallengeMap stored;
    ChallengeProgress(prof, stored);
    CounterMap weapons, uses;
    CounterProgress(prof, "weapons", weapons);
    CounterProgress(prof, "weapon_uses", uses);
    std::vector<std::string> notes;
    int tiers = 0, levels = 0;

    const Json::Members &chd = deltas.get("challenges").members();
    for (size_t i = 0; i < chd.size(); i++) {
        double amount = 0;
        if (chd[i].second.truthy()) PyFloat(chd[i].second, amount);
        Advance r = AdvanceChallenge(s, stored, chd[i].first, amount);
        if (!r.known) { notes.push_back("unknown challenge " + Quoted(chd[i].first)); continue; }
        if (r.deactivated) { notes.push_back("challenge " + r.ref + " is deactivated"); continue; }
        tiers += r.tiers;
    }

    /* WeaponLeveling.credit: an unknown weapon is still credited into the map, with no level. */
    const Json::Members &wd = deltas.get("weapons").members();
    for (size_t i = 0; i < wd.size(); i++) {
        const std::string key = Lower(wd[i].first);
        int64_t amount = 0;
        PyIntOr0(wd[i].second, amount);
        int64_t before = weapons.get(key, 0), after = std::max<int64_t>(0, before + amount);
        weapons.set(key, after);
        const CareerWeapon *w = CareerWeaponGet(s, key);
        if (!w) { notes.push_back("unknown weapon " + Quoted(wd[i].first)); continue; }
        levels += w->LevelForXp(after) - w->LevelForXp(before);
    }
    const Json::Members &ud = deltas.get("weapon_uses").members();
    for (size_t i = 0; i < ud.size(); i++) {
        const std::string key = Lower(ud[i].first);
        int64_t amount = 0;
        PyIntOr0(ud[i].second, amount);
        uses.set(key, uses.get(key, 0) + amount);
    }

    /* Campaign missions. A completion completes the mission's storymission challenge, at most to its
     * target: a replay neither re-pays the reward nor pushes the counter past 1. A failure is
     * recorded and unlocks nothing. */
    Ordered<std::string, std::pair<int64_t, int64_t> > coop; /* mission -> (completed, failed) */
    const Json::Members &cm = prof.get("coop_missions").members();
    for (size_t i = 0; i < cm.size(); i++) {
        if (!cm[i].second.isObject()) continue;
        int64_t done = 0, failed = 0;
        if (!PyIntOr0(cm[i].second.get("completed"), done)) done = 0;
        if (!PyIntOr0(cm[i].second.get("failed"), failed)) failed = 0;
        coop.set(cm[i].first, std::make_pair(std::max<int64_t>(0, done), std::max<int64_t>(0, failed)));
    }
    std::vector<std::string> coopDone;
    for (int pass = 0; pass < 2; pass++) {
        const Json::Members &bucket = deltas.get(pass == 0 ? "coop_completed" : "coop_failed").members();
        for (size_t i = 0; i < bucket.size(); i++) {
            double f;
            if (!PyFloat(bucket[i].second, f) || f != f || f > 9e18 || f < -9e18) {
                notes.push_back("unusable coop mission " + Quoted(bucket[i].first));
                continue;
            }
            int64_t n = (int64_t)f;
            const std::string name = Lower(Strip(bucket[i].first));
            if (n <= 0 || name.compare(0, 5, "coop_")) {
                if (n > 0) notes.push_back("not a coop mission " + Quoted(bucket[i].first));
                continue;
            }
            std::pair<int64_t, int64_t> rec = coop.get(name, std::pair<int64_t, int64_t>(0, 0));
            (pass == 0 ? rec.first : rec.second) += n;
            coop.set(name, rec);
            if (pass != 0) continue;
            coopDone.push_back(name);
            std::string ref = CareerCoopMissionChallenge(s, name);
            if (ref.empty()) continue; /* the prologue: recorded, no unlock */
            const CareerChallenge *c = CareerChallengeGet(s, ref);
            double have = stored.get(c->id, 0.0);
            if (have >= 1.0) continue;
            Advance r = AdvanceChallenge(s, stored, ref, 1.0 - have);
            if (r.known && !r.deactivated) tiers += r.tiers;
            else notes.push_back("coop challenge " + ref + " is " + (r.deactivated ? "deactivated" : "unknown"));
        }
    }
    if (!coop.rows.empty() || prof.has("coop_missions")) {
        Json out = Json::MakeObject();
        for (size_t i = 0; i < coop.rows.size(); i++) {
            Json rec = Json::MakeObject();
            rec["completed"] = Json((long long)coop.rows[i].second.first);
            rec["failed"] = Json((long long)coop.rows[i].second.second);
            out[coop.rows[i].first] = rec;
        }
        prof["coop_missions"] = out;
    }

    Json chOut = Json::MakeObject(), wOut = Json::MakeObject(), uOut = Json::MakeObject();
    for (size_t i = 0; i < stored.rows.size(); i++) chOut[Fmt("%lld", (long long)stored.rows[i].first)] = Json(stored.rows[i].second);
    for (size_t i = 0; i < weapons.rows.size(); i++) wOut[weapons.rows[i].first] = Json((long long)weapons.rows[i].second);
    for (size_t i = 0; i < uses.rows.size(); i++) uOut[uses.rows[i].first] = Json((long long)uses.rows[i].second);
    prof["challenges"] = chOut;
    prof["weapons"] = wOut;
    prof["weapon_uses"] = uOut;

    /* Story intel: a SET. Every index reported with a positive value is held from now on. */
    std::vector<int> have = ProfileCollectibles(prof);
    std::set<int> held(have.begin(), have.end());
    std::vector<int> gained;
    const Json::Members &cd = deltas.get("collectibles").members();
    for (size_t i = 0; i < cd.size(); i++) {
        int64_t idx;
        double n;
        if (!PyInt(Json(cd[i].first), idx) || !PyFloat(cd[i].second, n)) {
            notes.push_back("unusable collectible " + Quoted(cd[i].first));
            continue;
        }
        if (n <= 0) continue;
        if (idx < 0 || idx >= kCollectiblesCount) {
            notes.push_back("collectible " + Quoted(cd[i].first) + " out of range");
            continue;
        }
        if (held.insert((int)idx).second) gained.push_back((int)idx);
    }
    if (!held.empty() || prof.has("collectibles")) {
        Json list = Json::MakeArray();
        for (std::set<int>::iterator it = held.begin(); it != held.end(); ++it) list.push(Json(*it));
        prof["collectibles"] = list;
    }

    std::vector<int64_t> afterFlags = CareerGrantedFlagIds(s, prof);
    size_t flagsGained = 0;
    for (size_t i = 0; i < afterFlags.size(); i++)
        if (!std::binary_search(beforeFlags.begin(), beforeFlags.end(), afterFlags[i])) flagsGained++;

    /* player_profiles.add_xp's summary of the credit */
    std::vector<std::string> bits;
    if (tiers) bits.push_back(Fmt("%d challenge tier(s)", tiers));
    if (levels) bits.push_back(Fmt("%d weapon level(s)", levels));
    if (flagsGained) bits.push_back(Fmt("%u unlock flag(s)", (unsigned)flagsGained));
    if (!coopDone.empty()) {
        std::string list;
        for (size_t i = 0; i < coopDone.size(); i++) list += (i ? "," : "") + coopDone[i];
        bits.push_back("coop mission complete " + list);
    }
    if (!gained.empty()) {
        std::sort(gained.begin(), gained.end());
        std::string list;
        for (size_t i = 0; i < gained.size(); i++) list += Fmt(i ? ",%d" : "%d", gained[i]);
        bits.push_back("intel " + list);
    }
    if (!notes.empty()) {
        std::string list;
        for (size_t i = 0; i < notes.size() && i < 3; i++) list += (i ? "; " : "") + notes[i];
        bits.push_back("skipped " + list);
    }
    if (bits.empty()) return "";
    std::string out = "  [progression: ";
    for (size_t i = 0; i < bits.size(); i++) out += (i ? ", " : "") + bits[i];
    return out + "]";
}

/* ==== rank_writeback ==== */

namespace {

/* ---- SHA-1 (the report id; the Python ledger keys on hashlib.sha1(raw).hexdigest()) ---- */

uint32_t Rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

std::string Sha1Hex(const Bytes &data)
{
    uint32_t h[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
    Bytes msg = data;
    uint64_t bits = (uint64_t)data.size() * 8;
    msg += (char)0x80;
    while (msg.size() % 64 != 56) msg += (char)0;
    for (int i = 7; i >= 0; i--) msg += (char)(unsigned char)(bits >> (8 * i));
    for (size_t off = 0; off < msg.size(); off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)(unsigned char)msg[off + 4 * i] << 24 | (uint32_t)(unsigned char)msg[off + 4 * i + 1] << 16 |
                   (uint32_t)(unsigned char)msg[off + 4 * i + 2] << 8 | (uint32_t)(unsigned char)msg[off + 4 * i + 3];
        for (int i = 16; i < 80; i++) w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
            uint32_t t = Rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = Rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    unsigned char out[20];
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < 4; k++) out[4 * i + k] = (unsigned char)(h[i] >> (24 - 8 * k));
    return Hex(out, 20);
}

/* raw.decode("utf-8"): well-formed UTF-8 (no overlongs, no surrogates, <= U+10FFFF). */
bool Utf8Ok(const Bytes &b)
{
    size_t i = 0, n = b.size();
    while (i < n) {
        unsigned c = (unsigned char)b[i];
        int more;
        unsigned cp, min;
        if (c < 0x80) { i++; continue; }
        else if ((c & 0xE0) == 0xC0) { more = 1; cp = c & 0x1F; min = 0x80; }
        else if ((c & 0xF0) == 0xE0) { more = 2; cp = c & 0x0F; min = 0x800; }
        else if ((c & 0xF8) == 0xF0) { more = 3; cp = c & 0x07; min = 0x10000; }
        else return false;
        if (i + (size_t)more >= n) return false;
        for (int k = 1; k <= more; k++) {
            unsigned t = (unsigned char)b[i + (size_t)k];
            if ((t & 0xC0) != 0x80) return false;
            cp = cp << 6 | (t & 0x3F);
        }
        if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += (size_t)more + 1;
    }
    return true;
}

std::string TimeText(Server &s, const char *fmt)
{
    time_t t = (time_t)s.Now();
    struct tm *tm = localtime(&t);
    char buf[40];
    if (!tm || !strftime(buf, sizeof(buf), fmt, tm)) return "0";
    return buf;
}

/* The permanent record beside the reports; best-effort. */
void Journal(Server &s, const std::string &dir, const std::string &text)
{
    FILE *f = fopen(PathJoin(dir, kJournalName).c_str(), "ab");
    if (!f) return;
    fprintf(f, "%s | %s\n", TimeText(s, "%Y-%m-%d %H:%M:%S").c_str(), text.c_str());
    fclose(f);
}

/* Consume-by-rename. -> the archived base name, "" when it could not be moved. */
std::string Archive(Server &s, const std::string &dir, const std::string &path, const char *kind)
{
    const std::string ts = TimeText(s, "%Y%m%d-%H%M%S");
    for (int n = 0; n < 100; n++) {
        std::string name = Fmt("match_report.%s.%s%s.json", kind, ts.c_str(), n ? Fmt("-%d", n).c_str() : "");
        std::string dst = PathJoin(dir, name);
        if (FileExists(dst)) continue;
        return rename(path.c_str(), dst.c_str()) == 0 ? name : "";
    }
    return "";
}

Json LoadState(Server &s)
{
    Json d = GetStore(s).KvGet(kStateKey);
    return d.isObject() ? d : Json::MakeObject();
}

void SaveState(Server &s, const std::string &rid, const Json &applied)
{
    Json st = Json::MakeObject();
    st["report_id"] = rid;
    st["applied"] = applied.isObject() ? applied : Json::MakeObject();
    st["ts"] = Json((double)s.Now());
    GetStore(s).KvSet(kStateKey, st);
}

/* player_profiles._uid_key on a report row's uid: a bare decimal string (or an int). Where the
 * Python lobby keys the profile by the string itself, the store keys by the u64: a uid that is 0 or
 * does not fit 64 bits cannot name an account and is refused; leading zeros collapse. */
bool RowUid(const Json &v, uint64_t &uid, std::string &key)
{
    if (!v.isString() && !v.isInt()) return false; /* a bool reads "True" in Python: not digits */
    const std::string t = Strip(PyStr(v));
    if (t.empty() || t.size() > 24 || !IsDigits(t)) return false;
    size_t z = t.find_first_not_of('0');
    if (z == std::string::npos) return false;
    const std::string digits = t.substr(z);
    if (digits.size() > 20 || (digits.size() == 20 && digits > "18446744073709551615")) return false;
    uid = strtoull(digits.c_str(), 0, 10);
    key = digits;
    return true;
}

int64_t RowTotal(const Json &row, const char *field, bool &ok)
{
    const Json &xp = row.get("xp");
    int64_t v = 0;
    ok = true;
    if (xp.truthy() && xp.has(field)) ok = PyInt(xp.get(field), v);
    return ok ? std::max<int64_t>(0, v) : 0;
}

/* _amounts: delta = what the live pump did not already persist (the shared blob's share),
 * full = the whole match (a profile never saw the pump's live saves). */
void Amounts(const Json &rep, int64_t &delta, int64_t &full, std::string &names)
{
    delta = full = 0;
    names.clear();
    const std::vector<Json> &rows = rep.get("players").items();
    bool first = true;
    for (size_t i = 0; i < rows.size(); i++) {
        if (rows[i].get("bot").truthy()) continue;
        bool ok;
        int64_t total = RowTotal(rows[i], "total", ok), appl = RowTotal(rows[i], "applied_live", ok);
        delta += std::max<int64_t>(0, total - appl);
        full += total;
        names += (first ? "" : ",") + PyStr(rows[i].get("name"));
        first = false;
    }
    if (names.empty()) names = "-";
}

/* _merge_lifetime: {secure-DDL member: per-match delta}, summed; the key set is open. */
void MergeLifetime(Json &dst, const Json &row)
{
    const Json &lt = row.get("lifetime");
    if (!lt.isObject()) return;
    const Json::Members &m = lt.members();
    for (size_t i = 0; i < m.size(); i++) {
        int64_t d;
        if (!PyInt(m[i].second, d)) continue;
        d = std::max<int64_t>(0, d);
        if (d) dst[m[i].first] = Json((long long)(dst.get(m[i].first).asInt() + d));
    }
}

/* _merge_progression: fold one row's progression object into an accumulator. Refs are validated
 * later, by ProgressionCredit, so a rename in the client's tables shows up in the journal. */
void MergeProgression(Json &dst, const Json &row)
{
    static const char *const buckets[] = { "challenges", "weapons", "weapon_uses", "collectibles" };
    const Json &raw = row.get("progression");
    const Json &src = raw.isObject() ? raw : Json::null();
    for (int b = 0; b < 4; b++) {
        const Json *part = &src.get(buckets[b]);
        if (!part->isObject() && b < 2) part = &row.get(buckets[b]); /* the older flat keys on the row itself */
        if (!part->isObject()) continue;
        if (!dst.has(buckets[b])) dst[buckets[b]] = Json::MakeObject();
        Json &into = dst[buckets[b]];
        const Json::Members &m = part->members();
        for (size_t i = 0; i < m.size(); i++) {
            double d;
            if (!PyFloat(m[i].second, d) || !d) continue;
            into[m[i].first] = Json(into.get(m[i].first).asReal() + d);
        }
    }
    static const char *const coop[2][2] = { { "coop_mission_completed", "coop_completed" }, { "coop_mission_failed", "coop_failed" } };
    for (int k = 0; k < 2; k++) {
        const Json &name = src.get(coop[k][0]);
        const std::string n = name.isString() ? Strip(name.asString()) : "";
        if (n.empty()) continue;
        if (!dst.has(coop[k][1])) dst[coop[k][1]] = Json::MakeObject();
        Json &into = dst[coop[k][1]];
        into[n] = Json((long long)(into.get(n).asInt() + 1));
    }
    /* "gp" is everything the server awarded; the part it paid for rank-ups is kept apart, because
     * the lobby pays rank-ups itself off the profile's own level mark. */
    int64_t gp = 0;
    if (!PyIntOr0(src.get("gp"), gp)) gp = 0;
    if (gp > 0) {
        dst["gp"] = Json((long long)(dst.get("gp").asInt() + gp));
        const Json &fixed = src.get("gp_fixed");
        int64_t ranks = 0;
        if (!fixed.isObject() || !PyIntOr0(fixed.get("rankups"), ranks)) ranks = 0;
        ranks = std::min(std::max<int64_t>(0, ranks), gp);
        if (ranks) dst["gp_rankups"] = Json((long long)(dst.get("gp_rankups").asInt() + ranks));
    }
}

/* One distinct row uid of a report (_per_uid_amounts): its OWN row's XP, name and counters. Two rows
 * sharing a uid (a reconnect) are summed, so the profile is credited once for the match. */
struct Entry {
    uint64_t uid;
    int64_t xp, kills, deaths, assists;
    std::string name;
    Json lifetime, progression;
    Entry() : uid(0), xp(0), kills(0), deaths(0), assists(0), lifetime(Json::MakeObject()), progression(Json::MakeObject()) {}
};
typedef std::map<std::string, Entry> PerUid; /* ordered as Python's sorted() orders the uid strings */

void PerUidAmounts(const Json &rep, PerUid &out)
{
    const std::vector<Json> &rows = rep.get("players").items();
    for (size_t i = 0; i < rows.size(); i++) {
        const Json &r = rows[i];
        if (r.get("bot").truthy()) continue;
        uint64_t uid;
        std::string key;
        if (!RowUid(r.get("uid"), uid, key)) continue; /* skipped, never folded into somebody else's total */
        Entry &e = out[key];
        e.uid = uid;
        bool ok;
        e.xp += RowTotal(r, "total", ok);
        if (e.name.empty() && r.get("name").truthy()) e.name = PyStr(r.get("name"));
        int64_t v;
        if (r.has("kills") && PyInt(r.get("kills"), v)) e.kills += std::max<int64_t>(0, v);
        if (r.has("deaths") && PyInt(r.get("deaths"), v)) e.deaths += std::max<int64_t>(0, v);
        if (r.has("assists") && PyInt(r.get("assists"), v)) e.assists += std::max<int64_t>(0, v);
        MergeLifetime(e.lifetime, r);
        MergeProgression(e.progression, r);
    }
}

bool HasProgression(const Json &prog)
{
    static const char *const keys[] = { "challenges", "weapons", "weapon_uses", "collectibles", "coop_completed", "coop_failed", "gp" };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (prog.get(keys[i]).truthy()) return true;
    return false;
}

/* The GP wallet, the same row the marketplace balance replies read (seeded with
 * CODOL_MARKETPLACE_BALANCE = 0 when the account has none yet). */
std::string CreditGp(Server &s, uint64_t uid, int64_t amount)
{
    if (amount <= 0) return "";
    int64_t balance = GetStore(s).WalletCredit(uid, kGpCurrency, amount, 0);
    return Fmt("+%lld GP (balance %lld)", (long long)amount, (long long)balance);
}

/* _apply_to_profile: credit `amount` XP and the row's counters to players/<uid>.json, re-render the
 * blob the client reads, pay the GP. `entry` NULL = the legacy aggregate over every human row (a
 * report whose rows name no account). */
bool ApplyToProfile(Server &s, uint64_t uid, int64_t amount, const std::string &reason, const Json &rep, const Entry *entry,
                    std::string &desc)
{
    Json stats = Json::MakeObject(), lifetime = Json::MakeObject(), prog = Json::MakeObject();
    stats["matches"] = 1; /* one report is one match per profile */
    std::string name;
    if (entry) {
        name = entry->name;
        stats["kills"] = Json((long long)entry->kills);
        stats["deaths"] = Json((long long)entry->deaths);
        stats["assists"] = Json((long long)entry->assists);
        lifetime = entry->lifetime;
        prog = entry->progression;
    } else {
        static const char *const counters[] = { "kills", "deaths", "assists" };
        const std::vector<Json> &rows = rep.get("players").items();
        for (size_t i = 0; i < rows.size(); i++) {
            const Json &r = rows[i];
            if (r.get("bot").truthy()) continue;
            if (name.empty() && r.get("name").truthy()) name = PyStr(r.get("name"));
            for (int k = 0; k < 3; k++) {
                int64_t v = 0;
                if (r.has(counters[k]) && !PyInt(r.get(counters[k]), v)) continue;
                stats[counters[k]] = Json((long long)(stats.get(counters[k]).asInt() + std::max<int64_t>(0, v)));
            }
            MergeLifetime(lifetime, r);
            MergeProgression(prog, r);
        }
    }
    RankupClaim rankup;
    std::string note = ProfileAddXp(s, uid, amount, reason, name, stats, lifetime, prog, &rankup);
    /* The Python lobby tests the WHOLE note for these words, progression summary included, so a report
     * whose progression names an unusable collectible / coop mission is "failed", retained, and its XP
     * credited again on every retry. The offline lobby tests the credit's own text only; the
     * differential tests reproduce the Python test. */
    std::string verdict = note;
    size_t cut = note.find("  [progression");
    if (!s.cfg.differential && cut != std::string::npos) verdict.erase(cut);
    if (verdict.empty() || Has(verdict, "FAILED") || Has(verdict, "ERROR") || Has(verdict, "unusable")) {
        desc = note.empty() ? Fmt("profile %llu: nothing credited", (unsigned long long)uid) : note;
        return false;
    }
    /* Re-render the blob the client reads now, so a failure is visible in THIS note... */
    std::string rn = EnsureBlob(s, uid);
    if (!rn.empty() && (Has(rn, "FAILED") || Has(rn, "ERROR") || Has(rn, "cannot read"))) {
        desc = note + " | " + rn;
        return false;
    }
    /* ...and hand the match server its copy: with a map rotation nobody returns to the front end
     * between matches, and the next map attaches mpdata_s.<uid> from the match folder. (The Python
     * lobby renders straight into the folder the match server reads.) */
    std::string ex = ExportMatchBlob(s, uid);
    if (Has(ex, "FAILED") || Has(ex, "no mpdata_s")) note += " | " + ex;
    /* GP rides the same target, after the XP has landed and never as a reason to retry. RANK-UP GP
     * has ONE authority, the profile's level mark: the server's own figure is taken out of "gp" and
     * the claim of this credit put in its place. */
    int64_t gp = std::max<int64_t>(0, prog.get("gp").asInt() - prog.get("gp_rankups").asInt());
    std::string gpNote = CreditGp(s, uid, gp + rankup.gp);
    if (!gpNote.empty()) {
        if (rankup.gp || prog.get("gp_rankups").truthy())
            gpNote += Fmt(" [rank-up GP %lld for levels %d..%d; server reported %lld]", (long long)rankup.gp, rankup.from, rankup.to,
                          (long long)prog.get("gp_rankups").asInt());
        note += " | " + gpNote;
    }
    desc = note;
    return true;
}

int RankVersion(Server &s)
{
    CareerState &c = State(s);
    if (!c.rankVersionTried) {
        c.rankVersionTried = true;
        Bytes text;
        Json doc;
        if (s.Resource("unlock_tables.proposed.json", text) && Json::Parse(text, doc)) c.rankVersion = (int)doc.get("rankTable_version").asInt(2);
    }
    return c.rankVersion;
}

/* _apply_to: fold `amount` XP into the shared (unkeyed) mpdata_s - the target of a report whose rows
 * name no account, asked for by a connection that has none either (rank_service.RankState). */
bool ApplyToShared(Server &s, int64_t amount, std::string &desc)
{
    Store &store = GetStore(s);
    Bytes src;
    PdBlob blob;
    const PdField *rank, *xp;
    if ((!store.GetStorageFile(0, kSharedBlob, src) && !s.Resource(kSeedBlob, src)) || !RankCount(s) || !blob.Load(s, src) ||
        blob.def->key != kSharedBlob || !(rank = blob.def->Find("rank")) || !(xp = blob.def->Find("experience"))) {
        desc = Fmt("could not read %s", kSharedBlob);
        return false;
    }
    int64_t was = RankToXp(s, blob.GetInt(*rank), blob.GetInt(*xp)), total = std::max<int64_t>(0, was + amount), in;
    int idx;
    Bytes out;
    if (!XpToRank(s, total, idx, in) || !blob.SetPath("rank", idx + 1) || !blob.SetPath("experience", (double)in) ||
        !blob.SetPath("rankVersion", RankVersion(s)) || !blob.Store(out) || !store.PutStorageFile(0, kSharedBlob, out, "writeback")) {
        desc = Fmt("save of %s failed", kSharedBlob);
        return false;
    }
    desc = Fmt("%s +%lld XP level %d->%d (%lld into rank, %lld to next)", kSharedBlob, (long long)amount, LevelForXp(s, was), idx + 1,
               (long long)in, (long long)std::max<int64_t>(0, RankXpToNext(s, idx) - in));
    return true;
}

struct Target {
    std::string key;    /* the retry-state key: "profile:<uid>" or "shared" */
    uint64_t uid;       /* 0 = the shared blob */
    int64_t amount;
    const Entry *entry; /* NULL = the legacy aggregate */
};

/* -> "" when nothing happened to this report (left for the connection that knows whose it is). */
std::string Consume(Server &s, const std::string &dir, const std::string &report, uint64_t connUid, bool background)
{
    Store &store = GetStore(s);
    Bytes raw;
    Json rep;
    std::string err, perr;
    if (!ReadFile(report, raw)) err = "the report cannot be read";
    else if (raw.size() >= 3 && !memcmp(raw.data(), "\xEF\xBB\xBF", 3)) err = "Unexpected UTF-8 BOM";
    else if (!Utf8Ok(raw)) err = "the report is not UTF-8";
    else if (!Json::Parse(raw, rep, &perr)) err = perr.empty() ? "not JSON" : perr;
    else err = RwbValidate(rep);
    if (!err.empty()) {
        std::string arch = Archive(s, dir, report, "bad");
        std::string note = "rank_writeback: BAD REPORT (" + err + ") -> archived " + (arch.empty() ? "FAILED-TO-ARCHIVE" : arch) + ", NOT applied";
        Journal(s, dir, note);
        return note;
    }

    PerUid perUid;
    PerUidAmounts(rep, perUid);
    /* No row names its account, so only the connection that asks for its profile can say whose it is. */
    if (background && perUid.empty()) return "";

    const std::string rid = Sha1Hex(raw);
    Json state = LoadState(s);
    const bool sameReport = state.get("report_id").str() == rid;
    Json applied = sameReport && state.get("applied").isObject() ? state.get("applied") : Json::MakeObject();
    int64_t delta, full;
    std::string names;
    Amounts(rep, delta, full, names);
    const std::string reasonText = rep.has("reason") ? PyStr(rep.get("reason")) : "?";

    /* THE LEDGER. The Python lobby records the report here and only MENTIONS a repeat: its duplicate
     * test is the retry state alone, which remembers one report, so re-dropping an older report after
     * a newer one credits it a second time. The offline lobby refuses any report the ledger has seen
     * unless it is the one being retried; the differential tests reproduce the Python behaviour. */
    const bool firstSeen = !store.HasReport(rid);
    if (!s.cfg.differential) {
        if (!firstSeen && !sameReport) {
            std::string arch = Archive(s, dir, report, "dup");
            std::string note = "rank_writeback: duplicate report, already applied (report " + reasonText + ", players " + names + ") -> " +
                               (arch.empty() ? "archive FAILED" : arch) + "  [ledger: report already recorded]";
            Journal(s, dir, note);
            return note;
        }
        /* state first, ledger second: a stop between the two can only leave a report that is retried */
        if (firstSeen) SaveState(s, rid, applied);
    }
    store.RecordMatchReport(rid, rep, false);

    /* WITH ROW UIDS: one target per distinct row uid, each credited its OWN row; the connection's uid
     * is not consulted. WITH NONE but a connection uid: that profile takes the aggregate. WITH
     * NEITHER: the shared blob takes the delta the live pump did not already persist. */
    std::vector<Target> targets;
    if (!perUid.empty()) {
        for (PerUid::iterator it = perUid.begin(); it != perUid.end(); ++it) {
            Target t = { "profile:" + it->first, it->second.uid, it->second.xp, &it->second };
            targets.push_back(t);
        }
    } else if (connUid) {
        Target t = { Fmt("profile:%llu", (unsigned long long)connUid), connUid, full, NULL };
        targets.push_back(t);
    } else {
        Target t = { "shared", 0, delta, NULL };
        targets.push_back(t);
    }

    /* A ZERO-XP MATCH IS STILL A MATCH: a profile target with lifetime deltas or progression is worth
     * applying at +0 XP. The shared blob is not: it holds only the rank triple. */
    Json aggLifetime = Json::MakeObject(), aggProgression = Json::MakeObject();
    const std::vector<Json> &rows = rep.get("players").items();
    for (size_t i = 0; i < rows.size(); i++) {
        if (rows[i].get("bot").truthy()) continue;
        MergeLifetime(aggLifetime, rows[i]);
        MergeProgression(aggProgression, rows[i]);
    }
    std::vector<char> worth(targets.size(), 0);
    bool any = false;
    for (size_t i = 0; i < targets.size(); i++) {
        const Target &t = targets[i];
        if (t.amount > 0) worth[i] = 1;
        else if (!t.uid) worth[i] = 0;
        else if (t.entry) worth[i] = t.entry->lifetime.truthy() || HasProgression(t.entry->progression);
        else worth[i] = aggLifetime.truthy() || HasProgression(aggProgression);
        any = any || worth[i];
    }
    if (!any) {
        std::string arch = Archive(s, dir, report, "applied");
        SaveState(s, rid, Json::MakeObject());
        std::string note = "rank_writeback: consumed report (" + reasonText + "; players " + names +
                           "), nothing to add (applied_live covers it) -> " + (arch.empty() ? "archive FAILED" : arch);
        Journal(s, dir, note);
        return note;
    }

    std::string descs;
    size_t done = 0;
    bool allOk = true;
    for (size_t i = 0; i < targets.size(); i++) {
        const Target &t = targets[i];
        if (applied.get(t.key).truthy() || !worth[i]) continue;
        std::string desc;
        bool ok = t.uid ? ApplyToProfile(s, t.uid, t.amount, "match_report " + reasonText, rep, t.entry, desc) : ApplyToShared(s, t.amount, desc);
        if (ok) {
            applied[t.key] = true; /* per target: a retry after a partial failure cannot pay a uid twice */
            SaveState(s, rid, applied);
        } else {
            allOk = false;
            desc = "FAILED: " + desc;
        }
        descs += (done++ ? "; " : "") + desc;
    }
    if (!allOk) {
        std::string note = "rank_writeback: " + descs + " -- report RETAINED for retry";
        Journal(s, dir, note);
        return note;
    }

    std::string arch = Archive(s, dir, report, done ? "applied" : "dup");
    if (done) {
        Json credited = Json::MakeObject(); /* {slot: the row's XP total} */
        for (size_t i = 0; i < rows.size(); i++) {
            if (rows[i].get("bot").truthy()) continue;
            bool ok;
            credited[rows[i].has("slot") ? PyStr(rows[i].get("slot")) : Fmt("%u", (unsigned)i)] = Json((long long)RowTotal(rows[i], "total", ok));
        }
        store.MarkReportApplied(rid, credited);
    }
    std::string note = "rank_writeback: " + (done ? descs : std::string("duplicate report, already applied")) + " (report " + reasonText +
                       ", players " + names + ") -> " + (arch.empty() ? "archive FAILED" : arch) +
                       (firstSeen ? "" : "  [ledger: report already recorded]");
    Journal(s, dir, note);
    return note;
}

} // namespace

std::string RwbReportId(const Bytes &raw) { return Sha1Hex(raw); }

/* Shape check for a format-1 report. The `lifetime` member names and the progression refs are OPEN:
 * a member the match server learns to measure tomorrow must not make today's lobby archive a whole
 * match as .bad. */
std::string RwbValidate(const Json &rep)
{
    if (!rep.isObject()) return "not a JSON object";
    const Json &fmt = rep.get("format");
    if (!((fmt.isBool() || fmt.isNumber()) && fmt.asReal() == 1.0)) return "format " + fmt.dump() + " is not 1";
    const Json &rows = rep.get("players");
    if (!rows.isArray() || !rows.size()) return "players is not a non-empty list";
    for (size_t i = 0; i < rows.size(); i++) {
        const Json &r = rows.at(i);
        const unsigned n = (unsigned)i;
        if (!r.isObject()) return Fmt("players[%u] is not an object", n);
        static const char *const need[] = { "slot", "name", "bot" };
        for (int k = 0; k < 3; k++)
            if (!r.has(need[k])) return Fmt("players[%u] lacks '%s'", n, need[k]);
        /* OPTIONAL: only a uid of the wrong TYPE is malformed; one that cannot be keyed is a routing question */
        if (r.has("uid") && !r.get("uid").isString() && !r.get("uid").isInt() && !r.get("uid").isBool())
            return Fmt("players[%u].uid is not a string or int", n);
        if (r.get("bot").truthy()) continue;
        const Json &xp = r.get("xp");
        if (!xp.isNull()) {
            if (!xp.isObject()) return Fmt("players[%u].xp is not an object", n);
            static const char *const fields[] = { "total", "applied_live" };
            for (int k = 0; k < 2; k++) {
                int64_t v;
                if (xp.has(fields[k]) && !PyInt(xp.get(fields[k]), v)) return Fmt("players[%u].xp.%s is not an int", n, fields[k]);
            }
        }
        const Json &lt = r.get("lifetime");
        if (!lt.isNull()) {
            if (!lt.isObject()) return Fmt("players[%u].lifetime is not an object", n);
            const Json::Members &m = lt.members();
            for (size_t k = 0; k < m.size(); k++)
                if (!m[k].second.isInt()) return Fmt("players[%u].lifetime.%s is not an int", n, m[k].first.c_str());
        }
    }
    return "";
}

/* maybe_apply: consume every waiting report, oldest first. A report RETAINED for retry stops the
 * walk: the retry state remembers one report at a time. */
std::string RankWritebackMaybeApply(Server &s, uint64_t uid, bool background)
{
    const std::string &dir = s.cfg.matchProfile;
    if (dir.empty()) return "";
    std::vector<std::string> reports;
    std::string legacy = PathJoin(dir, kReportName);
    if (FileExists(legacy)) reports.push_back(legacy);
    std::vector<std::string> names = ListDir(dir), spool;
    const size_t plen = strlen(kSpoolPrefix);
    for (size_t i = 0; i < names.size(); i++) {
        const std::string &n = names[i];
        if (n.size() >= plen + 5 && !n.compare(0, plen, kSpoolPrefix) && !n.compare(n.size() - 5, 5, ".json")) spool.push_back(n);
    }
    std::sort(spool.begin(), spool.end()); /* the names sort in write order; "<name>.tmp" never matches */
    for (size_t i = 0; i < spool.size(); i++) reports.push_back(PathJoin(dir, spool[i]));
    std::string out;
    for (size_t i = 0; i < reports.size(); i++) {
        std::string note = Consume(s, dir, reports[i], uid, background);
        if (note.empty()) continue;
        out += (out.empty() ? "" : " || ") + note;
        if (Has(note, "RETAINED")) break;
    }
    return out;
}

/* The match-report poller of dw-capture.py's main loop: with a map rotation nobody returns to the
 * front end between matches, so reports are folded within a second of the write, well before the
 * next map attaches the player's blob. */
void ServiceTick(Server &s)
{
    CareerState &c = State(s);
    double now = s.host ? s.host->Mono() : 0;
    if (c.polled && s.host && now - c.lastPoll < 1.0) return;
    c.polled = true;
    c.lastPoll = now;
    std::string fold = RankWritebackMaybeApply(s, 0, true);
    if (!fold.empty()) s.Log("=== match report poller: " + fold + " ===");
}

} // namespace dw

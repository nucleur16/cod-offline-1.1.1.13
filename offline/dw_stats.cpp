/*
 * dw_stats.cpp - BD_STATS_SERVICE (4): the leaderboards (career/stats_service.py).
 *
 * MEASURED off C1.exe (vftable 0xC81F0C): a reply record is one bare LeaderBoardRow<10>, sizeof 0x98,
 * read by bdStatsInfo::deserialize 0xA48F00 + LeaderBoardRow::deserialize 0x7276A0 as 15 tagged fields:
 *   UInt64 entityID (+0x08; the "which row is me" search at 0x726ED0 compares it with the local id)
 *   Int64  score    (+0x18; tag 9)
 *   UInt64 rank     (+0x20; the visible rank; 0 = "row not present"; row[0].rank-1 is the scroll offset)
 *   String name     (+0x28; maxLen 64)
 *   UInt32 numColumns (+0x6C)
 *   Int32 x 10      (+0x70; tag 7, SIGNED - a wrong tag desyncs every following row of the reply)
 * The client only ever sends task 4 readStatsByRank [UInt32 lb][UInt64 startRank][UInt32 numRows] and
 * task 3 readStatsByEntityID [UInt32 lb][UInt64 entityId] (always its own id). The row array is a
 * static 100 elements: never more than 100 records.
 *
 * The local player's row is real (entity id, name, experience out of the mpdata_s the lobby serves
 * that account); every other row is synthetic and deterministic in (leaderboard id, rank), because the
 * client re-requests the same page and keys its scroll and its self-search off the values.
 */
#include <algorithm>
#include "dw.h"
#include "dw_profiles.h"
#include "dw_state.h"
#include "dw_store.h"

namespace dw {

namespace {

const int kMaxRows = 100;       /* the client's static row array */
const int kNumColumns = 10;
const int kNameMaxLen = 64;     /* `push 40h` on the name reader at 0xA48F60 */
/* Identity of a connection with no latched uid / a profile without a name. The first pair is the
 * Python lobby's built-in default (career/stats_service.py), reproduced only for the differential
 * tests; the product uses a neutral one. */
const uint64_t kPyDefaultEntity = 9095494740303931418ULL;
const char *const kPyDefaultName = "Chud";
const uint64_t kDefaultEntity = 1;
const char *const kDefaultName = "Player";
const int64_t kDefaultExperience = 3582550;
const uint64_t kMyRank = 7;     /* where the local player sits; identical for task 3 and task 4 */
const uint64_t kLadderSize = 1000;
const size_t kOffExperience = 0x1130, kOffRank = 0x13EC; /* mp/playersecuredata.def v11 */

/* Small deterministic hash over 64-bit values (stats_service._mix): FNV-1a over each value's
 * significant bytes, with a separator step. */
struct Mix {
    uint32_t h;
    Mix() : h(0x811C9DC5u) {}
    Mix &add(uint64_t v)
    {
        for (;;) {
            h = (h ^ (uint32_t)(v & 0xFF)) * 0x01000193u;
            v >>= 8;
            if (!v) break;
        }
        h = (h ^ 0xA5u) * 0x01000193u;
        return *this;
    }
};

std::string FillerName(uint64_t lb, uint64_t rank)
{
    static const char *const first[] = { "Reaper", "Ghost", "Viper", "Nomad", "Frost", "Wraith", "Talon", "Ronin", "Cobra", "Hawk",
                                         "Onyx", "Zero", "Vector", "Havoc", "Saber", "Delta", "Kilo", "Bravo", "Sierra", "Echo" };
    static const char *const suffix[] = { "", "_CN", "77", "X", "_01", "Pro", "_MVP", "99", "_TX", "Z" };
    uint32_t h = Mix().add(lb).add(rank).add(0x4E).h;
    return std::string(first[h % 20]) + suffix[(h >> 8) % 10];
}

/* Ten Int32s that read like an MP stat line and fall off with rank. The column MEANINGS per
 * leaderboard are not known (the labels are guesses); the types are measured. */
void ColumnsFor(uint64_t lb, uint64_t rank, bool haveXp, int64_t experience, int64_t cols[10])
{
    uint32_t h = Mix().add(lb).add(rank).h;
    int64_t jitter = h % 97;
    int64_t scale = std::max<int64_t>(1, 2000 - (int64_t)rank * 17);
    int64_t kills = scale * 3 + jitter;
    /* Python: int(kills * 0.62). In integers, because the 32-bit build's x87 product truncates one
     * lower where the double product rounds up to a whole number; kills * 62 / 100 equals the double
     * result for every kills below 200000 (checked exhaustively; kills never exceeds 6045). */
    int64_t deaths = std::max<int64_t>(1, kills * 62 / 100 + (h >> 7) % 40);
    int64_t xp = haveXp ? experience : scale * 900 + jitter * 31;
    int64_t wins = std::max<int64_t>(0, scale / 9 + (h >> 11) % 15);
    int64_t losses = std::max<int64_t>(0, wins / 2 + (h >> 13) % 11);
    cols[0] = xp & 0x7FFFFFFF;                                  /* experience / score */
    cols[1] = kills;
    cols[2] = deaths;
    cols[3] = std::max<int64_t>(0, kills - deaths);
    cols[4] = wins;
    cols[5] = losses;
    cols[6] = std::max<int64_t>(1, kills / 7 + (h >> 3) % 9);   /* best killstreak */
    cols[7] = kills / 5 + (h >> 5) % 60;                        /* headshots */
    cols[8] = scale * 41 + jitter * 7;                          /* time played */
    cols[9] = std::max<int64_t>(1, (kills * 100) / std::max<int64_t>(1, deaths)); /* K/D x100 */
}

/* A stable, obviously synthetic id that cannot collide with a real player's. */
uint64_t EntityFor(uint64_t lb, uint64_t rank)
{
    uint64_t h = Mix().add(lb).add(rank).add(0xE1).h;
    return (0x1000000000000000ULL | (h << 16) | (rank & 0xFFFF)) & 0x7FFFFFFFFFFFFFFFULL;
}

Bytes Row(uint64_t entity, uint64_t rank, const Bytes &name, const int64_t cols[10], int64_t score)
{
    Bytes out = w_u64(entity) + w_i64(score) + w_u64(rank) + w_str(name, kNameMaxLen) + w_u32(kNumColumns);
    for (int i = 0; i < kNumColumns; i++) out += w_i32((int32_t)cols[i]);
    return out;
}

/* str.encode("latin1", "replace") of the profile's (UTF-8) name: what the client's 8-bit name field
 * can carry, '?' for the rest. */
Bytes Latin1(const std::string &utf8)
{
    Bytes out;
    for (size_t i = 0; i < utf8.size();) {
        unsigned c = (unsigned char)utf8[i];
        int more = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        if (more < 0 || i + (size_t)more >= utf8.size()) { out += '?'; i++; continue; } /* not UTF-8: one '?' per byte */
        unsigned cp = more == 0 ? c : more == 1 ? c & 0x1F : more == 2 ? c & 0x0F : c & 0x07;
        for (int k = 1; k <= more; k++) cp = cp << 6 | ((unsigned char)utf8[i + (size_t)k] & 0x3F);
        out += cp < 0x100 ? (char)cp : '?';
        i += (size_t)more + 1;
    }
    return out;
}

uint32_t U32At(const Bytes &b, size_t off)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)(unsigned char)b[off + (size_t)i] << (8 * i);
    return v;
}

/* read_mpdata_s: the experience out of the mpdata_s the lobby serves THIS uid - its own rendered
 * blob, else the shared one, else the unlock-all seed - read at the DDL's measured offsets. */
int64_t ServedExperience(Server &s, uint64_t uid)
{
    Store &store = GetStore(s);
    for (int cand = 0; cand < 3; cand++) {
        Bytes data;
        bool have = cand == 0 ? uid && store.GetStorageFile(uid, "mpdata_s", data)
                  : cand == 1 ? store.GetStorageFile(0, "mpdata_s", data) || s.Resource("mpdata_s", data)
                              : s.Resource("mpdata_s.unlockall.proposed", data);
        if (!have) continue;
        if (!data.empty() && data[0] == 0x78) { /* the stored blob is a raw zlib stream */
            Bytes raw;
            if (!Inflate(data, raw)) continue;
            data.swap(raw);
        }
        if (data.size() < kOffRank + 4) continue;
        uint32_t rank = U32At(data, kOffRank), exp = U32At(data, kOffExperience);
        if (rank <= 255 && exp <= 0x7FFFFFFFu) return exp; /* a bad offset must not put nonsense on the board */
    }
    return kDefaultExperience;
}

/* local_identity: the logged-in account - its own id, its recorded name, its served experience. */
struct Identity {
    uint64_t entity;
    Bytes name;
    int64_t experience;
};

Identity LocalIdentity(Server &s, uint64_t uid)
{
    Identity me;
    me.entity = uid ? uid : (s.cfg.differential ? kPyDefaultEntity : kDefaultEntity);
    me.name = s.cfg.differential ? kPyDefaultName : kDefaultName;
    me.experience = ServedExperience(s, uid);
    Json prof;
    if (uid && ProfileLoad(s, uid, prof, false) && prof.get("name").truthy())
        me.name = Latin1(prof.get("name").isString() ? prof.get("name").asString() : prof.get("name").dump());
    return me;
}

Bytes RowForRank(uint64_t lb, uint64_t rank, const Identity &me)
{
    int64_t cols[10];
    if (rank == kMyRank) {
        ColumnsFor(lb, rank, true, me.experience, cols);
        return Row(me.entity, rank, me.name, cols, me.experience);
    }
    ColumnsFor(lb, rank, false, 0, cols);
    return Row(EntityFor(lb, rank), rank, FillerName(lb, rank), cols, cols[0]);
}

/* The positional integer arguments (stats_service._numbers). */
std::vector<uint64_t> Numbers(const Vals &args)
{
    std::vector<uint64_t> out;
    for (size_t i = 0; i < args.size(); i++)
        if ((args[i].tag >= 1 && args[i].tag <= 10) || args[i].tag == 12) out.push_back(args[i].u);
    return out;
}

} // namespace

Reply StatsTask(Server &s, Conn &c, int task, const Vals &args)
{
    std::vector<uint64_t> nums = Numbers(args);
    if (task == 4) { /* readStatsByRank */
        if (nums.size() < 3) return Reply::Ok(Fmt("readStatsByRank with %u args -- expected 3", (unsigned)nums.size()));
        uint64_t lb = nums[0], start = std::max<uint64_t>(1, nums[1]);
        uint64_t count = std::min<uint64_t>(nums[2], kMaxRows); /* never past the client's 100-element array */
        Identity me = LocalIdentity(s, c.uid);
        Reply r = Reply::Ok();
        size_t bytes = 0;
        for (uint64_t i = 0; i < count && start <= kLadderSize && start + i <= kLadderSize; i++) {
            r.results.push_back(RowForRank(lb, start + i, me));
            bytes += r.results.back().size();
        }
        r.note = Fmt("readStatsByRank lb=%llu ranks %llu..%llu -> %u rows (%u B)%s", (unsigned long long)lb, (unsigned long long)start,
                     (unsigned long long)(start + count - 1), (unsigned)r.results.size(), (unsigned)bytes,
                     r.results.empty() ? " [start rank past end of ladder]" : "");
        return r;
    }
    if (task == 3) { /* readStatsByEntityID: "give me MY row" */
        if (nums.size() < 2) return Reply::Ok(Fmt("readStatsByEntityID with %u args -- expected 2", (unsigned)nums.size()));
        uint64_t lb = nums[0], entity = nums[1];
        Identity me = LocalIdentity(s, c.uid);
        int64_t cols[10];
        Reply r = Reply::Ok();
        if (entity == me.entity) {
            ColumnsFor(lb, kMyRank, true, me.experience, cols);
            r.results.push_back(Row(me.entity, kMyRank, me.name, cols, me.experience));
            r.note = Fmt("readStatsByEntityID lb=%llu self(%llu) -> rank %llu", (unsigned long long)lb, (unsigned long long)entity,
                         (unsigned long long)kMyRank);
            return r;
        }
        /* Someone else's row: a stable synthetic entry rather than an error, so the client's "not
         * globally ranked" fallback stays reserved for genuinely absent players. */
        uint64_t rank = 1 + Mix().add(lb).add(entity).h % kLadderSize;
        ColumnsFor(lb, rank, false, 0, cols);
        r.results.push_back(Row(entity, rank, FillerName(lb, rank), cols, cols[0]));
        r.note = Fmt("readStatsByEntityID lb=%llu other(%llu) -> rank %llu", (unsigned long long)lb, (unsigned long long)entity,
                     (unsigned long long)rank);
        return r;
    }
    return Reply();
}

} // namespace dw

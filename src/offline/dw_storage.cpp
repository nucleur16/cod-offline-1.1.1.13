/*
 * dw_storage.cpp - BD_STORAGE_SERVICE, service 10 (demonware/dw_storage.py) and the account row of
 * the uid latch (dw-capture.py build_task_reply).
 *
 * The per-player file channel: UPLOAD (1), GETFILE (3), LISTALLPUBFILES (6) and GETPUBFILE (7). It is
 * how the client fetches its own mpdata / mpdata_s profile and the playlist / FFOTD public files, and
 * how it uploads its classes blob back. Two things here are load-bearing beyond "reading a file":
 *   - GETFILE of mpdata_s is the fold point for progression: it applies any pending match report and
 *     brings the player's mpdata_s file up to the career profile before serving it;
 *   - the uid on a GETFILE is where the server learns who it is talking to (the latch in dw_core.cpp).
 *
 * Where the Python lobby keeps per-uid files as resources\<name>.<uid> and SQLite rows, they live in
 * the Store here; "the file resources\<name>" is the store's shared copy, else the served resource.
 *
 * WIRE, MEASURED: GETFILE is [String name][UInt64 ownerId][NoType]; UPLOAD is
 * [String name][Bool][Blob][UInt64(0)][NoType] - its owner field is always ZERO, so uploads rely on
 * the uid latched from the connection's earlier GETFILEs. A GETFILE / GETPUBFILE result is one blob.
 */
#include <string.h>
#include "dw.h"
#include "dw_playerdata.h"
#include "dw_profiles.h"
#include "dw_state.h"
#include "dw_store.h"

namespace dw {

namespace {

const char *const kFfotdName = "ffotd_client.ff";
const char *const kBroadcastName = "broadcast.info"; /* the lobby ticker: pubfile id 4, C1.exe 0x0058F630 */
const char *const kUploadsSubdir = "uploads";

/* HD-3a: the names the CLIENT may write into the profile family. MEASURED: the only names this
 * client ever UPLOADs are "mpdata" and "Recentmetlist"; "mpdata_s" and "blacklist" are the other two
 * members of the per-player family. Any other name is kept, but diverted to uploads/. */
bool ClientUploadName(const std::string &n) { return n == "mpdata" || n == "mpdata_s" || n == "Recentmetlist" || n == "blacklist"; }

/* THE NEW-ACCOUNT SEEDS (paths.MPDATA_SEED / MPDATA_S_SEED): both spell a LEVEL-1 account. */
const char *SeedFor(const std::string &n)
{
    if (n == "mpdata") return "mpdata.level1.proposed";
    if (n == "mpdata_s") return "mpdata_s.level1.proposed";
    return NULL;
}

/* os.path.basename on Windows: after the last separator, and past a drive letter. */
std::string Basename(const std::string &name)
{
    size_t cut = name.find_last_of("/\\");
    std::string base = cut == std::string::npos ? name : name.substr(cut + 1);
    if (cut == std::string::npos && base.size() >= 2 && base[1] == ':') base = base.substr(2);
    return base;
}

bool EndsWith(const std::string &s, const char *suffix)
{
    size_t n = strlen(suffix);
    return s.size() >= n && !s.compare(s.size() - n, n, suffix);
}

/* What the client actually fetches is the dot-less profile family (per-uid resolution happens
 * server-side) plus *.info playlists/configs, ffotd_client.ff and *.txt tables. Anything else - the
 * JSON tables, other players' blobs - is refused. */
bool ServableName(const std::string &name)
{
    if (name.empty() || Basename(name) != name || name[0] == '.') return false;
    return name.find('.') == std::string::npos || EndsWith(name, ".info") || EndsWith(name, ".ff") || EndsWith(name, ".txt");
}

/* A zero-length or malformed FFOTD *buffer* is FATAL on this client (WIN_ZONE_ERROR -> int3 at
 * 0x004CD3A5), whereas no file (BD_NO_FILE) is a clean no-op (0x004CD3B6). Only the container envelope
 * is checked: magic + version + minimum length (header 0x15 + 0x100 RSA block). */
bool FfotdStructuralOk(const Bytes &data)
{
    if (data.size() < 0x115) return false;
    if (memcmp(data.data(), "IWffu100", 8) && memcmp(data.data(), "IWff0100", 8)) return false;
    return PdU32(data, 8) == 0x14E; /* cmp eax,0x14E @0x004B5E3B */
}

/* "resources\<name> is a file": the store's shared copy (an upload from a connection without a uid),
 * else the served resource. */
bool SharedExists(Server &s, const std::string &name)
{
    Bytes tmp;
    return GetStore(s).GetStorageFile(0, name, tmp) || s.ResourceExists(name);
}

bool SharedRead(Server &s, const std::string &name, Bytes &out)
{
    return GetStore(s).GetStorageFile(0, name, out) || s.Resource(name, out);
}

/* ---- the serve-time mpdata filters ---- */

/* The hea identity of one characterClasses row, or "". Found by ITEM TYPE rather than by slot
 * position: the slot order has been seen in two permutations, and only one group is the identity. */
std::string RowIdentity(const unsigned *rec, const std::map<int, PdItemName> &names)
{
    for (int i = 0; i < 20; i++) {
        if (!rec[i] || rec[i] == 0xFFFF) continue;
        std::map<int, PdItemName>::const_iterator it = names.find((int)rec[i]);
        if (it == names.end() || it->second.name.empty()) continue;
        std::string nm = it->second.name;
        if (!nm.compare(0, 3, "co_")) nm = nm.substr(3);
        if (!nm.compare(0, 4, "hea_")) return nm;
    }
    return "";
}

void ReadRow(const Bytes &buf, int row, unsigned *rec)
{
    size_t off = (size_t)(PD_CHARACTER_CLASSES_OFF + PD_CHARACTER_CLASSES_STRIDE * row);
    for (int i = 0; i < 20; i++) rec[i] = PdU16(buf, off + 2 * (size_t)i);
}

void WriteRow(Bytes &buf, int row, const unsigned *rec)
{
    size_t off = (size_t)(PD_CHARACTER_CLASSES_OFF + PD_CHARACTER_CLASSES_STRIDE * row);
    for (int i = 0; i < 20; i++) PdPutU16(buf, off + 2 * (size_t)i, rec ? rec[i] : 0);
}

bool RowEmpty(const unsigned *rec)
{
    for (int i = 0; i < 20; i++) if (rec[i]) return false;
    return true;
}

/* mp/cyborgclasstable.csv: the Zombies roster is four characters (catalogue/zm_characters.py). */
bool IsCoreZm(const std::string &hea)
{
    return hea == "hea_zmlubushield" || hea == "hea_zmcowboy" || hea == "hea_zmpunk" || hea == "hea_zmgirl";
}

/* THE ZOMBIES ROSTER (zm_characters.filter_profile), applied to an mpdata on its way to the client.
 * The Zombies character picker is built out of the profile's own characterClasses records, so every
 * zombie-faction record the profile carries shows up in it. Every zombie record that is not one of
 * the four roster characters is cleared, duplicates collapse to the first, zmCharacterIndex is
 * repointed at a survivor and a profile with none gets the first roster character planted.
 * Multiplayer records are untouched: the array is fixed-stride, so clearing shifts nothing.
 * Applied on the SERVE, not on the upload: the client re-uploads its own mpdata whenever it edits a
 * character. -> true when the buffer changed (CRC already fixed). */
bool ZmFilterProfile(Server &s, Bytes &buf, std::string &notes)
{
    const std::map<int, PdItemName> &names = PdItemNames(s);
    std::map<std::string, int> seenCore;
    std::vector<int> surviving;
    std::vector<std::string> list;
    unsigned rec[20];
    for (int row = 0; row < PD_CHARACTER_CLASSES_COUNT; row++) {
        ReadRow(buf, row, rec);
        if (RowEmpty(rec)) continue;
        std::string hea = RowIdentity(rec, names);
        /* the def table is the authority; the name shape catches hea_zmtightgirl, which has parts but no row */
        if (hea.compare(0, 6, "hea_zm")) continue; /* an mp character, or not a character */
        if (!IsCoreZm(hea)) {
            WriteRow(buf, row, NULL);
            list.push_back(Fmt("cleared characterClasses[%d] %s (not a core Zombies character)", row, hea.c_str()));
            continue;
        }
        if (seenCore.count(hea)) {
            WriteRow(buf, row, NULL);
            list.push_back(Fmt("cleared characterClasses[%d] %s (duplicate of [%d])", row, hea.c_str(), seenCore[hea]));
            continue;
        }
        seenCore[hea] = row;
        surviving.push_back(row);
    }
    if (surviving.empty()) {
        /* NOBODY LEFT: AS_HaveZombieCharacter walks this very array looking for one. Plant the first
         * roster character, dressed from its own mp/cocharacterdeftable.csv row, in the lowest unused
         * record: fpa 282, boo 283, fja 281, han 284, hea 279, heg 280 of hea_zmlubushield. */
        for (int row = 0; row < PD_CHARACTER_CLASSES_COUNT; row++) {
            ReadRow(buf, row, rec);
            if (!RowEmpty(rec)) continue;
            static const unsigned planted[20] = { 282, 283, 281, 284, 279, 0, 280 };
            WriteRow(buf, row, planted);
            surviving.push_back(row);
            list.push_back(Fmt("planted characterClasses[%d] hea_zmlubushield (profile had no core Zombies character)", row));
            break;
        }
    }
    unsigned idx = PdU16(buf, PD_ZM_CHARACTER_INDEX_OFF);
    bool named = false;
    for (size_t i = 0; i < surviving.size(); i++) if ((unsigned)surviving[i] == idx) named = true;
    if (!surviving.empty() && !named) {
        PdPutU16(buf, PD_ZM_CHARACTER_INDEX_OFF, (unsigned)surviving[0]);
        ReadRow(buf, surviving[0], rec);
        list.push_back(Fmt("zmCharacterIndex %u -> %d (%s)", idx, surviving[0], RowIdentity(rec, names).c_str()));
    }
    if (list.empty()) return false;
    PdPutU32(buf, 0, Crc32(buf.data() + 4, buf.size() - 4));
    for (size_t i = 0; i < list.size(); i++) notes += (i ? "; " : "") + list[i];
    return true;
}

/* dw_storage._zm_roster: on the stored (zlib) form. A blob that is not an inflatable 32768-byte
 * profile is served untouched - an unfiltered profile beats none. */
std::string ZmRoster(Server &s, const std::string &name, Bytes &data)
{
    if (name != "mpdata" || data.empty()) return "";
    Bytes buf, notes, out;
    if (!Inflate(data, buf) || buf.size() != PD_BUF_SIZE) return " | zm roster: zm roster filter skipped (not a profile)";
    if (!ZmFilterProfile(s, buf, notes)) return "";
    if (!Deflate(buf, out, 1)) return " | zm roster filter skipped (deflate)";
    data.swap(out);
    return " | zm roster: " + notes;
}

/* STORY INTEL (dw_storage._collectibles). The match server reports every intel pickup and the
 * profile keeps the union; the bits themselves live in mpdata, the blob the CLIENT owns and uploads,
 * so they are OR'd in on the serve: an upload that predates the pickup cannot lose them.
 * mp/playerdata.def (v220, 0xE4FD5218) puts collectibles[i] at record bit 31528 + i, i.e.
 * decompressed byte 4 + 3941 + i / 8, bit i % 8; C1's scoreboard intel list reads exactly this bit
 * (C1.exe 0x463b20). A buffer of any other definition is left alone. */
std::string Collectibles(Server &s, const std::string &name, Bytes &data, uint64_t uid)
{
    if (name != "mpdata" || data.empty() || !uid) return "";
    Json prof;
    if (!ProfileLoad(s, uid, prof, false)) return "";
    std::vector<int> ids = ProfileCollectibles(prof);
    if (ids.empty()) return "";
    Bytes buf, out;
    if (!Inflate(data, buf) || buf.size() != PD_BUF_SIZE || PdU32(buf, 4) != 220 || PdU32(buf, 8) != 0xE4FD5218u) return "";
    int added = 0;
    for (size_t i = 0; i < ids.size(); i++) {
        size_t off = (size_t)(4 + 3941 + ids[i] / 8);
        unsigned char bit = (unsigned char)(1u << (ids[i] % 8));
        if ((unsigned char)buf[off] & bit) continue;
        buf[off] = (char)((unsigned char)buf[off] | bit);
        added++;
    }
    if (!added) return "";
    PdPutU32(buf, 0, Crc32(buf.data() + 4, buf.size() - 4));
    if (!Deflate(buf, out, 1)) return "";
    data.swap(out);
    return Fmt(" | intel: set %d collectible bit(s)", added);
}

/* Every serve-time mpdata filter, in order. */
std::string ServeFilters(Server &s, const std::string &name, Bytes &data, uint64_t uid)
{
    std::string note = ZmRoster(s, name, data);
    return note + Collectibles(s, name, data, uid);
}

Reply Served(const Bytes &data, const std::string &note)
{
    Reply r = Reply::Ok(note);
    r.results.push_back(w_blob(data));
    return r;
}

/* One bdFileInfo, EXACTLY in the order bdFileInfo::deserialize @0x00A36440 reads it:
 * fileSize, fileID, createTime, modifiedTime (the FFOTD gate value: != 0 and != -1), visibility,
 * ownerID, fileName[128]. The resource layer has one timestamp, so createTime == modifiedTime (the
 * Python lobby reports the file's own creation time there). */
Bytes PubfileInfoRecord(Server &s, const std::string &name, size_t size)
{
    int64_t mtime = s.ResourceMtime(name);
    if (!mtime) mtime = 1;
    uint32_t fileId = Crc32(name.data(), name.size());
    if (!fileId) fileId = 1;
    return w_u32((uint32_t)size) + w_u64(fileId) + w_u32((uint32_t)mtime) + w_u32((uint32_t)mtime) + w_bool(true) + w_u64(0) +
           w_str(name, 128);
}

/* ---- GETFILE (3) / GETPUBFILE (7) ---- */

Reply GetFile(Server &s, Conn &c, int task, const Val *nameVal)
{
    uint64_t uid = c.uid;
    std::string name = nameVal ? nameVal->s : "";
    if (!nameVal || !ServableName(name)) return Reply(BD_NO_FILE, "\"" + name + "\" is not a servable name -> BD_NO_FILE");
    Store &store = GetStore(s);
    std::string wb;
    Bytes data;
    /* R4: before mpdata_s goes out, fold any waiting match report into the profile, then make sure
     * the player's mpdata_s is that profile, so the blob about to go on the wire is the career,
     * whoever wrote the file last. mpdata too, so a match's story intel is in the profile before
     * Collectibles() sets its bits below. */
    if (task == 3 && (name == "mpdata_s" || name == "mpdata")) {
        std::string n = RankWritebackMaybeApply(s, uid, false);
        if (!n.empty()) wb = n + " | ";
        if (uid) {
            n = EnsureBlob(s, uid);
            if (!n.empty()) wb += n + " | ";
        }
    }
    /* HD-5: the per-uid copy wins over both the shared file and the seed. Task 3 only - GETPUBFILE
     * serves publisher-global data that has no per-player identity. */
    if (task == 3 && uid && store.GetStorageFile(uid, name, data)) {
        std::string zm = ServeFilters(s, name, data, uid);
        return Served(data, Fmt("%sserved \"%s\" per-uid (%llu, %u B)%s", wb.c_str(), name.c_str(), (unsigned long long)uid,
                                (unsigned)data.size(), zm.c_str()));
    }
    bool isFile = SharedExists(s, name);
    const char *seed = SeedFor(name);
    if (!isFile && seed && s.Resource(seed, data)) {
        std::string zm = ServeFilters(s, name, data, uid);
        return Served(data, Fmt("%sSEEDED \"%s\" from %s (%u B)%s", wb.c_str(), name.c_str(), seed, (unsigned)data.size(), zm.c_str()));
    }
    if (name == kFfotdName && isFile) {
        SharedRead(s, name, data);
        if (!FfotdStructuralOk(data)) /* refuse rather than brick the client */
            return Reply(BD_NO_FILE, Fmt("\"%s\" present but NOT a valid FFOTD container (%u B) -> BD_NO_FILE (client skips it safely)",
                                         name.c_str(), (unsigned)data.size()));
        return Served(data, Fmt("%sserved FFOTD \"%s\" (%u B) -> pre-match menu unlock", wb.c_str(), name.c_str(), (unsigned)data.size()));
    }
    /* LB-TICKER: the Python lobby renders its operator notices table into broadcast.info here. The
     * offline lobby has no notices table, which is the Python lobby's "no live notice" arm: the
     * file on disk is served as it is. */
    if (task == 7 && name == kBroadcastName) wb = "ticker: no live notice | " + wb;
    if (isFile && SharedRead(s, name, data)) {
        std::string zm = ServeFilters(s, name, data, uid);
        return Served(data, Fmt("%sserved \"%s\" (%u B)%s", wb.c_str(), name.c_str(), (unsigned)data.size(), zm.c_str()));
    }
    return Reply(BD_NO_FILE, "\"" + name + "\" not in resources/ -> BD_NO_FILE");
}

/* ---- UPLOAD (1) ---- */

Reply Upload(Server &s, Conn &c, const Val *nameVal, const Val *blob)
{
    uint64_t uid = c.uid;
    /* HD-3c: a real error, not a fake success - an upload that stored nothing must not be
     * acknowledged as stored. BD_NO_FILE is the ONLY storage error this client is MEASURED to tolerate. */
    if (!nameVal || nameVal->s.empty() || !blob)
        return Reply(BD_NO_FILE, Fmt("upload with no %s -> BD_NO_FILE (refused, nothing stored)", (!nameVal || nameVal->s.empty()) ? "name" : "blob"));
    std::string base = Basename(nameVal->s), where;
    Store &store = GetStore(s);
    bool ok;
    if (ClientUploadName(base)) {
        /* HD-5: key the profile per-user; without a latched uid it is the shared copy. */
        where = uid ? Fmt("\"%s.%llu\"", base.c_str(), (unsigned long long)uid) : "\"" + base + "\"";
        ok = store.PutStorageFile(uid, base, blob->s, "upload");
    } else {
        /* HD-3a: not a name this client is known to write - keep it, but out of the served names. */
        where = std::string(kUploadsSubdir) + "/" + base + " (name not in allow-list, diverted)";
        ok = !base.empty() && store.PutStorageFile(0, std::string(kUploadsSubdir) + "/" + base, blob->s, "upload");
    }
    if (!ok) return Reply(BD_NO_FILE, "upload " + where + " FAILED to write -> BD_NO_FILE");
    /* An UPLOADED mpdata_s outranks the render: absorb takes the HIGHER of the two into the profile
     * and never demotes it. */
    std::string absorbed;
    if (base == "mpdata_s" && uid) {
        absorbed = AbsorbBlob(s, uid);
        if (!absorbed.empty()) absorbed = " | " + absorbed;
    }
    return Reply::Ok(Fmt("stored %s (%u B)%s", where.c_str(), (unsigned)blob->s.size(), absorbed.c_str()));
}

/* ---- LISTALLPUBFILES (6) = bdStorage::listAllPublisherFiles @ C1.exe 0x00A44DA0 ----
 * Args MEASURED [UInt32(0) startIndex, UInt16(32) maxResults, UInt16(0)].
 *
 * THIS LIST IS THE FFOTD GATE (MEASURED in C1.exe): DB_TryLoadXFileInternal @0x004CD260 bails with
 * "FFOTD is not available." unless pubfile slot 3's +0x48 is != 0 and != -1, and that field is written
 * ONLY by the task-6 result walker 0x0071D090 / 0x00592950 from the listed record's modifiedTime (a
 * name missing from the list stores -1). modifiedTime must be STABLE across polls: the 10-minute
 * re-poll at 0x0071D1F0 treats a larger value as "file updated" and re-fetches. broadcast.info is
 * listed too, which is what makes the ticker refresh without a relog. */
Reply ListAllPubFiles(Server &s, const Vals &args)
{
    const Val *n = First(args, T_UINT16);
    static const char *const names[] = { kFfotdName, kBroadcastName };
    Reply r = Reply::Ok();
    std::string listed;
    bool ffotd = false;
    size_t maxr = n ? (size_t)n->u : 32;
    for (int i = 0; i < 2; i++) {
        Bytes data;
        if (!SharedExists(s, names[i]) || !SharedRead(s, names[i], data)) continue;
        if (i == 0 && !FfotdStructuralOk(data)) continue; /* never list what GETPUBFILE refuses */
        if (r.results.size() >= maxr) continue;
        r.results.push_back(PubfileInfoRecord(s, names[i], data.size()));
        listed += (listed.empty() ? "" : ", ") + std::string(names[i]);
        if (i == 0) ffotd = true;
    }
    r.note = Fmt("LISTALLPUBFILES maxResults=%d -> %u bdFileInfo [%s]%s", n ? (int)n->u : -1, (unsigned)r.results.size(), listed.c_str(),
                 ffotd ? " (FFOTD gate satisfied)" : "");
    return r;
}

} // namespace

Reply StorageTask(Server &s, Conn &c, int task, const Vals &args)
{
    const Val *name = First(args, T_STRING, T_STRING17, T_MBSTRING);
    if (task == 3 || task == 7) return GetFile(s, c, task, name);
    if (task == 1) return Upload(s, c, name, First(args, T_BLOB));
    if (task == 6) return ListAllPubFiles(s, args);
    return Reply(); /* not handled here */
}

/* THE ACCOUNT ROW. The latch is the only moment the lobby learns who is on the other end (the auth
 * ticket is a fixed one), so it is where the account gets its uid, gamertag, last_login and
 * login_count. Once per connection: dw_core.cpp latches one-shot. */
void OnUidLatched(Server &s, Conn &c)
{
    if (!c.uid) return;
    GetStore(s).TouchLogin(c.uid, LivePlayerName(s, &c), "");
}

} // namespace dw

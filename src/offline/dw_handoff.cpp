/*
 * dw_handoff.cpp - the map / gametype selection of a room, and the room-message records.
 * Port of handoff/map_handoff.py (select, identify_playlist, the playlist / room latches, the
 * round-robin) and handoff/startgame_map.py (dwRoomInfo, JoinResponse, the push-7 envelope, the
 * inbound room-info scan), with the Python lobby's default switches: CODOL_MAP_POLICY=room,
 * CODOL_MAP_PREFER=mp_killhouse_sh, CODOL_MAP_TYPES=mp,coop,zm,survival, CODOL_MAP_ROTATE=1.
 * Ported at the Python branch fix/hosted-survival-spawn-20261028 (58db75a): usermaps and zm room
 * picks and the Zombies difficulty (f8235cd), level zones without a _load zone and the category
 * check of the room latch (91c483c), Survival as its own category on an MP level with authored
 * placements (8789d5f, 3742464, 9853181, 5b85e01).
 *
 * map_selection.state.json and map_rotation.state.json are module state here (HandoffState): they
 * do not survive a restart, and neither does the room that wrote them.
 * Not ported: startgame.txt (stamp), the spawn origin and fs_crc columns of the selection - they
 * are inputs of the retired Python match server, nothing the client or the embedded host reads.
 */
#include <string.h>
#include <algorithm>
#include "dw.h"
#include "dw_state.h"
#include "dw_handoff.h"
#include "dw_rooms.h"

namespace dw {

static const char kDefaultTargetMap[] = "mp_killhouse_sh";
static const char kCtorDefaultMap[] = "mp_crash_tropical"; /* dwRoomInfo ctor (C1 0x7089e0): "no map named" */
static const uint32_t kCrcPlaylist = 3615625517u;           /* crc32("playlist"): the mode-1 sentinel */
/* desc[26] is the Zombies room creator's difficulty (0 easy / 1 normal / 2 hard / 3 endless, the room
 * UI's fourth choice where mp/comaplist.csv column 23 offers it). MEASURED by the Python lobby
 * 2026-10-20: zm_sumpf_sh on hard sent desc[24..27] = crc, 0, 2, 0; easy rooms 0, 0, 0. */
enum { D_MAXPLAYERS = 2, D_NAME = 3, D_MODE = 5, D_LEAGUE = 11, D_GT_CRC = 23, D_MAP_CRC = 24, D_DIFFICULTY = 26, COOP_LEAGUE_SENTINEL = 255 };
static const double kPlaylistFreshSeconds = 300.0, kRoomLatchTtl = 1800.0;

/* Gametype names the room UI can pick but no playlist declares (measured: "war"). */
static const char *const kExtraGametypes[] = { "war", "dm", "tdm", "sd", "dom", "sab", "ctf", "koth", "arena", "gtnw",
                                               "oneflag", "hq", "vip", "dd", "conf", "infect", "gun", "shrp", "survival" };
/* SURVIVAL IS ITS OWN CATEGORY (map_handoff.SURVIVAL_LEVELS). C1's survival button sends a coop
 * descriptor whose desc[24] is crc32("coop_survival_<level>"); a room may also name gametype
 * "survival" on the MP level. The match host runs it as category "survival": g_gametype survival on
 * the unchanged MP level, with the level's placements maps/mp/survival/<level>.gsc. Only levels
 * with that data can host it; a request for another level (mp_afghan has none in any CoDOL build)
 * plays the first installed supported level. */
static const char *const kSurvivalLevels[] = { "mp_fav_tropical", "mp_cross_fire", "mp_crash_tropical", "mp_firingrange",
                                               "mp_storm_spring", "mp_seatown", "mp_ambush_sh", "mp_abandon_sh", "mp_highrise",
                                               "mp_terminal_cls", "mp_subbase_sh", "mp_hardhat_sh", "mp_qadeem_sh", "mp_strike_sh" };
/* Session-map spellings the coop-category flows put in desc[24] (measured: coop_survival_mp_afghan). */
static const char *const kCoopSessionPrefixes[] = { "coop_training_", "coop_prolog_", "coop_survival_", "coop_spec_",
                                                    "coop_spec1_", "coop_defense_", "coop_take_" };
/* handoff/installed_zones.json: the retail install's zone list (every client runs the same one),
 * used when no zone folder is configured. A served "installed_zones.json" resource overrides it. */
static const char kZoneManifest[] =
    "code_post_gfx_default_mp code_post_gfx_mp code_pre_gfx_mp common_gametype_coop common_gametype_mp common_gametype_zm "
    "common_mp coop_defense_mp_crash_tropical coop_defense_mp_crash_tropical_load coop_prolog_mp_crash_intro "
    "coop_prolog_mp_crash_intro_load coop_spec1_mp_killhouse_sh coop_spec1_mp_killhouse_sh_load coop_spec_mp_crash_tropical "
    "coop_spec_mp_crash_tropical_load coop_spec_mp_estate_tropical coop_spec_mp_estate_tropical_load coop_spec_mp_fav_tropical "
    "coop_spec_mp_fav_tropical_load coop_take_mp_estate_tropical coop_take_mp_estate_tropical_load coop_take_mp_fav_tropical "
    "coop_take_mp_fav_tropical_load coop_training_mp_killhouse_sh coop_training_mp_killhouse_sh_load development_mp "
    "localized_code_post_gfx_mp localized_code_pre_gfx_mp localized_common_gametype_coop localized_common_gametype_mp "
    "localized_common_gametype_zm localized_common_mp localized_ui_mp mp_afghan mp_afghan_load mp_bloc_sh mp_bloc_sh_load "
    "mp_bog_sh mp_bog_sh_load mp_cargoship_sh mp_cargoship_sh_load mp_crash_intro mp_crash_tropical mp_crash_tropical_load "
    "mp_cross_fire mp_cross_fire_load mp_estate_tropical mp_estate_tropical_load mp_fav_tropical mp_fav_tropical_load "
    "mp_killhouse_sh mp_killhouse_sh_load mp_nuked mp_nuked_load mp_overgrown mp_overgrown_load mp_playlists mp_rust "
    "mp_rust_load mp_storm_spring mp_storm_spring_load mp_underpass mp_underpass_load patch_code_post_gfx_mp "
    "patch_common_gametype_coop patch_common_gametype_mp patch_common_gametype_zm patch_common_mp "
    "patch_coop_spec1_mp_killhouse_sh patch_coop_training_mp_killhouse_sh patch_development_mp "
    "patch_localized_code_post_gfx_mp patch_localized_code_pre_gfx_mp patch_localized_common_gametype_coop "
    "patch_localized_common_gametype_mp patch_localized_common_gametype_zm patch_localized_common_mp patch_mp_killhouse_sh "
    "patch_mp_playlists patch_ui_mp patch_zm_prototype_sh patch_zm_zombie_training ui_mp ui_viewer_mp zm_asylum_sh "
    "zm_asylum_sh_load zm_prototype_sh zm_prototype_sh_load zm_zombie_training zm_zombie_training_load";

typedef std::vector<std::pair<std::string, std::string> > Rotation; /* (map, gametype) */

struct PlaylistMeta {
    int64_t league, minplayers; /* -1 = not declared (Python None) */
    std::set<std::string> flags;
    std::string nameEn;
    PlaylistMeta() : league(-1), minplayers(-1) {}
};

struct DescFields {
    bool ok;
    int64_t maxplayers, mode, league; /* -1 = the field is not a number */
    int64_t difficulty;               /* desc[26]; -1 = the descriptor is too short or the field is not a number */
    uint32_t gtCrc, mapCrc;
    DescFields() : ok(false), maxplayers(-1), mode(-1), league(-1), difficulty(-1), gtCrc(0), mapCrc(0) {}
};

struct HandoffState {
    bool loaded;
    std::map<uint32_t, std::string> crc;  /* crc32(name) -> name */
    std::vector<int> order;               /* playlist numbers in file order */
    std::map<int, Rotation> playlists;
    std::map<int, PlaylistMeta> meta;
    bool zonesLoaded;
    int64_t zoneDirMtime, usermapsMtime, gameZonesMtime;
    std::set<std::string> zones, usermaps;
    int zonesGen, crcGen; /* the crc vocabulary is rebuilt when the installed set changes (a new usermap) */
    /* map_selection.state.json */
    int latchPlaylist;                    /* 0 = none */
    double latchPlaylistTime;
    std::string latchPlaylistWhy;
    bool hasRoomLatch, roomHasGametype;
    std::string roomMap, roomGametype;
    int roomMaxPlayers;
    double roomTime;
    /* map_rotation.state.json: playlist -> next index */
    std::map<int, int> rotation;
    /* the newest full descriptor any connection sent */
    DescFields logDesc;
    HandoffState()
        : loaded(false), zonesLoaded(false), zoneDirMtime(0), usermapsMtime(0), gameZonesMtime(0), zonesGen(0), crcGen(-1), latchPlaylist(0), latchPlaylistTime(0), hasRoomLatch(false),
          roomHasGametype(false), roomMaxPlayers(0), roomTime(0) {}
};

HandoffState *HandoffNew() { return new HandoffState(); }
void HandoffFree(HandoffState *h) { delete h; }
static HandoffState &H(Server &s) { return *Rooms(s).handoff; }

/* ---- small text helpers with Python's str semantics on latin1-decoded bytes ---- */

static bool PySpace(unsigned char c) { return c == ' ' || (c >= 9 && c <= 13) || (c >= 0x1c && c <= 0x1f) || c == 0x85 || c == 0xa0; }
static bool PyLineBreak(unsigned char c) { return c == '\n' || c == '\r' || c == 0x0b || c == 0x0c || (c >= 0x1c && c <= 0x1e) || c == 0x85; }
static bool IdentChar(unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
static bool PyWord(unsigned char c)
{
    return IdentChar(c) || (c >= 0xC0 && c != 0xD7 && c != 0xF7) || c == 0xAA || c == 0xB5 || c == 0xBA || c == 0xB2 || c == 0xB3 ||
           c == 0xB9 || (c >= 0xBC && c <= 0xBE);
}

static std::string PyStrip(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && PySpace((unsigned char)s[a])) a++;
    while (b > a && PySpace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

static bool StartsWith(const std::string &s, const char *prefix)
{
    size_t n = strlen(prefix);
    return s.size() >= n && !memcmp(s.data(), prefix, n);
}
static bool EndsWith(const std::string &s, const char *suffix)
{
    size_t n = strlen(suffix);
    return s.size() >= n && !memcmp(s.data() + s.size() - n, suffix, n);
}

struct Cursor { /* a hand-rolled matcher for the handful of anchored patterns of _load_playlists */
    const std::string &s;
    size_t i;
    Cursor(const std::string &text) : s(text), i(0) {}
    bool Lit(const char *w)
    {
        size_t n = strlen(w);
        if (s.size() - i < n || memcmp(s.data() + i, w, n)) return false;
        i += n;
        return true;
    }
    bool Ws() /* \s+ */
    {
        size_t j = i;
        while (i < s.size() && PySpace((unsigned char)s[i])) i++;
        return i > j;
    }
    bool Digits(int64_t &v) /* \d+ */
    {
        size_t j = i;
        v = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            if (v < 100000000000LL) v = v * 10 + (s[i] - '0');
            i++;
        }
        return i > j;
    }
    bool Ident(std::string &out) /* [A-Za-z0-9_]+ */
    {
        size_t j = i;
        while (i < s.size() && IdentChar((unsigned char)s[i])) i++;
        out = s.substr(j, i - j);
        return i > j;
    }
    bool End() const { return i == s.size(); }
};

std::string BaseMap(const std::string &playlistMap)
{
    size_t i = playlistMap.find("_mp_");
    return i == std::string::npos ? playlistMap : playlistMap.substr(i + 1);
}

/* gametype name -> the <gt> of common_gametype_<gt>. "" when the name is unknown. */
static std::string ZoneCategory(const std::string &gametype)
{
    if (gametype.empty()) return "";
    if (gametype == "coop" || gametype == "zm" || gametype == "survival") return gametype;
    return "mp";
}

static uint32_t NameCrc(const std::string &name) { return Crc32(name.data(), name.size()); }

/* ---- installed zones ---- */

static void ZoneBases(const std::vector<std::string> &names, std::set<std::string> &out)
{
    for (size_t i = 0; i < names.size(); i++)
        if (!StartsWith(names[i], "patch_") && !EndsWith(names[i], "_load")) out.insert(names[i]);
}

static void ManifestZones(Server &s, std::set<std::string> &out)
{
    std::vector<std::string> names;
    Bytes text;
    Json doc;
    if (s.Resource("installed_zones.json", text) && Json::Parse(text, doc) && doc.get("zones").isArray()) {
        const std::vector<Json> &z = doc.get("zones").items();
        for (size_t i = 0; i < z.size(); i++)
            if (z[i].isString()) names.push_back(z[i].asString());
    } else {
        const char *p = kZoneManifest;
        while (*p) {
            const char *e = strchr(p, ' ');
            if (!e) e = p + strlen(p);
            names.push_back(std::string(p, (size_t)(e - p)));
            p = *e ? e + 1 : e;
        }
    }
    ZoneBases(names, out);
}

static std::string Lowered(std::string v)
{
    for (size_t i = 0; i < v.size(); i++)
        if (v[i] >= 'A' && v[i] <= 'Z') v[i] = (char)(v[i] + 32);
    return v;
}

/* map_handoff.usermaps: maps ported from other CoDOL builds. They are installed as
 * <game>/usermaps/<map>/<map>.ff (Kernel33 usermaps.c hands C1 that file for a zone missing under
 * client_shipRetail), or by the map pack in the game's own client_shipRetail/chinese: every mp_ /
 * zm_ zone there that is not a _load zone and not the playlist table. A level zone needs no
 * <map>_load.ff (stock mp_crash_intro ships none). */
static void Usermaps(Server &s, std::set<std::string> &out)
{
    if (s.cfg.gameDir.empty()) return;
    std::string dir = PathJoin(s.cfg.gameDir, "usermaps");
    std::vector<std::string> names = ListDir(dir);
    for (size_t i = 0; i < names.size(); i++)
        if (FileExists(PathJoin(PathJoin(dir, names[i]), names[i] + ".ff"))) out.insert(names[i]);
    std::vector<std::string> files = ListDir(PathJoin(PathJoin(s.cfg.gameDir, "client_shipRetail"), "chinese"));
    for (size_t i = 0; i < files.size(); i++) {
        std::string f = Lowered(files[i]);
        if (!(StartsWith(f, "mp_") || StartsWith(f, "zm_")) || !EndsWith(f, ".ff") || EndsWith(f, "_load.ff")) continue;
        f.erase(f.size() - 3);
        if (f != "mp_playlists") out.insert(f);
    }
}

/* map_handoff.installed_maps: the zone folder when it holds fastfiles, else the manifest; plus the
 * usermaps. */
static const std::set<std::string> &InstalledMaps(Server &s)
{
    HandoffState &h = H(s);
    int64_t mtime = s.cfg.zoneDir.empty() ? 0 : FileMtime(s.cfg.zoneDir);
    int64_t um = s.cfg.gameDir.empty() ? 0 : FileMtime(PathJoin(s.cfg.gameDir, "usermaps"));
    int64_t gz = s.cfg.gameDir.empty() ? 0 : FileMtime(PathJoin(PathJoin(s.cfg.gameDir, "client_shipRetail"), "chinese"));
    if (h.zonesLoaded && mtime == h.zoneDirMtime && um == h.usermapsMtime && gz == h.gameZonesMtime) return h.zones;
    std::set<std::string> zones, usermaps;
    if (!s.cfg.zoneDir.empty()) {
        std::vector<std::string> files = ListDir(s.cfg.zoneDir), names;
        for (size_t i = 0; i < files.size(); i++) {
            const std::string &f = files[i];
            if (f.size() < 3) continue;
            if (Lowered(f.substr(f.size() - 3)) == ".ff") names.push_back(f.substr(0, f.size() - 3));
        }
        ZoneBases(names, zones);
    }
    if (zones.empty()) ManifestZones(s, zones);
    Usermaps(s, usermaps);
    zones.insert(usermaps.begin(), usermaps.end());
    if (!h.zonesLoaded || zones != h.zones) h.zonesGen++;
    h.zones.swap(zones);
    h.usermaps.swap(usermaps);
    h.zonesLoaded = true;
    h.zoneDirMtime = mtime;
    h.usermapsMtime = um;
    h.gameZonesMtime = gz;
    return h.zones;
}

bool HandoffZoneInstalled(Server &s, const std::string &name)
{
    const std::set<std::string> &inst = InstalledMaps(s);
    return inst.empty() || inst.count(name) || inst.count(BaseMap(name));
}

/* map_handoff.available_survival_levels. With no game install known (the standalone lobby, the
 * differential tests) every supported level counts. Otherwise a level needs its MP zone, and:
 *   - when the embedder knows the game's zones (Host::SurvivalSupported), a coop_survival_<level> row in
 *     mp/comaplist.csv (usermaps_patch.ff carries one per level with placements; the scripts themselves are in
 *     main/sh1x_svr_scripts.iwd), the same list p2p.c P2PCheck holds the embedded host to;
 *   - when it does not (no embedder, or the table is not loaded yet), the gametype and placement scripts in a loose
 *     main/sh1x_rawfiles tree. */
std::vector<std::string> HandoffSurvivalLevels(Server &s)
{
    std::vector<std::string> out;
    const size_t n = sizeof(kSurvivalLevels) / sizeof(kSurvivalLevels[0]);
    if (s.cfg.gameDir.empty()) {
        out.assign(kSurvivalLevels, kSurvivalLevels + n);
        return out;
    }
    std::string mp = PathJoin(PathJoin(PathJoin(PathJoin(s.cfg.gameDir, "main"), "sh1x_rawfiles"), "maps"), "mp");
    const bool gametype = FileExists(PathJoin(PathJoin(mp, "gametypes"), "survival.gsc"));
    for (size_t i = 0; i < n; i++) {
        std::string level = kSurvivalLevels[i];
        int listed = !s.cfg.differential && s.host ? s.host->SurvivalSupported(level) : -1;
        if (listed == 0) continue;
        if (listed < 0 && !(gametype && FileExists(PathJoin(PathJoin(mp, "survival"), level + ".gsc")))) continue;
        if (!FileExists(PathJoin(PathJoin(PathJoin(s.cfg.gameDir, "client_shipRetail"), "chinese"), level + ".ff")) &&
            !FileExists(PathJoin(PathJoin(PathJoin(s.cfg.gameDir, "usermaps"), level), level + ".ff")))
            continue;
        out.push_back(level);
    }
    return out;
}

/* Whether `level` itself can be played as Survival: one of the levels above, or any other MP level the game's own
 * zones list (a coop_survival_<level> row; every MP map has one since the generic placements) whose zone is installed.
 * kSurvivalLevels is only the authored levels, in the order the default is picked from. */
static bool SurvivalLevelAvailable(Server &s, const std::vector<std::string> &avail, const std::string &level)
{
    if (std::find(avail.begin(), avail.end(), level) != avail.end()) return true;
    if (s.cfg.gameDir.empty() || s.cfg.differential || !s.host || s.host->SurvivalSupported(level) != 1) return false;
    return FileExists(PathJoin(PathJoin(PathJoin(s.cfg.gameDir, "client_shipRetail"), "chinese"), level + ".ff")) ||
           FileExists(PathJoin(PathJoin(PathJoin(s.cfg.gameDir, "usermaps"), level), level + ".ff"));
}

/* ---- the playlist file: the name<->crc32 dictionary and the rotations (_load_playlists) ---- */

static void LoadPlaylists(Server &s)
{
    HandoffState &h = H(s);
    const std::set<std::string> &inst = InstalledMaps(s);
    /* keyed on the installed set as well: a map folder added under usermaps enters the crc
     * vocabulary without a restart */
    if (h.loaded && h.crcGen == h.zonesGen) return;
    Bytes text;
    if (!s.Resource("mp_playlists_shipRetail_cb2.info", text)) return; /* Python: an empty table, retried next call */
    h.crc.clear();
    h.order.clear();
    h.playlists.clear();
    h.meta.clear();
    std::set<std::string> names;
    bool haveCur = false;
    int cur = 0;
    size_t at = 0;
    while (at < text.size()) {
        size_t e = at;
        while (e < text.size() && !PyLineBreak((unsigned char)text[e])) e++;
        std::string ln = PyStrip(text.substr(at, e - at));
        at = (e < text.size() && text[e] == '\r' && e + 1 < text.size() && text[e + 1] == '\n') ? e + 2 : e + 1;

        Cursor pl(ln);
        int64_t n = 0, league = 0;
        if (pl.Lit("playlist") && pl.Ws() && pl.Digits(n) && (pl.End() || !PyWord((unsigned char)ln[pl.i]))) {
            cur = (int)n;
            haveCur = true;
            if (!h.playlists.count(cur)) {
                h.playlists[cur] = Rotation();
                h.order.push_back(cur);
            }
            PlaylistMeta &m = h.meta[cur];
            if (pl.Ws() && pl.Lit("league") && pl.Ws() && pl.Digits(league)) m.league = league;
            continue;
        }
        Cursor gt(ln);
        if (gt.Lit("gametype") && gt.Ws()) { /* ^gametype\s+(\S+) */
            size_t j = gt.i;
            while (gt.i < ln.size() && !PySpace((unsigned char)ln[gt.i])) gt.i++;
            if (gt.i > j) {
                names.insert(ln.substr(j, gt.i - j));
                continue;
            }
        }
        Cursor rot(ln);
        std::string mapName, gtName;
        int64_t weight = 0;
        if (rot.Ident(mapName) && rot.Lit(",") && rot.Ident(gtName) && rot.Lit(",") && rot.Digits(weight) && rot.End()) {
            names.insert(mapName);
            names.insert(gtName);
            if (haveCur) h.playlists[cur].push_back(std::make_pair(mapName, gtName));
            continue;
        }
        if (!haveCur) continue;
        PlaylistMeta &m = h.meta[cur];
        Cursor rule(ln);
        int64_t v = 0;
        if (rule.Lit("rule") && rule.Ws() && rule.Lit("party_minplayers") && rule.Ws() && rule.Digits(v)) {
            m.minplayers = v;
            continue;
        }
        Cursor nm(ln);
        if (nm.Lit("name") && nm.Ws() && nm.Lit("english") && nm.Ws() && nm.Lit("\"")) {
            size_t q = ln.rfind('"');
            if (q != std::string::npos && q >= nm.i) m.nameEn = ln.substr(nm.i, q - nm.i);
            continue;
        }
        if (ln == "combattraining" || ln == "mptraining3v3" || ln == "ranked" || ln == "nospecialequipment" ||
            ln == "notacticalequipment" || ln == "nokillstreak2" || ln == "nokillstreak3")
            m.flags.insert(ln);
    }
    names.insert("playlist");
    /* The room vocabulary first, so that anything the .info declares overwrites it. */
    for (size_t i = 0; i < sizeof(kExtraGametypes) / sizeof(kExtraGametypes[0]); i++) h.crc[NameCrc(kExtraGametypes[i])] = kExtraGametypes[i];
    for (std::set<std::string>::const_iterator it = inst.begin(); it != inst.end(); ++it) {
        const std::string &z = *it;
        if (!(StartsWith(z, "mp_") || StartsWith(z, "zm_") || StartsWith(z, "coop_")) || z == "mp_playlists") continue;
        h.crc[NameCrc(z)] = z;
        if (StartsWith(z, "mp_"))
            for (size_t k = 0; k < sizeof(kCoopSessionPrefixes) / sizeof(kCoopSessionPrefixes[0]); k++) {
                std::string spelled = kCoopSessionPrefixes[k] + z;
                h.crc[NameCrc(spelled)] = spelled;
            }
    }
    for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it) h.crc[NameCrc(*it)] = *it;
    h.loaded = true;
    h.crcGen = h.zonesGen;
}

static std::string CrcName(HandoffState &h, uint32_t crc)
{
    std::map<uint32_t, std::string>::const_iterator it = h.crc.find(crc);
    return it == h.crc.end() ? std::string() : it->second;
}

/* ---- the descriptor ---- */

static bool IsNumber(const Val &v) { return (v.tag >= 1 && v.tag <= 10) || v.tag == 13 || v.tag == 14; }

static DescFields FieldsOf(const Vals *vals)
{
    DescFields d;
    if (!vals || vals->size() <= (size_t)D_MAP_CRC) return d;
    const Vals &v = *vals;
    if (!IsNumber(v[D_GT_CRC]) || !IsNumber(v[D_MAP_CRC])) return d;
    if (IsNumber(v[D_MAXPLAYERS])) d.maxplayers = (int64_t)(v[D_MAXPLAYERS].u & 0xFFFFFFFFu);
    if (IsNumber(v[D_MODE])) d.mode = (int64_t)(v[D_MODE].u & 0xFFFFFFFFu);
    if (IsNumber(v[D_LEAGUE])) d.league = (int64_t)(v[D_LEAGUE].u & 0xFFFFFFFFu);
    if (v.size() > (size_t)D_DIFFICULTY && IsNumber(v[D_DIFFICULTY])) d.difficulty = (int64_t)(v[D_DIFFICULTY].u & 0xFFFFFFFFu);
    d.gtCrc = (uint32_t)v[D_GT_CRC].u;
    d.mapCrc = (uint32_t)v[D_MAP_CRC].u;
    d.ok = true;
    return d;
}

std::string DescPlayerName(const Vals *desc)
{
    if (!desc || desc->size() <= (size_t)D_NAME || (*desc)[D_NAME].tag != T_STRING) return "";
    return PyStrip((*desc)[D_NAME].s);
}

void HandoffObserveDescriptor(Server &s, int task, const Vals &args)
{
    /* map_handoff._desc_from_lobby_log: the newest 21/1 or 21/12 request line with more than 24
     * fields. The Python lobby finds it in its own log; this is the same answer without the log. */
    Vals vals;
    for (size_t i = 0; i < args.size(); i++)
        if (args[i].tag != 0) vals.push_back(args[i]);
    if (task == 12) vals.erase(vals.begin(), vals.begin() + (vals.size() < 2 ? vals.size() : 2));
    DescFields d = FieldsOf(&vals);
    if (d.ok) H(s).logDesc = d;
}

/* ---- the latches ---- */

static void LatchPlaylist(Server &s, int number, const std::string &why)
{
    HandoffState &h = H(s);
    if (!number || h.latchPlaylist == number) return;
    h.latchPlaylist = number;
    h.latchPlaylistTime = s.host->Mono();
    h.latchPlaylistWhy = why;
}

bool HandoffLatchRoom(Server &s, const std::string &map, const std::string *gametype, int maxPlayers, const std::string &)
{
    HandoffState &h = H(s);
    if (map.empty()) return false;
    double now = s.host->Mono();
    bool sameGt = gametype ? (h.roomHasGametype && h.roomGametype == *gametype) : !h.roomHasGametype;
    if (h.hasRoomLatch && h.roomMap == map && sameGt && now - h.roomTime < 5.0) return false;
    h.hasRoomLatch = true;
    h.roomMap = map;
    h.roomHasGametype = gametype != NULL;
    h.roomGametype = gametype ? *gametype : std::string();
    h.roomMaxPlayers = maxPlayers;
    h.roomTime = now;
    return true;
}

static bool RoomLatchLive(Server &s)
{
    HandoffState &h = H(s);
    return h.hasRoomLatch && !h.roomMap.empty() && s.host->Mono() - h.roomTime <= kRoomLatchTtl;
}

bool HandoffRoomLatchGametype(Server &s, std::string &gametype)
{
    if (!RoomLatchLive(s) || !H(s).roomHasGametype) return false;
    gametype = H(s).roomGametype;
    return true;
}

/* ---- descriptor -> playlist (identify_playlist) ---- */

static bool AllOfCategory(const Rotation &r, const std::string &want)
{
    if (r.empty()) return false;
    for (size_t i = 0; i < r.size(); i++)
        if (ZoneCategory(r[i].second) != want) return false;
    return true;
}

/* -> the playlist number, or -1. */
static int IdentifyPlaylist(Server &s, const DescFields &d, std::string &why)
{
    HandoffState &h = H(s);
    if (!d.ok) { why = "no descriptor"; return -1; }
    if (d.gtCrc == kCrcPlaylist) { /* mode 1: the number is literally on the wire */
        if (d.mapCrc <= 0x7FFFFFFFu && h.playlists.count((int)d.mapCrc)) {
            why = Fmt("mode 1, playlist number on the wire (desc[24]=%u)", d.mapCrc);
            return (int)d.mapCrc;
        }
        why = Fmt("mode 1 but playlist %u is not in the .info", d.mapCrc);
        return -1;
    }
    std::string gtName = CrcName(h, d.gtCrc), cat = ZoneCategory(gtName);
    int64_t league = d.league, minp = d.maxplayers;

    /* coop / zm, and league-255 descriptors whose gametype is not an mp one: the playlist is
     * identified by the session map being in a candidate's rotation. */
    if (gtName == "coop" || gtName == "zm" || (league == COOP_LEAGUE_SENTINEL && (cat.empty() || cat == "coop" || cat == "zm"))) {
        std::string want = (cat == "coop" || cat == "zm") ? cat : "coop";
        std::vector<int> cands;
        for (size_t i = 0; i < h.order.size(); i++)
            if (AllOfCategory(h.playlists[h.order[i]], want)) cands.push_back(h.order[i]);
        std::string mapName = CrcName(h, d.mapCrc);
        if (!mapName.empty() && mapName != kCtorDefaultMap) {
            std::vector<int> inRot;
            for (size_t i = 0; i < cands.size(); i++) {
                const Rotation &r = h.playlists[cands[i]];
                for (size_t k = 0; k < r.size(); k++)
                    if (r[k].first == mapName) { inRot.push_back(cands[i]); break; }
            }
            if (inRot.size() == 1) {
                why = Fmt("%s descriptor, session map %s is in playlist %d's rotation", want.c_str(), mapName.c_str(), inRot[0]);
                return inRot[0];
            }
            if (inRot.empty()) {
                why = Fmt("%s descriptor names session map %s which is in NO %s playlist (survival / spec-ops flow)", want.c_str(),
                          mapName.c_str(), want.c_str());
                return -1;
            }
        }
        why = Fmt("%s descriptor (league=%lld gametype=%s)", want.c_str(), (long long)league, gtName.c_str());
        return cands.size() == 1 ? cands[0] : -1;
    }

    /* mode 0: (league, party_minplayers), restricted by the gametype's flag; a fresh mode-1 latch
     * corrects the stale fingerprint that follows a public-matchmaking pick. */
    int latched = 0;
    double latchAge = 0;
    if (h.latchPlaylist) {
        latchAge = s.host->Mono() - h.latchPlaylistTime;
        if (latchAge <= kPlaylistFreshSeconds) latched = h.latchPlaylist;
    }
    const char *flag = gtName == "combat" ? "combattraining" : NULL;
    std::vector<int> byLeague;
    for (size_t i = 0; i < h.order.size(); i++) {
        const PlaylistMeta &m = h.meta[h.order[i]];
        if (flag && !m.flags.count(flag)) continue;
        if (m.league == league) byLeague.push_back(h.order[i]);
    }
    if (byLeague.size() == 1) {
        int n = byLeague[0];
        const PlaylistMeta &m = h.meta[n];
        if (m.minplayers == minp) {
            why = Fmt("mode 0, league %lld -> playlist %d uniquely and party_minplayers matches desc[2]", (long long)league, n);
            return n;
        }
        if (latched && h.playlists.count(latched)) {
            why = Fmt("mode 0 fingerprint (league %lld -> playlist %d) is stale; the fresh mode-1 latch says playlist %d (%.0f s ago)",
                      (long long)league, n, latched, latchAge);
            return latched;
        }
        why = Fmt("mode 0, league %lld -> playlist %d uniquely, party_minplayers differs (no fresh latch)", (long long)league, n);
        return n;
    }
    std::vector<int> byBoth;
    for (size_t i = 0; i < byLeague.size(); i++)
        if (h.meta[byLeague[i]].minplayers == minp) byBoth.push_back(byLeague[i]);
    if (byBoth.size() == 1) {
        why = Fmt("mode 0, league %lld + party_minplayers %lld -> playlist %d", (long long)league, (long long)minp, byBoth[0]);
        return byBoth[0];
    }
    if (latched && std::find(byLeague.begin(), byLeague.end(), latched) != byLeague.end()) {
        why = Fmt("mode 0, league %lld ambiguous -- resolved by the fresh mode-1 latch: playlist %d", (long long)league, latched);
        return latched;
    }
    why = Fmt("mode 0, league=%lld party_minplayers=%lld gametype=%s -> %u candidates", (long long)league, (long long)minp,
              gtName.c_str(), (unsigned)byLeague.size());
    return -1;
}

/* -> the map the descriptor says the ROOM is showing ("" when it names none) and its gametype. */
static std::string RoomFromDesc(Server &s, const DescFields &d, std::string &gametype)
{
    HandoffState &h = H(s);
    gametype.clear();
    if (!d.ok || d.gtCrc == kCrcPlaylist) return "";
    std::string gt = CrcName(h, d.gtCrc), cat = ZoneCategory(gt);
    if (cat == "coop" || cat == "zm") return ""; /* a coop / zm descriptor is not a room */
    std::string m = CrcName(h, d.mapCrc);
    if (m.empty() || m == kCtorDefaultMap) return "";
    if (!HandoffZoneInstalled(s, m)) return "";
    gametype = gt;
    return m;
}

static Rotation LiveRotation(Server &s, const Rotation &rotation)
{
    Rotation live;
    for (size_t i = 0; i < rotation.size(); i++)
        if (HandoffZoneInstalled(s, rotation[i].first)) live.push_back(rotation[i]);
    return live;
}

/* ---- THE selection (map_handoff.select) ---- */

Selection HandoffSelect(Server &s, const Vals *desc)
{
    LoadPlaylists(s);
    HandoffState &h = H(s);
    Selection sel;
    std::vector<std::string> &chain = sel.chain;

    /* 1. where the descriptor comes from */
    DescFields d = (desc && !desc->empty()) ? FieldsOf(desc) : DescFields();
    std::string source = d.ok ? "descriptor" : "";
    if (!d.ok && h.logDesc.ok) {
        d = h.logDesc;
        source = "lobbylog";
    }
    if (d.ok)
        chain.push_back(Fmt("descriptor(%s) mode=%lld d2=%lld league=%lld gt=%s(%u) map=%s(%u)", source.c_str(), (long long)d.mode,
                            (long long)d.maxplayers, (long long)d.league, CrcName(h, d.gtCrc).c_str(), d.gtCrc,
                            CrcName(h, d.mapCrc).c_str(), d.mapCrc));
    else
        chain.push_back("no descriptor available");

    /* 2. descriptor -> playlist */
    std::string why;
    int pl = IdentifyPlaylist(s, d, why);
    chain.push_back(Fmt("playlist: %d -- %s", pl, why.c_str()));
    if (pl > 0 && d.ok && d.gtCrc == kCrcPlaylist) LatchPlaylist(s, pl, why);
    if (pl < 0) {
        std::string descCat = d.ok ? ZoneCategory(CrcName(h, d.gtCrc)) : "";
        bool roomShape = d.ok && d.league == COOP_LEAGUE_SENTINEL;
        if (h.latchPlaylist && h.playlists.count(h.latchPlaylist) && descCat != "coop" && descCat != "zm" && !roomShape) {
            pl = h.latchPlaylist;
            source = "latch";
            chain.push_back(Fmt("playlist: fell back to the LATCHED %d", pl));
        }
    }
    bool resolved = pl >= 0;
    Rotation rotation;
    if (pl >= 0 && h.playlists.count(pl)) rotation = h.playlists[pl];
    const PlaylistMeta *meta = (pl >= 0 && h.meta.count(pl)) ? &h.meta[pl] : NULL;

    /* 2b. the room's own pick: the descriptor first, the typeId-15 latch for a league-255 room
     * whose desc[24] is the ctor default (the one map the descriptor cannot express). */
    std::string roomGt, roomMap = RoomFromDesc(s, d, roomGt);
    bool roomHasGt = !roomGt.empty();
    if (!roomMap.empty()) {
        chain.push_back("room: " + roomMap + " -- the descriptor named the room's own map");
    } else {
        bool latch = d.ok && d.league == COOP_LEAGUE_SENTINEL && RoomLatchLive(s);
        /* THE LATCH MUST BE THE SAME CATEGORY AS THE DESCRIPTOR (Python 2026-10-23: a war room whose
         * desc[24] did not decode fell through to a zm room latch broadcast 202 s earlier and loaded
         * zm_theater_sh). A latch of another category is some earlier room, never this one. */
        if (latch) {
            std::string dCat = ZoneCategory(CrcName(h, d.gtCrc));
            std::string lCat = h.roomHasGametype ? ZoneCategory(h.roomGametype) : std::string();
            if (!dCat.empty() && !lCat.empty() && lCat != dCat) {
                chain.push_back("room: IGNORING the typeId-15 latch " + h.roomMap + " (gametype=" + h.roomGametype + "): the descriptor is a " +
                                dCat + " room");
                latch = false;
            }
        }
        if (latch && HandoffZoneInstalled(s, h.roomMap)) {
            roomMap = h.roomMap;
            roomGt = h.roomGametype;
            roomHasGt = h.roomHasGametype && !roomGt.empty();
            chain.push_back("room: " + roomMap + " -- the typeId-15 dwRoomInfo the client broadcast");
        } else {
            chain.push_back("room: no room selection on the wire -- falling through to the rotation");
        }
    }

    /* 3. gametype */
    std::string gtName = d.ok ? CrcName(h, d.gtCrc) : "";
    if (gtName == "playlist") gtName.clear();
    if (gtName.empty() && !rotation.empty()) {
        std::set<std::string> gts;
        for (size_t i = 0; i < rotation.size(); i++) gts.insert(rotation[i].second);
        if (gts.size() == 1) gtName = *gts.begin();
    }
    if (!roomMap.empty() && roomHasGt) gtName = roomGt;
    std::string gametype = gtName.empty() ? "combat" : gtName;
    bool gtFromPlaylist = false;
    /* desc[23]="combat" is the combat-training tag; for any other playlist the rotation column decides. */
    if (!(!roomMap.empty() && roomHasGt) && pl >= 0 && gametype == "combat" && !(meta && meta->flags.count("combattraining"))) {
        std::set<std::string> gts;
        for (size_t i = 0; i < rotation.size(); i++) gts.insert(rotation[i].second);
        if (!gts.empty() && !(gts.size() == 1 && *gts.begin() == "combat")) {
            gametype = *gts.begin(); /* sorted: the first name */
            gtFromPlaylist = true;
        }
    }
    chain.push_back("gametype: " + gametype);
    std::string ztype = ZoneCategory(gametype);
    if (ztype.empty()) ztype = "mp";

    /* 4. the map */
    std::string sessionMap, mapName;
    bool rotated = false;
    int rotationIx = -1;
    std::string descMap = d.ok ? CrcName(h, d.mapCrc) : "";
    if (!roomMap.empty()) {
        sessionMap = roomMap;
        mapName = BaseMap(roomMap);
        source = "room";
        resolved = true;
        chain.push_back("map: " + sessionMap + " -- FOLLOWING THE ROOM");
    } else if ((ztype == "coop" || ztype == "zm") && !descMap.empty()) {
        sessionMap = descMap;
        mapName = BaseMap(sessionMap);
        chain.push_back("map: " + sessionMap + " from the " + ztype + " descriptor (zone " + mapName + ")");
    } else if (!rotation.empty()) {
        Rotation live = LiveRotation(s, rotation);
        if (!live.empty()) {
            std::map<int, int>::const_iterator it = h.rotation.find(pl);
            int i = (it == h.rotation.end() ? 0 : it->second) % (int)live.size();
            sessionMap = live[(size_t)i].first;
            mapName = BaseMap(sessionMap);
            rotated = true;
            rotationIx = i;
            chain.push_back(Fmt("map: %s -- playlist %d rotation, round-robin entry %d of %u installed", sessionMap.c_str(), pl, i + 1,
                                (unsigned)live.size()));
            const std::string &gtRot = live[(size_t)i].second;
            if ((gtName.empty() || gtFromPlaylist) && !gtRot.empty() && gtRot != gametype) {
                gametype = gtRot;
                ztype = ZoneCategory(gametype);
                if (ztype.empty()) ztype = "mp";
                chain.push_back("gametype: refined to " + gtRot + " -- the picked rotation entry's own column");
            }
        } else {
            mapName = kDefaultTargetMap;
            source = "pin";
            resolved = false;
            chain.push_back(Fmt("map: %s -- PIN (LAST RESORT): playlist %d has no installed map", mapName.c_str(), pl));
        }
    } else {
        mapName = kDefaultTargetMap;
        source = "pin";
        resolved = false;
        chain.push_back("map: " + mapName + " -- PIN (LAST RESORT): no room map on the wire, no resolvable playlist rotation");
    }
    if (sessionMap.empty()) sessionMap = mapName;

    /* Every entry path carries Survival's category: the coop_survival_<level> descriptor of C1's
     * survival button, a room that names gametype survival, a latch. The MP level's own spelling is
     * the map identity for both the host and the client (there is no Survival session zone). */
    if (StartsWith(sessionMap, "coop_survival_") || gametype == "survival") {
        std::string level = BaseMap(sessionMap);
        std::vector<std::string> avail = HandoffSurvivalLevels(s);
        ztype = "survival";
        gametype = "survival";
        if (SurvivalLevelAvailable(s, avail, level)) {
            mapName = sessionMap = level;
            chain.push_back("type: survival -- MP level " + level + " (the selected MP level has survival placements)");
        } else if (!avail.empty()) {
            /* not a rotation: the selection is resolved more than once per match and must not move */
            mapName = sessionMap = avail[0];
            chain.push_back("type: survival -- MP level " + avail[0] + " (" + level + " unavailable; default survival level)");
        } else {
            sel.error = "No installed Survival level has both its MP archive and survival placement script";
            mapName = sessionMap = level;
            chain.push_back("type: survival -- " + sel.error);
        }
    }

    /* Zombies: the room creator's difficulty (3 = Endless: no round cap, no rescue, _zombiemode.gsc);
     * anything else on the wire is easy. */
    if (ztype == "zm") {
        if (d.ok && d.difficulty >= 0 && d.difficulty <= 3) {
            static const char *const names[] = { "easy", "normal", "hard", "endless" };
            sel.difficulty = (int)d.difficulty;
            chain.push_back(Fmt("difficulty: %d (%s) from desc[%d]", sel.difficulty, names[sel.difficulty], (int)D_DIFFICULTY));
        } else {
            sel.difficulty = 0;
            chain.push_back("difficulty: not 0..3 on the wire -- easy (0)");
        }
    }

    sel.map = mapName;
    sel.sessionMap = sessionMap;
    sel.type = ztype;
    sel.gametype = gametype;
    sel.category = ztype;
    sel.playlist = pl;
    sel.playlistName = meta ? meta->nameEn : std::string();
    sel.source = source.empty() ? "default" : source;
    sel.roomMap = source == "room";
    sel.resolved = resolved;
    sel.rotated = rotated;
    sel.rotationIndex = rotationIx;
    return sel;
}

std::string HandoffRotationAdvance(Server &s, const Selection &sel)
{
    HandoffState &h = H(s);
    if (!sel.rotated || sel.playlist < 0 || !h.playlists.count(sel.playlist)) return "";
    Rotation live = LiveRotation(s, h.playlists[sel.playlist]);
    if (live.empty()) return "";
    int n = (int)live.size(), cur = h.rotation.count(sel.playlist) ? h.rotation[sel.playlist] % n : 0, next = (cur + 1) % n;
    h.rotation[sel.playlist] = next;
    return Fmt("playlist %d round-robin advanced %d -> %d of %d (next: %s)", sel.playlist, cur + 1, next + 1, n, live[(size_t)next].first.c_str());
}

static bool SafeToken(const std::string &v)
{
    if (v.empty() || v.size() > 48) return false;
    for (size_t i = 0; i < v.size(); i++)
        if (!IdentChar((unsigned char)v[i])) return false;
    return true;
}

bool HandoffSelectionSafe(const Selection &sel, std::string *why)
{
    const char *bad = !SafeToken(sel.map) ? "map" : !SafeToken(sel.sessionMap) ? "session_map" : !SafeToken(sel.type) ? "type" :
                      !SafeToken(sel.gametype) ? "gametype" : NULL;
    if (bad && why) *why = std::string(bad) + " is not a bare identifier";
    return !bad;
}

std::string Selection::Summary() const
{
    return Fmt("map=%s session_map=%s type=%s gametype=%s playlist=%d %s via %s", map.c_str(), sessionMap.c_str(), type.c_str(),
               gametype.c_str(), playlist, resolved ? "RESOLVED" : "DEFAULTED", source.c_str());
}

/* ---- bit plumbing (bdBitBuffer, LSB first) ---- */

void MsgWriter::Bits(uint64_t value, int n)
{
    for (int i = 0; i < n; i++) {
        if ((pos >> 3) >= buf.size()) buf += '\0';
        if ((value >> i) & 1) buf[pos >> 3] = (char)((unsigned char)buf[pos >> 3] | (1u << (pos & 7)));
        pos++;
    }
}
void MsgWriter::Raw(const Bytes &data)
{
    for (size_t i = 0; i < data.size(); i++) Bits((unsigned char)data[i], 8);
}
void MsgWriter::Cstr(const std::string &s)
{
    Raw(s);
    Bits(0, 8);
}

uint64_t MsgReader::Bits(int n)
{
    uint64_t v = 0;
    for (int i = 0; i < n; i++) {
        size_t byte = pos >> 3;
        if (byte >= b.size()) { eof = true; return 0; }
        v |= (uint64_t)(((unsigned char)b[byte] >> (pos & 7)) & 1) << i;
        pos++;
    }
    return v;
}
std::string MsgReader::Cstr()
{
    std::string out;
    for (;;) {
        unsigned c = (unsigned)Bits(8);
        if (eof || !c) return out;
        out += (char)c;
    }
}

/* ---- the dwRoomInfo record (reader C1 0x708d20, writer 0x708ab0) ----
 * cstr gametype, cstr MAP, u8 flag0c, u8 flag0d, u8 maxPlayers, cstr hostName, u8 flag14, cstr extra,
 * cstr altGametype, u32, u8 x3, u64, u8, then flag0c != 0xff ? u8 u8 : u32 24 + 24 bytes.
 * Every value but the map and gametype is the dwRoomInfo constructor's own default. */
static void WriteRoomInfo(MsgWriter &w, const std::string &map, const std::string &gametype, int flag0c)
{
    w.Cstr(gametype);
    w.Cstr(map);
    w.Bits((unsigned)flag0c, 8);
    w.Bits(0, 8);
    w.Bits(18, 8); /* maxPlayers */
    w.Cstr("");    /* hostName */
    w.Bits(1, 8);  /* flag14 */
    w.Cstr("");    /* extra */
    w.Cstr("");    /* altGametype */
    w.Bits(0, 32);
    w.Bits(0, 8);
    w.Bits(0, 8);
    w.Bits(0, 8);
    w.Bits(0, 64);
    w.Bits(0, 8);
    if ((flag0c & 0xFF) != 0xFF) {
        w.Bits(0xFF, 8);
        w.Bits(0xFF, 8);
    } else {
        w.Bits(24, 32);
        w.Raw(Bytes(24, '\0'));
    }
}

Bytes BuildRoomInfoRecord(const std::string &map, const std::string &gametype, int flag0c)
{
    MsgWriter w;
    w.Bits(0, 1);  /* the lone leading bool bit */
    w.Bits(15, 8); /* typeId: dwRoomInfo update */
    WriteRoomInfo(w, map, gametype, flag0c);
    return w_u32_raw((uint32_t)w.buf.size()) + w.buf;
}

Bytes BuildJoinResponseRecord(const std::string &map, const std::string &gametype)
{
    MsgWriter w;
    w.Bits(0, 1);
    w.Bits(5, 8);  /* typeId: JoinResponse */
    w.Bits(0, 32); /* code: must be 0 or the reader stops */
    w.Bits(0, 32); /* memberCount 0: a member in here is fatal on C1 (Com_Error 702) */
    WriteRoomInfo(w, map, gametype, 0xFF);
    return w_u32_raw((uint32_t)w.buf.size()) + w.buf;
}

Bytes MapPushPayload(const Bytes &rec, uint64_t roomId, uint64_t sender, const std::string &senderName, uint32_t trailer)
{
    Bytes msg(1, (char)LOBBY_SERVICE_PUSH_MESSAGE);
    msg += w_u32(7);
    msg += w_u64(sender);
    msg += w_name(senderName, 64);
    msg += w_u64(roomId);
    msg += w_blob(rec);
    msg += w_u32(trailer);
    return msg;
}

static uint32_t Le32(const Bytes &b, size_t at)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)(unsigned char)b[at + (size_t)i] << (8 * i);
    return v;
}

static bool ReadRoomInfo(MsgReader &r, RoomInfo &out)
{
    out.gametype = r.Cstr();
    out.map = r.Cstr();
    out.flag0c = (int)r.Bits(8);
    out.flag0d = (int)r.Bits(8);
    out.maxPlayers = (int)r.Bits(8);
    out.hostName = r.Cstr();
    return !r.eof;
}

/* startgame_map.scan_room_info: the client's own dwRoomInfo records inside one inbound batch. */
std::vector<RoomInfo> ScanRoomInfo(const Bytes &blob)
{
    std::vector<RoomInfo> out;
    size_t off = 0;
    while (off + 4 <= blob.size()) {
        uint32_t n = Le32(blob, off);
        size_t start = off + 4, avail = blob.size() - start;
        Bytes body = blob.substr(start, n < avail ? n : avail);
        off = n < avail ? start + n : blob.size();
        if (body.empty() || n == 0 || n > blob.size()) break;
        MsgReader br(body);
        br.Bits(1);
        int tid = (int)br.Bits(8);
        if (br.eof) continue;
        if (tid == 15) { /* a bare room-info record */
            RoomInfo ri;
            if (ReadRoomInfo(br, ri)) out.push_back(ri);
            continue;
        }
        if (tid != 1) continue; /* not a manifold */
        uint32_t count = (uint32_t)br.Bits(32);
        if (br.eof || count > 256) continue;
        for (uint32_t k = 0; k < count; k++) {
            if (br.Bits(1)) br.Bits(64); /* target id */
            uint32_t ln = (uint32_t)br.Bits(32);
            if (br.eof || ln == 0 || (uint64_t)ln * 8 > br.Left()) break;
            Bytes sub;
            for (uint32_t x = 0; x < ln; x++) sub += (char)br.Bits(8);
            MsgReader sb(sub);
            sb.Bits(1);
            int st = (int)sb.Bits(8);
            if (sb.eof) break;
            if (st != 15) continue;
            RoomInfo ri;
            if (!ReadRoomInfo(sb, ri)) break;
            out.push_back(ri);
        }
    }
    return out;
}

/* dw_rooms._room_batch_typeids: [u32 len][body]*, body = bit + u8 typeId; typeId 1 is a manifold of
 * { bit hasTarget [u64], u32 len, len bytes }. The first unparseable byte ends the walk. */
std::set<int> RoomBatchTypeIds(const Bytes &blob)
{
    std::set<int> out;
    size_t p = 0;
    while (p + 4 <= blob.size()) {
        uint32_t n = Le32(blob, p);
        size_t start = p + 4, avail = blob.size() - start;
        Bytes body = blob.substr(start, n < avail ? n : avail);
        p = n < avail ? start + n : blob.size();
        if (body.empty()) continue;
        MsgReader br(body);
        br.Bits(1);
        int t = (int)br.Bits(8);
        if (br.eof) return out;
        if (t != 1) {
            out.insert(t);
            continue;
        }
        uint32_t count = (uint32_t)br.Bits(32);
        if (br.eof) return out;
        for (uint32_t k = 0; k < count; k++) {
            if (br.Bits(1)) br.Bits(64);
            uint32_t ln = (uint32_t)br.Bits(32);
            if (br.eof || (uint64_t)ln * 8 > br.Left()) return out;
            Bytes sub;
            for (uint32_t x = 0; x < ln; x++) sub += (char)br.Bits(8);
            if (sub.empty()) continue;
            MsgReader sb(sub);
            sb.Bits(1);
            int st = (int)sb.Bits(8);
            if (sb.eof) return out;
            out.insert(st);
        }
    }
    return out;
}

} // namespace dw

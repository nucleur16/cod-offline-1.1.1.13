/*
 * dw_devpanel.cpp - the in-room developer panel (demonware/dw_devpanel.py), served as the Python
 * lobby serves it with CODOL_OFFLINE=1.
 *
 * C1's room view shows the host a dedicated-server list, "Start DS" and "Start LS". The list is ONE
 * findSessions (21/5) whose fourth Int32 criterion is 15; Start DS sends the global instant message
 * "mmbook" to the row's first u64, expects "mmbook accepted" back (push 21), then sends
 * "mmstart <console commands>" to the server and START_GAME to its own room, and connects to the
 * row's address. The lobby plays the server: it answers the booking and starts the match host.
 *
 * Push 21 (dispatcher 0x00A32841 case 0x00A3367D): u64 sender, string(0x40) name, blob(0x1000); the
 * handler 0x007022E0 reads the blob through an UNTYPED bdByteBuffer, so it holds [u32 len][text].
 */
#include <string.h>
#include "dw.h"
#include "dw_state.h"
#include "dw_rooms.h"

namespace dw {

static const uint64_t kDsUidBase = 0x5348315800000000ULL; /* "SH1X" + the port */
enum { PUSH_GLOBAL_INSTANT_MESSAGE = 21, DS_SEARCH_CRITERION = 15, MSG_MAX = 0x400 };

bool DevPanelIsDsSearch(const Vals &args)
{
    int n = 0;
    for (size_t i = 0; i < args.size(); i++)
        if (args[i].tag == T_INT32 && n++ == 3) return args[i].u == DS_SEARCH_CRITERION;
    return false;
}

/* -> the port a panel id names, or 0. */
static int PortOf(const Val *uid)
{
    if (!uid) return 0;
    uint64_t port = uid->u - kDsUidBase;
    return (uid->u >= kDsUidBase && port >= 1 && port <= 65535) ? (int)port : 0;
}

/* One row per match host the lobby would start on.
 *  - differential: the Python lobby's single instance, which never answered a status poll.
 *  - the product: the embedded host, when the owner ticked Local Play; no row otherwise (there is
 *    no server to book). */
std::vector<DsRow> DevPanelServers(Server &s, Conn &c)
{
    std::vector<DsRow> rows;
    DsRow row;
    if (s.cfg.differential) {
        row.port = s.cfg.mmHostPort;
        row.host = Fmt("sh1x:%d", row.port);
        row.room = "(no status)";
    } else {
        LocalRoute route;
        if (!s.host->GetLocalRoute(ConnUid(s, c), route) || !route.enabled || route.port < 1 || route.port > 65535) return rows;
        row.port = route.port;
        row.host = Fmt("local:%d", row.port);
        row.room = route.map.empty() ? std::string("(local play)") : route.map + " " + route.gametype;
    }
    row.uid = kDsUidBase + (uint64_t)row.port;
    rows.push_back(row);
    return rows;
}

static Bytes Push21(uint64_t sender, const std::string &name, const std::string &text)
{
    Bytes raw = text.substr(0, MSG_MAX), msg(1, (char)LOBBY_SERVICE_PUSH_MESSAGE);
    msg += w_u32(PUSH_GLOBAL_INSTANT_MESSAGE);
    msg += w_u64(sender);
    msg += w_name(name, 64);
    msg += w_blob(w_u32_raw((uint32_t)raw.size()) + raw);
    return msg;
}

/* Every argument that can carry an instant-message body, in the order the Python decode tries them. */
static void AddText(std::vector<std::string> &out, const Bytes &raw)
{
    size_t z = raw.find('\0');
    Bytes t = z == Bytes::npos ? raw : raw.substr(0, z);
    if (!t.empty()) out.push_back(t);
}

static std::vector<std::string> Texts(const Vals &args)
{
    std::vector<std::string> out;
    for (size_t i = 0; i < args.size(); i++) {
        const Val &v = args[i];
        if (v.isStr() && !v.s.empty()) {
            AddText(out, v.s);
        } else if (v.tag == T_BLOB && !v.s.empty()) {
            const Bytes &b = v.s;
            if (b.size() >= 4) { /* untyped blob: [u32 len][text] */
                uint32_t n = 0;
                for (int k = 0; k < 4; k++) n |= (uint32_t)(unsigned char)b[(size_t)k] << (8 * k);
                if (n > 0 && n <= b.size() - 4) AddText(out, b.substr(4, n));
            }
            if (b[0] == 0x13) { /* a typed stream holding blobs; a pad byte that cannot end the walk early */
                Vals inner = ParseValues(b, (uint8_t)((unsigned char)b[b.size() - 1] + 1));
                for (size_t k = 0; k < inner.size(); k++)
                    if (inner[k].tag == T_BLOB && !inner[k].s.empty()) AddText(out, inner[k].s);
            }
            AddText(out, b);
        }
    }
    return out;
}

/* The map an "mmstart sv_resetServer; ...; map <map>; ..." command line names ("" when none). */
static std::string MmstartMap(const std::string &text)
{
    std::string rest = text.size() > 7 ? text.substr(7) : std::string(), found;
    size_t at = 0;
    while (at <= rest.size()) {
        size_t e = rest.find(';', at);
        if (e == std::string::npos) e = rest.size();
        const char *p = rest.c_str() + at, *end = rest.c_str() + e;
        while (p < end && (*p == ' ' || (*p >= 9 && *p <= 13))) p++;
        size_t klen = 0;
        if ((size_t)(end - p) >= 6 && !memcmp(p, "devmap", 6)) klen = 6;
        else if ((size_t)(end - p) >= 3 && !memcmp(p, "map", 3)) klen = 3;
        if (klen) {
            const char *q = p + klen, *v;
            while (q < end && (*q == ' ' || (*q >= 9 && *q <= 13))) q++;
            v = q;
            while (q < end && !(*q == ' ' || (*q >= 9 && *q <= 13))) q++;
            if (v > p + klen && q > v) found.assign(v, (size_t)(q - v)); /* the last one wins, as dict assignment does */
        }
        at = e + 1;
    }
    return found;
}

Reply DevPanelInstantMessage(Server &s, Conn &c, int task, const Vals &args)
{
    std::vector<std::string> texts = Texts(args);
    const std::string *text = NULL;
    for (size_t i = 0; i < texts.size() && !text; i++)
        if (texts[i].size() >= 2 && texts[i][0] == 'm' && texts[i][1] == 'm') text = &texts[i];
    if (!text) return Reply(); /* not an instant message the panel cares about */
    const Val *to = First(args, T_UINT64);
    size_t we = 0;
    while (we < text->size() && !((*text)[we] == ' ' || ((*text)[we] >= 9 && (*text)[we] <= 13))) we++;
    std::string word = text->substr(0, we);
    Reply rep = Reply::Ok(Fmt("INSTANT MESSAGE task=%d to=%llu '%s'", task, (unsigned long long)(to ? to->u : 0), text->substr(0, 160).c_str()));

    int port = PortOf(to);
    if (!port) {
        /* Addressed to another live player: relay it as a push 21, as Demonware does. */
        if (to && to->u) {
            for (std::map<int, Conn *>::iterator it = s.conns.begin(); it != s.conns.end(); ++it) {
                Conn *peer = it->second;
                if (peer->idx == c.idx || ConnUid(s, *peer) != to->u) continue;
                peer->Queue(Push21(ConnUid(s, c), ConnName(s, c), *text), "push 21 instant message '" + word + "'");
                rep.note += Fmt(" -> relayed to conn#%d", peer->idx);
                return rep;
            }
        }
        /* Addressed to nobody we know (a stale list): treat it as the first known server. */
        std::vector<DsRow> rows = DevPanelServers(s, c);
        if (rows.empty()) {
            rep.note += " -> no server known, dropped";
            return rep;
        }
        port = rows[0].port;
        rep.note += Fmt(" (unknown recipient -> :%d)", port);
    }
    std::string hostName = Fmt(s.cfg.differential ? "sh1x:%d" : "local:%d", port);
    uint64_t sender = PortOf(to) == port ? to->u : kDsUidBase + (uint64_t)port;

    if (word == "mmbook") {
        c.Queue(Push21(sender, hostName, "mmbook accepted"), "push 21 instant message 'mmbook accepted' from " + hostName);
        rep.note += Fmt(" -> DS :%d BOOKED, answering 'mmbook accepted'", port);
        return rep;
    }
    if (word == "mmstart") {
        /* The map it names is the room's pick; resolve exactly as a Ready press does and start the
         * match host. The CLIENT sends START_GAME to its room and connects to the row's address. */
        std::string map = MmstartMap(*text);
        if (!map.empty()) {
            std::string gt;
            bool hasGt = HandoffRoomLatchGametype(s, gt);
            HandoffLatchRoom(s, map, hasGt ? &gt : NULL, 0, "dev panel mmstart");
        }
        Selection sel = ResolveSelection(s, &c, "ready");
        rep.note += " | selection " + sel.map + "/" + sel.gametype + "/" + sel.type;
        MatchRoute route = RouteMatch(s, c, sel);
        rep.note += " | " + route.note;
        if (route.start && route.local) {
            const Vals *desc = Desc(s, &c);
            Bytes key;
            for (size_t i = 0; desc && i < desc->size(); i++)
                if ((*desc)[i].tag == T_BLOB && (*desc)[i].s.size() == 16) { key = (*desc)[i].s; break; }
            if (key.size() == 16) rep.note += SeckeyStartgameWrite(s, &c, key);
            if (!NotifyLocalStart(s, c, sel, route, c.seckey.size() == 16 ? c.seckey : key, rep.note)) route.start = false;
        }
        rep.note += Fmt(" -> DS :%d %s", port, route.start ? "started" : "NOT started");
        return rep;
    }
    /* mmgod / mmkskills / mmunlimitedammo / mmendgame / mmzmjumpround: acknowledged, logged. */
    rep.note += Fmt(" -> DS :%d has no handler for '%s' (ignored)", port, word.c_str());
    return rep;
}

} // namespace dw

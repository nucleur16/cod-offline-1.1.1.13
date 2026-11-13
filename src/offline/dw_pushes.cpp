/*
 * dw_pushes.cpp - the mid-session pushes the dispatcher triggers after a service-80 task.
 * Port of build_reward_push / _maybe_push_inventory and build_ticker_push / _maybe_push_ticker in
 * demonware/dw_rooms.py, with the Python lobby's defaults (CODOL_INV_PUSH unset = 0).
 *
 * Push 40 (onTencentPlayerReward, C1 0x00702A40 via dispatcher case 0x00A33B10): makes the client
 * send 80/10 getInventory. The automatic push is OFF by default - the entitlements ride in the
 * service-80 task-30 reply instead - so only the manual trigger file fires it.
 * Push 35 (group-messaging broadcast -> the ticker 0x005BB560): once per front-end entry (task 30)
 * when a ticker notice is live. The notices table (notices/lobby_notices.py over the lobby database)
 * is not ported: it is EMPTY, which in the Python lobby means "no live notice" and no push at all.
 */
#include "dw.h"
#include "dw_state.h"
#include "dw_rooms.h"

namespace dw {

enum {
    MARKETPLACE_SERVICE = 80,
    MP_GET_ENTITLEMENT = 30, /* the front-end-entry beacon: login and every return from a match */
    PUSH_GROUP_MESSAGING_BROADCAST = 35,
    PUSH_TENCENT_PLAYER_REWARD = 40,
    INV_PUSH_MAX_BLOB = 0x200,
    TICKER_MAX_TEXT = 0x3FA,
};
/* The shipped text of SF_UI_UPDATE_PLAYER_DATA ("updating player info"), UTF-8: the dialog the
 * client shows for any reward text cannot be suppressed, only worded. */
static const char kInvPushText[] = "\xE6\x9B\xB4\xE6\x96\xB0\xE7\x8E\xA9\xE5\xAE\xB6\xE4\xBF\xA1\xE6\x81\xAF\xE2\x80\xA6";

/* The exact field walk of push-dispatcher case 40. The blob is base64 TEXT, its length is on the
 * wire twice (a standalone u32, then the blob's own), and a zero-length blob skips getInventory. */
static Bytes BuildRewardPush(const std::string &text, std::string &note)
{
    Bytes blob = Base64(text).substr(0, INV_PUSH_MAX_BLOB);
    if (blob.empty()) blob = "QQ==";
    Bytes msg(1, (char)LOBBY_SERVICE_PUSH_MESSAGE);
    msg += w_u32(PUSH_TENCENT_PLAYER_REWARD);
    msg += w_u64(0); /* -> vtable+0x80, a no-op */
    msg += w_u64(0);
    msg += w_u64(0); /* -> onTencentPlayerReward args 1/2 */
    msg += w_u32(0); /* -> arg 3 */
    msg += w_str("CODOL", 32);
    msg += w_u32((uint32_t)blob.size()); /* the DECLARED size (0x00A33C02) */
    msg += w_blob(blob);
    note = Fmt("push 40 onTencentPlayerReward blob=%uB -> client sends 80/10 getInventory", (unsigned)blob.size());
    return msg;
}

/* The exact field walk of push-dispatcher case 35: the outer blob is itself a bdByteBuffer stream
 * holding ONE nested blob, the text (`<MSG P=.. T=.. />body`, T a FUTURE epoch). */
static Bytes BuildTickerPush(const Bytes &text, std::string &note)
{
    Bytes raw = text.substr(0, TICKER_MAX_TEXT);
    if (raw.empty()) raw = " "; /* a zero-length text returns at 0x005BB586 */
    Bytes msg(1, (char)LOBBY_SERVICE_PUSH_MESSAGE);
    msg += w_u32(PUSH_GROUP_MESSAGING_BROADCAST);
    msg += w_u64(0);
    msg += w_name("CODOL", 64);
    msg += w_blob(w_blob(raw));
    note = Fmt("push 35 group-messaging broadcast -> ticker (%uB text, %uB frame)", (unsigned)raw.size(), (unsigned)msg.size());
    return msg;
}

/* lobby_notices.render_ticker over an EMPTY notices table: "ticker: no live notice". */
static bool RenderTicker(Server &, Bytes &) { return false; }

/* An operator poke: a file of this name in the resources override folder re-fires a push once. */
static bool TakeTrigger(Server &s, const char *name)
{
    if (s.cfg.resources.empty()) return false;
    std::string path = PathJoin(s.cfg.resources, name);
    if (!FileExists(path)) return false;
    RemoveFile(path);
    return true;
}

static void MaybePushInventory(Server &s, Conn &c, ConnRoom &cr)
{
    cr.mpTasks++;
    if (!TakeTrigger(s, "repush_inventory")) return; /* the automatic push is off (CODOL_INV_PUSH=0) */
    cr.invPushed = true;
    cr.invPushAt = s.host->Mono();
    cr.invPushes++;
    std::string note;
    Bytes msg = BuildRewardPush(kInvPushText, note);
    c.Queue(msg, note + Fmt(" (MANUAL re-trigger; push #%d on this connection)", cr.invPushes));
}

static void MaybePushTicker(Server &s, Conn &c, ConnRoom &cr, int task)
{
    double now = s.host->Mono();
    bool forced = TakeTrigger(s, "repush_ticker");
    if (!forced) {
        if (task != MP_GET_ENTITLEMENT) return;
        if (cr.tickPushAt != 0 && now - cr.tickPushAt < 20.0) return;
    }
    Bytes data;
    if (!RenderTicker(s, data)) return;                 /* nothing live: say nothing */
    if (!forced && data == cr.tickText) return;         /* already on screen this entry */
    std::string note;
    Bytes msg = BuildTickerPush(data, note);
    cr.tickPushAt = now;
    cr.tickText = data;
    cr.tickPushes++;
    c.Queue(msg, note + Fmt(" (%s; push #%d)", forced ? "MANUAL" : "front-end entry", cr.tickPushes));
}

void AfterTask(Server &s, Conn &c, int service, int task)
{
    if (service != MARKETPLACE_SERVICE) return;
    ConnRoom &cr = ConnState(s, c);
    MaybePushInventory(s, c, cr);
    MaybePushTicker(s, c, cr, task);
}

} // namespace dw

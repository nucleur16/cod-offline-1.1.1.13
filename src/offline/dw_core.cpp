/*
 * dw_core.cpp - connections, framing, auth and task dispatch of the offline Demonware lobby.
 * Port of dw-capture.py (serve / _dispatch_frame / build_task_reply) and demonware/dw_auth.py.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "dw.h"
#include "dw_state.h"

namespace dw {

Config::Config()
    : mmHostIp("127.0.0.1"), mmHostPort(17760), mmHostUid(1000001), fixedTime(0), stunPort(3074), stunAltPort(3078), stunMappedPort(3074),
      differential(false) {}

int64_t Host::Now() { return (int64_t)time(NULL); }
double Host::Mono() { return (double)clock() / CLOCKS_PER_SEC; }

Conn::Conn()
    : idx(0), server(NULL), preambleSeen(false), closing(false), outMc(1), txn(1), uid(0), hasDesc(false), opened(0),
      lastRx(0), lastTx(0), room(0) {}

void Conn::Queue(const Bytes &msg, const std::string &note)
{
    Push p;
    p.msg = msg;
    p.note = note;
    push.push_back(p);
}

Server::Server(const Config &config, Host *h) : cfg(config), host(h), st(NULL), nextConn(0) { StateInit(*this); }

Server::~Server()
{
    for (std::map<int, Conn *>::iterator it = conns.begin(); it != conns.end(); ++it) delete it->second;
    conns.clear();
    StateFree(*this);
}

int64_t Server::Now() const { return cfg.fixedTime ? cfg.fixedTime : host->Now(); }
void Server::Log(const std::string &line) const { host->Log(line); }

Conn *Server::Open()
{
    Conn *c = new Conn();
    c->idx = ++nextConn;
    c->server = this;
    c->opened = c->lastRx = c->lastTx = host->Mono();
    conns[c->idx] = c;
    Log(Fmt("conn#%d OPEN", c->idx));
    return c;
}

void Server::Close(Conn *c)
{
    if (!c) return;
    Log(Fmt("conn#%d CLOSED", c->idx));
    conns.erase(c->idx);
    OnConnClosed(*this, *c);
    delete c;
}

void Server::FlushPushes(Conn *c)
{
    for (size_t i = 0; i < c->push.size(); i++) {
        uint32_t mc = c->outMc++;
        c->outbuf += EncryptFrame(c->push[i].msg, mc);
        Log(Fmt("conn#%d    -> PUSH %s outMc=%u", c->idx, c->push[i].note.c_str(), mc));
    }
    c->push.clear();
}

/* ---- bdAuthTicket and the two connect replies (demonware/dw_auth.py) ---- */

struct BitWriter { /* LSB-first, as bdBitBuffer::writeBits */
    Bytes buf;
    size_t pos;
    BitWriter() : pos(0) {}
    void Bits(uint64_t value, int n)
    {
        for (int i = 0; i < n; i++) {
            if ((pos >> 3) >= buf.size()) buf += '\0';
            if ((value >> i) & 1) buf[pos >> 3] = (char)((unsigned char)buf[pos >> 3] | (1u << (pos & 7)));
            pos++;
        }
    }
    void Raw(const Bytes &data)
    {
        for (size_t i = 0; i < data.size(); i++) Bits((unsigned char)data[i], 8);
    }
};

static void PutLe(unsigned char *p, uint64_t v, int n)
{
    for (int i = 0; i < n; i++) p[i] = (unsigned char)(v >> (8 * i));
}

/* 128-byte bdAuthTicket, 3DES-encrypted exactly as the client decrypts it (build_ticket). */
static Bytes BuildTicket(uint32_t ivSeed)
{
    unsigned char t[128], key[24], digest[24], seed[4];
    memset(t, 0, sizeof(t));
    PutLe(t + 0, 0xEFBDADDE, 4);        /* BD_MAGIC_NUMBER as this build reads it */
    t[4] = 0;                           /* m_type = USER_TO_SERVICE */
    PutLe(t + 5, 1002, 4);              /* titleID, == dwauth.ini line 5 (hex 3ea) */
    PutLe(t + 9, 0, 4);                 /* timeIssued */
    PutLe(t + 13, 0x7FFFFFFF, 4);       /* timeExpires */
    PutLe(t + 17, 1002, 8);             /* licenseID */
    PutLe(t + 25, 1, 8);                /* userID */
    memcpy(t + 33, "offline", 7);       /* username[64] */
    for (int i = 0; i < 24; i++) t[97 + i] = (unsigned char)i; /* sessionKey[24]: ours to choose */
    t[121] = 0x09; t[122] = 0x04; t[123] = 0x00;
    DwTiger(t, 0x79, digest);
    memcpy(t + 124, digest, 4);
    DwTiger("simplePassword", 14, key);  /* measured: Tiger(this) == the client's m_userKey */
    PutLe(seed, ivSeed, 4);
    DwTiger(seed, 4, digest);
    Bytes out(128, '\0');
    DwDes3Cbc(key, digest, 0, t, (unsigned char *)&out[0], 128);
    return out;
}

/* body = [u8 encType][message]; message[0] == 10 (BD_AUTH_ACCOUNT_FOR_MMP_REQUEST). */
static bool BuildAuthReply(const Bytes &body, Bytes &frame, std::string &why)
{
    if (body.size() < 3) { why = "frame too short"; return false; }
    if ((unsigned char)body[1] != 10) { why = "msgType is not BD_AUTH_ACCOUNT_FOR_MMP_REQUEST"; return false; }
    if (body[0] != 0) { why = "auth request is not plaintext"; return false; }
    /* ivSeed sits after: seed byte (8) + writeBool (1) + 5-bit tag */
    uint32_t ivSeed = 0;
    for (int i = 0; i < 32; i++) {
        size_t p = 8 + 1 + 5 + (size_t)i;
        if ((p >> 3) + 1 < body.size() && (((unsigned char)body[1 + (p >> 3)] >> (p & 7)) & 1)) ivSeed |= 1u << i;
    }
    BitWriter w;
    w.Bits(11, 8);      /* BD_AUTH_ACCOUNT_FOR_MMP_REPLY */
    w.Bits(1, 1);       /* writeBool(true) */
    w.Bits(8, 5);       /* UInt32 tag */
    w.Bits(700, 32);    /* BD_AUTH_NO_ERROR */
    w.Bits(8, 5);
    w.Bits(ivSeed, 32);
    Bytes ticket = BuildTicket(ivSeed);
    w.Raw(ticket);
    w.Raw(ticket);
    Bytes out(1, '\0'); /* ENC_PLAINTEXT */
    out += w.buf;
    frame = w_u32_raw((uint32_t)out.size()) + out;
    why = Fmt("auth errorCode=700 ivSeed=0x%08X", ivSeed);
    return true;
}

/* BD_LOBBY_CONNECTION_ID (4): [4][tag 10][u64 connId], encrypted with messageCount 0. */
static bool BuildLobbyReply(Server &s, Conn &c, const Bytes &body, Bytes &frame, std::string &why)
{
    if (body.size() < 3) { why = "frame too short"; return false; }
    Bytes message(1, (char)4);
    message += w_u64(1);
    OnLobbyHandshake(s, c);
    frame = EncryptFrame(message, 0);
    why = "BD_LOBBY_CONNECTION_ID connId=1";
    return true;
}

/* ---- the small services that live in dw-capture.py itself ---- */

Reply TitleUtilitiesTask(Server &s, Conn &, int task, const Vals &)
{
    if (task == 6) { /* getServerTime: ONE record carrying ONE tag-8 UInt32 of Unix seconds */
        int64_t now = s.Now();
        Reply r = Reply::Ok(Fmt("getServerTime -> %lld", (long long)now));
        r.results.push_back(w_u32((uint32_t)now));
        return r;
    }
    return Reply();
}

Reply MessagingTask(Server &s, Conn &c, int task, const Vals &args)
{
    Reply dev = DevPanelInstantMessage(s, c, task, args);
    if (dev.err != DECLINED) return dev;
    if (task == 1 || task == 5) return Reply::Ok("messaging paged fetch -- answered EMPTY (record shape in .tvm0)");
    return Reply();
}

Reply TencentTask(Server &, Conn &, int task, const Vals &)
{
    if (task == 4) return Reply::Ok("registerCodoID -- status ACK, empty success");
    if (task == 10) return Reply::Ok("claimRewards -- status ACK, empty success");
    return Reply();
}

Reply ChatTask(Server &s, Conn &c, int task, const Vals &args)
{
    const Val *ctype = First(args, T_UCHAR8), *cid = First(args, T_UINT64);
    std::pair<int, uint64_t> key(ctype ? (int)ctype->u : 0, cid ? cid->u : 0);
    if (task == 1)
        return Reply::Ok(Fmt("CHAT state fetch channel=(%d,%llu) backlog=%u -- answered EMPTY", key.first,
                             (unsigned long long)key.second, (unsigned)s.st->chat[key].size()));
    if (task == 3) { /* typed player chat: Blob = [u32 0][u32 len LE][payload]; stored, not relayed */
        const Val *blob = First(args, T_BLOB);
        Bytes payload;
        if (blob && blob->s.size() >= 8) {
            uint32_t len = 0;
            for (int i = 0; i < 4; i++) len |= (uint32_t)(unsigned char)blob->s[4 + i] << (8 * i);
            payload = blob->s.substr(8, len);
        }
        std::vector<Bytes> &box = s.st->chat[key];
        box.push_back(payload);
        if (box.size() > 50) box.erase(box.begin(), box.begin() + (box.size() - 50));
        (void)c;
        return Reply::Ok(Fmt("CHAT send channel=(%d,%llu) stored (%u in channel; not relayed)", key.first,
                             (unsigned long long)key.second, (unsigned)box.size()));
    }
    return Reply();
}

/* ---- dispatch ---- */

static const char *ServiceName(int svc)
{
    switch (svc) {
    case 1: return "LOBBY_GATEWAY"; case 2: return "USER_MGMT"; case 3: return "TEAM_MGMT"; case 4: return "STATS";
    case 6: return "MESSAGING"; case 7: return "AUTH"; case 8: return "PROFILE_MGMT"; case 9: return "FRIENDS";
    case 10: return "STORAGE"; case 11: return "CHANNEL_CHAT"; case 12: return "TITLE_UTILITIES"; case 15: return "KEY_ARCHIVE";
    case 18: return "BANDWIDTH_TEST"; case 21: return "MATCHMAKING"; case 23: return "COUNTER"; case 28: return "GROUP";
    case 50: return "CONTENT_STREAMING"; case 52: return "TAGS"; case 55: return "VOTERANK"; case 58: return "POOLED_STORAGE";
    case 71: return "TENCENT"; case 80: return "MARKETPLACE"; case 102: return "CHAT";
    }
    return "?";
}

static bool BuildTaskReply(Server &s, Conn &c, const Bytes &payload, Bytes &frame, std::string &why)
{
    uint32_t mc;
    int svc;
    Bytes body;
    if (!DecryptClientFrame(payload, mc, svc, body)) { why = "encrypted frame could not be decrypted"; return false; }
    if (body.size() < 2 || body[0] != 3) { why = Fmt("service %d request does not start with a UChar8 taskId", svc); return false; }
    int task = (unsigned char)body[1];
    Vals all = ParseValues(body, (uint8_t)(mc & 0xFF));
    Vals args(all.begin() + (all.empty() ? 0 : 1), all.end());
    s.Log(Fmt("conn#%d    << mc=%u service=%d (%s) task=%d args=%s", c.idx, mc, svc, ServiceName(svc), task,
              FmtValues(args).c_str()));

    /* HD-5: latch the wire identity from the first STORAGE task that carries an owner id. One-shot. */
    if (svc == 10) {
        const Val *owner = First(args, T_UINT64);
        if (owner && owner->u && !c.uid) {
            c.uid = owner->u;
            OnUidLatched(s, c);
        }
    }

    Reply r;
    switch (svc) {
    case 11: r = RoomTask(s, c, task, args); break;
    case 10: r = StorageTask(s, c, task, args); break;
    case 80: r = MarketplaceTask(s, c, task, args); break;
    case 12: r = TitleUtilitiesTask(s, c, task, args); break;
    case 21: r = MatchmakingTask(s, c, task, args); break;
    case 6: r = MessagingTask(s, c, task, args); break;
    case 71: r = TencentTask(s, c, task, args); break;
    case 4: r = StatsTask(s, c, task, args); break;
    case 102: r = ChatTask(s, c, task, args); break;
    }
    int err = r.err;
    if (err == DECLINED) { /* the empty-success fall-through, as the Python lobby answers every unknown task */
        err = BD_NO_ERROR;
        r.results.clear();
        r.note = "UNHANDLED";
    }
    AfterTask(s, c, svc, task);

    uint64_t txn = c.txn++;
    Bytes msg(1, (char)LOBBY_SERVICE_TASK_REPLY);
    msg += w_u64(txn);
    msg += w_u32((uint32_t)err);
    if (err == BD_NO_ERROR) {
        msg += w_u8((unsigned)task);
        msg += w_u32((uint32_t)r.results.size());
        if (!r.results.empty()) {
            msg += w_u32((uint32_t)r.results.size()); /* totalNumResults: read only when numResults != 0 */
            for (size_t i = 0; i < r.results.size(); i++) msg += r.results[i];
        }
    }
    uint32_t outMc = c.outMc++;
    frame = EncryptFrame(msg, outMc);
    why = Fmt("TASK_REPLY svc=%d(%s) task=%d err=%d results=%u txn=%llu outMc=%u  [%s]", svc, ServiceName(svc), task, err,
              (unsigned)r.results.size(), (unsigned long long)txn, outMc, r.note.c_str());
    return true;
}

void Server::Feed(Conn *c, const void *data, size_t n)
{
    c->lastRx = host->Mono();
    c->inbuf.append((const char *)data, n);
    Bytes &buf = c->inbuf;
    for (;;) {
        if (buf.size() < 4) break;
        /* The lobby preamble is NOT framed: bdLobbyServiceImpl::onConnect writes
         * [u32 BD_PROTOCOL_VERSION][u32 receiveBufferSize] with sendRaw(). Never echoed. */
        if (!c->preambleSeen && buf.size() >= 8) {
            uint32_t ver = 0, rbuf = 0;
            for (int i = 0; i < 4; i++) { ver |= (uint32_t)(unsigned char)buf[i] << (8 * i); rbuf |= (uint32_t)(unsigned char)buf[4 + i] << (8 * i); }
            if ((ver == 180 || ver == 200) && (rbuf == 0x10000 || rbuf == 0x40000 || rbuf == 0x80000 || rbuf == 0x100000)) {
                buf.erase(0, 8);
                c->preambleSeen = true;
                continue;
            }
        }
        uint32_t len = 0;
        for (int i = 0; i < 4; i++) len |= (uint32_t)(unsigned char)buf[i] << (8 * i);
        if (len == 0) { /* keepAlive: answer it, as a real peer does */
            buf.erase(0, 4);
            c->outbuf.append("\0\0\0\0", 4);
            FlushPushes(c);
            continue;
        }
        if (len > 0x10000) { /* desync: an insane length can never become a frame by waiting */
            Log(Fmt("conn#%d !! FRAME DESYNC: length 0x%08X -- closing", c->idx, len));
            c->closing = true;
            break;
        }
        if (buf.size() < 4 + (size_t)len) break;
        Bytes payload = buf.substr(4, len);
        buf.erase(0, 4 + (size_t)len);
        Bytes frame;
        std::string why;
        bool ok;
        if (payload[0] == 1) ok = BuildTaskReply(*this, *c, payload, frame, why);
        else if (payload.size() > 1 && payload[1] == 7) ok = BuildLobbyReply(*this, *c, payload, frame, why);
        else ok = BuildAuthReply(payload, frame, why);
        if (ok) {
            c->outbuf += frame;
            Log(Fmt("conn#%d    -> %s", c->idx, why.c_str()));
        } else {
            Log(Fmt("conn#%d    -> no reply built (%s)", c->idx, why.c_str()));
        }
        /* Server-initiated frames go out after the task's own acknowledgement, to every connection
         * a handler queued one on (room fan-out). */
        for (std::map<int, Conn *>::iterator it = conns.begin(); it != conns.end(); ++it) FlushPushes(it->second);
    }
    if (buf.size() > 0x20000) c->closing = true;
}

void Server::Tick()
{
    ServiceTick(*this);
    double now = host->Mono();
    for (std::map<int, Conn *>::iterator it = conns.begin(); it != conns.end(); ++it) {
        Conn *c = it->second;
        /* The client drops a peer it has not heard for 2 x BD_KEEP_ALIVE_PERIOD (80 s): send a bare
         * u32 0 after 20 s of silence, as the Python lobby's 20 s socket timeout does. */
        if (now - c->lastRx >= 20.0 && now - c->lastTx >= 20.0) {
            c->outbuf.append("\0\0\0\0", 4);
            c->lastTx = now;
        }
        if (now - c->lastRx > 600.0) c->closing = true;
        FlushPushes(c);
    }
}

/* ---- bdSDK STUN, types 20/21, 30/31 and the 14/15 keepalive (dw-capture.py stun_reply) ---- */

bool StunReply(const Config &cfg, const Bytes &data, Bytes &out, int &via, std::string &what)
{
    static const unsigned char ip[4] = { 127, 0, 0, 1 }, alt[4] = { 127, 0, 0, 2 };
    if (data.size() < 3) return false;
    int t = (unsigned char)data[0];
    unsigned port = (unsigned)cfg.stunMappedPort, changed = (unsigned)cfg.stunPort;
    via = 0;
    out.assign(1, (char)0);
    out += data.substr(1, 2); /* protocol version echoed */
    if (t == 20) {
        /* The second address pair is the CHANGED-ADDRESS: the alternate IP on the PRIMARY port
         * (sub_A5DCD0 state 2: Open NAT needs IP == expected and port != expected). */
        int flag = data.size() > 3 ? (unsigned char)data[3] : 0;
        out[0] = (char)21;
        out.append((const char *)ip, 4);
        out += (char)(port & 0xFF); out += (char)(port >> 8);
        out.append((const char *)alt, 4);
        out += (char)(changed & 0xFF); out += (char)(changed >> 8);
        via = flag;
        what = Fmt("NAT discovery 20->21 flag=%d", flag);
        return true;
    }
    if (t == 30) {
        uint32_t ipLe = 0x0100007f ^ 0x2112A442;
        unsigned xport = port ^ 0x2112;
        out[0] = (char)31;
        out.append((const char *)ip, 4);
        out += (char)(port & 0xFF); out += (char)(port >> 8);
        for (int i = 0; i < 4; i++) out += (char)(ipLe >> (8 * i));
        out += (char)(xport & 0xFF); out += (char)(xport >> 8);
        what = "IP discovery 30->31";
        return true;
    }
    if (t == 14 && data.size() == 29) { /* keepalive: echo with the observed address in the sentinel slots */
        out = data;
        out[0] = (char)15;
        memcpy(&out[18], ip, 4);
        out[22] = (char)(port & 0xFF); out[23] = (char)(port >> 8);
        memcpy(&out[24], ip, 4);
        what = "keepalive 14->15";
        return true;
    }
    return false;
}

} // namespace dw

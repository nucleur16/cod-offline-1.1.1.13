/* bdDTLS handshake abuse limits (sh1x security audit 2026-10-26).
 *  - A different INIT for an established association is staged: the live
 *    association and its player survive until the staged handshake's
 *    COOKIE_ECHO completes, so a spoofed INIT cannot disconnect anyone.
 *  - Incomplete handshakes are capped per source IP and in total, and a
 *    retransmitted INIT does not extend a pending slot's life.
 *  - Unauthenticated replies (NAT echo, INIT_ACK) are rate limited.
 * The handshake crypto is stubbed; the slot logic is the real code.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sys_main.h"
static int test_hosted;
#define Sys_IsHosted() test_hosted /* network.c: a hosted server (inside the client), or a dedicated one */
#include "network.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

cvar_t *com_developer;
SH1X_ONDEMAND(, serverStatic_t, svs, ); /* server.h: svs is what this pointer points at */
cvar_t *sv_maxclients;
static unsigned clock_ms = 1000;
static int sent, dropped;
static const byte lobby_key[16] = {9, 9, 9, 9};

unsigned long long FrameProfileNow(void) { return 0; }
void FrameProfileEnd(frame_section_t section, unsigned long long start)
{ (void)section; (void)start; }
short ShortSwap(short value)
{ return (short)(((unsigned short)value >> 8) | ((unsigned short)value << 8)); }
time_t Com_GetRealtime(void) { return 1800000000; }
unsigned int Sys_Milliseconds(void) { return clock_ms; }
int AtomicWriteHomeFile(const char *path, const void *data, size_t size)
{ (void)path; (void)data; (void)size; return 1; }
int AtomicWriteFile(const char *path, const void *data, size_t size)
{ (void)path; (void)data; (void)size; return 1; }
void QDECL Com_DPrintf(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void QDECL Com_Printf(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void QDECL Com_PrintWarning(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
const char *NET_AdrToString(netadr_t *a) { (void)a; return "203.0.113.9:17733"; }
qboolean Sys_SendPacket(int length, const void *data, netadr_t *to)
{ (void)length; (void)data; (void)to; ++sent; return qtrue; }
qboolean Sys_RandomBytes(byte *out, int n) { memset(out, 1, (size_t)n); return qtrue; }
void MSG_InitReadOnly(msg_t *msg, byte *data, int length) { (void)msg; (void)data; (void)length; }
__optimize3 __regparm2 void SV_PacketEvent(netadr_t *from, msg_t *msg) { (void)from; (void)msg; }
long FS_SV_FOpenFileRead(const char *name, fileHandle_t *file) { (void)name; *file = 0; return -1; }
int FS_Read(void *buffer, int len, fileHandle_t f) { (void)buffer; (void)len; (void)f; return 0; }
qboolean FS_FCloseFile(fileHandle_t f) { (void)f; return qtrue; }
int AdmissionRequired(void) { return 1; }
/* Not reached here; PE linking resolves every reference of network.c, dropped functions included. */
cvar_t *Cvar_RegisterString(const char *name, const char *value, unsigned short flags, const char *description)
{ (void)name; (void)value; (void)flags; (void)description; return NULL; }
void AdmissionInit(void) { }
int AdmissionIdentify(const uint8_t ip[4], const uint8_t shared_x[28], const uint8_t data_key[24], admission_entry_t *out)
{ (void)ip; (void)shared_x; (void)data_key; (void)out; return 0; }
size_t AssociationSeal(association_t *s, uint8_t *out, size_t cap, const uint8_t *data, size_t n)
{ (void)s; (void)out; (void)cap; (void)data; (void)n; return 0; }
size_t AdmissionCandidates(const uint8_t ip[4], uint8_t keys[][16], size_t max)
{ (void)ip; assert(max >= 1); memcpy(keys[0], lobby_key, 16); return 1; }
/* Stand-ins with the real functions' state contract (ol_handshake.c). */
int HandshakeInit(handshake_t *h, uint8_t *response, const uint8_t *packet, size_t n,
                        const uint8_t id[8], const uint8_t ip[4], uint16_t port, uint16_t tag,
                        const uint8_t key[28], ecdh_rng_t random, void *context)
{
    (void)id; (void)ip; (void)port; (void)tag; (void)key; (void)random; (void)context;
    assert(n == 16);
    memset(response, 0, 38);
    if ((h->pending || h->established) && !memcmp(h->init, packet, 16)) return 2;
    memset(h, 0, sizeof(*h));
    memcpy(h->init, packet, 16); h->pending = 1;
    return 1;
}
int HandshakeCookie(handshake_t *h, uint8_t *response, const uint8_t *packet, size_t n,
                          const uint8_t key[16], ecdh_rng_t random, void *context)
{
    (void)response; (void)key; (void)random; (void)context; assert(n == 189);
    if (h->established) return !memcmp(h->echo, packet, 189) ? 2 : 0;
    if (!h->pending || packet[100] != h->init[6]) return 0; /* echo must match its INIT */
    memcpy(h->echo, packet, 189); h->pending = 0; h->established = 1; h->data.active = 1;
    return 1;
}
int AssociationOpenDetailed(association_t *s, uint8_t *out, size_t cap,
                                  size_t *out_length, const uint8_t *record, size_t length,
                                  const uint8_t candidate[16], const char **reason)
{ (void)s; (void)out; (void)cap; (void)out_length; (void)record; (void)length; (void)candidate;
  if (reason) *reason = "test"; return 0; }
void SV_DropClientNoNotify(client_t *drop, const char *reason) { (void)drop; (void)reason; ++dropped; }

static netadr_t addr(unsigned last, unsigned port)
{
    netadr_t a = {0};
    a.type = NA_IP; a.ip[0] = 203; a.ip[2] = 113; a.ip[3] = (byte)last; a.port = (unsigned short)port;
    return a;
}
static void init_packet(byte p[16], byte nonce)
{
    static const byte id[8] = {0x46, 0, 0, 0, 0, 0, 0, 0};
    memset(p, 0, 16); p[0] = 1; p[1] = 2; p[6] = nonce; p[7] = 1; memcpy(p + 8, id, 8);
}
static void cookie_packet(byte p[189], byte nonce)
{ memset(p, 0, 189); p[0] = 3; p[1] = 2; p[100] = nonce; }
/* A server silent (one long frame) for `silent` ms after it answered an INIT, then C1's 45 COOKIE_ECHOs one second
 * apart (it never sends a second INIT): how many of them it answers. */
static int echoes_answered(unsigned silent)
{
    netadr_t a = addr(1, 30001);
    byte init[16], echo[189];
    int answered = 0, i;
    memset(peers, 0, sizeof(peers));
    clock_ms += 60000; sent = 0;
    init_packet(init, 7); cookie_packet(echo, 7);
    NetworkReceive(&a, init, 16);
    assert(sent == 1);
    clock_ms += silent;
    for (i = 0; i < 45; ++i) {
        int before = sent;
        NetworkReceive(&a, echo, 189);
        answered += sent - before;
        clock_ms += 1000;
    }
    return answered;
}

int main(void)
{
    cvar_t key = {0}, id = {0};
    cvar_t maxclients = {0};
    netadr_t victim = addr(9, 17733), attacker;
    byte init[16], cookie[189];
    peer_t *p;
    unsigned i;

    key.string = "session-key-path"; id.string = "4600000000000000";
    key_path = &key; session_id = &id;
    strcpy(configured_path, key.string); strcpy(configured_id, id.string);
    sessions_loaded = 1;
    sh1x_od_svs = calloc(1, sizeof(*sh1x_od_svs)); /* Sys_OnDemandInit */
    assert(sh1x_od_svs);
    maxclients.integer = 4; sv_maxclients = &maxclients;
    svs.clients[0].state = CS_ACTIVE; svs.clients[0].netchan.remoteAddress = victim;

    /* The victim's association is established by its own INIT/COOKIE. */
    init_packet(init, 1);
    assert(NetworkReceive(&victim, init, 16) == 1);
    cookie_packet(cookie, 1);
    assert(NetworkReceive(&victim, cookie, 189) == 1);
    p = find(&victim, clock_ms, 0);
    /* A new INIT on a fresh slot drops a stale client at that address, as before. */
    assert(p && p->handshake.established && dropped == 1);
    dropped = 0;

    /* A spoofed different INIT from the victim's address: staged, nothing dropped. */
    clock_ms += 2000; sent = 0;
    init_packet(init, 2);
    assert(NetworkReceive(&victim, init, 16) == 1);
    assert(sent == 1 && dropped == 0);
    assert(p->handshake.established && p->has_staged && p->handshake.init[6] == 1);
    /* Without its COOKIE_ECHO the stage lapses; the live association stays. */
    clock_ms += 11000;
    cookie_packet(cookie, 2);
    assert(NetworkReceive(&victim, cookie, 189) == 1);
    assert(!p->has_staged && p->handshake.established && p->handshake.init[6] == 1 && dropped == 0);

    /* A real reconnect completes the staged handshake: then, and only then,
     * the old association is replaced and its player dropped. */
    clock_ms += 2000;
    init_packet(init, 3);
    assert(NetworkReceive(&victim, init, 16) == 1);
    cookie_packet(cookie, 3);
    assert(NetworkReceive(&victim, cookie, 189) == 1);
    assert(!p->has_staged && p->handshake.established && p->handshake.init[6] == 3 && dropped == 1);

    /* One host may hold at most PENDING_PER_IP incomplete handshakes. */
    for (i = 0; i < 10; ++i) {
        clock_ms += 200;
        attacker = addr(50, 40000 + i);
        init_packet(init, (byte)(10 + i));
        NetworkReceive(&attacker, init, 16);
    }
    {
        unsigned pending = 0;
        for (i = 0; i < PEERS; ++i)
            if (peers[i].used && !peers[i].handshake.established) ++pending;
        assert(pending == PENDING_PER_IP);
    }
    /* Resending the same INIT does not keep a pending slot alive. */
    memset(peers, 0, sizeof(peers));
    attacker = addr(60, 40000);
    init_packet(init, 10);
    clock_ms += 2000;
    NetworkReceive(&attacker, init, 16);
    assert(find(&attacker, clock_ms, 0) != NULL);
    clock_ms += 5000; NetworkReceive(&attacker, init, 16);
    clock_ms += 4000; NetworkReceive(&attacker, init, 16);
    clock_ms += 1500; /* 10.5 s after the first INIT, 1.5 s after a resend */
    assert(find(&attacker, clock_ms, 0) == NULL);

    /* Many sources cannot fill the whole pool with pending handshakes. */
    memset(peers, 0, sizeof(peers));
    for (i = 0; i < PEERS; ++i) {
        clock_ms += 20;
        attacker = addr(100 + (i % 100), 50000 + i);
        attacker.ip[2] = (byte)(i / 100 + 1);
        attacker.ip[1] = (byte)i;
        init_packet(init, (byte)i);
        NetworkReceive(&attacker, init, 16);
    }
    {
        unsigned used = 0;
        for (i = 0; i < PEERS; ++i) used += peers[i].used;
        assert(used == PENDING_TOTAL);
    }

    /* The NAT echo is rate limited per source address. */
    {
        byte nat[29] = {13, 2, 0};
        netadr_t probe = addr(77, 1234);
        clock_ms += 5000; sent = 0;
        for (i = 0; i < 50; ++i) NetworkReceive(&probe, nat, sizeof(nat));
        assert(sent == UNAUTH_PER_ADDRESS);
    }
    /* A half-open handshake lives 10 s on a dedicated server and 60 s on a hosted one (a map load is one frame). */
    assert(echoes_answered(10000) == 45 && echoes_answered(10001) == 0);
    test_hosted = 1;
    assert(echoes_answered(10001) == 45 && echoes_answered(60000) == 45 && echoes_answered(60001) == 0);
    test_hosted = 0;
    puts("CoDOL handshake: spoofed INITs are staged, pending slots are capped, unauthenticated replies are rate limited");
    return 0;
}

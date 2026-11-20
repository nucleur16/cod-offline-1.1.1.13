/* A DATA record that the association's own key opens must not touch the
 * admission roster or securitykey.bin: both are files, and the lookup used to
 * run for every received record (30 a second per client, on the main thread).
 * A record the association key does not open still falls back to the lobby
 * keys, and the one that opens it becomes the association's key.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "network.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

cvar_t *com_developer;
static unsigned clock_ms = 1000;
static int roster_lookups, packets, opens_with_candidate;
static int association_key_ok = 1;
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
{ (void)length; (void)data; (void)to; return qtrue; }
qboolean Sys_RandomBytes(byte *out, int n) { memset(out, 1, (size_t)n); return qtrue; }
void MSG_InitReadOnly(msg_t *msg, byte *data, int length) { (void)msg; (void)data; (void)length; }
__optimize3 __regparm2 void SV_PacketEvent(netadr_t *from, msg_t *msg) { (void)from; (void)msg; ++packets; }
long FS_SV_FOpenFileRead(const char *name, fileHandle_t *file) { (void)name; *file = 0; return -1; }
int FS_Read(void *buffer, int len, fileHandle_t f) { (void)buffer; (void)len; (void)f; return 0; }
qboolean FS_FCloseFile(fileHandle_t f) { (void)f; return qtrue; }
int AdmissionRequired(void) { return 1; }
size_t AdmissionCandidates(const uint8_t ip[4], uint8_t keys[][16], size_t max)
{
    (void)ip; assert(max >= 1);
    ++roster_lookups;
    memcpy(keys[0], lobby_key, 16);
    return 1;
}
int HandshakeInit(handshake_t *h, uint8_t *response, const uint8_t *packet, size_t n,
                        const uint8_t id[8], const uint8_t ip[4], uint16_t port, uint16_t tag,
                        const uint8_t key[28], ecdh_rng_t random, void *context)
{ (void)h; (void)response; (void)packet; (void)n; (void)id; (void)ip; (void)port; (void)tag;
  (void)key; (void)random; (void)context; assert(!"no handshake in this test"); return 0; }
int HandshakeCookie(handshake_t *h, uint8_t *response, const uint8_t *packet, size_t n,
                          const uint8_t key[16], ecdh_rng_t random, void *context)
{ (void)h; (void)response; (void)packet; (void)n; (void)key; (void)random; (void)context;
  assert(!"no handshake in this test"); return 0; }
int AssociationOpenDetailed(association_t *s, uint8_t *out, size_t cap,
                                  size_t *out_length, const uint8_t *record, size_t length,
                                  const uint8_t candidate[16], const char **reason)
{
    (void)s; (void)out; (void)cap; (void)record; (void)length;
    if(reason) *reason = "test";
    if(!candidate && !association_key_ok) return 0;
    if(candidate) {
        ++opens_with_candidate;
        if(memcmp(candidate, lobby_key, 16)) return 0;
        association_key_ok = 1; /* the rotated key is now the association's */
    }
    *out_length = 8;
    return 1;
}

int main(void)
{
    cvar_t key = {0}, id = {0};
    netadr_t address = {0};
    peer_t *p = &peers[0];
    byte record[32] = {6, 2};
    int i;

    key.string = "session-key-path"; id.string = "4600000000000000";
    key_path = &key; session_id = &id;
    strcpy(configured_path, key.string); strcpy(configured_id, id.string);
    sessions_loaded = 1;
    address.type = NA_IP;
    address.ip[0] = 203; address.ip[2] = 113; address.ip[3] = 9; address.port = 17733;
    p->used = 1; p->address = address; p->touched = clock_ms;
    p->handshake.established = 1; p->handshake.data.active = 1; p->handshake.data.received = 1;

    for(i = 0; i < 90; ++i) { clock_ms += 33; assert(NetworkReceive(&address, record, sizeof(record)) == 1); }
    assert(packets == 90);
    assert(roster_lookups == 0 && opens_with_candidate == 0);

    /* The lobby rotated the key: the association key fails, the roster is
     * read once, and its key opens the record and is kept. */
    association_key_ok = 0;
    assert(NetworkReceive(&address, record, sizeof(record)) == 1);
    assert(packets == 91 && roster_lookups == 1 && opens_with_candidate == 1);
    assert(NetworkReceive(&address, record, sizeof(record)) == 1);
    assert(packets == 92 && roster_lookups == 1);
    puts("CoDOL DATA records opened by the association key skip the admission roster; rotated keys still fall back");
    return 0;
}

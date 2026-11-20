/* Run the native session persistence/send path with a bounded fake filesystem.
 * Unreferenced transport entry points are removed by section GC.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "network.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

cvar_t *com_developer;
static char saved[PEERS * 200];
static int writes, fail_write, sent;
static unsigned clock_ms = 1000;

unsigned long long FrameProfileNow(void) { return 0; }
void FrameProfileEnd(frame_section_t section, unsigned long long start)
{ (void)section; (void)start; }
short ShortSwap(short value)
{ return (short)(((unsigned short)value >> 8) | ((unsigned short)value << 8)); }
time_t Com_GetRealtime(void) { return 1800000000; }
unsigned int Sys_Milliseconds(void) { return clock_ms; }
static int mock_write(const char *path, const void *data, size_t size)
{
    assert(!strcmp(path, SESSION_FILE));
    ++writes;
    if(fail_write) return 0;
    assert(size < sizeof(saved));
    memcpy(saved, data, size); saved[size] = 0;
    return 1;
}
int FS_SV_HomeWriteFile(const char *path, const void *data, int size)
{ return mock_write(path, data, (size_t)size) ? size : -1; }
int AtomicWriteHomeFile(const char *path, const void *data, size_t size)
{ return mock_write(path, data, size); }
int AtomicWriteFile(const char *path, const void *data, size_t size)
{ return mock_write(path, data, size); }
void QDECL Com_DPrintf(conChannel_t channel, const char *fmt, ...)
{ (void)channel; (void)fmt; }
void QDECL Com_PrintWarning(conChannel_t channel, const char *fmt, ...)
{ (void)channel; (void)fmt; }
/* The next unsafe_seals records carry a chunk type (1-14) at byte 6, where a
 * real record has its first MAC byte; C1's connection store would parse them. */
static int unsafe_seals;
static unsigned last_type;
size_t AssociationSeal(association_t *s, uint8_t *out, size_t cap,
                             const uint8_t *plain, size_t length)
{
    (void)plain; (void)length;
    assert(cap >= 16 && !s->send_exhausted);
    memset(out, 0, 16);
    out[0] = 6; out[1] = 2;
    out[6] = unsafe_seals > 0 ? (uint8_t)(1 + unsafe_seals-- % 14) : 0x80;
    ++s->send_next;
    return 16;
}
qboolean Sys_SendPacket(int length, const void *data, netadr_t *to)
{
    (void)to; assert(length == 16);
    last_type = ((const uint8_t *)data)[6];
    ++sent; return qtrue;
}

static unsigned saved_send_counter(void)
{
    unsigned ip[4], port, local, peer, send, receive, window;
    long timestamp;
    char x[57], key[49];
    assert(sscanf(saved, "%u.%u.%u.%u %u %56s %48s %u %u %u %u %u %ld",
        &ip[0], &ip[1], &ip[2], &ip[3], &port, x, key,
        &local, &peer, &send, &receive, &window, &timestamp) == 13);
    return send;
}

int main(void)
{
    cvar_t key = {0}, id = {0};
    netadr_t address = {0};
    peer_t *p = &peers[0];
    unsigned high, used;
    int i;

    key.string = "session-key-path";
    id.string = "4600000000000000";
    key_path = &key; session_id = &id;
    strcpy(configured_path, key.string);
    strcpy(configured_id, id.string);
    sessions_loaded = 1;
    address.type = NA_IP;
    address.ip[0] = 203; address.ip[1] = 0; address.ip[2] = 113; address.ip[3] = 9;
    address.port = 17733;
    p->used = 1; p->address = address; p->handshake.established = 1;
    p->handshake.data.active = 1; p->handshake.data.send_next = 1;
    save_sessions();
    /* One block lasts minutes of snapshots: no save before it runs out. */
    assert(SESSION_SEND_MARGIN >= 4096u);
    for(i = 0; i < (int)SESSION_SEND_MARGIN - 1; ++i)
        assert(NetworkSend(&address, "x", 1) == 1);
    assert(writes == 1);
    for(i = 0; i < 300; ++i) assert(NetworkSend(&address, "x", 1) == 1);
    used = p->handshake.data.send_next - 1;
    high = saved_send_counter();
    assert(used == SESSION_SEND_MARGIN + 299u && sent == (int)used);
    assert(high > used); /* The persisted value covers every sent record. */
    assert(high + SESSION_SEND_MARGIN > used); /* Restart skips the reserve. */
    assert(writes >= 2); /* Crossing a range persisted the next one. */

    while(p->handshake.data.send_next < high)
        assert(NetworkSend(&address, "x", 1) == 1);
    fail_write = 1;
    used = p->handshake.data.send_next;
    assert(NetworkSend(&address, "x", 1) == 0);
    assert(p->handshake.data.send_next == used && sent == (int)(used - 1));
    fail_write = 0;
    assert(NetworkSend(&address, "x", 1) == 1);
    assert(saved_send_counter() > used);

    /* A record whose byte 6 parses as a chunk is sealed again under the next
     * counter; only the inert one is sent. */
    used = p->handshake.data.send_next; i = sent;
    unsafe_seals = 3;
    assert(NetworkSend(&address, "x", 1) == 1);
    assert(p->handshake.data.send_next == used + 4 && sent == i + 1 && last_type == 0x80);
    /* The retry is capped, and every counter it used stays reserved. */
    used = p->handshake.data.send_next; i = sent;
    unsafe_seals = 1000;
    assert(NetworkSend(&address, "x", 1) == 1);
    assert(p->handshake.data.send_next == used + 17 && sent == i + 1);
    assert(saved_send_counter() >= p->handshake.data.send_next);
    unsafe_seals = 0;
    puts("CoDOL persisted send-counter reservation, write-failure gate and C1 store-inert records passed");
    return 0;
}

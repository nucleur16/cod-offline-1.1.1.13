/* Exact native persistence across private match homes; no socket or server.
 * Filesystem and clock are explicit boundaries.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "network.c"
#include <assert.h>

cvar_t *com_developer;
static char home_text[PEERS * 200 + 1];
static time_t clock_seconds = 1800000000;
unsigned long long FrameProfileNow(void) { return 0; }
void FrameProfileEnd(frame_section_t section, unsigned long long start)
{ (void)section; (void)start; }
time_t Com_GetRealtime(void) { return clock_seconds; }
unsigned int Sys_Milliseconds(void) { return 1000; }
short ShortSwap(short v) { return (short)(((unsigned short)v >> 8) | ((unsigned short)v << 8)); }
void QDECL Com_DPrintf(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void QDECL Com_PrintWarning(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
int AtomicWriteHomeFile(const char *path, const void *data, size_t length)
{
    assert(!strcmp(path, SESSION_FILE) && length < sizeof(home_text));
    memcpy(home_text, data, length); home_text[length] = 0; return 1;
}
int AtomicWriteFile(const char *path, const void *data, size_t length)
{
    FILE *f = fopen(path, "wb"); int ok;
    if(!f) return 0;
    ok = fwrite(data, 1, length, f) == length;
    return fclose(f) == 0 && ok;
}
long FS_SV_FOpenFileRead(const char *path, fileHandle_t *handle)
{ assert(!strcmp(path, SESSION_FILE)); *handle = 1; return (long)strlen(home_text); }
int FS_Read(void *out, int length, fileHandle_t handle)
{ assert(handle == 1 && (size_t)length == strlen(home_text)); memcpy(out, home_text, length); return length; }
qboolean FS_FCloseFile(fileHandle_t handle) { assert(handle == 1); return qtrue; }

static peer_t expected;
static void seed(void)
{
    peer_t *p = &peers[0];
    memset(peers, 0, sizeof(peers));
    p->used = 1; p->address.type = NA_IP; p->address.port = BigShort(30001);
    p->address.ip[0] = 192; p->address.ip[1] = 0; p->address.ip[2] = 2; p->address.ip[3] = 1;
    p->handshake.established = p->handshake.data.active = p->handshake.data.received = 1;
    memset(p->handshake.data.shared_x, 0x31, 28); memset(p->handshake.data.key, 0x72, 24);
    p->handshake.data.local_tag = 123; p->handshake.data.peer_tag = 456;
    p->handshake.data.send_next = 500; p->handshake.data.receive_high = 900;
    p->handshake.data.receive_window = 7; sessions_loaded = 1;
    expected = *p;
}
static void restore(void)
{ memset(peers, 0, sizeof(peers)); sessions_loaded = 0; load_sessions(); }
static void assert_resumed(void)
{
    peer_t *p = &peers[0];
    assert(p->used && p->handshake.established && p->handshake.data.active);
    assert(!memcmp(p->handshake.data.key, expected.handshake.data.key, 24));
    assert(!memcmp(p->handshake.data.shared_x, expected.handshake.data.shared_x, 28));
    assert(p->handshake.data.local_tag == 123 && p->handshake.data.peer_tag == 456);
    assert(p->handshake.data.receive_high == 900 && p->handshake.data.receive_window == 7);
    assert(p->handshake.data.send_next == 500 + 2 * SESSION_SEND_MARGIN);
}
int main(int argc, char **argv)
{
    char oversized[PEERS * 200 + 1] = {0};
    assert(argc == 3 && strlen(argv[1]) < sizeof(configured_session_path));
    /* Baseline: keeping the old home works; a new match home loses recovery. */
    seed(); assert(save_sessions()); restore(); assert_resumed();
    home_text[0] = 0; restore(); assert(!peers[0].used);
    /* Candidate: fresh match homes resume the same per-port transport file. */
    strcpy(configured_session_path, argv[1]); seed(); assert(save_sessions());
    home_text[0] = 0; restore(); assert_resumed();
    /* Another configured port must never receive the first port's state. */
    strcpy(configured_session_path, argv[2]); restore(); assert(!peers[0].used);
    strcpy(configured_session_path, argv[1]); restore(); assert_resumed();
    clock_seconds += SESSION_TTL + 1; restore(); assert(!peers[0].used);
    clock_seconds -= SESSION_TTL + 1;
    assert(AtomicWriteFile(argv[1], oversized, sizeof(oversized)));
    restore(); assert(!peers[0].used);
    assert(remove(argv[1]) == 0);
    puts("CoDOL session home-loss reproduction, per-port resume, counters, isolation, TTL and size bound passed");
    return 0;
}

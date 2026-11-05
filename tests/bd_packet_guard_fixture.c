/* The actual exact-C1 entry guard, with only the Windows install APIs stubbed.
 * This exercises the x86 thiscall forward path and the physical-length gate;
 * it cannot validate C1's downstream parser or the installed trampoline. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif

static unsigned char *fake_packet, *fake_data;
#define BD_PACKET_DESERIALIZE_VA ((uintptr_t)fake_packet)
#define BD_DATA_DESERIALIZE_VA   ((uintptr_t)fake_data)

#ifdef _WIN32
#include <windows.h>
#else
typedef unsigned char BYTE;
typedef unsigned long DWORD;
typedef uintptr_t DWORD_PTR;
typedef size_t SIZE_T;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 4
#define PAGE_EXECUTE_READ 0x20
static void *VirtualAlloc(void *where, SIZE_T n, DWORD alloc, DWORD protect)
{
    void *p;
    (void)where; (void)n; (void)alloc; (void)protect;
    p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
static BOOL VirtualProtect(void *where, SIZE_T n, DWORD protect, DWORD *old)
{ (void)n; (void)protect; *old = PAGE_READWRITE; return mprotect(where, 4096, PROT_READ | PROT_EXEC) == 0; }
static BOOL VirtualFree(void *where, SIZE_T n, DWORD mode)
{ (void)n; (void)mode; return munmap(where, 4096) == 0; }
static void *GetCurrentProcess(void) { return NULL; }
static BOOL FlushInstructionCache(void *process, const void *where, SIZE_T n)
{ (void)process; (void)where; (void)n; return TRUE; }
#endif

static BOOL Readable(const void *where, SIZE_T n) { return where != NULL && n <= 10; }
static void WriteCode(void *where, const void *source, SIZE_T n) { memcpy(where, source, n); }
static void Log(const char *format, ...) { (void)format; }
#include "../bd_packet_guard.c"

static int forwarded;
static int data_forwarded;
static BYTE __attribute__((thiscall)) Original(void *self, const BYTE *data, int size)
{
    assert(self == &forwarded && data && size == 7);
    ++forwarded;
    return 1;
}
static BYTE __attribute__((thiscall)) OriginalData(void *self, const BYTE *area,
    int area_size, unsigned *area_cursor, const BYTE *tail, int tail_size,
    unsigned *tail_cursor)
{
    (void)area; (void)area_size; (void)area_cursor;
    (void)tail; (void)tail_size; (void)tail_cursor;
    assert(self == &data_forwarded);
    ++data_forwarded;
    return 1;
}

int main(void)
{
#ifdef _WIN32
    static volatile int compile_install;
    if (compile_install) return InstallBdPacketLengthGuard() ? 0 : 1;
#endif
    BYTE packet[7] = { 5, 0, 0, 0, 0, 0, 2 };
    g_bdPacketDeserializeOriginal = Original;
    assert(BdPacketDeserializeGuard(&forwarded, packet, 7) == 1 && forwarded == 1);
    packet[0] = 6;
    assert(BdPacketDeserializeGuard(&forwarded, packet, 7) == 0 && forwarded == 1);
    packet[0] = 0xff; packet[1] = 0xff;
    assert(BdPacketDeserializeGuard(&forwarded, packet, 7) == 0 && forwarded == 1);
    assert(BdPacketDeserializeGuard(&forwarded, packet, 6) == 0 && forwarded == 1);
    assert(BdPacketDeserializeGuard(&forwarded, NULL, 7) == 0 && forwarded == 1);
    {
        BYTE area[16] = { 2, 6, 0, 0, 0, 3, 2, 1, 2, 3 };
        BYTE tail[2] = { 8, 9 };
        unsigned area_at = 0, tail_at = 0;
        g_bdDataDeserializeOriginal = OriginalData;
        assert(BdDataDeserializeGuard(&data_forwarded, area, 10, &area_at,
                                      tail, 2, &tail_at) == 1 && data_forwarded == 1);
        area[5] = 4; /* claims four area bytes, only three remain */
        assert(BdDataDeserializeGuard(&data_forwarded, area, 10, &area_at,
                                      tail, 2, &tail_at) == 0 && data_forwarded == 1);
        area[5] = 3; area[6] = 3; /* claims three tail bytes, only two exist */
        assert(BdDataDeserializeGuard(&data_forwarded, area, 10, &area_at,
                                      tail, 2, &tail_at) == 0 && data_forwarded == 1);
        area[6] = 2; tail_at = 1; /* tail cursor leaves only one byte */
        assert(BdDataDeserializeGuard(&data_forwarded, area, 10, &area_at,
                                      tail, 2, &tail_at) == 0 && data_forwarded == 1);
        tail_at = 0; area[5] = 0x80; area[6] = 4; /* two-byte var16 */
        assert(BdDataDeserializeGuard(&data_forwarded, area, 10, &area_at,
                                      tail, 2, &tail_at) == 0 && data_forwarded == 1);
        area[5] = 3; area[6] = 2; area_at = 6; /* header cannot fit */
        assert(BdDataDeserializeGuard(&data_forwarded, area, 10, &area_at,
                                      tail, 2, &tail_at) == 0 && data_forwarded == 1);
        area_at = 0; area[1] = 4; area[5] = 1;
        assert(BdDataDeserializeGuard(&data_forwarded, area, 6, &area_at,
                                      NULL, 0, &tail_at) == 0 && data_forwarded == 1);
        area[1] = 0;
        assert(BdDataDeserializeGuard(&data_forwarded, area, 5, &area_at,
                                      NULL, 0, &tail_at) == 1 && data_forwarded == 2);
    }
    /* Install both exact-signature jumps on private Linux pages, then run the
     * 32-bit thiscall path through each real trampoline. The surrogate tails
     * replace Windows SEH setup after the copied five-byte prologue. */
#ifndef _WIN32
    {
        static const BYTE packet_head[10] = { 0x55, 0x8b, 0xec, 0x6a, 0xff,
                                              0x68, 0xc9, 0x10, 0xb3, 0x00 };
        static const BYTE data_head[10] = { 0x55, 0x8b, 0xec, 0x6a, 0xff,
                                            0x68, 0xab, 0x0d, 0xb3, 0x00 };
        static const BYTE packet_tail[7] = { 0x89, 0xec, 0x5d, 0xb0, 1, 0xc2, 8 };
        static const BYTE data_tail[7] = { 0x89, 0xec, 0x5d, 0xb0, 1, 0xc2, 24 };
        BYTE area[5] = { 2, 0, 0, 0, 0 };
        unsigned area_at = 0, tail_at = 0;
        BYTE *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(page != MAP_FAILED);
        fake_packet = page + 32;
        fake_data = page + 256;
        memcpy(fake_packet, packet_head, 10);
        memcpy(fake_data, data_head, 10);
        fake_packet[6] ^= 1;
        assert(!InstallBdPacketLengthGuard()); /* exact bytes differ: fail closed */
        fake_packet[6] ^= 1;
        fake_data[6] ^= 1;
        assert(!InstallBdPacketLengthGuard());
        assert(!memcmp(fake_packet, packet_head, 10)); /* no partial patch */
        fake_data[6] ^= 1;
        assert(InstallBdPacketLengthGuard());
        memcpy(fake_packet + 5, packet_tail, sizeof(packet_tail));
        fake_packet[12] = 0; /* ret 8 immediate high byte */
        memcpy(fake_data + 5, data_tail, sizeof(data_tail));
        fake_data[12] = 0; /* ret 24 immediate high byte */
        packet[0] = 5; packet[1] = 0;
        assert(((BdPacketDeserializeFn)(void *)fake_packet)(&forwarded, packet, 7) == 1);
        packet[0] = 6;
        assert(((BdPacketDeserializeFn)(void *)fake_packet)(&forwarded, packet, 7) == 0);
        assert(((BdDataDeserializeFn)(void *)fake_data)(&data_forwarded, area, 5,
               &area_at, NULL, 0, &tail_at) == 1);
        area[1] = 4; /* tail length missing */
        assert(((BdDataDeserializeFn)(void *)fake_data)(&data_forwarded, area, 5,
               &area_at, NULL, 0, &tail_at) == 0);
        assert(munmap(page, 4096) == 0);
    }
#endif
    puts("PASS: exact-C1 BD packet and DATA source-length prechecks with x86 thiscall forwarding");
    return 0;
}

/* test_hunk_user.c - the retained Hunk_UserAlloc with blocks around the 2 GB line and at the end of the address space.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The compare of a request's end against its block's end was signed. An end past 0x80000000 counted as below a block
 * under 2 GB, so the request was committed past the block (the in-process host's "System is out of memory ... Line:
 * 426" while a Survival map's scripts compiled, 2026-11-24), and a fixed user's block across 2 GB was full at once
 * ("Hunk_UserAlloc: out of memory"). The addresses below are those of the two failures.
 * Needs a process that can use 4 GB: any 32-bit process under a 64-bit Linux kernel, --large-address-aware on Windows. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#endif

typedef unsigned int u32;
struct HunkUser { struct HunkUser *current, *next; u32 maxSize, end, pos; const char *name; unsigned char fixed, tempMem; int type; };
extern struct HunkUser *Hunk_UserCreate(int size, const char *name, unsigned char fixed, unsigned char tempMem, int type);
extern void *Hunk_UserAlloc(struct HunkUser *user, int size, int alignment);
extern void Hunk_UserReset(struct HunkUser *user);

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define STUB(n) void n(void) { printf("FAIL: unexpected call of %s\n", #n); exit(1); }
STUB(Com_Printf) STUB(PMem_DumpMemStats) STUB(XAnimFreeList) STUB(XModelPartsFree) STUB(XAnimFree) STUB(FS_LoadStack)
STUB(R_ReflectionProbeRegisterDvars) STUB(Cmd_AddCommand) STUB(SL_GetString_) STUB(SL_ConvertToString) STUB(SL_FindString)
STUB(SL_RemoveRefToString) STUB(Com_Memset) STUB(Com_PrintError) STUB(FS_HashFileName) STUB(Q_stricmp)
void *useFastFile, *r_reflectionProbeGenerate, *com_dedicated;

static jmp_buf failed;
static const char *error;
void Com_Error(int code, const char *fmt, ...) { (void)code; error = fmt; longjmp(failed, 1); }
void Sys_OutOfMemErrorInternal(const char *file, int line) { (void)file; (void)line; error = "VirtualAlloc"; longjmp(failed, 1); }

/* Reservations are placed by the test; commits are real, and must lie in one reservation. */
static struct { u32 base, size; } blocks[16];
static unsigned nblocks, reserves;
static u32 place;       /* the next reservation's address; 0: none can be had */

void *_VirtualAlloc(void *address, int size, int type, int protect)
{
    u32 a = (u32)(size_t)address, n = (u32)size;
    unsigned i;
    (void)protect;
    if (type == 0x2000) {
        ++reserves;
        CHECK(reserves < 16);
        if (!place) return NULL;
        n = (n + 0xffff) & ~0xffffu;
        blocks[nblocks].base = place;
        blocks[nblocks++].size = n;
        a = place;
        place = 0;
        return (void *)(size_t)a;
    }
    CHECK(type == 0x1000);
    for (i = 0; i < nblocks; ++i)
        if (a >= blocks[i].base && a - blocks[i].base < blocks[i].size && n <= blocks[i].size - (a - blocks[i].base)) {
#ifdef _WIN32
            u32 lo = a & ~0xfffu, hi = (a + n + 0xfff) & ~0xfffu;
            return VirtualAlloc((void *)(size_t)lo, hi - lo, MEM_COMMIT, PAGE_READWRITE) ? address : NULL;
#else
            return address;     /* the test maps its blocks readable and writable */
#endif
        }
    return NULL;        /* outside every reservation: ERROR_INVALID_ADDRESS */
}
int _VirtualFree(void *address, int size, int type) { (void)address; (void)size; (void)type; return 1; }

static int reserve_at(u32 base, u32 size)
{
#ifdef _WIN32
    return VirtualAlloc((void *)(size_t)base, size, MEM_RESERVE, PAGE_READWRITE) == (void *)(size_t)base;
#else
    return mmap((void *)(size_t)base, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0) == (void *)(size_t)base;
#endif
}

static void *alloc(struct HunkUser *u, u32 size, int alignment)
{
    if (setjmp(failed)) return NULL;
    return Hunk_UserAlloc(u, (int)size, alignment);
}
static struct HunkUser *create(u32 base, u32 size, int fixed)
{
    place = base; reserves = 0; nblocks = 0; error = NULL;
    if (setjmp(failed)) return NULL;
    return Hunk_UserCreate((int)size, "test", (unsigned char)fixed, 0, 0);
}

int main(void)
{
    struct HunkUser *u;
    unsigned char *p;

    /* 1. The script debug memory (16 MB, grows) under 2 GB: its second opcode table (12 MB) ends past 2 GB. */
    CHECK(reserve_at(0x7eeb0000u, 0x1000000u) && reserve_at(0x90000000u, 0x1000000u));
    CHECK((u = create(0x7eeb0000u, 0x1000000u, 0)) != NULL);
    CHECK(alloc(u, 0xf61c4cu, 4) == (void *)(size_t)0x7eeb0020u && u->pos == 0x7fe11c6cu);
    place = 0x90000000u;
    p = alloc(u, 0xc00000u, 4);
    CHECK(p == (void *)(size_t)0x90000020u && reserves == 2 && !error);
    memset(p, 0x5a, 0xc00000u);
    CHECK(u->pos == 0x7fe11c6cu && u->current == (void *)(size_t)0x90000000u && u->next == u->current);
    /* what still fits goes into the new block */
    CHECK(alloc(u, 0x1000, 4) == (void *)(size_t)0x90c00020u && reserves == 2);

    /* 2. The program buffer (1 MB, fixed) across 2 GB holds exactly its size. */
    CHECK(reserve_at(0x7fff0000u, 0x100000u));
    CHECK((u = create(0x7fff0000u, 0x100000u, 1)) != NULL);
    CHECK(alloc(u, 0, 1) == (void *)(size_t)0x7fff0020u && !error);
    p = alloc(u, 0xfffe0u, 1);
    CHECK(p == (void *)(size_t)0x7fff0020u && !error);
    memset(p, 0xa5, 0xfffe0u);
    CHECK(!alloc(u, 1, 1) && error && strstr(error, "Hunk_UserAlloc"));

    /* 3. A request larger than the user's block gets a block of its own size, once (it used to reserve blocks until
     * the address space was gone). */
    CHECK(reserve_at(0xa0000000u, 0x10000u) && reserve_at(0xa1000000u, 0x1810000u));
    CHECK((u = create(0xa0000000u, 0x10000u, 0)) != NULL);
    place = 0xa1000000u;
    p = alloc(u, 0x1800000u, 4);
    CHECK(p == (void *)(size_t)0xa1000020u && reserves == 2 && !error && u->maxSize == 0x10000u);
    memset(p, 1, 0x1800000u);

    /* 4. An end past 4 GB is past every block. */
    CHECK(reserve_at(0xb0000000u, 0x10000u) && reserve_at(0xb1000000u, 0x2010000u));
    CHECK((u = create(0xb0000000u, 0x10000u, 0)) != NULL);
    u->pos = u->end = 0xfffe0000u;
    place = 0xb1000000u;
    CHECK(alloc(u, 0x2000000u, 4) == (void *)(size_t)0xb1000020u && reserves == 2 && !error);

    /* 5. No address space left: the out-of-memory error, not a write to the header at 0. */
    CHECK(reserve_at(0xc0000000u, 0x10000u));
    CHECK((u = create(0xc0000000u, 0x10000u, 0)) != NULL);
    CHECK(!alloc(u, 0x20000u, 4) && error && !strcmp(error, "VirtualAlloc"));
    CHECK(!create(0, 0x10000u, 0) && error && !strcmp(error, "VirtualAlloc"));

    puts("PASS: Hunk_UserAlloc at 2 GB and 4 GB, oversize requests, no address space");
    return 0;
}

/* Exercise the actual x86 snapshot entry hook on private executable pages. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
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
static void *VirtualAlloc(void *p, SIZE_T n, DWORD a, DWORD prot)
{ (void)p; (void)n; (void)a; (void)prot;
  p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return p == MAP_FAILED ? NULL : p; }
static BOOL VirtualProtect(void *p, SIZE_T n, DWORD prot, DWORD *old)
{ (void)n; (void)prot; *old = PAGE_READWRITE; return mprotect(p, 4096, PROT_READ | PROT_EXEC) == 0; }
static BOOL VirtualFree(void *p, SIZE_T n, DWORD type)
{ (void)n; (void)type; return munmap(p, 4096) == 0; }
static void *GetCurrentProcess(void) { return NULL; }
static BOOL FlushInstructionCache(void *p, const void *a, SIZE_T n)
{ (void)p; (void)a; (void)n; return TRUE; }
#endif

static BYTE *fake_entry;
static BYTE *fake_run;
static BYTE *fake_exit;
static BYTE *fake_entity;
static BYTE *fake_generic;
static unsigned fake_count;
#define SNAPSHOT_APPEND_VA ((uintptr_t)fake_entry)
#define SNAPSHOT_FULL_ENTITY_COUNT_VA ((uintptr_t)&fake_count)
#define SNAPSHOT_PLAYERSTATE_RUN_VA ((uintptr_t)fake_run)
#define SNAPSHOT_PLAYERSTATE_EXIT_VA ((uintptr_t)fake_exit)
#define SNAPSHOT_ENTITY_RUN_VA ((uintptr_t)fake_entity)
#define SNAPSHOT_ENTITY_EXIT_VA ((uintptr_t)fake_exit)
#define SNAPSHOT_GENERIC_RUN_VA ((uintptr_t)fake_generic)
#define SNAPSHOT_GENERIC_EXIT_VA ((uintptr_t)fake_exit)
static BOOL Readable(const void *p, SIZE_T n) { return p != NULL && n <= 11; }
static void WriteCode(void *p, const void *s, SIZE_T n) { memcpy(p, s, n); }
static void Log(const char *fmt, ...) { (void)fmt; }
#include "../src/guards/snapshot_guard.c"

#ifndef _WIN32
static int __attribute__((noinline)) Run(BYTE *entry, unsigned entity, BYTE *snap, unsigned *msg)
{
    int value;
    __asm__ volatile(
        "movl %2, %%esi\n\t"
        "movl %3, %%ecx\n\t"
        "pushl %4\n\t"
        "call *%1\n\t"
        "addl $4, %%esp\n\t"
        : "=a"(value)
        : "m"(entry), "m"(snap), "m"(entity), "m"(msg)
        : "ecx", "edx", "esi", "memory", "cc");
    return value;
}
static int __attribute__((noinline)) RunPlayerstate(BYTE *entry, unsigned index,
                                                     BYTE *frame, unsigned *msg)
{
    int value;
    BYTE *local_ebp = frame + 0x3388;
    __asm__ volatile(
        "movl %1, %%edx\n\t"
        "movl %2, %%edi\n\t"
        "movl %3, %%esi\n\t"
        "movl %4, %%ecx\n\t"
        "pushl %%ebp\n\t"
        "movl %5, %%ebp\n\t"
        "xorl %%eax, %%eax\n\t"
        "call *%%edx\n\t"
        "popl %%ebp\n\t"
        : "=a"(value)
        : "m"(entry), "m"(index), "m"(msg), "m"(frame), "m"(local_ebp)
        : "ecx", "edx", "esi", "edi", "memory", "cc");
    return value;
}
static int __attribute__((noinline)) RunEntity(BYTE *entry, unsigned index,
                                               BYTE *frame)
{
    int value;
    BYTE *local_ebp = frame + 8;
    __asm__ volatile(
        "movl %1, %%edx\n\t"
        "movl %2, %%esi\n\t"
        "pushl %%ebp\n\t"
        "movl %3, %%ebp\n\t"
        "call *%%edx\n\t"
        "popl %%ebp\n\t"
        : "=a"(value)
        : "m"(entry), "m"(index), "m"(local_ebp)
        : "ecx", "edx", "esi", "memory", "cc");
    return value;
}
static int __attribute__((noinline)) RunGeneric(BYTE *entry, unsigned run,
                                                BYTE *frame, unsigned *msg)
{
    int value;
    BYTE *local_ebp = frame + 24;
    __asm__ volatile(
        "movl %1, %%edx\n\t"
        "movl %2, %%esi\n\t"
        "movl %3, %%edi\n\t"
        "pushl %%ebp\n\t"
        "movl %4, %%ebp\n\t"
        "xorl %%eax, %%eax\n\t" /* previous field index */
        "leal 1f, %%ecx\n\t"
        "pushl %%ecx\n\t" /* stands in for the pending run-bits argument */
        "call *%%edx\n\t"
        "1: popl %%ebp\n\t"
        : "=a"(value)
        : "m"(entry), "m"(run), "m"(msg), "m"(local_ebp)
        : "ecx", "edx", "esi", "edi", "memory", "cc");
    return value;
}
#endif

int main(void)
{
#ifdef _WIN32
    static volatile int compile_install;
    if (compile_install) return InstallSnapshotBoundsGuards() ? 0 : 1;
#else
    static const BYTE expected[11] = {
        0x55, 0x8b, 0xec, 0x51, 0xf6, 0x86, 0x6c, 0x33, 0, 0, 8
    };
    /* Surrogate body: restore copied prologue stack and report forwarding. */
    static const BYTE tail[] = { 0x59, 0x5d, 0xb8, 0x7b, 0, 0, 0, 0xc3 };
    static const BYTE run_expected[5] = { 0x8d, 0x48, 0x01, 0x3b, 0xcf };
    static const BYTE run_tail[3] = { 0x89, 0xc8, 0xc3 }; /* return copied LEA's ECX */
    static const BYTE entity_tail[1] = { 0xc3 }; /* return copied INC's EAX */
    static const BYTE entity_expected[6] = { 0x8b, 0x45, 0xf8, 0x40, 0x3b, 0xc6 };
    static const BYTE generic_expected[6] = { 0x03, 0xf0, 0x40, 0x83, 0xc4, 0x04 };
    static const BYTE generic_tail[3] = { 0x89, 0xf0, 0xc3 }; /* return copied ADD's ESI */
    static const BYTE exit_tail[3] = { 0x31, 0xc0, 0xc3 };
    BYTE *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    BYTE snap[0x3388] = {0};
    BYTE frame[0x3388] = {0};
    BYTE entity_frame[32] = {0};
    BYTE generic_frame[64] = {0};
    unsigned msg = 0;
    assert(page != MAP_FAILED);
    fake_entry = page + 64;
    fake_run = page + 256;
    fake_exit = page + 512;
    fake_entity = page + 768;
    fake_generic = page + 1024;
    memcpy(fake_entry, expected, 11);
    memcpy(fake_entry + 11, tail, sizeof(tail));
    memcpy(fake_run, run_expected, 5);
    memcpy(fake_run + 5, run_tail, sizeof(run_tail));
    memcpy(fake_exit, exit_tail, sizeof(exit_tail));
    memcpy(fake_entity, entity_expected, sizeof(entity_expected));
    memcpy(fake_entity + 6, entity_tail, sizeof(entity_tail));
    memcpy(fake_generic, generic_expected, sizeof(generic_expected));
    memcpy(fake_generic + 6, generic_tail, sizeof(generic_tail));
    fake_entry[5] ^= 1;
    assert(!InstallSnapshotBoundsGuards());
    fake_entry[5] ^= 1;
    fake_run[0] ^= 1;
    assert(!InstallSnapshotBoundsGuards());
    assert(!memcmp(fake_entry, expected, 11));
    fake_run[0] ^= 1;
    fake_entity[0] ^= 1;
    assert(!InstallSnapshotBoundsGuards());
    assert(!memcmp(fake_entry, expected, 11));
    fake_entity[0] ^= 1;
    fake_generic[0] ^= 1;
    assert(!InstallSnapshotBoundsGuards());
    assert(!memcmp(fake_entry, expected, 11));
    fake_generic[0] ^= 1;
    assert(InstallSnapshotBoundsGuards());
    assert(fake_entry[0] == 0xe9 && fake_entry[5] == 0x90);

    snap[0x336c] = 8;
    fake_count = 0;
    assert(Run(fake_entry, 0, snap, &msg) == 123 && msg == 0);
    fake_count = 0x3ff;
    assert(Run(fake_entry, 0x7fe, snap, &msg) == 123 && msg == 0);
    *(unsigned *)(void *)(frame + 4) = 1; /* [EBP-0x3384] */
    assert(RunPlayerstate(fake_run, 0, frame, &msg) == 1 && msg == 0);
    assert(RunPlayerstate(fake_run, 1, frame, &msg) == 1 && msg == 0); /* terminal slot */
    assert(RunPlayerstate(fake_run, 2, frame, &msg) == 0 && msg == 1);
    msg = 0;
    *(unsigned *)(void *)(frame + 4) = 244;
    assert(RunPlayerstate(fake_run, 244, frame, &msg) == 1 && msg == 0);
    assert(RunPlayerstate(fake_run, 245, frame, &msg) == 0 && msg == 1);
    msg = 0;
    assert(RunPlayerstate(fake_run, 294, frame, &msg) == 0 && msg == 1);
    *(unsigned *)(void *)entity_frame = 0; /* [EBP-8], prior index */
    *(unsigned *)(void *)(entity_frame + 4) = 1; /* [EBP-4], field count */
    *(unsigned **)(void *)(entity_frame + 16) = &msg; /* [EBP+8] */
    msg = 0;
    assert(RunEntity(fake_entity, 0, entity_frame) == 1 && msg == 0);
    assert(RunEntity(fake_entity, 1, entity_frame) == 1 && msg == 0); /* terminal slot */
    assert(RunEntity(fake_entity, 2, entity_frame) == 0 && msg == 1);
    *(unsigned *)(void *)(entity_frame + 4) = 0x42;
    msg = 0;
    assert(RunEntity(fake_entity, 0x41, entity_frame) == 1 && msg == 0);
    assert(RunEntity(fake_entity, 0x42, entity_frame) == 0 && msg == 1);
    msg = 0;
    assert(RunEntity(fake_entity, 0xffffffffu, entity_frame) == 0 && msg == 1);
    *(unsigned *)(void *)(generic_frame + 16) = 1; /* [EBP-8], inclusive last index */
    *(unsigned *)(void *)(generic_frame + 48) = 0x4d; /* [EBP+0x18], table maximum */
    msg = 0;
    assert(RunGeneric(fake_generic, 1, generic_frame, &msg) == 1 && msg == 0);
    assert(RunGeneric(fake_generic, 2, generic_frame, &msg) == 0 && msg == 1);
    *(unsigned *)(void *)(generic_frame + 16) = 0x4d;
    msg = 0;
    assert(RunGeneric(fake_generic, 0x4d, generic_frame, &msg) == 0x4d && msg == 0);
    assert(RunGeneric(fake_generic, 0x4e, generic_frame, &msg) == 0 && msg == 1);
    fake_count = 0x400;
    assert(Run(fake_entry, 0x7fe, snap, &msg) == 0 && msg == 1);
    msg = 0;
    fake_count = 0;
    assert(Run(fake_entry, 0xffffffffu, snap, &msg) == 0 && msg == 1);
    msg = 0;
    assert(Run(fake_entry, 0x7ff, snap, &msg) == 0 && msg == 1);
    msg = 0;
    snap[0x336c] = 0;
    assert(Run(fake_entry, 0x7fe, snap, &msg) == 123 && msg == 0);
    assert(munmap(page, 4096) == 0);
    puts("PASS: exact-C1 snapshot append, playerstate, entity and shared delta bounds hooks");
#endif
    return 0;
}

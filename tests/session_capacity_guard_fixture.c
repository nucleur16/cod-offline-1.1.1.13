/* 32-bit entry/return fixture for exact C1 session-update capacity guard. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef uintptr_t DWORD_PTR;
typedef int BOOL;
#define TRUE 1
#define FALSE 0

static BYTE g_entry[6] = { 0x8b, 0x0d, 0x00, 0x4f, 0x7f, 0x02 };
static uint8_t g_public_dvar[0x10];
static uint8_t g_total_dvar[0x10];
static void *g_public_slot = g_public_dvar;
static void *g_total_slot = g_total_dvar;
static BOOL Readable(const void *p, size_t n) { return p != NULL && n == 6; }
static void WriteCode(void *p, const void *src, size_t n) { memcpy(p, src, n); }
static void Log(const char *ignored) { (void)ignored; }

#define SESSION_CAPACITY_ENTRY_VA ((uintptr_t)g_entry)
#define SESSION_PUBLIC_DVAR_SLOT_VA ((uintptr_t)&g_public_slot)
#define SESSION_TOTAL_DVAR_SLOT_VA ((uintptr_t)&g_total_slot)
#include "../src/guards/session_capacity_guard.c"

/* Invalid guard returns directly to this function's caller; valid guard
 * returns here with ECX equal to the original public dvar pointer. */
static int __attribute__((naked, noinline)) FixtureSession(void)
{
    __asm__ volatile(
        "call SessionCapacityEntryGuard\n\t"
        "cmpl g_public_slot, %%ecx\n\t"
        "jne 1f\n\t"
        "movl $1, %%eax\n\t"
        "ret\n\t"
        "1: movl $-99, %%eax\n\t"
        "ret\n\t"
        : : : "eax", "ecx", "edx", "cc", "memory");
}

static void set_values(int total, int public_count)
{
    memcpy(g_total_dvar + 0xc, &total, sizeof(total));
    memcpy(g_public_dvar + 0xc, &public_count, sizeof(public_count));
}

int main(void)
{
    int32_t rel;
    assert(InstallSessionCapacityGuard());
    assert(g_entry[0] == 0xe8 && g_entry[5] == 0x90);
    memcpy(&rel, g_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_entry + 5) + rel == (uintptr_t)SessionCapacityEntryGuard);
    set_values(19, 19);
    assert(FixtureSession() == 1);
    set_values(19, 0);
    assert(FixtureSession() == 1);
    set_values(0, 0);
    assert(FixtureSession() == 1);
    set_values(20, 1);
    assert(FixtureSession() == 0);
    set_values(19, 20);
    assert(FixtureSession() == 0);
    set_values(-1, 0);
    assert(FixtureSession() == 0);
    set_values(19, -1);
    assert(FixtureSession() == 0);
    set_values(INT_MAX, INT_MAX);
    assert(FixtureSession() == 0);
    g_public_slot = NULL;
    assert(FixtureSession() == 0);
    g_public_slot = g_public_dvar;
    g_total_slot = NULL;
    assert(FixtureSession() == 0);
    g_total_slot = g_total_dvar;
    g_entry[0] = 0;
    assert(!InstallSessionCapacityGuard());
    puts("session capacity guard fixture passed");
    return 0;
}

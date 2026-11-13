/* 32-bit branch/ABI fixture for exact C1 19-record party loop guard. */
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

static BYTE g_entry[5] = { 0x3b, 0x71, 0x0c, 0x7c, 0x9f };
static BOOL Readable(const void *p, size_t n) { return p != NULL && n == 5; }
static void WriteCode(void *p, const void *src, size_t n) { memcpy(p, src, n); }
static void Log(const char *ignored) { (void)ignored; }

static int __attribute__((naked, noinline, used)) FixtureLoopTarget(void)
{
    __asm__ volatile("movl $1, %eax\n\t popl %esi\n\t ret\n\t");
}

#define PARTY_SLOT_LOOP_COMPARE_VA ((uintptr_t)g_entry)
#define PARTY_SLOT_LOOP_BODY_VA ((uintptr_t)FixtureLoopTarget)
#include "../src/guards/party_slot_loop_guard.c"

static int __attribute__((naked, noinline)) FixtureDispatch(
    int index __attribute__((unused)), void *dvar __attribute__((unused)))
{
    __asm__ volatile(
        "pushl %%esi\n\t"
        "movl 8(%%esp), %%esi\n\t"
        "movl 12(%%esp), %%ecx\n\t"
        "call PartySlotLoopGuard\n\t"
        "xorl %%eax, %%eax\n\t"
        "popl %%esi\n\t"
        "ret\n\t"
        : : : "eax", "ecx", "cc", "memory");
}

static void check(int index, int count, int want)
{
    uint8_t dvar[0x10] = {0};
    memcpy(dvar + 0xc, &count, sizeof(count));
    assert(FixtureDispatch(index, dvar) == want);
}

int main(void)
{
    int32_t rel;
    assert(InstallPartySlotLoopGuard());
    assert(g_entry[0] == 0xe8);
    memcpy(&rel, g_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_entry + 5) + rel == (uintptr_t)PartySlotLoopGuard);
    check(0, 1, 1);
    check(0, 19, 1);
    check(18, 19, 1);
    check(19, 19, 0);
    check(9, 10, 1);
    check(10, 10, 0);
    check(18, 20, 1);
    check(19, 20, 0);
    check(19, INT_MAX, 0);
    check(0, 0, 0);
    check(0, -1, 0);
    assert(FixtureDispatch(0, NULL) == 0);
    g_entry[0] = 0;
    assert(!InstallPartySlotLoopGuard());
    puts("party slot loop guard fixture passed");
    return 0;
}

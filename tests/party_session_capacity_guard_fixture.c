/* 32-bit register/store fixture for exact C1 party-session capacity guard. */
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

static BYTE g_entry[6] = { 0x89, 0x46, 0x54, 0x89, 0x4e, 0x50 };
static BOOL Readable(const void *p, size_t n) { return p != NULL && n == 6; }
static void WriteCode(void *p, const void *src, size_t n) { memcpy(p, src, n); }
static void Log(const char *ignored) { (void)ignored; }

#define PARTY_SESSION_CAPACITY_STORE_VA ((uintptr_t)g_entry)
#include "../src/guards/party_session_capacity_guard.c"

static int __attribute__((naked, noinline)) FixtureDispatch(
    void *session __attribute__((unused)), int private_count __attribute__((unused)),
    int public_count __attribute__((unused)))
{
    __asm__ volatile(
        "pushl %%esi\n\t"
        "movl 8(%%esp), %%esi\n\t"
        "movl 12(%%esp), %%eax\n\t"
        "movl 16(%%esp), %%ecx\n\t"
        "movl $0x12345678, %%edx\n\t"
        "call PartySessionCapacityGuard\n\t"
        "cmpl $0x12345678, %%edx\n\t"
        "jne 1f\n\t"
        "popl %%esi\n\t"
        "ret\n\t"
        "1: movl $-99, %%eax\n\t"
        "popl %%esi\n\t"
        "ret\n\t"
        : : : "eax", "ecx", "edx", "cc", "memory");
}

static void check(int private_count, int public_count, int want_private, int want_public)
{
    uint8_t session[0x58] = {0};
    int actual_private, actual_public;
    assert(FixtureDispatch(session, private_count, public_count) == want_private);
    memcpy(&actual_private, session + 0x54, sizeof(actual_private));
    memcpy(&actual_public, session + 0x50, sizeof(actual_public));
    assert(actual_private == want_private && actual_public == want_public);
}

int main(void)
{
    int32_t rel;
    assert(InstallPartySessionCapacityGuard());
    assert(g_entry[0] == 0xe8 && g_entry[5] == 0x90);
    memcpy(&rel, g_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_entry + 5) + rel == (uintptr_t)PartySessionCapacityGuard);
    check(0, 0, 0, 0);
    check(0, 19, 0, 19);
    check(19, 0, 19, 0);
    check(10, 9, 10, 9);
    check(10, 10, 0, 0);
    check(20, 0, 0, 0);
    check(0, 20, 0, 0);
    check(-1, 0, 0, 0);
    check(0, -1, 0, 0);
    check(INT_MAX, INT_MAX, 0, 0);
    g_entry[0] = 0;
    assert(!InstallPartySessionCapacityGuard());
    puts("party session capacity guard fixture passed");
    return 0;
}

/* 32-bit execution fixture for the exact C1 server-info slot guard. */
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

static BYTE g_call_entry[6] = { 0x89, 0x83, 0x6c, 0x01, 0x00, 0x00 };
static BOOL Readable(const void *p, size_t n) { return p != NULL && n == 6; }
static void WriteCode(void *p, const void *src, size_t n) { memcpy(p, src, n); }
static void Log(const char *ignored) { (void)ignored; }

#define SERVER_INFO_CAPACITY_CALL_VA ((uintptr_t)g_call_entry)
#include "../src/guards/server_info_capacity_guard.c"

static int __attribute__((naked, noinline)) FixtureDispatch(void *state __attribute__((unused)),
                                                              int public_count __attribute__((unused)))
{
    __asm__ volatile(
        "pushl %%ebx\n\t"
        "movl 8(%%esp), %%ebx\n\t"
        "movl 12(%%esp), %%eax\n\t"
        "movl $0x12345678, %%ecx\n\t"
        "call ServerInfoCapacityGuard\n\t"
        "cmpl $0x12345678, %%ecx\n\t"
        "jne 1f\n\t"
        "popl %%ebx\n\t"
        "ret\n\t"
        "1: movl $-99, %%eax\n\t"
        "popl %%ebx\n\t"
        "ret\n\t"
        : : : "eax", "ecx", "edx", "cc", "memory");
}

static void check(int total, int public_count, int want_total, int want_public)
{
    uint8_t state[0x170] = {0};
    int actual_total, actual_public;
    memcpy(state + 0x168, &total, sizeof(total));
    assert(FixtureDispatch(state, public_count) == want_public);
    memcpy(&actual_total, state + 0x168, sizeof(actual_total));
    memcpy(&actual_public, state + 0x16c, sizeof(actual_public));
    assert(actual_total == want_total && actual_public == want_public);
}

int main(void)
{
    int32_t rel;
    assert(InstallServerInfoCapacityGuard());
    assert(g_call_entry[0] == 0xe8 && g_call_entry[5] == 0x90);
    memcpy(&rel, g_call_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_call_entry + 5) + rel == (uintptr_t)ServerInfoCapacityGuard);
    check(19, 19, 19, 19);
    check(19, 0, 19, 0);
    check(0, 0, 0, 0);
    check(20, 1, 0, 0);
    check(19, 20, 0, 0);
    check(-1, 0, 0, 0);
    check(19, -1, 0, 0);
    check(INT_MAX, INT_MAX, 0, 0);
    g_call_entry[0] = 0;
    assert(!InstallServerInfoCapacityGuard());
    puts("server-info capacity guard fixture passed");
    return 0;
}

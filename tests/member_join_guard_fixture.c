/* Execute the exact C1 memberJoin call-site guard with its 32-bit cdecl ABI. */
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

static BYTE g_call_entry[5] = { 0xe8, 0xb8, 0xb1, 0xff, 0xff };
static int original_calls;
static int __attribute__((noinline, used)) FixtureOriginal(void *party)
{
    assert(party != NULL);
    ++original_calls;
    return 1;
}
static BOOL Readable(const void *p, size_t n) { return p != NULL && n == 5; }
static void WriteCode(void *p, const void *src, size_t n) { memcpy(p, src, n); }
static void Log(const char *ignored) { (void)ignored; }

#define MEMBER_JOIN_CALL_VA ((uintptr_t)g_call_entry)
#define MEMBER_JOIN_ORIGINAL_VA ((uintptr_t)FixtureOriginal)
#include "../src/guards/member_join_guard.c"

static int dispatch(void *party)
{
    return MemberJoinSlotGuard(party);
}

static void set_capacity(uint8_t *config, int public_count, int private_count)
{
    memcpy(config + 0x50, &public_count, sizeof(public_count));
    memcpy(config + 0x54, &private_count, sizeof(private_count));
}

int main(void)
{
    uint8_t config[0x58] = {0};
    void *party = config;
    int32_t rel;
    assert(InstallMemberJoinSlotGuard());
    assert(g_call_entry[0] == 0xe8);
    memcpy(&rel, g_call_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_call_entry + 5) + rel == (uintptr_t)MemberJoinSlotGuard);
    set_capacity(config, 10, 9);
    assert(dispatch(&party) == 1 && original_calls == 1);
    set_capacity(config, 0, 19);
    assert(dispatch(&party) == 1 && original_calls == 2);
    set_capacity(config, 19, 0);
    assert(dispatch(&party) == 1 && original_calls == 3);
    set_capacity(config, 10, 10);
    assert(dispatch(&party) == 0 && original_calls == 3);
    set_capacity(config, -1, 20);
    assert(dispatch(&party) == 0 && original_calls == 3);
    set_capacity(config, 20, -1);
    assert(dispatch(&party) == 0 && original_calls == 3);
    set_capacity(config, INT_MAX, INT_MAX);
    assert(dispatch(&party) == 0 && original_calls == 3);
    assert(dispatch(&party) == 0 && original_calls == 3); /* no change */
    assert(dispatch(NULL) == 0 && original_calls == 3);
    party = NULL;
    assert(dispatch(&party) == 0 && original_calls == 3);
    g_call_entry[0] = 0;
    assert(!InstallMemberJoinSlotGuard());
    puts("memberJoin slot guard fixture passed");
    return 0;
}

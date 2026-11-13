/* Execute the exact-site joinParty count hook with the 32-bit C1 register ABI. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef uintptr_t DWORD_PTR;
typedef int BOOL;
#define TRUE 1
#define FALSE 0

static BYTE g_entry[5] = { 0x83, 0xfe, 0x01, 0x7d, 0x07 };
static BYTE g_builder_entry[5] = { 0x83, 0xfb, 0x02, 0x7c, 0x4c };
static BYTE g_fill_entry[6] = { 0x85, 0xc0, 0x7e, 0x1c, 0x8b, 0xca };
static void __attribute__((naked, used)) FixtureValid(void)
{ __asm__ volatile("movl $1, %eax\n\t popl %ebx\n\t popl %esi\n\t popl %ebp\n\t ret\n\t"); }
static void __attribute__((naked, used)) FixtureLow(void)
{ __asm__ volatile("xorl %eax, %eax\n\t popl %ebx\n\t popl %esi\n\t popl %ebp\n\t ret\n\t"); }
static void __attribute__((naked, used)) FixtureHigh(void)
{ __asm__ volatile("movl $-1, %eax\n\t popl %ebx\n\t popl %esi\n\t popl %ebp\n\t ret\n\t"); }
static void __attribute__((naked, used)) FixturePair(void)
{ __asm__ volatile("movl $2, %eax\n\t popl %ebx\n\t popl %ebp\n\t ret\n\t"); }
static void __attribute__((naked, used)) FixtureSingle(void)
{ __asm__ volatile("movl $1, %eax\n\t popl %ebx\n\t popl %ebp\n\t ret\n\t"); }
static void __attribute__((naked, used)) FixtureBuilderError(void)
{ __asm__ volatile("movl $-1, %eax\n\t popl %ebx\n\t popl %ebp\n\t ret\n\t"); }
static void __attribute__((naked, used)) FixtureFillWrite(void)
{ __asm__ volatile("movl %ecx, %eax\n\t popl %ebp\n\t ret\n\t"); }
static void __attribute__((naked, used)) FixtureFillSkip(void)
{ __asm__ volatile("movl $-2, %eax\n\t popl %ebp\n\t ret\n\t"); }
static void __attribute__((naked, used)) FixtureFillError(void)
{ __asm__ volatile("movl $-1, %eax\n\t popl %ebp\n\t ret\n\t"); }
static BOOL Readable(const void *p, size_t n) { return p != NULL && n <= 6; }
static void WriteCode(void *p, const void *src, size_t n) { memcpy(p, src, n); }
static void Log(const char *ignored) { (void)ignored; }

#define JOIN_PARTY_COUNT_VA ((uintptr_t)g_entry)
#define JOIN_PARTY_COUNT_VALID ((uintptr_t)FixtureValid)
#define JOIN_PARTY_COUNT_LOW ((uintptr_t)FixtureLow)
#define JOIN_PARTY_COUNT_HIGH ((uintptr_t)FixtureHigh)
#define JOIN_PARTY_BUILDER_VA ((uintptr_t)g_builder_entry)
#define JOIN_PARTY_BUILDER_PAIR ((uintptr_t)FixturePair)
#define JOIN_PARTY_BUILDER_SINGLE ((uintptr_t)FixtureSingle)
#define JOIN_PARTY_BUILDER_ERROR ((uintptr_t)FixtureBuilderError)
#define JOIN_PARTY_FILL_VA ((uintptr_t)g_fill_entry)
#define JOIN_PARTY_FILL_WRITE ((uintptr_t)FixtureFillWrite)
#define JOIN_PARTY_FILL_SKIP ((uintptr_t)FixtureFillSkip)
#define JOIN_PARTY_FILL_ERROR ((uintptr_t)FixtureFillError)
#include "../src/guards/join_party_guard.c"

static int __attribute__((naked, used)) FixtureDispatch(
    int subparties __attribute__((unused)), int players __attribute__((unused)))
{
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %esi\n\t"
        "pushl %ebx\n\t"
        "movl 8(%ebp), %esi\n\t"
        "movl 12(%ebp), %ebx\n\t"
        "jmp JoinPartyCountGuard\n\t");
}

static int __attribute__((naked, used)) FixtureBuilderDispatch(
    int existing __attribute__((unused)), int incoming __attribute__((unused)))
{
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "movl 8(%ebp), %edx\n\t"
        "movl 12(%ebp), %ebx\n\t"
        "xorl %eax, %eax\n\t"
        "jmp JoinPartyBuilderGuard\n\t");
}

static int __attribute__((naked, used)) FixtureFillDispatch(
    int existing __attribute__((unused)), int padding __attribute__((unused)))
{
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "movl 8(%ebp), %edx\n\t"
        "movl 12(%ebp), %eax\n\t"
        "jmp JoinPartyFillGuard\n\t");
}

int main(void)
{
    int32_t rel;
    assert(InstallJoinPartyCountGuard());
    assert(g_entry[0] == 0xe9);
    memcpy(&rel, g_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_entry + 5) + rel == (uintptr_t)JoinPartyCountGuard);
    g_joinPartyCountValid = FixtureValid;
    g_joinPartyCountLow = FixtureLow;
    g_joinPartyCountHigh = FixtureHigh;
    assert(FixtureDispatch(1, 1) == 1);
    assert(FixtureDispatch(19, 19) == 1);
    assert(FixtureDispatch(0, 1) == 0);
    assert(FixtureDispatch(-1, 1) == 0);
    assert(FixtureDispatch(20, 1) == -1);
    assert(FixtureDispatch(INT_MAX, 1) == -1);
    assert(FixtureDispatch(1, 0) == -1);
    assert(FixtureDispatch(1, 20) == -1);
    assert(FixtureDispatch(1, INT_MAX) == -1);
    assert(InstallJoinPartyBuilderGuard());
    assert(g_builder_entry[0] == 0xe9);
    memcpy(&rel, g_builder_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_builder_entry + 5) + rel == (uintptr_t)JoinPartyBuilderGuard);
    g_joinPartyBuilderPair = FixturePair;
    g_joinPartyBuilderSingle = FixtureSingle;
    g_joinPartyBuilderError = FixtureBuilderError;
    assert(FixtureBuilderDispatch(0, 1) == 1);
    assert(FixtureBuilderDispatch(0, 19) == 2);
    assert(FixtureBuilderDispatch(18, 1) == 1);
    assert(FixtureBuilderDispatch(17, 2) == 2);
    assert(FixtureBuilderDispatch(19, 0) == 1);
    assert(FixtureBuilderDispatch(1, 19) == -1);
    assert(FixtureBuilderDispatch(0, 20) == -1);
    assert(FixtureBuilderDispatch(-1, 1) == -1);
    assert(FixtureBuilderDispatch(20, 0) == -1);
    assert(FixtureBuilderDispatch(INT_MAX, INT_MAX) == -1);
    g_builder_entry[0] = 0;
    assert(!InstallJoinPartyBuilderGuard());
    assert(InstallJoinPartyFillGuard());
    assert(g_fill_entry[0] == 0xe9 && g_fill_entry[5] == 0x90);
    memcpy(&rel, g_fill_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_fill_entry + 5) + rel == (uintptr_t)JoinPartyFillGuard);
    g_joinPartyFillWrite = FixtureFillWrite;
    g_joinPartyFillSkip = FixtureFillSkip;
    g_joinPartyFillError = FixtureFillError;
    assert(FixtureFillDispatch(0, 1) == 0);
    assert(FixtureFillDispatch(18, 1) == 18);
    assert(FixtureFillDispatch(19, 0) == -2);
    assert(FixtureFillDispatch(0, 0) == -2);
    assert(FixtureFillDispatch(19, 1) == -1);
    assert(FixtureFillDispatch(1, 19) == -1);
    assert(FixtureFillDispatch(0, INT_MAX) == -1);
    assert(FixtureFillDispatch(-1, 1) == -1);
    g_fill_entry[0] = 0;
    assert(!InstallJoinPartyFillGuard());
    g_entry[0] = 0;
    assert(!InstallJoinPartyCountGuard());
    puts("joinParty count guard fixture passed");
    return 0;
}

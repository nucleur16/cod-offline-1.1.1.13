/* 32-bit Linux fixture for the actual C1 challenge sender hook assembly.
 * It uses local stubs for C1's two register-ABI address comparators. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef uintptr_t DWORD_PTR;
typedef int BOOL;
#define TRUE 1
#define FALSE 0

static BYTE g_entry[6] = { 0x8b, 0x3d, 0x68, 0x80, 0x6f, 0x01 };
static BYTE g_connectCall[5] = { 0xe8, 0xea, 0xcd, 0x10, 0 };
static DWORD g_encodedClientPtr;
static BYTE g_client[0x2c];
static int g_baseCalls, g_extraCalls;

static void __attribute__((naked, used)) FixtureBaseCompare(void)
{
    __asm__ volatile(
        "incl g_baseCalls\n\t"
        "movl (%%ecx), %%eax\n\t"
        "cmpl (%%edx), %%eax\n\t"
        "jne 1f\n\t"
        "movl 4(%%ecx), %%eax\n\t"
        "cmpl 4(%%edx), %%eax\n\t"
        "jne 1f\n\t"
        "xorl %%eax, %%eax\n\t"
        "ret\n\t"
        "1: movl $1, %%eax\n\t"
        "ret\n\t" : : : "eax", "cc", "memory");
}

static void __attribute__((naked, used)) FixtureExtraCompare(void)
{
    __asm__ volatile(
        "incl g_extraCalls\n\t"
        "movl 0x14(%%ecx), %%eax\n\t"
        "cmpl 0x14(%%edx), %%eax\n\t"
        "jne 1f\n\t"
        "xorl %%eax, %%eax\n\t"
        "ret\n\t"
        "1: movl $1, %%eax\n\t"
        "ret\n\t" : : : "eax", "cc", "memory");
}

static void __attribute__((naked, used)) FixtureAccepted(void)
{
    __asm__ volatile("movl $1, %eax\n\t leal -12(%ebp), %esp\n\t popl %edi\n\t popl %esi\n\t popl %ebx\n\t popl %ebp\n\t ret\n\t");
}
static void __attribute__((naked, used)) FixtureRejected(void)
{
    __asm__ volatile("xorl %eax, %eax\n\t leal -12(%ebp), %esp\n\t popl %edi\n\t popl %esi\n\t popl %ebx\n\t popl %ebp\n\t ret\n\t");
}

static BOOL Readable(const void *p, size_t n) { return p != NULL && n <= 6; }
static void WriteCode(void *p, const void *src, size_t n) { memcpy(p, src, n); }
static void Log(const char *ignored) { (void)ignored; }

#define CHALLENGE_SENDER_ENTRY_VA ((uintptr_t)g_entry)
#define CHALLENGE_SENDER_REJECT_VA ((uintptr_t)FixtureRejected)
#define CHALLENGE_CLIENT_PTR_VA ((uintptr_t)&g_encodedClientPtr)
#define CHALLENGE_CLIENT_PTR_XOR 0x26F950CCu
#define CHALLENGE_COMPARE_BASE_VA ((uintptr_t)FixtureBaseCompare)
#define CHALLENGE_COMPARE_EXTRA_VA ((uintptr_t)FixtureExtraCompare)
#define CONNECT_SENDER_CALL_VA ((uintptr_t)g_connectCall)
#include "../challenge_sender_guard.c"

static int __attribute__((naked, used)) FixtureDispatch(const BYTE *source)
{
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "subl $0xc70, %esp\n\t"
        "movl 8(%ebp), %esi\n\t"
        "leal -0xc68(%ebp), %edi\n\t"
        "movl $6, %ecx\n\t"
        "rep movsl\n\t"
        "jmp ChallengeSenderGuard\n\t");
}

static void check(const BYTE *source, int accepted, int baseCalls, int extraCalls)
{
    g_baseCalls = g_extraCalls = 0;
    assert(FixtureDispatch(source) == accepted);
    assert(g_baseCalls == baseCalls);
    assert(g_extraCalls == extraCalls);
}

static void check_connect(const BYTE *source, const BYTE *pending,
                          int accepted, int baseCalls, int extraCalls)
{
    ChallengeNetAddress first, second;
    memcpy(first.bytes, source, 24);
    memcpy(second.bytes, pending, 24);
    g_baseCalls = g_extraCalls = 0;
    assert(ConnectSenderCompare(first, second) == accepted);
    assert(g_baseCalls == baseCalls);
    assert(g_extraCalls == extraCalls);
}

int main(void)
{
    BYTE expected[24] = { 0 }, source[24];
    int32_t rel;
    expected[0] = 4;
    expected[4] = 127;
    expected[5] = 0;
    expected[6] = 0;
    expected[7] = 1;
    expected[8] = 0x39; expected[9] = 0x30; /* port WORD, independent of opaque+0x14 */
    expected[0x14] = 0x61;
    expected[0x15] = 0x45;
    memcpy(g_client + 0x14, expected, sizeof(expected));
    g_encodedClientPtr = (DWORD)(uintptr_t)g_client ^ CHALLENGE_CLIENT_PTR_XOR;

    assert(InstallChallengeSenderGuard());
    assert(g_entry[0] == 0xe9 && g_entry[5] == 0x90);
    memcpy(&rel, g_entry + 1, sizeof(rel));
    assert((uintptr_t)(g_entry + 5) + rel == (uintptr_t)ChallengeSenderGuard);
    assert(InstallConnectSenderGuard());
    assert(g_connectCall[0] == 0xe8);
    memcpy(&rel, g_connectCall + 1, sizeof(rel));
    assert((uintptr_t)(g_connectCall + 5) + rel == (uintptr_t)ConnectSenderCompare);
    g_challengeSenderContinue = FixtureAccepted;
    g_challengeSenderReject = FixtureRejected;

    memcpy(source, expected, sizeof(source));
    check(source, 1, 1, 1);
    check_connect(source, expected, 1, 1, 1);
    source[8] ^= 1;
    check(source, 0, 1, 1);
    check_connect(source, expected, 0, 1, 1);
    source[8] ^= 1;
    expected[0] = source[0] = 5; /* preserve non-type4 comparator semantics */
    memcpy(g_client + 0x14, expected, sizeof(expected));
    source[8] ^= 1;
    check(source, 1, 1, 1);
    check_connect(source, expected, 1, 1, 1);
    source[8] ^= 1;
    expected[0] = source[0] = 4;
    memcpy(g_client + 0x14, expected, sizeof(expected));
    source[4] ^= 1;
    check(source, 0, 1, 0);
    check_connect(source, expected, 0, 1, 0);
    source[4] ^= 1;
    source[0x14] ^= 1;
    check(source, 0, 1, 1);
    check_connect(source, expected, 0, 1, 1);
    source[0x14] ^= 1;
    source[0] = 5;
    check(source, 0, 1, 0);
    g_entry[0] = 0;
    assert(!InstallChallengeSenderGuard());
    g_connectCall[0] = 0;
    assert(!InstallConnectSenderGuard());
    puts("challenge sender guard fixture passed");
    return 0;
}

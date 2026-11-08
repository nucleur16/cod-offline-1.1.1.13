/* Bind a state-3 challengeResponse to the pending address used by the
 * getchallenge send path in the exact C1 1.1.1.13 image. */
#ifndef CHALLENGE_SENDER_ENTRY_VA
#define CHALLENGE_SENDER_ENTRY_VA    0x00496729u
#define CHALLENGE_SENDER_REJECT_VA   0x00496716u
#define CHALLENGE_CLIENT_PTR_VA      0x016F8068u
#define CHALLENGE_CLIENT_PTR_XOR     0x26F950CCu
#define CHALLENGE_COMPARE_BASE_VA    0x005A3580u
#define CHALLENGE_COMPARE_EXTRA_VA   0x005A3700u
#endif
#ifndef CONNECT_SENDER_CALL_VA
#define CONNECT_SENDER_CALL_VA       0x00496891u
#endif

typedef struct { BYTE bytes[24]; } ChallengeNetAddress;

static void *g_challengeSenderContinue;
static void *g_challengeSenderReject;

/* Exact hook ABI at 0x496729: EBP owns the dispatcher frame, and the packet
 * source's six dwords are at EBP-0xC68. The displaced six-byte MOV loads the
 * encoded client pointer into EDI. Both C1 comparators receive pointers in
 * ECX/EDX and return zero on equality. Use both: for address type 4 the
 * base comparator checks +4..+7 while the second checks +0x14.
 * Explicit IPv4 WORD+8 equality is also required: the exact sockaddr-family2
 * converter0x61DEB0 sets type4/ip+4/port+8, whereas +0x14 is opaque extra data.
 * TEST $0 restores every defined success flag from the original TEST of zero;
 * POPAL still restores every saved register, and caller stack cleanup is unchanged.
 * Rejected packets take the dispatcher's existing zero-result return at 0x496716. */
static void __attribute__((naked)) ChallengeSenderGuard(void)
{
    __asm__ volatile(
        "movl %2, %%edi\n\t"
        "pushal\n\t"
        "xorl %3, %%edi\n\t"
        "leal 0x14(%%edi), %%ecx\n\t"
        "leal -0xc68(%%ebp), %%edx\n\t"
        "movl %4, %%eax\n\t"
        "call *%%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jne 1f\n\t"
        "leal 0x14(%%edi), %%ecx\n\t"
        "leal -0xc68(%%ebp), %%edx\n\t"
        "movl %5, %%eax\n\t"
        "call *%%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jne 1f\n\t"
        "cmpl $4, 0x14(%%edi)\n\t"
        "jne 3f\n\t"
        "movzwl 0x1c(%%edi), %%eax\n\t"
        "cmpw -0xc60(%%ebp), %%ax\n\t"
        "jne 1f\n\t"
        "3: testl $0, %%eax\n\t"
        "1: popal\n\t"
        "jne 2f\n\t"
        "jmp *%0\n\t"
        "2: jmp *%1\n\t"
        : : "m"(g_challengeSenderContinue), "m"(g_challengeSenderReject),
            "m"(*(volatile const DWORD *)(DWORD_PTR)CHALLENGE_CLIENT_PTR_VA),
            "i"(CHALLENGE_CLIENT_PTR_XOR), "i"(CHALLENGE_COMPARE_BASE_VA),
            "i"(CHALLENGE_COMPARE_EXTRA_VA)
        : "memory", "cc");
}

static BOOL InstallChallengeSenderGuard(void)
{
    static const BYTE expected[6] = { 0x8b, 0x3d, 0x68, 0x80, 0x6f, 0x01 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)CHALLENGE_SENDER_ENTRY_VA;
    BYTE jump[6] = { 0xe9, 0, 0, 0, 0, 0x90 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    g_challengeSenderContinue = entry + sizeof(expected);
    g_challengeSenderReject = (void *)(DWORD_PTR)CHALLENGE_SENDER_REJECT_VA;
    rel = (DWORD)(DWORD_PTR)ChallengeSenderGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("challenge sender guard: C1 jump did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("challenge sender guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

/* The state-4 branch has already pushed two 24-byte addresses by value:
 * packet source first, pending destination second. Replace only the base
 * comparison CALL at 0x496891. C1's caller remains responsible for removing
 * the 0x30 argument bytes. */
static int __attribute__((naked)) ConnectSenderCompare(
    ChallengeNetAddress source, ChallengeNetAddress pending)
{
    __asm__ volatile(
        "leal 4(%%esp), %%edx\n\t"
        "leal 0x1c(%%esp), %%ecx\n\t"
        "movl %0, %%eax\n\t"
        "call *%%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jne 1f\n\t"
        "leal 4(%%esp), %%edx\n\t"
        "leal 0x1c(%%esp), %%ecx\n\t"
        "movl %1, %%eax\n\t"
        "call *%%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jne 1f\n\t"
        "cmpl $4, 4(%%esp)\n\t"
        "jne 2f\n\t"
        "movzwl 0xc(%%esp), %%eax\n\t"
        "cmpw 0x24(%%esp), %%ax\n\t"
        "jne 1f\n\t"
        "2: testl $0, %%eax\n\t"
        "movl $1, %%eax\n\t"
        "ret\n\t"
        "1: xorl %%eax, %%eax\n\t"
        "ret\n\t"
        : : "i"(CHALLENGE_COMPARE_BASE_VA), "i"(CHALLENGE_COMPARE_EXTRA_VA)
        : "eax", "ecx", "edx", "memory", "cc");
}

static BOOL InstallConnectSenderGuard(void)
{
    static const BYTE expected[5] = { 0xe8, 0xea, 0xcd, 0x10, 0 };
    BYTE *const call = (BYTE *)(DWORD_PTR)CONNECT_SENDER_CALL_VA;
    BYTE patched[5];
    DWORD rel;
    if (!Readable(call, sizeof(expected)) || memcmp(call, expected, sizeof(expected)))
        return FALSE;
    rel = (DWORD)(DWORD_PTR)ConnectSenderCompare - (DWORD)(DWORD_PTR)(call + 5);
    memcpy(patched, expected, sizeof(patched));
    memcpy(patched + 1, &rel, 4);
    WriteCode(call, patched, sizeof(patched));
    if (memcmp(call, patched, sizeof(patched))) {
        Log("connect sender guard: C1 call did not install");
        WriteCode(call, expected, sizeof(expected));
        if (memcmp(call, expected, sizeof(expected)))
            Log("connect sender guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

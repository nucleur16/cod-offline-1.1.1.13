/* Exact C1 1.1.1.13 joinParty count bounds. Token 3 is the player count in
 * EBX; token 7 is the subparty count in ESI. The handler writes one dword per
 * subparty to [ESP+0x30+index*4]. Its downstream party table has 19 slots.
 * The player count also feeds an unchecked signed addition in that builder. */
#ifndef JOIN_PARTY_COUNT_VA
#define JOIN_PARTY_COUNT_VA       0x004B17CDu
#define JOIN_PARTY_COUNT_VALID    0x004B17D9u
#define JOIN_PARTY_COUNT_LOW      0x004B17D2u
#define JOIN_PARTY_COUNT_HIGH     0x004B1781u
#endif
#ifndef JOIN_PARTY_BUILDER_VA
#define JOIN_PARTY_BUILDER_VA     0x004AC8C0u
#define JOIN_PARTY_BUILDER_PAIR   0x004AC8C5u
#define JOIN_PARTY_BUILDER_SINGLE 0x004AC911u
#define JOIN_PARTY_BUILDER_ERROR  0x004ACB25u
#endif
#ifndef JOIN_PARTY_FILL_VA
#define JOIN_PARTY_FILL_VA        0x004AC945u
#define JOIN_PARTY_FILL_WRITE     0x004AC94Bu
#define JOIN_PARTY_FILL_SKIP      0x004AC965u
#define JOIN_PARTY_FILL_ERROR     0x004ACB25u
#endif

static void *g_joinPartyCountValid;
static void *g_joinPartyCountLow;
static void *g_joinPartyCountHigh;
static void *g_joinPartyBuilderPair;
static void *g_joinPartyBuilderSingle;
static void *g_joinPartyBuilderError;
static void *g_joinPartyFillWrite;
static void *g_joinPartyFillSkip;
static void *g_joinPartyFillError;

/* Replace CMP ESI,1 / JGE with the same lower test and conservative maxima
 * for both attacker-controlled counts. A nonpositive subparty count keeps
 * C1's error reply; out-of-range values use its balanced cleanup/return. */
static void __attribute__((naked)) JoinPartyCountGuard(void)
{
    __asm__ volatile(
        "cmpl $1, %%esi\n\t"
        "jl 1f\n\t"
        "cmpl $19, %%esi\n\t"
        "jg 2f\n\t"
        "cmpl $1, %%ebx\n\t"
        "jl 2f\n\t"
        "cmpl $19, %%ebx\n\t"
        "jg 2f\n\t"
        "jmp *%0\n\t"
        "1: jmp *%1\n\t"
        "2: jmp *%2\n\t"
        : : "m"(g_joinPartyCountValid), "m"(g_joinPartyCountLow),
            "m"(g_joinPartyCountHigh)
        : "memory", "cc");
}

static BOOL InstallJoinPartyCountGuard(void)
{
    static const BYTE expected[5] = { 0x83, 0xfe, 0x01, 0x7d, 0x07 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)JOIN_PARTY_COUNT_VA;
    BYTE jump[5] = { 0xe9, 0, 0, 0, 0 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    g_joinPartyCountValid = (void *)(DWORD_PTR)JOIN_PARTY_COUNT_VALID;
    g_joinPartyCountLow = (void *)(DWORD_PTR)JOIN_PARTY_COUNT_LOW;
    g_joinPartyCountHigh = (void *)(DWORD_PTR)JOIN_PARTY_COUNT_HIGH;
    rel = (DWORD)(DWORD_PTR)JoinPartyCountGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("joinParty count guard: C1 jump did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("joinParty count guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

/* The builder's 19-record buffer is cleared for 0x6d4 bytes at 0x4ac549.
 * EDX holds the number of existing groups, EBX the incoming group count.
 * Reject a sum above 19 before its 0x5c-stride indexed stores. Restore EAX,
 * which is the original loop index, before entering the original branches. */
static void __attribute__((naked)) JoinPartyBuilderGuard(void)
{
    __asm__ volatile(
        "testl %%edx, %%edx\n\t"
        "jl 2f\n\t"
        "cmpl $19, %%edx\n\t"
        "jg 2f\n\t"
        "testl %%ebx, %%ebx\n\t"
        "jl 2f\n\t"
        "movl $19, %%eax\n\t"
        "subl %%edx, %%eax\n\t"
        "cmpl %%eax, %%ebx\n\t"
        "jg 2f\n\t"
        "xorl %%eax, %%eax\n\t"
        "cmpl $2, %%ebx\n\t"
        "jl 1f\n\t"
        "jmp *%0\n\t"
        "1: jmp *%1\n\t"
        "2: jmp *%2\n\t"
        : : "m"(g_joinPartyBuilderPair), "m"(g_joinPartyBuilderSingle),
            "m"(g_joinPartyBuilderError)
        : "memory", "cc");
}

static BOOL InstallJoinPartyBuilderGuard(void)
{
    static const BYTE expected[5] = { 0x83, 0xfb, 0x02, 0x7c, 0x4c };
    BYTE *const entry = (BYTE *)(DWORD_PTR)JOIN_PARTY_BUILDER_VA;
    BYTE jump[5] = { 0xe9, 0, 0, 0, 0 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    g_joinPartyBuilderPair = (void *)(DWORD_PTR)JOIN_PARTY_BUILDER_PAIR;
    g_joinPartyBuilderSingle = (void *)(DWORD_PTR)JOIN_PARTY_BUILDER_SINGLE;
    g_joinPartyBuilderError = (void *)(DWORD_PTR)JOIN_PARTY_BUILDER_ERROR;
    rel = (DWORD)(DWORD_PTR)JoinPartyBuilderGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("joinParty builder guard: C1 jump did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("joinParty builder guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

/* The optional filler appends EAX more 0x5c-byte records after EDX existing
 * records. Bound their sum before the first store, even if packet-supplied
 * group sizes made the computed remaining-player count inconsistent. The
 * patched six bytes include MOV ECX,EDX, so emulate it before returning
 * at the following IMUL. */
static void __attribute__((naked)) JoinPartyFillGuard(void)
{
    __asm__ volatile(
        "testl %%eax, %%eax\n\t"
        "jle 1f\n\t"
        "testl %%edx, %%edx\n\t"
        "jl 2f\n\t"
        "cmpl $19, %%edx\n\t"
        "jg 2f\n\t"
        "movl $19, %%ecx\n\t"
        "subl %%edx, %%ecx\n\t"
        "cmpl %%ecx, %%eax\n\t"
        "jg 2f\n\t"
        "movl %%edx, %%ecx\n\t"
        "jmp *%0\n\t"
        "1: jmp *%1\n\t"
        "2: jmp *%2\n\t"
        : : "m"(g_joinPartyFillWrite), "m"(g_joinPartyFillSkip),
            "m"(g_joinPartyFillError)
        : "memory", "cc");
}

static BOOL InstallJoinPartyFillGuard(void)
{
    static const BYTE expected[6] = { 0x85, 0xc0, 0x7e, 0x1c, 0x8b, 0xca };
    BYTE *const entry = (BYTE *)(DWORD_PTR)JOIN_PARTY_FILL_VA;
    BYTE jump[6] = { 0xe9, 0, 0, 0, 0, 0x90 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    g_joinPartyFillWrite = (void *)(DWORD_PTR)JOIN_PARTY_FILL_WRITE;
    g_joinPartyFillSkip = (void *)(DWORD_PTR)JOIN_PARTY_FILL_SKIP;
    g_joinPartyFillError = (void *)(DWORD_PTR)JOIN_PARTY_FILL_ERROR;
    rel = (DWORD)(DWORD_PTR)JoinPartyFillGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("joinParty fill guard: C1 jump did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("joinParty fill guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

/* Exact C1 session creation stores unsigned byte slot counts without a 19 cap. */
#ifndef PARTY_SESSION_CAPACITY_STORE_VA
#define PARTY_SESSION_CAPACITY_STORE_VA 0x0068AF81u
#endif

/* ESI points to the session object, EAX is private slots, ECX public slots.
 * EDX is live at the next instruction and is never changed here. Invalid
 * counts are stored as zero so no out-of-range party array bound is exposed. */
static void __attribute__((naked)) PartySessionCapacityGuard(void)
{
    __asm__ volatile(
        "testl %%eax, %%eax\n\t"
        "js 1f\n\t"
        "cmpl $19, %%eax\n\t"
        "jg 1f\n\t"
        "testl %%ecx, %%ecx\n\t"
        "js 1f\n\t"
        "cmpl $19, %%ecx\n\t"
        "jg 1f\n\t"
        "pushl %%eax\n\t"
        "addl %%ecx, %%eax\n\t"
        "cmpl $19, %%eax\n\t"
        "popl %%eax\n\t"
        "jg 1f\n\t"
        "jmp 2f\n\t"
        "1: xorl %%eax, %%eax\n\t"
        "xorl %%ecx, %%ecx\n\t"
        "2: movl %%eax, 0x54(%%esi)\n\t"
        "movl %%ecx, 0x50(%%esi)\n\t"
        "ret\n\t"
        : : : "eax", "ecx", "cc", "memory");
}

static BOOL InstallPartySessionCapacityGuard(void)
{
    static const BYTE expected[6] = { 0x89, 0x46, 0x54, 0x89, 0x4e, 0x50 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)PARTY_SESSION_CAPACITY_STORE_VA;
    BYTE replacement[6] = { 0xe8, 0, 0, 0, 0, 0x90 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    rel = (DWORD)(DWORD_PTR)PartySessionCapacityGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(replacement + 1, &rel, 4);
    WriteCode(entry, replacement, sizeof(replacement));
    if (memcmp(entry, replacement, sizeof(replacement))) {
        Log("party session capacity guard: C1 store did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("party session capacity guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

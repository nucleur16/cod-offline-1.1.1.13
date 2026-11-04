/* Exact C1 party-session refresh can derive a negative slot field from count. */
#ifndef PARTY_SESSION_REFRESH_STORE_VA
#define PARTY_SESSION_REFRESH_STORE_VA 0x0068B3EFu
#endif

/* ESI is the session object, EAX/ECX are the two capacity fields, EDX is
 * live at the following flags store. Reject negative or >19 combined
 * capacities before the two original MOVs. */
static void __attribute__((naked)) PartySessionRefreshGuard(void)
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
        "2: movl %%eax, 0x50(%%esi)\n\t"
        "movl %%ecx, 0x54(%%esi)\n\t"
        "ret\n\t"
        : : : "eax", "ecx", "cc", "memory");
}

static BOOL InstallPartySessionRefreshGuard(void)
{
    static const BYTE expected[6] = { 0x89, 0x46, 0x50, 0x89, 0x4e, 0x54 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)PARTY_SESSION_REFRESH_STORE_VA;
    BYTE replacement[6] = { 0xe8, 0, 0, 0, 0, 0x90 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    rel = (DWORD)(DWORD_PTR)PartySessionRefreshGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(replacement + 1, &rel, 4);
    WriteCode(entry, replacement, sizeof(replacement));
    if (memcmp(entry, replacement, sizeof(replacement))) {
        Log("party session refresh guard: C1 store did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("party session refresh guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

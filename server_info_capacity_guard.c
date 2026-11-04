/* Exact C1 CG_ParseServerInfo slot values feed a 19-entry party array. */
#ifndef SERVER_INFO_CAPACITY_CALL_VA
#define SERVER_INFO_CAPACITY_CALL_VA 0x00465FA2u
#endif

/* At the replaced instruction, EBX is the server-info state, EAX is the
 * parsed public count, and [EBX+0x168] is the parsed total count. ECX must
 * survive for the following test; EDX is dead before the next use. The
 * original MOV does not set flags, but the next instruction is TEST ECX. */
static void __attribute__((naked)) ServerInfoCapacityGuard(void)
{
    __asm__ volatile(
        "movl 0x168(%%ebx), %%edx\n\t"
        "testl %%edx, %%edx\n\t"
        "js 1f\n\t"
        "cmpl $19, %%edx\n\t"
        "jg 1f\n\t"
        "testl %%eax, %%eax\n\t"
        "js 1f\n\t"
        "cmpl %%edx, %%eax\n\t"
        "jg 1f\n\t"
        "movl %%eax, 0x16c(%%ebx)\n\t"
        "ret\n\t"
        "1: xorl %%eax, %%eax\n\t"
        "movl %%eax, 0x168(%%ebx)\n\t"
        "movl %%eax, 0x16c(%%ebx)\n\t"
        "ret\n\t"
        : : : "eax", "edx", "cc", "memory");
}

static BOOL InstallServerInfoCapacityGuard(void)
{
    static const BYTE expected[6] = { 0x89, 0x83, 0x6c, 0x01, 0x00, 0x00 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)SERVER_INFO_CAPACITY_CALL_VA;
    BYTE replacement[6] = { 0xe8, 0, 0, 0, 0, 0x90 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    rel = (DWORD)(DWORD_PTR)ServerInfoCapacityGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(replacement + 1, &rel, 4);
    WriteCode(entry, replacement, sizeof(replacement));
    if (memcmp(entry, replacement, sizeof(replacement))) {
        Log("server-info capacity guard: C1 call did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("server-info capacity guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

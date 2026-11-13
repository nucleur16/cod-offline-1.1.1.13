/* Exact C1 CL_Live_SessionUpdate slot values feed the 19-entry party array. */
#ifndef SESSION_CAPACITY_ENTRY_VA
#define SESSION_CAPACITY_ENTRY_VA 0x0049C350u
#endif
#ifndef SESSION_PUBLIC_DVAR_SLOT_VA
#define SESSION_PUBLIC_DVAR_SLOT_VA 0x027F4F00u
#endif
#ifndef SESSION_TOTAL_DVAR_SLOT_VA
#define SESSION_TOTAL_DVAR_SLOT_VA 0x0172010Cu
#endif

/* Replaces the first MOV in a no-prolog, no-argument function. A valid
 * call recreates ECX and returns to entry+6. An invalid pair discards
 * this injected return address and returns to the original caller,
 * leaving the last accepted party configuration unchanged. EAX and EDX
 * are overwritten before use on the normal path. */
static void __attribute__((naked)) SessionCapacityEntryGuard(void)
{
    __asm__ volatile(
        "movl %c0, %%ecx\n\t"
        "testl %%ecx, %%ecx\n\t"
        "jz 1f\n\t"
        "movl %c1, %%edx\n\t"
        "testl %%edx, %%edx\n\t"
        "jz 1f\n\t"
        "movl 0xc(%%edx), %%edx\n\t"
        "testl %%edx, %%edx\n\t"
        "js 1f\n\t"
        "cmpl $19, %%edx\n\t"
        "jg 1f\n\t"
        "movl 0xc(%%ecx), %%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "js 1f\n\t"
        "cmpl %%edx, %%eax\n\t"
        "jg 1f\n\t"
        "ret\n\t"
        "1: xorl %%eax, %%eax\n\t"
        "addl $4, %%esp\n\t"
        "ret\n\t"
        :
        : "i"(SESSION_PUBLIC_DVAR_SLOT_VA),
          "i"(SESSION_TOTAL_DVAR_SLOT_VA)
        : "eax", "ecx", "edx", "cc", "memory");
}

static BOOL InstallSessionCapacityGuard(void)
{
    static const BYTE expected[6] = { 0x8b, 0x0d, 0x00, 0x4f, 0x7f, 0x02 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)SESSION_CAPACITY_ENTRY_VA;
    BYTE replacement[6] = { 0xe8, 0, 0, 0, 0, 0x90 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    rel = (DWORD)(DWORD_PTR)SessionCapacityEntryGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(replacement + 1, &rel, 4);
    WriteCode(entry, replacement, sizeof(replacement));
    if (memcmp(entry, replacement, sizeof(replacement))) {
        Log("session capacity guard: C1 entry did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("session capacity guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

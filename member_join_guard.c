/* Exact C1 memberJoin slot bound. Its only call to 0x4ad370 scans the party
 * array with config public+private capacity, while the array has 19 entries.
 * Reject an invalid configured bound before the first indexed read/write. */
#ifndef MEMBER_JOIN_CALL_VA
#define MEMBER_JOIN_CALL_VA 0x004B21B3u
#endif
#ifndef MEMBER_JOIN_ORIGINAL_VA
#define MEMBER_JOIN_ORIGINAL_VA 0x004AD370u
#endif

static void *g_memberJoinOriginal;

/* Called with C1's cdecl stack: return address, party pointer, then the
 * remaining 0x58 bytes of arguments. A tail jump keeps that stack intact.
 * EAX/ECX/EDX are caller-saved; EBX/ESI/EDI/EBP are not changed. */
static int __attribute__((naked)) MemberJoinSlotGuard(void *party __attribute__((unused)))
{
    __asm__ volatile(
        "movl 4(%%esp), %%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jz 1f\n\t"
        "movl (%%eax), %%eax\n\t"
        "testl %%eax, %%eax\n\t"
        "jz 1f\n\t"
        "movl 0x50(%%eax), %%edx\n\t"
        "movl 0x54(%%eax), %%ecx\n\t"
        "testl %%edx, %%edx\n\t"
        "js 1f\n\t"
        "testl %%ecx, %%ecx\n\t"
        "js 1f\n\t"
        "cmpl $19, %%edx\n\t"
        "jg 1f\n\t"
        "cmpl $19, %%ecx\n\t"
        "jg 1f\n\t"
        "addl %%edx, %%ecx\n\t"
        "cmpl $19, %%ecx\n\t"
        "jg 1f\n\t"
        "jmp *%0\n\t"
        "1: xorl %%eax, %%eax\n\t"
        "ret\n\t"
        : : "m"(g_memberJoinOriginal) : "eax", "ecx", "edx", "cc", "memory");
}

static BOOL InstallMemberJoinSlotGuard(void)
{
    static const BYTE expected[5] = { 0xe8, 0xb8, 0xb1, 0xff, 0xff };
    BYTE *const entry = (BYTE *)(DWORD_PTR)MEMBER_JOIN_CALL_VA;
    BYTE jump[5] = { 0xe8, 0, 0, 0, 0 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    g_memberJoinOriginal = (void *)(DWORD_PTR)MEMBER_JOIN_ORIGINAL_VA;
    rel = (DWORD)(DWORD_PTR)MemberJoinSlotGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("memberJoin slot guard: C1 call did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("memberJoin slot guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

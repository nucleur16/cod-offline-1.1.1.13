/* Exact C1 party refresh loops over 19 records, but uses a dvar bound. */
#ifndef PARTY_SLOT_LOOP_COMPARE_VA
#define PARTY_SLOT_LOOP_COMPARE_VA 0x004B398Cu
#endif
#ifndef PARTY_SLOT_LOOP_BODY_VA
#define PARTY_SLOT_LOOP_BODY_VA 0x004B3930u
#endif

/* At the replaced CMP/JL, ESI is the next index and ECX the total-slot
 * dvar pointer. If both original signed count and array limit allow one
 * more iteration, retarget only the injected CALL return to the original
 * loop body. Otherwise continue after the replaced branch. No registers
 * are changed. */
static void __attribute__((naked)) PartySlotLoopGuard(void)
{
    __asm__ volatile(
        "cmpl $19, %%esi\n\t"
        "jge 1f\n\t"
        "testl %%ecx, %%ecx\n\t"
        "jz 1f\n\t"
        "cmpl 0xc(%%ecx), %%esi\n\t"
        "jge 1f\n\t"
        "movl $%c0, (%%esp)\n\t"
        "1: ret\n\t"
        : : "i"(PARTY_SLOT_LOOP_BODY_VA) : "cc", "memory");
}

static BOOL InstallPartySlotLoopGuard(void)
{
    static const BYTE expected[5] = { 0x3b, 0x71, 0x0c, 0x7c, 0x9f };
    BYTE *const entry = (BYTE *)(DWORD_PTR)PARTY_SLOT_LOOP_COMPARE_VA;
    BYTE replacement[5] = { 0xe8, 0, 0, 0, 0 };
    DWORD rel;
    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    rel = (DWORD)(DWORD_PTR)PartySlotLoopGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(replacement + 1, &rel, 4);
    WriteCode(entry, replacement, sizeof(replacement));
    if (memcmp(entry, replacement, sizeof(replacement))) {
        Log("party slot loop guard: C1 branch did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("party slot loop guard: rollback failed");
        return FALSE;
    }
    return TRUE;
}

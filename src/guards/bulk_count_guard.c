/* Exact C1 1.1.1.13 bulk record count bound. The destination at
 * *0x6c267dc+0x7234d4 is cleared for 0x4fc bytes by the corresponding
 * writer. Its 0x3c-byte header leaves exactly 19 0x40-byte records. */
#ifndef BULK_COUNT_RUN_VA
#define BULK_COUNT_RUN_VA 0x0072FECAu
#define BULK_COUNT_EMPTY_VA 0x0072FEEBu
#define BULK_COUNT_EXIT_VA 0x0072FE8Cu
#endif
#ifndef BULK_INDIRECT_COUNT_RUN_VA
#define BULK_INDIRECT_COUNT_RUN_VA 0x007310C6u
#define BULK_INDIRECT_COUNT_SKIP_VA 0x007310FDu
#endif
#ifndef BULK_POSTREAD_GUARD_VA
#define BULK_POSTREAD_GUARD_VA 0x00731428u
#define BULK_POSTREAD_SKIP_VA 0x007314DCu
#endif

static void *g_bulkCountContinue;
static void *g_bulkCountEmpty;
static void *g_bulkCountExit;
static void *g_bulkIndirectContinue;
static void *g_bulkIndirectSkip;
static void *g_bulkPostContinue;
static void *g_bulkPostSkip;

/* Exact hook ABI: EDI points at the destination/count, EBX is the zero-based
 * record index, ESI is the message whose first dword is its overflow flag.
 * Reproduce the displaced CMP/JLE/LEA instructions on accepted counts. */
static void __attribute__((naked)) BulkCountRunGuard(void)
{
    __asm__ volatile(
        "cmpl $19, (%%edi)\n\t"
        "jg 2f\n\t"
        "cmpl %%ebx, (%%edi)\n\t"
        "jle 1f\n\t"
        "leal 0x3c(%%edi), %%ecx\n\t"
        "jmp *%0\n\t"
        "1: jmp *%1\n\t"
        "2: movl $1, (%%esi)\n\t"
        "jmp *%2\n\t"
        : : "m"(g_bulkCountContinue), "m"(g_bulkCountEmpty),
            "m"(g_bulkCountExit)
        : "memory", "cc");
}

/* The second reader uses an indirect read callback. ESI is the same global
 * state base (loaded at 0x730d62), and the count is at ESI+0x7234d4. The
 * overwritten MOV does not affect flags; restore the original count-vs-zero
 * flags before returning to the adjacent JLE. Oversize counts become empty
 * before the 0x40-byte loop or its downstream copy can run. */
static void __attribute__((naked)) BulkIndirectCountRunGuard(void)
{
    __asm__ volatile(
        "movl $0, -0x10(%%ebp)\n\t"
        "cmpl $19, 0x7234d4(%%esi)\n\t"
        "jg 1f\n\t"
        "cmpl $0, 0x7234d4(%%esi)\n\t"
        "jmp *%0\n\t"
        "1: movl $0, 0x7234d4(%%esi)\n\t"
        "jmp *%1\n\t"
        : : "m"(g_bulkIndirectContinue), "m"(g_bulkIndirectSkip)
        : "memory", "cc");
}

/* The postreader is also called independently at 0x599b06. Its destination
 * holds 19 records, and a separate exact-image iterator at 0x70cf14-0x70cf53
 * bounds the 0xb50-stride player table to indices 0..18. Validate the entire
 * input list before copying or indexing any record. */
static void __attribute__((naked)) BulkPostreadGuard(void)
{
    __asm__ volatile(
        "movl (%%edx), %%ecx\n\t"
        "cmpl $19, %%ecx\n\t"
        "ja 2f\n\t"
        "testl %%ecx, %%ecx\n\t"
        "jz 1f\n\t"
        "leal 0x3c(%%edx), %%eax\n\t"
        "3: cmpl $19, (%%eax)\n\t"
        "jae 2f\n\t"
        "addl $0x40, %%eax\n\t"
        "decl %%ecx\n\t"
        "jne 3b\n\t"
        "1: movl %%edx, -0x10(%%ebp)\n\t"
        "cmpl $0, (%%edx)\n\t"
        "jmp *%0\n\t"
        "2: movl $0, 0xbeb18(%%ebx)\n\t"
        "jmp *%1\n\t"
        : : "m"(g_bulkPostContinue), "m"(g_bulkPostSkip)
        : "eax", "ecx", "memory", "cc");
}

static BOOL InstallBulkCountGuard(void)
{
    static const BYTE expected[7] = { 0x39, 0x1f, 0x7e, 0x1d, 0x8d, 0x4f, 0x3c };
    static const BYTE indirect_expected[7] = { 0xc7, 0x45, 0xf0, 0, 0, 0, 0 };
    static const BYTE post_expected[6] = { 0x83, 0x3a, 0, 0x89, 0x55, 0xf0 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)BULK_COUNT_RUN_VA;
    BYTE *const indirect = (BYTE *)(DWORD_PTR)BULK_INDIRECT_COUNT_RUN_VA;
    BYTE *const post = (BYTE *)(DWORD_PTR)BULK_POSTREAD_GUARD_VA;
    BYTE jump[7] = { 0xe9, 0, 0, 0, 0, 0x90, 0x90 };
    DWORD rel;

    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)) ||
        !Readable(indirect, sizeof(indirect_expected)) ||
        memcmp(indirect, indirect_expected, sizeof(indirect_expected)) ||
        !Readable(post, sizeof(post_expected)) ||
        memcmp(post, post_expected, sizeof(post_expected)))
        return FALSE;
    g_bulkCountContinue = entry + sizeof(expected);
    g_bulkCountEmpty = (void *)(DWORD_PTR)BULK_COUNT_EMPTY_VA;
    g_bulkCountExit = (void *)(DWORD_PTR)BULK_COUNT_EXIT_VA;
    rel = (DWORD)(DWORD_PTR)BulkCountRunGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("bulk count guard: parser jump did not install");
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(entry, expected, sizeof(expected)))
            Log("bulk count guard: parser jump rollback failed");
        return FALSE;
    }
    g_bulkIndirectContinue = indirect + sizeof(indirect_expected);
    g_bulkIndirectSkip = (void *)(DWORD_PTR)BULK_INDIRECT_COUNT_SKIP_VA;
    rel = (DWORD)(DWORD_PTR)BulkIndirectCountRunGuard - (DWORD)(DWORD_PTR)(indirect + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(indirect, jump, sizeof(jump));
    if (memcmp(indirect, jump, sizeof(jump))) {
        Log("bulk count guard: indirect reader jump did not install");
        WriteCode(indirect, indirect_expected, sizeof(indirect_expected));
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(indirect, indirect_expected, sizeof(indirect_expected)) ||
            memcmp(entry, expected, sizeof(expected)))
            Log("bulk count guard: parser jump rollback failed");
        return FALSE;
    }
    g_bulkPostContinue = post + sizeof(post_expected);
    g_bulkPostSkip = (void *)(DWORD_PTR)BULK_POSTREAD_SKIP_VA;
    rel = (DWORD)(DWORD_PTR)BulkPostreadGuard - (DWORD)(DWORD_PTR)(post + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(post, jump, sizeof(post_expected));
    if (memcmp(post, jump, sizeof(post_expected))) {
        Log("bulk count guard: postreader jump did not install");
        WriteCode(post, post_expected, sizeof(post_expected));
        WriteCode(indirect, indirect_expected, sizeof(indirect_expected));
        WriteCode(entry, expected, sizeof(expected));
        if (memcmp(post, post_expected, sizeof(post_expected)) ||
            memcmp(indirect, indirect_expected, sizeof(indirect_expected)) ||
            memcmp(entry, expected, sizeof(expected)))
            Log("bulk count guard: postreader rollback failed");
        return FALSE;
    }
    return TRUE;
}

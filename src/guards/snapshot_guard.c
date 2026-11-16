/* Exact C1 1.1.1.13 snapshot bounds. A full snapshot writes 0x114-byte
 * entities into a 1024-entry static array at 0x6d36abc. Its append routine
 * lacks a pre-write count check, and three field decoders accept wire runs
 * beyond their descriptor limits. Reject through the message overflow flag
 * before the relevant write, skip-copy or descriptor access. */
#ifndef SNAPSHOT_APPEND_VA
#define SNAPSHOT_APPEND_VA 0x0049CA00u
#define SNAPSHOT_FULL_ENTITY_COUNT_VA 0x06D36AB8u
#endif
#ifndef SNAPSHOT_PLAYERSTATE_RUN_VA
#define SNAPSHOT_PLAYERSTATE_RUN_VA 0x005A1B19u
#define SNAPSHOT_PLAYERSTATE_EXIT_VA 0x005A2071u
#endif
#ifndef SNAPSHOT_ENTITY_RUN_VA
#define SNAPSHOT_ENTITY_RUN_VA 0x005A0D52u
#define SNAPSHOT_ENTITY_EXIT_VA 0x005A0D40u
#endif
#ifndef SNAPSHOT_GENERIC_RUN_VA
#define SNAPSHOT_GENERIC_RUN_VA 0x005A0AE4u
#define SNAPSHOT_GENERIC_EXIT_VA 0x005A0BEEu
#endif

typedef void (*SnapshotAppendFn)(void);
static SnapshotAppendFn g_snapshotAppendOriginal;
static SnapshotAppendFn g_snapshotPlayerstateOriginal;
static void *g_snapshotPlayerstateExit;
static SnapshotAppendFn g_snapshotEntityOriginal;
static void *g_snapshotEntityExit;
static SnapshotAppendFn g_snapshotGenericOriginal;
static void *g_snapshotGenericExit;
#if K33_DIAGNOSTICS
static volatile unsigned g_snapshotGuardCalls[4], g_snapshotGuardRejects[4];
static volatile unsigned g_snapshotEntityRejectedIndex, g_snapshotEntityRejectedCount;
static volatile unsigned g_snapshotInvalidObserved;
static void *g_snapshotInvalidFramePrintOriginal;
static void __cdecl SnapshotInvalidFrameReport(const DWORD *regs) __attribute__((used, noinline));
static void __cdecl SnapshotInvalidFrameReport(const DWORD *regs)
{
    static DWORD last;
    static unsigned reports;
    const BYTE *base = (const BYTE *)(DWORD_PTR)regs[7]; /* pushal saved EAX */
    const BYTE *frame = (const BYTE *)(DWORD_PTR)regs[4]; /* EBX */
    const BYTE *conn = (const BYTE *)(DWORD_PTR)regs[0]; /* EDI */
    DWORD now = GetTickCount();
    if (reports >= 16 || (last && now - last < 150) ||
        !Readable(base + 0x3378, 4) || !Readable(frame + 0x3378, 4) ||
        !Readable(conn + 0x9ce0, 4)) return;
    last = now;
    ++reports;
    g_snapshotInvalidObserved = 1;
    Log("snapshot invalid base: ref=%p valid=%u fields=%u/%u/%u current=%p valid=%u fields=%u/%u/%u connseq=%u delta=%d",
        base, *(const DWORD *)(base + 0x3368), *(const DWORD *)(base + 0x3370),
        *(const DWORD *)(base + 0x3374), *(const DWORD *)(base + 0x3378),
        frame, *(const DWORD *)(frame + 0x3368), *(const DWORD *)(frame + 0x3370),
        *(const DWORD *)(frame + 0x3374), *(const DWORD *)(frame + 0x3378),
        *(const DWORD *)(conn + 0x9cdc) ^ 0xa9022cfdu, regs[6]);
}
static void __attribute__((naked)) SnapshotInvalidFramePrintHook(void)
{
    __asm__ volatile(
        "pushal\n\t"
        "pushl %%esp\n\t"
        "call _SnapshotInvalidFrameReport\n\t"
        "addl $4, %%esp\n\t"
        "popal\n\t"
        "jmp *%0\n\t"
        : : "m"(g_snapshotInvalidFramePrintOriginal) : "memory");
}
static BOOL InstallSnapshotInvalidFrameDiagnostic(void)
{
    const DWORD site = 0x0049da3du, original = 0x00594540u;
    if (!CallsTarget(site, original)) return FALSE;
    g_snapshotInvalidFramePrintOriginal = (void *)(DWORD_PTR)original;
    RedirectCall(site, (void *)SnapshotInvalidFramePrintHook);
    return CallsTarget(site, (DWORD)(DWORD_PTR)SnapshotInvalidFramePrintHook);
}
static void SnapshotGuardReport(void)
{
    static DWORD last;
    static unsigned reports;
    DWORD now = GetTickCount();
    if (!g_snapshotInvalidObserved || reports >= 8 || now - last < 2000) return;
    last = now;
    ++reports;
    Log("snapshot guard diagnostic: calls %u/%u/%u/%u rejects %u/%u/%u/%u",
        g_snapshotGuardCalls[0], g_snapshotGuardCalls[1],
        g_snapshotGuardCalls[2], g_snapshotGuardCalls[3],
        g_snapshotGuardRejects[0], g_snapshotGuardRejects[1],
        g_snapshotGuardRejects[2], g_snapshotGuardRejects[3]);
    if (g_snapshotGuardRejects[2])
        Log("snapshot entity reject diagnostic: index=%u count=%u",
            g_snapshotEntityRejectedIndex, g_snapshotEntityRejectedCount);
}
#endif

/* Entry ABI: ESI=snapshot, ECX=entity number, first stack argument=msg.
 * The caller checks msg[0] after each append and stops parsing when set.
 * EAX is ignored by both exact call sites. Valid calls tail-jump to a
 * trampoline containing the complete 11-byte prologue. */
static void __attribute__((naked)) SnapshotAppendGuard(void)
{
    __asm__ volatile(
        "testl %%ecx, %%ecx\n\t"
        "js 1f\n\t"
        "cmpl $0x7fe, %%ecx\n\t"
        "ja 1f\n\t"
        "testb $8, 0x336c(%%esi)\n\t"
        "jz 2f\n\t"
        "cmpl $0x400, %1\n\t"
        "jae 1f\n\t"
        "2: "
#if K33_DIAGNOSTICS
        "incl %2\n\t"
#endif
        "jmp *%0\n\t"
        "1: "
#if K33_DIAGNOSTICS
        "incl %3\n\t"
#endif
        "movl 4(%%esp), %%eax\n\t"
        "movl $1, (%%eax)\n\t"
        "xorl %%eax, %%eax\n\t"
        "ret\n\t"
        : : "m"(g_snapshotAppendOriginal),
            "m"(*(volatile unsigned *)(DWORD_PTR)SNAPSHOT_FULL_ENTITY_COUNT_VA)
#if K33_DIAGNOSTICS
            , "m"(g_snapshotGuardCalls[0]), "m"(g_snapshotGuardRejects[0])
#endif
        : "memory");
}

/* At 0x5a1b19, EDI is the decoded playerstate field index and EBP owns the
 * 0x3384-local decoded field count. C1 reads the descriptor at EDI before
 * its loop's EDI<count test, including the valid EDI==count terminal slot.
 * The 245-entry table still requires EDI<245. A run beyond count or the
 * table is rejected before descriptor access. */
static void __attribute__((naked)) SnapshotPlayerstateRunGuard(void)
{
    __asm__ volatile(
        "cmpl $0xf5, %%edi\n\t"
        "jae 1f\n\t"
        "cmpl -0x3384(%%ebp), %%edi\n\t"
        "ja 1f\n\t"
        
#if K33_DIAGNOSTICS
        "incl %2\n\t"
#endif
        "jmp *%0\n\t"
        "1: "
#if K33_DIAGNOSTICS
        "incl %3\n\t"
#endif
        "movl $1, (%%esi)\n\t"
        "jmp *%1\n\t"
        : : "m"(g_snapshotPlayerstateOriginal), "m"(g_snapshotPlayerstateExit)
#if K33_DIAGNOSTICS
            , "m"(g_snapshotGuardCalls[1]), "m"(g_snapshotGuardRejects[1])
#endif
        : "memory");
}

/* The entity delta decoder has the same first-run shape. ESI is a field
 * index, [EBP-4] the validated decoded count, and [EBP+8] the message.
 * C1 legitimately reads the ESI==count terminal descriptor before its
 * comparison at 0x5a0dbc; a real Outpost baseline used index=count=48.
 * The hard descriptor-table bound remains ESI<0x42. */
static void __attribute__((naked)) SnapshotEntityRunGuard(void)
{
    __asm__ volatile(
        "cmpl $0x42, %%esi\n\t"
        "jae 1f\n\t"
        "cmpl -4(%%ebp), %%esi\n\t"
        "ja 1f\n\t"
        
#if K33_DIAGNOSTICS
        "incl %2\n\t"
#endif
        "jmp *%0\n\t"
        "1: "
#if K33_DIAGNOSTICS
        "incl %3\n\t"
        "movl %%esi, %4\n\t"
        "pushl %%eax\n\t"
        "movl -4(%%ebp), %%eax\n\t"
        "movl %%eax, %5\n\t"
        "popl %%eax\n\t"
#endif
        "pushl %%eax\n\t"
        "movl 8(%%ebp), %%eax\n\t"
        "movl $1, (%%eax)\n\t"
        "popl %%eax\n\t"
        "jmp *%1\n\t"
        : : "m"(g_snapshotEntityOriginal), "m"(g_snapshotEntityExit)
#if K33_DIAGNOSTICS
            , "m"(g_snapshotGuardCalls[2]), "m"(g_snapshotGuardRejects[2]),
              "m"(g_snapshotEntityRejectedIndex), "m"(g_snapshotEntityRejectedCount)
#endif
        : "memory");
}

/* Shared delta decoder for two further snapshot record types. ESI is the
 * just-decoded run, EAX the previous index, [EBP-8] the inclusive decoded
 * last index, and [EBP+0x18] the inclusive descriptor-table maximum. The
 * pending 5a09d0 stack argument must be removed on both paths. */
static void __attribute__((naked)) SnapshotGenericRunGuard(void)
{
    __asm__ volatile(
        "pushl %%edx\n\t"
        "leal (%%esi,%%eax), %%edx\n\t"
        "cmpl -8(%%ebp), %%edx\n\t"
        "ja 1f\n\t"
        "cmpl 0x18(%%ebp), %%edx\n\t"
        "ja 1f\n\t"
        "popl %%edx\n\t"
#if K33_DIAGNOSTICS
        "incl %2\n\t"
#endif
        "jmp *%0\n\t"
        "1: popl %%edx\n\t"
#if K33_DIAGNOSTICS
        "incl %3\n\t"
#endif
        "addl $4, %%esp\n\t"
        "movl $1, (%%edi)\n\t"
        "jmp *%1\n\t"
        : : "m"(g_snapshotGenericOriginal), "m"(g_snapshotGenericExit)
#if K33_DIAGNOSTICS
            , "m"(g_snapshotGuardCalls[3]), "m"(g_snapshotGuardRejects[3])
#endif
        : "memory");
}

static BOOL InstallSnapshotAppendGuard(void)
{
    static const BYTE expected[11] = {
        0x55, 0x8b, 0xec, 0x51, 0xf6, 0x86, 0x6c, 0x33, 0x00, 0x00, 0x08
    };
    BYTE *const entry = (BYTE *)(DWORD_PTR)SNAPSHOT_APPEND_VA;
    BYTE *trampoline;
    BYTE jump[11] = { 0xe9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    DWORD rel, old;

    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    trampoline = (BYTE *)VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!trampoline) return FALSE;
    memcpy(trampoline, expected, sizeof(expected));
    trampoline[11] = 0xe9;
    rel = (DWORD)(DWORD_PTR)(entry + 11) - (DWORD)(DWORD_PTR)(trampoline + 16);
    memcpy(trampoline + 12, &rel, 4);
    if (!VirtualProtect(trampoline, 16, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, 16);
    memcpy(&g_snapshotAppendOriginal, &trampoline, sizeof(trampoline));
    rel = (DWORD)(DWORD_PTR)SnapshotAppendGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("snapshot guard: parser jump did not install");
        return FALSE; /* startup stops before game entry */
    }
    return TRUE;
}

static BOOL InstallSnapshotPlayerstateRunGuard(void)
{
    static const BYTE expected[5] = { 0x8d, 0x48, 0x01, 0x3b, 0xcf };
    BYTE *const entry = (BYTE *)(DWORD_PTR)SNAPSHOT_PLAYERSTATE_RUN_VA;
    BYTE *trampoline;
    BYTE jump[5] = { 0xe9, 0, 0, 0, 0 };
    DWORD rel, old;

    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    trampoline = (BYTE *)VirtualAlloc(NULL, 10, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!trampoline) return FALSE;
    memcpy(trampoline, expected, sizeof(expected));
    trampoline[5] = 0xe9;
    rel = (DWORD)(DWORD_PTR)(entry + 5) - (DWORD)(DWORD_PTR)(trampoline + 10);
    memcpy(trampoline + 6, &rel, 4);
    if (!VirtualProtect(trampoline, 10, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, 10);
    memcpy(&g_snapshotPlayerstateOriginal, &trampoline, sizeof(trampoline));
    g_snapshotPlayerstateExit = (void *)(DWORD_PTR)SNAPSHOT_PLAYERSTATE_EXIT_VA;
    rel = (DWORD)(DWORD_PTR)SnapshotPlayerstateRunGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("snapshot guard: playerstate jump did not install");
        return FALSE;
    }
    return TRUE;
}

static BOOL InstallSnapshotEntityRunGuard(void)
{
    static const BYTE expected[6] = { 0x8b, 0x45, 0xf8, 0x40, 0x3b, 0xc6 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)SNAPSHOT_ENTITY_RUN_VA;
    BYTE *trampoline;
    BYTE jump[6] = { 0xe9, 0, 0, 0, 0, 0x90 };
    DWORD rel, old;

    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    trampoline = (BYTE *)VirtualAlloc(NULL, 11, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!trampoline) return FALSE;
    memcpy(trampoline, expected, sizeof(expected));
    trampoline[6] = 0xe9;
    rel = (DWORD)(DWORD_PTR)(entry + 6) - (DWORD)(DWORD_PTR)(trampoline + 11);
    memcpy(trampoline + 7, &rel, 4);
    if (!VirtualProtect(trampoline, 11, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, 11);
    memcpy(&g_snapshotEntityOriginal, &trampoline, sizeof(trampoline));
    g_snapshotEntityExit = (void *)(DWORD_PTR)SNAPSHOT_ENTITY_EXIT_VA;
    rel = (DWORD)(DWORD_PTR)SnapshotEntityRunGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("snapshot guard: entity-run jump did not install");
        return FALSE;
    }
    return TRUE;
}

static BOOL InstallSnapshotGenericRunGuard(void)
{
    static const BYTE expected[6] = { 0x03, 0xf0, 0x40, 0x83, 0xc4, 0x04 };
    BYTE *const entry = (BYTE *)(DWORD_PTR)SNAPSHOT_GENERIC_RUN_VA;
    BYTE *trampoline;
    BYTE jump[6] = { 0xe9, 0, 0, 0, 0, 0x90 };
    DWORD rel, old;

    if (!Readable(entry, sizeof(expected)) || memcmp(entry, expected, sizeof(expected)))
        return FALSE;
    trampoline = (BYTE *)VirtualAlloc(NULL, 11, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!trampoline) return FALSE;
    memcpy(trampoline, expected, sizeof(expected));
    trampoline[6] = 0xe9;
    rel = (DWORD)(DWORD_PTR)(entry + 6) - (DWORD)(DWORD_PTR)(trampoline + 11);
    memcpy(trampoline + 7, &rel, 4);
    if (!VirtualProtect(trampoline, 11, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, 11);
    memcpy(&g_snapshotGenericOriginal, &trampoline, sizeof(trampoline));
    g_snapshotGenericExit = (void *)(DWORD_PTR)SNAPSHOT_GENERIC_EXIT_VA;
    rel = (DWORD)(DWORD_PTR)SnapshotGenericRunGuard - (DWORD)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    if (memcmp(entry, jump, sizeof(jump))) {
        Log("snapshot guard: shared-run jump did not install");
        return FALSE;
    }
    return TRUE;
}

static BOOL InstallSnapshotBoundsGuards(void)
{
    static const BYTE append[11] = {
        0x55, 0x8b, 0xec, 0x51, 0xf6, 0x86, 0x6c, 0x33, 0, 0, 8
    };
    static const BYTE run[5] = { 0x8d, 0x48, 0x01, 0x3b, 0xcf };
    static const BYTE entity[6] = { 0x8b, 0x45, 0xf8, 0x40, 0x3b, 0xc6 };
    static const BYTE generic[6] = { 0x03, 0xf0, 0x40, 0x83, 0xc4, 0x04 };
    const void *a = (const void *)(DWORD_PTR)SNAPSHOT_APPEND_VA;
    const void *r = (const void *)(DWORD_PTR)SNAPSHOT_PLAYERSTATE_RUN_VA;
    const void *e = (const void *)(DWORD_PTR)SNAPSHOT_ENTITY_RUN_VA;
    const void *g = (const void *)(DWORD_PTR)SNAPSHOT_GENERIC_RUN_VA;
    /* All exact sites must match before any can be changed. */
    if (!Readable(a, sizeof(append)) || memcmp(a, append, sizeof(append)) ||
        !Readable(r, sizeof(run)) || memcmp(r, run, sizeof(run)) ||
        !Readable(e, sizeof(entity)) || memcmp(e, entity, sizeof(entity)) ||
        !Readable(g, sizeof(generic)) || memcmp(g, generic, sizeof(generic))) return FALSE;
    return InstallSnapshotAppendGuard() && InstallSnapshotPlayerstateRunGuard() &&
           InstallSnapshotEntityRunGuard() && InstallSnapshotGenericRunGuard();
}

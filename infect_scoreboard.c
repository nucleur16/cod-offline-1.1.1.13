/* Exact C1 1.1.1.13 scoreboard query correction, shared by EN/ZH loader profiles.
 *
 * The full native b parser caches clientInfo.team in each genuine score row.
 * CG's snapshot team writer does not recalculate that cached field. The SDK
 * builder 5B4A50 takes ECX=gametype, EDX=GFx object and six caller-cleaned args
 * {count,score,score,scoreRows,nameCache,localClient}. Both retained call sites
 * below are byte checked before replacement. Each query now brings only the
 * team field of existing, valid real rows in line with current applied CG
 * clientInfo. No row, name, score, rank, ping or transport event is created.
 * No timing/ACK assumption or persistent slot/gamestate cache is involved.
 */
#define INFECT_SDK_BUILDER_VA 0x005B4A50u
#define INFECT_SDK_CALL_MAIN  0x005AD0EDu
#define INFECT_SDK_CALL_MAP   0x005B4534u
#define INFECT_SCORE_ROWS    19
#define INFECT_SCORE_SIZE    0x40u
#define INFECT_NAME_SIZE     0x48u
#define INFECT_CLIENTINFO    0x15AE24u
#define INFECT_CLIENT_STRIDE 0xB50u

static BOOL InfectedScoreboardCurrentInfo(const BYTE *cg, int slot, const BYTE *nameCache,
                                           DWORD *team)
{
    const BYTE *info, *cached;
    if (slot < 0 || slot >= INFECT_SCORE_ROWS) return FALSE;
    info = cg + INFECT_CLIENTINFO + (DWORD)slot * INFECT_CLIENT_STRIDE;
    cached = nameCache + (DWORD)slot * INFECT_NAME_SIZE;
    if (!Readable(info, 0x50) || !Readable(cached, INFECT_NAME_SIZE) ||
        !*(const DWORD *)info || !*(const DWORD *)cached)
        return FALSE;
    /* A reused slot/renamed client must wait for a genuine b/name-cache update.
     * The SDK cache publisher copies at most 63 bytes then writes its NUL. */
    if (strncmp((const char *)info + 0x0C, (const char *)cached + 4, 63))
        return FALSE;
    *team = *(const DWORD *)(info + 0x4C);
    /* Only active survivor/infected sides are corrected. Unassigned, spectator
     * and other state retains the native row until its ordinary b response. */
    return *team == 1 || *team == 2;
}

__attribute__((used, noinline))
void __cdecl InfectedScoreboardPrepareRows(const char *gametype, int count,
                                          BYTE *scoreRows, const BYTE *nameCache)
{
    BYTE *cg;
    const BYTE *snap;
    DWORD currentCount;
    if (count < 1 || count > INFECT_SCORE_ROWS ||
        !Readable(gametype, 7) || memcmp(gametype, "infect", 7) ||
        !Readable(scoreRows, (SIZE_T)count * INFECT_SCORE_SIZE) ||
        !Readable(nameCache, INFECT_SCORE_ROWS * INFECT_NAME_SIZE))
        return;
    cg = ParityCg();
    if (!cg) return;
    /* Require the active snapshot to belong to this exact CG generation. */
    snap = *(const BYTE *const *)(cg + CG_SNAP);
    if ((snap != cg + CG_SNAPBUF && snap != cg + CG_SNAPBUF + CG_SNAPBUF_SIZE) ||
        !Readable(snap + SNAP_SERVERTIME, 12) ||
        *(const int *)(snap + SNAP_SERVERTIME) <= 0 ||
        *(const DWORD *)(snap + SNAP_NUM_CLIENTS) > INFECT_SCORE_ROWS ||
        !Readable(cg + 0xBEB18, 4))
        return;
    currentCount = *(const DWORD *)(cg + 0xBEB18);
    if (currentCount != (DWORD)count) return;
    /* Both proven callers pass one of these two engine-owned real vectors. */
    if (scoreRows != cg + 0xBEB70 && scoreRows != (BYTE *)(DWORD_PTR)0x02609354u)
        return;
    for (int i = 0; i < count; ++i) {
        BYTE *row = scoreRows + (DWORD)i * INFECT_SCORE_SIZE;
        int slot = *(const int *)row;
        DWORD team;
        if (InfectedScoreboardCurrentInfo(cg, slot, nameCache, &team))
            *(DWORD *)(row + 0x10) = team;
    }
}

/* Preserve every entry register and original caller-cleaned argument. Native
 * arguments remain in place; this hook tail-jumps the unchanged SDK builder.
 * pushal saves EAX,ECX,EDX,EBX,entryESP,EBP,ESI,EDI; the original gametype is
 * +24 and original count/row/name arguments become +36/+48/+52 respectively.
 */
void InfectedScoreboardCallHook(void);
__asm__(
    ".text\n"
    ".globl _InfectedScoreboardCallHook\n"
    "_InfectedScoreboardCallHook:\n"
    "  pushal\n"
    "  movl 52(%esp), %eax\n"  /* original arg5: valid name cache */
    "  pushl %eax\n"
    "  movl 52(%esp), %eax\n"  /* original arg4: real score vector */
    "  pushl %eax\n"
    "  movl 44(%esp), %eax\n"  /* original arg1: count */
    "  pushl %eax\n"
    "  movl 36(%esp), %eax\n"  /* saved ECX: gametype */
    "  pushl %eax\n"
    "  call _InfectedScoreboardPrepareRows\n"
    "  addl $16, %esp\n"
    "  popal\n"
    "  pushl $0x005B4A50\n"
    "  ret\n"
);

static BOOL InfectedScoreboardPatchCall(DWORD address)
{
    BYTE *call = (BYTE *)(DWORD_PTR)address;
    LONG before = (LONG)(INFECT_SDK_BUILDER_VA - (address + 5));
    LONG after = (LONG)((DWORD_PTR)InfectedScoreboardCallHook - (address + 5));
    if (!Readable(call, 5) || call[0] != 0xE8 || memcmp(call + 1, &before, 4)) {
        Log("infect scoreboard: exact SDK call bytes differ at 0x%08lX; retained native behavior", (unsigned long)address);
        return FALSE;
    }
    WriteCode(call + 1, &after, 4);
    return TRUE;
}

static void PatchInfectedScoreboard(void)
{
    int count = 0;
    count += InfectedScoreboardPatchCall(INFECT_SDK_CALL_MAIN);
    count += InfectedScoreboardPatchCall(INFECT_SDK_CALL_MAP);
    Log("infect scoreboard: current applied clientInfo teams on %d/2 exact native SDK query calls", count);
}

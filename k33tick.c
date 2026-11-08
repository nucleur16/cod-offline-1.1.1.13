/*
 * k33tick.c - one main-thread callback list for the features ported from codol-classic's d3d9.dll (theatre.c,
 * quick_room.c, name_display.c). Included by kernel33.c after discord.c.
 *
 * codol-classic ran these from its Direct3D EndScene hook; Kernel33 has no D3D proxy, so they run where voice.c and
 * discord.c already run: the Com_EventLoop call at 0x595E9D (Com_Frame), once per frame on the main thread, outside
 * any render or Scaleform callback. Commands they issue go through Cbuf_AddText and run in the same frame's
 * Cbuf_Execute.
 *
 * The call is a chain: voice.c points it at VOICE_TickHook, discord.c at DISCORD_TickHook (each then jumps to what
 * the call reached before). K33InstallMainTicks runs after both and adds K33_MainTickHook in front of whichever is
 * there; it refuses an unknown target, so it never hides a hook it does not know.
 *
 * K33OnMainTick(fn) registers a callback (at most K33_MAX_TICKS, before InstallMainTicks or later: the list is only
 * read on the main thread and only appended before the game runs).
 */

#define K33_MAX_TICKS 8

typedef void (*K33TickFn)(void);
static K33TickFn g_k33Ticks[K33_MAX_TICKS];
static int g_k33TickCount;
/* Installed before this dispatcher; its frame hook must remain in the chain. */
void PI_TickHook(void);
#if CODOL_KERNEL_ROOM_CONTEXT && !CODOL_LOCAL_TESTING
/* Production skips the legacy PI layer; retain the verified room/name tick chain. */
void KRC_MainTickHook(void);
#endif
__attribute__((used)) DWORD g_k33TickNext;   /* what the hooked call reached before us */

__attribute__((unused)) static BOOL K33OnMainTick(K33TickFn fn)
{
    if (!fn || g_k33TickCount >= K33_MAX_TICKS) return FALSE;
    for (int i = 0; i < g_k33TickCount; i++)
        if (g_k33Ticks[i] == fn) return TRUE;
    g_k33Ticks[g_k33TickCount++] = fn;
    return TRUE;
}

__attribute__((used)) void __cdecl K33RunMainTicks(void)
{
    for (int i = 0; i < g_k33TickCount; i++) g_k33Ticks[i]();
}

void K33_MainTickHook(void);
__asm__(
    ".text\n"
    ".globl _K33_MainTickHook\n"
    "_K33_MainTickHook:\n"            /* the call at 0x595E9D: the ported ticks, then whatever it called before */
    "    pushal\n"
    "    pushfl\n"
    "    call _K33RunMainTicks\n"
    "    popfl\n"
    "    popal\n"
    "    jmp *_g_k33TickNext\n");

__attribute__((unused)) static BOOL K33HasMainTick(K33TickFn fn)
{
    for (int i = 0; i < g_k33TickCount; i++)
        if (g_k33Ticks[i] == fn) return TRUE;
    return FALSE;
}

static BOOL K33InstallMainTicks(void)
{
    if (!g_k33TickCount) return FALSE;
    const BYTE *call = (const BYTE *)(DWORD_PTR)VOICE_EVENTLOOP_CALL_VA;
    if (!Readable(call, 5) || call[0] != 0xE8) {
        Log("ticks: Com_EventLoop call 0x%08X differs; %d ported feature(s) not ticked", VOICE_EVENTLOOP_CALL_VA,
            g_k33TickCount);
        return FALSE;
    }
    LONG rel;
    memcpy(&rel, call + 1, 4);
    DWORD target = VOICE_EVENTLOOP_CALL_VA + 5 + (DWORD)rel;
    if (target != VOICE_EVENTLOOP_FUNC_VA && target != (DWORD)(DWORD_PTR)VOICE_TickHook &&
#if CODOL_KERNEL_ROOM_CONTEXT && !CODOL_LOCAL_TESTING
        target != (DWORD)(DWORD_PTR)KRC_MainTickHook &&
#endif
        target != (DWORD)(DWORD_PTR)DISCORD_TickHook && target != (DWORD)(DWORD_PTR)PI_TickHook) {
        Log("ticks: Com_EventLoop call 0x%08X reaches unknown 0x%08lX; %d ported feature(s) not ticked",
            VOICE_EVENTLOOP_CALL_VA, (unsigned long)target, g_k33TickCount);
        return FALSE;
    }
    g_k33TickNext = target;
    RedirectCall(VOICE_EVENTLOOP_CALL_VA, (void *)K33_MainTickHook);
    if (!CallsTarget(VOICE_EVENTLOOP_CALL_VA, (DWORD)(DWORD_PTR)K33_MainTickHook)) {
        Log("ticks: main-thread dispatcher readback differs; migration remains unqualified");
        return FALSE;
    }
    Log("ticks: %d ported feature tick(s) on Com_EventLoop call 0x%08X (chained to 0x%08lX)", g_k33TickCount,
        VOICE_EVENTLOOP_CALL_VA, (unsigned long)target);
    return TRUE;
}

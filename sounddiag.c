/* Sound voice diagnostics: logs every sound C1 refuses to start for want of a voice, with the alias and its channel,
 * and what holds the voices while refusals happen, to Kernel33.log. For "gunshots sometimes do not play" on ported
 * maps: it tells a channel at its cap (too many sounds of that channel already) from a full voice pool (every voice
 * busy with sounds of the same or higher channel priority).
 *
 * C1 1.1.1.13 (every site and table reference byte-checked before anything is patched):
 *   - the play paths read the alias' channel ((alias->flags >> 9) & 0x3F, flags at +0x38) and first ask
 *     SND_ChannelHasRoom 0x736C10 (edx = channel, al = 1 when activeCount[ch] + loading streams of ch < cap[ch]):
 *       2D  0x73E1D0 (esi = play params, alias at +0) call at 0x73E1FB, refused -> return -1
 *       3D  0x73E4B0 (esi = play params)              call at 0x73E4DB
 *       stream 0x73EDC0 (ebx = play params)           call at 0x73EDFE (after its voice was taken by 0x737F60)
 *   - then the pool allocators (return the voice index, -1 when none; each re-checks the cap, takes a free voice,
 *     else SND_StealVoice 0x737960, which only takes a voice of a numerically lower channel priority, or at equal
 *     priority a quieter/farther one):
 *       2D  0x737AC0 (edi = channel, [esp+4] = params) called from 0x73E20E, voices 0..7   (count 0x6C3873C)
 *       3D  0x737B30 (edi = channel, [esp+4] = params) called from 0x73E4EE, voices 8..39  (count 0x6C38740)
 *       stream 0x737BB0 ([esp+4] = channel, edi = params) called from 0x737F77 (0x737F60), voices 40..52 (count
 *       0x6C38744; 40..44 are the ambient bed / music, streamed aliases get 45..52)
 *     Those six calls go through the stubs below, which call the original and log when it says no.
 *   - voices: 53 x 0xA8 at 0x6C36474: +0x00 state (1 loading stream, 2 playing), +0x0C entity, +0x10 channel,
 *     +0x14 handle, +0x68 alias, +0x94 byte busy. A voice is free when +0x14, +0x68 and +0x94 are 0 (0x737AE6),
 *     a stream voice when +0x00 and its stream slot (0x6C3900C + 4 * (voice - 40)) are 0 (0x737BE8); SND_StopVoice
 *     0x73DEA0 zeroes +0x94/+0x14/+0x00/+0x68 and decrements the channel's active count.
 *   - channels: 0x6C36470 count, 0x50-byte records at 0x6C34F68: name[64] (0x736C85 compares it inline),
 *     priority +0x40, 3D +0x44, restricted +0x45, cap +0x48, active count +0x4C.
 *
 * Log lines (Kernel33.log, written by a background thread about once a second, never from the sound path):
 *   snd refuse <2d|3d|stream> alias=<name> ch=<channel>(<priority>) t=<ms> why=<cap|pool> <detail>
 *   snd refuse ... repeated N more times in 1 s            (identical refusals are coalesced per second)
 *   snd voices t=<ms> 2d=U/8 3d=U/32 stream=U/13 refusals=N   + one "snd   vNN ..." line per busy voice
 *   snd chans <name>=<active>/<cap>(p<priority>) ...         (channels with a nonzero active count)
 * t is GetTickCount() milliseconds. The snapshot is written each second while a refusal happened in the last 3 s,
 * and at level 2 also every 5 s regardless.
 *
 * Toggle: dvar k33_snddiag 0 off (default) / 1 refusals + snapshots around them / 2 also a snapshot every 5 s.
 * Startup value: "+set k33_snddiag N" on the command line, else CODOL_SNDDIAG=N, else 0; 0 at startup installs nothing.
 * Memory C1 hands over (play params, alias, alias name) is read with ReadProcessMemory, which fails instead of
 * faulting, so a bad pointer only costs the name ("?"). */

#define SDIAG_HASROOM_VA      0x00736C10u
#define SDIAG_ALLOC2D_VA      0x00737AC0u
#define SDIAG_ALLOC3D_VA      0x00737B30u
#define SDIAG_ALLOCSTREAM_VA  0x00737BB0u
#define SDIAG_CAP2D_CALL_VA   0x0073E1FBu
#define SDIAG_CAP3D_CALL_VA   0x0073E4DBu
#define SDIAG_CAPSTR_CALL_VA  0x0073EDFEu
#define SDIAG_AL2D_CALL_VA    0x0073E20Eu
#define SDIAG_AL3D_CALL_VA    0x0073E4EEu
#define SDIAG_ALSTR_CALL_VA   0x00737F77u
#define SDIAG_VOICES_VA       0x06C36474u
#define SDIAG_VOICE_SIZE      0xA8u
#define SDIAG_VOICE_COUNT     53
#define SDIAG_V_STATE         0x00u
#define SDIAG_V_ENT           0x0Cu
#define SDIAG_V_CHANNEL       0x10u
#define SDIAG_V_HANDLE        0x14u
#define SDIAG_V_ALIAS         0x68u
#define SDIAG_V_BUSY          0x94u
#define SDIAG_STREAMSLOT_VA   0x06C3900Cu
#define SDIAG_POOLS_VA        0x06C3873Cu /* int 2D, 3D, stream */
#define SDIAG_CHANNELS_VA     0x06C34F68u
#define SDIAG_CHANNEL_SIZE    0x50u
#define SDIAG_CH_PRIO         0x40u
#define SDIAG_CH_CAP          0x48u
#define SDIAG_CH_COUNT        0x4Cu
#define SDIAG_CHCOUNT_VA      0x06C36470u
#define SDIAG_MAX_CHANNELS    64
#define SDIAG_ALIAS_FLAGS     0x38u
#define SDIAG_DVAR_REG_VA     0x00624420u /* Dvar_RegisterInt(name, value, min, max, flags, desc) */
#define SDIAG_DVAR_COUNT_VA   0x02AB98C4u

enum { SDIAG_2D, SDIAG_3D, SDIAG_STREAM };
static const char *const kSdPool[] = { "2d", "3d", "stream" };
static const int kSdPoolBase[] = { 0, 8, 40 }, kSdPoolMax[] = { 8, 32, 13 };

/* soundaliases/channels.def order, used when the live table's name is unreadable */
static const char *const kSdChannelNames[] = {
    "physics", "ambdist1", "ambdist2", "auto", "auto2", "autodog", "bulletimpact", "bulletwhizby",
    "explosiveimpact", "element", "auto2d", "vehicle", "vehiclelimited", "menu", "body", "body2d", "reload",
    "reload2d", "item", "effects1", "effects2", "weapon", "weapon2d", "nonshock", "voice", "local", "local2",
    "local3", "ambient", "hurt", "player1", "player2", "music", "musicnopause", "mission", "announcer", "shellshock",
};

static int SndVoicesHasRoom(int ok, DWORD params, int channel, int pool);
static BOOL SvStartupOn(void);
static void SvRegisterDvars(void);
static BOOL g_svOn;
static volatile LONG g_svLimitRefused, g_svLimitTook, g_svNearestTook;

typedef struct { int pool, why, channel; DWORD alias, first, count; char line[200]; } SdKey;

static CRITICAL_SECTION g_sdLock;
static int g_sdStartLevel;
static const BYTE *volatile g_sdDvar;
static volatile LONG g_sdDvarTried;
static BOOL g_sdDvarOk;
static DWORD g_sdMainThread;
static volatile DWORD g_sdLastRefuse;
static volatile LONG g_sdRefusals, g_sdDropped;
static char g_sdText[16384];
static int g_sdTextLen;
static SdKey g_sdKeys[64];
static int g_sdKeyCount;

/* the page-walk check: every region the range touches is committed and readable */
static BOOL SdReadable(const void *p, SIZE_T n)
{
    const BYTE *at = p, *end = (const BYTE *)p + n;
    if (end < at) return FALSE;
    do {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(at, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return FALSE;
        at = (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
    } while (at < end);
    return TRUE;
}

/* reads that fail instead of faulting */
static BOOL SdPeek(DWORD va, void *dst, SIZE_T n)
{
    SIZE_T got = 0;
    return va >= 0x10000u && ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(DWORD_PTR)va, dst, n, &got) && got == n;
}

static void SdString(DWORD va, char *dst, int cap)
{
    int len = 0;
    while (len < cap - 1) {
        DWORD at = va + (DWORD)len, chunk = 0x1000u - (at & 0xFFFu);
        if (chunk > (DWORD)(cap - 1 - len)) chunk = (DWORD)(cap - 1 - len);
        if (!SdPeek(at, dst + len, chunk)) break;
        DWORD i = 0;
        while (i < chunk && dst[len + i]) i++;
        len += (int)i;
        if (i < chunk) break;
    }
    dst[len] = 0;
    for (int i = 0; i < len; i++)
        if ((BYTE)dst[i] < 0x20 || (BYTE)dst[i] > 0x7E) dst[i] = '?';
    if (!len) lstrcpynA(dst, "?", cap);
}

static void SdAliasName(DWORD alias, char *dst, int cap)
{
    DWORD name = 0;
    if (alias && SdPeek(alias, &name, 4) && name) SdString(name, dst, cap);
    else lstrcpynA(dst, alias ? "?" : "(none)", cap);
}

static int SdChannelCount(void)
{
    int n = *(volatile const int *)(DWORD_PTR)SDIAG_CHCOUNT_VA;
    return n < 0 ? 0 : n > SDIAG_MAX_CHANNELS ? SDIAG_MAX_CHANNELS : n;
}

static const BYTE *SdChannel(int ch)
{
    return ch >= 0 && ch < SDIAG_MAX_CHANNELS ? (const BYTE *)(DWORD_PTR)(SDIAG_CHANNELS_VA + (DWORD)ch * SDIAG_CHANNEL_SIZE) : NULL;
}

static int SdChannelInt(int ch, DWORD off)
{
    const BYTE *c = SdChannel(ch);
    return c ? *(volatile const int *)(c + off) : 0;
}

static void SdChannelName(int ch, char *dst, int cap)
{
    const BYTE *c = SdChannel(ch);
    int len = 0;
    if (c) {
        while (len < 63 && len < cap - 1 && c[len] > 0x20 && c[len] < 0x7F) { dst[len] = (char)c[len]; len++; }
        if (len < 63 && c[len]) len = 0;
    }
    dst[len] = 0;
    if (!len) {
        if (ch >= 0 && ch < (int)(sizeof(kSdChannelNames) / sizeof(kSdChannelNames[0]))) lstrcpynA(dst, kSdChannelNames[ch], cap);
        else _snprintf(dst, cap, "ch%d", ch), dst[cap - 1] = 0;
    }
}

static const BYTE *SdVoice(int i) { return (const BYTE *)(DWORD_PTR)(SDIAG_VOICES_VA + (DWORD)i * SDIAG_VOICE_SIZE); }

static int SdVoiceInt(int i, DWORD off) { return *(volatile const int *)(SdVoice(i) + off); }

static BOOL SdVoiceUsed(int i)
{
    if (i >= kSdPoolBase[SDIAG_STREAM])
        return SdVoiceInt(i, SDIAG_V_STATE) || *(volatile const int *)(DWORD_PTR)(SDIAG_STREAMSLOT_VA + 4u * (DWORD)(i - 40)) ||
               SdVoiceInt(i, SDIAG_V_ALIAS);
    return SdVoice(i)[SDIAG_V_BUSY] || SdVoiceInt(i, SDIAG_V_HANDLE) || SdVoiceInt(i, SDIAG_V_ALIAS);
}

static int SdPoolSize(int pool)
{
    int n = *(volatile const int *)(DWORD_PTR)(SDIAG_POOLS_VA + 4u * (DWORD)pool);
    return n < 0 ? 0 : n > kSdPoolMax[pool] ? kSdPoolMax[pool] : n;
}

static int SdPoolUsed(int pool)
{
    int used = 0, n = SdPoolSize(pool);
    for (int i = 0; i < n; i++) used += SdVoiceUsed(kSdPoolBase[pool] + i);
    return used;
}

/* what SND_ChannelHasRoom adds up: the channel's active count plus its loading stream voices */
static int SdChannelActive(int ch)
{
    int n = SdChannelInt(ch, SDIAG_CH_COUNT);
    for (int i = 40; i < SDIAG_VOICE_COUNT; i++)
        if (SdVoiceInt(i, SDIAG_V_STATE) == 1 && SdVoiceInt(i, SDIAG_V_CHANNEL) == ch) n++;
    return n;
}

static int SdLevel(void)
{
#if CODOL_DIAGNOSTICS
    const BYTE *d = g_sdDvar;
    int v = d ? *(volatile const int *)(d + 0xC) : g_sdStartLevel;
    return v < 0 ? 0 : v > 2 ? 2 : v;
#else
    return 0;
#endif
}

/* main thread, from the play path: register the dvar once C1 has registered its own */
static void SdRegisterDvar(void)
{
    typedef const BYTE *(__cdecl *RegisterIntFn)(const char *, int, int, int, unsigned, const char *);
    if (!g_sdDvarOk || g_sdDvarTried || GetCurrentThreadId() != g_sdMainThread ||
        *(volatile const int *)(DWORD_PTR)SDIAG_DVAR_COUNT_VA <= 100)
        return;
    g_sdDvarTried = 1;
#if CODOL_DIAGNOSTICS
    const BYTE *d = ((RegisterIntFn)(DWORD_PTR)SDIAG_DVAR_REG_VA)(
        "k33_snddiag", g_sdStartLevel, 0, 2, 0,
        "Kernel33 sound voice diagnostics in Kernel33.log: 0 off, 1 refusals (+ snapshots around them), 2 + snapshot every 5 s");
    if (d && SdReadable(d, 0x10)) g_sdDvar = d;
    Log("snddiag: dvar k33_snddiag %s", g_sdDvar ? "registered" : "could not be registered, startup value kept");
#endif
    SvRegisterDvars();
}

static void SdAppend(const char *line, int len)
{
    if (g_sdTextLen + len <= (int)sizeof(g_sdText)) {
        memcpy(g_sdText + g_sdTextLen, line, len);
        g_sdTextLen += len;
    } else {
        g_sdDropped++;
    }
}

/* any thread: one refusal. why 0 = channel cap, 1 = no voice in the pool. */
static void SdRefused(int pool, int why, DWORD params, int channel)
{
    if (SdLevel() <= 0) return;
    DWORD now = GetTickCount(), alias = 0;
    InterlockedIncrement(&g_sdRefusals);
    g_sdLastRefuse = now ? now : 1;
    if (params) SdPeek(params, &alias, 4);
    if (channel < 0 && alias) {
        DWORD flags = 0;
        if (SdPeek(alias + SDIAG_ALIAS_FLAGS, &flags, 4)) channel = (int)((flags >> 9) & 0x3F);
    }

    EnterCriticalSection(&g_sdLock);
    for (int i = 0; i < g_sdKeyCount; i++) {
        SdKey *k = &g_sdKeys[i];
        if (k->pool == pool && k->why == why && k->channel == channel && k->alias == alias) {
            k->count++;
            LeaveCriticalSection(&g_sdLock);
            return;
        }
    }
    LeaveCriticalSection(&g_sdLock);

    char name[96], ch[64], detail[64], line[200];
    SdAliasName(alias, name, sizeof(name));
    SdChannelName(channel, ch, sizeof(ch));
    if (why == 2)
        _snprintf(detail, sizeof(detail), "why=limit k33_sndAliasLimit");
    else if (why == 0)
        _snprintf(detail, sizeof(detail), "why=cap %s=%d/%d", ch, SdChannelActive(channel), SdChannelInt(channel, SDIAG_CH_CAP));
    else
        _snprintf(detail, sizeof(detail), "why=pool %s=%d/%d", kSdPool[pool], SdPoolUsed(pool), SdPoolSize(pool));
    detail[sizeof(detail) - 1] = 0;
    int len = _snprintf(line, sizeof(line) - 2, "snd refuse %s alias=%s ch=%s(%d) t=%lu %s", kSdPool[pool], name, ch,
                        SdChannelInt(channel, SDIAG_CH_PRIO), (unsigned long)now, detail);
    if (len < 0) len = (int)sizeof(line) - 2;
    line[len++] = '\r';
    line[len++] = '\n';

    EnterCriticalSection(&g_sdLock);
    int i;
    for (i = 0; i < g_sdKeyCount; i++) {
        SdKey *k = &g_sdKeys[i];
        if (k->pool == pool && k->why == why && k->channel == channel && k->alias == alias) { k->count++; break; }
    }
    if (i == g_sdKeyCount) {
        if (g_sdKeyCount < (int)(sizeof(g_sdKeys) / sizeof(g_sdKeys[0]))) {
            SdKey *k = &g_sdKeys[g_sdKeyCount++];
            k->pool = pool;
            k->why = why;
            k->channel = channel;
            k->alias = alias;
            k->first = now;
            k->count = 1;
            lstrcpynA(k->line, line, (int)sizeof(k->line));
            SdAppend(line, len);
        } else {
            g_sdDropped++;
        }
    }
    LeaveCriticalSection(&g_sdLock);
}

/* the stubs' C side */
__attribute__((used)) int __cdecl SndDiagHasRoom(int ok, DWORD params, int channel, int pool)
{
    if (!g_sdDvarTried) SdRegisterDvar();
#if CODOL_DIAGNOSTICS
    LONG before = g_svLimitRefused;
#endif
    ok = SndVoicesHasRoom(ok & 0xFF, params, channel, pool);
#if CODOL_DIAGNOSTICS
    if (!ok) SdRefused(pool, g_svLimitRefused != before ? 2 : 0, params, channel);
#endif
    return ok;
}

#if CODOL_DIAGNOSTICS
__attribute__((used)) void __cdecl SndDiagAllocFailed(DWORD params, int channel, int pool)
{
#if CODOL_DIAGNOSTICS
    if (channel < 0 || channel >= SDIAG_MAX_CHANNELS) channel = -1;
    int full = channel >= 0 && SdChannelActive(channel) >= SdChannelInt(channel, SDIAG_CH_CAP);
    SdRefused(pool, full ? 0 : 1, params, channel);
#endif
}
#endif

void SNDDIAG_HasRoom2D(void);
void SNDDIAG_HasRoom3D(void);
void SNDDIAG_HasRoomStream(void);
#if CODOL_DIAGNOSTICS
void SNDDIAG_Alloc2D(void);
void SNDDIAG_Alloc3D(void);
void SNDDIAG_AllocStream(void);
#endif

/* SND_ChannelHasRoom keeps its register interface (edx = channel in, al out; it clobbers ecx/edx itself, and its
 * callers only test al), the allocators theirs (edi / [esp+4] in, eax out). ebx/esi/edi/ebp survive the C calls. */
__asm__(
    ".text\n"
    ".globl _SNDDIAG_HasRoom2D\n"
    "_SNDDIAG_HasRoom2D:\n"               /* 0x73E1FB: esi = play params */
    "    pushl $0\n"
    "    pushl %esi\n"
    "    jmp 1f\n"
    ".globl _SNDDIAG_HasRoom3D\n"
    "_SNDDIAG_HasRoom3D:\n"               /* 0x73E4DB: esi = play params */
    "    pushl $1\n"
    "    pushl %esi\n"
    "    jmp 1f\n"
    ".globl _SNDDIAG_HasRoomStream\n"
    "_SNDDIAG_HasRoomStream:\n"           /* 0x73EDFE: ebx = play params */
    "    pushl $2\n"
    "    pushl %ebx\n"
    "1:  pushl %edx\n"                    /* channel */
    "    movl $0x736C10, %eax\n"
    "    call *%eax\n"
    "    popl %edx\n"
    "    pushl %eax\n"
    "    pushl 8(%esp)\n"                 /* pool */
    "    pushl %edx\n"                    /* channel */
    "    pushl 12(%esp)\n"                /* params */
    "    pushl %eax\n"                    /* ok (al) */
    "    call _SndDiagHasRoom\n"            /* returns what C1 acts on (al) */
    "    addl $20, %esp\n"
    "    addl $8, %esp\n"
    "    ret\n"
#if CODOL_DIAGNOSTICS
    ".globl _SNDDIAG_Alloc2D\n"
    "_SNDDIAG_Alloc2D:\n"                 /* 0x73E20E: edi = channel, [esp+4] = params */
    "    pushl 4(%esp)\n"
    "    movl $0x737AC0, %eax\n"
    "    call *%eax\n"
    "    addl $4, %esp\n"
    "    testl %eax, %eax\n"
    "    jns 2f\n"
    "    pushl %eax\n"
    "    pushl $0\n"
    "    jmp 3f\n"
    ".globl _SNDDIAG_Alloc3D\n"
    "_SNDDIAG_Alloc3D:\n"                 /* 0x73E4EE: edi = channel, [esp+4] = params */
    "    pushl 4(%esp)\n"
    "    movl $0x737B30, %eax\n"
    "    call *%eax\n"
    "    addl $4, %esp\n"
    "    testl %eax, %eax\n"
    "    jns 2f\n"
    "    pushl %eax\n"
    "    pushl $1\n"
    "3:  pushl %edi\n"                    /* channel */
    "    pushl 16(%esp)\n"                /* params */
    "    call _SndDiagAllocFailed\n"
    "    addl $12, %esp\n"
    "    popl %eax\n"
    "2:  ret\n"
    ".globl _SNDDIAG_AllocStream\n"
    "_SNDDIAG_AllocStream:\n"             /* 0x737F77: [esp+4] = channel, edi = params */
    "    pushl 4(%esp)\n"
    "    movl $0x737BB0, %eax\n"
    "    call *%eax\n"
    "    addl $4, %esp\n"
    "    testl %eax, %eax\n"
    "    jns 4f\n"
    "    pushl %eax\n"
    "    pushl $2\n"
    "    pushl 12(%esp)\n"                /* channel */
    "    pushl %edi\n"                    /* params */
    "    call _SndDiagAllocFailed\n"
    "    addl $12, %esp\n"
    "    popl %eax\n"
    "4:  ret\n"
#endif
);

static char g_sdOut[32768];
static int g_sdOutLen;

static void SdOut(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void SdOut(const char *fmt, ...)
{
    int room = (int)sizeof(g_sdOut) - g_sdOutLen - 2;
    if (room <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    int len = _vsnprintf(g_sdOut + g_sdOutLen, room, fmt, ap);
    va_end(ap);
    if (len < 0) len = room;
    g_sdOutLen += len;
    g_sdOut[g_sdOutLen++] = '\r';
    g_sdOut[g_sdOutLen++] = '\n';
}

static void SdSnapshot(DWORD now, LONG refusals)
{
    SdOut("snd voices t=%lu 2d=%d/%d 3d=%d/%d stream=%d/%d refusals=%ld", (unsigned long)now, SdPoolUsed(SDIAG_2D),
          SdPoolSize(SDIAG_2D), SdPoolUsed(SDIAG_3D), SdPoolSize(SDIAG_3D), SdPoolUsed(SDIAG_STREAM), SdPoolSize(SDIAG_STREAM),
          (long)refusals);
    for (int pool = 0; pool < 3; pool++) {
        int n = SdPoolSize(pool);
        for (int j = 0; j < n; j++) {
            int i = kSdPoolBase[pool] + j;
            if (!SdVoiceUsed(i)) continue;
            int ch = SdVoiceInt(i, SDIAG_V_CHANNEL);
            char name[96], chName[64];
            SdAliasName((DWORD)SdVoiceInt(i, SDIAG_V_ALIAS), name, sizeof(name));
            SdChannelName(ch, chName, sizeof(chName));
            SdOut("snd   v%02d %s ch=%s(%d) alias=%s ent=%d st=%d", i, kSdPool[pool], chName, SdChannelInt(ch, SDIAG_CH_PRIO),
                  name, SdVoiceInt(i, SDIAG_V_ENT), SdVoiceInt(i, SDIAG_V_STATE));
        }
    }
    char line[1024];
    int len = _snprintf(line, sizeof(line), "snd chans"), count = SdChannelCount();
    for (int ch = 0; ch < count && len > 0 && len < (int)sizeof(line) - 80; ch++) {
        int active = SdChannelActive(ch);
        if (!active) continue;
        char chName[64];
        SdChannelName(ch, chName, sizeof(chName));
        int n = _snprintf(line + len, sizeof(line) - len, " %s=%d/%d(p%d)", chName, active, SdChannelInt(ch, SDIAG_CH_CAP),
                          SdChannelInt(ch, SDIAG_CH_PRIO));
        if (n < 0) break;
        len += n;
    }
    line[sizeof(line) - 1] = 0;
    SdOut("%s", line);
}

static void SdWriteLog(const char *text, int len)
{
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    while (n && path[n - 1] != '\\') n--;
    lstrcpynA(path + n, "Kernel33.log", MAX_PATH - n);
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w;
    WriteFile(h, text, (DWORD)len, &w, NULL);
    CloseHandle(h);
}

/* once a second: refusal lines, repeat counts, snapshot; one append to Kernel33.log */
static DWORD WINAPI SdThread(LPVOID unused)
{
    DWORD lastSnap = 0;
    for (;;) {
        Sleep(1000);
        DWORD now = GetTickCount();
        LONG refusals = InterlockedExchange(&g_sdRefusals, 0), dropped = InterlockedExchange(&g_sdDropped, 0);
        g_sdOutLen = 0;
        EnterCriticalSection(&g_sdLock);
        if (g_sdTextLen) {
            memcpy(g_sdOut, g_sdText, g_sdTextLen);
            g_sdOutLen = g_sdTextLen;
            g_sdTextLen = 0;
        }
        static SdKey repeats[64];
        int nRepeats = 0;
        for (int i = 0; i < g_sdKeyCount; i++)
            if (g_sdKeys[i].count > 1) repeats[nRepeats++] = g_sdKeys[i];
        g_sdKeyCount = 0;
        LeaveCriticalSection(&g_sdLock);

        for (int i = 0; i < nRepeats; i++) {
            int len = (int)strlen(repeats[i].line);
            while (len && (repeats[i].line[len - 1] == '\n' || repeats[i].line[len - 1] == '\r')) repeats[i].line[--len] = 0;
            SdOut("%s repeated %lu more times in 1 s", repeats[i].line, (unsigned long)(repeats[i].count - 1));
        }
        if (dropped) SdOut("snd refuse: %ld more lines dropped (buffer full)", (long)dropped);

        int level = SdLevel();
        DWORD last = g_sdLastRefuse;
        BOOL recent = last && now - last <= 3000;
        static LONG lastLimit, lastTook, lastNear;
        LONG lr = g_svLimitRefused, lt = g_svLimitTook, ln = g_svNearestTook;
        if (level >= 1 && (lr != lastLimit || lt != lastTook || ln != lastNear))
            SdOut("snd policy t=%lu limit_refused=%ld limit_took=%ld nearest_took=%ld (totals)", (unsigned long)now, (long)lr,
                  (long)lt, (long)ln);
        lastLimit = lr; lastTook = lt; lastNear = ln;
        if (level >= 1 && (recent || (level >= 2 && now - lastSnap >= 5000))) {
            SdSnapshot(now, refusals);
            lastSnap = now;
        }
        if (g_sdOutLen) SdWriteLog(g_sdOut, g_sdOutLen);
    }
    return 0;
}

/* "k33_snddiag N" anywhere on the command line (last wins), else CODOL_SNDDIAG=N, else 0. */
static int SdStartLevel(void)
{
    int level = 0;
    char env[16];
    DWORD n = GetEnvironmentVariableA("CODOL_SNDDIAG", env, sizeof(env));
    if (n && n < sizeof(env)) level = atoi(env);
    const char *p = GetCommandLineA();
    while (p && (p = strstr(p, "k33_snddiag")) != NULL) {
        p += 11;
        if (*p != ' ' && *p != '\t' && *p != '"') continue;
        while (*p == ' ' || *p == '\t' || *p == '"') p++;
        if (*p >= '0' && *p <= '9') level = atoi(p);
    }
    return level < 0 ? 0 : level > 2 ? 2 : level;
}

static void InstallSoundDiag(void)
{
    static const struct { DWORD va; BYTE bytes[16]; int n; } kChecks[] = {
        /* the three play paths: channel = (alias->flags >> 9) & 0x3F ; mov edx, edi ; ... */
        { SDIAG_CAP2D_CALL_VA - 14, { 0x8B, 0x7A, 0x38, 0xC1, 0xFF, 0x09, 0x83, 0xE7, 0x3F, 0x8B, 0xD7, 0x89, 0x7D, 0xEC }, 14 },
        { SDIAG_CAP3D_CALL_VA - 14, { 0x8B, 0x7A, 0x38, 0xC1, 0xFF, 0x09, 0x83, 0xE7, 0x3F, 0x8B, 0xD7, 0x89, 0x7D, 0xE8 }, 14 },
        { SDIAG_CAPSTR_CALL_VA - 13, { 0x8B, 0x03, 0x8B, 0x78, 0x38, 0xC1, 0xFF, 0x09, 0x83, 0xE7, 0x3F, 0x8B, 0xD7 }, 13 },
        /* ... ret ; push esi ; call alloc (2D, 3D) and push channel ; call alloc (stream) */
        { SDIAG_AL2D_CALL_VA - 5, { 0x8B, 0xE5, 0x5D, 0xC3, 0x56 }, 5 },
        { SDIAG_AL3D_CALL_VA - 5, { 0x8B, 0xE5, 0x5D, 0xC3, 0x56 }, 5 },
        { SDIAG_ALSTR_CALL_VA - 12, { 0x8B, 0x07, 0x8B, 0x40, 0x38, 0xC1, 0xF8, 0x09, 0x83, 0xE0, 0x3F, 0x50 }, 12 },
        /* SND_ChannelHasRoom: stream voices 40.. state +0 / channel +0x10, active count and cap */
        { SDIAG_HASROOM_VA, { 0x33, 0xC9, 0xB8, 0xC4, 0x7E, 0xC3, 0x06, 0x83, 0x78, 0xF0, 0x01, 0x75, 0x05, 0x3B, 0x10, 0x75 }, 16 },
        { SDIAG_HASROOM_VA + 0x24, { 0x8B, 0x90, 0xB4, 0x4F, 0xC3, 0x06, 0x03, 0xD1, 0x33, 0xC9, 0x3B, 0x90, 0xB0, 0x4F, 0xC3, 0x06 }, 16 },
        /* allocators: register interface, pool counts, first voice of each pool, free test */
        { SDIAG_ALLOC2D_VA, { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0xD7, 0xE8 }, 7 },
        { SDIAG_ALLOC2D_VA + 0x15, { 0x8B, 0x35, 0x3C, 0x87, 0xC3, 0x06, 0x33, 0xC0, 0x85, 0xF6, 0x7E, 0x29, 0xB9, 0x88, 0x64, 0xC3 }, 16 },
        { SDIAG_ALLOC2D_VA + 0x26, { 0x80, 0xB9, 0x80, 0x00, 0x00, 0x00, 0x00, 0x75, 0x10, 0x83, 0x39, 0x00, 0x75, 0x0B, 0x83, 0x79 }, 16 },
        { SDIAG_ALLOC3D_VA, { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0xD7, 0xE8 }, 7 },
        { SDIAG_ALLOC3D_VA + 0x15, { 0x8B, 0x35, 0x40, 0x87, 0xC3, 0x06, 0x33, 0xC9, 0x85, 0xF6, 0x7E, 0x28, 0xB8, 0xC8, 0x69, 0xC3 }, 16 },
        { SDIAG_ALLOCSTREAM_VA, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x1C, 0x53, 0x8B, 0x5D, 0x08, 0x56, 0x8B, 0xD3, 0xE8 }, 14 },
        { SDIAG_ALLOCSTREAM_VA + 0x1F, { 0x8B, 0x35, 0x44, 0x87, 0xC3, 0x06, 0xB8, 0x05, 0x00, 0x00, 0x00, 0x3B, 0xF0, 0x7E, 0x26, 0xBA }, 16 },
        { SDIAG_ALLOCSTREAM_VA + 0x2F, { 0xFC, 0x81, 0xC3, 0x06, 0xB9, 0x20, 0x90, 0xC3, 0x06 }, 9 },
        /* SND_StopVoice: busy +0x94, handle +0x14, state +0, alias +0x68, channel +0x10 */
        { 0x0073DED4u, { 0x88, 0x9E, 0x08, 0x65, 0xC3, 0x06, 0x89, 0x9E, 0x88, 0x64, 0xC3, 0x06, 0x89, 0x9E, 0x74, 0x64 }, 16 },
        { 0x0073DEE4u, { 0xC3, 0x06, 0x89, 0x9E, 0xDC, 0x64, 0xC3, 0x06, 0x8B, 0xB6, 0x84, 0x64, 0xC3, 0x06 }, 14 },
        /* SND_StealVoice: entity +0x0C; channel lookup: count and the inline names */
        { 0x00737A2Cu, { 0x8B, 0x47, 0x84 }, 3 },
        { 0x00736C56u, { 0x83, 0x3D, 0x70, 0x64, 0xC3, 0x06, 0x00 }, 7 },
        { 0x00736C6Du, { 0xBE, 0x68, 0x4F, 0xC3, 0x06 }, 5 },
    };
    static const struct { DWORD site, target; void (*stub)(void); const char *what; } kSites[] = {
        { SDIAG_CAP2D_CALL_VA, SDIAG_HASROOM_VA, SNDDIAG_HasRoom2D, "2d cap" },
        { SDIAG_CAP3D_CALL_VA, SDIAG_HASROOM_VA, SNDDIAG_HasRoom3D, "3d cap" },
        { SDIAG_CAPSTR_CALL_VA, SDIAG_HASROOM_VA, SNDDIAG_HasRoomStream, "stream cap" },
#if CODOL_DIAGNOSTICS
        { SDIAG_AL2D_CALL_VA, SDIAG_ALLOC2D_VA, SNDDIAG_Alloc2D, "2d alloc" },
        { SDIAG_AL3D_CALL_VA, SDIAG_ALLOC3D_VA, SNDDIAG_Alloc3D, "3d alloc" },
        { SDIAG_ALSTR_CALL_VA, SDIAG_ALLOCSTREAM_VA, SNDDIAG_AllocStream, "stream alloc" },
#else
        { SDIAG_AL2D_CALL_VA, SDIAG_ALLOC2D_VA, 0, "2d alloc" },
        { SDIAG_AL3D_CALL_VA, SDIAG_ALLOC3D_VA, 0, "3d alloc" },
        { SDIAG_ALSTR_CALL_VA, SDIAG_ALLOCSTREAM_VA, 0, "stream alloc" },
#endif
    };
    static const BYTE kRegisterInt[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x64, 0xA1, 0x2C, 0x00, 0x00, 0x00 };

#if CODOL_DIAGNOSTICS
    g_sdStartLevel = SdStartLevel();
#else
    g_sdStartLevel = 0;
#endif
    g_svOn = SvStartupOn();
    if (!g_sdStartLevel && !g_svOn) { Log("snddiag: off (k33_snddiag 0, codol_soundvoices 0 at startup), no hooks installed"); return; }
    for (unsigned i = 0; i < sizeof(kChecks) / sizeof(kChecks[0]); i++) {
        const BYTE *p = (const BYTE *)(DWORD_PTR)kChecks[i].va;
        if (!SdReadable(p, kChecks[i].n) || memcmp(p, kChecks[i].bytes, kChecks[i].n) != 0) {
            Log("snddiag: bytes at 0x%08lX do not match, not installed", (unsigned long)kChecks[i].va);
            return;
        }
    }
    for (unsigned i = 0; i < sizeof(kSites) / sizeof(kSites[0]); i++)
        if (!CallsTarget(kSites[i].site, kSites[i].target)) {
            Log("snddiag: %s call at 0x%08lX does not match, not installed", kSites[i].what, (unsigned long)kSites[i].site);
            return;
        }
    if (!SdReadable((void *)(DWORD_PTR)SDIAG_VOICES_VA, SDIAG_VOICE_COUNT * SDIAG_VOICE_SIZE) ||
        !SdReadable((void *)(DWORD_PTR)SDIAG_CHANNELS_VA, SDIAG_MAX_CHANNELS * SDIAG_CHANNEL_SIZE) ||
        !SdReadable((void *)(DWORD_PTR)SDIAG_CHCOUNT_VA, 4) || !SdReadable((void *)(DWORD_PTR)SDIAG_POOLS_VA, 12) ||
        !SdReadable((void *)(DWORD_PTR)SDIAG_STREAMSLOT_VA, 13 * 4)) {
        Log("snddiag: voice/channel tables not readable, not installed");
        return;
    }
    g_sdDvarOk = SdReadable((void *)(DWORD_PTR)SDIAG_DVAR_REG_VA, sizeof(kRegisterInt)) &&
                 !memcmp((void *)(DWORD_PTR)SDIAG_DVAR_REG_VA, kRegisterInt, sizeof(kRegisterInt)) &&
                 SdReadable((void *)(DWORD_PTR)SDIAG_DVAR_COUNT_VA, 4);
#if CODOL_DIAGNOSTICS
    InitializeCriticalSection(&g_sdLock);
#endif
    g_sdMainThread = GetCurrentThreadId();
#if CODOL_DIAGNOSTICS
    HANDLE t = CreateThread(NULL, 0, SdThread, NULL, 0, NULL);
    if (!t) { Log("snddiag: no log thread, not installed"); return; }
    SetThreadPriority(t, THREAD_PRIORITY_BELOW_NORMAL);
    CloseHandle(t);
#endif
    for (unsigned i = 0; i < sizeof(kSites) / sizeof(kSites[0]); i++)
#if !CODOL_DIAGNOSTICS
        if (kSites[i].stub)
#endif
            RedirectCall(kSites[i].site, (void *)kSites[i].stub);
    Log("snddiag: 6 hooks installed (cap checks 0x%08X/0x%08X/0x%08X, allocators 0x%08X/0x%08X/0x%08X), k33_snddiag %d at startup%s",
        SDIAG_CAP2D_CALL_VA, SDIAG_CAP3D_CALL_VA, SDIAG_CAPSTR_CALL_VA, SDIAG_AL2D_CALL_VA, SDIAG_AL3D_CALL_VA,
        SDIAG_ALSTR_CALL_VA, g_sdStartLevel, g_sdDvarOk ? "" : ", no dvar (Dvar_RegisterInt bytes differ)");
}

/* Sound voices: who gets one of C1's 32 3D voices when they are short. Two rules, applied where the 3D play path asks
 * SND_ChannelHasRoom (the call at 0x73E4DB that sounddiag.c already routes through its stub):
 *
 *  1. Alias limit. A one-shot (non-looping) alias on a low-priority channel (priority <= 1: physics, ambdist1/2, auto2,
 *     element, ...) holds at most k33_sndAliasLimit voices (default 4). One more play of it takes the voice of its
 *     farthest copy when it is nearer, else it is not started. C1 1.1.1.13's aliases have no per-alias limit (IW5's
 *     limitCount came later), and the effects of the ported 3.3.3.3 maps rely on one: mp_abandon_sh_mk's five
 *     fireworks launchers play "fireworks_launch" (auto2, priority 1) about 50 times a second; measured 2026-10-19..21
 *     (Kernel33.log snddiag snapshots) it held 19.7 of the 32 3D voices on average, the pool was full in 231 of 263
 *     snapshots, and the map's own loops, emitters and the players' gear rattles were refused for want of a voice.
 *  2. Nearest wins at a channel cap. SND_ChannelHasRoom refuses a sound when its channel already plays its cap
 *     (ambdist1/ambdist2/physics 6, bulletimpact 10), however near it is; the first six emitters to start keep the
 *     channel while nearer ones stay silent (snddiag: amb_wind_gust 3733 cap refusals, emt_water_bubbly_med_lp 2129,
 *     lantern_loop 1669). When the channel is full, the sound takes the voice of the farthest 3D voice on that channel
 *     if that one is farther away; a loop that loses its voice is restarted by C1 when it is again among the nearest
 *     (entity loops are re-requested every frame). C1's own SND_StealVoice 0x737960 already does this between
 *     channels (lower priority first, then farther), but the cap check comes before it.
 *
 * C1 1.1.1.13 (byte-checked by InstallSoundDiag before any hook is placed):
 *   - 3D play params (esi at 0x73E4DB): +0x00 snd_alias_t *, +0x10 origin, +0x1C volume.
 *   - snd_alias_t: +0x00 aliasName, +0x04 (SND_StealVoice keeps a voice whose alias has it set from a sound whose alias
 *     has not), +0x38 flags (bit 0 looping, channel in bits 9..14).
 *   - voices 8..39 are the 3D pool (0x6C36474 + i * 0xA8): +0x0C entity, +0x10 channel, +0x48 volume, +0x68 alias,
 *     +0x88 offset from the entity; SND_StopVoice 0x73DEA0 (cdecl, voice index) frees one and drops its channel count.
 *   - SND_StealVoice's distance: 0x737620 (cdecl entity, out vec3; edi = the voice's offset at +0x88) puts the voice in
 *     the world, 0x736E10 (ecx = origin) returns the squared distance to the nearest listener in st(0); channels that
 *     are not 3D (+0x44 == 0) compare -volume instead (0x737994 / 0x737A49).
 *   - channel records 0x6C34F68 + ch * 0x50: priority +0x40, 3D +0x44, cap +0x48, active count +0x4C.
 *
 * Dvars: k33_sndAliasLimit 0..32 (0 = no limit), k33_sndNearest 0/1. "+set k33_soundvoices 0" on the command line
 * turns both off at startup. Counters are written to Kernel33.log by sounddiag's thread ("snd policy ...") when
 * k33_snddiag is 1 or more. */

#define SV_SCORE_VA      0x00736E10u
#define SV_ORIGIN_VA     0x00737620u
#define SV_STOPVOICE_VA  0x0073DEA0u
#define SV_V_ENT         0x0Cu
#define SV_V_CHANNEL     0x10u
#define SV_V_VOLUME      0x48u
#define SV_V_ALIAS       0x68u
#define SV_V_OFFSET      0x88u
#define SV_P_ORIGIN      0x10u
#define SV_P_VOLUME      0x1Cu
#define SV_CH_3D         0x44u
#define SV_3D_FIRST      8
#define SV_3D_LAST       39
#define SV_LIMIT_MAX_PRIO 1
#define SV_LIMIT_DEFAULT  4

static BOOL g_svOn = TRUE;
static const BYTE *volatile g_svLimitDvar, *volatile g_svNearestDvar;
static volatile LONG g_svLimitRefused, g_svLimitTook, g_svNearestTook;

static int SvLimit(void)
{
    const BYTE *d = g_svLimitDvar;
    int v = d ? *(volatile const int *)(d + 0xC) : SV_LIMIT_DEFAULT;
    return g_svOn ? (v < 0 ? 0 : v) : 0;
}

static BOOL SvNearest(void)
{
    const BYTE *d = g_svNearestDvar;
    return g_svOn && (d ? *(volatile const int *)(d + 0xC) != 0 : TRUE);
}

static BOOL SvStartupOn(void)
{
    const char *p = GetCommandLineA(), *q;
    BOOL on = TRUE;
    while ((q = strstr(p, "k33_soundvoices")) != NULL) {
        q += sizeof("k33_soundvoices") - 1;
        while (*q == ' ' || *q == '"') q++;
        on = *q != '0';
        p = q;
    }
    return on;
}

/* main thread, once C1 has registered its own dvars (called from SdRegisterDvar) */
static void SvRegisterDvars(void)
{
    g_svLimitDvar = C1RegisterInt("k33_sndAliasLimit", SV_LIMIT_DEFAULT, 0, 32, 0,
        "Kernel33: most voices one one-shot ambient/effect sound alias (channel priority <= 1) may hold; 0 = no limit");
    g_svNearestDvar = C1RegisterInt("k33_sndNearest", 1, 0, 1, 0,
        "Kernel33: a 3D sound whose channel is at its cap takes the voice of that channel's farthest sound when nearer");
}

static const BYTE *SvVoice(int i) { return (const BYTE *)(DWORD_PTR)(SDIAG_VOICES_VA + (DWORD)i * SDIAG_VOICE_SIZE); }

static BOOL SvVoiceBusy(int i)
{
    const BYTE *v = SvVoice(i);
    return v[SDIAG_V_BUSY] || *(volatile const int *)(v + SDIAG_V_HANDLE) || *(volatile const int *)(v + SV_V_ALIAS);
}

static float SvScoreAt(const float *origin)
{
    float r;
    DWORD c = (DWORD)(DWORD_PTR)origin;
    __asm__ volatile("movl $0x736E10, %%eax\n\tcall *%%eax" : "=t"(r), "+c"(c) : : "eax", "edx", "memory", "cc");
    return r;
}

/* the voice's squared distance (3D channel) or -volume, as SND_StealVoice scores it */
static float SvVoiceScore(int i, BOOL is3d)
{
    const BYTE *v = SvVoice(i);
    if (!is3d) return -*(volatile const float *)(v + SV_V_VOLUME);
    float origin[3];
    int ent = *(volatile const int *)(v + SV_V_ENT);
    const float *offset = (const float *)(v + SV_V_OFFSET);
    float *out = origin;
    __asm__ volatile("pushl %%esi\n\tpushl %%ebx\n\tmovl $0x737620, %%eax\n\tcall *%%eax\n\taddl $8, %%esp"
                     : "+b"(ent), "+S"(out), "+D"(offset) : : "eax", "ecx", "edx", "memory", "cc");
    return SvScoreAt(origin);
}

static void SvStopVoice(int i)
{
    ((void (__cdecl *)(int))(DWORD_PTR)SV_STOPVOICE_VA)(i);
}

static BOOL SvChannelHasRoom(int channel)
{
    DWORD ok;
    DWORD ch = (DWORD)channel;
    __asm__ volatile("movl $0x736C10, %%eax\n\tcall *%%eax" : "=a"(ok), "+d"(ch) : : "ecx", "memory", "cc");
    return (ok & 0xFF) != 0;
}

/* the 3D play path's channel check (sounddiag's stub, pool 1): ok is SND_ChannelHasRoom's answer; returns the one C1 acts on */
static int SndVoicesHasRoom(int ok, DWORD params, int channel, int pool)
{
    if (pool != 1 || !g_svOn || !params || channel < 0 || channel >= SDIAG_MAX_CHANNELS) return ok;
    const BYTE *alias = *(const BYTE *const *)(DWORD_PTR)params;
    if (!alias) return ok;
    const BYTE *ch = (const BYTE *)(DWORD_PTR)(SDIAG_CHANNELS_VA + (DWORD)channel * SDIAG_CHANNEL_SIZE);
    int prio = *(const int *)(ch + SDIAG_CH_PRIO);
    BOOL is3d = ch[SV_CH_3D] != 0;
    DWORD flags = *(const DWORD *)(alias + SDIAG_ALIAS_FLAGS);
    BOOL tagged = *(const DWORD *)(alias + 4) != 0;
    float score = 0;
    BOOL scored = FALSE;

    int limit = SvLimit();
    if (limit > 0 && prio <= SV_LIMIT_MAX_PRIO && !(flags & 1)) {
        const char *name = *(const char *const *)alias;
        int count = 0, worst = -1;
        float worstScore = 0;
        for (int i = SV_3D_FIRST; i <= SV_3D_LAST; i++) {
            if (!SvVoiceBusy(i)) continue;
            const BYTE *va = *(const BYTE *const *)(SvVoice(i) + SV_V_ALIAS);
            if (!va) continue;
            const char *vn = *(const char *const *)va;
            if (va != alias && (!name || !vn || strcmp(name, vn) != 0)) continue;
            count++;
            float s = SvVoiceScore(i, is3d);
            if (worst < 0 || s > worstScore) { worst = i; worstScore = s; }
        }
        if (count >= limit) {
            if (!scored) { score = is3d ? SvScoreAt((const float *)(DWORD_PTR)(params + SV_P_ORIGIN)) : -*(const float *)(DWORD_PTR)(params + SV_P_VOLUME); scored = TRUE; }
            if (worst < 0 || !(worstScore > score)) {
#if K33_DIAGNOSTICS
                InterlockedIncrement(&g_svLimitRefused);
#endif
                return 0;
            }
            SvStopVoice(worst);
#if K33_DIAGNOSTICS
            InterlockedIncrement(&g_svLimitTook);
#endif
            ok = SvChannelHasRoom(channel);
        }
    }

    if (!ok && SvNearest()) {
        int worst = -1;
        float worstScore = 0;
        for (int i = SV_3D_FIRST; i <= SV_3D_LAST; i++) {
            if (!SvVoiceBusy(i) || *(volatile const int *)(SvVoice(i) + SV_V_CHANNEL) != channel) continue;
            const BYTE *va = *(const BYTE *const *)(SvVoice(i) + SV_V_ALIAS);
            if (!va || (!tagged && *(const DWORD *)(va + 4))) continue;
            float s = SvVoiceScore(i, is3d);
            if (worst < 0 || s > worstScore) { worst = i; worstScore = s; }
        }
        if (worst >= 0) {
            if (!scored) { score = is3d ? SvScoreAt((const float *)(DWORD_PTR)(params + SV_P_ORIGIN)) : -*(const float *)(DWORD_PTR)(params + SV_P_VOLUME); scored = TRUE; }
            if (worstScore > score) {
                SvStopVoice(worst);
#if K33_DIAGNOSTICS
                InterlockedIncrement(&g_svNearestTook);
#endif
                ok = SvChannelHasRoom(channel);
            }
        }
    }
    return ok;
}


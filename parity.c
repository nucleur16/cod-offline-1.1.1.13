/*
 * Parity logger (parity-log branch). Included by kernel33.c, so it shares Log/Readable/WriteCode
 * and the call-site helpers; it adds nothing to the release DLL on main.
 *
 * Purpose: whenever the client disagrees with what the server sent, or cannot resolve something
 * the server references, say so in the game's own console log (main\console_mp-00N.log), next to
 * the game's own warnings, so the server can be brought to retail parity. Every line reads
 *     ^3PARITY t=<cg.time> st=<cg.snap->serverTime> msg=<that snapshot's messageNum> <category>: <details>
 * t/st/msg are -1 when there is no cgame (menus, loading).
 *
 * Output goes through C1's Com_Printf (0x5941B0: vsnprintf into 0x2020 bytes, then
 * Com_PrintMessage 0x594120, which locks the console (0x2B62AB0) and appends to the log file in
 * 0x594010). Channel 0x14 is the one C1's own cgame warnings use (CalcMuzzlePoint 0x4826AA,
 * CG_PredictPlayerState 0x45F682); every channel but 9 reaches the log file.
 * Lines from a thread other than the main one, and every missing-asset line (those come from
 * inside the DB lock), are queued and printed by the next main-thread hook.
 *
 * Rate limit: each finding has a key (category + what differs). The first 5 of a key are logged
 * (50 at parity_log 2), then every 100th with "[xN]" so the count still shows. Missing assets,
 * menus and local variables are logged once per name.
 *
 * Toggle: dvar "parity_log" 0 off / 1 normal (default) / 2 verbose, registered as an int with the
 * archive flag on the first main-thread hook (C1 refuses console sets of non-archive dvars, see
 * PatchDvarRegistrations). Its startup value comes from "+set parity_log N" on the command line
 * or the environment variable CODOL_PARITY_LOG=N (command line wins). A startup value of 0
 * installs nothing at all. Verbose lowers the prediction thresholds and raises the per-key limit.
 *
 * Every hook redirects one E8 call whose 6 preceding bytes and target are checked first; a site
 * that does not match is skipped and named in Kernel33.log. Pre-hooks save every register
 * (pushal), call the logger, restore them and jump to the original with the original stack;
 * post-hooks call the original with the same arguments and registers and return its result
 * untouched. Nothing a hook does changes game state.
 */

#define PARITY_COM_PRINTF_VA   0x005941B0u
#define PARITY_CHANNEL         0x14
#define DVAR_REGISTER_INT_VA   0x00624420u

/* Encoded global pointers: [0x16D3B58 + [index]*4] ^ key (docs/CODOL_C1_VIRTUALIZED_20260928.md). */
#define ENC_PTR_TABLE_VA       0x016D3B58u
#define CG_INDEX_VA            0x06C3D2ACu
#define CG_KEY                 0x490B400Cu

/* cg_t: the predicted playerState is at cg+0 (0x3368 bytes, CG_PredictPlayerState 0x45F3A0 runs
 * Pmove on it), snap/nextSnap at +0x35E0/+0x35E4 (CG_ProcessSnapshots 0x46DA20), cg.time at
 * +0xB0350 (0x43A834), clientinfo[64] at +0x15AE24 stride 0xB50. */
#define CG_SNAP                0x35E0
#define CG_TIME                0xB0350

/* snapshot_t (CL_GetSnapshot 0x486F50 fills it): ps at 0, snapFlags +0x3368, serverTime +0x3370,
 * numEntities +0x3374 (<= 0x400), entities +0xE404 stride 0x114 (number +0, eType +4, weapon
 * +0xD4), numClients +0x3378 (<= 0x13), clientStates +0x53404 stride 0x138 (clientNum +0,
 * world-gun entities +0x12C/+0x130, copied to clientinfo +0xAF4/+0xAF8 at 0x46C979/0x46C982). */
#define SNAP_SERVERTIME        0x3370
#define SNAP_NUM_ENTITIES      0x3374
#define SNAP_NUM_CLIENTS       0x3378
#define SNAP_ENTITIES          0xE404
#define SNAP_CLIENTS           0x53404
#define ES_SIZE                0x114
#define CS_SIZE                0x138

/* playerState (C1 offsets: src/codol/snapshot/codol_player_fields.inc in the server repo,
 * docs/CODOL_C1_MOVEMENT_LAYOUT.md, src/codol/weapon/codol_weapon_state.h). */
#define PS_COMMANDTIME         0x000
#define PS_VELOCITY            0x028
#define PS_GROUNDENT           0x0A8
#define PS_EVENTSEQ            0x0F0
#define PS_EVENTS              0x0F4
#define PS_EVENTPARMS          0x104
#define PS_CLIENTNUM           0x140
#define PS_LINKWEAPONENT       0x1EC
#define PS_WEAPON              0x33C

#define MAX_GENTITIES          2048
#define ENTITYNUM_WORLD        2046
#define ENTITYNUM_NONE         2047
#define EVENT_NAMES_VA         0x00CD81D8u /* EV_NONE .. EV_DTP_LAND, NULL-terminated */
#define EVENT_COUNT            187
#define ET_EVENTS              0x15        /* eType = ET_EVENTS + event (0x441E13) */
#define ET_PLAYER              1
#define ET_MISSILE             4
#define NUM_WEAPONS_VA         0x06C3D28Cu /* CG_Missile clamps es weapon >= this to 0 */

#define ASSET_TYPE_NAMES_VA    0x00CD55B0u /* physpreset .. weaponattach */
#define ASSET_TYPE_COUNT       44
#define CS_OFFSETS_VA          0x06D0D130u /* gameState.stringOffsets[] */
#define CS_DATA_VA             0x06D1370Cu /* gameState.stringData */
#define CS_COUNT               ((CS_DATA_VA - CS_OFFSETS_VA) / 4)
#define CS_DATA_LIMIT          0x40000u

static int g_parityStartLevel = 1;
static const BYTE *volatile g_parityDvar;
static DWORD g_mainThread;
static CRITICAL_SECTION g_parityLock;
static int g_parityHooks, g_paritySkipped;
static BOOL g_parityBanner, g_parityDvarTried, g_csReadable;
static volatile LONG g_parityEmitting;

static int ParityLevel(void)
{
    const BYTE *d = g_parityDvar;
    int v = d ? *(volatile const int *)(d + 0xC) : g_parityStartLevel;
    return v < 0 ? 0 : v > 2 ? 2 : v;
}

/* ---- rate limiting ---------------------------------------------------------------------- */

#define PARITY_KEYS 4096
typedef struct { DWORD key, count; } ParityKey;
static ParityKey g_parityKeys[PARITY_KEYS];

static DWORD ParityHash(DWORD h, const char *s)
{
    while (*s) { h ^= (BYTE)*s++; h *= 16777619u; }
    return h ? h : 1;
}

/* How many times this key has been reported, this one included. A full table counts every
 * further new key as a first sighting, which only means a few extra lines. Caller holds the lock. */
static DWORD ParityBump(DWORD key)
{
    for (DWORD i = 0; i < 64; i++) {
        ParityKey *k = &g_parityKeys[(key + i) & (PARITY_KEYS - 1)];
        if (k->key == key) return ++k->count;
        if (!k->key) { k->key = key; k->count = 1; return 1; }
    }
    return 1;
}

/* ---- output ----------------------------------------------------------------------------- */

#define PARITY_LINE 400
#define PARITY_QUEUE 256
static char g_parityQueue[PARITY_QUEUE][PARITY_LINE];
static int g_parityQueueHead, g_parityQueueCount;
static DWORD g_parityQueueDropped;

static void ParityPrint(const char *line)
{
    if (InterlockedExchange(&g_parityEmitting, 1)) return; /* never re-enter Com_Printf from a hook */
    ((void(__cdecl *)(int, const char *, ...))(DWORD_PTR)PARITY_COM_PRINTF_VA)(PARITY_CHANNEL, "%s", line);
    InterlockedExchange(&g_parityEmitting, 0);
}

static void ParityQueue(const char *line)
{
    EnterCriticalSection(&g_parityLock);
    if (g_parityQueueCount == PARITY_QUEUE) {
        g_parityQueueDropped++;
    } else {
        lstrcpynA(g_parityQueue[(g_parityQueueHead + g_parityQueueCount) % PARITY_QUEUE], line, PARITY_LINE);
        g_parityQueueCount++;
    }
    LeaveCriticalSection(&g_parityLock);
}

static void ParityFlush(void)
{
    char line[PARITY_LINE];
    for (;;) {
        DWORD dropped = 0;
        EnterCriticalSection(&g_parityLock);
        if (!g_parityQueueCount && !g_parityQueueDropped) { LeaveCriticalSection(&g_parityLock); return; }
        if (g_parityQueueCount) {
            lstrcpynA(line, g_parityQueue[g_parityQueueHead], PARITY_LINE);
            g_parityQueueHead = (g_parityQueueHead + 1) % PARITY_QUEUE;
            g_parityQueueCount--;
        } else {
            dropped = g_parityQueueDropped;
            g_parityQueueDropped = 0;
        }
        LeaveCriticalSection(&g_parityLock);
        if (dropped) {
            _snprintf(line, sizeof(line), "^3PARITY queue: %lu line(s) dropped while the main thread was busy\n",
                      (unsigned long)dropped);
            line[sizeof(line) - 1] = 0;
        }
        ParityPrint(line);
    }
}

static void ParityRegisterDvar(void)
{
    typedef const BYTE *(__cdecl *RegisterIntFn)(const char *, int, int, int, unsigned, const char *);
    g_parityDvarTried = TRUE;
    const BYTE *d = ((RegisterIntFn)(DWORD_PTR)DVAR_REGISTER_INT_VA)(
        "parity_log", g_parityStartLevel, 0, 2, 0x1 /* archive */,
        "Kernel33 parity logger: 0 off, 1 normal, 2 verbose");
    if (d && Readable(d, 0x10)) g_parityDvar = d;
    Log("parity: dvar parity_log %s", g_parityDvar ? "registered" : "could not be registered, startup value kept");
}

/* Main-thread housekeeping, run by every main-thread hook: the banner, the dvar and the queue. */
static void ParityTick(void)
{
    if (GetCurrentThreadId() != g_mainThread) return;
    if (!g_parityDvarTried) ParityRegisterDvar();
    if (!g_parityBanner) {
        char line[160];
        g_parityBanner = TRUE;
        _snprintf(line, sizeof(line), "^3PARITY logger active: %d hooks installed, %d skipped (parity_log %d)\n",
                  g_parityHooks, g_paritySkipped, ParityLevel());
        line[sizeof(line) - 1] = 0;
        ParityPrint(line);
    }
    if (g_parityQueueCount || g_parityQueueDropped) ParityFlush();
}

static BYTE *ParityCg(void)
{
    DWORD idx = *(volatile DWORD *)(DWORD_PTR)CG_INDEX_VA;
    if (idx > 3) return NULL;
    DWORD enc = *(volatile DWORD *)(DWORD_PTR)(ENC_PTR_TABLE_VA + idx * 4);
    if (!enc) return NULL;
    BYTE *cg = (BYTE *)(DWORD_PTR)(enc ^ CG_KEY);
    return Readable(cg, CG_SNAP + 8) && Readable(cg + CG_TIME, 4) ? cg : NULL;
}

/* messageNum of the snapshot in each of cg's two snapshot buffers (CG_ReadNextSnapshot 0x46D9F5
 * fills cg+0x35E8 / cg+0x35E8+0x556B0 with CL_GetSnapshot(num)); snapshot_t has no messageNum. */
static const BYTE *volatile g_snapBuf[2];
static volatile int g_snapNum[2];

static int ParitySnapNum(const BYTE *snap)
{
    for (int i = 0; i < 2; i++)
        if (snap && g_snapBuf[i] == snap) return g_snapNum[i];
    return -1;
}

#define PR_ONCE 0x1 /* log a key once, with no repeat count */
#define PR_QUEUE 0x2 /* always queue (caller may hold a game lock) */

static void ParityReport(unsigned flags, const char *cat, const char *key, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

static void ParityReport(unsigned flags, const char *cat, const char *key, const char *fmt, ...)
{
    int level = ParityLevel();
    if (!level) return;
    DWORD hash = ParityHash(ParityHash(2166136261u, cat), key);
    EnterCriticalSection(&g_parityLock);
    DWORD n = ParityBump(hash);
    LeaveCriticalSection(&g_parityLock);
    DWORD limit = (flags & PR_ONCE) ? 1 : level >= 2 ? 50 : 5;
    if (n > limit && ((flags & PR_ONCE) || n % 100)) return;

    int t = -1, st = -1, msg = -1;
    BYTE *cg = ParityCg();
    if (cg) {
        const BYTE *snap = *(const BYTE *const *)(cg + CG_SNAP);
        t = *(const int *)(cg + CG_TIME);
        if (snap && Readable(snap + SNAP_SERVERTIME, 4)) {
            st = *(const int *)(snap + SNAP_SERVERTIME);
            msg = ParitySnapNum(snap);
        }
    }
    char body[PARITY_LINE - 64], line[PARITY_LINE];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    body[sizeof(body) - 1] = 0;
    if (n > limit)
        _snprintf(line, sizeof(line), "^3PARITY t=%d st=%d msg=%d %s: %s [x%lu]\n", t, st, msg, cat, body,
                  (unsigned long)n);
    else
        _snprintf(line, sizeof(line), "^3PARITY t=%d st=%d msg=%d %s: %s\n", t, st, msg, cat, body);
    line[sizeof(line) - 2] = '\n';
    line[sizeof(line) - 1] = 0;

    if (!(flags & PR_QUEUE) && GetCurrentThreadId() == g_mainThread) {
        if (g_parityQueueCount || g_parityQueueDropped) ParityFlush();
        ParityPrint(line);
    } else {
        ParityQueue(line);
    }
}

static const char *ParityEventName(int ev)
{
    if (ev < 0 || ev >= EVENT_COUNT) return "?";
    const char *name = *(const char *const *)(DWORD_PTR)(EVENT_NAMES_VA + ev * 4);
    return name ? name : "?";
}

/* ---- 1. event replays: CG_CheckPlayerstateEvents ----------------------------------------- */
/* CG_TransitionPlayerState 0x45D780 (called from CG_DrawActiveFrame 0x47895E with ecx = cg, i.e.
 * the predicted playerState, and eax = cg+0x1740D0, the transient copy 0x45F000 saved from the
 * previous frame's prediction) calls CG_CheckPlayerstateEvents 0x45D680 at 0x45D7DE with
 * (localClient, ops) and esi = ps. For i in [seq-4, seq): a slot at or past the old sequence is a
 * new event; a slot below it whose event differs from ops->events[i&3] is fired AGAIN (0x45D6E0..
 * 0x45D708 -> CG_EntityEvent). That second case is the predicted-vs-server disagreement behind
 * double shots and doubled footsteps: the client predicted one event, the server sent another
 * under the same sequence number. ops = {eFlags +0, weapon +4, eventSequence +8, events[4] +0xC,
 * unpredictable sequence +0x1C}; it has no parms, so the previous frame's parms come from the
 * copy ParitySaveOld takes at the same moment 0x45F000 builds ops. */
#define EVENTS_CALL_VA   0x0045D7DEu
#define EVENTS_FUNC_VA   0x0045D680u
#define SAVEOLD_CALL_VA  0x004787A3u
#define SAVEOLD_FUNC_VA  0x0045F000u

typedef struct {
    BOOL valid;
    int commandTime, eventSeq;
    int events[4], parms[4];
    DWORD fields[64];
    float velocity[3];
} ParityPrev;
static ParityPrev g_prev;

__attribute__((used)) void __cdecl ParityEvents(int localClient, const BYTE *ops, const BYTE *ps)
{
    if (!ParityLevel()) return;
    int seq = *(const int *)(ps + PS_EVENTSEQ);
    int old = *(const int *)(ops + 8);
    if (old > seq + 0x200) old -= 0x800; /* as 0x45D6AC..0x45D6B0 */
    const int *ev = (const int *)(ps + PS_EVENTS), *parm = (const int *)(ps + PS_EVENTPARMS);
    const int *oldEv = (const int *)(ops + 0xC);
    char key[64];

    if (seq - old > 4) {
        ParityReport(0, "events", "overflow", "%d new playerState events in one frame (seq %d -> %d), "
                     "only the newest 4 slots exist; %d never fired", seq - old, old, seq, seq - old - 4);
    }
    for (int i = seq - 4; i < seq; i++) {
        int slot = i & 3;
        if (i >= old) {
            if ((unsigned)ev[slot] >= EVENT_COUNT) {
                _snprintf(key, sizeof(key), "unknown:%d", ev[slot]);
                ParityReport(0, "events", key, "playerState event %d is not a C1 event (seq %d parm %d)",
                             ev[slot], i, parm[slot]);
            }
            continue;
        }
        if (old - i >= 4 || ev[slot] == oldEv[slot]) continue;
        BOOL haveOldParm = g_prev.valid && g_prev.eventSeq == *(const int *)(ops + 8) &&
                           g_prev.events[slot] == oldEv[slot];
        char oldParm[16];
        if (haveOldParm) _snprintf(oldParm, sizeof(oldParm), "%d", g_prev.parms[slot]);
        else lstrcpyA(oldParm, "?");
        _snprintf(key, sizeof(key), "replay:%d>%d", oldEv[slot], ev[slot]);
        ParityReport(0, "event-replay", key,
                     "seq %d slot %d changed under the same sequence, so C1 fires it again: "
                     "predicted %s(%d) parm %s -> snapshot %s(%d) parm %d",
                     i, slot, ParityEventName(oldEv[slot]), oldEv[slot], oldParm,
                     ParityEventName(ev[slot]), ev[slot], parm[slot]);
    }
}

/* ---- 2. prediction errors ----------------------------------------------------------------- */
/* CG_DrawActiveFrame calls 0x45F000 at 0x4787A3, which copies the predicted playerState's
 * commandTime/origin/... into cg+0x1740B4 (C1's "oldPlayerState") before the new snapshots are
 * processed. ParitySaveOld takes its own copy of the fields below at that call. Then
 * CG_PredictPlayerState 0x45F3A0 rebuilds the prediction from the snapshot and, when the replay
 * reaches the old commandTime (0x45F5CD), measures the origin error with VectorLength 0x402490
 * at 0x45F64D (>0.1 prints "Prediction Error" under cg_showmiss). ParityPredCheck runs right after
 * that VectorLength (edi = cg, the playerState as rebuilt from the snapshot at the same
 * commandTime) and compares every field, so any field the server and C1's Pmove disagree on is
 * named, not only the origin. */
#define PRED_CALL_VA     0x0045F64Du
#define VECLEN_FUNC_VA   0x00402490u

enum { PF_INT, PF_HEX, PF_BYTE };
typedef struct { WORD off; BYTE kind; const char *name; } ParityField;

static const ParityField kPredFields[] = {
    { 0x004, PF_INT, "pm_type" },          { 0x008, PF_INT, "pm_time" },
    { 0x00C, PF_HEX, "pm_flags" },         { 0x010, PF_HEX, "otherFlags" },
    { 0x014, PF_HEX, "ps+0x14" },          { 0x018, PF_BYTE, "bobCycle" },
    { 0x0A8, PF_INT, "groundEntityNum" },  { 0x0B8, PF_INT, "jumpTime" },
    { 0x0E8, PF_BYTE, "movementDir" },     { 0x0EC, PF_HEX, "stanceAnimFlags" },
    { 0x158, PF_INT, "viewHeightTarget" }, { 0x160, PF_INT, "viewHeightLerpTime" },
    { 0x1EC, PF_INT, "linkWeaponEnt" },    { 0x234, PF_INT, "diveEndTime" },
    { 0x238, PF_INT, "sprintButtonUpRequired" }, { 0x23C, PF_INT, "sprintDelay" },
    { 0x240, PF_INT, "lastSprintStart" },  { 0x244, PF_INT, "lastSprintEnd" },
    { 0x25C, PF_INT, "mantleTime" },       { 0x260, PF_INT, "mantleTransition" },
    { 0x264, PF_HEX, "mantleFlags" },
    /* hand 0 and hand 1 (akimbo), ps + hand*0x1C (PM_Weapon_Hand 0x41ACE0) */
    { 0x268, PF_HEX, "weapAnim[0]" },      { 0x26C, PF_INT, "weaponTime[0]" },
    { 0x270, PF_INT, "hand0+0x8" },        { 0x274, PF_INT, "weaponTimer2[0]" },
    { 0x278, PF_INT, "weaponstate[0]" },   { 0x27C, PF_HEX, "handFlags[0]" },
    { 0x284, PF_HEX, "weapAnim[1]" },      { 0x288, PF_INT, "weaponTime[1]" },
    { 0x28C, PF_INT, "hand1+0x8" },        { 0x290, PF_INT, "weaponTimer2[1]" },
    { 0x294, PF_INT, "weaponstate[1]" },   { 0x298, PF_HEX, "handFlags[1]" },
    { 0x328, PF_INT, "offhandWeapon" },    { PS_WEAPON, PF_INT, "weapon" },
    { 0x340, PF_INT, "primaryWeapon" },    { 0x344, PF_HEX, "weapFlags" },
    { 0x35C, PF_INT, "dualWield" },
    { 0x4B0, PF_HEX, "perks[0]" }, { 0x4B4, PF_HEX, "perks[1]" }, { 0x4B8, PF_HEX, "perks[2]" },
    { 0x4BC, PF_HEX, "perks[3]" }, { 0x4C0, PF_HEX, "perks[4]" }, { 0x4C4, PF_HEX, "perks[5]" },
    { 0x4C8, PF_HEX, "perks[6]" }, { 0x4CC, PF_HEX, "perks[7]" },
};
#define PRED_FIELD_COUNT (sizeof(kPredFields) / sizeof(kPredFields[0]))

static DWORD ParityFieldValue(const BYTE *ps, const ParityField *f)
{
    DWORD v = *(const DWORD *)(ps + f->off);
    return f->kind == PF_BYTE ? (v & 0xFF) : v;
}

__attribute__((used)) void __cdecl ParitySaveOld(void)
{
    ParityTick();
    BYTE *cg = ParityCg();
    if (!cg || !ParityLevel()) { g_prev.valid = FALSE; return; }
    const BYTE *ps = cg; /* the predicted playerState is cg+0 */
    g_prev.commandTime = *(const int *)(ps + PS_COMMANDTIME);
    g_prev.eventSeq = *(const int *)(ps + PS_EVENTSEQ);
    memcpy(g_prev.events, ps + PS_EVENTS, sizeof(g_prev.events));
    memcpy(g_prev.parms, ps + PS_EVENTPARMS, sizeof(g_prev.parms));
    memcpy(g_prev.velocity, ps + PS_VELOCITY, sizeof(g_prev.velocity));
    for (unsigned i = 0; i < PRED_FIELD_COUNT; i++) g_prev.fields[i] = ParityFieldValue(ps, &kPredFields[i]);
    g_prev.valid = TRUE;
}

__attribute__((used)) void __cdecl ParityPredCheck(const float *delta, const BYTE *cg, DWORD lenBits)
{
    int level = ParityLevel();
    if (!level || !g_prev.valid || *(const int *)(cg + PS_COMMANDTIME) != g_prev.commandTime) return;
    float len;
    memcpy(&len, &lenBits, sizeof(len));
    int cmdTime = g_prev.commandTime;

    if (len > (level >= 2 ? 0.01f : 0.1f))
        ParityReport(0, "pred", "origin", "origin error %.3f (client-predicted minus from-snapshot %.3f %.3f %.3f) "
                     "at commandTime %d", len, delta[0], delta[1], delta[2], cmdTime);

    const float *v = (const float *)(cg + PS_VELOCITY);
    float dv = 0;
    for (int i = 0; i < 3; i++) {
        float d = v[i] - g_prev.velocity[i];
        if (d < 0) d = -d;
        if (d > dv) dv = d;
    }
    if (dv > (level >= 2 ? 0.01f : 1.0f))
        ParityReport(0, "pred", "velocity", "velocity client (%.2f %.2f %.2f) snapshot (%.2f %.2f %.2f), max diff %.2f "
                     "at commandTime %d", g_prev.velocity[0], g_prev.velocity[1], g_prev.velocity[2],
                     v[0], v[1], v[2], dv, cmdTime);

    for (unsigned i = 0; i < PRED_FIELD_COUNT; i++) {
        const ParityField *f = &kPredFields[i];
        DWORD now = ParityFieldValue(cg, f), was = g_prev.fields[i];
        if (now == was) continue;
        if (f->kind == PF_HEX)
            ParityReport(0, "pred", f->name, "%s (ps+0x%X) client 0x%lX snapshot 0x%lX (diff bits 0x%lX) at commandTime %d",
                         f->name, f->off, (unsigned long)was, (unsigned long)now, (unsigned long)(was ^ now), cmdTime);
        else
            ParityReport(0, "pred", f->name, "%s (ps+0x%X) client %ld snapshot %ld (diff %ld) at commandTime %d",
                         f->name, f->off, (long)was, (long)now, (long)now - (long)was, cmdTime);
    }
}

/* ---- 4a. entity events: unknown event numbers --------------------------------------------- */
/* CG_EntityEvent 0x8389849 (ecx = centity, edx = event, args localClient, replay flag) lives in the
 * protected .tvm0 range, so its four .text call sites are hooked instead:
 *   0x441E19 event entities (eType - ET_EVENTS), 0x44806E the entity event ring,
 *   0x45D708 / 0x45D769 the predicted player's events (both CG_CheckPlayerstateEvents loops).
 * cent+0xE4 is es.number, cent+0xE8 es.eType (CalcMuzzlePoint 0x48268F). */
#define ENTEVENT_FUNC_VA 0x08389849u

__attribute__((used)) void __cdecl ParityEntityEvent(const BYTE *cent, int event)
{
    if ((unsigned)event < EVENT_COUNT || !ParityLevel()) return;
    char key[32];
    _snprintf(key, sizeof(key), "%d", event);
    ParityReport(0, "event-unknown", key, "entity %d (eType %d) carries event %d, past C1's last event %d (%s)",
                 *(const int *)(cent + 0xE4), *(const int *)(cent + 0xE8), event, EVENT_COUNT - 1,
                 ParityEventName(EVENT_COUNT - 1));
}

/* ---- 4b. snapshot decode and references -------------------------------------------------- */
/* CG_ReadNextSnapshot 0x46D970 calls CL_GetSnapshot 0x486F50 at 0x46D9F5 (eax = messageNum, arg =
 * cg snapshot buffer). After a successful copy the snapshot is checked: entity numbers in range,
 * strictly ascending and not sentinels; eTypes and weapons the client knows; each clientState's
 * world-gun entities (+0x12C/+0x130; the client reads them through clientinfo +0xAF4/+0xAF8 at
 * 0x44C15F, 0x480180..0x4802C1 and CalcMuzzlePoint 0x48262B, treating any non-zero value as an
 * entity number) present whenever their owner's player entity is; the playerState's ground and
 * linked-weapon entities present. */
#define GETSNAP_CALL_VA  0x0046D9F5u
#define GETSNAP_FUNC_VA  0x00486F50u

static BOOL InSnap(const BYTE *bits, int num)
{
    return num >= 0 && num < MAX_GENTITIES && (bits[num >> 3] & (1 << (num & 7)));
}

__attribute__((used)) void __cdecl ParitySnapshot(int ok, const BYTE *snap, int num)
{
    if (!ok) return;
    int slot = g_snapBuf[0] == snap ? 0 : g_snapBuf[1] == snap ? 1 : !g_snapBuf[0] ? 0 : 1;
    g_snapBuf[slot] = snap;
    g_snapNum[slot] = num;
    if (!ParityLevel()) return;

    BYTE bits[MAX_GENTITIES / 8];
    char key[48];
    memset(bits, 0, sizeof(bits));
    int numEnts = *(const int *)(snap + SNAP_NUM_ENTITIES);
    int numClients = *(const int *)(snap + SNAP_NUM_CLIENTS);
    int numWeapons = *(volatile const int *)(DWORD_PTR)NUM_WEAPONS_VA;
    if (numEnts < 0 || numEnts > 0x400 || numClients < 0 || numClients > 0x13) {
        ParityReport(0, "snap", "counts", "snapshot %d counts out of range: %d entities, %d clients",
                     num, numEnts, numClients);
        return;
    }
    int prev = -1;
    for (int i = 0; i < numEnts; i++) {
        const BYTE *es = snap + SNAP_ENTITIES + i * ES_SIZE;
        int n = *(const int *)es, type = *(const int *)(es + 4);
        if (n < 0 || n >= ENTITYNUM_WORLD) {
            _snprintf(key, sizeof(key), "entnum:%d", n);
            ParityReport(0, "snap-entity", key, "snapshot %d entity #%d has number %d (%s)", num, i, n,
                         n == ENTITYNUM_NONE ? "ENTITYNUM_NONE" : n == ENTITYNUM_WORLD ? "ENTITYNUM_WORLD" : "out of range");
            continue;
        }
        if (n <= prev) {
            ParityReport(0, "snap-entity", "order", "snapshot %d entity #%d number %d follows %d (not ascending)",
                         num, i, n, prev);
        }
        prev = n;
        bits[n >> 3] |= (BYTE)(1 << (n & 7));
        if (type < 0 || type >= ET_EVENTS + EVENT_COUNT) {
            _snprintf(key, sizeof(key), "etype:%d", type);
            ParityReport(0, "snap-entity", key, "entity %d has eType %d (event entities end at %d)", n, type,
                         ET_EVENTS + EVENT_COUNT - 1);
        }
        int weapon = *(const int *)(es + 0xD4);
        if ((type == ET_PLAYER || type == ET_MISSILE) && numWeapons > 0 && (weapon < 0 || weapon >= numWeapons)) {
            _snprintf(key, sizeof(key), "weapon:%d:%d", type, weapon);
            ParityReport(0, "snap-entity", key, "entity %d eType %d weapon %d, client has %d weapons (CG_Missile "
                         "clamps it to 0)", n, type, weapon, numWeapons);
        }
    }

    int self = *(const int *)(snap + PS_CLIENTNUM);
    for (int i = 0; i < numClients; i++) {
        const BYTE *cs = snap + SNAP_CLIENTS + i * CS_SIZE;
        int client = *(const int *)cs;
        if (client < 0 || client >= 64) {
            _snprintf(key, sizeof(key), "clientnum:%d", client);
            ParityReport(0, "snap-client", key, "snapshot %d clientState #%d has clientNum %d", num, i, client);
            continue;
        }
        for (int g = 0; g < 2; g++) {
            int ent = *(const int *)(cs + 0x12C + g * 4);
            if (!ent) continue;
            if (ent >= ENTITYNUM_WORLD || ent < 0) {
                _snprintf(key, sizeof(key), "worldgun-sentinel:%d", g);
                ParityReport(0, "snap-client", key, "client %d clientState+0x%X = %d; the client uses any non-zero "
                             "value as an entity (clientinfo+0x%X), 0 means none", client, 0x12C + g * 4, ent,
                             0xAF4 + g * 4);
            } else if (client != self && InSnap(bits, client) && !InSnap(bits, ent)) {
                _snprintf(key, sizeof(key), "worldgun-missing:%d", g);
                ParityReport(0, "snap-client", key, "client %d is in snapshot %d but its world-gun entity %d "
                             "(clientState+0x%X) is not", client, num, ent, 0x12C + g * 4);
            }
        }
    }

    int ground = *(const int *)(snap + PS_GROUNDENT);
    if (ground >= 0 && ground < ENTITYNUM_WORLD && ground != self && !InSnap(bits, ground)) {
        ParityReport(0, "snap-ps", "groundEntityNum", "playerState groundEntityNum %d is not in snapshot %d",
                     ground, num);
    }
    int link = *(const int *)(snap + PS_LINKWEAPONENT);
    if (link >= 0 && link < ENTITYNUM_WORLD && link != self && !InSnap(bits, link)) {
        ParityReport(0, "snap-ps", "linkWeaponEnt", "playerState linkWeaponEnt %d is not in snapshot %d", link, num);
    }
    int weapon = *(const int *)(snap + PS_WEAPON);
    if (numWeapons > 0 && (weapon < 0 || weapon >= numWeapons)) {
        _snprintf(key, sizeof(key), "weapon:%d", weapon);
        ParityReport(0, "snap-ps", key, "playerState weapon %d, client has %d weapons", weapon, numWeapons);
    }
}

/* ---- 3. missing assets ------------------------------------------------------------------ */
/* DB_FindXAssetHeader 0x4CB8E0(type, name): when no zone has the asset it creates a default
 * entry with 0x4CB720 (eax = type, arg = name; call at 0x4CBD21) and returns the type's default
 * (misc/missing_fx for fx, the "Missing FX" box), marking the entry so later lookups of the name
 * get the default silently. A zone that references a missing asset (",name" in DB_LinkXAssetEntry
 * 0x4CC390) reaches 0x4CB720 from 0x4CC47F. Localize entries (type 27) return NULL instead and
 * never get here. Both calls run under the DB lock, so the line is only queued. The configstring
 * that carries the name, if any, is looked up in gameState (stringOffsets 0x6D0D130, data
 * 0x6D1370C; CG_RegisterGraphics 0x44F040 resolves effect configstrings 0xE44+ that way). */
#define DEFASSET_FUNC_VA     0x004CB720u
#define DEFASSET_LOOKUP_RET  0x004CBD26u
#define DEFASSET_ZONEREF_RET 0x004CC484u

static int ParityConfigstringOf(const char *name)
{
    if (!g_csReadable) return -1;
    const DWORD *offsets = (const DWORD *)(DWORD_PTR)CS_OFFSETS_VA;
    for (DWORD i = 0; i < CS_COUNT; i++) {
        DWORD off = offsets[i];
        if (!off || off >= CS_DATA_LIMIT - 256) continue;
        if (lstrcmpiA((const char *)(DWORD_PTR)(CS_DATA_VA + off), name) == 0) return (int)i;
    }
    return -1;
}

__attribute__((used)) void __cdecl ParityDefaultAsset(DWORD ret, int type, const char *name)
{
    if (!name || !ParityLevel()) return;
    const char *typeName = "?";
    if (type >= 0 && type < ASSET_TYPE_COUNT) {
        const char *t = *(const char *const *)(DWORD_PTR)(ASSET_TYPE_NAMES_VA + type * 4);
        if (t) typeName = t;
    }
    char key[300];
    _snprintf(key, sizeof(key), "%d:%s", type, name);
    key[sizeof(key) - 1] = 0;
    int cs = ParityConfigstringOf(name);
    char csText[32] = "";
    if (cs >= 0) _snprintf(csText, sizeof(csText), ", configstring %d", cs);
    ParityReport(PR_ONCE | PR_QUEUE, "asset", key, "%s '%s' is not in any loaded zone, the client uses the default %s%s%s",
                 typeName, name, typeName, csText,
                 ret == DEFASSET_ZONEREF_RET ? " (referenced from a zone)" : "");
}

/* ---- 5. menus and UI variables ----------------------------------------------------------- */
/* Menus_OpenByName 0x5F4FB0: with Scaleform on ("2225" == "on") it first offers the name to
 * 0x6B7940; what that does not open is looked up with Menus_FindByName 0x5F95C0 (call at
 * 0x5F50C6) and silently ignored when missing (IW3 printed "unable to find menu"). A NULL from
 * that call means no menu of that name exists anywhere.
 * The UI expression functions localVarString/Bool/Int/Float (0x5DE1E0/0x5DE280/0x5DE300/
 * 0x5DE370) look the name up with 0x5DE150 (ecx = operand {type +0, string +4}); unknown names
 * silently read as ""/0. A wrong operand type already prints "Must use a string as the name of a
 * localVar"; getperk (0x5DE6DB) already prints "Perk slot name '%s' not found", so neither is
 * repeated here. */
#define MENUFIND_CALL_VA 0x005F50C6u
#define MENUFIND_FUNC_VA 0x005F95C0u
#define LOCALVAR_FUNC_VA 0x005DE150u

__attribute__((used)) void __cdecl ParityMenuFind(const char *name, const void *menu)
{
    ParityTick();
    if (menu || !name) return;
    ParityReport(PR_ONCE, "menu", name, "Menus_OpenByName('%s'): no Scaleform movie or menu of that name", name);
}

__attribute__((used)) void __cdecl ParityLocalVar(const int *operand, const void *found)
{
    ParityTick();
    if (found || !operand || operand[0] != 2) return;
    const char *name = (const char *)(DWORD_PTR)operand[1];
    if (!name) return;
    ParityReport(PR_ONCE, "localvar", name, "UI localVar '%s' is not defined; it reads as empty/0", name);
}

/* ---- thunks ----------------------------------------------------------------------------- */

__attribute__((used)) DWORD g_phSaveOld = SAVEOLD_FUNC_VA;
__attribute__((used)) DWORD g_phEvents = EVENTS_FUNC_VA;
__attribute__((used)) DWORD g_phVecLen = VECLEN_FUNC_VA;
__attribute__((used)) DWORD g_phEntEvent = ENTEVENT_FUNC_VA;
__attribute__((used)) DWORD g_phGetSnap = GETSNAP_FUNC_VA;
__attribute__((used)) DWORD g_phDefaultAsset = DEFASSET_FUNC_VA;
__attribute__((used)) DWORD g_phMenuFind = MENUFIND_FUNC_VA;
__attribute__((used)) DWORD g_phLocalVar = LOCALVAR_FUNC_VA;

void PH_SaveOldHook(void);
void PH_EventsHook(void);
void PH_PredHook(void);
void PH_EntEventHook(void);
void PH_SnapHook(void);
void PH_DefaultAssetHook(void);
void PH_MenuFindHook(void);
void PH_LocalVarHook(void);
__asm__(
    ".text\n"
    /* pre-hook, no arguments */
    ".globl _PH_SaveOldHook\n"
    "_PH_SaveOldHook:\n"
    "    pushal\n"
    "    call _ParitySaveOld\n"
    "    popal\n"
    "    jmp *_g_phSaveOld\n"

    /* pre-hook: (localClient, ops), esi = ps */
    ".globl _PH_EventsHook\n"
    "_PH_EventsHook:\n"
    "    pushal\n"
    "    pushl %esi\n"
    "    pushl 0x2c(%esp)\n"           /* ops */
    "    pushl 0x2c(%esp)\n"           /* localClient */
    "    call _ParityEvents\n"
    "    addl $12, %esp\n"
    "    popal\n"
    "    jmp *_g_phEvents\n"

    /* post-hook: float VectorLength(const float *v), edi = cg; the result goes back in st(0) */
    ".globl _PH_PredHook\n"
    "_PH_PredHook:\n"
    "    pushl 4(%esp)\n"
    "    call *_g_phVecLen\n"
    "    addl $4, %esp\n"
    "    subl $4, %esp\n"
    "    fstps (%esp)\n"               /* x87 stack empty while C runs */
    "    pushal\n"
    "    pushl 32(%esp)\n"             /* length bits */
    "    pushl %edi\n"                 /* cg */
    "    pushl 48(%esp)\n"             /* delta vector */
    "    call _ParityPredCheck\n"
    "    addl $12, %esp\n"
    "    popal\n"
    "    flds (%esp)\n"
    "    addl $4, %esp\n"
    "    ret\n"

    /* pre-hook: ecx = centity, edx = event */
    ".globl _PH_EntEventHook\n"
    "_PH_EntEventHook:\n"
    "    pushal\n"
    "    pushl %edx\n"
    "    pushl %ecx\n"
    "    call _ParityEntityEvent\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    jmp *_g_phEntEvent\n"

    /* post-hook: CL_GetSnapshot(buffer), eax = messageNum */
    ".globl _PH_SnapHook\n"
    "_PH_SnapHook:\n"
    "    pushl %eax\n"
    "    pushl 8(%esp)\n"
    "    call *_g_phGetSnap\n"
    "    addl $4, %esp\n"
    "    popl %ecx\n"
    "    pushal\n"
    "    pushl %ecx\n"                 /* messageNum */
    "    pushl 40(%esp)\n"             /* buffer */
    "    pushl %eax\n"                 /* result */
    "    call _ParitySnapshot\n"
    "    addl $12, %esp\n"
    "    popal\n"
    "    ret\n"

    /* pre-hook: eax = type, (name); the return address names the site */
    ".globl _PH_DefaultAssetHook\n"
    "_PH_DefaultAssetHook:\n"
    "    pushal\n"
    "    pushl 0x24(%esp)\n"           /* name */
    "    pushl %eax\n"                 /* type */
    "    pushl 0x28(%esp)\n"           /* return address */
    "    call _ParityDefaultAsset\n"
    "    addl $12, %esp\n"
    "    popal\n"
    "    jmp *_g_phDefaultAsset\n"

    /* post-hook: Menus_FindByName(dc, name) */
    ".globl _PH_MenuFindHook\n"
    "_PH_MenuFindHook:\n"
    "    pushl 8(%esp)\n"
    "    pushl 8(%esp)\n"
    "    call *_g_phMenuFind\n"
    "    addl $8, %esp\n"
    "    pushal\n"
    "    pushl %eax\n"
    "    pushl 0x2c(%esp)\n"           /* name */
    "    call _ParityMenuFind\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    ret\n"

    /* post-hook: local variable lookup, ecx = operand */
    ".globl _PH_LocalVarHook\n"
    "_PH_LocalVarHook:\n"
    "    pushl %ecx\n"
    "    call *_g_phLocalVar\n"
    "    popl %ecx\n"
    "    pushal\n"
    "    pushl %eax\n"
    "    pushl %ecx\n"
    "    call _ParityLocalVar\n"
    "    addl $8, %esp\n"
    "    popal\n"
    "    ret\n");

/* ---- install ---------------------------------------------------------------------------- */

typedef struct {
    const char *what;
    DWORD site, target;
    BYTE before[6]; /* the bytes ahead of the E8, as in C1.exe */
    void (*hook)(void);
} ParityCallPatch;

static const ParityCallPatch kParityPatches[] = {
    { "prediction: save old state (CG_DrawActiveFrame)", SAVEOLD_CALL_VA, SAVEOLD_FUNC_VA,
      { 0x89, 0x0D, 0x84, 0xD2, 0xC3, 0x06 }, PH_SaveOldHook },
    { "prediction: error check (CG_PredictPlayerState VectorLength)", PRED_CALL_VA, VECLEN_FUNC_VA,
      { 0xD8, 0x65, 0xAC, 0xD9, 0x5D, 0xB8 }, PH_PredHook },
    { "events: CG_CheckPlayerstateEvents", EVENTS_CALL_VA, EVENTS_FUNC_VA,
      { 0xE4, 0x1B, 0xFF, 0xFF, 0x57, 0x53 }, PH_EventsHook },
    { "events: CG_EntityEvent from event entities", 0x00441E19u, ENTEVENT_FUNC_VA,
      { 0x83, 0xEA, 0x15, 0x57, 0x8B, 0xCE }, PH_EntEventHook },
    { "events: CG_EntityEvent from the entity event ring", 0x0044806Eu, ENTEVENT_FUNC_VA,
      { 0x89, 0x86, 0x90, 0x01, 0x00, 0x00 }, PH_EntEventHook },
    { "events: CG_EntityEvent from predicted events", 0x0045D708u, ENTEVENT_FUNC_VA,
      { 0x89, 0x83, 0xF8, 0x34, 0x00, 0x00 }, PH_EntEventHook },
    { "events: CG_EntityEvent from unpredictable events", 0x0045D769u, ENTEVENT_FUNC_VA,
      { 0x89, 0x83, 0xF8, 0x34, 0x00, 0x00 }, PH_EntEventHook },
    { "snapshot: CL_GetSnapshot (CG_ReadNextSnapshot)", GETSNAP_CALL_VA, GETSNAP_FUNC_VA,
      { 0x39, 0xE8, 0x35, 0x00, 0x00, 0x53 }, PH_SnapHook },
    { "assets: default for a missing lookup (DB_FindXAssetHeader)", 0x004CBD21u, DEFASSET_FUNC_VA,
      { 0x8B, 0x45, 0x0C, 0x50, 0x8B, 0xC6 }, PH_DefaultAssetHook },
    { "assets: default for a missing zone reference", 0x004CC47Fu, DEFASSET_FUNC_VA,
      { 0x8B, 0x45, 0xFC, 0x50, 0x8B, 0xC7 }, PH_DefaultAssetHook },
    { "ui: Menus_FindByName (Menus_OpenByName)", MENUFIND_CALL_VA, MENUFIND_FUNC_VA,
      { 0x62, 0xC7, 0xE1, 0x07, 0x53, 0x57 }, PH_MenuFindHook },
    { "ui: localVarString lookup", 0x005DE1EFu, LOCALVAR_FUNC_VA,
      { 0x53, 0x8B, 0x5D, 0x08, 0x8B, 0xCB }, PH_LocalVarHook },
    { "ui: localVarBool lookup", 0x005DE283u, LOCALVAR_FUNC_VA,
      { 0xCC, 0xCC, 0xCC, 0x51, 0x8B, 0xCF }, PH_LocalVarHook },
    { "ui: localVarInt lookup", 0x005DE303u, LOCALVAR_FUNC_VA,
      { 0xCC, 0xCC, 0xCC, 0x51, 0x8B, 0xCF }, PH_LocalVarHook },
    { "ui: localVarFloat lookup", 0x005DE378u, LOCALVAR_FUNC_VA,
      { 0xEC, 0x83, 0xEC, 0x08, 0x8B, 0xCF }, PH_LocalVarHook },
};

/* Things the logger reads directly, checked once: the print and dvar functions' prologues and the
 * name tables. If any is off, nothing is installed. */
static BOOL ParityTablesMatch(void)
{
    static const BYTE kComPrintf[] = { 0x55, 0x8B, 0xEC, 0xB8, 0x28, 0x20, 0x00, 0x00 };
    static const BYTE kRegisterInt[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x64, 0xA1, 0x2C, 0x00, 0x00, 0x00 };
    const char *const *ev = (const char *const *)(DWORD_PTR)EVENT_NAMES_VA;
    const char *const *types = (const char *const *)(DWORD_PTR)ASSET_TYPE_NAMES_VA;
    return Readable((void *)(DWORD_PTR)PARITY_COM_PRINTF_VA, sizeof(kComPrintf)) &&
           memcmp((void *)(DWORD_PTR)PARITY_COM_PRINTF_VA, kComPrintf, sizeof(kComPrintf)) == 0 &&
           Readable((void *)(DWORD_PTR)DVAR_REGISTER_INT_VA, sizeof(kRegisterInt)) &&
           memcmp((void *)(DWORD_PTR)DVAR_REGISTER_INT_VA, kRegisterInt, sizeof(kRegisterInt)) == 0 &&
           Readable(ev, (EVENT_COUNT + 1) * 4) && !ev[EVENT_COUNT] && ev[0] && Readable(ev[0], 8) &&
           memcmp(ev[0], "EV_NONE", 8) == 0 && ev[EVENT_COUNT - 1] && Readable(ev[EVENT_COUNT - 1], 11) &&
           memcmp(ev[EVENT_COUNT - 1], "EV_DTP_LAND", 11) == 0 &&
           Readable(types, ASSET_TYPE_COUNT * 4) && types[30] && Readable(types[30], 3) &&
           memcmp(types[30], "fx", 3) == 0;
}

/* "parity_log N" anywhere on the command line (last wins), else CODOL_PARITY_LOG=N, else 1. */
static int ParityStartLevel(void)
{
    int level = 1;
    char env[16];
    DWORD n = GetEnvironmentVariableA("CODOL_PARITY_LOG", env, sizeof(env));
    if (n && n < sizeof(env)) level = atoi(env);
    const char *p = GetCommandLineA();
    while ((p = strstr(p, "parity_log")) != NULL) {
        p += 10;
        if (*p != ' ' && *p != '\t' && *p != '"') continue;
        while (*p == ' ' || *p == '\t' || *p == '"') p++;
        if (*p >= '0' && *p <= '9') level = atoi(p);
    }
    return level < 0 ? 0 : level > 2 ? 2 : level;
}

static void InstallParityLogger(void)
{
    g_parityStartLevel = ParityStartLevel();
    if (!g_parityStartLevel) {
        Log("parity: off (parity_log 0 at startup), no hooks installed");
        return;
    }
    if (!ParityTablesMatch()) {
        Log("parity: Com_Printf / Dvar_RegisterInt / event or asset name tables do not match, not installed");
        return;
    }
    InitializeCriticalSection(&g_parityLock);
    g_mainThread = GetCurrentThreadId();
    g_csReadable = Readable((void *)(DWORD_PTR)CS_OFFSETS_VA, CS_COUNT * 4) &&
                   Readable((void *)(DWORD_PTR)CS_DATA_VA, CS_DATA_LIMIT);
    for (unsigned i = 0; i < sizeof(kParityPatches) / sizeof(kParityPatches[0]); i++) {
        const ParityCallPatch *p = &kParityPatches[i];
        const BYTE *site = (const BYTE *)(DWORD_PTR)p->site;
        if (!Readable(site - 6, 6) || memcmp(site - 6, p->before, 6) != 0 || !CallsTarget(p->site, p->target)) {
            Log("parity: %s: bytes at 0x%08lX do not match, skipped", p->what, (unsigned long)p->site);
            g_paritySkipped++;
            continue;
        }
        RedirectCall(p->site, (void *)p->hook);
        g_parityHooks++;
    }
    Log("parity: logger active, %d hooks installed, %d skipped, parity_log %d at startup%s", g_parityHooks,
        g_paritySkipped, g_parityStartLevel, g_csReadable ? "" : ", configstring lookup off");
}

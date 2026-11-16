/*
 * locale_switch.c - the language, changed while the game runs (k33_language; docs/LANGUAGE_SWITCH_20261112.md).
 * Included by kernel33.c after voice.c (its command arguments) and k33tick.c; the pure part is locale_switch_policy.h.
 *
 * What a language is: which string a localize name (asset type 27) resolves to. A language's strings are one zone,
 * codol_<name>_locale.ff. A switch unloads the zone of the language on screen and loads the other one, with C1's
 * own zone loader: the old strings' memory, asset entries and localize pool slots are freed and the new ones are
 * allocated. Nothing of a language that is not on screen stays in memory.
 *
 * Why that needs anything from this file. C1 unloads zones by alloc-flag group (DB_LoadXAssets {NULL, 0, flags}),
 * so the language zone has a flag of its own, 0x10 (LR_ALLOC_FLAG). But two zones with different flags that carry
 * the same name both keep an asset entry (DB_LinkXAssetEntry 0x4CC61B: the first stays active, the second is
 * chained behind it as an override, 0x4CC648), and English shares 8,206 names with C1's stock zones. C1's entry
 * table 0x1C402C8 holds 59,999 entries for all assets of all types (uint16 links, it cannot grow); an English
 * client uses 40,342 at the front end and about 47,500 in a match, so 8,206 more is not a margin. So:
 *
 *   1. The language zone loads last of the startup zones (after code_post_gfx, kernel33.c SubmitPatchZones), when
 *      every stock code zone already holds its names.
 *   2. LR_LinkHook (the flag compare at 0x4CC61B) sees each of its strings whose name is held. It puts the
 *      language's string into the holder's pool slot in place (one pointer store; the holder's own pointer is
 *      remembered here, by slot) and has C1 throw the new entry away, as C1 does for equal flags: no entry, no
 *      slot. A holder that C1 can unload (UI, gametype, map) also keeps the new entry as C1's ordinary override,
 *      so the name outlives it. Names only the language has are ordinary entries of the language zone.
 *      MEASURED (English, front end): 8,206 strings in place, 1,966 entries of its own, 40,343 entries in all -
 *      one more than with the zone ahead of code_pre_gfx under flag 1 (its fonts_ingame.gfx marker).
 *   3. A switch, on the main thread while C1's database is idle: every slot that still carries the language's
 *      pointer gets its own pointer back, DB_LoadXAssets {NULL, 0, 0x10} unloads the zone (C1 frees its entries,
 *      slots and memory), DB_LoadXAssets {name, 0x10, 0} loads the other one through the same hook.
 *
 * Memory. A zone's memory is a block of a PMem stack (0x2B2C97C + type * 0x10C: {.., count, pos, {name, pos}[32]},
 * inlined at 0x4CD6AF: flags 1 and 0x10 use stack 0, the code zones' stack, every other flag stack 1). PMem_Free
 * is virtualized; MEASURED in a running client: freeing the top block pops it (count and pos go back to the
 * block below), freeing a block that is not the top only clears its name and gives nothing back until everything
 * above it is freed. Stack 0 is never freed, so the language zone must be its top block: it is, because it loads
 * last and C1 loads nothing with flag 1 afterwards (gametype and map zones are flag 4, stack 1; measured at the
 * front end, on a loading screen, in matches and after them). LrGate checks it before every switch and answers
 * "restart" otherwise, so a switch never leaks.
 *
 * C1's own unloads. 0x599CB0 unloads the groups 0x1E whenever a match ends (mov eax, 1Eh at 0x599CB7); C1 loads
 * nothing with 0x10 itself, so that mask is patched to 0x0E and the language zone stays for the session.
 *
 * Pointers. C1 hands out raw char pointers into zone memory (0x5DA3B2 returns entry->value). After an unload the
 * old zone's block is free but still mapped; the next zone loaded there overwrites it. Scaleform text is a copy
 * (the translator 0x6A5AC0 copies), so a movie keeps the old language's text until it sets it again
 * (k33_languageSerial). MEASURED by scanning the process's writable memory for pointers into the zone's block
 * before a switch: see the doc ("Who holds pointers").
 *
 * The font movie (scaleform/fonts_ingame.gfx) must not be in the language zone: a movie Scaleform has loaded is
 * memory C1 keeps using. It is in codol_english_font, an ordinary flag-1 zone ahead of code_pre_gfx that loads
 * when the client starts in English and stays (zone_packs/, kernel33.c).
 */
#define LS_PUBLISH(dst, src) InterlockedExchangePointer((PVOID volatile *)(dst), (PVOID)(src))
#include "locale_switch_policy.h"

#define LS_LOCALIZE          LR_LOCALIZE_TYPE
#define LR_UNLOAD_FLAG       0x10          /* the language zone's alloc flag while LrUnloadZone unloads it; no C1 zone has it */
#define LR_LINK_CMP_VA       0x004CC61Bu   /* DB_LinkXAssetEntry: cmp ecx, [eax+0x1F0E154] ; jne 0x4CC648 (override) */
#define LR_LINK_START_VA     0x004CC600u   /* the bytes checked around it */
#define LR_FREE_ENTRY_VA     0x004CB320u   /* frees the entry in esi: what the equal-flags path calls at 0x4CC639 */
#define LR_UNLOAD_ZONE_VA    0x004CD9A0u   /* DB_UnloadXZone(zone, createDefault): C1's loader calls it per zone */
#define LR_PMEM_SELECT_VA    0x004CD6AFu   /* the inlined PMem_BeginAlloc: which stack a zone's flags use */
#define LR_IDLE_READ_VA      0x004CC8D7u   /* mov eax, [0x26090BC] in the function that takes a load's results */
#define LR_UNLOAD_UI_CALL_VA 0x0059864Au   /* C1's own "unload group 2" call of DB_LoadXAssets */
#define LR_UNLOAD_MAP_CALL_VA 0x004CCE6Cu  /* and its "unload groups 6" call before a map loads */
#define LR_MAP_LOAD_CALL_VA  0x004CD0F0u   /* the map load's DB_LoadXAssets: gametype zones, and common_mp (flag 1) */
#define LS_ZONES_VA          0x01F0E108u   /* 0xA8 bytes each: name +0xC, alloc flags +0x4C, PMem stack +0x50 */
#define LS_ZONE_SIZE         0xA8u
#define LS_ZONE_MAX          35
#define LR_ZONE_LIMIT        33            /* C1 holds 33 zones (0x4CC824) */
#define LS_ZONE_COUNT_VA     0x018FCFBCu
#define LS_ZONE_ORDER_VA     0x01C402A4u   /* zone indices in load order */
#define LR_LOADED_FLAGS_VA   0x0187490Cu   /* the alloc flags of everything loaded: C1 tests it before an unload call */
#define LS_ENTRIES_VA        0x01C402C8u   /* 24 bytes each: type +0, header +8, zone +0x10, next +0x12, override +0x14 */
#define LS_ENTRY_MAX         60040u
#define LR_ENTRY_FREE_VA     0x018779BCu   /* head of the free entries (a free entry's first dword is the next one) */
#define LR_TYPE_COUNT_VA     (0x018FCFC0u + 4 * LS_LOCALIZE)   /* assets of the type that hold a pool slot */
#define LS_HASH_VA           0x018FD9D8u   /* 60000 uint16 buckets */
#define LS_HASH_SIZE         60000u
#define LS_DB_LOCK_VA        0x02007A5Cu   /* {readers, writers}: DB_FindXAssetHeader's read lock, 0x4CB953-0x4CB990 */
#define LR_REPLACE_MODE_VA   0x019A2F6Cu   /* nonzero: the linker replaces active entries (0x4CC0BF); never seen set */
#define LS_DB_READY_VA       0x026090BCu   /* event: nothing loading and the results taken (0x4CC8D7, set at 0x4CCA1A) */
#define LS_POOL_VA           (0x00CD5028u + 4 * LS_LOCALIZE)   /* the localize pool block {free head, entries[]} */
#define LS_POOL_SIZE_VA      (0x00CD4D68u + 4 * LS_LOCALIZE)
#define LR_PMEM_VA           0x02B2C97Cu   /* PMem stack 0: {name, count, pos, {name, pos}[32]}; stack 1 is 0x10C on */
#define LS_REGISTER_STRING_VA 0x00624740u  /* Dvar_RegisterString(name, value, flags, description), as voice.c calls it */
#define LR_INDEX_SLOTS       32768u        /* the loaded language's strings by name: 24,576 (English has 10,172) */
#define LR_WAIT_MS           60000u        /* a request waits this long for an idle database, then is "restart" */
#define LR_NEED_ENTRIES      4096u         /* free asset entries and pool slots a switch wants (English takes 1,966) */

static int g_lsCheck = -1;                 /* -1 not looked at, 0 a site differs, 1 every site is C1's */
static BOOL g_lsInstalled;
static volatile LONG g_lsShown = -1;       /* index in g_langs of the language on screen */
static int g_lsStartup = -1;               /* index of the language the client started in (-1: not in the list) */
static char g_lrName[64];                  /* the language zone that is loaded or loading ("" = none) */
static LsTable g_lrIndex;                  /* its strings whose name another zone holds, by name (zone memory) */
static const char **g_lrStock, **g_lrMine; /* per localize pool slot: the holder's own string / the language's */
static DWORD g_lrPoolFirst;
static unsigned g_lrPoolSlots;
static volatile LONG g_lrInPlace, g_lrChained, g_lrDropped;   /* counted by the hook while a zone links */
__attribute__((used)) volatile LONG g_lrUnloadZone = -1;   /* the zone slot LrUnloadZone is unloading, else -1 */
__attribute__((used)) DWORD g_lrUnloadResume = LR_UNLOAD_ZONE_VA + 6;
static DWORD g_lrZoneSig;                  /* the zone set the slots were last reconciled against */

static const BYTE *volatile g_lsDvLanguage, *volatile g_lsDvList, *volatile g_lsDvState, *volatile g_lsDvSerial;
static BOOL g_lsDvarsDone;
static int g_lsWanted = -1;                /* the last request that was accepted (on screen, or saved for restart) */
static char g_lsSeen[64];                  /* k33_language as last handled */
static int g_lsState, g_lsSerial;
enum { LR_PHASE_IDLE, LR_PHASE_WAIT, LR_PHASE_LOADING, LR_PHASE_STARTUP };
static int g_lrPhase = LR_PHASE_STARTUP, g_lrTarget = -1;
static DWORD g_lrSince;
#if K33_LOCAL_TESTING
static int g_lrCycles;                     /* loc_cycle: switches still to do */
static BOOL g_lrForce;                     /* loc_forceUnload: the next switch ignores LR_GATE_NO_STACK (measurement) */
#endif
static BOOL g_lrNoReorder;                 /* +set loc_noReorder 1 (test builds): LrMapLoadHook passes through */
#if K33_LOCAL_TESTING
#endif

typedef struct LrStats {
    unsigned entriesFree, localize, zones, slotsFree;
    unsigned stackCount[2], stackPos[2];
    char top[2][64];
} LrStats;

/* Called by LoadPatchDir before anything is queued, and again by InstallLocaleSwitch: bytes only, nothing written.
 * Bytes are C1's as it runs (the image on disk is packed: its import references differ, which is why the sites
 * here are ones without an import call). */
static BOOL LocaleSwitchCheck(void)
{
    /* 0x4CC600: movzx edx, byte [ebx+10h] ; mov eax, [0x18FD9D4] ; imul edx, 0A8h ; mov ecx, [edx+0x1F0E154] ;
     * imul eax, 0A8h ; (0x4CC61B) cmp ecx, [eax+0x1F0E154] ; jne 0x4CC648 ; mov edx, [esi] ;
     * mov eax, [edx*4+0xCD5238] ; test eax, eax ; je +9 ; mov ecx, [esi+8] ; push ecx ; call eax ; add esp, 4 ;
     * call 0x4CB320 ; mov edx, [ebx+8] ; mov eax, [ebp+0Ch] ; mov [eax], edx ; jmp 0x4CC675 ;
     * (0x4CC648) mov cx, [ebx+14h] ; mov [esi+14h], cx */
    static const BYTE kLink[] = { 0x0F, 0xB6, 0x53, 0x10, 0xA1, 0xD4, 0xD9, 0x8F, 0x01, 0x69, 0xD2, 0xA8, 0x00, 0x00, 0x00,
                                  0x8B, 0x8A, 0x54, 0xE1, 0xF0, 0x01, 0x69, 0xC0, 0xA8, 0x00, 0x00, 0x00,
                                  0x3B, 0x88, 0x54, 0xE1, 0xF0, 0x01, 0x75, 0x25, 0x8B, 0x16, 0x8B, 0x04, 0x95, 0x38, 0x52,
                                  0xCD, 0x00, 0x85, 0xC0, 0x74, 0x09, 0x8B, 0x4E, 0x08, 0x51, 0xFF, 0xD0, 0x83, 0xC4, 0x04,
                                  0xE8, 0xE2, 0xEC, 0xFF, 0xFF, 0x8B, 0x53, 0x08, 0x8B, 0x45, 0x0C, 0x89, 0x10, 0xEB, 0x2D,
                                  0x66, 0x8B, 0x4B, 0x14, 0x66, 0x89, 0x4E, 0x14 };
    /* 0x4CB320: mov ecx, [esi+8] ; mov eax, [esi] ; mov edx, [eax*4+0xCD4F78] ; dec dword [eax*4+0x18FCFC0] ;
     * push ecx ; mov ecx, [eax*4+0xCD5028] ; push ecx ; call edx ; mov eax, [0x18779BC] */
    static const BYTE kFree[] = { 0x8B, 0x4E, 0x08, 0x8B, 0x06, 0x8B, 0x14, 0x85, 0x78, 0x4F, 0xCD, 0x00, 0xFF, 0x0C, 0x85,
                                  0xC0, 0xCF, 0x8F, 0x01, 0x51, 0x8B, 0x0C, 0x85, 0x28, 0x50, 0xCD, 0x00, 0x51, 0xFF, 0xD2,
                                  0xA1, 0xBC, 0x79, 0x87, 0x01 };
    /* 0x4CD6AF: mov eax, [ebp+0Ch] ; dec eax ; je +0Ch ; sub eax, 0Fh ; je +7 ; mov eax, 1 ; jmp +2 ; xor eax, eax ;
     * mov [0x18C0FB4], eax ; mov eax, [0x18C0FB4] ; imul eax, 10Ch ; add eax, 0x2B2C97C */
    static const BYTE kPmem[] = { 0x8B, 0x45, 0x0C, 0x48, 0x74, 0x0C, 0x83, 0xE8, 0x0F, 0x74, 0x07, 0xB8, 0x01, 0x00, 0x00,
                                  0x00, 0xEB, 0x02, 0x33, 0xC0, 0xA3, 0xB4, 0x0F, 0x8C, 0x01, 0xA1, 0xB4, 0x0F, 0x8C, 0x01,
                                  0x69, 0xC0, 0x0C, 0x01, 0x00, 0x00, 0x05, 0x7C, 0xC9, 0xB2, 0x02 };
    static const BYTE kIdle[] = { 0xA1, 0xBC, 0x90, 0x60, 0x02 };
    /* 0x4CD9A0: push ebp ; mov ebp, esp ; mov eax, [ebp+8] ; imul eax, 0A8h ; sub esp, 1Ch ;
     * test byte [eax+0x1F0E154], 4 */
    static const BYTE kUnload[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x69, 0xC0, 0xA8, 0x00, 0x00, 0x00, 0x83, 0xEC, 0x1C,
                                    0xF6, 0x80, 0x54, 0xE1, 0xF0, 0x01, 0x04 };
    DWORD bad = 0;
    if (g_lsCheck >= 0) return g_lsCheck;
#define LS_SAME(va, bytes) (Readable((void *)(DWORD_PTR)(va), sizeof(bytes)) && !memcmp((void *)(DWORD_PTR)(va), bytes, sizeof(bytes)))
    if (!LS_SAME(LR_LINK_START_VA, kLink)) bad = LR_LINK_CMP_VA;
    else if (!LS_SAME(LR_FREE_ENTRY_VA, kFree)) bad = LR_FREE_ENTRY_VA;
    else if (!LS_SAME(LR_UNLOAD_ZONE_VA, kUnload)) bad = LR_UNLOAD_ZONE_VA;
    else if (!LS_SAME(LR_PMEM_SELECT_VA, kPmem)) bad = LR_PMEM_SELECT_VA;
    else if (!LS_SAME(LR_IDLE_READ_VA, kIdle)) bad = LR_IDLE_READ_VA;
    else if (!CallsTarget(LR_UNLOAD_UI_CALL_VA, DB_LOAD_XASSETS_VA) || !CallsTarget(LR_UNLOAD_MAP_CALL_VA, DB_LOAD_XASSETS_VA))
        bad = DB_LOAD_XASSETS_VA;
    else if (!CallsTarget(POSTGFX_LOAD_CALL_VA, DB_LOAD_XASSETS_VA)) bad = POSTGFX_LOAD_CALL_VA;
    else if (!CallsTarget(LR_MAP_LOAD_CALL_VA, DB_LOAD_XASSETS_VA)) bad = LR_MAP_LOAD_CALL_VA;
    else if (!Readable((void *)(DWORD_PTR)LS_ZONES_VA, LS_ZONE_MAX * LS_ZONE_SIZE) ||
             !Readable((void *)(DWORD_PTR)LS_ENTRIES_VA, LS_ENTRY_MAX * 24) ||
             !Readable((void *)(DWORD_PTR)LS_HASH_VA, LS_HASH_SIZE * 2) || !Readable((void *)(DWORD_PTR)LS_DB_LOCK_VA, 8) ||
             !Readable((void *)(DWORD_PTR)LS_DB_READY_VA, 4) || !Readable((void *)(DWORD_PTR)LR_PMEM_VA, 2 * 0x10C) ||
             !Readable((void *)(DWORD_PTR)LS_POOL_VA, 4) || !Readable((void *)(DWORD_PTR)LS_POOL_SIZE_VA, 4) ||
             !Readable((void *)(DWORD_PTR)LR_LOADED_FLAGS_VA, 4) || !Readable((void *)(DWORD_PTR)LR_REPLACE_MODE_VA, 4))
        bad = LS_ZONES_VA;
#undef LS_SAME
    g_lsCheck = !bad;
    if (bad)
        Log("language: bytes at 0x%08lX differ; no live switch (k33_language is saved and applies on restart)",
            (unsigned long)bad);
    return g_lsCheck;
}

static const char *LrZoneName(unsigned zone)
{
    return (const char *)(DWORD_PTR)(LS_ZONES_VA + zone * LS_ZONE_SIZE + 0xC);
}

/* The slot (index in C1's zone table) of the loaded zone `name`, -1 when it is not loaded. */
static int LrFindZone(const char *name)
{
    unsigned count = *(volatile unsigned *)(DWORD_PTR)LS_ZONE_COUNT_VA;
    const BYTE *order = (const BYTE *)(DWORD_PTR)LS_ZONE_ORDER_VA;
    for (unsigned i = 0; i < count && i < LS_ZONE_MAX; i++)
        if (order[i] < LS_ZONE_MAX && !lstrcmpiA(LrZoneName(order[i]), name)) return order[i];
    return -1;
}

static LsSlot *LrPoolSlot(const void *header, unsigned *index)
{
    DWORD off = (DWORD)(DWORD_PTR)header - g_lrPoolFirst;
    if (!header || !g_lrPoolSlots || off >= g_lrPoolSlots * 8 || (off & 7)) return NULL;
    *index = off >> 3;
    return (LsSlot *)header;
}

/* The language's string goes into the holder's slot; the holder's own pointer is kept for the unload. */
static void LrPutInPlace(LsSlot *held, unsigned index, const char *value)
{
    if (held->value == value) return;
    g_lrStock[index] = held->value;
    g_lrMine[index] = value;
    InterlockedExchangePointer((PVOID volatile *)&held->value, (PVOID)value);
}

/* The thread that links the zone (C1's database thread, under the database write lock). ebx / esi of 0x4CC61B: the
 * entry that holds the name and the new entry, whose header is a pool slot filled from the zone's asset (0x4CC521):
 * its two pointers are the zone's own memory. Returns an LrLink. */
__attribute__((used)) int __cdecl LrLink(BYTE *holder, const BYTE *fresh)
{
    unsigned zone = fresh[0x10], index = 0;
    if (!g_lrName[0] || !g_lrStock || zone >= LS_ZONE_MAX || lstrcmpiA(LrZoneName(zone), g_lrName) != 0) return LR_LINK_C1;
    const LsSlot *mine = *(const LsSlot *const *)(fresh + 8);
    LsSlot *held = LrPoolSlot(*(void *const *)(holder + 8), &index);
    unsigned holderZone = holder[0x10];
    if (!mine || !mine->value || !mine->name || !held || holderZone >= LS_ZONE_MAX) return LR_LINK_C1;
    enum LrLink link = LrLinkDecision(LS_LOCALIZE, 1, holderZone == zone,
                                      *(const DWORD *)(DWORD_PTR)(LS_ZONES_VA + holderZone * LS_ZONE_SIZE + 0x4C));
    if (link == LR_LINK_C1) return link;
    if (LsInsert(&g_lrIndex, mine->name, mine->value) < 0) InterlockedIncrement(&g_lrDropped);
    LrPutInPlace(held, index, mine->value);
    InterlockedIncrement(link == LR_LINK_DISCARD ? &g_lrInPlace : &g_lrChained);
    return link;
}

void LR_LinkHook(void);
__asm__(
    ".text\n"
    ".globl _LR_LinkHook\n"
    "_LR_LinkHook:\n"                 /* called from 0x4CC61B: edi = type, ebx = holder, esi = new entry, */
    "    cmpl $27, %edi\n"            /* ecx = the holder zone's alloc flags, eax = the loading zone * 0xA8 */
    "    jne 9f\n"
    "    pushl %eax\n"
    "    pushl %ecx\n"
    "    pushl %edx\n"
    "    pushl %esi\n"
    "    pushl %ebx\n"
    "    call _LrLink\n"
    "    addl $8, %esp\n"
    "    popl %edx\n"
    "    popl %ecx\n"
    "    popl %eax\n"
    "9:  cmpl 0x1F0E154(%eax), %ecx\n" /* C1's own compare, and C1's own decision */
    "    ret\n");

/* DB_UnloadXZone's entry. C1's loader unloads with createDefault 1: an entry without an override is not freed but
 * turned into a copy of the type's default asset (0x4CDB0B) until the next load is in, and a type with a pool but
 * no default asset is fatal there (WIN_LOAD_ASSET_ERROR, 0x4CDB79). The localize default is CGAME_UNKNOWN, which
 * stock C1 has only in localized_common_mp, i.e. in a match: MEASURED, unloading codol_chinese_locale at the front
 * end ended the client with that error. Nothing holds a pointer to a localize entry (the callers read the value
 * and keep that), so for the language zone, and only when LrUnloadZone asked, createDefault is 0: its entries and
 * slots are freed at once (0x4CDAD1), which is what the request is. */
static const char *LrZoneName(unsigned zone);
/* Test builds log the language zone's unload (C1's own unloads pass through untouched and unlogged). */
__attribute__((used)) void __cdecl LrUnloadSeen(unsigned zone, unsigned createDefault, DWORD caller)
{
#if K33_LOCAL_TESTING
    if ((LONG)zone == g_lrUnloadZone)
        Log("language: zone %u \"%s\" unloads with createDefault %u -> 0 (thread %lu, from 0x%08lX)", zone,
            zone < LS_ZONE_MAX ? LrZoneName(zone) : "?", createDefault & 0xFF, GetCurrentThreadId(), caller);
#else
    (void)zone; (void)createDefault; (void)caller;
#endif
}

void LR_UnloadHook(void);
__asm__(
    ".text\n"
    ".globl _LR_UnloadHook\n"
    "_LR_UnloadHook:\n"               /* jumped to from 0x4CD9A0: [esp+4] = zone, [esp+8] = createDefault */
    "    pushal\n"
    "    pushl 32(%esp)\n"            /* the return address */
    "    pushl 44(%esp)\n"            /* createDefault */
    "    pushl 44(%esp)\n"            /* zone */
    "    call _LrUnloadSeen\n"
    "    addl $12, %esp\n"
    "    popal\n"
    "    pushl %eax\n"
    "    movl 8(%esp), %eax\n"
    "    cmpl _g_lrUnloadZone, %eax\n"
    "    jne 1f\n"
    "    movl $0, 12(%esp)\n"
    "1:  popl %eax\n"
    "    pushl %ebp\n"                /* the six bytes the jump replaced */
    "    movl %esp, %ebp\n"
    "    movl 8(%ebp), %eax\n"
    "    jmp *_g_lrUnloadResume\n");

/* DB_FindXAssetHeader's read lock (0x4CB953: count up, wait while a writer is in; 0x4CB989: count down). */
static void LsReadLock(void)
{
    volatile LONG *lock = (volatile LONG *)(DWORD_PTR)LS_DB_LOCK_VA;
    InterlockedIncrement(&lock[0]);
    while (lock[1]) Sleep(1);
}

static void LsReadUnlock(void)
{
    InterlockedDecrement((volatile LONG *)(DWORD_PTR)LS_DB_LOCK_VA);
}

static BOOL LrDbIdle(void)
{
    HANDLE ready = *(HANDLE volatile *)(DWORD_PTR)LS_DB_READY_VA;
    return ready && WaitForSingleObject(ready, 0) == WAIT_OBJECT_0;
}

/* Main thread, database idle. Every active localize entry of a zone that is not the language zone:
 *   restore   its slot still carries the language's pointer -> the holder's own pointer back (before an unload);
 *   otherwise its name is one of the language's (g_lrIndex) and the slot carries something else -> in place.
 *             That is the state the hook leaves; it differs only after C1 unloaded a holder and promoted another
 *             zone's override into the slot (0x4CDBF0 copies the override's header over it).
 * A slot is trusted only through the entry that owns it now, never through what was remembered about it: a slot
 * that was freed has its first pointer overwritten by the pool's free list, one that was promoted over has the
 * promoted zone's pointer. Returns how many slots were changed. */
static unsigned LrSweep(BOOL restore)
{
    const BYTE *entries = (const BYTE *)(DWORD_PTR)LS_ENTRIES_VA;
    const WORD *hash = (const WORD *)(DWORD_PTR)LS_HASH_VA;
    int zone = g_lrName[0] ? LrFindZone(g_lrName) : -1;
    unsigned changed = 0;
    if (!g_lrStock) return 0;
    LsReadLock();
    for (unsigned b = 0; b < LS_HASH_SIZE; b++) {
        unsigned idx = hash[b];
        for (unsigned guard = 0; idx && idx < LS_ENTRY_MAX && guard < LS_ENTRY_MAX; guard++) {
            const BYTE *e = entries + idx * 24;
            unsigned index = 0;
            LsSlot *held = *(const DWORD *)e == LS_LOCALIZE && e[0x10] != (unsigned)zone ? LrPoolSlot(*(void *const *)(e + 8), &index) : NULL;
            if (held && restore) {
                if (g_lrMine[index] && held->value == g_lrMine[index]) {
                    InterlockedExchangePointer((PVOID volatile *)&held->value, (PVOID)g_lrStock[index]);
                    changed++;
                }
            } else if (held && held->name) {
                const LsSlot *mine = LsFind(&g_lrIndex, held->name);
                if (mine && held->value != mine->value) {
                    LrPutInPlace(held, index, mine->value);
                    changed++;
                }
            }
            idx = *(const WORD *)(e + 0x12);
        }
    }
    LsReadUnlock();
    if (restore) {
        memset(g_lrStock, 0, g_lrPoolSlots * sizeof(*g_lrStock));
        memset(g_lrMine, 0, g_lrPoolSlots * sizeof(*g_lrMine));
    }
    return changed;
}

/* Zone names and flags, folded: changes whenever C1 loaded or unloaded something. */
static DWORD LrZoneSignature(void)
{
    unsigned count = *(volatile unsigned *)(DWORD_PTR)LS_ZONE_COUNT_VA;
    const BYTE *order = (const BYTE *)(DWORD_PTR)LS_ZONE_ORDER_VA;
    DWORD h = 2166136261u ^ count;
    for (unsigned i = 0; i < count && i < LS_ZONE_MAX; i++) {
        if (order[i] >= LS_ZONE_MAX) continue;
        for (const char *p = LrZoneName(order[i]); *p; p++) h = (h ^ (BYTE)*p) * 16777619u;
        h = (h ^ order[i]) * 16777619u;
    }
    return h;
}

static void LrReadStats(LrStats *s)
{
    memset(s, 0, sizeof(*s));
    s->zones = *(volatile unsigned *)(DWORD_PTR)LS_ZONE_COUNT_VA;
    s->localize = *(volatile unsigned *)(DWORD_PTR)LR_TYPE_COUNT_VA;
    LsReadLock();
    for (DWORD p = *(volatile DWORD *)(DWORD_PTR)LR_ENTRY_FREE_VA; p >= LS_ENTRIES_VA && p < LS_ENTRIES_VA + LS_ENTRY_MAX * 24 &&
         s->entriesFree < LS_ENTRY_MAX; p = *(volatile DWORD *)(DWORD_PTR)p)
        s->entriesFree++;
    LsReadUnlock();
    s->slotsFree = g_lrPoolSlots > s->localize ? g_lrPoolSlots - s->localize : 0;
    for (int t = 0; t < 2; t++) {
        const DWORD *stack = (const DWORD *)(DWORD_PTR)(LR_PMEM_VA + t * 0x10C);
        s->stackCount[t] = stack[1];
        s->stackPos[t] = stack[2];
        if (stack[1] >= 1 && stack[1] <= 32) {
            const char *top = (const char *)(DWORD_PTR)stack[3 + (stack[1] - 1) * 2];
            if (top && Readable(top, 1)) lstrcpynA(s->top[t], top, sizeof(s->top[t]));
        }
    }
}

static void LsPoke(const BYTE *d, int v)
{
    if (d && *(volatile const int *)(d + 0xC) != v) *(volatile int *)(d + 0xC) = v;   /* read-only publish, as VoiceDvarPoke */
}

static void LsPublish(int state)
{
    g_lsState = state;
    LsPoke(g_lsDvState, g_lsState);
    LsPoke(g_lsDvSerial, g_lsSerial);
}

/* Language i's zone name, "" when it has none (zh without codol_chinese_locale: C1's own text). */
static void LrZoneOf(int lang, char out[64])
{
    out[0] = 0;
    if (lang >= 0 && lang < g_langCount && (g_langLocale >> lang & 1u))
        _snprintf(out, 63, "codol_%s_locale", LanguageZoneStem(g_langs[lang])), out[63] = 0;
}

static unsigned LsReadyMask(void)
{
    unsigned ready = g_langLocale;
    for (int i = 0; i < g_langCount; i++)
        if (!strcmp(g_langs[i], "zh") && !(g_langZones >> i & 1u)) ready |= 1u << i;
    return ready;
}

static void LsRegisterDvars(void)
{
    typedef const BYTE *(__cdecl *RegStr)(const char *, const char *, unsigned, const char *);
    RegStr regs = (RegStr)(DWORD_PTR)LS_REGISTER_STRING_VA;
    /* push ebp ; mov ebp,esp ; and esp,-8 ; sub esp,10h ; mov edx,[ebp+14h] ; mov ecx,[ebp+0Ch] */
    static const BYTE kRegStr[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x10, 0x8B, 0x55, 0x14, 0x8B, 0x4D, 0x0C };
    char list[LANG_MAX * LANG_NAME_MAX];
    g_lsDvarsDone = TRUE;
    if (!Readable((void *)(DWORD_PTR)LS_REGISTER_STRING_VA, sizeof(kRegStr)) ||
        memcmp((void *)(DWORD_PTR)LS_REGISTER_STRING_VA, kRegStr, sizeof(kRegStr)) != 0) {
        Log("language: Dvar_RegisterString 0x%08X differs; k33_language is not registered", LS_REGISTER_STRING_VA);
        return;
    }
    LanguageListJoin(g_langs, g_langCount, list, sizeof(list));
    g_lsDvLanguage = regs("k33_language", g_lang, 0x1,
                          "Language: a name of k33_languageList. Changing it switches the game's text now when "
                          "k33_languageState returns to 0, else on the next start (2)");
    g_lsDvList = regs("k33_languageList", list, 0, "Installed languages, ';' separated (read only)");
    g_lsDvState = C1RegisterInt("k33_languageState", 0, 0, 2, 0,
                                "0 idle, 1 switching, 2 the chosen language applies on the next start (read only)");
    g_lsDvSerial = C1RegisterInt("k33_languageSerial", 0, 0, 0x7FFFFFFF, 0,
                                 "Counts completed live language switches: re-read text when it changes (read only)");
    /* The config ran before this dvar existed, so C1 kept its saved line as a plain string and the registration
     * adopted it; the client may have started in another language (command line, or the saved one is not installed).
     * The dvar names the language on screen: put it there through the command buffer, not as a request. */
    _snprintf(g_lsSeen, sizeof(g_lsSeen), "%s", g_lang);
    g_lsSeen[sizeof(g_lsSeen) - 1] = 0;
    g_lsWanted = g_lsStartup;
    char now[64];
    if (!C1DvarString("k33_language", now, sizeof(now)) || strcmp(now, g_lang) != 0) C1QueueSet("k33_language", g_lang);
    if (g_lsDvList && (!C1DvarString("k33_languageList", now, sizeof(now)) || strcmp(now, list) != 0))
        C1QueueSet("k33_languageList", list);
    LsPublish(LS_STATE_IDLE);
    Log("language: k33_language \"%s\" of \"%s\"; live switch %s", g_lang, list,
        g_lsInstalled ? "ready" : "unavailable (a change applies on restart)");
}

static void LrLoadZone(const char *name)
{
    static XZoneInfo info;
    lstrcpynA(g_lrName, name, sizeof(g_lrName));
    g_lrInPlace = g_lrChained = 0;
    info.name = g_lrName;
    info.allocFlags = 1;                                           /* a code zone: C1 never unloads it */
    info.freeFlags = 0;
    ((LoadXAssetsFn)(DWORD_PTR)DB_LOAD_XASSETS_VA)(&info, 1, 0);   /* queued for the database thread, as C1's own loads */
}

/* The holders get their own strings back, then C1 unloads the zone: entries, pool slots and the zone's memory.
 * C1 unloads by alloc-flag group and the zone is in the code zones' group, which nothing unloads. For the length of
 * this call the zone's flag (zone +0x4C) is LR_UNLOAD_FLAG, a group of one, and C1's "loaded flags" word has that
 * bit, as it would if the zone had been loaded with it. MEASURED: the unload runs inside the call, on this thread;
 * the zone is gone when it returns, its PMem block popped, and the loaded-flags bit is clear again. */
static void LrUnloadZone(void)
{
    XZoneInfo info = { NULL, 0, LR_UNLOAD_FLAG };
    int zone = g_lrName[0] ? LrFindZone(g_lrName) : -1;
    LrSweep(TRUE);
    if (g_lrIndex.slots) {
        memset(g_lrIndex.slots, 0, LR_INDEX_SLOTS * sizeof(LsSlot));
        g_lrIndex.count = 0;
    }
    g_lrName[0] = 0;
    if (zone < 0) return;
    volatile DWORD *flags = (volatile DWORD *)(DWORD_PTR)(LS_ZONES_VA + (unsigned)zone * LS_ZONE_SIZE + 0x4C);
    volatile DWORD *loaded = (volatile DWORD *)(DWORD_PTR)LR_LOADED_FLAGS_VA;
    *flags = LR_UNLOAD_FLAG;
    *loaded |= LR_UNLOAD_FLAG;
    InterlockedExchange(&g_lrUnloadZone, zone);
    ((LoadXAssetsFn)(DWORD_PTR)DB_LOAD_XASSETS_VA)(&info, 1, 0);
    InterlockedExchange(&g_lrUnloadZone, -1);
    *loaded &= ~(DWORD)LR_UNLOAD_FLAG;
}

/* The map load's zone list (0x4CD0F0). The first match of a session adds common_mp, patch_common_mp,
 * localized_common_mp and patch_localized_common_mp with flag 1 (0x4CCD80, only while common_mp is not loaded):
 * code zones that C1 then keeps for the session, on PMem stack 0 above the language zone. MEASURED: after that the
 * language zone is block 14 of 19 and an unload of it frees nothing (see the doc). So when that list carries a
 * flag-1 zone and the language zone is the top block of stack 0, the language zone is unloaded here (C1 has just
 * unloaded the previous map's groups at 0x4CCE6C, on this thread, synchronously) and put at the end of the list:
 * it loads right after them, as the top block again, and its strings go in place over theirs. */
static void __cdecl LrMapLoadHook(XZoneInfo *info, unsigned count, int sync)
{
    XZoneInfo all[40];
    LrStats s;
    char zone[64];
    BOOL code = FALSE;
    for (unsigned i = 0; info && i < count; i++) code |= info[i].allocFlags == 1;
    /* No idle test here: C1 itself has just unloaded groups 6 from this same function (0x4CCE6C) without one, and
     * MEASURED: C1's "idle" event is not set at this point although nothing is loading. */
    if (!code || g_lrNoReorder || !g_lsInstalled || !g_lrName[0] || g_lrPhase != LR_PHASE_IDLE || count >= 40) {
        if (code && g_lsInstalled && g_lrName[0])
            Log("language: a map load adds code zones above \"%s\"; it stays where it is (%s): a later switch applies on "
                "restart", g_lrName, g_lrNoReorder ? "loc_noReorder" : g_lrPhase != LR_PHASE_IDLE ? "a switch is running" :
                "list too long");
        ((LoadXAssetsFn)(DWORD_PTR)DB_LOAD_XASSETS_VA)(info, count, sync);
        return;
    }
    LrReadStats(&s);
    if (lstrcmpiA(s.top[0], g_lrName) != 0) {
        Log("language: \"%s\" is not the top block of stack 0 (\"%s\" is); left where it is", g_lrName, s.top[0]);
        ((LoadXAssetsFn)(DWORD_PTR)DB_LOAD_XASSETS_VA)(info, count, sync);
        return;
    }
    lstrcpynA(zone, g_lrName, sizeof(zone));
    LrUnloadZone();
    memcpy(all, info, count * sizeof(*info));
    lstrcpynA(g_lrName, zone, sizeof(g_lrName));
    g_lrInPlace = g_lrChained = 0;
    all[count].name = g_lrName;
    all[count].allocFlags = 1;
    all[count].freeFlags = 0;
    Log("language: the map load adds code zones; \"%s\" is unloaded and loads again after them (%u zones in the list)",
        zone, count + 1);
    ((LoadXAssetsFn)(DWORD_PTR)DB_LOAD_XASSETS_VA)(all, count + 1, sync);
}

__attribute__((unused)) static const char *LrGateText(enum LrGateAnswer a)
{
    switch (a) {
    case LR_GATE_NO_HOOKS: return "no live switch in this client";
    case LR_GATE_NO_OLD_STYLE: return "the startup language's zones are not one codol_<name>_locale zone";
    case LR_GATE_NO_REPLACE_MODE: return "C1's linker is replacing assets";
    case LR_GATE_NO_STACK: return "the language zone is not the top block of its memory stack";
    case LR_GATE_NO_ZONES: return "C1's zone table is full";
    case LR_GATE_NO_ENTRIES: return "too few free asset entries";
    case LR_GATE_NO_SLOTS: return "too few free localize slots";
    default: return "the database stayed busy";
    }
}

static enum LrGateAnswer LrGateNow(LrStats *s)
{
    LrGate g;
    LrReadStats(s);
    g.armed = g_lsInstalled;
    g.oldStyle = g_langOldStyle;
    g.dbIdle = LrDbIdle();
    g.replaceMode = *(volatile int *)(DWORD_PTR)LR_REPLACE_MODE_VA != 0;
    g.loaded = g_lrName[0] != 0;
    g.onTop = g.loaded && !lstrcmpiA(s->top[0], g_lrName);
#if K33_LOCAL_TESTING
    if (g_lrForce && g.loaded && !g.onTop) {
        g_lrForce = FALSE;
        g.onTop = TRUE;
        Log("language: loc_forceUnload: unloading \"%s\" although it is not the top block of its stack", g_lrName);
    }
#endif
    g.zones = (int)s->zones;
    g.zoneMax = LR_ZONE_LIMIT;
    g.freeEntries = s->entriesFree;
    g.needEntries = LR_NEED_ENTRIES;
    g.freeSlots = s->slotsFree;
    g.needSlots = LR_NEED_ENTRIES;
    return LrGateDecide(&g);
}

static void LrLogStats(const char *what, const LrStats *s)
{
    Log("language: %s: %u asset entries free of 59999, %u localize of %u, %u zones; stack 0: %u blocks, 0x%X, top "
        "\"%s\"; stack 1: %u blocks, 0x%X", what, s->entriesFree, s->localize, g_lrPoolSlots, s->zones, s->stackCount[0],
        s->stackPos[0], s->top[0], s->stackCount[1], s->stackPos[1]);
    (void)what; (void)s;
}

/* A request that was accepted as live, every frame until it is done. */
static void LrSwitchTick(void)
{
    LrStats s;
    char zone[64];
    if (g_lrPhase == LR_PHASE_WAIT) {
        enum LrGateAnswer a = LrGateNow(&s);
        if (a == LR_GATE_WAIT && GetTickCount() - g_lrSince < LR_WAIT_MS) return;
        if (a != LR_GATE_GO) {
            g_lrPhase = LR_PHASE_IDLE;
            LsPublish(LS_STATE_RESTART);
            Log("language: \"%s\" saved, applies on restart (%s); \"%s\" stays on screen", g_langs[g_lrTarget], LrGateText(a),
                g_lsShown >= 0 ? g_langs[g_lsShown] : g_lang);
            return;
        }
        LrLogStats("before the switch", &s);
        LrUnloadZone();
        LrZoneOf(g_lrTarget, zone);
        if (zone[0]) LrLoadZone(zone);
        g_lrPhase = LR_PHASE_LOADING;
        g_lrSince = GetTickCount();
        return;
    }
    if (!LrDbIdle()) {
        if (GetTickCount() - g_lrSince < LR_WAIT_MS) return;
        Log("language: the database did not finish loading \"%s\" within %u s", g_lrName, LR_WAIT_MS / 1000);
    }
    /* LR_PHASE_LOADING, the load is in (or the language has no zone: C1's own text) */
    LrZoneOf(g_lrTarget, zone);
    BOOL in = !zone[0] || LrFindZone(zone) >= 0;
    unsigned late = in ? LrSweep(FALSE) : 0;
    g_lrZoneSig = LrZoneSignature();
    LrReadStats(&s);
    g_lrPhase = LR_PHASE_IDLE;
    if (!in) {
        /* nothing of the old language is left and the new one did not load: C1's own text is on screen */
        g_lrName[0] = 0;
        LsPublish(LS_STATE_RESTART);
        Log("language: zone \"%s\" did not load; C1's own text is on screen and \"%s\" applies on restart", zone,
            g_langs[g_lrTarget]);
        return;
    }
    const char *from = g_lsShown >= 0 ? g_langs[g_lsShown] : g_lang;
    (void)from; (void)late;
    InterlockedExchange(&g_lsShown, g_lrTarget);
    g_lsSerial++;
    LsPublish(LS_STATE_IDLE);
    Log("language: \"%s\" -> \"%s\" live (serial %d, %lu ms): %ld string(s) in place, %ld also chained, %u after the "
        "load%s", from, g_langs[g_lrTarget], g_lsSerial, (unsigned long)(GetTickCount() - g_lrSince), (long)g_lrInPlace,
        (long)g_lrChained, late, g_lrDropped ? " (index full: some strings are not re-applied after a zone unload)" : "");
    LrLogStats("after the switch", &s);
}

/* Main thread, every frame: a changed k33_language is a request. */
static void LocaleSwitchTick(void)
{
    if (!g_lsDvarsDone) {
        if (C1DvarsReady()) LsRegisterDvars();
        return;
    }
    if (g_lrPhase == LR_PHASE_STARTUP) {
        /* the startup zones are in once the code_post_gfx list was handed over and the database is idle */
        if (!g_lsInstalled || (!g_postGfxSubmitted && g_postGfxHooked) || !LrDbIdle() ||
            LrFindZone("localized_code_post_gfx_mp") < 0) {
            if (!g_lsInstalled) g_lrPhase = LR_PHASE_IDLE;
            return;
        }
        if (g_lrName[0] && LrFindZone(g_lrName) < 0) {
            /* the code_post_gfx list was not this DLL's to extend (its call site differs): load the zone now */
            char zone[64];
            lstrcpynA(zone, g_lrName, sizeof(zone));
            Log("language: zone \"%s\" was not loaded with the startup zones; loading it now", zone);
            g_lrTarget = g_lsStartup;
            LrLoadZone(zone);
            g_lrPhase = LR_PHASE_LOADING;
            g_lrSince = GetTickCount();
            return;
        }
        LrStats s;
        unsigned late = g_lrName[0] ? LrSweep(FALSE) : 0;
        g_lrZoneSig = LrZoneSignature();
        g_lrPhase = LR_PHASE_IDLE;
        LrReadStats(&s);
        (void)late;
        Log("language: \"%s\" zone \"%s\" %s: %ld string(s) in place, %ld also chained, %u after the load", g_lang, g_lrName,
            !g_lrName[0] ? "(none: C1's own text)" : LrFindZone(g_lrName) >= 0 ? "loaded" : "DID NOT LOAD",
            (long)g_lrInPlace, (long)g_lrChained, late);
        LrLogStats("startup", &s);
        return;
    }
    if (g_lrPhase != LR_PHASE_IDLE) {
        LrSwitchTick();
        return;
    }
    /* C1 loaded or unloaded zones since the last look (a map, the UI): a holder may be gone and another zone's
     * string promoted into its slot. Put the language's back. */
    if (g_lsInstalled && g_lrName[0] && LrDbIdle()) {
        DWORD sig = LrZoneSignature();
        if (sig != g_lrZoneSig) {
            unsigned late = LrSweep(FALSE);
            g_lrZoneSig = sig;
            if (late) Log("language: %u string(s) put back in place after C1 changed zones", late);
        }
    }
    char now[64], lang[LANG_NAME_MAX];
    if (!C1DvarString("k33_language", now, sizeof(now))) return;
#if K33_LOCAL_TESTING
    if (!strcmp(now, g_lsSeen) && g_lrCycles > 0 && g_lsState == LS_STATE_IDLE && g_langCount > 1) {
        g_lrCycles--;
        C1QueueSet("k33_language", g_langs[(g_lsShown + 1) % g_langCount]);
        return;
    }
#endif
    if (!strcmp(now, g_lsSeen)) return;
    _snprintf(g_lsSeen, sizeof(g_lsSeen), "%s", now);
    g_lsSeen[sizeof(g_lsSeen) - 1] = 0;
    int request = LanguageCanonical(now, lang) ? LanguageListFind(g_langs, g_langCount, lang) : -1;
    if (request >= 0 && strcmp(now, g_langs[request]) != 0) {   /* "English", "ZH": store the list's spelling */
        C1QueueSet("k33_language", g_langs[request]);
        _snprintf(g_lsSeen, sizeof(g_lsSeen), "%s", g_langs[request]);
    }
    LsWorld w = { g_lsInstalled, g_langCount, (int)g_lsShown, LsReadyMask() };
    const char *shown = w.shown >= 0 ? g_langs[w.shown] : g_lang;
    (void)shown;   /* only logged (Log is compiled out of shipping builds) */
    switch (LsDecide(&w, request)) {
    case LS_ACT_REVERT: {
        const char *back = g_lsWanted >= 0 ? g_langs[g_lsWanted] : g_lang;
        Log("language: \"%.32s\" is not an installed language; k33_language stays \"%s\"", now, back);
        C1QueueSet("k33_language", back);
        _snprintf(g_lsSeen, sizeof(g_lsSeen), "%s", back);
        break;
    }
    case LS_ACT_NONE:
        g_lsWanted = request;
        if (g_lsState != LS_STATE_IDLE) Log("language: \"%s\" again, nothing left for the next start", shown);
        LsPublish(LS_STATE_IDLE);
        break;
    case LS_ACT_LIVE:
        g_lsWanted = request;
        g_lrTarget = request;
        g_lrPhase = LR_PHASE_WAIT;
        g_lrSince = GetTickCount();
        LsPublish(LS_STATE_SWITCHING);
        LrSwitchTick();                               /* an idle database: unload and queue the load in this frame */
        break;
    case LS_ACT_RESTART:
        g_lsWanted = request;
        LsPublish(LS_STATE_RESTART);
        Log("language: \"%s\" saved, applies on restart (%s); \"%s\" stays on screen", g_langs[request],
            !g_lsInstalled ? "no live switch in this client" : "its strings are not one codol_<name>_locale zone", shown);
        break;
    }
}

/* k33_setlanguage [name]: sets k33_language through the command buffer; without a name, says where things are. */
static void __cdecl LsCmdSetLanguage(void)
{
    char list[LANG_MAX * LANG_NAME_MAX], lang[LANG_NAME_MAX];
    LanguageListJoin(g_langs, g_langCount, list, sizeof(list));
    if (VoiceArgc() < 2) {
        LONG shown = g_lsShown;
        C1Printf("k33_setlanguage <%s>: \"%s\" on screen, state %d, serial %d\n", list,
                 shown >= 0 ? g_langs[shown] : g_lang, g_lsState, g_lsSerial);
        return;
    }
    if (!LanguageCanonical(VoiceArgv(1), lang) || LanguageListFind(g_langs, g_langCount, lang) < 0) {
        C1Printf("k33_setlanguage: not an installed language (%s)\n", list);
        return;
    }
    if (!C1QueueSet("k33_language", lang)) C1Printf("k33_setlanguage: the command buffer is full\n");
}

static C1Cmd g_lsCmd = { NULL, "k33_setlanguage", (void *)LsCmdSetLanguage };

#if K33_LOCAL_TESTING
/* Test builds: "loc_tableStats [text]" logs the tables' use; "loc_cycle <n>" switches to the next language n times,
 * each when the one before is done. */
static void __cdecl LrCmdStat(void)
{
    LrStats s;
    LrReadStats(&s);
    LrLogStats(VoiceArgc() > 1 ? VoiceArgv(1) : "now", &s);
}

static void __cdecl LrCmdCycle(void)
{
    g_lrCycles = VoiceArgc() > 1 ? atoi(VoiceArgv(1)) : 0;
    Log("language: %d test switch(es) queued", g_lrCycles);
}

static C1Cmd g_lrCmdStat = { NULL, "loc_tableStats", (void *)LrCmdStat };
static C1Cmd g_lrCmdCycle = { NULL, "loc_cycle", (void *)LrCmdCycle };
static void __cdecl LrCmdForce(void) { g_lrForce = TRUE; Log("language: loc_forceUnload armed"); }
static C1Cmd g_lrCmdForce = { NULL, "loc_forceUnload", (void *)LrCmdForce };
#endif

/* After PatchPatchDir (the language list, the zones) and before K33InstallMainTicks. */
static void InstallLocaleSwitch(void)
{
    g_lsStartup = LanguageListFind(g_langs, g_langCount, g_lang);
    g_lsShown = g_lsStartup;
    if (!K33OnMainTick(LocaleSwitchTick)) {
        Log("language: no main-thread tick left; k33_language is not registered (command line and saved choice only)");
        return;
    }
    C1AddCommand(&g_lsCmd);
#if K33_LOCAL_TESTING
    C1AddCommand(&g_lrCmdStat);
    C1AddCommand(&g_lrCmdCycle);
    C1AddCommand(&g_lrCmdForce);
    {
        const char *cl = GetCommandLineA();
        g_lrNoReorder = cl && strstr(cl, "loc_noReorder 1") != NULL;
    }
#endif
    if (!g_langCount) {
        Log("language: no language zone in the patch folder; \"%s\" is all there is", g_lang);
        return;
    }
    if (!g_langReload) return;             /* LoadPatchDir queued the zones the old way and said why */
    DWORD pool = *(volatile DWORD *)(DWORD_PTR)LS_POOL_VA;
    int size = *(volatile int *)(DWORD_PTR)LS_POOL_SIZE_VA;
    BYTE *block = size > 0 && size <= 0x10000 && pool
                      ? (BYTE *)VirtualAlloc(NULL, (SIZE_T)size * 2 * sizeof(char *) + LR_INDEX_SLOTS * sizeof(LsSlot),
                                             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) : NULL;
    if (!block) {
        /* the zones are queued for this mechanism already: without the hook they still load and work as plain
         * override zones would not, so this is fatal for the feature only, and said loudly */
        Log("language: no memory for the slot table (%lu); THE LANGUAGE ZONE WILL NOT SHOW", GetLastError());
        g_langReload = FALSE;
        return;
    }
    g_lrPoolFirst = pool + 4;
    g_lrPoolSlots = (unsigned)size;
    g_lrStock = (const char **)block;
    g_lrMine = g_lrStock + size;
    LsTableInit(&g_lrIndex, (LsSlot *)(g_lrMine + size), LR_INDEX_SLOTS);
    LrZoneOf(g_lsStartup, g_lrName);       /* kernel33.c queued it after the code_post_gfx zones */
    {
        BYTE call[6] = { 0xE8, 0, 0, 0, 0, 0x90 };
        LONG rel = (LONG)((DWORD_PTR)LR_LinkHook - (LR_LINK_CMP_VA + 5));
        memcpy(call + 1, &rel, 4);
        WriteCode((void *)(DWORD_PTR)LR_LINK_CMP_VA, call, sizeof(call));
        BYTE jump[6] = { 0xE9, 0, 0, 0, 0, 0x90 };
        rel = (LONG)((DWORD_PTR)LR_UnloadHook - (LR_UNLOAD_ZONE_VA + 5));
        memcpy(jump + 1, &rel, 4);
        WriteCode((void *)(DWORD_PTR)LR_UNLOAD_ZONE_VA, jump, sizeof(jump));
        RedirectCall(LR_MAP_LOAD_CALL_VA, (void *)LrMapLoadHook);
    }
    g_lsInstalled = TRUE;
    Log("language: live switch installed (link hook 0x%08X, unload hook 0x%08X); \"%s\" is zone \"%s\"", LR_LINK_CMP_VA,
        LR_UNLOAD_ZONE_VA, g_lang, g_lrName);
}

/*
 * locale_switch.c - the language, changed while the game runs (k33_language; docs/LANGUAGE_SWITCH_20261112.md).
 * Included by kernel33.c after voice.c (its command arguments) and k33tick.c; the pure part is locale_switch_policy.h.
 *
 * What a language is: which string a localize name (asset type 27) resolves to. C1 resolves every one through
 * DB_FindXAssetHeader(27, name) 0x4CB8E0, called from eleven sites (kLsFindSites: the plain lookup 0x5DA380, the
 * safe translate 0x5F49E0, eight UI text builders 0x5FA7xx-0x5FCDxx and the Scaleform translator 0x6A5AC0, which
 * first asks DB_IsXAssetDefault-style 0x4CB850 whether the name exists). MEASURED: capstone over C1.exe, every
 * call of 0x4CB8E0 / 0x4CB850 that follows "push 1Bh"; all eleven read the result as LocalizeEntry {value, name}
 * (mov eax, [eax]) at once and keep no pointer to the entry.
 *
 * Why the strings are not swapped inside C1's own pool (what was asked for first, and what this file does not do):
 *   - MEASURED (a running 1.1.1.13 client and its crash dumps, read only): C1's asset entry table 0x1C402C8 has
 *     59,999 usable 24-byte entries for every asset of every type (free list 0x18779BC, built at
 *     0x4CB26B-0x4CB294; uint16 links, so it cannot grow past 65,535). An English client uses 40,273 of them at
 *     the front end, 47,530 in a match on mp_killhouse_sh (a small map) and 46,980 at the front end after it.
 *     The localize pool was never the limit: this table is.
 *   - A language can only be swapped in C1's pool if both languages have entries. English at startup displaces
 *     the stock string of every name both have (up to 7,739 of codol_english's 9,705; the other 1,966 are names
 *     C1 does not have), and C1's linker discards the displaced one (0x4CC621: same alloc flags, the first zone
 *     keeps the name): the Chinese text is not in memory, and the stock zones cannot be read a second time.
 *     Keeping them (the locale zone under an alloc flag of its own, so the stock strings chain as overrides at
 *     0x4CC648) costs up to those 7,739 entries in every English client, switched or not: about 55,300 of 59,999
 *     in that small match. INFERRED: a larger map or mode then ends in "exceeded limit" (0x4CB2DF, fatal); no
 *     larger match was measured, and that is not a margin to ship on.
 *   - Unloading a zone frees its memory (PMem_Free 0x8373109, virtualized: not readable; the zone list of a
 *     PMem stack has 32 entries and no bounds check, 0x4CD6C8-0x4CD6EA) while C1 hands out raw char pointers
 *     into it (0x5DA3B2), and the English zone also carries scaleform/fonts_ingame.gfx.
 * So nothing is unloaded and nothing in C1's pool is rewritten. Instead:
 *
 *   1. Every other language's codol_<name>_locale zone is loaded once, at startup, after the code_post_gfx zones
 *      (kernel33.c queues it as a PATCH_POST_GFX zone; alloc flag 1 like every code zone). By then most names it
 *      carries are held by a stock zone or by the startup language, so C1's linker discards those strings as
 *      duplicates: no asset entry, no pool slot. The discard is the call at 0x4CC639 (free the new entry, esi;
 *      ebx = the entry that keeps the name); LS_DiscardHook copies the string into this DLL's table for that
 *      language first.
 *   2. The same hook keeps the stock strings that a language's zone displaced (the startup language's at startup;
 *      a second language's when common_mp and the like load later with the code zones' flag), in one table: the
 *      text C1 would show without that zone.
 *   3. A name of a second language that no zone held yet becomes an ordinary C1 entry (a few English-only keys,
 *      and names of zones that load later: UI, gametype, map). So that such an entry is never shown in another
 *      language, the entries that belong to a language's zone are mapped once (LsMapOwners: pool slot -> entry;
 *      language zones are permanent, so their entries and slots never move).
 *   4. The eleven lookups go through LsFindHook: the shown language's table (second languages only), else C1's
 *      entry; when that entry belongs to a language that is not the one shown, the displaced stock string, else
 *      the stock entry chained under it (a zone with other alloc flags, 0x4CC648), else nothing. That is what a
 *      client started in the shown language resolves, name for name. For a client that never switches, an entry
 *      of the startup language or of a stock zone is returned as C1 returned it: two array reads more than before.
 * A switch is one store of the language index: it cannot be half done, needs no idle database and works in a
 * match. The tables are insert-only and their strings are never freed, so a pointer C1 was given stays valid.
 *
 * Not changed by a switch: the font movie (fonts_ingame.gfx of the zone that loaded first stays until restart),
 * and text a movie or the HUD already holds (the translator hands Scaleform a copy): a movie re-reads its text
 * when k33_languageSerial changes.
 */
#define LS_PUBLISH(dst, src) InterlockedExchangePointer((PVOID volatile *)(dst), (PVOID)(src))
#include "locale_switch_policy.h"

#define LS_LOCALIZE          27
#define LS_FIND_VA           0x004CB8E0u   /* XAssetHeader __cdecl DB_FindXAssetHeader(int type, const char *name) */
#define LS_EXISTS_VA         0x004CB850u   /* bool __cdecl (int type, const char *name): the translator's "is there one" */
#define LS_EXISTS_CALL_VA    0x006A5AFAu
#define LS_DISCARD_CALL_VA   0x004CC639u   /* DB_LinkXAssetEntry: call 0x4CB320 (free the duplicate, esi) */
#define LS_FREE_ENTRY_VA     0x004CB320u
#define LS_ZONES_VA          0x01F0E108u   /* 0xA8 bytes each: name +0xC, alloc flags +0x4C */
#define LS_ZONE_SIZE         0xA8u
#define LS_ZONE_MAX          35
#define LS_ZONE_COUNT_VA     0x018FCFBCu
#define LS_ZONE_ORDER_VA     0x01C402A4u   /* zone indices in load order */
#define LS_ENTRIES_VA        0x01C402C8u   /* 24 bytes each: type +0, header +8, zone +0x10, next +0x12, override +0x14 */
#define LS_ENTRY_MAX         60040u
#define LS_HASH_VA           0x018FD9D8u   /* 60000 uint16 buckets */
#define LS_HASH_SIZE         60000u
#define LS_DB_LOCK_VA        0x02007A5Cu   /* {readers, writers}: DB_FindXAssetHeader's read lock, 0x4CB953-0x4CB990 */
#define LS_DB_THREAD_VA      0x02609060u
#define LS_DB_READY_VA       0x026090BCu   /* event handle: nothing loading and the results taken (tested at 0x4CC8D7) */
#define LS_POOL_VA           (0x00CD5028u + 4 * LS_LOCALIZE)   /* the localize pool block {free head, entries[]} */
#define LS_POOL_SIZE_VA      (0x00CD4D68u + 4 * LS_LOCALIZE)
#define LS_REGISTER_STRING_VA 0x00624740u  /* Dvar_RegisterString(name, value, flags, description), as voice.c calls it */
#define LS_TABLE_SLOTS       32768u        /* per language: 24,576 strings (English has 10,154) */

static const DWORD kLsFindSites[] = { 0x005DA3A6u, 0x005F4A13u, 0x005FA7C2u, 0x005FA965u, 0x005FAAFBu, 0x005FACABu,
                                      0x005FAED5u, 0x005FC8A6u, 0x005FCB1Du, 0x005FCD87u, 0x006A5B23u };

static int g_lsCheck = -1;                 /* -1 not looked at, 0 a site differs, 1 every site is C1's */
static BOOL g_lsInstalled;
static volatile LONG g_lsShown = -1;       /* index in g_langs of the language on screen */
static int g_lsStartup = -1;               /* index of the language the client started in (-1: not in the list) */
static LsTable g_lsTables[CODOL_LANG_MAX], g_lsStock;
static volatile LONG g_lsWriter, g_lsDropped;
static const WORD *volatile g_lsOwner;     /* per localize pool slot: the entry of a language's zone, else 0 */
static const signed char *g_lsOwnerLang;   /* and that zone's language */
static DWORD g_lsPoolFirst, g_lsPoolBytes; /* the pool's first entry and its size in bytes */
static int g_lsOwned;
__attribute__((used)) DWORD g_lsFreeEntry = LS_FREE_ENTRY_VA;

static const BYTE *volatile g_lsDvLanguage, *volatile g_lsDvList, *volatile g_lsDvState, *volatile g_lsDvSerial;
static BOOL g_lsDvarsDone;
static int g_lsWanted = -1;                /* the last request that was accepted (on screen, or saved for restart) */
static char g_lsSeen[64];                  /* k33_language as last handled */
static int g_lsState, g_lsSerial;
static BOOL g_lsMapFailed;

/* Called by LoadPatchDir before anything is queued, and again by InstallLocaleSwitch: bytes only, nothing written. */
static BOOL LocaleSwitchCheck(void)
{
    static const BYTE kFind[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x51, 0x53, 0x8B, 0x1D, 0x50, 0xD3, 0xA1, 0x09 };
    static const BYTE kExists[] = { 0x55, 0x8B, 0xEC, 0x80, 0x3D, 0x9A, 0x55, 0x06, 0x02, 0x00, 0x56 };
    /* 0x4CC623: mov edx,[esi] ; mov eax,[edx*4+0xCD5238] ; test eax,eax ; je +9 ; mov ecx,[esi+8] ; push ecx ;
     * call eax ; add esp,4 ; (0x4CC639) call 0x4CB320 ; mov edx,[ebx+8] */
    static const BYTE kDiscard[] = { 0x8B, 0x16, 0x8B, 0x04, 0x95, 0x38, 0x52, 0xCD, 0x00, 0x85, 0xC0, 0x74, 0x09,
                                     0x8B, 0x4E, 0x08, 0x51, 0xFF, 0xD0, 0x83, 0xC4, 0x04 };
    static const BYTE kAfterDiscard[] = { 0x8B, 0x53, 0x08 };
    /* 0x4CB320: mov ecx,[esi+8] ; mov eax,[esi] ; mov edx,[eax*4+0xCD4F78] */
    static const BYTE kFree[] = { 0x8B, 0x4E, 0x08, 0x8B, 0x06, 0x8B, 0x14, 0x85, 0x78, 0x4F, 0xCD, 0x00 };
    static const BYTE kCleanup[] = { 0x83, 0xC4, 0x08, 0x85, 0xC0 };          /* add esp,8 ; test eax,eax: NULL is handled */
    static const BYTE kExistsCleanup[] = { 0x83, 0xC4, 0x08, 0x84, 0xC0 };    /* add esp,8 ; test al,al */
    DWORD bad = 0;
    if (g_lsCheck >= 0) return g_lsCheck;
#define LS_SAME(va, bytes) (Readable((void *)(DWORD_PTR)(va), sizeof(bytes)) && !memcmp((void *)(DWORD_PTR)(va), bytes, sizeof(bytes)))
    if (!LS_SAME(LS_FIND_VA, kFind)) bad = LS_FIND_VA;
    else if (!LS_SAME(LS_EXISTS_VA, kExists)) bad = LS_EXISTS_VA;
    else if (!LS_SAME(LS_FREE_ENTRY_VA, kFree)) bad = LS_FREE_ENTRY_VA;
    else if (!LS_SAME(LS_DISCARD_CALL_VA - sizeof(kDiscard), kDiscard) || !CallsTarget(LS_DISCARD_CALL_VA, LS_FREE_ENTRY_VA) ||
             !LS_SAME(LS_DISCARD_CALL_VA + 5, kAfterDiscard)) bad = LS_DISCARD_CALL_VA;
    else if (!Readable((void *)(DWORD_PTR)(LS_EXISTS_CALL_VA - 2), 2) ||
             ((const BYTE *)(DWORD_PTR)LS_EXISTS_CALL_VA)[-2] != 0x6A || ((const BYTE *)(DWORD_PTR)LS_EXISTS_CALL_VA)[-1] != LS_LOCALIZE ||
             !CallsTarget(LS_EXISTS_CALL_VA, LS_EXISTS_VA) || !LS_SAME(LS_EXISTS_CALL_VA + 5, kExistsCleanup)) bad = LS_EXISTS_CALL_VA;
    else if (!Readable((void *)(DWORD_PTR)LS_ZONES_VA, LS_ZONE_MAX * LS_ZONE_SIZE) ||
             !Readable((void *)(DWORD_PTR)LS_ENTRIES_VA, LS_ENTRY_MAX * 24) ||
             !Readable((void *)(DWORD_PTR)LS_HASH_VA, LS_HASH_SIZE * 2) || !Readable((void *)(DWORD_PTR)LS_DB_LOCK_VA, 8) ||
             !Readable((void *)(DWORD_PTR)LS_DB_THREAD_VA, 4) || !Readable((void *)(DWORD_PTR)LS_DB_READY_VA, 4) ||
             !Readable((void *)(DWORD_PTR)LS_POOL_VA, 4) || !Readable((void *)(DWORD_PTR)LS_POOL_SIZE_VA, 4)) bad = LS_ZONES_VA;
    for (unsigned i = 0; !bad && i < sizeof(kLsFindSites) / sizeof(kLsFindSites[0]); i++) {
        const BYTE *call = (const BYTE *)(DWORD_PTR)kLsFindSites[i];
        if (!Readable(call - 2, 2) || call[-2] != 0x6A || call[-1] != LS_LOCALIZE ||
            !CallsTarget(kLsFindSites[i], LS_FIND_VA) || !LS_SAME(kLsFindSites[i] + 5, kCleanup))
            bad = kLsFindSites[i];
    }
#undef LS_SAME
    g_lsCheck = !bad;
    if (bad)
        Log("language: bytes at 0x%08lX differ; no live switch (k33_language is saved and applies on restart)",
            (unsigned long)bad);
    return g_lsCheck;
}

/* The language (index in g_langs) of the zone in slot `zone`, -1 for a zone of no language. Any thread. */
static int LsZoneLanguage(unsigned zone)
{
    char name[64], lang[CODOL_LANG_NAME_MAX];
    if (zone >= LS_ZONE_MAX) return -1;
    memcpy(name, (const char *)(DWORD_PTR)(LS_ZONES_VA + zone * LS_ZONE_SIZE + 0xC), sizeof(name));
    name[sizeof(name) - 1] = 0;
    if (!CodolPatchZoneLanguage(name, lang)) return -1;
    return CodolLanguageListFind(g_langs, g_langCount, lang);
}

static char *LsCopy(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)HeapAlloc(GetProcessHeap(), 0, n);
    if (p) memcpy(p, s, n);
    return p;
}

/* The thread that links the zone (C1's database thread). esi / ebx of 0x4CC639: the duplicate about to be freed
 * and the entry that keeps its name. Strings are copied: the table does not depend on any zone staying loaded. */
__attribute__((used)) void __cdecl LsCapture(const BYTE *fresh, const BYTE *holder)
{
    if (*(const DWORD *)fresh != LS_LOCALIZE) return;
    const char *const *entry = *(const char *const *const *)(fresh + 8);
    if (!entry || !entry[0] || !entry[1]) return;
    int route = LsCaptureRoute(LsZoneLanguage(fresh[0x10]), LsZoneLanguage(holder[0x10]), g_lsStartup);
    if (route == LS_ROUTE_NONE) return;
    LsTable *t = route == LS_ROUTE_STOCK ? &g_lsStock : &g_lsTables[route];
    while (InterlockedExchange(&g_lsWriter, 1)) Sleep(0);
    if (!t->slots) {
        LsSlot *slots = (LsSlot *)VirtualAlloc(NULL, LS_TABLE_SLOTS * sizeof(LsSlot), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (slots) LsTableInit(t, slots, LS_TABLE_SLOTS);   /* zeroed pages: a reader that sees it finds nothing yet */
    }
    if (t->slots && !LsFind(t, entry[1])) {
        char *name = LsCopy(entry[1]), *value = LsCopy(entry[0]);
        if (!name || !value || LsInsert(t, name, value) != 1) InterlockedIncrement(&g_lsDropped);
    } else if (!t->slots) {
        InterlockedIncrement(&g_lsDropped);
    }
    InterlockedExchange(&g_lsWriter, 0);
}

void LS_DiscardHook(void);
__asm__(
    ".text\n"
    ".globl _LS_DiscardHook\n"
    "_LS_DiscardHook:\n"              /* the call at 0x4CC639: esi = the duplicate, ebx = the entry that keeps the name */
    "    pushal\n"
    "    pushfl\n"
    "    pushl %ebx\n"
    "    pushl %esi\n"
    "    call _LsCapture\n"
    "    addl $8, %esp\n"
    "    popfl\n"
    "    popal\n"
    "    jmp *_g_lsFreeEntry\n");     /* C1's free, with its registers and its return address */

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

/* Main thread, once, when the database is idle after the startup zones: every localize entry whose zone is a
 * language's, by pool slot. Those zones have alloc flag 1 and are never unloaded, an entry that holds a name stays
 * the head of its override chain (0x4CC648 chains later zones behind it), and its pool slot is its own until the
 * zone goes, so the map stays true for the whole session. */
static BOOL LsMapOwners(void)
{
    HANDLE ready = *(HANDLE volatile *)(DWORD_PTR)LS_DB_READY_VA;
    if (!ready || WaitForSingleObject(ready, 0) != WAIT_OBJECT_0) return FALSE;
    DWORD pool = *(volatile DWORD *)(DWORD_PTR)LS_POOL_VA;
    int size = *(volatile int *)(DWORD_PTR)LS_POOL_SIZE_VA;
    if (!pool || size < 1 || size > 0x10000 || !Readable((void *)(DWORD_PTR)pool, 4 + (SIZE_T)size * 8)) return FALSE;
    BYTE *block = (BYTE *)VirtualAlloc(NULL, (SIZE_T)size * 3, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!block) return FALSE;
    WORD *owner = (WORD *)block;
    signed char *lang = (signed char *)(block + (SIZE_T)size * 2);
    const BYTE *entries = (const BYTE *)(DWORD_PTR)LS_ENTRIES_VA;
    const WORD *hash = (const WORD *)(DWORD_PTR)LS_HASH_VA;
    int owned = 0;
    LsReadLock();
    for (unsigned b = 0; b < LS_HASH_SIZE; b++) {
        unsigned idx = hash[b];
        for (unsigned guard = 0; idx && idx < LS_ENTRY_MAX && guard < LS_ENTRY_MAX; guard++) {
            const BYTE *e = entries + idx * 24;
            if (*(const DWORD *)e == LS_LOCALIZE) {
                int l = LsZoneLanguage(e[0x10]);
                DWORD off = *(const DWORD *)(e + 8) - (pool + 4);
                if (l >= 0 && off < (DWORD)size * 8 && !(off & 7)) {
                    owner[off >> 3] = (WORD)idx;
                    lang[off >> 3] = (signed char)l;
                    owned++;
                }
            }
            idx = *(const WORD *)(e + 0x12);
        }
    }
    LsReadUnlock();
    g_lsPoolFirst = pool + 4;
    g_lsPoolBytes = (DWORD)size * 8;
    g_lsOwnerLang = lang;
    g_lsOwned = owned;
    InterlockedExchangePointer((PVOID volatile *)&g_lsOwner, owner);
    return TRUE;
}

/* The header of the first-linked entry of a zone of no language chained under `e` (the last of the chain: C1 puts
 * each later zone's entry right behind the holder, 0x4CC648-0x4CC66C), NULL when there is none. */
static void *LsChainedStock(const BYTE *e)
{
    const BYTE *entries = (const BYTE *)(DWORD_PTR)LS_ENTRIES_VA;
    void *found = NULL;
    LsReadLock();
    unsigned ov = *(const WORD *)(e + 0x14);
    for (unsigned guard = 0; ov && ov < LS_ENTRY_MAX && guard < 64; guard++) {
        const BYTE *o = entries + ov * 24;
        if (LsZoneLanguage(o[0x10]) < 0 && *(void *const *)(o + 8)) found = *(void *const *)(o + 8);
        ov = *(const WORD *)(o + 0x14);
    }
    LsReadUnlock();
    return found;
}

__attribute__((used)) void *__cdecl LsFindHook(int type, const char *name)
{
    typedef void *(__cdecl *FindFn)(int, const char *);
    FindFn find = (FindFn)(DWORD_PTR)LS_FIND_VA;
    const WORD *owner = g_lsOwner;
    /* the database thread links under the write lock and asks for no text: it gets C1's function and nothing else */
    if (type != LS_LOCALIZE || !name || !owner || GetCurrentThreadId() == *(volatile DWORD *)(DWORD_PTR)LS_DB_THREAD_VA)
        return find(type, name);
    LONG shown = g_lsShown;
    LsSlot *own = LsOwnTable(g_lsTables, g_langCount, shown, g_lsStartup, name);
    if (own) return own;
    void *header = find(type, name);
    DWORD off = (DWORD)(DWORD_PTR)header - g_lsPoolFirst;
    if (!header || off >= g_lsPoolBytes || (off & 7) || !owner[off >> 3]) return header;
    if (!LsEntryForeign(g_lsOwnerLang[off >> 3], shown)) return header;
    return LsForeignAnswer(&g_lsStock, name, LsChainedStock((const BYTE *)(DWORD_PTR)LS_ENTRIES_VA + owner[off >> 3] * 24));
}

__attribute__((used)) int __cdecl LsExistsHook(int type, const char *name)
{
    typedef int(__cdecl *ExistsFn)(int, const char *);
    if (type == LS_LOCALIZE && name && g_lsOwner && LsOwnTable(g_lsTables, g_langCount, g_lsShown, g_lsStartup, name))
        return 1;
    return ((ExistsFn)(DWORD_PTR)LS_EXISTS_VA)(type, name);
}

/* Language i can be shown now: the startup language always; another one once the language zones' entries are
 * mapped and its zone's strings were captured; zh without a zone of its own is C1's stock text, which the
 * displaced-strings table and C1 already hold. */
static unsigned LsReadyMask(void)
{
    unsigned ready = 0;
    for (int i = 0; i < g_langCount; i++)
        if (i == g_lsStartup ||
            (g_lsOwner && ((g_lsTables[i].slots && g_lsTables[i].count) ||
                           (!strcmp(g_langs[i], "zh") && !(g_langZones >> i & 1u)))))
            ready |= 1u << i;
    return ready;
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

static void LsRegisterDvars(void)
{
    typedef const BYTE *(__cdecl *RegStr)(const char *, const char *, unsigned, const char *);
    RegStr regs = (RegStr)(DWORD_PTR)LS_REGISTER_STRING_VA;
    /* push ebp ; mov ebp,esp ; and esp,-8 ; sub esp,10h ; mov edx,[ebp+14h] ; mov ecx,[ebp+0Ch] */
    static const BYTE kRegStr[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x10, 0x8B, 0x55, 0x14, 0x8B, 0x4D, 0x0C };
    char list[CODOL_LANG_MAX * CODOL_LANG_NAME_MAX];
    g_lsDvarsDone = TRUE;
    if (!Readable((void *)(DWORD_PTR)LS_REGISTER_STRING_VA, sizeof(kRegStr)) ||
        memcmp((void *)(DWORD_PTR)LS_REGISTER_STRING_VA, kRegStr, sizeof(kRegStr)) != 0) {
        Log("language: Dvar_RegisterString 0x%08X differs; k33_language is not registered", LS_REGISTER_STRING_VA);
        return;
    }
    CodolLanguageListJoin(g_langs, g_langCount, list, sizeof(list));
    g_lsDvLanguage = regs("k33_language", g_lang, 0x1,
                          "Language: a name of k33_languageList. Changing it switches the game's text now when "
                          "k33_languageState stays 0, else on the next start (2)");
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

/* Main thread, every frame: a changed k33_language is a request. */
static void LocaleSwitchTick(void)
{
    if (!g_lsDvarsDone) {
        if (C1DvarsReady()) LsRegisterDvars();
        return;
    }
    /* once the startup zones are in: the code_post_gfx list was handed over (or is not hooked: nothing more comes) */
    if (g_lsInstalled && !g_lsOwner && !g_lsMapFailed && (g_postGfxSubmitted || !g_postGfxHooked)) {
        static DWORD since;
        if (!since) since = GetTickCount() | 1;
        if (LsMapOwners()) {
            unsigned second = 0;
            for (int i = 0; i < g_langCount; i++) second += i == g_lsStartup ? 0 : g_lsTables[i].count;
            Log("language: %d string(s) belong to language zones; %u of other languages and %u displaced stock strings "
                "kept%s", g_lsOwned, second, g_lsStock.count, g_lsDropped ? " (some were dropped: table full)" : "");
        } else if (GetTickCount() - since > 120000) {
            g_lsMapFailed = TRUE;
            Log("language: the database was not idle within 120 s of startup; no live switch in this session");
        }
    }
    char now[64], lang[CODOL_LANG_NAME_MAX];
    if (!C1DvarString("k33_language", now, sizeof(now)) || !strcmp(now, g_lsSeen)) return;
    _snprintf(g_lsSeen, sizeof(g_lsSeen), "%s", now);
    g_lsSeen[sizeof(g_lsSeen) - 1] = 0;
    int request = CodolLanguageCanonical(now, lang) ? CodolLanguageListFind(g_langs, g_langCount, lang) : -1;
    if (request >= 0 && strcmp(now, g_langs[request]) != 0) {   /* "English", "ZH": store the list's spelling */
        C1QueueSet("k33_language", g_langs[request]);
        _snprintf(g_lsSeen, sizeof(g_lsSeen), "%s", g_langs[request]);
    }
    LsWorld w = { g_lsInstalled, g_langCount, g_lsStartup, (int)g_lsShown, LsReadyMask() };
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
        LsPublish(LS_STATE_SWITCHING);
        g_lsWanted = request;
        InterlockedExchange(&g_lsShown, request);     /* the switch: every lookup from here on is the new language */
        g_lsSerial++;
        LsPublish(LS_STATE_IDLE);
        Log("language: \"%s\" -> \"%s\" live (serial %d; %u of its strings and %u displaced stock strings in memory)",
            shown, g_langs[request], g_lsSerial, request == g_lsStartup ? 0u : g_lsTables[request].count, g_lsStock.count);
        break;
    case LS_ACT_RESTART:
        g_lsWanted = request;
        LsPublish(LS_STATE_RESTART);
        Log("language: \"%s\" saved, applies on restart (%s); \"%s\" stays on screen", g_langs[request],
            !g_lsInstalled ? "no live switch in this client" : !g_lsOwner ? "the startup zones are still loading" :
            "its strings are not in memory", shown);
        break;
    }
}

/* k33_setlanguage [name]: sets k33_language through the command buffer; without a name, says where things are. */
static void __cdecl LsCmdSetLanguage(void)
{
    char list[CODOL_LANG_MAX * CODOL_LANG_NAME_MAX], lang[CODOL_LANG_NAME_MAX];
    CodolLanguageListJoin(g_langs, g_langCount, list, sizeof(list));
    if (VoiceArgc() < 2) {
        LONG shown = g_lsShown;
        C1Printf("k33_setlanguage <%s>: \"%s\" on screen, state %d, serial %d\n", list,
                 shown >= 0 ? g_langs[shown] : g_lang, g_lsState, g_lsSerial);
        return;
    }
    if (!CodolLanguageCanonical(VoiceArgv(1), lang) || CodolLanguageListFind(g_langs, g_langCount, lang) < 0) {
        C1Printf("k33_setlanguage: not an installed language (%s)\n", list);
        return;
    }
    if (!C1QueueSet("k33_language", lang)) C1Printf("k33_setlanguage: the command buffer is full\n");
}

static C1Cmd g_lsCmd = { NULL, "k33_setlanguage", (void *)LsCmdSetLanguage };

/* After PatchPatchDir (the language list) and before K33InstallMainTicks. */
static void InstallLocaleSwitch(void)
{
    g_lsStartup = CodolLanguageListFind(g_langs, g_langCount, g_lang);
    g_lsShown = g_lsStartup;
    if (!K33OnMainTick(LocaleSwitchTick)) {
        Log("language: no main-thread tick left; k33_language is not registered (command line and saved choice only)");
        return;
    }
    C1AddCommand(&g_lsCmd);
    if (!g_langCount) {
        Log("language: no language zone in the patch folder; \"%s\" is all there is", g_lang);
        return;
    }
    if (!LocaleSwitchCheck()) return;      /* logged there */
    RedirectCall(LS_DISCARD_CALL_VA, (void *)LS_DiscardHook);
    RedirectCall(LS_EXISTS_CALL_VA, (void *)LsExistsHook);
    for (unsigned i = 0; i < sizeof(kLsFindSites) / sizeof(kLsFindSites[0]); i++)
        RedirectCall(kLsFindSites[i], (void *)LsFindHook);
    g_lsInstalled = TRUE;
    Log("language: live switch installed (%u lookups, the linker's duplicate discard 0x%08X); started in \"%s\"",
        (unsigned)(sizeof(kLsFindSites) / sizeof(kLsFindSites[0])) + 1, LS_DISCARD_CALL_VA, g_lang);
}

#pragma once
/* locale_switch_policy.h - the pure part of locale_switch.c (docs/LANGUAGE_SWITCH_20261112.md), tested by
 * tests/locale_switch_policy_test.c on any compiler: the index of the loaded language zone's strings, what C1's
 * linker does with one of them, whether a switch may start now, and what a request does. */
#include <stddef.h>
#include <string.h>

/* One string, laid out as C1's LocalizeEntry {value, name} (a localize pool slot is exactly this). */
typedef struct LsSlot { const char *value; const char *name; } LsSlot;

/* Open addressing, insert only, never resized. name is written last (LS_PUBLISH): a reader that sees a name sees
 * its value. One writer at a time (C1's database thread while it links the language zone); the table is emptied
 * (LsTableInit over zeroed slots) only on the main thread while the database is idle. The strings are not copies:
 * they are the zone's own memory and are valid exactly as long as the zone is loaded. */
typedef struct LsTable { LsSlot *slots; unsigned mask; unsigned count; } LsTable;

#ifndef LS_PUBLISH
#define LS_PUBLISH(dst, src) (*(dst) = (src))
#endif

static unsigned char LsFold(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return (unsigned char)(c + 32);
    return c == '\\' ? '/' : c;
}

/* Names compare as C1 compares asset names (0x4CC435: ASCII case folded), with its hash's '\\' == '/'. */
static int LsNameEqual(const char *a, const char *b)
{
    for (;; a++, b++) {
        unsigned char x = LsFold((unsigned char)*a), y = LsFold((unsigned char)*b);
        if (x != y) return 0;
        if (!x) return 1;
    }
}

static unsigned LsHash(const char *name)
{
    unsigned h = 2166136261u;
    for (; *name; name++) h = (h ^ LsFold((unsigned char)*name)) * 16777619u;
    return h;
}

/* slots: zeroed memory for `capacity` slots; capacity must be a power of two. */
static void LsTableInit(LsTable *t, LsSlot *slots, unsigned capacity)
{
    t->slots = slots;
    t->mask = capacity - 1;
    t->count = 0;
}

static LsSlot *LsFind(const LsTable *t, const char *name)
{
    unsigned i, n;
    if (!t || !t->slots || !name) return NULL;
    for (i = LsHash(name) & t->mask, n = 0; n <= t->mask; i = (i + 1) & t->mask, n++) {
        const char *have = t->slots[i].name;
        if (!have) return NULL;
        if (LsNameEqual(have, name)) return &t->slots[i];
    }
    return NULL;
}

/* 1 stored, 0 the name is already there (the first string of a name stays: C1 keeps the first asset it links), -1 the
 * table is full (three quarters, so a lookup always ends at an empty slot). name and value must outlive the table. */
static int LsInsert(LsTable *t, const char *name, const char *value)
{
    unsigned i, n;
    if (!t || !t->slots || !name || !value) return -1;
    for (i = LsHash(name) & t->mask, n = 0; n <= t->mask; i = (i + 1) & t->mask, n++) {
        const char *have = t->slots[i].name;
        if (!have) {
            if (t->count >= (t->mask + 1) / 4 * 3) return -1;
            t->slots[i].value = value;
            LS_PUBLISH(&t->slots[i].name, name);
            t->count++;
            return 1;
        }
        if (LsNameEqual(have, name)) return 0;
    }
    return -1;
}

/* What C1's linker does with a localize string of the language zone whose name another zone already holds
 * (DB_LinkXAssetEntry 0x4CC61B, where C1 compares the two zones' alloc flags). The language zone has an alloc flag
 * of its own, so C1 would keep both: the holder active and the new one as an override entry behind it, one asset
 * entry and one pool slot more per name (7,739 for English). Instead the holder's pool slot gets the language's
 * string in place (locale_switch.c remembers the holder's own string for the unload), and:
 *   - the holder's zone has alloc flag 1 (a code zone: never unloaded): the new entry is thrown away, as C1 does
 *     for two zones with the same flags. No entry, no slot.
 *   - the holder's zone can be unloaded (UI 2, gametype/map 4, load screen 8): the new entry is also chained as
 *     an override, C1's own way, so the name survives that zone's unload (C1 promotes the override into the
 *     holder's slot, 0x4CDBF0). That is the entry C1 uses today for the same pair, the other way round.
 * Anything else (another asset type, a zone that is not the language zone, the language zone's own duplicate) is
 * C1's own decision. */
enum LrLink { LR_LINK_C1 = 0, LR_LINK_DISCARD = 1, LR_LINK_OVERRIDE = 2 };
#define LR_LOCALIZE_TYPE 27
static enum LrLink LrLinkDecision(int type, int freshIsLanguageZone, int holderIsLanguageZone, unsigned holderAllocFlags)
{
    if (type != LR_LOCALIZE_TYPE || !freshIsLanguageZone || holderIsLanguageZone) return LR_LINK_C1;
    return holderAllocFlags == 1 ? LR_LINK_DISCARD : LR_LINK_OVERRIDE;
}

/* May the language zone be unloaded and another loaded in this frame? Everything is read before anything is
 * touched. LR_GATE_WAIT: ask again next frame (the database is loading). The LR_GATE_NO_* answers end the request
 * as "saved, applies on restart". */
typedef struct LrGate {
    int armed;          /* every C1 site checked and hooked */
    int oldStyle;       /* the startup language came from zones that are not one <name>_locale: C1 owns them */
    int dbIdle;         /* C1's "database idle, results taken" event is set */
    int replaceMode;    /* C1's linker is in its replace mode (0x19A2F6C != 0) */
    int loaded;         /* a language zone is loaded now */
    int onTop;          /* ... and its memory is the top block of its PMem stack (freeing it pops the stack) */
    int zones, zoneMax; /* zones loaded, C1's limit */
    unsigned freeEntries, needEntries; /* asset entries: free now / what the new zone may take */
    unsigned freeSlots, needSlots;     /* localize pool slots, the same */
} LrGate;
enum LrGateAnswer { LR_GATE_GO, LR_GATE_WAIT, LR_GATE_NO_HOOKS, LR_GATE_NO_OLD_STYLE, LR_GATE_NO_REPLACE_MODE,
                    LR_GATE_NO_STACK, LR_GATE_NO_ZONES, LR_GATE_NO_ENTRIES, LR_GATE_NO_SLOTS };
static enum LrGateAnswer LrGateDecide(const LrGate *g)
{
    if (!g->armed) return LR_GATE_NO_HOOKS;
    if (g->oldStyle) return LR_GATE_NO_OLD_STYLE;
    if (!g->dbIdle) return LR_GATE_WAIT;
    if (g->replaceMode) return LR_GATE_NO_REPLACE_MODE;
    if (g->loaded && !g->onTop) return LR_GATE_NO_STACK;
    /* the unload gives back the old zone's slot, entries and pool slots first; none of that is counted on */
    if (!g->loaded && g->zones >= g->zoneMax) return LR_GATE_NO_ZONES;
    if (g->freeEntries < g->needEntries) return LR_GATE_NO_ENTRIES;
    if (g->freeSlots < g->needSlots) return LR_GATE_NO_SLOTS;
    return LR_GATE_GO;
}

/* k33_languageState */
enum LsState { LS_STATE_IDLE = 0, LS_STATE_SWITCHING = 1, LS_STATE_RESTART = 2 };

enum LsAction {
    LS_ACT_NONE,      /* the request is the language on screen */
    LS_ACT_LIVE,      /* unload the language zone and load the requested one (when LrGateDecide lets it) */
    LS_ACT_RESTART,   /* keep the request saved; it applies on the next start */
    LS_ACT_REVERT     /* not an installed language: put k33_language back */
};

typedef struct LsWorld {
    int armed;        /* every C1 site checked and hooked */
    int count;        /* installed languages */
    int shown;        /* index of the language on screen */
    unsigned ready;   /* bit i: language i can be loaded (one <name>_locale zone, or zh: C1's own text) */
} LsWorld;

/* Decided before anything is touched. */
static enum LsAction LsDecide(const LsWorld *w, int request)
{
    if (request < 0 || request >= w->count) return LS_ACT_REVERT;
    if (request == w->shown) return LS_ACT_NONE;
    if (!w->armed) return LS_ACT_RESTART;
    return (w->ready >> request & 1u) ? LS_ACT_LIVE : LS_ACT_RESTART;
}

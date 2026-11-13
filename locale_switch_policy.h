#pragma once
/* locale_switch_policy.h - the pure part of locale_switch.c (docs/LANGUAGE_SWITCH_20261112.md), tested by
 * tests/locale_switch_policy_test.c on any compiler: the string tables a language is shown from, which table a
 * string that C1 threw away belongs in, what a lookup uses before C1's own entry, and what a request does. */
#include <stddef.h>
#include <string.h>

/* One string, laid out as C1's LocalizeEntry {value, name}: a pointer to a slot is what DB_FindXAssetHeader(27, name)
 * returns, so C1's callers (mov eax, [eax]) read a slot exactly as they read a pool entry. */
typedef struct LsSlot { const char *value; const char *name; } LsSlot;

/* Open addressing, insert only, never resized: a slot that was published stays where it is and keeps its strings,
 * so a reader needs no lock and a pointer handed to C1 never dangles. name is written last (LS_PUBLISH): a reader
 * that sees a name sees its value. One writer at a time (locale_switch.c holds a lock around LsInsert). */
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

/* Which table keeps a localize string that C1's linker is about to throw away because a zone with the same alloc
 * flags already holds the name (DB_LinkXAssetEntry 0x4CC623). fresh / holder: the language (list index) of the zone
 * the string comes from and of the zone that holds the name, -1 for a zone of no language (stock, shared).
 *   - from a language's zone that is not the startup language's: that language's table. This is how a second
 *     language gets into memory at all: its zone loads after every stock code zone, so nearly every string of it
 *     is such a duplicate and costs no asset entry.
 *   - from a zone of no language, displaced by a language's zone (the startup language's at startup; a second
 *     language's for the names of zones that load later with the code zones' flags, common_mp): the stock table,
 *     the text C1 itself would show there.
 *   - anything else (stock displaced by stock, the startup language's own zones): not kept. */
enum { LS_ROUTE_NONE = -1, LS_ROUTE_STOCK = -2 };
static int LsCaptureRoute(int fresh, int holder, int startup)
{
    if (fresh >= 0) return fresh != startup ? fresh : LS_ROUTE_NONE;
    return holder >= 0 ? LS_ROUTE_STOCK : LS_ROUTE_NONE;
}

/* A lookup, in the order locale_switch.c asks:
 *   1. LsOwnTable: the shown language's captured strings (it has none while it is the startup language: those are
 *      C1's own entries).
 *   2. C1's entry. LsEntryForeign: it belongs to a language's zone that is not the one shown (the startup
 *      language's while another is shown; a second language's name that reached the pool because no zone held it
 *      yet). Then, LsForeignAnswer: the displaced stock string, else the first-linked entry of a zone of no
 *      language chained under it, else nothing: exactly what a client started in the shown language resolves. */
static LsSlot *LsOwnTable(const LsTable *tables, int count, int shown, int startup, const char *name)
{
    if (shown < 0 || shown >= count || shown == startup) return NULL;
    return LsFind(&tables[shown], name);
}

static int LsEntryForeign(int owner, int shown)
{
    return owner >= 0 && owner != shown;
}

static void *LsForeignAnswer(const LsTable *stock, const char *name, void *chained)
{
    LsSlot *s = LsFind(stock, name);
    return s ? (void *)s : chained;
}

/* k33_languageState */
enum LsState { LS_STATE_IDLE = 0, LS_STATE_SWITCHING = 1, LS_STATE_RESTART = 2 };

enum LsAction {
    LS_ACT_NONE,      /* the request is the language on screen */
    LS_ACT_LIVE,      /* show it now */
    LS_ACT_RESTART,   /* keep the request saved; it applies on the next start */
    LS_ACT_REVERT     /* not an installed language: put k33_language back */
};

typedef struct LsWorld {
    int armed;        /* every C1 site checked and hooked */
    int count;        /* installed languages */
    int startup;      /* index of the language the client started in (-1: none of the list) */
    int shown;        /* index of the language on screen */
    unsigned ready;   /* bit i: language i's strings are in memory */
} LsWorld;

/* Decided before anything is touched: a request either changes the language completely (one store) or not at all. */
static enum LsAction LsDecide(const LsWorld *w, int request)
{
    if (request < 0 || request >= w->count) return LS_ACT_REVERT;
    if (request == w->shown) return LS_ACT_NONE;
    if (!w->armed) return LS_ACT_RESTART;
    if (request == w->startup || (w->ready >> request & 1u)) return LS_ACT_LIVE;
    return LS_ACT_RESTART;
}

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../locale_switch_policy.h"

int main(void)
{
    LsSlot *slots = calloc(64, sizeof(LsSlot));
    LsSlot *stockSlots = calloc(64, sizeof(LsSlot));
    LsTable t, stock, none = { NULL, 0, 0 };
    assert(slots && stockSlots);
    LsTableInit(&t, slots, 64);
    LsTableInit(&stock, stockSlots, 64);

    /* a slot is a LocalizeEntry: value first, then name */
    assert(offsetof(LsSlot, value) == 0 && offsetof(LsSlot, name) == sizeof(char *));

    /* names: ASCII case folded, '\\' is '/', nothing else */
    assert(LsNameEqual("MENU_OK", "menu_ok") && LsNameEqual("a\\b", "A/B") && LsNameEqual("", ""));
    assert(!LsNameEqual("MENU_OK", "MENU_OK2") && !LsNameEqual("MENU_OK2", "MENU_OK") && !LsNameEqual("a", ""));
    assert(LsHash("MENU_OK") == LsHash("menu_ok") && LsHash("a\\b") == LsHash("A/b"));

    /* empty and missing tables answer nothing */
    assert(!LsFind(&t, "MENU_OK") && !LsFind(&none, "MENU_OK") && !LsFind(NULL, "MENU_OK") && !LsFind(&t, NULL));
    assert(LsInsert(&none, "A", "b") == -1 && LsInsert(&t, NULL, "b") == -1 && LsInsert(&t, "A", NULL) == -1);

    /* the first string of a name stays, whatever its case */
    assert(LsInsert(&t, "MENU_OK", "OK") == 1 && t.count == 1);
    assert(LsInsert(&t, "menu_ok", "Okay") == 0 && t.count == 1);
    assert(!strcmp(LsFind(&t, "Menu_Ok")->value, "OK") && !strcmp(LsFind(&t, "Menu_Ok")->name, "MENU_OK"));

    /* a published slot never moves: the pointer C1 was given stays valid while the table fills */
    {
        static char names[64][16];
        LsSlot *first = LsFind(&t, "MENU_OK");
        int stored = 1, full = 0;
        for (int i = 0; i < 64; i++) {
            int r;
            sprintf(names[i], "KEY_%d", i);
            r = LsInsert(&t, names[i], names[i]);
            if (r == 1) stored++; else { assert(r == -1); full++; }
        }
        assert(stored == 48 && full == 17 && t.count == 48);       /* three quarters of 64 */
        assert(LsFind(&t, "MENU_OK") == first && !strcmp(first->value, "OK"));
        for (int i = 0; i < 47; i++) assert(LsFind(&t, names[i]) && LsFind(&t, names[i])->value == names[i]);
        assert(!LsFind(&t, "KEY_63") && !LsFind(&t, "NOT_THERE"));   /* a miss ends at an empty slot */
        assert(LsInsert(&t, "key_3", "x") == 0);                    /* a full table still knows what it has */
    }

    /* which table keeps a string C1 throws away (languages: 0 = en, 1 = fr, 2 = zh) */
    assert(LsCaptureRoute(2, 0, 0) == 2);                /* zh's zone, name held by the startup language en */
    assert(LsCaptureRoute(2, -1, 0) == 2);               /* zh's zone, name held by a stock zone */
    assert(LsCaptureRoute(1, 2, 0) == 1);                /* fr's zone, name held by zh's zone */
    assert(LsCaptureRoute(-1, 0, 0) == LS_ROUTE_STOCK);  /* a stock string the startup language displaced */
    assert(LsCaptureRoute(-1, 2, 0) == LS_ROUTE_STOCK);  /* a later stock zone's string a second language displaced */
    assert(LsCaptureRoute(-1, -1, 0) == LS_ROUTE_NONE);  /* stock displaced by stock: C1's first one is in the pool */
    assert(LsCaptureRoute(0, 0, 0) == LS_ROUTE_NONE);    /* the startup language's own duplicate */
    assert(LsCaptureRoute(0, -1, 0) == LS_ROUTE_NONE);
    assert(LsCaptureRoute(-1, -1, -1) == LS_ROUTE_NONE);
    assert(LsCaptureRoute(1, -1, -1) == 1);              /* started in C1's own language: every zone is a second one */

    /* a lookup: the shown language's captured string first (never for the startup language) */
    {
        LsTable tables[3];
        int marker;
        memset(tables, 0, sizeof(tables));
        tables[2] = t;
        assert(LsInsert(&stock, "MENU_OK", "stock ok") == 1 && LsInsert(&stock, "MENU_BACK", "stock back") == 1);
        assert(!strcmp(LsOwnTable(tables, 3, 2, 0, "menu_ok")->value, "OK"));
        assert(!LsOwnTable(tables, 3, 2, 0, "MENU_BACK"));           /* zh has no such string: C1 is asked */
        assert(!LsOwnTable(tables, 3, 2, 2, "MENU_OK"));             /* the startup language: C1's own entries */
        assert(!LsOwnTable(tables, 3, 1, 0, "MENU_OK"));             /* a language with no table */
        assert(!LsOwnTable(tables, 3, -1, 0, "MENU_OK") && !LsOwnTable(tables, 3, 3, 0, "MENU_OK"));
        /* then C1's entry, unless it belongs to a language that is not the one shown */
        assert(!LsEntryForeign(-1, 0) && !LsEntryForeign(-1, -1));   /* a stock entry */
        assert(!LsEntryForeign(0, 0) && !LsEntryForeign(2, 2));      /* the shown language's own entry */
        assert(LsEntryForeign(0, 2) && LsEntryForeign(2, 0) && LsEntryForeign(0, -1));
        /* for such an entry: the displaced stock string, else the stock entry chained under it, else nothing */
        assert(!strcmp(((LsSlot *)LsForeignAnswer(&stock, "menu_back", &marker))->value, "stock back"));
        assert(LsForeignAnswer(&stock, "GAMETYPE_ONLY", &marker) == &marker);
        assert(LsForeignAnswer(&stock, "ONE_LANGUAGE_ONLY", NULL) == NULL);
        assert(LsForeignAnswer(&none, "MENU_OK", NULL) == NULL);
    }

    /* requests (languages: 0 = en startup, 1 = fr not loaded, 2 = zh loaded) */
    {
        LsWorld w = { 1, 3, 0, 0, 1u << 2 };
        assert(LsDecide(&w, 0) == LS_ACT_NONE);
        assert(LsDecide(&w, 2) == LS_ACT_LIVE);
        assert(LsDecide(&w, 1) == LS_ACT_RESTART);
        assert(LsDecide(&w, -1) == LS_ACT_REVERT && LsDecide(&w, 3) == LS_ACT_REVERT);
        w.shown = 2;                                   /* zh on screen */
        assert(LsDecide(&w, 2) == LS_ACT_NONE);
        assert(LsDecide(&w, 0) == LS_ACT_LIVE);        /* back to the startup language: always in memory */
        assert(LsDecide(&w, 1) == LS_ACT_RESTART);
        w.armed = 0;                                   /* a C1 site did not match: nothing is live */
        w.shown = 0;
        assert(LsDecide(&w, 2) == LS_ACT_RESTART && LsDecide(&w, 0) == LS_ACT_NONE && LsDecide(&w, 9) == LS_ACT_REVERT);
        w.armed = 1;
        w.startup = -1;                                /* started in a language without a zone (stock only) */
        w.shown = -1;
        w.ready = 0;
        assert(LsDecide(&w, 0) == LS_ACT_RESTART && LsDecide(&w, 2) == LS_ACT_RESTART);
        w.count = 0;
        assert(LsDecide(&w, 0) == LS_ACT_REVERT);
    }
    assert(LS_STATE_IDLE == 0 && LS_STATE_SWITCHING == 1 && LS_STATE_RESTART == 2);   /* the menus read these numbers */
    free(slots);
    free(stockSlots);
    puts("language tables, capture routing, lookup order and request decisions passed");
    return 0;
}

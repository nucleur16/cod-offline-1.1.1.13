#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/locale_switch_policy.h"

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

    /* the link decision for a string of the language zone whose name another zone holds (locale_switch.c LrLink) */
    assert(LrLinkDecision(27, 1, 0, 1) == LR_LINK_DISCARD);     /* held by a code zone (flag 1): in place, no entry */
    assert(LrLinkDecision(27, 1, 0, 2) == LR_LINK_OVERRIDE);    /* held by the UI (2): in place, and C1's override kept */
    assert(LrLinkDecision(27, 1, 0, 4) == LR_LINK_OVERRIDE);    /* gametype / map */
    assert(LrLinkDecision(27, 1, 0, 8) == LR_LINK_OVERRIDE);
    assert(LrLinkDecision(27, 1, 1, 1) == LR_LINK_C1);          /* the language zone's own duplicate */
    assert(LrLinkDecision(27, 0, 0, 1) == LR_LINK_C1);          /* another zone's string: C1 decides */
    assert(LrLinkDecision(28, 1, 0, 1) == LR_LINK_C1);          /* another asset type (the font movie, rawfiles) */
    assert(LR_LINK_C1 == 0);                                    /* the asm hook falls through to C1's compare on 0 */

    /* may a switch start now (everything read before anything is touched) */
    {
        LrGate g = { 1, 0, 1, 0, 1, 1, 20, 33, 19657, 4096, 12789, 4096 };
        assert(LrGateDecide(&g) == LR_GATE_GO);
        g.dbIdle = 0; assert(LrGateDecide(&g) == LR_GATE_WAIT); g.dbIdle = 1;
        g.onTop = 0; assert(LrGateDecide(&g) == LR_GATE_NO_STACK);           /* a mid-stack free rewinds the stack */
        g.loaded = 0; assert(LrGateDecide(&g) == LR_GATE_GO);                /* nothing to unload (zh, C1's own) */
        g.zones = 33; assert(LrGateDecide(&g) == LR_GATE_NO_ZONES);
        g.loaded = 1; g.onTop = 1; assert(LrGateDecide(&g) == LR_GATE_GO);   /* the unload frees a zone slot first */
        g.zones = 20;
        g.freeEntries = 4095; assert(LrGateDecide(&g) == LR_GATE_NO_ENTRIES); g.freeEntries = 19657;
        g.freeSlots = 100; assert(LrGateDecide(&g) == LR_GATE_NO_SLOTS); g.freeSlots = 12789;
        g.replaceMode = 1; assert(LrGateDecide(&g) == LR_GATE_NO_REPLACE_MODE); g.replaceMode = 0;
        g.oldStyle = 1; assert(LrGateDecide(&g) == LR_GATE_NO_OLD_STYLE); g.oldStyle = 0;
        g.armed = 0; assert(LrGateDecide(&g) == LR_GATE_NO_HOOKS);
        g.dbIdle = 0; assert(LrGateDecide(&g) == LR_GATE_NO_HOOKS);         /* a client without hooks never waits */
    }

    /* requests (languages: 0 = en on screen, 1 = fr without a zone of its own, 2 = zh) */
    {
        LsWorld w = { 1, 3, 0, (1u << 0) | (1u << 2) };
        assert(LsDecide(&w, 0) == LS_ACT_NONE);
        assert(LsDecide(&w, 2) == LS_ACT_LIVE);
        assert(LsDecide(&w, 1) == LS_ACT_RESTART);
        assert(LsDecide(&w, -1) == LS_ACT_REVERT && LsDecide(&w, 3) == LS_ACT_REVERT);
        w.shown = 2;
        assert(LsDecide(&w, 2) == LS_ACT_NONE && LsDecide(&w, 0) == LS_ACT_LIVE && LsDecide(&w, 1) == LS_ACT_RESTART);
        w.armed = 0;                                   /* a C1 site did not match: nothing is live */
        w.shown = 0;
        assert(LsDecide(&w, 2) == LS_ACT_RESTART && LsDecide(&w, 0) == LS_ACT_NONE && LsDecide(&w, 9) == LS_ACT_REVERT);
        w.armed = 1;
        w.shown = -1;                                  /* started in a language that is not in the list */
        w.ready = 0;
        assert(LsDecide(&w, 0) == LS_ACT_RESTART && LsDecide(&w, 2) == LS_ACT_RESTART);
        w.count = 0;
        assert(LsDecide(&w, 0) == LS_ACT_REVERT);
    }
    assert(LS_STATE_IDLE == 0 && LS_STATE_SWITCHING == 1 && LS_STATE_RESTART == 2);   /* the menus read these numbers */
    free(slots);
    free(stockSlots);
    puts("language index, link decision, switch gate and request decisions passed");
    return 0;
}

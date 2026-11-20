/* The retained XAnim notify paths on a server DObj, against src/asmsource/_xanim.asm.
 *
 * A play-once clip set by script (CoDOL's setAnim passes T5's notifyType 2,
 * src/online/script/script_anim.c) used to queue a client "end" notify
 * into the fixed 128-entry g_notifyList on every server update once the clip
 * had ended. Only DObjUpdateClientInfo empties that list, and a dedicated
 * server never calls it, so entry 128 overwrote g_endNotetrackName (the next
 * symbol) with a string pointer: the next server "end" notify then passed that
 * pointer to Scr_AddConstString ("SL_AddRefToString ... already freed (stale
 * handle)") and Scr_NotifyNum, and MT_GetRefByIndex failed fatally.
 *
 * This drives XAnimSetGoalWeight / DObjUpdateServerInfo / XAnimClearTree with
 * a reference-counted script string stand-in and checks that a server update
 * never touches the client notify list, that the "end" notify reaches script
 * once per finished clip, and that every script string is balanced.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "q_shared.h"
#include "xassets/xanims.h"
/* q_shared.h routes assert to the engine handler; this test wants libc's. */
#undef assert
#include <assert.h>

/* ---- script string stand-in ------------------------------------------- */
#define STRINGS 64
static struct { int used, refs; unsigned len; char data[64]; } strings[STRINGS];

static unsigned sl_find_or_add(const char *s, unsigned len)
{
    unsigned i;
    assert(len <= sizeof(strings[0].data));
    for (i = 1; i < STRINGS; ++i)
        if (strings[i].used && strings[i].len == len && !memcmp(strings[i].data, s, len)) {
            ++strings[i].refs;
            return i;
        }
    for (i = 1; i < STRINGS; ++i)
        if (!strings[i].used) {
            strings[i].used = 1; strings[i].refs = 1; strings[i].len = len;
            memcpy(strings[i].data, s, len);
            return i;
        }
    abort();
}
static void sl_release(unsigned v)
{
    assert(v && v < STRINGS && strings[v].used && strings[v].refs > 0);
    if (!--strings[v].refs) strings[v].used = 0;
}
unsigned SL_GetStringOfSize(const char *s, unsigned user, unsigned len, int type)
{ (void)user; (void)type; return sl_find_or_add(s, len); }
unsigned SL_GetString_(const char *s, unsigned user, int type)
{ (void)user; (void)type; return sl_find_or_add(s, (unsigned)strlen(s) + 1); }
void SL_AddRefToString(unsigned v)
{ assert(v && v < STRINGS && strings[v].used); ++strings[v].refs; }
void SL_RemoveRefToString(unsigned v) { sl_release(v); }
void SL_RemoveRefToStringOfSize(unsigned v, unsigned len) { assert(len == strings[v].len); sl_release(v); }
const char *SL_ConvertToString(unsigned v)
{ assert(v && v < STRINGS && strings[v].used); return strings[v].data; }

/* ---- script VM stand-in: what XAnimProcessServerNotify hands script ----- */
static unsigned params[4], param_count;
static struct { unsigned ent, name, note; } notes[64];
static unsigned note_count;
void Scr_AddConstString(unsigned v)
{ assert(param_count < 4); SL_AddRefToString(v); params[param_count++] = v; }
void Scr_NotifyNum(int ent, unsigned classnum, unsigned name, unsigned count)
{
    assert(classnum == 0 && count == 1 && param_count == 1);
    assert(note_count < 64);
    notes[note_count].ent = (unsigned)ent; notes[note_count].name = name; notes[note_count].note = params[0];
    ++note_count;
    SL_RemoveRefToString(params[--param_count]);
}

/* ---- engine services this fixture never reaches ----------------------- */
static void never(const char *what) { fprintf(stderr, "unexpected call: %s\n", what); abort(); }
void *Hunk_FindDataForFile(int t, const char *n) { (void)t; (void)n; never("Hunk_FindDataForFile"); return 0; }
void *DB_FindXAssetHeaderReal(int t, const char *n) { (void)t; (void)n; never("DB_FindXAssetHeaderReal"); return 0; }
void *Hunk_AllocAlignInternal(int s, int a) { (void)s; (void)a; never("Hunk_AllocAlignInternal"); return 0; }
void XAnim_CalcDeltaForTime(const void *p, float t, float *r, float *tr) { (void)p; (void)t; (void)r; (void)tr; never("XAnim_CalcDeltaForTime"); }
void Com_sprintfPos(char *b, int s, int *p, const char *f, ...) { (void)b; (void)s; (void)p; (void)f; never("Com_sprintfPos"); }
void Com_Error(int c, const char *f, ...)
{ va_list ap; (void)c; va_start(ap, f); vfprintf(stderr, f, ap); va_end(ap); never("Com_Error"); }
int DB_IsXAssetDefault(int t, const char *n) { (void)t; (void)n; never("DB_IsXAssetDefault"); return 0; }
void *Hunk_AllocDebugMem(int s) { (void)s; never("Hunk_AllocDebugMem"); return 0; }
void Hunk_FreeDebugMem(void *p) { (void)p; never("Hunk_FreeDebugMem"); }
void *XAnimLoadFile(const char *n, void *(*a)(int)) { (void)n; (void)a; never("XAnimLoadFile"); return 0; }
void Hunk_SetDataForFile(int t, const char *n, void *d, void *(*a)(int)) { (void)t; (void)n; (void)d; (void)a; never("Hunk_SetDataForFile"); }
void Com_PrintWarning(int c, const char *f, ...) { (void)c; (void)f; never("Com_PrintWarning"); }
int Com_sprintf(char *b, int s, const char *f, ...) { (void)b; (void)s; (void)f; never("Com_sprintf"); return 0; }
void Com_Printf(int c, const char *f, ...) { (void)c; (void)f; never("Com_Printf"); }
int Hunk_DataOnHunk(void *d) { (void)d; never("Hunk_DataOnHunk"); return 0; }
void *Hunk_AddData(int t, void *d, void *(*a)(int)) { (void)t; (void)d; (void)a; never("Hunk_AddData"); return 0; }
char *va(const char *f, ...) { (void)f; never("va"); return 0; }
void Scr_AddFloat(float v) { (void)v; never("Scr_AddFloat"); }
void Scr_AddArray(void) { never("Scr_AddArray"); }
static struct { char name[12]; int integer; } developer;
void *com_developer = &developer;
void *useFastFile = 0;

/* ---- the retained XAnim under test ------------------------------------ */
extern XAnimInfo g_xAnimInfo[4096];
extern int g_notifyListSize;
extern unsigned g_endNotetrackName;
extern void _GLOBAL__I__Z9XAnimInitv(void);
extern void XAnimInit(void);
extern void XAnimShutdown(void);
extern int XAnimSetGoalWeight(void *obj, unsigned anim, float goalWeight, float goalTime, float rate,
                              unsigned notifyName, unsigned notifyType, int restart);
extern int DObjUpdateServerInfo(void *obj, float dtime, int notify);
extern void DObjInitServerTime(void *obj, float dtime);
extern void XAnimClearTree(struct XAnimTree_s *tree);
extern float XAnimGetTime(const struct XAnimTree_s *tree, unsigned anim);

/* The retained DObj layout (src/dobj.h; the asm reads only these fields). */
typedef struct {
    struct XAnimTree_s *tree;   /* 0x00 */
    uint16_t duplicateParts;    /* 0x04 */
    uint16_t entnum;            /* 0x06 entity number + 1 */
    uint8_t pad8, numModels;    /* 0x08, 0x09 */
    uint8_t rest[0xa0 - 0x0a];
    void *models;               /* 0xa0 */
    uint32_t tail;
} test_dobj_t;
_Static_assert(sizeof(test_dobj_t) == 0xa8, "retained DObj size");

static XAnimParts clip;
static XAnimNotifyInfo clip_notes[2];
static union { XAnim_s anims; char bytes[sizeof(XAnim_s) + sizeof(XAnimEntry)]; } tree_def;
static struct XAnimTree_s tree;
static test_dobj_t obj;

static int live_strings(void)
{
    int i, n = 0;
    for (i = 1; i < STRINGS; ++i) n += strings[i].used ? strings[i].refs : 0;
    return n;
}

/* One play-once clip, run for `frames` 50 ms server frames past its start. */
static unsigned play_once(unsigned notifyName, unsigned notifyType, int frames)
{
    int i;
    unsigned before = note_count;
    assert(!XAnimSetGoalWeight(&obj, 1, 1.0f, 0.0f, 1.0f, notifyName, notifyType, 1));
    for (i = 0; i < frames; ++i) {
        /* G_RunFrame: install the frame's time, then update with notifies
         * until no notetrack interrupts the step (_g_main_mp.asm
         * G_RunFrame_550, the server's only DObjUpdateServerInfo caller). */
        DObjInitServerTime(&obj, 0.05f);
        while (DObjUpdateServerInfo(&obj, 0.05f, 1))
            ;
        /* A dedicated server has no reader of the client notify list. */
        assert(g_notifyListSize == 0);
    }
    assert(XAnimGetTime(&tree, 1) == 1.0f);
    return note_count - before;
}

int main(void)
{
    unsigned end, flag, i, baseline, n;

    _GLOBAL__I__Z9XAnimInitv();
    XAnimInit();
    end = g_endNotetrackName;
    assert(end && !strcmp(SL_ConvertToString(end), "end"));

    clip.name = "door_open"; clip.numframes = 20; clip.framerate = 20.0f; clip.frequency = 1.0f;
    clip.bLoop = 0;
    /* One mid-clip notetrack and none at the end: XAnimProcessServerNotify
     * finds no notetrack left after it and raises g_endNotetrackName. */
    clip_notes[0].name = (ScriptString)SL_GetString_("door_open_fx", 0, 4);
    clip_notes[0].time = 0.52f;
    clip_notes[1].name = (ScriptString)SL_GetString_("end", 0, 4);
    clip_notes[1].time = 1.0f;
    clip.notify = clip_notes; clip.notifyCount = 1;
    tree_def.anims.debugName = "vehicles"; tree_def.anims.size = 2;
    tree_def.anims.entries[0].numAnims = 1;
    tree_def.anims.entries[0].u.animParent.children = 1;
    tree_def.anims.entries[1].parent = 0;
    tree_def.anims.entries[1].u.parts = &clip;
    tree.anims = &tree_def.anims;
    obj.tree = &tree; obj.entnum = 5;
    baseline = live_strings();

    /* 1. setAnim without a notify name (the take-fav van door, the killhouse
     *    jeep door): ends silently, and twenty seconds of updates after its
     *    end leave g_endNotetrackName intact. Before the fix the client list
     *    filled at one entry per update and entry 128 overwrote it. */
    n = play_once(0, 2, 400);
    assert(n == 0);
    assert(g_endNotetrackName == end);
    XAnimClearTree(&tree);
    assert(live_strings() == baseline);

    /* 2. setFlaggedAnim / setAnim with a notify name: the notetrack, then
     *    "end", reach script on the DObj's entity once each, and nothing more
     *    while the clip holds its last frame (T5's currentAnimTime != time
     *    gate; IW3 raised "end" again on every update). The clip's reference
     *    to the name is released with it. */
    flag = SL_GetString_("door_anim", 0, 6);
    n = play_once(flag, 2, 400);
    assert(n == 2);
    assert(notes[note_count - 2].ent == 4 && notes[note_count - 2].name == flag &&
           notes[note_count - 2].note == clip_notes[0].name);
    assert(notes[note_count - 1].ent == 4 && notes[note_count - 1].name == flag &&
           notes[note_count - 1].note == end);
    assert(g_endNotetrackName == end);
    XAnimClearTree(&tree);
    SL_RemoveRefToString(flag);
    assert(live_strings() == baseline);

    /* 3. The same clip restarted repeatedly: one notetrack and one "end" per
     *    playthrough. */
    flag = SL_GetString_("door_anim", 0, 6);
    for (i = 0; i < 5; ++i) {
        assert(play_once(flag, 2, 40) == 2);
        assert(notes[note_count - 1].note == end);
    }
    XAnimClearTree(&tree);
    SL_RemoveRefToString(flag);
    assert(live_strings() == baseline);
    assert(g_endNotetrackName == end);

    /* 4. A clip that ends in its own "end" notetrack (the converter's usual
     *    last entry): that notetrack is the one "end", not a second one. */
    clip.notifyCount = 2;
    flag = SL_GetString_("door_anim", 0, 6);
    for (i = 0; i < 3; ++i) {
        assert(play_once(flag, 2, 60) == 2);
        assert(notes[note_count - 2].note == clip_notes[0].name);
        assert(notes[note_count - 1].note == end && notes[note_count - 1].name == flag);
    }
    XAnimClearTree(&tree);
    SL_RemoveRefToString(flag);
    assert(live_strings() == baseline);
    assert(g_endNotetrackName == end);

    XAnimShutdown();
    SL_RemoveRefToString(clip_notes[0].name);
    SL_RemoveRefToString(clip_notes[1].name);
    assert(live_strings() == 0);
    puts("test_xanim_notify: PASS");
    return 0;
}

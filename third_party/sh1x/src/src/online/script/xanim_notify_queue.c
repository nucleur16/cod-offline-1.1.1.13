#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Server notetrack delivery, one notify per script run.
 *
 * XAnimProcessServerNotify (src/asmsource/_xanim.asm) raises every notetrack
 * an animation crosses in one time update as Scr_AddConstString(<note>) +
 * Scr_NotifyNum(entnum, 0, <flag>, 1).  The notified threads only resume at
 * the next Scr_RunCurrentThreads, which G_RunFrame calls after
 * G_DObjUpdateServerTime returns.  A thread that waits with
 * `self waittill( flag, note )` -- T5's animscripts DoNoteTracks, and every
 * zm animscript pump ported from it -- is therefore woken by the FIRST note
 * of the update and is no longer waiting when the second one is raised, so
 * the second is lost.
 *
 * MEASURED 2026-10-15 (zm_prototype_sh pathtest soak, every note the window
 * traversal's DoNoteTracks received): ai_zombie_traverse_v2 authors "blend"
 * and "end" both at t 1.0 (xanim dump: frame 44 of 44).  24 of 48 window
 * climbs used it; each received "blend" and never "end", so the traversal
 * animscript ran until the native 8 s negotiation timeout and the zombie
 * stood inside the window for 6.5 s after landing.  traverse_v1 ("blend"
 * 0.948, "end" 1.0) never missed one.
 *
 * T5 delivers both: its DoNoteTracks receives every note of a clip.  So the
 * notifies of one update are queued here, in order, and raised once the tree
 * update has finished, with the entity's woken threads run between two
 * consecutive notes (each note then reaches a waiter that has re-armed its
 * waittill) and after the last, as G_RunFrame does after an update.
 * Running script only after the update returns keeps scripts from touching
 * a tree XAnimUpdateTimeAndNotetrack is still walking.  A thread woken by an
 * earlier note may free the entity; its later notes are then dropped.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "scr_vm.h"
#include "g_shared.h"
#include "xanim_notify_queue.h"

void Scr_RunCurrentThreads(void);

#define XANIM_NOTIFY_QUEUE 64

typedef struct {
    int entnum;
    unsigned classnum, notifyName, paramCount, note;
} xanim_notify_t;

static xanim_notify_t s_queue[XANIM_NOTIFY_QUEUE];
static unsigned s_queued;
static unsigned s_pendingNote;
static int s_hasPendingNote;
static int s_depth;

/* Replaces the Scr_AddConstString that precedes every server notify in
 * XAnimProcessServerNotify: the note is held until the notify is queued. */
void XAnimNoteString(unsigned int note)
{
    s_pendingNote = note;
    s_hasPendingNote = 1;
}

/* Replaces Scr_NotifyNum in XAnimProcessServerNotify.  Outside a server time
 * update (s_depth 0), or with the queue full, the note is raised at once as
 * before. */
void XAnimQueueNotify(int entnum, unsigned int classnum, unsigned int notifyName, unsigned int paramCount)
{
    unsigned note = s_pendingNote;
    int hasNote = s_hasPendingNote;
    s_hasPendingNote = 0;
    if (!s_depth || s_queued >= XANIM_NOTIFY_QUEUE) {
        if (hasNote) Scr_AddConstString(note);
        Scr_NotifyNum(entnum, classnum, notifyName, hasNote ? paramCount : 0);
        return;
    }
    s_queue[s_queued].entnum = entnum;
    s_queue[s_queued].classnum = classnum;
    s_queue[s_queued].notifyName = notifyName;
    s_queue[s_queued].paramCount = hasNote ? paramCount : 0;
    s_queue[s_queued].note = note;
    ++s_queued;
}

void XAnimNotifyBegin(void)
{
    ++s_depth;
}

/* Raises the queued notes in order, running the threads each one woke
 * before raising the next.  Returns how many were raised. */
int XAnimNotifyFlush(void)
{
    if (s_depth > 0) --s_depth;
    if (s_depth) return 0;
    unsigned count = s_queued, i;
    xanim_notify_t pending[XANIM_NOTIFY_QUEUE];
    for (i = 0; i < count; ++i) pending[i] = s_queue[i];
    s_queued = 0;
    for (i = 0; i < count; ++i) {
        if (!pending[i].classnum && ((unsigned)pending[i].entnum >= MAX_GENTITIES ||
            !g_entities[pending[i].entnum].r.inuse)) continue;
        if (pending[i].paramCount) Scr_AddConstString(pending[i].note);
        Scr_NotifyNum(pending[i].entnum, pending[i].classnum, pending[i].notifyName, pending[i].paramCount);
        Scr_RunCurrentThreads();
    }
    return (int)count;
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}
int PersistentTailAnimNotifyQuiet(void){return !s_queued&&!s_hasPendingNote&&!s_depth;}
int PersistentTailAnimNotifyEmpty(void){return !s_queued&&!s_hasPendingNote&&!s_depth&&!s_pendingNote&&resident_tail_zero(s_queue,sizeof(s_queue));}
void PersistentTailAnimNotifyReset(void){memset(s_queue,0,sizeof(s_queue));s_pendingNote=0;}
#endif

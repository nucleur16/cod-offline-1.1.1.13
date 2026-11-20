/* Server frame cadence and hitch attribution; see frame_profile.h.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include <string.h>
#include "frame_profile.h"
#include "q_shared.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "sys_main.h"

static const char *const section_names[FP_COUNT] = {
    "packet", "session-save", "admission", "game", "send"
};

typedef struct {
    unsigned long long worst_us[FP_COUNT];
    /* Summed G_RunFrame time: over the window's real length it shows a
     * game that cannot keep up (per-frame worst alone hid a 3.8 s stall
     * made of 188 frames of ~20 ms each, 2026-10-21). */
    unsigned long long game_total_us;
    unsigned long long actor_us[FPA_COUNT];
    unsigned actor_frames;
    unsigned long long script_us;
    unsigned long long print_us, print_worst_us;
    unsigned prints;
    /* The worst game frame's own script and actor time. */
    unsigned long long worst_script_us, worst_actor_us, worst_frame_print_us;
    unsigned count[FP_COUNT];
    unsigned batches, doubled, max_frames, late;
    unsigned long long max_gap_us;
    unsigned long long window_start_us;
} frame_window_t;

static frame_window_t window;
/* Script and actor time since the last G_RunFrame ended (this frame's). */
static unsigned long long frame_script_us, frame_actor_us, frame_print_us;
static unsigned long long last_batch_us, last_hitch_print_us;

unsigned long long FrameProfileNow(void)
{
    return Sys_MicrosecondsLong();
}

void FrameProfileEnd(frame_section_t section, unsigned long long start)
{
    unsigned long long now, took;
    if((unsigned)section >= FP_COUNT || !start) return;
    now = Sys_MicrosecondsLong();
    if(now < start) return;
    took = now - start;
    ++window.count[section];
    if(section == FP_GAME) window.game_total_us += took;
    if(took > window.worst_us[section]) {
        window.worst_us[section] = took;
        if(section == FP_GAME) {
            window.worst_script_us = frame_script_us;
            window.worst_actor_us = frame_actor_us;
            window.worst_frame_print_us = frame_print_us;
        }
    }
    if(section == FP_GAME) frame_script_us = frame_actor_us = frame_print_us = 0;
    if(took >= (unsigned long long)FRAME_HITCH_MS * 1000ull &&
       now - last_hitch_print_us >= 1000000ull) {
        last_hitch_print_us = now;
        ((void)0);
    }
}

void FrameProfileActor(const unsigned long long *us)
{
    int i;
    for(i = 0; i < FPA_COUNT; ++i) window.actor_us[i] += us[i];
    ++window.actor_frames;
    frame_actor_us += us[FPA_TOTAL];
}

void FrameProfilePrint(unsigned long long us)
{
    window.print_us += us;
    frame_print_us += us;
    ++window.prints;
    if(us > window.print_worst_us) window.print_worst_us = us;
}

void FrameProfileScript(unsigned long long us)
{
    window.script_us += us;
    frame_script_us += us;
}

static void report(unsigned long long now)
{
    ((void)0);
    memset(&window, 0, sizeof(window));
    window.window_start_us = now;
}

void FrameProfileBatch(int gameFrames)
{
    unsigned long long now = Sys_MicrosecondsLong();
    if(last_batch_us && now > last_batch_us) {
        unsigned long long gap = now - last_batch_us;
        if(gap > window.max_gap_us) window.max_gap_us = gap;
        /* One 60 Hz frame is 16.7 ms; a batch later than 20 ms is a late
         * snapshot (and, with no catch-up, a shortened next interval). */
        if(gap > 20000ull) ++window.late;
    }
    last_batch_us = now;
    ++window.batches;
    if(gameFrames >= 2) ++window.doubled;
    if(gameFrames > 0 && (unsigned)gameFrames > window.max_frames)
        window.max_frames = (unsigned)gameFrames;
    if(!window.window_start_us) window.window_start_us = now;
    if(now - window.window_start_us >= 1000000ull) {
        static cvar_t *profile;
        if(!profile) profile = Cvar_RegisterBool("frameProfile", qfalse, 0,
            "Print the once-a-second CoDOL frame line even with debugTrace off");
        if(OL_TRACE_ON() || (profile && profile->boolean)) report(now);
        else { memset(&window, 0, sizeof(window)); window.window_start_us = now; }
    }
}
#endif

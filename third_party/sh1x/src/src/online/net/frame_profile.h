/* Server frame cadence and hitch attribution.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The retained engine reports only "Hitch warning: N msec frame time" past
 * 500 ms and nothing about where the time went. A dom session on 2026-10-13
 * showed two such hitches (926 and 1122 ms) plus a steady 3-6 doubled frames
 * a second that never reached the warning. This module times the sections a
 * stall can come from and the real spacing of the game-frame batches:
 *
 *   - with ol_debugTrace on, one "CoDOL frame:" line a second: batches,
 *     batches that ran two or more game frames (one snapshot for them), the
 *     largest real gap between batches, and each section's worst time;
 *     (frameProfile 1 prints the same line with tracing off);
 *   - always, one "CoDOL hitch:" line (at most one a second) when a single
 *     section took FRAME_HITCH_MS or more, naming the section.
 *
 * Sections are timed with Sys_MicrosecondsLong (QueryPerformanceCounter on
 * Windows), a few tens of nanoseconds a call. */
#ifndef FRAME_PROFILE_H
#define FRAME_PROFILE_H

#define FRAME_HITCH_MS 100

typedef enum {
    FP_PACKET,        /* one received datagram, transport and commands */
    FP_SESSION_SAVE,  /* bdDTLS session-file replacement */
    FP_ADMISSION,     /* admission roster / session key file lookups */
    FP_GAME,          /* one G_RunFrame */
    FP_SEND,          /* SV_SendClientMessages */
    FP_COUNT
} frame_section_t;

#ifdef __cplusplus
extern "C" {
#endif
unsigned long long FrameProfileNow(void);
/* Account `start` .. now to a section. */
void FrameProfileEnd(frame_section_t section, unsigned long long start);
/* A batch of `gameFrames` G_RunFrame calls just ran (SV_Frame). */
void FrameProfileBatch(int gameFrames);

/* How far svs.time lags real time, in ms: the unsimulated residual plus the
 * real time since SV_Frame last took its usec. Normally under one frame;
 * after a stall it is the stall. Commands a client timed during that stall
 * are not ahead of the server's clock, only of its stale svs.time. */
int ServerClockLagMs(void);

/* One actor's ActorRunFrame and its timed parts, in microseconds,
 * summed into the second's "CoDOL frame:" line (the per-actor "frame took"
 * lines were the only actor cost readout, and only past 4 ms). */
enum {
    FPA_TOTAL, FPA_ROUTE, FPA_GOAL, FPA_SIGHT,
    FPA_SCRIPT, FPA_COVER, FPA_PUSH, FPA_ANIM,
    FPA_AUDIT, FPA_COUNT
};
void FrameProfileActor(const unsigned long long *us /* [FPA_COUNT] */);
/* One console/logfile line (Sys_Print + Com_PrintLogfile), in microseconds. */
void FrameProfilePrint(unsigned long long us);
/* One Scr_RunCurrentThreads (script VM time slice), in microseconds. */
void FrameProfileScript(unsigned long long us);
#ifdef __cplusplus
}
#endif
#endif

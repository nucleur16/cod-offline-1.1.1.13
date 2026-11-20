/* Remote human player publication timeline.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * A bot thinks once per server frame, so its playerState (and the entity
 * state built from it) advances by exactly one frame per snapshot. A human's
 * playerState only advances when a usermove packet arrives: C1 batches ~3
 * commands into a ~34 ms packet, so at sv_fps 60 the position a snapshot can
 * publish advances {0, 0, ~35 ms} from snapshot to snapshot, and on a real
 * connection the packets also arrive with jitter.
 *
 * e0200d2b tried to hide that with the IW3 g_smoothClients trajectory
 * (TR_LINEAR_STOP, trTime = ps.commandTime, trDuration 50 ms): C1 evaluates
 * each snapshot's trajectory at that snapshot's serverTime (0x44337e ->
 * 0x442ac0 -> BG_EvaluateTrajectory 0x40ca40), which only lands on the
 * player's present position while serverTime - commandTime stays inside the
 * 50 ms window. A remote client's commands are executed a whole round trip
 * plus up to one packet gap after their serverTime, so over the internet the
 * window is always exhausted: every snapshot evaluates to origin(commandTime)
 * + velocity * 50 ms, which moves exactly as lumpily as the commands arrive.
 * Over loopback the lag is small enough that the window works, which is why
 * the local test looked smooth while other real players were a slideshow.
 *
 * This module republishes a human on an even timeline instead. Every executed
 * command is recorded as a sample (commandTime, origin, view angles,
 * velocity); each server frame publishes the pose interpolated at
 * frameTime - delay, where the delay follows the recent peak of
 * frameTime - commandTime. The published time therefore advances by one frame
 * per frame and stays inside the samples the server has already simulated,
 * so the published path is the player's real path (no extrapolation
 * overshoot), only shifted by about one packet gap more than the raw latest
 * command. Antilag rewinds through the archived snapshot (sv_snapshot.c
 * SV_GetClientPositionsFromCachedSnap), which carries this same pose, so hit
 * registration follows what the shooter was shown.
 *
 * The algorithm is engine-free so tests/test_player_lerp.c can drive
 * it; the per-client glue (bot exclusion, the smoothHumans switch and
 * the tracePlayerLerp report) is compiled only into the server. */
#ifndef PLAYER_LERP_H
#define PLAYER_LERP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 8)

#define PLAYER_LERP_SAMPLES 128u     /* > 1 s of C1 commands */
#define PLAYER_LERP_MAX_DELAY 400.0f /* antilag rewinds at most 400 ms */
#define PLAYER_LERP_EXTRAPOLATE 50   /* C1's own g_smoothClients window */

typedef struct {
    int time;
    float origin[3];
    float angles[3];
    float velocity[3];
} player_lerp_sample_t;

typedef struct {
    player_lerp_sample_t samples[PLAYER_LERP_SAMPLES];
    unsigned head;           /* index of the newest sample */
    unsigned count;
    int teleport_bit;
    /* Published timeline, valid once has_pub is set. */
    int has_pub;
    int pub_frame;           /* server frame time the cached pose belongs to */
    float pub_time;          /* command-clock time that pose was taken at */
    float delay;             /* pub_frame - pub_time, slewed */
    float peak;              /* recent peak of frame time - newest command */
    float pub_origin[3];
    float pub_angles[3];
    /* Report counters, cleared by the reader. */
    unsigned frames, extrapolated, resets;
    float lag_max;
} player_lerp_t;

void PlayerLerpReset(player_lerp_t *lerp);

/* One simulated playerState. Equal times overwrite the newest sample (a
 * ClientEndFrame republish of the last command, or a script move). A toggled
 * teleport bit, a backwards time or a move no velocity explains starts a new
 * history, so the published pose snaps rather than slides across a spawn or
 * teleport. */
void PlayerLerpRecord(player_lerp_t *lerp, int time,
                            const float origin[3], const float angles[3],
                            const float velocity[3], int teleport_bit);

/* The pose to publish for server frame time `now`. Returns 0 when there is no
 * sample. Calls for the same `now` return the same pose. */
int PlayerLerpPublish(player_lerp_t *lerp, int now,
                            float origin[3], float angles[3]);

#ifdef OL_SERVER
/* Server glue, keyed by client number. Capture runs wherever the engine
 * rebuilds a player's entity state from its playerState
 * (G_UpdateEntityStateFromPlayerState). Pose returns 1 and the pose to
 * publish for a human client that is alive in normal or last-stand movement;
 * 0 means publish the entity state unchanged (bots, the dead, spectators,
 * linked players, or smoothHumans 0). */
void PlayerLerpCapture(int client, int commandTime, int pmType, int eFlags,
                             const float origin[3], const float angles[3],
                             const float velocity[3]);
int PlayerLerpPose(int client, int now, float origin[3], float angles[3]);
void PlayerLerpResetClient(int client);
#endif

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
#endif

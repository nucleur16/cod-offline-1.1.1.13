/* A resting player takes the spot C1's own prediction settles on.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * See rest_settle.c for the evidence and the bounds. */
#ifndef REST_SETTLE_H
#define REST_SETTLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Called by SV_UserMove after the packet's commands have run.  `predicted` and
 * `predictedTime` are the origin and command time C1 reported in the packet's
 * trailer; `input_idle` is non-zero when no executed command in the packet
 * asked to move; `was_still` is non-zero when the playerState velocity was
 * zero before the packet ran.  Returns 1 when the server adopted the
 * predicted origin. */
int RestSettleAfterUsercmds(int clientNum, const float *predicted,
                                  int predictedTime, int input_idle,
                                  int was_still);
void RestSettleResetClient(int clientNum);
/* Registers settleRestingPrediction (InitAssetCommands). */
void RegisterRestSettle(void);

#ifdef __cplusplus
}
#endif
#endif

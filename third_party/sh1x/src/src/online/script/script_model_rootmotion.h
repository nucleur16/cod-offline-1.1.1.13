/* Animation root motion on a script_model.
 *
 * <script_model> codol_animScriptedModel( <notify>, <origin>, <angles>, %anim
 *                                         [, <rate> [, <blend time>]] )
 * <script_model> codol_stopAnimScriptedModel()
 *
 * Neither the server DObj nor C1 moves a script model by its clip's root
 * motion (the delta is not part of the bone pose). codol_animScriptedModel
 * plays the clip as setflaggedanimknobrestart does (time 0, notetracks and
 * "end" raised on <notify>), puts the model at <origin>/<angles>, and from
 * then on RunScriptModelRootMotion, once per server frame, places the
 * model where the clip's own accumulated motion puts it:
 *
 *   origin = start + R(start angles) * XAnimGetRelDelta(anim, 0, f).trans
 *   yaw    = start yaw + RotationToYaw(XAnimGetRelDelta(anim, 0, f).rot)
 *
 * f is the clip's time in the entity's own server XAnim tree, so the pose
 * (which C1 plays from the same restart command on the snapshot clock) and
 * the position share one clock. When the clip completes (f reaches 1, or the
 * clip leaves the tree) the model holds the clip's last position until the
 * script moves it, codol_stopAnimScriptedModel is called, or the entity is
 * freed.
 *
 * Wire encoding (C1 1.1.1.13, see docs/ENDGAME_TOP3.md "Fourth stage"):
 * an entity origin is an INTEGER on C1's wire (delta.c position(),
 * MSG_ReadOriginFloat shape), so neither TR_STATIONARY nor TR_INTERPOLATE can
 * carry a sub-unit walk; trDelta (-80) and trTime (-72) are exact. C1
 * evaluates a TR_LINEAR eType 6 (0x443337 dispatch -> 0x40ca40 case 2:
 * base + (cg.time - trTime) * 0.001 * delta) every render frame from the
 * current snapshot. Each frame publishes TR_LINEAR anchored ROOTMOTION_ANCHOR
 * ms in the past: base = round(p - v*K), delta = (p - base)/K, trTime = now-K.
 * That passes exactly through p at the frame's time and moves at v (to within
 * 0.87/K units per ms) until the next snapshot replaces it.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SCRIPT_MODEL_ROOTMOTION_H
#define SCRIPT_MODEL_ROOTMOTION_H
#ifdef OL_SERVER
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
struct XAnimTree_s;

/* Starts root motion for anim (an index in tree) on entity, which is placed
 * at origin/angles. Returns 0 when every slot is busy. */
int ScriptModelRootMotionBegin(struct gentity_s *entity, struct XAnimTree_s *tree,
                                     unsigned anim, const float *origin,
                                     const float *angles, float rate);
/* Releases the entity: its last published trajectory stays as it is. */
void ScriptModelRootMotionStop(unsigned entity);
/* Once per server frame, after the entities ran and before the linked world
 * weapons follow their parents (G_RunFrame). */
void RunScriptModelRootMotion(void);
void ResetScriptModelRootMotion(unsigned entity);
void ResetScriptModelRootMotions(void);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
#endif

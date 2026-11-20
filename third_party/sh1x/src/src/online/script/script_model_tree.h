/* Script-model animtrees: T5's setanim family on a script_model.
 *
 * Why this exists (C1 1.1.1.13, MEASURED this pass)
 * -------------------------------------------------
 * C1 has two ways to animate an ET_SCRIPTMOVER, and only one of them plays a
 * clip from its start.
 *
 *   1. The entity-state arm 0x43fcf9..0x43fdb7, which scriptModelPlayAnim
 *      feeds (model_animation.h): wire +060 names an entry of the
 *      realm-0 tree selected by +094, and +06c must be 0. When the DObj is
 *      not already running that entry (0x63a5f0) it starts it with goal
 *      weight 1, blend 1 and rate 1, then sets its time with
 *      0x63c310(entry, dobj, 0x61bea0(0, duration) / duration). 0x61bea0 is
 *      flrand: `seed = seed*0x343fd + 0x269ec3`, `min + (seed>>17) *
 *      2^-15 * (max - min)`. So this arm starts every clip at a RANDOM
 *      phase - the IW4 ambient-prop behaviour (swaying trees start out of
 *      step). For a one-shot such as the mystery-box lid that is a lid that
 *      pops part-way open (or all the way) and fades in over a second.
 *
 *   2. The snapshot animation-command executor 0x443e90, driven per snapshot
 *      by 0x46aff0, which the actor journal already feeds
 *      (actor_anim_journal.h, script_anim.c). It needs only a
 *      DObj with a tree on the entity (0x46b0f1 "No DObj", 0x46b124 "No anim
 *      tree"), and for eType 6 the DObj's tree is the realm-0 tree named by
 *      +094 (0x443540 arm 0x4435bb). It applies T5's command set exactly:
 *      set weight / knob / knob-all with the restart bit, and set time.
 *
 * T5 animates a script model the second way: UseAnimTree, then SetAnim*
 * (OpenBLOPS g_scr_main_mp.cpp GScr_SetAnimInternal works on any entity with
 * a tree). This module gives a script_model that server tree: it owns an
 * XAnimTree_s for it, rebuilds the server DObj on it (G_DObjUpdate asks
 * ActorOwnedTree), publishes +094 = the tree's client id with +060 = 0
 * (so arm 1 never fires), and hands the entity to the actor pose capture so
 * the existing journal delivers its commands and bootstraps a viewer that
 * enters PVS late from the captured server pose.
 *
 * Binding is implicit: the first setanim-family call on a script_model binds
 * it to the tree its %anim reference was compiled against (T5 would have
 * required an explicit UseAnimTree with that same tree).
 *
 * Helicopters and vehicles (eType 12 / 14) bind the same way: C1 0x443540
 * names their DObj tree from the same wire +094, which for a vehicle is
 * published per definition slot (SetVehicleDefinitionAnimTree), and
 * the command executor plays their commands like any other entity's. That
 * is how a helicopter's rotors turn: IW4 _vehicle build_drive loops
 * bh_rotors (main_rotor_jnt / tail_rotor_jnt), which C1 lists in
 * animlists/common.animlist's =vehicles block for every match.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SCRIPT_MODEL_TREE_H
#define SCRIPT_MODEL_TREE_H
#ifdef OL_SERVER
#include "dog_animation.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
struct XAnimTree_s;

/* An entity this module may give a tree: a script_model (in use,
 * ET_SCRIPTMOVER, class script_model, not a brush model, with a model), or a
 * helicopter/vehicle with a model whose wheels the RC producer does not own. */
int ScriptModelTreeEligible(const struct gentity_s *entity);
/* Bind entity to the retained (realm 1) tree server_tree and rebuild its
 * server DObj on it. Re-binding the same tree succeeds; a different tree is
 * refused. Returns 1, or 0 with *why naming the refusal. */
int ScriptModelBindAnimTree(struct gentity_s *entity, unsigned server_tree,
                                  const char **why);
/* The tree the server DObj of this entity must be built on, or NULL. */
struct XAnimTree_s *ScriptModelAnimTree(const struct gentity_s *entity);
/* Appends the bound script models to an actor capture-source list; never
 * refuses, it simply stops at capacity. */
unsigned ScriptModelTreeCaptureSources(dog_anim_capture_source_t *out,
                                             unsigned capacity);
/* Entity slot lifetime (free / reinitialise) and map lifetime. */
void ResetScriptModelTree(unsigned entity);
void ResetScriptModelTrees(void);
/* animmodelprobe (developer): every bound script model, its trees, the
 * wire +060/+094/+06c it publishes and its captured pose / bootstrap. */
void RegisterScriptModelTreeProbe(void);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
#endif

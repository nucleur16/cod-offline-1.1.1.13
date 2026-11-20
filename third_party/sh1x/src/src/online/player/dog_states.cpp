/* Ported T5 dog state transitions: OpenBLOPS bg_dog_animations_mp.cpp.
 * Every legacy reference below resolves by animation name into the actual C1
 * tree; no T5 animation index is written to native XAnim or the wire.
 * Staged for parent review. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "dog_states.h"
#include "dog_animation.h"
#include "model_animation.h"
#include "dobj.h"
#include "xassets/xanims.h"
#include <math.h>
#include <string.h>
#include <limits.h>
extern "C" XAnim_s *Scr_GetAnims(unsigned);
struct dog_state_context {
    DObj *obj; unsigned entity,generation,time,tree;
    float rate,lean,aimUp,aimLeft;
    unsigned refs[69],count,notifyIndex;
    int failed;
    actor_anim_event_t events[128];
};
enum {
ACTOR_ANIMATION_IDLE = 0,
ACTOR_ANIMATION_MOVE_RUN = 1,
ACTOR_ANIMATION_MOVE_START = 2,
ACTOR_ANIMATION_MOVE_STOP = 3,
ACTOR_ANIMATION_MOVE_WALK = 4,
ACTOR_ANIMATION_MOVE_TURN_LEFT = 5,
ACTOR_ANIMATION_MOVE_TURN_RIGHT = 6,
ACTOR_ANIMATION_MOVE_RUN_TURN_LEFT = 7,
ACTOR_ANIMATION_MOVE_RUN_TURN_RIGHT = 8,
ACTOR_ANIMATION_MOVE_TURN_AROUND_LEFT = 9,
ACTOR_ANIMATION_MOVE_TURN_AROUND_RIGHT = 10,
ACTOR_ANIMATION_MOVE_RUN_TURN_AROUND_LEFT = 11,
ACTOR_ANIMATION_MOVE_RUN_TURN_AROUND_RIGHT = 12,
ACTOR_ANIMATION_MOVE_ATTACK = 13,
ACTOR_ANIMATION_STOP_IDLE = 14,
ACTOR_ANIMATION_STOP_ATTACKIDLE = 15,
ACTOR_ANIMATION_STOP_ATTACKIDLE_BARK = 16,
ACTOR_ANIMATION_STOP_ATTACKIDLE_GROWL = 17,
ACTOR_ANIMATION_PAIN_MAIN = 18,
ACTOR_ANIMATION_PAIN_FRONT = 19,
ACTOR_ANIMATION_PAIN_BACK = 20,
ACTOR_ANIMATION_PAIN_LEFT = 21,
ACTOR_ANIMATION_PAIN_RIGHT = 22,
ACTOR_ANIMATION_PAIN_RUN_FRONT = 23,
ACTOR_ANIMATION_PAIN_RUN_BACK = 24,
ACTOR_ANIMATION_PAIN_RUN_LEFT = 25,
ACTOR_ANIMATION_PAIN_RUN_RIGHT = 26,
ACTOR_ANIMATION_DEATH_FRONT = 27,
ACTOR_ANIMATION_DEATH_BACK = 28,
ACTOR_ANIMATION_DEATH_LEFT = 29,
ACTOR_ANIMATION_DEATH_RIGHT = 30,
ACTOR_ANIMATION_FLASHED = 31,
ACTOR_ANIMATION_COMBAT_ATTACKIDLE = 32,
ACTOR_ANIMATION_COMBAT_ATTACKIDLE_BARK = 33,
ACTOR_ANIMATION_COMBAT_ATTACKIDLE_GROWL = 34,
ACTOR_ANIMATION_COMBAT_ATTACK_RUN = 35,
ACTOR_ANIMATION_COMBAT_ATTACK_PLAYER_CLOSE_RANGE = 36,
ACTOR_ANIMATION_COMBAT_ATTACK_MISS = 37,
ACTOR_ANIMATION_COMBAT_ATTACK_MISS_LEFT = 38,
ACTOR_ANIMATION_COMBAT_ATTACK_MISS_RIGHT = 39,
ACTOR_ANIMATION_TRAVERSE_JUMP_UP_40 = 40,
ACTOR_ANIMATION_TRAVERSE_JUMP_UP_80 = 41,
ACTOR_ANIMATION_TRAVERSE_JUMP_DOWN_40 = 42,
ACTOR_ANIMATION_TRAVERSE_JUMP_DOWN_80 = 43,
ACTOR_ANIMATION_TRAVERSE_WALLHOP = 44,
ACTOR_ANIMATION_TRAVERSE_WINDOW = 45,
ACTOR_ANIMATION_TRAVERSE_THROUGH_HOLE_42 = 46,
/* Added by this port. maps\mpnimscripts\dog_combat.gsc reaches these five
 * through the synced-melee/instant-kill branch. OpenBLOPS bg_dog has no rows
 * for them (T5 drives that sequence from its own melee code), but the clips
 * they need are all declared by C1's dog tree and IW4's animscripts\dog * dog_combat drives exactly this set, so they are implemented instead of
 * failing the first time the branch is taken. */
ACTOR_ANIMATION_COMBAT_ATTACK_PLAYER = 47,
ACTOR_ANIMATION_COMBAT_ATTACK_PLAYER_EARLY = 48,
ACTOR_ANIMATION_COMBAT_ATTACK_PLAYER_LUNGE = 49,
ACTOR_ANIMATION_COMBAT_PLAYER_NECK_SNAP = 50,
ACTOR_ANIMATION_COMBAT_PLAYER_NECK_MISS = 51,
/* Added for the zm hellhound (2026-10-15): T5 animscripts/zombie_dog_death.gsc
 * :383-395 plays one of five tesla deaths when self.tesla_death is set. The
 * clips are in common_zombie.animlist's "=zombie_dog" group. */
ACTOR_ANIMATION_DEATH_TESLA_A = 52,
ACTOR_ANIMATION_DEATH_TESLA_B = 53,
ACTOR_ANIMATION_DEATH_TESLA_C = 54,
ACTOR_ANIMATION_DEATH_TESLA_D = 55,
ACTOR_ANIMATION_DEATH_TESLA_E = 56,
ACTOR_ANIMATION_COUNT = 57
};
/* Clip indices for the four clips appended to clipNames below. */
enum {
DOG_CLIP_ATTACK_PLAYER = 60,
DOG_CLIP_ATTACK_PLAYER_LATE = 61,
DOG_CLIP_PLAYER_NECK_SNAP = 62,
DOG_CLIP_PLAYER_NECK_MISS = 63,
DOG_CLIP_DEATH_TESLA_A = 64,
DOG_CLIP_COUNT = 69
};
/* Clip table, bound BY NAME in the actual C1 dog tree.
 *
 * C1 1.1.1.13 ships `animtrees/dog.atr` as rawfile #206 of `common_mp.zone`
 * (1980 bytes), and that file is byte-identical to the IW4 dog tree - it is
 * NOT T5's. Every entry below is therefore checked against the 47 nodes that
 * tree really declares. The T5 clips OpenBLOPS bg_dog_animations_mp.cpp names
 * for turns, directional pain, directional death, the low run attack and
 * traverse_down_80 simply do not exist in C1 (the T5 spelling is also
 * "german_shepard", one 'h', which never appears in any C1 zone), so each one
 * is replaced by the nearest clip the C1 tree does declare. Binding is
 * all-or-nothing per apply, so a single unbound name would silently disable
 * EVERY dog animation; substitutions are made here, once, in the open.
 *
 * substitution                       reason
 *   turn 90 L/R  -> run_start_L/R    C1 bakes turn-in-place into the run
 *   turn 180 L/R -> run_start_180_*  starts, exactly as IW4's dog_move does
 *   pain_* (all) -> run_pain         run_pain is C1's only pain clip
 *   death_* (all)-> death_front      death_front is C1's only death clip
 *   run_attack_low -> attack_player  C1's in-place player bite
 *   traverse_down_80 -> _down_40     C1 declares up_40/up_80/down_40 only
 *
 * Re-audited 2026-09-16 against docs/zone_assets/ALL_xanim.txt (every xanim
 * in every mounted zone): the MP zones (common_gametype_mp, mp_*) ship
 * exactly the 37 german_shepherd_* clips the 47-node tree names, nothing
 * more. The T5-style variety (turn_90/180 in place, pain_hit_front/back/
 * left/right, run_pain_front, death_hit_back/left/right, run_attack_low,
 * traverse_down_96/126/190, trot) exists only as zombie_dog_* xanims in
 * common_gametype_zm with animtrees/zombie_dog.atr, which no MP zone mounts
 * and which the MP dog model is not rigged for. So the substitutions above
 * are the ceiling for MP; extending dog.atr server-side would not help
 * because C1 resolves the same tree from common_mp.zone and the indices
 * must agree. 31 of the 52 states run their real T5 clip; 21 (8 turns to
 * run_start_L/R/180_L/180_R, 8 pains to run_pain, 3 deaths to death_front,
 * the close-range bite, traverse_down_80) run the nearest MP clip. */
static const char *const clipNames[DOG_CLIP_COUNT] = {
"root",
"german_shepherd_idle",
"german_shepherd_walk",
"german_shepherd_run_stop",
"german_shepherd_run_pain",
"german_shepherd_run_attack",
"german_shepherd_run_start_L",           /* T5 run_turn_90_left */
"german_shepherd_run_start_R",           /* T5 run_turn_90_right */
"german_shepherd_run_start_L",           /* T5 turn_90_left */
"german_shepherd_run_start_R",           /* T5 turn_90_right */
"german_shepherd_run_start_180_L",       /* T5 run_turn_180_left */
"german_shepherd_run_start_180_R",       /* T5 run_turn_180_right */
"german_shepherd_run_start_180_L",
"german_shepherd_run_start_180_R",
"german_shepherd_run_pain",              /* T5 run_pain_hit_front */
"german_shepherd_run_pain",              /* T5 run_pain_hit_back */
"german_shepherd_run_pain",              /* T5 run_pain_hit_left */
"german_shepherd_run_pain",              /* T5 run_pain_hit_right */
"german_shepherd_run_pain",              /* T5 pain_hit_front */
"german_shepherd_run_pain",              /* T5 pain_hit_back */
"german_shepherd_run_pain",              /* T5 pain_hit_left */
"german_shepherd_run_pain",              /* T5 pain_hit_right */
"german_shepherd_death_front",
"german_shepherd_death_front",           /* T5 death_hit_back */
"german_shepherd_death_front",           /* T5 death_hit_left */
"german_shepherd_death_front",           /* T5 death_hit_right */
"german_shepherd_run_flashbang",
"german_shepherd_attack_player_miss",
"german_shepherd_attack_player_miss_turnL",
"german_shepherd_attack_player_miss_turnR",
"german_shepherd_attack_player",         /* T5 run_attack_low */
"german_shepherd_traverse_up_40",
"german_shepherd_traverse_up_80",
"german_shepherd_traverse_down_40",
"german_shepherd_traverse_down_40",      /* T5 traverse_down_80 */
"german_shepherd_run_jump_40",
"german_shepherd_run_jump_window_40",
"german_shepherd_walk",
"german_shepherd_run_start_knob",
"german_shepherd_run_knob",
"german_shepherd_attackidle_knob",
"german_shepherd_look_2",
"german_shepherd_look_8",
"german_shepherd_look_4",
"german_shepherd_look_6",
"german_shepherd_run_start",
"german_shepherd_run",
"german_shepherd_run_lean_L",
"german_shepherd_run_lean_R",
"german_shepherd_attackidle",
"german_shepherd_attackidle_bark",
"german_shepherd_attackidle_growl",
"german_shepherd_look_down",
"german_shepherd_attack_look_down",
"german_shepherd_look_up",
"german_shepherd_attack_look_up",
"german_shepherd_look_right",
"german_shepherd_attack_look_right",
"german_shepherd_look_left",
"german_shepherd_attack_look_left",
/* The synced-melee clips T5's bg_dog table has no slot for. All four are
 * declared by C1's dog tree and IW4's animscripts\dog\dog_combat drives
 * exactly this set, so the states below are implemented rather than left to
 * fail the first time a script reaches them. */
"german_shepherd_attack_player",
"german_shepherd_attack_player_late",
"german_shepherd_player_neck_snap",
"german_shepherd_player_neck_miss",
/* The hellhound's five tesla deaths; the MP dog has no tesla weapon and its
 * tree declares one death, which stands in so binding stays total. */
"german_shepherd_death_front",
"german_shepherd_death_front",
"german_shepherd_death_front",
"german_shepherd_death_front",
"german_shepherd_death_front"
};
/* The zm hellhound's clip table: the same DOG_CLIP_COUNT slots, bound BY NAME in C1's
 * animtrees/zombie_dog.atr (rawfile of common_gametype_zm, 2278 bytes,
 * composed at actor selector 4 / serverinfo-2482 bit 0x10, C1 0x4083f0).
 *
 * THE BINDABLE SET IS THE ANIMLIST, NOT THE .atr. C1 builds the "zombie_dog"
 * tree from common_zombie.animlist's "=zombie_dog" group (60 entries,
 * MEASURED 2026-10-07), and the server registers the same entries
 * (RegisterServerAnimListReferences) so the tree indices agree; a node
 * the .atr declares but the group does not list (walk, run_start and its
 * knob, run_start_l/r/180, traverse_down_96, run_flashbang, run_pain) is in
 * neither tree. MEASURED on the first probe: binding "zombie_dog_walk"
 * failed. Every slot below therefore names a group entry; the substitutions
 * are commented on their rows. The turn slots take T5's own turn clips,
 * which sit under zombie_dog_turn_knob: the turn states set them with the
 * complete flag, which weights their parent too. */
static const char *const zombieDogClipNames[DOG_CLIP_COUNT] = {
"root",
"zombie_dog_idle",
"zombie_dog_trot",                    /* walk */
"zombie_dog_run_stop",
"zombie_dog_run_pain_front",          /* run_pain */
"zombie_dog_run_attack",
"zombie_dog_run_turn_90_left",
"zombie_dog_run_turn_90_right",
"zombie_dog_turn_90_left",
"zombie_dog_turn_90_right",
"zombie_dog_run_turn_180_left",
"zombie_dog_run_turn_180_right",
"zombie_dog_turn_180_left",
"zombie_dog_turn_180_right",
"zombie_dog_run_pain_front",          /* run_pain_hit_front */
"zombie_dog_run_pain_front",          /* run_pain_hit_back */
"zombie_dog_run_pain_front",          /* run_pain_hit_left */
"zombie_dog_run_pain_front",          /* run_pain_hit_right */
"zombie_dog_pain_hit_front",
"zombie_dog_pain_hit_back",
"zombie_dog_pain_hit_left",
"zombie_dog_pain_hit_right",
"zombie_dog_death_front",
"zombie_dog_death_hit_back",
"zombie_dog_death_hit_left",
"zombie_dog_death_hit_right",
"zombie_dog_pain_hit_front",          /* run_flashbang */
"zombie_dog_run_attack_miss",         /* attack_player_miss */
"zombie_dog_attack_player_miss_turnl",
"zombie_dog_attack_player_miss_turnr",
"zombie_dog_run_attack_low",
"zombie_dog_traverse_up_40",
"zombie_dog_traverse_up_80",
"zombie_dog_traverse_down_40",
"zombie_dog_traverse_down_126",       /* T5 traverse_down_80 */
"zombie_dog_run_jump_40",
"zombie_dog_run_jump_window_40",
"zombie_dog_trot",
"zombie_dog_run_knob",                /* run_start_knob */
"zombie_dog_run_knob",
"zombie_dog_attackidle_knob",
"zombie_dog_look_2",
"zombie_dog_look_8",
"zombie_dog_look_4",
"zombie_dog_look_6",
"zombie_dog_run",                     /* run_start */
"zombie_dog_run",
"zombie_dog_run_lean_l",
"zombie_dog_run_lean_r",
"zombie_dog_attackidle",
"zombie_dog_attackidle_bark",
"zombie_dog_attackidle_growl",
"zombie_dog_look_down",
"zombie_dog_attack_look_down",
"zombie_dog_look_up",
"zombie_dog_attack_look_up",
"zombie_dog_look_right",
"zombie_dog_attack_look_right",
"zombie_dog_look_left",
"zombie_dog_attack_look_left",
"zombie_dog_run_attack_low",          /* attack_player */
"zombie_dog_run_attack",              /* attack_player_late */
"zombie_dog_run_attack",              /* player_neck_snap */
"zombie_dog_run_attack_miss",         /* player_neck_miss */
"zombie_dog_tesla_death_a",
"zombie_dog_tesla_death_b",
"zombie_dog_tesla_death_c",
"zombie_dog_tesla_death_d",
"zombie_dog_tesla_death_e"
};
static void goal(dog_state_context *ctx,unsigned ref,float weight,float blend,float rate,
    unsigned notify,int restart,unsigned type,int knob) {
    if(ctx->count>=128 || ref>=DOG_CLIP_COUNT){ctx->failed=1;return;}
    actor_anim_event_t *e=&ctx->events[ctx->count];
    memset(e,0,sizeof(*e));e->generation=ctx->generation;
    e->command.entity=ctx->entity;e->command.time=ctx->time;e->command.ref=ctx->refs[ref];
    e->command.type=type;e->command.goal_weight=weight;e->command.blend=blend;
    e->command.playback_rate=rate;e->command.arg2=(restart?2:0)|(knob?1:0);
    if(notify){if(ctx->notifyIndex!=UINT_MAX)ctx->failed=1;ctx->notifyIndex=ctx->count;}
    ++ctx->count;
}
static void clearGoal(dog_state_context *c,XAnimTree_s *,unsigned ref,float blend,int) {
    goal(c,ref,0,blend,0,0,0,1,0);
}
static void setGoal(dog_state_context *c,DObj *,unsigned ref,float weight,float blend,float rate,unsigned notify,unsigned,int restart,int) {
    goal(c,ref,weight,blend,rate,notify,restart,3,0);
}
/* (3,1) = plain + complete, NOT (4,0). Under C1's encoding (type is
 * plain/knob, arg2 bit0 is complete -- see dog_animation.c apply_batch
 * and C1 0x443e90) the old (4,0) was a KNOB with no complete flag, so every
 * completeGoal diverged from what the client performed. setGoal (3,0) and
 * completeKnob (4,1) mean the same under both readings and are unchanged. */
static void completeGoal(dog_state_context *c,DObj *,unsigned ref,float weight,float blend,float rate,unsigned notify,unsigned,int restart,int) {
    goal(c,ref,weight,blend,rate,notify,restart,3,1);
}
static void completeKnob(dog_state_context *c,DObj *,unsigned ref,float weight,float blend,float rate,unsigned notify,unsigned,int restart,int) {
    goal(c,ref,weight,blend,rate,notify,restart,4,1);
}
static void BG_Dog_Look_At(dog_state_context *ctx);
static void BG_Dog_Move_Start(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Stop(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Run(dog_state_context *ctx, int prevState);
static void BG_Dog_GetRunAnimWeights(dog_state_context *ctx, float leanAmount, float *center, float *left, float *right);
static void BG_Dog_Init_Normal_Look_At(dog_state_context *ctx);
static void BG_Dog_Clear_Look_At(dog_state_context *ctx);
static void BG_Dog_Move_Run_Think(dog_state_context *ctx);
static void BG_Dog_Move_Walk(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Walk_Think(dog_state_context *ctx);
static void BG_Dog_Move_Turn_Left(dog_state_context *ctx, int prevState);
static void BG_Dog_Clear_All_Turns(dog_state_context *ctx, DObj *obj);
static void BG_Dog_Move_Turn_Right(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Run_Turn_Left(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Run_Turn_Right(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Turn_Around_Left(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Turn_Around_Right(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Run_Turn_Around_Left(dog_state_context *ctx, int prevState);
static void BG_Dog_Move_Run_Turn_Around_Right(dog_state_context *ctx, int prevState);
static void BG_Dog_Stop_Idle(dog_state_context *ctx, int prevState);
static void BG_Dog_Stop_Init(dog_state_context *ctx);
static void BG_Dog_Init_Attack_Look_At(dog_state_context *ctx);
static void BG_Dog_Stop_AttackIdle(dog_state_context *ctx, int prevState);
static void BG_Dog_Stop_AttackIdle_Bark(dog_state_context *ctx, int prevState);
static void BG_Dog_Stop_AttackIdle_Growl(dog_state_context *ctx, int prevState);
static void BG_Dog_Pain_Main(dog_state_context *ctx, int prevState);
static void BG_Dog_Pain_Front(dog_state_context *ctx, int prevState);
static void BG_Dog_Pain_Back(dog_state_context *ctx, int prevState);
static void BG_Dog_Pain_Left(dog_state_context *ctx, int prevState);
static void BG_Dog_Pain_Right(dog_state_context *ctx, int prevState);
static void BG_Dog_Run_Pain_Front(dog_state_context *ctx, int prevState);
static void BG_Dog_Run_Pain_Back(dog_state_context *ctx, int prevState);
static void BG_Dog_Run_Pain_Left(dog_state_context *ctx, int prevState);
static void BG_Dog_Run_Pain_Right(dog_state_context *ctx, int prevState);
static void BG_Dog_Death_Front(dog_state_context *ctx, int prevState);
static void BG_Dog_Death_Back(dog_state_context *ctx, int prevState);
static void BG_Dog_Death_Left(dog_state_context *ctx, int prevState);
static void BG_Dog_Death_Right(dog_state_context *ctx, int prevState);
static void BG_Dog_Flashed(dog_state_context *ctx, int prevState);
static void BG_Dog_Combat_AttackIdle(dog_state_context *ctx, int prevState);
static void BG_Dog_Combat_AttackIdle_Bark(dog_state_context *ctx, int prevState);
static void BG_Dog_Combat_AttackIdle_Growl(dog_state_context *ctx, int prevState);
static void BG_Dog_Combat_Attack_Run(dog_state_context *ctx, int prevState);
static void BG_Dog_Combat_Attack_Player_Close_Range(dog_state_context *ctx, int prevState);
static void BG_Dog_Combat_Attack_Miss(dog_state_context *ctx, int prevState);
static void BG_Dog_Combat_Attack_Miss_Left(dog_state_context *ctx, int prevState);
static void BG_Dog_Combat_Attack_Miss_Right(dog_state_context *ctx, int prevState);
static void BG_Dog_Clear_Traverse_Anims(dog_state_context *ctx, int prevState);
static void BG_Dog_Traverse_Jump_Up_40(dog_state_context *ctx, int prevState);
static void BG_Dog_Traverse_Jump_Up_80(dog_state_context *ctx, int prevState);
static void BG_Dog_Traverse_Jump_Down_40(dog_state_context *ctx, int prevState);
static void BG_Dog_Traverse_Jump_Down_80(dog_state_context *ctx, int prevState);
static void BG_Dog_Traverse_Wallhop(dog_state_context *ctx, int prevState);
static void BG_Dog_Traverse_Window(dog_state_context *ctx, int prevState);
static void BG_Dog_Traverse_Through_Hole_42(dog_state_context *ctx, int prevState);

static void BG_Dog_Look_At(dog_state_context *ctx)
{
    float rightWeight; 
    DObj *obj; 
    float downWeight; 
    float leftWeight; 
    float upWeight; 

    obj = ctx->obj;
    if ( obj )
    {
        upWeight = 0.0f;
        downWeight = 0.0f;
        leftWeight = 0.0f;
        rightWeight = 0.0f;
        if ( ctx->aimUp <= 0.0 )
            downWeight = -1.0 * ctx->aimUp;
        else
            upWeight = ctx->aimUp;
        if ( ctx->aimLeft <= 0.0 )
            rightWeight = -1.0 * ctx->aimLeft;
        else
            leftWeight = ctx->aimLeft;
        setGoal(ctx, obj, 0x2Au, upWeight, 0.050000001, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x29u, downWeight, 0.050000001, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x2Cu, leftWeight, 0.050000001, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x2Bu, rightWeight, 0.050000001, ctx->rate, 0, 0, 0, -1);
    }
}

static void BG_Dog_Move_Start(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.0, -1);
        completeGoal(ctx, obj, 0x2Du, 1.0, 0.2, 1.0, 0, 0, 1, -1);
        completeKnob(ctx, 
            obj,
            0x26u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Move_Stop(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0x2Du, 0.0, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 0x27u, 0.2, -1);
        completeGoal(ctx, 
            obj,
            3u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Move_Run(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v4; 
    XAnimTree_s *v5; 
    XAnimTree_s *v6; 
    XAnimTree_s *v7; 
    DObj *obj; 
    float right; 
    float left; 
    float center; 

    obj = ctx->obj;
    if (obj)
    {
        if (prevState != ACTOR_ANIMATION_MOVE_WALK)
        {
            Tree = DObjGetTree(obj);
            clearGoal(ctx, Tree, 0x2Du, 0.0, -1);
            v4 = DObjGetTree(obj);
            clearGoal(ctx, v4, 3u, 0.0, -1);
            v5 = DObjGetTree(obj);
            clearGoal(ctx, v5, 0xAu, 0.2, -1);
            v6 = DObjGetTree(obj);
            clearGoal(ctx, v6, 0xBu, 0.2, -1);
            BG_Dog_Clear_Traverse_Anims(ctx, prevState);
            BG_Dog_Init_Normal_Look_At(ctx);
        }
        center = 0.0f;
        left = 0.0f;
        right = 0.0f;
        BG_Dog_GetRunAnimWeights(ctx, ctx->lean, &center, &left, &right);
        v7 = DObjGetTree(obj);
        clearGoal(ctx, v7, 2u, 0.30000001, -1);
        completeGoal(ctx, obj, 0x2Eu, center, 0.2, 1.0, 0, 0, 0, -1);
        completeGoal(ctx, obj, 0x2Fu, left, 0.2, 1.0, 0, 0, 0, -1);
        completeGoal(ctx, obj, 0x30u, right, 0.2, 1.0, 0, 0, 0, -1);
        completeKnob(ctx, 
            obj,
            0x27u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            0,
            -1);
        BG_Dog_Look_At(ctx);
    }
}

static void BG_Dog_GetRunAnimWeights(dog_state_context *ctx, float leanAmount, float *center, float *left, float *right)
{
    if (!isfinite(leanAmount)) { ctx->failed = 1; return; }
    if ( leanAmount <= 0.0 )
    {
        if ( leanAmount < 0.0 && leanAmount > -0.94999999 )
            leanAmount = -0.95f;
    }
    else if ( leanAmount < 0.94999999 )
    {
        leanAmount = 0.95f;
    }
    if ( leanAmount <= 0.0 )
    {
        if ( leanAmount >= 0.0 )
        {
            *left = 0.0f;
            *right = 0.0f;
            *center = 1.0f;
        }
        else
        {
            *right = 0.0f;
            *left = (float)(leanAmount + 1.0) * 20.0;
            if ( *left > 1.0 )
                *left = 1.0f;
            if ( *left < 0.0 )
                *left = 0.0f;
            *center = 1.0 - *left;
        }
    }
    else
    {
        *left = 0.0f;
        *right = (float)(1.0 - leanAmount) * 20.0;
        if ( *right <= 1.0 )
        {
            if ( *right < 0.0 )
                *right = 0.0f;
        }
        else
        {
            *right = 1.0f;
        }
        *center = 1.0 - *right;
    }
}

static void BG_Dog_Init_Normal_Look_At(dog_state_context *ctx)
{
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        BG_Dog_Clear_Look_At(ctx);
        setGoal(ctx, obj, 0x36u, 1.0, 0.0, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x34u, 1.0, 0.0, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x3Au, 1.0, 0.0, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x38u, 1.0, 0.0, ctx->rate, 0, 0, 0, -1);
    }
}

static void BG_Dog_Clear_Look_At(dog_state_context *ctx)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    XAnimTree_s *v4; 
    XAnimTree_s *v5; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0x29u, 0.0, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 0x2Au, 0.0, -1);
        v4 = DObjGetTree(obj);
        clearGoal(ctx, v4, 0x2Cu, 0.0, -1);
        v5 = DObjGetTree(obj);
        clearGoal(ctx, v5, 0x2Bu, 0.0, -1);
    }
}

static void BG_Dog_Move_Run_Think(dog_state_context *ctx)
{
    XAnimTree_s *Tree; 
    DObj *obj; 
    float right; 
    float left; 
    float center; 

    obj = ctx->obj;
    if ( obj )
    {
        center = 0.0f;
        left = 0.0f;
        right = 0.0f;
        BG_Dog_GetRunAnimWeights(ctx, ctx->lean, &center, &left, &right);
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 2u, 0.30000001, -1);
        completeGoal(ctx, obj, 0x2Eu, center, 0.2, 1.0, 0, 0, 0, -1);
        completeGoal(ctx, obj, 0x2Fu, left, 0.2, 1.0, 0, 0, 0, -1);
        completeGoal(ctx, obj, 0x30u, right, 0.2, 1.0, 0, 0, 0, -1);
        completeKnob(ctx, 
            obj,
            0x27u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            0,
            -1);
        BG_Dog_Look_At(ctx);
    }
}

static void BG_Dog_Move_Walk(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v4; 
    XAnimTree_s *v5; 
    XAnimTree_s *v6; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        if ( prevState != ACTOR_ANIMATION_MOVE_RUN )
        {
            Tree = DObjGetTree(obj);
            clearGoal(ctx, Tree, 0x2Du, 0.0, -1);
            v4 = DObjGetTree(obj);
            clearGoal(ctx, v4, 3u, 0.0, -1);
            v5 = DObjGetTree(obj);
            clearGoal(ctx, v5, 0xAu, 0.2, -1);
            v6 = DObjGetTree(obj);
            clearGoal(ctx, v6, 0xBu, 0.2, -1);
            BG_Dog_Clear_Traverse_Anims(ctx, prevState);
        }
        /* T5 zombie_dog_move.gsc restarts the trot once, when the move
         * script starts (main :45 SetFlaggedAnimRestart), and its moveLoop
         * re-sets it every 0.2 s WITHOUT a restart (:99 setflaggedanim).
         * The hellhound's moveLoop re-enters move_walk every 0.2 s, so the
         * restart is for entering from another state only. MEASURED (t5cap
         * dog clip timeline, 2026-10-22): T5's trot ran whole 1.8 s cycles;
         * CoDOL's restarted every 0.2 s and never got past 0.11 -- a hound
         * that slid along in the trot's first frames. */
        completeGoal(ctx, 
            obj,
            2u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            prevState != ACTOR_ANIMATION_MOVE_WALK,
            -1);
    }
}

static void BG_Dog_Move_Walk_Think(dog_state_context *ctx)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0x27u, 0.30000001, -1);
        /* No restart: the think runs every server frame (above). */
        completeGoal(ctx, 
            obj,
            2u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            0,
            -1);
    }
}

static void BG_Dog_Move_Turn_Left(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.2, -1);
        BG_Dog_Clear_All_Turns(ctx, obj);
        completeGoal(ctx, 
            obj,
            8u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Clear_All_Turns(dog_state_context *ctx, DObj *obj)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v2; 
    XAnimTree_s *v3; 
    XAnimTree_s *v4; 
    XAnimTree_s *v5; 
    XAnimTree_s *v6; 
    XAnimTree_s *v7; 
    XAnimTree_s *v8; 

    Tree = DObjGetTree(obj);
    clearGoal(ctx, Tree, 8u, 0.0, -1);
    v2 = DObjGetTree(obj);
    clearGoal(ctx, v2, 9u, 0.0, -1);
    v3 = DObjGetTree(obj);
    clearGoal(ctx, v3, 6u, 0.0, -1);
    v4 = DObjGetTree(obj);
    clearGoal(ctx, v4, 7u, 0.0, -1);
    v5 = DObjGetTree(obj);
    clearGoal(ctx, v5, 0xCu, 0.0, -1);
    v6 = DObjGetTree(obj);
    clearGoal(ctx, v6, 0xDu, 0.0, -1);
    v7 = DObjGetTree(obj);
    clearGoal(ctx, v7, 0xAu, 0.0, -1);
    v8 = DObjGetTree(obj);
    clearGoal(ctx, v8, 0xBu, 0.0, -1);
}

static void BG_Dog_Move_Turn_Right(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.2, -1);
        BG_Dog_Clear_All_Turns(ctx, obj);
        completeGoal(ctx, 
            obj,
            9u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Move_Run_Turn_Left(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.2, -1);
        BG_Dog_Clear_All_Turns(ctx, obj);
        completeGoal(ctx, 
            obj,
            6u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Move_Run_Turn_Right(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.2, -1);
        BG_Dog_Clear_All_Turns(ctx, obj);
        completeGoal(ctx, 
            obj,
            7u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Move_Turn_Around_Left(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.2, -1);
        BG_Dog_Clear_All_Turns(ctx, obj);
        completeGoal(ctx, 
            obj,
            0xCu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Move_Turn_Around_Right(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.2, -1);
        BG_Dog_Clear_All_Turns(ctx, obj);
        completeGoal(ctx, 
            obj,
            0xDu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Move_Run_Turn_Around_Left(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.2, -1);
        BG_Dog_Clear_All_Turns(ctx, obj);
        completeGoal(ctx, 
            obj,
            0xAu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Move_Run_Turn_Around_Right(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 3u, 0.2, -1);
        BG_Dog_Clear_All_Turns(ctx, obj);
        completeGoal(ctx, 
            obj,
            0xBu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Stop_Idle(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        if ( prevState < ACTOR_ANIMATION_STOP_IDLE || prevState > ACTOR_ANIMATION_STOP_ATTACKIDLE_GROWL )
            BG_Dog_Stop_Init(ctx);
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0x28u, 0.2, -1);
        completeGoal(ctx, 
            obj,
            1u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Stop_Init(dog_state_context *ctx)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v3; 
    XAnimTree_s *v4; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        v3 = DObjGetTree(obj);
        clearGoal(ctx, v3, 1u, 0.2, -1);
        v4 = DObjGetTree(obj);
        clearGoal(ctx, v4, 0x28u, 0.2, -1);
        BG_Dog_Init_Attack_Look_At(ctx);
    }
}

static void BG_Dog_Init_Attack_Look_At(dog_state_context *ctx)
{
    DObj *obj; 

    obj = ctx->obj;

    if ( obj )
    {
        BG_Dog_Clear_Look_At(ctx);
        setGoal(ctx, obj, 0x37u, 1.0, 0.0, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x35u, 1.0, 0.0, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x3Bu, 1.0, 0.0, ctx->rate, 0, 0, 0, -1);
        setGoal(ctx, obj, 0x39u, 1.0, 0.0, ctx->rate, 0, 0, 0, -1);
    }
}

static void BG_Dog_Stop_AttackIdle(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v4; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        if ( prevState < ACTOR_ANIMATION_STOP_IDLE || prevState > ACTOR_ANIMATION_STOP_ATTACKIDLE_GROWL )
            BG_Dog_Stop_Init(ctx);
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 1u, 0.2, -1);
        v4 = DObjGetTree(obj);
        clearGoal(ctx, v4, 0x28u, 0.1, -1);
        completeGoal(ctx, 
            obj,
            0x31u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Stop_AttackIdle_Bark(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v4; 
    DObj *obj; 

    obj = ctx->obj;

    if ( obj )
    {
        if ( prevState < ACTOR_ANIMATION_STOP_IDLE || prevState > ACTOR_ANIMATION_STOP_ATTACKIDLE_GROWL )
            BG_Dog_Stop_Init(ctx);
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 1u, 0.2, -1);
        v4 = DObjGetTree(obj);
        clearGoal(ctx, v4, 0x28u, 0.1, -1);
        completeGoal(ctx, 
            obj,
            0x32u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Stop_AttackIdle_Growl(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v4; 
    DObj *obj; 

    obj = ctx->obj;

    if ( obj )
    {
        if ( prevState < ACTOR_ANIMATION_STOP_IDLE || prevState > ACTOR_ANIMATION_STOP_ATTACKIDLE_GROWL )
            BG_Dog_Stop_Init(ctx);
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 1u, 0.2, -1);
        v4 = DObjGetTree(obj);
        clearGoal(ctx, v4, 0x28u, 0.1, -1);
        completeGoal(ctx, 
            obj,
            0x33u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Pain_Main(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;

    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            4u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Pain_Front(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;

    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x12u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Pain_Back(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x13u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Pain_Left(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x14u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Pain_Right(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;

    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x15u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Run_Pain_Front(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0xEu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Run_Pain_Back(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0xFu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Run_Pain_Left(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x10u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Run_Pain_Right(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if ( obj )
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x11u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Death_Front(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x16u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Death_Back(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x17u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Death_Left(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x18u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Death_Right(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x19u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Flashed(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x1Au,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Combat_AttackIdle(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x31u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Combat_AttackIdle_Bark(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x32u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Combat_AttackIdle_Growl(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x33u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Combat_Attack_Run(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            5u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Combat_Attack_Player_Close_Range(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.1, -1);
        completeGoal(ctx, 
            obj,
            0x1Eu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Combat_Attack_Miss(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.1, -1);
        completeGoal(ctx, 
            obj,
            0x1Bu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
        BG_Dog_Init_Normal_Look_At(ctx);
    }
}

static void BG_Dog_Combat_Attack_Miss_Left(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.1, -1);
        completeGoal(ctx, 
            obj,
            0x1Cu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Combat_Attack_Miss_Right(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.1, -1);
        completeGoal(ctx, 
            obj,
            0x1Du,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Clear_Traverse_Anims(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    XAnimTree_s *v4; 
    XAnimTree_s *v5; 
    XAnimTree_s *v6; 
    XAnimTree_s *v7; 
    XAnimTree_s *v8; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj
        && (prevState == ACTOR_ANIMATION_TRAVERSE_JUMP_UP_40
            || prevState == ACTOR_ANIMATION_TRAVERSE_JUMP_UP_80
            || prevState == ACTOR_ANIMATION_TRAVERSE_JUMP_DOWN_40
            || prevState == ACTOR_ANIMATION_TRAVERSE_JUMP_DOWN_80
            || prevState == ACTOR_ANIMATION_TRAVERSE_WALLHOP
            || prevState == ACTOR_ANIMATION_TRAVERSE_WINDOW
            || prevState == ACTOR_ANIMATION_TRAVERSE_THROUGH_HOLE_42))
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0x1Fu, 0.30000001, -1);
        v4 = DObjGetTree(obj);
        clearGoal(ctx, v4, 0x20u, 0.30000001, -1);
        v5 = DObjGetTree(obj);
        clearGoal(ctx, v5, 0x21u, 0.30000001, -1);
        v6 = DObjGetTree(obj);
        clearGoal(ctx, v6, 0x22u, 0.30000001, -1);
        v7 = DObjGetTree(obj);
        clearGoal(ctx, v7, 0x23u, 0.30000001, -1);
        v8 = DObjGetTree(obj);
        clearGoal(ctx, v8, 0x24u, 0.30000001, -1);
    }
}

static void BG_Dog_Traverse_Jump_Up_40(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x1Fu,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Traverse_Jump_Up_80(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x20u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Traverse_Jump_Down_40(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x21u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Traverse_Jump_Down_80(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x22u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Traverse_Wallhop(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x23u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Traverse_Window(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, 
            obj,
            0x24u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            1,
            -1);
    }
}

static void BG_Dog_Traverse_Through_Hole_42(dog_state_context *ctx, int prevState)
{
    XAnimTree_s *Tree; 
    DObj *obj; 

    obj = ctx->obj;
    if (obj)
    {
        Tree = DObjGetTree(obj);
        clearGoal(ctx, Tree, 0, 0.2, -1);
        completeGoal(ctx, obj, 0x2Eu, 1.0, 0.2, 1.0, 0, 0, 0, -1);
        completeKnob(ctx, 
            obj,
            0x27u,
            1.0,
            0.2,
            ctx->rate,
            1,
            2u,
            0,
            -1);
    }
}
/* Synced melee against a player. IW4 animscripts\dog\dog_combat is the
 * behavioural reference for each of these, because C1 ships IW4's dog tree:
 *   dog_combat.gsc:475-478  attack_player restarted complete, with
 *                           attack_player_late alongside it at zero blend
 *   dog_combat.gsc:318-319  "dog_early": both clips at the randomised rate
 *   dog_combat.gsc:327      "dog_lunge": attack_player alone at that rate
 *   dog_combat.gsc:860/902  the neck snap/miss, knob-restarted at .2 blend
 * Each is a transition, not a per-frame call: repeating one restarts the clip.
 * The final goal of each carries the completion notification the script's
 * DoNoteTracks("done") waits on. */
static void BG_Dog_Combat_Attack_Player(dog_state_context *ctx, int prevState)
{
    DObj *obj = ctx->obj;
    if (!obj) return;
    clearGoal(ctx, DObjGetTree(obj), 0, 0.1, -1);
    completeGoal(ctx, obj, DOG_CLIP_ATTACK_PLAYER_LATE, 1.0, 0.0, ctx->rate, 0, 2u, 1, -1);
    completeGoal(ctx, obj, DOG_CLIP_ATTACK_PLAYER, 1.0, 0.2, ctx->rate, 1, 2u, 1, -1);
}
static void BG_Dog_Combat_Attack_Player_Early(dog_state_context *ctx, int prevState)
{
    DObj *obj = ctx->obj;
    if (!obj) return;
    clearGoal(ctx, DObjGetTree(obj), 0, 0.1, -1);
    completeGoal(ctx, obj, DOG_CLIP_ATTACK_PLAYER_LATE, 1.0, 0.2, ctx->rate, 0, 2u, 1, -1);
    completeGoal(ctx, obj, DOG_CLIP_ATTACK_PLAYER, 1.0, 0.2, ctx->rate, 1, 2u, 1, -1);
}
static void BG_Dog_Combat_Attack_Player_Lunge(dog_state_context *ctx, int prevState)
{
    DObj *obj = ctx->obj;
    if (!obj) return;
    clearGoal(ctx, DObjGetTree(obj), 0, 0.1, -1);
    completeGoal(ctx, obj, DOG_CLIP_ATTACK_PLAYER, 1.0, 0.2, ctx->rate, 1, 2u, 1, -1);
}
static void BG_Dog_Combat_Player_Neck_Snap(dog_state_context *ctx, int prevState)
{
    DObj *obj = ctx->obj;
    if (!obj) return;
    clearGoal(ctx, DObjGetTree(obj), 0, 0.2, -1);
    completeKnob(ctx, obj, DOG_CLIP_PLAYER_NECK_SNAP, 1.0, 0.2, ctx->rate, 1, 2u, 1, -1);
}
static void BG_Dog_Combat_Player_Neck_Miss(dog_state_context *ctx, int prevState)
{
    DObj *obj = ctx->obj;
    if (!obj) return;
    clearGoal(ctx, DObjGetTree(obj), 0, 0.2, -1);
    completeKnob(ctx, obj, DOG_CLIP_PLAYER_NECK_MISS, 1.0, 0.2, ctx->rate, 1, 2u, 1, -1);
}
/* T5 animscripts/zombie_dog_death.gsc:392-395: clearanim(%root, 0.2), then
 * setflaggedanimrestart of the chosen tesla death at weight 1, blend 0.2 --
 * the same goal shape as BG_Dog_Death_Front. */
static void BG_Dog_Death_Tesla(dog_state_context *ctx, unsigned clip)
{
    DObj *obj = ctx->obj;
    if (!obj) return;
    clearGoal(ctx, DObjGetTree(obj), 0, 0.2, -1);
    completeGoal(ctx, obj, clip, 1.0, 0.2, ctx->rate, 1, 2u, 1, -1);
}
static void BG_Dog_Death_Tesla_A(dog_state_context *ctx, int) { BG_Dog_Death_Tesla(ctx, DOG_CLIP_DEATH_TESLA_A + 0); }
static void BG_Dog_Death_Tesla_B(dog_state_context *ctx, int) { BG_Dog_Death_Tesla(ctx, DOG_CLIP_DEATH_TESLA_A + 1); }
static void BG_Dog_Death_Tesla_C(dog_state_context *ctx, int) { BG_Dog_Death_Tesla(ctx, DOG_CLIP_DEATH_TESLA_A + 2); }
static void BG_Dog_Death_Tesla_D(dog_state_context *ctx, int) { BG_Dog_Death_Tesla(ctx, DOG_CLIP_DEATH_TESLA_A + 3); }
static void BG_Dog_Death_Tesla_E(dog_state_context *ctx, int) { BG_Dog_Death_Tesla(ctx, DOG_CLIP_DEATH_TESLA_A + 4); }
struct dog_state_row {const char *name; void (*enter)(dog_state_context *,int); void (*think)(dog_state_context *);};
static const dog_state_row stateRows[] = {
{"idle",NULL,NULL},
{"move_run",BG_Dog_Move_Run,BG_Dog_Move_Run_Think},
{"move_start",BG_Dog_Move_Start,NULL},
{"move_stop",BG_Dog_Move_Stop,NULL},
{"move_walk",BG_Dog_Move_Walk,BG_Dog_Move_Walk_Think},
{"move_turn_left",BG_Dog_Move_Turn_Left,NULL},
{"move_turn_right",BG_Dog_Move_Turn_Right,NULL},
{"move_run_turn_left",BG_Dog_Move_Run_Turn_Left,NULL},
{"move_run_turn_right",BG_Dog_Move_Run_Turn_Right,NULL},
{"move_turn_around_left",BG_Dog_Move_Turn_Around_Left,NULL},
{"move_turn_around_right",BG_Dog_Move_Turn_Around_Right,NULL},
{"move_run_turn_around_left",BG_Dog_Move_Run_Turn_Around_Left,NULL},
{"move_run_turn_around_right",BG_Dog_Move_Run_Turn_Around_Right,NULL},
{"move_attack",NULL,NULL},
{"stop_idle",BG_Dog_Stop_Idle,BG_Dog_Look_At},
{"stop_attackidle",BG_Dog_Stop_AttackIdle,BG_Dog_Look_At},
{"stop_attackidle_bark",BG_Dog_Stop_AttackIdle_Bark,BG_Dog_Look_At},
{"stop_attackidle_growl",BG_Dog_Stop_AttackIdle_Growl,BG_Dog_Look_At},
{"pain_main",BG_Dog_Pain_Main,NULL},
{"pain_front",BG_Dog_Pain_Front,NULL},
{"pain_back",BG_Dog_Pain_Back,NULL},
{"pain_left",BG_Dog_Pain_Left,NULL},
{"pain_right",BG_Dog_Pain_Right,NULL},
{"pain_run_front",BG_Dog_Run_Pain_Front,NULL},
{"pain_run_back",BG_Dog_Run_Pain_Back,NULL},
{"pain_run_left",BG_Dog_Run_Pain_Left,NULL},
{"pain_run_right",BG_Dog_Run_Pain_Right,NULL},
{"death_front",BG_Dog_Death_Front,NULL},
{"death_back",BG_Dog_Death_Back,NULL},
{"death_left",BG_Dog_Death_Left,NULL},
{"death_right",BG_Dog_Death_Right,NULL},
{"flashed",BG_Dog_Flashed,NULL},
{"combat_attackidle",BG_Dog_Combat_AttackIdle,NULL},
{"combat_attackidle_bark",BG_Dog_Combat_AttackIdle_Bark,NULL},
{"combat_attackidle_growl",BG_Dog_Combat_AttackIdle_Growl,NULL},
{"combat_attack_run",BG_Dog_Combat_Attack_Run,NULL},
{"combat_attack_player_close_range",BG_Dog_Combat_Attack_Player_Close_Range,NULL},
{"combat_attack_miss",BG_Dog_Combat_Attack_Miss,BG_Dog_Look_At},
{"combat_attack_miss_left",BG_Dog_Combat_Attack_Miss_Left,NULL},
{"combat_attack_miss_right",BG_Dog_Combat_Attack_Miss_Right,NULL},
{"traverse_jump_up_40",BG_Dog_Traverse_Jump_Up_40,NULL},
{"traverse_jump_up_80",BG_Dog_Traverse_Jump_Up_80,NULL},
{"traverse_jump_down_40",BG_Dog_Traverse_Jump_Down_40,NULL},
{"traverse_jump_down_80",BG_Dog_Traverse_Jump_Down_80,NULL},
{"traverse_wallhop",BG_Dog_Traverse_Wallhop,NULL},
{"traverse_window",BG_Dog_Traverse_Window,NULL},
{"traverse_through_hole_42",BG_Dog_Traverse_Through_Hole_42,NULL},
{"combat_attack_player",BG_Dog_Combat_Attack_Player,NULL},
{"combat_attack_player_early",BG_Dog_Combat_Attack_Player_Early,NULL},
{"combat_attack_player_lunge",BG_Dog_Combat_Attack_Player_Lunge,NULL},
{"combat_player_neck_snap",BG_Dog_Combat_Player_Neck_Snap,NULL},
{"combat_player_neck_miss",BG_Dog_Combat_Player_Neck_Miss,NULL},
{"death_tesla_a",BG_Dog_Death_Tesla_A,NULL},
{"death_tesla_b",BG_Dog_Death_Tesla_B,NULL},
{"death_tesla_c",BG_Dog_Death_Tesla_C,NULL},
{"death_tesla_d",BG_Dog_Death_Tesla_D,NULL},
{"death_tesla_e",BG_Dog_Death_Tesla_E,NULL}
};
int DogStateIndex(const char *name) {
    if(name)for(unsigned i=0;i<sizeof(stateRows)/sizeof(*stateRows);++i)
        if(!strcmp(name,stateRows[i].name))return (int)i;
    return -1;
}
/* Resolving 59 names through the tree on every transition is wasteful and,
 * worse, a single unbound name used to fail silently for every state. Bind
 * once per loaded tree, name the clip that could not be bound, and keep the
 * failure sticky so the log carries one line instead of one per frame. One
 * cache per clip table: the MP dog and the zm hellhound bind different trees
 * through the same state code. */
struct dog_clip_binding {unsigned tree,refs[DOG_CLIP_COUNT];int bound,reported;};
static dog_clip_binding s_binding,s_zombieBinding;
static int ClipBindings(dog_clip_binding *b,const char *const *names,unsigned tree,
    const unsigned **refs,const char **missing) {
    if(missing)*missing=NULL;
    if(refs)*refs=NULL;
    if(b->bound && b->tree==tree) {
        if(refs)*refs=b->refs;
        return 1;
    }
    if(b->reported && b->tree==tree) return 0;
    b->tree=tree;b->bound=0;b->refs[0]=0;
    for(unsigned i=1;i<DOG_CLIP_COUNT;++i)
        if(!FindAnimInServerTree(tree,names[i],&b->refs[i])) {
            b->reported=1;
            if(missing)*missing=names[i];
            return 0;
        }
    b->bound=1;b->reported=0;
    if(refs)*refs=b->refs;
    return 1;
}
int DogClipBindings(unsigned tree,const unsigned **refs,const char **missing) {
    return ClipBindings(&s_binding,clipNames,tree,refs,missing);
}
int ZombieDogClipBindings(unsigned tree,const unsigned **refs,const char **missing) {
    return ClipBindings(&s_zombieBinding,zombieDogClipNames,tree,refs,missing);
}
void DogClipBindingsReset(void) {
    memset(&s_binding,0,sizeof(s_binding));
    memset(&s_zombieBinding,0,sizeof(s_zombieBinding));
}
const char *DogClipName(unsigned index) {return index<DOG_CLIP_COUNT?clipNames[index]:NULL;}
const char *ZombieDogClipName(unsigned index) {return index<DOG_CLIP_COUNT?zombieDogClipNames[index]:NULL;}
unsigned DogClipCount(void) {return DOG_CLIP_COUNT;}
unsigned DogStateCount(void) {return (unsigned)(sizeof(stateRows)/sizeof(*stateRows));}
const char *DogStateName(unsigned index) {
    return index<sizeof(stateRows)/sizeof(*stateRows)?stateRows[index].name:NULL;
}
static int DogStateApply(int zombie,DObj *obj,unsigned entity,unsigned generation,unsigned tree,unsigned time,
    int state,int previous,int entering,float rate,float lean,float aimUp,float aimLeft) {
    if(!obj || !obj->tree || obj->tree->anims!=Scr_GetAnims(tree) || state<0 || state>=ACTOR_ANIMATION_COUNT ||
       previous<0 || previous>=ACTOR_ANIMATION_COUNT || !isfinite(rate) || rate<=0 || rate>5 ||
       !isfinite(lean) || !isfinite(aimUp) || !isfinite(aimLeft))return 0;
    dog_state_context ctx={};ctx.obj=obj;ctx.entity=entity;ctx.generation=generation;
    ctx.tree=tree;ctx.time=time;ctx.rate=rate;ctx.lean=lean;ctx.aimUp=aimUp;ctx.aimLeft=aimLeft;
    ctx.notifyIndex=UINT_MAX;
    {
        const unsigned *refs=NULL; const char *missing=NULL;
        if(!(zombie ? ZombieDogClipBindings(tree,&refs,&missing) :
                      DogClipBindings(tree,&refs,&missing))) {
            if(missing)
                Com_PrintWarning(CON_CHANNEL_SERVER,
                    "CoDOL dog: clip '%s' is not in the loaded C1 %s tree; dog animation is disabled\n",
                    missing,zombie?"zombie_dog":"dog");
            return 0;
        }
        memcpy(ctx.refs,refs,sizeof(ctx.refs));
    }
    if(entering){if(stateRows[state].enter)stateRows[state].enter(&ctx,previous);}
    else if(stateRows[state].think)stateRows[state].think(&ctx);
    if(ctx.failed)return 0;
    if(!ctx.count)return 1; // original idle/move_attack have no native callback
    if(ctx.notifyIndex!=UINT_MAX)
        return DogAnimApplyNotifiedBatch(obj,ctx.events,ctx.count,ctx.notifyIndex,"done");
    return DogAnimApplyBatch(obj,ctx.events,ctx.count);
}
int OL_DogStateApply(DObj *obj,unsigned entity,unsigned generation,unsigned tree,unsigned time,
    int state,int previous,int entering,float rate,float lean,float aimUp,float aimLeft) {
    return DogStateApply(0,obj,entity,generation,tree,time,state,previous,entering,rate,lean,aimUp,aimLeft);
}
/* The hellhound runs the very same T5 state machine (OpenBLOPS
 * bg_dog_animations_mp.cpp), against its own tree's clips. */
int ZombieDogStateApply(DObj *obj,unsigned entity,unsigned generation,unsigned tree,unsigned time,
    int state,int previous,int entering,float rate,float lean,float aimUp,float aimLeft) {
    return DogStateApply(1,obj,entity,generation,tree,time,state,previous,entering,rate,lean,aimUp,aimLeft);
}

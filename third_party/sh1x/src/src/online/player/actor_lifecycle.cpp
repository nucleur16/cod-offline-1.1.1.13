#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* C1 actor pool 6C4F80: 32 stable identities; SP_actor6C4B50 allocates a
 * sentient after the actor and rolls back if either pool is exhausted.
 * T5 Actor_FinishSpawning runs the original aitype main on that entity.
 * This is native ownership, not AI or a replacement dog script.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
/* The retained headers declare C functions without linkage guards. */
extern "C" {
#include "actor_t5_root.h"
#include "actor_lifecycle.h"
#include "threat_bias.h"
#include "script_source.h"
#include "sv_state.h"
#include "actor_scripts.h"
#include "actor_delivery.h"
#include "late_corpse.h"
#include "g_shared.h"
#include "game/surfaceflags.h"
#include "entity.h"
#include "dobj.h"
#include "cscr_stringlist.h"
#include "cscr_variable.h"
#include "scr_vm_functions.h"
#include <string.h>
#include <math.h>
#include "server.h"
#include "actor_physics.h"
#include "dog_states.h"
#include "scr_vm.h"
#include "model_animation.h"
#include "xassets/xanims.h"
#include "xassets/weapondef.h"
#include <stddef.h>
#include "vehicle_wheel_anim.h"
#include "script_model_tree.h"
}
extern "C" XAnim_s *Scr_GetAnims(unsigned);
extern "C" unsigned long long Sys_MicrosecondsLong(void);
/* Per-actor-frame cost split (ol_debugTrace 1): the route planner and the
 * cover choice, the two searches over the level's node graph.  An actor
 * frame over 4 ms is logged with both. */
static unsigned long long s_framePlanUs, s_frameCoverUs, s_frameScriptUs, s_frameGoalUs, s_frameSightUs;
static unsigned long long s_framePushUs, s_frameAnimUs;
/* Set by ActorNearestWalkableNode (reachable 2) when it proved no goal-end
 * candidate routable from the start node; read by the planner right after. */
static int s_goalEndUnroutable;

extern "C" {
#include "bot_nav.h"
#include "actor_cover.h"
#include "actor_targeting.h"
/* script_path_nodes.c: the map node record behind a graph node. */
const char *PathNodeValue(unsigned index, const char *key);
}
extern "C" {
void Scr_DecTime(void);
void Scr_IncTime(void);
void Scr_RunCurrentThreads(void);
#include "dog_combat.h"
#include "player_appearance.h"
}
#include "actor_contact_probe.h"
#include "actor_audit.h"
#include "actor_threat_limits.h"
#include "frame_profile.h"
#include "actor_fire.h"
#include "actor_drop.h"
#include "actor_ammo.h"
#include "perk_query.h"
#include "weapon_native.h"

struct actor_lifetime_s {
    gentity_s *entity;
    sentient_t *sentient;
    unsigned generation;
    unsigned char allocated, typeStarted, initialized, scriptsStarted, switching;
    actor_script_table_t scripts;
    short animThread;
    int animFunction, moveMode, animMode, scriptAnimMode;
    /* The species row this actor was initialized as; NULL until aitype
     * main() sets self.type. Every former "dog" literal in this file is
     * now a column of it (actor_scripts.h). */
    const actor_species_t *species;
    int enemyNum;
    unsigned enemyGeneration;
    actor_target_cache_t targetReachability;
    float goal[3], goalRadius, goalHeight, stopAnimDistSq, lookAheadDir[3];
    unsigned char pathEndAnimNotified;
    unsigned char hasGoal, goalNotified;
    unsigned short route[BOT_NODES];
    int routeCount, routeCursor, goalNode;
    /* Waypoint links this actor wedged on (see the stuck detector): the bot
     * graph marks links for players, and a "crouch" link can be a knee-high
     * wall a player vaults but a dog cannot step (mp_killhouse_sh nodes
     * 99/100 -> 103/104: a 32-unit cinderblock wall).  The planner skips
     * them for this actor from then on. */
    unsigned short bannedLinkFrom[8], bannedLinkTo[8];
    unsigned bannedLinkCount;
    /* Start nodes this actor wedged walking straight at from off the graph
     * (the route's first leg, where there is no link to ban): Prototype's
     * west yard node 255 (-865 102) sits behind a steep dirt face beside the
     * car that a +18 sweep clears but the actor's step cannot, and every
     * re-plan picked it again as the nearest node. */
    unsigned short bannedStart[4];
    unsigned bannedStartCount;
    /* Failed directed routes (e.g. retreating from inside a window to its
     * exterior spawner) must not exhaust the graph every server frame.
     * Kept separately from routeFailNotified, which only limits notifications. */
    int routeRetryTime, routeFailedTime;
    float routeFailedGoal[3], routeFailedOrigin[3];
    unsigned routeFailedBannedLinks, routeFailedBannedStarts;
    int shortcutBanTime;
    /* Dog wall hop (T5 Actor_Dog_Exposed_Think -> the species "jump"
     * animscript over an obstacle in the path): the traversal rides the
     * negotiation hold with no begin/end node; start and landing are kept
     * so a hop that ends in solid can be finished honestly. */
    unsigned char wallHop;
    float hopStart[3], hopLanding[3];
    int hopTime, hopDiagTime;
    int lastRootMotionTime;
    actor_physics_t physics;
    unsigned serverTree;
    XAnimTree_s tree;
    int animationState, lastAnimationTime;
    float animationRate, aimUp, aimLeft, lean, meleeAttackDistance;
    int scriptOrient; // 0 default, 1 fixed yaw, 2 enemy, 3 enemy/motion, 4 goal, 5 motion, 6 animation (coop face current)
    float scriptYaw;
    unsigned char safeToChangeScript, painPending;
    float damageYaw;
    int deathTime, enemySeenTime, talkMask;
    /* IW4 pain.gsc crawlingPain is an incapacitated LIVE actor, never a
     * resurrected corpse. The same damageable entity/health/team stays on
     * the wire until another hit or the real bleedout damage kills it. */
    int wounded, woundAttackerNum, woundAttackerUseCount;
    int woundMod, woundWeapon, woundHitLoc;
    float woundDirection[3];
    unsigned enemyModel;
    unsigned short weaponName;
    int lastMeleeTime;
    unsigned char ignoreAll;
    /* SHARED ACTOR FIELDS (zm hellhound parity, 2026-10-15). T5 actor_s
     * allowPain (byte, actor_fields.cpp:93, default 1 at actor_mp.cpp:260;
     * Actor_Pain pushes AIS_PAIN only while it is set, actor_mp.cpp:2080) and
     * pFavoriteEnemy (sentient handle, actor_fields.cpp:92; the threat pass
     * scores it 0x7ffffffe, actor_threat.cpp:317-321). The zm hellhound and
     * zombie both set allowpain = false; T5 _zombiemode_ai_dogs pins
     * favoriteenemy on every hound. */
    unsigned char allowPain;
    int favoriteNum;
    unsigned favoriteGeneration;
    float engageMin, engageMinFalloff, engageMax, engageMaxFalloff;
    /* Flashbang state (T5 actor_s flashBanged / flashBangedStrength).
     * maps\mp\_dogs::flash_dogs sets it, the think then runs the species'
     * flashed animscript, and dog_flashed::main clears it with
     * setFlashBanged( false ) when its animation completes. */
    int flashBanged;
    float flashStrength;
    /* Diagnostics ("CoDOL actor:", every species): one summary line per second per actor and
     * one line per goal/route/animscript/animstate transition. */
    int diagTime, diagState, movesAccepted, movesRejected;
    /* Hostile sentients the last perception pass considered. "enemy -1
     * cand 0" is a targeting/registration failure; "enemy -1 cand 1" on a
     * sighted species is an occluded target. */
    int diagCandidates;
    float diagOrigin[3], lastTranslation[3], diagGoal[3];
    int savedContents;
    unsigned char routeFailNotified, routeDiagPrinted;
    /* Log rate limits: the route-FAILED and goal-set lines fire per goal
     * change, and a chased player changes the goal every frame (5,804 +
     * 2,476 lines in one zm session).  At most one line per 5 s per actor,
     * carrying the count since the last one. */
    int routeFailLogTime, routeFailCount, goalLogTime, goalLogCount;
    /* T5 actor_s::pathWaitTime: after a failed plan no new plan runs before
     * this time (Actor_HandleInvalidPath, level.time + 500); a chase whose
     * route failed stands until then. */
    int pathWaitTime;
    /* T5 codeGoal: the enemy attack spot carries its own (small) radius so
     * the chase never ends inside the script's 128-unit goalradius. */
    float chaseRadius;
    /* T5 Path_UpdateLeanAmount inputs and the stuck detector. */
    float prevMoveDir[2], stuckOrigin[3];
    int stuckTime, stuckCount;
    /* Root motion the animation asked for since the stuck window opened: a
     * wedge is motion asked for and refused; a clip that asks for none is a
     * stall of the animation and gets no repath. */
    float stuckAttempt;
    unsigned char stallDiagPrinted, windowDiagPrinted;
    /* T5 Actor_Negotiation_Start/Think: a node_negotiation_begin -> _end
     * traversal.  pending is raised by the route follower when the actor
     * reaches the begin node of such a leg; RunFrame then aligns the actor to
     * the node, hands the species' traversal animscript the pair, and holds
     * the mover (no chase goal, no stuck detector, no script re-selection)
     * until that thread ends or times out. */
    unsigned char negotiating, negotiationPending;
    int negotiationBegin, negotiationEnd, negotiationStartTime;
    float negotiationStartOrigin[3];
    /* T5 AIS_SCRIPTEDANIM (GScr_AnimScripted -> Actor_PushState + Actor_KillAnimScript
     * + animscripts/zombie_scripted): the animscript is dead, the clip the
     * builtin published owns the tree, the mover only faces the given angles,
     * consumes the origin offset and applies the clip's root motion.  Popped by
     * stopanimscripted(), the clip's length, or death. */
    unsigned char scripted, scriptedAlign;
    int scriptedStartTime, scriptedEndTime;
    float scriptedOrigin[3];
    /* Physics verdicts inside the stuck window: moves the physics refused
     * (start in solid) and moves it throttled to under half the asked
     * displacement.  A wedge needs one of them; a slow cycle has neither. */
    int stuckRejected, stuckBlocked;
    unsigned char slowDiagPrinted;
    int wedgeDiagCount; float wedgeDiagSpot[3];
    /* T5 actor_s::deathContents and the death state's start time (iStateTime).
     * T5 Actor_Death_Think (OpenBLOPS actor_death.cpp) copies deathContents
     * onto the dying actor 500 ms into AIS_DEATH and relinks it, while the
     * death animscript still runs; until then the body keeps its live
     * contents. The Actor_SetDefaults value (actor_mp.cpp:793) is
     * 0x4002000, CONTENTS_CORPSE | CONTENTS_CLIPSHOT (decompiled there as
     * &objBuf[1824][4]: 0x2000 above the &objBuf[1758][2] that actor_mp.cpp
     * :2921 identifies as FL_OBSTACLE 0x4000000). Corpse contents are in no
     * player (0x2810011) or actor (0x2820015) clipmask, so the dying body
     * stops blocking movement; CLIPSHOT is in the bullet mask (0x2806831,
     * T5 0x280E833), so it still stops bullets. T5 zm scripts lower it to
     * CONTENTS_CORPSE alone for a dissolving thundergun death
     * (_zombiemode_spawner.gsc:4930 setDeathContents). */
    int deathContents, deathStateTime;
    /* The route direction before separation steering (T5 path lookahead):
     * the dog turn decision reads it.  diagSteer feeds actorcontact. */
    float routeDir[2], diagSteer;
    /* This frame's ActorGoalDirection verdict (1: walking a route),
     * read by other actors' overlap push-out. */
    int lastRoute;
    int probeAccepted, probeRejected, probeBlocked, probeDeltaFails;
    /* This frame's leg: 1 a waypoint route node, 0 the goal itself (actorcontact). */
    int usingRoute;
    /* The straight shortcut's hull verdict (ActorShortcutClear): when it was
     * last walked, toward which goal, the answer, and since when the shortcut
     * has been clear or blocked (the hysteresis).  routeGoal: the goal the
     * current route was planned for.  finalLeg: the route ran out (for a goal
     * at finalLegGoal) and the actor is on the straight leg from it to the
     * goal, which is walked without re-testing (T5 Path's last segment). */
    int shortcutCheckTime, shortcutClearSince, shortcutBlockedSince;
    int prevRoute;  /* last frame's GoalDirection verdict; lastRoute is cleared at frame start */
    unsigned char shortcutHullClear, finalLeg;
    float shortcutGoal[3], finalLegGoal[3], routeGoal[3];
    /* codol_actorRide: the actor rides a moving entity (the coop opening
     * jeep's driver): every frame it is placed at rideLocal in that
     * entity's frame, evaluated from its trajectories at level.time, and
     * runs no AI or movement of its own. rideNum is the entity number + 1
     * (0: not riding); rideCount guards against a reused slot. */
    int rideNum, rideSpawnCount;
    float rideLocal[3], rideYaw;
    /* Coop cover (actor_cover.c, ActorCoverThink): the claimed cover
     * index (-1 none), whether the soldier has reached it, when cover is
     * next looked for and the held node next re-validated, and the nodes
     * he gave up on (flanked, too close), each with the time he may take
     * it again.  coverEnabled is the script's switch
     * (codol_actorUseCover; the soldier animscripts set it). */
    int coverIndex;
    unsigned char coverEnabled, coverArrived;
    int coverEvalTime, coverCheckTime, coverArrivedTime, coverDiagTime;
    int coverAvoid[4], coverAvoidUntil[4];
    unsigned coverAvoidNext;
    /* MP attack dog perception (ActorUpdateMpDogThreat, T5 MP actor_threat /
     * actor_events): per client, when this dog last had information about
     * it (sentient_info_t lastKnownPosTime), last saw it (VisCache) and was
     * last attacked by it (iLastAttackMeTime); when its new-enemy team
     * event is due (sentient_s iEnemyNotifyTime); whether it had an enemy
     * last pass (an enemy lost to death still notifies "enemy"). */
    int mpKnownTime[MAX_CLIENTS], mpVisibleTime[MAX_CLIENTS], mpAttackedTime[MAX_CLIENTS];
    int mpNotifyTime;
    unsigned char mpHadEnemy;
    /* Last frame's route verdict, for the MP dog's arrival (see the
     * animscript choice in ActorRunFrameBody). */
    unsigned char mpRouteLast;
};
static actor_lifetime_t actors[MAX_ACTOR_SLOTS];
/* SLOT REUSE AND C1's ACTOR TREES. C1 keeps one composition record per actor
 * slot (cgs+0x168514, stride 0x29c) and per corpse slot (cgs+0x2a70), each
 * holding the animation trees its entity's DObj animates with (+0x18, one per
 * tree selector). When the client takes a snapshot (C1 0x46c3a0, after the
 * entity pass 0x46c670), it walks the previous snapshot's actors: an actor
 * whose entity is now a corpse (eType 20) hands its actor record's tree to
 * the corpse record named by entityState+58 and takes that corpse record's
 * old tree in exchange (0x46c4de / 0x46c4f2 -> 0x42a5c0). If the actor slot
 * is already flagged by another actor in the new snapshot (0x46c4c8: the
 * slot was given to a new actor in the same snapshot), that live actor's
 * tree is the one handed to the corpse, and the walking actor is left with
 * the corpse slot's old tree, which a ragdolled corpse may still be driving
 * (static analysis of C1.exe 1.1.1.13). That matches the user's report of
 * 2026-10-21: walking zombies and dogs folding and turning upside down.
 * Before e3d558a6, actor corpses never reached the client, so the exchange
 * never ran.
 *
 * So a slot is not given out again until the client has certainly seen its
 * previous owner leave: a free slot released at least SLOT_REUSE_MS ago
 * (stamps below; 0 = never used) is preferred, otherwise the one released
 * longest ago. A snapshot spans at most one 50 ms client snap interval;
 * one second also covers dropped snapshots, as G_Spawn's 500 ms entity guard
 * does for entity numbers. */
#define SLOT_REUSE_MS 1000
static int actorSlotReleased[MAX_ACTOR_SLOTS];
static int SlotCooled(int released)
{
    return !released || level.time - (released - 1) >= SLOT_REUSE_MS || released - 1 > level.time;
}
struct actor_corpse_owner {
    gentity_s *entity;unsigned generation;int time;
    XAnimTree_s tree;actor_physics_t physics;
    /* What the live actor's composition record carried for the per-recipient
     * model choice (enemy model, sentient team), kept for a client that
     * never saw it alive (late_corpse.cpp). */
    unsigned enemyModel,team;
};
/* Actor corpse slots. T5 keeps 8 (OpenBLOPS actor_corpse.cpp, cl_main_mp.h
 * MAX_ACTOR_CORPSES 8, ai_corpseCount 1..8). C1 keeps 16: MEASURED in C1.exe
 * 1.1.1.13, the client's actor-corpse composition pool at cgs+0x2a70 (stride
 * 0x29c, five tree pointers at +0x18) is cleared by 0x450543..0x450566 and
 * given its trees by 0x45057d..0x4505a7, both for exactly 0x10 records, and
 * freed by 0x4501dd..0x45021f, also 0x10. 0x42a6c3..0x42a6e3 resolves an
 * eType 20 entity's record from the entity-state actor slot byte (+0x13c)
 * with no bounds check, so a slot index of 16 or more writes past the pool
 * on every client. 16 is therefore the hard ceiling for actor corpses; the
 * IW4-era configure_mp.csv shipped with C1 also tops ai_corpseCount out at
 * 16. ai_corpseCount (1..16, default 16) selects how many are kept. */
#define MAX_ACTOR_CORPSES 16
static actor_corpse_owner corpses[MAX_ACTOR_CORPSES];
static int corpseSlotReleased[MAX_ACTOR_CORPSES];
static cvar_t *ai_corpseCount;
/* 1: path pace is the blended average root speed (T5 parity); 0: per-frame delta. */
static cvar_t *actor_t5_pace;
static cvar_t *actor_pace_trace;
static unsigned ActorCorpseLimit(void)
{
    if(!ai_corpseCount)
        ai_corpseCount=Cvar_RegisterInt("ai_corpseCount",MAX_ACTOR_CORPSES,1,MAX_ACTOR_CORPSES,0,
            "Actor corpses (zombies, dogs) kept in the world; C1 renders at most 16");
    int n=ai_corpseCount?ai_corpseCount->integer:MAX_ACTOR_CORPSES;
    if(n<1)n=1;
    if(n>MAX_ACTOR_CORPSES)n=MAX_ACTOR_CORPSES;
    return (unsigned)n;
}
static int ActorBecomeCorpse(gentity_s *entity);
/* T5 SentientInfo_Copy gates teammate information by the RECEIVER species:
 * a sender only shares sight with a species it was told to talk to. The
 * dog row owns bit 1, so setTalkToSpecies("dog") behaves exactly as the
 * former hardcoded `talkMask & 1` did. */
static unsigned ActorTalkBit(const actor_lifetime_t *a)
{
    return a && a->species ? a->species->talkBit : 0u;
}
static int ActorMayTarget(actor_lifetime_t *a, const sentient_t *s);
/* T5 Actor_CheckIgnore's group half (actor_threat.cpp:36-44): the candidate's
 * threat bias group is one the actor's group was told to ignore
 * (setignoremegroup). Kept apart from ActorMayTarget, which is hostility and
 * also drives the dogs' noise events and the bots' dog targets. */
static int ActorIgnoresGroup(const actor_lifetime_t *a, const sentient_t *s)
{
    return a && a->sentient && s &&
        ThreatBias_Ignores(s->threatBiasGroup, a->sentient->threatBiasGroup);
}
/* The bias part of T5 Actor_UpdateSingleThreat for this candidate. */
static int ActorThreatScore(const actor_lifetime_t *a, const sentient_t *s, float distance)
{
    return ThreatBias_Score(s->threatBias, s->threatBiasGroup, a->sentient->threatBiasGroup, distance);
}


actor_lifetime_t *ActorForEntity(gentity_s *entity)
{
    if (!entity || !entity->r.inuse) return NULL;
    sentient_t *s = Sentient_ForEntity(entity);
    if (!s || !s->isActor || (unsigned)s->actorSlot >= MAX_ACTOR_SLOTS)
        return NULL;
    actor_lifetime_t *a = &actors[s->actorSlot];
    if (!a->allocated || a->entity != entity || a->sentient != s ||
        a->generation != ACTOR_INCARNATION(entity->useCount) ||
        s->handle.number != entity->s.number + 1) return NULL;
    return a;
}

actor_lifetime_t *ActorAllocate(gentity_s *entity, int team)
{
    if (!entity || !entity->r.inuse || entity->client ||
        entity->s.number < 0 || entity->s.number >= ENTITYNUM_WORLD ||
        entity->s.eType == ET_ACTOR_SPAWNER ||
        (unsigned)team >= SENTIENT_TEAM_COUNT ||
        Sentient_ForEntity(entity)) return NULL;
    /* A cooled free slot first, else the one released longest ago (see
     * SLOT_REUSE_MS). */
    unsigned slot = MAX_ACTOR_SLOTS, warm = MAX_ACTOR_SLOTS;
    for (unsigned i=0; i<MAX_ACTOR_SLOTS; ++i) {
        if (actors[i].allocated || Sentient_ActorBySlot((int)i)) continue;
        if (SlotCooled(actorSlotReleased[i])) { slot = i; break; }
        if (warm == MAX_ACTOR_SLOTS || actorSlotReleased[i] < actorSlotReleased[warm]) warm = i;
    }
    if (slot == MAX_ACTOR_SLOTS) slot = warm;
    if (slot == MAX_ACTOR_SLOTS) return NULL;
    /* C1 SP_actor scans all75 records directly. Class0 bypasses the
     * non-actor24-record recycling policy of makeEntitySentient. */
    sentient_t *s = Sentient_Alloc(0);
    if (!s) return NULL;
    Sentient_Setup(s, entity, team);
    s->isActor = 1;
    s->actorSlot = (int)slot;
    actor_lifetime_t *a = &actors[slot];
    memset(a, 0, sizeof(*a));
    a->entity = entity;
    a->sentient = s;
    a->generation = ACTOR_INCARNATION(entity->useCount);
    a->allocated = 1;
    a->enemyNum = -1;
    a->allowPain = 1;
    a->favoriteNum = -1;
    a->lastRootMotionTime = -1;
    a->lastAnimationTime = -1;
    a->animationRate = 1.0f;
    a->safeToChangeScript = 1;
    a->goalRadius = 4.0f;
    a->goalHeight = 80.0f;
    a->goalNode = -1;
    a->deathTime = -1;
    a->woundAttackerNum = ENTITYNUM_WORLD;
    a->deathContents = CONTENTS_CORPSE | CONTENTS_CLIPSHOT;
    a->deathStateTime = -1;
    a->lastMeleeTime = -1;
    a->talkMask = -1;
    a->coverIndex = -1;
    entity->s.lerp.u.actor.actorSlot = (int)slot;
    return a;
}

int ActorRunTypeMain(gentity_s *entity, int mainHandle)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !mainHandle || a->typeStarted) return 0;
    unsigned generation = a->generation;
    /* Mark before entering GSC: a recursive initialization cannot run main
     * twice, and a script error must not cause a second partially run main. */
    a->typeStarted = 1;
    short thread = Scr_ExecEntThread(entity, mainHandle, 0);
    Scr_FreeThread(thread);
    a = ActorForEntity(entity);
    return a && a->generation == generation;
}

/* Separate native actor namespace. Non-actor writes return zero so the VM
 * retains its ordinary dynamic-field fallback rather than stealing script
 * fields from players, vehicles or generic script entities. */
extern "C" void Scr_AddClassField(unsigned int, const char *, unsigned int);
extern "C" void ActorRegisterFields(void)
{
    Scr_AddClassField(0, "team", 0x8000);
    Scr_AddClassField(0, "aiteam", 0x8000);
    Scr_AddClassField(0, "type", 0x8001);
    Scr_AddClassField(0, "enemy", 0x8002);
    Scr_AddClassField(0, "goalradius", 0x8003);
    Scr_AddClassField(0, "goalheight", 0x8004);
    Scr_AddClassField(0, "safetochangescript", 0x8005);
    Scr_AddClassField(0, "animplaybackrate", 0x8006);
    Scr_AddClassField(0, "meleeattackdist", 0x8007);
    Scr_AddClassField(0, "damageyaw", 0x8008);
    Scr_AddClassField(0, "ignoreall", 0x8009);
    Scr_AddClassField(0, "script_owner", 0x800a);
    Scr_AddClassField(0, "aiweapon", 0x800b);
    Scr_AddClassField(0, "stopanimdistsq", 0x800c);
    Scr_AddClassField(0, "lookaheaddir", 0x800d);
    Scr_AddClassField(0, "allowpain", 0x800e);
    Scr_AddClassField(0, "favoriteenemy", 0x800f);
    /* Read-only, for the T5 telemetry capture (maps/_zombiemode_t5cap.gsc):
     * the native animscript the actor is running. */
    Scr_AddClassField(0, "codolanimscript", 0x8010);
    /* T5 sentient_fields.cpp:18 / actor_fields.cpp:22 "threatbias" (F_INT):
     * a sentient field, so players carry it too; see the dispatch below. */
    Scr_AddClassField(0, "threatbias", 0x8011);
}
/* The favourite enemy, or NULL once it is gone (freed slot, new incarnation). */
static gentity_s *ActorFavoriteEnemy(actor_lifetime_t *a)
{
    if (!a || (unsigned)a->favoriteNum >= ENTITYNUM_WORLD) return NULL;
    gentity_s *e = &g_entities[a->favoriteNum];
    if (!e->r.inuse || (unsigned)e->useCount != a->favoriteGeneration) {
        a->favoriteNum = -1;
        return NULL;
    }
    return e;
}
static const char *ActorTeamNames[SENTIENT_TEAM_COUNT] = {
    "none", "axis", "allies", "team3", "team4", "team5", "team6"
};
extern "C" int ActorSetField(unsigned classnum, int entnum, int offset)
{
    if (classnum || (unsigned)entnum >= ENTITYNUM_WORLD) return 0;
    /* threatbias belongs to every sentient, player or actor; a non-sentient
     * keeps the VM's dynamic field. */
    if (offset == 0x8011) return Sentient_SetThreatBiasField(&g_entities[entnum]);
    actor_lifetime_t *a = ActorForEntity(&g_entities[entnum]);
    if (!a) return 0;
    if(offset==0x800d){Scr_Error("lookaheaddir is owned by native path following");return 1;}
    if(offset==0x800e){a->allowPain=Scr_GetInt(0)!=0;if(!a->allowPain)a->painPending=0;return 1;}
    if(offset==0x800f){
        /* T5 F_SENTIENTHANDLE: undefined clears it; anything else must be a sentient. */
        if(Scr_GetType(0)==VAR_UNDEFINED){a->favoriteNum=-1;return 1;}
        gentity_s *e=Scr_GetEntity(0);
        if(!e || !Sentient_ForEntity(e)){Scr_ParamError(0,"favoriteenemy must be a sentient");return 1;}
        a->favoriteNum=e->s.number;a->favoriteGeneration=(unsigned)e->useCount;return 1;
    }
    if(offset==0x800c){float value=Scr_GetFloat(0);if(!isfinite(value)||value<0)Scr_ParamError(0,"stopanimdistsq must be finite and nonnegative");else a->stopAnimDistSq=value;return 1;}
    if(offset==0x800b){Scr_SetString(&a->weaponName,Scr_GetConstString(0));return 1;}
    if(offset==0x8009){a->ignoreAll=Scr_GetInt(0)!=0;if(a->ignoreAll)ActorSetEnemy(a->entity,NULL);return 1;}
    if(offset==0x800a){Scr_Error("script_owner is managed through setentityowner");return 1;}
    if (offset == 0x8008) {
        Scr_Error("actor damageyaw is owned by native damage processing");return 1;
    }
    if (offset == 0x8007) {
        float distance = Scr_GetFloat(0);
        if (!isfinite(distance) || distance < 0) Scr_ParamError(0, "meleeattackdist must be finite and nonnegative");
        else a->meleeAttackDistance = distance;
        return 1;
    }
    if (offset == 0x8005) {
        a->safeToChangeScript = Scr_GetInt(0) != 0;
        return 1;
    }
    if (offset == 0x8006) {
        float rate = Scr_GetFloat(0);
        if (!isfinite(rate) || rate <= 0 || rate > 5) {
            Scr_ParamError(0, "actor animation rate exceeds native animation command range (0,5]");
            return 1;
        }
        a->animationRate = rate;
        return 1;
    }
    if (offset == 0x8002) {
        Scr_Error("actor enemy is read-only; native perception owns enemy selection");
        return 1;
    }
    if (offset == 0x8003 || offset == 0x8004) {
        float number = Scr_GetFloat(0);
        if (!isfinite(number) || number < 0) {
            Scr_ParamError(0, "actor goal dimension must be finite and >= 0");
            return 1;
        }
        if (offset == 0x8003) a->goalRadius = fmaxf(4.0f, number);
        else a->goalHeight = fmaxf(80.0f, number);
        a->goalNotified = 0;
        return 1;
    }
    const char *value = Scr_GetString(0);
    if (offset == 0x8000) {
        for (unsigned i = 0; i < SENTIENT_TEAM_COUNT; ++i)
            /* T5 _dogs::dog_manager_get_team returns "free" in a non-team
             * game; T5 sentients carry TEAM_FREE. Here FFA players are team
             * NONE too, so a free dog is team NONE and targets by owner. */
            if (!strcmp(value, ActorTeamNames[i]) || (!i && (!strcmp(value, "neutral") || !strcmp(value, "free")))) {
                /* Real targeting/class-mask consumers read this same record. */
                a->sentient->team = (int)i;
                a->entity->team = (int)i;
                return 1;
            }
        Scr_Error("actor team: unknown native sentient team");
        return 1;
    }
    if (offset == 0x8001) {
        const actor_species_t *row = ActorSpeciesByName(value);
        if (!row) {
            Scr_Error("actor type: no such native actor species");
            return 1;
        }
        a->species = row;
        /* A coop species walks the MP level a player walks: the bot
         * waypoint graph it routes on was built for players, and these
         * levels block players with player clip (0x10000) where the art has
         * no collision of its own -- fences, rails, window gaps, the edges
         * of roofs. C1's actor collision categories (0x2820015, set at
         * spawn) carry monster clip instead, which an MP level never
         * authors, so a soldier walked through what a player cannot.
         * Only coop rows: dog and zombie keep their measured mask. */
        if (row->category && ZoneTypeRunsSoldiers(row->category) && !row->quadruped)
            a->entity->clipmask |= 0x10000;
        /* C1 composition tree selector is independently resolved when its
         * actual DObj is initialized. T5 species zero is NOT a C1 tree index. */
        return 1;
    }
    Scr_Error("actor field: invalid registered field offset");
    return 1;
}
extern "C" void ActorGetField(unsigned classnum, int entnum, int offset)
{
    if (classnum || (unsigned)entnum >= ENTITYNUM_WORLD) return;
    if (offset == 0x8011) { Sentient_GetThreatBiasField(&g_entities[entnum]); return; }
    actor_lifetime_t *a = ActorForEntity(&g_entities[entnum]);
    if (!a) return;
    if(offset==0x800d){Scr_AddVector(a->lookAheadDir);return;}
    if(offset==0x800e){Scr_AddBool(a->allowPain);return;}
    if(offset==0x800f){gentity_s *f=ActorFavoriteEnemy(a);if(f)Scr_AddEntity(f);return;}
    if(offset==0x800c){Scr_AddFloat(a->stopAnimDistSq);return;}
    if(offset==0x8010){
        const char *fn = a->scripted?"scripted":a->negotiating?"traverse":!a->animFunction?"none":
            a->animFunction==a->scripts.move?"move":a->animFunction==a->scripts.stop?"stop":
            a->animFunction==a->scripts.combat?"combat":a->animFunction==a->scripts.jump?"traverse":
            a->animFunction==a->scripts.pain?"pain":a->animFunction==a->scripts.death?"death":
            a->animFunction==a->scripts.turn?"turn":a->animFunction==a->scripts.flashed?"flashed":"other";
        Scr_AddString(fn);return;}
    if(offset==0x800b){Scr_AddString(a->weaponName?SL_ConvertToString(a->weaponName):"");return;}
    if(offset==0x8009){Scr_AddBool(a->ignoreAll);return;}
    if(offset==0x800a){if(a->entity->r.ownerNum.isDefined())Scr_AddEntity(a->entity->r.ownerNum.ent());return;}
    if (offset == 0x8000 && (unsigned)a->sentient->team < SENTIENT_TEAM_COUNT)
        Scr_AddString(ActorTeamNames[a->sentient->team]);
    else if (offset == 0x8001 && a->species)
        Scr_AddString(a->species->name);
    else if (offset == 0x8002) {
        gentity_s *enemy = ActorEnemy(&g_entities[entnum]);
        if (enemy) Scr_AddEntity(enemy);
    } else if (offset == 0x8003) Scr_AddFloat(a->goalRadius);
    else if (offset == 0x8004) Scr_AddFloat(a->goalHeight);
    else if (offset == 0x8005) Scr_AddBool(a->safeToChangeScript);
    else if (offset == 0x8006) Scr_AddFloat(a->animationRate);
    else if (offset == 0x8007) Scr_AddFloat(a->meleeAttackDistance);
    else if (offset == 0x8008) Scr_AddFloat(a->damageYaw);
}

gentity_s *ActorEnemy(gentity_s *entity)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || (unsigned)a->enemyNum >= ENTITYNUM_WORLD) return NULL;
    gentity_s *enemy = &g_entities[a->enemyNum];
    if (!enemy->r.inuse || (unsigned)enemy->useCount != a->enemyGeneration ||
        enemy->health <= 0 || (enemy->flags & 4) || !Sentient_ForEntity(enemy)) {
        a->enemyNum = -1;
        DogCombatRelease(entity);
        return NULL;
    }
    return enemy;
}

int ActorSetEnemy(gentity_s *entity, gentity_s *enemy)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a) return 0;
    /* The entrance walk sets ignoreall until its barrier has been torn.
     * Damage can arrive during that walk; it must not let the attacker
     * replace the scripted entrance goal with a chase goal. */
    if (enemy && a->ignoreAll) return 0;
    sentient_t *target = enemy ? Sentient_ForEntity(enemy) : NULL;
    if (enemy && (!enemy->r.inuse || enemy == entity || enemy->health <= 0 || (enemy->flags & 4) ||
        !target || target->dontTarget || !ActorMayTarget(a, target) ||
        ActorIgnoresGroup(a, target))) return 0;
    int changed = a->enemyNum != (enemy ? enemy->s.number : -1) ||
        (enemy && a->enemyGeneration != (unsigned)enemy->useCount);
    if (!changed) return 1;
    DogCombatRelease(entity);
    a->enemyNum = enemy ? enemy->s.number : -1;
    a->enemyGeneration = enemy ? (unsigned)enemy->useCount : 0;
    if (changed && enemy) {
        unsigned name = SL_GetString("enemy", 0);
        Scr_Notify(entity, (unsigned short)name, 0);
        SL_RemoveRefToString(name);
    }
    return 1;
}

/* Straight-line reachability for the direct-to-goal shortcut. The sweep is
 * lifted by the actor's step height: waypoints and goals sit on the floor,
 * and a floor-touching capsule reports every ramp, kerb and slope seam as a
 * hit, which is what kept every route "blocked". Physics still steps/slides. */
/* Other live actors do not block an actor's own movement or reachability
 * sweeps (T5 dogs negotiate around each other; here they pass through).
 * Contents are read live by the clip world, so no relink is needed. */
extern "C" void ActorsPassable(gentity_s *self, int passable)
{
    for (unsigned i = 0; i < MAX_ACTOR_SLOTS; ++i) {
        actor_lifetime_t *o = &actors[i];
        if (!o->allocated || !o->entity || o->entity == self || !o->entity->r.inuse) continue;
        if (passable) { o->savedContents = o->entity->r.contents; o->entity->r.contents = 0; }
        else o->entity->r.contents = o->savedContents;
    }
}
static int ActorClearSegment(gentity_s *entity, const float *from, const float *to)
{
    trace_t trace;
    float mins[3], maxs[3], start[3], end[3];
    memcpy(mins, entity->r.mins, 12); memcpy(maxs, entity->r.maxs, 12);
    memcpy(start, from, 12); memcpy(end, to, 12);
    mins[2] += 18.0f; if (mins[2] > maxs[2] - 8.0f) mins[2] = maxs[2] - 8.0f;
    ActorsPassable(entity, 1);
    G_TraceCapsule(&trace, start, mins, maxs, end, entity->s.number, entity->clipmask);
    ActorsPassable(entity, 0);
    return !trace.startsolid && !trace.allsolid && trace.fraction == 1.0f;
}
/* Can this hull WALK from `from` to `to` in a straight line?  The step test
 * the actor's own physics applies (actor_physics.cpp: up 18, across,
 * down), along the whole segment, rather than the +18 lifted sweep above.
 * T5 takes a path shortcut only when Path_PredictionTrace -- the actor's
 * real hull stepping along the ground -- reaches the point (OpenBLOPS
 * actor_navigation.cpp, Path_UpdateLookahead); the lifted sweep passes
 * clean over a knee-high wall the 18-unit step cannot climb, so an actor
 * took that "shortcut" and wedged on the wall (reported 2026-10-02 on
 * mp_rust for dogs leaving toward exit node (704 392 -208); MEASURED the
 * same day, actorcontact cross/follow there: dogs near node 7
 * (1034 -169 -233) flipped between route and shortcut up to 117 times in
 * 2 s, 3,917 flips in 154 s).  Each 48-unit chunk is swept flat; a hit is stepped (up 18,
 * 16 across, down), and after every chunk the hull is dropped to the floor
 * so a later obstacle is met from the ground, not from wherever a slope
 * left it.  Other actors are passable (separation and the push-out deal
 * with them), as in the lifted sweep.  Bounded: a segment needing more
 * than 96 chunks or steps is reported as not walkable and the route is
 * used. */
static int ActorHullWalk(gentity_s *entity, const float *from, const float *to,
    int maskOverride = 0, float goalHeight = 80.0f)
{
    const float *mins = entity->r.mins, *maxs = entity->r.maxs;
    int pass = entity->s.number, mask = maskOverride ? maskOverride : entity->clipmask, walked = 0;
    float pos[3] = {from[0], from[1], from[2] + 1.0f};
    trace_t tr;
    ActorsPassable(entity, 1);
    G_TraceCapsule(&tr, pos, mins, maxs, pos, pass, mask);
    if (tr.startsolid || tr.allsolid) { pos[2] = from[2]; }
    for (int iteration = 0; iteration < 96; ++iteration) {
        float dx = to[0] - pos[0], dy = to[1] - pos[1];
        float distance = sqrtf(dx*dx + dy*dy);
        if (distance < 1.0f) { walked = 1; break; }
        dx /= distance; dy /= distance;
        float chunk = fminf(distance, 48.0f);
        float target[3] = {pos[0] + dx*chunk, pos[1] + dy*chunk, pos[2]};
        G_TraceCapsule(&tr, pos, mins, maxs, target, pass, mask);
        if (tr.startsolid || tr.allsolid) break;
        if (tr.fraction < 1.0f) {
            float hitFraction = tr.fraction;
            float hit[3] = {pos[0] + (target[0]-pos[0])*tr.fraction,
                pos[1] + (target[1]-pos[1])*tr.fraction, pos[2]};
            float up[3] = {hit[0], hit[1], hit[2] + 18.0f};
            G_TraceCapsule(&tr, hit, mins, maxs, up, pass, mask);
            if (tr.startsolid || tr.allsolid || tr.fraction <= 0.05f) break;
            float raised[3] = {hit[0], hit[1], hit[2] + 18.0f*tr.fraction};
            float across = fminf(16.0f, fmaxf(1.0f, distance - chunk*hitFraction));
            float ahead[3] = {raised[0] + dx*across, raised[1] + dy*across, raised[2]};
            G_TraceCapsule(&tr, raised, mins, maxs, ahead, pass, mask);
            if (tr.startsolid || tr.allsolid || tr.fraction*across < 1.0f) break;
            for (int i = 0; i < 3; ++i) pos[i] = raised[i] + (ahead[i]-raised[i])*tr.fraction;
        } else memcpy(pos, target, 12);
        /* Back onto the floor, at most 64 down: a stair flight descends under
         * 48 per 48-unit chunk.  No floor within 64 is a ledge, which T5
         * actors never walk off outside a traversal: the line is not
         * walkable.  MEASURED 2026-10-02 (zm_prototype_sh): letting the walk
         * fall made an upstairs node the "walkable" end of a route to a
         * player on the floor below, and the zombies went upstairs. */
        float down[3] = {pos[0], pos[1], pos[2] - 64.0f};
        G_TraceCapsule(&tr, pos, mins, maxs, down, pass, mask);
        if (tr.startsolid || tr.allsolid || tr.fraction >= 1.0f) break;
        if (tr.normal[2] < 0.7f) break;   /* a face too steep to stand on */
        pos[2] -= 64.0f*tr.fraction;
        pos[2] += 1.0f;
    }
    ActorsPassable(entity, 0);
    /* Arrived on the goal's own floor (goalheight 80), not above or below it. */
    return walked && fabsf(pos[2] - to[2]) <= goalHeight;
}
/* The straight shortcut to the goal, with hysteresis.  The lifted sweep is
 * the cheap per-frame gate; the hull walk is re-run at most every 250 ms
 * (or when the goal moves 48 units).  The shortcut is "clear" when both
 * pass.  An actor on a waypoint route leaves it only once the shortcut has
 * stayed clear for 250 ms; an actor walking the straight leg keeps it
 * through a blocked verdict shorter than 150 ms (a chased goal sliding
 * along a wall flickers the lifted sweep from frame to frame).  T5
 * Path_UpdateLookahead commits to its path the same way: the lookahead
 * trims a corner only when the prediction trace says it can, and never
 * trades the path for a line every other frame.  MEASURED 2026-10-02
 * (mp_rust, natural dog chases): with a two-verdict streak alone a dog
 * whose attack spot moved re-ran the walk every few frames, reached the
 * streak in 100 ms and still switched up to 38 times in 2 s. */
static int ActorShortcutClear(gentity_s *entity, actor_lifetime_t *a, const float *goal)
{
    int clear = ActorClearSegment(entity, entity->r.currentOrigin, goal);
    if (clear) {
        float gx = goal[0]-a->shortcutGoal[0], gy = goal[1]-a->shortcutGoal[1], gz = goal[2]-a->shortcutGoal[2];
        if (!a->shortcutCheckTime || level.time - a->shortcutCheckTime >= 250 || level.time < a->shortcutCheckTime ||
            gx*gx + gy*gy + gz*gz > 48.0f*48.0f) {
            a->shortcutHullClear = (unsigned char)ActorHullWalk(entity, entity->r.currentOrigin, goal);
            a->shortcutCheckTime = level.time;
            memcpy(a->shortcutGoal, goal, 12);
        }
        clear = a->shortcutHullClear;
    }
    /* level.time + 1: zero means "not in this state". */
    if (clear) { a->shortcutBlockedSince = 0; if (!a->shortcutClearSince) a->shortcutClearSince = level.time + 1; }
    else { a->shortcutClearSince = 0; if (!a->shortcutBlockedSince) a->shortcutBlockedSince = level.time + 1; }
    /* On a route: walking one now, or last frame (a chased goal that moved
     * resets the route in ActorSetGoal before this runs). */
    int onRoute = (a->routeCount > 0 && a->routeCursor < a->routeCount) || (a->prevRoute == 1 && a->usingRoute);
    if (onRoute) return clear && level.time + 1 - a->shortcutClearSince >= 250;
    if (clear) return 1;
    /* Last frame walked the straight leg: hold it through a short block. */
    return a->prevRoute == 1 && !a->usingRoute && level.time + 1 - a->shortcutBlockedSince < 150;
}
/* Check the first movement step toward the node after a proposed waypoint
 * advance with the actor's real hull.  The lifted straight-line shortcut
 * intentionally ignores floor seams; it also misses the steep dirt face at
 * Prototype node 435, where a +18 sweep is clear but the actual actor's
 * forward trace hits a non-walkable (normal z .08/.22) slope at fraction 0.
 * A floor contact (normal z >= .7) is not an obstacle.  When the face is
 * steep, mirror physicsMove's up/forward/down step trial before refusing to
 * advance: a climbable kerb must remain traversable. */
static int ActorRouteContinuationBlocked(gentity_s *entity, const float *target)
{
    const float *start = entity->r.currentOrigin;
    float dx = target[0]-start[0], dy = target[1]-start[1];
    float length = sqrtf(dx*dx + dy*dy);
    if (length < 0.001f) return 0;
    float travel = fminf(length, 16.0f) / length;
    float ahead[3] = {start[0]+dx*travel, start[1]+dy*travel, start[2]};
    trace_t tr, upTrace, raisedTrace, landing;
    ActorsPassable(entity, 1);
    G_TraceCapsule(&tr, start, entity->r.mins, entity->r.maxs,
        ahead, entity->s.number, entity->clipmask);
    if (!tr.startsolid && !tr.allsolid &&
        (tr.fraction >= 1.0f || tr.normal[2] >= 0.7f)) {
        ActorsPassable(entity, 0);
        return 0;
    }
    float up[3] = {start[0], start[1], start[2]+18.0f};
    G_TraceCapsule(&upTrace, start, entity->r.mins, entity->r.maxs,
        up, entity->s.number, entity->clipmask);
    if (upTrace.startsolid || upTrace.allsolid || upTrace.fraction <= 0.0f) {
        ActorsPassable(entity, 0);
        return 1;
    }
    float lift = 18.0f*upTrace.fraction;
    float raised[3] = {start[0], start[1], start[2]+lift};
    float raisedAhead[3] = {ahead[0], ahead[1], ahead[2]+lift};
    G_TraceCapsule(&raisedTrace, raised, entity->r.mins, entity->r.maxs,
        raisedAhead, entity->s.number, entity->clipmask);
    if (raisedTrace.startsolid || raisedTrace.allsolid || raisedTrace.fraction < 1.0f) {
        ActorsPassable(entity, 0);
        return 1;
    }
    float down[3] = {raisedAhead[0], raisedAhead[1], raisedAhead[2]-lift-0.25f};
    G_TraceCapsule(&landing, raisedAhead, entity->r.mins, entity->r.maxs,
        down, entity->s.number, entity->clipmask);
    ActorsPassable(entity, 0);
    return landing.startsolid || landing.allsolid ||
        landing.fraction >= 1.0f || landing.normal[2] < 0.7f;
}
/* Waypoint links are authored for player traversal and the combat-training
 * bots walk them without a per-edge trace; a per-edge capsule sweep rejected
 * every sloped link. Dogs skip prone/climb links (no traversal animations). */
static int ActorRouteWalkable(int stance)
{
    /* BOT_NEGOTIATE is the map's node_negotiation_begin -> _end pair,
     * the link T5 Path_IsNegotiationLink hands EVERY actor (a window sill on
     * the zm maps: 12 of them ring zm_prototype_sh, and they are the only
     * links between the spawner yard and the interior).  Rejecting it as a
     * CLIMB, as this filter did until 2026-09-21, made every zombie route
     * fail on a connected graph -- see docs/ZOMBIE_ROUTE_20260921.md. */
    return stance == BOT_STAND || stance == BOT_CROUCH || stance == BOT_NEGOTIATE;
}
static int ActorStartBanned(const actor_lifetime_t *a, int node)
{
    unsigned n = a->bannedStartCount < 4 ? a->bannedStartCount : 4;
    for (unsigned i = 0; i < n; ++i)
        if (a->bannedStart[i] == node) return 1;
    return 0;
}
static int ActorRouteContinuationBlocked(gentity_s *entity, const float *target);
/* The route endpoints for an actor: the nearest waypoint it may stand on and,
 * for the start, can walk to.  A bare nearest-node pick can land on a CLIMB
 * node (a player mantle, e.g. mp_killhouse_sh node 59 on the ladder at
 * (3691 169)), which the stance filter then refuses as an endpoint, so every
 * route to a goal beside it failed with "no path between nearest nodes"; and
 * the nearest node to a dog can be across a knee-high wall (killhouse node
 * 102 from (3199 -489)), which it then ran into for good.  The start is the
 * nearest of the eight closest walkable nodes the dog's hull can reach in a
 * straight line, else the nearest walkable one. */
static int ActorRouteEdge(int from, int to, void *context);
static int ActorNearestWalkableNode(gentity_s *entity, const actor_lifetime_t *a,
    const float *point, int reachable, int from)
{
    /* Every species since 2026-10-01: zombies wedged on the same node-behind-
     * an-obstacle starts the dogs did (Prototype yard, VPS 17733 log). */
    int nearest = BotNavNearest(point);
    if (nearest < 0 || !a) return nearest;
    enum { CANDIDATES = 8 };
    int best[CANDIDATES]; float bestDistance[CANDIDATES]; int count = 0;
    for (int i = 0, n = BotNavCount(); i < n; ++i) {
        const float *p = BotNavPoint(i);
        if (!p || !ActorRouteWalkable(BotNavStance(i))) continue;
        if (reachable == 1 && ActorStartBanned(a, i)) continue;
        float d = (float)Vec3DistanceSq(p, point);
        int slot;
        if (count < CANDIDATES) slot = count++;
        else if (d >= bestDistance[CANDIDATES-1]) continue;
        else slot = CANDIDATES - 1;
        while (slot > 0 && bestDistance[slot-1] > d) {
            best[slot] = best[slot-1]; bestDistance[slot] = bestDistance[slot-1]; --slot;
        }
        best[slot] = i; bestDistance[slot] = d;
    }
    if (!count) return nearest;
    /* Reachable: the lifted straight sweep is clear AND the actor's own first
     * step toward the node is (the lifted sweep passes over a knee-high
     * steep face the step cannot climb). */
    if (reachable == 1)
        for (int c = 0; c < count; ++c)
            if (ActorClearSegment(entity, point, BotNavPoint(best[c])) &&
                !ActorRouteContinuationBlocked(entity, BotNavPoint(best[c]))) return best[c];
    /* The goal end (reachable 2): the nearest node the route can reach from
     * the start node `from` over this actor's walkable links, and that the
     * hull can walk from to the goal, as T5 Path_FindPath ends its path at a
     * node with a clear last leg.  The bare nearest node can stand across a
     * low wall from the goal, and the last leg from it then wedged on the
     * wall; or it can be a node only a climb link reaches (mp_rust node 166
     * (1486 991 -133), on a ledge 100 above the goals below it), and the
     * route failed outright, so the actor fell back to the straight line on
     * that frame and planned again on the next (MEASURED 2026-10-02, mp_rust
     * chases: "no path between nearest nodes", 11 links rejected by the
     * stance filter). */
    if (reachable == 2) {
        static unsigned short scratch[BOT_NODES];
        int routable[CANDIDATES];
        int known = 0;
        for (int c = 0; c < count; ++c) routable[c] = -1;
        /* Keep the tested early return for the nearest walkable last leg.
         * If a search exhausts its frontier, use upstream's closed-set answer
         * for the remaining candidates instead of repeating the same search. */
        for (int pass = 0; pass < 2; ++pass) {
            for (int c = 0; c < count; ++c) {
                if (!pass && bestDistance[c] > 512.0f*512.0f) continue;
                if (routable[c] < 0) {
                    if (from < 0 || best[c] == from) routable[c] = 1;
                    else if (BotNavComponent(best[c]) != BotNavComponent(from)) routable[c] = 0;
                    else {
                        int reached = known ? BotNavLastSearchReached(best[c]) : -1;
                        if (reached >= 0) routable[c] = reached;
                        else {
                            routable[c] = BotNavRouteFiltered(from, best[c], scratch,
                                BOT_NODES, ActorRouteEdge, entity) > 0;
                            known = !routable[c] && BotNavLastRouteStats()->stop == 1;
                        }
                    }
                }
                if (routable[c] && (pass || ActorHullWalk(entity, BotNavPoint(best[c]), point)))
                    return best[c];
            }
        }
        /* Preserve upstream's no-path marker and never plan best[0] again. */
        s_goalEndUnroutable = 1;
    }
    return best[0];
}
/* A dog embedded in solid with no clear waypoint within 64 units cannot
 * move at all (every sweep starts solid).  T5 has no such state -- its
 * actors never leave the navmesh -- so the dog is placed on the nearest
 * clear walkable waypoint that no living player can see (a pop in view is
 * worse than a longer hop), else the nearest clear one. */
static int ActorWaypointHidden(const float *point)
{
    for (int i = 0; i < level.maxclients; ++i) {
        gentity_s *player = &g_entities[i];
        if (!player->r.inuse || !player->client || player->health <= 0) continue;
        float eye[3] = {player->r.currentOrigin[0], player->r.currentOrigin[1],
            player->r.currentOrigin[2] + player->client->ps.viewHeightCurrent};
        float target[3] = {point[0], point[1], point[2] + 32.0f}, zero[3] = {0,0,0};
        trace_t tr;
        G_TraceCapsule(&tr, eye, zero, zero, target, i, 0x1);
        if (!tr.startsolid && tr.fraction >= 1.0f) return 0;
    }
    return 1;
}
static int ActorRecoverFromSolid(gentity_s *entity, actor_lifetime_t *a)
{
    int bestHidden = -1, bestAny = -1;
    float hiddenDistance = INFINITY, anyDistance = INFINITY, bestOrigin[2][3];
    for (int i = 0, n = BotNavCount(); i < n; ++i) {
        const float *p = BotNavPoint(i);
        if (!p || !ActorRouteWalkable(BotNavStance(i))) continue;
        float d = (float)Vec3DistanceSq(p, entity->r.currentOrigin);
        if (d >= anyDistance && d >= hiddenDistance) continue;
        float origin[3] = {p[0], p[1], p[2]+1.0f};
        trace_t probe;
        ActorsPassable(entity, 1);
        G_TraceCapsule(&probe, origin, entity->r.mins, entity->r.maxs, origin, entity->s.number, entity->clipmask);
        ActorsPassable(entity, 0);
        if (probe.startsolid || probe.allsolid) continue;
        if (d < anyDistance) { anyDistance = d; bestAny = i; memcpy(bestOrigin[1], origin, 12); }
        if (d < hiddenDistance && ActorWaypointHidden(origin)) {
            hiddenDistance = d; bestHidden = i; memcpy(bestOrigin[0], origin, 12);
        }
    }
    int pick = bestHidden >= 0 ? 0 : 1, node = bestHidden >= 0 ? bestHidden : bestAny;
    if (node < 0) return 0;
    ((void)0);
    G_SetOrigin(entity, bestOrigin[pick]);
    memcpy(a->sentient->origin, bestOrigin[pick], 12);
    memset(a->physics.velocity, 0, 12);
    a->routeCount = a->routeCursor = 0;
    a->routeRetryTime = 0;
    SV_LinkEntity(entity);
    return 1;
}
static int ActorLinkBanned(const actor_lifetime_t *a, int from, int to)
{
    unsigned n = a->bannedLinkCount < 8 ? a->bannedLinkCount : 8;
    for (unsigned i = 0; i < n; ++i)
        if ((a->bannedLinkFrom[i] == from && a->bannedLinkTo[i] == to) ||
            (a->bannedLinkFrom[i] == to && a->bannedLinkTo[i] == from)) return 1;
    return 0;
}
static int ActorRouteEdge(int from, int to, void *context)
{
    if (!ActorRouteWalkable(BotNavStance(from)) || !ActorRouteWalkable(BotNavStance(to))) return 0;
    const actor_lifetime_t *a = context ? ActorForEntity((gentity_s *)context) : NULL;
    return !a || !ActorLinkBanned(a, from, to);
}
/* Reachability is about a walkable route, not visibility: a player around
 * an open corner remains eligible, a nearer player across a closed door does
 * not. Keep start/end hull legs strict so nearest-node projection cannot
 * silently move either endpoint through a wall or onto another floor. */
static int ActorTargetNearestNodes(const float *point, int *best)
{
    float distances[8];
    int count = 0;
    for (int i = 0, n = BotNavCount(); i < n; ++i) {
        const float *p = BotNavPoint(i);
        if (!p || !ActorRouteWalkable(BotNavStance(i))) continue;
        float d = (float)Vec3DistanceSq(p, point);
        if (d > 512.0f * 512.0f) continue;
        int slot;
        if (count < 8) slot = count++;
        else if (d >= distances[7]) continue;
        else slot = 7;
        while (slot > 0 && distances[slot - 1] > d) {
            best[slot] = best[slot - 1]; distances[slot] = distances[slot - 1]; --slot;
        }
        best[slot] = i; distances[slot] = d;
    }
    return count;
}
static int ActorRequiresReachablePlayer(const actor_lifetime_t *a)
{ return a && a->species && a->species->perfectInfo && !strcmp(a->species->category, "zm"); }
static int ActorReachablePlayer(actor_lifetime_t *a, gentity_s *player)
{
    if (!a || !a->entity || !player || !player->client || !BotNavCount()) return 0;
    int starts[8], ends[8], start = -1;
    int ns = ActorTargetNearestNodes(a->entity->r.currentOrigin, starts);
    /* Bodies are moving combatants, not room/door barriers. World, actor
     * clip, doors and debris remain in the actor's normal movement mask. */
    int mask = a->entity->clipmask & ~CONTENTS_BODY;
    for (int i = 0; i < ns; ++i) {
        if (ActorStartBanned(a, starts[i])) continue;
        if (ActorHullWalk(a->entity, a->entity->r.currentOrigin,
                BotNavPoint(starts[i]), mask, 18.0f)) { start = starts[i]; break; }
    }
    if (start < 0) return 0;
    int ne = ActorTargetNearestNodes(player->r.currentOrigin, ends);
    for (int i = 0; i < ne; ++i) {
        if (!ActorTargetNodeReachable(&a->targetReachability, start, ends[i],
                a->bannedLinkCount, ActorRouteEdge, a->entity)) continue;
        if (ActorHullWalk(a->entity, BotNavPoint(ends[i]),
                player->r.currentOrigin, mask, 18.0f)) return 1;
    }
    return 0;
}
/* Once per actor: which nodes the failed route ran between and why the
 * search gave up, so a failure on a connected graph names its cause. */

/* T5 Actor_PathEndActions notifies "bad_path" when no route exists; the
 * ported _dogs.gsc patrol waits on it to pick another node. Once per goal. */
enum { ACTOR_PATH_WAIT_MS = 500, ACTOR_PATH_WAIT_SPREAD_MS = 250 };
/* T5's 500 ms, plus a per-entity spread: actors that failed on the same
 * frame (one unreachable goal shared by a crowd) otherwise all plan again on
 * the same frame every 500 ms (MEASURED 2026-10-21: 24 zombies, 50 ms
 * frames twice a second). */
static int ActorPathWaitTime(const gentity_s *entity)
{
    return level.time + ACTOR_PATH_WAIT_MS + (entity->s.number * 37) % ACTOR_PATH_WAIT_SPREAD_MS;
}
static void ActorRouteFailed(gentity_s *entity, actor_lifetime_t *a, const char *why)
{
    if (a->routeFailNotified) return;
    a->routeFailNotified = 1;
    ++a->routeFailCount;
    if (level.time - a->routeFailLogTime >= 5000) {
        ((void)0);
        a->routeFailLogTime = level.time; a->routeFailCount = 0;
    }
    unsigned name = SL_GetString("bad_path", 0);
    Scr_Notify(entity, (unsigned short)name, 0);
    SL_RemoveRefToString(name);
}
static int ActorRouteRetryPending(gentity_s *entity, actor_lifetime_t *a)
{
    if (!a->routeRetryTime) return 0;
    /* Compare against the failed attempt, not the last setGoalPos call:
     * scripts may re-issue the same goal or move it a few units each tick. */
    if (level.time < a->routeFailedTime || level.time >= a->routeRetryTime ||
        Vec3DistanceSq(a->goal, a->routeFailedGoal) > 64.0f*64.0f ||
        Vec3DistanceSq(entity->r.currentOrigin, a->routeFailedOrigin) > 64.0f*64.0f ||
        a->bannedLinkCount != a->routeFailedBannedLinks ||
        a->bannedStartCount != a->routeFailedBannedStarts) {
        a->routeRetryTime = 0;
        return 0;
    }
    return 1;
}
static void ActorRouteRetryLater(gentity_s *entity, actor_lifetime_t *a)
{
    a->routeFailedTime = level.time;
    a->routeRetryTime = level.time + 500;
    memcpy(a->routeFailedGoal, a->goal, 12);
    memcpy(a->routeFailedOrigin, entity->r.currentOrigin, 12);
    a->routeFailedBannedLinks = a->bannedLinkCount;
    a->routeFailedBannedStarts = a->bannedStartCount;
}
static void ActorRouteTopologyChanged(void)
{
    /* Barrier/door changes can make the unchanged goal reachable at once. */
    for (unsigned i = 0; i < MAX_ACTOR_SLOTS; ++i)
        actors[i].routeRetryTime = 0;
}
int ActorSetGoal(gentity_s *entity, const float *goal)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !goal || !isfinite(goal[0]) || !isfinite(goal[1]) ||
        !isfinite(goal[2])) return 0;
    int changed = !a->hasGoal || fabsf(goal[0]-a->goal[0])>1 ||
        fabsf(goal[1]-a->goal[1])>1 || fabsf(goal[2]-a->goal[2])>1;
    /* T5 Actor_SetGoalPos re-paths only when the destination leaves the
     * current path's last node: a chased enemy moves the attack spot every
     * frame, and re-planning from the nearest node (usually just behind the
     * dog) each frame made the dog ping-pong around that node. */
    int keepRoute = a->hasGoal && a->routeCount > 0 && a->routeCursor < a->routeCount &&
        (BotNavNearest(goal) == (int)a->route[a->routeCount-1] ||
         /* the route ends at the node with a walkable last leg to the goal it
          * was planned for (ActorNearestWalkableNode, reachable 2), not
          * always the nearest one: keep it while the goal stays near that */
         (goal[0]-a->routeGoal[0])*(goal[0]-a->routeGoal[0]) + (goal[1]-a->routeGoal[1])*(goal[1]-a->routeGoal[1]) <= 64.0f*64.0f);
    memcpy(a->goal, goal, sizeof(a->goal));
    a->hasGoal = 1;
    a->chaseRadius = 0;
    a->goalNode = -1;
    a->goalNotified = 0;
    a->routeFailNotified = 0;
    if (!keepRoute) a->routeCount = a->routeCursor = 0;
    /* The committed last leg (finalLeg) stays committed while the goal stays
     * within 64 units of where it was when the route ran out: a chased
     * enemy moves the goal every few frames. */
    if (a->finalLeg) {
        float fx = goal[0]-a->finalLegGoal[0], fy = goal[1]-a->finalLegGoal[1];
        if (fx*fx + fy*fy > 64.0f*64.0f || fabsf(goal[2]-a->finalLegGoal[2]) > 40.0f) a->finalLeg = 0;
    }
    float gx=goal[0]-a->diagGoal[0], gy=goal[1]-a->diagGoal[1], gz=goal[2]-a->diagGoal[2];
    if (changed && gx*gx+gy*gy+gz*gz > 64*64) {
        memcpy(a->diagGoal, goal, 12);
        ++a->goalLogCount;
        if (level.time - a->goalLogTime >= 5000) {
            ((void)0);
            a->goalLogTime = level.time; a->goalLogCount = 0;
        }
    }
    return 1;
}
/* actorcontact scenarios (actor_contact_probe.c, developer
 * only): place a live actor, stop it hunting and send it to a goal with the
 * given radius, so actor-actor contact can be staged on any map. */
extern "C" int ActorDevDrive(gentity_s *entity, const float *origin, const float *goal, float radius)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->initialized || entity->health <= 0 || a->negotiating || a->scripted) return 0;
    a->ignoreAll = 1;
    a->enemyNum = -1;
    DogCombatRelease(entity);
    if (origin) {
        float o[3] = {origin[0], origin[1], origin[2]};
        G_SetOrigin(entity, o);
        memcpy(a->sentient->origin, o, 12);
        memset(a->physics.velocity, 0, 12);
        SV_LinkEntity(entity);
    }
    a->routeRetryTime = 0;
    a->stuckTime = 0; a->stuckCount = 0;
    if (!ActorSetGoal(entity, goal)) return 0;
    a->goalRadius = radius;
    return 1;
}
int ActorGoalDirection(gentity_s *entity, float *direction)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->hasGoal || !direction || !entity->clipmask) return 0;
    memset(direction, 0, 3 * sizeof(float));
    float dx = a->goal[0] - entity->r.currentOrigin[0];
    float dy = a->goal[1] - entity->r.currentOrigin[1];
    float dz = a->goal[2] - entity->r.currentOrigin[2];
    float radius = a->chaseRadius > 0 ? a->chaseRadius : a->goalRadius;
    if (dx*dx + dy*dy <= radius*radius && fabsf(dz) <= a->goalHeight) {
        if (!a->goalNotified) {
            a->goalNotified = 1;
            unsigned name = SL_GetString("goal", 0);
            Scr_Notify(entity, (unsigned short)name, 0);
            SL_RemoveRefToString(name);
        }
        return 2;
    }
    const float *next = a->goal;
    /* A wedge on the straight shortcut suspends it for a few seconds: the
     * +18 sweep passes over a knee-high wall the actor's step cannot climb
     * under a low overhang (zm_zombie_training's final-room yard at
     * (-478 3876 433): every frame took the shortcut into the wall, the
     * route over node 74 was never followed, and with no route leg there
     * was no link to ban). */
    /* The straight leg is taken when the hull can walk it (ActorShortcutClear:
     * the real step test, with hysteresis), or when the route has already
     * run out and this is its last leg.  MEASURED 2026-10-02 (mp_rust,
     * actorcontact cross/follow at (1070 280 -240)): with the lifted
     * sweep alone deciding every frame, dogs beside a low obstacle switched
     * between the one-node route "node 7 -> 7" and the straight line on
     * alternate frames, turning 70+ degrees each time (the dog turn
     * animscript then looped); see the probe's route-flip tally. */
    int shortcut = a->finalLeg ||
        (level.time >= a->shortcutBanTime && ActorShortcutClear(entity, a, next));
    if (!shortcut) {
        if (a->routeCursor >= a->routeCount &&
            a->pathWaitTime - level.time > 0 && a->pathWaitTime - level.time <= ACTOR_PATH_WAIT_MS + ACTOR_PATH_WAIT_SPREAD_MS) {
            /* T5 Actor_FindPathToGoalDirect plans only once level.time
             * reaches pathWaitTime, which Actor_HandleInvalidPath sets 500 ms
             * after a failure.  Planning again every frame cost each actor
             * ~3 ms (an unreachable goal exhausts the frontier, once per
             * goal-end candidate): MEASURED 2026-10-21 zm_sumpf_sh, the solo
             * last-stand retreat goal (8178 770 -702) was unreachable for
             * seven zombies and hounds, every 16.7 ms game frame took ~20 ms,
             * and the unbounded catch-up spiralled into 0.5-3.8 s stalls
             * with no snapshots.  Until then the actor stands, as after the
             * failure itself (no straight line through what the planner
             * refused; see below). */
            return 0;
        }
        if (a->routeCursor >= a->routeCount) {
            if (ActorRouteRetryPending(entity, a)) {
                /* Chases retain their existing straight-line fallback;
                 * scripted goals wait without running another full search. */
                if (!a->chaseRadius) return 0;
                goto straight;
            }
            unsigned long long planStart = Sys_MicrosecondsLong();
            int from = ActorNearestWalkableNode(entity, a, entity->r.currentOrigin, 1, -1);
            s_goalEndUnroutable = 0;
            int to = ActorNearestWalkableNode(entity, a, a->goal, 2, from);
            int unroutable = s_goalEndUnroutable;
            const float *start = BotNavPoint(from), *end = BotNavPoint(to);
            /* MEASURED 2026-09-20: CoDOL ships pathnodes for NO map -- no
             * pathdata asset class exists in the zone inventory, the zm maps
             * are plain game_map_mp and ALL_rawfile.txt has no node entries.
             * The only graphs on this server are the 15 hand-authored
             * bin/home/main/waypoints/mp_*_wp.csv, all multiplayer, so on a
             * zm map BotNavCount() is 0 and there is nothing to plan
             * over. Refusing to move is then not caution, it is the actor
             * standing still forever; the straight line is the only path
             * there is, and physics still slides it along geometry. With a
             * graph loaded the chaseRadius gate stands: a scripted patrol
             * goal should use the nodes rather than walk into a wall. */
            if (!start || !end) {
                if (BotNavCount() > 0) ActorRouteRetryLater(entity, a);
                ActorRouteFailed(entity, a, "no waypoint graph");
                if (!a->chaseRadius && BotNavCount() > 0) return 0;
                goto straight;
            }
            /* A script goal (not a chase) whose nearest waypoint is more than
             * 512 units away lies off the graph: the waypoints cover the
             * playable area, and the map's own path nodes, which
             * _dogs::init_node_arrays patrols between, also cover level
             * geometry outside it (mp_killhouse_sh (-143 -1478), ~3000
             * units from any waypoint).  Walking the straight line from
             * the last waypoint wedged the dog on the boundary wall until it
             * left; T5 Path_FindPath fails such a goal, so this reports
             * bad_path and the patrol picks another node.  Dogs only: the zm
             * entrance walk owns its own goals and bad_path handling. */
            if (!a->chaseRadius && a->species && !strcmp(a->species->name, "dog") &&
                Vec3DistanceSq(end, a->goal) > 512.0f*512.0f) {
                ActorRouteRetryLater(entity, a);
                ActorRouteFailed(entity, a, "goal is off the waypoint graph");
                a->pathWaitTime = ActorPathWaitTime(entity);
                return 0;
            }
            a->routeCount = unroutable ? 0 : BotNavRouteFiltered(from, to, a->route,
                BOT_NODES, ActorRouteEdge, entity);
            s_framePlanUs += Sys_MicrosecondsLong() - planStart;
            a->routeCursor = 0;
            memcpy(a->routeGoal, a->goal, 12);
            if (!a->routeCount) {
                ActorRouteRetryLater(entity, a);
                a->pathWaitTime = ActorPathWaitTime(entity);
                ActorRouteFailed(entity, a, "no path between nearest nodes");
                /* Upstream T5 behavior: no straight fallback through closed
                 * doors/debris. Keep the tested retry throttle as well. */
                return 0;
            }
            a->routeRetryTime = 0;
            /* T5 paths start at the actor and its lookahead trims a first node
             * when the second is walkable from here (Path_UpdateLookahead).
             * Planned from the nearest node, a re-plan while moving put that
             * node behind the actor every time a chased goal moved, and the
             * dog turned round to it and back (MEASURED 2026-10-02, mp_rust:
             * one dog re-planned "111 -> 82/81/88/179" 15 times in 2 s and
             * ran move_run_turn_around_left between them).  A negotiation
             * begin node is never skipped. */
            if (a->routeCount >= 2 && BotNavNegotiationEnd(a->route[0]) != (int)a->route[1]) {
                const float *first = BotNavPoint(a->route[0]), *second = BotNavPoint(a->route[1]);
                if (first && second &&
                    Vec3DistanceSq(entity->r.currentOrigin, second) < Vec3DistanceSq(first, second) &&
                    ActorClearSegment(entity, entity->r.currentOrigin, second) &&
                    ActorHullWalk(entity, entity->r.currentOrigin, second))
                    a->routeCursor = 1;
            }
        }
        /* Advance past nodes reached (24 units: a run frame moves ~17) or
         * passed within the following link's 24-unit corridor.  A bare dot
         * test marked a node passed even when the actor was far to the side:
         * on Prototype it skipped node 268 from 181 units away and aimed
         * through a brush at node 270; another actor skipped node 154 from
         * 103 away and walked into world clip toward node 515. */
        while (a->routeCursor < a->routeCount) {
            const float *node = BotNavPoint(a->route[a->routeCursor]);
            if (!node) { a->routeCount = 0; return 0; }
            dx = node[0] - entity->r.currentOrigin[0];
            dy = node[1] - entity->r.currentOrigin[1];
            int passed = dx*dx + dy*dy <= 24.0f*24.0f;
            /* A negotiation begin node whose end node is the next leg is
             * REACHED, never walked past: T5 starts the traversal at the
             * node (Actor_Negotiation_Start) and the traversal animation is
             * authored from it.  The "behind me" shortcut below would skip
             * it from 30 units to the side and send the mover into the wall. */
            int beginOfLeg = a->routeCursor + 1 < a->routeCount &&
                BotNavNegotiationEnd(a->route[a->routeCursor]) == (int)a->route[a->routeCursor+1];
            if (!passed && !beginOfLeg && a->routeCursor + 1 < a->routeCount) {
                const float *after = BotNavPoint(a->route[a->routeCursor+1]);
                if (after) {
                    float legX = after[0]-node[0], legY = after[1]-node[1];
                    float legSq = legX*legX + legY*legY;
                    float cross = dx*legY - dy*legX;
                    passed = legSq > 0 && dx*legX + dy*legY < 0 &&
                        cross*cross <= 24.0f*24.0f*legSq;
                }
            }
            if (passed && !beginOfLeg && a->routeCursor + 1 < a->routeCount) {
                const float *after = BotNavPoint(a->route[a->routeCursor+1]);
                if (after) {
                    float legX = after[0]-node[0], legY = after[1]-node[1];
                    float legSq = legX*legX + legY*legY;
                    float cross = dx*legY - dy*legX;
                    /* The static walk used a centered 31-unit hull.  Only an
                     * actor off that centerline needs this extra live probe;
                     * centered actors can use the graph's normal step path. */
                    /* The 16-unit continuation probe sees only the first
                     * step; T5 Path_UpdateLookahead moves on to the next
                     * point only when Path_PredictionTrace -- the hull
                     * walking the whole way -- reaches it.  MEASURED
                     * 2026-10-23, zm_theater_sh window (656 -1145 346): the
                     * one way to its boards is a 62-unit gap (x 707..769) in
                     * a thin wall at y -1189, and node 470 (736 -1208) leads
                     * through it to node 475 (720 -1136).  Zombies reached
                     * 470 within the corridor 19 units to its side, the probe
                     * 27 units short of the wall passed, and they turned for
                     * 475 into the gap's edge; the wedge then banned link
                     * 470->475, the only way in, and every zombie of spawner
                     * (624 -1344 360) stood at the wall for good. */
                    if (legSq > 0 && cross*cross > 8.0f*8.0f*legSq &&
                        (ActorRouteContinuationBlocked(entity, after) ||
                         !ActorHullWalk(entity, entity->r.currentOrigin, after))) passed = 0;
                }
            }
            if (!passed) break;
            if (beginOfLeg && !a->negotiating) {
                a->negotiationPending = 1;
                a->negotiationBegin = a->route[a->routeCursor];
                a->negotiationEnd = a->route[a->routeCursor+1];
                break;   /* RunFrame starts the traversal; the cursor stays on the begin node */
            }
            ++a->routeCursor;
        }
        if (a->routeCursor >= a->routeCount) {
            /* Route exhausted: final leg straight at the goal; physics
             * slides along whatever the reachability sweep disliked.  The
             * leg is committed to (finalLeg) until the goal moves away or
             * the actor wedges on it: re-testing it every frame planned the
             * same exhausted route again and again. */
            a->finalLeg = 1;
            memcpy(a->finalLegGoal, a->goal, 12);
            a->routeCount = a->routeCursor = 0;
            next = a->goal;
        } else {
            next = BotNavPoint(a->route[a->routeCursor]);
            if (!next) { a->routeCount = 0; return 0; }
        }
    }
straight:
    if (next == a->goal) a->routeCount = a->routeCursor = 0;
    a->usingRoute = next != a->goal;
    float length = 0;
    for (unsigned i = 0; i < 3; ++i) {
        direction[i] = next[i] - entity->r.currentOrigin[i];
        length += direction[i]*direction[i];
    }
    if (length <= 0) return 0;
    length = sqrtf(length);
    for (unsigned i = 0; i < 3; ++i) direction[i] /= length;
    a->routeDir[0] = direction[0]; a->routeDir[1] = direction[1];
    /* Separation steering (minimal T5 actor avoidance): other live actors are
     * passable to the clip sweeps above, so steer around any inside two
     * capsule radii instead of walking into/through them and piling up.
     * MEASURED 2026-10-02 (actorcontact, zm_prototype_sh crowd around
     * a test client): two overlapping zombies 11-18 apart summed a steer of
     * 0.8-1.07 and the raw sum swung their walk 45-65 degrees off the route
     * (route yaw 105 -> 40, -169 -> -122, -63 -> -108); they orbited each
     * other and made under 16 units of progress per 2 s for 6 s.  The same
     * sum fed the dog turn check (70 degrees) and set off turn animscripts.
     * T5 avoids by dodging around the other actor (Path_AttemptDodge), never
     * by walking away from its path: only the part of the steer across the
     * route is used, a neighbour straight ahead is passed on the right
     * (two actors meeting head-on each go right and clear), and the
     * deflection is capped at ~31 degrees, inside dog_turn90_angle. */
    float steer[2] = {0, 0};
    for (unsigned i = 0; i < MAX_ACTOR_SLOTS; ++i) {
        actor_lifetime_t *o = &actors[i];
        if (!o->allocated || !o->initialized || o->entity == entity || !o->entity || !o->entity->r.inuse ||
            o->entity->health <= 0) continue;
        float ox = entity->r.currentOrigin[0] - o->entity->r.currentOrigin[0];
        float oy = entity->r.currentOrigin[1] - o->entity->r.currentOrigin[1];
        float oz = entity->r.currentOrigin[2] - o->entity->r.currentOrigin[2];
        float d = sqrtf(ox*ox + oy*oy);
        if (fabsf(oz) > 48.0f || d >= 40.0f) continue;
        if (d < 1.0f) { ox = -direction[1]; oy = direction[0]; d = 1.0f; }
        float w = (40.0f - d) / 40.0f;
        steer[0] += ox / d * w; steer[1] += oy / d * w;
    }
    a->diagSteer = sqrtf(steer[0]*steer[0] + steer[1]*steer[1]);
    float flat = sqrtf(direction[0]*direction[0] + direction[1]*direction[1]);
    if ((steer[0] != 0 || steer[1] != 0) && flat > 0.001f) {
        float fx = direction[0] / flat, fy = direction[1] / flat;
        float rightX = fy, rightY = -fx;
        float along = steer[0]*fx + steer[1]*fy;
        float across = steer[0]*rightX + steer[1]*rightY;
        if (along < 0) {
            /* Someone ahead: sidestep by how far ahead it is, on the side
             * the steer already leans to, else on the right. */
            float side = across < -0.05f ? -1.0f : 1.0f;
            across += side * -along;
        }
        across = fmaxf(-0.6f, fminf(0.6f, across));
        direction[0] = fx + rightX * across; direction[1] = fy + rightY * across; direction[2] = 0;
        float h = sqrtf(direction[0]*direction[0] + direction[1]*direction[1]);
        direction[0] /= h; direction[1] /= h;
    }
    return 1;
}

/* Apply the real native animation/physics displacement supplied for this
 * frame. No fixed dog speed, teleport-to-target or timed bite is substituted.
 * Route direction is consumed by the AI/orientation producer before this call.
 * Full step/gravity physics remains the movement owner's responsibility. */
int ActorMoveDelta(gentity_s *entity, const float *delta)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->initialized || !delta || !entity->clipmask) return 0;
    float end[3], origin[3];
    for (unsigned i = 0; i < 3; ++i) {
        if (!isfinite(delta[i])) return 0;
        end[i] = entity->r.currentOrigin[i] + delta[i];
        if (!isfinite(end[i])) return 0;
    }
    trace_t trace;
    G_TraceCapsule(&trace, entity->r.currentOrigin, entity->r.mins, entity->r.maxs,
                  end, entity->s.number, entity->clipmask);
    if (trace.startsolid || trace.allsolid) { ++a->probeDeltaFails; return 0; }
    for (unsigned i = 0; i < 3; ++i)
        origin[i] = entity->r.currentOrigin[i] + delta[i] * trace.fraction;
    G_SetOrigin(entity, origin);
    memcpy(a->sentient->origin, origin, sizeof(origin));
    SV_LinkEntity(entity);
    if (trace.fraction < 1.0f) a->routeCount = 0;
    return 1;
}

/* ActorMoveDelta with the other live actors passable: world, movers
 * and players still stop the move.  For moves whose whole point is to get
 * out of another actor (the overlap push-out and the wedge nudge): with the
 * other actor solid, every such sweep starts inside its CONTENTS_BODY and is
 * refused (MEASURED 2026-10-02, actorcontact: overlapping zombies
 * logged ~2 refused push-outs per frame, 218-240 per 2 s, and stayed fused). */
static int ActorMoveDeltaPastActors(gentity_s *entity, const float *delta)
{
    ActorsPassable(entity, 1);
    int moved = ActorMoveDelta(entity, delta);
    ActorsPassable(entity, 0);
    return moved;
}

extern "C" void XAnimCalcDelta(DObj *, unsigned, float *, float *, unsigned char);
/* The retained XAnim info pool (src/asmsource/_xanim.asm:13269, 4096 records
 * of 56 bytes). The per-second diagnostic walks it directly, for two MEASURED
 * reasons (docs/ZOMBIE_ANIM_KNOB_20260921.md section 2):
 *   - XAnimGetDescendantWithGreatestWeight(tree, x) takes an INFO index, not
 *     an anim index: its first instructions are `lea eax,[edx*8]; shl edx,6;
 *     sub edx,eax; lea eax,[edx+g_xAnimInfo]` (_xanim.asm:2045-2049), and
 *     both retained callers (:2410, :2589) hand it the info index they were
 *     iterating. Passing anim 0 read info slot 0 -- the pool's null record --
 *     and always answered 0, so the field never named anything.
 *   - XAnimGetAnimName reads XAnim_s::debugAnimNames, which is NULL at
 *     developer 0 (model_animation.c:FindAnimInServerTree notes
 *     the same), so the name was "" on every production boot regardless.
 * A leaf's XAnimParts::name is real asset data and is used instead. */
extern "C" XAnimInfo g_xAnimInfo[4096];
#define XANIM_INFO_COUNT 4096u

/* Describe what an actor's server animtree is playing: follow the heaviest
 * child from the root info down to a leaf and report that clip's state.
 * "w" is the blended weight XAnimCalcDelta uses, "g" the goal the script
 * asked for, "t"/"ot" the clip's normalised time and the previous frame's
 * (root motion is their difference), "infos" the record count of the whole
 * tree. */
/* T5 path pace (2026-10-21, docs/ZOMBIE_T5_MEASURED.md section 1).
 * MEASURED in real T5 (Plutonium, t5cap): T5 moves a pathing actor by the
 * per-frame root motion of its clip (Actor_GetAnimDeltas -> XAnimCalcDelta),
 * and T5's clips have their own root tracks -- walk_v4 covers a steady 47-48
 * u/s every 50 ms frame, walk_fast_v1 58 u/s with a 110 u/s lurch once a
 * cycle.  C1 ships the same clips at the same lengths with different tracks:
 * some linear (walk_fast_v1, a flat 65 u/s), some raw and uneven (walk_v2
 * steps 0.74 -> 3.18 units per 30 Hz frame, walk_v4 10 -> 113 u/s), so the
 * C1 delta made CoDOL zombies surge and stall (walk_v4 6-63 u/s per frame,
 * medians off by up to 40 %).  The path distance of a frame is therefore
 * taken from T5's own track of each clip (actor_t5_root.c, measured
 * with getMoveDelta) between the clip's previous and current time, blended
 * as the tree blends; a delta clip with no T5 profile moves at its C1
 * average speed.  Returns -1 when the tree has no weighted delta leaf. */
/* The zombies-mode species ("zombie", "zombie_dog"): the ones the T5
 * captures measured.  Coop soldiers and the MP dog keep their behaviour. */
static int ActorIsZmSpecies(const actor_lifetime_t *a)
{
    return a && a->species && a->species->name && !strncmp(a->species->name, "zombie", 6);
}
extern "C" void XAnimGetAbsDelta(const XAnim_s *anims, unsigned animIndex, float *rot, float *trans, float time);
static int s_paceTrace;
static int ActorSubtreeHasDelta(const XAnimTree_s *tree, unsigned info, int depth)
{
    if (!info || info >= XANIM_INFO_COUNT || depth > 32) return 0;
    const XAnimInfo *n = &g_xAnimInfo[info];
    if (n->animIndex >= tree->anims->size) return 0;
    const XAnimEntry *e = &tree->anims->entries[n->animIndex];
    if (!e->numAnims) {
        /* A delta clip that never moves is not motion: C1 flags the hound's
         * look-at additives (zombie_dog_look_up/_right, beside the run knob)
         * as delta clips with zero travel, and counting them took 10-17 % of
         * the run's weight -- the hound ran 290-355 u/s instead of T5's
         * flat 362 (ai_tracePace, staged run on zm_sumpf_sh). */
        const XAnimParts *parts = e->u.parts;
        if (!parts || !parts->bDelta || !parts->numframes) return 0;
        const t5_root_profile_t *profile = T5RootProfile(parts->name);
        if (profile) return profile->cumulative[T5_ROOT_STEPS] > 0.5f;
        float rot[4] = {0, 0, 0, 0}, trans[3] = {0, 0, 0};
        XAnimGetAbsDelta(tree->anims, n->animIndex, rot, trans, 1.0f);
        return isfinite(trans[0]) && isfinite(trans[1]) && trans[0]*trans[0] + trans[1]*trans[1] > 0.25f;
    }
    unsigned guard = 0;
    for (unsigned c = n->children; c && c < XANIM_INFO_COUNT && guard < 256; c = g_xAnimInfo[c].next, ++guard)
        if (g_xAnimInfo[c].state.weight > 0.0f && ActorSubtreeHasDelta(tree, c, depth + 1)) return 1;
    return 0;
}

static float ActorTreeRootTravel_r(const XAnimTree_s *tree, unsigned info, float weight, float seconds,
                                   float rate, int depth, int *found)
{
    if (!info || info >= XANIM_INFO_COUNT || depth > 32 || weight <= 0.0f) return 0.0f;
    const XAnimInfo *n = &g_xAnimInfo[info];
    unsigned anim = n->animIndex;
    if (anim >= tree->anims->size) return 0.0f;
    const XAnimEntry *e = &tree->anims->entries[anim];
    float r = n->state.rate > 0.0f ? n->state.rate : rate;
    if (!e->numAnims) {
        const XAnimParts *parts = e->u.parts;
        if (!parts || !parts->bDelta || parts->numframes == 0 || parts->framerate <= 0.0f) return 0.0f;
        *found = 1;
        float length = (float)parts->numframes / parts->framerate;
        /* A profile only fits a clip of T5's length: C1's ai_zombie_walk_v6 is
         * 5.0 s where T5's is 4.2 s, and its feet keep C1's cycle, so such a
         * clip moves at its own average speed instead (22.8 u/s; T5 22.9). */
        const t5_root_profile_t *profile = T5RootProfile(parts->name);
        if (profile && fabsf(profile->length - length) > 0.02f * profile->length) profile = 0;
        if (s_paceTrace) Com_Printf(CON_CHANNEL_SERVER, "  pace leaf %s w %.3f t %.4f ot %.4f cyc %d/%d rate %.2f prof %d\n",
            parts->name, weight, n->state.time, n->state.oldTime, n->state.cycleCount, n->state.oldCycleCount, r, profile ? 1 : 0);
        if (profile) {
            float now = T5RootTravel(profile, n->state.time);
            float before = T5RootTravel(profile, n->state.oldTime);
            int cycles = n->state.cycleCount - n->state.oldCycleCount;
            float d = now - before + (float)cycles * profile->cumulative[T5_ROOT_STEPS];
            if (d < 0.0f) d = cycles >= 0 ? now : 0.0f;   /* restarted this frame */
            return weight * d;
        }
        float rot[4] = {0, 0, 0, 0}, trans[3] = {0, 0, 0};
        XAnimGetAbsDelta(tree->anims, anim, rot, trans, 1.0f);
        if (!isfinite(trans[0]) || !isfinite(trans[1])) return 0.0f;
        return weight * sqrtf(trans[0] * trans[0] + trans[1] * trans[1]) / length * r * seconds;
    }
    /* Siblings without any root motion under them (the hound's look-at and
     * aim additives, which sit beside the run knob at weight 1) share no
     * weight with the moving branch, as in XAnimCalcDelta: only delta
     * branches split the motion. */
    float total = 0.0f;
    unsigned guard = 0;
    for (unsigned c = n->children; c && c < XANIM_INFO_COUNT && guard < 256; c = g_xAnimInfo[c].next, ++guard)
        if (g_xAnimInfo[c].state.weight > 0.0f && ActorSubtreeHasDelta(tree, c, depth + 1))
            total += g_xAnimInfo[c].state.weight;
    if (total <= 0.0f) return 0.0f;
    float travel = 0.0f;
    guard = 0;
    for (unsigned c = n->children; c && c < XANIM_INFO_COUNT && guard < 256; c = g_xAnimInfo[c].next, ++guard)
        if (g_xAnimInfo[c].state.weight > 0.0f && ActorSubtreeHasDelta(tree, c, depth + 1))
            travel += ActorTreeRootTravel_r(tree, c, weight * g_xAnimInfo[c].state.weight / total, seconds, r, depth + 1, found);
    return travel;
}

static float ActorTreeRootTravel(const XAnimTree_s *tree, float seconds)
{
    if (!tree || !tree->anims || !tree->children) return -1.0f;
    int found = 0;
    float travel = ActorTreeRootTravel_r(tree, tree->children, 1.0f, seconds, 1.0f, 0, &found);
    return found && isfinite(travel) ? travel : -1.0f;
}

int ActorAdvanceGoalAnimation(gentity_s *entity, int time, int useGoalWeight)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->initialized || time <= a->lastRootMotionTime) return 0;
    DObj *obj = Com_GetServerDObj(entity->s.number);
    if (!obj || !DObjGetTree(obj)) return 0;
    unsigned generation = a->generation;
    float direction[3]={0,0,0}, rotation[4]={0,0,0,0}, translation[3]={0,0,0};
    int mode=a->scriptAnimMode ? a->scriptAnimMode : a->animMode;
    /* T5 runs the dog turn animscript as AI_ANIM_MOVE_CODE: the path moves
     * and orients the dog (orient to motion) while the turn clip plays; the
     * script's animMode( "zonly_physics" ) leaves code in charge of xy.  Here
     * the path's pace is the clip's forward root motion (the C1 turn clips
     * are run starts), and the native facing turns the dog. */
    if(a->scripts.turn && a->animFunction==a->scripts.turn && a->species && a->species->quadruped)
        mode=a->animMode;
    int route = mode==1 && a->moveMode ? ActorGoalDirection(entity, direction) : 0;
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) return 0;
    float seconds=(level.time-level.previousTime)*.001f;
    if (!g_gravity || seconds<=0 || seconds>.25f) return 0;
    a->lastRootMotionTime=time;
    if(mode==8) {memset(a->physics.velocity,0,12);return 1;}
    XAnimCalcDelta(obj,0,rotation,translation,useGoalWeight!=0);
    for(int i=0;i<3;++i)if(!isfinite(translation[i]))return 0;
    if(!isfinite(rotation[0]) || !isfinite(rotation[1]))return 0;
    float delta[3]={0,0,0};
    if(mode==1 && route==1) {
        /* The path pace is horizontal: the route direction points at the
         * next node or goal in 3D, and its Z share used to go into the
         * gravity move as wish Z.  physicsMove takes a positive wish Z on
         * the ground as an upward velocity and adds it to the stored one in
         * the air, so an actor heading for a node up a flight of stairs or a
         * slope rose off the floor, kept climbing for a few frames and then
         * dropped back (the "launched on stairs" report, 2026-10-05).  T5
         * path motion is planar too (Path_UpdateLookahead / AIPhys gravity
         * moves take the 2D lookahead); height comes from the step-up, the
         * ground snap and gravity in physicsMove. */
        float distance=sqrtf(translation[0]*translation[0]+translation[1]*translation[1]);
        if (actor_t5_pace && actor_t5_pace->integer && ActorIsZmSpecies(a)) {
            s_paceTrace = actor_pace_trace && actor_pace_trace->integer == entity->s.number;
            float travel=ActorTreeRootTravel(DObjGetTree(obj),seconds);
            if (s_paceTrace) Com_Printf(CON_CHANNEL_SERVER, "pace ent %d t %d dt %.3f travel %.2f c1 %.2f\n",
                entity->s.number, level.time, seconds, travel, distance);
            s_paceTrace = 0;
            if (travel>=0.0f) distance=travel;
        }
        float flat=sqrtf(direction[0]*direction[0]+direction[1]*direction[1]);
        a->stuckAttempt+=distance;
        if(flat>0.0001f) {
            delta[0]=direction[0]/flat*distance;
            delta[1]=direction[1]/flat*distance;
        }
    } else if(mode==2 || mode==4 || mode==5 || mode==6 || mode==7) {
        float yaw=entity->r.currentAngles[1]*.01745329252f;
        delta[0]=cosf(yaw)*translation[0]-sinf(yaw)*translation[1];
        delta[1]=sinf(yaw)*translation[0]+cosf(yaw)*translation[1];
        delta[2]=(mode==5 || mode==6)?translation[2]:0;
    }
    if(mode>=3 && mode<=7 && (rotation[0]!=0 || rotation[1]!=0)) {
        float norm=rotation[0]*rotation[0]+rotation[1]*rotation[1];
        float turn=atan2f(2*rotation[0]*rotation[1]/norm,1-2*rotation[0]*rotation[0]/norm)*57.295779513f;
        entity->r.currentAngles[1]=fmodf(entity->r.currentAngles[1]+turn+360,360);
        memcpy(entity->s.lerp.apos.trBase,entity->r.currentAngles,12);
    }
    int moved=ActorPhysicsMoveMode(entity,&a->physics,delta,seconds,
        g_gravity->value,18.0f,mode==5?1:mode==6?2:mode==7?3:0);
    memcpy(a->lastTranslation,translation,12);
    if (!moved) ++a->stuckRejected;
    if (a->physics.blocked) ++a->stuckBlocked;
    if (moved) ++a->probeAccepted; else ++a->probeRejected;
    if (a->physics.blocked) ++a->probeBlocked;
    if (moved) memcpy(a->sentient->origin, entity->r.currentOrigin, 12);
    return moved;
}

/* Team NONE is free-for-all: everything except the owner and fellow actors is
 * hostile (T5 TEAM_FREE dogs). Any other team is hostile to other teams. */
static int ActorMayTarget(actor_lifetime_t *a, const sentient_t *s)
{
    if (!s || !s->entity) return 0;
    if (ActorRequiresReachablePlayer(a) &&
        (!s->entity->client || s->entity->client->sess.sessionState != SESS_STATE_PLAYING ||
         !ActorReachablePlayer(a, s->entity))) return 0;
    if (a->sentient->team) return s->team && s->team != a->sentient->team;
    if (s->isActor) return 0;
    return s->entity->s.number != a->sentient->ownerNum;
}
/* MP sight acquisition over the real sentient registry. A retained enemy
 * remains available when temporarily occluded; visible challengers are ranked
 * by the bias and distance terms of T5 Actor_UpdateSingleThreat
 * (actor_threat.cpp:422-458): enemy->iThreatBias + Actor_GetThreatBias(enemy
 * group, self group) + Actor_ThreatFromDistance, and a candidate whose group
 * this actor's group ignores is dropped (Actor_CheckIgnore, :246). Every
 * candidate that reaches the comparison is visible or perfectly known, so
 * T5's visibility/awareness term is the same constant for all of them and is
 * left out; equal threat goes to the nearer one, which with no biases set is
 * exactly the former nearest-visible rule (the distance term is monotonic
 * below 2500 and zero beyond it). Suppression, scariness, attacker count and
 * covering fire are not modelled for these species.
 *
 * A species whose perception column says perfectInfo (the zm zombie) skips
 * the sight trace entirely and takes the nearest hostile sentient. That is
 * not a shortcut around the trace: it is what T5 does for this actor at both
 * levels. MEASURED, OpenBLOPS Game/Server/game/actor_spawner.cpp:199-203 --
 * the ENEMYINFO spawnflag the actor_enemy_zombie_zm QUAKED block declares
 * runs Actor_GetPerfectInfo over EVERY sentient at spawn, which is the write
 * (actor_senses.cpp:514, lastKnownPosTime = level.time) that satisfies the
 * one awareness gate in the threat pass (actor_threat.cpp:241). MEASURED,
 * T5 _zombiemode_spawner.gsc:4013 find_flesh picks its target with
 * get_closest_valid_player, which is _zombiemode_utility.gsc GetClosest() --
 * distance only -- and pins it with self.favoriteenemy, which the threat
 * pass scores at 0x7ffffffe (actor_threat.cpp:317-321) and the sight pass
 * exempts from the FOV cone (actor_senses.cpp:303-307).
 *
 * Why it matters here, MEASURED 2026-09-20 in bin/home/main/qconsole.log:
 * every live zm zombie logged "enemy -1" for the whole match (360/360 diag
 * samples). The zm spawn structs put them 1000-2500 units from the player
 * behind map geometry, so the capsule trace below could never return a clear
 * line, no enemy was ever acquired, chaseRadius stayed 0, the straight-line
 * fallback in ActorGoalDirection was skipped and they logged
 * "moved 0/s" forever -- which is also why the player never saw one. */
static void ActorUpdateVisibleEnemy(gentity_s *entity)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    if(!a)return;
    gentity_s *best=NULL;
    float bestDistance=INFINITY;
    int bestThreat=THREATBIAS_IGNORE;
    int perfect=a->species && a->species->perfectInfo;
    int candidates=0;
    /* A retained enemy whose group this actor has since been told to ignore
     * is dropped: T5 re-filters every candidate each threat pass. */
    {
        gentity_s *current=ActorEnemy(entity);
        if(current && ActorIgnoresGroup(a, Sentient_ForEntity(current)))
            ActorSetEnemy(entity,NULL);
    }
    float eye[3];
    for(int i=0;i<3;++i)eye[i]=entity->r.currentOrigin[i]+(entity->r.mins[i]+entity->r.maxs[i])*.5f;
    for(sentient_t *s=Sentient_First(SENTIENT_MASK_ALL);s;
        s=Sentient_Next(s,SENTIENT_MASK_ALL)) {
        gentity_s *candidate=s->entity;
        if(!candidate || candidate==entity || !candidate->r.inuse || candidate->health<=0 || (candidate->flags & 4) ||
            s->dontTarget || !ActorMayTarget(a, s) || ActorIgnoresGroup(a, s))continue;
        ++candidates;
        float target[3],distance=0;
        for(int i=0;i<3;++i){target[i]=candidate->r.currentOrigin[i]+(candidate->r.mins[i]+candidate->r.maxs[i])*.5f;
            float d=target[i]-eye[i];distance+=d*d;}
        /* The score needs no trace, so a candidate that cannot win is
         * rejected before the sight test. */
        int threat=ActorThreatScore(a, s, sqrtf(distance));
        if(best && (threat<bestThreat || (threat==bestThreat && distance>=bestDistance)))continue;
        if(!perfect) {
            trace_t trace;float zero[3]={0,0,0};
            G_TraceCapsule(&trace,eye,zero,zero,target,entity->s.number,entity->clipmask & ~0x10000);
            // Endpoint is inside the target; accept its own entity hit as sight.
            if(trace.startsolid || trace.allsolid ||
                (trace.fraction<1 && (trace.hitType!=TRACE_HITTYPE_ENTITY || trace.hitId!=candidate->s.number)))continue;
        }
        best=candidate;bestDistance=distance;bestThreat=threat;
    }
    /* T5 Actor_UpdateThreat: a favourite enemy the actor is aware of (perfect
     * information, or in sight) outranks every other candidate, unless it is
     * ignored (setdonttarget / dontTarget, or an ignoreme group -- T5 drops
     * those before the favourite test, actor_threat.cpp:246) or not hostile. */
    gentity_s *favorite=ActorFavoriteEnemy(a);
    if(favorite && favorite!=entity && favorite->health>0) {
        sentient_t *fs=Sentient_ForEntity(favorite);
        int ok=fs && !fs->dontTarget && ActorMayTarget(a, fs) && !ActorIgnoresGroup(a, fs);
        if(ok && favorite->client && favorite->client->sess.sessionState!=SESS_STATE_PLAYING)ok=0;
        if(ok && !perfect) {
            float target[3];
            for(int i=0;i<3;++i)target[i]=favorite->r.currentOrigin[i]+(favorite->r.mins[i]+favorite->r.maxs[i])*.5f;
            trace_t trace;float zero[3]={0,0,0};
            G_TraceCapsule(&trace,eye,zero,zero,target,entity->s.number,entity->clipmask & ~0x10000);
            if(trace.startsolid || trace.allsolid ||
                (trace.fraction<1 && (trace.hitType!=TRACE_HITTYPE_ENTITY || trace.hitId!=favorite->s.number)))ok=0;
        }
        if(ok)best=favorite;
    }
    a->diagCandidates=candidates;
    if(best) {
        a->enemySeenTime=level.time;
        ActorSetEnemy(entity,best);
    } else {
        // T5 SentientInfo_Copy gates teammate information by sender species
        // mask and timestamp. Here the represented sight record is the target.
        for(unsigned i=0;i<MAX_ACTOR_SLOTS;++i) {
            actor_lifetime_t *sender=&actors[i];
            if(!sender->initialized || sender==a || !(sender->talkMask & ActorTalkBit(a)) ||
                sender->enemySeenTime<=a->enemySeenTime ||
                sender->sentient->team!=a->sentient->team)continue;
            gentity_s *known=ActorEnemy(sender->entity);
            if(known && ActorSetEnemy(entity,known))a->enemySeenTime=sender->enemySeenTime;
        }
    }
}
void ActorInheritSight(gentity_s *entity)
{
    actor_lifetime_t *a=ActorForEntity(entity);if(!a)return;
    for(unsigned i=0;i<MAX_ACTOR_SLOTS;++i) {
        actor_lifetime_t *sender=&actors[i];
        if(!sender->initialized || sender==a || !(sender->talkMask & ActorTalkBit(a)) ||
           sender->sentient->team!=a->sentient->team || sender->enemySeenTime<=a->enemySeenTime)continue;
        gentity_s *enemy=ActorEnemy(sender->entity);
        if(enemy && ActorSetEnemy(entity,enemy))a->enemySeenTime=sender->enemySeenTime;
    }
}
static int ActorIsCoopSpecies(const actor_lifetime_t *a);
static void ActorOrientToDirection(gentity_s *entity,const float *direction,float seconds)
{
    if(!direction || seconds<=0 || seconds>.25f)return;
    if(direction[0]==0 && direction[1]==0)return;
    float desired=atan2f(direction[1],direction[0])*57.295779513f;
    float delta=fmodf(desired-entity->r.currentAngles[1],360.0f);
    if(delta>180)delta-=360;if(delta< -180)delta+=360;
    // T5 Actor_UpdateBodyAngle's exposed/prone dog rate is 360 deg/s in the
    // MP binary.  An actor turns at most 220 deg/s: MEASURED in real T5 SP (Plutonium
    // t5cap, zombie_cod5_prototype, 2026-10-21) every capped turn step is
    // exactly 11.0 degrees per 50 ms think (119 of 123 steps over 10 deg).
    // OpenBLOPS' 300 (actor_orientation.cpp:14) is the MP binary's constant.
    actor_lifetime_t *turner=ActorForEntity(entity);
    // Hellhounds too: the T5 zombie_cod5_sumpf capture caps every hound turn
    // step at the same 11.0 degrees per think (38 of 41 steps over 10 deg).
    // Only the zm species were measured; coop soldiers and the MP dog keep
    // the MP binary's 300 / 360.
    int zm=ActorIsZmSpecies(turner);
    float rate=zm ? 220.0f : turner && turner->species && !turner->species->quadruped ? 300.0f : 360.0f;
    delta=fmaxf(-rate*seconds,fminf(rate*seconds,delta));
    float yaw=fmodf(entity->r.currentAngles[1]+delta+360.0f,360.0f);
    entity->r.currentAngles[0]=entity->r.currentAngles[2]=0;
    entity->r.currentAngles[1]=yaw;
    memcpy(entity->s.lerp.apos.trBase,entity->r.currentAngles,12);
}

/* ---------------------------------------------------------------------------
 * Negotiation (T5 actor_negotiation.cpp, OpenBLOPS Game/Server/game).
 *
 * The map's node_negotiation_begin -> node_negotiation_end pairs are the zm
 * window sills: MEASURED 2026-09-21 on zm_prototype_sh, the pf4 sill is a
 * 42-unit world brush (floor z 1 -> 42 at y 1018 -> 2 inside) under a lintel
 * that leaves ~108 units of headroom at the begin node -- an 18-unit step
 * never clears it, and the pairs carry animscript "zombie_mantle_over_40"
 * for exactly that reason.  T5's sequence:
 *   Actor_Negotiation_Start  - orient mode DONT_CHANGE, traverse mode
 *                              NOGRAVITY, momentum cleared;
 *   Actor_Negotiation_Think  - run the node's animscript
 *                              (animscripts/traverse/<animscript>::main) as
 *                              the actor's anim-specific script in
 *                              AI_ANIM_USE_BOTH_DELTAS_NOCLIP (the script
 *                              asks traverseMode "nogravity"+"noclip"); the
 *                              actor moves by the clip's root motion only;
 *   cleanup                  - when the script thread dies: nearest node :=
 *                              the end node, wish/velocity zeroed, path
 *                              recalculated, state popped.
 * The GSC half here is maps/mp/animscripts/<species>_jump::main(begin,
 * begin_angles, end, animscript), the species' traversal module (this
 * port's "jump" slot IS T5's traversal slot; zombie_jump.gsc is
 * T5 animscripts/traverse/zombie_mantle_over_40 + shared::DoTraverse).
 * MEASURED (obj/claude-workers/xanim_probe.py on common_gametype_zm): the
 * clips carry the link -- ai_zombie_traverse_v1 +64.6 units forward over 58
 * frames, v5 +62.7, v2 +70.1, v6 +108.6, crawl_v1 +76.8 -- and a "gravity
 * on" notetrack at t 0.85-0.98; only v1/v2 carry any z.  The sill is
 * crossed in noclip, as T5 crosses it, and the actor lands on the end node.
 * Alignment: T5 AnimScripted lerps the actor onto the given origin/angles;
 * the route follower only hands over within 24 units of the begin node, and
 * that remainder is snapped here so the authored clip starts where it was
 * authored to start.
 * ------------------------------------------------------------------------ */
struct actor_switch_params { float begin[3], angles[3], end[3]; const char *script; };
static const actor_switch_params *s_switchParams;
static const char *ActorNodeAnimscript(int node)
{
    int record = BotNavRecord(node);
    const char *v = record >= 0 ? PathNodeValue((unsigned)record, "animscript") : NULL;
    return v && *v ? v : "";
}
static int ActorStartNegotiation(gentity_s *entity)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a) return 0;
    int begin = a->negotiationBegin, end = a->negotiationEnd;
    const float *bp = BotNavPoint(begin), *ep = BotNavPoint(end);
    if (!bp || !ep || !a->scripts.jump || !a->scriptsStarted) return 0;
    if (BotNavLinkBlocked(begin, end)) {
        /* The barrier went back up (repair) after this route was planned. */
        a->routeCount = a->routeCursor = 0;
        return 0;
    }
    int record = BotNavRecord(begin);
    const char *anglesText = record >= 0 ? PathNodeValue((unsigned)record, "angles") : NULL;
    actor_switch_params params;
    params.script = ActorNodeAnimscript(begin);
    memcpy(params.begin, bp, 12); memcpy(params.end, ep, 12);
    params.angles[0] = params.angles[2] = 0;
    if (!anglesText || sscanf(anglesText, "%f %f %f", &params.angles[0], &params.angles[1], &params.angles[2]) != 3 ||
        !isfinite(params.angles[1])) {
        params.angles[0] = params.angles[2] = 0;
        params.angles[1] = atan2f(ep[1]-bp[1], ep[0]-bp[0]) * 57.295779513f;
    }
    float from[3]; memcpy(from, entity->r.currentOrigin, 12);
    float origin[3] = {bp[0], bp[1], entity->r.currentOrigin[2]};
    float angles[3] = {0, params.angles[1], 0};
    a->safeToChangeScript = 1;
    G_SetOrigin(entity, origin);
    memcpy(entity->r.currentAngles, angles, 12);
    memcpy(entity->s.lerp.apos.trBase, angles, 12);
    memcpy(a->sentient->origin, origin, 12);
    memset(a->physics.velocity, 0, sizeof(a->physics.velocity));
    a->prevMoveDir[0] = a->prevMoveDir[1] = 0;
    SV_LinkEntity(entity);
    a->negotiating = 1;
    a->negotiationPending = 0;
    a->negotiationStartTime = level.time;
    memcpy(a->negotiationStartOrigin, origin, 12);
    a->stuckTime = 0; a->stuckCount = 0; a->stuckAttempt = 0;
    ((void)0);
    s_switchParams = &params;
    int ok = ActorSwitchScript(entity, a->scripts.jump, 0, 1);
    s_switchParams = NULL;
    a = ActorForEntity(entity);
    if (!a) return 0;
    if (!ok || a->animFunction != a->scripts.jump) {
        ((void)0);
        a->negotiating = 0;
        a->routeCount = a->routeCursor = 0;
        return 0;
    }
    a->safeToChangeScript = 0;   /* nothing replaces the traversal but its own end (or death) */
    return 1;
}
/* ---------------------------------------------------------------------------
 * Dog wall hop.
 *
 * T5 Actor_Dog_Exposed_Think (OpenBLOPS game/actor_dog_exposed.cpp) runs the
 * species jump animscript (dog_jump::main -> "traverse_wallhop") with
 * AI_ANIM_USE_BOTH_DELTAS_NOCLIP when Actor_Dog_ObstacleInPath finds a solid
 * 32 units ahead at +20, and its map traversals (animscripts/traverse/
 * mantle_over_40, mantle_on_40, jump_down_40) cross 40-unit walls and ledges
 * over node_negotiation links.  CoDOL MP maps are navigated on the bot
 * waypoint graph, whose vault/mantle links (mp_killhouse_sh 99/100 -> 103/104:
 * a 32-unit cinderblock wall) carry no traversal of their own, so the dog
 * wedged on them.  Here a dog whose path is blocked by a low obstacle it can
 * clear takes the T5 traversal the geometry calls for:
 *   level landing       -> "traverse_wallhop"      (C1 german_shepherd_run_jump_40,  140 units)
 *   landing 18..48 up   -> "traverse_jump_up_40"   (C1 german_shepherd_traverse_up_40, 116 fwd, +40)
 *   landing 18..48 down -> "traverse_jump_down_40" (C1 german_shepherd_traverse_down_40, 106 fwd, -40)
 *   low opening         -> "traverse_window"       (C1 german_shepherd_run_jump_window_40, 192 units;
 *                          T5 mantle_window_36 / mantle_over_40), else the wall hop clip through it
 * The distances are the clips' own root motion (xanim delta tracks,
 * common_gametype_mp).  The hop needs a clear sweep 48 units up, over the
 * obstacle to the landing, and a floor there.
 * ------------------------------------------------------------------------ */
/* Every quadruped species (the MP dog, the zm hellhound) runs the dog's
 * attack spots, hop and turn paths; species.quadruped says which. */
static int ActorIsDog(const actor_lifetime_t *a)
{
    return a && a->species && a->species->quadruped;
}

/* ---------------------------------------------------------------------------
 * MP attack dog perception: T5 MP Actor_UpdateThreat with the AI events that
 * feed it (OpenBLOPS actor_threat.cpp :200-480, :586-800; actor_events.cpp
 * Actor_BroadcastPointEvent / Actor_ReceivePointEvent; sentient.cpp
 * Sentient_SetEnemy; the dvar defaults in game_mp/g_main_mp.cpp).
 *
 * MEASURED in real T5 MP (Plutonium t5mp, mp_nuked, four dogs against one
 * player, docs/MP_DOGS_T5_PARITY.md): only two dogs ever took the
 * player as their enemy -- ai_maxAttackerCount 2 drops a third from the
 * threat list -- and the other two patrolled past him for the whole minute.
 * Dogs learned of a player from sight, from a packmate that took a new enemy
 * (within 1024, a second after it did), from a packmate's pain (512) or
 * death (1024) at his hands, from his footsteps (512) and his gunshots
 * (2048).  The sight pass below (every species) knows no attacker limit and
 * hands every sighting to the whole pack at once: all four dogs mobbed the
 * one player, each new one waiting beside him in the stop script for an
 * attack spot.  The zm hound and the coop soldier keep that pass.
 * ------------------------------------------------------------------------ */
static cvar_t *ai_maxAttackerCount, *ai_eventDistNewEnemy, *ai_eventDistPain, *ai_eventDistDeath,
    *ai_eventDistFootstep, *ai_eventDistFootstepLite, *ai_eventDistGunShot, *ai_eventDistSilencedShot;
static void ActorRegisterMpDogDvars(void)
{
    ai_maxAttackerCount=Cvar_RegisterInt("ai_maxAttackerCount",2,0,1000,0,"Max number of AI's that will attack one player");
    ai_eventDistNewEnemy=Cvar_RegisterFloat("ai_eventDistNewEnemy",1024,0,30000,0,"Distance used for AI event");
    ai_eventDistPain=Cvar_RegisterFloat("ai_eventDistPain",512,0,30000,0,"Distance used for AI event");
    ai_eventDistDeath=Cvar_RegisterFloat("ai_eventDistDeath",1024,0,30000,0,"Distance used for AI event");
    ai_eventDistFootstep=Cvar_RegisterFloat("ai_eventDistFootstep",512,0,30000,0,"Distance used for AI event");
    ai_eventDistFootstepLite=Cvar_RegisterFloat("ai_eventDistFootstepLite",256,0,30000,0,"Distance used for AI event");
    ai_eventDistGunShot=Cvar_RegisterFloat("ai_eventDistGunShot",2048,0,30000,0,"Distance used for AI event");
    ai_eventDistSilencedShot=Cvar_RegisterFloat("ai_eventDistSilencedShot",128,0,30000,0,"Distance used for AI event");
}
static int ActorIsMpDog(const actor_lifetime_t *a)
{
    return ActorIsDog(a) && !ActorIsZmSpecies(a);
}
static int ActorClientNum(const gentity_s *e)
{
    return e && e->client && e->s.number>=0 && e->s.number<MAX_CLIENTS ? e->s.number : -1;
}
/* T5 Actor_GetPerfectInfo / Actor_UpdateLastKnownPos. */
static void MpDogKnow(actor_lifetime_t *a,const gentity_s *who)
{
    int c=ActorClientNum(who);
    if(c>=0)a->mpKnownTime[c]=level.time>0?level.time:1;
}
/* T5 sentient_s attackerCount: the actors whose enemy is target, self aside. */
static int ActorAttackerCount(const gentity_s *target,const actor_lifetime_t *self)
{
    int n=0;
    for(unsigned i=0;i<MAX_ACTOR_SLOTS;++i){
        const actor_lifetime_t *o=&actors[i];
        if(!o->initialized || o==self || !o->entity || o->entity->health<=0)continue;
        if(o->enemyNum==target->s.number && o->enemyGeneration==(unsigned)target->useCount)++n;
    }
    return n;
}
enum { MPDOG_EV_NEW_ENEMY, MPDOG_EV_PAIN, MPDOG_EV_DEATH, MPDOG_EV_NOISE };
/* T5 Actor_BroadcastPointEvent to the MP dogs within radius of origin.  A
 * team event (new enemy, pain, death) from a dog reaches its packmates (its
 * team and owner, never itself: Actor_IsOnSameTeam); a noise (footstep,
 * gunshot) from a player reaches the dogs hostile to him.  Every receiver
 * gets perfect information about `about` (Actor_EventNewEnemy's
 * SentientInfo_Copy, Actor_EventPain/Death/Gunshot's Actor_GetPerfectInfo,
 * Actor_EventFootstep's Actor_UpdateLastKnownPos); pain also counts as
 * `about` attacking the receiver (Actor_EventPain -> Actor_WasAttackedBy). */
static void MpDogBroadcast(int type,gentity_s *originator,const float *origin,float radius,gentity_s *about)
{
    int c=ActorClientNum(about);
    if(!originator || c<0 || radius<=0)return;
    actor_lifetime_t *from=ActorForEntity(originator);
    sentient_t *aboutSentient=Sentient_ForEntity(about);
    for(unsigned i=0;i<MAX_ACTOR_SLOTS;++i){
        actor_lifetime_t *r=&actors[i];
        if(!r->initialized || r==from || !r->entity || r->entity->health<=0 || !r->sentient || !ActorIsMpDog(r))continue;
        if(type==MPDOG_EV_NOISE){
            if(!aboutSentient || !ActorMayTarget(r,aboutSentient))continue;
        }else if(!from || !from->sentient || r->sentient->team!=from->sentient->team ||
                 r->sentient->ownerNum!=from->sentient->ownerNum)continue;
        float dx=r->entity->r.currentOrigin[0]-origin[0],dy=r->entity->r.currentOrigin[1]-origin[1],
              dz=r->entity->r.currentOrigin[2]-origin[2];
        if(dx*dx+dy*dy+dz*dz>radius*radius)continue;
        MpDogKnow(r,about);
        if(type==MPDOG_EV_PAIN)r->mpAttackedTime[c]=level.time>0?level.time:1;
    }
}
/* T5 G_ClientEvents (game_mp/g_active_mp.cpp:697-712): a player's footstep,
 * unless he is ducked or carries the quiet-footsteps perk, is an
 * AI_EV_FOOTSTEP (512) -- AI_EV_FOOTSTEP_LITE (256) for a walk -- to the
 * actors hostile to him.  C1's quiet-footsteps perk is specialty_quieter
 * (Ninja Pro, modQuieter -0.9; T5's Ninja is the same name).  The
 * footsteps follow the run cycle, so here a moving grounded player is heard
 * every frame; the radius is computed once a frame per client. */
static int residentFootstepTime[MAX_CLIENTS];
static float residentFootstepRadius[MAX_CLIENTS];
static float MpDogFootstepRadius(const gentity_s *player)
{

    int c=ActorClientNum(player);
    if(c<0)return 0;
    if(residentFootstepTime[c]==level.time)return residentFootstepRadius[c];
    const playerState_t *ps=&player->client->ps;
    float speed=sqrtf(ps->velocity[0]*ps->velocity[0]+ps->velocity[1]*ps->velocity[1]),radius=0;
    if(ps->groundEntityNum!=ENTITYNUM_NONE && !(ps->pm_flags&3) && speed>20.0f &&
       PlayerHasNamedPerk(ps,"specialty_quieter")!=1)
        radius=speed<130.0f ? ai_eventDistFootstepLite->value : ai_eventDistFootstep->value;
    residentFootstepTime[c]=level.time;residentFootstepRadius[c]=radius;
    return radius;
}
/* T5 FireWeapon's AI_EV_GUNSHOT / AI_EV_SILENCED_SHOT for a player's shot,
 * called from NotifyWeaponFired. */
extern "C" void ActorWeaponFired(gentity_s *ent,int weapon)
{
    if(ActorClientNum(ent)<0 || !ai_eventDistGunShot)return;
    int silenced=0;
    if(weapon>0)GetNativeWeaponSilenced(BG_GetWeaponDef(weapon),&silenced);
    MpDogBroadcast(MPDOG_EV_NOISE,ent,ent->r.currentOrigin,
        silenced?ai_eventDistSilencedShot->value:ai_eventDistGunShot->value,ent);
}
/* The player bots' targets among the actors (ol_bot.c acquire): the MP
 * attack dogs hostile to the bot -- the dogs' own rule run backwards, so a
 * bot never shoots its own team's dogs or, in FFA, its own.  MEASURED in T5
 * MP (Plutonium t5mp, mp_nuked, three bots against the dogs): T5's bots
 * shot the dogs as they would players and killed all 8 that came at them;
 * this server's bots only scanned the client slots and let the dogs bite
 * them unopposed (0 dogs hit).  Fills the dogs' entity
 * numbers and aim points (the middle of the dog's body); returns the count. */
extern "C" int ActorBotTargets(const gentity_s *bot,int *entities,float (*points)[3],int max)
{
    int n=0;
    sentient_t *bs=bot?Sentient_ForEntity((gentity_s *)bot):NULL;
    if(!bs)return 0;
    for(unsigned i=0;i<MAX_ACTOR_SLOTS && n<max;++i){
        actor_lifetime_t *a=&actors[i];
        if(!a->initialized || !a->entity || !a->entity->r.inuse || a->entity->health<=0 || !ActorIsMpDog(a) || !a->sentient)continue;
        if(!ActorMayTarget(a,bs))continue;
        entities[n]=a->entity->s.number;
        for(int k=0;k<3;++k)points[n][k]=a->entity->r.currentOrigin[k]+(a->entity->r.mins[k]+a->entity->r.maxs[k])*.5f;
        ++n;
    }
    return n;
}
/* The enemy changed to none: T5 Sentient_SetEnemy notifies "enemy" for
 * that too, which is what restarts maps\mp\_dogs::dog_patrol_when_no_enemy. */
static void MpDogLoseEnemy(gentity_s *entity,actor_lifetime_t *a)
{
    DogCombatRelease(entity);
    a->enemyNum=-1;a->enemyGeneration=0;a->mpNotifyTime=0;a->mpHadEnemy=0;
    unsigned name=SL_GetString("enemy",0);
    Scr_Notify(entity,(unsigned short)name,0);
    SL_RemoveRefToString(name);
}
static void ActorUpdateMpDogThreat(gentity_s *entity)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    if(!a)return;
    gentity_s *current=ActorEnemy(entity);
    /* Sentient_SetEnemy: the new-enemy team event goes out a second after
     * the enemy was taken (iEnemyNotifyTime). */
    if(current && a->mpNotifyTime && level.time>=a->mpNotifyTime){
        a->mpNotifyTime=0;
        MpDogBroadcast(MPDOG_EV_NEW_ENEMY,entity,entity->r.currentOrigin,ai_eventDistNewEnemy->value,current);
    }
    float eye[3];
    for(int i=0;i<3;++i)eye[i]=entity->r.currentOrigin[i]+(entity->r.mins[i]+entity->r.maxs[i])*.5f;
    gentity_s *best=NULL;
    int bestThreat=-2147483647,goodOnly=0,candidates=0,bestVisible=0;
    int maxAttackers=ai_maxAttackerCount->integer;
    for(int c=0;c<level.maxclients && c<MAX_CLIENTS;++c){
        gentity_s *p=&g_entities[c];
        if(!p->r.inuse || !p->client || p->health<=0 || p->client->sess.sessionState!=SESS_STATE_PLAYING)continue;
        sentient_t *s=Sentient_ForEntity(p);
        if(!s || s->dontTarget || !ActorMayTarget(a,s))continue;
        ++candidates;
        float target[3];
        for(int i=0;i<3;++i)target[i]=p->r.currentOrigin[i]+(p->r.mins[i]+p->r.maxs[i])*.5f;
        trace_t trace;float zero[3]={0,0,0};
        G_TraceCapsule(&trace,eye,zero,zero,target,entity->s.number,entity->clipmask & ~0x10000);
        int visible=!trace.startsolid && !trace.allsolid &&
            (trace.fraction>=1 || (trace.hitType==TRACE_HITTYPE_ENTITY && trace.hitId==p->s.number));
        if(visible){a->mpVisibleTime[c]=level.time>0?level.time:1;MpDogKnow(a,p);}
        float dx=p->r.currentOrigin[0]-entity->r.currentOrigin[0],dy=p->r.currentOrigin[1]-entity->r.currentOrigin[1],
              dz=p->r.currentOrigin[2]-entity->r.currentOrigin[2],d2=dx*dx+dy*dy+dz*dz;
        float heard=MpDogFootstepRadius(p);
        if(heard>0 && d2<=heard*heard)MpDogKnow(a,p);
        /* Actor_CheckIgnore (actor_threat.cpp:246): an ignoreme group is
         * skipped after the senses have run, so awareness survives the
         * group being un-ignored later. */
        if(ActorIgnoresGroup(a,s))continue;
        /* "unaware": never known, never a target. */
        if(a->mpKnownTime[c]<=0)continue;
        int isCurrent=p==current;
        int attackers=ActorAttackerCount(p,a);
        /* T5 Actor_ShouldIgnoreFromAttackerCount. The MP limit (default 2)
         * cannot admit a Survival dog after a normal infantry squad already
         * acquired the player: attackerCount includes every actor species.
         * The Survival wave's native actor budget is its admission limit;
         * keep MP's configurable cap and current-enemy retention unchanged. */
        if(!ActorCandidateWithinAttackerLimit(ServerZoneType(),
                isCurrent,attackers,maxAttackers,MAX_ACTOR_SLOTS))continue;
        int seen=a->mpVisibleTime[c] && level.time-a->mpVisibleTime[c]<10000;
        int attackedMe=a->mpAttackedTime[c] && level.time-a->mpAttackedTime[c]<10000;
        int threatening=seen || attackedMe;
        if(!threatening && goodOnly)continue;
        int aware=visible || level.time-a->mpKnownTime[c]<10000;
        float distance=sqrtf(d2);
        /* enemy->iThreatBias + Actor_GetThreatBias (actor_threat.cpp:455),
         * Actor_ThreatFromVisibilityAndAwareness, _FromDistance,
         * _FromAttackerCount and _BonusForCurrentEnemy (scariness and
         * covering fire are zero for a dog). */
        long long sum=ActorThreatScore(a,s,distance);
        sum+=visible?1000:aware?500:0;
        sum+=attackers*-150<-1000?-1000:attackers*-150;
        if(isCurrent)sum+=aware?(p->health<p->maxHealth*0.8f?1000:500):250;
        int threat=sum>0x7ffffffdLL?0x7ffffffd:sum<-0x7fffffffLL?-0x7fffffff:(int)sum;
        if(threat>bestThreat || (!goodOnly && threatening)){
            best=p;bestThreat=threat;goodOnly=threatening;bestVisible=visible;
        }
    }
    a->diagCandidates=candidates;
    if(best){
        if(best!=current){
            if(current && a->mpNotifyTime)
                MpDogBroadcast(MPDOG_EV_NEW_ENEMY,entity,entity->r.currentOrigin,ai_eventDistNewEnemy->value,current);
            if(ActorSetEnemy(entity,best))a->mpNotifyTime=level.time+1000;
        }
        if(bestVisible)a->enemySeenTime=level.time;
        a->mpHadEnemy=1;
    }else if(current || a->mpHadEnemy){
        MpDogLoseEnemy(entity,a);
    }
}
/* Sweep hull (mins, maxs) from raised to the landing xy at the same height,
 * drop it to a floor, and require the dog's own hull to stand there. */
static int ActorHopLanding(gentity_s *entity, const float *raised, const float *mins,
    const float *maxs, const float *dir, float distance, float *landing)
{
    trace_t tr;
    float over[3] = {raised[0]+dir[0]*distance, raised[1]+dir[1]*distance, raised[2]};
    G_TraceCapsule(&tr, raised, mins, maxs, over, entity->s.number, entity->clipmask);
    if (tr.startsolid || tr.allsolid || tr.fraction < 1.0f) return 0;
    float down[3] = {over[0], over[1], over[2]-96.0f};
    G_TraceCapsule(&tr, over, mins, maxs, down, entity->s.number, entity->clipmask);
    if (tr.startsolid || tr.allsolid || tr.fraction >= 1.0f || tr.normal[2] < 0.7f) return 0;
    landing[0] = over[0]; landing[1] = over[1]; landing[2] = over[2] - 96.0f*tr.fraction + 0.25f;
    G_TraceCapsule(&tr, landing, entity->r.mins, entity->r.maxs, landing, entity->s.number, entity->clipmask);
    return !tr.startsolid && !tr.allsolid;
}
static int ActorStartWallHop(gentity_s *entity, const float *direction)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!ActorIsDog(a) || !a->scripts.jump || !a->scriptsStarted || a->negotiating) return 0;
    if (a->hopTime && level.time - a->hopTime < 1000) return 0;
    float len = sqrtf(direction[0]*direction[0] + direction[1]*direction[1]);
    if (len < 0.001f) return 0;
    float dir[3] = {direction[0]/len, direction[1]/len, 0};
    const float *o = entity->r.currentOrigin;
    trace_t tr;
    float mins[3], ahead[3] = {o[0]+dir[0]*24.0f, o[1]+dir[1]*24.0f, o[2]};
    memcpy(mins, entity->r.mins, 12); mins[2] += 18.0f;
    ActorsPassable(entity, 1);
    /* T5 Actor_Dog_ObstacleInPath looks 32 ahead; the hull's own radius (15)
     * plus this 24-unit sweep reaches the same point.  A step (<18) is not an
     * obstacle; a slope (normal z >= .7) is walked, not hopped. */
    G_TraceCapsule(&tr, o, mins, entity->r.maxs, ahead, entity->s.number, entity->clipmask);
    int blocked = !tr.startsolid && !tr.allsolid && tr.fraction < 1.0f && tr.normal[2] < 0.7f;
    float raised[3] = {o[0], o[1], o[2]+48.0f}, landing[3];
    const char *state = NULL, *hopWhy = "no clear landing over it";
    if (blocked) {
        /* Open above (a wall): the whole dog clears it 48 up.  T5 dog_jump /
         * traverse\shared dog_jump_up / dog_jump_down. */
        G_TraceCapsule(&tr, o, entity->r.mins, entity->r.maxs, raised, entity->s.number, entity->clipmask);
        if (!tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f) {
            static const struct { const char *state; float distance, minDz, maxDz; } hops[3] = {
                {"traverse_wallhop", 140.0f, -18.0f, 18.0f},
                {"traverse_jump_up_40", 116.0f, 18.0f, 48.0f},
                {"traverse_jump_down_40", 106.0f, -48.0f, -18.0f},
            };
            for (int h = 0; h < 3 && !state; ++h) {
                if (!ActorHopLanding(entity, raised, entity->r.mins, entity->r.maxs, dir, hops[h].distance, landing)) continue;
                float dz = landing[2] - o[2];
                if (dz >= hops[h].minDz && dz <= hops[h].maxDz) state = hops[h].state;
            }
        } else {
            /* A low opening (a wall with a beam or sill above it: the
             * killhouse crouch passages, nodes 99/100 -> 103/104): the dog
             * dives through it.  T5 traverse\mantle_window_36 and
             * mantle_over_40 -> dog_wall_and_window_hop( "traverse_window" ),
             * C1 german_shepherd_run_jump_window_40, 192 units.  The opening
             * is probed with a 16-unit-tall slice of the dog's hull 36 up
             * (the gap over a <=36-unit wall) and the landing must be level. */
            float sliceMins[3] = {entity->r.mins[0], entity->r.mins[1], 0};
            float sliceMaxs[3] = {entity->r.maxs[0], entity->r.maxs[1], 16.0f};
            float window[3] = {o[0], o[1], o[2]+36.0f};
            G_TraceCapsule(&tr, o, sliceMins, sliceMaxs, window, entity->s.number, entity->clipmask);
            if (tr.startsolid || tr.allsolid || tr.fraction < 1.0f) hopWhy = "no headroom over it";
            else if (ActorHopLanding(entity, window, sliceMins, sliceMaxs, dir, 192.0f, landing) &&
                     fabsf(landing[2] - o[2]) <= 18.0f)
                state = "traverse_window";
            /* No room for the 192-unit dive: the 140-unit wall hop clip
             * through the same opening. */
            else if (ActorHopLanding(entity, window, sliceMins, sliceMaxs, dir, 140.0f, landing) &&
                     fabsf(landing[2] - o[2]) <= 18.0f)
                state = "traverse_wallhop";
        }
    }
    ActorsPassable(entity, 0);
    if (!state) {
        if (blocked && (!a->hopDiagTime || level.time - a->hopDiagTime >= 5000)) {
            a->hopDiagTime = level.time;
            ((void)0);
        }
        return 0;
    }
    actor_switch_params params;
    params.script = state;
    memcpy(params.begin, o, 12); memcpy(params.end, landing, 12);
    params.angles[0] = params.angles[2] = 0;
    params.angles[1] = atan2f(dir[1], dir[0]) * 57.295779513f;
    float angles[3] = {0, params.angles[1], 0};
    memcpy(entity->r.currentAngles, angles, 12);
    memcpy(entity->s.lerp.apos.trBase, angles, 12);
    memset(a->physics.velocity, 0, sizeof(a->physics.velocity));
    a->safeToChangeScript = 1;
    a->negotiating = 1;
    a->negotiationPending = 0;
    a->negotiationBegin = a->negotiationEnd = -1;
    a->negotiationStartTime = level.time;
    memcpy(a->negotiationStartOrigin, o, 12);
    a->wallHop = 1;
    memcpy(a->hopStart, o, 12); memcpy(a->hopLanding, landing, 12);
    a->stuckTime = 0; a->stuckCount = 0; a->stuckAttempt = 0;
    ((void)0);
    unsigned generation = a->generation;
    s_switchParams = &params;
    int ok = ActorSwitchScript(entity, a->scripts.jump, 0, 1);
    s_switchParams = NULL;
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) return 1;
    if (!ok || a->animFunction != a->scripts.jump) {
        ((void)0);
        a->negotiating = 0; a->wallHop = 0;
        return 0;
    }
    /* Actor_SetAnimScript(..., AI_ANIM_USE_BOTH_DELTAS_NOCLIP): the clip's
     * root motion carries the dog over, through the obstacle's clip. */
    a->scriptAnimMode = 5;
    a->scriptOrient = 1;
    a->scriptYaw = params.angles[1];
    a->safeToChangeScript = 0;
    return 1;
}
static void ActorFinishWallHop(gentity_s *entity, const char *why)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a) return;
    trace_t probe;
    const float *o = entity->r.currentOrigin;
    ActorsPassable(entity, 1);
    G_TraceCapsule(&probe, o, entity->r.mins, entity->r.maxs, o, entity->s.number, entity->clipmask);
    ActorsPassable(entity, 0);
    int solid = probe.startsolid || probe.allsolid;
    if (solid) {
        /* Ended inside the wall (a clip shorter than the gap, or a timeout):
         * finish at the landing the hop was planned for, else back where it
         * started.  Both were clear when the hop began. */
        const float *targets[2] = {a->hopLanding, a->hopStart};
        for (int t = 0; t < 2; ++t) {
            ActorsPassable(entity, 1);
            G_TraceCapsule(&probe, targets[t], entity->r.mins, entity->r.maxs, targets[t], entity->s.number, entity->clipmask);
            ActorsPassable(entity, 0);
            if (probe.startsolid || probe.allsolid) continue;
            float origin[3]; memcpy(origin, targets[t], 12);
            G_SetOrigin(entity, origin);
            memcpy(a->sentient->origin, origin, 12);
            SV_LinkEntity(entity);
            break;
        }
    }
    ((void)0);
    a->negotiating = 0;
    a->negotiationPending = 0;
    a->wallHop = 0;
    a->hopTime = level.time;
    a->routeCount = a->routeCursor = 0;
    memset(a->physics.velocity, 0, sizeof(a->physics.velocity));
    a->prevMoveDir[0] = a->prevMoveDir[1] = 0;
    a->scriptAnimMode = 0;
    a->scriptOrient = 0;
    a->safeToChangeScript = 1;
    a->stuckTime = 0; a->stuckCount = 0; a->stuckAttempt = 0;
}
static void ActorFinishNegotiation(gentity_s *entity, const char *why)
{
    actor_lifetime_t *hop = ActorForEntity(entity);
    if (hop && hop->wallHop) { ActorFinishWallHop(entity, why); return; }
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a) return;
    const float *bp = BotNavPoint(a->negotiationBegin), *ep = BotNavPoint(a->negotiationEnd);
    const float *o = entity->r.currentOrigin;
    float db = bp ? sqrtf((o[0]-bp[0])*(o[0]-bp[0]) + (o[1]-bp[1])*(o[1]-bp[1])) : 0;
    float de = ep ? sqrtf((o[0]-ep[0])*(o[0]-ep[0]) + (o[1]-ep[1])*(o[1]-ep[1])) : 0;
    trace_t probe;
    ActorsPassable(entity, 1);
    G_TraceCapsule(&probe, o, entity->r.mins, entity->r.maxs, o, entity->s.number, entity->clipmask);
    ActorsPassable(entity, 0);
    int solid = probe.startsolid || probe.allsolid;
    int crossed = ep && bp && de < db;
    if (ep && (!crossed || solid)) {
        /* The clip's root motion did not carry the actor over (a short or
         * delta-less clip, or a timeout): finish the link the honest way and
         * say so.  The end node is on the walkable graph by construction. */
        static const float lifts[3] = {1.0f, 20.0f, 44.0f};
        int snapped = 0;
        for (int l = 0; l < 3 && !snapped; ++l) {
            float origin[3] = {ep[0], ep[1], ep[2]+lifts[l]};
            ActorsPassable(entity, 1);
            G_TraceCapsule(&probe, origin, entity->r.mins, entity->r.maxs, origin, entity->s.number, entity->clipmask);
            ActorsPassable(entity, 0);
            if (probe.startsolid || probe.allsolid) continue;
            ((void)0);
            G_SetOrigin(entity, origin);
            memcpy(a->sentient->origin, origin, 12);
            SV_LinkEntity(entity);
            snapped = 1;
        }
        if (!snapped)
            ((void)0);
    } else {
        ((void)0);
    }
    /* T5 cleanup: momentum zero, path recalculated from the end node, state
     * popped; the ordinary selection resumes next frame. */
    a->negotiating = 0;
    a->negotiationPending = 0;
    a->routeCount = a->routeCursor = 0;
    memset(a->physics.velocity, 0, sizeof(a->physics.velocity));
    a->prevMoveDir[0] = a->prevMoveDir[1] = 0;
    a->scriptAnimMode = 0;
    a->scriptOrient = 0;
    a->safeToChangeScript = 1;
    a->stuckTime = 0; a->stuckCount = 0; a->stuckAttempt = 0;
}
static void ActorNegotiationFrame(gentity_s *entity)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a) return;
    unsigned generation = a->generation;
    int alive = a->animFunction == a->scripts.jump && a->animThread &&
        Scr_GetObjectType((unsigned short)a->animThread) != VAR_DEAD_THREAD;
    if (!alive) { ActorFinishNegotiation(entity, "complete"); return; }
    /* A coop soldier's ladder (the climb cycle for the ladder's height plus
     * the step on or off) runs longer than any zm window. */
    int timeout = a->wallHop ? 3000 : ActorIsCoopSpecies(a) ? 15000 : 8000;
    if (level.time - a->negotiationStartTime > timeout) { ActorFinishNegotiation(entity, "TIMED OUT"); return; }
    /* The traversal script owns facing ("face angle" = scriptOrient 1). */
    if (a->scriptOrient == 1) {
        float facing[3] = {cosf(a->scriptYaw*.01745329252f), sinf(a->scriptYaw*.01745329252f), 0};
        ActorOrientToDirection(entity, facing, (level.time-level.previousTime)*.001f);
    }
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) return;
    ActorAdvanceGoalAnimation(entity, level.time, 0);
}
/* Once per actor: the stuck detector fired next to a negotiation node.  Name
 * what the capsule hits on the way to the end node, the heights, the link's
 * cut state and the animscript, so a window stall is measured, not guessed. */


/* Every real wedge, once per actor per 48-unit spot: what the capsule sees
 * where it stands, below it, and 16 units along its wish direction at floor
 * level and one step up, plus the graph leg it was walking.  The window
 * detail above is added when a negotiation node is at hand. */
static const char *ActorHitKind(const trace_t *tr)
{
    return tr->fraction >= 1.0f && !tr->startsolid ? "nothing" :
        tr->hitType == TRACE_HITTYPE_ENTITY ? "entity" : tr->hitType == TRACE_HITTYPE_DYNENT_MODEL ? "dynent model" :
        tr->hitType == TRACE_HITTYPE_DYNENT_BRUSH ? "dynent brush" : tr->hitType == TRACE_HITTYPE_GLASS ? "glass" : "world";
}
static const char *ActorHitClass(const trace_t *tr)
{
    if (tr->hitType == TRACE_HITTYPE_ENTITY && tr->hitId < MAX_GENTITIES && g_entities[tr->hitId].classname)
        return SL_ConvertToString(g_entities[tr->hitId].classname);
    return "";
}


/* ------------------------------------------------------------------------
 * T5 AIS_SCRIPTEDANIM.  GScr_AnimScripted_SP (OpenBLOPS g_scr_main_mp.cpp
 * :20578) pushes the state, kills the animscript and hands animscripts/
 * zombie_scripted.gsc the parameters; that script publishes the clip with
 * the flagged knob-all and waits for "killanimscript".  Here the builtin
 * (script_anim.c) publishes the clip itself and this owns the actor
 * state: no goal, no route, no selection, no stuck detector while it holds.
 * MEASURED 2026-09-21 part 2: with the tear played by setFlaggedAnimKnobAll-
 * Restart from the entity thread, the move animscript's own knob-all loop
 * (zombie_move::MoveStandRun, every zombie_run_anim_update_time) drove the
 * walk cycle back to weight 1 over the tear clip, so no "board" note was
 * ever delivered and no board came down all session.
 * ------------------------------------------------------------------------ */
static void ActorFinishScripted(gentity_s *entity, const char *why)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a) return;
    ((void)0);
    a->scripted = 0; a->scriptedAlign = 0;
    a->safeToChangeScript = 1;
    a->scriptOrient = 0; a->scriptAnimMode = 0;
    a->routeCount = a->routeCursor = 0;
    memset(a->physics.velocity, 0, sizeof(a->physics.velocity));
    a->prevMoveDir[0] = a->prevMoveDir[1] = 0;
    a->stuckTime = 0; a->stuckCount = 0; a->stuckAttempt = 0; a->stuckRejected = a->stuckBlocked = 0;
}
static void ActorScriptedFrame(gentity_s *entity)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a) return;
    unsigned generation = a->generation;
    if (level.time >= a->scriptedEndTime) { ActorFinishScripted(entity, "ran its length"); return; }
    float seconds = (level.time-level.previousTime)*.001f;
    if (a->scriptOrient == 1) {
        float facing[3] = {cosf(a->scriptYaw*.01745329252f), sinf(a->scriptYaw*.01745329252f), 0};
        ActorOrientToDirection(entity, facing, seconds);
    }
    if (a->scriptedAlign) {
        /* GScr_UpdateScriptedAnim consumes the origin offset a little per
         * update rather than snapping (T5 SP: 0.25 units per 50 ms tick, for
         * its cutscene-scale offsets).  The tear spot is 2-32 units off after
         * a goalradius-2 walk: 120 u/s closes that inside the 0.3 s blend.
         * The slide is made with the other actors passable: the spot is
         * this actor's reservation, the three window spots are 28 apart
         * (narrower than two 15-radius capsules) and a crowd waits right
         * behind them, so against solid neighbours the slide started inside
         * one or met one at once and never moved.  MEASURED 2026-10-05
         * (pathtest soaks, zm_prototype_sh / zm_asylum_sh): 5-8% of board
         * tears began 10-32 units short of the spot and ended just as far
         * from it -- the zombie tore the boards from behind the crowd. */
        float d[3] = {a->scriptedOrigin[0]-entity->r.currentOrigin[0], a->scriptedOrigin[1]-entity->r.currentOrigin[1], 0};
        float len = sqrtf(d[0]*d[0]+d[1]*d[1]);
        float most = fmaxf(2.0f, 120.0f*seconds);
        if (len <= most) a->scriptedAlign = 0;
        else { d[0] *= most/len; d[1] *= most/len; }
        if (len > 0.1f) ActorMoveDeltaPastActors(entity, d);
        a = ActorForEntity(entity);
        if (!a || a->generation != generation) return;
    }
    ActorAdvanceGoalAnimation(entity, level.time, 0);
}
extern "C" int ActorScriptedAnimBegin(gentity_s *entity, const float *origin, const float *angles,
                                            const char *mode, int durationMsec)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->initialized || !a->scriptsStarted || entity->health <= 0) return 0;
    unsigned generation = a->generation;
    if (!a->scripted) ActorStopScript(entity);   /* T5 Actor_KillAnimScript */
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) return 0;
    static const char *names[]={"none","normal","pos deltas","angle deltas",
        "gravity","noclip","nogravity","zonly_physics","nophysics"};
    int m;
    for (m = 0; m < 9; ++m) if (mode && !strcmp(mode, names[m])) break;
    const float *o = entity->r.currentOrigin;
    float dx = origin[0]-o[0], dy = origin[1]-o[1], dz = origin[2]-o[2];
    int align = isfinite(dx) && isfinite(dy) && isfinite(dz) && dx*dx+dy*dy <= 64.0f*64.0f && fabsf(dz) <= 64.0f;
    a->scripted = 1;
    a->scriptedAlign = (unsigned char)align;
    a->safeToChangeScript = 0;
    a->scriptedStartTime = level.time;
    a->scriptedEndTime = level.time + (durationMsec > 0 ? durationMsec : 0) + 500;
    a->scriptAnimMode = m < 9 ? m : 1;
    a->scriptOrient = 1;
    a->scriptYaw = isfinite(angles[1]) ? angles[1] : entity->r.currentAngles[1];
    /* A delta mode ("angle deltas" .. "zonly_physics": IW4 scene playback,
     * maps/_coopanim.gsc) turns the actor by the clip's own yaw delta
     * (ActorAdvanceGoalAnimation). Holding the scene angle as well
     * would undo every turn the clip makes, so the actor is set to the
     * scene's start yaw once and then left to the clip. */
    if (a->scriptAnimMode >= 3 && a->scriptAnimMode <= 7 && isfinite(angles[1])) {
        a->scriptOrient = 0;
        entity->r.currentAngles[1] = a->scriptYaw;
        memcpy(entity->s.lerp.apos.trBase, entity->r.currentAngles, 12);
    }
    memcpy(a->scriptedOrigin, origin, 12);
    /* "noclip" / "nogravity" (T5 AI_ANIM_USE_BOTH_DELTAS_NOCLIP): the clip
     * owns all three axes, so the actor starts exactly at the given origin,
     * height included -- a zm riser starts 45 (v1) or 14 (v2) units under
     * the floor and ai_zombie_traverse_ground_* carries it up (+45.2 / +14.8
     * z, xanim dump).  T5 AnimScripted lerps onto that origin; the riser is
     * hidden while it does, so it is placed at once here. */
    if (align && (a->scriptAnimMode == 5 || a->scriptAnimMode == 6)) {
        float placed[3] = {origin[0], origin[1], origin[2]};
        G_SetOrigin(entity, placed);
        memcpy(a->sentient->origin, placed, 12);
        SV_LinkEntity(entity);
        a->scriptedAlign = 0;
    }
    a->routeCount = a->routeCursor = 0;
    a->negotiationPending = 0;
    a->moveMode = 0;   /* no route direction: the clip's own delta (if any) is the only motion */
    memset(a->physics.velocity, 0, sizeof(a->physics.velocity));
    a->prevMoveDir[0] = a->prevMoveDir[1] = 0;
    a->stuckTime = 0; a->stuckCount = 0; a->stuckAttempt = 0; a->stuckRejected = a->stuckBlocked = 0;
    ((void)0);
    return 1;
}
extern "C" int ActorScriptedAnimStop(gentity_s *entity, const char *why)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->scripted) return 0;
    ActorFinishScripted(entity, why ? why : "stopped");
    return 1;
}

static actor_lifetime_t *ActorMethodReceiver(scr_entref_t ref);
static int ActorCoopPlace(gentity_s *entity,const float *origin,float *out);
static int ActorIsCoopSpecies(const actor_lifetime_t *a);
extern "C" unsigned int __cdecl Scr_GetConstLowercaseString(unsigned int index);
extern "C" void __cdecl BG_EvaluateTrajectory(trajectory_t *tr, int atTime, float *result);
/* The parent's origin and angles at `time`, from its trajectories: a mover
 * may run after the actor in this frame, so its currentOrigin can still be
 * last frame's. */
static void ActorRideParentFrame(gentity_s *parent,int time,float *origin,float *angles)
{
    BG_EvaluateTrajectory(&parent->s.lerp.pos,time,origin);
    BG_EvaluateTrajectory(&parent->s.lerp.apos,time,angles);
}
/* Riding: placed at its seat in the parent's frame, no AI, no movement. The
 * animscript threads still run, so the pose the script set keeps playing. */
static int ActorRideFrame(gentity_s *entity,actor_lifetime_t *a)
{
    gentity_s *parent=&g_entities[a->rideNum-1];
    if(!parent->r.inuse || parent->useCount!=a->rideSpawnCount) {
        Com_Printf(CON_CHANNEL_SERVER,"CoDOL actor ride: ent %d lost its vehicle (ent %d); stopped\n",
                   entity->s.number,a->rideNum-1);
        a->rideNum=0;
        return 0;
    }
    float origin[3],angles[3],forward[3],right[3],up[3],pos[3],face[3];
    ActorRideParentFrame(parent,level.time,origin,angles);
    AngleVectors(angles,forward,right,up);
    for(int i=0;i<3;++i)
        pos[i]=origin[i]+forward[i]*a->rideLocal[0]+right[i]*a->rideLocal[1]+up[i]*a->rideLocal[2];
    face[0]=0;face[1]=fmodf(angles[1]+a->rideYaw+720.0f,360.0f);face[2]=0;
    /* A coop rider carried by a collision-less mover along a hand-placed
     * path (defense crash: Ghost down the stairs on a script_origin) is
     * kept out of the world the path cuts through: lifted, up to a stair's
     * height, until the capsule is clear. MEASURED 2026-10-08
     * (codol_actorAudit): 90 frames of that ride had his capsule inside the
     * stairs. A vehicle seat (the parent has collision of its own) is left
     * exactly where the tag puts it. */
    if(ActorIsCoopSpecies(a) && !parent->r.contents) {
        trace_t t;
        int mask=entity->clipmask & ~0x2000000;
        G_TraceCapsule(&t,pos,entity->r.mins,entity->r.maxs,pos,entity->s.number,mask);
        if(t.startsolid || t.allsolid) {
            int cleared=0;
            for(float lift=2.0f;lift<=24.0f && !cleared;lift+=2.0f) {
                float up[3]={pos[0],pos[1],pos[2]+lift};
                G_TraceCapsule(&t,up,entity->r.mins,entity->r.maxs,up,entity->s.number,mask);
                if(!t.startsolid && !t.allsolid){pos[2]=up[2];cleared=1;}
            }
            static int rideLiftLog;
            if(level.time-rideLiftLog>=1000) {
                rideLiftLog=level.time;
                ((void)0);
            }
        }
    }
    G_SetOrigin(entity,pos);
    ActorAuditPlaced(entity);
    memcpy(entity->r.currentAngles,face,12);
    memcpy(entity->s.lerp.apos.trBase,face,12);
    memcpy(a->sentient->origin,pos,12);
    memset(a->physics.velocity,0,12);
    a->routeCount=a->routeCursor=0;
    SV_LinkEntity(entity);
    
    return 1;
}
/* <actor> codol_actorRide( <entity>, <tag> ): ride the entity at its tag
 * from now on (the tag's place and yaw in the entity's frame are taken once,
 * here). <actor> codol_actorRide(): get off, where it stands. The retained
 * linkTo refuses actors ("does not currently support linkTo"), and a
 * script-side follow loop lags the mover by a frame. Coop matches only: the
 * opening jeep's driver (_cooptraining seat_driver). */
static void ActorRide(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    const char *zone=ServerZoneType();
    if(!ZoneTypeRunsSoldiers(zone)){Scr_Error("codol_actorRide is available in coop and survival matches only");return;}
    if(!Scr_GetNumParam() || Scr_GetType(0)==VAR_UNDEFINED){
        if(a->rideNum) {
            /* Off the seat onto clear floor beside where the seat was: the
             * seat is inside the vehicle's clip, and the next physics move
             * from there would start embedded (and the stuck recovery could
             * push the actor out on any side). */
            float clear[3];
            if(ActorCoopPlace(a->entity,a->entity->r.currentOrigin,clear)) {
                G_SetOrigin(a->entity,clear);
                memcpy(a->sentient->origin,clear,12);
                SV_LinkEntity(a->entity);
                ActorAuditPlaced(a->entity);
            }
            ((void)0);
        }
        memset(a->physics.velocity,0,12);
        a->rideNum=0;return;
    }
    gentity_s *parent=Scr_GetEntity(0);
    if(!parent || parent==a->entity){Scr_ParamError(0,"codol_actorRide needs another entity");return;}
    float matrix[4][3],origin[3],angles[3],forward[3],right[3],up[3],d[3];
    memcpy(matrix[3],parent->r.currentOrigin,12);
    float tagYaw=0;
    /* Third parameter true: ride from where the actor stands now, facing as
     * he faces (IW4 animontag places a rider at his seat clip's start, which
     * is an offset from the seat tag: maps\_coopanim places him there
     * first, with getStartOrigin on the tag). */
    int keep=Scr_GetNumParam()>2 && Scr_GetType(2)!=VAR_UNDEFINED && Scr_GetInt(2);
    int tagged=Scr_GetNumParam()>1 && Scr_GetType(1)!=VAR_UNDEFINED;
    if(tagged && !keep){
        unsigned tag=Scr_GetConstLowercaseString(1);
        if(!G_DObjGetWorldTagMatrix(parent,tag,matrix)){
            Scr_ParamError(1,va("codol_actorRide: tag '%s' not found",SL_ConvertToString(tag)));return;
        }
        tagYaw=atan2f(matrix[0][1],matrix[0][0])*57.295779513f;
    }
    if(keep){
        memcpy(matrix[3],a->entity->r.currentOrigin,12);
        tagYaw=a->entity->r.currentAngles[1];
    }
    memcpy(origin,parent->r.currentOrigin,12);memcpy(angles,parent->r.currentAngles,12);
    AngleVectors(angles,forward,right,up);
    for(int i=0;i<3;++i)d[i]=matrix[3][i]-origin[i];
    a->rideLocal[0]=d[0]*forward[0]+d[1]*forward[1]+d[2]*forward[2];
    a->rideLocal[1]=d[0]*right[0]+d[1]*right[1]+d[2]*right[2];
    a->rideLocal[2]=d[0]*up[0]+d[1]*up[1]+d[2]*up[2];
    a->rideYaw=(tagged || keep) ? fmodf(tagYaw-angles[1]+720.0f,360.0f) : 0;
    a->rideNum=parent->s.number+1;
    a->rideSpawnCount=parent->useCount;
    ((void)0);
    ActorRideFrame(a->entity,a);
}
static int ActorWoundedFrame(gentity_s *entity,actor_lifetime_t *a)
{
    if (a->wounded && entity->health>0) {
        /* Keep the original pain thread/root motion, without combat routing
         * pulling a crawling soldier upright or steering him toward a goal.
         * A real lethal hit always takes the death branch above first. */
        if (a->animFunction==a->scripts.pain && a->animThread &&
            Scr_GetObjectType((unsigned short)a->animThread)!=VAR_DEAD_THREAD) {
            ActorAdvanceGoalAnimation(entity,level.time,0);
            return 1;
        }
        a->wounded=0; /* interrupted/nonlethal completion resumes normal AI */
        a->safeToChangeScript=1;
    }
    return 0;
}

static int ActorRunFrameBody(gentity_s *entity);
/* The frame, then the developer collision audit (codol_actorAudit). */
/* <actor> codol_actorSetWeapon( <weapon> ): the bullet/projectile weapon the actor
 * carries, published in its entity state (C1 reads it for the actor's DObj
 * and for every fire event on the actor).  Coop matches only. */
static void ActorSetWeaponMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    const char *zone=ServerZoneType();
    if(!ZoneTypeRunsSoldiers(zone)){Scr_Error("codol_actorSetWeapon is available in coop and survival matches only");return;}
    const char *why=NULL;
    int weapon=ActorWeaponIndex(Scr_GetString(0),&why);
    if(!weapon){Scr_ParamError(0,va("codol_actorSetWeapon: %s",why?why:"bad weapon"));return;}
    ActorSetWeapon(a->entity,weapon);
}
static void ActorDropWeaponMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    gentity_s *item=ActorDropWeapon(a->entity);
    if(item)Scr_AddEntity(item);else Scr_AddUndefined();
}
static void ActorReloadCompleteMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    Scr_AddInt(ActorAmmoReloadComplete(a->entity));
}
static void ActorClipCapacityMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    Scr_AddInt(ActorAmmoCapacity(a->entity));
}
static void ActorClipRemainingMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    Scr_AddInt(ActorAmmoRemaining(a->entity));
}
/* <actor> codol_actorShoot( <weapon>, <start>, <end> ): one round of the
 * actor's own weapon fire -- native bullet/projectile launch (the actor as attacker
 * and weapon entity, so it passes his own hit boxes and its impacts name
 * him), and EV_FIRE_WEAPON on the actor so C1 plays the weapon's fire sound
 * and its world muzzle flash on the actor's flash tag (IW4 Actor_Shoot,
 * T5 Actor_ShootWeapon).  See actor_fire.c.  Coop matches only. */
static void ActorShootMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    const char *zone=ServerZoneType();
    if(!ZoneTypeRunsSoldiers(zone)){Scr_Error("codol_actorShoot is available in coop and survival matches only");return;}
    if(a->entity->health<=0) return;
    const char *why=NULL;
    int weapon=ActorWeaponIndex(Scr_GetString(0),&why);
    if(!weapon){Scr_ParamError(0,va("codol_actorShoot: %s",why?why:"bad weapon"));return;}
    float start[3],end[3];
    Scr_GetVector(1,start);Scr_GetVector(2,end);
    for(int i=0;i<3;++i)
        if(!isfinite(start[i])||!isfinite(end[i])){Scr_Error("codol_actorShoot requires finite vectors");return;}
    ActorFireWeapon(a->entity,weapon,start,end);
}
/* ---------------------------------------------------------------------------
 * Coop cover (IW4 SP: the actor's claimed node, FindBestCoverNode, the
 * cover animscripts).
 *
 * A coop soldier with an enemy and cover enabled (codol_actorUseCover; the
 * soldier animscripts turn it on) claims a cover node of the level's IW4
 * node set (actor_cover.c), routes onto it, and once there is
 * "attacking": the frame runs his combat animscript, which plays IW4's
 * cover behaviour for the node's type (soldier_cover.gsc).  The node is
 * re-validated twice a second and given up -- avoided for a while -- when
 * the enemy flanks it (out of the node's arc), sees round or over a wall,
 * closes inside 120 units, or leaves the engagement band; the script gives
 * one up with codol_actorLeaveCover.  With no cover to be had the soldier
 * fights in the open as before (ActorMeleeRangeGoal + exposed
 * combat).  Returns -1 no cover, 0 on the way, 1 in cover.
 * ------------------------------------------------------------------------ */
static void ActorEye(gentity_s *entity,float *position);
static void ActorCoverDrop(gentity_s *entity, actor_lifetime_t *a, int avoidMs, const char *why)
{
    if (a->coverIndex < 0) return;
    ((void)0);
    if (avoidMs > 0) {
        unsigned slot = a->coverAvoidNext++ % 4;
        a->coverAvoid[slot] = a->coverIndex;
        a->coverAvoidUntil[slot] = level.time + avoidMs;
    }
    CoverRelease(a->coverIndex, entity->s.number);
    a->coverIndex = -1;
    a->coverArrived = 0;
    if (a->coverEvalTime > level.time + 400) a->coverEvalTime = level.time + 400;
}
static void ActorEnemyEye(gentity_s *enemy, float *eye)
{
    if (enemy->client) {
        memcpy(eye, enemy->r.currentOrigin, 12);
        eye[2] += enemy->client->ps.viewHeightCurrent > 0 ? enemy->client->ps.viewHeightCurrent : 60.0f;
    } else if (ActorForEntity(enemy)) ActorEye(enemy, eye);
    else { memcpy(eye, enemy->r.currentOrigin, 12); eye[2] += 48.0f; }
}
static float ActorDist2D(const float *a, const float *b)
{
    float x = a[0]-b[0], y = a[1]-b[1];
    return sqrtf(x*x + y*y);
}
static int ActorCoverThink(gentity_s *entity, actor_lifetime_t *a, gentity_s *enemy)
{
    if (!a->coverEnabled || a->ignoreAll || !ActorIsCoopSpecies(a)) {
        ActorCoverDrop(entity, a, 0, "cover use turned off");
        return -1;
    }
    float eye[3]; ActorEnemyEye(enemy, eye);
    const float *feet = enemy->r.currentOrigin, *origin = entity->r.currentOrigin;
    float maxDist = a->engageMaxFalloff > 0 ? a->engageMaxFalloff : a->engageMax > 0 ? a->engageMax : a->meleeAttackDistance;
    if (maxDist <= 0) maxDist = 1400.0f;
    float minDist = fmaxf(a->engageMin, 128.0f);
    if (a->coverIndex >= 0 && level.time >= a->coverCheckTime) {
        a->coverCheckTime = level.time + 500;
        const char *why = "";
        float p[3]; CoverPoint(a->coverIndex, p);
        float de = ActorDist2D(p, feet);
        if (CoverClaimant(a->coverIndex) != entity->s.number) ActorCoverDrop(entity, a, 0, "claim lost");
        else if (!CoverStillValid(a->coverIndex, feet, eye, &why)) ActorCoverDrop(entity, a, 8000, why);
        else if (de > maxDist * 1.25f) ActorCoverDrop(entity, a, 4000, "the enemy is beyond the engagement band");
        else if (de < 120.0f) ActorCoverDrop(entity, a, 6000, "the enemy is on top of the node");
    }
    if (a->coverIndex < 0 && level.time >= a->coverEvalTime) {
        a->coverEvalTime = level.time + 1500 + (int)(entity->s.number * 37 % 1000);
        int avoid[4], avoidCount = 0;
        for (int i = 0; i < 4; ++i) if (a->coverAvoidUntil[i] > level.time) avoid[avoidCount++] = a->coverAvoid[i];
        int start = BotNavNearest(origin);
        float search = fminf(1200.0f, fmaxf(600.0f, ActorDist2D(origin, feet)));
        const char *why = "";
        int found = 0;
        unsigned long long findStart = Sys_MicrosecondsLong();
        int cover = CoverFind(entity->s.number, origin, feet, eye, minDist, maxDist, search,
            start >= 0 ? BotNavComponent(start) : -1, avoid, avoidCount, &why, &found);
        s_frameCoverUs += Sys_MicrosecondsLong() - findStart;
        if (cover >= 0 && CoverClaim(cover, entity->s.number)) {
            float p[3]; CoverPoint(cover, p);
            a->coverIndex = cover; a->coverArrived = 0;
            a->coverCheckTime = level.time + 500;
            ((void)0);
        } else if (level.time - a->coverDiagTime >= 5000) {
            a->coverDiagTime = level.time;
            ((void)0);
        }
    }
    if (a->coverIndex < 0) return -1;
    float p[3]; CoverPoint(a->coverIndex, p);
    float d = ActorDist2D(origin, p), dz = fabsf(origin[2] - p[2]);
    if (!a->coverArrived && d <= 16.0f && dz < 48.0f) {
        a->coverArrived = 1; a->coverArrivedTime = level.time;
        ((void)0);
    } else if (a->coverArrived && (d > 96.0f || dz > 64.0f)) a->coverArrived = 0;
    if (!a->hasGoal || ActorDist2D(a->goal, p) > 1.0f || fabsf(a->goal[2] - p[2]) > 1.0f)
        ActorSetGoal(entity, p);
    a->chaseRadius = a->coverArrived ? 128.0f : 8.0f;
    return a->coverArrived;
}

/* ---------------------------------------------------------------------------
 * Coop soldier methods (coop matches only; new builtins, 2026-10-14):
 *   <actor> codol_actorUseCover( <bool> )        cover selection on/off
 *   <actor> codol_actorCoverNode()               the claimed cover pathnode, or undefined
 *   <actor> codol_actorCoverPoint()              that node on the floor (the route's end)
 *   <actor> codol_actorCoverArrived()            true once he stands on it
 *   <actor> codol_actorLeaveCover( <avoid ms> )  give it up (avoided for that long)
 *   <actor> codol_actorGoal()                    the native goal (cover point, chase
 *                                                spot or script goal), or undefined
 *   <actor> codol_actorLookahead()               the route's direction (2D unit
 *                                                vector), undefined when not routing
 *   <actor> codol_actorThrowGrenade( <weapon>, <start>, <velocity>, <fuse s> )
 *                                                IW4 Actor_ThrowGrenade: a live
 *                                                grenade owned by the actor
 * ------------------------------------------------------------------------ */
static actor_lifetime_t *ActorCoopReceiver(scr_entref_t ref, const char *name)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return NULL;
    const char *zone=ServerZoneType();
    if(!ZoneTypeRunsSoldiers(zone)){Scr_Error(va("%s is available in coop and survival matches only",name));return NULL;}
    return a;
}
static void ActorUseCoverMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorCoopReceiver(ref,"codol_actorUseCover");if(!a)return;
    a->coverEnabled=(unsigned char)(Scr_GetInt(0)!=0);
    if(!a->coverEnabled)ActorCoverDrop(a->entity,a,0,"cover use turned off by script");
}
static void ActorCoverNodeMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorCoopReceiver(ref,"codol_actorCoverNode");if(!a)return;
    int record=a->coverIndex>=0?CoverRecord(a->coverIndex):-1;
    if(record>=0)Scr_AddEntityNum(record,2);
}
static void ActorCoverPointMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorCoopReceiver(ref,"codol_actorCoverPoint");if(!a)return;
    float p[3];
    if(a->coverIndex>=0 && CoverPoint(a->coverIndex,p))Scr_AddVector(p);
}
static void ActorCoverArrivedMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorCoopReceiver(ref,"codol_actorCoverArrived");if(!a)return;
    Scr_AddBool(a->coverIndex>=0 && a->coverArrived);
}
static void ActorLeaveCoverMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorCoopReceiver(ref,"codol_actorLeaveCover");if(!a)return;
    int avoid=Scr_GetNumParam()>0?Scr_GetInt(0):0;
    ActorCoverDrop(a->entity,a,avoid>0?avoid:0,"the animscript gave it up");
}
static void ActorLookaheadMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorCoopReceiver(ref,"codol_actorLookahead");if(!a)return;
    float d=sqrtf(a->routeDir[0]*a->routeDir[0]+a->routeDir[1]*a->routeDir[1]);
    if(a->prevRoute!=1 || d<0.001f)return;
    float v[3]={a->routeDir[0]/d,a->routeDir[1]/d,0};
    Scr_AddVector(v);
}
static void ActorGoalMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorCoopReceiver(ref,"codol_actorGoal");if(!a)return;
    if(a->hasGoal)Scr_AddVector(a->goal);
}
extern "C" gentity_s *G_FireGrenade(gentity_s *parent,float *start,float *dir,int weapon,
    unsigned char weaponModel,int rotate,int fuseTime);
static void ActorThrowGrenadeMethod(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorCoopReceiver(ref,"codol_actorThrowGrenade");if(!a)return;
    if(a->entity->health<=0)return;
    const char *name=Scr_GetString(0);
    int weapon=G_GetWeaponIndexForName(name);
    WeaponDef *def=weapon>0?BG_GetWeaponDef(weapon):NULL;
    if(!def){Scr_ParamError(0,va("codol_actorThrowGrenade: unknown weapon '%s'",name));return;}
    if(def->weapType!=WEAPTYPE_GRENADE){Scr_ParamError(0,va("codol_actorThrowGrenade: '%s' is not a grenade",name));return;}
    float start[3],velocity[3];
    Scr_GetVector(1,start);Scr_GetVector(2,velocity);
    float fuse=Scr_GetNumParam()>3?Scr_GetFloat(3):3.0f;
    for(int i=0;i<3;++i)
        if(!isfinite(start[i])||!isfinite(velocity[i])){Scr_Error("codol_actorThrowGrenade requires finite vectors");return;}
    if(!isfinite(fuse)||fuse<0.25f)fuse=0.25f;
    if(fuse>10.0f)fuse=10.0f;
    gentity_s *grenade=G_FireGrenade(a->entity,start,velocity,weapon,0,1,(int)(fuse*1000.0f));
    if(grenade)Scr_AddEntity(grenade);
}

int ActorRunFrame(gentity_s *entity)
{
    static cvar_t *debugTrace;
    unsigned long long start = Sys_MicrosecondsLong();
    s_framePlanUs = s_frameCoverUs = s_frameScriptUs = s_frameGoalUs = s_frameSightUs = 0;
    s_framePushUs = s_frameAnimUs = 0;
    int result = ActorRunFrameBody(entity);
    unsigned long long auditStart = Sys_MicrosecondsLong();
    ActorAuditFrame(entity);
    unsigned long long end = Sys_MicrosecondsLong(), took = end - start;
    {
        unsigned long long parts[FPA_COUNT] = {took, s_framePlanUs, s_frameGoalUs, s_frameSightUs,
            s_frameScriptUs, s_frameCoverUs, s_framePushUs, s_frameAnimUs, end - auditStart};
        FrameProfileActor(parts);
    }
    if (took > 4000) {
        if (!debugTrace) debugTrace = Cvar_FindVar("debugTrace");
        if (debugTrace && debugTrace->integer)
            ((void)0);
    }
    return result;
}
/* T5 zombie_dog_move.gsc plays the trot and the run at self.moveplaybackrate
 * (1), not the per-hound animplaybackrate (0.9-1.1) that its idles use.
 * MEASURED (t5cap, zombie_cod5_sumpf): every T5 hound trots at 135-136 u/s;
 * CoDOL's spread 122-149 with the jittered rate.
 *
 * The other T5 hound scripts pass rate 1.0 too (zombie_dog_combat run_attack
 * and combatIdle, zombie_dog_death, zombie_dog_pain, zombie_dog_turn and the
 * traverse\zombie_shared dog hops); only zombie_dog_stop's idle and attack
 * idles play at self.animplaybackrate. MEASURED (t5cap dog clip timeline,
 * 2026-10-22): every T5 hound's run_attack, death_hit_* and turn clips run at
 * 1.00 (hounds of animplaybackrate 0.91-1.08); CoDOL's ran at 0.88-1.09. */
static int ActorIsMpDog(const actor_lifetime_t *a);
/* The MP dog plays every state at rate 1: T5 MP drives the dog's states
 * through bg_dog_animations_mp.cpp, every one at s_animRate 1.0, and its
 * dog_init animplaybackrate (0.9-1.1) feeds none of them.  MEASURED (T5 MP,
 * Plutonium t5mp, mp_nuked): every T5 dog runs 318-319 u/s whatever its
 * animplaybackrate (0.92-1.09); this server's ran 297-351 with it. */
static float ActorStateRate(const actor_lifetime_t *a, int state)
{
    if (ActorIsZmSpecies(a) && a->species->quadruped) {
        const char *name = a->species->stateName ? a->species->stateName((unsigned)state) : 0;
        if (!name || strncmp(name, "stop_", 5)) return 1.0f;
    }
    if (ActorIsMpDog(a)) return 1.0f;
    return a->animationRate;
}

static int ActorRunFrameBody(gentity_s *entity)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->initialized) return 0;
    if (a->rideNum && entity->health > 0 && ActorRideFrame(entity,a)) return 1;
    unsigned generation = a->generation;
    if (entity->health <= 0) {
        if(a->animFunction==a->scripts.death && a->animThread &&
           Scr_GetObjectType((unsigned short)a->animThread)==VAR_DEAD_THREAD) {
            ActorBecomeCorpse(entity);return 1;
        }
        /* T5 Actor_Death_Start stamps iStateTime; Actor_Death_Think applies
         * deathContents from 500 ms on; here they go on at once (below).
         *
         * The dying body's own moves stop colliding with bodies at the same
         * start. MEASURED 2026-10-13 (zm_prototype_sh probe, corpseprobe
         * <ent> walk <client>): from 500 ms players walk into the dying body
         * (CORPSE|CLIPSHOT is in no player clipmask), but the body's death
         * frame still swept its live clipmask 0x2820015, which holds
         * CONTENTS_BODY, so its ground probe started solid inside the
         * player's hull and the embedded-start recovery (clearSpot,
         * actor_physics.cpp) stepped it up to 16 units clear of the
         * player every frame: a test client walking through a dying zombie
         * bulldozed it 98-100 units along its path (twice; the body stayed
         * put with this change, also twice). T5 resolves no such overlap
         * (its corpse clipmask, actor_corpse.cpp:686, is 0x820011: no
         * bodies either). Without CONTENTS_BODY the body is neither stopped
         * by nor pushed out of a player or another actor; the world, clips
         * and monster clip still hold it. */
        /* The death contents (CORPSE|CLIPSHOT: no player, actor or player
         * clip mask holds either, bullets still hit through CLIPSHOT) go on
         * the first dying frame, the user's call (2026-10-21: "collision
         * off the second they die"); T5 Actor_Death_Think waits 500 ms. */
        if (a->deathStateTime < 0) {
            a->deathStateTime = level.time;
            entity->clipmask &= ~CONTENTS_BODY;
        }
        if (entity->r.contents != a->deathContents) {
            ((void)0);
            entity->r.contents = a->deathContents;
            SV_LinkEntity(entity);
        }
        a->scripted = 0; a->scriptedAlign = 0;
        a->safeToChangeScript=1;
        if (a->coverIndex >= 0) { CoverRelease(a->coverIndex, entity->s.number); a->coverIndex = -1; a->coverArrived = 0; }
        ActorSwitchScript(entity, a->scripts.death, 0, 1);
        a=ActorForEntity(entity);
        if(a && a->generation==generation)ActorAdvanceGoalAnimation(entity,level.time,0);
        return 1;
    }
    if (ActorWoundedFrame(entity,a))return 1;
    { unsigned long long t0 = Sys_MicrosecondsLong();
      if(!a->ignoreAll){ if(ActorIsMpDog(a))ActorUpdateMpDogThreat(entity); else ActorUpdateVisibleEnemy(entity); }
      s_frameSightUs += Sys_MicrosecondsLong() - t0; }
    a=ActorForEntity(entity);
    if(!a || a->generation!=generation)return 1;
    gentity_s *enemy = ActorEnemy(entity);
    
    /* T5 Actor_Negotiation_Think: while the traversal animscript runs, the
     * actor is its root motion and nothing else -- no chase goal, no route,
     * no stuck detector, no script re-selection.  The thread ending (or the
     * safety timeout) pops the state. */
    a->lastRoute = 0;
    if (a->negotiating) { ActorNegotiationFrame(entity); return 1; }
    /* T5 AIS_SCRIPTEDANIM: the same hold while a scripted clip plays. */
    if (a->scripted) { ActorScriptedFrame(entity); return 1; }
    int attacking = enemy && a->animFunction == a->scripts.combat && !a->safeToChangeScript;
    int probeGoalSet = -1, probeInRange = -1;
    /* Coop cover first: a soldier holding (or heading for) a cover node is
     * "attacking" from it and routes onto it instead of chasing. */
    int cover = -1;
    if (enemy && ActorIsCoopSpecies(a)) cover = ActorCoverThink(entity, a, enemy);
    else if (a->coverIndex >= 0) ActorCoverDrop(entity, a, 0, "no enemy");
    if (cover >= 0) {
        if (!attacking) attacking = cover;
    } else if (enemy && !attacking) {
        dog_attack_goal_t attack;
        // Original aitype/GSC owns attack distance; zero is not replaced by a
        // guessed reach. Synced player melee requires a separate native owner.
        // The four attack spots are the dog's alone (T5 actor_dog_exposed.cpp);
        // every other species chases the enemy and swings in melee range.
        int dog = ActorIsDog(a);
        int zmHound = dog && ActorIsZmSpecies(a);
        if (zmHound) ZmHoundCombatGoal(entity, enemy, a->meleeAttackDistance, &attack);
        else if (dog) DogCombatGoal(entity, enemy, a->meleeAttackDistance, 0, &attack);
        else ActorMeleeRangeGoal(entity, enemy, a->meleeAttackDistance, &attack);
        attacking = attack.inAttackRange;
        probeGoalSet = attack.goalSet; probeInRange = attack.inAttackRange;
        const float *goal = attack.goalSet ? attack.goal : enemy->r.currentOrigin;
        float dx = goal[0]-a->goal[0], dy = goal[1]-a->goal[1];
        // T5 invalidates a chase path only once the destination moved >15 units.
        if (!a->hasGoal || dx*dx+dy*dy>225.0f || fabsf(goal[2]-a->goal[2])>15.0f)
            ActorSetGoal(entity, goal);
        /* T5 Actor_UpdateMeleeGoalPos gives the attack spot its own code-goal
         * radius; with the script's 128-unit goalradius the dog reached "goal"
         * ~100 units short of bite range and thrashed move_start/stop_attackidle
         * every frame the enemy moved. Reach the spot itself (bite range).
         * The spot is meleeAttackDist from the enemy and T5 attacks inside
         * meleeAttackDist + 15 (Actor_Dog_GetEnemyPos), so the arrival
         * radius must be under 15: at 24 a dog walking straight in stopped
         * 121-126 units out and barked there for good (MEASURED 2026-10-02,
         * mp_rust, frozen target: dog2goal 23, d2d 121, inRange 0). */
        // A zombie closes to T5 find_flesh's goalradius 32 (_zombiemode_spawner.gsc
        // :3943); it swings as soon as ActorMeleeRangeGoal says it can.
        /* A zm hound runs in to the player (ZmHoundCombatGoal). */
        a->chaseRadius = attack.goalSet ? 12.0f : zmHound ? 24.0f : dog ? fmaxf(24.0f, a->meleeAttackDistance) : 32.0f;
    }
    float direction[3] = {0,0,0};
    unsigned long long goalStart = Sys_MicrosecondsLong();
    int route = ActorGoalDirection(entity, direction);
    s_frameGoalUs += Sys_MicrosecondsLong() - goalStart;
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) return 1;
    a->lastRoute = route; a->prevRoute = route;
    if (a->negotiationPending) {
        a->negotiationPending = 0;
        if (ActorStartNegotiation(entity)) return 1;
        a = ActorForEntity(entity);
        if (!a || a->generation != generation) return 1;
    }
    /* A moving dog whose path runs into a low wall hops it (see
     * ActorStartWallHop).  Not while biting, flashed, in pain or turning. */
    if (route == 1 && ActorIsDog(a) && !attacking && !a->flashBanged && !a->painPending &&
        (a->animFunction == a->scripts.move || a->animFunction == a->scripts.turn || !a->animFunction) &&
        ActorStartWallHop(entity, direction))
        return 1;
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) return 1;
    /* T5 Path_UpdateLeanAmount/Path_UpdateMomentum: lean is the dot of the
     * wish direction with last frame's, negated when turning right; the run
     * knob's lean_L/lean_R weights come from it (BG_Dog_GetRunAnimWeights). */
    if (route == 1 && (a->prevMoveDir[0] != 0 || a->prevMoveDir[1] != 0)) {
        float lean = direction[0]*a->prevMoveDir[0] + direction[1]*a->prevMoveDir[1];
        if (lean > 0 && direction[0]*a->prevMoveDir[1] - direction[1]*a->prevMoveDir[0] < 0) lean = -lean;
        a->lean = fmaxf(-1.0f, fminf(1.0f, lean));
    } else a->lean = 0;
    if (route == 1) { a->prevMoveDir[0] = direction[0]; a->prevMoveDir[1] = direction[1]; }
    else a->prevMoveDir[0] = a->prevMoveDir[1] = 0;
    /* Stuck detector: a dog with somewhere to go that has not moved 8 units in
     * a second is wedged (corner, other dogs, unreachable spot). Drop the route
     * so it re-plans from the nearest node, release its attack-spot claim so
     * the next pick is a different spot, and on a repeat tell the script
     * (bad_path) so a patrol picks another node. */
    if (route == 1) {
        float sx = entity->r.currentOrigin[0]-a->stuckOrigin[0], sy = entity->r.currentOrigin[1]-a->stuckOrigin[1];
        if (sx*sx+sy*sy > 64.0f || !a->stuckTime) {
            memcpy(a->stuckOrigin, entity->r.currentOrigin, 12); a->stuckTime = level.time; a->stuckCount = 0;
            a->stuckAttempt = 0; a->stuckRejected = a->stuckBlocked = 0;
        } else if (level.time - a->stuckTime >= 1000 && a->stuckAttempt < 8.0f) {
            /* MEASURED 2026-09-21 (docs/ZOMBIE_WINDOW_20260921.md): every
             * yard "stuck" of ent 576 on zm_prototype_sh sat on clear, flat
             * floor with rm (0 0 0) and ai_zombie_walk_v7 at t 0.37-0.49, and
             * the clip's own delta track is exactly zero from t 0.353 to
             * 0.579 -- an authored one-second pause.  That is not a wedge;
             * repathing and snapping it to a node only teleported it
             * backwards every second.  Wait the stall out.  Part 2: walk_v3
             * (t 0.58-0.75) and walk_v9 (t 0.33-0.48, 0.69-0.84) pause the
             * same way; the delta tracks were read correctly. */
            if (!a->stallDiagPrinted) {
                a->stallDiagPrinted = 1;
                char animState[160] = "";
                ((void)0);
                ((void)0);
            }
            a->stuckTime = level.time; a->stuckAttempt = 0; a->stuckRejected = a->stuckBlocked = 0;
        } else if (level.time - a->stuckTime >= 1000) {
            /* MEASURED 2026-09-21 part 2: ent 576 at (-1327 -844) was walking
             * walk_v3's slow section (6-8 u/s at rate 0.55) with phys ok 60
             * rej 0 -- the old "asked >= 8, moved < 8" verdict called that a
             * wedge, repathed, snapped it to node 379 (the rubberband) and
             * sent bad_path, which burned an entrance.  A wedge is motion
             * the PHYSICS refused or throttled; a slow cycle is neither. */
            int wedged = a->stuckRejected > 0 || a->stuckBlocked >= 15;
            if (!wedged) {
                if (!a->slowDiagPrinted) {
                    a->slowDiagPrinted = 1;
                    char animState[160] = "";
                    ((void)0);
                    ((void)0);
                }
                a->stuckTime = level.time; a->stuckAttempt = 0; a->stuckRejected = a->stuckBlocked = 0;
            } else {
                ++a->stuckCount;
                ((void)0);
                ((void)0);
                /* The link the actor was following when the physics refused
                 * it cannot be walked by this actor (a player-only vault or
                 * duck, a ledge): replanning over it again only wedges it on
                 * the same wall every second.  Ban it for this actor. */
                if (a->routeCursor >= a->routeCount) {
                    a->shortcutBanTime = level.time + 5000;
                    a->finalLeg = 0;
                    ((void)0);
                }
                if (a->routeCursor < a->routeCount) {
                    int next = a->route[a->routeCursor];
                    int prev = a->routeCursor > 0 ? a->route[a->routeCursor-1] : BotNavNearest(entity->r.currentOrigin);
                    /* Wedged walking from off the graph to the route's own
                     * start node: no link to ban, so the start pick avoids
                     * that node for this actor; the next plan starts from a
                     * node it can step to. */
                    if (a->routeCursor == 0 && !ActorStartBanned(a, next)) {
                        a->bannedStart[a->bannedStartCount++ % 4] = (unsigned short)next;
                        ((void)0);
                    }
                    if (prev >= 0 && prev != next && !ActorLinkBanned(a, prev, next)) {
                        unsigned slot = a->bannedLinkCount++ % 8;
                        a->bannedLinkFrom[slot] = (unsigned short)prev;
                        a->bannedLinkTo[slot] = (unsigned short)next;
                        ((void)0);
                    }
                }
                a->routeCount = a->routeCursor = 0;
                DogCombatRelease(entity);
                a->stuckTime = level.time; a->stuckAttempt = 0; a->stuckRejected = a->stuckBlocked = 0;
                if (a->stuckCount >= 2) {
                    a->stuckCount = 0;
                    /* Second second in the same spot.  T5 has no teleport here
                     * and neither does this: the physics lifts a capsule that
                     * settled into the floor (actor_physics.cpp), so what
                     * is left is a wall or a pile -- step sideways out of it.
                     * Only an actor that is startsolid where it stands, which
                     * cannot move at all, is placed on the nearest clear node,
                     * and only within 64 units (the old 256 was the "slide
                     * across the yard" the user saw).  No bad_path from here:
                     * that notify is the route planner's (T5 Path_FindPath),
                     * and firing it from a wedge cost one entrance per second. */
                    trace_t probe;
                    ActorsPassable(entity, 1);
                    G_TraceCapsule(&probe, entity->r.currentOrigin, entity->r.mins, entity->r.maxs, entity->r.currentOrigin, entity->s.number, entity->clipmask);
                    ActorsPassable(entity, 0);
                    if (probe.startsolid || probe.allsolid) {
                        int candidates[2] = {-1, BotNavNearest(entity->r.currentOrigin)};
                        if (a->routeCursor < a->routeCount) candidates[0] = a->route[a->routeCursor];
                        int snapped = 0;
                        for (int c = 0; c < 2 && !snapped; ++c) {
                            const float *point = candidates[c] >= 0 ? BotNavPoint(candidates[c]) : NULL;
                            if (!point) continue;
                            float dx = point[0]-entity->r.currentOrigin[0], dy = point[1]-entity->r.currentOrigin[1];
                            if (dx*dx+dy*dy >= 64.0f*64.0f) continue;
                            static const float lifts[3] = {1.0f, 20.0f, 44.0f};
                            for (int l = 0; l < 3 && !snapped; ++l) {
                                float origin[3] = {point[0], point[1], point[2]+lifts[l]};
                                ActorsPassable(entity, 1);
                                G_TraceCapsule(&probe, origin, entity->r.mins, entity->r.maxs, origin, entity->s.number, entity->clipmask);
                                ActorsPassable(entity, 0);
                                if (probe.startsolid || probe.allsolid) continue;
                                ((void)0);
                                G_SetOrigin(entity, origin);
                                memcpy(a->sentient->origin, origin, 12);
                                memset(a->physics.velocity, 0, 12);
                                SV_LinkEntity(entity);
                                snapped = 1;
                            }
                        }
                        if (!snapped && ActorIsDog(a) && ActorRecoverFromSolid(entity, a)) snapped = 1;
                        if (!snapped) ((void)0);
                    } else {
                        float nudge[3] = {-direction[1]*12.0f, direction[0]*12.0f, 0};
                        if ((level.time/1000)&1) { nudge[0] = -nudge[0]; nudge[1] = -nudge[1]; }
                        ActorMoveDeltaPastActors(entity, nudge);
                    }
                }
            }
        }
    } else { a->stuckTime = 0; a->stuckCount = 0; }
    /* Radial push-out between overlapping actors (capsule radius 15): the
     * separation steering above avoids most piles, this resolves the rest.
     * It moves with the other actors passable (ActorMoveDeltaPastActors);
     * the plain move always started solid inside the actor it was leaving.
     * Contact closer than 28 is pushed, not 30: the three zm window attack
     * spots are 28 apart (_zombiemode_blockers.gsc blocker_attack_spots), and
     * zombies walking onto neighbouring spots must not be shoved off them.
     * An actor walking a route is not pushed back along it by one that is
     * standing still, only aside: T5 moves the actor that is standing in the
     * way (Actor_MoveAwayNoWorse hands it pCloseEnt and it walks clear), not
     * the one on the move.  A full radial push held a walking zombie 26
     * units behind an idle one for 38 s on zm_prototype_sh (walk forward ~1
     * unit a frame, pushed back 1); the standing actor is pushed clear
     * instead and the walker passes.  Two walkers both take the full push. */
    unsigned long long pushStart = Sys_MicrosecondsLong();
    for (unsigned i = 0; i < MAX_ACTOR_SLOTS; ++i) {
        actor_lifetime_t *o = &actors[i];
        if (!o->allocated || !o->initialized || o->entity == entity || !o->entity || !o->entity->r.inuse ||
            o->entity->health <= 0) continue;
        float ox = entity->r.currentOrigin[0] - o->entity->r.currentOrigin[0];
        float oy = entity->r.currentOrigin[1] - o->entity->r.currentOrigin[1];
        float oz = entity->r.currentOrigin[2] - o->entity->r.currentOrigin[2];
        float d = sqrtf(ox*ox + oy*oy);
        /* Zombies keep T5's spacing: MEASURED (t5cap, Prototype and Asylum
         * crowds around a frozen player) T5 zombies stand 28-40 apart, median
         * 32-33; CoDOL's packed at 24-28, so fewer stood out at 48-56 swinging
         * from the second row.  28 is kept next to an actor on a scripted clip
         * (a window attack spot; the spots are 28 apart), next to a zombie
         * still working its window (ignoreall until zombie_think's hunt), and
         * for every other species.  MEASURED: with 32 everywhere, zombies
         * between two boards were pushed off neighbouring spots and the gap
         * to the next board grew from 1.5 s to 1.7 s (p90 1.9 -> 3.0 s). */
        float spacing = ActorIsZmSpecies(a) && ActorIsZmSpecies(o) && !o->scripted && !a->scripted &&
            !a->ignoreAll && !o->ignoreAll ? 32.0f : 28.0f;
        /* Two zm hounds keep T5's hound spacing: MEASURED (t5cap, six hounds
         * queued at a frozen player on zombie_cod5_sumpf) neighbouring T5
         * hounds stood 37-41 apart, so the queue ran 31, 71, 101, 137 units
         * out; at 32 these stood 30-35 apart, the second hound at 60-62. */
        if (spacing == 32.0f && a->species && o->species && a->species->quadruped && o->species->quadruped)
            spacing = 40.0f;
        if (fabsf(oz) > 48.0f || d >= spacing) continue;
        if (d < 1.0f) { ox = (float)((entity->s.number & 1) ? 1 : -1); oy = 0; d = 1.0f; }
        float push = fminf(4.0f, (spacing - d) * 0.5f);
        float delta[3] = {ox / d * push, oy / d * push, 0};
        if (route == 1 && o->lastRoute != 1) {
            float back = delta[0]*a->routeDir[0] + delta[1]*a->routeDir[1];
            if (back < 0) { delta[0] -= back*a->routeDir[0]; delta[1] -= back*a->routeDir[1]; }
        }
        int rc = a->routeCount, rcur = a->routeCursor; /* a blocked push is not a route failure */
        ActorMoveDeltaPastActors(entity, delta);
        a = ActorForEntity(entity);
        if (!a || a->generation != generation) return 1;
        a->routeCount = rc; a->routeCursor = rcur;
    }
    s_framePushUs += Sys_MicrosecondsLong() - pushStart;
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) return 1;
    memcpy(a->lookAheadDir,direction,sizeof(a->lookAheadDir));
    // T5 Actor_PathEndActions: the original move script owns stop blending.
    // Only a complete remaining route may emit stop_soon; melee approach does not.
    if (route == 1 && a->stopAnimDistSq > 0) {
        float dx=entity->r.currentOrigin[0]-a->goal[0];
        float dy=entity->r.currentOrigin[1]-a->goal[1];
        float distanceSq=dx*dx+dy*dy;
        const char *event=NULL;
        if(a->pathEndAnimNotified && distanceSq>a->stopAnimDistSq) {
            a->pathEndAnimNotified=0;event="run";
        } else if(!a->pathEndAnimNotified && distanceSq<a->stopAnimDistSq &&
                  a->routeCursor>=a->routeCount && !enemy) {
            a->pathEndAnimNotified=1;event="stop_soon";Scr_AddFloat(sqrtf(distanceSq));
        }
        if(event) {
            unsigned name=SL_GetString(event,0);
            Scr_Notify(entity,(unsigned short)name,event[0]=='s'?1:0);
            SL_RemoveRefToString(name);
            a=ActorForEntity(entity);
            if(!a || a->generation!=generation)return 1;
        }
    }
    /* Pain is a script-owned state, not a one-frame pulse. Let the original
     * DoNoteTracksForTime finish; death above can still interrupt immediately.
     * T5 Actor_Dog_Think: a flashbanged dog that is not in a synced melee (MP
     * dogs never start one: dog_cant_kill_in_one_hit is always true) runs
     * the flashed animscript (Actor_Exposed_FlashBanged: move stop, orient
     * don't change) instead of attacking or moving, until the script that
     * clears the flag ends. */
    int painRunning = a->animFunction==a->scripts.pain && a->animThread &&
        Scr_GetObjectType((unsigned short)a->animThread)!=VAR_DEAD_THREAD;
    int flashRunning = a->animFunction==a->scripts.flashed && a->animThread &&
        Scr_GetObjectType((unsigned short)a->animThread)!=VAR_DEAD_THREAD;
    int flashed = (a->flashBanged || flashRunning) && !a->painPending && !painRunning;
    /* T5 Actor_Dog_ShouldTurn (actor_dog_exposed.cpp): a moving dog whose
     * path direction is more than dog_turn90_angle off its facing, with at
     * least dog_turn_min_goal_dist of path left, runs the species turn
     * animscript (dog_turn: 90/180 turn states, which C1 maps to its
     * run_start_L/R and run_start_180_L/R clips); a running turn is not
     * interrupted until the script says it is safe. */
    int turnRunning = a->scripts.turn && a->animFunction==a->scripts.turn && a->animThread &&
        Scr_GetObjectType((unsigned short)a->animThread)!=VAR_DEAD_THREAD && !a->safeToChangeScript;
    int turning = 0;
    if (ActorIsDog(a) && a->scripts.turn && route == 1 && !attacking && !flashed &&
        !a->painPending && !painRunning) {
        if (turnRunning) turning = 1;
        else if (a->animFunction == a->scripts.move && (direction[0] != 0 || direction[1] != 0)) {
            /* _dogs.gsc registers these from script (setdvar: string
             * dvars), so read them parsed; T5 defaults when unset. */
            float threshold = Cvar_FindVar("dog_turn90_angle") ? Cvar_VariableValue("dog_turn90_angle") : 70.0f;
            float minGoal = Cvar_FindVar("dog_turn_min_goal_dist") ? Cvar_VariableValue("dog_turn_min_goal_dist") : 40.0f;
            float gx = a->goal[0]-entity->r.currentOrigin[0], gy = a->goal[1]-entity->r.currentOrigin[1];
            /* T5 Actor_Dog_GetDeltaTurnYaw reads Path.lookaheadDir: the
             * route, not the separation-steered walk direction. */
            float yaw = (a->routeDir[0] != 0 || a->routeDir[1] != 0) ?
                atan2f(a->routeDir[1], a->routeDir[0]) * 57.295779513f :
                atan2f(direction[1], direction[0]) * 57.295779513f;
            float delta = fmodf(yaw - entity->r.currentAngles[1], 360.0f);
            if (delta > 180) delta -= 360; if (delta < -180) delta += 360;
            if ((a->routeCursor < a->routeCount || gx*gx+gy*gy >= minGoal*minGoal) && fabsf(delta) > threshold)
                turning = 1;
        }
    }
    if (ActorContactProbeActive()) {
        actor_contact_sample_t sample;
        memset(&sample, 0, sizeof(sample));
        sample.species = a->species ? a->species->name : NULL;
        sample.route = route; sample.attacking = attacking; sample.turning = turning;
        sample.hasGoal = a->hasGoal; sample.enemyNum = enemy ? enemy->s.number : -1;
        sample.goalSet = probeGoalSet; sample.inRange = probeInRange;
        memcpy(sample.goal, a->goal, 12); memcpy(sample.dir, direction, 12);
        sample.rawDir[0] = a->routeDir[0]; sample.rawDir[1] = a->routeDir[1];
        sample.steer = a->diagSteer; sample.meleeDistance = a->meleeAttackDistance;
        sample.physAccepted = a->probeAccepted; sample.physRejected = a->probeRejected;
        sample.physBlocked = a->probeBlocked; sample.deltaFails = a->probeDeltaFails;
        sample.routeMode = a->usingRoute;
        a->probeAccepted = a->probeRejected = a->probeBlocked = a->probeDeltaFails = 0;
        ActorContactProbeSample(entity, &sample);
    }
    float facing[3] = {direction[0],direction[1],0};
    if (flashed) {
        facing[0]=facing[1]=0;
    } else if (a->scriptOrient == 1) {
        facing[0]=cosf(a->scriptYaw*.01745329252f);
        facing[1]=sinf(a->scriptYaw*.01745329252f);
    } else if ((a->scriptOrient == 2 || a->scriptOrient == 3 ||
                (!a->scriptOrient && attacking)) && enemy) {
        facing[0]=enemy->r.currentOrigin[0]-entity->r.currentOrigin[0];
        facing[1]=enemy->r.currentOrigin[1]-entity->r.currentOrigin[1];
    } else if (a->scriptOrient == 2) {
        facing[0]=facing[1]=0;
    } else if (a->scriptOrient == 4 && a->hasGoal) {
        facing[0]=a->goal[0]-entity->r.currentOrigin[0];
        facing[1]=a->goal[1]-entity->r.currentOrigin[1];
    } else if (a->scriptOrient == 6) {
        /* "face current" on a coop actor: the yaw is left to the clip's
         * own rotation (arrivals, exits, turns), never pulled back. */
        facing[0]=facing[1]=0;
    }
    ActorOrientToDirection(entity,facing,(level.time-level.previousTime)*.001f);
    // Original species GSC remains the animation-state producer. Native path
    // progress chooses movement versus stop; no timer-driven dog imitation.
    /* T5 Actor_Dog_Exposed_Think picks move or stop from the path it had
     * when the think began; the path that ends during the think notifies
     * "goal", and the script answers (maps\mp\_dogs::dog_patrol sets the
     * next patrol node) before the next think, so a T5 patrol dog runs
     * straight through its nodes.  MEASURED (T5 MP, Plutonium t5mp,
     * mp_nuked, a dog patrolling for a minute): no stop script and no
     * move_start at any patrol node; here every node cost a frame of the
     * stop script and a fresh move_start (the run-start clip).  So the
     * frame an MP dog arrives keeps its move script. */
    int arrived = ActorIsMpDog(a) && route != 1 && a->mpRouteLast == 1 &&
        a->animFunction == a->scripts.move && !attacking && !turning;
    if (ActorIsMpDog(a)) a->mpRouteLast = (unsigned char)(route == 1);
    int function = (a->painPending || painRunning) ? a->scripts.pain : flashed ? a->scripts.flashed :
        attacking ? a->scripts.combat : turning ? a->scripts.turn :
        (route == 1 || arrived ? a->scripts.move : a->scripts.stop);
    unsigned long long switchStart = Sys_MicrosecondsLong();
    int switched = ActorSwitchScript(entity, function, !flashed && !attacking && route == 1 ? 3 : 0, 1);
    s_frameScriptUs += Sys_MicrosecondsLong() - switchStart;
    if (!switched) return 1;
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) return 1;
    if(a->animFunction==a->scripts.pain)a->painPending=0;
    unsigned long long animStart = Sys_MicrosecondsLong();
    if(a->lastAnimationTime<level.time && a->species && a->species->stateApply) {
        DObj *obj=Com_GetServerDObj(entity->s.number);
        if(!a->species->stateApply(obj,entity->s.number,a->generation,a->serverTree,level.time,
            a->animationState,a->animationState,0,ActorStateRate(a,a->animationState),a->lean,a->aimUp,a->aimLeft)) return 1;
        a->lastAnimationTime=level.time;
    }
    ActorAdvanceGoalAnimation(entity, level.time, 0);
    s_frameAnimUs += Sys_MicrosecondsLong() - animStart;
    return 1;
}

extern "C" int PathNodeOrigin(unsigned index, float *origin);
static void ActorSetGoalNode(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum >= ENTITYNUM_WORLD ||
        !ActorForEntity(&g_entities[ref.entnum])) {
        Scr_ObjectError("setgoalnode requires a live native actor");
        return;
    }
    scr_entref_t node = Scr_GetEntityRefValue(0);
    float point[3];
    if (node.classnum != 2 || !PathNodeOrigin(node.entnum, point)) {
        Scr_ParamError(0, "setgoalnode requires a current path node with an origin");
        return;
    }
    /* T5 drops every path node to the floor at load (OpenBLOPS pathnode.cpp
     * node_droptofloor: a 15-radius, 30-high capsule from 1 above the
     * authored origin to 256 below, mask 0x820011), so a goal node is a point
     * on the ground.  The class-2 record keeps the authored origin; an actor
     * sent to it chased a point in the air it could never reach within
     * goalheight (reported 2026-10-02, mp_rust: exit node (512 256 -155), 85
     * above the floor, held leaving dogs until dog_leave_failsafe deleted
     * them).  A node in solid or floating keeps its authored origin. */
    {
        static const float dropMins[3] = {-15.0f, -15.0f, 0.0f}, dropMaxs[3] = {15.0f, 15.0f, 30.0f};
        float start[3] = {point[0], point[1], point[2] + 1.0f}, end[3] = {point[0], point[1], point[2] - 256.0f};
        trace_t drop;
        G_TraceCapsule(&drop, start, dropMins, dropMaxs, end, ENTITYNUM_NONE, 0x820011);
        if (!drop.startsolid && !drop.allsolid && drop.fraction < 1.0f)
            point[2] = start[2] + (end[2] - start[2]) * drop.fraction;
    }
    ActorSetGoal(&g_entities[ref.entnum], point);
    ActorForEntity(&g_entities[ref.entnum])->goalNode = node.entnum;
}
static actor_lifetime_t *ActorMethodReceiver(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum>=ENTITYNUM_WORLD) {
        Scr_ObjectError("method requires a native actor"); return NULL;
    }
    actor_lifetime_t *a=ActorForEntity(&g_entities[ref.entnum]);
    if (!a) Scr_ObjectError("method requires a live native actor");
    return a;
}
static void ActorSetAnimationState(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);
    if (!a) return;
    /* The animation-state machine is a species column. A species whose
     * backend is NULL has no native clip table (T5 animates it from GSC
     * %anim references instead) and must say so, rather than be handed some
     * other species' state names. */
    if (!a->species || !a->species->stateApply || !a->species->stateIndex) {
        /* Not a gap to be filled with an invented clip table: T5 has no
         * native animation state machine for any species but the dog, and
         * animates the rest from GSC. That scripted surface is registered
         * (src/online/script/script_anim.c: setanim, setanimknob,
         * setanimknoball, the setflaggedanim* family, clearanim,
         * setanimtime, getanimtime) and publishes onto the same wire this
         * method does, so a species with no native backend animates by
         * calling those from its animscripts instead of this. */
        Scr_Error("setanimstate: this actor species has no native animation "
                  "state machine; animate it from its animscripts with "
                  "setanim/setflaggedanimknoball and a %anim reference");
        return;
    }
    int state=a->species->stateIndex(Scr_GetString(0));
    if (state<0) {Scr_ParamError(0,"unknown original MP actor animation state");return;}
    DObj *obj=Com_GetServerDObj(a->entity->s.number);
    if (!a->tree.anims || !a->species->stateApply(obj,a->entity->s.number,a->generation,
        a->serverTree,level.time,state,a->animationState,1,ActorStateRate(a,state),a->lean,a->aimUp,a->aimLeft)) {
        ((void)0);
        Scr_Error("actor animation state could not publish its native C1 tree transaction");return;
    }
    if (state != a->diagState) {
        ((void)0);
        a->diagState = state;
    }
    a->animationState=state;
    a->lastAnimationTime=level.time;
}
static void ActorSetAimAnimationWeights(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);
    if (!a)return;
    float up=Scr_GetFloat(0),left=Scr_GetFloat(1);
    if (!isfinite(up)||!isfinite(left)){Scr_Error("actor aim weights must be finite");return;}
    a->aimUp=up;a->aimLeft=left;
}
static void ActorDeltaTurnYaw(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);
    if(!a)return;
    if(!a->hasGoal){Scr_AddFloat(0);return;}
    const float *point=a->goal;
    if(a->routeCursor<a->routeCount) {
        const float *route=BotNavPoint(a->route[a->routeCursor]);
        if(route)point=route;
    }
    float yaw=atan2f(point[1]-a->entity->r.currentOrigin[1],point[0]-a->entity->r.currentOrigin[0])*57.295779513f;
    float delta=fmodf(yaw-a->entity->r.currentAngles[1],360.0f);
    if(delta>180)delta-=360;if(delta< -180)delta+=360;
    Scr_AddFloat(delta);
}
unsigned ActorEnemyModel(gentity_s *entity)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    return a?a->enemyModel:0;
}
static void ActorSetEnemyModel(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    const char *name=Scr_GetString(0);
    /* CoDOL replaces G_ModelIndex with the runtime bank registrar (the same
     * substitution _g_items.asm makes under OL_SERVER): a killstreak model
     * name is only known when the reward fires, so it cannot be a level-load
     * precache. Returns 0 when the bank is full, never an unpublished slot. */
    a->enemyModel=*name?RegisterRuntimeModel(name):0;
    ((void)0);
}
static void ActorOwner(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    gentity_s *owner=Scr_GetEntity(0);
    a->entity->r.ownerNum.setEnt(owner);
    a->sentient->ownerNum=owner?owner->s.number:-1;
}
static void ActorClearOwner(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    a->entity->r.ownerNum.setEnt(NULL);a->sentient->ownerNum=-1;
}
static void ActorClearTarget(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    ActorSetEnemy(a->entity,NULL);
}
/* A destination whose capsule starts solid freezes the actor for good: the
 * first sweep in actor_physics.cpp slide() returns 0 on startsolid, so
 * EVERY move is rejected and the origin never changes again -- MEASURED
 * 2026-09-20, 116 of 360 zm diag samples logged "phys ok 0 rej 60 grounded 0"
 * with zero velocity, i.e. a third of the zombies were wedged in geometry
 * from the moment they were placed.
 *
 * T5 does not place an actor into solid either: SpawnActor's CHECK_SPAWN arm
 * refuses the spawn outright when SpotWouldTelefrag(ent) (OpenBLOPS
 * Game/Server/game/actor_spawner.cpp:141-157). Refusing is wrong for this
 * caller -- the zm spawn structs are all this map offers, and dropping them
 * would just mean fewer zombies -- so the same test is used to LIFT the
 * capsule out instead, one step height at a time up to its own height, then
 * settle it back down onto whatever floor the lift cleared. Actors are made
 * passable for the test exactly as the physics move does, so overlapping
 * another zombie is not mistaken for solid. A destination that cannot be
 * resolved is left where script asked and named in the log. */
static int ActorResolveSolidOrigin(gentity_s *entity,float *origin)
{
    trace_t trace;
    ActorsPassable(entity,1);
    G_TraceCapsule(&trace,origin,entity->r.mins,entity->r.maxs,origin,
                   entity->s.number,entity->clipmask);
    int solid=trace.startsolid || trace.allsolid;
    if(solid) {
        float height=entity->r.maxs[2]-entity->r.mins[2];
        for(float lift=18.0f;lift<=height+18.0f;lift+=18.0f) {
            float probe[3]={origin[0],origin[1],origin[2]+lift};
            G_TraceCapsule(&trace,probe,entity->r.mins,entity->r.maxs,probe,
                           entity->s.number,entity->clipmask);
            if(trace.startsolid || trace.allsolid)continue;
            float floorPos[3]={probe[0],probe[1],probe[2]-lift-18.0f};
            G_TraceCapsule(&trace,probe,entity->r.mins,entity->r.maxs,floorPos,
                           entity->s.number,entity->clipmask);
            if(!trace.startsolid && !trace.allsolid && trace.fraction<1)
                probe[2]+=(floorPos[2]-probe[2])*trace.fraction;
            ((void)0);
            memcpy(origin,probe,12);
            ActorsPassable(entity,0);
            return 1;
        }
        ((void)0);
    }
    ActorsPassable(entity,0);
    return !solid;
}
/* A coop placement (forceteleport, getting off a ride): the nearest spot
 * where the actor's capsule is clear of the world and stands on walkable
 * floor, on the same side of any wall as the point asked for.
 *
 * MEASURED 2026-10-08 (codol_actorAudit, coop_spec_mp_estate_tropical):
 * authored SP spawner origins sit inside the MP level's geometry often
 * enough -- "(1382 2818 153) is solid and no lift up to 90 units clears
 * it" left a soldier standing inside a wall for the rest of his life (7
 * audit reports), and the duel spot (1395 706 50) did the same.  The lift
 * above only tries straight up.  Here the search widens sideways in rings,
 * and a candidate is taken only when the line from the asked-for point to
 * it at chest height is clear (when that point is itself in the open) --
 * so a soldier embedded in a thin wall comes out on the side his centre
 * was on, never through it.  Last resort: the nearest waypoint node within
 * 192 units with a clear line.  Returns 1 with *out set when placed. */
static int ActorCoopFloor(gentity_s *entity,const float *at,float *floorOut)
{
    trace_t t;
    float down[3]={at[0],at[1],at[2]-160.0f};
    G_TraceCapsule(&t,at,entity->r.mins,entity->r.maxs,at,entity->s.number,entity->clipmask);
    if(t.startsolid || t.allsolid) return 0;
    G_TraceCapsule(&t,at,entity->r.mins,entity->r.maxs,down,entity->s.number,entity->clipmask);
    if(t.startsolid || t.allsolid || t.fraction>=1 || t.normal[2]<0.7f) return 0;
    for(int i=0;i<3;++i) floorOut[i]=at[i]+(down[i]-at[i])*t.fraction;
    floorOut[2]+=0.25f;
    return 1;
}
static int ActorCoopLineClear(gentity_s *entity,const float *a,const float *b)
{
    trace_t t; float zero[3]={0,0,0};
    float from[3]={a[0],a[1],a[2]+40.0f}, to[3]={b[0],b[1],b[2]+40.0f};
    G_TraceCapsule(&t,from,zero,zero,to,entity->s.number,entity->clipmask & ~0x2000000);
    return !t.startsolid && !t.allsolid && t.fraction>=1;
}
static int ActorCoopPlace(gentity_s *entity,const float *origin,float *out)
{
    static const float ring[16][2]={{1,0},{.9239f,.3827f},{.7071f,.7071f},{.3827f,.9239f},{0,1},
        {-.3827f,.9239f},{-.7071f,.7071f},{-.9239f,.3827f},{-1,0},{-.9239f,-.3827f},{-.7071f,-.7071f},
        {-.3827f,-.9239f},{0,-1},{.3827f,-.9239f},{.7071f,-.7071f},{.9239f,-.3827f}};
    static const float radii[]={8,16,24,32,48,64,96};
    float floorPos[3],cand[3];
    trace_t t; float zero[3]={0,0,0};
    float chest[3]={origin[0],origin[1],origin[2]+40.0f};
    int placed=0;
    ActorsPassable(entity,1);
    /* Is the asked-for point's chest in the open?  Then every candidate
     * must be reachable from it in a straight line. */
    G_TraceCapsule(&t,chest,zero,zero,chest,entity->s.number,entity->clipmask & ~0x2000000);
    int centreOpen=!t.startsolid && !t.allsolid;
    /* The point itself, and lifts out of a floor the origin sinks into. */
    for(float lift=1.0f;lift<=19.0f && !placed;lift+=6.0f) {
        float at[3]={origin[0],origin[1],origin[2]+lift};
        if(ActorCoopFloor(entity,at,floorPos) && fabsf(floorPos[2]-origin[2])<=48.0f){memcpy(out,floorPos,12);placed=1;}
    }
    for(unsigned r=0;r<sizeof(radii)/sizeof(radii[0]) && !placed;++r)
        for(int k=0;k<16 && !placed;++k) {
            cand[0]=origin[0]+ring[k][0]*radii[r];
            cand[1]=origin[1]+ring[k][1]*radii[r];
            cand[2]=origin[2]+19.0f;
            if(!ActorCoopFloor(entity,cand,floorPos) || fabsf(floorPos[2]-origin[2])>48.0f) continue;
            if(centreOpen && !ActorCoopLineClear(entity,origin,floorPos)) continue;
            memcpy(out,floorPos,12);placed=1;
        }
    if(!placed && BotNavCount()>0) {
        int node=BotNavNearest(origin);
        const float *p=node>=0 ? BotNavPoint(node) : NULL;
        if(p) {
            float dx=p[0]-origin[0],dy=p[1]-origin[1],dz=p[2]-origin[2];
            float at[3]={p[0],p[1],p[2]+19.0f};
            if(dx*dx+dy*dy+dz*dz<=192.0f*192.0f && ActorCoopFloor(entity,at,floorPos) &&
               (!centreOpen || ActorCoopLineClear(entity,origin,floorPos))) {memcpy(out,floorPos,12);placed=1;}
        }
    }
    ActorsPassable(entity,0);
    if(placed && (fabsf(out[0]-origin[0])>1 || fabsf(out[1]-origin[1])>1 || fabsf(out[2]-origin[2])>2))
        ((void)0);
    if(!placed)
        ((void)0);
    return placed;
}
static int ActorIsCoopSpecies(const actor_lifetime_t *a)
{
    return a && a->species && a->species->category && ZoneTypeRunsSoldiers(a->species->category) && !a->species->quadruped;
}
static void ActorForceTeleport(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    float origin[3],angles[3];Scr_GetVector(0,origin);
    if(Scr_GetNumParam()>1)Scr_GetVector(1,angles);else memcpy(angles,a->entity->r.currentAngles,12);
    for(int i=0;i<3;++i)if(!isfinite(origin[i])||!isfinite(angles[i])){Scr_Error("actor teleport requires finite vectors");return;}
    /* A coop actor is placed only where it is clear and standing (see
     * ActorCoopPlace); the script learns whether that worked. */
    int placed=1;
    if(ActorIsCoopSpecies(a)) {
        float clear[3];
        if(ActorCoopPlace(a->entity,origin,clear)) memcpy(origin,clear,12);
        else placed=ActorResolveSolidOrigin(a->entity,origin);
    } else ActorResolveSolidOrigin(a->entity,origin);
    G_SetOrigin(a->entity,origin);
    ActorAuditPlaced(a->entity);
    memcpy(a->entity->r.currentAngles,angles,12);memcpy(a->entity->s.lerp.apos.trBase,angles,12);
    memcpy(a->sentient->origin,origin,12);memset(&a->physics,0,sizeof(a->physics));
    a->routeCount=a->routeCursor=0;
    a->routeRetryTime=0;
    SV_LinkEntity(a->entity);
    if(ActorIsCoopSpecies(a)) Scr_AddInt(placed);
}
struct actor_melee_params {
    float forward[3],right[3],up[3],muzzleTrace[3],gunForward[3];
    WeaponDef *weapon;
};
static_assert(offsetof(actor_melee_params,weapon)==0x3c,"retained weaponParms ABI");
extern "C" gentity_s *ActorMeleeTrace(gentity_s *,actor_melee_params *,float,float,float);
static cvar_t *actorMeleeRange,*actorMeleeWidth,*actorMeleeHeight;
static void ActorEye(gentity_s *entity,float *position)
{
    float matrix[4][3];unsigned tag=SL_GetString("tag_eye",0);
    int found=G_DObjGetWorldTagMatrix(entity,tag,matrix);
    SL_RemoveRefToString(tag);
    if(found)memcpy(position,matrix[3],12);
    else {memcpy(position,entity->r.currentOrigin,12);position[2]+=64;}
}
static void ActorGetEye(scr_entref_t ref)
{
    if(!ref.classnum && ref.entnum<ENTITYNUM_WORLD && ActorForEntity(&g_entities[ref.entnum])) {
        float eye[3];ActorEye(&g_entities[ref.entnum],eye);Scr_AddVector(eye);return;
    }
    Scr_GetEye(ref);
}
static void ActorMelee(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    if(a->lastMeleeTime==level.time)return;
    const char *name=a->weaponName?SL_ConvertToString(a->weaponName):"";
    int weapon=G_GetWeaponIndexForName(name);
    if(!weapon){Scr_Error("actor melee requires its real aiweapon definition");return;}
    actor_melee_params params={};params.weapon=BG_GetWeaponDef(weapon);
    if(!params.weapon){Scr_Error("actor melee definition unavailable");return;}
    ActorEye(a->entity,params.muzzleTrace);
    AngleVectors(a->entity->r.currentAngles,params.forward,params.right,params.up);
    float direction[3];int hasDirection=Scr_GetNumParam()!=0;
    if(hasDirection){Scr_GetVector(0,direction);for(int i=0;i<3;++i)if(!isfinite(direction[i])){Scr_ParamError(0,"melee direction must be finite");return;}}
    gentity_s *enemy=ActorEnemy(a->entity);
    if(enemy) {
        float target[3];
        if(enemy->client){memcpy(target,enemy->r.currentOrigin,12);target[2]+=enemy->client->ps.viewHeightCurrent;}
        else ActorEye(enemy,target);
        for(int i=0;i<3;++i)params.forward[i]=target[i]-params.muzzleTrace[i];
        if(hasDirection) {
            float length=sqrtf(direction[0]*direction[0]+direction[1]*direction[1]);
            if(length==0){Scr_ParamError(0,"melee direction needs horizontal component");return;}
            float scale=sqrtf(params.forward[0]*params.forward[0]+params.forward[1]*params.forward[1])/length;
            params.forward[0]=direction[0]*scale;params.forward[1]=direction[1]*scale;
        }
    } else if(hasDirection)memcpy(params.forward,direction,12);
    float length=sqrtf(params.forward[0]*params.forward[0]+params.forward[1]*params.forward[1]+params.forward[2]*params.forward[2]);
    if(length==0)return;
    for(int i=0;i<3;++i)params.forward[i]/=length;
    memcpy(params.gunForward,params.forward,12);
    a->lastMeleeTime=level.time;
    a->entity->s.weapon=weapon;
    gentity_s *hit=ActorMeleeTrace(a->entity,&params,actorMeleeRange->value,
        actorMeleeWidth->value,actorMeleeHeight->value);
    if(hit && hit->r.inuse)Scr_AddEntity(hit);
}
static void ActorVelocity(scr_entref_t ref)
{
    if(!ref.classnum && ref.entnum<ENTITYNUM_WORLD) {
        actor_lifetime_t *a=ActorForEntity(&g_entities[ref.entnum]);
        if(a){Scr_AddVector(a->physics.velocity);return;}
    }
    PlayerCmd_GetVelocity(ref);
}
static void ActorTalkSpecies(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    int mask=0;
    for(unsigned i=0;i<Scr_GetNumParam();++i) {
        const char *species=Scr_GetString(i);
        if(!strcmp(species,"all")){mask=-1;break;}
        const actor_species_t *row=ActorSpeciesByName(species);
        if(row)mask|=row->talkBit;
        // Original T5 ignores unrecognized species rather than errors.
    }
    a->talkMask=mask;
}
static void ActorEngagementMin(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    float distance=Scr_GetFloat(0),falloff=Scr_GetFloat(1);
    if(!isfinite(distance)||!isfinite(falloff)||falloff>distance){Scr_Error("minimum distance falloff must be <= minimum distance");return;}
    a->engageMin=distance;a->engageMinFalloff=falloff;
}
static void ActorEngagementMax(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    float distance=Scr_GetFloat(0),falloff=Scr_GetFloat(1);
    if(!isfinite(distance)||!isfinite(falloff)||falloff<distance){Scr_Error("maximum distance falloff must be >= maximum distance");return;}
    a->engageMax=distance;a->engageMaxFalloff=falloff;
}
static void ActorAnimationMode(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);
    if(!a)return;
    const char *name=Scr_GetString(0);
    static const char *names[]={"none","normal","pos deltas","angle deltas",
        "gravity","noclip","nogravity","zonly_physics","nophysics"};
    int mode;
    for(mode=0;mode<9;++mode)if(!strcmp(name,names[mode]))break;
    if(mode==9){Scr_ParamError(0,"unsupported actor animmode (point-relative anchor is not implemented)");return;}
    if(mode && (Scr_GetNumParam()<2 || Scr_GetInt(1))) {
        // Clear current route, not the authored script goal.
        a->routeCount=a->routeCursor=0;
    }
    a->scriptAnimMode=mode;
}
static void ActorSetGoalPosition(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);
    if (!a) return;
    float goal[3]; Scr_GetVector(0,goal);
    if (!ActorSetGoal(a->entity,goal))
        Scr_ParamError(0,"setgoalpos requires a finite position");
}
static void ActorOrientMode(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);
    if (!a) return;
    const char *mode=Scr_GetString(0);
    int orientation=0;
    float yaw=a->entity->r.currentAngles[1], vector[3];
    if (!strcmp(mode,"face angle")) {orientation=1;yaw=Scr_GetFloat(1);}
    /* IW4 "face current" leaves the yaw to the animation (AI_ORIENT_DONT_CHANGE):
     * a coop soldier's arrivals, exits and turn clips carry their own
     * rotation, which a fixed yaw would fight every frame.  The mp and zm
     * dogs keep the fixed current yaw they were tuned on. */
    else if (!strcmp(mode,"face current")) orientation=ActorIsCoopSpecies(a)?6:1;
    else if (!strcmp(mode,"face direction") || !strcmp(mode,"face point")) {
        Scr_GetVector(1,vector);
        if (!isfinite(vector[0]) || !isfinite(vector[1]) || !isfinite(vector[2])) {
            Scr_ParamError(1,"orientation vector must be finite");return;
        }
        int point=!strcmp(mode,"face point");
        if (point) for(int i=0;i<3;++i) vector[i]-=a->entity->r.currentOrigin[i];
        if (!point && vector[0]==0 && vector[1]==0) {
            Scr_ParamError(1,"cannot face (0,0,*)");return;
        }
        orientation=1;
        if (!point || vector[0]*vector[0]+vector[1]*vector[1]>=1)
            yaw=atan2f(vector[1],vector[0])*57.295779513f;
    } else if (!strcmp(mode,"face enemy")) orientation=2;
    else if (!strcmp(mode,"face enemy or motion")) orientation=3;
    else if (!strcmp(mode,"face goal")) orientation=4;
    else if (!strcmp(mode,"face motion")) orientation=5;
    else if (strcmp(mode,"face default")) {Scr_ParamError(0,"unknown actor orientation mode");return;}
    if (!isfinite(yaw)) {Scr_ParamError(1,"orientation yaw must be finite");return;}
    a->scriptOrient=orientation;
    a->scriptYaw=yaw;
    if (Scr_GetNumParam()>2 && Scr_GetInt(2)>0) {
        a->entity->r.currentAngles[1]=yaw;
        memcpy(a->entity->s.lerp.apos.trBase,a->entity->r.currentAngles,12);
    }
}
/* ---------------------------------------------------------------------------
 * The remaining engine surface the ported dog scripts call.
 *
 * These are AI/actor facilities CoD4x never had: the dog animscripts are the
 * first script in this tree that asks the engine whether an entity is an AI,
 * whether it can see another entity, where to aim at it, or to flashbang it.
 * Each is implemented against the native actor record, with an honest arm for
 * a non-actor receiver rather than a constant stand-in.
 * ------------------------------------------------------------------------ */

/* isai( <entity> )
 * IW4/T5 return true only for an actual AI actor. A player, a script model and
 * a bot client are all false here: a bot is a client, not a sentient actor. */
static void ScrActorIsAI(void)
{
    gentity_s *entity = Scr_GetEntity(0);
    Scr_AddBool(entity && !entity->client && ActorForEntity(entity) != NULL);
}

/* <actor> setFlashBanged( <on>, [<strength>] )
 * T5 ActorCmd_SetFlashBanged / Actor_SetFlashed (OpenBLOPS
 * game/actor_script_cmd.cpp, game_mp/actor_mp.cpp): one or two arguments,
 * strength defaults to 1; "off" clears flag and strength, "on" raises the
 * strength to the larger of the two and sets the flag.  The flag has no
 * expiry: the dog think (Actor_Dog_Think -> Actor_Exposed_FlashBanged) runs
 * the flashed animscript while it is set, and that script clears it. */
static void ActorSetFlashBanged(scr_entref_t ref)
{
    actor_lifetime_t *a = ActorMethodReceiver(ref); if (!a) return;
    unsigned params = Scr_GetNumParam();
    if (params != 1 && params != 2) { Scr_Error("Invalid call to setFlashBanged()."); return; }
    float strength = params == 2 ? Scr_GetFloat(1) : 1.0f;
    if (!isfinite(strength)) { Scr_ParamError(1, "flashbang strength must be finite"); return; }
    if (!Scr_GetInt(0)) { a->flashBanged = 0; a->flashStrength = 0; return; }
    if (strength > a->flashStrength) a->flashStrength = strength;
    a->flashBanged = 1;
}

/* <actor> getFlashBangedStrength()
 * T5 ActorCmd_GetFlashBangedStrength: the stored strength. dog_flashed
 * multiplies it by 1000 for its flashendtime. */
static void ActorGetFlashBangedStrength(scr_entref_t ref)
{
    actor_lifetime_t *a = ActorMethodReceiver(ref); if (!a) return;
    Scr_AddFloat(a->flashStrength);
}

/* C1 0x578a10's actor arm (actor +0xffc): the eye of an actor, 0 for an
 * entity that is not one. Used by the turret aim point. */
extern "C" int ActorEyePosition(gentity_s *entity, float *position)
{
    if (!entity || !entity->r.inuse || !ActorForEntity(entity)) return 0;
    ActorEye(entity, position);
    return 1;
}

/* Aim point: the eye of a client, the tag_eye of an actor. */
static int ScrShootAtPosition(gentity_s *entity, float *position)
{
    if (!entity || !entity->r.inuse) return 0;
    if (entity->client) {
        memcpy(position, entity->r.currentOrigin, 12);
        position[2] += entity->client->ps.viewHeightCurrent;
        return 1;
    }
    if (ActorForEntity(entity)) { ActorEye(entity, position); return 1; }
    memcpy(position, entity->r.currentOrigin, 12);
    position[2] += (entity->r.mins[2] + entity->r.maxs[2]) * .5f;
    return 1;
}

/* <entity> getShootAtPos()
 * Called on the dog's ENEMY, which in MP is always a player, so the client arm
 * is the reachable one. Nothing is returned for a freed entity, which is the
 * undefined result dog_shared::trackLoop already tests for. */
static void ScrGetShootAtPos(scr_entref_t ref)
{
    float position[3];
    if (ref.classnum || ref.entnum >= ENTITYNUM_WORLD) {
        Scr_ObjectError("getshootatpos requires an entity"); return;
    }
    if (ScrShootAtPosition(&g_entities[ref.entnum], position)) Scr_AddVector(position);
}

/* <entity> canSee( <other> )
 * The same eye-to-centre visibility the native actor perception uses, so a
 * script asking "can I see him" and the native enemy selection cannot
 * disagree. A trace that ends on the target's own entity counts as sight,
 * because the endpoint is inside its bounds. */
static void ScrCanSee(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum >= ENTITYNUM_WORLD) {
        Scr_ObjectError("cansee requires an entity"); return;
    }
    gentity_s *self = &g_entities[ref.entnum];
    gentity_s *other = Scr_GetEntity(0);
    if (!self->r.inuse || !other || !other->r.inuse) { Scr_AddBool(qfalse); return; }
    float eye[3], target[3];
    if (!ScrShootAtPosition(self, eye) || !ScrShootAtPosition(other, target)) {
        Scr_AddBool(qfalse); return;
    }
    trace_t trace; float zero[3] = {0,0,0};
    /* A zm actor sees past the other actors.  MEASURED (t5cap, three hounds
     * placed on one path at a frozen player, zm_sumpf_sh): in T5 every hound
     * sees the player over the first one biting him and breaks into the run
     * at ~480 units; here the 2nd and 3rd hounds' cansee hit the first hound
     * and they trotted the whole way.  Coop soldiers keep body occlusion. */
    actor_lifetime_t *seer = ActorForEntity(self);
    int passActors = ActorIsZmSpecies(seer);
    if (passActors) ActorsPassable(self, 1);
    G_TraceCapsule(&trace, eye, zero, zero, target, self->s.number, self->clipmask);
    if (passActors) ActorsPassable(self, 0);
    Scr_AddBool(!trace.startsolid && !trace.allsolid &&
        (trace.fraction >= 1.0f ||
         (trace.hitType == TRACE_HITTYPE_ENTITY && trace.hitId == other->s.number)));
}

/* <actor> teleport( <origin>, [<angles>] )
 * dog_combat nudges an attacking dog out of a player it would otherwise
 * overlap. Unlike forceteleport this keeps the actor's route and goal: it is
 * a position correction inside one attack, not a respawn. */
static void ActorTeleport(scr_entref_t ref)
{
    actor_lifetime_t *a = ActorMethodReceiver(ref); if (!a) return;
    float origin[3], angles[3];
    Scr_GetVector(0, origin);
    for (int i = 0; i < 3; ++i)
        if (!isfinite(origin[i])) { Scr_ParamError(0, "teleport origin must be finite"); return; }
    if (Scr_GetNumParam() > 1) {
        Scr_GetVector(1, angles);
        for (int i = 0; i < 3; ++i)
            if (!isfinite(angles[i])) { Scr_ParamError(1, "teleport angles must be finite"); return; }
        memcpy(a->entity->r.currentAngles, angles, 12);
        memcpy(a->entity->s.lerp.apos.trBase, angles, 12);
    }
    G_SetOrigin(a->entity, origin);
    ActorAuditPlaced(a->entity);
    memcpy(a->sentient->origin, origin, 12);
    memset(a->physics.velocity, 0, sizeof(a->physics.velocity));
    a->routeRetryTime = 0;
    SV_LinkEntity(a->entity);
}

/* <entity> disconnectpaths() / <entity> connectpaths()
 * T5 Path_DisconnectPaths / Path_ConnectPaths on the map graph: the zm
 * barrier clip cuts its window's negotiation link while the boards stand
 * (_zombiemode_blockers::blocker_init, T5's DISCONNECT_PATHS spawnflag on the
 * clip), zombie_goto_entrance restores it after the tear, the repair loop
 * cuts it again.  Any linked entity may call them (doors, debris). */
static void ScrEntDisconnectPaths(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum >= ENTITYNUM_WORLD) { Scr_ObjectError("disconnectpaths requires an entity"); return; }
    gentity_s *e = &g_entities[ref.entnum];
    if (!e->r.inuse || e->r.absmin[0] > e->r.absmax[0]) { Scr_ObjectError("disconnectpaths requires a linked entity with bounds"); return; }
    int n = BotNavDisconnectPaths(ref.entnum, e->r.absmin, e->r.absmax);
    if (n) ActorRouteTopologyChanged();
    ((void)0);
}
static void ScrEntConnectPaths(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum >= ENTITYNUM_WORLD) { Scr_ObjectError("connectpaths requires an entity"); return; }
    int n = BotNavConnectPaths(ref.entnum);
    if (n) ActorRouteTopologyChanged();
    ((void)0);
}
static void ActorFinishDamage(scr_entref_t ref);
/* codol_getWalkableWaypoints()
 * The origins of the loaded bot waypoint graph's standable nodes (stand,
 * crouch and negotiation stances, the ones ActorRouteWalkable accepts), for
 * maps\mp\_dogs when the map has no path nodes of its own. */
static void ScrWalkableWaypoints(void)
{
    Scr_MakeArray();
    for (int i = 0, n = BotNavCount(); i < n; ++i) {
        const float *p = BotNavPoint(i);
        if (!p || !ActorRouteWalkable(BotNavStance(i))) continue;
        Scr_AddVector(p);
        Scr_AddArray();
    }
}
/* findpath( start, end [, ignoreEnt, allowNegotiationLinks] )
 * T5's FindPath (SP builtin 0x0040a420; OpenBLOPS g_scr_main_mp.cpp lists it
 * as an SP stub): true when a path exists from start to end.  Here the path
 * is the waypoint graph the actors walk: end must be standable (dropped to
 * the floor by T5's node_droptofloor rule, not in solid, not floating), the
 * nearest walkable waypoint to it within 512 units, and a route over the
 * walkable links (ActorRouteEdge) from the nearest walkable waypoint to
 * start.  With no graph loaded every point is reachable in a straight line,
 * which is the only path the actors have there.  maps/mp/_dogs.gsc uses it to
 * send a leaving dog to an exit node it can reach (dog_get_exit_node). */
static int ActorNearestWalkableWaypoint(const float *point, float *distanceSq)
{
    int best = -1; float bestD = 0;
    for (int i = 0, n = BotNavCount(); i < n; ++i) {
        const float *p = BotNavPoint(i);
        if (!p || !ActorRouteWalkable(BotNavStance(i))) continue;
        float d = (float)Vec3DistanceSq(p, point);
        if (best < 0 || d < bestD) { best = i; bestD = d; }
    }
    if (distanceSq) *distanceSq = bestD;
    return best;
}
/* <actor> canReachPlayer(<player>): exact actor route eligibility shared
 * with native acquisition, retained enemies and explicit enemy assignments. */
static void ActorCanReachPlayer(scr_entref_t ref)
{
    actor_lifetime_t *a = ActorMethodReceiver(ref);
    if (!a) return;
    gentity_s *player = Scr_GetEntity(0);
    if (!player || !player->client) { Scr_ParamError(0, "canReachPlayer requires a player"); return; }
    sentient_t *s = Sentient_ForEntity(player);
    Scr_AddBool(player->r.inuse && player->health > 0 && s && !s->dontTarget && ActorMayTarget(a, s) &&
        (ActorRequiresReachablePlayer(a) || ActorReachablePlayer(a, player)));
}
static void ScrFindPath(void)
{
    static unsigned short path[BOT_NODES];
    float start[3], end[3];
    Scr_GetVector(0, start); Scr_GetVector(1, end);
    for (int i = 0; i < 3; ++i)
        if (!isfinite(start[i]) || !isfinite(end[i])) { Scr_Error("findpath: points must be finite"); return; }
    static const float dropMins[3] = {-15.0f, -15.0f, 0.0f}, dropMaxs[3] = {15.0f, 15.0f, 30.0f};
    float top[3] = {end[0], end[1], end[2] + 1.0f}, bottom[3] = {end[0], end[1], end[2] - 256.0f};
    trace_t drop;
    G_TraceCapsule(&drop, top, dropMins, dropMaxs, bottom, ENTITYNUM_NONE, 0x820011);
    if (drop.startsolid || drop.allsolid || drop.fraction >= 1.0f) { Scr_AddBool(0); return; }
    end[2] = top[2] + (bottom[2] - top[2]) * drop.fraction;
    if (!BotNavCount()) { Scr_AddBool(1); return; }
    float endDistance;
    int from = ActorNearestWalkableWaypoint(start, NULL), to = ActorNearestWalkableWaypoint(end, &endDistance);
    if (from < 0 || to < 0 || endDistance > 512.0f*512.0f) { Scr_AddBool(0); return; }
    Scr_AddBool(from == to || BotNavRouteFiltered(from, to, path, BOT_NODES, ActorRouteEdge, NULL) > 0);
}
static void ActorBeginWounded(scr_entref_t ref);
static void ActorFinishWounded(scr_entref_t ref);
extern "C" void ActorRegisterSpawnMethods(void);
extern "C" void ActorRegisterMethods(void)
{
    Scr_AddMethod("canreachplayer", ActorCanReachPlayer, qfalse);
    Scr_AddMethod("codol_actorbeginwounded", ActorBeginWounded, qfalse);
    Scr_AddMethod("codol_actorfinishwounded", ActorFinishWounded, qfalse);
    Scr_AddMethod("disconnectpaths", ScrEntDisconnectPaths, qfalse);
    Scr_AddMethod("connectpaths", ScrEntConnectPaths, qfalse);
    actor_pace_trace=Cvar_RegisterInt("ai_tracePace",-1,-1,4095,0,"Print the T5 path pace of this actor entity every frame");
    actor_t5_pace=Cvar_RegisterBool("actorT5Pace",qtrue,0,"Actors walk paths at their cycle's average root speed, as T5 does");
    actorMeleeRange=Cvar_RegisterFloat("ai_meleeRange",64,0,1000,0,"AI melee range");
    actorMeleeWidth=Cvar_RegisterFloat("ai_meleeWidth",20,0,1000,0,"AI melee width");
    actorMeleeHeight=Cvar_RegisterFloat("ai_meleeHeight",10,0,1000,0,"AI melee height");
    ActorRegisterMpDogDvars();
    Scr_AddMethod("melee",ActorMelee,qfalse);
    ActorRegisterSpawnMethods();
    ThreatBias_RegisterScript();
    Scr_AddMethod("setenemymodel", ActorSetEnemyModel, qfalse);
    Scr_AddMethod("setentityowner", ActorOwner, qfalse);
    Scr_AddMethod("clearentityowner", ActorClearOwner, qfalse);
    Scr_AddMethod("clearentitytarget", ActorClearTarget, qfalse);
    Scr_AddMethod("forceteleport", ActorForceTeleport, qfalse);
    Scr_AddMethod("codol_actorride", ActorRide, qfalse);
    Scr_AddMethod("codol_actorshoot", ActorShootMethod, qfalse);
    Scr_AddMethod("codol_actorsetweapon", ActorSetWeaponMethod, qfalse);
    Scr_AddMethod("codol_actordropweapon", ActorDropWeaponMethod, qfalse);
    Scr_AddMethod("codol_actorreloadcomplete", ActorReloadCompleteMethod, qfalse);
    Scr_AddMethod("codol_actorclipcapacity", ActorClipCapacityMethod, qfalse);
    Scr_AddMethod("codol_actorclipremaining", ActorClipRemainingMethod, qfalse);
    Scr_AddMethod("codol_actorusecover", ActorUseCoverMethod, qfalse);
    Scr_AddMethod("codol_actorcovernode", ActorCoverNodeMethod, qfalse);
    Scr_AddMethod("codol_actorcoverpoint", ActorCoverPointMethod, qfalse);
    Scr_AddMethod("codol_actorcoverarrived", ActorCoverArrivedMethod, qfalse);
    Scr_AddMethod("codol_actorleavecover", ActorLeaveCoverMethod, qfalse);
    Scr_AddMethod("codol_actorlookahead", ActorLookaheadMethod, qfalse);
    Scr_AddMethod("codol_actorgoal", ActorGoalMethod, qfalse);
    Scr_AddMethod("codol_actorthrowgrenade", ActorThrowGrenadeMethod, qfalse);
    /* scr_vm_main.c registers the retained player-only geteye/getvelocity
     * first and Scr_AddMethod refuses a duplicate name, so these actor-aware
     * wrappers (which fall through to the retained implementation for a
     * non-actor receiver) silently lost: "entity N is not a player" from
     * dog_pain.gsc. Replace the registration. */
    Scr_RemoveMethod("geteye");
    Scr_RemoveMethod("getvelocity");
    Scr_AddMethod("geteye", ActorGetEye, qfalse);
    Scr_AddMethod("getvelocity", ActorVelocity, qfalse);
    Scr_AddMethod("settalktospecies", ActorTalkSpecies, qfalse);
    Scr_AddMethod("setengagementmindist", ActorEngagementMin, qfalse);
    Scr_AddMethod("setengagementmaxdist", ActorEngagementMax, qfalse);
    Scr_AddMethod("animmode", ActorAnimationMode, qfalse);
    Scr_AddMethod("setgoalpos", ActorSetGoalPosition, qfalse);
    Scr_AddMethod("orientmode", ActorOrientMode, qfalse);
    Scr_AddMethod("setgoalnode", ActorSetGoalNode, qfalse);
    Scr_AddMethod("setanimstate", ActorSetAnimationState, qfalse);
    Scr_AddMethod("setaimanimweights", ActorSetAimAnimationWeights, qfalse);
    Scr_AddMethod("getdeltaturnyaw", ActorDeltaTurnYaw, qfalse);
    /* Engine surface the ported dog scripts need; see the block above. */
    Scr_AddFunction("isai",ScrActorIsAI,qfalse);
    Scr_AddMethod("setflashbanged",ActorSetFlashBanged,qfalse);
    Scr_AddMethod("getflashbangedstrength",ActorGetFlashBangedStrength,qfalse);
    Scr_AddMethod("getshootatpos",ScrGetShootAtPos,qfalse);
    Scr_AddMethod("cansee",ScrCanSee,qfalse);
    Scr_AddMethod("teleport",ActorTeleport,qfalse);
    Scr_AddMethod("finishactordamage",ActorFinishDamage,qfalse);
    Scr_AddFunction("codol_getwalkablewaypoints",ScrWalkableWaypoints,qfalse);
    Scr_AddFunction("findpath",ScrFindPath,qfalse);
}

static int ActorScriptIsBound(const actor_lifetime_t *a, int function)
{
    const actor_script_table_t *s = &a->scripts;
    return function && (function == s->stop || function == s->move ||
        function == s->combat || function == s->death || function == s->pain ||
        function == s->flashed || function == s->turn || function == s->jump);
}

int ActorInitScripts(gentity_s *entity, const actor_script_table_t *table)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->typeStarted || !a->species || a->scriptsStarted || !table ||
        !table->init || !table->stop || !table->move || !table->combat ||
        !table->death || !table->pain || !table->flashed || !table->turn ||
        !table->jump) return 0;
    unsigned generation = a->generation;
    a->scripts = *table;
    a->scriptsStarted = 1;
    /* T5 Actor_InitAnimScript executes species init separately from the
     * replaceable movement/combat script. Its background child threads belong
     * to the entity and must not be killed on every movement transition. */
    short thread = Scr_ExecEntThread(entity, table->init, 0);
    Scr_FreeThread(thread);
    a = ActorForEntity(entity);
    return a && a->generation == generation && a->scriptsStarted;
}

void ActorStopScript(gentity_s *entity)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a) return;
    short thread = a->animThread;
    a->animThread = 0;
    a->animFunction = 0;
    /* Clear ownership before notifying: an endon or callback may free self. */
    if (thread) {
        Scr_Notify(entity, scr_const.killanimscript, 0);
        Scr_FreeThread(thread);
    }
}

int ActorSwitchScript(gentity_s *entity, int function, int moveMode, int animMode)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->scriptsStarted || a->switching ||
        !ActorScriptIsBound(a, function) || gScrVmPub.function_count ||
        gScrVmPub.inparamcount || gScrVmPub.outparamcount ||
        gScrVmPub.top != gScrVmPub.stack) return 0;
    unsigned generation = a->generation;
    if (a->animFunction == function && a->animThread &&
        Scr_GetObjectType((unsigned short)a->animThread)==VAR_DEAD_THREAD) {
        Scr_FreeThread(a->animThread);
        a->animThread=0;a->animFunction=0;
    }
    if (a->animFunction == function) {
        a->animMode = animMode;
        if (a->moveMode != moveMode) {
            a->moveMode = moveMode;
            /* Original dog_move listens to this event; do not restart it
             * merely because the native movement intent changed. */
            unsigned name = SL_GetString("movemode", 0);
            Scr_Notify(entity, (unsigned short)name, 0);
            SL_RemoveRefToString(name);
        }
        a = ActorForEntity(entity);
        return a && a->generation == generation;
    }
    if (a->animFunction && !a->safeToChangeScript) return 1;
    a->switching = 1;
    /* T5 Actor_SetAnimScript processes the canceled script at the previous
     * script time, before executing the replacement. This is an AI-frame
     * operation, not a nested call from a script builtin. */
    Scr_DecTime();
    ActorStopScript(entity);
    Scr_RunCurrentThreads();
    a = ActorForEntity(entity);
    if (!a || a->generation != generation) {
        Scr_IncTime();
        return 0;
    }
    ((void)0);
    a->animFunction = function;
    a->scriptOrient = 0; // T5 clears prior script orientation on replacement.
    a->scriptAnimMode = 0;
    a->moveMode = moveMode;
    a->animMode = animMode;
    /* Parameters are pushed last-first (Scr_PlayerKilled).  Only the
     * negotiation entry carries any: main(begin_origin, begin_angles,
     * end_origin, animscript). */
    unsigned numParams = 0;
    if (s_switchParams) {
        Scr_AddString(s_switchParams->script);
        Scr_AddVector(s_switchParams->end);
        Scr_AddVector(s_switchParams->angles);
        Scr_AddVector(s_switchParams->begin);
        numParams = 4;
        s_switchParams = NULL;
    }
    short thread = Scr_ExecEntThread(entity, function, numParams);
    a = ActorForEntity(entity);
    if (a && a->generation == generation) {
        a->animThread = thread; // keep reference until replacement or free
        a->switching = 0;
    } else if (thread) {
        Scr_FreeThread(thread);
    }
    /* Shared entity frame still owns DObj ticking; do not tick it twice. */
    Scr_IncTime();
    a = ActorForEntity(entity);
    return a && a->generation == generation && a->animFunction == function;
}

int ActorScriptModes(gentity_s *entity, int *moveMode, int *animMode)
{
    actor_lifetime_t *a = ActorForEntity(entity);
    if (!a || !a->scriptsStarted || !moveMode || !animMode) return 0;
    *moveMode = a->moveMode;
    *animMode = a->animMode;
    return 1;
}

extern "C" void G_DObjUpdate(gentity_s *entity);
XAnimTree_s *ActorOwnedTree(gentity_s *entity)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    if(a && a->tree.anims)return &a->tree;
    for(unsigned i=0;i<MAX_ACTOR_CORPSES;++i)if(corpses[i].entity==entity &&
        entity->r.inuse && corpses[i].generation==ACTOR_INCARNATION(entity->useCount))return &corpses[i].tree;
    /* The other server-animated entities: an RC-XD whose wheel clips are
     * driven natively (vehicle_wheel_anim.h), and a script_model given
     * a tree by the setanim family (script_model_tree.h). G_DObjUpdate
     * asks here. */
    if(XAnimTree_s *wheels=VehicleWheelAnimTree(entity))return wheels;
    return ScriptModelAnimTree(entity);
}
#define ANIM_WARN(...) Com_PrintWarning(CON_CHANNEL_SERVER, "CoDOL actor: animation: " __VA_ARGS__)
int ActorPrepareAnimation(gentity_s *entity)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    if (!a || !a->typeStarted) { ANIM_WARN("ent %d is not a started actor\n", entity->s.number); return 0; }
    if (!a->species) { ANIM_WARN("actor ent %d: aitype main never set self.type to a known species\n", entity->s.number); return 0; }
    const actor_species_t *row=a->species;
    if (!entity->model) { ANIM_WARN("actor ent %d has no model (character script setModel failed?)\n", entity->s.number); return 0; }
    if (entity->s.eType!=ET_ACTOR || a->tree.anims) { ANIM_WARN("actor ent %d: eType %d / tree already owned\n", entity->s.number, entity->s.eType); return 0; }
    unsigned serverTree, treeCount=ServerAnimTreeCount();
    for(serverTree=1;serverTree<=treeCount;++serverTree) {
        const char *name=ServerAnimTreeName(serverTree);
        if(name && !strcmp(name,row->animtree))break;
    }
    if(serverTree>treeCount) {
        /* The registry only holds trees a compiled script declared with
         * #using_animtree; maps/mp/_dogs.gsc declares "dog" and the zm script
         * set must declare this species' own tree for the same reason.
         * Name every slot: a registered-but-unnamed slot means the snapshot
         * did not run, which is a different defect from no declaration. */
        char dump[512]; unsigned i, used=0;
        dump[0]=0;
        for(i=1;i<=treeCount && used+72<sizeof(dump);++i) {
            const char *name=ServerAnimTreeName(i);
            used+=(unsigned)snprintf(dump+used,sizeof(dump)-used,"%s%u='%s'%s",
                used?" ":"",i,name?name:"<unnamed>",
                Scr_GetAnims(i)?"":"(no xanim)");
        }
        ANIM_WARN("no server animtree named \"%s\" is registered for species '%s' (%u registered: %s; a loaded script must declare #using_animtree(\"%s\") so animtrees/%s.atr mounts)\n",
                  row->animtree, row->name, treeCount, treeCount?dump:"none",
                  row->animtree, row->animtree);
        return 0;
    }
    XAnim_s *anims=Scr_GetAnims(serverTree);
    if(!anims) { ANIM_WARN("server animtree \"%s\" (%u) has no XAnim set\n", row->animtree, serverTree); return 0; }
    // Retained XAnimCreateTree initializes exactly this 8-byte representation.
    // Actor owns stable storage; native XAnimClearTree releases node records.
    a->tree.anims=anims;
    a->tree.children=0;
    a->tree.pad=0;
    a->serverTree=serverTree;
    /* C1 0x4083f0 registers the client's five actor composition trees at
     * bgs+0x90174, one per serverinfo-2482 bit; 0x428ba6 reads entityState
     * +0x59 as the index into it. The species row carries the index its
     * animtree is registered at (dog = 1, the verified C1 common_dog slot). */
    entity->s.lerp.u.actor.treeSelector=(int)row->treeSelector;
    G_DObjUpdate(entity); // native callsite supplies this owned tree to create
    DObj *obj=Com_GetServerDObj(entity->s.number);
    if(!obj || DObjGetTree(obj)!=&a->tree) {
        ANIM_WARN("actor ent %d: server DObj %s the owned '%s' tree (model %u)\n",
                  entity->s.number, obj?"does not use":"was not created for",
                  row->animtree, entity->model);
        return 0;
    }
    return 1;
}
int ActorFinishInitialization(gentity_s *entity, unsigned treeSelector)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    if(!a || !a->typeStarted || !a->scriptsStarted || a->initialized ||
       !a->species || treeSelector!=a->species->treeSelector ||
       entity->s.eType!=ET_ACTOR || !entity->model || !a->tree.anims)return 0;
    DObj *obj=Com_GetServerDObj(entity->s.number);
    if(!obj || obj->entnum!=entity->s.number+1 || DObjGetTree(obj)!=&a->tree)return 0;
    a->initialized=1;
    return 1;
}

int ActorCaptureSources(dog_anim_capture_source_t *out,
                            unsigned capacity, unsigned *count)
{
    dog_anim_capture_source_t sources[MAX_ACTOR_SLOTS];
    unsigned n=0;
    if (!count || (!out && capacity)) return 0;
    *count=0;
    for (unsigned i=0; i<MAX_ACTOR_SLOTS; ++i) {
        actor_lifetime_t *a=&actors[i];
        if (!a->allocated || !a->initialized) continue;
        if (ActorForEntity(a->entity) != a) return 0;
        DObj *obj=Com_GetServerDObj(a->entity->s.number);
        if (!obj || obj->entnum != a->entity->s.number+1 || !DObjGetTree(obj)) return 0;
        sources[n].entity=(unsigned)a->entity->s.number;
        sources[n].generation=a->generation;
        sources[n].tree=DObjGetTree(obj);
        ++n;
    }
    if (n>capacity) return 0;
    if (n) memcpy(out,sources,n*sizeof(*out));
    /* Wheel-animated vehicles ride the same pose capture and journal delivery
     * (C1 plays their commands through the same executor 0x443e90). They take
     * only the capacity actors leave; one that does not fit is simply not
     * visible to the animation channel this frame. */
    n+=VehicleWheelAnimCaptureSources(out+n,capacity-n);
    /* Script models animated by snapshot commands (the mystery-box lid) ride
     * it the same way, after the vehicles. */
    n+=ScriptModelTreeCaptureSources(out+n,capacity-n);
    *count=n;
    return 1;
}

int ActorArchivePose(int frame, unsigned time)
{
    dog_anim_capture_source_t sources[MAX_ACTOR_SLOTS];
    unsigned count;
    if (!ActorCaptureSources(sources, MAX_ACTOR_SLOTS, &count))
        return 0;
    return DogAnimArchiveStore(frame, time, sources, count);
}

static void ActorRecordDamage(gentity_s *entity,gentity_s *attacker,const float *direction)
{
    actor_lifetime_t *a=ActorForEntity(entity);if(!a)return;
    float x=direction?-direction[0]:0,y=direction?-direction[1]:0;
    if(!direction && attacker){x=attacker->r.currentOrigin[0]-entity->r.currentOrigin[0];y=attacker->r.currentOrigin[1]-entity->r.currentOrigin[1];}
    float yaw=(x!=0 || y!=0)?atan2f(y,x)*57.295779513f:entity->r.currentAngles[1];
    a->damageYaw=fmodf(yaw-entity->r.currentAngles[1]+360,360);
    /* An MP dog shot by a player is attacked by him (T5 Actor_WasAttackedBy:
     * perfect information, iLastAttackMeTime) and its packmates within
     * ai_eventDistPain hear its pain (Actor_Pain -> AI_EV_PAIN); the threat
     * pass then picks the enemy within the attacker limit. */
    int c=ActorClientNum(attacker);
    if(ActorIsMpDog(a) && c>=0 && attacker!=entity){
        MpDogKnow(a,attacker);
        a->mpAttackedTime[c]=level.time>0?level.time:1;
        if(entity->health>0)
            MpDogBroadcast(MPDOG_EV_PAIN,entity,entity->r.currentOrigin,ai_eventDistPain->value,attacker);
        return;
    }
    if(attacker && attacker!=entity)ActorSetEnemy(entity,attacker);
}
extern "C" void ActorPain(gentity_s *entity,gentity_s *attacker,int damage,
    const float *point,int meansOfDeath,const float *direction,hitLocation_t location,int weapon)
{
    (void)damage;(void)point;(void)meansOfDeath;(void)location;(void)weapon;
    ActorRecordDamage(entity,attacker,direction);
    actor_lifetime_t *a=ActorForEntity(entity);
    /* T5 Actor_Pain (actor_mp.cpp:2080): no AIS_PAIN while allowpain is off. */
    if(a && entity->health>0) {
        /* Retain the actual damaging entity generation for IW4's
         * killWrapper(last_dmg_player) bleedout credit, never a reused slot. */
        a->woundAttackerNum=attacker && attacker->r.inuse ? attacker->s.number : ENTITYNUM_WORLD;
        a->woundAttackerUseCount=attacker && attacker->r.inuse ? attacker->useCount : 0;
        a->woundMod=meansOfDeath; a->woundWeapon=weapon; a->woundHitLoc=(int)location;
        memcpy(a->woundDirection,direction?direction:vec3_origin,sizeof(a->woundDirection));
        if(a->allowPain && !a->wounded)a->painPending=1;
    }
}
/* ---------------------------------------------------------------------------
 * Script damage and death callbacks for actors (T5 MP, OpenBLOPS
 * Game/Server/game_mp/g_combat_mp.cpp G_Damage actor arm,
 * g_scr_main_mp.cpp Scr_ActorDamage / Scr_ActorKilled and
 * game/actor_script_cmd.cpp ActorCmd_finishActorDamage).
 *
 * T5 hands every damage an actor takes to
 * maps/mp/gametypes/_callbacksetup::CodeCallback_ActorDamage, and the actor
 * only loses health when script answers with finishActorDamage; that is where
 * _globallogic_actor::Callback_ActorDamage refuses friendly fire, damage from
 * the dogs' own owner and between dogs of one owner, and draws the attacker's
 * hit marker.  The retained G_Damage applied actor damage natively with no
 * such gate, so a teammate - or the owner himself - could shoot the dogs dead.
 * The dispatcher mirrors VehicleDamageCallback: finishActorDamage
 * re-enters G_Damage under a gate, so health, the "damage"/"death" notifies
 * and the pain/die handlers stay the retained ones.  A script tree without the
 * callback (handle 0, the zm tree) keeps the native path, entered once more
 * with the scaled damage below.
 *
 * Before the callback T5 scales the damage by the weapon's hit-location
 * multiplier unless it is a melee hit, and never passes less than 1; it does
 * so whether or not script defines the callback, and exactly once (the
 * finishActorDamage re-entry is not scaled again).  Actors are traced through
 * their hit boxes (actor_spawn.cpp svFlags bit 2), so hitLoc is the
 * struck location, not HITLOC_NONE.  The
 * multiplier here is C1's (G_GetWeaponHitLocationMultiplier 0x521010, the
 * same arithmetic _g_combat_mp.asm G_DamageClient_50 uses for players).
 * ------------------------------------------------------------------------ */
extern "C" int __cdecl G_MeansOfDeathFromScriptParam(int paramIndex);
extern "C" int __cdecl G_GetHitLocationIndexFromString(unsigned int constString);
extern "C" void __cdecl G_Damage(gentity_s *targ, gentity_s *inflictor, gentity_s *attacker,
    const float *dir, const float *point, int damage, int dflags, int mod, int weapon,
    int hitLoc, unsigned int modelIndex, unsigned int partName, int timeOffset);
extern "C" float g_fHitLocDamageMult[];
static int actorDamageHandle, actorKilledHandle, actorDamageFinishing;
/* The GSC decides which authored fall/crawl fits and the random/cooldown
 * policy. Native state validates that this is still a low-health soldier
 * pain state; it never caps a lethal hit, adds health, or suppresses death. */
static void ActorBeginWounded(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    if(Scr_GetNumParam()){Scr_Error("codol_actorBeginWounded takes no arguments");return;}
    gentity_s *entity=a->entity;
    if(!ActorIsCoopSpecies(a) || !entity->r.inuse || entity->s.eType!=ET_ACTOR ||
       !entity->takedamage || !a->allowPain || a->rideNum || a->negotiating || a->scripted ||
       a->deathTime>=0 || a->wounded || entity->health<=0 ||
       entity->maxHealth<=0 || entity->health>100 ||
       (float)entity->health >= (float)entity->maxHealth*.4f ||
       a->animFunction!=a->scripts.pain) { Scr_AddBool(false);return; }
    a->wounded=1; a->painPending=0; a->safeToChangeScript=0;
    a->hasGoal=0; a->routeCount=a->routeCursor=0;
    if(a->coverIndex>=0) {
        CoverRelease(a->coverIndex,entity->s.number);
        a->coverIndex=-1;a->coverArrived=0;
    }
    Scr_AddBool(true);
}
static void ActorFinishWounded(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    if(Scr_GetNumParam()){Scr_Error("codol_actorFinishWounded takes no arguments");return;}
    gentity_s *entity=a->entity;
    if(!a->wounded || a->deathTime>=0 || entity->health<=0)return;
    gentity_s *attacker=&g_entities[ENTITYNUM_WORLD];
    if(a->woundAttackerNum>=0 && a->woundAttackerNum<ENTITYNUM_WORLD) {
        gentity_s *candidate=&g_entities[a->woundAttackerNum];
        if(candidate->r.inuse && candidate->useCount==a->woundAttackerUseCount)attacker=candidate;
    }
    float direction[3];memcpy(direction,a->woundDirection,sizeof(direction));
    if(direction[0]==0 && direction[1]==0 && direction[2]==0)direction[2]=1;
    int finishing=actorDamageFinishing;actorDamageFinishing=1;
    /* Already-scaled remaining health, through the retained damage/notify/
     * die path exactly once. No direct death event or reward is fabricated. */
    G_Damage(entity,attacker,attacker,direction,entity->r.currentOrigin,entity->health,0,
        a->woundMod,a->woundWeapon,a->woundHitLoc,0,0,0);
    actorDamageFinishing=finishing;
}
static gentity_s *actorDamageScaledTarget;
/* src/g_combat_mp.cpp modNames[7] is MOD_MELEE (the C enum is not built). */
static const int ACTOR_MOD_MELEE=7;
extern "C" void ActorDamage_LoadCallbacks(void)
{
    /* The category's own callback module (ScriptEntryPoints). */
    const char *callbacks=CurrentScriptEntryPoints()->callbackModule;
    actorDamageHandle=Scr_GetFunctionHandle(callbacks,"CodeCallback_ActorDamage");
    actorKilledHandle=Scr_GetFunctionHandle(callbacks,"CodeCallback_ActorKilled");
    actorDamageFinishing=0;
    actorDamageScaledTarget=NULL;
}
static void ActorPushWeapon(int weapon)
{
    WeaponDef *def=weapon>0?BG_GetWeaponDef(weapon):NULL;
    Scr_AddString(def?def->szInternalName:"none");
}
static void ActorPushMeansOfDeath(int mod)
{
    if(mod>=0 && mod<NATIVE_DAMAGE_MOD_COUNT && modNames[mod])Scr_AddConstString(*modNames[mod]);
    else Scr_AddString("badMOD");
}
static void ActorPushHitLocation(int hitLoc)
{
    Scr_AddConstString(G_GetHitLocationString((hitLocation_t)(hitLoc>=0 && hitLoc<HITLOC_NUM?hitLoc:HITLOC_NONE)));
}
static float ActorHitLocationMultiplier(int hitLoc,int weapon)
{
    if(hitLoc<0 || hitLoc>=HITLOC_NUM)return 1.0f;
    WeaponDef *def=weapon>0?BG_GetWeaponDef(weapon):NULL;
    if(def && def->weapType==WEAPTYPE_BULLET && (int)def->weapClass!=7)
        return def->locationDamageMultipliers[hitLoc]*g_fHitLocDamageMult[hitLoc];
    return g_fHitLocDamageMult[hitLoc];
}
extern "C" int ActorDamageCallback(gentity_s *targ,gentity_s *inflictor,gentity_s *attacker,
    const float *dir,const float *point,int damage,int dflags,int mod,int weapon,
    int hitLoc,unsigned int modelIndex,unsigned int partName,int timeOffset)
{
    if(!targ || targ->client || actorDamageFinishing)return 0;
    /* The scaled re-entry below: consumed by the first callback it reaches,
     * which is this G_Damage's own (nothing between its entry and this call
     * can return early for a target already past those gates). */
    if(actorDamageScaledTarget){
        if(actorDamageScaledTarget==targ){actorDamageScaledTarget=NULL;return 0;}
        actorDamageScaledTarget=NULL;
    }
    actor_lifetime_t *a=ActorForEntity(targ);
    if(!a || a->deathTime>=0 || targ->health<=0)return 0;
    if(mod!=ACTOR_MOD_MELEE)damage=(int)(ActorHitLocationMultiplier(hitLoc,weapon)*(float)damage);
    if(damage<=0)damage=1;
    if(!actorDamageHandle) {
        /* No CodeCallback_ActorDamage (the zm tree defines none): T5
         * G_DamageActor still scales by the hit location before the damage
         * lands, so do it here and run the native path once with the scaled
         * damage, keeping the struck model and bone for the "damage" notify. */
        actorDamageScaledTarget=targ;
        G_Damage(targ,inflictor,attacker,dir,point,damage,dflags,mod,weapon,hitLoc,modelIndex,partName,timeOffset);
        actorDamageScaledTarget=NULL;
        return 1;
    }
    /* Scr_ActorDamage pushes last-first: (eInflictor, eAttacker, iDamage,
     * iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset). */
    Scr_AddInt(timeOffset);
    ActorPushHitLocation(hitLoc);
    Scr_AddVector(dir?dir:vec3_origin);
    Scr_AddVector(point?point:vec3_origin);
    ActorPushWeapon(weapon);
    ActorPushMeansOfDeath(mod);
    Scr_AddInt(dflags);
    Scr_AddInt(damage);
    if(attacker)Scr_AddEntity(attacker);else Scr_AddUndefined();
    if(inflictor)Scr_AddEntity(inflictor);else Scr_AddUndefined();
    short thread=Scr_ExecEntThread(targ,actorDamageHandle,10);
    Scr_FreeThread(thread);
    return 1;
}
/* <actor> finishActorDamage( eInflictor, eAttacker, iDamage, iDFlags,
 *     sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime )
 * T5 ActorCmd_finishActorDamage: world defaults for an undefined inflictor or
 * attacker, damage <= 0 does nothing, damage to a dead actor is an error. */
static void ActorFinishDamage(scr_entref_t ref)
{
    actor_lifetime_t *a=ActorMethodReceiver(ref);if(!a)return;
    if(Scr_GetNumParam()!=10){Scr_Error("finishactordamage requires 10 arguments");return;}
    int damage=Scr_GetInt(2);
    if(damage<=0)return;
    if(a->deathTime>=0 || a->entity->health<=0){Scr_Error("Trying to do damage to a actor that is already dead");return;}
    gentity_s *inflictor=&g_entities[ENTITYNUM_WORLD],*attacker=&g_entities[ENTITYNUM_WORLD];
    if(Scr_GetType(0)==VAR_POINTER && Scr_GetPointerType(0)==VAR_ENTITY)inflictor=Scr_GetEntity(0);
    if(Scr_GetType(1)==VAR_POINTER && Scr_GetPointerType(1)==VAR_ENTITY)attacker=Scr_GetEntity(1);
    int dflags=Scr_GetInt(3);
    int mod=G_MeansOfDeathFromScriptParam(4);
    int weapon=G_GetWeaponIndexForName(Scr_GetString(5));
    if(!weapon && (unsigned short)Scr_GetConstString(5)!=scr_const.none) {
        Scr_Error(va("weapon [%s] has not been precached",Scr_GetString(5)));return;
    }
    float point[3]={0,0,0},dir[3]={0,0,0};
    const float *pointArg=NULL;
    if(Scr_GetType(6)!=VAR_UNDEFINED){Scr_GetVector(6,point);pointArg=point;}
    if(Scr_GetType(7)!=VAR_UNDEFINED)Scr_GetVector(7,dir);
    int hitLoc=G_GetHitLocationIndexFromString((unsigned short)Scr_GetConstString(8));
    int timeOffset=Scr_GetInt(9);
    /* The retained G_Damage normalizes dir whenever it runs its tail; a zero
     * direction reads as straight up there, as T5 doDamage substitutes (see
     * Scr_DoDamage for the null-direction crash this avoids). */
    if(dir[0]==0 && dir[1]==0 && dir[2]==0)dir[2]=1;
    actorDamageFinishing=1;
    G_Damage(a->entity,inflictor,attacker,dir,pointArg,damage,dflags,mod,weapon,hitLoc,0,0,timeOffset);
    actorDamageFinishing=0;
}
/* T5 MP Actor_Die -> Scr_ActorKilled: (eInflictor, eAttacker, iDamage,
 * sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime). */
static void ActorKilledCallback(gentity_s *entity,gentity_s *inflictor,gentity_s *attacker,
    int damage,int mod,int weapon,const float *dir,int hitLoc,int timeOffset)
{
    if(!actorKilledHandle)return;
    Scr_AddInt(timeOffset);
    ActorPushHitLocation(hitLoc);
    Scr_AddVector(dir?dir:vec3_origin);
    ActorPushWeapon(weapon);
    ActorPushMeansOfDeath(mod);
    Scr_AddInt(damage);
    if(attacker)Scr_AddEntity(attacker);else Scr_AddUndefined();
    if(inflictor)Scr_AddEntity(inflictor);else Scr_AddUndefined();
    short thread=Scr_ExecEntThread(entity,actorKilledHandle,8);
    Scr_FreeThread(thread);
}
extern "C" void ActorDie(gentity_s *entity,gentity_s *inflictor,gentity_s *attacker,
    int damage,int meansOfDeath,int weapon,const float *direction,hitLocation_t location,int timeOffset)
{
    ActorRecordDamage(entity,attacker,direction);
    actor_lifetime_t *a=ActorForEntity(entity);if(!a || a->deathTime>=0)return;
    a->deathTime=level.time;a->wounded=0;a->painPending=0;a->safeToChangeScript=1;
    entity->takedamage=0;
    /* T5 Actor_Death -> AI_EV_DEATH: the packmates within ai_eventDistDeath
     * learn who killed this dog. */
    if(ActorIsMpDog(a) && ActorClientNum(attacker)>=0)
        MpDogBroadcast(MPDOG_EV_DEATH,entity,entity->r.currentOrigin,ai_eventDistDeath->value,attacker);
    DogCombatRelease(entity);
    // Generic G_Damage already sent death. Do not send a duplicate reward event.
    ActorKilledCallback(entity,inflictor,attacker,damage,meansOfDeath,weapon,direction,(int)location,timeOffset);
}

/* The corpse slot for a new body: a free one below the limit, else the one
 * T5 G_GetFreeActorCorpseIndex would give up -- the farthest corpse behind
 * the player, else the farthest at least 45 degrees off the view direction,
 * else the farthest. T5 measures from the first player only; with several
 * players a corpse counts as behind (or off-axis) only when it is so for
 * every player in the game, and "farthest" is its distance to the nearest
 * of them, so a body somebody is looking at is the last to go. With no
 * player in the game the oldest corpse is replaced. */
static unsigned ActorCorpseSlot(unsigned limit)
{
    /* A cooled free slot first, then the free one released longest ago
     * (SLOT_REUSE_MS): the exchange that gives this slot's old tree to
     * the dying actor's slot must not find that tree still in use. */
    unsigned warm=limit;
    for(unsigned i=0;i<limit;++i)if(!corpses[i].entity) {
        if(SlotCooled(corpseSlotReleased[i]))return i;
        if(warm==limit || corpseSlotReleased[i]<corpseSlotReleased[warm])warm=i;
    }
    if(warm<limit)return warm;
    float eye[MAX_CLIENTS][3],view[MAX_CLIENTS][2];unsigned viewers=0;
    for(int c=0;c<level.maxclients && c<MAX_CLIENTS;++c) {
        gentity_s *p=&g_entities[c];
        if(!p->r.inuse || !p->client || p->client->sess.sessionState!=SESS_STATE_PLAYING)continue;
        float yaw=p->client->ps.viewangles[1]*.01745329252f;
        memcpy(eye[viewers],p->r.currentOrigin,12);
        view[viewers][0]=cosf(yaw);view[viewers][1]=sinf(yaw);
        ++viewers;
    }
    unsigned oldest=0;
    int behind=-1,side=-1,farthest=-1;
    float behindDist=-1,sideDist=-1,farDist=-1;
    for(unsigned i=0;i<limit;++i) {
        if(corpses[i].time<corpses[oldest].time)oldest=i;
        if(!viewers || !corpses[i].entity)continue;
        const float *o=corpses[i].entity->r.currentOrigin;
        float nearest=-1;int allBehind=1,allSide=1;
        for(unsigned v=0;v<viewers;++v) {
            float dx=o[0]-eye[v][0],dy=o[1]-eye[v][1];
            float distSq=dx*dx+dy*dy, dot=dx*view[v][0]+dy*view[v][1];
            if(nearest<0 || distSq<nearest)nearest=distSq;
            if(dot>=0)allBehind=0;
            if(dot>=0 && .5f*distSq<dot*dot)allSide=0;
        }
        if(allBehind && nearest>behindDist){behindDist=nearest;behind=(int)i;}
        if(allSide && nearest>sideDist){sideDist=nearest;side=(int)i;}
        if(nearest>farDist){farDist=nearest;farthest=(int)i;}
    }
    if(behind>=0)return (unsigned)behind;
    if(side>=0)return (unsigned)side;
    if(farthest>=0)return (unsigned)farthest;
    return oldest;
}
static int ActorBecomeCorpse(gentity_s *entity)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    if(!a || !a->tree.anims)return 0;
    /* Captured before ActorRelease below invalidates the record: the
     * corpse keeps composing out of the same client actor tree the live
     * actor used, which is a species column, not always the dog's. */
    unsigned treeSelector=a->species?a->species->treeSelector:1u;
    unsigned limit=ActorCorpseLimit();
    /* ai_corpseCount lowered since the last death: T5 G_RemoveActorCorpses
     * frees the slots above the new count. */
    for(unsigned i=limit;i<MAX_ACTOR_CORPSES;++i)
        if(corpses[i].entity && corpses[i].entity!=entity)G_FreeEntity(corpses[i].entity);
    unsigned index=ActorCorpseSlot(limit);
    if(corpses[index].entity && corpses[index].entity!=entity)G_FreeEntity(corpses[index].entity);
    actor_corpse_owner pending={};
    actor_corpse_owner *body=&pending;
    body->entity=entity;body->generation=ACTOR_INCARNATION(entity->useCount);body->time=level.time;
    body->tree=a->tree;body->physics=a->physics;
    body->enemyModel=a->enemyModel;body->team=a->sentient?(unsigned)a->sentient->team:0u;
    memset(&a->tree,0,sizeof(a->tree)); // transfer nodes, do not clear them
    DObj *obj=Com_GetServerDObj(entity->s.number);
    if(obj)obj->tree=NULL;
    ActorRelease(entity);
    corpses[index]=pending;
    body=&corpses[index];
    if(obj)obj->tree=&body->tree;
    entity->s.eType=ET_ACTOR_CORPSE;
    /* The launch is spent by now (the client consumed it when it created
     * the ragdoll); never let a corpse hand-off apply it a second time. */
    entity->s.lerp.u.actor.ragdollHitLoc=0;
    memset(entity->s.lerp.u.actor.ragdollForce,0,sizeof(entity->s.lerp.u.actor.ragdollForce));
    entity->s.lerp.u.actor.actorSlot=(int)index;
    entity->s.lerp.u.actor.treeSelector=(int)treeSelector;
    entity->r.contents=CONTENTS_CORPSE;
    entity->clipmask=0x820011;
    entity->takedamage=0;
    // C1 46C4DF -> 42A5C0 copies the previous actor composition into the
    // corpse slot and swaps animation trees when this same entity changes type.
    Scr_FreeEntityNum(entity->s.number,0);
    G_DObjUpdate(entity);
    SV_LinkEntity(entity);
    return 1;
}
int ActorRunCorpseFrame(gentity_s *entity)
{
    for(unsigned i=0;i<MAX_ACTOR_CORPSES;++i)if(corpses[i].entity==entity &&
       corpses[i].generation==ACTOR_INCARNATION(entity->useCount)) {
        float dt=(level.time-level.previousTime)*.001f,zero[3]={0,0,0};
        if(g_gravity && dt>0 && dt<=.25f)
            ActorPhysicsMove(entity,&corpses[i].physics,zero,dt,g_gravity->value,0);
        return 1;
    }
    return 0;
}

void ActorForget(gentity_s *entity)
{
    ActorAmmoForget(entity);
    for(unsigned i=0;i<MAX_ACTOR_CORPSES;++i)if(corpses[i].entity==entity) {
        DObj *obj=Com_GetServerDObj(entity->s.number);
        if(obj && obj->tree==&corpses[i].tree)obj->tree=NULL;
        if(corpses[i].tree.anims)XAnimClearTree(&corpses[i].tree);
        memset(&corpses[i],0,sizeof(corpses[i]));
        corpseSlotReleased[i]=level.time+1;
    }
    /* Free hooks run while the entity is still allocated, but scan identity
     * directly so teardown also works if sentient's index was already cleared. */
    for (unsigned i=0; i<MAX_ACTOR_SLOTS; ++i)
        if (actors[i].allocated && actors[i].entity==entity) {
            short thread = actors[i].animThread;
            /* Child threads of the current animscript (dog_stop::lookAtTarget
             * -> dog_shared::trackLoop) end on "killanimscript". This runs
             * before Scr_FreeEntity / the corpse's Scr_FreeEntityNum, so a
             * deleted or corpse-converted dog cannot leave a thread that
             * wakes on an entity whose script fields are gone ("undefined is
             * not a field object" at self.a in trackLoop). */
            if (actors[i].scriptsStarted && entity->r.inuse)
                Scr_Notify(entity, scr_const.killanimscript, 0);
            DogCombatRelease(entity);
            if (actors[i].coverIndex >= 0) CoverRelease(actors[i].coverIndex, entity->s.number);
            DObj *obj=Com_GetServerDObj(entity->s.number);
            if(obj && obj->tree==&actors[i].tree)obj->tree=NULL;
            if(actors[i].tree.anims)XAnimClearTree(&actors[i].tree);
            /* server.h leaves the actor record packed. Scr_SetString's
             * actual C++ body dereferences uint16_t*: marshal through an
             * aligned local rather than passing an odd packed field. */
            unsigned short releasedWeaponName=actors[i].weaponName;
            Scr_SetString(&releasedWeaponName,0);
            actors[i].weaponName=releasedWeaponName;
            memset(&actors[i],0,sizeof(actors[i]));
            actorSlotReleased[i]=level.time+1;
            // G_FreeEntity owns termination of entity scripts. Drop only the
            // extra actor-held reference here; no callbacks during teardown.
            if (thread) Scr_FreeThread(thread);
        }
}
void ActorRelease(gentity_s *entity)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    if (!a) return;
    sentient_t *s=a->sentient;
    ActorForget(entity);
    Sentient_Free(s);
}
/* Late corpse presentation (late_corpse.cpp). A corpse slot's body,
 * valid only while its entity is still that corpse incarnation. */
int ActorCorpseGet(unsigned index, actor_corpse_info_t *out)
{
    if (index>=MAX_ACTOR_CORPSES || !out) return 0;
    actor_corpse_owner *c=&corpses[index];
    gentity_s *e=c->entity;
    if (!e || !e->r.inuse || e->s.eType!=ET_ACTOR_CORPSE ||
        c->generation!=ACTOR_INCARNATION(e->useCount) ||
        (unsigned)e->s.lerp.u.actor.actorSlot!=index || !c->tree.anims) return 0;
    out->entity=e;out->number=(unsigned)e->s.number;out->generation=c->generation;
    out->tree=&c->tree;out->enemyModel=c->enemyModel;out->team=c->team;
    return 1;
}
/* An actor slot nobody holds and nobody has released within
 * SLOT_REUSE_MS: no client can still have a record in it. */
int ActorSlotIdle(unsigned slot)
{
    return slot<MAX_ACTOR_SLOTS && !actors[slot].allocated &&
           !Sentient_ActorBySlot((int)slot) && SlotCooled(actorSlotReleased[slot]);
}
/* Keep a free slot warm while a client is shown a record in it, so the
 * allocator hands it out last, and only SLOT_REUSE_MS after the last
 * use. */
void ActorSlotHold(unsigned slot)
{
    if (slot<MAX_ACTOR_SLOTS && !actors[slot].allocated) actorSlotReleased[slot]=level.time+1;
}
void ActorLifetimeReset(void) { ActorAmmoReset();memset(actors,0,sizeof(actors));memset(corpses,0,sizeof(corpses));
    memset(actorSlotReleased,0,sizeof(actorSlotReleased));memset(corpseSlotReleased,0,sizeof(corpseSlotReleased));ActorDeliveryReset();LateCorpseReset();ActorCorpseLimit(); }
/* Script-visible identity of a live actor, for getaiarray/getaispeciesarray
 * (script_ai_arrays.c). 0 for anything that is not a live actor. */
extern "C" int ActorScriptIdentity(gentity_s *entity, int *team, const char **species)
{
    actor_lifetime_t *a=ActorForEntity(entity);
    if(!a || !a->initialized || !a->sentient || entity->s.eType!=ET_ACTOR)return 0;
    if(team)*team=a->sentient->team;
    if(species)*species=a->species?a->species->name:NULL;
    return 1;
}
#endif

#ifdef OL_SERVER
#include "persistent_ai.h"

/* Full room reset is allowed only after G_FreeEntity/ActorForget released
 * every owned tree, script string and thread. Never replace that with memset. */
static int resident_actor_zero(const void *data,size_t size) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<size;++i)if(p[i])return 0;
    return 1;
}
extern "C" int PersistentActorsReleased(void) {
    return resident_actor_zero(actors,sizeof(actors)) &&
        resident_actor_zero(corpses,sizeof(corpses)) &&
        !s_switchParams && !actorDamageFinishing && !actorDamageScaledTarget;
}
extern "C" int PersistentActorStaticsEmpty(void) {
    return PersistentActorsReleased() &&
        resident_actor_zero(actorSlotReleased,sizeof(actorSlotReleased)) &&
        resident_actor_zero(corpseSlotReleased,sizeof(corpseSlotReleased)) &&
        resident_actor_zero(residentFootstepTime,sizeof(residentFootstepTime)) &&
        resident_actor_zero(residentFootstepRadius,sizeof(residentFootstepRadius)) &&
        !actorDamageHandle && !actorKilledHandle && !s_goalEndUnroutable && !s_paceTrace;
}
extern "C" int PersistentActorStaticsReset(void) {
    if(!PersistentActorsReleased())return 0;
    ActorLifetimeReset();
    memset(residentFootstepTime,0,sizeof(residentFootstepTime));
    memset(residentFootstepRadius,0,sizeof(residentFootstepRadius));
    actorDamageHandle=actorKilledHandle=0;s_goalEndUnroutable=s_paceTrace=0;
    return PersistentActorStaticsEmpty();
}

#endif

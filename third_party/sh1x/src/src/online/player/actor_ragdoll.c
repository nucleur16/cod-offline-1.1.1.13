/* Actor ragdolls: impact launch, and keeping a started ragdoll one.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * <actor> startRagdollFromImpact(<hitloc>, <force>)   (C1's own method)
 *   C1 1.1.1.13 0x555740, entity method table 0xcd3190 entry 96. It accepts
 *   an actor or actor corpse only (eType 0x12 / 0x14, 0x5557a5..0x5557ad),
 *   needs two parameters, turns the hit location name into its index over
 *   the 20 hit location names (0x55585d..0x5558a7; an unknown name is index
 *   0), stores that index at entityState+5c and the force vector at +60..+68
 *   (0x555884..0x555897) and starts the ragdoll (0x555590: pos/apos trType
 *   1 -> 12, 5 -> 11, anything else -> 10 evaluated into trBase with zero
 *   delta).
 *
 *   The client is where the force acts. C1's entity lerp 0x4434ec, for a pos
 *   trType 10..12 with no ragdoll yet, creates one (0x4430d0); for an actor or
 *   actor corpse it passes nextState+5c and &nextState+60 (cent+0x140/+0x144,
 *   0x44316a..0x443188) to the ragdoll constructor 0x66e940, which keeps the
 *   force (+0x954) only when ragdoll.cfg maps that hit location to a bone
 *   (0x66e9c4..0x66ea18; "none" is unmapped). Everything else gets no impulse.
 *
 * <entity> launchRagdoll(<force>[, <hitloc>])          (T5, OpenBLOPS
 *   g_scr_main_mp.cpp GScr_RagdollLaunch)
 *   T5 queues an EV_PHYS_LAUNCH temp entity, but only for an entity that is
 *   already a ragdoll. C1's event table (0xcd81d8) has no EV_PHYS_LAUNCH: the
 *   impulse reaches a C1 ragdoll only through the impact words above. So on an
 *   actor or actor corpse this is startRagdollFromImpact with T5's argument
 *   order and T5's force converted to C1's units (below); with no hit
 *   location it uses torso_upper, because C1 applies no force without a
 *   mapped hit location. Like T5, it does nothing to an entity that is not a
 *   ragdoll yet. Other entity types have no C1 launch path and raise a script
 *   error.
 *
 * FORCE UNITS. The two engines read the vector differently.
 *   C1 (0x6732a8 -> 0x672d70, MEASURED): with a mapped hit location the
 *     ragdoll's bones start with linear VELOCITY, not an impulse: the bone
 *     the hit location names gets 0.2 * force, every other bone
 *     0.8 * force / (bones - 1) (constants 0xc84990 / 0xc84628); a hit
 *     location of none falls back to the animation's own velocity
 *     (0x672ad0) and the force is ignored.  IW4 (whose ragdoll C1 kept)
 *     sends a bullet death at 10 * max(damage, 300) = 3000 and more
 *     (IW4 animscripts/death.gsc doImmediateRagdollDeath).
 *   T5 (OpenBLOPS ragdoll_update.cpp Ragdoll_Launch / _LaunchUpdate) applies
 *     an IMPULSE of force * ragdoll_bullet_force (500): all of it on the
 *     hit-location bone, or force * 500 / bones on every bone when the hit
 *     location is none. T5's own bullet launch is about 27
 *     (zombie_death.gsc launch_ragdoll_based_on_damage_type, 60 * 0.45).
 *   Matching a bullet death across the two (C1 0.2 * 3000 = 600 u/s on the
 *     hit bone; T5 27 * 500 on a 20-mass torso, about 675) gives 125 C1 units
 *     per T5 unit for a hit-location launch. A whole-body T5 launch (no hit
 *     location, the Thundergun fling) spreads 500 * force / 12 over the
 *     12-bone C1 zombie ragdoll (ragdoll.cfg def 1, 138 mass, about 3.5 *
 *     force per bone); C1's spread bones get 0.8 / 11 of the force, so that
 *     is 48 C1 units per T5 unit. ragdollLaunchScale scales both.
 *   Before this conversion a Thundergun fling of 200-300 reached C1 as bone
 *     speeds of 15-60 u/s: the bodies dropped where they stood.
 *
 * KEEPING A RAGDOLL. Every actor origin producer ends in the retained
 * G_SetOrigin, which resets pos.trType to TR_STATIONARY, so a startRagdoll on
 * a dying actor (zombie_death.gsc waitForRagdoll, the thundergun fling) was
 * undone by the actor's next frame, before any snapshot carried it. T5's MP
 * actor re-applies it every frame after think (actor_mp.cpp:1493/:1504,
 * `trType = isRagdoll ? 14 : 1`); ActorRagdollHold/Restore bracket the
 * actor and actor-corpse frames in G_RunFrameForEntity the same way.
 *
 * traceActorEvents 1 (developer) logs every event an actor or actor
 * corpse queued since its previous frame, with the frame count, so a burst
 * that pushes an event out of the four-slot queue can be seen. */
#ifdef OL_SERVER
#include "q_shared.h"
#include "g_shared.h"
#include "entity.h"
#include "scr_vm.h"
#include "scr_vm_functions.h"
#include "cscr_stringlist.h"
#include "bg_public.h"
#include "cvar.h"
#include "qcommon_io.h"
#include "qcommon.h"
#include "actor_ragdoll.h"
#include <math.h>
#include <string.h>
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

extern int G_GetHitLocationIndexFromString(unsigned short name);
static cvar_t *launch_scale_dvar, *trace_ragdoll_dvar;
extern unsigned int __cdecl Scr_GetConstLowercaseString(unsigned int);

static int is_ragdoll_type(int type)
{
    return type >= TR_RAGDOLL && type <= TR_RAGDOLL_INTERPOLATE;
}

static int actor_body(const gentity_t *ent)
{
    return ent->s.eType == ET_ACTOR || ent->s.eType == ET_ACTOR_CORPSE;
}

/* C1 0x555590, one trajectory. */
static void start_ragdoll_trajectory(trajectory_t *tr)
{
    if (tr->trType == TR_INTERPOLATE) tr->trType = TR_RAGDOLL_INTERPOLATE;
    else if (tr->trType == TR_GRAVITY) tr->trType = TR_RAGDOLL_GRAVITY;
    else if (!is_ragdoll_type(tr->trType)) {
        float base[3];
        BG_EvaluateTrajectory(tr, level.time, base);
        VectorCopy(base, tr->trBase);
        VectorClear(tr->trDelta);
        tr->trType = TR_RAGDOLL;
    }
}

static void set_impact(gentity_t *ent, int hitloc, const float *force)
{
    if (hitloc <= HITLOC_NONE) hitloc = HITLOC_TORSO_UPR;
    ent->s.lerp.u.actor.ragdollHitLoc = hitloc;
    VectorCopy(force, ent->s.lerp.u.actor.ragdollForce);
}

static int read_force(unsigned index, float *force)
{
    Scr_GetVector(index, force);
    if (!isfinite(force[0]) || !isfinite(force[1]) || !isfinite(force[2])) {
        Scr_ParamError(index, "ragdoll force must be finite");
        return 0;
    }
    return 1;
}

void Scr_StartRagdollFromImpact(scr_entref_t ref)
{
    gentity_t *ent = VM_GetGEntityForEntRef(ref);
    float force[3];
    int hitloc;
    if (!ent) return;
    if (Scr_GetNumParam() < 2) {
        Scr_Error("GScr_StartRagdollFromImpact requires at least 2 parameters.");
        return;
    }
    if (!actor_body(ent)) {
        Scr_ObjectError("GScr_StartRagdollFromImpact must be called on an actor.");
        return;
    }
    hitloc = G_GetHitLocationIndexFromString(Scr_GetConstLowercaseString(0));
    if (!read_force(1, force)) return;
    set_impact(ent, hitloc, force);
    start_ragdoll_trajectory(&ent->s.lerp.pos);
    start_ragdoll_trajectory(&ent->s.lerp.apos);
}

void Scr_LaunchRagdoll(scr_entref_t ref)
{
    gentity_t *ent = VM_GetGEntityForEntRef(ref);
    float force[3];
    int hitloc = HITLOC_NONE;
    unsigned count;
    if (!ent) return;
    count = Scr_GetNumParam();
    if (count < 1 || count > 2) {
        Scr_Error("Usage: <entity> launchRagdoll( <force> [, <hitloc>] )");
        return;
    }
    if (!actor_body(ent)) {
        Scr_ObjectError(va("launchRagdoll: entity %i is not an actor or actor corpse; "
                           "C1 launches only those ragdolls", ent->s.number));
        return;
    }
    if (!read_force(0, force)) return;
    if (count > 1) hitloc = G_GetHitLocationIndexFromString(Scr_GetConstLowercaseString(1));
    /* T5: only a ragdoll is launched. */
    if (!is_ragdoll_type(ent->s.lerp.pos.trType)) return;
    if (!launch_scale_dvar)
        launch_scale_dvar = Cvar_RegisterFloat("ragdollLaunchScale", 1.0f, 0.0f, 20.0f, 0,
            "Multiplies the T5-to-C1 conversion of launchRagdoll forces (1 = calibrated)");
    if (!trace_ragdoll_dvar)
        trace_ragdoll_dvar = Cvar_RegisterBool("traceRagdollLaunch", qtrue, 0,
            "Log each launchRagdoll as it is published to C1");
    {
        float k = (hitloc > HITLOC_NONE ? 125.0f : 48.0f) *
                  (launch_scale_dvar ? launch_scale_dvar->value : 1.0f);
        float in[3];
        VectorCopy(force, in);
        force[0] *= k; force[1] *= k; force[2] *= k;
        set_impact(ent, hitloc, force);
        if (trace_ragdoll_dvar && trace_ragdoll_dvar->boolean)
            Com_Printf(CON_CHANNEL_SERVER,
                "CoDOL ragdoll launch: ent %d type %d trType %d T5 force (%.0f %.0f %.0f) -> C1 hitloc %d force (%.0f %.0f %.0f)\n",
                ent->s.number, ent->s.eType, ent->s.lerp.pos.trType, in[0], in[1], in[2],
                ent->s.lerp.u.actor.ragdollHitLoc, force[0], force[1], force[2]);
    }
}

void ActorRagdollHold(const gentity_t *ent, ragdoll_hold_t *hold)
{
    hold->pos = ent->s.lerp.pos.trType;
    hold->apos = ent->s.lerp.apos.trType;
}

void ActorRagdollRestore(gentity_t *ent, const ragdoll_hold_t *hold)
{
    if (!ent->r.inuse || !actor_body(ent)) return;
    if (!is_ragdoll_type(hold->pos)) {
        /* Never ragdolled: no impact words left over from a spawner's state. */
        if (ent->s.eType == ET_ACTOR && !is_ragdoll_type(ent->s.lerp.pos.trType)) {
            ent->s.lerp.u.actor.ragdollHitLoc = 0;
            VectorClear(ent->s.lerp.u.actor.ragdollForce);
        }
        return;
    }
    if (!is_ragdoll_type(ent->s.lerp.pos.trType)) ent->s.lerp.pos.trType = hold->pos;
    if (is_ragdoll_type(hold->apos) && !is_ragdoll_type(ent->s.lerp.apos.trType))
        ent->s.lerp.apos.trType = hold->apos;
}

/* ---- traceActorEvents ------------------------------------------- */
static cvar_t *trace_dvar;
static int trace_sequence[MAX_GENTITIES];
static int trace_use[MAX_GENTITIES];

void ActorTraceEvents(const gentity_t *ent)
{
    int n = ent->s.number, from, to, i;
    if (!com_developer || !com_developer->integer) return;
    if (!trace_dvar)
        trace_dvar = Cvar_RegisterBool("traceActorEvents", qfalse, 0,
            "Developer: log every event an actor or actor corpse queues, per frame");
    if (!trace_dvar || !trace_dvar->boolean || n < 0 || n >= MAX_GENTITIES) return;
    to = ent->s.eventSequence;
    if (trace_use[n] != ent->useCount + 1) {
        trace_use[n] = ent->useCount + 1;
        trace_sequence[n] = to;
        return;
    }
    from = trace_sequence[n];
    trace_sequence[n] = to;
    if (to == from) return;
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL actor events: ent %d type %d time %d: %d new%s\n",
        n, ent->s.eType, level.time, to - from, to - from > 4 ? " (OVERFLOW: oldest lost)" : "");
    for (i = from; i < to; ++i) {
        int slot = i & 3;
        int event, parm;
        memcpy(&event, &ent->s.events[slot], sizeof(event));
        memcpy(&parm, &ent->s.eventParms[slot], sizeof(parm));
        if (i < to - 4) continue;
        Com_Printf(CON_CHANNEL_SERVER, "CoDOL actor events:   seq %d event %d parm %d\n", i, event, parm);
    }
}
#endif

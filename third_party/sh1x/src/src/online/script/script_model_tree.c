#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Script-model animtrees. See script_model_tree.h for the C1 evidence.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "script_model_tree.h"
#include "script_model_rootmotion.h"
#include "actor_anim_journal.h"
#include "dog_animation.h"
#include "model_animation.h"
#include "entitystate_ext.h"
#include "playerstate_ext.h"
#include "entities.h"
#include "frame_builder.h"
#include "engine_state.h"
#include "vehicle_wheel_anim.h"
#include "scr_vm_functions.h"
#include "xassets/xanims.h"
#include "dobj.h"
#include "cmd.h"
#include "cscr_stringlist.h"
#include <stdlib.h>
#include <string.h>
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"
#include "scr_vm.h"

extern XAnim_s *Scr_GetAnims(unsigned int index);
extern void G_DObjUpdate(gentity_t *entity);
extern void XAnimClearTree(struct XAnimTree_s *tree);
extern unsigned OL_G_ModelName(int index);
extern int RefreshServerAssetNames(void);
extern char entity_seed_failure[160];

/* A map binds a handful: one lid per box spot (zm_asylum_sh authors five),
 * plus every flying helicopter for its rotors (an MP match can have the
 * attack helicopter, two little birds and care-package drops at once; a coop
 * mission a few movers). */
#define SCRIPT_MODEL_TREE_OWNERS 32

typedef struct {
    int used;
    unsigned entity;
    int useCount;
    unsigned server_tree;
    unsigned client_tree;
    struct XAnimTree_s tree;
} script_model_tree_t;

static script_model_tree_t owners[SCRIPT_MODEL_TREE_OWNERS];

static script_model_tree_t *owner_for(const gentity_t *e)
{
    unsigned i;
    if(!e) return NULL;
    for(i = 0; i < SCRIPT_MODEL_TREE_OWNERS; i++)
        if(owners[i].used && owners[i].entity == (unsigned)e->s.number &&
           owners[i].useCount == e->useCount) return &owners[i];
    return NULL;
}

static const char *model_name(const gentity_t *e)
{
    unsigned handle = e && e->model ? OL_G_ModelName(e->model) : 0;
    return handle ? SL_ConvertToString(handle) : "(no model)";
}

static int is_script_model(const gentity_t *e)
{
    return e && e->r.inuse && e->s.eType == ET_SCRIPTMOVER &&
           e->classname == scr_const.script_model && e->s.solid != SOLID_BMODEL &&
           e->model;
}

/* A helicopter or vehicle entity (spawnHelicopter / spawnVehicle) with a
 * model, whose wheels the RC producer does not already animate. C1 0x443540
 * gives eType 12 and 14 the realm-0 tree named by wire +094 exactly as it does
 * eType 6, and the snapshot animation-command executor 0x443e90 plays the
 * commands on it: IW4 _vehicle's build_drive rotor loop (bh_rotors) needs
 * nothing more. */
static int is_vehicle(const gentity_t *e)
{
    return e && e->r.inuse && (e->s.eType == ET_HELICOPTER || e->s.eType == ET_VEHICLE) &&
           e->model && !VehicleWheelAnimTree(e);
}

int ScriptModelTreeEligible(const gentity_t *e)
{
    return is_script_model(e) || is_vehicle(e);
}

struct XAnimTree_s *ScriptModelAnimTree(const gentity_t *e)
{
    script_model_tree_t *o = owner_for(e);
    return o && o->tree.anims ? &o->tree : NULL;
}

int ScriptModelBindAnimTree(gentity_t *e, unsigned server_tree, const char **why)
{
    static char reason[160];
    script_model_tree_t *o;
    XAnim_s *anims;
    unsigned client_tree = 0, i;
    DObj *obj;
    const char *tree_name;
    if(why) *why = "";
    unsigned identity = 0;
    int vehicle;
    if(!ScriptModelTreeEligible(e)) {
        if(why) *why = "only a script_model or a helicopter/vehicle with a model can carry an animtree";
        return 0;
    }
    vehicle = is_vehicle(e);
    /* A vehicle's +094 is chosen by its captured definition slot
     * (engine_state.c OverlayEngineVehiclePose), not by a
     * per-entity capture. */
    if(vehicle && (!EntityStateVehicleIdentity(&e->s, &identity) || !(identity >> 4) ||
                   (identity >> 4) >= 32)) {
        if(why) *why = "this vehicle has no captured vehicle definition slot";
        return 0;
    }
    o = owner_for(e);
    if(o) {
        if(o->server_tree == server_tree) return 1;
        if(why) *why = "this script_model is already bound to a different animtree";
        return 0;
    }
    if(!server_tree || server_tree > ServerAnimTreeCount() ||
       !(anims = Scr_GetAnims(server_tree)) || !anims->size) {
        if(why) *why = "the animation's animtree was never built on this server";
        return 0;
    }
    tree_name = ServerAnimTreeName(server_tree);
    if(!ClientAnimTreeForServerTree(server_tree, &client_tree) || !client_tree ||
       client_tree > MODEL_ANIM_CLIENT_TREES) {
        Com_sprintf(reason, sizeof(reason),
            "animtree '%s' has no 4-bit client tree id (no loaded animlist registers it)",
            tree_name ? tree_name : "?");
        if(why) *why = reason;
        return 0;
    }
    for(i = 0, o = NULL; i < SCRIPT_MODEL_TREE_OWNERS; i++)
        if(!owners[i].used) { o = &owners[i]; break; }
    if(!o) {
        if(why) *why = "every script-model animtree slot is in use";
        return 0;
    }
    memset(o, 0, sizeof(*o));
    o->entity = (unsigned)e->s.number;
    o->useCount = e->useCount;
    o->server_tree = server_tree;
    o->client_tree = client_tree;
    o->tree.anims = anims;
    o->used = 1;
    /* G_DObjUpdate asks ActorOwnedTree, which answers this tree. */
    G_DObjUpdate(e);
    obj = Com_GetServerDObj(e->s.number);
    if(!obj || DObjGetTree(obj) != &o->tree) {
        memset(o, 0, sizeof(*o));
        G_DObjUpdate(e);
        if(why) *why = "the server DObj could not be rebuilt on the animtree";
        return 0;
    }
    if(vehicle) {
        /* Wire +094 of every snapshot of this definition names the client
         * tree from now on (map-static, like the RC wheels), so C1 builds the
         * vehicle's DObj with it (0x44372b). Bound in the frame it spawned,
         * before any snapshot carried it. */
        SetVehicleDefinitionAnimTree(identity >> 4, client_tree);
        Com_Printf(CON_CHANNEL_SERVER,
            "CoDOL vehicle anim: ent %d '%s' (eType %d, definition %u) bound server tree %u '%s' (%u entries) / client tree %u\n",
            e->s.number, model_name(e), e->s.eType, identity >> 4, server_tree, tree_name ? tree_name : "?",
            anims->size, client_tree);
        return 1;
    }
    /* +094 names the client tree from now on, so C1 builds this entity's
     * DObj with it (0x4435bb); +060 = 0 keeps the random-phase arm 0x43fd0e
     * off. Commands then arrive through the journal. */
    if(!EntityStateModelAnimCaptured(&e->s, 0, client_tree)) {
        memset(o, 0, sizeof(*o));
        G_DObjUpdate(e);
        if(why) *why = "the entity-state animtree selector could not be captured";
        return 0;
    }
    ClearModelAnimation((unsigned)e->s.number);
    ((void)0);
    return 1;
}

unsigned ScriptModelTreeCaptureSources(dog_anim_capture_source_t *out, unsigned capacity)
{
    unsigned i, n = 0;
    for(i = 0; i < SCRIPT_MODEL_TREE_OWNERS && n < capacity; i++) {
        const script_model_tree_t *o = &owners[i];
        const gentity_t *e;
        DObj *obj;
        if(!o->used || o->entity >= MAX_GENTITIES) continue;
        e = &g_entities[o->entity];
        if(!e->r.inuse || e->useCount != o->useCount) continue;
        obj = Com_GetServerDObj((int)o->entity);
        if(!obj || obj->entnum != o->entity + 1 || DObjGetTree(obj) != &o->tree) continue;
        out[n].entity = o->entity;
        out[n].generation = ACTOR_INCARNATION(o->useCount);
        out[n].tree = &o->tree;
        n++;
    }
    return n;
}

void ResetScriptModelTree(unsigned entity)
{
    unsigned i;
    ResetScriptModelRootMotion(entity);
    for(i = 0; i < SCRIPT_MODEL_TREE_OWNERS; i++) {
        script_model_tree_t *o = &owners[i];
        if(!o->used || o->entity != entity) continue;
        /* G_FreeEntity has already cleared the DObj's tree (this one) before
         * its slot reset reaches here; a reinitialised slot without a free
         * still holds records, so release them. */
        if(o->tree.children) XAnimClearTree(&o->tree);
        memset(o, 0, sizeof(*o));
    }
}

void ResetScriptModelTrees(void)
{
    ResetScriptModelRootMotions();
    memset(owners, 0, sizeof(owners));
}

/* ---------------------------------------------------------------------- */

static int probe_field(const entity_state_t *wire, unsigned offset, unsigned *value)
{
    unsigned f;
    for(f = 0; f < ENTITY_FIELDS; ++f) {
        const entity_field_t *field = EntityField(wire->type, f);
        if(field && field->offset == offset) { *value = wire->field[f]; return 1; }
    }
    return 0;
}

static const char *entry_name(const XAnim_s *anims, unsigned ref)
{
    if(!anims || ref >= anims->size) return "?";
    if(!anims->entries[ref].numAnims && anims->entries[ref].u.parts &&
       anims->entries[ref].u.parts->name) return anims->entries[ref].u.parts->name;
    return ref ? "(blend)" : "(root)";
}

static void AnimModelProbe_f(void)
{
    static dog_anim_pose_t pose;
    static actor_anim_event_t events[ACTOR_ANIM_SNAPSHOT_LIMIT];
    unsigned i, k, bound = 0;
    if(!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "animmodelprobe needs developer 1\n");
        return;
    }
    RefreshServerAssetNames();
    Com_Printf(CON_CHANNEL_DONT_FILTER, "animmodelprobe: %u client trees:", ClientAnimTreeCount());
    for(i = 1; i <= ClientAnimTreeCount(); ++i)
        Com_Printf(CON_CHANNEL_DONT_FILTER, " %u=%s", i,
                   ClientAnimTreeName(i) ? ClientAnimTreeName(i) : "?");
    Com_Printf(CON_CHANNEL_DONT_FILTER, "\n");
    for(i = 0; i < SCRIPT_MODEL_TREE_OWNERS; i++) {
        script_model_tree_t *o = &owners[i];
        gentity_t *e;
        entity_state_t wire;
        unsigned v060 = 0, v094 = 0, v06c = 0, v064 = 0, n = 0;
        int result;
        if(!o->used || o->entity >= MAX_GENTITIES) continue;
        ++bound;
        e = &g_entities[o->entity];
        entity_seed_failure[0] = '\0';
        result = ProbeEntityWire(&e->s, 0, &wire);
        if(result == 1) {
            probe_field(&wire, 0x060, &v060);
            probe_field(&wire, 0x094, &v094);
            probe_field(&wire, 0x06c, &v06c);
            /* Character clone: client + 1 (codol_setCharacterClone). */
            probe_field(&wire, 0x064, &v064);
        }
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "  ent %u '%s' useCount %d%s server tree %u (%u entries) client tree %u%s -> wire %s +060=%u +094=%u +06c=%u\n",
            o->entity, model_name(e), e->useCount, e->r.inuse ? "" : " FREED", o->server_tree,
            o->tree.anims ? o->tree.anims->size : 0u, o->client_tree,
            (e->r.svFlags & SVF_NOCLIENT) ? " hidden" : "",
            result == 1 ? "published" : (result ? "REFUSED(common overlay)" : entity_seed_failure),
            v060, v094, v06c);
        Com_Printf(CON_CHANNEL_DONT_FILTER, "    +064=%u origin (%.1f %.1f %.1f) yaw %.1f\n",
            v064, e->r.currentOrigin[0], e->r.currentOrigin[1], e->r.currentOrigin[2],
            e->r.currentAngles[1]);
        /* Entities linked to this model: a clone's world guns. */
        for(k = 0; k < (unsigned)MAX_GENTITIES; ++k) {
            const gentity_t *c = &g_entities[k];
            unsigned info = 0;
            float lo[3], la[3];
            if(!c->r.inuse || !c->tagInfo || c->tagInfo->parent != e) continue;
            if(!EntityStateLink(&c->s, &info, lo, la)) info = 0;
            Com_Printf(CON_CHANNEL_DONT_FILTER,
                "    child %u weapon %d tag %s link 0x%08x%s\n", k,
                c->s.lerp.u.scriptMover.weaponIndex,
                c->tagInfo->name ? SL_ConvertToString(c->tagInfo->name) : "<none>", info,
                (c->r.svFlags & SVF_NOCLIENT) ? " hidden" : "");
        }
        for(k = 0; o->tree.anims && k < o->tree.anims->size; ++k)
            Com_Printf(CON_CHANNEL_DONT_FILTER, "    entry %u parent %u %s\n", k,
                       o->tree.anims->entries[k].parent, entry_name(o->tree.anims, k));
        if(!DogAnimCapture(&o->tree, o->entity, ACTOR_INCARNATION(o->useCount),
                                 (uint32_t)(level.time ? level.time : 50), &pose)) {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "    pose capture FAILED: %s\n",
                       actor_delivery_failure ? actor_delivery_failure : "?");
            continue;
        }
        for(k = 0; k < pose.count; ++k)
            Com_Printf(CON_CHANNEL_DONT_FILTER,
                "    node %u ref %u %s parent %u: weight %.2f goal %.2f blend %.2f rate %.2f time %.3f\n",
                k, pose.nodes[k].ref, entry_name(o->tree.anims, pose.nodes[k].ref), pose.nodes[k].parent,
                pose.nodes[k].current_weight, pose.nodes[k].goal_weight, pose.nodes[k].remaining_blend,
                pose.nodes[k].playback_rate, pose.nodes[k].fraction);
        if(!DogAnimBootstrap(&pose, events, ACTOR_ANIM_SNAPSHOT_LIMIT, &n))
            Com_Printf(CON_CHANNEL_DONT_FILTER, "    bootstrap FAILED: %s\n",
                       actor_delivery_failure ? actor_delivery_failure : "?");
        else
            Com_Printf(CON_CHANNEL_DONT_FILTER, "    bootstrap: %u commands\n", n);
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER, "animmodelprobe: %u bound script models\n", bound);
    /* Players' published camera links (cameraLinkTo), as the snapshot sees them. */
    for(i = 0; i < (unsigned)level.maxclients && i < MAX_CLIENTS; ++i) {
        const gentity_t *p = &g_entities[i];
        const ps_ext_t *ext;
        if(!p->r.inuse || !p->client || !(ext = PlayerStateExt(&p->client->ps)) || !ext->links_known)
            continue;
        if(!(ext->links.cameraFlags & 2u)) continue;
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "animmodelprobe: client %u camera ent %u tag slot %u flags 0x%x pm_type %d origin (%.1f %.1f %.1f)\n",
            i, ext->links.cameraEntNum, ext->links.cameraTagIndex, ext->links.cameraFlags,
            p->client->ps.pm_type, p->client->ps.origin[0], p->client->ps.origin[1], p->client->ps.origin[2]);
    }
}

void RegisterScriptModelTreeProbe(void)
{
    Cmd_AddCommand("animmodelprobe", AnimModelProbe_f);
}
#endif

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentScriptModelResourcesReleased(void) {
    for(unsigned i=0;i<SCRIPT_MODEL_TREE_OWNERS;++i)if(owners[i].used||owners[i].tree.children)return 0;
    return 1;
}

int PersistentScriptModelsEmpty(void){return resident_entity_zero(owners,sizeof(owners));}
#endif

#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Compass objectives at C1's 32 slots.
 *
 * C1's MSG_ReadDeltaPlayerstate reads a hard-coded 32 objective slots of
 * stride 0x24 (0x5a1f9c / 0x5a1fd6; player_arrays.h), and the IW4
 * scripts it runs allocate ids up to 31 (_gameobjects.gsc getNextObjID caps
 * at 31; every useObject/carryObject, airdrop crate and tank takes ids). The
 * retained IW3 builtins (_g_scr_main_mp.asm Scr_Objective_*) rejected any
 * index above 15 with a script error, which killed the calling thread: an
 * emergency airdrop in dom (6 flag ids + 8 crate ids) pushed the next crate
 * past 15 and left it half set up with no compass icon.
 *
 * level.objectives[16] (level+0x24, stride 0x1c) cannot grow in place, and
 * G_RunFrame copies exactly those 16 into ps->objective with the per-client
 * team filter. Slots 0..15 therefore stay in level.objectives, unchanged for
 * the retained copy; slots 16..31 live here and are filtered by the same rule
 * at snapshot time (FillHighObjectives). The builtins below are the
 * retained ones re-implemented over both halves with IW3's exact semantics
 * (state names, broadcast flag handling, integral positions, icon checks).
 */
#ifdef OL_SERVER
#include "scr_vm.h"
#include "g_shared.h"
#include "cscr_stringlist.h"
#include "script_objectives.h"
#include <string.h>

int G_MaterialIndex(const char *name);

static objective_t ol_high_objectives[OBJECTIVE_TOTAL - OBJECTIVE_RETAINED];
/* objective_player: the one client an objective is shown to, or -1 for the
 * team rule alone. Neither the retained objective_t (stride 0x1c, copied by
 * G_RunFrame) nor C1's wire slot has room for it, so it lives beside both
 * halves and the snapshot builder drops the slot for everyone else
 * (ObjectiveHiddenMask). */
static int objective_client[OBJECTIVE_TOTAL];

static void reset_objective_clients(void)
{
    unsigned i;
    for (i = 0; i < OBJECTIVE_TOTAL; ++i)
        objective_client[i] = -1;
}

void ResetObjectives(void)
{
    unsigned i;
    memset(ol_high_objectives, 0, sizeof(ol_high_objectives));
    for (i = 0; i < OBJECTIVE_TOTAL - OBJECTIVE_RETAINED; ++i)
        ol_high_objectives[i].entNum = OBJECTIVE_NO_ENTITY;
    reset_objective_clients();
}

static objective_t *objective_slot(unsigned index)
{
    return index < OBJECTIVE_RETAINED ? &level.objectives[index]
                                            : &ol_high_objectives[index - OBJECTIVE_RETAINED];
}

/* Reads parameter 0; a script error for an index C1 cannot carry. */
static objective_t *objective_argument_index(unsigned param, unsigned *out_index)
{
    int index = Scr_GetInt(param);
    if ((unsigned)index >= OBJECTIVE_TOTAL) {
        Scr_ParamError(param, va("index %i is an illegal objective index. Valid indexes are 0 to %i\n",
                                 index, OBJECTIVE_TOTAL - 1));
        return NULL;
    }
    if (out_index) *out_index = (unsigned)index;
    return objective_slot((unsigned)index);
}

static objective_t *objective_argument(unsigned param)
{
    return objective_argument_index(param, NULL);
}

/* An objective bound to an entity made it broadcast (svFlags 0x10); drop
 * that when the binding goes away, as every retained builtin does. */
static void release_entity(objective_t *objective)
{
    int entnum = objective->entNum;
    if (entnum != OBJECTIVE_NO_ENTITY) {
        if (entnum >= 0 && entnum < MAX_GENTITIES && g_entities[entnum].r.inuse)
            g_entities[entnum].r.svFlags &= ~0x10;
        objective->entNum = OBJECTIVE_NO_ENTITY;
    }
}

/* "empty" 0, "active" 1, "invisible" 2, "current" 4; -1 after the error. */
static int objective_state(unsigned param, int use_name)
{
    unsigned short name = (unsigned short)Scr_GetConstString(param);
    if (name == scr_const.empty) return OBJST_EMPTY;
    if (name == scr_const.invisible) return OBJST_INVISIBLE;
    if (name == scr_const.current) return OBJST_CURRENT;
    if (name == scr_const.active) return OBJST_ACTIVE;
    Scr_ParamError(param, va("Illegal objective state \"%s\". Valid states are \"empty\", \"invisible\", \"current\", \"active\"\n",
                             use_name ? Scr_GetString(param) : SL_ConvertToString(name)));
    return OBJST_EMPTY;
}

static void objective_position(objective_t *objective, unsigned param)
{
    unsigned axis;
    Scr_GetVector(param, objective->origin);
    for (axis = 0; axis < 3; ++axis) objective->origin[axis] = (float)(int)objective->origin[axis];
}

static void objective_icon(objective_t *objective, unsigned param)
{
    const char *name = Scr_GetString(param);
    unsigned i;
    for (i = 0; name[i]; ++i)
        if ((unsigned char)(name[i] - 0x20) > 0x5e)
            Scr_ParamError(param, va("Illegal character '%c'(ascii %i) in objective icon name: %s\n",
                                     name[i], (unsigned char)name[i], name));
    if (i > 0x3f)
        Scr_ParamError(param, va("Objective icon name is too long (> %i): %s\n", 0x3f, name));
    objective->icon = G_MaterialIndex(name);
}

void OL_Scr_Objective_Add(void)
{
    int count = Scr_GetNumParam();
    objective_t *objective;
    if (count < 2)
        Scr_Error("objective_add needs at least the first two parameters out of its parameter list of: index state [string] [position]\n");
    objective = objective_argument(0);
    if (!objective) return;
    release_entity(objective);
    objective->state = objective_state(1, 0);
    if (count > 2) {
        objective_position(objective, 2);
        objective->entNum = OBJECTIVE_NO_ENTITY;
        if (count > 3) objective_icon(objective, 3);
    }
    objective->teamNum = TEAM_FREE;
}

void OL_Scr_Objective_Delete(void)
{
    unsigned index = 0;
    objective_t *objective = objective_argument_index(0, &index);
    if (!objective) return;
    objective_client[index] = -1;
    release_entity(objective);
    memset(objective, 0, sizeof(*objective));
    objective->entNum = OBJECTIVE_NO_ENTITY;
}

void OL_Scr_Objective_State(void)
{
    objective_t *objective = objective_argument(0);
    if (!objective) return;
    objective->state = objective_state(1, 1);
    if (objective->state == OBJST_EMPTY || objective->state == OBJST_INVISIBLE)
        release_entity(objective);
}

void OL_Scr_Objective_Icon(void)
{
    objective_t *objective = objective_argument(0);
    if (objective) objective_icon(objective, 1);
}

void OL_Scr_Objective_Position(void)
{
    objective_t *objective = objective_argument(0);
    if (!objective) return;
    release_entity(objective);
    objective_position(objective, 1);
}

void OL_Scr_Objective_OnEntity(void)
{
    objective_t *objective = objective_argument(0);
    gentity_t *entity;
    if (!objective) return;
    release_entity(objective);
    entity = Scr_GetEntity(1);
    entity->r.svFlags |= 0x10;
    objective->entNum = entity->s.number;
}

void OL_Scr_Objective_Current(void)
{
    unsigned char current[OBJECTIVE_TOTAL];
    int count = Scr_GetNumParam(), i;
    memset(current, 0, sizeof(current));
    for (i = 0; i < count; ++i) {
        int index = Scr_GetInt(i);
        if ((unsigned)index >= OBJECTIVE_TOTAL) {
            Scr_ParamError(i, va("index %i is an illegal objective index. Valid indexes are 0 to %i\n",
                                 index, OBJECTIVE_TOTAL - 1));
            continue;
        }
        current[index] = 1;
    }
    for (i = 0; i < OBJECTIVE_TOTAL; ++i) {
        objective_t *objective = objective_slot((unsigned)i);
        if (current[i]) objective->state = OBJST_CURRENT;
        else if (objective->state == OBJST_CURRENT) objective->state = OBJST_ACTIVE;
    }
}

void Scr_Objective_Team(void)
{
    objective_t *objective = objective_argument(0);
    unsigned short team;
    if (!objective) return;
    team = (unsigned short)Scr_GetConstString(1);
    if (team == scr_const.allies) objective->teamNum = TEAM_BLUE; /* allies 2 */
    else if (team == scr_const.axis) objective->teamNum = TEAM_RED; /* axis 1 */
    else if (team == scr_const.none) objective->teamNum = TEAM_FREE;
    else Scr_ParamError(1, va("Illegal team string '%s'. Must be allies, axis, or none.", SL_ConvertToString(team)));
}

/* objective_player( index, clientNum ): IW4/IW5 show the objective to that
 * one client only (IW5 conf.gsc puts a dog tag's icon on the killer's
 * compass and screen). The team rule still applies on top. objective_add and
 * objective_delete clear the restriction. */
void Scr_Objective_Player(void)
{
    unsigned index = 0;
    int client;
    objective_t *objective = objective_argument_index(0, &index);
    if (!objective) return;
    client = Scr_GetInt(1);
    if (client < 0 || client >= MAX_CLIENTS) {
        Scr_ParamError(1, va("client number %i is not a client slot (0 to %i)\n", client, MAX_CLIENTS - 1));
        return;
    }
    objective_client[index] = client;
}

/* Slots restricted by objective_player to a client other than `client`. */
uint32_t ObjectiveHiddenMask(int client)
{
    uint32_t mask = 0;
    unsigned i;
    for (i = 0; i < OBJECTIVE_TOTAL; ++i)
        if (objective_client[i] >= 0 && objective_client[i] != client)
            mask |= (uint32_t)1u << i;
    return mask;
}

/* Used objective slots and the highest id in use, for limits. */
int ObjectivesInUse(unsigned *highest)
{
    unsigned i;
    int used = 0;
    if (highest) *highest = 0;
    for (i = 0; i < OBJECTIVE_TOTAL; ++i)
        if (objective_slot(i)->state != OBJST_EMPTY) {
            ++used;
            if (highest) *highest = i;
        }
    return used;
}

/* The G_RunFrame rule (_g_main_mp.asm G_RunFrame_280..290) for the slots it
 * does not copy: a used objective reaches a client whose team matches, or
 * every client when it has no team; anything else arrives with state 0. The
 * team is the one of the client the state belongs to (ps->clientNum), which
 * for a spectator following someone is the followed player's. */
void FillHighObjectives(int client, objective_t *out)
{
    int team = -1;
    unsigned i;
    if (client >= 0 && client < MAX_CLIENTS && g_entities[client].client)
        team = g_entities[client].client->sess.cs.team;
    for (i = 0; i < OBJECTIVE_TOTAL - OBJECTIVE_RETAINED; ++i) {
        const objective_t *objective = &ol_high_objectives[i];
        out[i] = *objective;
        if (objective->state != OBJST_EMPTY && objective->teamNum && objective->teamNum != team)
            out[i].state = OBJST_EMPTY;
    }
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailObjectivesEmpty(void){objective_t expected;memset(&expected,0,sizeof(expected));expected.entNum=OBJECTIVE_NO_ENTITY;
 for(unsigned i=0;i<OBJECTIVE_TOTAL-OBJECTIVE_RETAINED;++i)if(memcmp(&expected,&ol_high_objectives[i],sizeof(expected)))return 0;
 for(unsigned i=0;i<OBJECTIVE_TOTAL;++i)if(objective_client[i]!=-1)return 0;return 1;}
void PersistentTailObjectivesReset(void){ResetObjectives();}
#endif

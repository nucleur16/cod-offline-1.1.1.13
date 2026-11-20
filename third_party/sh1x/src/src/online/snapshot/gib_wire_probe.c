/* CoDOL zm gib wire probe (developer diagnostic, developer 1 only).
 *
 * gibprobe
 *     Builds the actor table exactly as a snapshot capture does
 *     (ProduceActorStates) and prints every record's base model and
 *     parts as wire slots with their names, then runs every live
 *     "zombie_gib_piece" script model through the entity snapshot path
 *     (ProbeEntityWire). A dismembered zombie swaps its body model and
 *     attaches leg/neck models (_zombiemode_gib.gsc); any of those the wire
 *     cannot carry fails the WHOLE actor table for every recipient, so this
 *     checks the gib states without a client.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "qcommon.h"
#include "cmd.h"
#include "cscr_stringlist.h"
#include <string.h>
#include "actors.h"
#include "entities.h"
#include "frame_builder.h"
#include "actor_state.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

extern int RefreshServerAssetNames(void);
extern unsigned OL_G_ModelName(int index);
extern char entity_seed_failure[160];
extern int G_FindConfigstringIndex(const char *name, int start, int max, int create, const char *errormsg);

/* actor_state.cpp record layout (CODOL_ACTOR_OFF_*). */
#define GIBPROBE_MAX_ACTORS 32 /* MAX_ACTOR_SLOTS, sentient.h */
#define GIBPROBE_OFF_ENTITY 0x04
#define GIBPROBE_OFF_MODEL 0x0c
#define GIBPROBE_OFF_PART_MODEL 0x14
#define GIBPROBE_OFF_PART_TAG 0x24

static uint32_t gibprobe_get(const actor_state_t *a, unsigned offset)
{
    uint32_t v;
    memcpy(&v, a->data + offset, sizeof(v));
    return v;
}

static const char *gibprobe_model_name(int index)
{
    unsigned handle = OL_G_ModelName(index);
    return handle ? SL_ConvertToString(handle) : "";
}

static void GibProbe_f(void)
{
    static actor_state_t actors[GIBPROBE_MAX_ACTORS];
    unsigned count = 0, i, p;
    int n, pieces = 0;

    if (!com_developer || !com_developer->integer) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "gibprobe needs developer 1\n");
        return;
    }
    RefreshServerAssetNames();

    if (!ProduceActorStates(actors, GIBPROBE_MAX_ACTORS, &count)) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "gibprobe: actor table REFUSED (an actor's model or part has no wire slot)\n");
    } else {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "gibprobe: actor table ok, %u records\n", count);
        for (i = 0; i < count; ++i) {
            unsigned entnum = gibprobe_get(&actors[i], GIBPROBE_OFF_ENTITY);
            const gentity_t *ent = entnum < MAX_GENTITIES ? &g_entities[entnum] : NULL;
            Com_Printf(CON_CHANNEL_DONT_FILTER, "  actor %u entity %u model wire %u (%s)",
                actors[i].number, entnum, gibprobe_get(&actors[i], GIBPROBE_OFF_MODEL),
                ent ? gibprobe_model_name(ent->model) : "?");
            for (p = 0; p < 4; ++p) {
                uint32_t model = gibprobe_get(&actors[i], GIBPROBE_OFF_PART_MODEL + 4 * p);
                if (!model) break;
                Com_Printf(CON_CHANNEL_DONT_FILTER, " | part %u wire %u tag %u (%s)", p, model,
                    gibprobe_get(&actors[i], GIBPROBE_OFF_PART_TAG + 4 * p),
                    ent && ent->attachModelNames[p] ? gibprobe_model_name(ent->attachModelNames[p]) : "?");
            }
            Com_Printf(CON_CHANNEL_DONT_FILTER, "\n");
        }
    }

    for (n = 0; n < MAX_GENTITIES; ++n) {
        gentity_t *ent = &g_entities[n];
        entity_state_t wire;
        int result;
        if (!ent->r.inuse || !ent->targetname ||
            strcmp(SL_ConvertToString(ent->targetname), "zombie_gib_piece"))
            continue;
        ++pieces;
        result = ProbeEntityWire(&ent->s, 0, &wire);
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "  piece %d eType %d model %d (%s) -> wire slot %d contents 0x%x weapon %d index %d: %s%s\n",
            n, ent->s.eType, ent->model, gibprobe_model_name(ent->model),
            ServerModelToWire(ent->model), ent->r.contents, ent->s.weapon, ent->s.index,
            result == 1 ? "published" : "REFUSED ",
            result == 1 ? "" : (result ? "(common overlay)" : entity_seed_failure));
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER, "gibprobe: %d live gib pieces\n", pieces);

    /* The effects the gib path plays: native id and both wire mappings
     * (playFx / playFxOnTag events carry one of them). */
    {
        static const char *const effects[] = {
            "misc/fx_zombie_bloodspurt_codo",
            "weapon/bullet/fx_flesh_gib_fatal_01_blue",
            "maps/zombie/zm_eye_lensflare_blue"
        };
        for (i = 0; i < sizeof(effects) / sizeof(effects[0]); ++i) {
            /* Lookup only: NativeEffectIndex would Scr_Error outside
             * a script for a name nothing precached. */
            int id = G_FindConfigstringIndex(effects[i], 0x63e, 100, 0, NULL);
            if (id <= 0) id = RegisterExtraServerEffect(effects[i], 0);
            Com_Printf(CON_CHANNEL_DONT_FILTER, "  effect %s: native %d wire %d byte %d\n", effects[i], id,
                id > 0 ? ServerEffectToWire((unsigned)id) : -1,
                id > 0 ? ServerByteEffectToWire((unsigned)id) : -1);
        }
    }
}

void RegisterGibProbe(void)
{
    Cmd_AddCommand("gibprobe", GibProbe_f);
}

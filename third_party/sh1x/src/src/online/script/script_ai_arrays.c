/* T5 actor arrays and player weapon parms for scripts.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * getaiarray([team, ...])            T5 SP 0x007f0970 (OpenBLOPS
 *                                    g_scr_main_mp.cpp GScr_GetAIArray_SP)
 *   Every live actor on any of the named teams ("axis", "allies", "neutral",
 *   "all"; no argument means all three), dogs excluded.
 * getaispeciesarray([team[, species]])   T5 SP 0x007f0b20
 *   One team (default all) and one species name (default "all"); dogs are
 *   included when asked for. T5's species are human, dog, zombie and
 *   zombie_dog; this server's actor species rows carry the same names
 *   (actor_scripts.cpp), and an unknown one is a parameter error.
 *   "Live" is T5's bIsAlive && !delayedDeath: health above zero.
 *
 * <player> getweaponmuzzlepoint() / getweaponforwarddir()
 *   T5 SP methods_3 entries 148/149 (handlers 0x005ba610 / 0x00510f50) fill a
 *   weaponParms with G_CalcMuzzlePoints and return muzzleTrace / forward.
 *   For a player that is IW3's CalcMuzzlePoints: forward from the gun angles
 *   (gclient fGunPitch/fGunYaw, view roll), muzzleTrace the player's view
 *   origin -- the same parms weapon_give.c builds for a thrown grenade.
 *   Players only, as T5's retail player path. */
#ifdef OL_SERVER
#include "q_shared.h"
#include "g_shared.h"
#include "entity.h"
#include "scr_vm.h"
#include "scr_vm_functions.h"
#include "cscr_stringlist.h"
#include <string.h>
#include "server.h"

extern int ActorScriptIdentity(gentity_t *entity, int *team, const char **species);

/* This server's sentient teams: 0 none (T5 neutral), 1 axis, 2 allies,
 * 3..6 team3..team6 (actor_lifecycle.cpp ActorTeamNames). */
#define AI_TEAMS_DEFAULT ((1u << 0) | (1u << 1) | (1u << 2))
static unsigned team_flag(const char *team, const char *function)
{
    if (!Q_stricmp(team, "axis")) return 1u << 1;
    if (!Q_stricmp(team, "allies")) return 1u << 2;
    if (!Q_stricmp(team, "neutral")) return 1u << 0;
    if (!Q_stricmp(team, "all")) return AI_TEAMS_DEFAULT;
    if (!Q_stricmp(team, "team3")) return 1u << 3;
    Scr_Error(va("unknown team '%s' in %s (should be axis, allies, or neutral)", team, function));
    return 0;
}

static int is_dog(const char *species)
{
    return species && (!strcmp(species, "dog") || !strcmp(species, "zombie_dog"));
}

static int known_species(const char *name)
{
    static const char *const names[] = { "human", "dog", "zombie", "zombie_dog" };
    unsigned i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!strcmp(name, names[i])) return 1;
    return 0;
}

static void add_actors(unsigned teams, const char *species, int dogs)
{
    int i;
    Scr_MakeArray();
    for (i = MAX_CLIENTS; i < level.num_entities; ++i) {
        gentity_t *ent = &g_entities[i];
        int team;
        const char *name = NULL;
        if (!ent->r.inuse || ent->s.eType != ET_ACTOR || ent->health <= 0) continue;
        if (!ActorScriptIdentity(ent, &team, &name)) continue;
        if (team < 0 || team > 31 || !(teams & (1u << team))) continue;
        if (!dogs && is_dog(name)) continue;
        if (species && (!name || strcmp(species, name))) continue;
        Scr_AddEntity(ent);
        Scr_AddArray();
    }
}

void Scr_GetAIArray(void)
{
    unsigned count = Scr_GetNumParam(), teams = 0, i;
    for (i = 0; i < count; ++i) teams |= team_flag(Scr_GetString(i), "getaiarray");
    if (!teams) teams = AI_TEAMS_DEFAULT;
    add_actors(teams, NULL, 0);
}

void Scr_GetAISpeciesArray(void)
{
    unsigned count = Scr_GetNumParam(), teams = AI_TEAMS_DEFAULT;
    const char *species = NULL;
    if (count) teams = team_flag(Scr_GetString(0), "getaiarray");
    if (count >= 2) {
        species = Scr_GetString(1);
        if (!strcmp(species, "all")) species = NULL;
        else if (!known_species(species)) {
            Scr_ParamError(1, va("unknown species '%s' (should be human, dog, or all)", species));
            return;
        }
    }
    add_actors(teams, species, 1);
}

static gentity_t *weapon_parms_player(scr_entref_t ref)
{
    if (ref.classnum) { Scr_ObjectError("not an entity"); return NULL; }
    if (ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client) {
        Scr_ObjectError(va("entity %i is not a player", ref.entnum));
        return NULL;
    }
    return &g_entities[ref.entnum];
}

void PlayerCmd_GetWeaponMuzzlePoint(scr_entref_t ref)
{
    gentity_t *ent = weapon_parms_player(ref);
    vec3_t origin;
    if (!ent) return;
    G_GetPlayerViewOrigin(&ent->client->ps, origin);
    Scr_AddVector(origin);
}

void PlayerCmd_GetWeaponForwardDir(scr_entref_t ref)
{
    gentity_t *ent = weapon_parms_player(ref);
    vec3_t angles, forward;
    if (!ent) return;
    angles[0] = ent->client->fGunPitch;
    angles[1] = ent->client->fGunYaw;
    angles[2] = ent->client->ps.viewangles[2];
    AngleVectors(angles, forward, NULL, NULL);
    Scr_AddVector(forward);
}
#endif

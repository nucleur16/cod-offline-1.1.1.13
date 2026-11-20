#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Original T5 aitype and species script loading/entry lifecycle.
 * Staged native implementation; no runtime script compilation or fake spawn.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
/* The retained headers declare C functions without linkage guards. */
extern "C" {
#include "actor_scripts.h"
#include "actor_lifecycle.h"
#include "sv_state.h"
#include "assets.h"
#include "g_shared.h"
#include "entity.h"
#include "cscr_stringlist.h"
#include "scr_vm.h"
#include "cm_public.h"
#include "qcommon.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
}
#include "dog_states.h"

/* --- the species table ---------------------------------------------------
 * See the long column note in actor_scripts.h for where each constant
 * was measured. Adding a species is a row plus its scripts; nothing in the
 * actor path may test a species name again. */
static const actor_species_t species_table[] = {
{
    /* name        */ "dog",
    /* animtree    */ "dog",
    /* treeSelector*/ 1,
    /* animlistMask*/ 0x02u,
    /* talkBit     */ 0x01u,
    /* category    */ "mp",
    /* classname   */ "actor_enemy_dog_mp",
    /* animscript  */ "maps/mp/animscripts/dog",
    /* perfectInfo */ 0,
    /* quadruped   */ 1,
    DogStateIndex, DogStateName, OL_DogStateApply
},
{
    /* The zm humanoid zombie.
     *
     * MEASURED 2026-09-20, C1.exe 0x40840b..0x40847a: the client's five actor
     * composition trees are, in bit order 0x01,0x02,0x04,0x08,0x10,
     *     generic_human, dog, generic_human, generic_human, zombie_dog
     * so serverinfo-2482 bit 0x08 -- the bit whose animlist is
     * animlists/common_zombie -- registers "generic_human" at tree selector 3.
     * That single bit does both halves of this species' binding:
     *   - it loads common_zombie.animlist (common_gametype_zm, 7,874 bytes),
     *     whose "=generic_human" group holds 220 anims including the whole
     *     ai_zombie_* set, which is what RegisterServerAnimListReferences
     *     resolves into the server's generic_human tree;
     *   - it registers the client composition tree this actor is composed
     *     against.
     * animtrees/generic_human.atr ships in common_mp (mounted for every asset
     * category); 389 of its 5,345 lines name ai_zombie_* nodes.
     *
     * Selector 4 / bit 0x10 is "zombie_dog", the hellhound, and needs no
     * animlist of its own (its 60 clips are in common_zombie.animlist too);
     * it is the third row below.
     *
     * stateApply is NULL: T5 animates humanoid AI from GSC through
     * #using_animtree %anim references, not from a native clip-state table
     * like bg_dog_animations_mp.cpp, and that scripted-animation surface
     * (setanim/setanimknob/animscripted builtins) does not exist here yet. */
    /* name        */ "zombie",
    /* animtree    */ "generic_human",
    /* treeSelector*/ 3,
    /* animlistMask*/ 0x08u,
    /* talkBit     */ 0x02u,
    /* category    */ "zm",
    /* classname   */ "actor_enemy_zombie_zm",
    /* animscript  */ "animscripts/zombie",
    /* perfectInfo */ 1,
    /* quadruped   */ 0,
    NULL, NULL, NULL
},
{
    /* The zm hellhound (T5 _zombiemode_ai_dogs). MEASURED 2026-10-07:
     *   - C1 0x40840b..0x40847a registers composition tree 4 as
     *     "zombie_dog" (0xc3f094) when serverinfo 2482 has bit 0x10; C1
     *     0x408310 loads animlists only for bits 0..3, so 0x10 is never
     *     cleared, and the "zombie_dog" tree itself is named by
     *     common_zombie.animlist (common_gametype_zm), which bit 0x08 loads.
     *   - animtrees/zombie_dog.atr ships in common_gametype_zm (rawfile,
     *     2278 bytes): T5's zombie dog tree with zombie_dog_* clips.
     *   - zombie_wolf, the model, ships in all three zm MAP zones; the maps'
     *     own "actor_zombie_dog" spawners name it. dog_bite_zm, the bite
     *     WeaponDef, ships in the same three zones.
     * Its classname deliberately differs from the maps' authored
     * actor_zombie_dog entity, exactly as actor_enemy_zombie_zm does: the
     * spawner is built at runtime with spawnactorspawner, so the map
     * entities stay inert.
     *
     * The animation backend is the MP dog's ported T5 state machine bound
     * against this tree (ZombieDogStateApply), and the animscripts are
     * the MP dog's set ported to the T5 top-level animscripts/zombie_dog_*. */
    /* name        */ "zombie_dog",
    /* animtree    */ "zombie_dog",
    /* treeSelector*/ 4,
    /* animlistMask*/ 0x10u,
    /* talkBit     */ 0x04u,
    /* category    */ "zm",
    /* classname   */ "actor_enemy_zombie_dog_zm",
    /* animscript  */ "animscripts/zombie_dog",
    /* perfectInfo */ 1,
    /* quadruped   */ 1,
    DogStateIndex, DogStateName, ZombieDogStateApply
},
{
    /* The coop (Spec Ops / campaign) human soldier.
     *
     * MEASURED 2026-10-07, docs/CAMPAIGN_MODE.md: every enemy the nine
     * coop addon MapEnts author is an IW4 SP actor_enemy_<faction>_<role>_so
     * spawner, and common_gametype_coop -- mounted for every coop match and for
     * no mp or zm match -- is the only zone carrying animlists/common_human,
     * the animlist C1 loads for serverinfo-2482 bit 0x01 and binds to
     * generic_human at composition tree selector 0 (C1.exe 0x40840b..0x40847a,
     * see the zombie row).  Its xanims (exposed_aim_5, run_lowready_F,
     * exposed_shoot_*, exposed_death*, ...) all name nodes of the generic_human
     * tree that common_mp ships.
     *
     * The classname is this server's own template, built at runtime with
     * spawnactorspawner() exactly like the zm zombie and the mp dog: the map's
     * authored spawners are many classnames (twenty-odd per map) whose IW4
     * aitype scripts ship in no CoDOL zone, so the coop mission script reads
     * each authored spawner's classname and hands the template the faction and
     * role before spawnactor.  No mp or zm map names this classname, and the
     * category column keeps its scripts out of every non-coop match. */
    /* name        */ "soldier",
    /* animtree    */ "generic_human",
    /* treeSelector*/ 0,
    /* animlistMask*/ 0x01u,
    /* talkBit     */ 0x04u,
    /* category    */ "coop",
    /* classname   */ "actor_enemy_coop_soldier",
    /* animscript  */ "animscripts/soldier",
    /* perfectInfo */ 0,
    /* quadruped   */ 0,
    NULL, NULL, NULL
},
{
    /* The coop (Spec Ops) dog: Survival's dog_reg / dog_splode (IW5
     * maps\_so_survival_ai spawn_dogs) and the actor_enemy_dog_so spawners
     * the coop addons author (coop_spec_mp_fav_tropical, every CoDOL
     * coop_survival_* zone, targetname dog_spawner).
     *
     * It is the mp dog's binding moved to the coop category: composition tree
     * selector 1 / serverinfo-2482 bit 0x02 is "dog" on C1 (see the zombie
     * row), and bit 0x02 loads animlists/common_dog, which
     * common_gametype_coop ships (docs/ACTOR_SPECIES_20260920.md: 55
     * german_shepherd xanims, german_sheperd_dog, dog_bite). animtrees/dog.atr
     * is common_mp's. The native T5 dog state machine is the mp dog's
     * (OL_DogStateApply); the animscripts are animscripts/coop_dog_*, the
     * category-clean zombie_dog set rebound to the dog tree, because the mp
     * set reaches multiplayer modules (see the category note in
     * ActorScriptsLoadMapTypes). The classname is the authored one: the
     * survival layer spawns through a template of it.
     *
     * Its name is "dog", the same as the mp row's: the aitype's self.type =
     * "dog", setTalkToSpecies("dog"), getAISpeciesArray(team, "dog") and the
     * native dog checks all name the species, and ActorSpeciesByName
     * answers the row of the mounted category (the categories never mix). */
    /* name        */ "dog",
    /* animtree    */ "dog",
    /* treeSelector*/ 1,
    /* animlistMask*/ 0x02u,
    /* talkBit     */ 0x01u,
    /* category    */ "coop",
    /* classname   */ "actor_enemy_dog_so",
    /* animscript  */ "animscripts/coop_dog",
    /* perfectInfo */ 1,
    /* quadruped   */ 1,
    DogStateIndex, DogStateName, OL_DogStateApply
},
{
    /* Spec Ops Survival's soldier: its own category since 2026-10-27
     * (common_gametype_survival on an unchanged mp_* level,
     * docs/CODOL_SURVIVAL_PLAN.md). The coop soldier's binding: tree
     * selector 0 / bit 0x01 (animlists/common_human, which
     * common_gametype_survival ships as 2.6.9.7's did) and the shared
     * animscripts/soldier_* set, which names no coop module. Same row name
     * and template classname as coop's; the lookups below answer the row of
     * the mounted category. */
    /* name        */ "soldier",
    /* animtree    */ "generic_human",
    /* treeSelector*/ 0,
    /* animlistMask*/ 0x01u,
    /* talkBit     */ 0x04u,
    /* category    */ "survival",
    /* classname   */ "actor_enemy_coop_soldier",
    /* animscript  */ "animscripts/soldier",
    /* perfectInfo */ 0,
    /* quadruped   */ 0,
    NULL, NULL, NULL
}
};
#define SPECIES_COUNT ((unsigned)(sizeof(species_table)/sizeof(species_table[0])))

unsigned ActorSpeciesCount(void) { return SPECIES_COUNT; }

/* Whether a species row serves a match of asset category `zone`. Survival
 * plays on the multiplayer script chain (MP weapons and classes, the MP dog
 * of maps/mp/animscripts/dog and its _dogs rules), so the multiplayer rows
 * serve it too, beside its own soldier row. */
static bool SpeciesServes(const actor_species_t *row, const char *zone)
{
    if (!row || !zone) return false;
    if (!strcmp(zone, row->category)) return true;
    return !strcmp(zone, "survival") && !strcmp(row->category, "mp");
}
const actor_species_t *ActorSpecies(unsigned index)
{
    return index < SPECIES_COUNT ? &species_table[index] : NULL;
}
/* Two rows may share a name across categories (the mp and coop "dog"): the
 * row of the mounted asset category wins, else the first row of that name. */
const actor_species_t *ActorSpeciesByName(const char *name)
{
    if (!name) return NULL;
    const char *zone = ServerZoneType();
    const actor_species_t *first = NULL;
    for (unsigned i = 0; i < SPECIES_COUNT; ++i) {
        if (strcmp(name, species_table[i].name)) continue;
        if (zone && SpeciesServes(&species_table[i], zone)) return &species_table[i];
        if (!first) first = &species_table[i];
    }
    return first;
}
/* Category-aware like ByName: coop and survival share their template
 * classnames. */
const actor_species_t *ActorSpeciesByClassname(const char *classname)
{
    if (!classname) return NULL;
    const char *zone = ServerZoneType();
    const actor_species_t *first = NULL;
    for (unsigned i = 0; i < SPECIES_COUNT; ++i) {
        if (strcmp(classname, species_table[i].classname)) continue;
        if (zone && SpeciesServes(&species_table[i], zone)) return &species_table[i];
        if (!first) first = &species_table[i];
    }
    return first;
}
unsigned ActorSpeciesAnimListMask(const char *category)
{
    unsigned mask = 0;
    for (unsigned i = 0; i < SPECIES_COUNT; ++i)
        if (!category || SpeciesServes(&species_table[i], category))
            mask |= species_table[i].animlistMask;
    return mask;
}

/* --- authored aitypes (IW4 SP's aitype contract) --------------------------
 * IW4 SP binds every authored spawner classname actor_<name> to its own
 * aitype\<name>.gsc: aitype main() sets the actor's gameplay fields and calls
 * its character\<name>::main() for the body and head, precache() precaches
 * them at level load, spawner() runs on the spawner. The coop addon MapEnts
 * author IW4 SP classnames (actor_enemy_favela_AR_so,
 * actor_ally_hero_ghost_socom_so, ...; docs/CAMPAIGN_MODE.md section 3),
 * so in a coop match an actor_ classname whose aitype script exists is a
 * soldier of that classname: the species row decides the animtree and the
 * animscripts, the aitype file decides the actor. The addons spell the same
 * classname in different case; the aitype file is the lower-case name, as IW4
 * ships it. A classname with no aitype script stays an inert spawner. The coop
 * dog's own classname (actor_enemy_dog_so) is a species row of its own, so it
 * is never taken for a soldier. The species row's own template
 * classname (actor_enemy_coop_soldier) is unchanged. */
static const struct { const char *category; const char *species; } authored_families[] = {
    { "coop", "soldier" },
    { "survival", "soldier" },
};
#define AUTHORED_FAMILY_COUNT ((unsigned)(sizeof(authored_families)/sizeof(authored_families[0])))

/* The species an authored actor_ classname of the mounted category belongs to,
 * or NULL. Says nothing about whether its aitype script exists. */
static const actor_species_t *AuthoredSpecies(const char *classname)
{
    if (!classname || Q_stricmpn(classname, "actor_", 6) || !classname[6]) return NULL;
    const char *zone = ServerZoneType();
    if (!zone) return NULL;
    for (unsigned i = 0; i < AUTHORED_FAMILY_COUNT; ++i)
        if (!strcmp(zone, authored_families[i].category))
            return ActorSpeciesByName(authored_families[i].species);
    return NULL;
}

struct actor_script_type {
    char classname[64];
    int main, precache, spawner;
    unsigned species;
    bool loaded, precached;
};
/* A coop map authors up to a dozen distinct classnames (the killhouse training
 * addon: twelve) besides the species templates. */
#define ACTOR_TYPE_SLOTS 64
static actor_script_type types[ACTOR_TYPE_SLOTS];
/* One animscript set per species, loaded at most once per map. */
static actor_script_table_t speciesScripts[SPECIES_COUNT];
static bool speciesLoaded[SPECIES_COUNT];

static int LoadMain(const char *file)
{
    if (!Scr_LoadScript(file)) return 0;
    return Scr_GetFunctionHandle(file, "main");
}

void ActorScriptsReset(void)
{
    memset(types, 0, sizeof(types));
    memset(speciesScripts, 0, sizeof(speciesScripts));
    memset(speciesLoaded, 0, sizeof(speciesLoaded));
}

/* Loads <animscript>_init .. <animscript>_jump for one species. All nine or
 * none: a half-bound set would leave ActorSwitchScript with a zero
 * handle for exactly the transition that needs it. */
static int LoadSpeciesScripts(unsigned index)
{
    if (index >= SPECIES_COUNT) return 0;
    if (speciesLoaded[index]) return 1;
    static const char *const suffix[] = {
        "_init", "_stop", "_move", "_combat", "_death",
        "_pain", "_flashed", "_turn", "_jump"
    };
    const char *prefix = species_table[index].animscript;
    size_t prefixLength = strlen(prefix);
    actor_script_table_t pending = {};
    int *slot[] = {
        &pending.init, &pending.stop, &pending.move, &pending.combat,
        &pending.death, &pending.pain, &pending.flashed, &pending.turn,
        &pending.jump
    };
    for (unsigned i = 0; i < 9; ++i) {
        char file[96];
        if (prefixLength + strlen(suffix[i]) >= sizeof(file)) return 0;
        memcpy(file, prefix, prefixLength);
        strcpy(file + prefixLength, suffix[i]);
        *slot[i] = LoadMain(file);
        if (!*slot[i]) return 0;
    }
    speciesScripts[index] = pending;
    speciesLoaded[index] = true;
    return 1;
}

/* A species row's own template classname, or an authored classname of the
 * mounted category (AuthoredSpecies). Returns 1 loaded, 0 failed, and -1 when
 * an authored classname has no aitype script (not an error: its spawners stay
 * inert, as every actor_ entity did before). */
static int LoadType(const char *classname)
{
    if (!classname || Q_stricmpn(classname, "actor_", 6) || !classname[6] ||
        strlen(classname) >= sizeof(types[0].classname)) return 0;
    /* aitype is a file identifier, not a free-form script path. */
    for (const char *c = classname + 6; *c; ++c)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
              (*c >= '0' && *c <= '9') || *c == '_')) return 0;
    /* Only a classname this build has a species for: an unknown actor_* must
     * not be given some other species' animscripts. */
    const actor_species_t *row = ActorSpeciesByClassname(classname);
    bool authored = false;
    if (!row) { row = AuthoredSpecies(classname); authored = true; }
    if (!row) return 0;
    unsigned index = (unsigned)(row - species_table);
    actor_script_type *slot = NULL;
    for (unsigned i = 0; i < ACTOR_TYPE_SLOTS; ++i) {
        if (types[i].loaded && !Q_stricmp(types[i].classname, classname)) return 1;
        if (!slot && !types[i].loaded) slot = &types[i];
    }
    if (!slot) {
        Com_PrintWarning(CON_CHANNEL_SERVER, "CoDOL actor: no free aitype slot for '%s' (%d in use)\n",
                         classname, ACTOR_TYPE_SLOTS);
        return 0;
    }
    if (!LoadSpeciesScripts(index)) return 0;
    char file[80];
    snprintf(file, sizeof(file), "aitype/%s", classname + 6);
    Q_strlwr(file);
    actor_script_type pending = {};
    pending.main = LoadMain(file);
    if (!pending.main) return authored ? -1 : 0;
    pending.precache = Scr_GetFunctionHandle(file, "precache");
    pending.spawner = Scr_GetFunctionHandle(file, "spawner");
    if (!pending.precache || !pending.spawner) return 0;
    strcpy(pending.classname, classname);
    pending.species = index;
    pending.loaded = true;
    *slot = pending;
    return 1;
}

int ActorScriptsLoadType(const char *classname)
{
    return LoadType(classname) == 1;
}

/* The retained tree declares COM_Parse but never defines it; the live parser
 * is Com_Parse, which returns a parseInfo_t whose token is what we want. */
static const char *actor_parse(char **cursor)
{
    const char *c = *cursor;
    parseInfo_t *info = Com_Parse(&c);
    *cursor = (char *)c;
    return info ? info->token : "";
}

/* Copies the server animtree registry's names out of the compiler objects
 * before Scr_EndLoadAnimTrees recycles them; see the long note above
 * ServerAnimTreeName in model_animation.c.  This is the last
 * script-loading step of GScr_LoadScripts, so every #using_animtree in the
 * compiled set has already registered its slot by the time the wrapper below
 * takes the snapshot. */
extern "C" void SnapshotServerAnimTreeNames(void);

static void ActorScriptsLoadMapTypes(void)
{
    // Read a private cursor; never advance the spawn parser's global cursor.
    char *cursor = CM_EntityString();
    if (!cursor) return;
    static char missing[32][64];
    unsigned missingCount = 0;
    while (cursor) {
        const char *token = actor_parse(&cursor);
        if (!token[0]) break;
        if (strcmp(token, "{")) {
            Com_Error(ERR_DROP, "actor preload: malformed map entity block");
            return;
        }
        char classname[64] = "";
        while (cursor) {
            token = actor_parse(&cursor);
            if (!strcmp(token, "}")) break;
            if (!token[0]) { Com_Error(ERR_DROP, "actor preload: truncated entity"); return; }
            bool isClassname = !strcmp(token, "classname");
            token = actor_parse(&cursor);
            if (!cursor) { Com_Error(ERR_DROP, "actor preload: missing entity value"); return; }
            if (isClassname) Q_strncpyz(classname, token, sizeof(classname));
        }
        // A map that authors a spawner for a species this build knows must get
        // that species' own aitype and animscripts. Any other actor_ classname
        // is left alone rather than given a species implementation guessed
        // from a substring in its name.
        if (ActorSpeciesByClassname(classname) &&
            !ActorScriptsLoadType(classname)) {
            Com_Error(ERR_DROP, "actor preload: incomplete aitype or animscripts for '%s'",
                      classname);
            return;
        }
        // An authored classname (AuthoredSpecies): its aitype is loaded here and
        // precached at level init the IW4 way, so spawnactor on it, or on a
        // template of the same classname, runs aitype/<name>::main(). One with
        // no aitype script is reported once and left inert; one whose aitype
        // exists but does not load is the fatal case above.
        if (classname[0] && !ActorSpeciesByClassname(classname) && AuthoredSpecies(classname)) {
            int status = LoadType(classname);
            if (status == 0) {
                Com_Error(ERR_DROP, "actor preload: incomplete aitype or animscripts for '%s'",
                          classname);
                return;
            }
            if (status < 0) {
                bool seen = false;
                for (unsigned i = 0; i < missingCount; ++i)
                    if (!Q_stricmp(missing[i], classname)) { seen = true; break; }
                if (!seen && missingCount < 32) {
                    Q_strncpyz(missing[missingCount++], classname, sizeof(missing[0]));
                    Com_Printf(CON_CHANNEL_SERVER,
                        "CoDOL actor: authored '%s' has no aitype script; its spawners stay inert\n",
                        classname);
                }
            }
        }
    }
    /* Survival authors its enemies in no map entity string: it plays on
     * unchanged mp_* levels (docs/CODOL_SURVIVAL_PLAN.md). Its AI type table
     * names each type's classname (mp/survival_waves.csv rows 100..199,
     * column 4; IW5's survival_ai table), so those aitypes are loaded and
     * precached here the way an authored spawner's are. The table is read
     * from the mounted zones, else loose from ol_scriptPath. */
    {
        const char *zoneNow = ServerZoneType();
        if (zoneNow && !strcmp(zoneNow, "survival")) {
            static const char *const tableName = "mp/survival_waves.csv";
            const engine_table_t *table = NULL;
            int status = LoadEngineStringTable(tableName, &table);
            if (status != 1) {
                cvar_t *root = Cvar_FindVar("scriptPath");
                if (root && *root->string)
                    status = LoadReferenceStringTable(root->string, tableName, &table);
            }
            unsigned loaded = 0, skipped = 0;
            /* Only the types this level's waves name (user direction
             * 2026-10-27: survival loads no more than it needs). The level's
             * table is the one maps/mp/gametypes/_survival.gsc picks:
             * mp/survival_waves_<level without mp_>.csv, else
             * mp/survival_waves_tier_1.csv, else the type table itself; the
             * refs are its squad type, specials and bosses (columns 3, 5, 7,
             * 8 of rows 0..99). An unreadable wave table loads every type. */
            char used[64][32];
            unsigned usedCount = 0;
            int haveUsed = 0;
            {
                const char *mapName = ServerMapName();
                char waveName[96];
                const engine_table_t *waves = NULL;
                int waveStatus = 0;
                if (mapName && *mapName) {
                    Com_sprintf(waveName, sizeof(waveName), "mp/survival_waves_%s.csv",
                        Q_stricmpn(mapName, "mp_", 3) ? mapName : mapName + 3);
                    waveStatus = LoadEngineStringTable(waveName, &waves);
                }
                if (waveStatus != 1)
                    waveStatus = LoadEngineStringTable("mp/survival_waves_tier_1.csv", &waves);
                if (waveStatus != 1) { waves = table; waveStatus = status; }
                if (waveStatus == 1 && waves && waves->values && waves->columnCount > 3) {
                    static const int columns[4] = { 3, 5, 7, 8 };
                    haveUsed = 1;
                    for (int row = 0; row < waves->rowCount; ++row) {
                        const char *const *cells = waves->values + (size_t)row * (size_t)waves->columnCount;
                        if (!cells[0] || cells[0][0] < '0' || cells[0][0] > '9') continue;
                        if (atoi(cells[0]) >= 100) continue;
                        for (int c = 0; c < 4; ++c) {
                            if (columns[c] >= waves->columnCount || !cells[columns[c]]) continue;
                            const char *p = cells[columns[c]];
                            while (*p) {
                                while (*p == ' ') ++p;
                                size_t len = 0;
                                while (p[len] && p[len] != ' ') ++len;
                                if (len && len < sizeof(used[0])) {
                                    unsigned k = 0;
                                    for (; k < usedCount; ++k)
                                        if (strlen(used[k]) == len && !Q_stricmpn(used[k], p, (int)len)) break;
                                    if (k == usedCount && usedCount < 64) {
                                        memcpy(used[usedCount], p, len); used[usedCount][len] = 0; ++usedCount;
                                    }
                                }
                                p += len;
                            }
                        }
                    }
                }
            }
            if (status == 1 && table && table->values && table->columnCount > 4) {
                for (int row = 0; row < table->rowCount; ++row) {
                    const char *const *cells = table->values + (size_t)row * (size_t)table->columnCount;
                    int index = cells[0] ? atoi(cells[0]) : 0;
                    const char *cls = cells[4];
                    if (index < 100 || index >= 200 || !cls || Q_stricmpn(cls, "actor_", 6)) continue;
                    if (haveUsed) {
                        unsigned k = 0;
                        for (; k < usedCount; ++k)
                            if (cells[1] && !Q_stricmp(used[k], cells[1])) break;
                        if (k == usedCount) { ++skipped; continue; }
                    }
                    int typeStatus = ActorSpeciesByClassname(cls) ? (ActorScriptsLoadType(cls) ? 1 : 0)
                                                                        : LoadType(cls);
                    if (typeStatus == 0) {
                        Com_Error(ERR_DROP, "actor preload: incomplete aitype or animscripts for survival type '%s'", cls);
                        return;
                    }
                    if (typeStatus > 0) ++loaded;
                    else Com_Printf(CON_CHANNEL_SERVER,
                        "CoDOL actor: survival type '%s' has no aitype script; it stays unspawnable\n", cls);
                }
            }
            Com_Printf(CON_CHANNEL_SERVER, "CoDOL actor: %u survival aitypes from %s%s (%u types this level's waves never name skipped)\n",
                       loaded, tableName, status == 1 ? "" : " (table not found)", skipped);
        }
    }
    /* Species preload. A species' aitype is loaded on every map of its asset
     * category, because the feature that spawns it (the dogs killstreak, the
     * zm zombie spawner) builds its own spawner through spawnactorspawner and
     * no CoDOL map authors one in its entity string (T5's maps did). A map
     * that does author one is still the fatal case above, because there the
     * missing scripts would leave a dead entity.
     *
     * THE CATEGORY COLUMN IS LOAD-BEARING. Before it existed, the multiplayer
     * dog aitype was preloaded in every category: aitype/enemy_dog_mp and the
     * nine maps/mp/animscripts/dog_* modules are multiplayer modules
     * (zm and coop have their own animscripts/ sets), and their
     * transitive file::function references reach maps/mp/gametypes/_spawnlogic,
     * whose maps\mp\gametypes\_callbacksetup::AbortLevel() does not exist in a
     * category _callbacksetup -- a fatal compile error at Game Initialization.
     * MEASURED 2026-09-20 on zm_prototype_sh: archives and configstrings
     * mounted correctly as zm, then "unknown function ... _spawnlogic.gsc line
     * 49" and "Server fatal crashed"
     * (bin/home/main/qconsole.zm-crash-20260920.log).
     *
     * The gate is the asset category, not the map name, because that is what
     * decides the script set (a coop match runs an mp_* level zone). */
    const char *zone = ServerZoneType();
    if (!zone) return;
    for (unsigned i = 0; i < SPECIES_COUNT; ++i) {
        const actor_species_t *row = &species_table[i];
        if (!SpeciesServes(row, zone)) {
            ((void)0);
            continue;
        }
        /* Asked of the template classname itself, not of the species: an
         * authored classname above may already have loaded this species'
         * animscripts without its template aitype (LoadType answers 1 at once
         * for a type already loaded). */
        if (!ActorScriptsLoadType(row->classname))
            Com_PrintWarning(CON_CHANNEL_SERVER,
                "CoDOL actor: aitype/%s or the %s_* animscripts did not load; "
                "the '%s' species is unavailable on this map\n",
                row->classname + 6, row->animscript, row->name);
    }
}

void OL_ActorScriptsLoadMapTypes(void)
{
    ActorScriptsLoadMapTypes();
    /* Unconditional, and after every return path above: the snapshot is not
     * species-specific, and the window in which the compiler's tree-name
     * objects are readable closes for good at BG_LoadAnim's
     * Scr_EndLoadAnimTrees. */
    SnapshotServerAnimTreeNames();
}

static actor_script_type *FindType(gentity_s *entity)
{
    if (!entity || !entity->r.inuse || !entity->classname) return NULL;
    const char *name = SL_ConvertToString(entity->classname);
    for (unsigned i = 0; i < ACTOR_TYPE_SLOTS; ++i)
        if (types[i].loaded && !Q_stricmp(types[i].classname, name)) return &types[i];
    return NULL;
}

int ActorScriptsPrecache(void)
{
    bool fresh[ACTOR_TYPE_SLOTS] = {};
    bool any = false;
    for (unsigned i = 0; i < ACTOR_TYPE_SLOTS; ++i) {
        actor_script_type *type = &types[i];
        if (!type->loaded || type->precached) continue;
        /* Called in level initialization while assets can be precached, before
         * any aitype main/spawner executes, as T5 FinishSpawningAll requires. */
        type->precached = true;
        fresh[i] = any = true;
        short thread = Scr_ExecThread(type->precache, 0);
        Scr_FreeThread(thread);
    }
    /* T5 FinishSpawningAll's other half: the map's own spawners were spawned
     * (G_SpawnEntitiesFromString) before their aitypes were precached, so
     * their aitype spawner() runs now, once, in map order. A spawner whose
     * type was already precached (a fast restart) ran it while spawning. */
    if (any) {
        for (int n = 0; n < level.num_entities; ++n) {
            gentity_s *entity = &g_entities[n];
            if (!entity->r.inuse || entity->s.eType != ET_ACTOR_SPAWNER) continue;
            actor_script_type *type = FindType(entity);
            if (type && fresh[type - types]) ActorScriptsSpawner(entity);
        }
    }
    return 1;
}

/* A map spawner whose aitype is loaded but not yet precached: its spawner()
 * is run by ActorScriptsPrecache. */
int ActorScriptsSpawnerPending(gentity_s *entity)
{
    actor_script_type *type = FindType(entity);
    return type && !type->precached;
}

/* True when this spawner's aitype scripts are loaded and precached, i.e. when
 * spawnactor on it can actually produce an initialized actor. */
int ActorScriptsSpawnerReady(gentity_s *entity)
{
    actor_script_type *type = FindType(entity);
    return type && type->precached && speciesLoaded[type->species] &&
        entity->s.eType == ET_ACTOR_SPAWNER;
}

int ActorScriptsSpawner(gentity_s *entity)
{
    actor_script_type *type = FindType(entity);
    if (!type || !type->precached || entity->s.eType != ET_ACTOR_SPAWNER) return 0;
    unsigned generation = (unsigned)entity->useCount;
    short thread = Scr_ExecEntThread(entity, type->spawner, 0);
    Scr_FreeThread(thread);
    return entity->r.inuse && (unsigned)entity->useCount == generation &&
        entity->s.eType == ET_ACTOR_SPAWNER;
}

/* Each stage names itself: a spawnactor that yields `undefined` must say why
 * in the log instead of leaving the calling script to dereference it. */
#define STAGE_WARN(...) Com_PrintWarning(CON_CHANNEL_SERVER, "CoDOL actor: actor init: " __VA_ARGS__)
int ActorScriptsInitialize(gentity_s *entity)
{
    actor_script_type *type = FindType(entity);
    const char *cls = entity && entity->classname ? SL_ConvertToString(entity->classname) : "?";
    if (!type) { STAGE_WARN("no loaded aitype for classname '%s'\n", cls); return 0; }
    if (!type->precached) { STAGE_WARN("aitype '%s' was never precached (level init order)\n", cls); return 0; }
    const actor_species_t *row = &species_table[type->species];
    if (!speciesLoaded[type->species]) { STAGE_WARN("%s_* animscripts are not loaded\n", row->animscript); return 0; }
    if (!ActorForEntity(entity)) { STAGE_WARN("ent %d has no actor record\n", entity->s.number); return 0; }
    if (!ActorRunTypeMain(entity, type->main)) {
        STAGE_WARN("aitype/%s::main() failed or deleted the actor\n", cls + 6); return 0;
    }
    if (!ActorPrepareAnimation(entity)) return 0; /* names its own cause */
    if (!ActorInitScripts(entity, &speciesScripts[type->species])) {
        STAGE_WARN("%s_init::main() failed, or the actor's type/species was not set to \"%s\" by aitype main\n",
                   row->animscript, row->name); return 0;
    }
    if (!ActorFinishInitialization(entity, row->treeSelector)) {
        STAGE_WARN("server DObj for ent %d does not own the '%s' animtree\n",
                   entity->s.number, row->animtree); return 0;
    }
    return 1;
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailActorScriptsEmpty(void){return resident_tail_zero(types,sizeof(types))&&resident_tail_zero(speciesScripts,sizeof(speciesScripts))&&resident_tail_zero(speciesLoaded,sizeof(speciesLoaded));}
void PersistentTailActorScriptsReset(void){ActorScriptsReset();}
#endif

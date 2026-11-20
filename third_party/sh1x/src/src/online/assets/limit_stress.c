/* Capacity report and stress test for the limits widened to C1's values
 * (developer diagnostic; docs: WEAPON_TABLE_20260915.md, the
 * script_objectives.c and sv_assets.c comments).
 *
 *   limits                    usage of every widened table
 *   limitstress models <n> [zone listing]
 *                                   register up to n more xmodels through
 *                                   the runtime outfit path, past the old
 *                                   511 native slots
 *   limitstress hints <n>     allocate n more hint strings, past 32
 *   limitstress weapons <n> [zone listing]
 *                                   register up to n more mounted stock
 *                                   weapons until the 128-entry ammo/clip
 *                                   pools refuse one (the gate, not an
 *                                   overrun)
 *
 * Each stress mode verifies what it made: a model slot must name the model
 * and translate to a C1 wire slot, a hint string must read back from the
 * C1 bank, the pools must never pass 128. */
#ifdef OL_SERVER
#include "server.h"
#include "g_shared.h"
#include "xassets.h"
#include "xassets/weapondef.h"
#include "cscr_stringlist.h"
#include "cmd.h"
#include "sv_state.h"
#include "sv_weapons.h"
#include "player_appearance.h"
#include "script_objectives.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern unsigned int bg_numAmmoTypes, bg_numWeapClips, bg_numSharedAmmoCaps;
extern struct XModel *cached_models[NATIVE_MODELS];
int OL_G_GetHintStringIndex(int *index, const char *text);
unsigned OL_G_ModelName(int index);
int BG_GetWeaponIndexForName(const char *, void (*)(unsigned));
void G_RegisterWeapon(unsigned);
int ObjectivesInUse(unsigned *highest);

static unsigned used_models(unsigned *highest)
{
    unsigned index, used = 0;
    *highest = 0;
    for (index = 1; index < NATIVE_MODELS; ++index) {
        unsigned handle = OL_G_ModelName((int)index);
        if (handle && handle != sv.emptyConfigString) { ++used; *highest = index; }
    }
    return used;
}

static int model_present(const char *name)
{
    unsigned handle = SL_FindLowercaseString(name), index;
    if (!handle) return 0;
    for (index = 1; index < NATIVE_MODELS; ++index)
        if (OL_G_ModelName((int)index) == handle) return 1;
    return 0;
}

static unsigned used_hints(void)
{
    unsigned slot, used = 0;
    const configstrings_t *bank = ServerConfigstrings();
    const configstrings_t *inherited = ServerInheritedConfigstrings();
    for (slot = 0; slot < HINTSTRING_SLOTS; ++slot) {
        const char *value = GetConfigstring(bank, CS_HINTSTRINGS + slot);
        if (!value && inherited) value = GetConfigstring(inherited, CS_HINTSTRINGS + slot);
        if (value && *value) ++used;
    }
    return used;
}

/* The sound bank is IW3's CS_SOUNDS table: G_SoundAliasIndex registers on
 * first use and Com_Errors the map on overflow, and C1 reads the same index
 * space as a byte (CS 3395+index). */
static unsigned used_sounds(unsigned *highest)
{
    unsigned index, used = 0;
    *highest = 0;
    for (index = 1; index < MAX_SOUNDS; ++index) {
        unsigned handle = (unsigned)SV_GetConfigstringIndex(CS_SOUNDS + index);
        if (handle && handle != sv.emptyConfigString) { ++used; *highest = index; }
    }
    return used;
}

static void Limits_f(void)
{
    unsigned highest_model, models = used_models(&highest_model), highest_objective = 0;
    unsigned objectives = (unsigned)ObjectivesInUse(&highest_objective);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "CoDOL limits: models %u used, highest slot %u of %u (IW3 %u)\n",
        models, highest_model, NATIVE_MODELS - 1, MAX_MODELS - 1);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "CoDOL limits: hint strings %u of %u (IW3 %u)\n",
        used_hints(), HINTSTRING_SLOTS, HINTSTRING_RETAINED);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "CoDOL limits: objectives %u in use, highest id %u of %u (IW3 15)\n",
        objectives, highest_objective, OBJECTIVE_TOTAL - 1);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "CoDOL limits: weapons %d of %u; pools ammo %u clip %u sharedcap %u of %u each\n",
        BG_GetNumWeapons(), WEAPON_SLOTS, bg_numAmmoTypes, bg_numWeapClips,
        bg_numSharedAmmoCaps, NATIVE_AMMO_POOL);
    {
        unsigned highest_sound, sounds = used_sounds(&highest_sound);
        Com_Printf(CON_CHANNEL_DONT_FILTER,
            "CoDOL limits: sound aliases %u used, highest slot %u of %u\n",
            sounds, highest_sound, MAX_SOUNDS - 1);
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "CoDOL limits: reliable command window %d (C1 ring 256)\n", MAX_RELIABLE_COMMANDS);
}

typedef struct {
    const char **names;
    unsigned count, capacity;
} name_list_t;

static void collect_name(void *header, void *data)
{
    name_list_t *list = (name_list_t *)data;
    const char *name = header ? *(const char **)header : NULL;
    if (!name || !*name || list->count >= list->capacity) return;
    list->names[list->count++] = name;
}

/* Mounted CoDOL assets are not in the IW3 asset pools, so the model list
 * comes from a zone listing (docs/zone_assets/<zone>.txt, "xmodel<TAB>name"
 * rows) when one is given. */
static char *list_buffer;
static void read_asset_list(const char *path, name_list_t *list, const char *type)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *line;
    free(list_buffer);
    list_buffer = NULL;
    if (!file) { Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress: cannot open %s\n", path); return; }
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    list_buffer = size > 0 ? malloc((size_t)size + 1) : NULL;
    if (!list_buffer || fread(list_buffer, 1, (size_t)size, file) != (size_t)size) { fclose(file); return; }
    fclose(file);
    list_buffer[size] = 0;
    for (line = strtok(list_buffer, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        size_t length = strlen(type);
        if (strncmp(line, type, length) || line[length] != 9 || list->count >= list->capacity) continue;
        {
            char *name = line + length + 1, *extra = strchr(name, '\t');
            if (extra) *extra = 0; /* "xmodel<TAB>name<TAB>~" rows */
            if (*name) list->names[list->count++] = name;
        }
    }
}

static void stress_models(unsigned wanted, const char *path)
{
    const char *names[8192];
    name_list_t list = { names, 0, 8192 };
    unsigned i, made = 0, checked_high = 0, highest, before = used_models(&highest);
    if (path && *path) read_asset_list(path, &list, "xmodel");
    else DB_EnumXAssets_FastFile(ASSET_TYPE_XMODEL, collect_name, &list, qtrue);
    Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress models: %u mounted xmodels, %u slots used, highest %u\n",
               list.count, before, highest);
    for (i = 0; i < list.count && made < wanted; ++i) {
        unsigned slot, handle;
        int wire;
        const char *name = names[i];
        if (strlen(name) >= 64 || strchr(name, ' ') || strchr(name, '"') || strchr(name, ';')) continue;
        if (model_present(name)) continue;
        slot = RegisterRuntimeModel(name);
        if (!slot) {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress models: '%s' refused after %u new\n", name, made);
            break;
        }
        ++made;
        handle = OL_G_ModelName((int)slot);
        wire = ServerModelToWire(slot);
        if (!handle || strcmp(SL_ConvertToString(handle), name) || !cached_models[slot] || wire <= 0) {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress models: FAIL slot %u '%s' reads '%s', wire %d\n",
                       slot, name, handle ? SL_ConvertToString(handle) : "", wire);
            return;
        }
        if (slot >= MAX_MODELS) ++checked_high;
    }
    used_models(&highest);
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "limitstress models: PASS %u new, %u above the old %u, highest slot now %u\n",
        made, checked_high, MAX_MODELS - 1, highest);
}

static void stress_hints(unsigned wanted)
{
    unsigned i, above = 0;
    for (i = 0; i < wanted; ++i) {
        char text[64];
        int index = -1;
        const char *published;
        Com_sprintf(text, sizeof(text), "CODOL_LIMITSTRESS_%u_%u", (unsigned)svs.time, i);
        if (!OL_G_GetHintStringIndex(&index, text)) {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress hints: refused after %u (index %d)\n", i, index);
            break;
        }
        /* The lower 32 publish through the retained refresh; read the high
         * ones straight back from the C1 bank. */
        if (index >= HINTSTRING_RETAINED) {
            published = GetConfigstring(ServerConfigstrings(), CS_HINTSTRINGS + (unsigned)index);
            if (!published || strcmp(published, text)) {
                Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress hints: FAIL slot %d reads '%s'\n",
                           index, published ? published : "");
                return;
            }
            ++above;
        }
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress hints: PASS %u allocated, %u above the old 32; %u of %u used\n",
               i, above, used_hints(), HINTSTRING_SLOTS);
}

static void stress_weapons(unsigned wanted, const char *path)
{
    const char *names[2048];
    name_list_t list = { names, 0, 2048 };
    unsigned i, made = 0, refused = 0;
    if (path && *path) read_asset_list(path, &list, "weapon");
    else DB_EnumXAssets_FastFile(ASSET_TYPE_WEAPON, collect_name, &list, qtrue);
    for (i = 0; i < list.count && made < wanted; ++i) {
        int before = BG_GetNumWeapons(), index;
        if (names[i][0] == '*' || !strcmp(names[i], "none")) continue;
        index = BG_GetWeaponIndexForName(names[i], G_RegisterWeapon);
        if (index <= 0) { ++refused; continue; }
        if (BG_GetNumWeapons() > before) ++made;
        if (bg_numAmmoTypes > NATIVE_AMMO_POOL || bg_numWeapClips > NATIVE_AMMO_POOL ||
            bg_numSharedAmmoCaps > NATIVE_AMMO_POOL) {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress weapons: FAIL pools passed %u\n", NATIVE_AMMO_POOL);
            return;
        }
        if (refused > 8) break;
    }
    /* Publish what was appended, as a runtime registration would. */
    RefreshServerWeaponMappings();
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "limitstress weapons: PASS %u new definitions, %u refused; defs %d, pools ammo %u clip %u cap %u (max %u)\n",
        made, refused, BG_GetNumWeapons(), bg_numAmmoTypes, bg_numWeapClips, bg_numSharedAmmoCaps,
        NATIVE_AMMO_POOL);
}

static void LimitStress_f(void)
{
    const char *mode = Cmd_Argv(1);
    unsigned count = Cmd_Argc() > 2 ? (unsigned)atoi(Cmd_Argv(2)) : 1;
    if (sv.state != SS_GAME) { Com_Printf(CON_CHANNEL_DONT_FILTER, "limitstress: no map running\n"); return; }
    if (!strcmp(mode, "models")) stress_models(count, Cmd_Argc() > 3 ? Cmd_Argv(3) : NULL);
    else if (!strcmp(mode, "hints")) stress_hints(count);
    else if (!strcmp(mode, "weapons")) stress_weapons(count, Cmd_Argc() > 3 ? Cmd_Argv(3) : NULL);
    else Com_Printf(CON_CHANNEL_DONT_FILTER, "usage: limitstress <models|hints|weapons> <count>\n");
}

void RegisterLimitCommands(void)
{
    Cmd_AddCommand("limits", Limits_f);
    Cmd_AddCommand("limitstress", LimitStress_f);
}
#endif

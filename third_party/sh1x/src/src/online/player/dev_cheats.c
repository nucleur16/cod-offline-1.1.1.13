/* Local developer cheat commands for the owner's own test servers.
 *
 * What lives where (the devhelp client command prints the short list):
 *
 *   god, demigod, notarget,    src/g_cmds.c (IW3 Cmd_*_f bodies); their gate is
 *   noclip, ufo, kill,         CheatsOk, which now asks CheatsPermitted.
 *   setviewpos                 noclip/ufo set gclient noclip/ufo, ClientEndFrame
 *                              publishes C1 pm_type 2/3 and both Pmoves run
 *                              their native free-move arm (pmove_c1.c).
 *   give, take, viewpos,       here, on the C1 weapon inventory API
 *   devset, addbot, kickbot,   (CODOL_WeaponBits*, GivePlayerWeapon,
 *   devhelp                    G_InitializeAmmo), the same path the GSC
 *                              giveWeapon/takeWeapon/giveMaxAmmo builtins take.
 *   dev_clientcmd              server console: run a client command as a
 *                              given client (bots have no console of their own).
 *   sv_botStop                 CVAR_CHEAT dvar read by ol_bot.c.
 *
 * Security (sec/server-audit-20261026): nothing here hands client text to
 * Cbuf. devset writes only a fixed list of dvars and only numbers (or one of
 * the bot difficulty names); weapon names are checked against a character set
 * and must already be a live or mounted definition before anything registers.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "q_shared.h"
#include "qcommon.h"
#include "cmd.h"
#include "cvar.h"
#include "g_shared.h"
#include "g_public.h"
#include "entity.h"
#include "server.h"
#include "scr_vm.h"
#include "bg_public.h"
#include "xassets/weapondef.h"
#include "weapon_bits.h"
#include "weapon_state.h"
#include "weapon_budget.h"
#include "weapon_native.h"
#include "weapon_compose.h"
#include "assets.h"
#include "dev_cheats.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern int BG_GetNumWeapons(void);
extern int BG_FindWeaponIndexForName(const char *name);
extern int __cdecl BG_GetAmmoPlayerMax(const playerState_t *ps, unsigned weapon, unsigned flag);
extern void __cdecl Add_Ammo(gentity_t *ent, unsigned weapon, unsigned char model, int count, int fillClip);
extern int __attribute__((regparm(3))) G_InitializeAmmo(gentity_t *ent, int weapon, unsigned char model, int flags);
extern int __cdecl G_GivePlayerWeapon(playerState_t *ps, int weapon, unsigned char model);
extern void __cdecl BG_TakePlayerWeapon(playerState_t *ps, unsigned weapon, int takeAmmo);
extern void __cdecl G_SelectWeaponIndex(int clientNum, int weapon);

#define DEV_FL_GODMODE   0x1
#define DEV_FL_NOTARGET  0x4

static cvar_t *dev_server;
static cvar_t *bot_stop;
static int cheats_seen;

/* ---- the gate ------------------------------------------------------------------------------- */

static int lobby_match_server(void)
{
    cvar_t *admission = Cvar_FindVar("admissionRequired");
    if (dev_server && dev_server->boolean) return 0;
    return admission && admission->boolean;
}

static int cheats_on(void)
{
    int on = g_cheats ? g_cheats->boolean : 0;
    if (on) cheats_seen = 1;
    return on;
}

int CheatsPermitted(void)
{
    return cheats_on() && !lobby_match_server();
}

const char *CheatsRefusal(void)
{
    if (!cheats_on()) return "cheats are off (sv_cheats 0: start the map with devmap)";
    if (lobby_match_server())
        return "this is a lobby match server (admissionRequired 1 without +set devServer 1)";
    return NULL;
}

int CheatsTainted(void)
{
    cheats_on();
    return cheats_seen;
}

int CheatsBlockProgression(const char *sink)
{
    static char warned[8][32];
    int i;
    if (!CheatsTainted()) return 0;
    for (i = 0; i < 8 && warned[i][0]; ++i)
        if (!strcmp(warned[i], sink)) return 1;
    if (i < 8) Q_strncpyz(warned[i], sink, sizeof(warned[i]));
    Com_PrintWarning(CON_CHANNEL_SERVER,
        "CoDOL cheats: %s not recorded - sv_cheats has been on in this server process; "
        "progression and stats stay off until the server is restarted\n", sink);
    return 1;
}

int DevMapAllowed(void)
{
    if (!lobby_match_server()) return 1;
    Com_Printf(CON_CHANNEL_DONT_FILTER,
        "devmap refused: this is a lobby match server (admissionRequired 1). "
        "Start a local server with +set devServer 1 (or admission off) for cheats.\n");
    return 0;
}

int DevBotStop(void)
{
    if (!bot_stop || !bot_stop->integer || !CheatsPermitted()) return 0;
    return bot_stop->integer;
}

/* player_die (_g_combat_mp.asm): a free-moving player is put back on normal
 * movement before the retained death gate, which refuses pm_type 2/3. */
void __cdecl DevLeaveFreeMove(gentity_t *ent)
{
    if (!ent || !ent->client) return;
    if (ent->client->ps.pm_type != 2 && ent->client->ps.pm_type != 3) return;
    ent->client->noclip = qfalse;
    ent->client->ufo = qfalse;
    ent->client->ps.pm_type = 0;
}

/* ---- messages ----------------------------------------------------------------------------------- */

void DevPrint(gentity_t *ent, const char *fmt, ...)
{
    char text[200], line[240];
    va_list ap;
    char *p;
    if (!ent) return;
    va_start(ap, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    /* One quoted argument for C1's 'e' game message; \x15 = not a
     * localization key (as GAME_UNKNOWNCLIENTCOMMAND's tail). */
    for (p = text; *p; ++p)
        if (*p == '"' || (unsigned char)*p < 0x20) *p = '\'';
    Com_sprintf(line, sizeof(line), "%c \"\x15%s\"", 'e', text);
    SV_GameSendServerCommand(ent - g_entities, 0, line);
}

static int refuse(gentity_t *ent)
{
    const char *why = CheatsRefusal();
    if (!why) return 0;
    DevPrint(ent, "Cheats refused: %s", why);
    return 1;
}

/* ---- weapons -------------------------------------------------------------------------------------- */

/* Engine and composed weapon names: letters, digits, '_', '-', '.', and for a
 * composed "*<base> <items...>" name '*' and spaces. */
static int weapon_name_ok(const char *name)
{
    const char *p;
    size_t n = name ? strlen(name) : 0;
    if (!n || n >= 128) return 0;
    for (p = name; *p; ++p) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
            *p == '_' || *p == '-' || *p == '.') continue;
        if (name[0] == '*' && (*p == '*' || *p == ' ')) continue;
        return 0;
    }
    return 1;
}

/* Find-only first; a name nothing has registered yet must be a mounted asset
 * (or a composed name) and the native table must have room before
 * G_GetWeaponIndexForName may register it, exactly the gate the bot and
 * profile loadout producers use. 0 = unknown, -1 = table full. */
static int weapon_index(const char *name)
{
    const struct WeaponDef *def = NULL;
    int index;
    if (!weapon_name_ok(name) || !strcmp(name, "none")) return 0;
    index = BG_FindWeaponIndexForName(name);
    if (index > 0 && index < BG_GetNumWeapons()) return index;
    if (name[0] == '*') {
        /* Composed: only a well-formed spec reaches the composer. */
        composed_weapon_t spec;
        if (ParseComposedWeaponName(name, &spec) != 1) return 0;
    } else if (!LoadMountedNullWeapon(name, &def) &&
               !LoadMountedNativeWeapon(name, LoadEngineWeaponCore, &def)) return 0;
    if (!NativeWeaponRoom()) return -1;
    index = G_GetWeaponIndexForName(name);
    return index > 0 && index < BG_GetNumWeapons() ? index : 0;
}

static const char *weapon_name(int weapon)
{
    const struct WeaponDef *def = weapon > 0 && weapon < BG_GetNumWeapons() ? BG_GetWeaponDef(weapon) : NULL;
    return def && def->szInternalName ? def->szInternalName : "?";
}

static void fill_ammo(gentity_t *ent)
{
    playerState_t *ps = &ent->client->ps;
    int i, count = BG_GetNumWeapons();
    for (i = 1; i < count; ++i) {
        if (!WeaponBitsOwned(ps, (unsigned)i)) continue;
        /* IW3 Cmd_Give_f "ammo": Add_Ammo(..., 998, fillClip 1) per held
         * weapon; Add_Ammo clamps to the player maximum. */
        Add_Ammo(ent, (unsigned)i, (unsigned char)WeaponBitsModelGet(ps, (unsigned)i), 998, 1);
    }
}

static void empty_ammo(gentity_t *ent)
{
    playerState_t *ps = &ent->client->ps;
    int i, count = BG_GetNumWeapons();
    for (i = 1; i < count; ++i) {
        const struct WeaponDef *def;
        if (!WeaponBitsOwned(ps, (unsigned)i) || !(def = BG_GetWeaponDef(i))) continue;
        if (def->iAmmoIndex >= 0 && def->iAmmoIndex < 128) ps->ammo[def->iAmmoIndex] = 0;
        if (def->iClipIndex >= 0 && def->iClipIndex < 128) ps->ammoclip[def->iClipIndex] = 0;
    }
}

/* PlayerCmd_takeAllWeapons, with the C1 scripted-weapon state cleared too. */
static void take_all_weapons(gentity_t *ent)
{
    playerState_t *ps = &ent->client->ps;
    int i, count = BG_GetNumWeapons();
    ps->weapon = 0;
    WeaponStateScriptedWeapon(ps, 0);
    for (i = 1; i < count; ++i)
        if (WeaponBitsOwned(ps, (unsigned)i)) BG_TakePlayerWeapon(ps, (unsigned)i, 1);
}

static void give_weapon(gentity_t *ent, const char *name, const char *model_arg)
{
    playerState_t *ps = &ent->client->ps;
    struct WeaponDef *def;
    int weapon = weapon_index(name), had;
    unsigned char model = 0;
    if (weapon < 0) { DevPrint(ent, "give: the weapon table is full; %s not registered", name); return; }
    if (!weapon) { DevPrint(ent, "give: unknown weapon or item '%s' (try give health|ammo|all)", name); return; }
    def = BG_GetWeaponDef(weapon);
    if (!def) return;
    if (model_arg && *model_arg) {
        int m = atoi(model_arg);
        if (m >= 0 && m < 16 && def->gunXModel[m]) model = (unsigned char)m;
    }
    had = WeaponBitsOwned(ps, (unsigned)weapon);
    if (!had && !G_GivePlayerWeapon(ps, weapon, model)) {
        DevPrint(ent, "give: %s refused (inventory full or no model %u)", weapon_name(weapon), model);
        return;
    }
    /* PlayerCmd_giveWeapon: ammo initialised with the had-it flag. */
    G_InitializeAmmo(ent, weapon, had ? (unsigned char)WeaponBitsModelGet(ps, (unsigned)weapon) : model, had);
    if (had) Add_Ammo(ent, (unsigned)weapon, (unsigned char)WeaponBitsModelGet(ps, (unsigned)weapon), 998, 1);
    if (def->offhandClass == 0 && def->weapClass != 9) G_SelectWeaponIndex(ent - g_entities, weapon);
    DevPrint(ent, "give: %s%s", weapon_name(weapon), had ? " (refilled)" : "");
}

static void cmd_give(gentity_t *ent)
{
    char name[128], amount_text[32];
    int amount, all;
    SV_Cmd_ArgvBuffer(1, name, sizeof(name));
    SV_Cmd_ArgvBuffer(2, amount_text, sizeof(amount_text));
    if (!name[0]) { DevPrint(ent, "usage: give <weapon [model]|health [n]|ammo|all>"); return; }
    amount = atoi(amount_text);
    all = !Q_stricmp(name, "all");
    if (all || !Q_stricmp(name, "health")) {
        int max = ent->maxHealth > 0 ? ent->maxHealth : 100;
        ent->health = amount ? ent->health + amount : max;
        if (ent->health < 1) ent->health = 1;
        ent->client->ps.stats[0] = ent->health;
        if (!all) { DevPrint(ent, "give: health %d", ent->health); return; }
    }
    if (all || !Q_stricmp(name, "ammo") || !Q_stricmp(name, "allammo")) {
        fill_ammo(ent);
        DevPrint(ent, all ? "give: health and ammo" : "give: ammo");
        return;
    }
    if (name[0] == '*') {
        /* A composed "*<tag> <n> ..." name arrives as several arguments. */
        char joined[128];
        SV_Cmd_Argsv(1, joined, sizeof(joined));
        give_weapon(ent, joined, NULL);
        return;
    }
    give_weapon(ent, name, amount_text);
}

static void cmd_take(gentity_t *ent)
{
    char name[128], amount_text[32];
    int amount, all, weapon;
    SV_Cmd_ArgvBuffer(1, name, sizeof(name));
    SV_Cmd_ArgvBuffer(2, amount_text, sizeof(amount_text));
    if (!name[0]) { DevPrint(ent, "usage: take <weapon|weapons|health [n]|ammo|all>"); return; }
    amount = atoi(amount_text);
    all = !Q_stricmp(name, "all");
    if (all || !Q_stricmp(name, "health")) {
        ent->health = amount ? ent->health - amount : 1;
        if (ent->health < 1) ent->health = 1;
        ent->client->ps.stats[0] = ent->health;
    }
    if (all || !Q_stricmp(name, "ammo")) empty_ammo(ent);
    if (all || !Q_stricmp(name, "weapons")) take_all_weapons(ent);
    if (all || !Q_stricmp(name, "health") || !Q_stricmp(name, "ammo") || !Q_stricmp(name, "weapons")) {
        DevPrint(ent, "take: %s", name);
        return;
    }
    weapon = weapon_name_ok(name) ? BG_FindWeaponIndexForName(name) : 0;
    if (weapon <= 0 || weapon >= BG_GetNumWeapons() || !WeaponBitsOwned(&ent->client->ps, (unsigned)weapon)) {
        DevPrint(ent, "take: you do not have '%s'", name);
        return;
    }
    if (ent->client->ps.weapon == weapon) {
        ent->client->ps.weapon = 0;
        WeaponStateScriptedWeapon(&ent->client->ps, 0);
    }
    BG_TakePlayerWeapon(&ent->client->ps, (unsigned)weapon, 1);
    DevPrint(ent, "take: %s", weapon_name(weapon));
}

/* ---- position, dvars, bots ---------------------------------------------------------------------- */

static void cmd_viewpos(gentity_t *ent)
{
    const playerState_t *ps = &ent->client->ps;
    DevPrint(ent, "viewpos: (%.1f %.1f %.1f) yaw %.1f pitch %.1f  setviewpos %.0f %.0f %.0f %.0f",
        ps->origin[0], ps->origin[1], ps->origin[2], ps->viewangles[YAW], ps->viewangles[PITCH],
        ps->origin[0], ps->origin[1], ps->origin[2], ps->viewangles[YAW]);
}

/* The server dvars a player may change with devset, and the value each had
 * before the first change (put back by the next plain map). */
typedef struct { const char *name; const char *what; int enum_value; char saved[64]; int changed; } dev_dvar_t;
static dev_dvar_t dev_dvars[] = {
    { "jump_height", "jump height (C1 replicates it, prediction agrees)", 0 },
    { "g_speed", "run speed (ps.speed)", 0 },
    { "g_gravity", "gravity (ps.gravity)", 0 },
    { "timescale", "server time scale", 0 },
    { "player_sprintSpeedScale", "sprint speed scale", 0 },
    { "bg_fallDamageMinHeight", "fall damage start height", 0 },
    { "bg_fallDamageMaxHeight", "fall damage lethal height", 0 },
    { "sv_botStop", "0 normal, 1 bots freeze, 2 bots hold fire", 0 },
    { "scr_bots", "training bot count", 0 },
    { "sv_botDifficulty", "recruit | regular | hardened | veteran", 1 },
};
#define DEV_DVAR_COUNT (int)(sizeof(dev_dvars) / sizeof(dev_dvars[0]))

static int number_ok(const char *text)
{
    char *end;
    double v;
    if (!text || !*text || strlen(text) > 24) return 0;
    v = strtod(text, &end);
    return *end == 0 && v == v && fabs(v) < 1.0e7;
}

static int difficulty_ok(const char *text)
{
    return !Q_stricmp(text, "recruit") || !Q_stricmp(text, "regular") ||
           !Q_stricmp(text, "hardened") || !Q_stricmp(text, "veteran");
}

static void cmd_devset(gentity_t *ent)
{
    char name[64], value[32];
    int i;
    cvar_t *var;
    SV_Cmd_ArgvBuffer(1, name, sizeof(name));
    SV_Cmd_ArgvBuffer(2, value, sizeof(value));
    for (i = 0; i < DEV_DVAR_COUNT; ++i)
        if (!Q_stricmp(name, dev_dvars[i].name)) break;
    if (!name[0] || i == DEV_DVAR_COUNT) {
        DevPrint(ent, "usage: devset <dvar> <value>; dvars: jump_height g_speed g_gravity timescale "
            "player_sprintSpeedScale bg_fallDamageMinHeight bg_fallDamageMaxHeight sv_botStop scr_bots sv_botDifficulty");
        return;
    }
    var = Cvar_FindVar(dev_dvars[i].name);
    if (!value[0]) {
        DevPrint(ent, "%s is %s - %s", dev_dvars[i].name, var ? Cvar_DisplayableValue(var) : "(unregistered)",
            dev_dvars[i].what);
        return;
    }
    if (dev_dvars[i].enum_value ? !difficulty_ok(value) : !number_ok(value)) {
        DevPrint(ent, "devset: '%s' is not a valid value for %s", value, dev_dvars[i].name);
        return;
    }
    if (!dev_dvars[i].changed) {
        Q_strncpyz(dev_dvars[i].saved, var ? Cvar_DisplayableValue(var) : "", sizeof(dev_dvars[i].saved));
        dev_dvars[i].changed = 1;
    }
    Cvar_Set(dev_dvars[i].name, value);
    var = Cvar_FindVar(dev_dvars[i].name);
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL cheats: client %d set %s %s\n", (int)(ent - g_entities), dev_dvars[i].name, value);
    DevPrint(ent, "%s %s", dev_dvars[i].name, var ? Cvar_DisplayableValue(var) : value);
}

void DevCheatsRestoreDvars(void)
{
    int i;
    for (i = 0; i < DEV_DVAR_COUNT; ++i) {
        if (!dev_dvars[i].changed) continue;
        dev_dvars[i].changed = 0;
        if (dev_dvars[i].saved[0]) Cvar_Set(dev_dvars[i].name, dev_dvars[i].saved);
    }
}

static int bot_count(void)
{
    int i, n = 0, limit = sv_maxclients ? sv_maxclients->integer : 0;
    for (i = 0; i < limit && i < MAX_CLIENTS; ++i)
        if (svs.clients[i].state >= CS_CONNECTED && svs.clients[i].netchan.remoteAddress.type == NA_BOT) ++n;
    return n;
}

static void cmd_addbot(gentity_t *ent)
{
    char text[16];
    int n, have = bot_count(), want;
    SV_Cmd_ArgvBuffer(1, text, sizeof(text));
    n = text[0] ? atoi(text) : 1;
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    /* The bot script creates it with setDvar: a string dvar. */
    want = Cvar_VariableIntegerValue("scr_bots");
    if (want < have) want = have;
    want += n;
    if (want > 16) want = 16;
    Cvar_Set("scr_bots", va("%d", want));
    DevPrint(ent, "addbot: scr_bots %d (the bot script adds them within a second)", want);
}

static void cmd_kickbot(gentity_t *ent)
{
    char text[16];
    int i, n, kicked = 0, limit = sv_maxclients ? sv_maxclients->integer : 0;
    int remaining;
    SV_Cmd_ArgvBuffer(1, text, sizeof(text));
    n = !text[0] ? 1 : !Q_stricmp(text, "all") ? MAX_CLIENTS : atoi(text);
    if (n < 1) n = 1;
    /* Lower the target first so the bot script does not replace them. */
    remaining = bot_count() - n;
    Cvar_Set("scr_bots", va("%d", remaining > 0 ? remaining : 0));
    for (i = (limit < MAX_CLIENTS ? limit : MAX_CLIENTS) - 1; i >= 0 && kicked < n; --i) {
        client_t *cl = &svs.clients[i];
        if (cl->state < CS_CONNECTED || cl->netchan.remoteAddress.type != NA_BOT) continue;
        SV_DropClient(cl, "EXE_PLAYERKICKED");
        ++kicked;
    }
    DevPrint(ent, "kickbot: %d bot%s removed", kicked, kicked == 1 ? "" : "s");
}

static void cmd_devhelp(gentity_t *ent)
{
    DevPrint(ent, "dev: god demigod notarget noclip ufo kill give take setviewpos viewpos devset addbot kickbot");
    DevPrint(ent, "give <weapon [model]|health [n]|ammo|all>  take <weapon|weapons|health [n]|ammo|all>");
    DevPrint(ent, "devset <jump_height|g_speed|g_gravity|timescale|sv_botStop|...> <value>  (%s)",
        CheatsPermitted() ? "cheats ON, progression off" : CheatsRefusal());
}

int DevClientCommand(gentity_t *ent, const char *verb)
{
    void (*fn)(gentity_t *) = NULL;
    if (!ent || !ent->client || !verb) return 0;
    if (!Q_stricmp(verb, "give")) fn = cmd_give;
    else if (!Q_stricmp(verb, "take")) fn = cmd_take;
    else if (!Q_stricmp(verb, "viewpos")) fn = cmd_viewpos;
    else if (!Q_stricmp(verb, "devset")) fn = cmd_devset;
    else if (!Q_stricmp(verb, "addbot")) fn = cmd_addbot;
    else if (!Q_stricmp(verb, "kickbot")) fn = cmd_kickbot;
    else if (!Q_stricmp(verb, "devhelp")) { cmd_devhelp(ent); return 1; }
    else return 0;
    if (refuse(ent)) return 1;
    /* Weapon and health changes need a living player, as IW3 CheatsOk. */
    if ((fn == cmd_give || fn == cmd_take) && (ent->health <= 0 ||
        ent->client->sess.sessionState != SESS_STATE_PLAYING)) {
        DevPrint(ent, "%s: you must be alive", verb);
        return 1;
    }
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL cheats: client %d %s\n", (int)(ent - g_entities), verb);
    fn(ent);
    return 1;
}

/* ---- server console --------------------------------------------------------------------------------- */

/* dev_clientcmd <clientnum|bots|all> <command ...>: the line runs through
 * SV_ExecuteClientCommand as if that client had sent it (so bots, which have
 * no console, can noclip, god, give ...). Console/rcon only. */
static void DevClientCmd_f(void)
{
    char who[16], line[512];
    int i, limit = sv_maxclients ? sv_maxclients->integer : 0, ran = 0;
    const char *why = CheatsRefusal();
    if (why) { Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_clientcmd refused: %s\n", why); return; }
    if (sv.state != SS_GAME || Cmd_Argc() < 3) {
        Com_Printf(CON_CHANNEL_DONT_FILTER, "usage (in a game): dev_clientcmd <clientnum|bots|all> <command ...>\n");
        return;
    }
    Q_strncpyz(who, Cmd_Argv(1), sizeof(who));
    Cmd_Argsv(2, line, sizeof(line));
    if (limit > MAX_CLIENTS) limit = MAX_CLIENTS;
    for (i = 0; i < limit; ++i) {
        client_t *cl = &svs.clients[i];
        int match;
        if (cl->state < CS_ACTIVE || !g_entities[i].client) continue;
        if (!Q_stricmp(who, "all")) match = 1;
        else if (!Q_stricmp(who, "bots")) match = cl->netchan.remoteAddress.type == NA_BOT;
        else match = who[0] >= '0' && who[0] <= '9' && atoi(who) == i;
        if (!match) continue;
        SV_ExecuteClientCommand(cl, line, qtrue, qfalse);
        ++ran;
    }
    Com_Printf(CON_CHANNEL_DONT_FILTER, "dev_clientcmd: \"%s\" ran for %d client%s\n", line, ran, ran == 1 ? "" : "s");
}

void DevCheatsSwitch_f(void);

/* "dev_cheats" prints the cheat state and the players; "dev_cheats on|off" switches sv_cheats
 * (net/dev_control.c). One command since the prefix was dropped:
 * the switch used to be a separate, prefixed command. */
static void DevCheats_f(void)
{
    const char *why;
    if (Cmd_Argc() == 2) {
        DevCheatsSwitch_f();
        return;
    }
    why = CheatsRefusal();
    Com_Printf(CON_CHANNEL_DONT_FILTER, "cheats: %s; devServer %d; progression %s\n",
        why ? why : "permitted", dev_server ? dev_server->integer : 0,
        CheatsTainted() ? "OFF until restart (sv_cheats has been on)" : "recorded");
    if (sv.state == SS_GAME) {
        int i, limit = sv_maxclients ? sv_maxclients->integer : 0;
        if (limit > MAX_CLIENTS) limit = MAX_CLIENTS;
        for (i = 0; i < limit; ++i) {
            gentity_t *ent = &g_entities[i];
            const playerState_t *ps;
            int w, owned = 0, count = BG_GetNumWeapons();
            if (svs.clients[i].state < CS_ACTIVE || !ent->client) continue;
            ps = &ent->client->ps;
            for (w = 1; w < count; ++w) owned += WeaponBitsOwned(ps, (unsigned)w) ? 1 : 0;
            Com_Printf(CON_CHANNEL_DONT_FILTER,
                "  %2d %-16s hp %3d pm_type %d%s%s%s%s%s at (%.0f %.0f %.0f) weapon %s, %d owned\n",
                i, svs.clients[i].name, ent->health, ps->pm_type,
                ent->flags & DEV_FL_GODMODE ? " god" : "", ent->flags & 2 ? " demigod" : "",
                ent->flags & DEV_FL_NOTARGET ? " notarget" : "", ent->client->noclip ? " noclip" : "",
                ent->client->ufo ? " ufo" : "", ps->origin[0], ps->origin[1], ps->origin[2],
                weapon_name(ps->weapon), owned);
        }
    }
}

extern int (*Cvar_CheatsPermitted)(void);

void RegisterDevCheatCommands(void)
{
    Cvar_CheatsPermitted = CheatsPermitted;
    dev_server = Cvar_RegisterBool("devServer", qfalse, 0,
        "Developer server: cheat commands work under devmap even with lobby admission on (set it on the command line)");
    bot_stop = Cvar_RegisterInt("sv_botStop", 0, 0, 2, CVAR_CHEAT,
        "Developer: 0 bots play, 1 bots stand still and hold fire, 2 bots move but never shoot (cheats only)");
    Cmd_AddCommand("dev_clientcmd", DevClientCmd_f);
    Cmd_AddCommand("dev_cheats", DevCheats_f);
}
#endif

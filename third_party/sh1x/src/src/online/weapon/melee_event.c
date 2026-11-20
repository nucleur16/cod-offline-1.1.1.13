/* The melee hit's events, as C1's own server produces them.
 *
 * Weapon_Melee_internal (src/asmsource/_g_weapon.asm) calls this once the
 * trace has resolved the struck entity, in place of IW3's blood event and
 * temp entity. C1 is client and server in one binary, so its producer at
 * 0x573a20 defines what the C1 client expects:
 *
 *   blood   0x573c3b..0x573c7c  EV_MELEE_BLOOD on the attacker, only when the
 *           attacker is a player, the struck entity is a player or an actor,
 *           the hit is not on a riot shield (trace partGroup 19, 0x573bb0),
 *           and the two are not on the same team (OnSameTeam 0x56f1e0). The
 *           one exception is a hashed enum dvar at 0x220cfa0 whose values are
 *           beginner_l..hardcore_l (registered at 0x52a814, default 1):
 *           at 4 teammates bleed too. This server has no such setting, so the
 *           exception never applies, as on C1 at its default.
 *   event   0x573c86..0x573ca6  EV_MELEE_HIT on a player or actor, else
 *           EV_MELEE_MISS; IW3 counted only players.
 *   fields  +80 struck entity (0x573cad), +d4 attacker weapon (0x573cc1),
 *           +d6 attacker's knife row (0x573cd4, see below), +e6 weapon model
 *           (0x573ce1), +ac parameter: bit 0 when the weapon has a viewmodel
 *           knife (def+0x2c, C1's knifeModel; IW3's is def+0x304) and the
 *           attacker is a player (0x573ce7..0x573d00), bit 1 on a riot-shield
 *           hit (0x573d0a..0x573d11).
 *
 * The knife row is the mp/coKnifeStatsTable.csv row an attacking player
 * carries (es+0x64, read at 0x573a5f only for eType 1); C1 writes it only
 * when that row resolves. It rides in the temp entity's un2 until the snapshot
 * overlay (OverlayEngineMeleeEvent) publishes it.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "g_public.h"
#include "bg.h"
#include "cm_public.h"
#include "xassets/weapondef.h"
#include "weapon_give.h"
#include "radius_damage.h"
#include "engine_state.h"
#include "weapon_native.h"
#include "assets.h"
#include "sentient.h"
#include <stdlib.h>

extern gentity_t *G_TempEntity(const float *origin, int event);
extern void __cdecl G_AddEvent(gentity_t *ent, int event, int parm);

/* coKnifeStatsTable.csv ships 21 rows (docs/MELEE_C1_PATH_20260920.md). */
#define KNIFE_TABLE_ROWS 21u

static int melee_character(const gentity_t *ent)
{
    return ent->client || ent->s.eType == ET_ACTOR;
}

gentity_t *__cdecl MeleeHitEvent(gentity_t *attacker, gentity_t *target,
    const weaponParms_t *wp, const trace_t *trace, const float *endpos)
{
    gentity_t *tent;
    int shield = trace->partGroup == HITLOC_SHIELD;
    int character = melee_character(target);
    unsigned row = 0;

    if (attacker->client && character && !shield && !OnSameTeam(attacker, target))
        G_AddEvent(attacker, EV_MELEE_BLOOD, 0);

    tent = G_TempEntity(endpos, character ? EV_MELEE_HIT : EV_MELEE_MISS);
    tent->s.otherEntityNum = target->s.number;
    tent->s.eventParm = (wp->weapDef && wp->weapDef->knifeModel && attacker->client ? 1 : 0) |
                        (shield ? 2 : 0);
    tent->s.weapon = attacker->s.weapon;
    tent->s.weaponModel = attacker->s.weaponModel;
    if (attacker->s.eType == ET_PLAYER) {
        row = ServerMeleeRow();
        if (row > KNIFE_TABLE_ROWS) row = 0;
    }
    /* A Zombies knife row owns its robot impact alias (zknife_standard,
     * row 13 +0x54: zmb_knife_impact_robot). That describes an actor victim,
     * not another connected player. C1's event-52 consumer takes its sound
     * override from +d6, independently of the attacker's ps+338 animation
     * row. Use its ordinary knife/body path for player contact, retaining
     * the shield flag's precedence and the row on actor/scenery contacts.
     * The body hint must survive a held gun with no knifeModel: the row
     * supplied that knife before this event-only override was removed. */
    if (target->client && row) {
        row = 0;
        tent->s.eventParm |= 1;
    }
    tent->s.un2 = (int)row;
    return tent;
}

/* C1 Weapon_Melee_internal 0x573bb9..0x573bdc, between the trace and the
 * events: an attacker that is not a client (gentity+0x17c, the same test that
 * gates the blood event at 0x573c3b), whose sentient record (gentity+0x184)
 * and the struck entity's both exist and carry the same team (record +0x08),
 * misses -- no event and no damage, the function returns NULL. An actor's
 * swing therefore never lands on its own side: the retained IW3 body had no
 * such test, so a zombie swinging at a player through the zombie in front of
 * it struck and killed that zombie instead (MEASURED 2026-10-05, probe log:
 * "actor trace from 579 ... hitId 575", 575 killed by three swings). */
int __cdecl MeleeSameTeamMiss(gentity_t *attacker, gentity_t *target)
{
    int attackerTeam, targetTeam;
    if (!attacker || !target || attacker->client) return 0;
    if (!Sentient_TeamForEntity(attacker, &attackerTeam) ||
        !Sentient_TeamForEntity(target, &targetTeam)) return 0;
    return attackerTeam == targetTeam;
}

/* The melee damage, as C1's Weapon_Melee_internal (0x573a20) chooses it.
 *
 *   0x573a43  damage = def+0x590 (iMeleeDamage)
 *   0x573a59  when the attacker is a player (eType 1) carrying a knife row
 *             (es+0x64, 1-based, bounded by the loaded row count):
 *   0x573a84    damage = row+0x10, mp/coKnifeStatsTable.csv column 4
 *   0x573ac0  unless the weapon sets def+0xbf0, which restores def+0x590
 *   0x573be2  after the trace, still with a row and no +0xbf0: when the
 *             struck entity faces away from the swing (its forward, from
 *             AngleVectors of its angles at 0x573c0a, dotted in x/y with the
 *             swing's forward is > 0, 0x573c25 against 0.0 at 0xb474cc),
 *   0x573c38    damage = row+0x14, column 5 (the back-stab damage)
 *   0x573d31  then adds rand() % 5, as IW3 does.
 *
 * The retained Weapon_Melee_internal read only iMeleeDamage, so every player
 * knifed with the held gun's 25 (+0..4): six knives for a round-1 zombie
 * instead of zknife_standard's one (150). The row is ServerMeleeRow(),
 * the same one the playerstate (ps+0x338) and the hit event publish; actors
 * carry none and keep their claw weapon's damage. */
static int knife_row_value(unsigned row, int column, int *out)
{
    const engine_table_t *table = NULL;
    const char *cell;
    if (!row || row > KNIFE_TABLE_ROWS) return 0;
    if (LoadEngineStringTable("mp/coKnifeStatsTable.csv", &table) != 1 || !table ||
        !table->values || column >= table->columnCount || (int)row > table->rowCount) return 0;
    cell = table->values[(row - 1u) * (unsigned)table->columnCount + (unsigned)column];
    if (!cell || !*cell) return 0;
    *out = atoi(cell);
    return 1;
}

static unsigned melee_attacker_row(const gentity_t *attacker, const weaponParms_t *wp)
{
    int ignores = 0;
    unsigned row;
    if (!attacker || attacker->s.eType != ET_PLAYER || !attacker->client) return 0;
    row = ServerMeleeRow();
    if (!row || row > KNIFE_TABLE_ROWS) return 0;
    if (wp && wp->weapDef && GetNativeWeaponMeleeIgnoresKnifeRow(wp->weapDef, &ignores) && ignores)
        return 0;
    return row;
}

int __cdecl MeleeDamage(gentity_t *attacker, const weaponParms_t *wp, int defDamage)
{
    int damage;
    unsigned row = melee_attacker_row(attacker, wp);
    if (row && knife_row_value(row, 4, &damage)) return damage;
    return defDamage;
}

int __cdecl MeleeBackstabDamage(gentity_t *attacker, gentity_t *target,
    const weaponParms_t *wp, int damage)
{
    vec3_t forward;
    int backstab;
    unsigned row = melee_attacker_row(attacker, wp);
    if (!row || !target || !wp) return damage;
    AngleVectors(target->r.currentAngles, forward, NULL, NULL);
    if (forward[0] * wp->forward[0] + forward[1] * wp->forward[1] > 0.0f &&
        knife_row_value(row, 5, &backstab))
        return backstab;
    return damage;
}

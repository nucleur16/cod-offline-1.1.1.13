#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Combat-training bot offhand use, driven through the real C1 offhand buttons
 * (0x4000 lethal, 0x8000 tactical, 0x10000 equipment) so the retained weapon
 * machine raises, cooks and throws the grenade and raises the engine's own
 * grenade_pullback / grenade_fire, exactly as a player's press does. Nothing
 * here spawns a projectile or applies an effect.
 *
 * Combat throws follow OpenBLOPS server_mp/sv_bot_mp.cpp Bot_UpdateGrenadeThrow:
 * decide (Bot_ShouldThrowGrenade / Bot_GrenadePickType), solve the launch
 * pitch (Bot_GrenadeInRange) plus T5's correction, refuse when the first 384
 * units of the throw are blocked (Bot_GrenadeSightTrace), hold the button
 * while the view settles and a cooking grenade burns down, then let go. The
 * bot stands still while it throws, as T5's does.
 *
 * Idle tactics follow T5 maps/mp/gametypes/_bot.gsc bot_equipment_think and
 * the IW4 waypoint tactics the CoDOL graphs carry: placed equipment at
 * "claymore" nodes (facing their authored yaw) or, on a graph with none, at a
 * corner of the bot's route; a lethal from "grenade" nodes along their
 * authored angles while an enemy is about.
 *
 * Script: <bot> codol_botThrowOffhand( slot, point ), <bot> codol_botFireAt(
 * point ), <bot> codol_botIsIdle(), <bot> codol_botOffhandBusy().
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include <math.h>
#include <string.h>
#include "bot_offhand.h"
#include "bot_offhand_logic.h"
#include "bot_nav.h"
#include "weapon_state.h"
#include "weapon_native.h"
#include "../weapon/weapon_bits.h"
#include "../weapon/weapon_flags.h"
#include "server.h"
#include "g_shared.h"
#include "scr_vm.h"
#include "cvar.h"
#include "bg_public.h"

#define BOT_PI 3.14159265358979323846f
/* C1 command buttons (see ol_bot.c). */
#define B_FIRE 1u
#define B_SPRINT 2u
#define B_MELEE 4u
#define B_RELOAD 16u
#define B_ADS 2048u
#define B_ADS_TRIGGER 0x100000u
#define B_OFFHANDS 0x1c000u

enum { MODE_NONE, MODE_THROW, MODE_FIRE };
typedef struct {
    int generation, lastThink, lastEnemy;
    int mode, pending, slot, kind, start, released, fuseThreshold, cooks, requested;
    int fireStart;
    float yaw, pitch;
    int grenadeTime, plantTime, nodeThrowTime, scriptedTime;
    unsigned random;
} offhand_t;
static offhand_t offhands[MAX_CLIENTS];
static cvar_t *allowGrenades, *minGrenadeTime, *maxGrenadeTime, *allowEquipment;
static int claymoreNodes = -1;

static float angle180(float v) { v = fmodf(v + 180.0f, 360.0f); if(v < 0) v += 360; return v - 180; }
static float dist2(const float *a, const float *b)
{ float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2]; return x * x + y * y + z * z; }
static unsigned roll(offhand_t *o)
{ o->random = o->random * 1664525u + 1013904223u; return (o->random >> 8) % 10000u; }
static int range_draw(offhand_t *o, int low, int high)
{ if(high < low) high = low; return low + (int)(roll(o) % (unsigned)(high - low + 1)); }

void BotOffhandReset(int slot)
{
    unsigned seed;
    if(slot < 0 || slot >= MAX_CLIENTS) return;
    seed = offhands[slot].random ? offhands[slot].random : (unsigned)slot * 2654435761u + 7u;
    memset(&offhands[slot], 0, sizeof(offhands[slot]));
    offhands[slot].random = seed;
}
void BotOffhandResetAll(void)
{
    int i;
    for(i = 0; i < MAX_CLIENTS; ++i) BotOffhandReset(i);
    claymoreNodes = -1;
}

static int has_name(const struct WeaponDef *def, const char *part)
{ return def && def->szInternalName && strstr(def->szInternalName, part) != NULL; }

/* The weapon a C1 offhand button would raise right now, or NULL: the same
 * gates as WeaponStateSelectOffhand (selected slot, slot restriction,
 * first magazine) plus ownership and the ground rule a bot cannot read. */
static const struct WeaponDef *slot_weapon(const playerState_t *ps, int slot)
{
    unsigned weapon = 0;
    const struct WeaponDef *def;
    if(!WeaponStateOffhandSlot(ps, (unsigned)slot, &weapon) || !weapon ||
       weapon >= (unsigned)BG_GetNumWeapons()) return NULL;
    if((unsigned)ps->weapFlags & (WEAP_OFFHAND_DISABLED | BOT_OFFHAND_BUTTON(slot))) return NULL;
    if(!WeaponBitsOwned(ps, weapon)) return NULL;
    def = BG_GetWeaponDef((int)weapon);
    if(!def || def->iClipIndex < 0 || def->iClipIndex >= 128 || ps->ammoclip[def->iClipIndex] <= 0) return NULL;
    return def;
}
static int lethal_kind(const struct WeaponDef *def)
{
    unsigned weaponClass = 0;
    if(!def) return BOT_GRENADE_NONE;
    if((GetNativeWeaponClass(def, &weaponClass) && weaponClass == 9u) || has_name(def, "knife"))
        return BOT_GRENADE_KNIFE;
    return BOT_GRENADE_FRAG;
}
static int tactical_kind(const struct WeaponDef *def)
{
    if(!def) return BOT_GRENADE_NONE;
    return has_name(def, "smoke") ? BOT_GRENADE_SMOKE : BOT_GRENADE_STUN;
}
/* Placed, self-triggering equipment worth leaving behind on a route. C4 needs
 * a detonation the bot would have to time, and a tactical insertion belongs
 * at a respawn spot, so neither is planted here. */
static int plantable(const struct WeaponDef *def)
{ return has_name(def, "claymore") || has_name(def, "bouncingbetty"); }

static int equipment_near(const float *point, float radius)
{
    int i;
    for(i = MAX_CLIENTS; i < level.num_entities; ++i) {
        const gentity_t *ent = &g_entities[i];
        if(!ent->r.inuse || ent->s.eType != ET_MISSILE) continue;
        if(dist2(ent->r.currentOrigin, point) < radius * radius) return 1;
    }
    return 0;
}
static int enemy_within(const gentity_t *self, float radius)
{
    int i, team = self->client->sess.cs.team;
    for(i = 0; i < level.maxclients; ++i) {
        const gentity_t *ent = &g_entities[i];
        if(ent == self || !ent->r.inuse || !ent->client || ent->health <= 0 ||
           ent->client->sess.sessionState != SESS_STATE_PLAYING ||
           ent->client->sess.cs.team == TEAM_SPECTATOR || (team && ent->client->sess.cs.team == team)) continue;
        if(dist2(ent->client->ps.origin, self->client->ps.origin) < radius * radius) return 1;
    }
    return 0;
}

static void eye(const gentity_t *self, float *out)
{ memcpy(out, self->client->ps.origin, 12); out[2] += self->client->ps.viewHeightCurrent; }
/* T5 Bot_GrenadeSightTrace: the first 384 units along the throw are clear. */
static int throw_clear(const gentity_t *self, float yaw, float pitch)
{
    static const float zero[3] = {0, 0, 0};
    float start[3], end[3], cp = cosf(pitch * BOT_PI / 180);
    trace_t tr;
    eye(self, start);
    end[0] = start[0] + 384 * cp * cosf(yaw * BOT_PI / 180);
    end[1] = start[1] + 384 * cp * sinf(yaw * BOT_PI / 180);
    end[2] = start[2] - 384 * sinf(pitch * BOT_PI / 180);
    G_TraceCapsule(&tr, start, zero, zero, end, self->s.number, 1);
    return !tr.startsolid && tr.fraction >= 1.0f;
}
static float yaw_to(const float *from, const float *to)
{ return atan2f(to[1] - from[1], to[0] - from[0]) * 180 / BOT_PI; }
static float flat(const float *a, const float *b)
{ float x = b[0] - a[0], y = b[1] - a[1]; return sqrtf(x * x + y * y); }

/* Launch angles that carry this offhand from the eye to the target point. */
static int aim_solution(offhand_t *o, const gentity_t *self, const struct WeaponDef *def, int kind,
                        const float *target, float *yaw, float *pitch)
{
    float start[3], range, height;
    eye(self, start);
    range = flat(start, target);
    height = target[2] - start[2];
    *yaw = yaw_to(start, target);
    if(kind == BOT_GRENADE_KNIFE || def->iProjectileSpeed <= 0) {
        *pitch = -atan2f(height, range > 1 ? range : 1) * 180 / BOT_PI;
        if(kind == BOT_GRENADE_KNIFE) *pitch += BotGrenadePitchOffset(kind, height, roll(o));
        return 1;
    }
    if(!g_gravity || !BotGrenadePitch(range, height, (float)def->iProjectileSpeed, g_gravity->value, pitch))
        return 0;
    *pitch += BotGrenadePitchOffset(kind, height, roll(o));
    if(*pitch < -70) *pitch = -70;
    if(*pitch > 70) *pitch = 70;
    return 1;
}

static void begin_throw(offhand_t *o, int slot, int kind, const struct WeaponDef *def, float yaw, float pitch, int now)
{
    if(com_developer && com_developer->integer)
        ((void)0);
    o->mode = MODE_THROW; o->pending = 0; o->slot = slot; o->kind = kind;
    o->yaw = yaw; o->pitch = pitch; o->start = now; o->released = 0;
    o->cooks = def && def->bCookOffHold;
    o->fuseThreshold = range_draw(o, 1700, 2500); /* T5 irand(1700, 2500) */
}

static void steer(const playerState_t *ps, ol_usercmd_t *cmd, float yaw, float pitch,
                  const bot_offhand_view_t *view)
{
    float dt = view->dt, fy, fp, y, p;
    if(!(dt > 0.01f)) dt = 0.01f;
    if(dt > 0.2f) dt = 0.2f;
    /* The difficulty table's tracking rates, never slower than a deliberate
     * player turning to throw. */
    fy = fminf(1, fmaxf(view->yawSpeed, 4.0f) * dt);
    fp = fminf(1, fmaxf(view->pitchSpeed, 4.0f) * dt);
    y = ps->viewangles[1] + angle180(yaw - ps->viewangles[1]) * fy;
    p = ps->viewangles[0] + angle180(pitch - ps->viewangles[0]) * fp;
    cmd->angles[0] = (unsigned short)ANGLE2SHORT(p - ps->delta_angles[0]);
    cmd->angles[1] = (unsigned short)ANGLE2SHORT(y - ps->delta_angles[1]);
}

/* An offhand the engine is raising, holding or throwing (native 15..21, 27). */
static int offhand_phase(int state) { return (state >= 15 && state <= 21) || state == 27; }

static int drive_throw(offhand_t *o, gentity_t *self, ol_usercmd_t *cmd, int now,
                       const bot_offhand_view_t *view)
{
    const playerState_t *ps = &self->client->ps;
    float yawError = angle180(o->yaw - ps->viewangles[1]), pitchError = angle180(o->pitch - ps->viewangles[0]);
    steer(ps, cmd, o->yaw, o->pitch, view);
    cmd->direction = 0;
    cmd->buttons &= ~(B_FIRE | B_ADS | B_ADS_TRIGGER | B_SPRINT | B_MELEE | B_RELOAD | B_OFFHANDS);
    if(!o->released) {
        /* The engine refused the press (weapon busy, slot emptied): stop. */
        if(now - o->start > 700 && !offhand_phase(ps->weaponstate)) {
            o->mode = MODE_NONE; o->grenadeTime = now + 3000;
            return 0;
        }
        if(BotGrenadeRelease(yawError, pitchError, ps->weaponstate, o->cooks, ps->damageCount > 0,
                                   ps->grenadeTimeLeft, o->fuseThreshold, now - o->start, 4000) ||
           (o->cooks && ps->grenadeTimeLeft > 0 && ps->grenadeTimeLeft < 900)) {
            o->released = now;
            return 1;
        }
        cmd->buttons |= BOT_OFFHAND_BUTTON(o->slot);
        return 1;
    }
    /* Keep the view on the throw until the projectile leaves the hand. */
    if(now - o->released > 1500 || (now - o->released > 250 && !offhand_phase(ps->weaponstate))) {
        o->mode = MODE_NONE;
        o->grenadeTime = now + (o->slot == BOT_SLOT_LETHAL ? range_draw(o, 5000, 9000) : range_draw(o, 3000, 6000));
        return 0;
    }
    return 1;
}

static int drive_fire(offhand_t *o, gentity_t *self, ol_usercmd_t *cmd, int now,
                      const bot_offhand_view_t *view)
{
    const playerState_t *ps = &self->client->ps;
    float error = fabsf(angle180(o->yaw - ps->viewangles[1])) + fabsf(angle180(o->pitch - ps->viewangles[0]));
    steer(ps, cmd, o->yaw, o->pitch, view);
    cmd->direction = 0;
    cmd->buttons &= ~(B_FIRE | B_ADS | B_ADS_TRIGGER | B_SPRINT | B_MELEE | B_RELOAD | B_OFFHANDS);
    if(!o->fireStart && (error < 4.0f || now - o->start > 1200)) o->fireStart = now;
    if(o->fireStart && now - o->fireStart < 400) cmd->buttons |= B_FIRE;
    if(o->fireStart && now - o->fireStart >= 1000) { o->mode = MODE_NONE; return 0; }
    if(now - o->start > 3000) { o->mode = MODE_NONE; return 0; }
    return 1;
}

static int claymore_node_count(void)
{
    int i, n = 0;
    if(claymoreNodes >= 0) return claymoreNodes;
    for(i = 0; i < BotNavCount(); ++i) {
        const bot_annotation_t *a = BotNavAnnotation(i);
        if(a && a->tactic == BOT_TACTIC_CLAYMORE && a->hasAngles) ++n;
    }
    return claymoreNodes = n;
}
/* An authored tactic node the bot is standing on, or -1. */
static int tactic_node_here(const gentity_t *self, int tactic)
{
    int i;
    for(i = 0; i < BotNavCount(); ++i) {
        const bot_annotation_t *a = BotNavAnnotation(i);
        const float *p;
        if(!a || a->tactic != tactic || !a->hasAngles) continue;
        p = BotNavPoint(i);
        if(flat(p, self->client->ps.origin) < 48 && fabsf(p[2] - self->client->ps.origin[2]) < 40) return i;
    }
    return -1;
}

static int idle_tactics(offhand_t *o, gentity_t *self, int now, const bot_offhand_view_t *view)
{
    const playerState_t *ps = &self->client->ps;
    const struct WeaponDef *def;
    int node;
    if(now < o->plantTime) return 0;
    o->plantTime = now + range_draw(o, 1000, 3000); /* T5 wait( randomintrange( 1, 3 ) ) */
    if(allowEquipment->integer && (def = slot_weapon(ps, BOT_SLOT_EQUIPMENT)) && plantable(def)) {
        float yaw;
        node = tactic_node_here(self, BOT_TACTIC_CLAYMORE);
        if(node >= 0 && !equipment_near(BotNavPoint(node), 128)) {
            begin_throw(o, BOT_SLOT_EQUIPMENT, BOT_GRENADE_NONE, def,
                        BotNavAnnotation(node)->angles[1], 30, now);
            o->plantTime = now + 15000;
            return 1;
        }
        if(!claymore_node_count() && view->corner && view->next && roll(o) < 3500u &&
           BotPathCornerPlant(ps->origin, view->corner, view->next, &yaw) &&
           !equipment_near(ps->origin, 256)) {
            begin_throw(o, BOT_SLOT_EQUIPMENT, BOT_GRENADE_NONE, def, yaw, 30, now);
            o->plantTime = now + 15000;
            return 1;
        }
    }
    if(allowGrenades->integer && now >= o->nodeThrowTime && (def = slot_weapon(ps, BOT_SLOT_LETHAL)) &&
       lethal_kind(def) == BOT_GRENADE_FRAG && (node = tactic_node_here(self, BOT_TACTIC_GRENADE)) >= 0 &&
       roll(o) < 3000u && enemy_within(self, 3000)) {
        const bot_annotation_t *a = BotNavAnnotation(node);
        if(throw_clear(self, a->angles[1], a->angles[0])) {
            begin_throw(o, BOT_SLOT_LETHAL, BOT_GRENADE_FRAG, def, a->angles[1], a->angles[0], now);
            o->nodeThrowTime = now + 20000;
            return 1;
        }
    }
    return 0;
}

static int combat_throw(offhand_t *o, gentity_t *self, int now, const bot_offhand_view_t *view)
{
    const playerState_t *ps = &self->client->ps;
    const struct WeaponDef *lethal, *tactical, *def;
    bot_grenade_situation_t s;
    float target[3], yaw, pitch;
    int slot, midpoint, clip = 1, kind;
    if(now < o->grenadeTime || !view->lastKnown) return 0;
    o->grenadeTime = now + range_draw(o, 1000, 2000);
    lethal = slot_weapon(ps, BOT_SLOT_LETHAL);
    tactical = slot_weapon(ps, BOT_SLOT_TACTICAL);
    if(!lethal && !tactical) return 0;
    memset(&s, 0, sizeof(s));
    s.lethal = lethal_kind(lethal);
    s.tactical = tactical_kind(tactical);
    s.lethalRadius = lethal ? (float)lethal->iExplosionRadius : 0;
    s.tacticalRadius = tactical ? fminf((float)tactical->iExplosionRadius, 350.0f) : 0;
    s.visible = view->seen;
    s.sinceSeen = now - view->lastSeen;
    s.minHidden = range_draw(o, minGrenadeTime->integer, maxGrenadeTime->integer);
    s.range = sqrtf(dist2(ps->origin, view->lastKnown));
    s.closeDistance = view->closeDistance;
    s.health = self->health;
    s.maxHealth = self->maxHealth > 0 ? self->maxHealth : 100;
    if(ps->weapon > 0 && ps->weapon < (unsigned)BG_GetNumWeapons())
        WeaponStateClipAmmo(ps, ps->weapon, 0, &clip);
    s.clipEmpty = clip <= 0;
    s.roll = roll(o);
    slot = BotGrenadeChoose(&s, &midpoint);
    if(slot < 0) return 0;
    def = slot == BOT_SLOT_LETHAL ? lethal : tactical;
    kind = slot == BOT_SLOT_LETHAL ? s.lethal : s.tactical;
    /* Grenades land at the feet of the last known position; a knife goes at
     * the chest. Smoke cover lands halfway. */
    memcpy(target, view->lastKnown, 12);
    if(kind != BOT_GRENADE_KNIFE) target[2] -= 44;
    if(midpoint) {
        target[0] = (ps->origin[0] + target[0]) * 0.5f;
        target[1] = (ps->origin[1] + target[1]) * 0.5f;
        target[2] = ps->origin[2];
    }
    if(!aim_solution(o, self, def, kind, target, &yaw, &pitch)) {
        if(com_developer && com_developer->integer > 1)
            Com_Printf(CON_CHANNEL_AI, "CoDOL bot offhand: %d cannot reach %.0f units with %s (speed %d)\n",
                self->s.number, s.range, def->szInternalName ? def->szInternalName : "?", def->iProjectileSpeed);
        o->grenadeTime = now + 2000;
        return 0;
    }
    /* T5 Bot_IsFacingEnemy: no about-face to throw. */
    if(fabsf(angle180(yaw - ps->viewangles[1])) > 90) return 0;
    if(!throw_clear(self, yaw, pitch)) { o->grenadeTime = now + 2000; return 0; }
    begin_throw(o, slot, kind, def, yaw, pitch, now);
    return 1;
}

int BotOffhandThink(int slot, gentity_t *self, ol_usercmd_t *cmd, int now,
                          const bot_offhand_view_t *view)
{
    offhand_t *o;
    const playerState_t *ps;
    if(slot < 0 || slot >= MAX_CLIENTS || !self || !self->client || !cmd || !view) return 0;
    o = &offhands[slot];
    ps = &self->client->ps;
    if(o->generation != self->useCount || now - o->lastThink > 1000 || now < o->lastThink) {
        int keepRequest = o->generation == self->useCount && o->pending;
        offhand_t saved = *o;
        BotOffhandReset(slot);
        o->generation = self->useCount;
        o->lastEnemy = now;
        o->grenadeTime = now + 2000;
        o->plantTime = now + 3000;
        if(keepRequest) {
            o->pending = saved.pending; o->mode = MODE_NONE; o->slot = saved.slot; o->kind = saved.kind;
            o->yaw = saved.yaw; o->pitch = saved.pitch; o->requested = saved.requested;
        }
    }
    o->lastThink = now;
    if(view->enemy >= 0 && (view->seen || now - view->lastSeen < 2000)) o->lastEnemy = now;
    /* A reward weapon (laptop, detonator, marker) is used by raising it; the
     * combat AI must not pull its trigger. Only codol_botFireAt throws one. */
    if(o->mode != MODE_FIRE && ps->weapon > 0 && ps->weapon < (unsigned)BG_GetNumWeapons()) {
        const struct WeaponDef *held = BG_GetWeaponDef((int)ps->weapon);
        if(held && held->szInternalName && (!strncmp(held->szInternalName, "killstreak_", 11) ||
           strstr(held->szInternalName, "_marker_")))
            cmd->buttons &= ~(B_FIRE | B_ADS | B_ADS_TRIGGER);
    }
    if(o->mode == MODE_THROW) return drive_throw(o, self, cmd, now, view);
    if(o->mode == MODE_FIRE) return drive_fire(o, self, cmd, now, view);
    if(view->busy || ps->groundEntityNum == ENTITYNUM_NONE || ps->weaponstate != 0) {
        if(o->pending && now - o->requested > 3000) o->pending = 0;
        return 0;
    }
    if(o->pending) {
        if(o->pending == MODE_FIRE) {
            o->pending = 0; o->mode = MODE_FIRE; o->start = now; o->fireStart = 0;
            return drive_fire(o, self, cmd, now, view);
        }
        {
            const struct WeaponDef *def = slot_weapon(ps, o->slot);
            o->pending = 0;
            if(def) {
                begin_throw(o, o->slot, o->kind, def, o->yaw, o->pitch, now);
                return drive_throw(o, self, cmd, now, view);
            }
        }
        return 0;
    }
    if(view->enemy >= 0) {
        if(allowGrenades->integer && combat_throw(o, self, now, view)) return drive_throw(o, self, cmd, now, view);
        return 0;
    }
    if(now - o->lastEnemy >= 3000 && idle_tactics(o, self, now, view)) return drive_throw(o, self, cmd, now, view);
    return 0;
}

/* ---- script ------------------------------------------------------------- */
static gentity_t *bot_receiver(scr_entref_t ref)
{
    gentity_t *ent;
    if(ref.classnum || ref.entnum >= MAX_CLIENTS) { Scr_ObjectError("not a player"); return NULL; }
    ent = &g_entities[ref.entnum];
    if(!ent->r.inuse || !ent->client || svs.clients[ref.entnum].netchan.remoteAddress.type != NA_BOT) {
        Scr_ObjectError("player is not a bot"); return NULL;
    }
    return ent;
}
static int receiver_current(const offhand_t *o, const gentity_t *ent)
{ return o->generation == ent->useCount && svs.time - o->lastThink <= 500; }

/* <bot> codol_botThrowOffhand( <slot 0 lethal | 1 tactical | 2 equipment>, <point> ):
 * throw that slot's offhand at the point (placed equipment is planted facing
 * it) through the real offhand button. True when the request was taken. */
static void throw_offhand(scr_entref_t ref)
{
    gentity_t *ent = bot_receiver(ref);
    offhand_t *o;
    const struct WeaponDef *def;
    float point[3], yaw, pitch, start[3];
    int slot, kind;
    if(!ent) return;
    if(Scr_GetNumParam() != 2) { Scr_Error("usage: <bot> codol_botThrowOffhand( <slot>, <point> )"); return; }
    slot = Scr_GetInt(0);
    Scr_GetVector(1, point);
    if(slot < 0 || slot > 2) { Scr_ParamError(0, "offhand slot must be 0..2"); return; }
    if(!isfinite(point[0]) || !isfinite(point[1]) || !isfinite(point[2])) { Scr_ParamError(1, "point must be finite"); return; }
    o = &offhands[ref.entnum];
    if(!receiver_current(o, ent) || o->mode || o->pending || ent->health <= 0 ||
       !(def = slot_weapon(&ent->client->ps, slot))) { Scr_AddBool(0); return; }
    kind = slot == BOT_SLOT_LETHAL ? lethal_kind(def) :
           slot == BOT_SLOT_TACTICAL ? tactical_kind(def) : BOT_GRENADE_NONE;
    eye(ent, start);
    if(slot == BOT_SLOT_EQUIPMENT && plantable(def)) { yaw = yaw_to(start, point); pitch = 30; }
    else if(!aim_solution(o, ent, def, kind, point, &yaw, &pitch)) {
        yaw = yaw_to(start, point); pitch = -45; /* out of reach: lob it as far as it goes */
    }
    o->pending = MODE_THROW; o->slot = slot; o->kind = kind; o->yaw = yaw; o->pitch = pitch; o->requested = svs.time;
    Scr_AddBool(1);
}

/* <bot> codol_botFireAt( <point> ): turn to the point (a ballistic arc for a
 * thrown weapon such as a care-package marker) and pull the trigger once. */
static void fire_at(scr_entref_t ref)
{
    gentity_t *ent = bot_receiver(ref);
    offhand_t *o;
    const playerState_t *ps;
    const struct WeaponDef *def = NULL;
    float point[3], start[3], yaw, pitch;
    if(!ent) return;
    if(Scr_GetNumParam() != 1) { Scr_Error("usage: <bot> codol_botFireAt( <point> )"); return; }
    Scr_GetVector(0, point);
    if(!isfinite(point[0]) || !isfinite(point[1]) || !isfinite(point[2])) { Scr_ParamError(0, "point must be finite"); return; }
    o = &offhands[ref.entnum];
    ps = &ent->client->ps;
    if(!receiver_current(o, ent) || o->mode || o->pending || ent->health <= 0) { Scr_AddBool(0); return; }
    if(ps->weapon > 0 && ps->weapon < (unsigned)BG_GetNumWeapons()) def = BG_GetWeaponDef((int)ps->weapon);
    eye(ent, start);
    if(!def || def->weapType != WEAPTYPE_GRENADE ||
       !aim_solution(o, ent, def, BOT_GRENADE_FRAG, point, &yaw, &pitch)) {
        yaw = yaw_to(start, point);
        pitch = -atan2f(point[2] - start[2], fmaxf(flat(start, point), 1)) * 180 / BOT_PI;
    }
    o->pending = MODE_FIRE; o->yaw = yaw; o->pitch = pitch; o->requested = svs.time;
    Scr_AddBool(1);
}

/* <bot> codol_botIsIdle(): alive under AI control, no enemy for two seconds
 * and no offhand action in progress (T5 bot_is_idle). */
static void is_idle(scr_entref_t ref)
{
    gentity_t *ent = bot_receiver(ref);
    offhand_t *o;
    if(!ent) return;
    o = &offhands[ref.entnum];
    Scr_AddBool(receiver_current(o, ent) && ent->health > 0 && !o->mode && !o->pending &&
                svs.time - o->lastEnemy >= 2000);
}
static void offhand_busy(scr_entref_t ref)
{
    gentity_t *ent = bot_receiver(ref);
    if(!ent) return;
    Scr_AddBool(offhands[ref.entnum].generation == ent->useCount &&
                (offhands[ref.entnum].mode || offhands[ref.entnum].pending));
}

void RegisterBotOffhandMethods(void)
{
    /* T5 sv_bot_mp.cpp dvar names and defaults. */
    allowGrenades = Cvar_RegisterBool("sv_botAllowGrenades", qtrue, 0, "Allow the bots to throw grenades");
    minGrenadeTime = Cvar_RegisterInt("sv_botMinGrenadeTime", 500, 0, 10000, 0,
        "The minimum amount of time in ms the bot must lose sight of his enemy before throwing a grenade.");
    maxGrenadeTime = Cvar_RegisterInt("sv_botMaxGrenadeTime", 1000, 0, 10000, 0,
        "The maximum amount of time in ms the bot must lose sight of his enemy before throwing a grenade.");
    allowEquipment = Cvar_RegisterBool("sv_botAllowEquipment", qtrue, 0,
        "Allow the bots to plant claymores and bouncing betties at chokepoints");
    BotOffhandResetAll();
    Scr_AddMethod("codol_botthrowoffhand", throw_offhand, 0);
    Scr_AddMethod("codol_botfireat", fire_at, 0);
    Scr_AddMethod("codol_botisidle", is_idle, 0);
    Scr_AddMethod("codol_botoffhandbusy", offhand_busy, 0);
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailOffhandEmpty(void){offhand_t expected;for(unsigned i=0;i<MAX_CLIENTS;++i){memset(&expected,0,sizeof(expected));expected.random=offhands[i].random;
 if(!expected.random||memcmp(&expected,&offhands[i],sizeof(expected)))return 0;}return claymoreNodes==-1;}
void PersistentTailOffhandReset(void){BotOffhandResetAll();}
#endif

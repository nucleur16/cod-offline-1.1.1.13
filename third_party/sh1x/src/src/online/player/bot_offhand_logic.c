/* Engine-free combat-training bot offhand decisions; see the header.
 * Adapted from OpenBLOPS Game/Server/server_mp/sv_bot_mp.cpp.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "bot_offhand_logic.h"
#include <math.h>

#define BOT_PI 3.14159265358979323846

/* T5 Bot_GrenadeInRange. T5 computes the full toss velocity (speed, speedUp,
 * relative up, forward) and then overwrites launchSpeedSq with speed^2, so the
 * solution uses the plain projectile speed; the extra lift is what the pitch
 * offset below takes back. Kept exactly so: the offsets were tuned on it. */
int BotGrenadePitch(float range, float height, float speed, float gravity, float *pitch)
{
    double v2, disc, numerator, denominator, angle;
    if(pitch) *pitch = 0.0f;
    if(!pitch || !(speed > 0.0f) || !(gravity > 0.0f) || !(range > 0.0f)) return 0;
    v2 = (double)speed * (double)speed;
    disc = v2 * v2 - (((double)range * range * gravity) + (2.0 * height * v2)) * gravity;
    if(disc < 0.0) return 0;
    numerator = v2 - sqrt(disc);
    denominator = (double)gravity * range;
    if(denominator == 0.0) return 0;
    angle = atan(numerator / denominator);
    if(!isfinite(angle)) return 0;
    *pitch = (float)(-angle * 180.0 / BOT_PI);
    return 1;
}

static float draw(unsigned roll, float low, float high)
{
    return low + (high - low) * (float)(roll % 10000u) / 9999.0f;
}

float BotGrenadePitchOffset(int kind, float height, unsigned roll)
{
    if(kind == BOT_GRENADE_KNIFE) return draw(roll, 1.0f, 3.0f);
    if(height <= -5.0f || height >= 5.0f) return draw(roll, 7.0f, 10.0f);
    return draw(roll, 10.0f, 13.0f);
}

/* T5 Bot_ShouldThrowGrenade + Bot_GrenadePickType, per decision:
 *  - an enemy that went out of sight between the drawn 0.5..1 s and 5 s ago is
 *    grenaded 75% of the time at its last known position (around a corner);
 *    a blade is never thrown blind, and with no lethal a stun grenade goes;
 *  - a visible enemy past close distance is grenaded when the magazine is
 *    empty (T5's only visible-enemy throw).
 * CoDOL additions, at low per-decision rates so they read as choices, not
 * spam: a throwing knife at a visible enemy in knife range, a stun grenade
 * at a visible enemy at mid range, and smoke halfway to a visible enemy when
 * the bot is hurt and too far to win the fight. Every explosive keeps T5's
 * own-blast rule: never inside the grenade's explosion radius. */
int BotGrenadeChoose(const bot_grenade_situation_t *s, int *midpoint)
{
    int hidden;
    if(midpoint) *midpoint = 0;
    if(!s || s->range <= 0.0f) return -1;
    hidden = !s->visible && s->sinceSeen >= s->minHidden && s->sinceSeen < 5000;
    if(hidden) {
        if(s->roll >= 7500u) return -1;
        if(s->lethal == BOT_GRENADE_FRAG && s->range > s->lethalRadius) return BOT_SLOT_LETHAL;
        if(s->tactical == BOT_GRENADE_STUN && s->range > s->tacticalRadius) return BOT_SLOT_TACTICAL;
        return -1;
    }
    if(!s->visible) return -1;
    if(s->clipEmpty && s->range >= s->closeDistance) {
        if(s->lethal == BOT_GRENADE_FRAG && s->range > s->lethalRadius) return BOT_SLOT_LETHAL;
        if(s->lethal == BOT_GRENADE_KNIFE && s->range <= 900.0f) return BOT_SLOT_LETHAL;
    }
    if(s->lethal == BOT_GRENADE_KNIFE && s->range >= 128.0f && s->range <= 900.0f && s->roll < 2500u)
        return BOT_SLOT_LETHAL;
    if(s->tactical == BOT_GRENADE_SMOKE && s->maxHealth > 0 && s->health * 10 < s->maxHealth * 6 &&
       s->range >= 600.0f && s->roll < 5000u) {
        if(midpoint) *midpoint = 1;
        return BOT_SLOT_TACTICAL;
    }
    if(s->tactical == BOT_GRENADE_STUN && s->range > s->tacticalRadius && s->range >= 350.0f &&
       s->range <= 1200.0f && s->roll < 1200u)
        return BOT_SLOT_TACTICAL;
    return -1;
}

int BotGrenadeRelease(float yawError, float pitchError, int weaponstate, int cooks,
                            int damaged, int fuseLeft, int fuseThreshold, int heldMs, int maxHold)
{
    if(heldMs >= maxHold) return 1;
    /* Native offhand phases: 15 init, 16 prepare, 17/18 held, 19 throw,
     * 20 end, 21 and 27 detonation. Before 17 the grenade is not up yet. */
    if(!((weaponstate >= 17 && weaponstate <= 21) || weaponstate == 27)) return 0;
    if(fabsf(yawError) >= 5.0f || fabsf(pitchError) >= 5.0f) return 0;
    if(cooks && !damaged && fuseLeft > fuseThreshold) return 0;
    return 1;
}

int BotPathCornerPlant(const float *origin, const float *corner, const float *next, float *yaw)
{
    float ax, ay, bx, by, la, lb, dot;
    if(!origin || !corner || !next || !yaw) return 0;
    ax = corner[0] - origin[0]; ay = corner[1] - origin[1];
    bx = next[0] - corner[0]; by = next[1] - corner[1];
    la = sqrtf(ax * ax + ay * ay); lb = sqrtf(bx * bx + by * by);
    if(la > 64.0f || la < 8.0f || lb < 64.0f) return 0;
    dot = (ax * bx + ay * by) / (la * lb);
    if(dot > 0.70710678f) return 0;
    *yaw = (float)(atan2(by, bx) * 180.0 / BOT_PI);
    return 1;
}

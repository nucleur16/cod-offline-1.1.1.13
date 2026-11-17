/* hud_style_policy.h - the pure parts of hud_style.c (no C1 memory): what the ported HUD items show.
 * tests/hud_style_policy_test.c checks them on the build machine. */
#pragma once

#include <math.h>

/* C1's weapon fire types (weaponDef + 0x350; MEASURED by value over the stock weapon defs: m4 0, usp / spas12 /
 * m21 / rpg 1, m16 / famas 3, the IW4 enum full auto, single shot, burst 2 / 3 / 4) and weapon classes
 * (weaponDef + 0xB64, MEASURED the same way: rifle 2, pistol 1, shotgun 5, rocket launcher 9). */
#define HUD_FIRE_FULLAUTO   0
#define HUD_FIRE_SINGLE     1
#define HUD_FIRE_BURST2     2
#define HUD_FIRE_BURST3     3
#define HUD_FIRE_BURST4     4
#define HUD_CLASS_SHOTGUN   5
#define HUD_CLASS_ROCKET    9

/* The fire-mode label the later builds' getweaponfiretype gives for a weapon: one of their @SF_WEAPON_*_NAME keys
 * (2.6.9.7 localized_code_post_gfx_mp). Which key a fire type and class get is INFERRED from the keys' names and
 * values (full auto, burst, semi-auto rifle, pump, single shot); the later code is packed and was not read. */
static const char *HudFireTypeKey(int fireType, int weapClass)
{
    switch (fireType) {
    case HUD_FIRE_FULLAUTO: return "@SF_WEAPON_FULL_AUTO_NAME";
    case HUD_FIRE_BURST2: return "@SF_WEAPON_BURST_2_NAME";
    case HUD_FIRE_BURST3: return "@SF_WEAPON_BURST_3_NAME";
    case HUD_FIRE_BURST4: return "@SF_WEAPON_BURST_4_NAME";
    case HUD_FIRE_SINGLE:
        if (weapClass == HUD_CLASS_SHOTGUN) return "@SF_WEAPON_SEMIAUTO_PUMP_NAME";
        if (weapClass == HUD_CLASS_ROCKET) return "@SF_WEAPON_ROCKET_NAME";
        return "@SF_WEAPON_SEMIAUTO_RIFLE_NAME";
    default: return "";
    }
}

/* The clip count the later ammo display draws: the right hand's clip, plus the left hand's only for an akimbo
 * weapon (C1's own display does the same, 0x004540F0 with 0x00417F50). */
static int HudClipRounds(int right, int left, int akimbo)
{
    int n = right + (akimbo ? left : 0);
    return n > 0 ? n : 0;
}

/* The clip gauge's fill, 0..1000: rounds / (clip size * hands), as the later build's bar (1.2.1.12 0x00455EF0). */
static int HudClipPermille(int rounds, int clipSize, int akimbo)
{
    long long cap = (long long)clipSize * (akimbo ? 2 : 1);
    if (cap <= 0 || rounds <= 0) return 0;
    long long v = (long long)rounds * 1000 / cap;
    return v > 1000 ? 1000 : (int)v;
}

/* The filled part of the gauge rect (x, w): it empties from its left end, anchored at the right (the later build
 * draws x' = x + (1 - f) * w, w' = f * w). */
static void HudClipBar(float x, float w, int permille, float *outX, float *outW)
{
    float f = (permille < 0 ? 0 : permille > 1000 ? 1000 : permille) / 1000.0f;
    *outW = w * f;
    *outX = x + (w - *outW);
}

/* The health fill, 0..1000: C1's own health fraction (0x00453EE0: cg + 0x190 health / cg + 0x198 max health,
 * clamped to 0..1, and 0 when either is 0 or the pm type at cg + 4 is 8). */
static int HudHealthPermille(int health, int maxHealth, int pmType)
{
    if (health <= 0 || maxHealth <= 0 || pmType == 8) return 0;
    long long v = (long long)health * 1000 / maxHealth;
    return v > 1000 ? 1000 : (int)v;
}

/* The SM2 health fill shown, in permille, after dt seconds: it drops to a lower health at once (damage) and eases up
 * to a higher one (regeneration), closing HUD_HEALTH_LERP_RATE of the gap per second (exponential), so a regen of
 * the whole bar takes about half a second; a gap under one permille is closed. dt <= 0 (first frame) or a shown
 * value outside 0..1000 starts at the health itself. */
#define HUD_HEALTH_LERP_RATE 8.0f
static float HudHealthLerp(float shown, int target, float dt)
{
    if (dt <= 0.0f || shown < 0.0f || shown > 1000.0f || target <= shown) return (float)target;
    if (dt > 0.25f) dt = 0.25f;                       /* a hitch eases, it does not jump */
    shown += ((float)target - shown) * (1.0f - expf(-HUD_HEALTH_LERP_RATE * dt));
    return (float)target - shown < 1.0f ? (float)target : shown;
}

/* Below this fill the health bar is drawn red (SM2). */
#define HUD_HEALTH_LOW_PERMILLE 300

/* The weapon icon in the item's rect with the image's own aspect: as high as the rect, as wide as the image's
 * width / height gives (at most the rect's width), right-aligned. A degenerate image fills the rect. */
static void HudIconFit(float x, float w, float h, int imageW, int imageH, float *outX, float *outW)
{
    float iw = (imageW > 0 && imageH > 0) ? h * (float)imageW / (float)imageH : w;
    if (iw > w) iw = w;
    *outW = iw;
    *outX = x + (w - iw);
}

/* The SM2 compass strip (ownerDraw 3005): C1's heading tape (0x004321B0) shows dvar compassTickertapeStretch
 * (249) of its material's width, centred on the view's heading. viper_heading_mp, the material it is drawn with,
 * carries two laps of the heading (720 degrees: N 3 6 E 12 15 S 21 24 W 30 33, twice) across its 1024 texels, so
 * C1's default 0.5 puts all 360 degrees into the strip: drawn 100 units wide (the stock killstreak menu's tape is
 * 400) the letters shrink to a few pixels and only W and S stay legible. The strip shows this many degrees instead,
 * so that one cardinal letter is always in it and the labels keep their size. */
#define HUD_TAPE_DEGREES     120
#define HUD_TAPE_LAP_DEGREES 720   /* viper_heading_mp's width in degrees */

/* compassTickertapeStretch for `degrees` of a tape whose material spans `lapDegrees`, inside the dvar's own
 * domain 0.01..1. */
static float HudTapeStretch(int degrees, int lapDegrees)
{
    float s = lapDegrees > 0 ? (float)degrees / (float)lapDegrees : 0.5f;
    return s < 0.01f ? 0.01f : s > 1.0f ? 1.0f : s;
}

/* C1's tape centres the material on heading / 360 (0x004322B3: -(cg + 0xB441C view yaw - cg + 0xBFA08 compass north)
 * / 360), one material width per turn. viper_heading_mp holds two laps in that width, so the stock tape ran its
 * labels at twice the heading: facing east it showed S, facing south N. The strip hands the tape a north of its own
 * for its call, (yaw + north) / 2, which halves the difference the tape reads: one turn, one lap of labels. A
 * jump of the view yaw by 360 moves the material by half its width, one whole lap: the same picture. */
static float HudTapeNorth(float viewYaw, float north)
{
    return (viewYaw + north) * 0.5f;
}

/* The HUD sway (cg_hudSway, every style): the whole HUD lags the view's turning, bobs with the player's steps, dips
 * with jumps and landings and is shaken by what shakes the camera (explosions, damage). Offsets are in the hosts'
 * 640x480 units; the menus add cg_hudSwayX / Y (hundredths) to every HUD item's rect. */
#define HUD_SWAY_LOOK       0.020f   /* units per degree / second of turning (live: 0.010 moved 1.2 units at 180 deg/s) */
#define HUD_SWAY_LOOK_MAX   720.0f   /* degrees / second counted at most */
#define HUD_SWAY_BOB        1.1f     /* units of step bob at running speed */
#define HUD_SWAY_RUN_SPEED  190.0f   /* units / second: C1's running speed */
#define HUD_SWAY_STEP_HZ    1.7f     /* steps per second at running speed (one bob cycle = two steps) */
#define HUD_SWAY_JUMP       0.010f   /* units per unit / second of vertical speed */
#define HUD_SWAY_JUMP_MAX   4.0f
#define HUD_SWAY_SHAKE      3.0f     /* units per degree of camera shake */
#define HUD_SWAY_SHAKE_MAX  8.0f
#define HUD_SWAY_MAX        12.0f
#define HUD_SWAY_OMEGA      16.0f    /* the spring that pulls the HUD back: rad / s */
#define HUD_SWAY_DAMPING    0.55f    /* < 1: a small overshoot when it settles */

struct HudSway {
    float x, y, vx, vy;              /* the sprung offset and its speed */
    float phase;                     /* step bob */
    float yaw, pitch;                /* last frame's view */
    int primed;
};

static float HudSwayClamp(float v, float m) { return v < -m ? -m : v > m ? m : v; }

static float HudSwayAngleDelta(float a, float b)
{
    float d = fmodf(a - b, 360.0f);
    if (d > 180.0f) d -= 360.0f;
    if (d < -180.0f) d += 360.0f;
    return d;
}

/* One frame. dt in seconds; yaw / pitch the player's view (degrees, IW: yaw left-positive, pitch down-positive);
 * shakeYaw / shakePitch what the rendered view adds to it (degrees); speed the horizontal speed and vz the
 * vertical one (units / s); alive 0 lets the HUD settle. Writes the offset to *outX / *outY. */
static void HudSwayStep(struct HudSway *s, float dt, float yaw, float pitch, float shakeYaw, float shakePitch,
                        float speed, float vz, int alive, float *outX, float *outY)
{
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;                         /* a hitch or a pause: no catapult */
    float tx = 0.0f, ty = 0.0f;
    if (!s->primed || !alive || dt == 0.0f) {
        s->yaw = yaw;
        s->pitch = pitch;
        s->primed = 1;
    }
    if (alive && dt > 0.0f) {
        float ry = HudSwayClamp(HudSwayAngleDelta(yaw, s->yaw) / dt, HUD_SWAY_LOOK_MAX);
        float rp = HudSwayClamp(HudSwayAngleDelta(pitch, s->pitch) / dt, HUD_SWAY_LOOK_MAX);
        /* turning right lowers the yaw: the HUD trails to the left; looking up lowers the pitch: it trails down */
        tx += ry * HUD_SWAY_LOOK;
        ty -= rp * HUD_SWAY_LOOK;
        float run = HudSwayClamp(speed / HUD_SWAY_RUN_SPEED, 1.3f);
        if (run < 0.0f) run = 0.0f;
        s->phase += dt * run * HUD_SWAY_STEP_HZ * 3.14159265f;
        if (s->phase > 6.2831853f) s->phase -= 6.2831853f;
        tx += sinf(s->phase) * HUD_SWAY_BOB * run;
        ty += fabsf(sinf(s->phase)) * HUD_SWAY_BOB * 0.8f * run;
        ty += HudSwayClamp(vz * HUD_SWAY_JUMP, HUD_SWAY_JUMP_MAX);     /* rising: it trails down */
    }
    s->yaw = yaw;
    s->pitch = pitch;
    /* a damped spring toward the target, in steps of at most 1/240 s */
    for (float left = dt; left > 0.0f; ) {
        float h = left < (1.0f / 240.0f) ? left : (1.0f / 240.0f);
        float w2 = HUD_SWAY_OMEGA * HUD_SWAY_OMEGA, c = 2.0f * HUD_SWAY_DAMPING * HUD_SWAY_OMEGA;
        s->vx += (w2 * (tx - s->x) - c * s->vx) * h;
        s->vy += (w2 * (ty - s->y) - c * s->vy) * h;
        s->x += s->vx * h;
        s->y += s->vy * h;
        left -= h;
    }
    s->x = HudSwayClamp(s->x, HUD_SWAY_MAX);
    s->y = HudSwayClamp(s->y, HUD_SWAY_MAX);
    /* the camera's own shake goes straight through: it is already a motion */
    float sx = alive ? HudSwayClamp(shakeYaw * HUD_SWAY_SHAKE, HUD_SWAY_SHAKE_MAX) : 0.0f;
    float sy = alive ? HudSwayClamp(-shakePitch * HUD_SWAY_SHAKE, HUD_SWAY_SHAKE_MAX) : 0.0f;
    *outX = HudSwayClamp(s->x + sx, HUD_SWAY_MAX);
    *outY = HudSwayClamp(s->y + sy, HUD_SWAY_MAX);
}

/* hud_style_policy.h - the pure parts of hud_style.c (no C1 memory): what the ported HUD items show.
 * tests/hud_style_policy_test.c checks them on the build machine. */
#pragma once

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

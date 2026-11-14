/* hud_style_policy.h: what the ported HUD items show, without the game. Run by tests/run_tests.sh. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/hud_style_policy.h"

int main(void)
{
    /* fire-mode labels: the measured C1 enum values of the stock weapons */
    assert(!strcmp(HudFireTypeKey(0, 2), "@SF_WEAPON_FULL_AUTO_NAME"));       /* m4_mp */
    assert(!strcmp(HudFireTypeKey(3, 2), "@SF_WEAPON_BURST_3_NAME"));         /* m16_mp, famas_mp */
    assert(!strcmp(HudFireTypeKey(1, 2), "@SF_WEAPON_SEMIAUTO_RIFLE_NAME"));  /* m21_mp */
    assert(!strcmp(HudFireTypeKey(1, 1), "@SF_WEAPON_SEMIAUTO_RIFLE_NAME"));  /* usp_mp */
    assert(!strcmp(HudFireTypeKey(1, 5), "@SF_WEAPON_SEMIAUTO_PUMP_NAME"));   /* spas12_mp */
    assert(!strcmp(HudFireTypeKey(1, 9), "@SF_WEAPON_ROCKET_NAME"));          /* rpg_mp */
    assert(!strcmp(HudFireTypeKey(2, 2), "@SF_WEAPON_BURST_2_NAME"));
    assert(!strcmp(HudFireTypeKey(4, 2), "@SF_WEAPON_BURST_4_NAME"));
    assert(!strcmp(HudFireTypeKey(7, 2), ""));

    /* the clip count: one hand unless akimbo (live: clip + left doubled a 30-round magazine to 60) */
    assert(HudClipRounds(30, 30, 0) == 30);
    assert(HudClipRounds(19, 30, 0) == 19);
    assert(HudClipRounds(12, 7, 1) == 19);
    assert(HudClipRounds(0, 0, 1) == 0);
    assert(HudClipRounds(-1, 0, 0) == 0);

    /* the gauge: rounds over the clip size of every hand */
    assert(HudClipPermille(30, 30, 0) == 1000);
    assert(HudClipPermille(19, 30, 0) == 633);
    assert(HudClipPermille(24, 12, 1) == 1000);
    assert(HudClipPermille(12, 12, 1) == 500);
    assert(HudClipPermille(5, 0, 0) == 0);
    assert(HudClipPermille(40, 30, 0) == 1000);

    /* the filled part stays anchored at the gauge's right end */
    float x, w;
    HudClipBar(-133.0f, 99.0f, 1000, &x, &w);
    assert(x == -133.0f && w == 99.0f);
    HudClipBar(-133.0f, 99.0f, 0, &x, &w);
    assert(w == 0.0f && x == -34.0f);
    HudClipBar(-133.0f, 99.0f, 500, &x, &w);
    assert(w == 49.5f && x + w == -34.0f);
    HudClipBar(-133.0f, 99.0f, 2000, &x, &w);
    assert(w == 99.0f);
    /* health: C1's own fraction (0x00453EE0) */
    assert(HudHealthPermille(100, 100, 0) == 1000);
    assert(HudHealthPermille(29, 100, 0) == 290 && HudHealthPermille(29, 100, 0) < HUD_HEALTH_LOW_PERMILLE);
    assert(HudHealthPermille(30, 100, 0) == HUD_HEALTH_LOW_PERMILLE);
    assert(HudHealthPermille(150, 100, 0) == 1000);
    assert(HudHealthPermille(50, 0, 0) == 0);
    assert(HudHealthPermille(-5, 100, 0) == 0);
    assert(HudHealthPermille(100, 100, 8) == 0);           /* pm type 8: no health shown, as C1 */

    /* the weapon icon keeps its image's aspect, right-aligned in the rect */
    HudIconFit(-90.0f, 72.0f, 18.0f, 256, 64, &x, &w);     /* 4:1 */
    assert(w == 72.0f && x == -90.0f);
    HudIconFit(-90.0f, 72.0f, 18.0f, 128, 64, &x, &w);     /* 2:1: half as wide, against the right edge */
    assert(w == 36.0f && x == -54.0f);
    HudIconFit(-90.0f, 72.0f, 18.0f, 512, 64, &x, &w);     /* wider than the rect: capped */
    assert(w == 72.0f && x == -90.0f);
    HudIconFit(-90.0f, 72.0f, 18.0f, 0, 64, &x, &w);       /* no image size: the rect */
    assert(w == 72.0f && x == -90.0f);
    puts("hud style policy passed");
    return 0;
}

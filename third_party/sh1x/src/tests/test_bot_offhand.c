/* Combat-training bot offhand decisions (src/online/player/bot_offhand_logic.c).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bot_offhand_logic.h"

static int failures;
#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while(0)
#define PI 3.14159265358979323846

/* Range a launch at this pitch reaches, flat ground, no drag. */
static double flat_range(double speed, double gravity, double pitchDegrees)
{
    double a = -pitchDegrees * PI / 180.0;
    return speed * speed * sin(2.0 * a) / gravity;
}

static bot_grenade_situation_t base(void)
{
    bot_grenade_situation_t s;
    memset(&s, 0, sizeof(s));
    s.lethal = BOT_GRENADE_FRAG;
    s.tactical = BOT_GRENADE_STUN;
    s.lethalRadius = 256;
    s.tacticalRadius = 300;
    s.minHidden = 750;
    s.range = 700;
    s.closeDistance = 256;
    s.health = s.maxHealth = 100;
    s.roll = 100;
    return s;
}

int main(void)
{
    float pitch, yaw;
    int midpoint;
    bot_grenade_situation_t s;

    /* Ballistics: the low solution lands where asked, negative looks up. */
    CHECK(BotGrenadePitch(400, 0, 800, 800, &pitch));
    CHECK(pitch < 0 && pitch > -45);
    CHECK(fabs(flat_range(800, 800, pitch) - 400) < 1.0);
    /* A higher target needs a steeper throw than a level one. */
    {
        float level, above;
        CHECK(BotGrenadePitch(500, 0, 900, 800, &level));
        CHECK(BotGrenadePitch(500, 150, 900, 800, &above));
        CHECK(above < level);
    }
    /* Beyond v^2/g it cannot reach; bad inputs never reach. */
    CHECK(!BotGrenadePitch(1000, 0, 800, 800, &pitch) && pitch == 0);
    CHECK(!BotGrenadePitch(0, 0, 800, 800, &pitch));
    CHECK(!BotGrenadePitch(400, 0, 0, 800, &pitch));
    CHECK(!BotGrenadePitch(400, 0, 800, 0, &pitch));

    /* T5 pitch corrections. */
    CHECK(BotGrenadePitchOffset(BOT_GRENADE_KNIFE, 0, 0) == 1.0f);
    CHECK(BotGrenadePitchOffset(BOT_GRENADE_KNIFE, 0, 9999) == 3.0f);
    CHECK(BotGrenadePitchOffset(BOT_GRENADE_FRAG, 0, 0) == 10.0f);
    CHECK(BotGrenadePitchOffset(BOT_GRENADE_FRAG, 0, 9999) == 13.0f);
    CHECK(BotGrenadePitchOffset(BOT_GRENADE_FRAG, 40, 0) == 7.0f);
    CHECK(BotGrenadePitchOffset(BOT_GRENADE_FRAG, -40, 9999) == 10.0f);

    /* Around a corner: enemy lost 1 s ago -> frag at the last known spot. */
    s = base(); s.sinceSeen = 1000;
    CHECK(BotGrenadeChoose(&s, &midpoint) == BOT_SLOT_LETHAL && !midpoint);
    /* Too soon after losing sight, or forgotten after 5 s: nothing. */
    s = base(); s.sinceSeen = 300;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1);
    s = base(); s.sinceSeen = 6000;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1);
    /* T5's 25% of decisions that hold the grenade. */
    s = base(); s.sinceSeen = 1000; s.roll = 8000;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1);
    /* Never inside the frag's own blast. */
    s = base(); s.sinceSeen = 1000; s.range = 200;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1);
    /* A knife is never thrown blind; the stun grenade goes instead. */
    s = base(); s.sinceSeen = 1000; s.lethal = BOT_GRENADE_KNIFE;
    CHECK(BotGrenadeChoose(&s, &midpoint) == BOT_SLOT_TACTICAL);
    /* Smoke is not thrown at a hidden enemy. */
    s = base(); s.sinceSeen = 1000; s.lethal = BOT_GRENADE_NONE; s.tactical = BOT_GRENADE_SMOKE;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1);

    /* Visible: T5 throws a frag only with an empty magazine past close range. */
    s = base(); s.visible = 1; s.roll = 9000;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1);
    s.clipEmpty = 1;
    CHECK(BotGrenadeChoose(&s, &midpoint) == BOT_SLOT_LETHAL);
    s.range = 200;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1);
    /* Knife at a visible enemy in range. */
    s = base(); s.visible = 1; s.lethal = BOT_GRENADE_KNIFE; s.range = 500; s.roll = 1000;
    CHECK(BotGrenadeChoose(&s, &midpoint) == BOT_SLOT_LETHAL);
    s.range = 1500;
    CHECK(BotGrenadeChoose(&s, &midpoint) != BOT_SLOT_LETHAL);
    /* Stun at mid range, not point blank. */
    s = base(); s.visible = 1; s.range = 600; s.roll = 500;
    CHECK(BotGrenadeChoose(&s, &midpoint) == BOT_SLOT_TACTICAL);
    s.range = 250;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1);
    /* Hurt and far: smoke lands halfway. Healthy: no smoke. */
    s = base(); s.visible = 1; s.tactical = BOT_GRENADE_SMOKE; s.range = 900; s.health = 40; s.roll = 3000;
    CHECK(BotGrenadeChoose(&s, &midpoint) == BOT_SLOT_TACTICAL && midpoint);
    s.health = 100;
    CHECK(BotGrenadeChoose(&s, &midpoint) == -1 && !midpoint);
    CHECK(BotGrenadeChoose(NULL, &midpoint) == -1);

    /* Release: aimed, raised, cooked. */
    CHECK(!BotGrenadeRelease(0, 0, 15, 0, 0, 0, 2000, 100, 4000));   /* still raising */
    CHECK(BotGrenadeRelease(1, -2, 18, 0, 0, 0, 2000, 400, 4000));   /* held and aimed */
    CHECK(!BotGrenadeRelease(8, 0, 18, 0, 0, 0, 2000, 400, 4000));   /* still turning */
    CHECK(!BotGrenadeRelease(0, 0, 18, 1, 0, 3000, 2000, 400, 4000)); /* cooking */
    CHECK(BotGrenadeRelease(0, 0, 18, 1, 0, 1800, 2000, 900, 4000));  /* cooked */
    CHECK(BotGrenadeRelease(0, 0, 18, 1, 1, 3000, 2000, 400, 4000));  /* hurt: let go */
    CHECK(BotGrenadeRelease(30, 30, 0, 1, 0, 3000, 2000, 4000, 4000));/* hold cap */
    CHECK(BotGrenadeRelease(0, 0, 27, 0, 0, 0, 2000, 100, 4000));    /* detonation phase */
    CHECK(!BotGrenadeRelease(0, 0, 23, 0, 0, 0, 2000, 100, 4000));   /* sprint, not an offhand */

    /* Plant at a route corner, facing the leg leaving it. */
    {
        float origin[3] = {0, 0, 0}, corner[3] = {40, 0, 0}, next[3] = {40, 300, 0}, straight[3] = {400, 0, 0};
        float far[3] = {200, 0, 0};
        CHECK(BotPathCornerPlant(origin, corner, next, &yaw) && fabs(yaw - 90) < 0.01);
        CHECK(!BotPathCornerPlant(origin, corner, straight, &yaw));
        CHECK(!BotPathCornerPlant(origin, far, next, &yaw));
        CHECK(!BotPathCornerPlant(NULL, corner, next, &yaw));
    }
    CHECK(BOT_OFFHAND_BUTTON(BOT_SLOT_LETHAL) == 0x4000u);
    CHECK(BOT_OFFHAND_BUTTON(BOT_SLOT_TACTICAL) == 0x8000u);
    CHECK(BOT_OFFHAND_BUTTON(BOT_SLOT_EQUIPMENT) == 0x10000u);

    if(failures) { fprintf(stderr, "test_bot_offhand: %d failure(s)\n", failures); return 1; }
    printf("test_bot_offhand: ok\n");
    return 0;
}

/* Engine-free decisions behind combat-training bot offhand use: grenade
 * ballistics, which slot to throw and when to let go. Kept apart from the
 * playerState glue in bot_offhand.c so it can be tested on the host.
 *
 * Reference: OpenBLOPS Game/Server/server_mp/sv_bot_mp.cpp
 * (Bot_GrenadeInRange, Bot_UpdateGrenadeThrow, Bot_ShouldThrowGrenade,
 * Bot_GrenadePickType). C1 has three selected offhand slots instead of T5's
 * lethal/tactical pair, and a third (equipment) slot that T5 places through a
 * weapon switch; see bot_offhand.c.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BOT_OFFHAND_LOGIC_H
#define BOT_OFFHAND_LOGIC_H

/* C1 selected offhand slots (ps+0x32c/0x330/0x334) and their buttons. */
enum { BOT_SLOT_LETHAL = 0, BOT_SLOT_TACTICAL = 1, BOT_SLOT_EQUIPMENT = 2 };
#define BOT_OFFHAND_BUTTON(slot) (0x4000u << (slot))

/* What the bot's offhand slots hold, as the decision needs it. */
enum {
    BOT_GRENADE_NONE = 0,
    BOT_GRENADE_FRAG,      /* lethal that explodes (frag, semtex) */
    BOT_GRENADE_KNIFE,     /* lethal thrown flat at a visible target */
    BOT_GRENADE_STUN,      /* flash, concussion, EMP */
    BOT_GRENADE_SMOKE
};

/* T5 Bot_GrenadeInRange: the launch pitch (degrees, Q3 convention: negative
 * looks up) that carries a projectile of the given speed over a horizontal
 * range to a target height under gravity. 0 when it cannot reach. */
int BotGrenadePitch(float range, float height, float speed, float gravity, float *pitch);
/* T5 Bot_UpdateGrenadeThrow's correction added to that pitch: 1..3 degrees
 * for a blade, 7..10 when the target is above or below, else 10..13. roll is
 * a uniform 0..9999 draw. */
float BotGrenadePitchOffset(int kind, float height, unsigned roll);

typedef struct {
    int lethal, tactical;          /* CODOL_BOT_GRENADE_* or NONE when unusable */
    float lethalRadius, tacticalRadius;
    int visible;                   /* enemy seen in this command */
    int sinceSeen;                 /* ms since the enemy was last seen */
    int minHidden;                 /* T5 sv_botMin/MaxGrenadeTime draw, ms */
    float range;                   /* to the enemy's last known position */
    float closeDistance;           /* sv_botCloseDistance */
    int health, maxHealth;
    int clipEmpty;
    unsigned roll;                 /* uniform 0..9999 */
} bot_grenade_situation_t;
/* Which slot to throw now (CODOL_BOT_SLOT_*), or -1. *midpoint is set when
 * the throw should land halfway to the enemy (smoke cover) rather than on
 * the last known position. */
int BotGrenadeChoose(const bot_grenade_situation_t *s, int *midpoint);

/* T5 release test: aimed within 5 degrees, the offhand is raised and held
 * (native weaponstate 17/18 or later), and a cooking grenade has burned down
 * to the drawn fuse unless the bot is being hurt. A hold past maxHold always
 * releases. Returns 1 to let go of the button. */
int BotGrenadeRelease(float yawError, float pitchError, int weaponstate, int cooks,
                            int damaged, int fuseLeft, int fuseThreshold, int heldMs, int maxHold);

/* T5 bot_equipment_think's placement test on a path: the bot is within 64
 * units of a waypoint where the route turns by more than 45 degrees. On 1,
 * *yaw is the direction the next leg leaves the corner. */
int BotPathCornerPlant(const float *origin, const float *corner, const float *next, float *yaw);

#endif

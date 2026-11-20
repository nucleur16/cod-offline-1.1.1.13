#ifndef _SV_BOTS_H
#define _SV_BOTS_H

#include "player.h"
#include "sys_cod4defs.h"
#include "q_shared.h"

/* server.h leaks a bare #pragma pack(1). sv_bots.cpp includes this header
 * first (natural layout: weapon at +32) while ol_bot.c, sv_main.c and
 * scr_vm_main.c include it after server.h (packed: weapon at +29), so the
 * combat-bot command builder read the script weapon request from the wrong
 * bytes and every scripted switchToWeapon on a bot was dropped (a bot could
 * never raise briefcase_bomb_mp to plant). Pin one layout everywhere. */
#pragma pack(push, 8)
/* Should keep attribute and this field layout.
 * Met situation where simple vector read into moveTo broke whole
 *  structure.
*/
/* !!!DO NOT CHANGE ALIGNED TO 1!!! */
/* T-Max: I have no idea why it changes ints' sizes to 1 byte and brokes whole structure x2. */
typedef struct __attribute__ ((aligned (4))) BotMovementInfo_t
{
    /* Actions */
    int buttons;
    /* Movement */
    int doMove;
    float moveTo[2];
    /* Rotation */
    int rotIterCount;
    short int rotFrac[2];
    qboolean lastAliveState;
    byte useSpamDelay;
    /* Weapon */
#ifdef OL_SERVER
    unsigned int weapon; /* native composed-weapon index; not an IW3 byte */
#else
    byte weapon;
#endif
} BotMovementInfo_t;

#pragma pack(pop)

extern BotMovementInfo_t g_botai[MAX_CLIENTS];

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void Scr_AddBotsMovement();
qboolean shouldSpamUseButton(gentity_t *bot);
void Bot_SetBotWeapon(int, unsigned int);


#ifdef __cplusplus
}
#endif //__cplusplus
#endif //_SV_BOTS_H

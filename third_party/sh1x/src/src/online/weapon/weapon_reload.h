#ifndef WEAPON_RELOAD_H
#define WEAPON_RELOAD_H
#include "player.h"
#include "xassets/weapondef.h"
/* C1 inventory byte2 and dual-mag reload producers; direct native callers. */
void WeaponReloadSelected(playerState_t *,unsigned weapon);
void WeaponReloadRemoved(playerState_t *,unsigned weapon);
void WeaponReloadClipFinished(playerState_t *);
int WeaponReloadInfo(const playerState_t *,const struct WeaponDef *,int empty,
    int *duration,int *add_time,int *fast);
int FastMagSetReloadingState(playerState_t *);
int FastMagReloadDelayedAction(playerState_t *);
int FastMagReloadClip(playerState_t *);
#endif

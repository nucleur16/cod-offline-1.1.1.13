#include "weapon_lock.h"
#include <string.h>
#include <math.h>
static int vector_valid(const float *v)
{
    return !v || (isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]));
}
void ResetWeaponLock(weapon_lock_t *state)
{
    if(!state)return;
    memset(state,0,sizeof(*state));state->entity=2047;
}
int SetWeaponLock(weapon_lock_t *state,unsigned entity,const float *position,
    int finalize,const float *missile_offset,int top_attack)
{
    weapon_lock_t next;
    if(!state || (!position && entity>=2047) || !vector_valid(position) ||
       (finalize && !vector_valid(missile_offset)))return 0;
    next=*state;
    if(position){next.flags|=0x40;memcpy(next.position,position,sizeof(next.position));}
    else {next.flags&=~0x40u;next.entity=entity;}
    next.flags|=1;
    if(!finalize)next.flags&=~14u;
    else {
        next.flags|=2;
        if(missile_offset)memcpy(next.missile_offset,missile_offset,sizeof(next.missile_offset));
        else memset(next.missile_offset,0,sizeof(next.missile_offset));
        /* Retail ORs the chosen mode. Only start/free clear previous modes. */
        next.flags|=top_attack ? 4:8;
    }
    *state=next;return 1;
}
void FreeWeaponLock(weapon_lock_t *state)
{
    if(!state)return;
    state->flags&=~15u;state->entity=2047;
}
int SetWeaponLockWarning(weapon_lock_t *state,unsigned mask,int enabled)
{
    if(!state || (mask!=0x10 && mask!=0x20))return 0;
    if(enabled)state->flags|=mask;else state->flags&=~mask;
    return 1;
}
int OverlayWeaponLock(player_fields_t *wire,const weapon_lock_t *state)
{
    player_fields_t next;unsigned found=0,i;
    if(!wire || !state || state->flags>127 || state->entity>2047 ||
       !vector_valid(state->position))return 0;
    next=*wire;
    for(i=0;i<PLAYER_FIELDS;++i) {
        unsigned offset=OL_PlayerField(i)->offset;
        if(offset==0x48c){next.field[i]=state->flags;++found;}
        else if(offset==0x490){next.field[i]=state->entity;++found;}
        else if(offset>=0x494 && offset<=0x49c && !(offset&3)) {
            memcpy(&next.field[i],&state->position[(offset-0x494)/4],4);++found;
        }
    }
    if(found!=5)return 0;
    *wire=next;return 1;
}

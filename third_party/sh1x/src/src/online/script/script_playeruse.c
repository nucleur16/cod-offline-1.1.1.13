#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "script_playeruse.h"
#include "scr_vm_functions.h"
#include "sv_state.h"
#include <stdint.h>
#include <string.h>

/* IW4 client+327c stores one disabled-use bit per target entity. This is
 * server selection state, not a playerState wire flag. It lives in IW4's
 * clientSession_t, which ClientSpawn preserves, so it survives a respawn and
 * is reset only with the client's connection and the level's input state. */
static uint32_t disabled[MAX_CLIENTS][(MAX_GENTITIES+31)/32];

static void set_player_use(scr_entref_t target,int blocked)
{
    gentity_t *player=Scr_GetEntity(0);
    int slot;
    uint32_t mask;
    if(!player || !player->client) {
        Scr_ParamError(0,"Not a player");return;
    }
    if(target.classnum || target.entnum>=MAX_GENTITIES) {
        Scr_ObjectError("Not an entity");return;
    }
    slot=player->s.number;
    if(slot<0 || slot>=MAX_CLIENTS) {
        Scr_ParamError(0,"Not a player");return;
    }
    mask=UINT32_C(1)<<(target.entnum&31);
    if(blocked)disabled[slot][target.entnum>>5]|=mask;
    else disabled[slot][target.entnum>>5]&=~mask;
}
/* IW4 4164d0 ->489a90 sets; 4d8d20 ->4bc370 clears. Both take
 * the player as script argument0 and the target as the method receiver. */
void Scr_DisablePlayerUse(scr_entref_t target) {set_player_use(target,1);}
void Scr_EnablePlayerUse(scr_entref_t target) {set_player_use(target,0);}

/* IW4 5fe737 calls4431e0 while constructing candidate use targets. */
int PlayerUseAllowed(const struct gentity_s *player,const struct gentity_s *target)
{
    int slot,number;
    if(!player || !player->client || !target)return 0;
    slot=player->s.number;number=target->s.number;
    if(slot<0 || slot>=MAX_CLIENTS || number<0 || number>=MAX_GENTITIES)return 0;
    /* hide()/showToPlayer use clientMask, including future connections.
     * SVF_NOCLIENT only suppresses snapshots: trigger_use_shared always sets
     * it on invisible use brushes, which must still offer hints and activate.
     * Rejecting that flag made every authored Zombies door unbuyable. */
    if((uint32_t)target->r.clientMask[(unsigned)slot>>5] &
       (UINT32_C(1)<<(slot&31)))return 0;
    return !(disabled[slot][(unsigned)number>>5]&(UINT32_C(1)<<(number&31)));
}
/* Reach of the generic use search (Player_UpdateCursorHints_400): the
 * distance from the player's eye to the centre of a candidate's absolute
 * bounds above which a trigger_use, item or usable script entity is not
 * offered.  IW3 MP hard-codes 128 and T5 MP registers player_useRadius 128
 * (g_main_mp.cpp:1872).  CoDOL's zombies maps come from Black Ops' single-
 * player zombies, whose Player_GetUseList (BlackOps.exe 817db0) has no such
 * dvar: every entity that is neither a grenade nor a vehicle is offered
 * within a constant 72.0 (818235, movss xmm0,[a2a8b8]).  Those maps' use
 * triggers are authored for that reach -- the zm_prototype_sh wall buys are
 * 3x56x12 plates on the chalk, the perk triggers the machine's own 32x30x103
 * -- so 128 offered them from almost twice as far, facing away included. */
float PlayerUseRadius(void)
{
    const char *zone=ServerZoneType();
    return (zone && !strcmp(zone,"zm")) ? 72.0f : 128.0f;
}
void PlayerUseResetClient(int slot)
{ if(slot>=0 && slot<MAX_CLIENTS)memset(disabled[slot],0,sizeof(disabled[slot])); }
void PlayerUseResetAll(void) {memset(disabled,0,sizeof(disabled));}
/* IW4 G_FreeEntityRefs 416250 calls the clear-bit helper at416392 for
 * every active player. Clear all rows here as inactive rows reset on spawn. */
void PlayerUseClearTarget(int number)
{
    unsigned slot;
    uint32_t keep;
    if(number<0 || number>=MAX_GENTITIES)return;
    keep=~(UINT32_C(1)<<(number&31));
    for(slot=0;slot<MAX_CLIENTS;++slot)disabled[slot][(unsigned)number>>5]&=keep;
}
#endif

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentPlayerUseEmpty(void) {return resident_static_zero(disabled,sizeof(disabled));}
#endif

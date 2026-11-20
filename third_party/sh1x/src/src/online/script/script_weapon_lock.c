#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "weapon_lock.h"
static weapon_lock_t *player_lock(scr_entref_t ref)
{
    weapon_lock_t *state;
    if(ref.classnum || ref.entnum>=32 || !g_entities[ref.entnum].client) {
        Scr_ObjectError("weapon lock requires a player");return NULL;
    }
    /* IW4's methods write the addressed player's own playerState
     * (0x5dd500 through ent->client at +0x158); so does this. The lock then
     * travels with that state into snapshots, the archive and any spectator
     * following it. */
    state=PlayerStateWeaponLock(&g_entities[ref.entnum].client->ps);
    if(!state)Scr_ObjectError("weapon lock requires a player");
    return state;
}
static void set_lock(scr_entref_t ref,int finalize)
{
    weapon_lock_t *state=player_lock(ref);
    vec3_t position,offset;const float *point=NULL,*relative=NULL;
    unsigned entity=0,type;int count,top_attack=0;
    if(!state)return;
    count=Scr_GetNumParam();
    if(count<1){Scr_Error("weapon lock requires an entity or position");return;}
    type=Scr_GetType(0);
    if(type==VAR_VECTOR){Scr_GetVector(0,position);point=position;}
    else if(type==VAR_POINTER && Scr_GetPointerType(0)==VAR_ENTITY) {
        gentity_t *target=Scr_GetEntity(0);
        if(!target || target->s.number<0 || target->s.number>=ENTITYNUM_WORLD) {
            Scr_Error("weapon lock target is not a gameplay entity");return;
        }
        entity=(unsigned)target->s.number;
    } else {Scr_Error("weapon lock target must be an entity or position");return;}
    if(finalize && count>=2){Scr_GetVector(1,offset);relative=offset;}
    /* IW4 weaponLockFinalize( target, offset, topAttack ). */
    if(finalize && count>=3)top_attack=Scr_GetInt(2)!=0;
    if(!SetWeaponLock(state,entity,point,finalize,relative,top_attack))
        Scr_Error("weapon lock target or offset is invalid");
}
void Scr_WeaponLockStart(scr_entref_t ref){set_lock(ref,0);}
void Scr_WeaponLockFinalize(scr_entref_t ref){set_lock(ref,1);}
void Scr_WeaponLockFree(scr_entref_t ref)
{ weapon_lock_t *state=player_lock(ref);if(state)FreeWeaponLock(state); }
static void warning(scr_entref_t ref,unsigned flag)
{
    weapon_lock_t *state=player_lock(ref);int enabled;
    if(!state)return;
    enabled=Scr_GetInt(0);
    SetWeaponLockWarning(state,flag,enabled!=0);
}
void Scr_WeaponLockTargetTooClose(scr_entref_t ref){warning(ref,0x10);}
void Scr_WeaponLockNoClearance(scr_entref_t ref){warning(ref,0x20);}
#endif

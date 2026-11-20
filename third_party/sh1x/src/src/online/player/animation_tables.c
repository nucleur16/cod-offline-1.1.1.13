#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "animation_tables.h"
#include "qcommon.h"
#include "weapon_state.h"
#include "player.h"
#include "xassets/weapondef.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
_Static_assert(sizeof(anim_script_slot_t)==0x204,"native animation script slot ABI");
_Static_assert(offsetof(anim_script_slot_t,items)==4,"native script item array ABI");
static void *script_owner;
_Alignas(16) unsigned char AnimationRecords[ANIM_RECORD_COUNT*0x60];
unsigned AnimationCount;
void BeginAnimationRecords(unsigned count)
{
    if(!count || count>ANIM_RECORD_COUNT) {
        Com_Error(ERR_DROP,"Invalid CoDOL animation tree size: %u (maximum %u)",count,ANIM_RECORD_COUNT);
        return;
    }
    memset(AnimationRecords,0,sizeof(AnimationRecords));
    AnimationCount=count;
}
typedef struct { unsigned char *data; uint32_t count,capacity; } command_list_t;
static command_list_t command_lists[2048];
static anim_script_slot_t extra[2][ANIM_MOVE_COUNT-ANIM_LEGACY_MOVE_COUNT];
static anim_script_slot_t extra_events[ANIM_EVENT_COUNT-ANIM_LEGACY_EVENT_COUNT];
extern struct bgs_s *bgs;
static uint32_t conditions[64][9][2];
static uint16_t known_conditions[64];
/* Ten unchanged IW3 indices, then nine CoDOL names in measured order. */
static const unsigned char condition_kind[19]={0,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1};

void ResetAnimMoveScripts(void *owner)
{
    for(unsigned i=0;i<2048;++i) free(command_lists[i].data);
    memset(command_lists,0,sizeof(command_lists));
    memset(extra,0,sizeof(extra));
    memset(extra_events,0,sizeof(extra_events));
    memset(conditions,0,sizeof(conditions));
    memset(known_conditions,0,sizeof(known_conditions));
    script_owner=owner;
    if(!owner) {
        memset(AnimationRecords,0,sizeof(AnimationRecords));
        AnimationCount=0;
    }
}
static command_list_t *command_list(void *item)
{
    uintptr_t base=(uintptr_t)script_owner+0x199b4, address=(uintptr_t)item;
    if(!script_owner || address<base || (address-base)%0x100 || (address-base)/0x100>=2048) {
        Com_Error(ERR_DROP,"Invalid native animation command item"); return NULL;
    }
    command_list_t *list=&command_lists[(address-base)/0x100];
    if(*(uint32_t *)((unsigned char *)item+0x7c)!=list->count) {
        Com_Error(ERR_DROP,"Native animation command count mismatch"); return NULL;
    }
    return list;
}
void *AppendAnimCommand(void *item)
{
    command_list_t *list=command_list(item);
    if(!list) return NULL;
    if(list->count==list->capacity) {
        /* Keep native signed command counts and allocation multiplication valid. */
        if(list->capacity>INT_MAX/32) {
            Com_Error(ERR_DROP,"Native animation command allocation overflow"); return NULL;
        }
        uint32_t capacity=list->capacity ? list->capacity*2 : 8;
        unsigned char *data=realloc(list->data,(size_t)capacity*16);
        if(!data) { Com_Error(ERR_DROP,"Out of memory for native animation commands"); return NULL; }
        list->data=data; list->capacity=capacity;
    }
    void *out=list->data+(size_t)list->count*16;
    memset(out,0,16);
    ++list->count;
    *(uint32_t *)((unsigned char *)item+0x7c)=list->count;
    return out;
}
void *AnimCommands(void *item)
{
    command_list_t *list=command_list(item);
    return list ? list->data : NULL;
}
anim_script_slot_t *AnimMoveScript(void *owner,int state,int movement,int canned)
{
    if(!owner || owner!=script_owner || state!=0 || movement<0 || movement>=ANIM_MOVE_COUNT ||
        (canned!=0 && canned!=1)) return NULL;
    if(movement<ANIM_LEGACY_MOVE_COUNT)
        return (anim_script_slot_t *)((unsigned char *)owner+
            (canned ? 0x116b0 : 0xc004)+(size_t)movement*sizeof(anim_script_slot_t));
    return &extra[canned][movement-ANIM_LEGACY_MOVE_COUNT];
}
anim_script_slot_t *RequireAnimMoveScript(void *owner,int state,int movement,int canned)
{
    anim_script_slot_t *slot=AnimMoveScript(owner,state,movement,canned);
    if(!slot) Com_Error(ERR_DROP,"Invalid native CoDOL animation movement lookup: state %d, movement %d, canned %d",
        state,movement,canned);
    return slot;
}
anim_script_slot_t *AnimEventScript(void *owner,int event)
{
    if(!owner || owner!=script_owner || event<0 || event>=ANIM_EVENT_COUNT) return NULL;
    if(event<ANIM_LEGACY_EVENT_COUNT)
        return (anim_script_slot_t *)((unsigned char *)owner+0x16f60+
            (size_t)event*sizeof(anim_script_slot_t));
    return &extra_events[event-ANIM_LEGACY_EVENT_COUNT];
}
anim_script_slot_t *RequireAnimEventScript(void *owner,int event)
{
    anim_script_slot_t *slot=AnimEventScript(owner,event);
    if(!slot) Com_Error(ERR_DROP,"Invalid native CoDOL animation event lookup: event %d",event);
    return slot;
}
static uint32_t *condition_value(int client,int condition,int write)
{
    if(!script_owner || script_owner!=(void *)bgs || client<0 || client>=64 ||
        condition<0 || condition>=ANIM_CONDITION_COUNT) return NULL;
    if(condition<ANIM_LEGACY_CONDITION_COUNT)
        return (uint32_t *)((unsigned char *)script_owner+0x99e5c+(size_t)client*0x4cc+condition*8);
    condition-=ANIM_LEGACY_CONDITION_COUNT;
    if(write) known_conditions[client]|=(uint16_t)(1u<<condition);
    if(!(known_conditions[client]&(1u<<condition))) {
        /* A CoDOL condition no native producer has written yet reads as its
         * zero value (false / "none" / string index 0) rather than ending
         * the level: the animation scripts test akimbo, fastmantle,
         * riotshieldnext, divedirection and scripted on ordinary moves such
         * as mantling. Reported once per condition in developer mode. */
        static uint32_t zero[2]; static uint16_t reported;
        if(!(reported&(1u<<condition))) {
            reported|=(uint16_t)(1u<<condition);
            ((void)0);
        }
        zero[0]=zero[1]=0; return zero;
    }
    return conditions[client][condition];
}
void SetAnimConditionValue(int client,int condition,uint64_t value)
{
    uint32_t *out=condition_value(client,condition,1);
    if(!out) { Com_Error(ERR_DROP,"Invalid native animation condition setter: client %d, condition %d",client,condition); return; }
    out[0]=(uint32_t)value; out[1]=(uint32_t)(value>>32);
}
void SetAnimConditionBit(int client,int condition,int bit)
{
    if(bit<0 || bit>=64) { Com_Error(ERR_DROP,"Invalid animation condition bit: %d",bit); return; }
    SetAnimConditionValue(client,condition,UINT64_C(1)<<bit);
}
/* Per-frame producer for the CoDOL conditions the playerState decides.
 * Called from ClientEndFrame beside the other native publishers.
 *   13 akimbo                 - the hand extension's dual-wield flag.
 *   16 playeranimtypeprimary  - held weapon's type, independently of an active
 *                               offhand selected by condition 0.
 * fastmantle is written by the mantle move (SetFastMantleAnimCondition);
 * riotshieldnext, divedirection and scripted stay at their zero value: no
 * native producer measures them yet. */
void UpdateAnimConditionsFromPlayerState(int client,const struct playerState_s *ps)
{
    const struct WeaponDef *held;
    if(!script_owner || script_owner!=(void *)bgs || client<0 || client>=64 || !ps) return;
    SetAnimConditionValue(client,13,WeaponStateIsDualWielding(ps) ? 1u : 0u);
    /* C1 405f78..405fc9 uses ps+33c (held weapon), while 405f0d..405f25
     * may select the offhand for condition0. Native ps.weapon owns that held
     * identity; no weapon-class coercion or loadout-slot assumption applies. */
    held=BG_GetWeaponDef(ps->weapon);
    if(!held || held->playerAnimType<0 || held->playerAnimType>=64) {
        Com_Error(ERR_DROP,"Invalid held weapon animation type for client %d",client);
        return;
    }
    SetAnimConditionBit(client,16,held->playerAnimType);
}
/* C1 PM_MantleMove stores (mantleState.flags >> 6) & 1 into condition 17
 * (clientinfo +0x92cf8 = conditions +0x92c70 + 17*8, both words) at
 * 0x0040a881..0x0040a8b3, immediately before it plays the mantle move. */
void SetFastMantleAnimCondition(int client,int fast)
{
    if(!script_owner || script_owner!=(void *)bgs || client<0 || client>=64) return;
    SetAnimConditionValue(client,17,fast ? 1u : 0u);
}
int AnimConditionMatches(const void *client_info,const anim_condition_t *condition)
{
    uintptr_t base=(uintptr_t)script_owner+0x99a04, address=(uintptr_t)client_info;
    uint32_t *value; int client;
    if(!script_owner || !condition || address<base || (address-base)%0x4cc ||
        (address-base)/0x4cc>=64) {
        Com_Error(ERR_DROP,"Invalid animation condition client"); return 0;
    }
    client=(int)((address-base)/0x4cc);
    value=condition_value(client,condition->index,0);
    if(!value) {
        Com_Error(ERR_DROP,"Animation condition has no native state: client %d, condition %d",client,condition->index); return 0;
    }
    if(condition_kind[condition->index]) return value[0]==condition->low;
    return !!((value[0]&condition->low)|(value[1]&condition->high));
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}
int PersistentTailAnimTablesCanRelease(void){
 if(AnimationCount>ANIM_RECORD_COUNT)return 0;
 for(unsigned i=0;i<2048;++i){const command_list_t *c=&command_lists[i];
 if(!c->data){if(c->count||c->capacity)return 0;continue;}
 if(!script_owner||!c->capacity||c->count>c->capacity)return 0;
 for(unsigned j=0;j<i;++j)if(c->data==command_lists[j].data)return 0;}return 1;}

int PersistentTailAnimTablesEmpty(void){return !script_owner&&!AnimationCount&&resident_tail_zero(command_lists,sizeof(command_lists))&&
 resident_tail_zero(extra,sizeof(extra))&&resident_tail_zero(extra_events,sizeof(extra_events))&&
 resident_tail_zero(conditions,sizeof(conditions))&&resident_tail_zero(known_conditions,sizeof(known_conditions))&&
 resident_tail_zero(AnimationRecords,sizeof(AnimationRecords));}
void PersistentTailAnimTablesReset(void){ResetAnimMoveScripts(NULL);}
#endif

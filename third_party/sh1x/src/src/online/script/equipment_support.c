/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "equipment_support.h"
#include "scr_vm_functions.h"
#include "radar_state.h"
#include <stdint.h>

/* C1 55caf0: makeScrambler(owner). The script mover itself emits the
 * scrambler flag; client distance/team checks decide who receives jamming.
 * This carrier and otherEntityNum already survive native entity archives. */
void Scr_MakeScrambler(scr_entref_t ref)
{
    gentity_t *entity,*owner;
    if(Scr_GetNumParam()!=1) {
        Scr_Error("makeScrambler requires exactly one entity argument");return;
    }
    if(ref.classnum || ref.entnum>=MAX_GENTITIES) {
        Scr_ObjectError("Not an entity");return;
    }
    entity=&g_entities[ref.entnum];
    if(entity->s.eType!=ET_SCRIPTMOVER) {
        Scr_ObjectError("Can only be called on a script mover");return;
    }
    if((uint32_t)entity->s.lerp.eFlags&UINT32_C(0x80000000)) {
        Scr_Error("Cannot change a 'willNeverChange' entity");return;
    }
    owner=Scr_GetEntity(0);
    if(!owner)return;
    /* C1 55cbf8 reads the actual owner entity number, not player/clientNum.
     * Any real entity is accepted; do not impose a player-only restriction. */
    entity->s.otherEntityNum=owner->s.number;
    entity->s.lerp.eFlags|=PS_EF_RADAR_JAMMER;
}
/* codol_setScramblerActive( <bool> ): switch a makeScrambler mover's jamming
 * off and back on (an EMP grenade disables equipment for a while). Clearing
 * the radar-jammer flag is all C1 reads; otherEntityNum keeps the owner. */
void Scr_SetScramblerActive(scr_entref_t ref)
{
    gentity_t *entity;
    if(Scr_GetNumParam()!=1) {
        Scr_Error("codol_setScramblerActive requires exactly one argument");return;
    }
    if(ref.classnum || ref.entnum>=MAX_GENTITIES) {
        Scr_ObjectError("Not an entity");return;
    }
    entity=&g_entities[ref.entnum];
    if(entity->s.eType!=ET_SCRIPTMOVER) {
        Scr_ObjectError("Can only be called on a script mover");return;
    }
    if(Scr_GetInt(0))
        entity->s.lerp.eFlags|=PS_EF_RADAR_JAMMER;
    else
        entity->s.lerp.eFlags&=~PS_EF_RADAR_JAMMER;
}
int OverlayEquipmentScrambler(entity_state_t *wire,const entityState_t *entity)
{
    entity_state_t next;
    unsigned i,found=0;
    if(!wire || !entity)return 0;
    if(entity->eType!=ET_SCRIPTMOVER)return 1;
    if(wire->type!=ET_SCRIPTMOVER)return 0;
    if(!(entity->lerp.eFlags&PS_EF_RADAR_JAMMER))return 1;
    if(entity->otherEntityNum<0 || entity->otherEntityNum>=MAX_GENTITIES)return 0;
    next=*wire;
    for(i=0;i<ENTITY_FIELDS;++i) {
        const entity_field_t *field=EntityField(next.type,i);
        if(!field)return 0;
        if(field->offset==0x008) {
            next.field[i]|=UINT32_C(0x200000);found|=1;
        }
        if(field->offset==0x080) {
            next.field[i]=(unsigned)entity->otherEntityNum;found|=2;
        }
    }
    if(found!=3)return 0;
    *wire=next;return 1;
}
#endif

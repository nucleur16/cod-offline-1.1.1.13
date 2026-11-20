/* Native CoDOL ability VM methods. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "abilities.h"
#include "playerstate_ext.h"
#include "perk_runtime.h"
#include <math.h>
enum { ABILITY_SET,ABILITY_UNSET,ABILITY_HAS,ABILITY_CLEAR,ABILITY_VALUE };
static void ability_method(scr_entref_t ref,unsigned operation)
{
    ps_ext_t *extension; client_abilities_t next={0};
    ability_definition_t definition; const engine_table_t *table;
    const char *name; int status,present; float value=0;
    unsigned parameters=operation==ABILITY_CLEAR ? 0 : operation==ABILITY_VALUE ? 2 : 1;
    if(ref.classnum) { Scr_ObjectError("not an entity"); return; }
    if(ref.entnum>=MAX_CLIENTS || !g_entities[ref.entnum].client) {
        Scr_ObjectError(va("entity %i is not a player",ref.entnum)); return;
    }
    /* C1 518850 goes straight from receiver validation to clearing four
     * masks. Like ClearPerks, it does not enforce a zero-argument call. */
    if(operation!=ABILITY_CLEAR && (unsigned)Scr_GetNumParam()!=parameters) {
        Scr_Error(va("ability modifier operation requires %u parameters",parameters)); return;
    }
    if(operation!=ABILITY_CLEAR) {
        name=Scr_GetString(0);
        if(LoadEngineStringTable("mp/coAbilityModifiersTable.csv",&table)!=1) {
            Scr_Error("ability modifier table unavailable"); return;
        }
        status=ReadAbilityDefinition(table,name,&definition);
        if(status!=1) {
            Scr_ParamError(0,va("unknown or malformed ability modifier '%s'",name)); return;
        }
        if(operation==ABILITY_VALUE) {
            value=Scr_GetFloat(1);
            if(definition.value_slot<0 || !isfinite(value)) {
                Scr_ParamError(1,"ability modifier requires a value slot and finite float"); return;
            }
        }
    }
    extension=PlayerStateExt(&g_entities[ref.entnum].client->ps);
    if(!extension) { Scr_Error("ability playerState storage unavailable"); return; }
    GetScriptAbilities(ref.entnum,&next);
    if(operation==ABILITY_HAS) {
        if(!HasAbility(&extension->abilities,&definition,&present)) {
            Scr_Error("invalid ability modifier query"); return;
        }
        Scr_AddInt(present); return;
    }
    switch(operation) {
    case ABILITY_SET: status=SetAbility(&next,&definition); break;
    case ABILITY_UNSET: status=UnsetAbility(&next,&definition); break;
    case ABILITY_VALUE: status=SetAbilityValue(&next,&definition,value); break;
    default: ClearAbilities(&next); status=1; break;
    }
    if(!status) { Scr_Error("invalid ability modifier update"); return; }
    if(!CommitScriptAbilities(ref.entnum,&next,
        operation==ABILITY_VALUE ? definition.value_slot : -1))
        Scr_Error("ability effect publication failed");
}
void Scr_SetAbilityModifier(scr_entref_t ref) { ability_method(ref,ABILITY_SET); }
void Scr_UnsetAbilityModifier(scr_entref_t ref) { ability_method(ref,ABILITY_UNSET); }
void Scr_HasAbilityModifier(scr_entref_t ref) { ability_method(ref,ABILITY_HAS); }
void Scr_ClearAbilityModifiers(scr_entref_t ref) { ability_method(ref,ABILITY_CLEAR); }
void Scr_SetAbilityModifierValue(scr_entref_t ref) { ability_method(ref,ABILITY_VALUE); }
/* sh1x script adaptation API, not a claimed C1 builtin. Exposes the native
 * selected-perk aggregation, including numeric effects without wire slots. */
void Scr_GetSelectedPerkModifierValue(scr_entref_t ref)
{
    const engine_table_t *table;
    ability_definition_t definition;
    const char *name;
    float value;
    if(ref.classnum) { Scr_ObjectError("not an entity"); return; }
    if(ref.entnum>=MAX_CLIENTS || !g_entities[ref.entnum].client) {
        Scr_ObjectError(va("entity %i is not a player",ref.entnum)); return;
    }
    if(Scr_GetNumParam()!=1) { Scr_Error("getSelectedPerkModifierValue requires one modifier name"); return; }
    name=Scr_GetString(0);
    if(LoadEngineStringTable("mp/coAbilityModifiersTable.csv",&table)!=1 ||
       ReadAbilityDefinition(table,name,&definition)!=1) {
        Scr_ParamError(0,va("unknown or malformed ability modifier '%s'",name)); return;
    }
    if(GetSelectedPerkValue(&g_entities[ref.entnum].client->ps,definition.id,&value))
        Scr_AddFloat(value);
    // No selected numeric contribution returns undefined, never a made-up default.
}
#endif

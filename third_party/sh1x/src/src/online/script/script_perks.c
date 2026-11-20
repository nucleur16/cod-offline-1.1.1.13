/* Native C1 perk slot VM methods. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "playerstate_ext.h"
#include "perk_runtime.h"
#include "sv_state.h"
#include "qcommon.h"
#include <string.h>
enum { PERK_SET, PERK_HAS, PERK_UNSET, PERK_CLEAR };
/* Adaptation for Zombies. The T5 zombies scripts name the perk-machine perks
 * by their T5 names (specialty_armorvest, specialty_quickrevive,
 * specialty_rof, specialty_fastreload, ...; the map's zombie_vending
 * script_noteworthy carries the same names). C1 defines those effects as the
 * no_slot rows specialty_zombie_<T5 suffix> (ids 43-57). Under a zm zone a
 * T5 name resolves to that row when one exists, so speed cola is not the
 * multiplayer perk1 Sleight of Hand that would evict a roster perk1 perk,
 * and the four other machine perks are known at all. */
static int read_definition(const engine_table_t *table,const char *name,
                           perk_definition_t *out)
{
    const char *zone=ServerZoneType();
    if(name && zone && !strcmp(zone,"zm") && !Q_stricmpn(name,"specialty_",10) &&
       Q_stricmpn(name,"specialty_zombie_",17)) {
        char alias[96];
        perk_definition_t zombie;
        if(strlen(name)-10 < sizeof(alias)-17) {
            Com_sprintf(alias,sizeof(alias),"specialty_zombie_%s",name+10);
            if(ReadPerkDefinition(table,alias,&zombie)==1 &&
               zombie.slot==PERK_NO_SLOT) {
                *out=zombie; return 1;
            }
        }
    }
    return ReadPerkDefinition(table,name,out);
}
static void perk_method(scr_entref_t ref, unsigned operation)
{
    ps_ext_t *extension;
    perk_definition_t definition={0,0};
    const engine_table_t *table;
    int status, present;
    perk_state_t next;
    slotless_perks_t slotless;
    int slotted;
    if(ref.classnum) { Scr_ObjectError("not an entity"); return; }
    if(ref.entnum>=MAX_CLIENTS || !g_entities[ref.entnum].client) {
        Scr_ObjectError(va("entity %i is not a player",ref.entnum)); return;
    }
    /* C1 ClearPerks has no parameter-count check; the other three require 1. */
    if(operation!=PERK_CLEAR) {
        const char *name;
        /* IW4's hasPerk takes an optional second flag (its scripts pass
         * "true" for the upgraded-perk check); the setters take one name. */
        if(Scr_GetNumParam()<1 || Scr_GetNumParam()>(operation==PERK_HAS ? 2 : 1)) {
            Scr_Error("perk operation requires one perk name"); return;
        }
        name=Scr_GetString(0);
        /* IW4's "specialty_null" is the empty perk slot: its scripts give it
         * and query it like any other perk and the engine answers with a
         * no-op. CoDOL's table has no such row, so it is answered here. */
        if(name && !Q_stricmp(name,"specialty_null")) {
            if(operation==PERK_HAS) Scr_AddInt(0);
            return;
        }
        if(LoadEngineStringTable("mp/coPerkStatsTable.csv",&table)!=1) {
            Scr_Error("perk table unavailable"); return;
        }
        if(read_definition(table,name,&definition)!=1) {
            /* The IW4 gametype scripts name IW4 perks that CoDOL's table does
             * not carry. Adaptation: an unknown name is reported once in
             * developer mode and treated as absent, instead of ending the
             * spawn. C1 itself rejects unknown names with this error. */
            static char reported[32][48]; static unsigned reported_count;
            unsigned i; int seen=0;
            for(i=0;i<reported_count;++i) if(!strcmp(reported[i],name)) { seen=1; break; }
            if(!seen && reported_count<32) {
                Q_strncpyz(reported[reported_count++],name,48);
                Com_DPrintf(CON_CHANNEL_SERVER,"CoDOL perk '%s' is not in mp/coPerkStatsTable.csv; "
                    "treated as absent (unknown or malformed perk in C1)\n",name);
            }
            if(operation==PERK_HAS) Scr_AddInt(0);
            return;
        }
        if(definition.slot==PERK_NO_SLOT && definition.id>=SLOTLESS_PERK_IDS) {
            Scr_Error(va("perk '%s' id %u is outside the slotless perk set",name,definition.id));
            return;
        }
    }
    extension=PlayerStateExt(&g_entities[ref.entnum].client->ps);
    if(!extension || !GetSlotlessPerks((unsigned)ref.entnum,&slotless)) {
        Scr_Error("perk playerState storage unavailable"); return;
    }
    next=extension->perks;
    slotted=operation==PERK_CLEAR || definition.slot!=PERK_NO_SLOT;
    switch(operation) {
    case PERK_SET:
        status=slotted ? SetPerk(&next,&definition) : SetSlotlessPerk(&slotless,&definition);
        break;
    case PERK_UNSET:
        status=slotted ? UnsetPerk(&next,&definition) : UnsetSlotlessPerk(&slotless,&definition);
        break;
    case PERK_HAS:
        if(!(slotted ? HasPerk(&extension->perks,&definition,&present)
                     : HasSlotlessPerk(&slotless,&definition,&present))) {
            Scr_Error("invalid perk query"); return;
        }
        Scr_AddInt(present); return;
    default:
        /* C1 ClearPerks empties the eight slots; the slotless set goes too. */
        ClearPerks(&next); memset(&slotless,0,sizeof(slotless)); status=1; break;
    }
    if(!status || !CommitPerkState(ref.entnum,&next,&slotless))
        Scr_Error("invalid perk effect update or malformed modifier table");
}
void Scr_SetPerk(scr_entref_t ref) { perk_method(ref,PERK_SET); }
void Scr_HasPerk(scr_entref_t ref) { perk_method(ref,PERK_HAS); }
void Scr_UnsetPerk(scr_entref_t ref) { perk_method(ref,PERK_UNSET); }
void Scr_ClearPerks(scr_entref_t ref) { perk_method(ref,PERK_CLEAR); }
#endif

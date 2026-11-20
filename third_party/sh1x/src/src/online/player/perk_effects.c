#include "perk_effects.h"
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int equal_name(const char *a, const char *b)
{
    unsigned x,y;
    if (!a || !b) return 0;
    do {
        x=(unsigned char)*a++; y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+='a'-'A';
        if(y>='A' && y<='Z')y+='a'-'A';
        if(x!=y)return 0;
    } while(x);
    return 1;
}
static int valid_table(const engine_table_t *table, int columns)
{
    return table && table->rowCount>=0 && table->columnCount>=columns &&
        (!table->rowCount || table->values) &&
        (size_t)table->rowCount<=SIZE_MAX/sizeof(*table->values)/(size_t)table->columnCount;
}
static const char *const *find_row(const engine_table_t *table,
    unsigned column,const char *key)
{
    int row;
    for(row=table->rowCount-1;row>=0;--row) {
        const char *const *cells=table->values+(size_t)row*table->columnCount;
        if(equal_name(cells[column],key))return cells;
    }
    return NULL;
}
static int number(const char *text,double *out,int *present)
{
    char *end;
    double value;
    if(!text)return 0;
    *present=!!*text;
    if(!*present) {*out=0;return 1;}
    errno=0;value=strtod(text,&end);
    if(end==text || *end || errno==ERANGE || !isfinite(value) ||
        value>FLT_MAX || value< -FLT_MAX)return 0;
    *out=value;return 1;
}
typedef struct {
    const char *const *row;
    double sum;
    int numeric;
} contribution_t;

/* Add one selected perk row's modifier deltas. */
static int add_perk(const engine_table_t *perk_table,
    const engine_table_t *ability_table,uint32_t perk_id,unsigned slot,
    contribution_t *contributions)
{
    char key[16];
    const char *const *perk;
    perk_definition_t perk_definition;
    unsigned column,id;
    snprintf(key,sizeof(key),"%u",perk_id);
    perk=find_row(perk_table,0,key);
    if(!perk)return 0;
    if(ReadPerkDefinition(perk_table,perk[1],&perk_definition)!=1 ||
        perk_definition.id!=perk_id || perk_definition.slot!=slot)
        return -1;
    for(column=15;column<=21;column+=2) {
        const char *const *ability;
        ability_definition_t definition;
        double delta=0;
        int has_delta=0;
        if(!perk[column])return -1;
        if(!*perk[column]) {
            if(column<21 && (!perk[column+1] || *perk[column+1]))return -1;
            continue;
        }
        ability=find_row(ability_table,1,perk[column]);
        if(!ability)return 0;
        if(ReadAbilityDefinition(ability_table,perk[column],&definition)!=1)
            return -1;
        if(column<21 && !number(perk[column+1],&delta,&has_delta))return -1;
        id=definition.id;
        if(contributions[id].row && contributions[id].row!=ability)return -1;
        contributions[id].row=ability;
        contributions[id].sum+=delta;
        contributions[id].numeric|=has_delta;
        if(!isfinite(contributions[id].sum))return -1;
    }
    return 1;
}

int BuildPerkEffects(const perk_state_t *selection,
    const slotless_perks_t *slotless,
    const engine_table_t *perk_table,
    const engine_table_t *ability_table,perk_effects_t *out)
{
    contribution_t contributions[128]={{0}};
    perk_effects_t result={0};
    int value_owners[ABILITY_VALUES];
    unsigned selected,id;
    int status;
    if(!selection || !out || !valid_table(perk_table,22) ||
        !valid_table(ability_table,7))return -1;
    for(id=0;id<ABILITY_VALUES;++id)value_owners[id]=-1;
    for(selected=0;selected<PERK_SLOTS;++selected) {
        if(!selection->id[selected])continue;
        status=add_perk(perk_table,ability_table,selection->id[selected],selected,contributions);
        if(status!=1)return status;
    }
    /* Slotless (Zombies no_slot) perks add their deltas exactly as a slotted
     * selection does; a modifier both name is summed once with its default. */
    for(id=1;slotless && id<SLOTLESS_PERK_IDS;++id) {
        if(!(slotless->bits[id>>5]&(UINT32_C(1)<<(id&31))))continue;
        status=add_perk(perk_table,ability_table,id,PERK_NO_SLOT,contributions);
        if(status!=1)return status;
    }
    for(id=0;id<128;++id) {
        const char *const *row=contributions[id].row;
        ability_definition_t definition;
        double base,min,max,value;
        int has_base,has_min,has_max,numeric,clientside;
        uint32_t bit=UINT32_C(0x80000000)>>(id&31);
        if(!row)continue;
        if(!number(row[3],&base,&has_base) || !number(row[4],&min,&has_min) ||
            !number(row[5],&max,&has_max) || (has_min && has_max && min>max))return -1;
        if(!row[6] || (strcmp(row[6],"0") && strcmp(row[6],"1")))return -1;
        clientside=!strcmp(row[6],"1");
        if(ReadAbilityDefinition(ability_table,row[1],&definition)!=1)return -1;
        numeric=contributions[id].numeric || has_base || definition.value_slot>=0;
        if(!numeric && (has_min || has_max))return -1;
        value=base+contributions[id].sum;
        if(has_min && value<min)value=min;
        if(has_max && value>max)value=max;
        if(!isfinite(value) || value>FLT_MAX || value< -FLT_MAX)return -1;
        result.abilities.masks[id>>5]|=bit;
        /* Some client-visible flags (modPistolDeath) also have numeric
         * server parameters but no serialized value slot. */
        if(!clientside || (numeric && definition.value_slot<0)) {
            result.server_masks[id>>5]|=bit;
            if(numeric) {
                result.server_value_masks[id>>5]|=bit;
                result.server_values[id]=(float)value;
            }
        }
        if(numeric && definition.value_slot>=0) {
            int slot=definition.value_slot;
            /* Distinct active modifiers cannot silently overwrite one value. */
            if(value_owners[slot]>=0 && result.abilities.values[slot]!=(float)value)
                return -1;
            value_owners[slot]=(int)id;
            result.abilities.values[slot]=(float)value;
        }
    }
    *out=result;
    return 1;
}

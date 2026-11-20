#include "ol_profile.h"
#include <string.h>
int ProfileCharacter(const uint8_t *profile,size_t size,const structured_schema_t *schema,
    const char *faction,profile_character_t *out)
{
    profile_character_t next; const structured_definition_t *def;
    const uint8_t *record; size_t capacity; structured_field_t field;
    structured_path_t selector={NULL,0};
    structured_path_t path[4]={{"characterClasses",0},{NULL,0},{"slots",0},{NULL,0}};
    uint32_t value; unsigned i;
    if(!out || !faction) return 0;
    if(!strcmp(faction,"tf")) selector.name="tfCharacterIndex";
    else if(!strcmp(faction,"sc")) selector.name="scCharacterIndex";
    else if(!strcmp(faction,"zm")) selector.name="zmCharacterIndex";
    else return 0;
    if(!ProfileRecord(profile,size,schema,&def,&record,&capacity) ||
       !ResolveStructuredField(def,&selector,1,&field) || field.type.kind!=9 ||
       !StructuredReadScalar(&def->layout,9,field.logical_index,record,capacity,&value) ||
       value>=48) return 0;
    next.character_index=value; path[1].index=(int32_t)value;
    for(i=0;i<20;++i) {
        path[3].index=(int32_t)i;
        if(!ResolveStructuredField(def,path,4,&field) || field.type.kind!=9 ||
           !StructuredReadScalar(&def->layout,9,field.logical_index,record,capacity,&value)) return 0;
        /* Preserve stored item bits; marketplace resolution validates the ID. */
        next.items[i]=(uint16_t)value;
    }
    *out=next; return 1;
}

#include "structured_schema.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void put(uint8_t *p,uint32_t x)
{ unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(x>>(8*i)); }
static void paths(void)
{
    uint8_t map[40]={0};
    structured_property_t rootprops[]={
        {"players",{6,0},1,{0,0}}, {"teams",{7,0},7,{0,0}}};
    structured_property_t playerprops[]={
        {"score",{0,0},0,{0,0}}, {"flag",{2,0},1,{0,0}},
        {"name",{3,4},2,{0,0}}};
    structured_struct_t structs[]={
        {2,0,0,10,rootprops},{3,0,0,3,playerprops}};
    structured_array_t arrays[]={{2,{5,1},100}};
    structured_array_t enumarrays[]={{0,{1,0},100}};
    structured_enum_member_t members[]={{"allies",0x12340002}};
    structured_enum_t enums[]={{1,members}};
    structured_definition_t d={0};
    structured_path_t path[]={{"players",0},{NULL,1},{"score",0}};
    structured_field_t f={0},saved;
    d.root=(structured_type_t){5,0};d.structs=structs;d.struct_count=2;
    d.indexed_arrays=arrays;d.indexed_count=1;d.enum_arrays=enumarrays;d.enum_array_count=1;
    d.enums=enums;d.enum_count=1;d.layout=(structured_layout_t){map,sizeof(map),16};
    put(map+16,9);put(map+20,127);put(map+24,12);put(map+36,15);
    assert(ResolveStructuredField(&d,path,3,&f));
    assert(f.logical_index==4 && f.mapped_address==9 && f.type.kind==0);
    path[2].name="flag";assert(ResolveStructuredField(&d,path,3,&f));
    assert(f.logical_index==5 && f.mapped_address==127 && f.type.kind==2);
    path[2].name="name";assert(ResolveStructuredField(&d,path,3,&f));
    assert(f.logical_index==6 && f.type.index==4);
    saved=f;
    put(map+24,13);assert(!ResolveStructuredField(&d,path,3,&f));
    assert(!memcmp(&f,&saved,sizeof(f)));put(map+24,12);
    path[1].index=-1;assert(!ResolveStructuredField(&d,path,3,&f));
    path[1].index=2;assert(!ResolveStructuredField(&d,path,3,&f));
    path[1].index=1;path[2].name="Name";
    assert(!ResolveStructuredField(&d,path,3,&f));
    assert(!ResolveStructuredField(&d,path,2,&f));
    assert(!ResolveStructuredField(&d,NULL,1,&f));
    path[0].name="teams";path[1].name="allies";
    assert(ResolveStructuredField(&d,path,2,&f));
    assert(f.logical_index==9 && f.mapped_address==15 && f.type.kind==1);
    path[1].name="axis";assert(!ResolveStructuredField(&d,path,2,&f));
    path[1].name="allies";enumarrays[0].count_or_enum=1;
    assert(!ResolveStructuredField(&d,path,2,&f));enumarrays[0].count_or_enum=0;
    rootprops[1].logical_index=UINT32_MAX;
    assert(!ResolveStructuredField(&d,path,2,&f));
    path[0].name="players";path[1].name=NULL;path[2].name="score";
    arrays[0].element.index=2;assert(!ResolveStructuredField(&d,path,3,&f));
    arrays[0].element.index=1;structs[1].logical_count=UINT32_MAX;
    assert(!ResolveStructuredField(&d,path,3,&f));
}
static void text_values(void)
{
    uint8_t map[4]={3,0,0,0},record[12],saved[12];const char *text="unchanged";
    structured_enum_member_t members[]={{"allies",0x12340102},{"axis",3}};
    structured_enum_t enums[]={{2,members}};
    structured_definition_t d={0};
    d.root=(structured_type_t){3,5};d.layout=(structured_layout_t){map,4,12};
    d.enums=enums;d.enum_count=1;
    memset(record,0xa5,sizeof(record));
    assert(StructuredWriteText(&d,NULL,0,record,sizeof(record),"abcd"));
    assert(!memcmp(record+3,"abcd",5) && record[2]==0xa5 && record[8]==0xa5);
    assert(StructuredWriteText(&d,NULL,0,record,sizeof(record),"x"));
    assert(record[3]=='x' && record[4]==0 && record[5]=='c');
    assert(StructuredReadText(&d,NULL,0,record,sizeof(record),&text));
    assert(text==(const char *)record+3 && !strcmp(text,"x"));
    memcpy(saved,record,sizeof(record));
    assert(!StructuredWriteText(&d,NULL,0,record,sizeof(record),"abcde"));
    assert(!StructuredWriteText(&d,NULL,0,record,11,"z"));
    assert(!memcmp(saved,record,sizeof(record)));
    memset(record+3,'a',5);text="unchanged";
    assert(!StructuredReadText(&d,NULL,0,record,sizeof(record),&text));
    assert(!strcmp(text,"unchanged"));
    d.root=(structured_type_t){4,0};
    assert(StructuredWriteText(&d,NULL,0,record,sizeof(record),"allies"));
    assert(record[3]==2 && record[4]==1 && record[5]=='a');
    assert(StructuredReadText(&d,NULL,0,record,sizeof(record),&text));
    assert(!strcmp(text,"allies"));memcpy(saved,record,sizeof(record));
    assert(!StructuredWriteText(&d,NULL,0,record,sizeof(record),"Allies"));
    assert(!memcmp(saved,record,sizeof(record)));
    record[3]=255;record[4]=255;text="unchanged";
    assert(!StructuredReadText(&d,NULL,0,record,sizeof(record),&text));
    assert(!strcmp(text,"unchanged"));
}
int main(void)
{
    uint8_t bytes[125]={0};asset_t asset={39,bytes,124};
    structured_schema_t schema={0},saved; unsigned i;
    put(bytes,UINT32_MAX);put(bytes+4,1);put(bytes+8,UINT32_MAX);bytes[12]='s';
    put(bytes+14,16);put(bytes+14+16,1);put(bytes+14+20,UINT32_MAX);
    put(bytes+14+40,5);put(bytes+14+48,4);put(bytes+14+52,1);put(bytes+14+56,UINT32_MAX);
    put(bytes+74,1);put(bytes+78,UINT32_MAX);put(bytes+82,4);put(bytes+90,1);
    put(bytes+94,UINT32_MAX);bytes[118]='x';
    assert(ReadStructuredSchema(&asset,NULL,NULL,&schema));
    assert(schema.count==1 && !strcmp(schema.name,"s"));
    assert(!strcmp(schema.definitions[0].structs[0].properties[0].name,"x"));
    assert(schema.definitions[0].layout.offset_map_size==4);
    saved=schema;
    for(i=0;i<124;++i) {
        asset.size=i;assert(!ReadStructuredSchema(&asset,NULL,NULL,&schema));
        assert(!memcmp(&schema,&saved,sizeof(schema)));
    }
    asset.size=125;assert(!ReadStructuredSchema(&asset,NULL,NULL,&schema));
    asset.size=124;put(bytes+94,0x30000001);
    assert(!ReadStructuredSchema(&asset,NULL,NULL,&schema));
    assert(!memcmp(&schema,&saved,sizeof(schema)));
    FreeStructuredSchema(&schema);FreeStructuredSchema(&schema);
    paths();text_values();
    puts("PASS: structured schema ownership, boundaries, typed paths and atomic failure");
}

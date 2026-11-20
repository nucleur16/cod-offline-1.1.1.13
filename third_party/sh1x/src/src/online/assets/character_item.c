#include "character_variant.h"
#include <stdio.h>
#include <string.h>
int CharacterSlotForGroup(const char *group)
{
    static const char *groups[]={"hea","fja","fpa","ves","han","boo","heg","elp","knp","pat","eye"};
    static const int slots[]={4,2,0,5,3,1,6,7,8,9,13};unsigned i;
    if(!group)return -1;
    for(i=0;i<sizeof(slots)/sizeof(*slots);++i)if(!strcmp(group,groups[i]))return slots[i];
    return -1;
}
int CharacterItem(const engine_table_t *table,uint16_t id,character_item_t *out)
{
    char key[6];int r;character_item_t next;
    if(!out || !table || table->rowCount<0 || table->columnCount<4 ||
       (table->rowCount && !table->values))return -1;
    snprintf(key,sizeof(key),"%u",(unsigned)id);
    for(r=table->rowCount-1;r>=0;--r) {
        const char *const *cells=table->values+(size_t)r*table->columnCount;
        /* C1 StringTable lookups skip NULL search cells. The patch-zone
         * writer also uses NULL for empty fields (emptyptr=null), including
         * the optional group of every killstreak. These are valid CSV cells,
         * not a malformed marketplace. */
        if(!cells[0])continue;
        if(strcmp(cells[0],key))continue;
        next=(character_item_t){cells[1] ? cells[1] : "",
            cells[2] ? cells[2] : "",cells[3] ? cells[3] : ""};
        *out=next;return 1;
    }
    return 0;
}
int LoadCharacterItemParts(uint16_t id,character_item_t *out,const engine_table_t **table)
{
    const engine_table_t *market=NULL,*parts=NULL;character_item_t item;
    char path[64];size_t n;int status;
    if(!out || !table)return -1;
    if(!id)return 0;
    status=LoadEngineStringTable("mp/coMarketplaceItems.csv",&market);
    if(status!=1)return status;
    status=CharacterItem(market,id,&item);if(status!=1)return status;
    if(!*item.name || !*item.group)return 0;
    /* Group comes from the mounted marketplace. Bound it before using it in
     * an asset path; don't substitute profile slot names or guessed filenames. */
    for(n=0;item.group[n];++n)
        if(n>=16 || !((item.group[n]>='a' && item.group[n]<='z') ||
           (item.group[n]>='A' && item.group[n]<='Z')))return -1;
    snprintf(path,sizeof(path),"mp/coCharItems/%s.csv",item.group);
    status=LoadEngineStringTable(path,&parts);if(status!=1)return status;
    *out=item;*table=parts;return 1;
}
int LoadCharacterItemModel(uint16_t id,unsigned gender,unsigned faction,
    const char *camo,const char *skin,const char **model)
{
    const char *ignored;
    return LoadCharacterItemModels(id,gender,faction,camo,skin,model,&ignored);
}
int LoadCharacterItemModels(uint16_t id,unsigned gender,unsigned faction,
    const char *camo,const char *skin,const char **model,const char **viewmodel)
{
    const engine_table_t *parts;character_item_t item;unsigned row;int status;const char *value;
    if(!model || !viewmodel || gender>1 || faction>2 || !camo || strlen(camo)!=3 || !skin)return -1;
    status=LoadCharacterItemParts(id,&item,&parts);if(status!=1)return status;
    status=CharacterVariant(parts,item.name,"model",gender,faction,camo,skin,&row,&value);
    if(status!=1)return status;
    *model=value;
    /* Column 4 is the first-person model of the same selected row. Only the
     * jacket and glove tables carry one, so its absence is ordinary. */
    *viewmodel=NULL;
    if(parts->columnCount>4) {
        const char *first_person=parts->values[(size_t)row*parts->columnCount+4];
        if(first_person && *first_person)*viewmodel=first_person;
    }
    return 1;
}
static unsigned fold(unsigned c){return c>='A' && c<='Z' ? c+32:c;}
static int same_name(const char *a,const char *b)
{
    while(*a && fold((unsigned char)*a)==fold((unsigned char)*b)){++a;++b;}
    return fold((unsigned char)*a)==fold((unsigned char)*b);
}
int CharacterSkinTone(uint16_t head_item,char out[4])
{
    const engine_table_t *market=NULL,*parts=NULL;character_item_t item;
    size_t name_length;int r;
    if(!out)return 0;
    memcpy(out,"wht",4);
    if(!head_item || head_item>=32768)return 0;
    if(LoadEngineStringTable("mp/coMarketplaceItems.csv",&market)!=1)return 0;
    if(CharacterItem(market,head_item,&item)!=1)return 0;
    /* The measured predicate is the item's marketplace type and group, not the
     * composition slot it happens to occupy. */
    if(strcmp(item.type,"cha") || strcmp(item.group,"hea"))return 0;
    if(LoadEngineStringTable("mp/coCharItems/hea.csv",&parts)!=1)return 0;
    if(parts->rowCount<0 || parts->columnCount<4 || (parts->rowCount && !parts->values))return 0;
    name_length=strlen(item.name);
    if(!name_length || name_length>SIZE_MAX-9)return 0;
    for(r=parts->rowCount-1;r>=0;--r) {
        const char *const *cells=parts->values+(size_t)r*parts->columnCount;
        const char *model;
        if(!cells[0])return 0;
        if(!same_name(cells[0],item.name))continue;
        model=cells[3];
        if(!model || strlen(model)<name_length+9)return 0;
        memcpy(out,model+name_length+6,3);out[3]=0;
        return 1;
    }
    return 0;
}

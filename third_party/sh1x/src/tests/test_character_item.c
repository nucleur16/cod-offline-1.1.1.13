#include "character_variant.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *market_cells[]={"0139","old","cha","fja","139","coat_old","cha","fja","139","coat","cha","fja"};
static const char *part_cells[]={"coat","model","m tf urb","coat_wht","badge","material","s ss sss","badge_material"};
static engine_table_t market={"market",4,3,market_cells},parts={"parts",4,2,part_cells};
static int reads;
int LoadEngineStringTable(const char *name,const engine_table_t **out)
{
    ++reads;
    if(!strcmp(name,"mp/coMarketplaceItems.csv")){*out=&market;return 1;}
    if(!strcmp(name,"mp/coCharItems/fja.csv")){*out=&parts;return 1;}
    return 0;
}
int main(void)
{
    character_item_t item={0};const char *model="unchanged";
    assert(CharacterItem(&market,139,&item)==1 && !strcmp(item.name,"coat"));
    /* Marketplace group selects the correct profile slot; it is not the
     * reverse-address order of the part-name string literals. */
    { uint16_t profile_slots[20]={0};
      profile_slots[2]=139;profile_slots[4]=160;profile_slots[13]=176;
      assert(profile_slots[CharacterSlotForGroup(item.group)]==139);
      assert(profile_slots[CharacterSlotForGroup("hea")]==160);
      assert(profile_slots[CharacterSlotForGroup("eye")]==176);
      assert(CharacterSlotForGroup("unknown")==-1); }
    assert(LoadCharacterItemModel(139,0,0,"urb","wht",&model)==1 && !strcmp(model,"coat_wht") && reads==2);
    model="unchanged";
    assert(LoadCharacterItemModel(139,1,1,"urb","wht",&model)==0 && !strcmp(model,"unchanged"));
    assert(LoadCharacterItemModel(0,0,0,"urb","wht",&model)==0);
    assert(LoadCharacterItemModel(65535,0,0,"urb","wht",&model)==0);
    market_cells[9]="badge";
    assert(LoadCharacterItemModel(139,0,0,"urb","wht",&model)==0 && !strcmp(model,"unchanged"));
    market_cells[9]="coat";market_cells[11]="../fja";
    assert(LoadCharacterItemModel(139,0,0,"urb","wht",&model)==-1 && !strcmp(model,"unchanged"));
    market_cells[11]=NULL;
    assert(CharacterItem(&market,139,&item)==1 && !strcmp(item.group,""));
    assert(LoadCharacterItemModel(139,0,0,"urb","wht",&model)==0);
    /* mp_weapons uses emptyptr=null: all kis rows have a NULL group.
     * Exercise the exact spawn-time failure, plus a trailing empty row. */
    { const char *values[]={"4001","uav","kis",NULL,NULL,NULL,NULL,NULL};
      engine_table_t patch={"mp/coMarketplaceItems.csv",4,2,values};
      assert(CharacterItem(&patch,4001,&item)==1);
      assert(!strcmp(item.name,"uav") && !strcmp(item.type,"kis") && !*item.group);
      assert(CharacterItem(&patch,4002,&item)==0);
      values[1]=NULL;values[2]=NULL;
      assert(CharacterItem(&patch,4001,&item)==1 && !*item.name && !*item.type);
      patch.columnCount=3;
      assert(CharacterItem(&patch,4001,&item)==-1); }
    puts("PASS: marketplace last-row lookup, canonical IDs, mounted part routing and material exclusion");
}

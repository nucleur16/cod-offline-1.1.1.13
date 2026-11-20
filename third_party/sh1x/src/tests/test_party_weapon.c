/* Actual party builtin/recipe/composition/override code with engine publication
 * fixtures. Rows and tactical override strings below come from exact C1 MP
 * tables; this does not claim that a live C1 drew the resulting weapon. */
#include "script_profile_loadout.h"
#include "weapon_compose.h"
#include "weapon_attachment.h"
#include "loadout_fastmag.h"
#include "scr_vm.h"
#include "qcommon.h"
#include "xassets/weapondef.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *market_values[33*4],*attachment_values[]={
    "usp_mp","cqc_tactical","tactical_usp","9",
    "usp_mp","mag_default","nop","0",
    "usp_mp","iro_default","nop","0"};
static char item_ids[32][8],item_names[32][24];
static const char *default_values[]={"usp_mp","mag","mag_default","y","iro","iro_default","y"};
static const char *art_values[]={"usp_mp","0","wea_usp_usp_blk","wea_usp_usp_blk_vmgun"};
static const char *knife_values[]={"cqc_tactical","usp_mp","1","","viewmodel_tac_knife","TAG_WEAPON_LEFT"};
static engine_table_t market={"market",4,33,market_values};
static engine_table_t attachments={"attachments",4,3,attachment_values};
static engine_table_t defaults={"defaults",7,1,default_values};
static engine_table_t art={"art",4,1,art_values};
static engine_table_t knife={"cqc",6,1,knife_values};
static engine_table_t empty={"empty",6,0,NULL};
static const engine_table_t *selected_knife=&knife;
static weapon_registry_t *registry;
static const char *script_args[16];
static unsigned published_base,publish_calls,load_calls;
static int arg_count,errors,room=1;
static char answer[128],published[128],idle[96],melee[96];
static struct WeaponDef native_definition;

int LoadEngineStringTable(const char *name,const engine_table_t **out)
{
    if(!strcmp(name,"mp/coMarketplaceItems.csv"))*out=&market;
    else if(!strcmp(name,"mp/coWeaponAttachmentTable.csv"))*out=&attachments;
    else if(!strcmp(name,"mp/coweapitemswea.csv"))*out=&defaults;
    else if(!strcmp(name,"mp/coweapmodels/usp_mp.csv"))*out=&art;
    else if(!strcmp(name,"mp/coweapitems/cqc.csv"))*out=selected_knife;
    else if(!strncmp(name,"mp/coweapitems/",15))*out=&empty;
    else return 0;
    return 1;
}
int Q_stricmp(const char *a,const char *b){return strcasecmp(a,b);}
int Scr_GetNumParam(void){return arg_count;}
unsigned int Scr_GetType(unsigned int index){assert(index<(unsigned)arg_count);return VAR_STRING;}
char *Scr_GetString(unsigned int index){assert(index<(unsigned)arg_count);return (char *)script_args[index];}
void Scr_AddString(const char *name){assert(strlen(name)<sizeof(answer));strcpy(answer,name);}
void Scr_Error(const char *message){(void)message;++errors;}
void Scr_ParamError(int parameter,const char *message){(void)parameter;(void)message;++errors;}
void Com_PrintWarning(conChannel_t channel,const char *format,...){(void)channel;(void)format;}
const weapon_registry_t *ServerWeaponRegistry(void){return registry;}
int NativeWeaponRoom(void){return room;}
const char *SafeStockWeapon(const char *name){return name;}
const char *ServerWeaponRegistryError(void){return "fixture";}
int EventFastMagCompositionBase(const loadout_recipe_t *recipe,unsigned *out)
{(void)recipe;(void)out;return 0;}

/* Exercise real C1 override application using the exact tactical_usp entries
 * that change idle and melee. Full mounted native loading is outside this
 * fixture; the linker wrapper makes that boundary explicit. */
int __wrap_LoadComposedWeapon(const char *name,const struct WeaponDef **out)
{
    composed_weapon_t record;
    weapon_source_image_t image={0};
    weapon_attachment_entry_t entries[]={
        {0x80,ATTACH_TEXT_0,UINT32_MAX,"[r] <w> <w>_tactical"},
        {0x98,ATTACH_TEXT_0,UINT32_MAX,"[r] knife_melee_slic <w>_tactical_mele"},
        {0xbf0,ATTACH_BOOL,UINT32_MAX,"1"},
        {0x5c0,ATTACH_MILLISECONDS,UINT32_MAX,"[+] -0.270"},
        {0x5c4,ATTACH_MILLISECONDS,UINT32_MAX,"[+] -0.630"},
        {0x5a4,ATTACH_MILLISECONDS,UINT32_MAX,"[+] -0.020"}};
    weapon_attachment_t tactical={0};
    const weapon_attachment_t *one=&tactical;
    const char *text,*asset;unsigned i,tactical_selected=0;
    const unsigned fields[]={0x590,0x5a0,0x5a4,0x5c0,0x5c4};
    const uint32_t values[]={135,79,100,800,1159}; /* Exact usp_mp header. */
    mounted_weapon_t source={0};
    assert(ParseComposedWeaponName(name,&record)==1);
    image.size=0xbfc;image.data=calloc(image.size,1);image.scalar=calloc(image.size,1);
    image.names=calloc(2,sizeof(*image.names));image.name_count=2;
    assert(image.data && image.scalar && image.names);
    strcpy(image.context,"usp");image.base="usp_mp";image.scalar[0xbf0]=1;
    for(i=0;i<sizeof(fields)/sizeof(*fields);++i) {
        memcpy(image.data+fields[i],values+i,4);
        memset(image.scalar+fields[i],1,4);
    }
    image.names[0]=(weapon_source_name_t){0x80,"viewmodel_usp_idle",NULL};
    image.names[1]=(weapon_source_name_t){0x98,"viewmodel_knife_melee_slice",NULL};
    tactical.name="tactical_usp";tactical.count=6;tactical.entries=entries;
    for(i=0;i<record.count;++i) {
        if(ComposedAttachmentAssetName("usp_mp",record.items[i],&asset)==1 &&
           !strcmp(asset,"tactical_usp")) {
            assert(record.variants[i]==9);
            assert(ApplyWeaponAttachments(&image,&one,1)==1);
            assert(image.data[0xbf0]==1);
            tactical_selected=1;
        }
    }
    assert(WeaponSourceName(&image,0x80,&text));strcpy(idle,text);
    assert(WeaponSourceName(&image,0x98,&text));strcpy(melee,text);
    source.asset=(asset_t){28,image.data,image.size};
    assert(ApplyNativeWeaponMelee(&source,&native_definition));
    assert(native_definition.iMeleeDamage==135 && native_definition.iMeleeDelay==79);
    assert(native_definition.iMeleeTime==(tactical_selected ? 530 : 800));
    assert(native_definition.meleeChargeTime==(tactical_selected ? 529 : 1159));
    assert(native_definition.meleeChargeDelay==(tactical_selected ? 80 : 100));
    FreeWeaponSourceImage(&image);
    ++load_calls;*out=&native_definition;return 1;
}

int PublishComposedWeapon(const char *name,const char *base,unsigned *slot)
{
    composed_weapon_t record;
    assert(!strcmp(base,"usp_mp"));
    assert(ParseComposedWeaponName(name,&record)==1 && record.base==published_base);
    strcpy(published,name);*slot=published_base+1;++publish_calls;return 1;
}

static void populate_registry(unsigned base)
{
    unsigned i;char name[40];
    if(registry)DestroyWeaponRegistry(registry);
    registry=CreateWeaponRegistry();assert(registry);
    for(i=0;i<=base;++i) {
        snprintf(name,sizeof(name),"other_%u_mp",i);
        assert(RegisterWeaponKeys(registry,i,i==base ? "usp_mp" : name,"ammo","clip",0,0,-1));
    }
    published_base=base;
}

static void fixture(void)
{
    unsigned i;
    market_values[0]="1035";market_values[1]="usp_mp";market_values[2]="wea";market_values[3]="pst";
    for(i=0;i<32;++i) {
        snprintf(item_ids[i],sizeof(item_ids[i]),"%u",2000+i);
        snprintf(item_names[i],sizeof(item_names[i]),"item_%u",i);
        market_values[4*(i+1)]=item_ids[i];market_values[4*(i+1)+1]=item_names[i];
        market_values[4*(i+1)+2]="wat";market_values[4*(i+1)+3]="unused";
    }
    market_values[4*12]="2011";market_values[4*12+1]="cqc_tactical";market_values[4*12+3]="cqc";
    market_values[4*31]="2030";market_values[4*31+1]="iro_default";market_values[4*31+3]="iro";
    market_values[4*32]="2031";market_values[4*32+1]="mag_default";market_values[4*32+3]="mag";
    populate_registry(37);
}

static void party_recipe(void)
{
    const char *world,*view,*tag;composed_weapon_t record;
    fixture();script_args[0]="usp_mp";script_args[1]="cqc_tactical";
    arg_count=2;Scr_GetPartyWeapon();
    assert(!errors && publish_calls==1 && load_calls==1);
    assert(!strcmp(answer,"*a 37 0 12 9 32 0 31 0") && !strcmp(answer,published));
    assert(ParseComposedWeaponName(answer,&record)==1 && record.base==37);
    assert(!strcmp(idle,"viewmodel_usp_tactical_idle"));
    assert(!strcmp(melee,"viewmodel_usp_tactical_melee"));
    assert(ComposedItemModelRow("usp_mp","cqc_tactical","cqc",record.flag,&world,&view,&tag)==1);
    assert(!*world && !strcmp(view,"viewmodel_tac_knife") && !strcmp(tag,"TAG_WEAPON_LEFT"));
    /* A changed published registry still drives the recipe, never hardcoded51. */
    populate_registry(88);Scr_GetPartyWeapon();
    assert(!strcmp(answer,"*a 88 0 12 9 32 0 31 0"));
    arg_count=1;Scr_GetPartyWeapon();
    assert(!strcmp(answer,"*a 88 0 32 0 31 0"));
    assert(!strcmp(idle,"viewmodel_usp_idle"));
    script_args[1]="unknown_attachment";arg_count=2;Scr_GetPartyWeapon();
    assert(errors==1 && publish_calls==3);
    arg_count=0;Scr_GetPartyWeapon();assert(errors==2);
    arg_count=17;Scr_GetPartyWeapon();assert(errors==3);
}

static void scored_rows(void)
{
    static const char *rows[]={
        "cqc_tactical","usp_mp","2","","other_two","TAG_WEAPON_LEFT",
        "cqc_tactical","usp_mp","1","","fallback_one","TAG_WEAPON_LEFT",
        "cqc_tactical","usp_mp","0","","neutral_zero","TAG_WEAPON_LEFT",
        "cqc_tactical","usp_mp","7","","first_seven","TAG_WEAPON_LEFT",
        "cqc_tactical","usp_mp","7","","last_seven","TAG_WEAPON_LEFT"};
    static const char *only_other[]={
        "cqc_tactical","usp_mp","2","","first_other","TAG_WEAPON_LEFT",
        "cqc_tactical","usp_mp","8","","last_other","TAG_WEAPON_LEFT"};
    static engine_table_t choices[]={
        {"cqc",6,5,rows},{"cqc",6,2,rows},{"cqc",6,2,only_other}};
    const char *view;
    selected_knife=choices;
    assert(ComposedItemModelRow("usp_mp","cqc_tactical","cqc",0,NULL,&view,NULL)==1 && !strcmp(view,"neutral_zero"));
    assert(ComposedItemModelRow("usp_mp","cqc_tactical","cqc",7,NULL,&view,NULL)==1 && !strcmp(view,"last_seven"));
    assert(ComposedItemModelRow("usp_mp","cqc_tactical","cqc",4,NULL,&view,NULL)==1 && !strcmp(view,"neutral_zero"));
    selected_knife=choices+1;
    assert(ComposedItemModelRow("usp_mp","cqc_tactical","cqc",0,NULL,&view,NULL)==1 && !strcmp(view,"fallback_one"));
    selected_knife=choices+2;
    assert(ComposedItemModelRow("usp_mp","cqc_tactical","cqc",3,NULL,&view,NULL)==1 && !strcmp(view,"last_other"));
    assert(ComposedItemModelRow("usp_mp","cqc_tactical","cqc",0,NULL,&view,NULL)==1 && !strcmp(view,"last_other"));
}

int main(void)
{
    party_recipe();scored_rows();DestroyWeaponRegistry(registry);
    puts("PASS: party attachments use published IDs, tactical USP animations/knife/tag, C1 camo fallback scoring and tie order");
}

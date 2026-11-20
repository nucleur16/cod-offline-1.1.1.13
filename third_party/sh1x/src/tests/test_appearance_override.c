/* Native stored-appearance lifecycle tests. The real appearance owner is
 * compiled below; only mounted assets/engine publication are supplied by the
 * fixture. Rendering, real asset loading and a C1 client remain separate. */
#include "../src/online/player/player_appearance.c"
#include "../src/online/script/script_appearance.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

level_locals_t level;
struct bgs_s level_bgs;
gentity_t g_entities[MAX_GENTITIES];
server_t sv;
XModel *cached_models[NATIVE_MODELS];
cvar_t *ol_debugTrace;
static gclient_t test_clients[MAX_CLIENTS];
static char model_names[MAX_MODELS][64];
static unsigned wire_offset=100,publication_count,clear_hands_count;
static int reject_publication,reject_refresh,reject_model;
static client_appearance_t published[MAX_CLIENTS];
static unsigned hands[MAX_CLIENTS];
static int script_params,script_errors,script_result;
static const char *script_strings[2];

typedef struct {
    const char *name;
    XModel native;
    mounted_model_skeleton_t mounted;
    xmodel_bone_name_t names[80];
} fixture_model_t;
static fixture_model_t fixtures[8];
static const char *const fixture_names[]={"profile_body","profile_head",
    "profile_arms","profile_hands","profile_body2","c_zom_ger_body1",
    "c_zom_ger_head","invalid_body"};

static fixture_model_t *fixture(const char *name)
{
    unsigned i;
    for(i=0;i<sizeof(fixtures)/sizeof(*fixtures);++i)
        if(!strcmp(fixtures[i].name,name))return &fixtures[i];
    return NULL;
}

int LoadMountedXModelSkeleton(const char *name,const mounted_model_skeleton_t **out)
{
    fixture_model_t *model=fixture(name);
    if(!model)return 0;
    *out=&model->mounted;return 1;
}

XModel *__cdecl SV_XModelGet(const char *name)
{
    fixture_model_t *model=fixture(name);
    if(reject_model || !model)return NULL;
    return &model->native;
}

int SV_GetConfigstringIndex(int index)
{
    assert(index>=CS_MODELS && index<CS_MODELS+MAX_MODELS);
    index-=CS_MODELS;
    return model_names[index][0] ? index : 0;
}

void SV_SetConfigstring(int index,const char *name)
{
    assert(index>CS_MODELS && index<CS_MODELS+MAX_MODELS);
    assert(strlen(name)<64);
    strcpy(model_names[index-CS_MODELS],name);
}

const char *SL_ConvertToString(unsigned int handle)
{
    return handle<MAX_MODELS ? model_names[handle] : NULL;
}

unsigned int SL_FindLowercaseString(const char *name)
{
    unsigned i;
    for(i=1;i<MAX_MODELS;++i)if(!strcmp(model_names[i],name))return i;
    return 0;
}

int FindHighModel(unsigned handle){(void)handle;return 0;}
int AllocHighModel(const char *name){(void)name;return 0;}
int RefreshServerAssetNames(void){return !reject_refresh;}
int ServerModelToWire(unsigned index){return index ? (int)(index+wire_offset) : 0;}

int PublishClientAppearance(unsigned client,const client_appearance_t *appearance)
{
    if(reject_publication)return 0;
    assert(client<MAX_CLIENTS && appearance->model && appearance->model<2048);
    published[client]=*appearance;++publication_count;return 1;
}

int PlayerStateSetViewmodels(playerState_t *ps,unsigned arms,unsigned hand)
{
    unsigned client=(unsigned)((gclient_t *)ps-test_clients);
    assert(client<MAX_CLIENTS);
    ps->viewmodelIndex=arms;hands[client]=hand;return 1;
}

void PlayerStateExtClearClientViewhands(unsigned client)
{assert(client<MAX_CLIENTS);hands[client]=0;++clear_hands_count;}
void KnifeCorpseCreated(unsigned corpse,unsigned client){(void)corpse;(void)client;}
void Scr_SetString(unsigned short *value,unsigned int handle){*value=(unsigned short)handle;}
int Scr_GetNumParam(void){return script_params;}
char *Scr_GetString(unsigned int index){assert(index<2);return (char *)script_strings[index];}
void Scr_Error(const char *message){(void)message;++script_errors;}
void Scr_ObjectError(const char *message){(void)message;++script_errors;}
void Scr_AddBool(bool value){script_result=value;}
void Com_Printf(conChannel_t channel,const char *format,...){(void)channel;(void)format;}
void Com_PrintWarning(conChannel_t channel,const char *format,...){(void)channel;(void)format;}
void Com_Error(int code,const char *format,...)
{(void)code;(void)format;assert(!"unexpected native appearance failure");abort();}
XModel *BuildGameplayPoseModel(const XModel *source,const char *(*name)(unsigned))
{(void)source;(void)name;return NULL;}

static unsigned native_index(const char *name)
{
    unsigned i;
    for(i=1;i<MAX_MODELS;++i)if(!strcmp(model_names[i],name))return i;
    return 0;
}

static void base(unsigned client,const char *body)
{
    character_composition_t composition={0};
    composition.models[0]=body;
    composition.models[4]="profile_head";
    composition.viewmodels[2]="profile_arms";
    composition.viewmodels[3]="profile_hands";
    assert(PublishCharacterComposition(client,&composition,"tf"));
}

static void setup(void)
{
    static const char *const body_bones[]={"tag_origin","j_mainroot","j_spinelower",
        "j_spine4","j_neck","j_head","j_wrist_le","j_wrist_ri"};
    unsigned i,j;
    memset(fixtures,0,sizeof(fixtures));
    memset(model_names,0,sizeof(model_names));
    memset(cached_models,0,sizeof(cached_models));
    memset(g_entities,0,sizeof(g_entities));
    memset(test_clients,0,sizeof(test_clients));
    memset(published,0,sizeof(published));
    memset(hands,0,sizeof(hands));
    memset(&level,0,sizeof(level));
    level.clients=test_clients;level.maxclients=MAX_CLIENTS;
    memset(&sv,0,sizeof(sv));
    wire_offset=100;publication_count=clear_hands_count=0;
    reject_publication=reject_refresh=reject_model=0;
    ResetClientCharacters();
    for(i=0;i<sizeof(fixtures)/sizeof(*fixtures);++i) {
        fixture_model_t *model=&fixtures[i];
        unsigned count=i==5 ? 73 : i==6 ? 29 : 8;
        model->name=fixture_names[i];model->native.name=model->name;
        model->native.numBones=(char)count;
        model->mounted.skeleton.name=model->name;
        model->mounted.skeleton.bones=count;model->mounted.skeleton.roots=1;
        model->mounted.bone_names.count=count;model->mounted.bone_names.names=model->names;
        for(j=0;j<count;++j)model->names[j].text="fixture_bone";
        for(j=0;j<sizeof(body_bones)/sizeof(*body_bones);++j)model->names[j].text=body_bones[j];
        if(i==6)model->names[0].text="j_spine4";
        if(i==7)model->names[1].text="not_a_player";
    }
    for(i=0;i<2;++i) {
        g_entities[i].r.inuse=1;g_entities[i].client=&test_clients[i];
        test_clients[i].ps.clientNum=(int)i;
        base(i,"profile_body");
    }
}

static void assert_models(unsigned client,const char *body,const char *head)
{
    const unsigned *models;unsigned count,arms,hand;
    assert(ClientCharacterModels(client,&models,&count,&arms,&hand));
    assert(count==2 && models[0]==native_index(body) && models[1]==native_index(head));
    assert(arms==native_index("profile_arms") && hand==native_index("profile_hands"));
}

static void set_and_restore(void)
{
    unsigned body,head,old_hands,old_arms;
    setup();old_hands=hands[0];old_arms=test_clients[0].ps.viewmodelIndex;
    /* A tagged objective survives replacing the character's tagless parts. */
    g_entities[0].attachModelNames[5]=77;g_entities[0].attachTagNames[5]=9;
    assert(SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    body=native_index("c_zom_ger_body1");head=native_index("c_zom_ger_head");
    assert_models(0,"c_zom_ger_body1","c_zom_ger_head");
    assert(published[0].model==body+100 && published[0].attach_model[0]==head+100);
    assert(!published[0].attach_model[1] && !published[0].attach_tag[0]);
    assert(g_entities[0].model==body && g_entities[0].attachModelNames[0]==head);
    assert(g_entities[0].attachModelNames[5]==77 && g_entities[0].attachTagNames[5]==9);
    assert(test_clients[0].ps.viewmodelIndex==(int)old_arms && hands[0]==old_hands);
    /* Each snapshot publication resolves stored native IDs in the current
     * wire namespace instead of persisting the old wire numbers. */
    wire_offset=500;
    assert(PublishStoredClientCharacter(0)==1);
    assert(published[0].model==body+500 && published[0].attach_model[0]==head+500);
    assert(ClearClientAppearanceModels(0));
    assert_models(0,"profile_body","profile_head");
    assert(g_entities[0].model==native_index("profile_body"));
    assert(published[0].model==native_index("profile_body")+500);
    assert(ClearClientAppearanceModels(0));
}

static void refresh_and_corpse(void)
{
    struct {XModel *model;uint16_t tag;uint8_t ignoreCollision,pad;} parts[4]={0};
    entityState_t entity={0};
    unsigned body,head;
    setup();
    assert(SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    body=native_index("c_zom_ger_body1");head=native_index("c_zom_ger_head");
    /* A normal loadout/profile refresh does not cancel the infected cosmetic. */
    base(0,"profile_body2");
    assert_models(0,"c_zom_ger_body1","c_zom_ger_head");
    assert(g_entities[0].model==body);
    entity.eType=ET_PLAYER;entity.number=0;parts[0].model=cached_models[body];
    assert(AppendCharacterDObjParts(&entity,parts,1,4)==2);
    assert(parts[1].model==cached_models[head]);
    CaptureCorpseCharacter(100,0);
    assert(ClearClientAppearanceModels(0));
    assert_models(0,"profile_body2","profile_head");
    /* The captured corpse retains the infected body after its owner restores. */
    entity.eType=ET_PLAYER_CORPSE;entity.number=100;
    parts[0].model=cached_models[body];parts[1].model=NULL;
    assert(AppendCharacterDObjParts(&entity,parts,1,4)==2);
    assert(parts[1].model==cached_models[head]);
}

static void invalid_pair_is_atomic(void)
{
    client_appearance_t before;
    const char *root;
    setup();before=published[0];
    assert(!SetClientAppearanceModels(0,"missing","c_zom_ger_head"));
    assert(!SetClientAppearanceModels(0,"invalid_body","c_zom_ger_head"));
    root=fixtures[6].names[0].text;fixtures[6].names[0].text="orphan_root";
    assert(!SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    fixtures[6].names[0].text=root;
    fixtures[6].mounted.bone_names.unresolved=1;
    assert(!SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    fixtures[6].mounted.bone_names.unresolved=0;
    fixtures[5].mounted.skeleton.bones=253;
    assert(!SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    fixtures[5].mounted.skeleton.bones=73;
    reject_model=1;
    assert(!SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    reject_model=0;
    fixtures[5].native.name="default_model";
    assert(!SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    fixtures[5].native.name=fixtures[5].name;
    reject_refresh=1;
    assert(!SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    reject_refresh=0;reject_publication=1;
    assert(!SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    reject_publication=0;
    assert(!memcmp(&before,&published[0],sizeof(before)));
    assert_models(0,"profile_body","profile_head");
    assert(g_entities[0].model==native_index("profile_body"));
    assert(SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    before=published[0];reject_publication=1;
    assert(!ClearClientAppearanceModels(0));
    assert(!memcmp(&before,&published[0],sizeof(before)));
    assert_models(0,"c_zom_ger_body1","c_zom_ger_head");
}

static void script_surface(void)
{
    scr_entref_t ref={0,0};
    setup();script_errors=0;script_result=-1;
    script_strings[0]="c_zom_ger_body1";script_strings[1]="c_zom_ger_head";
    script_params=2;
    Scr_SetAppearanceModels(ref);
    assert(!script_errors && script_result==1);
    assert_models(0,"c_zom_ger_body1","c_zom_ger_head");
    script_params=0;
    Scr_ClearAppearanceModels(ref);
    assert(!script_errors && script_result==1);
    assert_models(0,"profile_body","profile_head");
    script_params=1;
    Scr_SetAppearanceModels(ref);
    assert(script_errors==1);
    Scr_ClearAppearanceModels(ref);
    assert(script_errors==2);
    ref.classnum=1;script_params=2;
    Scr_SetAppearanceModels(ref);
    assert(script_errors==3);
    script_params=0;
    Scr_ClearAppearanceModels(ref);
    assert(script_errors==4);
}

static void connection_and_map_lifetime(void)
{
    setup();
    assert(SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    assert(SetClientAppearanceModels(1,"c_zom_ger_body1","c_zom_ger_head"));
    ClearClientCharacter(0);
    assert(!PublishStoredClientCharacter(0));
    assert(PublishStoredClientCharacter(1)==1);
    assert(clear_hands_count==1);
    base(0,"profile_body");
    assert_models(0,"profile_body","profile_head");
    ResetClientCharacters();
    assert(!PublishStoredClientCharacter(0) && !PublishStoredClientCharacter(1));
    assert(!SetClientAppearanceModels(0,"c_zom_ger_body1","c_zom_ger_head"));
    base(0,"profile_body");
    assert_models(0,"profile_body","profile_head");
}

int main(void)
{
    _Static_assert(sizeof(void *)==4,"engine ABI tests require -m32");
    set_and_restore();refresh_and_corpse();invalid_pair_is_atomic();connection_and_map_lifetime();script_surface();
    puts("PASS: stored body/head override, namespace refresh, latest-profile restore, corpse isolation, atomic failure and lifecycle reset");
}

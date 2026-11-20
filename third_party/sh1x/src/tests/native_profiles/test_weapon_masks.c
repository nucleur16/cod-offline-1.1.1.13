/* Execute the upstream method plus actual native eight-word entity masks.
 * Asset resolution, VM argument/error sinks, bone lookup and DObj application
 * are named doubles. This does not prove rendered C1 model appearance. */
#include "g_shared.h"
#include "dobj.h"
#include "scr_vm.h"
#include "qcommon_io.h"
#include "weapon_native.h"
#include "hide_parts.h"
#include "entitystate_ext.h"
#include "script_weapon_visual_queries.h"
#include "xassets/weapondef.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
void ScrCmd_UseWeaponHideTags(scr_entref_t ref);

gentity_t g_entities[2];
static DObj model;
static struct WeaponDef weapon;
static weapon_hide_tag_handles_t tags;
static int bones[65536],available=1,resolved=1,mounted=1,errors,objects,prints,lookups,writes,cases;
static uint32_t applied[8];
static char *name="weapon";
const void *EntityStateExtLiveBase(unsigned *n) { *n=2;return g_entities; }
const void *EntityStateExtSnapshotBase(unsigned *n) { *n=0;return NULL; }
const void *EntityStateExtArchiveBase(unsigned *n) { *n=0;return NULL; }
void QDECL Com_PrintWarning(conChannel_t c,const char *f,...) { (void)c;(void)f;assert(0); }
void QDECL Com_PrintError(conChannel_t c,const char *f,...) { (void)c;(void)f;++prints; }
void __cdecl Scr_ObjectError(const char *s) { assert(!strcmp(s,"not an entity"));++objects; }
void __cdecl Scr_Error(const char *s) { assert(!strcmp(s,"Entity has no model"));++errors; }
char *__cdecl Scr_GetString(unsigned i) { assert(i==0);return name; }
int __cdecl G_GetWeaponIndexForName(const char *s) { assert(s==name);return resolved; }
int BG_GetNumWeapons(void) { return 10; }
struct WeaponDef *__cdecl BG_GetWeaponDef(int i) { assert(i==1);return &weapon; }
int Q_stricmp(const char *a,const char *b) { return strcasecmp(a,b); }
DObj *__cdecl Com_GetServerDObj(int i) { assert(i==0);return available?&model:NULL; }
int __cdecl SV_DObjGetBoneIndex(gentity_t *e,unsigned t) { assert(e==g_entities&&t<65536);++lookups;return bones[t]; }
int GetNativeWeaponHideTags(const struct WeaponDef *w,const weapon_hide_tag_handles_t **t)
{ assert(w==&weapon);*t=&tags;return mounted; }
void __cdecl DObjSetHidePartBits(DObj *obj,const unsigned *b)
{ assert(obj==&model);memcpy(applied,b,sizeof(applied));++writes; }
#define CHECK(x) do { assert(x);++cases; } while(0)
static void reset(void)
{
    int i; memset(&weapon,0,sizeof(weapon));memset(&tags,0,sizeof(tags));
    for(i=0;i<65536;++i)bones[i]=-1;
    available=resolved=mounted=1;errors=objects=prints=lookups=writes=0;name="weapon";
    model.numBones=253;g_entities[0].s.number=0;
    ShowAllEntityParts(g_entities,&model);writes=0;
}
int main(void)
{
    scr_entref_t ref={0};uint32_t b[8],seed[8]={0x80,0,0,0,0,0,0,1};unsigned i;
    reset();ref.classnum=1;ScrCmd_UseWeaponHideTags(ref);CHECK(objects==1&&!errors&&!lookups&&!writes);ref.classnum=0;
    reset();available=0;ScrCmd_UseWeaponHideTags(ref);CHECK(errors==1&&!objects&&!lookups&&!writes);
    for(i=0;i<4;++i){reset();resolved=i==0?0:i==1?-1:i==2?10:99;ScrCmd_UseWeaponHideTags(ref);CHECK(prints==1&&!errors&&!objects&&!writes);}
    reset();resolved=0;name="";ScrCmd_UseWeaponHideTags(ref);CHECK(!prints&&!writes);
    reset();resolved=0;name="NONE";ScrCmd_UseWeaponHideTags(ref);CHECK(!prints&&!writes);
    /* All32 source slots, across every native mask word; existing bits survive. */
    reset();StoreEntityHidePartBits(g_entities,seed);
    for(i=0;i<32;++i){tags.tags[i]=(unsigned short)(i+1);bones[i+1]=(int)(i*8);}
    ScrCmd_UseWeaponHideTags(ref);GetEntityHidePartBits(g_entities,b);
    CHECK(lookups==32&&writes==32&&!errors&&!prints);
    for(i=0;i<8;++i)CHECK(b[i]==(0x80808080u|seed[i])&&applied[i]==b[i]);
    /* Missing and out-of-model bones do not terminate or mutate the mask. */
    reset();tags.tags[0]=100;tags.tags[1]=101;tags.tags[2]=102;bones[100]=-1;bones[101]=253;bones[102]=252;
    ScrCmd_UseWeaponHideTags(ref);GetEntityHidePartBits(g_entities,b);
    CHECK(lookups==3&&writes==2&&!errors);CHECK(b[7]==8);for(i=0;i<7;++i)CHECK(!b[i]);
    /* First zero is a terminator, never a sparse-list skip. */
    reset();tags.tags[0]=1;tags.tags[2]=2;bones[1]=128;bones[2]=252;
    ScrCmd_UseWeaponHideTags(ref);GetEntityHidePartBits(g_entities,b);CHECK(lookups==1&&b[4]==0x80000000u&&!b[7]);
    /* Retained fastfile fallback is exactly eight slots, preserving upper words. */
    reset();mounted=0;StoreEntityHidePartBits(g_entities,seed);
    for(i=0;i<8;++i){weapon.hideTags[i]=(unsigned short)(i+1);bones[i+1]=(int)(i*32);}
    ScrCmd_UseWeaponHideTags(ref);GetEntityHidePartBits(g_entities,b);CHECK(lookups==8);
    for(i=0;i<8;++i)CHECK(b[i]==(0x80000000u|seed[i]));
    ShowAllEntityParts(g_entities,&model);GetEntityHidePartBits(g_entities,b);
    for(i=0;i<8;++i)CHECK(!b[i]&&!applied[i]);
    printf("weapon_hide_masks assertions=%d actual_method_and_eight_word_storage=PASS\n",cases);return 0;
}

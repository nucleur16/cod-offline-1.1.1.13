#include "server.h"
#include "g_shared.h"
#include "script_entity_fx.h"
#include <assert.h>
#include <stdio.h>
gentity_t g_entities[MAX_GENTITIES];
static gclient_t player;
static int argc,result,errors;
int Scr_GetNumParam(void){return argc;}
void Scr_AddBool(bool n){result=n;}
void Scr_ObjectError(const char *s){assert(s);++errors;}
void Scr_Error(const char *s){assert(s);++errors;}
int main(void)
{
 scr_entref_t ref={0,0};g_entities[0].client=&player;g_entities[0].health=100;
 player.ps.weaponstate=0;PlayerCmd_ViewmodelFXReady(ref);assert(result);
 player.ps.weaponstate=5;PlayerCmd_ViewmodelFXReady(ref);assert(result);
 player.ps.weaponstate=6;PlayerCmd_ViewmodelFXReady(ref);assert(!result);
 player.ps.weaponstate=0;player.ps.viewlocked=1;PlayerCmd_ViewmodelFXReady(ref);assert(!result);
 player.ps.viewlocked=0;g_entities[0].health=0;PlayerCmd_ViewmodelFXReady(ref);assert(!result);
 argc=1;PlayerCmd_ViewmodelFXReady(ref);assert(errors==1);argc=0;
 ref.classnum=1;PlayerCmd_ViewmodelFXReady(ref);assert(errors==2);
 puts("PASS production viewmodel readiness live/dead, native firing/reload, viewlock and argument/receiver gates");
}

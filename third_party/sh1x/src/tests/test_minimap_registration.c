/* Actual C1 minimap bank registration; configstring/network engine boundaries. */
#include "script_minimap.h"
#include "sv_state.h"
#include "server.h"
#include "qcommon.h"
#include "scr_vm.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
serverStatic_t svs;
cvar_t *sv_maxclients;
static configstrings_t bank,inherited;
static char values[15][64];
static unsigned writes,errors;
configstrings_t *ServerConfigstrings(void){return &bank;}
const configstrings_t *ServerInheritedConfigstrings(void){return &inherited;}
const char *GetConfigstring(const configstrings_t *s,unsigned i){assert(i>=6085&&i<=6099);return s==&bank?values[i-6085]:"";}
int OL_SetConfigstring(configstrings_t *s,unsigned i,const char *v){assert(s==&bank&&i>=6085&&i<=6099);assert(strlen(v)<64);strcpy(values[i-6085],v);++writes;return 1;}
int Q_stricmp(const char *a,const char *b){return strcasecmp(a,b);}
void Scr_Error(const char *s){(void)s;++errors;}
void SV_SendServerCommandNoLoss(client_t *c,const char *fmt,...){(void)c;(void)fmt;assert(0);}
int main(void){
 assert(!RegisterMinimapIcon(""));assert(!writes);
 assert(RegisterMinimapIcon("compass_objpoint_helicopter_busy")==1);assert(writes==1);
 assert(RegisterMinimapIcon("COMPASS_OBJPOINT_HELICOPTER_BUSY")==1);assert(writes==1);
 assert(RegisterMinimapIcon("hud_minimap_pavelow_red")==2);assert(writes==2);
 assert(!RegisterMinimapIcon("bad;name"));assert(errors==1&&writes==2);
 for(unsigned i=2;i<15;++i){char name[32];snprintf(name,sizeof(name),"icon_%u",i);assert(RegisterMinimapIcon(name)==i+1);}
 assert(RegisterMinimapIcon("hud_minimap_pavelow_red")==2);
 assert(!RegisterMinimapIcon("overflow"));assert(errors==2&&writes==15);
 puts("PASS: actual minimap registration uses CS6085..6099 selectors1..15, reuses case-insensitive names, rejects invalid/exhausted bank");
}

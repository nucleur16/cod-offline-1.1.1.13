#include "g_shared.h"
#include "cvar.h"
#include "qcommon_io.h"
#include "dev_cheats.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static cvar_t cheats,admission;
cvar_t *g_cheats=&cheats;
cvar_t *Cvar_FindVar(const char *name){return !strcmp(name,"admissionRequired")?&admission:NULL;}
void Q_strncpyz(char *dest,const char *src,int n){snprintf(dest,(size_t)n,"%s",src);}
void Com_Printf(conChannel_t channel,const char *fmt,...){(void)channel;assert(fmt);}
void Com_PrintWarning(conChannel_t channel,const char *fmt,...){(void)channel;assert(fmt);}
int main(void)
{
 assert(!CheatsTainted() && !CheatsPermitted());
 assert(CheatsRefusal() && !CheatsBlockProgression("test"));
 cheats.boolean=1;admission.boolean=1;
 assert(!CheatsPermitted() && CheatsRefusal() && !DevMapAllowed());
 admission.boolean=0;
 assert(CheatsPermitted() && !CheatsRefusal() && DevMapAllowed());
 assert(CheatsBlockProgression("test") && CheatsBlockProgression("test"));
 cheats.boolean=0;
 assert(!CheatsPermitted() && CheatsTainted() && CheatsBlockProgression("other"));
 puts("PASS: production cheats require sv_cheats and isolated server; tainted progression stays disabled");
}

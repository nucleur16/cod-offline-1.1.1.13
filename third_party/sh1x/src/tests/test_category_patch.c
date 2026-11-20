/* Execute the actual ranked patch-folder enumerator at filesystem boundaries. */
#include "category_patch.h"
#include "load_blocks.h"
#include "qcommon_io.h"
#include "cvar.h"
#include "filesystem.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
static cvar_t base,folder; cvar_t *fs_basepath=&base;
static int use_override,freed,warnings;
static char *files[]={"zm_pap_camo.ff","survival_balance.ff","mp_weapons_postgfx_mp.ff","mp_unused_custommap.ff","mp_weapons.ff","mp_killhouse_sh.ff","coop_survival_old.ff","mp_codol_weapons_ui_mp.ff","mp_codol_weapons_backup.ff","common_extras.ff"};
cvar_t *Cvar_FindVar(const char *n){assert(!strcmp(n,"patchZonePath"));return use_override?&folder:NULL;}
int Q_stricmp(const char*a,const char*b){return strcasecmp(a,b);}
int Q_stricmpn(const char*a,const char*b,int n){return strncasecmp(a,b,n);}
void Q_strncpyz(char*d,const char*s,int n){snprintf(d,n,"%s",s);}
int Com_sprintf(char*d,int n,const char*f,...){va_list a;va_start(a,f);int k=vsnprintf(d,n,f,a);va_end(a);return k;}
void Com_PrintWarning(conChannel_t c,const char*f,...){(void)c;(void)f;++warnings;}
void Com_Printf(conChannel_t c,const char*f,...){(void)c;(void)f;}
char **Sys_ListFiles(const char*d,const char*e,char*f,int*n,qboolean w){(void)f;assert(!w);assert(!strcmp(e,".ff"));assert(!strcmp(d,use_override?"override":"game/client_shipRetail/patch"));*n=sizeof(files)/sizeof(files[0]);return files;}
void Sys_FreeFileList(char**p){assert(p==files);++freed;}
int BuildZonePath(const char*n,char*out,size_t size){snprintf(out,size,"stock/%s.ff",n);return 1;}
qboolean FS_FileExistsOSPath(const char*p){return !strcmp(p,"stock/mp_killhouse_sh.ff");}
static size_t run(const char*category,size_t capacity,const char**paths,int*ranks,char storage[][MAX_OSPATH]){
 paths[0]="base.ff";ranks[0]=3001;return AppendCategoryPatchZones(category,paths,ranks,storage,1,capacity);
}
int main(void){
 const char *paths[16];int ranks[16];char storage[16][MAX_OSPATH];size_t n;
 base.string="game";folder.string="override";
 assert(CategoryPatchZoneApplies("survival","mp_weapons.ff"));
 assert(CategoryPatchZoneApplies("survival","MP_CODOL_WEAPONS_POSTGFX_MP.FF"));
 for(unsigned i=0;i<sizeof(files)/sizeof(files[0]);++i){
  const char*f=files[i];int expected=!strcmp(f,"survival_balance.ff")||!strcmp(f,"mp_weapons.ff")||!strcmp(f,"mp_weapons_postgfx_mp.ff");
  assert(CategoryPatchZoneApplies("survival",f)==expected);
 }
 assert(!CategoryPatchZoneApplies("survival","../mp_weapons.ff"));
 /* an install from before the zone names dropped "codol": its old files still apply, and only while the new one is not there */
 assert(CategoryPatchZoneApplies("survival","mp_codol_weapons.ff")); /* LEGACY_ZONE */
 assert(CategoryPatchZoneApplies("survival","MP_CODOL_WEAPONS_POSTGFX_MP.FF")); /* LEGACY_ZONE */
 {char *both[]={"mp_codol_weapons.ff","mp_weapons.ff"},*old_only[]={"mp_codol_weapons.ff","mp_killhouse_sh.ff"}; /* LEGACY_ZONE */
  assert(CategoryPatchZoneSuperseded("mp_codol_weapons.ff",both,2)); /* LEGACY_ZONE */
  assert(!CategoryPatchZoneSuperseded("mp_codol_weapons.ff",old_only,2)); /* LEGACY_ZONE */
  assert(!CategoryPatchZoneSuperseded("mp_weapons.ff",both,2));}
 assert(!CategoryPatchZoneApplies("zm","mp_weapons.ff"));
 assert(!CategoryPatchZoneApplies("coop","mp_weapons.ff"));
 n=run("survival",16,paths,ranks,storage);assert(n==4&&freed==1);
 assert(!strcmp(paths[1],"game/client_shipRetail/patch/mp_weapons.ff"));
 assert(!strcmp(paths[2],"game/client_shipRetail/patch/mp_weapons_postgfx_mp.ff"));
 assert(!strcmp(paths[3],"game/client_shipRetail/patch/survival_balance.ff"));
 assert(ranks[1]<ranks[2]&&ranks[2]<ranks[3]&&ranks[3]<ranks[0]);
 use_override=1;n=run("survival",2,paths,ranks,storage);assert(n==2&&strstr(paths[1],"override/mp_weapons.ff")&&warnings==2);
 n=run("mp",16,paths,ranks,storage);assert(n==6); /* all original MPprefix files except stock map */
 n=run("zm",16,paths,ranks,storage);assert(n==2&&strstr(paths[1],"zm_pap_camo"));
 n=run("coop",16,paths,ranks,storage);assert(n==2&&strstr(paths[1],"coop_survival_old"));
 puts("PASS: actual category patch enumerator borrows only Survival shared weapon bases, sorted precedence, stock collision/capacity/override and other categories preserved");
}

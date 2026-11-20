/* Exact merged startup function, filesystem/dvar/archive mount boundaries
 * doubled. Verifies ranked paths/counts/slot lifetime, not archive decoding. */
#include "q_shared.h"
#include "cvar.h"
#include "filesystem.h"
#include "qcommon_io.h"
#include "qcommon_parsecmdline.h"
#include "client_crypto.h"
#include "load_blocks.h"
#include "match_report.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static cvar_t base,key,infected,other;
cvar_t *fs_basepath=&base;
static int ffotd,auto_infected,fail_build,fail_mount,mounts,cases,sinks;
static size_t mounted_count;
static char paths_copy[11][MAX_OSPATH];static int ranks_copy[11];
void Com_StartupVariable(const char *s) { assert(s&&*s); }
cvar_t *Cvar_RegisterString(const char *n,const char *v,unsigned short f,const char *d)
{ (void)v;(void)f;(void)d;return !strcmp(n,"infectedModelZone")?&infected:&other; }
cvar_t *Cvar_FindVar(const char *n) { return !strcmp(n,"clientExe")?&key:&other; }
void InstallMatchDataSink(void) { ++sinks; }
int DefaultClientKeyPath(const char *b,char *out,size_t n)
{ assert(b==base.string);return snprintf(out,n,"%s/client.exe",b)>0; }
int BuildZonePath(const char *n,char *out,size_t cap)
{ if(fail_build)return 0;return snprintf(out,cap,"/stock/%s.ff",n)>0; }
qboolean FS_FileExistsOSPath(const char *p)
{ if(strstr(p,"ffotd_client"))return ffotd;return auto_infected; }
void Q_strncpyz(char *d,const char *s,int n) { assert(n>0);snprintf(d,(size_t)n,"%s",s); }
int MountRankedArchives(const char *const *p,const int *r,size_t n,const char *k,size_t *failed,unsigned *asset)
{
 size_t i;assert(n<=11&&k&&*k);++mounts;mounted_count=n;
 for(i=0;i<n;++i){snprintf(paths_copy[i],sizeof(paths_copy[i]),"%s",p[i]);ranks_copy[i]=r[i];}
 *failed=0;*asset=1;return !fail_mount;
}
const char *SEH_GetLanguageName(unsigned i) { assert(i==0);return "chinese"; }
int SEH_GetCurrentLanguage(void) { return 0; }
void QDECL Com_PrintError(conChannel_t c,const char *f,...) { (void)c;(void)f; }
void QDECL Com_Printf(conChannel_t c,const char *f,...) { (void)c;(void)f; }
#include "startup.inc"
#define CHECK(x) do {assert(x);++cases;}while(0)
static void reset(void)
{ base.string="/base";key.string="/client.exe";infected.string="";other.string="";ffotd=auto_infected=fail_build=fail_mount=mounts=sinks=0;mounted_count=0;memset(paths_copy,0,sizeof(paths_copy)); }
int main(void)
{
 int f,z;size_t i;char too_long[MAX_OSPATH+1];memset(too_long,'x',sizeof(too_long)-1);too_long[sizeof(too_long)-1]=0;
 for(f=0;f<2;++f)for(z=0;z<2;++z){
  reset();ffotd=f;auto_infected=z;CHECK(LoadStartupArchives()==1);CHECK(mounts==1&&sinks==1&&mounted_count==(size_t)(9+f+z));
  for(i=0;i<9;++i)CHECK(strstr(paths_copy[i],"/stock/")==paths_copy[i]);
  if(f)CHECK(!strcmp(paths_copy[9],"/stock/ffotd_client.ff")&&ranks_copy[9]==LINK_RANK_FFOTD);
  if(z)CHECK(!strcmp(paths_copy[9+f],"/base/client_shipRetail/patch/infected_cyborg_postgfx_mp.ff")&&ranks_copy[9+f]==LINK_RANK_PATCH_FOLDER);
 }
 reset();ffotd=1;infected.string="/custom/appearance.ff";CHECK(LoadStartupArchives()==1);CHECK(mounted_count==11&&!strcmp(paths_copy[10],infected.string)&&ranks_copy[10]==LINK_RANK_PATCH_FOLDER);
 reset();infected.string=too_long;CHECK(!LoadStartupArchives()&&!mounts);
 reset();fail_build=1;CHECK(!LoadStartupArchives()&&!mounts);
 reset();fail_mount=1;CHECK(!LoadStartupArchives()&&mounts==1);
 reset();key.string="";CHECK(LoadStartupArchives()==1&&mounts==1);
 printf("startup_ranked_archives assertions=%d exact_function=PASS\n",cases);return 0;
}

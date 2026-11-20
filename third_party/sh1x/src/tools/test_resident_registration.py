"""Compile actual resident registration/identity validation with engine providers.
Never boots a server. Main-union build must not claim a frozen qualification.
"""
import re,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/online/net/persistent_runtime.c').read_text()
hex_body=re.search(r'static int hex_id\(.*?^}',s,re.M|re.S).group()
register=s[s.index('void PersistentRuntimeRegister(void)'):]
commands=re.findall(r'Cmd_AddCommand\("[^"\n]+",([a-z_]+)\)',register)
prolog=r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "persistent_build.h"
#define CVAR_INIT 1
#define SS_DEAD 0
typedef struct {const char *string;int boolean;} cvar_t;
typedef struct {char boot[33];} policy_t;
typedef struct {int known,humans,bots;} persistent_observation_t;
static policy_t policy;
static int registered,enabled,quarantined,ever_world,commands,initializations,observations;
static cvar_t boot_var,root_var={"/tmp"},slot_var={"11111111111111111111111111111111"},catalogue_var={"2222222222222222222222222222222222222222222222222222222222222222"};
static cvar_t *root,*slot,*catalogue,*com_sv_running;
static struct {int state;} sv;
static struct {int initialized;} svs;
static struct {int num_entities;} level;
static struct {int function_count;} gScrVmPub;
static void unused_command(void){}
static cvar_t *Cvar_RegisterString(const char *name,const char *value,int flags,const char *help){
 (void)value;(void)help;assert(flags==CVAR_INIT);
 if(!strcmp(name,"persistentBoot"))return &boot_var;
 if(!strcmp(name,"persistentRoot"))return &root_var;
 if(!strcmp(name,"persistentSlot"))return &slot_var;
 assert(!strcmp(name,"persistentCatalogue"));return &catalogue_var;
}
static void Cmd_AddCommand(const char *name,void (*fn)(void)){assert(name && fn);++commands;}
static int PersistentInit(policy_t *p,const char *boot){(void)p;(void)boot;++initializations;return 1;}
static const char *Cvar_GetVariantString(const char *name){(void)name;return "";}
static int Cvar_VariableBooleanValue(const char *name){(void)name;return 0;}
static void PersistentEventsEnable(int on){(void)on;}
static persistent_observation_t observe(void){++observations;return (persistent_observation_t){1,0,0};}
static int Scr_IsSystemActive(int n){(void)n;return 0;}
'''
main=r'''
int main(int argc,char **argv){
 (void)argv;boot_var.string=argc>1?"0123456789abcdef0123456789abcdef":"";
 assert(!strcmp(RESIDENT_PROFILE_NAME,"unqualified") && !RESIDENT_PROFILE_SHA[0]);
 PersistentRuntimeRegister();int first=commands;assert(first>0);
 PersistentRuntimeRegister();assert(commands==first);
 assert(!initializations && !observations);
 if(argc>1)assert(enabled && quarantined);else assert(!enabled && !quarantined);
 puts("PASS: real resident registration defaults disabled and quarantines unqualified explicit boot before allocation");
}
'''
with tempfile.TemporaryDirectory(prefix='codol-resident-register-') as d:
 p=Path(d);c=p/'register.c';exe=p/'register'
 c.write_text(prolog+'\n'.join('#define '+n+' unused_command' for n in commands)+'\n'+hex_body+'\n'+register+main)
 subprocess.run(['gcc','-std=c99','-O1','-I'+str(root/'src/online/net'),str(c),'-o',str(exe)],check=True)
 for args in [[],['explicit-boot']]:subprocess.run([str(exe),*args],check=True)

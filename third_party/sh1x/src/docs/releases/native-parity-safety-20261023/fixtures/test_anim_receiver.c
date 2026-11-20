/* Actual CODOL_Scr_GetAnimTime/anim_param/receiver + retained Scr_GetAnim,
 * Scr_GetAnims and XAnimGetTime. Named entity/VM exception boundaries only. */
#include "scr_vm_functions.h"
#include "cscr_variable.h"
#include "xassets/xanims.h"
#include "codol_script_model_tree.h"
#include "codol_script_anim.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
gentity_t g_entities[MAX_GENTITIES];
scrVmPub_t gScrVmPub;struct scrVmGlob_t gScrVmGlob;struct scrCompilePub_t gScrCompilePub;scrVarPub_t gScrVarPub;
unsigned char gScrAnimPub[0x500];XAnimInfo g_xAnimInfo[4096];
const char *var_typename[32]={[0]="undefined",[6]="int",[11]="animation"};
static jmp_buf error_jump;static int error_kind,error_index,results;static float result;
static struct XAnimTree_s tree,other_tree,*owned,*dobj_tree;static DObj dobj;static int has_dobj;
static struct {const char *name;unsigned size;const char **debug;XAnimEntry entries[4];} anims,other;
static VariableValue value;static scr_entref_t receiver;
char *va(const char *fmt,...){static char text[2048];va_list ap;va_start(ap,fmt);vsnprintf(text,sizeof(text),fmt,ap);va_end(ap);return text;}
void Scr_ParamError(int index,const char *text){assert(text&&*text);error_kind=1;error_index=index;longjmp(error_jump,1);}
void Scr_ObjectError(const char *text){assert(text&&*text);error_kind=2;longjmp(error_jump,1);}
void Scr_Error(const char *text){assert(text&&*text);error_kind=3;longjmp(error_jump,1);}
void Scr_ErrorJumpOut(void){error_kind=1;error_index=gScrVarPub.error_index-1;longjmp(error_jump,1);}
void Sys_Error(const char *fmt,...){(void)fmt;abort();}
void RemoveRefToValue(int type,union VariableUnion val){(void)type;(void)val;}
void Q_strncpyz(char *dest,const char *source,int size){assert(size>0);snprintf(dest,(size_t)size,"%s",source);}
int Scr_GetNumParam(void){return gScrVmPub.outparamcount;}
unsigned Scr_GetType(unsigned index){assert(index<gScrVmPub.outparamcount);return value.type;}
void Scr_AddFloat(float f){results++;result=f;}
struct XAnimTree_s *CODOL_ActorOwnedTree(gentity_t *ent){assert(ent==&g_entities[1]);return owned;}
int CODOL_ScriptModelTreeEligible(const gentity_t *ent){(void)ent;return 0;}
int CODOL_ScriptModelBindAnimTree(gentity_t *ent,unsigned id,const char **why){(void)ent;(void)id;(void)why;abort();}
DObj *Com_GetServerDObj(int handle){assert(handle==1);return has_dobj?&dobj:NULL;}
struct XAnimTree_s *DObjGetTree(DObj *obj){assert(obj==&dobj);return dobj_tree;}
int DB_IsXAssetDefault(int type,const void *ptr){(void)type;(void)ptr;return 0;}
static void set_lookup(unsigned id,void *ptr){memcpy(gScrAnimPub+0x20c+4*id,&ptr,4);}
static void setup(void){
 memset(g_entities,0,sizeof(g_entities));memset(&gScrVmPub,0,sizeof(gScrVmPub));memset(&gScrVarPub,0,sizeof(gScrVarPub));memset(gScrAnimPub,0,sizeof(gScrAnimPub));memset(g_xAnimInfo,0,sizeof(g_xAnimInfo));memset(&anims,0,sizeof(anims));memset(&other,0,sizeof(other));
 anims.size=4;other.size=4;tree=(struct XAnimTree_s){(XAnim_s *)&anims,1,0};other_tree=(struct XAnimTree_s){(XAnim_s *)&other,0,0};owned=dobj_tree=&tree;has_dobj=1;
 g_entities[1].r.inuse=1;g_entities[1].s.number=1;receiver=(scr_entref_t){1,0};
 value.type=VAR_ANIMATION;value.u.intValue=0x10001;gScrVmPub.top=&value;gScrVmPub.outparamcount=1;gScrVmPub.function_count=1;
 *(unsigned *)(gScrAnimPub+0x410)=2;set_lookup(1,&anims);set_lookup(2,&other);
 g_xAnimInfo[1].animIndex=1;g_xAnimInfo[1].state.time=0.375f;
 error_kind=0;error_index=-1;results=0;result=-100;
}
static void run_case(unsigned n){
 setup();
 switch(n){
 case 0:break; /* Actual native tree phase, no debug-name metadata. */
 case 1:receiver.classnum=1;break;
 case 2:receiver.entnum=MAX_GENTITIES;break;
 case 3:g_entities[1].r.inuse=0;break;
 case 4:owned=NULL;break;
 case 5:tree.anims=NULL;break;
 case 6:anims.size=0;break;
 case 7:has_dobj=0;break;
 case 8:dobj_tree=&other_tree;break;
 case 9:gScrVmPub.outparamcount=0;break;
 case 10:gScrVmPub.outparamcount=2;break;
 case 11:value.type=VAR_INTEGER;break;
 case 12:value.u.intValue=0;break;
 case 13:set_lookup(1,NULL);break;
 case 14:value.u.intValue=0x30001;break;
 case 15:value.u.intValue=0xffff0001u;break;
 case 16:value.u.intValue=0x20001;break;
 case 17:value.u.intValue=0x10004;break;
 case 18:value.u.intValue=0x1ffff;break;
 case 19:anims.entries[1].numAnims=2;anims.entries[1].u.animParent.flags=1;g_xAnimInfo[1].state.time=0.625f;break;
 case 20:anims.entries[1].numAnims=2;break;
 case 21:tree.children=0;break;
 case 22:*(unsigned *)(gScrAnimPub+0x410)=128;break;
 case 23:anims.entries[1].numAnims=2;anims.entries[1].u.animParent.flags=2;g_xAnimInfo[1].state.time=0.875f;break;
 default:abort();
 }
 if(!setjmp(error_jump))CODOL_Scr_GetAnimTime(receiver);
 if(n==0||n==19||n==21||n==23){float want=n==0?0.375f:n==19?0.625f:n==21?0.0f:0.875f;assert(!error_kind&&results==1&&fabsf(result-want)<0.00001f);}
 else {assert(error_kind&&results==0);if(n>=12&&n<=18)assert(error_kind==1&&error_index==0);}
}
int main(int argc,char **argv){
 _Static_assert(sizeof(void *)==4,"exact retained x86 ABI");_Static_assert(sizeof(XAnimInfo)==56,"retained info size");_Static_assert(offsetof(XAnimInfo,state)==24,"retained phase offset");
 if(argc==2&&!strcmp(argv[1],"--donor")){
  run_case(0);
  unsigned cases[]={12,13};for(unsigned i=0;i<2;i++){pid_t child=fork();assert(child>=0);if(!child){alarm(3);run_case(cases[i]);_exit(0);}int status;assert(waitpid(child,&status,0)==child);assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGSEGV);printf("DONOR_EXPECTED_SIGSEGV case=%u\n",cases[i]);}
  puts("DONOR_VALID_PHASE_AND_TWO_CRASH_COUNTEREXAMPLES_PASS");return 0;
 }
 for(unsigned n=0;n<24;n++)run_case(n);
 puts("ACTUAL_NATIVE_ANIM_RECEIVER_24_CASES_PASS");return 0;
}

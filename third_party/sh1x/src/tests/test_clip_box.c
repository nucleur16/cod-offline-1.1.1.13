#include "g_shared.h"
#include "entity.h"
#include "clip_box.h"
#include "scr_vm.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
gentity_t g_entities[MAX_GENTITIES];
static int argc=2,args[2]={26,85},errors,links;
int Scr_GetNumParam(void){return argc;}
int Scr_GetInt(unsigned i){return args[i];}
void Scr_Error(const char *s){assert(s);++errors;}
void Scr_ObjectError(const char *s){Scr_Error(s);}
void Scr_ParamError(int i,const char *s){assert(i==0||i==1);Scr_Error(s);}
char *va(const char *fmt,...){static char b[1024];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);return b;}
void SV_LinkEntity(gentity_t *ent){assert(ent==&g_entities[100]);++links;}
int main(void)
{
 scr_entref_t ref={100,0};gentity_t *e=&g_entities[100];int solid=0;
 e->r.inuse=1;e->s.eType=ET_SCRIPTMOVER;
 Scr_SetClipBox(ref);
 assert(!errors && links==1 && e->r.contents==CLIP_BOX_CONTENTS);
 assert(ClipBoxSolid(100,e->r.mins,e->r.maxs,e->r.contents,0,&solid));
 assert((solid&255)-1==26 && 2-((solid>>8)&255)==2 && ((solid>>16)&255)-33==85);
 /* The wire overlay changes the reserved native byte1 zero to C1 byte1 two. */
 assert(e->r.mins[0]==-26 && e->r.maxs[0]==26 && !e->r.mins[2] && e->r.maxs[2]==85);
 assert(!ClipBoxSolid(100,e->r.mins,e->r.maxs,0,0,&solid));
 assert(!ClipBoxSolid(100,e->r.mins,e->r.maxs,e->r.contents,1,&solid));
 e->r.maxs[0]++;assert(!ClipBoxSolid(100,e->r.mins,e->r.maxs,e->r.contents,0,&solid));e->r.maxs[0]--;
 ClipBoxFreed(100);assert(!ClipBoxSolid(100,e->r.mins,e->r.maxs,e->r.contents,0,&solid));
 args[0]=255;Scr_SetClipBox(ref);assert(errors==1 && links==1);
 args[0]=26;args[1]=223;Scr_SetClipBox(ref);assert(errors==2 && links==1);
 args[1]=85;argc=1;Scr_SetClipBox(ref);assert(errors==3 && links==1);
 argc=2;e->client=(void *)1;Scr_SetClipBox(ref);assert(errors==4 && links==1);e->client=NULL;
 ref.classnum=1;Scr_SetClipBox(ref);assert(errors==5 && links==1);ref.classnum=0;
 Scr_SetClipBox(ref);argc=0;Scr_SetClipBox(ref);
 assert(links==3 && !e->r.contents && !e->r.mins[0] && !e->r.maxs[2]);
 assert(!ClipBoxSolid(100,e->r.mins,e->r.maxs,e->r.contents,0,&solid));
 puts("PASS: production clipbox receiver/arguments, native bounds, solid eligibility, removal and slot cleanup");
}

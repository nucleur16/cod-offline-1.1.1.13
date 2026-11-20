/* Included ONLY by the actual cvar.c owner, after its original definitions.
 * Source-native ownership, no external mutation of private cvar fields.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdlib.h>
#define RESIDENT_DVARS 2048u
typedef struct {
    char *name,*current_string,*latched_string;
    cvar_t *original;
    CvarValue current,latched;
    CvarLimits limits;
    unsigned short flags;
    unsigned char type;
} resident_dvar_t;
static resident_dvar_t resident_dvars[RESIDENT_DVARS];
static unsigned resident_dvar_count;
static char resident_dvar_scope[65];

static int resident_digest(const char *s) {
    unsigned i,nonzero=0;
    if(!s||strlen(s)!=64)return 0;
    for(i=0;i<64;++i){if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;nonzero|=s[i]!='0';}
    return nonzero!=0;
}
static char *resident_copy(const char *s,size_t limit) {
    size_t size;char *copy;
    if(!s)s="";
    size=strnlen(s,limit+1);if(size>limit)return NULL;
    copy=malloc(size+1);if(copy)memcpy(copy,s,size+1);return copy;
}
int PersistentCvarsEmpty(void) {return !resident_dvar_scope[0]&&!resident_dvar_count;}
int PersistentCvarsBegin(const char *digest) {
    if(!PersistentRuntimeEnabled()||!resident_digest(digest)||!PersistentCvarsEmpty())return 0;
    strcpy(resident_dvar_scope,digest);return 1;
}
int PersistentCvarsRecord(const char *name) {
    resident_dvar_t pending;unsigned i;cvar_t *var;
    if(!PersistentRuntimeEnabled())return 1;
    /* The resident idle map runs the ordinary script initialization outside
     * any room. Only assigned-room writes belong to a rollback scope. */
    if(!resident_dvar_scope[0])return 1;
    if(!name||!*name||strlen(name)>255||!Cvar_ValidateString(name))return 0;
    for(i=0;i<resident_dvar_count;++i)if(!Q_stricmp(resident_dvars[i].name,name))return 1;
    if(resident_dvar_count==RESIDENT_DVARS)return 0;
    memset(&pending,0,sizeof(pending));
    pending.name=resident_copy(name,255);if(!pending.name)return 0;
    var=Cvar_FindVar(name);pending.original=var;
    if(var) {
        pending.current=var->current;pending.latched=var->latched;pending.limits=var->limits;
        pending.flags=(unsigned short)var->flags;pending.type=var->type;
        if(var->type==CVAR_STRING) {
            pending.current_string=resident_copy(var->string,CVAR_STRING_SIZE-1);
            pending.latched_string=resident_copy(var->latchedString,CVAR_STRING_SIZE-1);
            if(!pending.current_string||!pending.latched_string) {
                free(pending.name);free(pending.current_string);free(pending.latched_string);return 0;
            }
        }
    }
    resident_dvars[resident_dvar_count++]=pending;return 1;
}
static int resident_remove_user(cvar_t *var) {
    cvar_t **list,**hash;
    if(!(var->flags&CVAR_USER_CREATED))return 0;
    list=&cvar_vars;while(*list&&*list!=var)list=&(*list)->next;
    hash=&hashTable[generateHashValue(var->name)];while(*hash&&*hash!=var)hash=&(*hash)->hashNext;
    if(!*list||!*hash)return 0;
    *list=var->next;*hash=var->hashNext;
    cvar_modifiedFlags|=var->flags;
    Cvar_FreeStrings(var);memset(var,0,sizeof(*var));
    while(cvar_numIndexes&&!cvar_indexes[cvar_numIndexes-1].name)--cvar_numIndexes;
    return 1;
}
int PersistentCvarsApply(const char *name,const char *text) {
    CvarValue initial;CvarLimits limits;cvar_t *var;
    if(!PersistentRuntimeEnabled()||!resident_dvar_scope[0]||!text||
       strnlen(text,CVAR_STRING_SIZE)>=CVAR_STRING_SIZE||
       !PersistentCvarsRecord(name))return 0;
    Sys_EnterCriticalSection(CRITSECT_CVAR);
    var=Cvar_FindVar(name);
    if(!var) {
        /* Force-setting an unknown name creates a permanent STRING in this
         * engine. Later G_RegisterDvars cannot promote it to bool/int/float.
         * Use the actual console-created lifetime until its native owner
         * registers the real type; do not invent that owner's definition. */
        memset(&initial,0,sizeof(initial));memset(&limits,0,sizeof(limits));
        initial.string=text;
        var=Cvar_Register(name,CVAR_STRING,CVAR_USER_CREATED,initial,limits,"");
    }
    if(var)Cvar_Set2(name,text,qtrue);
    Sys_LeaveCriticalSection(CRITSECT_CVAR);
    return var!=NULL;
}
static int resident_set_values(cvar_t *var,const CvarValue *current,const CvarValue *latched) {
    if(var->type==CVAR_STRING) {
        char *a=CopyString(current->string?current->string:""),*b=CopyString(latched->string?latched->string:"");
        if(!a||!b){if(a)FreeString(a);if(b)FreeString(b);return 0;}
        if(var->string&&var->string!=nullstring)FreeString(var->string);
        if(var->latchedString&&var->latchedString!=nullstring)FreeString(var->latchedString);
        var->string=a;var->latchedString=b;
        return !strcmp(var->string,current->string?current->string:"") &&
            !strcmp(var->latchedString,latched->string?latched->string:"");
    }
    var->current=*current;var->latched=*latched;
    return !memcmp(&var->current,current,sizeof(*current))&&!memcmp(&var->latched,latched,sizeof(*latched));
}
static int resident_restore_one(resident_dvar_t *saved) {
    cvar_t *var=Cvar_FindVar(saved->name);
    if(!saved->original) {
        if(!var)return 1;
        if(var->flags&CVAR_USER_CREATED)return resident_remove_user(var);
        /* A native registration promoted this initially absent script value.
         * Preserve its stable native handle/definition, restore native default. */
        /* Cvar_SetVariant treats strings case-insensitively, even on reset.
         * Restore exact native default bytes here rather than retaining a
         * previous script value whose spelling differs only in case. */
        if(!resident_set_values(var,&var->reset,&var->reset))return 0;
        var->modified=qtrue;cvar_modifiedFlags|=var->flags;return 1;
    }
    /* Startup +set creates a string before a lazy native bool registration
     * (matchReportSpool does this on the first populated map). Preserve the
     * native handle/type and restore the two original boolean values. */
    if(var==saved->original&&(saved->flags&CVAR_USER_CREATED)&&saved->type==CVAR_STRING&&
       !(var->flags&CVAR_USER_CREATED)&&var->type==CVAR_BOOL&&
       saved->current_string&&saved->latched_string&&
       (!strcmp(saved->current_string,"0")||!strcmp(saved->current_string,"1"))&&
       (!strcmp(saved->latched_string,"0")||!strcmp(saved->latched_string,"1"))) {
        CvarValue current=var->current,latched=var->latched;
        current.boolean=saved->current_string[0]=='1';latched.boolean=saved->latched_string[0]=='1';
        if(!resident_set_values(var,&current,&latched))return 0;
        var->modified=qtrue;cvar_modifiedFlags|=var->flags;return 1;
    }
    if(var!=saved->original||var->type!=saved->type||memcmp(&var->limits,&saved->limits,sizeof(var->limits)))return 0;
    {
        CvarValue current=saved->current,latched=saved->latched;
        if(var->type==CVAR_STRING){current.string=saved->current_string;latched.string=saved->latched_string;}
        if(!resident_set_values(var,&current,&latched))return 0;
    }
    cvar_modifiedFlags|=var->flags|saved->flags;var->flags=(short)saved->flags;var->modified=qtrue;
    if(var->type==CVAR_STRING)return !strcmp(var->string,saved->current_string)&&!strcmp(var->latchedString,saved->latched_string);
    return !memcmp(&var->current,&saved->current,sizeof(var->current))&&!memcmp(&var->latched,&saved->latched,sizeof(var->latched));
}
int PersistentCvarsRestore(const char *digest) {
    unsigned i;
    if(!resident_digest(digest)||strcmp(digest,resident_dvar_scope))return 0;
    /* A failure keeps remaining ownership metadata intact and quarantines the
     * reset caller. It never grants Idle after a partial restoration. */
    for(i=resident_dvar_count;i>0;--i)if(!resident_restore_one(&resident_dvars[i-1])) {
        /* Names identify the refused owner without exposing its values. */
        Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL resident dvar restoration refused: %s\n",resident_dvars[i-1].name);
        return 0;
    }
    for(i=0;i<resident_dvar_count;++i) {
        free(resident_dvars[i].name);free(resident_dvars[i].current_string);free(resident_dvars[i].latched_string);
    }
    memset(resident_dvars,0,sizeof(resident_dvars));resident_dvar_count=0;resident_dvar_scope[0]=0;return 1;
}

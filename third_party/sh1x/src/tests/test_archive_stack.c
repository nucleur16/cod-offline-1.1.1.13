/* Regression for the retained assembly -> archived player-state call path.
 * The caller supplies four-byte stack alignment, not GCC's preferred 16.
 */
#include "g_shared.h"
#include "server.h"
#include "playerstate_ext.h"
#include "qcommon.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const void *PlayerStateExtLiveBase(unsigned *n){*n=0;return NULL;}
const void *PlayerStateExtSnapshotBase(unsigned *n){*n=0;return NULL;}
const void *PlayerStateExtArchiveBase(unsigned *n){*n=0;return NULL;}
void Com_PrintWarning(conChannel_t c,const char *f,...){(void)c;(void)f;}
qboolean Assert_MyHandler(const char *e,const char *f,int l,const char *fn,const char *fmt,...)
{(void)fmt;fprintf(stderr,"%s %s:%d %s\n",e,f,l,fn);abort();}
void theatre_read_misaligned(msg_t *,ps_ext_t *,const ps_ext_t *);
__asm__(".text\n.globl theatre_read_misaligned\n"
        "theatre_read_misaligned:\n"
        "pushl %ebp\nmovl %esp,%ebp\nsubl $4,%esp\n"
        "pushl 16(%ebp)\npushl 12(%ebp)\npushl 8(%ebp)\n"
        "call PlayerStateExtRead\nleave\nret\n");
int main(void)
{
    ps_ext_t input,output;
    byte data[8192];msg_t message;int size;unsigned i;
    PlayerStateExtFetch(&input,NULL);
    for(i=0;i<PERK_SLOTS;++i) input.perks.id[i]=i+1;
    MSG_Init(&message,data,sizeof(data));
    PlayerStateExtWrite(&message,&input,NULL);
    assert(!message.overflowed);size=message.cursize;
    MSG_Init(&message,data,sizeof(data));message.cursize=size;
    theatre_read_misaligned(&message,&output,NULL);
    assert(!message.overflowed);
    assert(!memcmp(&input.perks,&output.perks,sizeof(input.perks)));
    puts("PASS: archived player-state read accepts retained assembly stack alignment");
    return 0;
}

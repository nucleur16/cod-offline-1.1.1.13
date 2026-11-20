#include "configstrings.h"
#include "notetrack_effects.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
 configstrings_t base={0},bank={0};unsigned char pinned[BYTE_EFFECT_SLOTS]={0};int slot;char row[64];
 assert(OL_SetConfigstring(&base,CS_EFFECTS+1,"old"));
 assert(OL_SetConfigstring(&base,CS_EFFECTS+300,"trap"));
 assert(OL_SetConfigstring(&base,CS_TAGGED_EFFECTS+1,"300tag_origin"));
 assert(OL_SetConfigstring(&base,CS_TEAM_THERMAL,"300thermal"));
 assert(OL_SetConfigstring(&base,CS_NOTETRACK_EFFECTS,"a;b;c;300;tail"));
 pinned[1]=1;slot=ReserveByteEffectSlot(&bank,&base,300,pinned);assert(slot==2);
 assert(!strcmp(GetEffectiveConfigstring(&bank,&base,CS_EFFECTS+2),"trap"));
 assert(!strcmp(GetEffectiveConfigstring(&bank,&base,CS_EFFECTS+1),"old"));
 snprintf(row,sizeof(row),"%03dtag_origin",slot);assert(!strcmp(GetEffectiveConfigstring(&bank,&base,CS_TAGGED_EFFECTS+1),row));
 assert(!strcmp(GetEffectiveConfigstring(&bank,&base,CS_TEAM_THERMAL),"002thermal"));
 assert(!strcmp(GetEffectiveConfigstring(&bank,&base,CS_NOTETRACK_EFFECTS),"a;b;c;2;tail"));
 /* All low slots pinned: no partial bank mutation on refusal. */
 memset(pinned,1,sizeof(pinned));assert(OL_SetConfigstring(&base,CS_EFFECTS+301,"other"));
 assert(ReserveByteEffectSlot(&bank,&base,301,pinned)<0);
 assert(!strcmp(GetEffectiveConfigstring(&bank,&base,CS_EFFECTS+2),"trap"));
 ClearConfigstrings(&bank);ClearConfigstrings(&base);
 puts("PASS production FX reservation swap, pin preservation, compound references and exhausted-bank refusal");
}

/* CoDOL effect audit (read-only diagnostic).
 *
 * effectaudit [name ...]
 *     With no argument: every effect name C1 holds after the gamestate
 *     (CS 3651 + 1..319, the names CG_RegisterGraphics 0x44f040..0x44f06a
 *     hands to DB_FindXAssetHeader(fx) into 0x16d9c8c).  With names: those
 *     effects.  Each name, and every effect its elements reference (impact,
 *     death and emitted effects, runner visuals), is looked up in the zones
 *     this server mounted for the match.  A name that is absent there is
 *     printed as MISSING: C1, which mounts the same category zones, resolves
 *     it to its default effect and draws the "Missing FX" placeholder.
 *
 * RefreshNotetrackEffects (map start, after the archives are mounted
 * and the category's inherited configstrings installed) applies the same
 * test to C1's actor notetrack effect rows (notetrack_effects.h).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "qcommon.h"
#include "cmd.h"
#include <string.h>
#include "effects.h"
#include "configstrings.h"
#include "sv_state.h"
#include "notetrack_effects.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

static unsigned audit_one(const char *root,unsigned slot)
{
    const char *names[96]; unsigned count=1,index,missing=0;
    names[0]=root;
    for(index=0;index<count;++index) {
        const effect_source_t *source; unsigned i,j;
        int status=LoadMountedEffectSource(names[index],&source);
        if(status!=1) {
            Com_Printf(CON_CHANNEL_SERVER,"effectaudit: slot %u %s%s%s MISSING (status %d)\n",slot,root,
                index ? " -> " : "",index ? names[index] : "",status);
            ++missing;
            continue;
        }
        for(i=0;i<source->prefix.count;++i) {
            const effect_element_t *element=source->elements+i;
            unsigned type=element->record[176];
            for(j=0;j<3+(type==14 ? element->visual_count : 0);++j) {
                const char *child=j<3 ? element->effect_names[j] : element->visuals[j-3].string;
                unsigned k;
                if(!child || !*child) continue;
                for(k=0;k<count;++k) if(!Q_stricmp(names[k],child)) break;
                if(k<count || count==sizeof(names)/sizeof(names[0])) continue;
                names[count++]=child;
            }
        }
    }
    return missing;
}

static void EffectAudit_f(void)
{
    unsigned slot,used=0,missing=0;
    int i;
    if(Cmd_Argc()>1) {
        for(i=1;i<Cmd_Argc();++i) missing+=audit_one(Cmd_Argv(i),0);
        Com_Printf(CON_CHANNEL_SERVER,"effectaudit: %d names, %u missing references\n",Cmd_Argc()-1,missing);
        return;
    }
    for(slot=1;slot<EFFECT_SLOTS;++slot) {
        const char *name=GetEffectiveConfigstring(ServerConfigstrings(),
            ServerInheritedConfigstrings(),CS_EFFECTS+slot);
        if(!name || !*name) continue;
        ++used;
        {
            unsigned lost=audit_one(name,slot);
            if(!lost) Com_Printf(CON_CHANNEL_SERVER,"effectaudit: slot %u %s ok\n",slot,name);
            missing+=lost;
        }
    }
    Com_Printf(CON_CHANNEL_SERVER,"effectaudit: %u effect slots in use, %u missing references\n",used,missing);
    for(slot=0;slot<NOTETRACK_EFFECT_SLOTS;++slot) {
        const char *row=GetEffectiveConfigstring(ServerConfigstrings(),
            ServerInheritedConfigstrings(),CS_NOTETRACK_EFFECTS+slot);
        if(row && *row)
            Com_Printf(CON_CHANNEL_SERVER,"effectaudit: notetrack CS %u %s\n",CS_NOTETRACK_EFFECTS+slot,row);
    }
}

static int mounted_effect(const char *name,void *context)
{
    const effect_source_t *source;
    (void)context;
    return LoadMountedEffectSource(name,&source)==1;
}
static void report_notetrack(unsigned index,const char *row,const char *effect,void *context)
{
    (void)context;
    Com_Printf(CON_CHANNEL_SERVER,"CoDOL notetrack effects: CS %u '%s' names effect '%s', absent from the mounted zones; the row keeps its sound only\n",
        index,row,effect);
}
/* Rows naming an effect C1 cannot find in this match's zones play their
 * sound only, instead of C1's "Missing FX" placeholder. */
int RefreshNotetrackEffects(void)
{
    int changed=DisableUnavailableNotetrackEffects(ServerConfigstrings(),
        ServerInheritedConfigstrings(),mounted_effect,NULL,report_notetrack);
    if(changed<0) Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL notetrack effects: the row overrides failed\n");
    return changed>=0;
}

void RegisterEffectAudit(void)
{
    Cmd_AddCommand("effectaudit",EffectAudit_f);
}

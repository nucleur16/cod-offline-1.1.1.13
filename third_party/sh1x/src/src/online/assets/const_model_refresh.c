/* Map start: constant model rows naming an xmodel the match's zones lack are
 * overridden with the shipped equivalent (const_models.h), so C1
 * registers the shipped model at that slot instead of its default model.
 *
 * Runs after the archives are mounted and the category's inherited
 * configstrings installed, and before any entity registers a model: a map
 * entity or script naming the shipped model then reuses the overridden
 * constant slot (RegisterInheritedModelName matches effective names).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "qcommon.h"
#include "configstrings.h"
#include "sv_state.h"
#include "const_models.h"
#include "xmodel_native.h"
#include "effects.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

static int mounted_model(const char *name,void *context)
{
    const XModel *model=NULL;
    (void)context;
    return LoadEnginePhysicsXModel(name,&model)==1;
}
static void report_model(unsigned index,const char *from,const char *to,void *context)
{
    (void)context;
    Com_Printf(CON_CHANNEL_SERVER,"CoDOL constant models: CS %u '%s' is in no mounted zone; publishing '%s'\n",
        index,from,to);
}
static int mounted_effect(const char *name,void *context)
{
    const effect_source_t *source=NULL;
    (void)context;
    return LoadMountedEffectSource(name,&source)==1;
}
static void report_effect(unsigned index,const char *from,const char *to,void *context)
{
    (void)context;
    Com_Printf(CON_CHANNEL_SERVER,"CoDOL constant effects: CS %u '%s' is in no mounted zone; publishing '%s'\n",
        index,from,to);
}
int RefreshConstModels(void)
{
    const char *zone_type=ServerZoneType();
    int changed,effects;
    if(!zone_type) return 0;
    changed=SubstituteConstModels(ServerConfigstrings(),
        ServerInheritedConfigstrings(),zone_type,mounted_model,NULL,report_model);
    if(changed<0) Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL constant models: the slot overrides failed\n");
    /* The constant effect rows the same way (const_models.c). */
    effects=SubstituteConstEffects(ServerConfigstrings(),
        ServerInheritedConfigstrings(),zone_type,mounted_effect,NULL,report_effect);
    if(effects<0) Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL constant effects: the slot overrides failed\n");
    return changed>=0 && effects>=0;
}

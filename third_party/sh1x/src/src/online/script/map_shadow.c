#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Authored map override; C1 renderer identity verified against A7C759/A853F1.
 * Stage only: replay an owned baseline into the next initialized map bank. */
#ifdef OL_SERVER
#include "scr_vm.h"
#include "replicated_dvars.h"
#include "sv_state.h"
#include "map_shadow.h"
#include <math.h>
#include <stdio.h>
static int server_owns_override;
static int map_baseline_pending;

void Scr_SetMapSunSampleSize(void)
{
    float value;
    char text[32];
    if (Scr_GetNumParam()!=1) {
        Scr_Error("codol_setmapsunsamplesize requires one float"); return;
    }
    value=Scr_GetFloat(0);
    if (!isfinite(value) || value<0.015625f || value>32.0f) {
        Scr_Error("codol_setmapsunsamplesize is outside C1 range [0.015625,32]"); return;
    }
    if (!ServerInheritedConfigstrings()) {
        Scr_Error("codol_setmapsunsamplesize requires installed map constants"); return;
    }
    if (!RefreshMapSunSampleSize()) {
        Scr_Error("codol_setmapsunsamplesize could not publish map baseline"); return;
    }
    snprintf(text,sizeof(text),"%.9g",(double)value);
    if (!PublishReplicatedDvar("1617",text)) {
        Scr_Error("codol_setmapsunsamplesize could not publish C1 map value"); return;
    }
    server_owns_override=1;
}

void ResetMapSunSampleSize(void)
{
    /* Keep ownership across map reset. A final reliable command to the old
     * map could be discarded and cannot serve late joiners on the next map. */
    map_baseline_pending=server_owns_override;
}

int RefreshMapSunSampleSize(void)
{
    if (!map_baseline_pending || !ServerInheritedConfigstrings()) return 1;
    /* C1 registration and ui_viewer_hide use this baseline. This is not
     * recovery of an arbitrary earlier client preference. Keep it in the new
     * bank so reconnects and late joiners receive the same map-scoped state. */
    if (!PublishReplicatedDvar("1617","0.25")) return 0;
    map_baseline_pending=0;
    return 1;
}
#endif

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentMapShadowKnown(void) {
    return (server_owns_override==0||server_owns_override==1)&&
        (map_baseline_pending==0||map_baseline_pending==1);
}

int PersistentMapShadowSettled(void){return PersistentMapShadowKnown()&&map_baseline_pending==server_owns_override;}
#endif

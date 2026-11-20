#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "sv_frame.h"
#include "frame_builder.h"
#include <string.h>
#include <stdlib.h>
static struct {
    hud_state_t *hud;
    uint32_t sequence;
    int valid;
} captures[MAX_CLIENTS];
void InvalidateClientSnapshotHud(unsigned client)
{
    if(client<MAX_CLIENTS) captures[client].valid=0;
}

void ClearClientSnapshotHud(unsigned client)
{
    if(client>=MAX_CLIENTS) return;
    free(captures[client].hud);
    captures[client].hud=NULL; captures[client].valid=0;
}
int CaptureClientSnapshotHud(unsigned client,uint32_t sequence,const hudElemState_t *source)
{
    hud_state_t next;
    if(client>=MAX_CLIENTS) return 0;
    captures[client].valid=0;
    player_failure=NULL;
    if(!source || !CollectServerHud(&next,source)) return 0;
    if(!captures[client].hud) captures[client].hud=malloc(sizeof(next));
    if(!captures[client].hud) return 0;
    *captures[client].hud=next;
    captures[client].sequence=sequence; captures[client].valid=1;
    return 1;
}
int CopyClientSnapshotHud(unsigned client,uint32_t sequence,hud_state_t *out)
{
    if(!out || client>=MAX_CLIENTS || !captures[client].valid || captures[client].sequence!=sequence) return 0;
    *out=*captures[client].hud; return 1;
}
int OverlayClientSnapshotPlayer(unsigned client,uint32_t sequence,player_fields_t *fields,
    player_array_updates_t *arrays,hud_state_t *hud,
    const playerState_t *source,const engine_player_bindings_t *bindings)
{
    hud_state_t captured;
    if(!CopyClientSnapshotHud(client,sequence,&captured)) {
        if(!player_failure || strncmp(player_failure,"HUD",3))
            player_failure="HUD capture missing for this sequence";
        return 0;
    }
    return OverlayEnginePlayerWithHud(fields,arrays,hud,source,bindings,&captured);
}
#endif

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
int PersistentHudCapturesEmpty(void) {for(unsigned i=0;i<MAX_CLIENTS;++i)if(captures[i].hud||captures[i].valid)return 0;return 1;}

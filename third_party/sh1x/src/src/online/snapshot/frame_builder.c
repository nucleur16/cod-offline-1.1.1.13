#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "frame_builder.h"
#include "actor_delivery.h"
#include "sv_weapons.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "qcommon.h"
#include "server.h"
#include <stdio.h>
#pragma pack(push, 8)
struct native_frame_s {
    snapshot_state_t state;
    player_fields_t player;
    player_array_updates_t updates;
    hud_state_t hud;
    entity_state_t entities[ENTITYNUM_WORLD];
    client_state_t clients[32];
    actor_state_t actors[32];
    animcmd_t *commands;
    size_t command_count;
};
#pragma pack(pop)
static native_frame_t *client_frames[MAX_CLIENTS];
static unsigned char client_frame_valid[MAX_CLIENTS];
const char *frame_failure;
void ClearClientNativeFrame(unsigned client)
{
    if(client>=MAX_CLIENTS) return;
    FreeNativeFrame(client_frames[client]); client_frames[client]=NULL;
    client_frame_valid[client]=0;
}
const native_frame_t *ClientNativeFrame(unsigned client,uint32_t sequence)
{
    if(client>=MAX_CLIENTS || !client_frame_valid[client] ||
       client_frames[client]->state.sequence!=sequence) return NULL;
    return client_frames[client];
}
int BuildClientNativeFrame(unsigned client,uint32_t sequence,const clientSnapshot_t *source,
    const native_frame_bindings_t *bindings)
{
    native_frame_bindings_t resolved;
    engine_player_bindings_t player;
    int viewmodel;
    const ps_ext_t *appearance;
    frame_failure="frame arguments";
    if(client>=MAX_CLIENTS) return 0;
    client_frame_valid[client]=0;
    if(!source || !bindings || !bindings->player) return 0;
    resolved=*bindings; player=*bindings->player;
    frame_failure="player viewmodel binding";
    if(source->ps.viewmodelIndex<0 ||
       (viewmodel=ServerModelToWire((unsigned)source->ps.viewmodelIndex))<0) return 0;
    player.viewmodel_index=(unsigned)viewmodel;
    appearance=PlayerStateExtConst(&source->ps);
    frame_failure="player viewhands binding";
    if(!appearance || (viewmodel=ServerModelToWire(appearance->viewhands_model))<0) return 0;
    player.viewhands_index=(unsigned)viewmodel;
    /* Ammo records never cost the client its snapshot: without a binding set
     * (the weapon mapping is unresolved, say) publish no ammo records this
     * frame, which the client reads as unchanged magazines and stock, and
     * keep the movement, view and inventory flowing.
     * BuildServerAmmoBindings has already warned, once per streak. */
    if(!BuildServerAmmoBindings(&player,&source->ps))
        player.clip_mask=player.reserve_mask=0;
    frame_failure="player event parameters";
    {
        unsigned visible,i;
        if(!ResolveEnginePlayerEventParametersVisible(&source->ps,
                player.event_parameters,&visible)) return 0;
        player.event_suppressed_slots=visible^15u;
        for(i=0;i<4;++i) if(!(visible&(1u<<i))) {
            static uint32_t reported[32];
            static unsigned reported_count;
            uint32_t event=(uint32_t)source->ps.events[i];
            unsigned k;
            for(k=0;k<reported_count;++k) if(reported[k]==event) break;
            if(k==reported_count && reported_count<32) {
                reported[reported_count++]=event;
                Com_DPrintf(CON_CHANNEL_SERVER,
                    "CoDOL player %d: unmapped primary event %u parameter %u dropped from the wire%s",
                    source->ps.clientNum,event,(uint32_t)source->ps.eventParms[i],"\n");
            }
        }
    }
    resolved.player=&player;
    if(!BuildNativeFrame(&client_frames[client],client,sequence,source,&resolved)) return 0;
    client_frame_valid[client]=1; frame_failure=NULL;
    /* Developer audit: which wire fields ever carry a non-zero value. The
     * complement, dumped every 30 s, is the set of fields the client reads
     * that no server path has published yet. */
    if(com_developer && com_developer->integer) {
        static unsigned char field_audit[PLAYER_FIELDS]; static int last_dump; unsigned i;
        const player_fields_t *pf=client_frames[client]->state.player;
        static unsigned char reported_view_bundle[MAX_CLIENTS];
        uint32_t arms=0,hands=0,viewheight=0,link_weapon=0,weapon=0;
        for(i=0;i<PLAYER_FIELDS;++i) if(pf->field[i]) field_audit[i]=1;
        for(i=0;i<PLAYER_FIELDS;++i) switch(OL_PlayerField(i)->offset) {
        case 0x144: arms=pf->field[i]; break;
        case 0x148: hands=pf->field[i]; break;
        case 0x15c: viewheight=pf->field[i]; break;
        case 0x1ec: link_weapon=pf->field[i]; break;
        case 0x33c: weapon=pf->field[i]; break;
        }
        /* One post-spawn bundle proves the values that reached the native C1
         * frame, after model-bank mapping and every later player overlay. The
         * prior field-audit line normally fires before character selection and
         * therefore could not diagnose absent hands or the floor camera. */
        if(source->ps.pm_type!=0) reported_view_bundle[client]=0;
        if(!reported_view_bundle[client] && source->ps.pm_type==0 && viewheight) {
            float height;
            memcpy(&height,&viewheight,sizeof(height));
            reported_view_bundle[client]=1;
            ((void)0);
        }
        if(svs.time-last_dump>30000) {
            char line[2048]; size_t used=0; unsigned count=0;
            last_dump=svs.time; line[0]=0;
            for(i=0;i<PLAYER_FIELDS && used<sizeof(line)-8;++i) if(!field_audit[i]) {
                used+=(size_t)snprintf(line+used,sizeof(line)-used,"%s%x",count ? "," : "",OL_PlayerField(i)->offset); ++count;
            }
            ((void)0);
        }
    }
    return 1;
}
void FreeNativeFrame(native_frame_t *frame)
{ if(frame) { free(frame->commands); free(frame); } }
const snapshot_state_t *NativeFrameState(const native_frame_t *frame)
{ return frame ? &frame->state : NULL; }
const player_array_updates_t *NativeFrameUpdates(const native_frame_t *frame)
{ return frame ? &frame->updates : NULL; }
const animcmd_t *NativeFrameCommands(const native_frame_t *frame,size_t *count)
{
    if(!frame || !count) return NULL;
    *count=frame->command_count; return frame->commands;
}
int BuildNativeFrame(native_frame_t **out,unsigned client,uint32_t sequence,
    const clientSnapshot_t *source,const native_frame_bindings_t *bindings)
{
    native_frame_t *next=NULL;
    size_t entities_emitted=0;
    size_t i;
    frame_failure="frame arguments";
    if(!out || !source || !bindings || !bindings->player_seed || !bindings->array_seed || !bindings->player ||
       client>=MAX_CLIENTS ||
       source->num_entities<0 || source->num_entities>ENTITYNUM_WORLD || source->num_clients<0 || source->num_clients>32 ||
       bindings->actor_count>32 || (bindings->actor_count && !bindings->actors) ||
       bindings->command_count>SIZE_MAX/sizeof(animcmd_t) || (bindings->command_count && !bindings->commands)) return 0;
    for(i=0;i<bindings->actor_count;++i)
        if(bindings->actors[i].number>=32 || (i && bindings->actors[i].number<=bindings->actors[i-1].number)) return 0;
    frame_failure="frame allocation";
    next=calloc(1,sizeof(*next));
    if(!next) return 0;
    if(bindings->command_count) {
        next->commands=malloc(bindings->command_count*sizeof(*next->commands)); if(!next->commands) goto fail;
        memcpy(next->commands,bindings->commands,bindings->command_count*sizeof(*next->commands));
    }
    next->command_count=bindings->command_count;
    next->player=*bindings->player_seed; next->updates=*bindings->array_seed;
    frame_failure="player or HUD conversion";
    /* The player event counter comes from the serialized state itself, so no
     * recipient-side clock is staged or committed here any more. */
    if(!OverlayClientSnapshotPlayer(client,sequence,&next->player,&next->updates,&next->hud,
            &source->ps,bindings->player)) goto fail;
    frame_failure="entity conversion";
    /* An entity the resolver has no seed arm for is dropped rather than
     * failing the frame, so one unsupported entity cannot stop every client
     * receiving snapshots. The state carries the number actually written. */
    if(!CollectServerEntitiesForClient(source->first_entity,source->num_entities,next->entities,
            ENTITYNUM_WORLD,bindings->entity_resolve,bindings->entity_context,&entities_emitted,
            source->ps.clientNum)) goto fail;
    /* Actor animation commands are appended to this recipient's command list
     * from the delivery journal. The emitted entity set is the PVS membership
     * the journal filters on, so a dog nobody can see costs no commands. */
    frame_failure="actor animation delivery";
    {
        unsigned actor_commands=0;
        size_t capacity=next->command_count+ACTOR_ANIM_SNAPSHOT_LIMIT;
        animcmd_t *grown;
        if(capacity<next->command_count || capacity>SIZE_MAX/sizeof(*grown))goto fail;
        grown=realloc(next->commands,capacity*sizeof(*grown));
        if(!grown)goto fail;
        next->commands=grown;
        if(!ActorDeliveryPrepare(client,sequence,
             (unsigned)svsHeader.clients[client].messageAcknowledge,
             next->entities,(unsigned)entities_emitted,next->commands+next->command_count,
             ACTOR_ANIM_SNAPSHOT_LIMIT,&actor_commands)) {
            /* A refused animation delivery drops this frame's actor animation
             * commands, never the snapshot: one bad command or a stalled
             * journal used to deafen the recipient for the rest of the match
             * (every frame rejected as "actor animation delivery"). The
             * recipient's delivery state is cleared so the next frame starts
             * a fresh timeline with a full bootstrap instead of resending a
             * pending capture that was never published. */
            /* Print once per distinct REASON, where "reason" ignores the
             * numbers embedded in the message. MEASURED 2026-09-20: this
             * dedupe never fired, because every failure string carries the
             * sequence ("no pose capture for sequence 206 ... seq 205") and
             * so differs every frame -- one zm match logged 1,169 copies of
             * a single fault and buried everything else. Digit runs collapse
             * to '#' for the COMPARISON only; the printed line keeps its
             * real numbers. */
            static char reported[MAX_CLIENTS][256];
            const char *why=actor_delivery_failure?actor_delivery_failure:"unknown";
            char shape[256]; unsigned si=0,wi=0;
            while(why[wi] && si+1<sizeof(shape)) {
                if(why[wi]>='0' && why[wi]<='9') {
                    shape[si++]='#';
                    while(why[wi]>='0' && why[wi]<='9') ++wi;
                } else shape[si++]=why[wi++];
            }
            shape[si]=0;
            if(strncmp(reported[client],shape,sizeof(reported[client])-1)) {
                Q_strncpyz(reported[client],shape,sizeof(reported[client]));
                Com_PrintWarning(CON_CHANNEL_SERVER,
                    "CoDOL actor: animation delivery to client %u dropped for sequence %u: %s "
                    "(further faults of this shape are silenced)\n",
                    client,sequence,why);
            }
            ActorDeliveryClear(client);
            actor_commands=0;
        }
        next->command_count+=actor_commands;
        /* MEASUREMENT (docs/ZOMBIE_ANIM_SMOOTHNESS_20260921.md): the
         * actor origin stream exactly as this snapshot carries it, one line
         * per actor-bearing snapshot, bounded to the first 150 such
         * snapshots of the process (2.5 s at the 60 Hz loopback cadence).
         * `org` is the float the entity state holds, `d` its change since
         * the previous snapshot that carried this entity to any client, and
         * `wire` the whole-unit delta the integer origin codec will publish
         * (floor(x+0.5), delta.c position()). `mode` is the pos
         * trajectory type as sent (1 = TR_INTERPOLATE). `cmds` is the
         * number of animation commands appended for this recipient. */
        {
            static unsigned wire_lines;
            static float last_origin[2048][3];
            static unsigned char last_valid[2048];
            static int ix=-1,iy=-1,iz=-1,imode=-1;
            if(wire_lines<150) {
                char line[1200]; size_t used=0; unsigned n,found=0;
                if(ix<0) {
                    unsigned k;
                    for(k=1;k<ENTITY_FIELDS;++k) {
                        const entity_field_t *f=EntityField(18,k);
                        if(!f) break;
                        if(f->offset==0x018) ix=(int)k; else if(f->offset==0x01c) iy=(int)k;
                        else if(f->offset==0x020) iz=(int)k; else if(f->offset==0x00c) imode=(int)k;
                    }
                }
                line[0]=0;
                for(n=0;n<entities_emitted && ix>=0 && iy>=0 && iz>=0 && imode>=0 && used<sizeof(line)-160;++n) {
                    const entity_state_t *e=&next->entities[n];
                    float o[3],d[3]={0,0,0}; int w[3]={0,0,0}; unsigned num;
                    if(e->type!=18 || e->number>=2048) continue;
                    num=e->number;
                    memcpy(&o[0],&e->field[ix],4); memcpy(&o[1],&e->field[iy],4); memcpy(&o[2],&e->field[iz],4);
                    if(last_valid[num]) {
                        unsigned a;
                        for(a=0;a<3;++a) {
                            d[a]=o[a]-last_origin[num][a];
                            w[a]=(int)floorf(o[a]+0.5f)-(int)floorf(last_origin[num][a]+0.5f);
                        }
                    }
                    memcpy(last_origin[num],o,sizeof(o)); last_valid[num]=1;
                    used+=(size_t)snprintf(line+used,sizeof(line)-used,
                        " | ent %u mode %u org (%.2f %.2f %.2f) d (%.2f %.2f %.2f) wire (%d %d %d)",
                        num,(unsigned)e->field[imode],o[0],o[1],o[2],d[0],d[1],d[2],w[0],w[1],w[2]);
                    ++found;
                }
                if(found) {
                    ++wire_lines;
                    ((void)0);
                }
            }
        }
    }
    frame_failure="client conversion";
    if(!CollectServerClientsResolved(source->first_client,source->num_clients,next->clients,32,
            bindings->client_resolve,bindings->client_context)) goto fail;
    if(bindings->actor_count) memcpy(next->actors,bindings->actors,bindings->actor_count*sizeof(*next->actors));
    next->state=(snapshot_state_t){sequence,&next->player,&next->hud,next->entities,entities_emitted,
        next->clients,(size_t)source->num_clients,next->actors,bindings->actor_count};
    FreeNativeFrame(*out); *out=next;
    frame_failure=NULL; return 1;
fail:
    FreeNativeFrame(next); return 0;
}
#endif

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
int PersistentNativeFramesEmpty(void) {return resident_storage_zero(client_frames,sizeof(client_frames)) && resident_storage_zero(client_frame_valid,sizeof(client_frame_valid));}

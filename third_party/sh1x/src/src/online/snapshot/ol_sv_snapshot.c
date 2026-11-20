#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Engine message bridge for the native CoDOL snapshot serializer. */
#include "sv_state.h"
#include "actor_delivery.h"
#include "late_corpse.h"
#include "glass_world.h"
#include "frame_builder.h"
#include <stdio.h>
#include "sv_frame.h"
#include "frame_builder.h"
#include "matchdata.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "sys_cod4defs.h"
#include "qcommon_io.h"
#include <ctype.h>
#include <errno.h>
int ApplyServerPositionBase(delta_context_t *context)
{
    const char *value=GetConfigstring(ServerConfigstrings(),CS_MAP_CENTER);
    float center[3]={0}; unsigned i;
    if(!context) return 0;
    if(!value) value=GetConfigstring(ServerInheritedConfigstrings(),CS_MAP_CENTER);
    if(value && *value) {
        for(i=0;i<3;++i) {
            char *end;
            while(isspace((unsigned char)*value)) ++value;
            errno=0; center[i]=strtof(value,&end);
            if(end==value || errno==ERANGE || (*end && !isspace((unsigned char)*end))) return 0;
            value=end;
        }
        while(isspace((unsigned char)*value)) ++value;
        if(*value) return 0;
    }
    return SetDeltaPositionBase(context,center);
}
static history_t *client_histories[MAX_CLIENTS];
static struct {
    const msg_t *message;
    const void *data;
    unsigned char *serialized;
    int size;
    uint32_t sequence;
    uint32_t match_time;
    int match_sent;
} client_pending[MAX_CLIENTS];

static void clear_pending(unsigned n)
{
    free(client_pending[n].serialized);
    memset(&client_pending[n], 0, sizeof(client_pending[n]));
}

void DiscardClientSnapshot(unsigned client_number)
{
    if (client_number >= MAX_CLIENTS) return;
    if (client_pending[client_number].message)
        FinishHistory(client_histories[client_number], client_pending[client_number].sequence, 0);
    clear_pending(client_number);
}
int ClientSnapshotPrepared(unsigned n, const msg_t *msg, uint32_t sequence)
{
    return n < MAX_CLIENTS && msg && msg->data && !msg->overflowed && !msg->splitData &&
        msg->cursize >= 0 && msg->maxsize >= msg->cursize && client_pending[n].serialized &&
        client_pending[n].message == msg && client_pending[n].data == msg->data &&
        client_pending[n].size == msg->cursize && client_pending[n].sequence == sequence &&
        !memcmp(client_pending[n].serialized, msg->data, (size_t)msg->cursize);
}
int ClientSnapshotPending(unsigned n)
{ return n<MAX_CLIENTS && client_pending[n].message!=NULL; }
int FinishClientSnapshot(unsigned n, const msg_t *msg, uint32_t sequence, int accepted)
{
    int result;
    if (accepted != 1 || !ClientSnapshotPrepared(n, msg, sequence)) {
        DiscardClientSnapshot(n); return 0;
    }
    result = FinishHistory(client_histories[n], sequence, 1);
    /* Only a fully serialized snapshot advances the animation journal's
     * delivered range; building a pending one must not. */
    if(result) ActorAnimPublished(n,sequence);
    if(result && client_pending[n].match_sent)
        ClientMatchDataSent(n,client_pending[n].match_time);
    clear_pending(n);
    return result;
}
const char *snapshot_failure;
int PrepareClientSnapshot(unsigned n, msg_t *msg,
    const snapshot_state_t *state, uint32_t acknowledged,
    const player_array_updates_t *updates, const animcmd_t *commands,
    size_t command_count, unsigned flags, int rov, delta_context_t *context)
{
    history_t *history;
    delta_context_t next;
    unsigned char *serialized = NULL;
    if(!context) { if(msg) msg->overflowed=qtrue; return 0; }
    next=*context;
    /* Reserve the copy before serialization so allocation failure cannot leave
     * a newly prepared frame or partially advance the caller's message. */
    if ((snapshot_failure="client/pending/message state",
         n >= MAX_CLIENTS || client_pending[n].message || !msg || !msg->data ||
         msg->overflowed || msg->splitData || msg->cursize < 0 ||
         msg->maxsize <= 0 || msg->maxsize > INT_MAX/8 || msg->maxsize < msg->cursize) ||
        (snapshot_failure="position base", !ApplyServerPositionBase(&next)) ||
        (snapshot_failure="serialized copy allocation", !(serialized = malloc((size_t)msg->maxsize))) ||
        (snapshot_failure="history allocation", !(history = ClientSnapshotHistory(n))) ||
        (snapshot_failure="prepare server snapshot",
         !PrepareServerSnapshot(msg, history, state, acknowledged, updates,
                                    commands, command_count, flags, rov, &next))) {
        free(serialized);
        if (msg) msg->overflowed = qtrue;
        return 0;
    }
    if(!(flags&2) && ClientMatchDataDue(n,(uint32_t)context->server_time)) {
        client_matchdata_t *match=ClientMatchData();ol_msg_t wire;
        MsgWriteInit(&wire,msg->data,(size_t)msg->maxsize-1); /* reserve EOF */
        wire.size=msg->cursize>0 ? (size_t)msg->cursize-1 : 0;
        if(!match || msg->cursize<=0 || msg->data[msg->cursize-1]!=5 ||
           !WriteClientMatchDataSection(&wire,match->name,match->record,sizeof(match->record))) {
            snapshot_failure="match data section";
            FinishHistory(history,state->sequence,0);free(serialized);
            msg->overflowed=qtrue;return 0;
        }
        wire.capacity++;WriteByte(&wire,5);msg->cursize=(int)wire.size;
        client_pending[n].match_sent=1;client_pending[n].match_time=(uint32_t)context->server_time;
    }
    client_pending[n].message = msg; client_pending[n].data = msg->data;
    memcpy(serialized, msg->data, (size_t)msg->cursize);
    client_pending[n].serialized = serialized;
    client_pending[n].size = msg->cursize; client_pending[n].sequence = state->sequence;
    *context=next;
    return 1;
}

history_t *ClientSnapshotHistory(unsigned client_number)
{
    if (client_number >= MAX_CLIENTS) return NULL;
    if (!client_histories[client_number]) client_histories[client_number] = CreateHistory();
    return client_histories[client_number];
}
/* The frame the next snapshot is delta-coded against, or NULL for a flag-8
 * (SNAPFLAG_BASELINE) frame.
 *
 * C1 keeps exactly ONE copy of a flag-8 frame's entities: the array at
 * 0x06d36abc, count 0x06d36ab8. Parsing a flag-8 frame zeroes that count
 * (0x49d969) and writes its entities there instead of the parse ring
 * (0x49ca0d); every frame delta-coded against ANY flag-8 frame reads its old
 * entities from that array (0x49c9c0, 0x49cc62), with the base frame's own
 * entity count; and CL_GetSnapshot copies a flag-8 snapshot's entities from
 * it too (0x487083). So C1 can decode only against the NEWEST flag-8 frame it
 * parsed, while our history kept every flag-8 frame as a valid base.
 *
 * Until an ACK comes back the server used to answer every frame with another
 * flag-8 frame, so after any gap (the ~2 s D3D device reset of an alt-tab, a
 * stall, a no-delta request, a recovery packet) several flag-8 frames were in
 * flight. The client ACKed one of them, we delta-coded against it, and C1
 * decoded against a later one: every unchanged field (copied from the old
 * entity, 0x5a0d52) and every XORed field came from the wrong entity, offset
 * by any difference in the two entity lists. That fabricates entity states:
 * an ET_FX whose effect byte is 0 (or an unregistered slot) dereferences a
 * NULL FxEffectDef at 0x4fe0ad (CG ET_FX 0x442510 -> 0x4423d0 -> FX spawn
 * 0x4fe430 -> 0x4fe0a0: EXE_CRASH 62c530,4fe0ad,4fe4c9,4424f7,442542), and
 * a player entity whose legs/torso animation decodes past the tree drops the
 * client with "79 534 998" (BG animation index check 0x406375, 998 >= 534
 * animations); in between, other players' models play the wrong animations.
 *
 * Two rules keep the client's single copy and our base the same frame:
 *  1. A flag-8 frame is a base only while it is the newest flag-8 frame we
 *     transmitted. An ACK of an older one is not a base.
 *  2. While that newest flag-8 frame L is still in flight - the client has
 *     not reported receiving anything at or after L, and L is inside the
 *     delta window - delta-code against L instead of sending another flag-8
 *     frame. If L arrived, the client holds exactly L. If L was lost, C1
 *     rejects the frame as "Delta frame too old" (messageNum check 0x49da86),
 *     its next packet is a no-delta request reporting a later message, and
 *     only then is a new flag-8 frame sent. One flag-8 frame is in flight at
 *     a time, and the next one goes out at least 32 frames after it unless
 *     the client has already moved past it. */
static const snapshot_state_t *select_snapshot_base(history_t *history,
    uint32_t acknowledged, uint32_t next_sequence, unsigned flags)
{
    const snapshot_state_t *base, *baseline;
    if ((flags & 8u) || !history || !HistoryLatestSequence(history)) return NULL;
    baseline = HistoryBaseline(history);
    base = FindHistoryBase(history, acknowledged, next_sequence);
    if (base && HistoryIsBaselineFrame(history, base->sequence) &&
        (!baseline || baseline->sequence != base->sequence)) base = NULL;
    if (!base && baseline && HistoryReceived(history) < baseline->sequence &&
        HistoryIsBaselineFrame(history, baseline->sequence))
        base = FindHistoryBase(history, baseline->sequence, next_sequence);
    return base;
}
void NoteClientSnapshotReceived(unsigned n, uint32_t received)
{
    history_t *history;
    if (n < MAX_CLIENTS && (history = ClientSnapshotHistory(n)))
        HistoryNoteReceived(history, received);
}
int ClientSnapshotHasBase(unsigned n, uint32_t acknowledged,
                                uint32_t next_sequence, unsigned flags)
{
    return n < MAX_CLIENTS &&
           select_snapshot_base(client_histories[n], acknowledged, next_sequence, flags) != NULL;
}
void ResetClientSnapshotHistory(unsigned client_number)
{
    if (client_number >= MAX_CLIENTS) return;
    AcknowledgeClientMatchData(client_number);
    ClearClientSnapshotHud(client_number);
    ActorDeliveryClear(client_number);
    LateCorpseClear(client_number);
    ClearClientNativeFrame(client_number);
    DiscardClientSnapshot(client_number);
    DestroyHistory(client_histories[client_number]);
    client_histories[client_number] = NULL;
}
void ResetSnapshotHistories(void)
{
    unsigned i;
    for (i = 0; i < MAX_CLIENTS; ++i) ResetClientSnapshotHistory(i);
}
int WriteServerSnapshot(msg_t *msg, const snapshot_state_t *state,
                              const snapshot_state_t *base,
                              const player_array_updates_t *updates,
                              const animcmd_t *commands, size_t command_count,
                              unsigned flags, int rov, delta_context_t *context)
{
    ol_msg_t wire;
    delta_context_t next;
    snapshot_state_t captured;
    glass_wire_piece_t glass[GLASS_MAX_PIECES];
    unsigned glass_count=0;
    if (!state) goto fail;
    captured=*state;
    if (!GlassWorldEnsureMap()) {
        snapshot_failure="glass map state unavailable"; goto fail;
    }
    if (!GlassWorldCapture(glass,GLASS_MAX_PIECES,&glass_count)) {
        snapshot_failure="glass capture failed"; goto fail;
    }
    captured.glass=glass;captured.glass_count=glass_count;
    state=&captured;
    if (!msg || !msg->data || msg->overflowed || msg->splitData || msg->cursize < 0 ||
        msg->maxsize < msg->cursize || msg->maxsize > INT_MAX/8 || !context) goto fail;
    next = *context;
        MsgWriteInit(&wire, msg->data+msg->cursize, msg->maxsize-msg->cursize);
    if (!OL_WriteSnapshot(&wire, state, base, updates, commands, command_count, flags, rov, &next)) {
        static char why[200];
        snprintf(why,sizeof(why),"OL_WriteSnapshot: %s (written %u of %u bytes room, cursize %d, base %s, entities %u, clients %u)",
            WriteSnapshotFailure() ? WriteSnapshotFailure() : "?",(unsigned)wire.size,
            (unsigned)(msg->maxsize-msg->cursize),msg->cursize,base ? "delta" : "full",
            (unsigned)state->entity_count,(unsigned)state->client_count);
        snapshot_failure=why; goto fail;
    }
    msg->cursize += wire.size;
    msg->bit = msg->cursize*8;
    *context = next;
    return 1;
fail:
    if (msg) msg->overflowed = qtrue;
    return 0;
}

static int write_server_snapshot_delta(msg_t *msg, const snapshot_state_t *state,
                              const snapshot_state_t *base, uint32_t delta_number,
                              const player_array_updates_t *updates,
                              const animcmd_t *commands, size_t command_count,
                              unsigned flags, int rov, delta_context_t *context)
{
    ol_msg_t wire;
    delta_context_t next;
    snapshot_state_t captured;
    glass_wire_piece_t glass[GLASS_MAX_PIECES];
    unsigned glass_count=0;
    if (!state) goto fail;
    captured=*state;
    if (!GlassWorldEnsureMap()) {
        snapshot_failure="glass map state unavailable"; goto fail;
    }
    if (!GlassWorldCapture(glass,GLASS_MAX_PIECES,&glass_count)) {
        snapshot_failure="glass capture failed"; goto fail;
    }
    captured.glass=glass;captured.glass_count=glass_count;
    state=&captured;
    if (!msg || !msg->data || msg->overflowed || msg->splitData || msg->cursize < 0 ||
        msg->maxsize < msg->cursize || msg->maxsize > INT_MAX/8 || !context) goto fail;
    next = *context;
    MsgWriteInit(&wire, msg->data+msg->cursize, msg->maxsize-msg->cursize);
    if (!WriteSnapshotDeltaNumber(&wire, state, base, delta_number, updates,
            commands, command_count, flags, rov, &next)) {
        snapshot_failure="WriteSnapshotDeltaNumber"; goto fail;
    }
    msg->cursize += wire.size; msg->bit = msg->cursize*8; *context = next;
    return 1;
fail:
    if (msg) msg->overflowed=qtrue;
    return 0;
}

/* Snapshot preparation no longer refuses a frame for an entity C1 cannot be
 * sent (left out) or a position outside the codec window (held at its edge).
 * Report each new kind at once and then at most every 600 further cases, so a
 * fallen corpse that lives 20 s logs a line or two, not one per frame. */
static void report_degraded(const char *what, unsigned total, const char *note,
                            unsigned *reported, char shape[160])
{
    char now[160]; unsigned i=0,o=0;
    if(total==*reported || !note) return;
    while(note[i] && o+1<sizeof(now)) {
        if(note[i]>='0'&&note[i]<='9') { now[o++]='#'; while(note[i]>='0'&&note[i]<='9') ++i; }
        else now[o++]=note[i++];
    }
    now[o]=0;
    if(!strcmp(now,shape) && total-*reported<600) return;
    Com_PrintWarning(CON_CHANNEL_SERVER,"CoDOL snapshot: %s (%u so far; latest %s)\n",what,total,note);
    *reported=total; snprintf(shape,160,"%s",now);
}
static void report_degraded_frames(void)
{
    static unsigned omitted_reported, clamp_reported;
    static char omitted_shape[160], clamp_shape[160];
    report_degraded("entity left out of a client frame, not encodable for C1",
        HistoryOmittedEntities(),HistoryOmittedNote(),&omitted_reported,omitted_shape);
    report_degraded("position outside C1's codec window held at the window edge",
        DeltaReachClamps(),DeltaReachClampNote(),&clamp_reported,clamp_shape);
}

int PrepareServerSnapshot(msg_t *msg, history_t *history,
                                const snapshot_state_t *state, uint32_t acknowledged,
                                const player_array_updates_t *updates,
                                const animcmd_t *commands, size_t command_count,
                                unsigned flags, int rov, delta_context_t *context)
{
    const snapshot_state_t *base, *ack_base, *baseline;
    uint32_t delta_number = 0;
    int baseline_frame;
    if (!state || !history || !context || flags > 255) goto fail;
    /* Never a flag-8 frame C1 no longer holds; see select_snapshot_base. */
    ack_base = select_snapshot_base(history, acknowledged, state->sequence, flags);
    baseline = HistoryBaseline(history);
    baseline_frame = (flags & 8u) != 0 ||
        (HistoryLatestSequence(history) == 0) || !ack_base;
    /* 2026-09-15: a client that requests a non-delta snapshot mid-match
     * (deltaMessage -1 after an overflow-recovery message) must receive a
     * flag-8 frame. C1 0x49dd7e refreshes its saved baseline only on flag
     * 8, and a deltaNum-0/flag-0 frame decoded against that stale copy
     * produced team 7 and the 0x00aea51c strlen crash (DMP9B39, DMPF1E1). */
    if (baseline_frame) {
        base = NULL; flags |= 8u; rov = 1;
    } else if (ack_base) {
        base = ack_base; flags &= ~8u;
        delta_number = state->sequence - ack_base->sequence;
    } else {
        /* C1 0x49d9ce selects its persistent noDeltaSnapshot here. Encode
         * every section against our matching pinned copy while leaving the
         * wire deltaNum zero. The leading player-base bit remains one. */
        base = baseline; flags &= ~8u;
    }
    if (!(baseline_frame ?
          PrepareBaselineHistory(history, state, rov, context) :
          PrepareHistory(history, state, base->sequence, rov, context))) {
        static char why[240];
        snprintf(why,sizeof(why),"prepare history: %s",HistoryFailure());
        snapshot_failure=why; goto fail;
    }
    report_degraded_frames();
    /* Serialize the reached state, not the raw one. The skip tests compare
     * against the held (already quantized) base, so a raw position or angle
     * never equals it and ~81% of entity-frames were resent every frame even
     * while static. The codecs produce the same wire value for a quantized
     * input. Measured with 12 bots: average delta 910 B -> 530 B. */
    if (HistoryPendingState(history)) state = HistoryPendingState(history);
    if (write_server_snapshot_delta(msg, state, base, delta_number, updates,
            commands, command_count, flags, rov, context)) return 1;
    FinishHistory(history, state->sequence, 0);
fail:
    if (msg) msg->overflowed = qtrue;
    return 0;
}

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
int PersistentSnapshotHistoriesEmpty(void) {return resident_storage_zero(client_histories,sizeof(client_histories)) && resident_storage_zero(client_pending,sizeof(client_pending));}

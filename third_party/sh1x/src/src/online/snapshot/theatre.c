/* Original local theatre capture transport, 2026-10-17. AGPL-3.0-or-later.
 * Stores native C1 gamestate/snapshot messages without IW3 demo compression.
 * C1CAP001 is an intermediate capture format, NOT a retail .demo file.
 */
#include "theatre.h"
#include "async_capture.h"
#include "sv_state.h"
#include "qcommon_io.h"
#include "sys_main.h"
#include "cvar.h"
#include "filesystem.h"
#include "network.h"
#include "connect.h"
#include "ol_profile.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>


typedef struct {
    async_capture_t writer;
    history_t *history;
    uint32_t next, previous, frames, started, bytes;
    int automatic, bot;
    uint64_t owner_uid;
    char capture_id[33], match_id[33], map[65], zone_type[5];
    char path[MAX_OSPATH];
} theatre_capture_t;
static theatre_capture_t captures[MAX_CLIENTS];
/* Quotas are per slot and map. Files are retained for an external archive
 * consumer; this policy never deletes completed captures. A failed slot stays
 * disabled until the next map or an explicit auto-record disable/re-enable. */
static struct { unsigned segments; int blocked; } auto_slots[MAX_CLIENTS];
static cvar_t *auto_record, *auto_seconds, *auto_segments, *auto_megabytes;
static int auto_was_enabled;
static uint32_t auto_serial;
#define THEATRE_PARTICIPANTS 256
static char match_id[33];
static uint64_t match_participants[THEATRE_PARTICIPANTS];
static unsigned match_participant_count;
static int match_participants_overflow;
static struct { uint64_t uid; int entered; uint32_t checked; int attempted; } identities[MAX_CLIENTS];
static byte capture_buffer[0x20000]; /* single server thread, never live msg data */

/* Never use the claimed connect xuid alone: optional admission mode also
 * accepts unauthenticated diagnostics. Require a matching attached account and
 * the DTLS admission (or the admitted party seat) before granting membership. */
static uint64_t authenticated_uid(unsigned slot)
{
    uint64_t uid, attached=0;
    admission_entry_t admission, member;
    client_t *client=&svs.clients[slot];
    if(client->state!=CS_ACTIVE || client->netchan.remoteAddress.type==NA_BOT) return 0;
    uid=ClientConnectIdentity(slot);
    if(!uid || !ClientSecureProfileAttached(slot,&attached) || attached!=uid) return 0;
    if(identities[slot].uid==uid && identities[slot].entered==client->enteredWorldTime) return uid;
    if(identities[slot].attempted && identities[slot].entered==client->enteredWorldTime &&
       (uint32_t)((uint32_t)svs.time-identities[slot].checked)<1000u) return 0;
    identities[slot].uid=0;identities[slot].entered=client->enteredWorldTime;
    identities[slot].checked=(uint32_t)svs.time;identities[slot].attempted=1;
    if(!NetworkAdmission(&client->netchan.remoteAddress,&admission)) return 0;
    if(admission.uid!=uid && (!admission.uid ||
       !AdmissionMember(client->netchan.remoteAddress.ip,admission.uid,uid,&member))) return 0;
    identities[slot].uid=uid;
    return uid;
}
static int random_id(char out[33])
{
    static const char hex[]="0123456789abcdef";
    byte bytes[16];unsigned i;
    if(!Sys_RandomBytes(bytes,sizeof(bytes))) return 0;
    for(i=0;i<sizeof(bytes);++i) {out[2*i]=hex[bytes[i]>>4];out[2*i+1]=hex[bytes[i]&15];}
    out[32]=0;return 1;
}
static void remember_participants(void)
{
    unsigned slot,i;
    for(slot=0;slot<(unsigned)sv_maxclients->integer && slot<MAX_CLIENTS;++slot) {
        uint64_t uid=authenticated_uid(slot);
        if(!uid) continue;
        for(i=0;i<match_participant_count && match_participants[i]!=uid;++i);
        if(i<match_participant_count) continue;
        if(match_participant_count==THEATRE_PARTICIPANTS) {match_participants_overflow=1;continue;}
        match_participants[match_participant_count++]=uid;
    }
}
static int identity_token(const char *in,char *out,size_t capacity)
{
    size_t i;
    if(!in || !*in || strlen(in)>=capacity) return 0;
    for(i=0;in[i];++i)
        if(!isalnum((unsigned char)in[i]) && in[i]!='_' && in[i]!='-') return 0;
    strcpy(out,in);return 1;
}
static int normal_stop(const char *reason)
{
    return !strcmp(reason,"stopped") || !strcmp(reason,"segment_complete") ||
        !strcmp(reason,"auto_disabled") || !strcmp(reason,"map_reset") ||
        !strcmp(reason,"client_dropped") || !strcmp(reason,"client_replaced") ||
        !strcmp(reason,"client_enter_world");
}
/* Publish JSON only after the capture stream is closed and flushed. The final
 * rename stays within fs_homepath, so readers never observe partial metadata.
 * No credential, IP address, or mutable player name is persisted. */
static int make_metadata(unsigned slot,const theatre_capture_t *capture,const char *reason,
                         char *text,size_t capacity,size_t *used)
{
    char safe_reason[65];
    size_t length;int n;unsigned i;
    if(!capture->capture_id[0] || !capture->match_id[0] ||
       !identity_token(reason,safe_reason,sizeof(safe_reason))) return 0;
    n=snprintf(text,capacity,
        "{\"schema\":1,\"capture_id\":\"%s\",\"match_id\":\"%s\","
        "\"slot\":%u,\"bot\":%s,\"owner_uid\":",capture->capture_id,capture->match_id,
        slot,capture->bot ? "true":"false");
    if(n<0 || (size_t)n>=capacity) return 0;
    length=(size_t)n;
    n=capture->owner_uid ? snprintf(text+length,capacity-length,"\"%llu\"",(unsigned long long)capture->owner_uid) :
        snprintf(text+length,capacity-length,"null");
    if(n<0 || (size_t)n>=capacity-length) return 0;
    length+=(size_t)n;
    n=snprintf(text+length,capacity-length,
        ",\"zone_type\":\"%s\",\"map\":\"%s\",\"started_ms\":%u,\"ended_ms\":%u,"
        "\"frames\":%u,\"bytes\":%u,\"reason\":\"%s\",\"completed\":%s,"
        "\"membership_complete\":%s,\"participant_uids\":[",capture->zone_type,capture->map,
        capture->started,(uint32_t)svs.time,capture->frames,capture->bytes,reason,
        normal_stop(reason) && capture->frames ? "true":"false",
        match_participants_overflow ? "false":"true");
    if(n<0 || (size_t)n>=capacity-length) return 0;
    length+=(size_t)n;
    /* Bot viewpoints are private diagnostics, even in a human match. */
    for(i=0;!capture->bot && i<match_participant_count;++i) {
        n=snprintf(text+length,capacity-length,"%s\"%llu\"",i ? ",":"",
            (unsigned long long)match_participants[i]);
        if(n<0 || (size_t)n>=capacity-length) return 0;
        length+=(size_t)n;
    }
    if(length+4>capacity) return 0;
    memcpy(text+length,"]}\n",3);length+=3;
    *used=length;return 1;
}
static void put32(byte *out,uint32_t value)
{
    out[0]=value;out[1]=value>>8;out[2]=value>>16;out[3]=value>>24;
}
static int record(theatre_capture_t *capture,unsigned type,uint32_t sequence,
                  const void *payload,unsigned length)
{
    byte header[16];
    put32(header,type);put32(header+4,sequence);
    put32(header+8,(uint32_t)svs.time);put32(header+12,length);
    if(!AsyncCaptureAppend(capture->writer,header,sizeof(header),payload,length))return 0;
    capture->bytes+=sizeof(header)+length;return 1;
}
int TheatreActive(unsigned slot)
{
    return slot<MAX_CLIENTS && captures[slot].history!=NULL;
}
void TheatreStop(unsigned slot,const char *reason)
{
    theatre_capture_t *capture;
    byte last[AC_TAIL_BYTES],failed[AC_TAIL_BYTES];
    char metadata[AC_METADATA_BYTES],failed_metadata[AC_METADATA_BYTES];
    size_t size=0,failed_size=0,n;
    static const char failure[]="async_capture_failed";
    if(slot>=MAX_CLIENTS)return;
    if(reason && (!strcmp(reason,"client_dropped") || !strcmp(reason,"client_replaced") ||
       !strcmp(reason,"client_enter_world")))memset(&identities[slot],0,sizeof(identities[slot]));
    if(!TheatreActive(slot))return;
    capture=&captures[slot];remember_participants();
    if(!reason)reason="stopped";
    if(AsyncCaptureError(capture->writer))reason=failure;
    if(!normal_stop(reason))auto_slots[slot].blocked=1;
    n=strlen(reason);if(n>sizeof(last)-16){reason=failure;n=strlen(reason);}
    put32(last,0);put32(last+4,capture->next);put32(last+8,(uint32_t)svs.time);put32(last+12,(uint32_t)n);
    memcpy(last+16,reason,n);
    put32(failed,0);put32(failed+4,capture->next);put32(failed+8,(uint32_t)svs.time);put32(failed+12,sizeof(failure)-1);
    memcpy(failed+16,failure,sizeof(failure)-1);
    capture->bytes+=(uint32_t)(16+n);
    if(!make_metadata(slot,capture,reason,metadata,sizeof(metadata),&size) ||
       !make_metadata(slot,capture,failure,failed_metadata,sizeof(failed_metadata),&failed_size)) {
        size=failed_size=0;
        Com_PrintError(CON_CHANNEL_SERVER,"THEATRE metadata could not be frozen slot=%u file=%s\n",slot,capture->path);
    }
    if(!AsyncCaptureClose(capture->writer,last,16+n,failed,16+sizeof(failure)-1,
                              metadata,size,failed_metadata,failed_size))
        Com_PrintError(CON_CHANNEL_SERVER,"THEATRE close admission failed slot=%u file=%s\n",slot,capture->path);
    Com_Printf(CON_CHANNEL_SERVER,"THEATRE CLOSING slot=%u frames=%u file=%s reason=%s (async)\n",
               slot,capture->frames,capture->path,reason);
    DestroyHistory(capture->history);memset(capture,0,sizeof(*capture));
}
int TheatreStart(unsigned slot,const char *name)
{
    theatre_capture_t *capture;
    client_t *client;
    msg_t message;
    unsigned i;
    char absolute[MAX_OSPATH];
    byte header[16];
    if(!sv_maxclients || !fs_homepath || !fs_homepath->string || !*fs_homepath->string ||
       slot>=MAX_CLIENTS || slot>=(unsigned)sv_maxclients->integer ||
       TheatreActive(slot) || !name || !*name || strlen(name)>40) return 0;
    for(i=0;name[i];++i)
        if(!isalnum((unsigned char)name[i]) && name[i]!='_' && name[i]!='-') return 0;
    client=&svs.clients[slot];
    if(client->state!=CS_ACTIVE) return 0;
    capture=&captures[slot];
    memset(capture,0,sizeof(*capture));
    if((!match_id[0] && !random_id(match_id)) || !random_id(capture->capture_id) ||
       !identity_token(ServerMapName(),capture->map,sizeof(capture->map)) ||
       !identity_token(ServerZoneType(),capture->zone_type,sizeof(capture->zone_type))) return 0;
    strcpy(capture->match_id,match_id);
    capture->bot=client->netchan.remoteAddress.type==NA_BOT;
    capture->owner_uid=authenticated_uid(slot);
    remember_participants();
    snprintf(capture->path,sizeof(capture->path),"demos/%s_%s.c1cap",name,capture->capture_id);
    if(!(capture->history=CreateHistory()))return 0;
    FS_BuildOSPathForThread(fs_homepath->string,capture->path,"",absolute,0);
    if(!*absolute || (absolute[strlen(absolute)-1]!='/' && absolute[strlen(absolute)-1]!='\\')) {
        DestroyHistory(capture->history);memset(capture,0,sizeof(*capture));return 0;
    }
    absolute[strlen(absolute)-1]=0;
    capture->writer=AsyncCaptureBegin(absolute,slot);
    if(!capture->writer) {
        DestroyHistory(capture->history);memset(capture,0,sizeof(*capture));return 0;
    }
    capture->next=1;
    capture->started=(uint32_t)svs.time;
    capture->bytes=sizeof(header);
    memcpy(header,"C1CAP001",8);put32(header+8,slot);put32(header+12,(uint32_t)svs.time);
    MSG_Init(&message,capture_buffer,sizeof(capture_buffer));
    MSG_WriteLong(&message,client->lastClientCommand);
    /* This native function includes C1's metadata and EOF. No IW3 tail or
     * Huffman wrapper is appended, unlike inherited SV_RecordClient. */
    SV_WriteTheatreGameState(&message,client);
    if(message.overflowed ||
       !AsyncCaptureAppend(capture->writer,header,sizeof(header),NULL,0) ||
       !record(capture,1,capture->next++,message.data,(unsigned)message.cursize)) {
        TheatreStop(slot,"bootstrap_failed");return 0;
    }
    Com_Printf(CON_CHANNEL_SERVER,"THEATRE QUEUED slot=%u file=%s format=C1CAP001 (async open)\n",slot,capture->path);
    return 1;
}
void TheatrePump(void)
{
    async_capture_result_t result;
    while(AsyncCapturePoll(&result)) {
        if(result.error) {
            if(result.tag<MAX_CLIENTS && AsyncCaptureResultMatches(&result,result.tag,captures[result.tag].writer)) {
                auto_slots[result.tag].blocked=1;
                if(captures[result.tag].automatic)TheatreStop(result.tag,"async_capture_failed");
            }
            Com_PrintError(CON_CHANNEL_SERVER,"THEATRE async capture failed slot=%u file=%s error=%d; no completed truncated film admitted\n",
                           result.tag,result.path,result.error);
        }else Com_Printf(CON_CHANNEL_SERVER,"THEATRE CLOSED slot=%u file=%s (flushed and closed)\n",result.tag,result.path);
    }
}
int TheatreShutdown(unsigned timeout_ms)
{
    unsigned slot;int done;
    for(slot=0;slot<MAX_CLIENTS;++slot)TheatreStop(slot,"server_shutdown");
    done=AsyncCaptureShutdown(timeout_ms);TheatrePump();
    if(!done)Com_PrintError(CON_CHANNEL_SERVER,"THEATRE writer shutdown timed out; incomplete captures remain unpublishable\n");
    return done;
}
void TheatreReset(void)
{
    unsigned slot;
    for(slot=0;slot<MAX_CLIENTS;++slot) TheatreStop(slot,"map_reset");
    memset(auto_slots,0,sizeof(auto_slots));
    memset(match_id,0,sizeof(match_id));
    memset(match_participants,0,sizeof(match_participants));
    match_participant_count=0;match_participants_overflow=0;
    memset(identities,0,sizeof(identities));
}
/* Called once per send frame. Automatic captures are personal recordings for
 * admitted humans; manual captures may still record diagnostic bot viewpoints. */
void TheatreAutoFrame(void)
{
    TheatrePump();
    unsigned slot;
    int enabled;
    if(sv.state==SS_GAME) remember_participants();
    if(!auto_record) {
        auto_record=Cvar_RegisterBool("theatreAutoRecord",qfalse,CVAR_ARCHIVE,
            "Automatically capture authenticated active players");
        auto_seconds=Cvar_RegisterInt("theatreSegmentSeconds",300,5,3600,CVAR_ARCHIVE,
            "Maximum automatic capture segment duration in seconds");
        auto_segments=Cvar_RegisterInt("theatreMaxSegments",24,1,240,CVAR_ARCHIVE,
            "Maximum automatic segments per recipient slot per map");
        auto_megabytes=Cvar_RegisterInt("theatreSegmentMiB",64,1,512,CVAR_ARCHIVE,
            "Maximum automatic capture file size in MiB");
    }
    enabled=auto_record->boolean;
    if(!enabled) {
        for(slot=0;slot<MAX_CLIENTS;++slot)
            if(captures[slot].automatic) TheatreStop(slot,"auto_disabled");
        if(auto_was_enabled) memset(auto_slots,0,sizeof(auto_slots));
        auto_was_enabled=0;
        return;
    }
    auto_was_enabled=1;
    if(sv.state!=SS_GAME) return;
    for(slot=0;slot<(unsigned)sv_maxclients->integer && slot<MAX_CLIENTS;++slot) {
        char name[41];
        uint64_t owner;
        theatre_capture_t *capture=&captures[slot];
        if(svs.clients[slot].state!=CS_ACTIVE || !svs.clients[slot].gentity) continue;
        /* The archive cannot publish bot/unowned captures. Do not create a
         * production spool for them, and stop an older automatic capture if
         * its authenticated connection identity is lost or replaced. */
        owner=authenticated_uid(slot);
        if(!owner) {
            if(capture->automatic) TheatreStop(slot,"auto_disabled");
            continue;
        }
        if(capture->automatic && capture->owner_uid!=owner)
            TheatreStop(slot,"client_replaced");
        if(capture->automatic && TheatreActive(slot) &&
           (uint32_t)((uint32_t)svs.time-capture->started)>=
             (uint32_t)auto_seconds->integer*1000u)
            TheatreStop(slot,"segment_complete");
        if(TheatreActive(slot) || auto_slots[slot].blocked ||
           auto_slots[slot].segments>=(unsigned)auto_segments->integer) continue;
        /* Human-readable prefix plus cryptographic capture ID; worker opens
         * exclusively, including when older segments are still draining. */
        snprintf(name,sizeof(name),"auto_%08x_%08x_%02u",
            Sys_Milliseconds(),++auto_serial,slot);
        if(!TheatreStart(slot,name)) {
            auto_slots[slot].blocked=1;
            Com_PrintError(CON_CHANNEL_SERVER,
                "THEATRE AUTO BLOCKED slot=%u reason=start_failed until=map_or_toggle\n",slot);
            continue;
        }
        captures[slot].automatic=1;
        ++auto_slots[slot].segments;
    }
}
void TheatreFrame(unsigned slot,const msg_t *prefix,const native_frame_t *native)
{
    theatre_capture_t *capture;
    snapshot_state_t state;
    const animcmd_t *commands;
    size_t command_count;
    delta_context_t context={svs.time,{0,0,0},0,0,1};
    msg_t message;
    if(!TheatreActive(slot)) return;
    if(AsyncCaptureError(captures[slot].writer)){TheatreStop(slot,"async_capture_failed");return;}
    capture=&captures[slot];
    if(!native || !prefix || prefix->overflowed || prefix->cursize<4 ||
       (unsigned)prefix->cursize>sizeof(capture_buffer) || !NativeFrameState(native)) {
        Com_PrintError(CON_CHANNEL_SERVER,"THEATRE frame rejected slot=%u builder=%s\n",slot,
            frame_failure ? frame_failure : "no captured native frame");
        TheatreStop(slot,"frame_capture_failed");return;
    }
    state=*NativeFrameState(native);
    state.sequence=capture->next;
    MSG_Init(&message,capture_buffer,sizeof(capture_buffer));
    memcpy(message.data,prefix->data,(size_t)prefix->cursize);
    message.cursize=prefix->cursize;
    commands=NativeFrameCommands(native,&command_count);
    /* Recording receives each committed frame in order; its previous sequence
     * is an explicit file dependency, not a fabricated network acknowledgment. */
    if(!ApplyServerPositionBase(&context) ||
       !PrepareServerSnapshot(&message,capture->history,&state,capture->previous,
         NativeFrameUpdates(native),commands,command_count,
         (unsigned)svs.snapFlagServerBit,1,&context)) {
        Com_PrintError(CON_CHANNEL_SERVER,"THEATRE serialization rejected slot=%u detail=%s\n",slot,
            snapshot_failure ? snapshot_failure : "position base");
        TheatreStop(slot,"frame_serialize_failed");return;
    }
    /* Leave room for the terminal record; rotate before writing a frame that
     * would cross the configured byte ceiling. Its history is discarded with
     * this file, and the next segment starts with a new full baseline. */
    if(capture->automatic && capture->bytes+16u+(unsigned)message.cursize+64u>
         (uint32_t)auto_megabytes->integer*1024u*1024u) {
        TheatreStop(slot,"segment_complete");return;
    }
    if(!record(capture,2,state.sequence,message.data,(unsigned)message.cursize)) {
        FinishHistory(capture->history,state.sequence,0);
        TheatreStop(slot,"file_write_failed");return;
    }
    if(!FinishHistory(capture->history,state.sequence,1)) {
        TheatreStop(slot,"history_commit_failed");return;
    }
    capture->previous=capture->next++;
    ++capture->frames;
}

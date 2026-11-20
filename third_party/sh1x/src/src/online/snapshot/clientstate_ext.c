#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Ability metadata owned by the retained clientState snapshot ring. */
#ifdef OL_SERVER
#include "clientstate_ext.h"
#include "server.h"
#include "player_worldweapon.h"
#include <string.h>
#include <math.h>

#define CS_EXT_SLOTS \
    (sizeof(((serverStatic_t *)0)->snapshotClients) / sizeof(clientState_t))
#pragma pack(push, 8)
typedef struct {
    client_abilities_t abilities;
    client_appearance_t appearance;
    unsigned char appearance_known;
    /* C1 clientState+134: original player method51dbb0 latches1. */
    unsigned char status_colour_enabled;
    /* clientState +0x12c/+0x130 as they were when this record was taken: the
     * world-gun children that exist in the SAME frame's entity set. The live
     * numbers are wrong for an archived (killcam) frame -- by then the owner
     * may have died and respawned with new children -- and C1 then has a
     * clientState naming an entity the snapshot does not carry (parity log
     * 2026-10-09: five dangling world guns in the first killcam snapshot). */
    unsigned world_weapon[2];
    unsigned char world_weapon_known;
    int sequence;
    int client;
    unsigned char captured;
} cs_abilities_t;
#pragma pack(pop)
#include "sys_ondemand.h"
SH1X_ONDEMAND(, cs_abilities_t, cs_snapshots, [CS_EXT_SLOTS]); /* sys_ondemand.h: allocated when the server starts */
#define snapshots (*sh1x_od_cs_snapshots)
/* Session clientState data: preserve across ClientSpawn's savedSess restore,
 * replace at client-info publication, clear on connection reuse/map reset.
 * It is deliberately separate from the spectator-copyable playerState. */
static client_abilities_t published[MAX_CLIENTS];
static client_appearance_t published_appearance[MAX_CLIENTS];
static unsigned char published_appearance_known[MAX_CLIENTS];
static unsigned char published_status_colour[MAX_CLIENTS];
/* Models a script attached to the player (IW4 Attach: the CTF carry flag on
 * j_spine4, ...). The character composition owns the whole published
 * attachment run, so without this they never reached C1. Appended after the
 * character parts when a frame is captured; resolved to wire IDs on the
 * game-frame path (UpdateClientScriptAttachments). */
static unsigned script_attach_model[MAX_CLIENTS][CLIENT_ATTACHMENTS];
static unsigned script_attach_tag[MAX_CLIENTS][CLIENT_ATTACHMENTS];
static unsigned script_attach_count[MAX_CLIENTS];

/* The published character with the script attachments appended. A part list
 * that already fills all sixteen slots keeps the character whole. */
static client_appearance_t merged_appearance(unsigned client)
{
    client_appearance_t out=published_appearance[client];
    unsigned used=0,i;
    while(used<CLIENT_ATTACHMENTS && out.attach_model[used]) ++used;
    for(i=0;i<script_attach_count[client] && used<CLIENT_ATTACHMENTS;++i,++used) {
        out.attach_model[used]=script_attach_model[client][i];
        out.attach_tag[used]=script_attach_tag[client][i];
    }
    return out;
}
#define CS_EXT_ARCHIVE \
    (sizeof(((serverStatic_t *)0)->cachedSnapshotClients) / sizeof(cachedClient_t))
SH1X_ONDEMAND(, cs_abilities_t, cs_archives, [CS_EXT_ARCHIVE]); /* sys_ondemand.h: allocated when the server starts */
#define archives (*sh1x_od_cs_archives)

static cs_abilities_t *archive_slot(const clientState_t *cs)
{
    uintptr_t address = (uintptr_t)cs;
    uintptr_t base = (uintptr_t)&svs.cachedSnapshotClients[0].cs;
    size_t slot;
    if (!cs || address < base || (address-base) % sizeof(cachedClient_t)) return NULL;
    slot = (address-base) / sizeof(cachedClient_t);
    return slot < CS_EXT_ARCHIVE ? &archives[slot] : NULL;
}

void ClearCapturedClientAbilities(void)
{
    memset(snapshots, 0, sizeof(snapshots));
    memset(published, 0, sizeof(published));
    memset(published_appearance,0,sizeof(published_appearance));
    memset(published_appearance_known,0,sizeof(published_appearance_known));
    memset(published_status_colour,0,sizeof(published_status_colour));
    memset(script_attach_count,0,sizeof(script_attach_count));
    ClearArchivedClientAbilities();
}

void ClearPublishedClientAbilities(unsigned client)
{
    if (client < MAX_CLIENTS) memset(&published[client], 0, sizeof(published[client]));
    if (client < MAX_CLIENTS) {
        memset(&published_appearance[client],0,sizeof(published_appearance[client]));
        published_appearance_known[client]=0;
        published_status_colour[client]=0;
        script_attach_count[client]=0;
    }
}

int PublishClientScriptAttachments(unsigned client,const unsigned *models,const unsigned *tags,unsigned count)
{
    unsigned i;
    if(client>=MAX_CLIENTS || count>CLIENT_ATTACHMENTS || (count && (!models || !tags))) return 0;
    for(i=0;i<count;++i)
        if(!models[i] || models[i]>=2048 || !tags[i] || tags[i]>=64) return 0;
    for(i=0;i<count;++i) {
        script_attach_model[client][i]=models[i];
        script_attach_tag[client][i]=tags[i];
    }
    script_attach_count[client]=count;
    return 1;
}

int PublishClientAppearance(unsigned client,const client_appearance_t *appearance)
{
    client_state_t validation={0};
    if(client>=MAX_CLIENTS || !OverlayClientAppearance(&validation,appearance)) return 0;
    published_appearance[client]=*appearance;
    published_appearance_known[client]=1;
    return 1;
}

void PublishClientAbilities(unsigned client, const playerState_t *ps)
{
    ps_ext_t state;
    if (client >= MAX_CLIENTS || !ps) return;
    PlayerStateExtFetch(&state, ps);
    if (state.abilities_known) published[client] = state.abilities;
    else memset(&published[client],0,sizeof(published[client]));
}

/* Own session store. C1 ClientSpawn50f67a/50f689 saves3368..36e3,
 * clears3a68 bytes, restores that range50f724/50f772, including359c.
 * Map/new-connection/disconnect reset clears only the published live byte;
 * already captured and archived records remain historical owners. */
int EnableClientStatusColour(unsigned client)
{
    if(client>=MAX_CLIENTS) return 0;
    published_status_colour[client]=1;
    return 1;
}
static int overlay_status_colour(client_state_t *out,unsigned enabled)
{
    unsigned i;
    if(!out || enabled>1) return 0;
    for(i=0;i<CLIENT_FIELDS;++i) {
        const entity_field_t *field=ClientField(i);
        if(field && field->offset==0x134) {
            out->field[i]=enabled;
            return 1;
        }
    }
    return 0;
}
int CapturePublishedClientAbilities(int sequence, unsigned client)
{
    ps_ext_t state = {0};
    if (client >= MAX_CLIENTS) return 0;
    state.abilities = published[client];
    state.abilities_known = 1;
    if(!CaptureClientAbilities(sequence, &state)) return 0;
    snapshots[sequence % svs.numSnapshotClients].status_colour_enabled=published_status_colour[client];
    snapshots[sequence % svs.numSnapshotClients].appearance=merged_appearance(client);
    snapshots[sequence % svs.numSnapshotClients].appearance_known=published_appearance_known[client];
    snapshots[sequence % svs.numSnapshotClients].world_weapon[0]=PlayerWorldWeaponEntity(client,0);
    snapshots[sequence % svs.numSnapshotClients].world_weapon[1]=PlayerWorldWeaponEntity(client,1);
    snapshots[sequence % svs.numSnapshotClients].world_weapon_known=1;
    return 1;
}

void ClearArchivedClientAbilities(void)
{
    memset(archives, 0, sizeof(archives));
}

void WriteArchivedClientAbilities(msg_t *msg, int client, const clientState_t *cache)
{
    cs_abilities_t value = {0}, *slot = cache ? archive_slot(cache) : NULL;
    unsigned i;
    if (!msg) return;
    if (client < 0 || client >= MAX_CLIENTS || (cache && !slot)) {
        msg->overflowed = 1; return;
    }
    if(slot) slot->captured=0; /* Failed writes cannot retain an older valid cache. */
    value.abilities = published[client];
    value.appearance=merged_appearance(client);
    value.appearance_known=published_appearance_known[client];
    value.status_colour_enabled=published_status_colour[client];
    /* The archive is written beside this frame's entity archive, so the live
     * children are the ones that frame's entity set holds. */
    value.world_weapon[0]=PlayerWorldWeaponEntity((unsigned)client,0);
    value.world_weapon[1]=PlayerWorldWeaponEntity((unsigned)client,1);
    value.world_weapon_known=1;
    if(value.world_weapon[0]>=2047 || value.world_weapon[1]>=2047) { msg->overflowed = 1; return; }
    for (i=0; i<ABILITY_VALUES; ++i)
        if (!isfinite(value.abilities.values[i])) { msg->overflowed = 1; return; }
    for (i=0; i<ABILITY_MASK_WORDS; ++i)
        MSG_WriteBits(msg, value.abilities.masks[i], 32);
    for (i=0; i<ABILITY_VALUES; ++i) {
        uint32_t word;
        memcpy(&word, &value.abilities.values[i], sizeof(word));
        MSG_WriteBits(msg, word, 32);
    }
    value.client = client;
    MSG_WriteBits(msg,value.appearance_known,1);
    if(value.appearance_known) {
        MSG_WriteBits(msg,value.appearance.model,11);
        for(i=0;i<CLIENT_ATTACHMENTS;++i) {
            MSG_WriteBits(msg,value.appearance.attach_model[i],11);
            MSG_WriteBits(msg,value.appearance.attach_tag[i],6);
        }
    }
    MSG_WriteBits(msg,value.world_weapon[0],11);
    MSG_WriteBits(msg,value.world_weapon[1],11);
    /* Same-process compressed archive record only, not the persisted film
     * protocol. Every writer/reader in this native changes together. */
    MSG_WriteBits(msg,value.status_colour_enabled,1);
    value.captured = !msg->overflowed;
    if (slot) *slot = value;
}

void ReadArchivedClientAbilities(msg_t *msg, const clientState_t *cache)
{
    cs_abilities_t value = {0}, *slot = archive_slot(cache);
    unsigned i;
    if (!msg) return;
    if (!slot) { msg->overflowed = 1; return; }
    slot->captured=0;
    for (i=0; i<ABILITY_MASK_WORDS; ++i)
        value.abilities.masks[i] = (uint32_t)MSG_ReadBits(msg, 32);
    for (i=0; i<ABILITY_VALUES; ++i) {
        uint32_t word = (uint32_t)MSG_ReadBits(msg, 32);
        memcpy(&value.abilities.values[i], &word, sizeof(word));
        if (!isfinite(value.abilities.values[i])) msg->overflowed = 1;
    }
    {
        int known=MSG_ReadBit(msg);
        client_state_t validation={0};
        if(known<0) { msg->overflowed=1; return; }
        value.appearance_known=(unsigned char)known;
        if(known) {
            value.appearance.model=(unsigned)MSG_ReadBits(msg,11);
            for(i=0;i<CLIENT_ATTACHMENTS;++i) {
                value.appearance.attach_model[i]=(unsigned)MSG_ReadBits(msg,11);
                value.appearance.attach_tag[i]=(unsigned)MSG_ReadBits(msg,6);
            }
            if(!OverlayClientAppearance(&validation,&value.appearance)) msg->overflowed=1;
        }
    }
    value.world_weapon[0]=(unsigned)MSG_ReadBits(msg,11);
    value.world_weapon[1]=(unsigned)MSG_ReadBits(msg,11);
    if(value.world_weapon[0]>=2047 || value.world_weapon[1]>=2047) msg->overflowed=1;
    value.world_weapon_known=1;
    {
        int enabled=MSG_ReadBit(msg);
        if(enabled<0) { msg->overflowed=1; return; }
        value.status_colour_enabled=(unsigned char)enabled;
    }
    value.client = cache->clientIndex;
    value.captured = !msg->overflowed;
    *slot = value;
}

int CaptureArchivedClientAbilities(int sequence, const clientState_t *cache)
{
    const cs_abilities_t *slot = archive_slot(cache);
    ps_ext_t state = {0};
    if (!slot || !slot->captured || slot->client != cache->clientIndex) return 0;
    state.abilities = slot->abilities;
    state.abilities_known = 1;
    if(!CaptureClientAbilities(sequence, &state)) return 0;
    snapshots[sequence % svs.numSnapshotClients].status_colour_enabled=slot->status_colour_enabled;
    snapshots[sequence % svs.numSnapshotClients].appearance=slot->appearance;
    snapshots[sequence % svs.numSnapshotClients].appearance_known=slot->appearance_known;
    memcpy(snapshots[sequence % svs.numSnapshotClients].world_weapon,slot->world_weapon,sizeof(slot->world_weapon));
    snapshots[sequence % svs.numSnapshotClients].world_weapon_known=slot->world_weapon_known;
    return 1;
}

int CaptureClientAbilities(int sequence, const ps_ext_t *state)
{
    cs_abilities_t next = {0};
    int slot;
    if (sequence < 0 || svs.numSnapshotClients <= 0 ||
        (size_t)svs.numSnapshotClients > CS_EXT_SLOTS) return 0;
    slot = sequence % svs.numSnapshotClients;
    next.sequence = sequence;
    next.client = svs.snapshotClients[slot].clientIndex;
    if (state && state->abilities_known) next.abilities = state->abilities;
    next.captured = 1;
    snapshots[slot] = next;
    return 1;
}

int OverlayCapturedClientAbilities(int sequence, client_state_t *out)
{
    const cs_abilities_t *held;
    int slot, count = svsHeader.numSnapshotClients;
    if (!out || sequence < 0 || count <= 0 ||
        (size_t)count > CS_EXT_SLOTS || !svsHeader.snapshotClients ||
        sequence >= svsHeader.nextSnapshotClients ||
        (int64_t)sequence < (int64_t)svsHeader.nextSnapshotClients - count) return 0;
    slot = sequence % count;
    held = &snapshots[slot];
    if (!held->captured || held->sequence != sequence ||
        held->client != svsHeader.snapshotClients[slot].clientIndex) return 0;
    {
        client_state_t next=*out;
        if(!OverlayClientAbilities(&next, &held->abilities) ||
           (held->appearance_known && !OverlayClientAppearance(&next,&held->appearance)) ||
           (held->world_weapon_known && !OverlayClientWorldWeapons(&next,held->world_weapon)) ||
           !overlay_status_colour(&next,held->status_colour_enabled)) return 0;
        *out=next;return 1;
    }
}
#endif

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentClientAbilitiesEmpty(void) {return resident_static_zero(snapshots,sizeof(snapshots)) && resident_static_zero(archives,sizeof(archives)) && resident_static_zero(published,sizeof(published)) && resident_static_zero(published_appearance,sizeof(published_appearance)) && resident_static_zero(published_appearance_known,sizeof(published_appearance_known)) && resident_static_zero(script_attach_count,sizeof(script_attach_count)) && resident_static_zero(published_status_colour,sizeof(published_status_colour));}
#endif

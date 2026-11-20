#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ol_stats.h"
#include "profile_updates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CLIENTS 32
#include "sys_ondemand.h"
typedef struct { uint32_t received; uint8_t bytes[STATS_SIZE]; } stats_slot_t;
SH1X_ONDEMAND(, stats_slot_t, client_stats, [CLIENTS]); /* sys_ondemand.h: allocated when the server starts */
#define stats (*sh1x_od_client_stats)
/* Script edits live here so that every profile reader observes them while the
 * uploaded bytes above stay intact for upload accounting and authentication. */
static uint8_t *session[CLIENTS];
static void drop_session(unsigned client)
{ if(client < CLIENTS) { free(session[client]); session[client] = NULL; } }
void ResetClientStats(unsigned client)
{ if(client < CLIENTS) { drop_session(client); memset(&stats[client], 0, sizeof(stats[client]));
    ResetProfileUpdates(client); } }
uint32_t ClientStatsMissing(unsigned client)
{ return client < CLIENTS ? STATS_MASK & ~stats[client].received : STATS_MASK; }
const uint8_t *ClientStatsUpload(unsigned client)
{ return client < CLIENTS && !ClientStatsMissing(client) ? stats[client].bytes : NULL; }
const uint8_t *ClientStats(unsigned client)
{ return client < CLIENTS && session[client] ? session[client] : ClientStatsUpload(client); }
uint8_t *ClientStatsWritable(unsigned client)
{
    const uint8_t *upload; uint8_t *copy;
    if(client >= CLIENTS) return NULL;
    if(session[client]) return session[client];
    upload = ClientStatsUpload(client); if(!upload) return NULL;
    copy = malloc(STATS_SIZE); if(!copy) return NULL;
    memcpy(copy, upload, STATS_SIZE); session[client] = copy; return copy;
}
static uint32_t profile_word(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
int ClientClassPerkItem(unsigned client,unsigned class_index,unsigned slot,int *out)
{
    const uint8_t *profile=ClientStats(client); unsigned offset,value;
    if(!profile || !out || class_index>=8 || slot<1 || slot>5 ||
       profile_word(profile+4)!=220 || profile_word(profile+8)!=UINT32_C(0xe4fd5218)) return 0;
    offset=0x1511+class_index*92+(6+slot)*2;
    value=profile[offset] | (unsigned)profile[offset+1]<<8;
    *out=value>=0x8000 ? (int)value-0x10000 : (int)value;
    return 1;
}
int AcceptClientStats(unsigned client, unsigned chunk, const uint8_t *data, size_t n)
{
    size_t offset, expected; uint32_t bit;
    if(client >= CLIENTS || chunk >= STATS_COUNT || !data) return 0;
    offset = chunk * STATS_CHUNK; expected = STATS_SIZE - offset;
    if(expected > STATS_CHUNK) expected = STATS_CHUNK;
    if(n != expected) return 0;
    bit = UINT32_C(1) << chunk;
    if(stats[client].received & bit) return !memcmp(stats[client].bytes + offset, data, n);
    /* Fresh upload bytes supersede any script edit of this client's profile. */
    drop_session(client);
    memcpy(stats[client].bytes + offset, data, n); stats[client].received |= bit;
    if(!ClientStatsMissing(client)) ProfileUploadReplaced(client);
    return 1;
}
size_t WriteStatsResponse(uint8_t *out, size_t cap, uint32_t missing)
{
    char text[64]; int n;
    if(!out || (missing & ~STATS_MASK)) return 0;
    n = snprintf(text, sizeof(text), "statResponse %u", missing);
    if(n < 0 || (size_t)n >= sizeof(text) || cap < (size_t)n + 5) return 0;
    memset(out, 255, 4); memcpy(out + 4, text, (size_t)n + 1); return (size_t)n + 5;
}

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
int PersistentPublicProfilesEmpty(void) {return resident_storage_zero(stats,sizeof(stats)) && resident_storage_zero(session,sizeof(session));}

#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#include "ol_profile.h"
#include "ol_stats.h"
#include <stdlib.h>
#include <string.h>
/* data is what was attached; session is the copy script edits are made on, so
 * that every reader observes them without altering the attached bytes. */
static struct {uint8_t *data,*session;uint64_t uid;} clients[32];
int AttachClientSecureProfile(unsigned client,uint64_t uid,const uint8_t *profile,size_t size,
    const structured_schema_t *schema)
{
    const structured_definition_t *def;const uint8_t *record;size_t capacity;uint8_t *copy;
    if(client>=32 || !uid || !ProfileRecord(profile,size,schema,&def,&record,&capacity))return 0;
    copy=malloc(STATS_SIZE);if(!copy)return 0;
    memcpy(copy,profile,STATS_SIZE);free(clients[client].data);
    free(clients[client].session);clients[client].session=NULL;
    clients[client].data=copy;clients[client].uid=uid;return 1;
}
const uint8_t *ClientSecureProfileAttached(unsigned client,uint64_t *uid)
{
    if(client>=32 || !clients[client].data)return NULL;
    if(uid)*uid=clients[client].uid;return clients[client].data;
}
const uint8_t *ClientSecureProfile(unsigned client,uint64_t *uid)
{
    if(client>=32 || !clients[client].data)return NULL;
    if(uid)*uid=clients[client].uid;
    return clients[client].session ? clients[client].session : clients[client].data;
}
uint8_t *ClientSecureProfileWritable(unsigned client)
{
    uint8_t *copy;
    if(client>=32 || !clients[client].data)return NULL;
    if(clients[client].session)return clients[client].session;
    copy=malloc(STATS_SIZE);if(!copy)return NULL;
    memcpy(copy,clients[client].data,STATS_SIZE);
    clients[client].session=copy;return copy;
}
void DetachClientSecureProfile(unsigned client)
{if(client<32){free(clients[client].data);free(clients[client].session);
    memset(clients+client,0,sizeof(clients[client]));}}
void ResetClientSecureProfiles(void)
{for(unsigned i=0;i<32;++i)DetachClientSecureProfile(i);}
/* See CarryClientSecureProfile in ol_profile.h. */
static struct {uint8_t *data;uint64_t uid;} carried[32];
void DropCarriedSecureProfile(unsigned client)
{if(client<32){free(carried[client].data);memset(carried+client,0,sizeof(carried[client]));}}
void CarryClientSecureProfile(unsigned client)
{
    const uint8_t *view;uint8_t *copy;
    if(client>=32)return;
    DropCarriedSecureProfile(client);
    view=clients[client].session ? clients[client].session : clients[client].data;
    if(!view || !clients[client].uid)return;
    copy=malloc(STATS_SIZE);if(!copy)return;
    memcpy(copy,view,STATS_SIZE);
    carried[client].data=copy;carried[client].uid=clients[client].uid;
}
int TakeCarriedSecureProfile(unsigned client,uint64_t uid,uint8_t *out,size_t capacity)
{
    int ok;
    if(client>=32 || !carried[client].data)return 0;
    ok=uid && carried[client].uid==uid && out && capacity>=STATS_SIZE;
    if(ok)memcpy(out,carried[client].data,STATS_SIZE);
    DropCarriedSecureProfile(client);
    return ok;
}

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
int PersistentSecureProfilesEmpty(void) {return resident_storage_zero(clients,sizeof(clients)) && resident_storage_zero(carried,sizeof(carried));}

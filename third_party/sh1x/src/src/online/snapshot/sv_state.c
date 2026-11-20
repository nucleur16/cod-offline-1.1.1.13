#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Source-level bridge for native CoDOL gamestate publication. */
#include "sv_state.h"
#include "gamestate_check.h"
#include "wire.h"
/* The authored sun-sample override is a full-server feature; this file is also
 * compiled into the portable configstring fixtures, which link no game code. */
#ifdef OL_SERVER
#include "map_shadow.h"
#else
#define RefreshMapSunSampleSize() 1
#define ResetMapSunSampleSize() ((void)0)
#endif
#include <stdio.h>
#include <string.h>

static configstrings_t configstrings;
static configstrings_t inherited_configstrings;
static char inherited_zone_type[16];
static char inherited_map_name[64];
static char inherited_map_zone_type[16];
static unsigned server_animlist_flags;
static char client_map_name[64];
/* The last gamestate sanity check (CheckGamestate) and whether it,
 * rather than a producer or capacity failure, rejected that gamestate. */
static gamestate_check_t last_gamestate_check;
static int last_gamestate_check_failed;
const gamestate_check_t *LastGamestateCheck(void) { return &last_gamestate_check; }
int GamestateCheckFailed(void) { return last_gamestate_check_failed; }
void ClearGamestateCheck(void)
{
    last_gamestate_check_failed = 0;
    memset(&last_gamestate_check, 0, sizeof(last_gamestate_check));
}

int SetServerAnimListFlags(unsigned flags)
{
    /* Five actor composition classes, C1 0x40842d..0x40847a: bits 0..4. */
    if(flags & ~31u) return 0;
    server_animlist_flags = flags;
    return 1;
}
unsigned ServerAnimListFlags(void) { return server_animlist_flags; }

configstrings_t *ServerConfigstrings(void) { return &configstrings; }
const configstrings_t *ServerInheritedConfigstrings(void)
{ return *inherited_zone_type ? &inherited_configstrings : NULL; }
const char *ServerMenuName(unsigned slot)
{
    const char *name;
    if(slot>=MENU_SLOTS) return NULL;
    name=GetConfigstring(&configstrings,OL_CS_MENUS+slot);
    return name ? name : GetConfigstring(ServerInheritedConfigstrings(),
                                               OL_CS_MENUS+slot);
}
int SetServerInheritedConfigstrings(const char *zone_type,size_t rows,const char *const *cells)
{
    uint32_t checksum;
    if(*inherited_zone_type || !ConstConfigChecksum(zone_type,&checksum) ||
       !ImportConstantConfigstrings(&inherited_configstrings,rows,cells)) return 0;
    strcpy(inherited_zone_type,zone_type);
    return RefreshMapSunSampleSize();
}
void ResetServerConfigstrings(void)
{
    ResetMapSunSampleSize();
    ClearConfigstrings(&configstrings);
    ClearConfigstrings(&inherited_configstrings);
    memset(inherited_zone_type,0,sizeof(inherited_zone_type));
    memset(inherited_map_name,0,sizeof(inherited_map_name));
    memset(inherited_map_zone_type,0,sizeof(inherited_map_zone_type));
    memset(client_map_name,0,sizeof(client_map_name));
    server_animlist_flags = 0;
}
int SetClientMapName(const char *client_map)
{
    if(!client_map || !*client_map) { client_map_name[0]=0; return 1; }
    if(strlen(client_map)>=sizeof(client_map_name) || strchr(client_map,'\\')) return 0;
    strcpy(client_map_name,client_map);
    return 1;
}
const char *ClientMapName(void)
{
    return client_map_name[0] ? client_map_name : NULL;
}
int SetServerMapIdentity(const char *map,const char *zone_type)
{
    size_t length,i;
    if(!map || !*map || !zone_type ||
       !IsZoneType(zone_type) ||
       (length=strlen(map))>=sizeof(inherited_map_name) || map[0]<'a' || map[0]>'z')
        return 0;
    for(i=1;i<length;++i)
        if((map[i]<'a' || map[i]>'z') && (map[i]<'0' || map[i]>'9') && map[i]!='_')
            return 0;
    if(*inherited_map_name)
        return !strcmp(inherited_map_name,map) && !strcmp(inherited_map_zone_type,zone_type);
    memcpy(inherited_map_name,map,length+1);
    strcpy(inherited_map_zone_type,zone_type);
    return 1;
}
/* The asset category the mounted map was installed with ("mp", "coop" or
 * "zm"). Latched by SetServerMapIdentity during map setup, so every
 * consumer sees the category whose archives and constant configstrings are
 * actually mounted rather than a dvar a later command could relatch.
 * NULL until a map identity is installed. */
int IsZoneType(const char *zone_type)
{
    return zone_type && (!strcmp(zone_type,"mp") || !strcmp(zone_type,"coop") ||
                         !strcmp(zone_type,"zm") || !strcmp(zone_type,"survival"));
}
int ZoneTypeRunsSoldiers(const char *zone_type)
{
    return zone_type && (!strcmp(zone_type,"coop") || !strcmp(zone_type,"survival"));
}
const char *ServerZoneType(void)
{
    return *inherited_map_zone_type ? inherited_map_zone_type : NULL;
}
/* The mounted map's name, latched alongside the category above and cleared
 * between maps. NULL until a map identity is installed. Consumers use it to
 * scope per-map state that would otherwise silently persist for the whole
 * process -- see the missing-xmodel report table in db_load.cpp. */
const char *ServerMapName(void)
{
    return *inherited_map_name ? inherited_map_name : NULL;
}
int ServerMapIdentityMatches(const char *map,const char *zone_type)
{
    return map && zone_type && *inherited_map_name &&
        !strcmp(inherited_map_name,map) && !strcmp(inherited_map_zone_type,zone_type) &&
        !strcmp(inherited_zone_type,zone_type);
}
int SetServerId(int32_t server_id)
{
    char value[16];
    int n = snprintf(value, sizeof(value), "%d", (int)server_id);
    return n > 0 && (size_t)n < sizeof(value) &&
        OL_SetConfigstring(&configstrings, CS_SERVER_ID, value);
}
int OL_SetServerInfo(const char *map,const char *gametype,unsigned max_clients,const char *hostname)
{
    char info[1024]; int length;
    if(!map || !*map || !gametype || !*gametype || !max_clients ||
       max_clients>MAX_GAME_CLIENTS || strchr(map,'\\') || strchr(gametype,'\\') ||
       (*inherited_map_name && strcmp(inherited_map_name,map)) ||
       !hostname || strlen(hostname)>255 || strpbrk(hostname,"\\\";\r\n")) return 0;
    /* CG_ParseServerinfo 0x465f57 reads "2485" at 0xc44704 as maxclients. */
    /* CG_ParseServerinfo 0x465bc1 copies numeric key 2492 into hostname[256]. */
    /* CG_ParseServerinfo465c27 stores key2482 for CG_Init's animlist loader.
     * Use the captured map-start mask, not a cvar that can change mid-map. */
    /* C1 keys its map-named client assets on this "mapname" (the dvar):
     * the animlist (0x450b3e), the createfx / _fx scripts (0x42df10, which
     * for a "coop_" name loads the level's AND the mission's) and the
     * world file (0x618950 strips "coop_<mode>_" back to the level). */
    length=snprintf(info,sizeof(info),"\\g_gametype\\%s\\mapname\\%s\\2485\\%u\\2492\\%s\\2482\\%u",gametype,
                    client_map_name[0] ? client_map_name : map,max_clients,hostname,server_animlist_flags);
    return length>=0 && (size_t)length<sizeof(info) && OL_SetConfigstring(&configstrings,0,info);
}

static int write_server_gamestate(msg_t *msg, const char *map, const char *zone_type,
                               const char *gametype, unsigned max_clients, uint32_t sequence,
                               uint32_t client_number, uint32_t checksum_feed,const char *hostname,
                               int replay)
{
    ol_msg_t wire;
    configstrings_t merged={0};
    const configstrings_t *published=&configstrings;
    unsigned i;
    uint32_t crc;
    const char *slowmotion;
    size_t limit;
    ClearGamestateCheck();
    if (!msg || msg->overflowed || msg->splitData || msg->cursize < 0 ||
        msg->maxsize < msg->cursize || !max_clients || max_clients>MAX_GAME_CLIENTS ||
        client_number>=max_clients || !map || !*map || !gametype || !*gametype ||
        strchr(map, '\\') || strchr(gametype, '\\') ||
        !ConstConfigChecksum(zone_type, &crc)) goto fail;
    /* These textual serverinfo keys are accepted by 1.1.1.13. The replicated
     * numeric dvar and asset producers populate their own CoDOL slots. */
    /* C1 466EB0 unconditionally scans CS 6132 into four stack locals during
     * gamestate initialization, without checking the scan result. An absent
     * value can schedule a ramp to an uninitialized float (observed in native
     * demo playback). Publish the neutral state only when no script supplied
     * a state; preserve an active scripted cinematic ramp verbatim. */
    slowmotion = GetConfigstring(&configstrings, CS_SLOWMOTION);
    if ((!slowmotion || !*slowmotion) &&
        !OL_SetConfigstring(&configstrings, CS_SLOWMOTION, "0 0 1 1")) goto fail;
    if (!OL_SetServerInfo(map,gametype,max_clients,hostname) ||
        !OL_SetConfigstring(&configstrings, 2, "cod")) goto fail;
    /* The client inherits its fastfile table before applying these overrides.
     * Message capacity alone cannot protect the separate 128 KiB string pool.
     * Require the mounted table for this zone so validation includes exactly
     * the inherited namespace the client will use. */
    if (!ServerMapIdentityMatches(map,zone_type) ||
        !ConfigstringPoolSize(&configstrings, &inherited_configstrings, NULL))
        goto fail;
    /* Live clients inherit the mounted constant table. C1's standalone demo
     * path instead installs only the gamestate's explicit entries (observed:
     * all 843 inherited model slots absent). Flatten the effective namespace
     * for recordings without mutating either bank. Explicit empties win too. */
    if(replay) {
        for(i=0;i<OL_MAX_CONFIGSTRINGS;++i) {
            const char *value=configstrings.values[i] ? configstrings.values[i] : inherited_configstrings.values[i];
            if(value && !OL_SetConfigstring(&merged,i,value)) goto fail;
        }
        published=&merged;
    }
    MsgWriteInit(&wire, msg->data + msg->cursize, msg->maxsize - msg->cursize);
    /* The header map is the one C1 builds its zone list and dvar from; a
     * coop client map name (SetClientMapName) goes there too. */
    if (!WriteGamestate(&wire, published, client_map_name[0] ? client_map_name : map, zone_type, crc,
                              sequence, client_number, checksum_feed)) goto fail;
    /* Decode what is about to go out and hold it to C1's limits, so a server
     * bug drops one client with a named reason instead of reaching C1. A
     * flattened replay carries the inherited bank itself. */
    limit = (size_t)msg->maxsize < MAX_MESSAGE ? (size_t)msg->maxsize : MAX_MESSAGE;
    if (!CheckGamestate(wire.data, wire.size, replay ? NULL : &inherited_configstrings,
                              max_clients, (size_t)msg->cursize + wire.size, limit,
                              &last_gamestate_check)) {
        last_gamestate_check_failed = 1;
        goto fail;
    }
    msg->cursize += wire.size;
    /* Gamestate ends in byte fields; do not share its bit pool with another svc. */
    msg->bit = msg->cursize * 8;
    ClearConfigstrings(&merged);
    return 1;
fail:
    ClearConfigstrings(&merged);
    if (msg) msg->overflowed = qtrue;
    return 0;
}

int WriteServerGamestate(msg_t *msg,const char *map,const char *zone_type,
    const char *gametype,unsigned max_clients,uint32_t sequence,uint32_t client_number,
    uint32_t checksum_feed,const char *hostname)
{
    return write_server_gamestate(msg,map,zone_type,gametype,max_clients,sequence,
        client_number,checksum_feed,hostname,0);
}
int WriteServerReplayGamestate(msg_t *msg,const char *map,const char *zone_type,
    const char *gametype,unsigned max_clients,uint32_t sequence,uint32_t client_number,
    uint32_t checksum_feed,const char *hostname)
{
    return write_server_gamestate(msg,map,zone_type,gametype,max_clients,sequence,
        client_number,checksum_feed,hostname,1);
}

#ifdef OL_SERVER
#include "persistent_world.h"
static int resident_world_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}
int PersistentConfigBanksEmpty(void){
    return resident_world_zero(&configstrings,sizeof(configstrings)) &&
        resident_world_zero(&inherited_configstrings,sizeof(inherited_configstrings)) &&
        !inherited_zone_type[0]&&!inherited_map_name[0]&&!inherited_map_zone_type[0]&&
        !client_map_name[0]&&!server_animlist_flags;}

#endif

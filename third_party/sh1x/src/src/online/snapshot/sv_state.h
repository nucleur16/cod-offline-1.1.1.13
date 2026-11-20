#ifndef SV_STATE_H
#define SV_STATE_H
#include "configstrings.h"
#include "gamestate_check.h"
#include "msg.h"
#include "snapshot.h"
#include "history.h"

/* C linkage: db_load.cpp and other C++ translation units consume these,
 * and without the guard they look for a mangled symbol that the C
 * definitions never emit. The sibling headers in this directory already
 * do this; this one was simply missed until a C++ consumer appeared. */
#ifdef __cplusplus
extern "C" {
#endif

/* Native semantic producers use CoDOL indices, never IW3 array offsets. */
configstrings_t *ServerConfigstrings(void);
/* Derive wide-position bases from published CS 16, including decimal parsing
 * and C1 rounding. Falls back to inherited CS 16; empty/absent means zero.
 * Other context members are preserved. */
int ApplyServerPositionBase(delta_context_t *);
void ResetServerConfigstrings(void);
/* Capture the map whose archives and constant configstring table are mounted.
 * A gamestate for another map/type would make C1 load one BSP and then apply
 * another map's namespace.  The identity is immutable until map reset. */
int SetServerMapIdentity(const char *map, const char *zone_type);
int ServerMapIdentityMatches(const char *map, const char *zone_type);
/* "mp", "coop" or "zm" for the map whose archives are mounted; NULL before
 * map setup installs an identity. Borrowed, valid until the next map reset. */
const char *ServerZoneType(void);
/* One of the asset categories this server mounts: "mp", "coop", "zm" or
 * "survival" (Spec Ops Survival, common_gametype_survival on an mp_* level). */
int IsZoneType(const char *zone_type);
/* The categories whose matches run the SP-style soldier actors (coop
 * missions and survival): actor weapons and fire, the level's own node graph,
 * the soldier species. */
int ZoneTypeRunsSoldiers(const char *zone_type);
/* The mounted map's name; NULL before map setup installs an identity.
 * Borrowed, valid until the next map reset. Use it to scope per-map state:
 * a process-lifetime cache silently masks a real failure on a later map. */
const char *ServerMapName(void);
/* Install once after map archives load; reset clears inherited and overrides.
 * NULL before installation. Constants are owned and immutable for this map. */
const configstrings_t *ServerInheritedConfigstrings(void);
/* Effective C1 menu name: override (including explicit clear), then inherited
 * fastfile value. NULL for an absent or out-of-range slot. Borrowed string. */
const char *ServerMenuName(unsigned slot);
int SetServerInheritedConfigstrings(const char *zone_type,size_t rows,const char *const *cells);
int OL_SetServerInfo(const char *map,const char *gametype,unsigned max_clients,const char *hostname);
/* The map name serverinfo tells C1 when it differs from the level the
 * server loaded (a coop match's playlist spelling, coop_<mode>_<level>; see
 * coopClientMapName in sv_main.c). NULL or "" publishes the level
 * name. Set during full map setup, before any gamestate; reset clears. */
int SetClientMapName(const char *client_map);
const char *ClientMapName(void);
/* Set once during full map setup before loading animlists or issuing a
 * gamestate. The four class bits are human/dog/civilian/zombie; reset clears.
 * The loader must use this same captured value as serverinfo key2482. */
int SetServerAnimListFlags(unsigned flags);
unsigned ServerAnimListFlags(void);
int SetServerId(int32_t server_id);
int PublishServerId(int32_t server_id);
/* Allocated lazily, outside retained client_t's assembly-sensitive layout.
 * Clear on client disconnect and server reset. Never share ACK bases between
 * slots. Returned pointers are invalid after the corresponding reset. */
history_t *ClientSnapshotHistory(unsigned client_number);
/* The newest server message this client reports having received
 * (client_t.messageAcknowledge); call before preparing its snapshot. */
void NoteClientSnapshotReceived(unsigned client_number, uint32_t received);
/* Whether the next snapshot preparation will delta-code against a retained
 * base (the ACKed frame, or the newest flag-8 frame while it is in flight).
 * A missing base requires an origin seed. */
int ClientSnapshotHasBase(unsigned client_number, uint32_t acknowledged,
                                uint32_t next_sequence, unsigned flags);
void ResetClientSnapshotHistory(unsigned client_number);
void ResetSnapshotHistories(void);
/* Bind prepared state to one complete outgoing message, including its EOF.
 * Owns a byte copy until finish/discard/reset; commit requires an exact match.
 * Preparation derives context.position_base from the published CoDOL map
 * center before history quantization/serialization; failure preserves context.
 * Begin/recovery must discard it before reusing a message buffer. */
int PrepareClientSnapshot(unsigned client_number, msg_t *msg,
    const snapshot_state_t *state, uint32_t acknowledged,
    const player_array_updates_t *updates, const animcmd_t *commands,
    size_t command_count, unsigned flags, int rov, delta_context_t *context);
int ClientSnapshotPrepared(unsigned client_number, const msg_t *msg, uint32_t sequence);
/* Any prepared frame, including one whose message/sequence no longer matches. */
int ClientSnapshotPending(unsigned client_number);
int FinishClientSnapshot(unsigned client_number, const msg_t *msg, uint32_t sequence, int accepted);
void DiscardClientSnapshot(unsigned client_number);
int WriteServerGamestate(msg_t *msg, const char *map, const char *zone_type,
                               const char *gametype, unsigned max_clients, uint32_t sequence,
                               uint32_t client_number, uint32_t checksum_feed,const char *hostname);
/* The sanity check of the gamestate the last Write*Gamestate call built
 * (gamestate_check.h). Failed() is nonzero only when that check, not a
 * producer or capacity failure, rejected it; its error names the item. */
const gamestate_check_t *LastGamestateCheck(void);
int GamestateCheckFailed(void);
/* Forget the last result; called before every gamestate build. */
void ClearGamestateCheck(void);
/* Standalone replay needs the effective inherited+override namespace. */
int WriteServerReplayGamestate(msg_t *msg,const char *map,const char *zone_type,
    const char *gametype,unsigned max_clients,uint32_t sequence,uint32_t client_number,
    uint32_t checksum_feed,const char *hostname);
/* Append a complete snapshot including EOF after reliable ACK/commands. On
 * failure marks overflow without advancing the engine message or context.
 * Caller must not append another EOF or commit snapshot history on failure. */
int WriteServerSnapshot(msg_t *msg, const snapshot_state_t *state,
                              const snapshot_state_t *base,
                              const player_array_updates_t *updates,
                              const animcmd_t *commands, size_t command_count,
                              unsigned flags, int read_origin_velocity,
                              delta_context_t *context);
/* Select an exact ACK base (otherwise full), prepare owned history and append.
 * Success requires FinishHistory(state->sequence, transport_accepted) after the
 * send attempt. Failed appends discard only the newly prepared frame. */
int PrepareServerSnapshot(msg_t *msg, history_t *history,
                                const snapshot_state_t *state, uint32_t acknowledged,
                                const player_array_updates_t *updates,
                                const animcmd_t *commands, size_t command_count,
                                unsigned flags, int read_origin_velocity,
                                delta_context_t *context);
#ifdef __cplusplus
}
#endif
#endif

/* Sanity check of a serialized CoDOL gamestate before it is sent to C1.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The check decodes the bytes WriteGamestate produced, with the same
 * reader C1's layout implies, and validates them against the limits C1
 * 1.1.1.13 enforces (configstrings.h, wire.h). It catches server
 * bugs that would otherwise reach a player as a client error or a corrupt
 * namespace; it is not a defence against a hostile host.
 *
 * The C1 gamestate (svc 2) carries a header, one configstring block and the
 * recipient's client number. It has no entity baseline section: C1 builds
 * every entity from the first snapshot. The only entity number in it is the
 * client number, which must be a valid client slot. */
#ifndef GAMESTATE_CHECK_H
#define GAMESTATE_CHECK_H
#include <stddef.h>
#include <stdint.h>
#include "configstrings.h"
#pragma pack(push, 8)

#ifdef __cplusplus
extern "C" {
#endif

/* C1's gamestate map name buffer (cl.mapname, 64 bytes with its NUL). */
#define GAMESTATE_MAP_CHARS 63

typedef struct {
    unsigned configstrings;  /* entries in the block */
    unsigned last_index;     /* highest index sent */
    unsigned client_number;
    size_t longest;          /* longest configstring value, in chars */
    unsigned longest_index;
    size_t gamestate_bytes;  /* the svc_gamestate section */
    size_t message_bytes;    /* the whole outgoing message */
    size_t message_limit;
    size_t pool_bytes;       /* C1's rebuilt string pool, inherited + sent */
    char error[256];         /* names the offending item on failure */
} gamestate_check_t;

/* data/size: the gamestate section alone, starting at its svc byte (2).
 * inherited: the constant bank C1 already holds for this zone (NULL for none,
 * e.g. a flattened replay gamestate). max_clients: the published 2485 value.
 * message_bytes: the full outgoing message including this section;
 * message_limit: the smaller of the send buffer and C1's maximum message.
 * Returns 1 when every check passes; otherwise 0 with out->error set. */
int CheckGamestate(const uint8_t *data, size_t size,
                         const configstrings_t *inherited, unsigned max_clients,
                         size_t message_bytes, size_t message_limit,
                         gamestate_check_t *out);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

#ifndef LATE_CORPSE_H
#define LATE_CORPSE_H
#ifdef OL_SERVER
/* Actor corpses for a client that did not watch the actor die.
 *
 * C1 (1.1.1.13) has no way to build an actor corpse from its entity state.
 * Its 16 corpse composition records (cgs+0x2a70, stride 0x29c) are filled
 * only by 0x42a5c0, which copies the dying actor's composition record
 * (cgs+0x168514) into the corpse record and swaps their animation trees.
 * 0x46c3a0 calls it for an actor record of the previous snapshot whose
 * entity is now eType 20, and 0x42a5c0 returns at once unless the entity's
 * previous eType (centity+2) was something else. A client that first gets
 * the body as eType 20 (it joined later, or the actor died out of its view)
 * therefore draws the entity from an empty corpse record: nothing.
 *
 * T5 draws such a body. To give C1 the same, the server takes that client
 * through the transition C1 does handle: the body is shown to it as an actor
 * (eType 18) in a free actor slot, with a composition record built from the
 * corpse entity and the corpse tree's pose bootstrapped by the actor
 * animation channel, until the client acknowledges one such snapshot; after
 * that it is the corpse again and C1's own 0x42a5c0 builds the record. A
 * client that watched the death is left alone. */
#include <stdint.h>
#include "actors.h"
#include "dog_animation.h"
struct entityState_s;
struct gentity_s;
#ifdef __cplusplus
extern "C" {
#endif

/* A live (non-archived) snapshot for client: decides which corpses it must
 * be shown as actors and rewrites those entity states in the snapshot ring
 * (eType 20 -> 18, corpse slot -> the borrowed actor slot). */
void LateCorpsePlan(unsigned client, uint32_t sequence, uint32_t acknowledged,
                          struct entityState_s *ring, unsigned ringSize,
                          unsigned first, unsigned count);
/* An archived snapshot for client: nothing is rewritten, any presentation
 * in progress restarts on the next live snapshot. */
void LateCorpseArchivedFrame(unsigned client);
void LateCorpseClear(unsigned client);
void LateCorpseReset(void);

/* The composition records for the bodies shown as actors in this sequence,
 * merged into out[] (sorted by slot). 0 on failure. */
int LateCorpseAppendRecords(unsigned client, uint32_t sequence,
                                  actor_state_t *out, unsigned capacity,
                                  unsigned *count,
                                  int (*build)(actor_state_t *, int, struct gentity_s *));
/* Their animation sources (corpse trees), appended after `count`. Returns
 * how many were appended. */
unsigned LateCorpseSources(unsigned client, uint32_t sequence,
                                 dog_anim_capture_source_t *out, unsigned capacity);

#ifdef __cplusplus
}
#endif
#endif
#endif

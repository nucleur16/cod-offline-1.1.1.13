#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Actor corpses for a client that did not watch the actor die: see
 * late_corpse.h for the C1 evidence and the approach. */
#ifdef OL_SERVER
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "q_shared.h"
#include "qcommon_io.h"
#include "entity.h"
#include "g_shared.h"
#include "late_corpse.h"
#include "actor_lifecycle.h"
#include "sentient.h"
int ServerModelToWire(unsigned index);
}

#define LATE_CORPSES 16
#define LATE_SLOTS 32

enum { LATE_NONE, LATE_BUILT, LATE_NEEDS, LATE_SHOWN };

struct late_state {
    unsigned generation;
    unsigned char state, slot;
    uint32_t since;
};
struct late_item {
    unsigned slot, corpse, number, generation, enemyModel, team;
    gentity_s *entity;
    const XAnimTree_s *tree;
};
struct late_client {
    late_state corpses[LATE_CORPSES];
    /* Entity numbers that carried a live actor record in the previous live
     * snapshot built for this client. */
    unsigned char actors[MAX_GENTITIES / 8];
    int actorsValid;
    /* What the snapshot `sequence` shows as actors. */
    uint32_t sequence;
    int planned;
    unsigned count;
    late_item items[LATE_CORPSES];
};
static late_client clients[MAX_CLIENTS];

/* Bounded trace: one line per presentation start and end. */
static unsigned traceLines;
#define LATE_TRACE(...) do { if (traceLines < 400) { ++traceLines; \
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL late corpse: " __VA_ARGS__); } } while (0)

static int bit(const unsigned char *set, unsigned n) { return n < MAX_GENTITIES && (set[n >> 3] >> (n & 7)) & 1; }

extern "C" void LateCorpseClear(unsigned client)
{
    if (client < MAX_CLIENTS) memset(&clients[client], 0, sizeof(clients[client]));
}
extern "C" void LateCorpseReset(void)
{
    memset(clients, 0, sizeof(clients));
}
extern "C" void LateCorpseArchivedFrame(unsigned client)
{
    if (client >= MAX_CLIENTS) return;
    late_client *c = &clients[client];
    /* The archived world carries no presentation record, so a body that was
     * being shown starts over once the client is back in the live world; and
     * the actor set this client last had is no longer the live one. */
    for (unsigned k = 0; k < LATE_CORPSES; ++k)
        if (c->corpses[k].state == LATE_SHOWN) c->corpses[k].state = LATE_NEEDS;
    c->actorsValid = 0;
    c->planned = 0;
    c->count = 0;
}

extern "C" void LateCorpsePlan(unsigned client, uint32_t sequence, uint32_t acknowledged,
                                     entityState_t *ring, unsigned ringSize,
                                     unsigned first, unsigned count)
{
    if (client >= MAX_CLIENTS || !ring || !ringSize) return;
    late_client *c = &clients[client];
    unsigned char current[MAX_GENTITIES / 8];
    int where[LATE_CORPSES];
    unsigned used = 0; /* actor slots this snapshot already shows, bit per slot */

    c->sequence = sequence;
    c->planned = 1;
    c->count = 0;

    memset(current, 0, sizeof(current));
    for (unsigned slot = 0; slot < LATE_SLOTS; ++slot) {
        sentient_t *s = Sentient_ActorBySlot((int)slot);
        if (!s || !s->entity) continue;
        unsigned n = (unsigned)s->entity->s.number;
        if (n < MAX_GENTITIES) current[n >> 3] |= (unsigned char)(1u << (n & 7));
        used |= 1u << slot;
    }

    for (unsigned k = 0; k < LATE_CORPSES; ++k) where[k] = -1;
    for (unsigned i = 0; i < count; ++i) {
        entityState_t *es = &ring[(first + i) % ringSize];
        if (es->eType != ET_ACTOR_CORPSE) continue;
        unsigned k = (unsigned)es->lerp.u.actor.actorSlot;
        if (k < LATE_CORPSES) where[k] = (int)((first + i) % ringSize);
    }

    for (unsigned k = 0; k < LATE_CORPSES; ++k) {
        late_state *st = &c->corpses[k];
        actor_corpse_info_t info;
        if (!ActorCorpseGet(k, &info)) { st->generation = 0; st->state = LATE_NONE; continue; }
        int index = where[k];
        if (index >= 0 && (unsigned)ring[index].number != info.number) index = -1;
        if (st->state == LATE_NONE || st->generation != info.generation) {
            /* First snapshot since the body appeared. C1 builds it in this
             * very snapshot when the previous one carried the actor's record
             * and this one carries the body (46c4f2 -> 42a5c0). */
            st->generation = info.generation;
            st->state = c->actorsValid && bit(c->actors, info.number) && index >= 0 ? LATE_BUILT : LATE_NEEDS;
        }
        if (st->state == LATE_SHOWN) {
            if (index < 0) {
                /* Out of view again before the client took it: C1 drops the
                 * actor record (42be50); start over when it is back. */
                st->state = LATE_NEEDS;
                LATE_TRACE("client %u ent %u corpse %u left view while shown as actor %u\n",
                           client, info.number, k, st->slot);
            } else if (used & (1u << st->slot)) {
                st->state = LATE_NEEDS;
                LATE_TRACE("client %u ent %u corpse %u: actor slot %u was taken by a live actor\n",
                           client, info.number, k, st->slot);
            } else if ((int32_t)(acknowledged - st->since) >= 0) {
                /* The client has a snapshot with the body as an actor; this
                 * one shows the corpse, and C1 makes the corpse record. */
                st->state = LATE_BUILT;
                LATE_TRACE("client %u ent %u corpse %u handed to C1 at seq %u (shown as actor %u since seq %u, ack %u)\n",
                           client, info.number, k, sequence, st->slot, st->since, acknowledged);
                continue;
            }
        }
        if (st->state == LATE_NEEDS && index >= 0) {
            unsigned pick = LATE_SLOTS;
            for (unsigned slot = 0; slot < LATE_SLOTS && pick == LATE_SLOTS; ++slot)
                if (!(used & (1u << slot)) && ActorSlotIdle(slot)) pick = slot;
            if (pick == LATE_SLOTS) continue; /* every slot busy: try next snapshot */
            st->state = LATE_SHOWN;
            st->slot = (unsigned char)pick;
            st->since = sequence;
            LATE_TRACE("client %u ent %u corpse %u not built on the client: shown as actor slot %u from seq %u\n",
                       client, info.number, k, pick, sequence);
        }
        if (st->state != LATE_SHOWN) continue;
        entityState_t *es = &ring[index];
        es->eType = ET_ACTOR;
        es->lerp.u.actor.actorSlot = st->slot;
        used |= 1u << st->slot;
        ActorSlotHold(st->slot);
        late_item *item = &c->items[c->count++];
        item->slot = st->slot; item->corpse = k; item->number = info.number;
        item->generation = info.generation; item->enemyModel = info.enemyModel;
        item->team = info.team; item->entity = info.entity; item->tree = info.tree;
    }
    memcpy(c->actors, current, sizeof(current));
    c->actorsValid = 1;
}

extern "C" int LateCorpseAppendRecords(unsigned client, uint32_t sequence,
    actor_state_t *out, unsigned capacity, unsigned *count,
    int (*build)(actor_state_t *, int, gentity_s *))
{
    if (client >= MAX_CLIENTS || !out || !count || !build) return 0;
    late_client *c = &clients[client];
    if (!c->planned || c->sequence != sequence) return 1;
    for (unsigned i = 0; i < c->count; ++i) {
        const late_item *item = &c->items[i];
        actor_state_t record;
        /* Never fail the recipient's frame over a body: one that cannot be
         * composed is left out (C1 draws it from an empty record, as before). */
        if (*count >= capacity || !build(&record, (int)item->slot, item->entity)) continue;
        /* The corpse no longer has a sentient or an actor: the metadata the
         * live record carried comes from the body. */
        record.enemyModel = 0;
        if (item->enemyModel) {
            int wire = ServerModelToWire(item->enemyModel);
            if (wire > 0) record.enemyModel = (unsigned)wire;
        }
        record.actorTeam = item->team;
        unsigned at = *count, dup = 0;
        for (unsigned j = 0; j < *count; ++j) if (out[j].number == record.number) dup = 1;
        if (dup) continue;
        while (at && out[at - 1].number > record.number) { out[at] = out[at - 1]; --at; }
        out[at] = record;
        ++*count;
    }
    return 1;
}

extern "C" unsigned LateCorpseSources(unsigned client, uint32_t sequence,
    dog_anim_capture_source_t *out, unsigned capacity)
{
    if (client >= MAX_CLIENTS || !out) return 0;
    late_client *c = &clients[client];
    if (!c->planned || c->sequence != sequence) return 0;
    unsigned n = 0;
    for (unsigned i = 0; i < c->count && n < capacity; ++i) {
        out[n].entity = c->items[i].number;
        out[n].generation = c->items[i].generation;
        out[n].tree = c->items[i].tree;
        ++n;
    }
    return n;
}
#endif

/* Resident teardown observes this owner's actual storage after its normal
 * release operation. The check does not mutate or manufacture cleanup. */
static int resident_storage_zero(const void *data,size_t size) {
    const unsigned char *bytes=(const unsigned char*)data;size_t i;
    for(i=0;i<size;++i)if(bytes[i])return 0;
    return 1;
}
extern "C" int PersistentLateCorpsesEmpty(void) {return resident_storage_zero(clients,sizeof(clients));}

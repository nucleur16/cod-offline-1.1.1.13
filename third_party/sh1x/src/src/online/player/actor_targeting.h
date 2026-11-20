#ifndef ACTOR_TARGETING_H
#define ACTOR_TARGETING_H
#include "bot_nav.h"
#include <stdint.h>

/* One directed flood per actor/start/topology/filter state, shared by all
 * candidate players. No borrowed script entity pointers survive a query. */
typedef struct {
    uint64_t topology;
    unsigned filterRevision;
    int start, valid;
    uint32_t nodes[(BOT_NODES + 31) / 32];
} actor_target_cache_t;

int ActorTargetNodeReachable(actor_target_cache_t *cache,
    int start, int destination, unsigned filterRevision,
    bot_edge_filter_t allow, void *context);
#endif

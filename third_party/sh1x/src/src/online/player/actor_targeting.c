/* CoDOL actor target reachability over the same live directed graph the
 * actor mover walks. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "actor_targeting.h"

int ActorTargetNodeReachable(actor_target_cache_t *cache,
    int start, int destination, unsigned filterRevision,
    bot_edge_filter_t allow, void *context)
{
    uint64_t topology = BotNavTopologyRevision();
    int count = BotNavCount();
    if (!cache || start < 0 || start >= count || destination < 0 || destination >= count)
        return 0; /* An absent graph is not evidence of reachability. */
    if (!cache->valid || cache->topology != topology || cache->start != start ||
        cache->filterRevision != filterRevision) {
        cache->valid = 0;
        if (!BotNavReachableNodes(start, cache->nodes,
                sizeof(cache->nodes) / sizeof(cache->nodes[0]), allow, context))
            return 0;
        cache->topology = topology;
        cache->start = start;
        cache->filterRevision = filterRevision;
        cache->valid = 1;
    }
    return (cache->nodes[(unsigned)destination >> 5] &
        (UINT32_C(1) << (destination & 31))) != 0;
}
#endif

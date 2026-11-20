/* Resident process / renewable match policy. Prototype; no engine hooks installed.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_POLICY_H
#define PERSISTENT_POLICY_H
#include <stdint.h>

enum persistent_phase {
    PS_OFF, PS_BOOTING, PS_IDLE, PS_CONFIGURING,
    PS_READY, PS_ACTIVE, PS_FINISHING, PS_RESETTING
};
enum persistent_end {
    PS_GAME_ENDED=1, PS_ABANDONED=2, PS_CANCELLED=3
};
enum persistent_clean {
    PS_VM=1u, PS_ENTITIES=2u, PS_TIMERS=4u,
    PS_PROFILES=8u, PS_REPORTS=16u, PS_ADMISSIONS=32u,
    PS_SNAPSHOTS=64u, PS_BOTS=128u, PS_MATCH_STATICS=256u,
    PS_ALL_CLEAN=511u
};
#pragma pack(push,8)
typedef struct {
    const char *boot, *lease;
    uint64_t epoch;
} persistent_identity_t;
typedef struct {
    unsigned known, humans, bots, world_ready, cleanup_mask;
} persistent_observation_t;
typedef struct {
    enum persistent_phase phase;
    char boot[33], lease[33], completed_lease[33], selection[65], report[65];
    /* i386 uint64_t aligns to4, amd64 to8. Explicit padding keeps the shared
     * engine/policy ABI fixed; pragma pack alone does not raise i386 alignment. */
    uint8_t reserved[7];
    uint64_t epoch, members[18], expiry;
    unsigned member_count, reserved_tail;
} persistent_policy_t;
#pragma pack(pop)

/* Empty boot means default off. Bad nonempty IDs are rejected. */
int PersistentInit(persistent_policy_t *, const char *boot);
int PersistentBootClean(persistent_policy_t *, const persistent_observation_t *);
/* One virgin process only: engine proves no world/VM/client storage has ever
 * existed. This is NOT a cleanup proof for a used world. */
int PersistentBootVacant(persistent_policy_t *, const persistent_observation_t *, int virgin_engine);
int PersistentBind(persistent_policy_t *, const persistent_identity_t *,
    const char *selection, const persistent_observation_t *);
/* Advance an unused assignment epoch while preserving the proved idle world.
 * This is a cancellation tombstone, never a fabricated map/reset completion. */
int PersistentCancelIdle(persistent_policy_t *,const persistent_identity_t *,
    const persistent_observation_t *);
int PersistentWorldReady(persistent_policy_t *, const persistent_identity_t *,
    const char *actual_selection, const persistent_observation_t *);
int PersistentGrant(persistent_policy_t *, const persistent_identity_t *,
    const uint64_t *members, unsigned count, uint64_t expiry, uint64_t now);
int PersistentStart(persistent_policy_t *, const persistent_identity_t *,
    const persistent_observation_t *, uint64_t now);
int PersistentAdmit(const persistent_policy_t *, const persistent_identity_t *,
    uint64_t uid, uint64_t now);
int PersistentEnd(persistent_policy_t *, const persistent_identity_t *,
    enum persistent_end, const persistent_observation_t *);
int PersistentReportDurable(persistent_policy_t *, const persistent_identity_t *,
    const char *report_digest);
int PersistentBeginReset(persistent_policy_t *, const persistent_identity_t *,
    const persistent_observation_t *);
int PersistentResetComplete(persistent_policy_t *, const persistent_identity_t *,
    const persistent_observation_t *);
/* Unexpected/unknown humans always retain simulation; this never pauses them. */
int PersistentSimulate(const persistent_policy_t *, const persistent_observation_t *);
#endif

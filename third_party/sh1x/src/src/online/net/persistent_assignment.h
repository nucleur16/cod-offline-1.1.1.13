/* Private bounded assignment carrier. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_ASSIGNMENT_H
#define PERSISTENT_ASSIGNMENT_H
#include <stddef.h>
#include <stdint.h>
#define PA_LIMIT 4096
#define PA_RULES 26
#pragma pack(push,8)
typedef struct {
    char slot[33], boot[33], lease[33], command[33];
    char profile[8], profile_sha[65], catalogue_sha[65];
    char category[64], map[64], session_map[64], gametype[64];
    char rules[PA_RULES][16], selection[65];
    uint64_t epoch, members[18], expiry;
    unsigned member_count;
} persistent_assignment_t;
typedef struct {
    char slot[33],boot[33],lease[33],request[65];
    uint32_t alignment; /* explicit i386/amd64 uint64 alignment */
    uint64_t epoch,sequence,expiry,members[18];
    unsigned member_count,reserved;
} persistent_grant_t;
#pragma pack(pop)
/* Callback must produce lower-case SHA256 hex plus NUL. */
typedef int (*pa_hash_t)(const void *, size_t, char[65]);
extern const char *const PersistentRuleNames[PA_RULES];
/* Syntax/domain/complete-rule validation, not catalogue or engine acceptance.
 * Destination is unchanged on EVERY failure. Never dispatches console input. */
int PersistentAssignmentDecode(const void *, size_t,
    persistent_assignment_t *, pa_hash_t);
/* Fixed digest-derived basename, owned private regular file, no symlinks,
 * bounded read and full digest verification. POSIX local worker carrier. */
int PersistentAssignmentRead(const char *root, const char *digest,
    persistent_assignment_t *, pa_hash_t);
int PersistentGrantDecode(const void *,size_t,persistent_grant_t *);
int PersistentGrantRead(const char *,const char *,persistent_grant_t *,pa_hash_t);
/* Shared private-file reader. Extension is fixed to assign/catalogue/grant; size is
 * unchanged on rejection. Caller supplies bounded storage and trusted hash. */
int PersistentBlobRead(const char *, const char *, const char *,
    void *, size_t, size_t *, pa_hash_t);
int PersistentCatalogueAllows(const char *, const char *,
    const persistent_assignment_t *, pa_hash_t);
/* Pick an idle world from the same verified profile catalogue. */
int PersistentCatalogueIdle(const char *,const char *,const char *,const char *,
    uint32_t,persistent_assignment_t *,pa_hash_t);
#endif

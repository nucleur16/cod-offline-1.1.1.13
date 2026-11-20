/* sys_ondemand.c - allocation of the large server state (see sys_ondemand.h).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "sys_ondemand.h"

#define SH1X_OD_ENTRY(key) extern void *sh1x_od_##key; extern const unsigned sh1x_od_##key##_size;
#include "sys_ondemand_list.h"
#undef SH1X_OD_ENTRY

typedef struct { void **slot; const unsigned *size; const char *name; } ondemand_t;

static const ondemand_t entries[] = {
#define SH1X_OD_ENTRY(key) { &sh1x_od_##key, &sh1x_od_##key##_size, #key },
#include "sys_ondemand_list.h"
#undef SH1X_OD_ENTRY
};

static int state;          /* 0 not yet, 1 done, -1 failed */
static unsigned allocated;

static void *od_alloc(unsigned size)
{
#ifdef _WIN32
    /* Committed zero pages; untouched pages cost no physical memory. */
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    return calloc(1, size);
#endif
}

int Sys_OnDemandInit(void)
{
    unsigned i;
    if (state) return state > 0 ? 0 : -1;
    for (i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        void *p = od_alloc(*entries[i].size);
        if (!p) {
            state = -1;
            return -1;
        }
        *entries[i].slot = p;
        allocated += *entries[i].size;
    }
    state = 1;
    return 0;
}

unsigned Sys_OnDemandBytes(void) { return allocated; }

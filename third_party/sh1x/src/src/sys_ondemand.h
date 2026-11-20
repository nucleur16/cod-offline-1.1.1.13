/* sys_ondemand.h - large server state allocated when the server starts, not held in the image's .bss.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The CoDOL client links this server into Kernel33.dll (makefile target host-lib). Everything in .bss is part of
 * that image: reserved and charged to commit in every client for its whole run, hosting or not, in a 32-bit
 * address space. The largest arrays (svs alone is 159 MB) are therefore pointers to arrays that
 * Sys_OnDemandInit allocates, zero-filled, at the start of Sys_MainInit, before anything reads them; they are
 * never freed (one server per process).
 *
 * A converted array keeps its declaration's shape and its uses unchanged:
 *
 *     SH1X_ONDEMAND(static, es_ext_t, es_snapshot_ext, [ES_EXT_SNAPSHOT]);
 *     #define snapshot_ext (*sh1x_od_es_snapshot_ext)
 *
 * `snapshot_ext[i]`, `&snapshot_ext[i]`, `sizeof(snapshot_ext)` and `sizeof(snapshot_ext[0])` mean what they meant.
 * Every key must also be listed in sys_ondemand_list.h. The storage class may be empty (an extern object) but not
 * static: the registry in sys_ondemand.c refers to every pointer by name.
 */
#ifndef SYS_ONDEMAND_H
#define SYS_ONDEMAND_H

#define SH1X_ONDEMAND(storage, type, key, dims) \
    storage type (*sh1x_od_##key) dims; \
    extern const unsigned sh1x_od_##key##_size; \
    const unsigned sh1x_od_##key##_size = sizeof(type dims)

#define SH1X_ONDEMAND_EXTERN(type, key, dims) extern type (*sh1x_od_##key) dims

#ifdef __cplusplus
extern "C" {
#endif
/* Allocates every registered array once (later calls return the first result). 0 when done, -1 when an
 * allocation failed (nothing may run then). */
int Sys_OnDemandInit(void);
/* Bytes Sys_OnDemandInit allocated (0 before). */
unsigned Sys_OnDemandBytes(void);
#ifdef __cplusplus
}
#endif

#endif

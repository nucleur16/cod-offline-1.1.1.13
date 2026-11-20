/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef STATS_H
#define STATS_H
#include <stddef.h>
#include <stdint.h>
#define STATS_SIZE 32768u
#define STATS_CHUNK 1240u
#define STATS_COUNT 27u
#define STATS_MASK UINT32_C(0x07ffffff)
void ResetClientStats(unsigned);
int AcceptClientStats(unsigned client, unsigned chunk, const uint8_t *, size_t);
uint32_t ClientStatsMissing(unsigned);
/* The view every profile reader shares: the session copy a script edit made,
 * otherwise the uploaded bytes. NULL until this client's upload is complete. */
const uint8_t *ClientStats(unsigned);
/* The uploaded bytes themselves, ignoring session edits. Upload accounting and
 * any authentication of the stored blob use this, never the shared view. */
const uint8_t *ClientStatsUpload(unsigned);
/* Copy-on-write session view of the completed upload, owned here. It is never
 * uploaded, stored or re-authenticated, and it is dropped by
 * ResetClientStats and by any further upload data for this client.
 * NULL without a complete upload, or when the copy cannot be made. */
uint8_t *ClientStatsWritable(unsigned);
/* Patched 1.1.1.13 playerdata v220/e4fd5218: eight packed 92-byte classes
 * at 0x1511, perk item words 7..11. Slots are one-based (1..5).
 * Requires this client's complete upload and matching schema. Preserves out
 * on failure; signed item values (including -1/0 empty) are retained.
 * Does not validate the profile CRC or interpret marketplace item names. */
int ClientClassPerkItem(unsigned client,unsigned class_index,unsigned slot,int *out);
size_t WriteStatsResponse(uint8_t *, size_t, uint32_t missing);
#endif

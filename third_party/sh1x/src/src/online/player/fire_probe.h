/* Developer fire-stream probe; see fire_probe.c.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef FIRE_PROBE_H
#define FIRE_PROBE_H
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* Called by SV_BotUserMove; 1 when the probe owns this bot slot this frame
 * (the bot's own command is then not generated). `client` is a client_t. */
int FireProbeFrame(void *client);
void RegisterFireProbe(void);
/* The bot slot a running probe drives, or -1; diagnostics that skip bots
 * (the stance trace) still follow the probe's bot. */
int FireProbeSlot(void);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

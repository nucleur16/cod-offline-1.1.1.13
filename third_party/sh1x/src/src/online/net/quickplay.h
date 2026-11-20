/* Continuous Quick Play lifecycle. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef QUICKPLAY_H
#define QUICKPLAY_H
#include <stdint.h>

int QuickPlayEnabled(void);
int QuickPlayPartiesEnabled(void);
const char *QuickPlaySession(void);
unsigned QuickPlayEpoch(void);
int QuickPlayJoinable(void);
void QuickPlayBeginMap(void);
void QuickPlayMapReady(void);
int QuickPlayAdmit(uint64_t uid);
/* Last eligible public bot seat, or -1. Never selects a human or reserved slot. */
int QuickPlayBotSeat(void);
/* The real worker's MP Training profile has twelve native seats, fill12/max11,
 * fixed bots0 and Quick disabled. Playlist1 permits one human; playlist3 or
 * playlist0/combat permits three. Ordinary/Quick connections are unchanged.
 * uid must be the current DirectConnect transaction's verified admission UID;
 * reconnect_slot may exclude only that UID's existing authenticated seat. */
int TrainingAdmit(uint64_t uid, int reconnect_slot);
/* Last reusable public bot for that verified Training arrival, or -1. Never
 * converts an empty/warming bot-only world into a human admission. */
int TrainingBotSeat(uint64_t uid);
/* Called only after normal authenticated DirectConnect attaches this UID.
 * Assignment cache survives a full game-VM/map reset, never a slot/UID reset. */
int QuickPlayBindClient(unsigned client, uint64_t uid);
void QuickPlayClearClient(unsigned client);
/* Native team numbers: 0 absent, 1 TEAM_RED/axis, 2 TEAM_BLUE/allies. */
unsigned QuickPlayClientTeam(unsigned client);
const char *QuickPlayClientGroup(unsigned client);
#endif

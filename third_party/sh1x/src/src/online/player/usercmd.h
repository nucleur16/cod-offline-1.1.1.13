/* Native CoDOL wire commands, independent of the IW3 assembly ABI.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef USERCMD_H
#define USERCMD_H
#include "ol_msg.h"
#pragma pack(push, 8)

#define MAX_USERCMDS 255
#define WEAPON_IDS 1024

typedef struct {
    uint32_t serverTime, buttons;
    uint16_t angles[3], weapon, offHandIndex, altModeWeapon, meleeChargeYaw;
    uint8_t direction, meleeChargeDist, selectedLocation[2], selectedLocationAngle;
    uint8_t remoteAngles[2];
} ol_usercmd_t;

/* Atomic output: no partially decoded command is published on failure. */
int ReadUsercmd(ol_msg_t *m, uint32_t key,
                      const ol_usercmd_t *from, ol_usercmd_t *to);
uint32_t UsercmdKey(uint32_t ack, uint32_t checksum,
                         const uint8_t *command, size_t command_length);
/* Gameplay input filter; preserves the original wire command. */
void FilterUsercmd(const ol_usercmd_t *wire, ol_usercmd_t *game);

/* Separate asset-ID translation. Unknown IDs return -1, never truncate.
 * Loaders register the resolved native weapon after loading its definition. */
void ResetWeaponMap(void);
int MapWeapon(unsigned wire, unsigned engine);
/* Replace both directions together. Entry zero is none; other native entries
 * must identify unique nonzero wire weapons. Failure preserves the old map. */
int ReplaceWeaponMap(const int *wire_by_engine,unsigned count);
/* Bumped by every change to the map above, so a cached producer can tell. */
unsigned WeaponMapGeneration(void);
int WeaponToEngine(unsigned wire);
int WeaponToWire(unsigned engine);
#pragma pack(pop)
#endif

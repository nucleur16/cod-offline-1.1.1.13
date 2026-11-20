/* Combat-training bot offhand use: lethal/tactical grenades thrown through
 * the real C1 offhand buttons, equipment planted at authored claymore spots
 * and route corners, and scripted aim-and-fire for killstreak markers.
 * Adapted from OpenBLOPS server_mp/sv_bot_mp.cpp Bot_UpdateGrenadeThrow and
 * T5 maps/mp/gametypes/_bot.gsc bot_equipment_think.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BOT_OFFHAND_H
#define BOT_OFFHAND_H
#include "usercmd.h"
#pragma pack(push, 8)
struct gentity_s;

/* What the combat AI knows this command, handed over by BotBuildCommand. */
typedef struct {
    int enemy;                 /* entity number, -1 for none */
    int seen;                  /* the enemy was seen in this command */
    int lastSeen;              /* svs.time it was last seen */
    const float *lastKnown;    /* last seen aim point */
    float yawSpeed, pitchSpeed;/* difficulty tracking rates, per second */
    float closeDistance;
    float dt;                  /* seconds since the previous command */
    int busy;                  /* use held, mantling, on a ladder */
    const float *corner, *next;/* next two route points, or NULL */
} bot_offhand_view_t;

void BotOffhandReset(int slot);
void BotOffhandResetAll(void);
/* Runs after movement. Returns 1 when an offhand action owns this command's
 * view angles, offhand/fire buttons and movement. */
int BotOffhandThink(int slot, struct gentity_s *self, ol_usercmd_t *cmd, int now,
                          const bot_offhand_view_t *view);
void RegisterBotOffhandMethods(void);

#pragma pack(pop)
#endif

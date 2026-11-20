/* Opus voice relay for C1 clients (Kernel33 voice.c is the other end).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef VOICE_H
#define VOICE_H
#include "q_shared.h"
#include "sys_net.h"
#include "msg.h"

#ifdef __cplusplus
extern "C" {
#endif

void VoiceInit(void);
/* "v" out-of-band packet from a client (SV_ConnectionlessPacket). The string
 * line "v" has been read; msg->readcount is at the binary payload. */
void VoicePacket(netadr_t *from, msg_t *msg);
/* 1 while clientNum's last voice packet is younger than
 * g_voiceChatTalkingDuration (500 ms): the entity gets C1's talk balloon. */
int VoiceIsTalking(int clientNum);
/* The script's PlayerKilled callback: who killed this victim, for the
 * killcam voice rule. attacker -1 = world/self. */
void VoiceNoteKill(int victim, int attacker);
/* A client slot was freed or reused. */
void VoiceClientReset(int clientNum);

#ifdef __cplusplus
}
#endif
#endif

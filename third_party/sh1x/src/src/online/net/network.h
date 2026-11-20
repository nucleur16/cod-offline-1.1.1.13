/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef NETWORK_H
#define NETWORK_H
#include "q_shared.h"
#include "admission.h"
void NetworkInit(void);
void NetworkReset(void);
int NetworkAuthenticated(netadr_t *);
/* Receive: 1 consumed, 0 plaintext diagnostic mode. Send: -1 diagnostic mode,
 * 0 failed/pending, 1 submitted. Raw handshakes bypass NET_SendPacket. */
int NetworkReceive(netadr_t *, const void *, int);
int NetworkSend(netadr_t *, const void *, int);
/* The lobby admission behind this source's established bdDTLS association:
 * the roster entry whose key the association authenticates with. -> 1/0. */
int NetworkAdmission(const netadr_t *, admission_entry_t *);
#endif

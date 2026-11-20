/* Bridges into the retained IW3 engine; no engine structure layout changes. */
#ifndef GAME_H
#define GAME_H
#include "usercmd.h"
#include "playerstate.h"
#include "msg.h"

int ReadUsercmdMSG(msg_t *msg, uint32_t key,
                         const ol_usercmd_t *from, ol_usercmd_t *to);
/* Whole movement payload after its count byte: zero-based command chain and
 * four-word predicted-origin trailer. Outputs and engine cursors commit only
 * after all commands/trailer validate; failure marks overflow. */
int ReadUsermoveMSG(msg_t *,uint32_t key,unsigned count,ol_usercmd_t *commands,
                          size_t capacity,float origin[3],uint32_t *origin_time);
int ReadUsermoveMSGFrom(msg_t *,uint32_t key,unsigned count,
                              const ol_usercmd_t *base,ol_usercmd_t *commands,
                              size_t capacity,float origin[3],uint32_t *origin_time);
int DefaultWireUsercmd(const struct playerState_s *,ol_usercmd_t *);
/* C1's usermove delta base: C1's MSG_SetDefaultUserCmd 0x59f810 (T5's,
 * minus the lean and sprint bits) over the wire playerState the client last
 * received (NULL -> all zero, C1's cleared cl.snap before its first snapshot). */
void DefaultWireUsercmdFromFields(const player_fields_t *held,ol_usercmd_t *out);
void ToEngineUsercmd(const ol_usercmd_t *wire, usercmd_t *engine,
                           unsigned current_weapon, unsigned current_offhand);
void RecordUsercmd(unsigned client, const ol_usercmd_t *wire);
const ol_usercmd_t *LastUsercmd(unsigned client);
void ResetClientInputCore(unsigned client);
/* C1's 200 ms post-spawn stale-weapon-0 gate; returns 1 when the command was
 * rewritten to keep the held weapon. */
int ApplySpawnWeaponGate(const struct playerState_s *ps, usercmd_t *engine);
void ResetClientInput(unsigned client);
/* The input state this header's own translation unit owns. */
void ResetGameInputCore(void);
/* Core plus the vision, player-builtin, remote-view, turret and HUD-icon
 * resets that live in other areas. */
void ResetGameInput(void);
void RecordActionSlot(unsigned client,unsigned slot,int down);
unsigned ActionSlotButtons(unsigned client);
void ClearActionSlotEdges(unsigned client);
void ResetActionSlots(unsigned client);
#endif

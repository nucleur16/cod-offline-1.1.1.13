#ifndef CLIENTSTATE_EXT_H
#define CLIENTSTATE_EXT_H
#include "playerstate_ext.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Capture beside the snapshot client-ring write, before advancing its cursor.
 * NULL means reset ability state. Never resolve a
 * client number to live gameplay state while converting a retained frame. */
int CaptureClientAbilities(int sequence, const ps_ext_t *state);
/* G_UpdateClientInfo-equivalent publication, before the end-frame spectator
 * branch can replace ps. The published record belongs to the session slot. */
void PublishClientAbilities(unsigned client, const playerState_t *ps);
void ClearPublishedClientAbilities(unsigned client);
/* C1 player method51dbb0: enable native HUD status colours for this session.
 * Original stripped method name unknown; explicit compatibility API only. */
int EnableClientStatusColour(unsigned client);
/* Profile appearance replaces the six-part retained clientState model set.
 * IDs must already be published in the CoDOL model/tag banks. Captured and
 * archived records retain all sixteen attachments independently of live state. */
int PublishClientAppearance(unsigned client,const client_appearance_t *);
/* Script-attached models (wire model and tag IDs, each nonzero, tag < 64),
 * appended after the published character parts in captured and archived
 * records. count 0 clears. Replaces the previous list; 0 on invalid input. */
int PublishClientScriptAttachments(unsigned client,const unsigned *models,const unsigned *tags,unsigned count);
int CapturePublishedClientAbilities(int sequence, unsigned client);
int OverlayCapturedClientAbilities(int sequence, client_state_t *out);
void ClearCapturedClientAbilities(void);
void ClearArchivedClientAbilities(void);
/* Server-internal archive record, immediately after each forced client delta,
 * before its optional playerState. Full and delta archives carry this record
 * even when GetFollowPlayerStateLocal declines to archive a playerState. */
void WriteArchivedClientAbilities(msg_t *, int client, const clientState_t *cache);
void ReadArchivedClientAbilities(msg_t *, const clientState_t *cache);
int CaptureArchivedClientAbilities(int sequence, const clientState_t *cache);
#ifdef __cplusplus
}
#endif
#endif

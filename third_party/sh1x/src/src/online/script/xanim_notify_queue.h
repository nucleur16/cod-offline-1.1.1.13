/* Server notetrack delivery queue (xanim_notify_queue.c).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef XANIM_NOTIFY_QUEUE_H
#define XANIM_NOTIFY_QUEUE_H
#ifdef __cplusplus
extern "C" {
#endif
void XAnimNoteString(unsigned int note);
void XAnimQueueNotify(int entnum, unsigned int classnum, unsigned int notifyName, unsigned int paramCount);
void XAnimNotifyBegin(void);
int XAnimNotifyFlush(void);
#ifdef __cplusplus
}
#endif
#endif

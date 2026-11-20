#ifndef THEATRE_H
#define THEATRE_H
#include "frame_builder.h"
/* Local capture transport, not yet the retail .demo container. Own history
 * and sequence numbers; never acknowledge or rebase a live client's stream. */
int TheatreActive(unsigned slot);
int TheatreStart(unsigned slot, const char *name);
void TheatreStop(unsigned slot, const char *reason);
void TheatreReset(void);
/* Main-thread completion/logging pump; shutdown drains only OS-owned I/O. */
void TheatrePump(void);
int TheatreShutdown(unsigned timeout_ms);
void TheatreAutoFrame(void);
void TheatreFrame(unsigned slot, const msg_t *prefix,
                       const native_frame_t *native);
#endif

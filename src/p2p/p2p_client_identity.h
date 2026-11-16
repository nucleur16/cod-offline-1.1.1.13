/* Main-thread routing intent only; the SDK independently authenticates DW
 * membership. Keep the migrated safe native reader when name_display is off. */
#ifndef P2P_CLIENT_IDENTITY_H
#define P2P_CLIENT_IDENTITY_H
#include <stdint.h>
#if K33_KERNEL_NAME_DISPLAY
static uint64_t KndLocalUID(void); /* exact migrated native reader, defined later */
static unsigned long long P2PLocalUid(void) { return (unsigned long long)KndLocalUID(); }
#else
static unsigned long long P2PLocalUid(void) { return NdLocalUid(); }
#endif
#endif

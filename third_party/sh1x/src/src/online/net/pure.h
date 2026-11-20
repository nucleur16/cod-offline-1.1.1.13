#ifndef PURE_H
#define PURE_H
#include <stdint.h>
#define MAX_PURE_IWDS 1024
/* cp @ <referenced signed checksums> <feed XOR count XOR checksums>.
 * Only archive checksums are validated; this is not fastfile verification. */
int VerifyPureChecksums(int argc, const char *const *argv,
                             const char *loaded_checksums, uint32_t feed);
/* The checksum feed a well-formed cp was built for: aggregate ^ count ^ all
 * checksums. C1 sends two cp per gamestate (measured 2026-09-28): the first
 * still carries the feed of the gamestate before this one, the second the
 * current feed. Returns 0 for a malformed command. */
int PureCommandFeed(int argc, const char *const *argv, uint32_t *feed);
#endif

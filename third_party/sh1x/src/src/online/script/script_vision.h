/* Native GSC vision sets.
 *
 * C1 1.1.1.13 and IW4 share one vision type enum. IW4 still carries the builtin
 * names and thunks them into a single handler with the type index: naked 0
 * (0x5dc950), night 1 (0x5dc960), missilecam 2 (0x5dc970), thermal 3
 * (0x5dc980), pain 4 (0x5dc990). C1 has the same thunks at 0x518070 + 0x20*i
 * over the handler 0x517d70, plus a sixth slot no script names. Its client-side
 * selector 0x47a5d0 confirms the ends of that order: index 4 is the one that
 * warns "Pain visionset was not set in script" and index 0 is the fallback.
 */
#ifndef SCRIPT_VISION_H
#define SCRIPT_VISION_H
#ifdef OL_SERVER

enum {
    VISION_NAKED = 0,
    VISION_NIGHT = 1,
    VISION_MISSILECAM = 2,
    VISION_THERMAL = 3,
    VISION_PAIN = 4,
    VISION_TYPES = 5
};

/* The per-player sets are gclient storage in C1 (client+0x38a0 and
 * client+0x38b8, inside the 0x3a68 bytes its full-gclient clears cover), so
 * this record is dropped wherever a gclient is cleared or a slot changes hands:
 * the ClientSpawn memset, ResetClientInput for a connecting or leaving
 * client, and ResetGameInput for a map or game reset. Nothing is
 * re-published from it; neither engine replays vision sets. */
void ResetPlayerVisionSets(unsigned client);
void ResetAllPlayerVisionSets(void);

/* The retained per-player record. An empty name means the player has no
 * override and follows the global set, which is how C1 0x517f00 resolves a
 * player's current naked vision. NULL is returned for a client or type outside
 * the engine's range. */
const char *PlayerVisionSetName(unsigned client, unsigned type);
int PlayerVisionSetDuration(unsigned client, unsigned type);

#endif
#endif

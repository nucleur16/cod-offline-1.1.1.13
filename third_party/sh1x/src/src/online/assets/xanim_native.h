#ifndef XANIM_NATIVE_H
#define XANIM_NATIVE_H
#include "assets.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
struct XAnimParts;
typedef struct {
    struct XAnimParts *parts;
    void *storage;
    size_t size;
} native_xanim_t;
/* Constructs owned payloads, including translation and quat2 delta tracks.
 * A CoDOL quat4 (delta3D) track is reduced to its yaw twist as the quat2
 * track; a clip carrying both rotation tracks is rejected. Native SL
 * handles are borrowed and must outlive the result. No global publication or
 * compressed-stream compatibility claim. Zero/prior out; atomic replacement. */
int BuildNativeXAnim(const mounted_xanim_t *,const xanim_handles_t *,native_xanim_t *);
void FreeNativeXAnim(native_xanim_t *);
/* Borrowed until full archive unmount, including retired archive generations.
 * The string provider/context must remain valid until then. Atomic output. */
int LoadMountedNativeXAnim(const char *,xanim_string_acquire_t,
    xanim_string_release_t,void *,const struct XAnimParts **);
int LoadEngineNativeXAnim(const char *,const struct XAnimParts **);
/* Diagnostic: resolves the persistent asset through DB lookup and evaluates
 * five finite-transform samples with an identity map; no client pose comparison. */
int CheckEngineXAnim(const char *,unsigned *bones);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

#ifndef ASSET_STREAM_H
#define ASSET_STREAM_H
#include <stddef.h>
#include <stdint.h>
/* Serialized dependency traversal; no GPU objects are created. Positions
 * advance only on success. Unsupported nested assets fail explicitly. */
int AdvanceImageStream(const uint8_t *,size_t,size_t *);
int AdvanceWaterStream(const uint8_t *,size_t,size_t *);
int AdvanceTechniqueSetStream(const uint8_t *,size_t,size_t *);
int AdvanceMaterialStream(const uint8_t *,size_t,size_t *);
int AdvanceModelSurfacesStream(const uint8_t *,size_t,size_t *);
int AdvancePhysicsPresetStream(const uint8_t *,size_t,size_t *);
int AdvancePhysicsMapStream(const uint8_t *,size_t,size_t *);
#endif

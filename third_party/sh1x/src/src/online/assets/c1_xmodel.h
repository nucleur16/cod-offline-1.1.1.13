/* XModel conversion from a C1 record. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef C1_XMODEL_H
#define C1_XMODEL_H
#include "ol_xmodel.h"
#pragma pack(push, 8) /* server.h leaves pack(1) behind (codol-header-packing-hazard) */
#ifdef __cplusplus
extern "C" {
#endif
/* record: C1's XModel (real pointers); header_copy: the same definition's header with serialized words (for the
 * references later steps resolve). text gives a record script string's text. 1 ok, 0 malformed. */
int ReadC1XModelSkeleton(const uint8_t *record,const uint8_t *header_copy,
    const char *(*text)(void *,unsigned),void *context,
    xmodel_skeleton_t *skeleton,xmodel_bone_names_t *names);
int ReadC1XModelGeometry(const uint8_t *record,const uint8_t *header_copy,mounted_model_geometry_t *out);
/* An XModelSurfs record as the inline stream C1's loader reads (owned out->data, free). */
int SerializeC1ModelSurfaces(const uint8_t *record,asset_t *out);
/* 0 when both conversions agree, else a code naming the first differing field. */
int CompareXModelConversions(const mounted_model_skeleton_t *,const mounted_model_geometry_t *,
    const mounted_model_skeleton_t *,const mounted_model_geometry_t *);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

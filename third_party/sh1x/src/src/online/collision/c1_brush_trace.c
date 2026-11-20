/* C1's axial brush phase (CM_TraceThroughBrush 0x5854f0..0x5855e2).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * See c1_brush_trace.h for the evidence. */
#include "c1_brush_trace.h"

#include <math.h>
#include <string.h>

int C1BrushAxialPhase(const float start[3], const float end[3],
                            const float size[3], const float bmins[3],
                            const float bmaxs[3], const int16_t material[2][3],
                            float *enterFrac, float *leaveFrac,
                            unsigned char *allsolid, float leadNormal[3],
                            int *leadMaterial)
{
    int i, lead = 0;

    for(i = 0; i < 3; ++i) {
        /* Trace extents, 0x586ab9..0x586ad7 and 0x588790..0x588863. */
        float delta = end[i] - start[i];
        float mid = (start[i] + end[i]) * 0.5f;
        float hda = fabsf(delta * 0.5f);
        float adelta = fabsf(delta);
        float inv = (double)adelta < (double)9.99999997e-07f ? 0.0f : 1.0f / adelta;
        /* The brush's Bounds as C1 stores them (midPoint, halfSize). */
        float bmid = (bmins[i] + bmaxs[i]) * 0.5f;
        float bhalf = (bmaxs[i] - bmins[i]) * 0.5f;
        float sign, a, b, c, d, f, enter;

        if(i < 2 && delta == 0.0f) {
            /* 0x585584..0x5855bf: a static axis only rejects the brush. */
            float t = mid - bmid;
            float s = size[i] + bhalf;
            if((double)fabsf(t) - (double)s >= 0.125)
                return C1_BRUSH_MISS;
            continue;
        }
        /* 0x5856b9..0x5856cf: travel direction, +1 for a zero delta. */
        sign = delta >= 0.0f ? 1.0f : -1.0f;
        a = (mid - bmid) * sign;
        b = (hda - size[i]) - bhalf;
        c = a + b;
        if(c > 0.0f) {
            /* 0x585712..0x585747: the end leaves the far face. */
            float exitFrac = ((hda + hda) - c) * inv;
            if(exitFrac <= *enterFrac) return C1_BRUSH_MISS;
            *allsolid = 0;
            if(exitFrac < *leaveFrac) *leaveFrac = exitFrac;
        }
        /* 0x58574c..0x5857b6: the start is outside the near face. */
        d = b - a;
        if(d <= 0.0f) continue;
        f = d - 0.125f;
        if(inv == 0.0f) {
            float g = (hda + hda) * *leaveFrac;
            if(g <= f) return C1_BRUSH_MISS;
        }
        enter = f * inv;
        if(enter >= *leaveFrac) return C1_BRUSH_MISS;
        if(*enterFrac < enter) *enterFrac = enter;
        else if(lead) continue;
        /* 0x5857c7..0x58581b: the entered face is mins when moving up the
         * axis, maxs when moving down it; its normal opposes the travel. */
        lead = 1;
        leadNormal[0] = leadNormal[1] = leadNormal[2] = 0.0f;
        leadNormal[i] = -sign;
        *leadMaterial = material[sign >= 0.0f ? 0 : 1][i];
    }
    return lead ? C1_BRUSH_AXIAL_LEAD : C1_BRUSH_NO_LEAD;
}

int __attribute__((cdecl)) C1TraceBrushAxial(const float *tw,
    const unsigned char *brush, float *fracs, unsigned char *allsolid,
    float *leadNormal, int *leadMaterial)
{
    float bmins[3], bmaxs[3];
    int16_t material[2][3];

    memcpy(bmins, brush + 0x00, sizeof(bmins));
    memcpy(bmaxs, brush + 0x10, sizeof(bmaxs));
    memcpy(material, brush + 0x24, sizeof(material));
    return C1BrushAxialPhase(tw, tw + 3, tw + 0x90 / 4, bmins, bmaxs,
                                   (const int16_t (*)[3])material, &fracs[0],
                                   &fracs[1], allsolid, leadNormal, leadMaterial);
}

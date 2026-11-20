/* C1 PM_CmdScale (air/spectator/ladder), spectator wish vector and jump event
 * rule. SPDX-License-Identifier: AGPL-3.0-or-later
 * See pmove_c1_cmdscale.h for the C1 addresses. */
#include "pmove_c1_cmdscale.h"
#include <math.h>

float C1AirCmdScale(int speed, int forwardmove, int rightmove,
                          unsigned pm_flags, float leanf, int pm_type,
                          float spectate_speed_scale)
{
    int f = forwardmove < 0 ? -forwardmove : forwardmove;
    int r = rightmove < 0 ? -rightmove : rightmove;
    int biggest = f > r ? f : r;
    float total, scale;
    if(!biggest) return 0.0f;
    /* 0x40feba: the integer sum of squares goes through a float before
     * C1's sqrt helper 0x401030. */
    total = sqrtf((float)(forwardmove * forwardmove + rightmove * rightmove));
    /* 0x40fef9: fild speed, fimul biggest (exact), over total * 127.0. */
    scale = (float)(speed * biggest) / (total * 127.0f);
    if((pm_flags & 0x40u) || leanf != 0.0f) scale *= 0.4f;
    if(pm_type == 2) scale *= 3.0f;
    if(pm_type == 3) scale *= 6.0f;
    if(pm_type == 5) scale *= spectate_speed_scale;
    return scale;
}

float C1SpectatorWish(const float forward[3], const float right[3],
                            int forwardmove, int rightmove, float scale,
                            float wishdir[3])
{
    float f = (float)forwardmove, r = (float)rightmove, length;
    int i;
    for(i = 0; i < 3; ++i)
        wishdir[i] = scale == 0.0f ? 0.0f : forward[i] * f * scale + right[i] * r;
    length = sqrtf(wishdir[0] * wishdir[0] + wishdir[1] * wishdir[1] +
                   wishdir[2] * wishdir[2]);
    if(length != 0.0f) {
        float inv = 1.0f / length;
        wishdir[0] *= inv; wishdir[1] *= inv; wishdir[2] *= inv;
    }
    return length;
}

int C1JumpEventParm(unsigned pm_flags, unsigned sflags, unsigned *parm)
{
    unsigned surface;
    if(pm_flags & 0x8u) { if(parm) *parm = 21u; return 1; }
    if(sflags & 0x2000u) return 0;
    surface = (sflags >> 20) & 31u;
    if(!surface) return 0;
    if(parm) *parm = surface;
    return 1;
}

void C1FoliageBox(const float hull_mins[3], const float hull_maxs[3],
                        float mins[3], float maxs[3])
{
    volatile float mid[3], half[3], lift;
    unsigned i;
    for(i = 0; i < 3; ++i) {
        mid[i] = (hull_mins[i] + hull_maxs[i]) * 0.5f;
        half[i] = (hull_maxs[i] - hull_mins[i]) * 0.5f;
    }
    /* 0x413c21..0x413c66: 0.75 * each of the six, stored as floats. */
    for(i = 0; i < 3; ++i) { mid[i] = mid[i] * 0.75f; half[i] = half[i] * 0.75f; }
    /* 0x413c69..0x413c86: (half.z + mid.z) from the UNSCALED hull, times
     * 0.075, added to both scaled z values. */
    lift = ((hull_maxs[2] - hull_mins[2]) * 0.5f + (hull_mins[2] + hull_maxs[2]) * 0.5f) *
           0.07500000298023224f;
    mid[2] = mid[2] + lift;
    half[2] = half[2] + lift;
    for(i = 0; i < 3; ++i) { mins[i] = mid[i] - half[i]; maxs[i] = mid[i] + half[i]; }
}

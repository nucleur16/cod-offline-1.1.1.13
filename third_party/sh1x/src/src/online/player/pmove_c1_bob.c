/* C1 PM_Footsteps walking bob (see the header).
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmove_c1_bob.h"

/* 0xc40938: sixteen floats indexed ads + 2 * (stance + 4 * backward). */
static const float c1_bob_table[16] = {
    0.335f, 0.305f,   /* stand */
    0.25f,  0.24f,    /* prone */
    0.34f,  0.315f,   /* crouch */
    0.25f,  0.24f,    /* last stand */
    0.36f,  0.325f,   /* stand, backward */
    0.25f,  0.24f,    /* prone, backward */
    0.34f,  0.315f,   /* crouch, backward */
    0.25f,  0.24f     /* last stand, backward */
};

float C1BobConstant(int anim_index, int ads, int sprinting)
{
    if(anim_index < 0 || anim_index > 7) anim_index = 0;
    /* 0x413ac1..0x413ac9: 0xc3e67c (0.5) only for a forward-standing sprint. */
    if(!anim_index && sprinting) return 0.5f;
    return c1_bob_table[(ads ? 1 : 0) + 2 * anim_index];
}

float C1BobDivisor(const c1_bob_input_t *in)
{
    /* volatile: each product is a float store in C1 (fstp [ebp-4]). The
     * constants are C1's doubles (0xc84b00..0xc84b18, 0xc846d8, 0xc845a0);
     * the x87 multiplies by them exactly and rounds the product to float. */
    volatile float d;
    if(!in) return 0.0f;
    d = (float)in->speed;
    if(in->forwardmove) {
        if(in->rightmove) {
            d = (float)((double)d * 0.9250000044703484);  /* 0x4136af */
            if(in->forwardmove < 0)
                d = (float)((double)d * 0.8499999940395355);  /* 0x4136bf */
        } else if(in->forwardmove < 0) {
            d = (float)((double)d * 0.699999988079071);   /* 0x4136ce */
        }
    } else if(in->rightmove) {
        d = (float)((double)d * 0.8500000089406967);      /* 0x4136df */
    }
    if(in->ads) d = (float)((double)d * 0.4000000059604645);   /* 0x4136f1 */
    else if(in->sprinting) d = d * 1.5f;                       /* 0x413702 */
    if(in->has_weapon) {
        if(in->move_speed_scale > 0.0f && !in->pmf_ads)
            d = d * in->move_speed_scale;           /* 0x413734 */
        else if(in->ads_move_speed_scale > 0.0f)
            d = d * in->ads_move_speed_scale;       /* 0x413749 */
    }
    d = in->stance_scale * d;                       /* 0x41375d */
    return d;
}

int C1BobCycle(const c1_bob_input_t *in)
{
    volatile float divisor, rate, cycle;
    if(!in) return -1;
    divisor = C1BobDivisor(in);
    if(!(divisor > 0.0f)) return -1;
    rate = in->xyspeed / divisor;
    rate = rate * C1BobConstant(in->anim_index, in->ads, in->sprinting);
    cycle = rate * (float)in->msec;
    cycle = cycle + (float)in->old_cycle;
    return ((int)cycle) & 255;
}

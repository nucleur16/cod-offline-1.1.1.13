/* AngleVectors as C1 0x61cbb0 computes it.
 *
 * C1 converts each angle with the FLOAT pi/180 (0.0174532924, stored as a
 * double at 0xc842f8), the product rounded once to a float by the x87 at 24
 * bits, then fsincos and float stores. The retained double pi/180 picks the
 * neighbouring float angle for about 9% of the 65536 command yaws and a
 * different sine or cosine for about 8%, so the server's wish direction was
 * one ulp off the client's on those yaws and its origin drifted from the
 * prediction while moving. Expected bits below were computed from C1's
 * arithmetic (float(a * 0.0174532924f), correctly rounded float sin/cos);
 * the "old" values are what the double pi/180 produced.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "q_math.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

qboolean Assert_MyHandler(const char *exp, const char *filename, int line,
                          const char *function, const char *fmt, ...)
{
    fprintf(stderr, "assert %s at %s:%d (%s)\n", exp, filename, line, function);
    (void)fmt;
    return qfalse;
}

static uint32_t bits(float v) { uint32_t u; memcpy(&u, &v, 4); return u; }

static void check(float yaw, uint32_t cos_bits, uint32_t sin_bits)
{
    vec3_t angles = { 0.0f, yaw, 0.0f }, forward, right, up;
    AngleVectors(angles, forward, right, up);
    assert(bits(forward[0]) == cos_bits);
    assert(bits(forward[1]) == sin_bits);
    assert(forward[2] == 0.0f || bits(forward[2]) == 0x80000000u);
    /* right = (sy, -cy, 0) and up = (0, 0, 1) for a level view. */
    assert(bits(right[0]) == sin_bits && bits(-right[1]) == cos_bits);
    assert(up[2] == 1.0f);
}

int main(void)
{
    /* Command yaws k * 360 / 65536 for k = 390, 1397, 1833, 2219. */
    check(2.142333984375f,  0x3f7fd232u, 0x3d191df0u); /* old 0x3f7fd231 / 0x3d191df1 */
    check(7.6739501953125f, 0x3f7db510u, 0x3e08bd8fu); /* old 0x3f7db50f / 0x3e08bd90 */
    check(10.0689697265625f,0x3f7c0e9eu, 0x3e33078eu); /* old 0x3f7c0e9d / 0x3e33078f */
    check(12.1893310546875f,0x3f7a3a82u, 0x3e5835d0u); /* old 0x3f7a3a81 / 0x3e5835d1 */
    /* Exact quarter turns stay exact. */
    {
        vec3_t angles = { 0.0f, 90.0f, 0.0f }, forward;
        AngleVectors(angles, forward, NULL, NULL);
        assert(forward[1] == 1.0f);
    }
    puts("PASS: AngleVectors converts with C1's float pi/180 and one rounding (0x61cbb0), matching its sine and cosine bit for bit on yaws the double constant missed");
    return 0;
}

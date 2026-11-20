/* C1.exe sub_5A0510 codecs, from alpha0022/entity_codec.py's active encoder.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "delta.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static double real(uint32_t word) { float f; memcpy(&f, &word, 4); return f; }
static uint32_t mask(unsigned n) { return n == 32 ? UINT32_MAX : (UINT32_C(1) << n) - 1; }
static void compressed(ol_msg_t *m, uint32_t v, unsigned n)
{
    unsigned low = n & 7;
    WriteBits(m, v & mask(low), low); v >>= low; n -= low;
    while (n) { WriteByte(m, v); v >>= 8; n -= 8; }
}
static double round_even(double x)
{
    double base = floor(x), part = x - base;
    return part > 0.5 || (part == 0.5 && fmod(base, 2.0) != 0) ? base + 1 : base;
}
int SetDeltaPositionBase(delta_context_t *context,const float center[3])
{
    int32_t base[3]; unsigned i;
    if(!context || !center) return 0;
    for(i=0;i<3;++i) {
        double value=round_even((double)center[i]+0x1p-30);
        if(!isfinite(value) || value<INT32_MIN || value>INT32_MAX) return 0;
        base[i]=(int32_t)value;
    }
    memcpy(context->position_base,base,sizeof(base)); return 1;
}
static void angle(ol_msg_t *m, double value)
{
    if (!isfinite(value)) { m->failed = 1; return; }
    WriteShort(m, (uint32_t)fmod(fmod(round_even(value * 182.04444885253906),
                                         65536.0) + 65536.0, 65536.0));
}
static int integer(ol_msg_t *m, int bits, uint32_t from, uint32_t to, int presence)
{
    unsigned n;
    int64_t value = (int32_t)to;
    if (!bits || bits < -32 || bits > 32) return 0;
    n = bits < 0 ? -bits : bits;
    if (bits > 0) { if (to > mask(n)) return 0; }
    else if (value < -(INT64_C(1) << (n - 1)) || value >= (INT64_C(1) << (n - 1))) return 0;
    if (presence) {
        WriteBits(m, !!to, 1);
        if (!to) return !m->failed;
    }
    compressed(m, (from ^ to) & mask(n), n); return !m->failed;
}
static void trimmed(ol_msg_t *m, uint32_t from, uint32_t to, unsigned n, int bias)
{
    double f = real(from), t = real(to), truncated;
    if (!isfinite(t) || !isfinite(f)) { m->failed = 1; return; }
    truncated = trunc(t);
    if (t == truncated && t >= -bias && t < bias && trunc(f) >= -bias && trunc(f) < bias) {
        uint32_t v = ((uint32_t)((int64_t)trunc(f) + bias) ^
                      (uint32_t)((int64_t)truncated + bias)) & mask(n);
        WriteBits(m, 0, 1); compressed(m, v, n);
    } else { WriteBits(m, 1, 1); WriteLong(m, from ^ to); }
}

static unsigned reach_clamps;
static char reach_clamp_note[160];
unsigned DeltaReachClamps(void) { return reach_clamps; }
const char *DeltaReachClampNote(void) { return reach_clamp_note; }
int ReachDeltaValue(int bits, uint32_t from, uint32_t to,
                          const delta_context_t *context, uint32_t *held)
{
    uint8_t buffer[32];
    ol_msg_t msg;
    delta_context_t ctx;
    double value;
    int axis = -1;
    float result;
    uint32_t out = to;
    if (!context || !held) return 0;
    ctx = *context;
    if (bits == -92 || bits == -83) axis = 0;
    if (bits == -91 || bits == -82) axis = 1;
    if (bits == -90 || bits == -81) axis = 2;
    if (axis >= 0 && isfinite(real(to))) {
        /* C1's position codec (MSG_ReadOriginFloat shape) carries only a
         * 16-bit window around the map center; there is no wider arm. A
         * finite value outside it (a corpse falling off mp_afghan reached
         * z -1.68e9) is held, and later serialized, as the nearest window
         * edge instead of refusing the snapshot: refusing failed every
         * frame for that client until the entity went away. The base value
         * stays strict: held bases are in the window by construction. */
        double lo = (double)ctx.position_base[axis] - 32768, hi = lo + 65535;
        value = floor(real(to) + .5);
        if (value < lo || value > hi) {
            result = (float)(value < lo ? lo : hi); memcpy(&to, &result, 4);
            ++reach_clamps;
            snprintf(reach_clamp_note, sizeof(reach_clamp_note),
                     "codec %d axis %d value %g held as %g (window %g..%g)",
                     bits, axis, real(out), (double)result, lo, hi);
            out = to;
        }
    }
    MsgWriteInit(&msg, buffer, sizeof(buffer));
    if (!WriteDeltaValue(&msg, bits, from, to, &ctx)) return 0;
    if (axis >= 0) {
        double lo = (double)ctx.position_base[axis] - 32768, hi = lo + 65535;
        value = floor(real(to) + .5);
        if (value < lo) value = lo;
        if (value > hi) value = hi;
        result = (float)value; memcpy(&out, &result, 4);
    } else if (bits == -100 || bits == -79 || bits == -87) {
        value = fmod(fmod(round_even(real(to)*182.04444885253906), 65536.)+65536., 65536.);
        if (value >= 32768.) value -= 65536.; /* MSG_ReadShort sign extends */
        result = (float)(value * .0054931640625); memcpy(&out, &result, 4);
    } else if (bits == -86) {
        result = (float)(round_even(real(to)*10.) / 10.); memcpy(&out, &result, 4);
    } else if (bits == -85) {
        uint32_t frgb = from & 0xffffffu, trgb = to & 0xffffffu;
        unsigned fa = from >> 24, ta = to >> 24;
        if (!(frgb == trgb && ((fa && ta == 0) || (!fa && ta == 255))))
            out = trgb | ((uint32_t)(ta & 0xf8u) << 24);
    } else if ((bits == -89 || bits == -99) && to == UINT32_C(0x80000000)) {
        /* The trimmed-integer arm does not preserve negative zero. */
        out = 0;
    }
    *held = out; return 1;
}
static int position(ol_msg_t *m, uint32_t from, uint32_t to, int axis,
                    delta_context_t *ctx)
{
    double f = real(from), t = real(to), lo = (double)ctx->position_base[axis] - 32768;
    double hi = lo + 65535;
    if (!isfinite(f) || !isfinite(t)) return 0;
    f = floor(f + 0.5); t = floor(t + 0.5);
    if (f < lo || f > hi || t < lo || t > hi) {
        if (!ctx->clamp_positions) return 0;
        if (f < lo) { f = lo; ++ctx->clamped; }
        if (f > hi) { f = hi; ++ctx->clamped; }
        if (t < lo) { t = lo; ++ctx->clamped; }
        if (t > hi) { t = hi; ++ctx->clamped; }
    }
    WriteBits(m, 1, 1); /* proven wide arm */
    WriteBits(m, (uint32_t)(f - lo) ^ (uint32_t)(t - lo), 16);
    return !m->failed;
}

int WriteDeltaValue(ol_msg_t *m, int bits, uint32_t from, uint32_t to,
                          delta_context_t *ctx)
{
    int64_t delta;
    unsigned width;
    double t;
    if (!m || !ctx || m->failed) return 0;
    switch (bits) {
    case -92: case -83: if (!position(m, from, to, 0, ctx)) goto fail; break;
    case -91: case -82: if (!position(m, from, to, 1, ctx)) goto fail; break;
    case -90: case -81: if (!position(m, from, to, 2, ctx)) goto fail; break;
    case -100: case -79:
        WriteBits(m, 0, 2); angle(m, real(to)); break;
    case -87: angle(m, real(to)); break;
    case -97: case -74: case -72: case -70:
        delta = (int64_t)ctx->server_time - to;
        WriteBits(m, delta < 0 || delta > 255, 1);
        if (delta >= 0 && delta <= 255) WriteBits(m, delta, 8);
        else WriteLong(m, to);
        break;
    case -69:
        delta = (int64_t)ctx->server_time - to;
        WriteBits(m, delta != 0, 1);
        if (!delta) break;
        if (delta > 0 && delta % 50 == 0 && delta / 50 <= 7) {
            WriteBits(m, 0, 1); WriteBits(m, delta / 50, 3);
        } else {
            WriteBits(m, 1, 1);
            if (delta > 0 && delta % 50 == 0 && delta / 50 <= 255) {
                WriteBits(m, 0, 1); WriteBits(m, delta / 50, 8);
            } else { WriteBits(m, 1, 1); WriteLong(m, to); }
        }
        break;
    case -80: case -78: case -73: case -71: WriteLong(m, to); break;
    case -88: WriteLong(m, from ^ to); break;
    case -77: if (to > 255) goto fail; compressed(m, to, 8); break;
    case -93: case -94: case -84:
        width = bits == -93 ? 11 : bits == -94 ? 8 : 7;
        if (to > mask(width)) goto fail;
        if (bits == -94) WriteByte(m, to); else WriteBits(m, to, width);
        break;
    case -65:
        if (to % 100 || to / 100 >= 1024) goto fail;
        WriteBits(m, to / 100, 10); break;
    case -86:
        t = round_even(real(to) * 10.0);
        if (!isfinite(t) || t < 0 || t > 63) goto fail;
        WriteBits(m, (uint32_t)t, 6); break;
    case -96:
        if (to > 2047) goto fail;
        WriteBits(m, to == 2046, 1);
        if (to == 2046) break;
        WriteBits(m, to == 0, 1);
        if (!to) break;
        WriteBits(m, to & 7, 3); WriteByte(m, to >> 3); break;
    case -98:
        if (to > 0x3fffffff) goto fail;
        WriteBits(m, 1, 1); WriteBits(m, to, 30); break;
    case -75:
        delta = (int64_t)to - from;
        if (delta >= 1 && delta <= 32) {
            WriteBits(m, 1, 1); WriteBits(m, delta - 1, 5);
        } else {
            if (to > 2047) goto fail;
            WriteBits(m, 0, 1); compressed(m, (from ^ to) & 2047, 11);
        }
        break;
    case -76:
        delta = (int64_t)(int32_t)to - (int32_t)from;
        if (delta >= -16 && delta <= 15) {
            WriteBits(m, 1, 1); WriteBits(m, delta + 16, 5);
        } else {
            WriteBits(m, 0, 1);
            if (!integer(m, -8, from, to, 0)) goto fail;
        }
        break;
    case -85: {
        /* C1 reader 0x5a085b (alpha0022 hudelem.md 3.2): bit 1 = same RGB
         * with alpha toggled to 0 or 255; otherwise a bit says whether RGB
         * follows as three bytes, then alpha as 5 bits (alpha >> 3). Alpha
         * is quantised to multiples of 8 on that arm; 255 is only reachable
         * through the toggle, which ReachDeltaValue mirrors. */
        uint32_t frgb = from & 0xffffffu, trgb = to & 0xffffffu;
        unsigned fa = from >> 24, ta = to >> 24;
        if (frgb == trgb && ((fa && ta == 0) || (!fa && ta == 255))) { WriteBits(m, 1, 1); break; }
        WriteBits(m, 0, 1);
        if (frgb == trgb) WriteBits(m, 1, 1);
        else {
            WriteBits(m, 0, 1);
            WriteByte(m, to); WriteByte(m, to >> 8); WriteByte(m, to >> 16);
        }
        WriteBits(m, ta >> 3, 5); break;
    }
    case 0:
        if (real(to) == 0.0) { WriteBits(m, 0, 1); WriteBits(m, to >> 31, 1); break; }
        WriteBits(m, 1, 1); trimmed(m, from, to, 13, 4096); break;
    case -89: trimmed(m, from, to, 13, 4096); break;
    case -99:
        WriteBits(m, to != 0, 1);
        if (to) trimmed(m, from, to, 12, 2048);
        break;
    default: if (!integer(m, bits, from, to, 1)) goto fail;
    }
    return !m->failed;
fail:
    m->failed = 1; return 0;
}

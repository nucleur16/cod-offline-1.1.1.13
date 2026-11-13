/*
 * dw_crypto.c - Tiger/192 and 3DES-CBC for the offline Demonware core (docs/OFFLINE_LOBBY.md).
 *
 * Port of the Python lobby's demonware/a22_tiger.py (Tiger1, pad 0x01, as bdSDK uses it) and of the
 * pycryptodome DES3.MODE_CBC calls in demonware/dw_wire.py / dw_auth.py. Both are plain public
 * algorithms; nothing here depends on C1 or on Windows.
 */
#include <string.h>
#include "dw.h"
#include "dw_tiger_sboxes.h"

typedef unsigned long long u64;

static void TigerRound(u64 *a, u64 *b, u64 *c, u64 x, unsigned mul)
{
    *c ^= x;
    *a -= kTigerT1[*c & 0xFF] ^ kTigerT2[(*c >> 16) & 0xFF] ^ kTigerT3[(*c >> 32) & 0xFF] ^ kTigerT4[(*c >> 48) & 0xFF];
    *b += kTigerT4[(*c >> 8) & 0xFF] ^ kTigerT3[(*c >> 24) & 0xFF] ^ kTigerT2[(*c >> 40) & 0xFF] ^ kTigerT1[(*c >> 56) & 0xFF];
    *b *= mul;
}

static void TigerPass(u64 *a, u64 *b, u64 *c, const u64 *x, unsigned mul)
{
    TigerRound(a, b, c, x[0], mul);
    TigerRound(b, c, a, x[1], mul);
    TigerRound(c, a, b, x[2], mul);
    TigerRound(a, b, c, x[3], mul);
    TigerRound(b, c, a, x[4], mul);
    TigerRound(c, a, b, x[5], mul);
    TigerRound(a, b, c, x[6], mul);
    TigerRound(b, c, a, x[7], mul);
}

static void TigerSchedule(u64 *x)
{
    x[0] -= x[7] ^ 0xA5A5A5A5A5A5A5A5ULL;
    x[1] ^= x[0];
    x[2] += x[1];
    x[3] -= x[2] ^ ((~x[1]) << 19);
    x[4] ^= x[3];
    x[5] += x[4];
    x[6] -= x[5] ^ ((~x[4]) >> 23);
    x[7] ^= x[6];
    x[0] += x[7];
    x[1] -= x[0] ^ ((~x[7]) << 19);
    x[2] ^= x[1];
    x[3] += x[2];
    x[4] -= x[3] ^ ((~x[2]) >> 23);
    x[5] ^= x[4];
    x[6] += x[5];
    x[7] -= x[6] ^ 0x0123456789ABCDEFULL;
}

static void TigerCompress(u64 *state, const unsigned char *block)
{
    u64 a = state[0], b = state[1], c = state[2], x[8];
    for (int i = 0; i < 8; i++) {
        u64 v = 0;
        for (int k = 7; k >= 0; k--) v = (v << 8) | block[i * 8 + k];
        x[i] = v;
    }
    TigerPass(&a, &b, &c, x, 5);
    TigerSchedule(x);
    TigerPass(&c, &a, &b, x, 7);
    TigerSchedule(x);
    TigerPass(&b, &c, &a, x, 9);
    state[0] = a ^ state[0];
    state[1] = b - state[1];
    state[2] = c + state[2];
}

void DwTiger(const void *data, size_t n, unsigned char out[24])
{
    u64 state[3] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, 0xF096A5B4C3B2E187ULL };
    const unsigned char *p = (const unsigned char *)data;
    size_t left = n;
    unsigned char tail[128];
    while (left >= 64) { TigerCompress(state, p); p += 64; left -= 64; }
    memset(tail, 0, sizeof(tail));
    memcpy(tail, p, left);
    tail[left] = 0x01;
    size_t total = left + 1 <= 56 ? 64 : 128;
    u64 bits = (u64)n * 8;
    for (int i = 0; i < 8; i++) tail[total - 8 + i] = (unsigned char)(bits >> (8 * i));
    for (size_t off = 0; off < total; off += 64) TigerCompress(state, tail + off);
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < 8; k++) out[i * 8 + k] = (unsigned char)(state[i] >> (8 * k));
}

/* ---- DES (FIPS 46-3 tables) ---------------------------------------------------------------- */

static const unsigned char kIP[64] = {
    58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4, 62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17, 9, 1, 59, 51, 43, 35, 27, 19, 11, 3, 61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7 };
static const unsigned char kFP[64] = {
    40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31, 38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27, 34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9, 49, 17, 57, 25 };
static const unsigned char kE[48] = {
    32, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9, 8, 9, 10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1 };
static const unsigned char kP[32] = {
    16, 7, 20, 21, 29, 12, 28, 17, 1, 15, 23, 26, 5, 18, 31, 10, 2, 8, 24, 14, 32, 27, 3, 9, 19, 13, 30, 6, 22, 11, 4, 25 };
static const unsigned char kPC1[56] = {
    57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18, 10, 2, 59, 51, 43, 35, 27, 19, 11, 3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15, 7, 62, 54, 46, 38, 30, 22, 14, 6, 61, 53, 45, 37, 29, 21, 13, 5, 28, 20, 12, 4 };
static const unsigned char kPC2[48] = {
    14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10, 23, 19, 12, 4, 26, 8, 16, 7, 27, 20, 13, 2,
    41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32 };
static const unsigned char kShifts[16] = { 1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1 };
static const unsigned char kS[8][64] = {
    { 14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7, 0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
      4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0, 15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13 },
    { 15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10, 3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
      0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15, 13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9 },
    { 10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8, 13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
      13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7, 1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12 },
    { 7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15, 13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
      10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4, 3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14 },
    { 2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9, 14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
      4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14, 11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3 },
    { 12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11, 10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
      9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6, 4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13 },
    { 4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1, 13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
      1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2, 6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12 },
    { 13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7, 1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
      7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8, 2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11 } };

static u64 Permute(u64 v, const unsigned char *table, int n, int width)
{
    u64 out = 0;
    for (int i = 0; i < n; i++) out = (out << 1) | ((v >> (width - table[i])) & 1);
    return out;
}

static void DesKey(const unsigned char key[8], u64 sub[16])
{
    u64 k = 0;
    for (int i = 0; i < 8; i++) k = (k << 8) | key[i];
    u64 cd = Permute(k, kPC1, 56, 64);
    unsigned c = (unsigned)(cd >> 28) & 0x0FFFFFFF, d = (unsigned)cd & 0x0FFFFFFF;
    for (int r = 0; r < 16; r++) {
        c = ((c << kShifts[r]) | (c >> (28 - kShifts[r]))) & 0x0FFFFFFF;
        d = ((d << kShifts[r]) | (d >> (28 - kShifts[r]))) & 0x0FFFFFFF;
        sub[r] = Permute(((u64)c << 28) | d, kPC2, 48, 56);
    }
}

static void DesBlock(const u64 sub[16], int decrypt, unsigned char block[8])
{
    u64 v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | block[i];
    v = Permute(v, kIP, 64, 64);
    unsigned l = (unsigned)(v >> 32), r = (unsigned)v;
    for (int round = 0; round < 16; round++) {
        u64 e = Permute(r, kE, 48, 32) ^ sub[decrypt ? 15 - round : round];
        unsigned f = 0;
        for (int s = 0; s < 8; s++) {
            unsigned six = (unsigned)(e >> (42 - 6 * s)) & 0x3F;
            unsigned row = ((six >> 4) & 2) | (six & 1), col = (six >> 1) & 0xF;
            f = (f << 4) | kS[s][row * 16 + col];
        }
        f = (unsigned)Permute(f, kP, 32, 32);
        unsigned t = l ^ f;
        l = r;
        r = t;
    }
    v = Permute(((u64)r << 32) | l, kFP, 64, 64);
    for (int i = 7; i >= 0; i--) { block[i] = (unsigned char)v; v >>= 8; }
}

/* EDE3, CBC. `n` must be a multiple of 8; in place when out == in. */
void DwDes3Cbc(const unsigned char key[24], const unsigned char iv[8], int decrypt,
               const unsigned char *in, unsigned char *out, size_t n)
{
    u64 k1[16], k2[16], k3[16];
    unsigned char chain[8], block[8], save[8];
    DesKey(key, k1);
    DesKey(key + 8, k2);
    DesKey(key + 16, k3);
    memcpy(chain, iv, 8);
    for (size_t off = 0; off + 8 <= n; off += 8) {
        memcpy(block, in + off, 8);
        if (!decrypt) {
            for (int i = 0; i < 8; i++) block[i] ^= chain[i];
            DesBlock(k1, 0, block);
            DesBlock(k2, 1, block);
            DesBlock(k3, 0, block);
            memcpy(chain, block, 8);
        } else {
            memcpy(save, block, 8);
            DesBlock(k3, 1, block);
            DesBlock(k2, 0, block);
            DesBlock(k1, 1, block);
            for (int i = 0; i < 8; i++) block[i] ^= chain[i];
            memcpy(chain, save, 8);
        }
        memcpy(out + off, block, 8);
    }
}

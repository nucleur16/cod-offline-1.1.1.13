/*
 * dw_wire.cpp - the Demonware wire codec: tagged values and the 3DES lobby frame.
 * Port of demonware/dw_wire.py (CODOL-EMU 1.1.1.13 Demonware emulator); its comments carry the
 * C1.exe provenance of every constant.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include "dw.h"

namespace dw {

/* The lobby session key is ticket offset 97, which the auth reply chooses: bytes(range(24)). */
static const unsigned char kSessionKey[24] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
static const uint32_t kLobbyPlaintextMagic = 0xDEADBEEF; /* cmp at C1 0x9CE410 */

std::string Fmt(const char *fmt, ...)
{
    char stack[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);
    if (n < 0) return std::string();
    if ((size_t)n < sizeof(stack)) return std::string(stack, (size_t)n);
    std::string out((size_t)n + 1, '\0');
    va_start(ap, fmt);
    vsnprintf(&out[0], out.size(), fmt, ap);
    va_end(ap);
    out.resize((size_t)n);
    return out;
}

std::string Hex(const void *data, size_t n)
{
    static const char digits[] = "0123456789abcdef";
    const unsigned char *p = (const unsigned char *)data;
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { out += digits[p[i] >> 4]; out += digits[p[i] & 15]; }
    return out;
}

/* Log-safe stand-in for a security key (the Python lobby prints sha256[:12]; any stable digest
 * serves, and Tiger is already here). */
std::string KeyFp(const Bytes &key)
{
    unsigned char digest[24];
    DwTiger(key.data(), key.size(), digest);
    return "tiger:" + Hex(digest, 6);
}

uint32_t Crc32(const void *data, size_t n)
{
    return (uint32_t)crc32(0L, (const Bytef *)data, (uInt)n);
}

static uint64_t ReadLe(const Bytes &b, size_t at, size_t n)
{
    uint64_t v = 0;
    for (size_t i = 0; i < n && at + i < b.size(); i++) v |= (uint64_t)(unsigned char)b[at + i] << (8 * i);
    return v;
}

static int TypeSize(int t)
{
    switch (t) {
    case 1: case 2: case 3: return 1;
    case 4: case 5: case 6: return 2;
    case 7: case 8: case 13: return 4;
    case 9: case 10: case 14: return 8;
    }
    return 0;
}

Vals ParseValues(const Bytes &b, uint8_t pad)
{
    Vals out;
    size_t i = 0, n = b.size();
    while (i < n) {
        if ((unsigned char)b[i] == pad) { /* the (byte)messageCount padding run ends the body */
            size_t k = i;
            while (k < n && (unsigned char)b[k] == pad) k++;
            if (k == n) break;
        }
        int t = (unsigned char)b[i++];
        if (t == 0) { out.push_back(Val(0, (uint64_t)0)); continue; }
        if (t == 16 || t == 17 || t == 18) {
            size_t e = b.find('\0', i);
            if (e == Bytes::npos) { out.push_back(Val(t, b.substr(i))); break; }
            out.push_back(Val(t, b.substr(i, e - i)));
            i = e + 1;
        } else if (t == 19) {
            /* readBlob (sub_9BA070): the length is a TAGGED u32. */
            if (i < n && b[i] == 8) i++;
            size_t len = (size_t)ReadLe(b, i, 4);
            i += 4;
            out.push_back(Val(t, i <= n ? b.substr(i, len) : Bytes()));
            i += len;
        } else if (t == 0x6C) {
            /* [0x6c][0x08][u32 byteLen][u32 count][count x u32]: service 28 (GROUP). */
            if (i < n && b[i] == 8) i++;
            size_t nbytes = (size_t)ReadLe(b, i, 4);
            i += 4;
            size_t cnt = (size_t)ReadLe(b, i, 4);
            i += 4;
            size_t have = i < n ? (n - i) / 4 : 0;
            size_t take = cnt;
            size_t byBytes = nbytes ? nbytes / 4 : cnt;
            if (byBytes < take) take = byBytes;
            if (have < take) take = have;
            Val v;
            v.tag = 0x6C;
            for (size_t k = 0; k < take; k++) v.arr.push_back((uint32_t)ReadLe(b, i + 4 * k, 4));
            out.push_back(v);
            i += 4 * take;
        } else if (TypeSize(t)) {
            int sz = TypeSize(t);
            out.push_back(Val(t, ReadLe(b, i, (size_t)sz)));
            i += (size_t)sz;
        } else {
            out.push_back(Val(T_UNKNOWN, b.substr(i - 1)));
            break;
        }
    }
    return out;
}

static void PutLe(Bytes &out, uint64_t v, int n)
{
    for (int i = 0; i < n; i++) out += (char)(unsigned char)(v >> (8 * i));
}

Bytes w_u8(unsigned v) { Bytes o(1, (char)3); o += (char)(v & 0xFF); return o; }
Bytes w_bool(bool v) { Bytes o(1, (char)1); o += (char)(v ? 1 : 0); return o; }
Bytes w_u16(unsigned v) { Bytes o(1, (char)6); PutLe(o, v, 2); return o; }
Bytes w_u32(uint32_t v) { Bytes o(1, (char)8); PutLe(o, v, 4); return o; }
Bytes w_i32(int32_t v) { Bytes o(1, (char)7); PutLe(o, (uint32_t)v, 4); return o; }
Bytes w_u64(uint64_t v) { Bytes o(1, (char)10); PutLe(o, v, 8); return o; }
Bytes w_i64(int64_t v) { Bytes o(1, (char)9); PutLe(o, (uint64_t)v, 8); return o; }
Bytes w_u32_raw(uint32_t v) { Bytes o; PutLe(o, v, 4); return o; }
Bytes w_blob(const Bytes &data) { Bytes o(1, (char)19); o += w_u32((uint32_t)data.size()); o += data; return o; }

Bytes w_str(const Bytes &s, size_t maxlen)
{
    Bytes o(1, (char)16);
    o += s.substr(0, maxlen ? maxlen - 1 : 0);
    o += '\0';
    return o;
}

Bytes Base64(const Bytes &raw)
{
    static const char abc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    Bytes out;
    size_t i = 0;
    for (; i + 2 < raw.size(); i += 3) {
        unsigned v = ((unsigned char)raw[i] << 16) | ((unsigned char)raw[i + 1] << 8) | (unsigned char)raw[i + 2];
        out += abc[v >> 18]; out += abc[(v >> 12) & 63]; out += abc[(v >> 6) & 63]; out += abc[v & 63];
    }
    if (i + 1 == raw.size()) {
        unsigned v = (unsigned char)raw[i] << 16;
        out += abc[v >> 18]; out += abc[(v >> 12) & 63]; out += "==";
    } else if (i + 2 == raw.size()) {
        unsigned v = ((unsigned char)raw[i] << 16) | ((unsigned char)raw[i + 1] << 8);
        out += abc[v >> 18]; out += abc[(v >> 12) & 63]; out += abc[(v >> 6) & 63]; out += '=';
    }
    return out;
}

bool Base64Decode(const Bytes &text, Bytes &out)
{
    unsigned acc = 0;
    int bits = 0;
    out.clear();
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char ch = (unsigned char)text[i];
        int v;
        if (ch >= 'A' && ch <= 'Z') v = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') v = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') v = ch - '0' + 52;
        else if (ch == '+') v = 62;
        else if (ch == '/') v = 63;
        else if (ch == '=') break;
        else if (ch == '\r' || ch == '\n' || ch == ' ') continue;
        else return false;
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out += (char)((acc >> bits) & 0xFF); }
    }
    return true;
}

/* NAME_MAX_RAW = 45: 4*((45+2)/3)+1 = 61 <= 0x40, the client's own encode cap. */
Bytes B64Name(const Bytes &plain)
{
    Bytes raw = plain;
    if (raw.size() > 45) {
        raw.resize(45);
        while (!raw.empty() && ((unsigned char)raw[raw.size() - 1] & 0xC0) == 0x80) raw.resize(raw.size() - 1);
    }
    return Base64(raw);
}

Bytes w_name(const Bytes &s, size_t maxlen) { return w_str(B64Name(s), maxlen); }

Bytes Reserialize(const Vals &vals, size_t from, size_t to)
{
    Bytes out;
    if (to > vals.size()) to = vals.size();
    for (size_t i = from; i < to; i++) {
        const Val &v = vals[i];
        if (v.tag == 0) out += '\0';
        else if (v.isStr()) { out += (char)v.tag; out += v.s; out += '\0'; }
        else if (v.tag == 19) out += w_blob(v.s);
        else if (TypeSize(v.tag)) { out += (char)v.tag; PutLe(out, v.u, TypeSize(v.tag)); }
    }
    return out;
}

const Val *First(const Vals &vals, int a, int b, int c)
{
    for (size_t i = 0; i < vals.size(); i++)
        if (vals[i].tag == a || vals[i].tag == b || vals[i].tag == c) return &vals[i];
    return NULL;
}

std::string FmtValues(const Vals &vals)
{
    static const char *const names[] = { "NoType", "Bool", "Char8", "UChar8", "WChar16", "Int16", "UInt16", "Int32",
        "UInt32", "Int64", "UInt64", "RangedInt32", "RangeUInt32", "Float32", "Float64" };
    std::string out = "[";
    for (size_t i = 0; i < vals.size(); i++) {
        const Val &v = vals[i];
        if (i) out += ", ";
        if (v.tag == 0) out += "NoType";
        else if (v.isStr()) out += "String(\"" + v.s.substr(0, 96) + "\")";
        else if (v.tag == 19 && v.s.size() == 16) out += "Blob[16]=" + KeyFp(v.s);
        else if (v.tag == 19) out += Fmt("Blob[%u]=", (unsigned)v.s.size()) + Hex(v.s.data(), v.s.size() < 32 ? v.s.size() : 32);
        else if (v.tag == 0x6C) out += Fmt("U32Array[%u]", (unsigned)v.arr.size());
        else if (v.tag == T_UNKNOWN) out += "<unknown tag>";
        else out += Fmt("%s(%llu)", v.tag < 15 ? names[v.tag] : "?", (unsigned long long)v.u);
    }
    return out + "]";
}

/* bdLobbyConnection::send(encrypt=true):
 *   [u32 sizePrefix][u8 encType=1][u32 messageCount][3DES-CBC ciphertext]
 *   plaintext = [u32 0xDEADBEEF][message][pad to /8 with (byte)messageCount] */
Bytes EncryptFrame(const Bytes &message, uint32_t mc)
{
    size_t cypher = (4 + message.size() + 7) & ~(size_t)7;
    Bytes buf(cypher, (char)(mc & 0xFF));
    for (int i = 0; i < 4; i++) buf[(size_t)i] = (char)(kLobbyPlaintextMagic >> (8 * i));
    memcpy(&buf[4], message.data(), message.size());
    unsigned char seed[4], digest[24];
    for (int i = 0; i < 4; i++) seed[i] = (unsigned char)(mc >> (8 * i));
    DwTiger(seed, 4, digest);
    DwDes3Cbc(kSessionKey, digest, 0, (const unsigned char *)buf.data(), (unsigned char *)&buf[0], cypher);
    Bytes out = w_u32_raw((uint32_t)(cypher + 5));
    out += (char)1;
    out += w_u32_raw(mc);
    out += buf;
    return out;
}

bool DecryptClientFrame(const Bytes &payload, uint32_t &mc, int &service, Bytes &body)
{
    if (payload.size() < 5 || payload[0] != 1) return false;
    size_t ct = payload.size() - 5;
    if (ct < 8 || ct % 8) return false;
    mc = (uint32_t)ReadLe(payload, 1, 4);
    unsigned char seed[4], digest[24];
    for (int i = 0; i < 4; i++) seed[i] = (unsigned char)(mc >> (8 * i));
    DwTiger(seed, 4, digest);
    Bytes pt(ct, '\0');
    DwDes3Cbc(kSessionKey, digest, 1, (const unsigned char *)payload.data() + 5, (unsigned char *)&pt[0], ct);
    service = (unsigned char)pt[4]; /* pt[0:4] is the client's signature: never verified by either side */
    body = pt.substr(5);
    return true;
}

} // namespace dw

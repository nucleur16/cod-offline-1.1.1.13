/* dw_json.cpp - see dw_json.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "dw_json.h"

namespace dw {

const Json &Json::null()
{
    static const Json n;
    return n;
}

int64_t Json::asInt(int64_t dflt) const
{
    switch (type_) {
    case Bool: case Int: case UInt: return i_;
    case Real: return (int64_t)d_;
    case String: {
        const char *p = s_.c_str();
        char *end = 0;
        long long v = strtoll(p, &end, 10);
        return (end && end != p && !*end) ? v : dflt;
    }
    default: return dflt;
    }
}

uint64_t Json::asUInt(uint64_t dflt) const
{
    switch (type_) {
    case Bool: case Int: case UInt: return (uint64_t)i_;
    case Real: return d_ < 0 ? 0 : (uint64_t)d_;
    case String: {
        const char *p = s_.c_str();
        char *end = 0;
        unsigned long long v = strtoull(p, &end, 10);
        return (end && end != p && !*end) ? v : dflt;
    }
    default: return dflt;
    }
}

double Json::asReal(double dflt) const
{
    switch (type_) {
    case Bool: case Int: return (double)i_;
    case UInt: return (double)(uint64_t)i_;
    case Real: return d_;
    case String: {
        const char *p = s_.c_str();
        char *end = 0;
        double v = strtod(p, &end);
        return (end && end != p && !*end) ? v : dflt;
    }
    default: return dflt;
    }
}

bool Json::asBool(bool dflt) const
{
    switch (type_) {
    case Null: return false;
    case Bool: case Int: case UInt: return i_ != 0;
    case Real: return d_ != 0;
    case String: return !s_.empty();
    case Array: return !a_.empty();
    case Object: return !o_.empty();
    }
    return dflt;
}

bool Json::has(const std::string &key) const
{
    if (type_ != Object) return false;
    for (size_t i = 0; i < o_.size(); i++) if (o_[i].first == key) return true;
    return false;
}

const Json &Json::get(const std::string &key) const
{
    if (type_ == Object)
        for (size_t i = 0; i < o_.size(); i++) if (o_[i].first == key) return o_[i].second;
    return null();
}

Json &Json::operator[](const std::string &key)
{
    if (type_ != Object) { clear(); type_ = Object; }
    for (size_t i = 0; i < o_.size(); i++) if (o_[i].first == key) return o_[i].second;
    o_.push_back(std::make_pair(key, Json()));
    return o_.back().second;
}

bool Json::erase(const std::string &key)
{
    if (type_ != Object) return false;
    for (size_t i = 0; i < o_.size(); i++)
        if (o_[i].first == key) { o_.erase(o_.begin() + (long)i); return true; }
    return false;
}

bool Json::operator==(const Json &o) const
{
    if (isNumber() && o.isNumber()) {
        if (isInt() && o.isInt()) return i_ == o.i_ && ((type_ == UInt) == (o.type_ == UInt) || i_ >= 0);
        return asReal() == o.asReal();
    }
    if (type_ != o.type_) return false;
    switch (type_) {
    case Null: return true;
    case Bool: return i_ == o.i_;
    case String: return s_ == o.s_;
    case Array: return a_ == o.a_;
    case Object:
        if (o_.size() != o.o_.size()) return false;
        for (size_t i = 0; i < o_.size(); i++)
            if (!o.has(o_[i].first) || !(o.get(o_[i].first) == o_[i].second)) return false;
        return true;
    default: return false;
    }
}

static void DumpString(std::string &out, const std::string &s)
{
    out += '"';
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) { char esc[8]; snprintf(esc, sizeof(esc), "\\u%04x", c); out += esc; }
            else out += (char)c;
        }
    }
    out += '"';
}

static bool KeyLess(const std::pair<std::string, Json> *a, const std::pair<std::string, Json> *b) { return a->first < b->first; }

void Json::dumpTo(std::string &out, int indent, int depth, bool sortKeys) const
{
    char num[64];
    switch (type_) {
    case Null: out += "null"; break;
    case Bool: out += i_ ? "true" : "false"; break;
    case Int: snprintf(num, sizeof(num), "%lld", (long long)i_); out += num; break;
    case UInt: snprintf(num, sizeof(num), "%llu", (unsigned long long)i_); out += num; break;
    case Real:
        if (d_ == floor(d_) && fabs(d_) < 1e15) snprintf(num, sizeof(num), "%.1f", d_);
        else snprintf(num, sizeof(num), "%.17g", d_);
        out += num;
        break;
    case String: DumpString(out, s_); break;
    case Array:
        if (a_.empty()) { out += "[]"; break; }
        out += '[';
        for (size_t i = 0; i < a_.size(); i++) {
            if (i) out += indent >= 0 ? "," : ", ";
            if (indent >= 0) { out += '\n'; out.append((size_t)(indent * (depth + 1)), ' '); }
            a_[i].dumpTo(out, indent, depth + 1, sortKeys);
        }
        if (indent >= 0) { out += '\n'; out.append((size_t)(indent * depth), ' '); }
        out += ']';
        break;
    case Object: {
        if (o_.empty()) { out += "{}"; break; }
        std::vector<const std::pair<std::string, Json> *> order;
        for (size_t i = 0; i < o_.size(); i++) order.push_back(&o_[i]);
        if (sortKeys) std::stable_sort(order.begin(), order.end(), KeyLess);
        out += '{';
        for (size_t i = 0; i < order.size(); i++) {
            if (i) out += indent >= 0 ? "," : ", ";
            if (indent >= 0) { out += '\n'; out.append((size_t)(indent * (depth + 1)), ' '); }
            DumpString(out, order[i]->first);
            out += ": ";
            order[i]->second.dumpTo(out, indent, depth + 1, sortKeys);
        }
        if (indent >= 0) { out += '\n'; out.append((size_t)(indent * depth), ' '); }
        out += '}';
        break;
    }
    }
}

std::string Json::dump(int indent, bool sortKeys) const
{
    std::string out;
    dumpTo(out, indent, 0, sortKeys);
    return out;
}

namespace {

struct Parser {
    const char *p, *end;
    std::string err;
    int depth;
    void ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++; }
    bool fail(const char *what) { if (err.empty()) err = what; return false; }

    static void utf8(std::string &out, unsigned cp)
    {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    bool hex4(unsigned &v)
    {
        if (end - p < 4) return false;
        v = 0;
        for (int i = 0; i < 4; i++) {
            char c = *p++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        return true;
    }
    bool string(std::string &out)
    {
        if (p >= end || *p != '"') return fail("expected a string");
        p++;
        out.clear();
        while (p < end && *p != '"') {
            if (*p != '\\') { out += *p++; continue; }
            if (++p >= end) return fail("bad escape");
            char c = *p++;
            switch (c) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                unsigned cp, lo;
                if (!hex4(cp)) return fail("bad \\u escape");
                if (cp >= 0xD800 && cp < 0xDC00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                    p += 2;
                    if (!hex4(lo)) return fail("bad \\u escape");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                utf8(out, cp);
                break;
            }
            default: out += c;
            }
        }
        if (p >= end) return fail("unterminated string");
        p++;
        return true;
    }
    bool value(Json &out)
    {
        ws();
        if (p >= end) return fail("unexpected end");
        if (++depth > 200) return fail("too deep");
        bool ok = true;
        char c = *p;
        if (c == '{') {
            p++;
            out = Json::MakeObject();
            ws();
            if (p < end && *p == '}') { p++; }
            else for (;;) {
                std::string key;
                ws();
                if (!string(key)) { ok = false; break; }
                ws();
                if (p >= end || *p != ':') { ok = fail("expected ':'"); break; }
                p++;
                Json v;
                if (!value(v)) { ok = false; break; }
                out.members().push_back(std::make_pair(key, v));
                ws();
                if (p < end && *p == ',') { p++; continue; }
                if (p < end && *p == '}') { p++; break; }
                ok = fail("expected ',' or '}'");
                break;
            }
        } else if (c == '[') {
            p++;
            out = Json::MakeArray();
            ws();
            if (p < end && *p == ']') { p++; }
            else for (;;) {
                Json v;
                if (!value(v)) { ok = false; break; }
                out.items().push_back(v);
                ws();
                if (p < end && *p == ',') { p++; continue; }
                if (p < end && *p == ']') { p++; break; }
                ok = fail("expected ',' or ']'");
                break;
            }
        } else if (c == '"') {
            std::string s;
            ok = string(s);
            if (ok) out = Json(s);
        } else if (end - p >= 4 && !strncmp(p, "true", 4)) { p += 4; out = Json(true); }
        else if (end - p >= 5 && !strncmp(p, "false", 5)) { p += 5; out = Json(false); }
        else if (end - p >= 4 && !strncmp(p, "null", 4)) { p += 4; out = Json(); }
        else if (end - p >= 3 && !strncmp(p, "NaN", 3)) { p += 3; out = Json(0.0); }
        else if (c == '-' || (c >= '0' && c <= '9')) {
            const char *start = p;
            bool real = false;
            if (*p == '-') p++;
            while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-')) {
                if (*p == '.' || *p == 'e' || *p == 'E') real = true;
                p++;
            }
            std::string text(start, (size_t)(p - start));
            if (real) out = Json(strtod(text.c_str(), 0));
            else if (text[0] == '-') out = Json((long long)strtoll(text.c_str(), 0, 10));
            else out = Json((unsigned long long)strtoull(text.c_str(), 0, 10));
        } else {
            ok = fail("unexpected character");
        }
        depth--;
        return ok;
    }
};

} // namespace

bool Json::Parse(const std::string &text, Json &out, std::string *error)
{
    Parser ps;
    ps.p = text.data();
    ps.end = text.data() + text.size();
    ps.depth = 0;
    if (text.size() >= 3 && !memcmp(text.data(), "\xEF\xBB\xBF", 3)) ps.p += 3;
    bool ok = ps.value(out);
    if (ok) { ps.ws(); if (ps.p != ps.end) ok = ps.fail("trailing data"); }
    if (!ok) { out = Json(); if (error) *error = ps.err; }
    return ok;
}

} // namespace dw

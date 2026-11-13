/*
 * dw_json.h - a small ordered JSON value for the offline lobby's tables and profiles.
 *
 * Objects keep insertion order (as Python dicts do), integers keep 64-bit precision in both the
 * signed and unsigned range (player ids are u64), and a missing key reads as null instead of failing:
 * the Python code this is ported from leans on dict.get() everywhere.
 */
#pragma once
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace dw {

class Json {
public:
    enum Type { Null, Bool, Int, UInt, Real, String, Array, Object };
    typedef std::vector<std::pair<std::string, Json> > Members;

    Json() : type_(Null), i_(0), d_(0) {}
    Json(bool v) : type_(Bool), i_(v ? 1 : 0), d_(0) {}
    Json(int v) : type_(Int), i_(v), d_(0) {}
    Json(unsigned v) : type_(Int), i_(v), d_(0) {}
    Json(long long v) : type_(Int), i_(v), d_(0) {}
    Json(unsigned long long v) : type_(v > 0x7FFFFFFFFFFFFFFFULL ? UInt : Int), i_((int64_t)v), d_(0) {}
    Json(double v) : type_(Real), i_(0), d_(v) {}
    Json(const char *v) : type_(String), i_(0), d_(0), s_(v ? v : "") {}
    Json(const std::string &v) : type_(String), i_(0), d_(0), s_(v) {}
    static Json MakeArray() { Json j; j.type_ = Array; return j; }
    static Json MakeObject() { Json j; j.type_ = Object; return j; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Null; }
    bool isBool() const { return type_ == Bool; }
    bool isNumber() const { return type_ == Int || type_ == UInt || type_ == Real; }
    bool isInt() const { return type_ == Int || type_ == UInt; }
    bool isString() const { return type_ == String; }
    bool isArray() const { return type_ == Array; }
    bool isObject() const { return type_ == Object; }

    /* Lenient readers: a number held as a string converts, anything else gives the default. */
    int64_t asInt(int64_t dflt = 0) const;
    uint64_t asUInt(uint64_t dflt = 0) const;
    double asReal(double dflt = 0) const;
    bool asBool(bool dflt = false) const;   /* Python truthiness */
    const std::string &asString() const { return s_; } /* "" unless a string */
    std::string str(const std::string &dflt = "") const { return type_ == String ? s_ : dflt; }
    bool truthy() const { return asBool(false); }

    /* arrays */
    size_t size() const { return type_ == Array ? a_.size() : type_ == Object ? o_.size() : 0; }
    const Json &at(size_t i) const { return type_ == Array && i < a_.size() ? a_[i] : null(); }
    Json &at(size_t i) { return a_[i]; }
    void push(const Json &v) { if (type_ != Array) { clear(); type_ = Array; } a_.push_back(v); }
    std::vector<Json> &items() { if (type_ != Array) { clear(); type_ = Array; } return a_; }
    const std::vector<Json> &items() const { return a_; }

    /* objects */
    bool has(const std::string &key) const;
    const Json &get(const std::string &key) const;   /* null when missing */
    Json &operator[](const std::string &key);         /* creates (and makes this an object) */
    const Json &operator[](const std::string &key) const { return get(key); }
    void set(const std::string &key, const Json &v) { (*this)[key] = v; }
    bool erase(const std::string &key);
    Members &members() { if (type_ != Object) { clear(); type_ = Object; } return o_; }
    const Members &members() const { return o_; }

    void clear() { type_ = Null; i_ = 0; d_ = 0; s_.clear(); a_.clear(); o_.clear(); }
    bool operator==(const Json &o) const;
    bool operator!=(const Json &o) const { return !(*this == o); }

    /* indent < 0: compact with ", " / ": " separators (Python json.dumps default);
     * indent >= 0: pretty, like json.dumps(indent=N). sortKeys as sort_keys=True.
     * Non-ASCII bytes are written through unchanged (ensure_ascii=False on UTF-8 text). */
    std::string dump(int indent = -1, bool sortKeys = false) const;
    static bool Parse(const std::string &text, Json &out, std::string *error = 0);
    static const Json &null();

private:
    void dumpTo(std::string &out, int indent, int depth, bool sortKeys) const;
    Type type_;
    int64_t i_;
    double d_;
    std::string s_;
    std::vector<Json> a_;
    Members o_;
};

} // namespace dw

/*
 * dw_playerdata.cpp - the player profile blob codec (dw_playerdata.h).
 * Port of store/playerdata_editor.py (BlobDef, Field, Blob, class_item_id) plus the two small
 * tables its loader leans on: catalogue/character_slots.py and model_configstrings.cha_item_names.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "dw.h"
#include "dw_json.h"
#include "dw_playerdata.h"
#include "dw_state.h"

namespace dw {

struct PlayerDataState {
    bool defsTried, namesLoaded;
    PdDef mpdata, mpdataS;
    bool haveMpdata, haveMpdataS;
    std::map<int, PdItemName> names;
    PlayerDataState() : defsTried(false), namesLoaded(false), haveMpdata(false), haveMpdataS(false) {}
};

static PlayerDataState &State(Server &s)
{
    if (!s.st->playerdata) s.st->playerdata = new PlayerDataState();
    return *s.st->playerdata;
}

void PlayerDataFree(Server &s)
{
    delete s.st->playerdata;
    s.st->playerdata = NULL;
}

/* ---- little-endian accessors (bounds-checked: a short buffer reads 0 and is never written) ---- */

unsigned PdU16(const Bytes &b, size_t o)
{
    return o + 2 <= b.size() ? (unsigned)(unsigned char)b[o] | ((unsigned)(unsigned char)b[o + 1] << 8) : 0;
}
void PdPutU16(Bytes &b, size_t o, unsigned v)
{
    if (o + 2 > b.size()) return;
    b[o] = (char)(v & 0xFF);
    b[o + 1] = (char)((v >> 8) & 0xFF);
}
uint32_t PdU32(const Bytes &b, size_t o)
{
    if (o + 4 > b.size()) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)(unsigned char)b[o + i] << (8 * i);
    return v;
}
void PdPutU32(Bytes &b, size_t o, uint32_t v)
{
    if (o + 4 > b.size()) return;
    for (int i = 0; i < 4; i++) b[o + i] = (char)((v >> (8 * i)) & 0xFF);
}

/* ---- the def expansion (BlobDef) ---- */

namespace {

struct RawProp { std::string name; int kind; int typeIndex; int leaf; };
struct RawStruct { std::vector<RawProp> props; int leafCount; };
struct RawArray { int count; int enumIndex; int elemKind; int elemTypeIndex; };
enum { K_STRUCT = 100, K_INDEXED, K_ENUMED, K_BAD };

int KindOf(const std::string &t)
{
    if (t == "int") return PD_INT;
    if (t == "byte") return PD_BYTE;
    if (t == "bool") return PD_BOOL;
    if (t == "float") return PD_FLOAT;
    if (t == "short") return PD_SHORT;
    if (t == "enum") return PD_ENUM;
    if (t == "string") return PD_STRING;
    if (t == "struct") return K_STRUCT;
    if (t == "indexedArray") return K_INDEXED;
    if (t == "enumedArray") return K_ENUMED;
    return K_BAD;
}

int Width(int kind) { return kind == PD_INT || kind == PD_FLOAT ? 4 : kind == PD_SHORT || kind == PD_ENUM ? 2 : kind == PD_BYTE ? 1 : 0; }

struct Expander {
    PdDef &def;
    std::vector<RawStruct> structs;
    std::vector<RawArray> indexed, enumed;
    std::vector<std::vector<std::string> > enumNames; /* per enum: the label of value 0..entryCount-1 */
    std::vector<int64_t> offsets;
    bool bad;
    Expander(PdDef &d) : def(d), bad(false) {}

    int emit(const std::string &path, int kind, int typeIndex, int leaf, const std::string &member,
             const std::vector<int> &indices, const std::string &prop)
    {
        if (leaf < 0 || (size_t)leaf >= offsets.size()) { bad = true; return leaf + 1; }
        int64_t raw = offsets[(size_t)leaf];
        PdField f;
        f.path = path;
        f.kind = kind;
        f.enumIndex = kind == PD_ENUM ? typeIndex : -1;
        f.leaf = leaf;
        if (kind == PD_BOOL) { f.byte = (int)(4 + raw / 8); f.bit = (int)(raw & 7); }
        else { f.byte = (int)(raw + 4); f.bit = -1; }
        f.member = member;
        f.indices = indices;
        f.prop = prop;
        def.byPath[path] = (int)def.fields.size(); /* a repeated path keeps the last leaf, as the dict does */
        def.fields.push_back(f);
        return leaf + 1;
    }

    int expand(const std::string &path, int kind, int ti, int leaf, const std::string &member,
               const std::vector<int> &indices, const std::string &prop, int depth)
    {
        if (bad || depth > 16 || def.fields.size() > 100000) { bad = true; return leaf; }
        if (kind < K_STRUCT) return emit(path, kind, ti, leaf, member, indices, prop);
        if (kind == K_STRUCT) {
            if (ti < 0 || (size_t)ti >= structs.size()) { bad = true; return leaf; }
            const RawStruct &st = structs[(size_t)ti];
            for (size_t i = 0; i < st.props.size(); i++) {
                const RawProp &p = st.props[i];
                expand(path + "." + p.name, p.kind, p.typeIndex, leaf + p.leaf, member, indices, p.name, depth + 1);
            }
            return leaf + st.leafCount;
        }
        if (kind == K_INDEXED) {
            if (ti < 0 || (size_t)ti >= indexed.size()) { bad = true; return leaf; }
            const RawArray &a = indexed[(size_t)ti];
            std::vector<int> idx(indices);
            idx.push_back(0);
            for (int i = 0; i < a.count && !bad; i++) {
                idx.back() = i;
                leaf = expand(path + Fmt("[%d]", i), a.elemKind, a.elemTypeIndex, leaf, member, idx, prop, depth + 1);
            }
            return leaf;
        }
        if (kind == K_ENUMED) {
            if (ti < 0 || (size_t)ti >= enumed.size()) { bad = true; return leaf; }
            const RawArray &a = enumed[(size_t)ti];
            if (a.enumIndex < 0 || (size_t)a.enumIndex >= enumNames.size()) { bad = true; return leaf; }
            const std::vector<std::string> &labels = enumNames[(size_t)a.enumIndex];
            std::vector<int> idx(indices);
            idx.push_back(0);
            for (size_t i = 0; i < labels.size() && !bad; i++) {
                idx.back() = (int)i;
                leaf = expand(path + "[" + labels[i] + "]", a.elemKind, a.elemTypeIndex, leaf, member, idx, prop, depth + 1);
            }
            return leaf;
        }
        bad = true;
        return leaf;
    }
};

bool BuildDef(const std::string &key, const Json &raw, PdDef &def)
{
    if (!raw.isObject()) return false;
    def = PdDef();
    def.key = key;
    def.asset = raw.get("asset").str(key);
    def.version = (uint32_t)raw.get("version").asUInt();
    def.formatChecksum = (uint32_t)raw.get("formatChecksum").asUInt();
    def.size = (int)raw.get("size").asInt();
    def.leafCount = (int)raw.get("leafCount").asInt();

    Expander ex(def);
    const std::vector<Json> &offs = raw.get("offsets").items();
    ex.offsets.reserve(offs.size());
    for (size_t i = 0; i < offs.size(); i++) ex.offsets.push_back(offs[i].asInt());

    const std::vector<Json> &enums = raw.get("enums").items();
    for (size_t e = 0; e < enums.size(); e++) {
        int count = (int)enums[e].get("entryCount").asInt();
        if (count < 0 || count > 65536) return false;
        std::vector<std::string> labels((size_t)count);
        for (int v = 0; v < count; v++) labels[(size_t)v] = Fmt("v%d", v);
        const std::vector<Json> &ents = enums[e].get("entries").items();
        for (size_t i = 0; i < ents.size(); i++) {
            int64_t index = ents[i].get("index").asInt(-1);
            if (index < 0 || index >= count) continue;
            std::string nm = ents[i].get("name").str();
            if (nm.empty()) nm = ents[i].get("nameRecovered").str();
            if (nm.empty()) nm = Fmt("v%d", (int)index);
            labels[(size_t)index] = nm;
        }
        ex.enumNames.push_back(labels);
    }
    const std::vector<Json> &structs = raw.get("structs").items();
    for (size_t i = 0; i < structs.size(); i++) {
        RawStruct st;
        st.leafCount = (int)structs[i].get("leafCount").asInt();
        const std::vector<Json> &props = structs[i].get("props").items();
        for (size_t j = 0; j < props.size(); j++) {
            RawProp p;
            p.kind = KindOf(props[j].get("type").str());
            p.typeIndex = (int)props[j].get("typeIndex").asInt();
            p.leaf = (int)props[j].get("leaf").asInt();
            p.name = props[j].get("name").str();
            if (p.name.empty()) p.name = props[j].get("nameRecovered").str(); /* a cross-block name the zone only points at */
            if (p.name.empty()) p.name = Fmt("<unnamed@leaf%+d>", p.leaf);
            st.props.push_back(p);
        }
        ex.structs.push_back(st);
    }
    const std::vector<Json> &ia = raw.get("indexedArrays").items();
    for (size_t i = 0; i < ia.size(); i++) {
        RawArray a;
        a.count = (int)ia[i].get("count").asInt();
        a.enumIndex = -1;
        a.elemKind = KindOf(ia[i].get("elemType").str());
        a.elemTypeIndex = (int)ia[i].get("elemTypeIndex").asInt();
        ex.indexed.push_back(a);
    }
    const std::vector<Json> &ea = raw.get("enumedArrays").items();
    for (size_t i = 0; i < ea.size(); i++) {
        RawArray a;
        a.count = 0;
        a.enumIndex = (int)ea[i].get("enumIndex").asInt();
        a.elemKind = KindOf(ea[i].get("elemType").str());
        a.elemTypeIndex = (int)ea[i].get("elemTypeIndex").asInt();
        ex.enumed.push_back(a);
    }

    int root = (int)raw.get("rootType").at(1).asInt(-1);
    if (root < 0 || (size_t)root >= ex.structs.size()) return false;
    const RawStruct &rs = ex.structs[(size_t)root];
    for (size_t i = 0; i < rs.props.size(); i++) {
        const RawProp &p = rs.props[i];
        size_t start = def.fields.size();
        ex.expand(p.name, p.kind, p.typeIndex, p.leaf, p.name, std::vector<int>(), p.name, 0);
        PdDef::Member m;
        m.name = p.name;
        m.firstLeaf = p.leaf;
        m.count = (int)(def.fields.size() - start);
        m.arrayCount = (p.kind == K_INDEXED && p.typeIndex >= 0 && (size_t)p.typeIndex < ex.indexed.size())
                           ? ex.indexed[(size_t)p.typeIndex].count : -1;
        def.members.push_back(m);
    }
    /* Expanding the root must reproduce the def's own leafCount, to the leaf. */
    return !ex.bad && (int)def.fields.size() == def.leafCount;
}

void LoadDefs(Server &s, PlayerDataState &st)
{
    st.defsTried = true;
    Bytes text;
    Json doc;
    if (!s.Resource("playerdata_defs.proposed.json", text) || !Json::Parse(text, doc)) {
        s.Log("playerdata: playerdata_defs.proposed.json is missing or unreadable -- profile blobs cannot be decoded");
        return;
    }
    const Json &defs = doc.get("defs");
    st.haveMpdata = BuildDef("mpdata", defs.get("mpdata"), st.mpdata);
    st.haveMpdataS = BuildDef("mpdata_s", defs.get("mpdata_s"), st.mpdataS);
    if (!st.haveMpdata || !st.haveMpdataS)
        s.Log(Fmt("playerdata: def expansion failed (mpdata %s, mpdata_s %s)", st.haveMpdata ? "ok" : "BAD", st.haveMpdataS ? "ok" : "BAD"));
}

} // namespace

const PdField *PdDef::Find(const std::string &path) const
{
    std::map<std::string, int>::const_iterator it = byPath.find(path);
    return it == byPath.end() ? NULL : &fields[(size_t)it->second];
}

std::vector<const PdField *> PdDef::MemberFields(const std::string &member) const
{
    std::vector<const PdField *> out;
    for (size_t i = 0; i < fields.size(); i++)
        if (fields[i].member == member) out.push_back(&fields[i]);
    return out;
}

int PdDef::ArrayCount(const std::string &member) const
{
    for (size_t i = 0; i < members.size(); i++)
        if (members[i].name == member) return members[i].arrayCount;
    return -1;
}

const PdDef *PlayerDataDef(Server &s, const std::string &key)
{
    PlayerDataState &st = State(s);
    if (!st.defsTried) LoadDefs(s, st);
    if (key == "mpdata") return st.haveMpdata ? &st.mpdata : NULL;
    if (key == "mpdata_s") return st.haveMpdataS ? &st.mpdataS : NULL;
    return NULL;
}

std::string PdIdentify(Server &s, const Bytes &buf)
{
    if (buf.size() < 12) return "";
    uint32_t ver = PdU32(buf, 4), chk = PdU32(buf, 8);
    static const char *const keys[] = { "mpdata", "mpdata_s" };
    for (int i = 0; i < 2; i++) {
        const PdDef *d = PlayerDataDef(s, keys[i]);
        if (d && d->version == ver && d->formatChecksum == chk) return keys[i];
    }
    return "";
}

/* ---- the character item table ---- */

const std::map<int, PdItemName> &PdItemNames(Server &s)
{
    PlayerDataState &st = State(s);
    if (st.namesLoaded) return st.names;
    st.namesLoaded = true;
    static const char *const files[] = { "items_cha.txt", "items_cha_unlockall.proposed" }; /* the second wins */
    for (int f = 0; f < 2; f++) {
        Bytes text;
        if (!s.Resource(files[f], text)) continue;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t eol = text.find('\n', pos);
            if (eol == std::string::npos) eol = text.size();
            std::vector<std::string> p; /* str.split(): runs of whitespace */
            size_t i = pos;
            while (i < eol) {
                while (i < eol && (unsigned char)text[i] <= ' ') i++;
                size_t j = i;
                while (j < eol && (unsigned char)text[j] > ' ') j++;
                if (j > i) p.push_back(text.substr(i, j - i));
                i = j;
            }
            pos = eol + 1;
            /* items_cha.txt: <id> <name> <part>      unlockall: <id> <name> <category> [<part>] */
            if (p.size() < 3 || p[0].empty() || p[0].size() > 9 || p[0].find_first_not_of("0123456789") != std::string::npos) continue;
            PdItemName n;
            n.name = p[1];
            n.part = p.size() > 3 ? p[3] : p[2];
            st.names[atoi(p[0].c_str())] = n;
        }
    }
    /* model_configstrings.LIVE_ITEM_NAMES: the ids measured off a live profile, as the fallback. */
    static const struct { int id; const char *name; } live[] = {
        { 103, "ves_base" }, { 107, "heg_zmcowboy" }, { 109, "elp_tactical" }, { 111, "knp_tactical" },
        { 115, "hea_socomb" }, { 120, "fpa_compblkjeans" }, { 124, "fpa_technoa" }, { 131, "knp_typea" },
        { 136, "boo_concepta" }, { 142, "fja_standard" }, { 147, "ves_ceramdisk" }, { 155, "heg_beret" },
        { 169, "han_basic" }, { 171, "hea_zmcowboy" }, { 172, "eye_combat" }, { 177, "fja_zmcowboy" },
        { 179, "fpa_zmcowboy" }, { 181, "boo_zmcowboy" }, { 183, "han_zmcowboy" }, { 361, "boo_support" },
        { 382, "hea_sean" }, { 383, "heg_sean" }, { 397, "fja_church" },
        { 500, "co_pat_wpn_mastery_sniper" }, { 501, "co_pat_wpn_mastery_smg" },
    };
    for (size_t i = 0; i < sizeof(live) / sizeof(live[0]); i++) {
        if (st.names.count(live[i].id)) continue;
        PdItemName n;
        n.name = live[i].name;
        n.part = n.name.substr(0, 3);
        st.names[live[i].id] = n;
    }
    return st.names;
}

/* ---- catalogue/character_slots.py ---- */

const char *const kPdProfileSlotOrder[20] = { "fpa", "boo", "fja", "han", "hea", "ves", "heg", "elp", "knp", "pat",
                                              "", "", "", "eye", "", "", "", "", "", "" };

/* C1 694818/694850 copies 18 profile slots without reordering, head at slot 4. Only a complete old
 * eye-first row (11 items, each of the group its legacy position demands) is permuted; an unknown
 * or mixed row stays byte-identical. -> true when any row moved. */
static bool NormalizeCharacterBuffer(Server &s, Bytes &buf)
{
    static const char *const legacyOrder[11] = { "eye", "pat", "knp", "elp", "heg", "boo", "han", "ves", "fpa", "fja", "hea" };
    static const int legacyDest[11] = { 13, 9, 8, 7, 6, 1, 3, 5, 0, 2, 4 };
    if (buf.size() != PD_BUF_SIZE) return false;
    const std::map<int, PdItemName> &names = PdItemNames(s);
    bool changed = false;
    for (int index = 0; index < PD_CHARACTER_CLASSES_COUNT; index++) {
        size_t start = (size_t)(PD_CHARACTER_CLASSES_OFF + index * PD_CHARACTER_CLASSES_STRIDE);
        unsigned row[20];
        for (int i = 0; i < 20; i++) row[i] = PdU16(buf, start + 2 * (size_t)i);
        bool legacy = row[10] > 0 && row[10] < 32768;
        for (int i = 11; i < 20 && legacy; i++) if (row[i]) legacy = false;
        for (int i = 0; i < 11 && legacy; i++) {
            if (!row[i]) continue;
            if (row[i] >= 32768) { legacy = false; break; }
            std::map<int, PdItemName>::const_iterator it = names.find((int)row[i]);
            if (it == names.end()) { legacy = false; break; }
            std::string nm = it->second.name;
            if (nm.compare(0, 3, "co_") == 0) nm = nm.substr(3);
            if (nm.substr(0, nm.find('_')) != legacyOrder[i]) legacy = false;
        }
        if (!legacy) continue;
        unsigned out[20];
        memset(out, 0, sizeof(out));
        for (int i = 0; i < 11; i++) out[legacyDest[i]] = row[i];
        for (int i = 0; i < 20; i++) PdPutU16(buf, start + 2 * (size_t)i, out[i]);
        changed = true;
    }
    return changed;
}

/* ---- Blob ---- */

bool PdBlob::Load(Server &s, const Bytes &stored, const std::string &key, std::string *err)
{
    def = NULL;
    buf.clear();
    Bytes raw;
    if (!Inflate(stored, raw)) { if (err) *err = "not a zlib stream"; return false; }
    if (raw.size() != PD_BUF_SIZE) { if (err) *err = Fmt("inflated to %u bytes, expected %d", (unsigned)raw.size(), (int)PD_BUF_SIZE); return false; }
    std::string k = key.empty() ? PdIdentify(s, raw) : key;
    if (k.empty()) {
        if (err) *err = Fmt("header version/formatChecksum (%u / 0x%08X) match neither def", PdU32(raw, 4), PdU32(raw, 8));
        return false;
    }
    const PdDef *d = PlayerDataDef(s, k);
    if (!d) { if (err) *err = "no def for " + k; return false; }
    def = d;
    buf.swap(raw);
    if (k == "mpdata" && NormalizeCharacterBuffer(s, buf)) FixCrc();
    return true;
}

bool PdBlob::Blank(Server &s, const std::string &key)
{
    def = PlayerDataDef(s, key);
    buf.clear();
    if (!def) return false;
    buf.assign(PD_BUF_SIZE, '\0');
    PdPutU32(buf, 4, def->version);
    PdPutU32(buf, 8, def->formatChecksum);
    FixCrc();
    return true;
}

double PdBlob::Get(const PdField &f) const
{
    if (f.byte < 0 || (size_t)f.byte + (size_t)Width(f.kind) + (f.kind == PD_BOOL ? 1 : 0) > buf.size()) return 0;
    size_t o = (size_t)f.byte;
    switch (f.kind) {
    case PD_BOOL: return ((unsigned char)buf[o] >> f.bit) & 1;
    case PD_BYTE: return (unsigned char)buf[o];
    case PD_SHORT: case PD_ENUM: return (int16_t)PdU16(buf, o);
    case PD_INT: return (int32_t)PdU32(buf, o);
    case PD_FLOAT: { uint32_t u = PdU32(buf, o); float v; memcpy(&v, &u, 4); return v; }
    }
    return 0; /* a DDL string cannot be read */
}

bool PdBlob::Set(const PdField &f, double value)
{
    if (f.byte < 0 || (size_t)f.byte + (size_t)Width(f.kind) + (f.kind == PD_BOOL ? 1 : 0) > buf.size()) return false;
    if (value != value) return false; /* NaN */
    size_t o = (size_t)f.byte;
    double iv = value < 0 ? ceil(value) : floor(value); /* int(value) */
    switch (f.kind) {
    case PD_BOOL:
        if (value != 0) buf[o] = (char)((unsigned char)buf[o] | (1u << f.bit));
        else buf[o] = (char)((unsigned char)buf[o] & ~(1u << f.bit));
        return true;
    case PD_BYTE:
        if (iv < 0 || iv > 255) return false;
        buf[o] = (char)(unsigned char)iv;
        return true;
    case PD_SHORT: case PD_ENUM:
        if (iv >= 32768.0 && iv <= 65535.0) iv -= 65536.0; /* 0xFFFF is a spelling of -1 */
        if (iv < -32768.0 || iv > 32767.0) return false;
        PdPutU16(buf, o, (unsigned)(uint16_t)(int16_t)iv);
        return true;
    case PD_INT:
        if (iv >= 2147483648.0 && iv <= 4294967295.0) iv -= 4294967296.0;
        if (iv < -2147483648.0 || iv > 2147483647.0) return false;
        PdPutU32(buf, o, (uint32_t)(int32_t)iv);
        return true;
    case PD_FLOAT: {
        if (fabs(value) > 3.4028234663852886e38) return false;
        float v = (float)value;
        uint32_t u;
        memcpy(&u, &v, 4);
        PdPutU32(buf, o, u);
        return true;
    }
    }
    return false;
}

bool PdBlob::GetPath(const std::string &path, double &out) const
{
    const PdField *f = def ? def->Find(path) : NULL;
    if (!f) return false;
    out = Get(*f);
    return true;
}

bool PdBlob::SetPath(const std::string &path, double value)
{
    const PdField *f = def ? def->Find(path) : NULL;
    return f && Set(*f, value);
}

uint32_t PdBlob::StoredCrc() const { return PdU32(buf, 0); }
uint32_t PdBlob::ComputedCrc() const { return buf.size() > 4 ? Crc32(buf.data() + 4, buf.size() - 4) : 0; }
void PdBlob::FixCrc() { PdPutU32(buf, 0, ComputedCrc()); }

bool PdBlob::Store(Bytes &out)
{
    out.clear();
    if (!ok()) return false;
    FixCrc();
    Bytes check;
    /* Never emit something we cannot read back. */
    if (!Deflate(buf, out, 1) || !Inflate(out, check) || check != buf) { out.clear(); return false; }
    return true;
}

/* ---- weaponClasses item ids ---- */

bool PdItemPlusOne(const std::string &prop)
{
    return prop == "perks" || prop == "killstreaks" || prop == "deathstreak" || prop == "lethal" || prop == "tactical" ||
           prop == "special" || prop == "meleeWpn";
}

int PdClassItemId(const std::string &prop, int64_t raw)
{
    if (PdItemPlusOne(prop)) return raw > 1 ? (int)(raw - 1) : -1;
    return (raw == 0 || raw == -1 || raw == 0xFFFF) ? -1 : (int)raw;
}

int PdClassItemRaw(const std::string &prop, int itemId)
{
    if (PdItemPlusOne(prop)) return itemId < 0 ? 1 : itemId + 1;
    return itemId; /* -1 = the 0xFFFF "slot unused" short */
}

std::set<int> PdEquippedItemIds(Server &s, const Bytes &storedMpdata)
{
    std::set<int> ids;
    PdBlob blob;
    if (!blob.Load(s, storedMpdata, "mpdata")) return ids;
    const std::vector<PdField> &fields = blob.def->fields;
    for (size_t i = 0; i < fields.size(); i++) {
        const PdField &f = fields[i];
        if (f.member == "weaponClasses") {
            const std::string &p = f.prop;
            if (p == "primaryBase" || p == "secondaryBase" || p == "primaryAttach" || p == "secondaryAttach" || PdItemPlusOne(p)) {
                int v = PdClassItemId(p, blob.GetInt(f)); /* own the id the client looks up, not the raw + 1 */
                if (v > 0) ids.insert(v);
            }
        } else if (f.member == "characterClasses" && f.prop == "slots") {
            int64_t v = blob.GetInt(f);
            if (v > 0) ids.insert((int)v);
        }
    }
    static const char *const callsign[] = { "cardBackground", "cardIcon", "cardNameplate", "cardTitle" };
    for (int i = 0; i < 4; i++) {
        const PdField *f = blob.def->Find(callsign[i]);
        if (f && f->kind != PD_FLOAT && blob.GetInt(*f) > 0) ids.insert((int)blob.GetInt(*f));
    }
    return ids;
}

} // namespace dw

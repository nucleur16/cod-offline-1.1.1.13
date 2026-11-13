/*
 * dw_playerdata.h - the player profile blob codec (store/playerdata_editor.py: BlobDef, Field, Blob).
 *
 * The two Demonware LiveStorage blobs of a CODOL profile:
 *   "mpdata"    mp/playerdata.def        version 220, formatChecksum 0xE4FD5218  (client-owned, uploaded)
 *   "mpdata_s"  mp/playersecuredata.def  version  11, formatChecksum 0x096B8F57  (server-authored)
 * Both are zlib streams that inflate to exactly 32768 bytes:
 *   [0x00] u32 CRC32 of buffer[4:]   [0x04] u32 def version   [0x08] u32 def formatChecksum   [0x0C..] DDL body
 * The layout comes from the resource playerdata_defs.proposed.json (an exact parse of both
 * StructuredDataDefSet assets). Byte mapping, MEASURED:
 *   non-bool leaf: byte = offsets[leaf] + 4
 *   bool leaf:     offsets[leaf] is a BIT offset -> byte = 4 + bit / 8, mask = 1 << (bit & 7)
 * Every offset is derived from the def's own `offsets` table; nothing here is a hand-written offset
 * except the two guarded record arrays named below.
 */
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include "dw.h"

namespace dw {

enum { PD_BUF_SIZE = 32768 };
enum PdKind { PD_INT, PD_BYTE, PD_BOOL, PD_FLOAT, PD_SHORT, PD_ENUM, PD_STRING };

/* One scalar leaf of a DDL buffer (playerdata_editor.Field). */
struct PdField {
    std::string path;      /* "weaponClasses[3].primaryAttach[0]" */
    std::string member;    /* owning root member */
    std::string prop;      /* leaf-most property name ("primaryAttach", "rank", ...) */
    int kind;              /* PdKind */
    int enumIndex;         /* def enum index when kind == PD_ENUM, else -1 */
    int leaf;              /* flattened scalar-leaf index */
    int byte;              /* absolute byte offset into the 32768-byte buffer */
    int bit;               /* bit within that byte for PD_BOOL, else -1 */
    std::vector<int> indices; /* array indices, outermost first */
    PdField() : kind(PD_INT), enumIndex(-1), leaf(0), byte(0), bit(-1) {}
};

/* An expanded StructuredDataDef (playerdata_editor.BlobDef). */
struct PdDef {
    std::string key, asset;
    uint32_t version, formatChecksum;
    int size, leafCount;
    std::vector<PdField> fields;            /* in leaf order: index == leaf */
    std::map<std::string, int> byPath;      /* path -> index into fields */
    struct Member { std::string name; int firstLeaf, count, arrayCount; };
    std::vector<Member> members;            /* root members; arrayCount = element count of an indexedArray, else -1 */

    const PdField *Find(const std::string &path) const;  /* NULL when the def has no such leaf */
    std::vector<const PdField *> MemberFields(const std::string &member) const;
    int ArrayCount(const std::string &member) const;     /* -1 unless the member is an indexedArray */
};

/* The def for "mpdata" / "mpdata_s", parsed once from playerdata_defs.proposed.json; NULL when the
 * resource is missing or does not expand to its own leafCount (never guessed). */
const PdDef *PlayerDataDef(Server &s, const std::string &key);
/* Which def wrote this inflated buffer, from its header: "mpdata", "mpdata_s" or "". */
std::string PdIdentify(Server &s, const Bytes &buf);
void PlayerDataFree(Server &s);

/* A loaded profile: the 32768-byte inflated buffer plus its def (playerdata_editor.Blob). */
struct PdBlob {
    const PdDef *def;
    Bytes buf;
    PdBlob() : def(NULL) {}

    /* Blob.from_file on stored (zlib) bytes. key "" = identify from the header. An "mpdata" buffer
     * has its legacy eye-first characterClasses rows moved to the canonical slot order
     * (catalogue/character_slots.normalize_character_buffer), as the Python loader does.
     * -> false with *err set when the bytes are not a profile of a known def. */
    bool Load(Server &s, const Bytes &stored, const std::string &key = "", std::string *err = NULL);
    /* Blob.blank: correct header, zero body, correct CRC. */
    bool Blank(Server &s, const std::string &key);
    bool ok() const { return def != NULL && buf.size() == PD_BUF_SIZE; }

    /* Field access. A bool reads 0/1, a float its value, the integer kinds their signed value. */
    double Get(const PdField &f) const;
    int64_t GetInt(const PdField &f) const { return (int64_t)Get(f); }
    /* -> false (buffer untouched) when the value does not fit the field (Blob.set raising ValueError). */
    bool Set(const PdField &f, double value);
    bool GetPath(const std::string &path, double &out) const;
    bool SetPath(const std::string &path, double value);

    uint32_t StoredCrc() const;
    uint32_t ComputedCrc() const;
    void FixCrc();
    /* Blob.save's bytes: CRC fixed, deflate level 1 (the client's own 78 01 header), verified to
     * inflate back to the buffer. */
    bool Store(Bytes &out);
};

/* ---- weaponClasses item ids (playerdata_editor.class_item_id / class_item_raw) ----
 * perks / killstreaks / deathstreak / lethal / tactical / special / meleeWpn are stored as item id + 1
 * (0 = empty slot, 1 = the item "none"); primaryBase / secondaryBase / *Attach are plain ids. */
bool PdItemPlusOne(const std::string &prop);
/* -> the item id, or -1 for an empty slot / "none". */
int PdClassItemId(const std::string &prop, int64_t raw);
/* item id (-1 = "none") -> the value weaponClasses stores for `prop`. */
int PdClassItemRaw(const std::string &prop, int itemId);

/* Every item id a stored mpdata references: weaponClasses bases / attachments / perks / killstreaks /
 * equipment, characterClasses slots and the four callsign scalars (dw_marketplace._equipped_item_ids).
 * Empty when the blob cannot be decoded. */
std::set<int> PdEquippedItemIds(Server &s, const Bytes &storedMpdata);

/* ---- the character item table (catalogue/model_configstrings.cha_item_names) ----
 * itemId -> (itemName, part) from items_cha.txt, items_cha_unlockall.proposed and the measured ids. */
/* ---- the weapon attachment table (catalogue/weapon_loadout.attach_table) ----
 * mp/coWeaponAttachmentTable.csv as the client and the match server read it: weapon -> attachment ->
 * the weapon level that unlocks it (0 = from the start). Resource weapon_attachments.txt. The Python
 * lobby uses it for the legality checks of its profile tools (store/playerdata_validate.py), not on
 * any request path; here it backs the same two questions. -> 1 yes, 0 no, -1 cannot tell. */
int PdWeaponRegistered(Server &s, const std::string &weapon);
int PdAttachmentLegal(Server &s, const std::string &weapon, const std::string &attachment, int *level = NULL);
/* The whole table, one "weapon,attachment,level" line per row in the resource's order. */
std::string PdAttachTableDump(Server &s);

struct PdItemName { std::string name, part; };
const std::map<int, PdItemName> &PdItemNames(Server &s);

/* ---- characterClasses, decompressed-mpdata offsets (MEASURED, model_configstrings / zm_characters) ---- */
enum {
    PD_CHARACTER_CLASSES_OFF = 0x7E9, PD_CHARACTER_CLASSES_STRIDE = 40, PD_CHARACTER_CLASSES_COUNT = 48,
    PD_ZM_CHARACTER_INDEX_OFF = 0x17F1,
};
/* Slot order of a characterClasses row (catalogue/character_slots.PROFILE_SLOT_ORDER); "" = unused. */
extern const char *const kPdProfileSlotOrder[20];

unsigned PdU16(const Bytes &buf, size_t off);
void PdPutU16(Bytes &buf, size_t off, unsigned v);
uint32_t PdU32(const Bytes &buf, size_t off);
void PdPutU32(Bytes &buf, size_t off, uint32_t v);

} // namespace dw

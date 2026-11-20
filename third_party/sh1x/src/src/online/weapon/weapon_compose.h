#ifndef WEAPON_COMPOSE_H
#define WEAPON_COMPOSE_H
#include "weapon_native.h"
#ifdef OL_SERVER
#include "sv_weapons.h"
#endif
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* CoDOL builds an attachment-equipped weapon from a name, not from a runtime
 * struct: C1's BG_BuildCustomWeaponDef (0x421590) parses
 *
 *     *<tag> <baseWeaponIndex> <flag> [<attachItemIndex> <variant>]...
 *
 * copies the base definition into a fresh custom record (0x4216c4), points the
 * copy's internal name at the record's own name buffer (0x421712), stores the
 * flag, pair count and base pointer (+0x90/+0x91/+0x94), keeps the accepted
 * item names, attachment defs and variants (+0xc94/+0xcf8/+0xd5c), applies each
 * attachment's override entries and then assembles the component models from
 * mp/coWeapModels/<base>.csv and mp/coWeapItems/<item>.csv (0x420320).
 *
 * This module owns the server side of that: one distinct definition per
 * composed name, never a second pointer to the base definition. */
#define COMPOSED_TAG_SIZE 16u   /* C1 record tag field, 0x4216d4. */
#define COMPOSED_NAME_SIZE 128u /* C1 record name field, 0x4216a6. */
#define COMPOSED_ATTACHMENTS 15u
typedef struct {
    char tag[COMPOSED_TAG_SIZE];
    char canonical[COMPOSED_NAME_SIZE]; /* the single spelling emitted and cached */
    unsigned base;                                  /* client weapon index */
    unsigned flag;                                  /* +0x90, also the coWeapModels selector */
    unsigned count;
    unsigned items[COMPOSED_ATTACHMENTS];     /* attachment item indices */
    unsigned variants[COMPOSED_ATTACHMENTS];  /* +0xd5c bytes */
} composed_weapon_t;
/* Exact grammar above. Rejects a missing '*', an empty or oversized tag, a
 * zero base, a byte-range violation in flag/variant and any trailing garbage;
 * a pair without its variant is rejected rather than defaulted. Pairs beyond
 * the array bound are rejected instead of silently dropped.
 * 1 parsed, 0 not a composed name, -1 malformed. Failure preserves out. */
int ParseComposedWeaponName(const char *name,composed_weapon_t *out);
/* The canonical spelling of a composed name: '*', tag, then one space before
 * every number, in the order the name gave them. C1 accepts other spacings of
 * the same fields; this module keys its definitions by this rendering so they
 * resolve to one weapon and one published slot. Item order is never rewritten,
 * because C1 stores and applies the pairs in the order it parsed them.
 * 1 rendered, 0 not a composed name, -1 malformed or no room. */
int CanonicalComposedWeaponName(const char *name,char *out,size_t size);
/* Base weapon name for a composed base index. The published registry is
 * authoritative, because register_stock also registers each definition's
 * partner names recursively and a slot can therefore differ from the table
 * ordinal; the table walk (0 none, 1 defaultweapon_mp, then column 0 of
 * mp/coWeaponAttachmentTable.csv by first appearance, 0x41fb59..0x41fc0c) is
 * only the fallback before the registry exists.
 * 1 resolved, 0 unknown, -1 table unavailable. */
int ComposedWeaponBaseName(unsigned index,const char **name);
/* Attachment item name for the index a composed name carries: the inverse of
 * C1's "wat" row numbering (0x41fa2e, 0x41fac0..0x41fb3d).
 * 1 resolved, 0 no such index, -1 table unavailable. */
int ComposedAttachmentItemName(unsigned index,const char **out);
/* The same row, with its marketplace group (column 3) as well: the group names
 * the mp/coweapitems/<group>.csv table this item's models and bone tag live
 * in. Either output may be null. 1 resolved, 0 no such index, -1 unavailable. */
int ComposedAttachmentItem(unsigned index,const char **name,const char **group);
/* The index and group for an item named rather than numbered, which is what a
 * default or filler item is. C1's marketplace lookups take the last matching
 * row. 1 resolved, 0 not a "wat" item, -1 table unavailable. */
int ComposedAttachmentItemIndex(const char *name,unsigned *index,const char **group);
int ComposedAttachmentGroup(const char *name,const char **group);
/* Column 3 of the mp/coWeaponAttachmentTable.csv (weapon, item) row: the
 * <variant> byte a composed name carries for this pair. The presence of the
 * row is also the legality test, because bg_attachIdx is filled only for the
 * pairs this table lists; an item with no row resolves to nothing on the
 * client whatever the name says.
 * 1 legal, 0 not legal for this weapon (or an unparsable column),
 * -1 table unavailable. */
int ComposedAttachmentVariant(const char *base_name,const char *item_name,unsigned *variant);
/* mp/coweapmodels/<weapon>.csv row <camo>: the base gun's world model (col 2)
 * and view model (col 3), which is the CoDOL runtime art a composed name's
 * flag selects. Either output may be null.
 * 1 row resolved, 0 the weapon's table has no such camo row, -1 the weapon has
 * no table at all - which callers must read as "cannot check", not "absent". */
int ComposedBaseModelRow(const char *base_name,unsigned camo,
    const char **world,const char **view);
/* mp/coweapitems/<group>.csv row (item, weapon, camo): the part's world model
 * (col 3), view model (col 4) and the bone TAG it merges onto (col 5). The
 * exact-pair camo scoring prefers 0 then 1 for a zero request, or the exact
 * camo then 0 for a nonzero request. Other rows remain lower-scoring fallbacks;
 * later rows win ties, as in C1 0x7283dc..0x728421. Any output may be null.
 * 1 resolved, 0 no row for this pair, -1 table unavailable. */
int ComposedItemModelRow(const char *base_name,const char *item_name,
    const char *group,unsigned camo,const char **world,const char **view,const char **tag);
/* The bone tag alone, measured camo-invariant across all of the recipe's
 * (weapon, item) pairs. 0 means the pair merges no model and can therefore
 * never collide with another part, which is the case for every cam_* item and
 * for sco_none, hbs_normal and gps_default. */
int ComposedAttachmentTag(const char *base_name,const char *item_name,
    const char *group,const char **tag);
/* Resolve a weapon-legal, model-producing item for a declared default group
 * whose literal default item has no attachment-table row. The mounted C1
 * tables contain this case (m9_mp declares iro_default but offers
 * iro_glowm9). The result borrows the mounted table. */
int ComposedLegalDefaultReplacement(const char *base_name,const char *group,
    const char **item,unsigned *index,unsigned *variant);
/* Where a composed name's camo flag came from. */
typedef enum {
    CAMO_NONE=0,            /* nothing known; the flag is zero */
    CAMO_ITEM_TABLE,        /* the (item, weapon) row of coweapitems/cam.csv */
    CAMO_ITEM_TABLE_GLOBAL, /* that item's id on the weapons it is listed for */
    CAMO_ATTACH_COLUMN,     /* last resort: coWeaponAttachmentTable column 3 */
    CAMO_NO_MODEL_ROW       /* id known but no coweapmodels row; flag forced to zero */
} camo_source_t;
/* The camo id a cam_* item contributes as this weapon's flag. It comes from
 * mp/coweapitems/cam.csv - the table the client scores every cam_* pair
 * against, whose column 2 is the coweapmodels row space - and NOT from the
 * pair's attachment-table variant, which is a different, write-only byte that
 * is zero for every premium camo. Pass the pair's variant, or a negative value
 * for none, as the last resort for an item the camo table does not list.
 * An id with no base model row degrades to zero so the client is never asked
 * for a gun mesh that is not there.
 * 1 resolved (flag may legitimately be zero), 0 no item given,
 * -1 the camo table is present but malformed. */
int ComposedCamoId(const char *base_name,const char *cam_item,int variant,
    unsigned *camo,camo_source_t *source);
/* One (slot, default item) group of a weapon's mp/coweapitemswea.csv record.
 * Both strings borrow the mounted table. */
typedef struct { const char *slot,*item; } weapon_default_slot_t;
/* The weapon's own declared default parts: the groups of its coweapitemswea
 * record whose flag is exactly "y". This is CoDOL's own answer to what the gun
 * wears when nothing is chosen, and it is per weapon, unlike any hand-picked
 * filler list. Weapons with no groups at all (at4_mp, stinger_mp, javelin_mp,
 * riotshield_mp) legitimately answer zero.
 * count is always the number found, even when it exceeds max.
 * 1 all written, 0 the table has no record for this weapon, -1 unavailable,
 * malformed or more groups than max. */
int WeaponDeclaredDefaults(const char *base_name,weapon_default_slot_t *out,
    unsigned max,unsigned *count);
/* The type-43 attachment asset for one (base weapon, attachment item) pair:
 * column 2 of the mp/coWeaponAttachmentTable.csv row whose column 0 is the
 * weapon and column 1 the item, which is what C1 resolves into
 * bg_attachIdx[weapon<<8|item] (0x41fb59..0x41fc96).
 * 1 resolved, 0 the pair has no row, -1 table unavailable. */
int ComposedAttachmentAssetName(const char *base_name,unsigned item_index,const char **out);
/* Build (or return the cached) definition for a composed name. The definition
 * is a distinct owned copy of the base weapon's native core whose internal
 * name is the composed name, so every pointer-keyed consumer sees a separate
 * weapon. 1 ready, 0 not a composed name, -1 unresolvable base or build
 * failure. Failures are not cached. */
int LoadComposedWeapon(const char *name,const struct WeaponDef **out);
/* Register adjusted ammo names and flags from the retained image. Resolve the
 * base index against this registry, including during unpublished replay.
 * Requires canonical name spelling and an earlier stock base slot. */
int RegisterComposedWeaponKeys(weapon_registry_t *,unsigned,const char *);
/* Current cached definition only, used after registration to enumerate its
 * adjusted partner names. Output borrows that composed generation. */
int ComposedWeaponTail(const char *,weapon_tail_t *);
/* The parsed record behind a composed definition, for consumers that need the
 * attachment identity. 1 found, 0 not a composed definition. */
int ComposedWeaponRecord(const struct WeaponDef *,const composed_weapon_t **);
/* The base definition a composed definition was copied from. Metadata queries
 * that this module cannot yet adjust per attachment resolve through this, so a
 * caller can tell base-derived metadata from composed metadata. */
int ComposedWeaponBaseDefinition(const struct WeaponDef *,const struct WeaponDef **);
/* The CoDOL runtime art a built composed definition's flag selects: the
 * mp/coweapmodels/<base>.csv world and view gun meshes the client assembles it
 * from. Either output may be null, and both are null when the weapon has no
 * recipe table or its flag has no row there. 1 art resolved, 0 not a composed
 * definition or no row. */
int ComposedWeaponBaseArt(const struct WeaponDef *,const char **world,const char **view);
/* The composed definition's own core, for the pointer-keyed metadata queries
 * (silenced, hide tags, perks inheritance, timings). The core is converted
 * from the base image after this name's attachment definitions were applied to
 * it, so these values are the composed weapon's own, not the base's. */
const native_weapon_core_t *ComposedWeaponCore(const struct WeaponDef *);
/* Whether this definition was converted from an image the attachment
 * transaction had been applied to. A caller can therefore tell an adjusted
 * definition from one that is not, instead of assuming. 1 found, 0 not a
 * composed definition. */
int ComposedWeaponAdjusted(const struct WeaponDef *,int *out);
/* Why the last LoadComposedWeapon failed, for callers that report it.
 * Empty after a success. Borrowed, replaced by the next failure. */
const char *ComposedWeaponError(void);
/* Archive replacement: move every current definition to the retired list.
 * Nothing is freed, because live native code holds these definition pointers
 * exactly as it holds the retired archives' own weapon cores. New lookups
 * build fresh definitions against the new mount. */
void RetireComposedWeapons(void);
/* Total unmount: free current and retired definitions together, after which no
 * previously returned definition pointer may be used. */
void ResetComposedWeapons(void);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif

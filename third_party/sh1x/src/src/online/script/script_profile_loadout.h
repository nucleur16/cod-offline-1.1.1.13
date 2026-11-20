#ifndef SCRIPT_PROFILE_LOADOUT_H
#define SCRIPT_PROFILE_LOADOUT_H
#include "assets.h"
#include "weapon_compose.h"
#pragma pack(push, 8)
/* CoDOL stores a multiplayer loadout in mp/playerdata.def's weaponClasses:
 * eight records of forty-six shorts, every one a marketplace item id with -1
 * for an empty slot. This module reads that record through the mounted
 * definition and resolves the ids against mp/coMarketplaceItems.csv. It does
 * not substitute defaults for empty or unresolvable slots.
 *
 * Appearance is the separate characterClasses[].slots[] path already served by
 * ComposeProfileCharacter, and is not duplicated here. */
#define LOADOUT_CLASSES 8u
#define LOADOUT_ATTACHMENTS 15u
#define LOADOUT_PERKS 5u
#define LOADOUT_KILLSTREAKS 4u
/* Two weapon slots per class, each with its own base and fifteen grouped
 * attachment slots. Slot order is the definition's own array order. */
#define LOADOUT_WEAPONS 2u
typedef struct {
    int base[LOADOUT_WEAPONS];
    int attachments[LOADOUT_WEAPONS][LOADOUT_ATTACHMENTS];
    int perks[LOADOUT_PERKS];
    int killstreaks[LOADOUT_KILLSTREAKS];
    int deathstreak,lethal,tactical,special,melee;
} profile_loadout_t;
/* Read one class record from this client's shared profile view. Every member
 * must resolve to a short in the mounted definition; a missing profile,
 * unmounted definition or a member of another type fails the whole read.
 * 1 loaded, 0 unavailable, -1 definition/profile mismatch. Failure preserves
 * out, and stored -1 slots stay -1. */
int ReadProfileLoadout(unsigned client,unsigned class_index,profile_loadout_t *out);
/* Marketplace row for a stored id: column 1 name, column 2 type ("wea" for a
 * weapon, "wat" for an attachment) and column 3 group. Ids that mean "empty"
 * (-1 and 0) return 0 without touching the outputs, an id with no row returns
 * 0, and an unmounted or malformed table returns -1. Strings borrow the
 * mounted table. C1's marketplace lookup takes the last matching row. */
int LoadoutItemName(int item,const char **name,const char **type,const char **group);
/* Attachment item index for a stored id, in the numbering C1 builds in
 * BG_InitWeaponDefs (0x41fa2e sets the count to one, the row loop at
 * 0x41fac0..0x41fb3d appends column 1 of every row whose column 2 is "wat"),
 * so the index is one plus the row's position among the "wat" rows.
 * 1 resolved, 0 not an attachment row, -1 table failure. */
int LoadoutAttachmentItemIndex(int item,unsigned *index);
/* Base weapon index in the client's own numbering: 0 none, 1 defaultweapon_mp,
 * then column 0 of mp/coWeaponAttachmentTable.csv in row order, each distinct
 * name taking the next index. C1 builds this in the same function's second
 * pass (0x41fb59..0x41fca4, registration through BG_GetWeaponIndexForName at
 * 0x41fc04). 1 resolved, 0 unknown name, -1 table failure. */
int LoadoutWeaponIndex(const char *name,unsigned *index);
/* Where a part in a composed loadout weapon came from. */
typedef enum {
    LOADOUT_PART_EQUIPPED=0, /* the class's own choice, never dropped */
    LOADOUT_PART_FILLER,     /* a group the class left empty */
    LOADOUT_PART_DECLARED    /* the weapon's own declared default */
} loadout_part_origin_t;
typedef struct {
    unsigned item;    /* attachment item index, as emitted in the name */
    unsigned variant; /* coWeaponAttachmentTable column 3 for this pair */
    const char *name,*group; /* borrowed marketplace strings */
    const char *tag;  /* bone tag it merges onto, null when it merges no model */
    loadout_part_origin_t origin;
} loadout_part_t;
/* Everything the recipe decided for one side of one class, so a caller can
 * report or audit the build instead of only seeing the name. Every string
 * borrows the mounted tables. */
typedef struct {
    char name[COMPOSED_NAME_SIZE]; /* the composed name, C1's own bound */
    const char *base_name;
    unsigned base_index,flag;
    /* Profile item identity above remains intact when an audited equivalent
     * existing asset supplies the runtime composition. */
    const char *composition_base_name;
    unsigned composition_base_index;
    const char *camo_item;          /* the cam_* item that set flag, or null */
    camo_source_t camo_source;
    /* mp/coweapmodels/<base>.csv row flag: the CoDOL runtime art the client
     * builds this gun from. Null when the weapon has no recipe table. */
    const char *base_world_model,*base_view_model;
    loadout_part_t parts[COMPOSED_ATTACHMENTS];
    unsigned count;
    unsigned skipped_illegal;   /* equipped items with no row for this weapon */
    unsigned skipped_duplicate; /* the same item stored in two slots */
    unsigned skipped_full;      /* additions the name had no room for */
    unsigned collisions;        /* equipped parts sharing one bone tag */
    unsigned missing_defaults;  /* declared default groups still uncovered */
} loadout_recipe_t;
/* Compose the custom weapon name C1's BG_BuildCustomWeaponDef (0x421590)
 * parses: '*' tag, base weapon index, flag byte, then one item/variant pair
 * per attachment, with the full CoDOL recipe behind it rather than the bare
 * slot list:
 *   - <flag> is the camo id of the class's equipped cam_* item, taken from
 *     mp/coweapitems/cam.csv and validated against the weapon's own
 *     coweapmodels rows (ComposedCamoId), not the pair's variant byte.
 *   - each pair carries its real coWeaponAttachmentTable variant. An equipped
 *     item absent from that weapon's table is an explicit recipe error.
 *   - empty groups receive the weapon's own declared defaults; generic filler
 *     recipes and unproven tag-collision filters do not override the profile.
 *   - all selected items and required defaults must fit, or composition fails.
 * Empty attachment slots are skipped and every populated one is considered, so
 * a fifteen-slot CoDOL loadout is not truncated to IW4's two.
 * Failure preserves out. 1 composed, 0 empty base or a base that is not a
 * "wea" row, -1 table failure, an unresolvable stored id, or no room for an
 * equipped item. */
int ComposeLoadoutWeaponRecipe(int base_item,const int *attachments,unsigned count,
    loadout_recipe_t *out);
/* The name alone, for callers that do not need the audit. Same statuses. */
int ComposeLoadoutWeapon(int base_item,const int *attachments,unsigned count,
    char *out,size_t size);
/* 1 when every declared default group this recipe still leaves uncovered is one
 * the weapon's own C1 tables cannot fill at all (wa2000_mp's gri): the build is
 * complete and legal and must be published, not degraded to the stock base,
 * whose gunXModel[0] is the shipped IW4-style body. 0 when any uncovered group
 * had a legal pair or a legal replacement, which is a composer defect and still
 * rejects the recipe. Shared by the bot and the human loadout bridges. */
int LoadoutMissingDefaultsUnconstructible(const loadout_recipe_t *recipe);
#pragma pack(pop)
#ifdef OL_SERVER
#include "scr_vm.h"
/* Native bridge for maps/mp/gametypes/_class.gsc. IW4's getters cannot express
 * a fifteen-slot grouped attachment record, so the script calls these instead;
 * which CoDOL member feeds which IW4 slot is decided in the adapted script,
 * not hidden in here. */
void Scr_GetLoadoutItem(scr_entref_t);
void Scr_GetLoadoutPerk(scr_entref_t);
void Scr_GetLoadoutKillstreak(scr_entref_t);
void Scr_GetLoadoutAttachment(scr_entref_t);
void Scr_GetLoadoutAttachments(scr_entref_t);
void Scr_GetLoadoutCamo(scr_entref_t);
void Scr_GetLoadoutWeapon(scr_entref_t);
void Scr_GetPartyWeapon(void);
void Scr_IsPartyAttachment(void);
/* The marketplace type column of a stored slot, so a script can tell what a
 * slot holds instead of guessing from its name. */
void Scr_GetLoadoutItemType(scr_entref_t);
/* Weapon-name helpers for composed names, which carry no underscores for
 * IW4's getBaseWeaponName/getWeaponAttachments to split. */
void Scr_GetWeaponBaseName(void);
void Scr_GetWeaponAttachmentNames(void);
#endif
#endif

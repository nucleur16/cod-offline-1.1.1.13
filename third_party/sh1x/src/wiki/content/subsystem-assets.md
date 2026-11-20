---
slug: subsystem-assets
title: Assets and streaming
space: subsystems
summary: src/codol/assets — reading fastfiles and encrypted IWDs, the mounted-view pattern, character composition, and the readers for models, animations, sounds and tables.
position: 80
tags: [assets]
---

This area reads the game's own data: zone files (fastfiles), encrypted IWD
archives, and the assets inside them. It is the second largest CoDOL area and
the one with the most surface, because every asset family needs its own reader.

## Zones and assets

```c
typedef struct {
    uint8_t *data; size_t size, table_offset;
    uint32_t asset_count, string_count, block_size[8];
} codol_zone_t;

typedef struct { uint32_t type; const uint8_t *data; size_t size; } codol_asset_t;
```

```c
int  CODOL_OpenZoneFile(codol_zone_t *out, const char *path);
int  CODOL_ZoneAsset(const codol_zone_t *zone, unsigned index, codol_asset_t *asset);
void CODOL_CloseZone(codol_zone_t *zone);
```

A zone owns a copy of its image; assets are **borrowed spans into it** and stay
valid until `CODOL_CloseZone`. A failed open leaves the output untouched.

The eight `block_size` entries matter: a fastfile is a serialized heap split
across numbered blocks, and pointer fields inside an asset are encoded
references into those blocks, not native pointers. `codol_load_blocks.h`
reconstructs the blocks so that references can be resolved — and the header
repeats the warning every time, because an encoded reference that is mistaken
for a pointer is a crash with a very confusing stack.

## Script strings and the null/empty distinction

```c
int CODOL_ReadScriptStrings(const codol_zone_t *zone, codol_script_string_t **out);
```

> Preserve null and unresolved references as `NULL`, with their original value;
> neither is interchangeable with an explicitly empty inline string.

That three-way distinction — absent, unresolvable, explicitly empty — runs
through this entire area and through [config
strings](/wiki/subsystem-snapshot) as well.

## Mounted views, not native structures

The pattern this area uses everywhere: a reader returns a **mounted view** over
the serialized bytes with the strings it needs resolved, and the conversion to a
native engine structure is a separate, explicit step.

```c
typedef struct {
    codol_asset_t asset;
    const char *strings[4];
    size_t dependency_offset;
} codol_mounted_weapon_t;
```

> Replayed serialized weapon view, **not an IW3 `WeaponDef`**. The four leading
> string references are resolved; all other pointer fields remain serialized.
> Never publish `asset.data` as a native weapon.

The same shape appears for animations (`codol_mounted_xanim_t`, with three
tracks and six streams), vehicles (`codol_mounted_vehicle_t`, 47 sound slots),
models (`codol_mounted_model_skeleton_t`) and weapon attachments.

Two views exist over tables and rawfiles specifically so retained IW3 code can
consume them unchanged:

```c
typedef struct { const char *name; int columnCount, rowCount; const char **values; } codol_engine_table_t;
typedef struct { const char *name; int len; const char *buffer; } codol_engine_rawfile_t;
```

Both live until archives are unmounted. Rawfile bytes **may contain NULs**;
`len` is exact, with an additional sentinel.

## Encrypted IWDs

```c
int      CODOL_InitIwdFilesystem(const char *client_exe);
int      CODOL_IsEncryptedIwd(const char *path);
unzFile  CODOL_OpenIwdArchive(const char *path);
int      CODOL_ReadIwdKey(const char *client_exe, uint8_t out[24]);
int      CODOL_IwdCryptDataChunk(const uint8_t key[24], uint32_t archive_key,
                                 uint32_t offset, const uint8_t *, uint8_t *, size_t);
```

The 24-byte key is read from the client executable. Two details are the whole
difficulty of this format:

- The **archive key is the CRC32 of the archive basename, including `.iwd`**,
  and filenames are keyed by their central-directory ordinal.
- Data is processed in **0x4000-byte chunks using each chunk's absolute file
  offset** — *never* reseeded at a caller's read boundary. A reader that
  restarts the keystream wherever the caller happened to ask for bytes produces
  plausible-looking garbage.

`CODOL_InitIwdFilesystem` failure clears previous key state, but existing ZIP
handles own their keys and remain independently usable.

## Streaming

`codol_asset_stream.h` is a set of pure traversal functions that advance a
cursor over a serialized dependency without creating anything:

```c
int CODOL_AdvanceImageStream(const uint8_t *, size_t, size_t *);
int CODOL_AdvanceMaterialStream(const uint8_t *, size_t, size_t *);
int CODOL_AdvanceModelSurfacesStream(const uint8_t *, size_t, size_t *);
int CODOL_AdvancePhysicsPresetStream(const uint8_t *, size_t, size_t *);
```

No GPU objects are created — this is a dedicated server. Positions advance only
on success, and an unsupported nested asset fails explicitly rather than being
skipped by a guessed size.

## Models

`codol_xmodel.h` splits an XModel into the parts a server actually needs:
skeleton and bone names, bounds and bone info, LODs, and collision surfaces.

```c
typedef struct {
    float mins[3], maxs[3];
    int boneIdx, contents, surfFlags;
    uint32_t triangle_count;
    float (*triangles)[12];
} codol_xmodel_collision_surface_t;
```

Unresolved references are counted (`unresolved`) rather than dropped, the same
convention as [collision](/wiki/subsystem-collision).

## Character composition

A CoDOL character is assembled from profile items rather than picked from a
list:

```c
int CODOL_ComposeProfileCharacterAuto(const uint8_t *profile, size_t size,
                                      const codol_structured_schema_t *schema,
                                      const char *faction, unsigned gender,
                                      const char *camo, unsigned rig_selector, ...);
```

Composition takes the player's profile plus a visual context — faction, gender,
camouflage, skin tone, chest-rig selector — and resolves each slot to an item
and a model name through the `coCharItems` tables. C1's own row-selection rules
are reproduced exactly: gender `0` male / `1` female, faction `0` tf / `1` sc /
`2` zm, and **only exact or shared variants are eligible**.

The `Auto` variant takes the skin tone from the profile's own head item, as C1
derives it, and reports the resolved tone back. Camouflage stays an explicit
input because C1 keeps it in the character context.

A `1` return means **name resolution finished** — including sparse selections.
It does not guarantee a complete outfit or that the geometry exists. Dropping
unmounted parts happens later, in
[`CODOL_SelectClientCharacter`](/wiki/subsystem-player).

`CODOL_CharacterSlotForGroup` maps a C1 UI group to a profile/composition slot.
The header is careful to say this is **not DObj attachment ordering** — a
different question with a different answer.

## The rest

| File | What it reads |
|---|---|
| `codol_fastfile.c` | The zone container itself. |
| `codol_asset_registry.c`, `codol_asset_commands.c` | What is mounted, and the console commands to inspect it. |
| `codol_material_native.c`, `codol_sound_native.c`, `codol_effects_native.c` | Per-family native conversion. |
| `codol_xanim_native.cpp`, `codol_sv_xanim.cpp` | Animations and their server-side evaluation. |
| `codol_stringtable.c` | CSV tables, exposed as `codol_engine_table_t`. |
| `codol_shock_rawfile.c` | Shellshock rawfiles. |
| `codol_map_custom.c` | Per-map custom data. |
| `codol_zone_path.c` | Where zones are looked for. |
| `codol_client_crypto.c`, `codol_iwd_crypto.c` | Key derivation and archive decryption. |

Browse every declaration in this area: [assets](/reference/areas/assets).

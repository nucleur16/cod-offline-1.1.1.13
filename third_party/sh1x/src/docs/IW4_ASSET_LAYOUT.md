# Server assets in the C1/IW4 layout (branch `feat/iw4-asset-layout`)

Goal (user, 2026-10-27): the server builds and holds its assets in C1's IW4-derived layout instead of IW3's.

- The in-process host in Kernel33 (`feat/p2p-inprocess`, `sh1x_host.dll` in the client) can then resolve assets
  from the client's own loaded zones instead of loading a second copy.
- The dedicated exe keeps working from the same layout through its own loader.

Status: **stage 1 (inventory and measurement) and the PMem part of stage 4 are done**. Stages 2-3 are the plan below.

## How assets get into memory today

- **Startup.** `R_Init` mounts the startup archives through `LoadStartupArchives`.
- **Map load.** `SV_SpawnServer` calls `LoadMapArchives`, then `CM_LoadMap` and `Com_LoadWorld`. The retained
  `DB_LoadXAssets` is not used under `OL_SERVER`.
- **Per zone** (`fastfile.c`, `load_blocks.c`):
  1. `OpenFastFile` decrypts with AES-CTR (the key from C1.exe) and inflates the zone into one malloc'd image.
     The image is kept for the archive's lifetime.
  2. `ReplayZone` rebuilds C1's eight XZoneMemory blocks with calloc. Pointers stay encoded as
     `(block<<28|offset)+1` and are never patched.
  3. **These replayed blocks are already the C1 layout.** Every `Read*` / `CODOL_Replay*` reader decodes
     C1's serialized structures.
- **Conversion.** Per-archive caches (`archive_t`, `asset_registry.c`) convert named assets on demand into
  IW3 engine structs: XModel, `clipMap_t`, WeaponDef, XAnimParts, `snd_alias_list_t`, FxEffectDef, Material,
  PhysPreset, StringTable, RawFile and ComWorld.
- **Lookup.** `DB_FindXAssetHeader` (`db_load.cpp`) hands out the converted IW3 struct. The retained asm reaches
  it through `DB_FindXAssetHeaderReal`.
- **PMem and hunk.** No CoDOL asset path allocates from PMem or the hunk. PMem holds only `$init` (1.5 MB), and
  the hunk is 10 MB.

## Measured memory (sh1x_mem, dedicated exe, this branch, 2026-10-27)

`sh1x_mem` (rcon/console) is new on this branch. It prints:
- PMem by allocation name, PMem used/size, and the hunk;
- per mounted archive set: zone images, replayed blocks, and per C1 asset type the record count and serialized
  bytes;
- process private bytes and working set.

| map | private | zone images | replayed blocks | largest types (serialized MB) |
|---|---|---|---|---|
| mp_killhouse_sh (war) | 969 MB | 251.1 MB | 185.3 MB | gfx_map 84.5, sound 66.7, xanim 30.4, xmodel 19.1, rawfile 15.5, techset 11.5, stringtable 5.5, fx 5.4, col_map_mp 5.1, weapon 2.2 |
| mp_rust (dm) | 825 MB | 198.6 MB | 132.0 MB | sound 66.7, gfx_map 39.6, xanim 30.4, xmodel 17.0, rawfile 15.5, techset 8.9 |
| mp_crash_tropical (coop) | 1018 MB | 299.6 MB | 195.1 MB | sound 112.9, gfx_map 57.5, xanim 57.5, xmodel 23.6, techset 16.2, rawfile 15.5 |

The rest of the private bytes is the image's 323 MB .bss (retained IW3 static arrays) plus the converted IW3
caches.

Before this branch, the fastfile block (PMem, 328 MB) was committed in full at start-up. A separate server
process measured 1.32 GB private before and 0.97 GB after (stage 4, below).

The zm probes (zm_theater_sh, zm_factory_sh) do not reach a running level at this base. `maps/_zombiemode_tokens.gsc`
line 139 calls `codolRequestReviveTokenSpend`, which no server branch implements. The rawfiles pointer at
`origin/main` (1c10bb3) carries that script; the live tree (3f91e86) does not. This is outside this work.

## Consumers that read IW3 offsets (the conversion cost)

- **clipMap.** About 70 `mov reg, cm` reads in the retained asm (`_cm_trace` 22, `_phys_world_collision` 16,
  `_cm_mesh` 14, `_DynEntity_load_obj` 11, ...) cover at least 25 field offsets. Brush stride is 80. C1's
  collision is IW4-shaped: 12-byte materials, 8-byte sides, 40-byte leaves, split brush bounds and contents
  (`ReadCollisionLayout`).
- **XModel.**
  - `_xmodel_utils.asm` reads +0x4, +0x1c, +0x20, +0x2c, +0xa0, +0xc4.
  - Also read by `_dobj*`, `_xanim*`, `_phys_*` and `_cm_staticmodel`.
  - C1's header is 0x168 with 56-byte LODs; IW3's is 220 with 28-byte LODs.
- **WeaponDef.** `bg_weaponDefs` has 127 refs and `BG_GetWeaponDef` 131 calls in 18 asm files, plus 110 C calls.
  C1's serialized offsets equal its runtime ones; `checkpmovedef` checks them against the client's
  `bg_weaponDefs` at 0x0150D720.
- **XAnimParts.** The xanim evaluator (`_xanim.asm` 13.4k lines, `_xanim_calc.asm` 5.4k) reads IW3 parts (88
  bytes). C1's parts use IW4 order with delta3D quaternions.
- **StringTable / RawFile.** `_com_stringtable.asm` reads `char**` cells. C1 has {string, hash} cells and
  compressed rawfiles.
- **Script strings.** Bone names, hide tags and notetracks are re-acquired as **server** SL handles. A client
  pool's `uint16` handles index the client's SL table, which the server cannot read.

## Plan for stages 2-3

**Stage 2: hold C1-layout records and stop converting.**
- Per asset type, in order of memory and risk:
  1. sound (names, aliases and flags only);
  2. rawfile / stringtable (accessors over the C1 cells);
  3. weapon (the C1 offsets are already known and checked);
  4. xmodel (an accessor layer for the asm offset sites);
  5. clipMap (accessor shims for the ~70 asm reads, or a C rewrite of `CM_*` over the IW4 shape);
  6. xanim, last.
- **Retained asm.** It keeps compiling against IW3 shapes through accessor shims. Each shim reads the C1 record
  and fills the few IW3 fields that site needs, until that asm is rewritten.
- **The dedicated loader.** It keeps replaying zones into C1-layout blocks. It patches the encoded references into
  real pointers (a pointer-fixup pass), so a C1 record in the server looks the same as one in the client's
  memory. That is the precondition for stage 3.
- **Drop the retained zone images** once nothing reads them after replay. That is 198-300 MB per map.

**The pointer-fixup pass (2026-10-27, `c1Fixups 1`).**
- `ReplayZone` records every pointer word C1's loader relocates, as `fixup_t {slot, target, size,
  kind}` with encoded addresses. Kinds: loaded data, an asset header (with its type), an alias word
  (`DB_ConvertOffsetToAlias`), a sound alias name, and a name C1 resolves to a header (`FxEffectDefRef`).
- Each reservation that reads source bytes is a chunk (source range to block address), so a field read at
  source address X lies at a known loader address. Pooled references get the size of the placement they point at.
- When an asset's loader finishes, its header is copied out of the temporary block 0 into an arena (block 8),
  as `DB_AddXAsset` copies it into C1's pool. The header's fix-ups move with it.
- Nothing is written into the blocks, so every existing reader is unchanged. With the cvar off (the default)
  nothing is collected. On the probe maps it costs 22-32 MB for 0.75-1.0 M fix-ups and 41-54 k header copies.
- Annotations mark what C1 changes itself, so a comparison skips it:
  - script-string arrays C1 converts to its own handles: bone names, xanim names and notetracks, weapon hide tags
    and notetrack maps;
  - bytes C1 writes while it loads or registers: XSurface zone handle, GfxDrawSurf, PhysPreset type, XModel lod
    runtime words, the surface count and +0x15D, clipMap isInUse, the techset remap, LoadedSound driver handles,
    the alias picker state, the weapon ammo/clip indices, and streamed lod surfaces.
- Host API: `Sh1xHost_FindC1Header`, `Sh1xHost_C1PointersIn` and `Sh1xHost_ResolveC1Pointer`. Kernel33's
  `p2passetcheck <type> * c1` walks the server copy and C1's record together. Data is compared recursively, byte
  for byte, assets by name.

**C1 records with real pointers (2026-10-27).** `C1Record(type, name)` copies a header copy and every
placement it reaches once, writing each pointer word with the address of the copy. Assets link by name, in the
server's link order. `p2passetcheck <type> * m` checks these records against C1's: every type matches as the header
copies did.

**Converters read C1 records (`c1Readers`, 0 off / 1 on / 2 both and compare).** xmodel is done: skeleton,
bone names, collision, bounds and lods come from the record. Self-check on 4 maps: 1743 models, 0 differ.

**Zone images released (`freeZoneImages`, default on, Windows).**
- A zone image is a view of a delete-on-close temporary file (`ZoneImageAllocate`). After the zone's replay
  the view is made read-only and trimmed from the working set (`ZoneImageRelease`). Its pages are
  file-backed, so they are not private commit; the system reads the same bytes back from the file on touch, and
  the address never changes. `ZoneImageTrim` trims touched pages again once a level is loaded.
- A CRC-32 per page is taken at the release. `sh1x_imagecheck` checks every page against it; `sh1x_mem` reports
  the counts. A write into an image faults at the writer (the view is read-only).
- **The 2026-10-27 live failure, and why the design changed.** On 17733 (exe 3d15175b) a reused mp_rust lost every
  rawfile and string-table name in its fourth match; every later map load failed on
  `mp/constConfigStrings_mp.csv`. A dedicated probe reproduced it after two full matches under memory pressure.
  Cause, measured: the temporary-file views (replayed blocks, `fileBackedBlocks`) closed their file handle
  right after mapping. Closing the last handle of a delete-on-close file deletes it at once; the mapping keeps
  working while its pages are resident, but a page the system wrote out and later evicted reads back as zeros (the
  file is gone once the handle closes, GetFileAttributes says so; in a failed probe the image views held zeros
  from their first page, and `sh1x_imagecheck` found 47747 of 73107 pages changed since their release). The first
  image design rebuilt released image pages from those blocks, so the images lost their names too.
  Fixes: every view keeps its file handle until it is unmapped (the file is still deleted then, or when the process
  dies); an image is released only after `FlushViewOfFile` and `FlushFileBuffers` succeed, and a block whose flush
  fails stays resident; zone images are read-only views of their own and no longer rebuild from the replay (no
  image map is collected).
- Regression test: `tests/test_zone_image_win.c` (`make test-zone-image-win`, mingw, run on
  Windows).
- Measured process private:

  | Map | Before | After |
  |---|---|---|
  | killhouse | 976 MB | 730 MB |
  | rust | 831 MB | 637 MB |
  | zm_factory | 1061 MB | 779 MB |
  | crash_tropical | 1025 MB | 727 MB |

- About 0.5 MB of image pages is touched again after a load. A map rotation (killhouse, rust, crash_tropical,
  killhouse) stays at 694-812 MB.
- In-process, a hosted killhouse match ran with 250 MB of images released.

**Converters on C1 records (complete, 2026-10-27).** With `c1Readers 1` every converted type builds from
the record C1 holds: xmodel, xanim, clipMap, weapon, fx, physpreset, collmap, material, xmodelsurfs, ComWorld,
glass map and groups, and addon MapEnts. Mode 2 compares the record build with the serialized one field by field;
on the 5 deploy maps every comparison is 0 differences.

**Replayed blocks in temporary files (`fileBackedBlocks`, default on, Windows).**
- A block of 1 MB or more is a view of a delete-on-close temporary file, so its pages are file-backed, not
  private memory.
- The pages are trimmed after each zone replay and each level load; the system writes them to the file and reads
  them back on touch.
- Process private, with images released as well:

  | Map | Before | After |
  |---|---|---|
  | killhouse | 730 MB | 547 MB |
  | rust | 637 MB | 509 MB |
  | zm_factory | 779 MB | 610 MB |
  | crash_tropical | 727 MB | 535 MB |

- Against the 976/831/1061/1025 MB measured before this work, that is about 45% less.
- A hosted killhouse match: 2629 to 2437 MB private.

**Stage 3: the in-process asset provider.** Started: rawfiles, string tables and sound alias lists of C1's startup
zones come from C1 (`Sh1xHost_SetC1AssetProvider`).
- In `sh1x_host.dll`, archive lookup resolves names in C1's loaded pools (C1 `DB_FindXAssetHeader` 0x4CB8E0; the
  pools at their C1 addresses) instead of mounting zones.
- Script-string fields go through C1's SL table (read-only) into server handles.
- Server-only augmentation stays server-owned. That covers trigger hulls appended as cmodels, the glass tails,
  coop addon map ents and composed `*` weapons.
- The client and server must agree on link order (ffotd and patch ranks). They must also agree on lifetime: the
  client's zone unloads at map change happen while the server is between maps.

**Stage 4 (done here, PMem part).**
- `pmemMegs` (INIT, default 328) sizes the fastfile block.
- On Windows the block is only reserved; each allocation commits its own pages.
- Kernel33 passes `+set pmemMegs 32` for an in-process host. All probed maps used 1.5 MB.
- **Re-hosting.** The user decided on one hosted session per client run, so there is no PMem/hunk re-initialisation.
  A second `p2phost` in the same client is refused with "restart the client to host again". Stopping is the
  existing quit path, which the host API already contains: measured exit code 0, port released, client running.

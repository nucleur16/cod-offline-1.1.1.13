# vlobby_merge: other builds' lobby scenes in 1.1.1.13's virtual lobby

The lobby's 3D scene is the map `mp_ui_viewer` (zone `ui_viewer_mp`). Each scene is a place in that one map; a row
of `mp/cameraPosTable.csv` names its character stand and camera (script_origin targetnames), vision and light.
`src/vlobby_env.c` is the environment switch (`ui_vlobbyEnv`, `vlobby_env`): stock rows (center, urb, woo, des, arc,
zombie) always, the merged scenes when the zone below is installed.

This tool builds `client_shipRetail\chinese\ui_viewer_vlobby_mp.ff`, which Kernel33 opens for `ui_viewer_mp` when it
is there (`VlobbyZoneOpen`): every stock asset of `ui_viewer_mp`, its `mp_ui_viewer` GfxWorld and MapEnts with the
scenes of other builds merged in, and the camera table with their rows (`mp/camerapostable_vlobby.csv`, served for
`mp/cameraPosTable.csv` by `VlobbyFindSubstitute`). Without the file the lobby is stock. The donor images the zone
does not carry go in `main_shared\iw_image_18.iwd` (a name no stock or map pack iwd uses).

The zone and iwd are build outputs (31 MB and 50 MB, made from the other builds' files), not checked in.

| build | scenes | moved by | names |
|---|---|---|---|
| 3.19.26.22 (3.5.2.6 has the same lobby) | lobby (2 cameras), character details, career, champion select, team lobby, zombie champions, DMZ, supply depot, daily reward | x -32768 | `v3_*` |
| 2.6.9.7 | lobby (2 cameras), team lobby | x -16384 | `v2_*` |
| 0.0.2.2 | urban, forest, desert, arctic and zombies scenes (earlier versions of 1.1.1.13's) | x -44032 | `v0_*` |

## Rebuild

1. Port the later builds' lobby worlds with the AssetExplorer (`port-map`; the world is in their `ui_mp`), against
   a 1.1.1.13 install **without the map pack** (a target with the pack's iwds misses images an install without it
   lacks: 84 images against 104 for 3.19.26.22):

        roots.txt: gfx_map:, col_map_mp:, com_map:, fx_map:, game_map_mp:, map_ents:maps/mp/mp_ui_viewer.d3dbsp (one per line)
        CodolSoundDumper port-map --game "<3.19.26.22>" --map ui_mp --target "<1.1.1.13>" --out v31926 --iwd-name iw_image_18.iwd --roots-file roots.txt
        CodolSoundDumper port-map --game "<CODOL_2.6.9.7>" --map ui_mp --target "<1.1.1.13>" --out v2697 --iwd-name iw_image_18.iwd --roots-file roots.txt
        CodolSoundDumper iwd-merge --input v31926/iw_image_18.iwd --input v2697/iw_image_18.iwd --game "<1.1.1.13>" --out iwd --names iw_image_18.iwd

   The merged iwd (113 images, 50 MB) installs as `main_shared\iw_image_18.iwd`.

2. The camera table: `python camera_rows.py <stock> <3.19.26.22> <2.6.9.7> camerapostable_vlobby.csv` with the three
   builds' `mp/camerapostable.csv` (`CodolSoundDumper zone-stringtables --input <code_post_gfx_mp / patch_code_post_gfx_mp .ff> --name camerapos --out <dir>`).

3. Merge (`dotnet build -c Release`; AeBin = the AssetExplorer's built `Codol.*.dll` folder):

        vlobby_merge merge "<1.1.1.13>\client_shipRetail\chinese\ui_viewer_mp.ff" "<1.1.1.13>\codoMP_client_shipRetail.exe" ui_viewer_vlobby_mp.ff ^
          v31926\ui_mp.ff=-32768,0,0,v3_,3.19.26.22 v2697\ui_mp.ff=-16384,0,0,v2_,2.6.9.7 ^
          "<0.0.2.2>\client_shipRetail\chinese\ui_viewer_mp.ff=-44032,0,0,v0_,0.0.2.2,-2500:-12500:-2000:3500:2500:4000,stockgrid" ^
          table=camerapostable_vlobby.csv

   A donor is `<ff>=<dx>,<dy>,<dz>,<prefix>,<tag>[,<keep box x0:y0:z0:x1:y1:z1>[,stockgrid]]` (offsets in whole light
   grid cells: x, y multiples of 32, z of 64). Leave a donor out to build without it.

   Diagnostics: `probe`, `scenes` (static models and surfaces around each stand, any build), `images`, `imgcat`
   (image categories), `check` (surface materials, static model models), `techcheck` (technique pass shaders),
   `bsp` (the dpvs BSP cell of every stand and camera), `dump` / `deref` (asset header bytes, the object a field
   points to). Test switches (`VLOBBY_TEST_*`, `VLOBBY_NO_COMPACT`, `VLOBBY_COMPACT_MAXV` / `_MAXT`) are read in
   Merge.cs / Compact.cs; none is needed for the build.

## How the merge works

The stock world has one cell; every donor surface and static model joins it (`Merge.cs`). Each donor is first
compacted (`Compact.cs`: surfaces with the same material, lightmap and flags in a dpvs range joined, its cell trees
replaced by one leaf), then the cell's AABB tree gets a new root over the stock tree and one node per donor; surfaces
stay grouped by the dpvs ranges (lit opaque, lit translucent, shadow caster, emissive), stock first in each, and every
index into them (sorted lists, sky surfaces, shadow geometry, brush model 0) is renumbered; static models, vertices,
indices, lightmaps, reflection probes and material memory are appended; light grid rows are re-based
(`LightGridMerge.cs`; a donor moves along the grid's row axis, x, far enough that no row holds two scenes; identical
colour sets are kept once). The donors' lightmaps and probe images are renamed (`*lightmapN_*`, `*reflection_probeN`).
Their primary lights are left out (their light is in their lightmaps and grid colours). The donors' stand and camera
entities (the ones the camera table names) are appended to the MapEnts, moved, their targetnames prefixed, without
their preview model and lineup pose keys. Assets named alike in another case are joined (C1 links names
case-insensitively; a second image of a name in one zone made C1 release a texture that was never created).

## Findings

- **C1 refuses scene positions outside the stock lobby's x range.** The row's stand and camera origins reach the scene
  through float dvars; `2110` (stand x) allows -1024 .. 7680 and `2104` (camera x) -1024 .. 10000
  (`'-32620' is not a valid value for dvar '2110'` in the console log). A refused value keeps the previous scene's,
  so the view stood at the old x with the new y and z: black, the stock sky, or (after `urb`) stock streets that
  passed for a ported scene. Kernel33 widens both domains when the merged zone is loaded (`VlWidenDomains`). The
  scenes cannot instead move along y inside the stock x range: the light grid's rows run along x and one row holds one
  scene in this merge.
- The lobby is a local server running `mp_ui_viewer`; all of its entities spawn. With the later builds' lineup stands
  (keys `model` cha_quartermaster, `animation` menu_lobby_slotN_*_idle, `script_noteworthy` lobby_slot_NN, none of
  which 1.1.1.13 has) it stops with `ERROR: 341 targetname`.
- Static models whose model C1 loads on demand for the character and weapon views (the items the lobby tables name,
  and the 2.6.9.7 stubs `,heg_baseball_tf_s_00`, `,mag_default_m4a1techncm_blk`, `,fx_rifle_shell`) are left out of
  the donors: 2.6.9.7's weapon rack (88 static models).
- 0.0.2.2: its world, material, technique and shader layouts are 1.1.1.13's (`zone-diff`), but its images differ:
  GfxImage is 0x20 bytes (1.1.1.13: 0x24, a u32 more before the name), the category byte is numbered anew (lightmaps
  2 -> 10, reflection probes 1 -> 9), the load flags of in-zone pixels too (lightmap 2 -> 0x08000003, probe cube
  0x10000 -> 0x08010301, `$outdoor` 2 -> 3), and the pixels follow as an insert (-2) where 1.1.1.13 has them inline
  (-1). The tool rewrites all four. XModel is 0x16C (1.1.1.13: 0x168) and XModelSurfs differ, so its static models
  take the stock model of the same name or are left out; its light grid (65536 distinct colours, more than a merged
  grid indexes) is replaced by the stock rows of the same places.
- Poses: 1.1.1.13 poses the lobby character from `mp/uivieweranimposes.csv` (tf_m / sc_m class poses, lobby_stand_01..06,
  lobby_sit_*, cyborg_menu_*) whatever the scene; its stands name no pose. The later lobbies' character stands name
  lobby_stand_05 (1.1.1.13 has it) and their lineup slots menu_lobby_slotN_rifle / _riot_idle (1.1.1.13 does not).
  No animation is ported: the character keeps its 1.1.1.13 class pose in every scene.

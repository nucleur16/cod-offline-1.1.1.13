# vlobby_merge: other builds' lobby scenes in 1.1.1.13's virtual lobby (work in progress)

The lobby's 3D scene is the map `mp_ui_viewer` (zone `ui_viewer_mp`). Each scene is a place in that one map; a row
of `mp/cameraPosTable.csv` names its character stand and camera (script_origin targetnames), vision and light.
`src/vlobby_env.c` is the environment switch (`ui_vlobbyEnv`, `vlobby_env`); it works today with the stock rows
(center, urb, woo, des, arc, zombie).

This tool builds `client_shipRetail\chinese\ui_viewer_vlobby_mp.ff`, which Kernel33 opens for `ui_viewer_mp` when it
is there (`VlobbyZoneOpen`): every stock asset of `ui_viewer_mp`, its `mp_ui_viewer` GfxWorld and MapEnts with the
scenes of other builds merged in, and the camera table with their rows (`mp/camerapostable_vlobby.csv`, served for
`mp/cameraPosTable.csv` by `VlobbyFindSubstitute`). Without the file the lobby is stock.

**Status (2026-11-20): not shipped.** The merged zone loads in the 1.1.1.13 loaders (stream consumed exactly, every
surface material, static model, technique pass checked), and the AssetExplorer reads it (`map-info`), but the client
still crashes with the donors in it: 2.6.9.7 alone in C1's material sort at load (a technique whose pass 0 pixel shader
reads 0x476, 0x00A8753F), 3.19.26.22 in the static model / surface pass once the urban scene is drawn (0x00AC3730).
The same pipeline with no donor (`VLOBBY_REBUILD_STOCK=1`: the stock world rebuilt, renumbered and written) runs, and
the ported 2.6.9.7 lobby zone loaded on its own (as `patch_ui_viewer_mp`, whose world then stands for the stock one)
runs too. What remains is in how the donors' render assets (techniques, shaders, vertex declarations that share names
with the stock ones) end up in one zone with the stock assets; see "Findings".

| build | scenes | moved by | names |
|---|---|---|---|
| 3.19.26.22 (3.5.2.6 has the same lobby) | lobby (3 cameras), career, champion select, team lobby, zombie champions, DMZ, supply depot, daily reward | x +16384 | `v3_*` |
| 2.6.9.7 | lobby (2 cameras), team lobby | x +24576 | `v2_*` |
| 0.0.2.2 | urban, forest, desert, arctic and zombies scenes (earlier versions of 1.1.1.13's) | x +36864 | `v0_*` |

## Rebuild

1. Port the later builds' lobby worlds with the AssetExplorer (`port-map`; the world is in their `ui_mp`), against
   a 1.1.1.13 install **without the map pack** (a target with the pack's iwds misses images an install without it
   lacks: 84 images against 104 for 3.19.26.22):

        roots.txt: gfx_map:, col_map_mp:, com_map:, fx_map:, game_map_mp:, map_ents:maps/mp/mp_ui_viewer.d3dbsp (one per line)
        CodolSoundDumper port-map --game "<3.19.26.22>" --map ui_mp --target "<1.1.1.13>" --out v31926 --iwd-name iw_image_18.iwd --roots-file roots.txt
        CodolSoundDumper port-map --game "<CODOL_2.6.9.7>" --map ui_mp --target "<1.1.1.13>" --out v2697 --iwd-name iw_image_18.iwd --roots-file roots.txt
        CodolSoundDumper iwd-merge --input v31926/iw_image_18.iwd --input v2697/iw_image_18.iwd --game "<1.1.1.13>" --out iwd --names iw_image_18.iwd

   The merged iwd (113 images, 50 MB) installs as `main_shared\iw_image_18.iwd` (a name no stock or map pack iwd uses).

2. The camera table: `python camera_rows.py <stock> <3.19.26.22> <2.6.9.7> camerapostable_vlobby.csv` with the three
   builds' `mp/camerapostable.csv` (`CodolSoundDumper zone-stringtables --input <code_post_gfx_mp / patch_code_post_gfx_mp .ff> --name camerapos --out <dir>`).

3. Merge (`dotnet build -c Release`; AeBin = the AssetExplorer's built `Codol.*.dll` folder):

        vlobby_merge merge "<1.1.1.13>\client_shipRetail\chinese\ui_viewer_mp.ff" "<1.1.1.13>\codoMP_client_shipRetail.exe" ui_viewer_vlobby_mp.ff ^
          v31926\ui_mp.ff=16384,0,0,v3_,3.19.26.22 v2697\ui_mp.ff=24576,0,0,v2_,2.6.9.7 ^
          "<0.0.2.2>\client_shipRetail\chinese\ui_viewer_mp.ff=36864,0,0,v0_,0.0.2.2,-2500:-12500:-2000:3500:2500:4000,stockgrid" ^
          table=camerapostable_vlobby.csv

   Diagnostics: `probe`, `scenes` (static models and surfaces around each stand, any build), `images`, `check`
   (surface materials, static model models), `techcheck` (technique pass shaders), `dump`.

## How the merge works

The stock world has one cell; every donor surface and static model joins it (`Merge.cs`): the cell's AABB tree gets a
new root over the stock tree and one node per donor over the donor's cell trees; surfaces stay grouped by the dpvs
ranges (lit opaque, lit translucent, shadow caster, emissive), stock first in each, and every index into them (sorted
lists, sky surfaces, shadow geometry, brush model 0) is renumbered; static models, vertices, indices, lightmaps,
reflection probes and material memory are appended; light grid rows are re-based (`LightGridMerge.cs`; a donor moves
along the grid's row axis far enough that no row holds two scenes; identical colour sets are kept once). The donors'
lightmaps and probe images are renamed (`*lightmapN_*`, `*reflection_probeN`). Their primary lights are left out
(their light is in their lightmaps and grid colours). The donors' stand and camera entities (the ones the camera
table names) are appended to the MapEnts, moved, their targetnames prefixed, without their preview model and lineup
pose keys. Assets named alike in another case are joined (C1 links names case-insensitively; a second image of a
name in one zone made C1 release a texture that was never created).

## Findings

- The lobby is a local server running `mp_ui_viewer`; all of its entities spawn. With the later builds' lineup stands
  (keys `model` cha_quartermaster, `animation` menu_lobby_slotN_*_idle, `script_noteworthy` lobby_slot_NN, none of
  which 1.1.1.13 has) it stops with `ERROR: 341 targetname`.
- Static models whose model C1 loads on demand for the character and weapon views (the items the lobby tables name,
  and the 2.6.9.7 stubs `,heg_baseball_tf_s_00`, `,mag_default_m4a1techncm_blk`, `,fx_rifle_shell`) are left out of
  the donors: 2.6.9.7's weapon rack (88 static models).
- 0.0.2.2: its world, material, technique and shader layouts are 1.1.1.13's (`zone-diff`), but GfxImage is 0x20 bytes
  (1.1.1.13: 0x24, a u32 more before the name; the tool rewrites the headers), XModel 0x16C (1.1.1.13: 0x168) and
  XModelSurfs differ, so its static models take the stock model of the same name or are left out; its light grid
  (65536 distinct colours, more than a merged grid indexes) is replaced by the stock rows of the same places.
- Poses: 1.1.1.13 poses the lobby character from `mp/uivieweranimposes.csv` (tf_m / sc_m class poses, lobby_stand_01..06,
  lobby_sit_*, cyborg_menu_*) whatever the scene; its stands name no pose. The later lobbies' character stands name
  lobby_stand_05 (1.1.1.13 has it) and their lineup slots menu_lobby_slotN_rifle / _riot_idle (1.1.1.13 does not).

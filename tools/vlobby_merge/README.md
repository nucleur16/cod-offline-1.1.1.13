# vlobby_merge: other builds' lobby scenes in 1.1.1.13's virtual lobby

The lobby's 3D scene is the map `mp_ui_viewer` (zone `ui_viewer_mp`). Each scene is a place in that one map; a row
of `mp/cameraPosTable.csv` names its character stand and camera (script_origin targetnames), vision and light.
`src/vlobby_env.c` is the switch: `vlobby_env` / `ui_vlobbyEnv` picks the menu scene (the main menu's and the
Multiplayer lobby's character view: center and the merged menu scenes). Rooms (the game lobby) keep their map's stock
environment scene.

This tool builds `client_shipRetail\chinese\ui_viewer_vlobby_mp.ff`, which Kernel33 opens for `ui_viewer_mp` when it
is there (`VlobbyZoneOpen`): every stock asset of `ui_viewer_mp`, its `mp_ui_viewer` GfxWorld and MapEnts with the
scenes of other builds merged in, and the camera table with their rows (`mp/camerapostable_vlobby.csv`, served for
`mp/cameraPosTable.csv` by `VlobbyFindSubstitute`). Without the file the lobby is stock. The donor images the zone
does not carry go in `main_shared\iw_image_18.iwd` (a name no stock or map pack iwd uses).

The zone and iwd are build outputs (31 MB and 50 MB, made from the other builds' files), not checked in.

| build | menu scenes (`vlobby_env`) | moved by |
|---|---|---|
| 3.19.26.22 (3.5.2.6 has the same lobby) | `v3_lobby`, `v3_dmz` (only the world's cells 6 and 2, `cells:6:2`) | x -32768 |
| 2.6.9.7 | `v2_lobby`, `v2_team` | x -16384 |
| 0.0.2.2 | `v0_menu` (its main menu: the urban stand from player_camera_urb); only its urban area is merged (keep box) | x -44032 |

The 3.x / 2.x menu scenes are framed like 1.1.1.13's own menu (`character_loadout_center`): `Framing.cs` places a
camera at the stock camera's offset from each donor stand (turned with the stand), and the row takes the stock
distance (397) and field of view (20); the scene's own row keeps its vision, light and depth of field. Rows that
share a stand frame alike, so one is kept per stand (3.19.26.22's lobby2 / details / career and 2.6.9.7's lobby2 were
the lobby stand under other cameras).

## Rebuild

1. Port the later builds' lobby worlds with the AssetExplorer (`port-map`; the world is in their `ui_mp`), against
   a 1.1.1.13 install **without the map pack** (a target with the pack's iwds misses images an install without it
   lacks: 84 images against 104 for 3.19.26.22):

        roots.txt: gfx_map:, col_map_mp:, com_map:, fx_map:, game_map_mp:, map_ents:maps/mp/mp_ui_viewer.d3dbsp (one per line)
        CodolSoundDumper port-map --game "<3.19.26.22>" --map ui_mp --target "<1.1.1.13>" --out v31926 --iwd-name iw_image_18.iwd --roots-file roots.txt
        CodolSoundDumper port-map --game "<CODOL_2.6.9.7>" --map ui_mp --target "<1.1.1.13>" --out v2697 --iwd-name iw_image_18.iwd --roots-file roots.txt
        CodolSoundDumper iwd-merge --input v31926/iw_image_18.iwd --input v2697/iw_image_18.iwd --game "<1.1.1.13>" --out iwd --names iw_image_18.iwd

   The merged iwd (113 images, 50 MB) is the merge's `iwd=` input (step 3).

2. The camera table and the framing cameras: `python camera_rows.py <stock> <3.19.26.22> <2.6.9.7> <0.0.2.2>
   camerapostable_vlobby.csv framing_vlobby.txt` with the four builds' `mp/camerapostable.csv` (`CodolSoundDumper zone-stringtables --input <code_post_gfx_mp / patch_code_post_gfx_mp .ff> --name camerapos --out <dir>`).

3. Merge (`dotnet build -c Release`; AeBin = the AssetExplorer's built `Codol.*.dll` folder):

        vlobby_merge merge "<1.1.1.13>\client_shipRetail\chinese\ui_viewer_mp.ff" "<1.1.1.13>\codoMP_client_shipRetail.exe" ui_viewer_vlobby_mp.ff ^
          v31926\ui_mp.ff=-32768,0,0,v3_,3.19.26.22,cells:6:2 v2697\ui_mp.ff=-16384,0,0,v2_,2.6.9.7 ^
          "<0.0.2.2>\client_shipRetail\chinese\ui_viewer_mp.ff=-44032,0,0,v0_,0.0.2.2,-2500:-1900:-2000:3500:2500:4000,stockgrid" ^
          table=camerapostable_vlobby.csv frame=framing_vlobby.txt iwd=iwd\iw_image_18.iwd

   It writes `iw_image_18.iwd` next to the zone: the `iwd=` entries and the 0.0.2.2 image files 1.1.1.13 lacks
   (`V0FileImages.cs`). It installs as `main_shared\iw_image_18.iwd`. `VLOBBY_EXTRA_IWDS` (folders, `;`) adds iwds
   installed next to the stock ones (a map pack's `main`) to the name collision check.

   `frame=` adds the framing cameras (`Framing.cs`). A donor is `<ff>=<dx>,<dy>,<dz>,<prefix>,<tag>[,<keep box x0:y0:z0:x1:y1:z1>][,stockgrid][,cells:<i>:<j>..]` (offsets in whole light
   grid cells: x, y multiples of 32, z of 64). Leave a donor out to build without it. `cells:` keeps only those cells
   of the donor world (`Cells.cs`; the `bsp` and `probe` diagnostics give each stand's cell and each cell's surfaces):
   the other cells' surfaces, static models and the reflection probes only they use stay out, and with them their
   materials, models and images (zone and iwd: an `iwd=` entry no image of the written zone names is not shipped).
   3.19.26.22's world is nine rooms: 6 the lobby (and champion select), 2 the DMZ lobby, 1 the team lobby, 0 the
   zombies champion select, 5 the daily reward, 8 the supply depot, 3 and 4 the item and weapon pages, 7 an empty
   room. Its champion, team, zombies champion, depot and reward scenes were taken out (2026-11-22), so it keeps 6 and 2.

   Diagnostics: `probe`, `scenes` (static models and surfaces around each stand, any build), `images`, `imgcat`
   (image categories), `check` (surface materials, static model models), `techcheck` (technique pass shaders),
   `bsp` (the dpvs BSP cell of every stand and camera), `dump` / `deref` (asset header bytes, the object a field
   points to), `v0img scan` / `flags` / `users` / `surfaces` (0.0.2.2 file images against the stock zones' images
   of the same name, load definition flags against the .iwi's, the materials using some images, the surfaces near
   the stands the keep box drops). Test switches (`VLOBBY_TEST_*`, `VLOBBY_NO_COMPACT`, `VLOBBY_COMPACT_MAXV` / `_MAXT`) are read in
   Merge.cs / Compact.cs; none is needed for the build.

## How the merge works

The stock world has one cell; every donor surface and static model joins it (`Merge.cs`). Each donor is first
compacted (`Compact.cs`: surfaces with the same material, lightmap, reflection probe and flags in a dpvs range joined, its cell trees
replaced by one leaf), then the cell's AABB tree gets a new root over the stock tree and one node per donor; surfaces
stay grouped by the dpvs ranges (lit opaque, lit translucent, shadow caster, emissive), stock first in each, and every
index into them (sorted lists, sky surfaces, shadow geometry, brush model 0) is renumbered; static models, vertices,
indices, lightmaps, reflection probes and material memory are appended; light grid rows are re-based
(`LightGridMerge.cs`; a donor moves along the grid's row axis, x, far enough that no row holds two scenes; identical
colour sets are kept once). The appended static models are not in C1's material order, so C1's load-time static model sort (0x00AA1F50) moves blocks of them; it renumbers only the cell's first tree node, which after the merge is the new root, and left the stock tree's leaves naming the wrong models (the vanishing vehicles): Kernel33 repairs every node's references after the sort (`src/smodel_sort.c`), so the merge needs no static model budget. The donors' lightmaps and probe images are renamed (`*lightmapN_*`, `*reflection_probeN`).
Their primary lights are left out (their light is in their lightmaps and grid colours). A donor's material memory
entries are kept only for the materials its kept surfaces use, and donor render assets nothing written leads to leave
the asset list, so what a keep box leaves out does not ship. The donors' stand and camera
entities (the ones the camera table names) are appended to the MapEnts, moved, their targetnames prefixed, without
their preview model and lineup pose keys. Assets named alike in another case are joined (C1 links names
case-insensitively; a second image of a name in one zone made C1 release a texture that was never created).

Reflection probes (`Cells.cs`). A world surface and a static model name their probe; the lobby character and its
weapon (scene entities) do not: C1 takes the cell their origin is in and the nearest probe of that cell's list. Probe 0
is the compiler's default cube (at 0,0,0, the same grey pixels in every build) and no cell lists it. The merged world
is one cell, so its list is every listed probe of every scene. Three things went wrong there before 2026-11-22:
Compact listed every probe of a donor, its probe 0 too, and the 3.19.26.22 lobby stand and the 0.0.2.2 menu stand are
nearer their donor's origin than their own probe (a flat grey reflection); the 2.6.9.7 lobby stand is nearer the team
room's probe than its own (which hangs 2200 units above it: the dark team-room reflection); and Compact joined
surfaces of one material across a donor's rooms, so a joined surface took its first member's probe. Now the cell list
is the donor cells' own lists, the probe is part of Compact's key, and for every menu scene the merge finds the probe
the donor's own world gives its stand (its BSP cell, the nearest of that cell's list); where the merged cell would
give another, that probe's origin moves to the stand (an origin only chooses a probe). The log's `probes:` lines list
each scene's probe and its build's.

## Findings

- **0.0.2.2's main menu** is its home page in character-loadout mode with terrain `urb`
  (`PageNavigationController` / `CharacterLoadoutService.TERRAIN_NAME` in its codo.gfx), hence `v0_menu`; the keep
  box takes 0.0.2.2's urban area only (y -1900 .. 2500: its desert, forest, zombies and arctic areas lie south of it),
  and the iwd ships only the 0.0.2.2 images the written zone holds.
- **Rooms** show `team_<environment>`, their map's arena environment; their characters stand in the slots of that
  environment's row of `mp/uiViewerScenes.csv` (urban: `s1_place4`, `s1_place2` ..), not at the row's stand.
- **The 3.19.26.22 lobby backdrop is meant to be blue.** Its studio is a black box with a flat panel wall behind the
  stand; the panel textures (`mtl_ui_bg_01/02_col`, `mtl_ui_softround_02/04_col`, `mtl_ui_dot_01_col`) are soft blues,
  and its rows add strong depth of field (512 .. 1024, 10). Its rows' 11 extra columns (colour grading: lift / gamma /
  gain triples, saturation, bloom) have no 1.1.1.13 counterpart, so the tint differs a little.

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
- 0.0.2.2 file images (pixels in an iwd, 181 the merged zone keeps before the left-out models drop 80 of them):
  `v0img scan` paired them with the stock zones' images of the same name (15287 pairs of the same size over the 200 zones). The header is
  1.1.1.13's byte for byte (+0x04..+0x1B; +0x1C, the new u32, is 0; +0x1A is 1 in map zones and 0 in ui_viewer_mp,
  as in 0.0.2.2's). Only the load definition differs: 0.0.2.2 an insert with flags 0, 1.1.1.13 inline with the .iwi
  header's flags word (`v0img flags`: 6144 stock file images, all equal: 0x340 colour and specular, 0x40000 / 0x40008
  normal maps, 0x360, 0x370, 0x140, 0x341 ...). Left as an insert, C1 took the -2 for a texture (d3d9.dll fault).
  Both builds read IWI version 9 (formats 1..13, same layout and picmip sizes, same wavelet tables), so the files are
  used as they are: 164 are byte-identical in 1.1.1.13's iwds; the others ship in iw_image_18.iwd (3 the zone uses:
  codo_ui_sun and the sky sides). A 0.0.2.2 file of a name 1.1.1.13 uses for other pixels would be renamed v0_<name>
  (none is).
- Poses: 1.1.1.13 poses the lobby character from `mp/uivieweranimposes.csv` (tf_m / sc_m class poses, lobby_stand_01..06,
  lobby_sit_*, cyborg_menu_*) whatever the scene; its stands name no pose. The later lobbies' character stands name
  lobby_stand_05 (1.1.1.13 has it) and their lineup slots menu_lobby_slotN_rifle / _riot_idle (1.1.1.13 does not).
  No animation is ported: the character keeps its 1.1.1.13 class pose in every scene.

## Baking the camera editor's cameras

The camera editor build (`feat/vlobby-camera-ui`, not in the release: `src/vlobby_camui.c`, F8 or `vlobby_camui` in
the lobby) saves `main\vlobby_cameras.txt` in its install, one line per camera row it changes. The release shows those
cameras without the editor once they are baked into the zone:

1. Copy the saved file here as `cameras_vlobby.txt` (checked in, so the chosen cameras stay with the tool; drop the
   lines of rows the table no longer has). The one checked in is the editor's save of 2026-11-22 for v0_menu, v2_lobby,
   v2_team, v3_dmz and v3_lobby.
2. After step 2 of the rebuild (`camera_rows.py`, which writes the table and the framing file anew), run

        python fold_cameras.py cameras_vlobby.txt

   It rewrites `camerapostable_vlobby.csv` and `framing_vlobby.txt` in place (other paths: `fold_cameras.py <cameras>
   <table.csv> <framing.txt>`). A row's `cam` becomes a new camera entity `k33cam_<row>` (a `place` line of the framing
   file, which `Framing.cs` adds to the MapEnts with the numbers as saved) named in the row's camera column, and the
   framing camera the row named before goes; `stand` likewise `k33stand_<row>` in the stand column; `dist`, `fov` and
   `dof` go to columns 4, 13 and 19-21. New entities, because rows share cameras and stands (`weapon_camera`,
   `player_pos_urb`). A row whose stand moved but whose camera did not keeps its framing camera, framed from the
   scene's own stand as the editor showed it (the merge keeps the stands the framing file names). Running it twice
   changes nothing.
3. Merge (step 3) with `frame=framing_vlobby.txt` as before. An install with the new zone no longer needs its
   `main\vlobby_cameras.txt` (its lines would apply over the baked values, which are the same). `vlobby_merge ents
   <zone> <exe> k33` lists the placed entities as the zone has them.

The file's format (`#` starts a comment; the groups a line names replace what the zone's row gives):

    cam <x> <y> <z> <pitch> <yaw>     dist <d>     fov <f>     stand <x> <y> <z> <yaw>     dof <near> <far> <blur>

A room row's (`team_*`) camera can be aimed by C1 at a room character rather than standing at its entity (the editor
then says so); baked, such a camera is where its entity is, which frames the room as the editor showed only when C1
does not aim it.

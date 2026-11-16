# CODOL 3.19.26.22 map ports in Create Room (2026-11-13)

The 33 maps ported from CODOL 3.19.26.22 (`D:\CoD\CODOL Builds\ported-3.19.26.22`, notes in the AssetExplorer
worktree's `CODOL-31926.md`) are installed like the other later maps: zones in `client_shipRetail\chinese\`, IWDs in
`main\`. This branch adds the front-end data that puts the MP ones into Create Room, and the server level scripts.

## Create Room (this repository)

Same route as mp_dome / mp_strike_sh / mp_qadeem_sh: `zones/usermaps_patch` (packed into `codol_common.ff` and the
locale packs) carries for each map
- a `mp/comaplist.csv` row (pvp, 4-18 players, every channel on; ids 98-123, sort keys 50-75);
- a `mp/basemaps.arena` entry; its `gametype` list is what the map's MapEnts spawn points and game objects support
  (a TDM-based mode only with both `mp_tdm_spawn_*_start` classes: Tunisia has neither, so no war/hc/thc/koth);
- `MPUI_<NAME>` / `MPUI_DESC_MAP_<NAME>` in English (`codol_english_usermaps`) and Chinese (`usermaps_patch`);
  Hunted, Showdown and Suburbs use `MPUI_MP_*` / `MPUI_DESC_MP_*` because `codol_english` already has the plain keys;
- a map-select image: a stock `ui_mp` picture of the same map where one exists (Backlot, Derail, District, Invasion,
  Skidrow, Showdown), otherwise `sfproto_<map>_usermap` in usermaps_ui_mp (see "Menu art and environment" below);
- a `ship` line in `shipping_maps.txt`.

In the list (26): mp_arkaden_sh Arkaden, mp_awp_arena AWP Arena, mp_backlot_sh Backlot, mp_boomtown Boomtown,
mp_bootleg_sh Bootleg, mp_carbon_sh Carbon, mp_derail_sh Derail, mp_district District, mp_fallen_sh Fallen,
mp_foundation_sh Foundation, mp_interchange_sh Interchange, mp_invasion_sh Invasion, mp_italy_sh Piazza,
mp_liberation_sh Liberation, mp_lockdown_sh Lockdown, mp_mission_sh Mission, mp_mogadishu_sh Bakaara,
mp_nightshift_sh Skidrow, mp_offshore Offshore, mp_overwatch_sh Overwatch, mp_radiation_sh Radiation,
mp_resistance_sh Resistance, mp_showdown Showdown, mp_tunisia Tunisia, mp_village Village, mp_wasteland_sh Wasteland.

Not in the list:
- zm_island_sh, mp_bridge, mp_hunted_spring, mp_suburbs: single-player / Spec Ops / zombies levels (mp_bridge and
  zm_island_sh have no MP spawns at all; Hunted and Suburbs are the coop_* base levels with only DM/TDM spawns and no
  objectives). Out of scope by request.
- mp_mountain: the sh1x server cannot build its collision (`CoDOL collision mp_mountain: resolved clipmap conversion
  failed` / `native assembly failed (-1)` -> server error). A port or server collision issue.
- mp_underground_sh: the server's script compile fails: `animation 'security_camera_destroy' not defined in anim tree
  'destructibles'`. The port's synthesized `mp_underground_sh.animlist` puts `security_camera_*` in the `destructibles`
  tree; the stock scripts have them in `destructibles_dlc`. A port fix (animlist tree name).
- mp_lookout_sh: every mode aborts the level (map_rotate right after load): its MapEnts have no
  `mp_tdm_spawn_allies_start` and no `mp_dm_spawn`; IW5 Lookout added its spawns from script through the faceoff
  gametype, which 1.1.1.13 does not have.

## Level scripts (server rawfiles; not in this repository)

The server GSC tree is the private submodule's `bin/home/codol_rawfiles` (staged into `main\sh1x_rawfiles` by
`scripts/build-server.sh`). `docs/MAPS_31926_RAWFILES.patch` adds, for all 29 MP ports:
`maps/mp/<map>.gsc`, `maps/mp/<map>_fx.gsc` and `maps/createfx/<map>_fx.gsc` (the zone's own 3.19.26.22 files, as
they ship), `maps/mp/<map>_precache.gsc` (the modules the zone's MapEnts name in `precache_script` keys) and, where a
fog source exists, `maps/createart/<map>_art.gsc` (written as `setExpFog`, like `mp_dome_art.gsc`). Apply it in
`codol_rawfiles` with `git apply`. Each level script's header lists its source and every adaptation.

Sources: 3.19.26.22 ships no level script (only `_fx`/`createfx`). The level scripts come from the original games:
IW5 decompiled scripts in `D:\CoD\COD-GSC-Source\IW5-GSC` where present; for the MW3 base maps whose scripts are only
compiled `ScriptFile` assets in `Call of Duty Modern Warfare 3\zone\english\*.ff`, the script was rebuilt from its
string and constant table (every MW3 base-map level script is the same short sequence; the table names each call's
arguments) and the fog from the zone's `maps/createart/<map>_fog.gsc` text; IW4 and IW3 scripts from the dumps (and the
CoD4 `raw\` folder); Black Ops scripts from `T5-STEAM-GSC`.

| map | level script source | adaptations / remaining notes |
|---|---|---|
| mp_radiation_sh | T5 mp_radiation (T5-STEAM-GSC), hand-translated: tunnel doors and switches (start open, cooldown, crush check), panel exploders, tunnel lights, diggers, conveyor belt | precache module and createart added; devgui, _ambientpackage amb, _teamset, unified spawning, spawncollision and no-turret triggers left out; T5 helpers (set_dvar_int_if_unset, trigger_thread, isPregame, exploder_stop) defined locally; prematch wait through gameFlag("prematch_done"); fade arguments of play/stoploopsound dropped; equipment on the doors detonated through claymoreArray/c4Array; evt_excavator_* sounds and green_light/blink_light effects are not in the zone (left out / guarded) |
| mp_arkaden_sh | IW5 mp_plaza2 (compiled ScriptFile in MW3 zone/english/mp_plaza2.ff; rebuilt from its string/const table) | ambientPlay( "ambient_mp_plaza2" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "city") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_arkaden_sh (the zone's) |
| mp_awp_arena | none (minimal level script) | no level script for this map exists in 3.19.26.22 or in any IW3/IW4/IW5/T5 dump on this PC |
| mp_backlot_sh | IW3 mp_backlot (COD-GSC-Source/IW3-GSC/maps/mp/mp_backlot.gsc) | ambientPlay( "ambient_backlot_ext" ) left out: the zone has no such alias; game["allies"]/["axis"] = marines/opfor and the *_soldiertype lines are left out (IW3 team setup; 1.1.1.13 takes its teams from the arena/teams scripts); no createart: IW3's maps/createart/mp_backlot_art.gsc is not in the dump; compass: compass_map_mp_backlot_sh (the zone's) |
| mp_boomtown | none (minimal level script) | no level script for this map exists in 3.19.26.22 or in any IW3/IW4/IW5/T5 dump on this PC |
| mp_bootleg_sh | IW5 mp_bootleg (compiled ScriptFile, mp_bootleg.ff; rebuilt from its string/const table) | ambientPlay( "ambient_mp_bootleg" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "city") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_bootleg_sh (the zone's) |
| mp_carbon_sh | IW5 mp_carbon (compiled ScriptFile, mp_carbon.ff; rebuilt from its string/const table) | ambientPlay( "ambient_mp_carbon" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_carbon_sh (the zone's); the teddy_1 physics prop thread (com_teddy_bear, "physics_finished") is not ported: its bytecode could not be decompiled |
| mp_derail_sh | IW4 mp_derail (COD-GSC-Source/IW4-GSC/maps/mp/mp_derail.gsc; this tree already has it as maps/mp/mp_derail.gsc) | ambientPlay( "ambient_mp_snow" ) left out: the zone has no such alias; compass: compass_map_mp_derail_sh (the zone's) |
| mp_district | IW3 mp_citystreets ("District"; COD-GSC-Source/IW3-GSC/maps/mp/mp_citystreets.gsc) | ambientPlay( "ambient_citystreets_day" ) left out: the zone has no such alias; game["allies"]/["axis"] = marines/opfor and the *_soldiertype lines are left out (IW3 team setup; 1.1.1.13 takes its teams from the arena/teams scripts); no createart: IW3's maps/createart/mp_citystreets_art.gsc is not in the dump; compass: compass_map_mp_citystreets (the zone's) |
| mp_fallen_sh | IW5 mp_lambeth (compiled ScriptFile, mp_lambeth.ff; rebuilt from its string/const table) | ambientPlay( "ambient_mp_lambeth" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "city") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_fallen_sh (the zone's) |
| mp_foundation_sh | IW5 mp_cement (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_cement.gsc) | ambientPlay( "ambient_mp_cement" ) left out: the zone has no such alias; compass: compass_map_mp_foundation_sh (the zone's); common_scripts\_interactive::init() and the level.func[] anim hooks are left out (IW5 interactive props; no _interactive in this tree); spawn_blocker_collision waits at most 30 s for level.airDropCrateCollision (IW5 waits forever) |
| mp_interchange_sh | IW5 mp_interchange (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_interchange.gsc) | ambientPlay( "ambient_mp_interchange" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "parkinglot") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_interchange_sh (the zone's) |
| mp_invasion_sh | IW4 mp_invasion (COD-GSC-Source/IW4-GSC/maps/mp/mp_invasion.gsc; this tree already has it as maps/mp/mp_invasion.gsc) | ambientPlay( "ambient_mp_urban" ) left out: the zone has no such alias; compass: compass_map_mp_invasion_sh (the zone's) |
| mp_italy_sh | IW5 mp_italy (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_italy.gsc) | ambientPlay( "ambient_mp_italy" ) left out: the zone has no such alias; compass: compass_map_mp_italy_sh (the zone's) |
| mp_liberation_sh | IW5 mp_park (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_park.gsc) | ambientPlay( "ambient_mp_park" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_liberation_sh (the zone's); IW5's decompiled turret loop deletes only the last turret (a decompiler artefact: foreach(var_2 in var_0) {} var_2 delete();); every misc_turret is deleted here, which is what the mode check intends |
| mp_lockdown_sh | IW5 mp_alpha (compiled ScriptFile, mp_alpha.ff; rebuilt from its string/const table) | ambientPlay( "ambient_mp_alpha" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "city") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_lockdown_sh (the zone's) |
| mp_lookout_sh | IW5 mp_restrepo_ss (compiled ScriptFile, patch_mp_restrepo_ss.ff; rebuilt from its string/const table) | no mp_restrepo_ss_fog/art source: no createart; ambientPlay( "ambient_mp_outpost" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_lookout_sh (the zone's); tdmSpawnAdjustments() (extra TDM spawns through IW5's maps\mp\gametypes\faceoff::createScriptSpawnPoint) is not ported: this tree has no faceoff gametype; the map's own mp_tdm_spawn points are used |
| mp_mission_sh | IW5 mp_bravo (compiled ScriptFile, mp_bravo.ff; rebuilt from its string/const table) | ambientPlay( "ambient_mp_bravo" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_mission_sh (the zone's) |
| mp_mogadishu_sh | IW5 mp_mogadishu (compiled ScriptFile, mp_mogadishu.ff; rebuilt from its string/const table) | ambientPlay( "ambient_mp_mogadishu" ) left out: the zone has no such alias; compass: compass_map_mp_mogadishu_sh (the zone's) |
| mp_mountain | T5 mp_mountain ("Summit"; COD-GSC-Source/T5-STEAM-GSC/maps/mp/mp_mountain.gsc) | compass: compass_map_mp_mountain (the zone's) |
| mp_nightshift_sh | IW4 mp_nightshift ("Skidrow"; COD-GSC-Source/IW4-GSC/maps/mp/mp_nightshift.gsc; this tree has it as maps/mp/mp_nightshift.gsc) | ambientPlay( "ambient_mp_urban" ) left out: the zone has no such alias; compass: compass_map_mp_nightshift_sh (the zone's) |
| mp_offshore | IW5 mp_roughneck ("Off Shore"; COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_roughneck.gsc) | ambient ambient_mp_roughneck -> ambient_mp_offshore (the zone's alias); audio_settings() (maps\mp\_audio::add_reverb "quarry") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_offshore (the zone's); the remote_uav / remote_tank checks in watchPlayerEnterWater are left out (IW5 killstreaks this tree does not have); sm_sunShadowScale takes the PC value (IW5 picks 0.5 on PS3) |
| mp_overwatch_sh | IW5 mp_overwatch (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_overwatch.gsc) | ambientPlay( "ambient_mp_overwatch" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "city") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_overwatch_sh (the zone's) |
| mp_resistance_sh | IW5 mp_paris (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_paris.gsc) | ambientPlay( "ambient_mp_paris" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "city") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_resistance_sh (the zone's) |
| mp_showdown | IW3 mp_showdown (COD-GSC-Source/IW3-GSC/maps/mp/mp_showdown.gsc) | ambient ambient_crossfire -> ambient_mp_showdown (the zone's alias); game["allies"]/["axis"] = marines/opfor and the *_soldiertype lines are left out (IW3 team setup; 1.1.1.13 takes its teams from the arena/teams scripts); no createart: IW3's maps/createart/mp_showdown_art.gsc is not in the dump; compass: compass_map_mp_showdown (the zone's) |
| mp_tunisia | none (minimal level script) | no level script for this map exists in 3.19.26.22 or in any IW3/IW4/IW5/T5 dump on this PC |
| mp_underground_sh | IW5 mp_underground (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_underground.gsc) | ambientPlay( "ambient_mp_underground" ) left out: the zone has no such alias; audio_settings() (maps\mp\_audio::add_reverb "arena") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_underground_sh (the zone's); ac130_thread returns when the level has no ac130_overhead_* entities (IW5 would loop over empty groups) |
| mp_village | IW5 mp_village (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_village.gsc) | audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio; compass: compass_map_mp_village (the zone's) |
| mp_wasteland_sh | IW4 mp_brecourt ("Wasteland"; COD-GSC-Source/IW4-GSC/maps/mp/mp_brecourt.gsc; this tree has it as maps/mp/mp_brecourt.gsc) | ambientPlay( "ambient_mp_rural" ) left out: the zone has no such alias; compass: compass_map_mp_wasteland_sh (the zone's) |
Check: every script set was compiled and loaded headless with the sh1x dedicated server (`k33p2p_dedrun.exe`, loopback,
4 bots, war; tunisia also dm/sd): no compile error, no `Could not load rawfile`, no script runtime error, except
mp_underground_sh (above), mp_mountain (collision, before scripts run) and mp_lookout_sh (spawns). Remaining warnings:
mp_bootleg_sh's chicken destructible anims are in no client animlist (server keeps the props static), and the
`Pathnode ... has no connections` map-data warning on several levels.

## Menu art and environment (2026-11-16, branch feat/ported-maps-menus-env)

**Map-select pictures.** The 20 maps without a stock picture get `sfproto_<map>_usermap` (200x120, the size of the
stock `sfproto_*` pictures) in `zones/usermaps_ui_mp/image/`, with the pixels in the zone (A8R8G8B8, one level, the
in-zone header the glyph images use). No IWD carries them, so a client without the map pack cannot fail on a missing
`.iwi`. `comaplist.csv` column 4 names them. `tools/map_menu_art/build_previews.py` makes them from decoded sources
(`tools/map_menu_art/iwi.py` decodes IWI versions 6/8/9/13): the original game's own map-select picture where the
map exists in one, else the map's own CODOL 3.19.26.22 loadscreen (its right part; the left third fades to black).
Checked live: every one of the 26 maps shows its picture in the Create Room carousel.

**Loadscreens.** Every port's IWD already carries `loadscreen_<map>.iwi` (1280x720 RGB, CODOL 3.19.26.22's art; the
earlier note that their pixels are in no IWD was wrong) and the `_load` zone's `$levelbriefing` names it. Checked live
on Showdown and Tunisia.

**Lobby environment.** The pre-match room draws the 3D virtual lobby (`ui_viewer_mp`). The arena entry's
`environment` (urban / forest / desert / arctic) picks its scene and camera (`mp/camerapostable.csv` `team_<env>`),
and `allieschar` / `axischar` the character sets; the room's map strip is the comaplist picture. 1.1.1.13 has no
per-map lobby music, lobby camera or Scaleform map-info entry beyond these, and `mapimage` is `blank` for every
usermap (mp_dome, mp_strike_sh and mp_qadeem_sh included). Environments now follow the original map's setting: AWP
Arena arctic (snow), Carbon and Radiation desert, Invasion urban (as stock mp_invasion). District and Showdown also
have older stock arena entries (urban), which the game finds first.

**Environment per map.** Every port zone carries `vision/<map>.vision` (except Showdown) and its sky (GfxWorld skies,
`sky_*` images in the port IWD), and `sun/<map>.sun` (sun sprite / flare / glare dvars only) where 3.19.26.22 had one.
Changes:
- fog: `maps/createart/mp_backlot_sh_art.gsc`, `mp_district_art.gsc` and `mp_showdown_art.gsc` take Call of Duty 4's
  createart fog (`docs/MAPS_31926_ENV.patch`, on top of MAPS_31926_RAWFILES.patch; branch `feat/ported-maps-env` in the
  13-rawfiles worktree `D:\CoD\k33_maps_menus_rawfiles_wt`). Derail, Invasion, Skidrow and Wasteland already run the IW4
  createart. Its `VisionSetNaked( "<iw4 name>" )` names no vision in the zone, and `_load` sets the map's own after it.
- vision: Showdown's zone has only `vision/mp_showdown_tp.vision`, which looks washed out as the main vision, so
  usermaps_patch carries `vision/mp_showdown.vision` with mp_tunisia's values (the same CODOL desert set and sky).
- ambient: Backlot plays its zone's `ambient_mp_backlot_sh_ext`. The other zones have no ambient alias the scripts
  could use (Offshore, Showdown, Tunisia and Village already play theirs). `maps\mp\_audio` (IW5 reverb) is not in 1.1.1.13.

| map | map-select picture | loadscreen (port IWD) | fog | vision | sun file | sky | compass | ambient | gaps |
|---|---|---|---|---|---|---|---|---|---|
| mp_arkaden_sh | MW3 preview_mp_plaza2 | loadscreen_mp_arkaden_sh (iw_model_06) | IW5 mp_plaza2_fog | zone | zone | sky_mp_plaza2 | compass_map_mp_arkaden_sh | - | ambient |
| mp_awp_arena | own loadscreen | loadscreen_mp_awp_arena (iw_model_07) | none | zone | zone | sky_mp_overwatch | compass_map_mp_awp_arena | - | fog, ambient (CODOL-only map, no source) |
| mp_backlot_sh | stock sfproto_mp_backlot | loadscreen_mp_backlot_sh (iw_sound_20) | CoD4 mp_backlot (new) | zone | zone | sky_mp_backlot_sh | compass_map_mp_backlot_sh | ambient_mp_backlot_sh_ext (new) | - |
| mp_boomtown | own loadscreen | loadscreen_mp_boomtown (iw_sound_16) | none | zone | zone | sky_mp_boomtown | compass_map_mp_boomtown | - | fog, ambient (CODOL-only map) |
| mp_bootleg_sh | MW3 preview_mp_bootleg | loadscreen_mp_bootleg_sh (iw_model_08) | IW5 | zone | none | sky_mp_bootleg | compass_map_mp_bootleg_sh | - | sun file, ambient |
| mp_carbon_sh | MW3 preview_mp_carbon | loadscreen_mp_carbon_sh (iw_model_09) | IW5 | zone | zone | sky_mp_carbon | compass_map_mp_carbon_sh | - | ambient |
| mp_derail_sh | stock sfproto_mp_derail | loadscreen_mp_derail_sh (iw_model_10) | IW4 mp_derail_art | zone | zone | sky_sp_cliffhanger | compass_map_mp_derail_sh | - | ambient |
| mp_district | stock sfproto_mp_district | loadscreen_mp_district_hh (iw_model_11) | CoD4 mp_citystreets (new) | zone | none | sky_mp_restrepo_ss | compass_map_mp_citystreets | - | sun file, ambient |
| mp_fallen_sh | MW3 preview_mp_lambeth | loadscreen_mp_fallen_sh (iw_model_12) | IW5 | zone | zone | sky_mp_fallen | compass_map_mp_fallen_sh | - | ambient |
| mp_foundation_sh | MW3 preview_mp_cement | loadscreen_mp_foundation_sh (iw_model_13) | IW5 | zone | zone | sky_mp_cement | compass_map_mp_foundation_sh | - | ambient |
| mp_interchange_sh | MW3 preview_mp_interchange | loadscreen_mp_interchange_sh (iw_model_14) | IW5 | zone | none | sky_mp_interchange | compass_map_mp_interchange_sh | - | sun file, ambient |
| mp_invasion_sh | stock sfproto_mp_invasion | loadscreen_mp_invasion_sh (iw_model_15) | IW4 mp_invasion_art | zone | zone | sky_sp_roadkill | compass_map_mp_invasion_sh | - | ambient |
| mp_italy_sh | MW3 preview_mp_italy | loadscreen_mp_italy_sh (iw_model_16) | IW5 | zone | zone | sky_mp_italy_sh | compass_map_mp_italy_sh | - | ambient |
| mp_liberation_sh | MW3 preview_mp_park | loadscreen_mp_liberation_sh (iw_model_17) | IW5 | zone | zone | sky_mp_park | compass_map_mp_liberation_sh | - | ambient |
| mp_lockdown_sh | MW3 preview_mp_alpha | loadscreen_mp_lockdown_sh (iw_model_19) | IW5 | zone | zone | sky_mp_alpha | compass_map_mp_lockdown_sh | - | ambient |
| mp_mission_sh | MW3 preview_mp_bravo | loadscreen_mp_mission_sh (iw_model_23) | IW5 | zone | zone | sky_mp_bravo | compass_map_mp_mission_sh | - | ambient |
| mp_mogadishu_sh | MW3 preview_mp_mogadishu | loadscreen_mp_mogadishu_sh (iw_sound_04) | IW5 | zone | zone | sky_mp_mogadishu_sh | compass_map_mp_mogadishu_sh | - | ambient |
| mp_nightshift_sh | stock sfproto_mp_nightshift | loadscreen_mp_nightshift_sh (iw_sound_05) | IW4 mp_nightshift_art | zone | none | sky_mp_crossfire | compass_map_mp_nightshift_sh | - | sun file, ambient |
| mp_offshore | MW3 preview_mp_roughneck | loadscreen_mp_offshore (iw_sound_06) | IW5 | zone | none | sky_mp_offshore | compass_map_mp_offshore | ambient_mp_offshore | sun file |
| mp_overwatch_sh | MW3 preview_mp_overwatch | loadscreen_mp_overwatch_sh (iw_sound_07) | IW5 | zone | none | sky_mp_overwatch | compass_map_mp_overwatch_sh | - | sun file, ambient |
| mp_radiation_sh | Black Ops menu_mp_map_select_radiation_big | loadscreen_mp_radiation_sh (iw_sound_17) | T5 createart | zone | zone | sky_mp_village_02 | compass_map_mp_radiation_sh | - | ambient |
| mp_resistance_sh | MW3 preview_mp_paris | loadscreen_mp_resistance_sh (iw_sound_18) | IW5 | zone | zone | sky_mp_paris | compass_map_mp_resistance_sh | - | ambient |
| mp_showdown | stock sfproto_mp_showdown | loadscreen_mp_showdown_hh (iw_sound_08) | CoD4 mp_showdown (new) | usermaps_patch (new) | zone | sky_tunisia | compass_map_mp_showdown | ambient_mp_showdown | - |
| mp_tunisia | own loadscreen | loadscreen_mp_tunisia (iw_sound_09) | none | zone | none | sky_tunisia | compass_map_mp_tunisia | ambient_mp_tunisia | fog, sun file (CODOL-only map) |
| mp_village | MW3 preview_mp_village | loadscreen_mp_village (iw_sound_11) | IW5 | zone | zone | sky_mp_village | compass_map_mp_village | ambient_mp_village | - |
| mp_wasteland_sh | MW2 preview_mp_brecourt | loadscreen_mp_wasteland_sh (iw_sound_12) | IW4 mp_brecourt_art | zone | zone | sky_mp_brecourt | compass_map_mp_wasteland_sh | - | ambient |

"zone" means the map zone's own 3.19.26.22 file. A missing `.sun` leaves the sun sprite / flare dvars at their
defaults. Neither MW3 nor 3.19.26.22 has one for those maps. This work used no IWD slot.

**mp_brl_derail removed.** It was deprecated, and mp_derail_sh is the Derail that stays. Removed: its comaplist row
(id 75; the other rows keep their ids, as the shipping build already leaves gaps for dev rows), its arena entry and its
`shipping_maps.txt` line. The `MPUI_DERAIL` / `MPUI_DESC_MAP_DERAIL` strings stay: they are stock (stock arena entries
use them), and mp_derail_sh uses `MPUI_DERAIL_SH`. The offline selector tests use stock mp_crash_intro, which also has
no `_load` zone, for the level-zone case.

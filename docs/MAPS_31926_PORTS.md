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
- a map-select image: none of the ports ships a preview image (their `_load` zones only name `loadscreen_*`, whose
  pixels are in no IWD), so a map reuses a stock `ui_mp` picture of the same map where one exists (Backlot, Derail,
  District, Invasion, Skidrow, Showdown) and has none otherwise (the carousel shows a black picture, no error);
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

zm_wallbuy_staging (2026-10-25): T5's wall buys the CoDOL zombies ports lack, with their outlines.

Server side (no zone needed): rawfiles branch feat/zm-wallbuys, maps/_zombiemode_weapons.gsc spawns the walls
(spawn_extra_wall_buys: a usable tag_origin script_model + the hidden C1 world model, picked up by init_weapon_upgrade /
weapon_spawn_think; grenade walls refill the buyer's own lethal). Probe-verified: 13/10/10/5/1 walls spawn on
factory/theater/sumpf/asylum/prototype with no script error. Buying was NOT tested with a client.

Client side (this folder): the chalk/neon outline at each new wall + CoDOL's own misc/glow_weapon_fx flare, as createfx
oneshots C1 draws itself. Rendered with the AssetExplorer's render-map (C1's own shaders): render\*.png.

Files (out\):
  client_shipRetail\chinese\zm_theater_sh.ff   md5 eafe8b53e258a4925098a0b03e8fbede  (installed 2026-10-23 zone + outlines)
  client_shipRetail\chinese\zm_factory_sh.ff   md5 d22cc9442652e8afa0531df6ac71d8e3
  client_shipRetail\chinese\zm_sumpf_sh.ff     md5 d96b8e9269be66a491058e65a733e87c
  main\iw_model_21.iwd                         md5 6398c76e8742426176a396b0c4b3fefc  NEW (grenade_bag_chalk_c,
                                                                                         chalk_weapon_explosives_c)
Asylum/prototype: GSC walls only (4 grenade walls + the upstairs double barrel; the BAR). No outline zone: their stock
zones already carry CoDOL's walls and outlines (user, 2026-10-26), and a Kernel33 patch-folder zone for them
(zm_wallbuys.ff) crashed C1 at startup (AV read 0x0000000C at 0x00A92825 right after loading it), so it was dropped.
iw_model_21: iw_image_18 is a usermap's, iw_model_20 the Kino worker's; Kernel33 hides non-stock iwds from the pure check.

Install (client closed):
  powershell -ExecutionPolicy Bypass -File "D:\CoD\CODOL Builds\zm_wallbuy_staging\install_zm_wallbuys.ps1"
  (copies into BOTH D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13 and D:\CoD\CODOL Builds\CODOL 1.1.1.13 map pack:
   chinese zones with .pre-wallbuy backups and main\iw_model_21.iwd; -Rollback undoes it)
  k33-devtest's main holds hardlinks of selected iwds only: add iw_model_21.iwd there too when testing with it.
  The server needs no zone (the VPS needs only the rawfiles); the three map zones are picked up at the next map load.

Rebuild (reproducible; idempotent, so it can run on top of another change to the same zones):
  tool_src\build_all.sh (Git Bash). Inputs are the INSTALLED zm_theater_sh/zm_factory_sh/zm_sumpf_sh.ff of
  $CODOL_GAME; tool_build\ is the AssetExplorer src + PortContext.InPlace (pap_patch_staging diff) + WallbuyCommands.cs
  (command zm-wallbuy; build: cd tool_build\src && dotnet build Codol.SoundDumper\Codol.SoundDumper.csproj -c Release).
  To stack another worker's createfx edit of theater: install their zone first, or add
  --rawfile maps/createfx/zm_theater_sh_fx.gsc=<edited text> to the theater line. Wall-buy entries (fxid wallbuy_*,
  flare entries with delay -15.25) are replaced, not duplicated. The Kino worker's patch_zm_theater_sh.ff carries no
  rawfiles, so it does not shadow these edits.
  The spec: tool_src\wallbuys.json, from tool_src\gen_wallbuys.py (T5 entity strings + T5 chalk quads exported from
  port-map'd T5 zones + ray casts against CoDOL's world to put each outline on CoDOL's wall). If it changes, regenerate
  the GSC table in maps/_zombiemode_weapons.gsc (extra_wall_buy_table) from the same json.

The wall-buy terminal (2026-10-27, user: "add the terminal model thing where the wallbuys as they're seen on prototype
and asylum"). CoDOL mounts codo_weapon_box (a console with a keyboard, a hand scanner and a lit arch the gun hangs in) on
every one of its own wall buys: 13 static models on zm_asylum_sh, 6 on zm_prototype_sh, one per wall and none elsewhere.
MEASURED (WallBox/: "WallBox measure --zone zm_asylum_sh --model codo_weapon_box" for the placements; ray casts against
the exported C1 worlds for the wall): its yaw is the wall weapon's (its +y axis out of the wall), its origin 4.3 out of
the wall surface (median of 19), +2.2 along the muzzle and 17.3 below the outline centre (gen_wallbuys.C1W offsets).
  - terminals.py writes each buy's "terminal" (model, origin, angles) into wallbuys.json from its chalk origin and
    facing; gen_wallbuys.py calls it, so a future map's walls get one automatically.
  - zm-wallbuy (WallbuyCommands.cs) copies the model from zm_asylum_sh into every map zone it edits, top-level (the
    server and C1 find it by name) and checks the written zone holds it.
  - gen_wall_table.py writes the GSC table (rawfiles maps/_zombiemode_weapons.gsc extra_wall_buy_table) with the
    terminal; spawn_extra_wall_buys places one per wall when codol_isModelMounted finds the model (rawfiles branch
    feat/zm-wallbuy-terminals). asylum/prototype are stock zones: their GSC walls use the zones' own definition.
  - Counts: zm_theater_sh 10 walls / 10 terminals, zm_factory_sh 13/13, zm_sumpf_sh 10/10, zm_asylum_sh 5/5 (GSC walls;
    plus CoDOL's own 13 static), zm_prototype_sh 1/1 (plus CoDOL's own 6).
  - GameMapRenderer_extra_models.diff: render-map draws CODOL_RENDER_EXTRA_MODELS=<file> lines (model x y z p y r) as
    entities, for shots of script-spawned walls.
The zones with the terminal are built and staged by D:\CoD\CODOL Builds\zm_perkclip_staging (tool_src/build_all.sh,
install_zm_perkclip.ps1), together with Kino's --native-techsets port and the 2013 perk machines.

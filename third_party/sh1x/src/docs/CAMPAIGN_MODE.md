# Campaign / co-op mode: what C1 calls it, what the maps carry, and the first playable mission layer

2026-10-07. This covers the research for the campaign (co-op / Spec Ops) mode and
the first implementation. It names assets, entities, functions and addresses only.
No extracted game text is reproduced, and no zone content is in the repo.

Evidence labels: **MEASURED** means read from the shipped data, the source or a probe
run. **INFERRED** means reasoned from that data but not proven by it.

Earlier documents this builds on:
`CATEGORY_SCRIPT_SETS_20260917.md` (the coop gametype contract and
`_coopmode.gsc`), `ADDON_MAP_ENTS_20260920.md` (the addon entity overlay),
`CATEGORY_WEAPON_WINDOW_20260917.md` (the coop weapon window) and
`ACTOR_SPECIES_20260920.md` (the actor species table and C1's composition trees).

---

## 1. What C1 calls the mode

| item | value | source |
|---|---|---|
| gametype | `coop`. No `teambased` line. Localized name `MPUI_COOPERATIVE`. | `mp_playlists.info` (`gametype coop` / `script coop`), `maps/mp/gametypes/coop.txt`, `_gametypes.txt` (code_post_gfx_mp) |
| asset category | `coop`. The server mounts `common_gametype_coop` (+patch, +localized), the base level `mp_<map>` and the playlist zone `coop_<mode>_mp_<map>`. | `LoadMapArchives`, src/online/assets/asset_commands.c. C1 builds the same set at `0x004CD0FE` (`strchr(map+5,'_')+1`). |
| playlist entries | playlist 27 (on-boarding) rotates `coop_prolog_mp_crash_intro,coop,1` and `coop_training_mp_killhouse_sh,coop,1`. The lobby's observed descriptor shapes are playlist 30 / `coop_training_mp_killhouse_sh`, `coop_spec_mp_estate_tropical` (no playlist), and `coop_survival_mp_afghan` (no zone ships). | `mp_playlists.info`; alpha0022 `resources/playlist_descriptors.json` `coop_zm_shapes` |
| arena entries | every `coop_*` row in `mp/basemaps.arena` has `gametype "coop"`, `allieschar "us_army"`, `axischar "opforce_composite"` | code_post_gfx_mp |
| map table | `mp/cocoopmapsettings.csv`: 11 playlist names, 4 of which ship a zone | common_gametype_coop |
| lobby handoff | `handoff/map_handoff.py` `COOP_SESSION_PREFIXES` (`coop_training_`, `coop_prolog_`, `coop_survival_`, `coop_spec_`, `coop_spec1_`, `coop_defense_`, `coop_take_`). `handoff/match_server_control.py` sends `set fs_game "coop"`, `set sv_sessionMap "<playlist>"`, `set sv_zoneType coop`, `set g_gametype coop`, `map <base mp_ level>`. | alpha0022 (read only) |

The nine shipped coop playlist zones are `coop_training_mp_killhouse_sh`,
`coop_spec1_mp_killhouse_sh`, `coop_prolog_mp_crash_intro`,
`coop_spec_mp_crash_tropical`, `coop_defense_mp_crash_tropical`,
`coop_spec_mp_estate_tropical`, `coop_take_mp_estate_tropical`,
`coop_spec_mp_fav_tropical` and `coop_take_mp_fav_tropical`.

**How the client loads it (MEASURED).** The map name decides the zone set. A
`coop_` prefix makes C1 mount the playlist zone plus the base level. The server keys
its world on the base spelling (`map mp_<map>`) and takes the playlist spelling in
`sv_sessionMap`. `LoadMapArchives` refuses only a contradiction it can prove:
a `zm_*` level is always zm and a `coop_*` name is always coop, but an `mp_*` level
can host coop. The asset category reaches `ServerZoneType()` before the entity
string is parsed and before any script is read. Both new native gates rely on that
ordering.

---

## 2. Where the campaign entities are

**They are not in the shared MP entity string.** The base `mp_*` zones carry the MP
entities only, with zero SP player starts on all five base levels (MEASURED in
`ADDON_MAP_ENTS_20260920.md` §5). Each coop playlist zone ships exactly one
`addon_map_ents` (IW4 asset type 42), `maps/mp/<playlist>.mapents`. The server
overlays it onto the base map's entity string at load time
(`InstallEngineClipMap` → `MergeMapEntities`): the base text comes
first, then the addon's text without its worldspawn, with the addon's `?N` trigger
models re-biased. So in a coop match the "shared map" really does carry the SP
entities alongside the MP ones. In an mp match the overlay is never mounted.

---

## 3. Catalogue of the SP entities, per map (MEASURED from the cached zones)

These counts come from the addon entity strings, using
`scratchpad/camp/addon.py` and `graph.py` on `%TEMP%\codol_zones\<playlist>.zone`.

| playlist | ents | enemy actor spawners | ally / civ actors | starts | triggers | vehicles | enemy driver |
|---|---:|---:|---:|---|---:|---|---|
| coop_training_mp_killhouse_sh | 277 | 0 | 66 ally | `info_player_start_so` ×2 | 38 (29 multiple, 2 flag_set_touching, 1 flag_set, 4 radius, 2 use) | blackhawk ×3, ch46e, hummer, littlebird | none (training course) |
| coop_spec1_mp_killhouse_sh | 96 | 0 | 3 ally | `_so` ×1 | 30 | none | none |
| coop_prolog_mp_crash_intro | 161 | 65 (favela ar/rpg/lmg/shgn, desert_elite) | 0 | `_so` ×2, `_pmcDefend` ×2 | 12 (6 `trigger_multiple_spawn`) | `script_vehicle` ×3 (player ch46 ride) | **trigger → spawner chains** (`enemy_a`..`enemy_e`, `badguys`; goal volumes `enemy_?_vol`) |
| coop_spec_mp_crash_tropical | 165 | 96 (82 `pmc_spawner`, 2 juggernaut) | 0 | `_so` ×2, `_pmcDefend` ×2 | 14 (AI-only `A/B/C_enemy_trigger` sf 9, laptop uses) | 2 | **pmc** (`info_volume_pmcDefend`) |
| coop_defense_mp_crash_tropical | 466 | 116 (groups `group_wave_1..3`, `group_a..f`, `group_investigate_heli`) + 3 dogs | 4 ally | `_so` ×6 | 30 | blackhawk_minigun_hero, `script_vehicle` | script-driven groups |
| coop_spec_mp_estate_tropical | 352 | 154 (137 `pmc_spawner`, 3 `juggernaut_spawner`) | 0 | `info_player_start_pmc` ×2 | 66 (13 flag_set, 5 flag_set_touching, 1 spawn, 36 radius bouncing betties, 1 use_flag_set) | none | **pmc** plus objective flags |
| coop_take_mp_estate_tropical | 477 | 112 (`ending_attacker`, `reinforce_*`, `patrol_*`, `post_breach_*`) | 1 ally, 2 vazquez | `_so` ×2 | 55 (27 flag_set, breachIcon) | mi-28 ×3, technical, zodiac | script-driven groups |
| coop_spec_mp_fav_tropical | 283 | 102 (87 `pmc_spawner`, 8 `juggernaut_spawner`) + 17 dogs | 16 civilians | `_pmc` ×2 | 20 | none | **pmc** |
| coop_take_mp_fav_tropical | 334 | 25 (`combat_zone_*`, villain) + 1 dog | 21 civ, 2 ally | `_pmc` ×4 | 73 (67 flag_set) | sedans, technical, uaz van | script-driven chase |

### 3.1 Classnames that matter

* **Spawners.** Every enemy is `actor_enemy_<faction>_<role>_so`, where the faction
  is `favela`, `desert_elite`, `juggernaut_russian`, `dog`, `villian_ortiz*` or
  `vazquez*`, and the role is `ar`, `ak47`, `smg`, `lmg`, `shgn`, `rpg`, `snpr`,
  `makarov` or `deserteagle`. Case varies (`favela_AR_so` / `favela_ar_so`).
  Spawnflag bit 1 (SPAWNER) is set on every enemy (sf 1/3/5/7, 775/1031 for
  juggernauts). Keys used: `targetname` (the group), `target` (a node/struct/volume),
  `count`, `script_noteworthy` (role hint, `juggernaut`), `script_forcespawn`,
  `script_accuracy`, `script_specialops`, `script_difficulty`, `script_laser`,
  `export`.
* **Starts.** `info_player_start_so`, `info_player_start_pmc` and
  `info_player_start_pmcDefend` (IW4 `_specialops_code.gsc`).
* **Triggers.** `trigger_multiple`, `trigger_multiple_spawn` (target = a spawner
  group), `trigger_multiple_flag_set`, `trigger_multiple_flag_set_touching`,
  `trigger_use_flag_set`, `trigger_multiple_breachIcon`, `trigger_radius`
  (`bouncing_betty_gameplay`), `trigger_use` (`intel_collectible`, laptops), and
  `trigger_once`. Keys used: `script_flag`, `script_flag_true`,
  `script_flag_false`, `script_flag_wait`, `script_noteworthy`.
* **Flags.** Estate spec flags include `obj_house_reach`, `cell_enemies`,
  `keyboard_used`, `final_score`, `reach_1st_evac_point`, `player_trying_to_escape`,
  `player_has_escaped`, `point1/2_retreat` and `patrol_start_*` (gated by
  `skip_*`). Take estate adds `player_escape*`, `final_enemy_wave`, `vazquez_dead`
  and similar. Fav spec has `spawn_trigger_a..i`, `technical_coming` and
  `player_has_escaped`.
* **Objective points.** `script_origin` entities named `obj_*`: estate spec has
  `obj_house`, `obj_house_breach`, `obj_clear_cell`, `obj_evacuation_1st` and
  `obj_evac_hostage_2nd`.
* **Volumes and structs.** `info_volume` and `info_volume_pmcDefend` (goal and
  defend volumes), `info_volume_breachroom`, `script_struct` chains (cover points
  with `script_combatmode cover` and `script_flag_wait`), and `info_vehicle_node`.
* **Not present.** No color nodes (`script_color_*`) and no path nodes
  (`node_*`) appear in any addon. AI movement uses the base map's bot waypoint graph
  (`waypoints/<map>_wp.csv`).

### 3.2 Which SP scripts exist

* **In the C1 zones: none.** A sweep of all 90 zones finds 55 `.gsc` rawfiles,
  and every one is a `maps/mp/<map>_fx` / `maps/createfx/<map>_fx` pair. No
  `maps/<map>.gsc` (SP) ships, and no IW4 `so_*` mission script ships either
  (MEASURED, CATEGORY_SCRIPT_SETS §2). The missions' own logic is gone. Only the
  entities remain.
* **In this repo.** `bin/home/codol_rawfiles/maps/_*.gsc` (407 files, including
  `_load`, `_utility`, `_spawner`, `_colors`, `_specialops*`, `_pmc`, `_coop` and
  `_gameskill`) and `animscripts/*` (50 files) are the vendored IW4 SP scripts.
  `COLLATION_MANIFEST.json` lists every one as `source_root: vendor`, taken from
  `alpha0022/vendor/iw4/...` by the now-retired `tools/collate_gsc.py`. They are
  dormant: no loader path ever names them. The MP loader asks for `maps/mp/...`,
  and the coop closure, measured below, never reaches them. They stand on the
  SP engine (`flag_init`/`ent_flag`, `level.player`, SP actor builtins), so they are
  used as references, not compiled.
* **References consulted.** `D:\CoD\COD-GSC-Source\IW4-GSC\maps\_pmc.gsc`
  (elimination), `_spawner.gsc` and `_load.gsc` (trigger and spawner semantics),
  `_utility.gsc` (flags), `_specialops_code.gsc` (starts) and the IW4
  `animscripts\*` (soldier behaviour).

---

## 4. What the engine supported for SP-style AI before this work

* **Native actor lifecycle** (src/online/player/codol_actor_*.cpp): spawnactor /
  spawnactorspawner, aitype `main`/`precache`/`spawner`, nine animscript modules
  per species, a native frame that acquires enemies (sight trace, or perfect info
  per species), plans routes over the bot waypoint graph, moves by clip root
  motion, switches to the species' `combat` module inside `meleeAttackDist` when
  `ActorMeleeRangeGoal`'s knee and head lines are clear, and runs pain,
  death and corpse handling. Species rows: `dog` (mp) and `zombie` (zm).
* **Scripted animation**: setanim*, setflaggedanim*, clearanim, getanimlength,
  animhasnotetrack, notetracks, animmode and orientmode
  (script_anim.c, actor_lifecycle.cpp).
* **Weapons**: no AI-fire builtin (`shoot` is not registered). `magicBullet`
  (script_magicbullet.c) is a native hitscan that publishes EV_FIRE_WEAPON
  (world-owned unless the owner is a player).
* **Map entities**: `actor_*` map entities are plain script entities. The map
  spawn table does not dispatch them, so they are inert templates the mission
  reads. Exact-classname spawn tables meant the SP trigger subclasses had no volume
  at all (see §5.2).
* **Client side (MEASURED, CODOL_ACTOR_SPECIES §3)**: C1 composes humans on tree
  selector 0 (`generic_human`) when serverinfo-2482 bit 0x01 is set, and loads
  `animlists/common_human`. That rawfile ships only in `common_gametype_coop`,
  together with the full IW4 exposed/run/death/pain human xanim set (`exposed_aim_5`,
  `run_lowready_F`, `exposed_shoot_*`, `exposed_death*`, ...). All of those are
  nodes of the `generic_human.atr` that `common_mp` ships.

---

## 5. Design of the first playable mission layer

### 5.1 Files

| file | role |
|---|---|
| `src/online/player/actor_scripts.cpp` | new species row `soldier`: category `coop`, classname `actor_enemy_coop_soldier`, animtree `generic_human`, tree selector 0, animlist bit 0x01, talk bit 0x04, animscript `maps/mp/animscripts/soldier`, sight-based (not perfect info) |
| `src/online/script/coop_entities.c` (new) | `CoopSpawnClassAlias`: in a coop match only, the spawn-table lookup of `trigger_multiple_*`, `trigger_use_*` (not `trigger_use_touch`), `trigger_radius_*`, `trigger_once_*`, `trigger_damage_*` and `info_volume_*` uses the base classname. The entity keeps its own classname. |
| `src/asmsource/_g_spawn_mp.asm` | `G_SpawnEntitiesFromString` calls the alias before its table walk (`%ifdef OL_SERVER`) |
| `coop/aitype/enemy_coop_soldier.gsc` (new) | the soldier template: faction and role handed in through `level.coop_soldier_next`, health and accuracy, role engagement distance (`meleeattackdist`), role weapon |
| `coop/character/character_coop_soldier.gsc` (new) | militia, shadow company and juggernaut bodies and heads, restricted to the models the mounted playlist zone ships |
| `coop/maps/mp/animscripts/soldier_{init,stop,move,combat,death,pain,flashed,turn,jump,shared}.gsc` (new) | the nine species modules plus helpers: aim pose, run and patrol walk, exposed burst fire through `magicBullet`, reload, death and pain clips |
| `coop/maps/mp/gametypes/_coopmission.gsc` (new) | the entity-driven mission (§5.3) |
| `coop/maps/mp/gametypes/_cooptest.gsc` (new) | probe harness, inert unless `+set scr_coop_test 1` |
| `coop/maps/mp/gametypes/_coopmode.gsc` | starts the mission, precaches `tag_origin`, adds health regen, AI damage scale, lives, no player-on-player damage, `spawned_player`, `Callback_ActorKilled` |
| `coop/maps/mp/gametypes/_callbacksetup.gsc` | `CodeCallback_ActorKilled` → `level.callbackActorKilled` |
| `tools/script_resolve.py` | the coop category seeds are now the soldier aitype and animscripts |

### 5.2 MP and zm isolation (the hard requirement)

1. **Scripts.** Everything new lives under `bin/home/codol_rawfiles/coop/`. That
   root is probed only when `ServerZoneType()` is `coop`
   (script_source.c). The coop closure is 20 scripts: 18 from `coop/` plus
   `codescripts/delete` and `codescripts/struct`. It names no MP module (resolver
   `--list-reachable`). No shared or MP script was edited.
2. **Species.** The `soldier` row's category is `coop`.
   `OL_ActorScriptsLoadMapTypes` preloads a species only in its own category,
   so mp and zm skip it and log the skip.
   `ActorSpeciesAnimListMask("mp")` and `("zm")` are unchanged (0x02 and
   0x08), and no mp or zm map authors `actor_enemy_coop_soldier`.
3. **Spawn alias.** `CoopSpawnClassAlias` returns its argument unchanged
   unless the category is `coop`. The SP subclasses occur only in the coop addon
   overlay, which mp never mounts.

**How "MP unchanged" was verified (MEASURED 2026-10-07):**

* `mp_estate_tropical`, `g_gametype war`, `sv_zoneType mp` was run twice on
  port 17777: once on this patch's exe and once on an exe built from the base
  commit `70b355bd` (`git archive` into the scratchpad, same make line). A
  scratchpad-only level-script overlay logged an entity census. The log lines
  compared were the census, the full lowering list, the weapon window, the
  animation-class mask, the waypoint graph, every error and every spawn. The two
  logs are **identical**: 504 entities, 33 classnames, the same per-classname
  counts and the same `mp_tdm_spawn` origins. The only difference is which spawn
  point the randomized bot spawn picked.
* `python tools/script_resolve.py --root bin/home/codol_rawfiles --only maps/mp`
  passes. The war/estate closure and the zm closure (`--category zm`) pass against
  the built binary.
* `make -j2 -k GAME=codol OBJ_DIR=obj/cp-test test` exits 0.

### 5.3 The mission rules (`_coopmission.gsc`)

* **Players** spawn at `info_player_start_pmcDefend` (defend maps) /
  `info_player_start_so` / `_pmc` / `info_player_start`, which the existing
  `_coopmode` already did and the addon overlay now supplies. The first player's
  start is chosen deterministically, and later players take the closest free
  start to it (§8.1).
* **Soldiers.** For each authored enemy spawner the mission reads the classname
  (faction, role), `script_noteworthy` (`juggernaut`) and `script_accuracy`, then
  spawns one native soldier from the runtime template at the spawner's origin and
  angles. Dogs, civilians and scripted hostages are skipped for now.
* **Elimination** (IW4 `_pmc` `mode_elimination`), on any map with
  `pmc_spawner`s: the kill goal is `scr_coop_enemies` (default 20, capped at the
  spawner count). Up to `scr_coop_max_alive` (default 10, capped at 24; the native
  pool is 32 actors) are alive at once. Spawners are picked at least 1024 units from
  every player. One soldier in three hunts the players, and all of them do once 6
  or fewer remain, when they are also marked on the compass.
* **Trigger chains** (IW4 `_spawner` / `_load`): a trigger whose `target` names
  enemy spawners spawns them on a player's touch (each spawner hands out `count`
  soldiers), once. A `trigger_*flag_set*` (or `targetname flag_set`) sets its
  `script_flag`. `script_flag_true` arms a trigger and `script_flag_false`
  disables it. A spawner with `script_flag_wait` spawns when its flag is set.
* **Group waves** (fallback): on a map with neither pmc spawners nor chained
  spawners, each spawner `targetname` group is one wave, the group nearest the
  players first.
* **Mission complete** comes from the elimination goal or the last wave being
  cleared, then any laptop objectives, then the map's exit trigger if it has one
  (§8). It also comes from the last chain spawner being spent and cleared, from
  the pit course being cleared (killhouse maps), or from `mission_complete` /
  `level_complete`. The first version also ended the mission on
  `player_has_escaped` and `reach_1st_evac_point` alone, so a player on estate
  could end it by walking into the house volume. Those flags are now only the
  exit, after the objective. **Mission failed** comes
  when every player is out of lives (`scr_coop_lives`, default 3 respawns). Either
  way: a HUD line, frozen controls, `scr_coop_end_delay` (10 s), then
  each player's result is reported (`codol_reportCoopMission`) and `exitLevel(false)`
  returns everyone to the lobby; the server then parks instead of rotating (§13).
* **Soldier combat** (`soldier_combat.gsc`): the native frame enters combat inside
  the role's engagement distance with a clear line. The soldier faces the enemy,
  holds `exposed_aim_5`, layers `exposed_shoot_*` on `add_fire`, and fires bursts
  with `magicBullet` from 32 units in front of the chest (superseded on
  2026-10-08: the soldier's own fire through `codol_actorShoot`, see "Coop AI
  combat" at the end). Hit chance is
  `self.accuracy`, halved at the edge of the engagement band; misses go a body width
  wide. He holds fire when a squadmate is in the line, and reloads every 30 rounds. The gun world model is not attached by default (§7).
  `_coopmode` scales damage from non-player attackers by
  `scr_coop_ai_damage_scale` (0.5, IW4 regular's ratio). Players regenerate after
  5 s.
* **Weapons.** An AI may fire only a slot of C1's coop weapon window that this
  server publishes (slots 1..31; MEASURED, the window stops at slot 32
  `mp5k_codo_so`). The roles map to `m16_basic_so`, `ump45_so`, `rpd_so`,
  `aa12_so`, `cheytac_silencer_so` / `barrett_so` and `beretta_so`.

---

## 6. What was verified on the probe (port 17777, scratchpad home, developer 1, test client)

| map (playlist) | load | player spawn | AI | end |
|---|---|---|---|---|
| coop_spec_mp_estate_tropical (**first map**) | addon overlaid, 154 spawners / 137 pmc / 22 triggers, no script errors | bot at `info_player_start_pmc` | pmc soldiers spawn ≥1024 u away, route over the 447-node graph, acquire the bot, enter combat, fire (`rpd_so` from 449 u, `cheytac_silencer_so` from 773 u) and kill it | **complete** (6 of 6 eliminated → MISSION COMPLETE → exitLevel → coop replay). **failed** (bot out of lives → MISSION FAILED → exitLevel). The flag triggers fire on touch (`obj_house_reach`, `player_trying_to_escape`). |
| coop_prolog_mp_crash_intro | addon overlaid, 65 spawners / 47 chained / 12 triggers | bot at `info_player_start_pmcDefend` | each of the 6 `trigger_multiple_spawn` triggers spawned its group on touch (5+8+7+9+8+10 spawners, badguys `count 2`) | **complete** (57 spawned, 57 killed → "every spawner spent and cleared" → exitLevel) |
| others | see §6.1 | | | |

### 6.1 Per-map status

Each map was loaded on the probe through the lobby's own rcon sequence
(`fs_game coop`, `sv_sessionMap <playlist>`, `sv_zoneType coop`,
`g_gametype coop`, `map <base>`). A test client was then added, and the harness
killed the enemies to drive each map to its end. No map produced a script error.

| playlist | mission rule picked | status |
|---|---|---|
| coop_spec_mp_estate_tropical | elimination (137 pmc) | **playable end to end**: spawn, AI fight, complete and fail both reach `exitLevel` |
| coop_spec_mp_crash_tropical | elimination (82 pmc) | **playable end to end**: AI fire (`ump45_so` at 1311 u, `cheytac_silencer_so` at 1000 u), complete → `exitLevel` |
| coop_spec_mp_fav_tropical | elimination (87 pmc) | **playable end to end** (complete → `exitLevel`). The 17 dogs and 16 civilians are not spawned. |
| coop_prolog_mp_crash_intro | trigger chains (47 chained) | **playable end to end** via the 6 spawn triggers. In the real mission those triggers sit on the ch46 ride path (z 731–1067), so on foot only `spawn_badguys` (z −61) is reachable; the heli ride is not implemented. |
| coop_defense_mp_crash_tropical | group waves (20 groups) | AI spawn by group and fight (rpd/m16/ump45/aa12 firing at 159–1358 u), waves clear in order. Completion was not run to the 20th wave in the survey. |
| coop_take_mp_estate_tropical | group waves (18 groups) | AI spawn and fight, waves clear. Breach rooms, vehicles and the vazquez chase are not implemented. |
| coop_take_mp_fav_tropical | group waves (16 groups) | AI spawn, waves clear. It is really a vehicle chase (`combat_zone_*`, sedans, technical), so the waves are a stand-in. |
| coop_training_mp_killhouse_sh | none (0 enemy spawners) | loads, player spawns, flag triggers fire. It is a training course with ally actors and range logic, so there is no mission end. |
| coop_spec1_mp_killhouse_sh | none (0 enemy spawners) | as training (`at_top_of_ladder` flag fires) |

---

## 7. Left / known gaps

* **Client rendering is not yet seen in C1.** Every check above is on the server
  with a test client. Still unproven: that C1 draws the selector-0 `generic_human`
  soldier with the mask-0x01 animlist (the zombie selector-3 path is the precedent),
  and that attached weapon models and the additive fire layer look right.
* **`codol_isWeaponPublished` returns 0 in coop** after an in-process map change,
  even for the player's own `mp5_silencer_so`, and the server warns "selection
  weapon 23 has no published C1 index" (hatchet). The engine-to-wire reverse map
  is not trustworthy in coop. The aitype falls back to mounted window slots. This is
  a pre-existing weapon-window issue to fix in src/online/weapon.
* **Pre-existing coop crash fixed here:** a player with a server DObj in coop hit
  `Sys_Error: model 'tag_origin' not precached` (the native world-gun child). Coop
  now precaches it.
* **A coop runtime script error stops the server** (as everywhere). Two were found
  during the probe. Float `++` on the client `.kills` field is fixed. The other is
  `attach` of the soldier's weapon world model: `weapon_cheytac` was precached by
  the aitype and attached fine on estate, yet it raised "model 'weapon_cheytac' not
  precached" on coop_spec_mp_crash_tropical after an in-process map change. The
  model-slot lookup (G_ModelIndex's constant-configstring and high-model paths) is
  the suspect. Until that is fixed natively, soldiers carry no visible gun model:
  the attach sits behind `+set scr_coop_ai_weapon_models 1`, off by default.
  Their shots, fire sound and tracers come from `magicBullet` regardless.
  (Superseded on 2026-10-08: the cause was a native model-slot clobber, now
  fixed; gun models are on, and magicBullet fire was never visible or audible
  to C1. See "Coop AI combat" at the end.)
* **The weapon window size varies by map** (MEASURED: 32 of 64 on estate / prolog,
  33 on crash and fav, 36 on spec1, 64 on training). The AI roles use only slots
  2..30.
* **Not done yet:** dogs (`actor_enemy_dog_so`: needs a coop dog species row on
  `common_dog`, which `common_gametype_coop` ships); RPG fire (the `rpg` role fires
  a rifle); grenades; cover use (`script_struct` cover chains, goal volumes
  `script_goalvolume` / `info_volume`); allies and civilians; vehicles and turrets
  (`script_vehicle_*`, `misc_turret`); breaching (`info_volume_breachroom`); the PMC
  defend mode (`info_volume_pmcDefend`) and juggernaut pacing; the objective HUD
  text and objective chains (`obj_*` origins); the downed / revive state
  (`EnableDeathShield`); challenge stars and timers; the training course logic
  (killhouse maps have no enemies); difficulty selection; the coop class chosen in
  the profile.
* **AI movement on these maps** uses the MP bot waypoint graph. On estate several
  links wedge on clip brushes. The actor stuck detector re-plans, but movement is
  rougher than on the zm maps the planner was tuned on.
* **Lobby:** no change is needed for these maps (it already sends the five
  commands above). `coop_survival_mp_afghan` ships no zone and stays refused.

---

## 8. Spawn fix, opening rides and the map's scripted events (2026-10-07, second pass)

### 8.1 "I spawned outside of the map" (coop_training_mp_killhouse_sh)

**Cause (MEASURED).** That addon authors two `info_player_start_so`:
`non_CQB_spawn_point` (5560 −1866 16) on the camp road in front of the
`humvee_player` jeep, which is the mission's opening, and `CQB_only_spawn_point`
(−133 −1129 8) by the hangar, which is IW4 trainer's skip-to-the-pit start. The
first-version pick was `points[randomInt(points.size)]`, IW4 `_specialops_code`'s
`random(p1_starts)`. That is only right because an IW4 `so_*` level script
deletes the starts it does not want, and none ships. So every load was a coin
toss. The VPS log of the user's bad load shows the player touching the hangar's
`at_ssdd_hanger` trigger, i.e. the CQB start. After his restart he landed at the
road start. Nothing about the first load itself differs (category, overlay and
entities are all identical): it was the random pick.

**Checks.** `checkstarts` in `_cooptest.gsc` ground-traces every start and drops
a test client on it. Every addon start on all nine maps has solid ground within
32 units and the player stays put. No start is in the void. The MP bot waypoint
graph only covers the MP play area, so the killhouse coop starts are 1500–3000
units from the nearest node. They are inside the full IW4 trainer level that the
`mp_killhouse_sh` BSP carries (its script_brushmodels span x −1619..5337,
y −4400..1275).

**Fix (`_coopmode.gsc`).**
* The first player's start is chosen deterministically. Starts are ranked by
  their authored names: "only" starts (CQB_only) last, numbered or "_2nd" starts
  (`player_start_point02`, `estate_player_start_2nd`, `4_coop_start_02`) after
  the plain one, ties in map order.
* A start must pass `isUsableStart` (ground within 256 units).
* Later players take the closest free start to the first (IW4
  `get_closest_exclude` / player 2 near player 1). The old code took the farthest,
  which would have sent player 2 to the CQB start.
* MEASURED on fresh probe processes (true first loads): training always spawns at
  `non_CQB_spawn_point` and logs `CoDOL coop: primary start ...`.

### 8.2 The opening rides (`_coopintro.gsc`, `_coopvehicle.gsc`)

| map | evidence | implemented |
|---|---|---|
| training | `script_vehicle_hummer_viewmodel` `humvee_player` (model `vehicle_hummer_viewmodel`, `checkmodel`: 44 bones, complete) → `info_vehicle_node` chain 6348 (25 mph) → 2870 (15) → 2349 (10) → 1933 −1748, ending at the `player_in_camp` trigger; `hummer_*` engine aliases and Price's `pit_pri_*` lines ship in the playlist zone | **implemented.** A script_model jeep follows the node chain at the authored speeds (mph × 17.6) with the riders linked and weapons lowered and the engine loop playing, then sets them down beside it. MEASURED: the ride fires `start_open_gate_handler` and `player_in_camp` on the way. **Dialogue is INFERRED**: `pit_pri_001`, `_002_1..3` and `_003` play in alias order, because no script or string says which lines belong to the ride (`level.coop_intro_lines`, `scr_coop_intro_dialogue 0`). |
| prolog | `script_vehicle` `seaknightPlayer` (`vehicle_ch46e_opened_door_interior`) → `script_struct` path `chopper_start` … `player_heli_crash` (907 718 185), `script_flag_set player_heli_a..f` on the nodes; the `trigger_multiple_spawn` volumes for `enemy_a..e` sit on the path at z 731–1067 | **implemented.** The riders fly the path with weapons up, and the path's flags are set. MEASURED: the five spawn triggers fire as the helicopter passes and the enemies engage it from 1086–1358 units. At the crash node the riders are set down at the crash site and the CH-46 is hidden. Not done: the crash animation and shellshock (`player_heli_crash` is set, nothing plays). |

### 8.3 Scripted events from the entity keys

Statuses: **impl** = implemented and seen on the probe; **partial**; **no** = not
implemented, with the reason.

| event | evidence (maps) | status |
|---|---|---|
| MP game-object cleanup | base-map ents with `script_gameobjectname` (hq crates, sab/dd bombzones, dom flags) and `targetname delete_on_load`, removed as IW4 MP `_gameobjects::main` and SP `_load` do | **impl** (96–267 entities per map) |
| per-mission entities | `script_specialopsname` on base ents (`course_end_blocker` for training; a `coop_spec2_mp_killhouse_sh` blocker that ships no zone) | **impl**: kept only for the named session map |
| flag triggers | every `trigger_*flag_set*` / `targetname flag_set` with `script_flag`, `script_flag_true`, `script_flag_false` (all maps) | **impl** |
| spawn triggers | `trigger_multiple_spawn` → spawner `target` chains (prolog `enemy_a..e`, `badguys`) | **impl** |
| flag-named spawner groups | flag `combat_zone_10..60` ↔ spawner `targetname combat_zone_<n>` (take fav) | **impl**: the group spawns when its flag is set |
| numbered objective chain | `obj_40_arrive`, `obj_45_arrive`, `obj_47_arrive`, `obj_50_complete`, `obj_75_arrive` flag triggers (take fav) | **impl**: compass objective per step in number order; the chain completes the mission |
| objective marker | `script_origin obj_house` + flag trigger `obj_house_reach` (estate spec, take estate) | **impl**: compass objective until the flag |
| laptops (IW4 `_pmc` mode_objective) | `script_model pmc_objective` A/B/C → `trigger_use` (crash spec) | **impl**: one objective each, done on use, required before the mission ends |
| exit / extraction | `reach_1st_evac_point` (estate spec); `player_has_escaped` (estate spec, fav spec, take estate). These are interior volumes, not IW4's out-of-bounds ring; estate's west box contains `estate_player_start` | **impl**: after the main phase, "get to the extraction point" with a compass marker, complete on the flag. The first version's "flag alone ends the mission" was wrong and is removed. |
| escape warning | `script_noteworthy player_trying_to_escape` triggers | **partial**: warns once per entry only on a map where no player start lies inside them (take estate). Disabled where they contain a start (estate spec, fav spec). |
| intel pickups | `trigger_use intel_collectible` + `script_model` (prolog, crash spec, estate spec: 4 each) | **impl**: picked up on use, model removed |
| weapon pickups | `trigger_use c4_pickup` → 3 C4 models (training) | **impl**: gives `c4_so` |
| player vehicle rides | `humvee_player`, `seaknightPlayer` (§8.2) | **impl** |
| vehicle groups | triggers with `script_vehiclespawngroup` / `script_vehiclestartmove` and vehicles of the same group with paths (training trucks and helis; prolog and crash spec `mi17` rappel helis gated `script_flag_true so_rappel_heli_*`; take fav technical) | **partial**: implemented as script_model movers along their paths. The prolog and crash rappel helis never come, because their gate flags are set only by the unshipped level script. No unload, no turret fire. |
| pit course (killhouse) | base-map pop-up targets (`target_enemy` / `target_friendly` brushmodels → `script_origin` pivot → aim-assist brush), 9 `target_trigger` course triggers (`script_linkTo pf1492_<n>` ↔ `script_linkName <n>`), `killhouse_buzzer`, `target_*` aliases (IW4 `trainer.gsc` / `so_killspree_trainer.gsc`) | **impl**: targets start down and pop up as each trigger is reached, take hits only while up, and play the hit / civilian buzzer sounds. A timer HUD counts up, and the run reports time, enemies and civilians hit. Clearing all 24 enemy targets completes the mission (training and spec1). MEASURED run: 24/24 in 19.5 s. Not ported: rail movement (`use_rail`), the melee-only target, star thresholds. |
| cargoship / deck / frag targets | `cargoship_target` `position_*_targets`, `deck_target` hostile / friendly, `grenade_target_dummy` (base killhouse) | **partial**: each group pops up when a player comes within 512 units and re-arms 20 s after it is cleared. They are not tied into a course. |
| obstacle course | `obstacleTraining_*`, `obstacle_course_start/end`, `start_course`, `crawl_dialog`, … flag triggers (training) | **partial**: the flags fire, but there is no timer or dialogue, because the mapping of the `pit_*` lines to these flags is unknown |
| ambient / scripted AI | `script_origin` `*_origin` animation anchors (sitman, pushups_guy, …), 66 ally actors with `script_friendname` / `script_forcecolor`, civilians (fav), dogs | **no**: there is no ally, civilian or dog species in coop yet |
| breach / slow-mo | `info_volume_breachroom`, `trigger_multiple_breachIcon`, `script_slowmo_breach` (take estate) | **no**: no door-breach script or time-scale support |
| doors / gates | base killhouse `slide_gate_*`, `gate_cqb_*` movers | **no**: their open direction and trigger are in the unshipped script |
| exploders, color triggers, `script_sound` | none present in any coop addon (key counts) | n/a |

### 8.4 Per-map end-to-end probe runs (test client, harness-driven kills)

| playlist | start (every load) | sequence seen | end |
|---|---|---|---|
| coop_training_mp_killhouse_sh | `non_CQB_spawn_point` | jeep ride into camp → pit course (24 targets, timer) | **COMPLETE** (course cleared in 19.5 s) |
| coop_spec1_mp_killhouse_sh | unnamed `_so` (3617 −2419 513) | pit course | **COMPLETE** |
| coop_prolog_mp_crash_intro | `pmcDefend`, then rides the CH-46 | heli ride; five spawn waves engage it; set down at the crash | the trigger chain completes once a player reaches `spawn_badguys` on the ground (the harness does not walk there) |
| coop_spec_mp_crash_tropical | `pmcDefend` | elimination + 3 laptops | **COMPLETE** |
| coop_defense_mp_crash_tropical | `player_start_point` | 19 group waves (the template no longer counts as a wave) | waves clear in order; 105 spawned / 101 killed before the harness loop ran out |
| coop_spec_mp_estate_tropical | `estate_player_start` | elimination → extraction at `reach_1st_evac_point` | **COMPLETE** |
| coop_take_mp_estate_tropical | `estate_player_start` | 17 group waves → extraction at `player_has_escaped` | **COMPLETE** (111 spawned and killed) |
| coop_spec_mp_fav_tropical | `info_player_start_pmc` | elimination → extraction at `player_has_escaped` | **COMPLETE** |
| coop_take_mp_fav_tropical | `info_player_start_pmc` | objective chain obj_40 → obj_75; `combat_zone_*` groups spawn on their flags | **COMPLETE** |

No script runtime error on any map in the final runs. Two found during this pass
are fixed: `getEntArray(value, "script_noteworthy")` returns undefined on this
server, and `setTenthsTimerUp(0)` is rejected. The group-wave scan treated the
runtime soldier template as a group; that is fixed too. AI hits on a player are
now capped at 40 per bullet: a sniper headshot one-shot a full-health test client.

## 9. The killhouse training mission end to end (2026-10-07, third pass)

User report on 8.x: the player "can't progress past the gate or doors on the
killhouse map", "the jeep just floats in air while I spawned on foot outside the
double car gates". This pass supersedes the training rows of 8.2 and 8.3 (jeep
dialogue, obstacle course, ambient AI, doors / gates, cargoship targets).

### 9.1 The jeep ride (`_coopintro.gsc`, `_coopvehicle.gsc`)

* **Height (MEASURED).** The authored humvee origin is z 92 and the vehicle
  nodes are z 40–44 over a road at z 0. The model is now parked on, and driven
  along, the ground under them (`ground_under`, `follow_path(..., snapGround)`);
  ground-vehicle groups in `_coopevents` use the same snap.
* **Seated at spawn.** `_coopmode::spawnPlayer` calls `_coopintro::board()`.
  Any player who spawns before the ride is over is linked into the next seat,
  including a teammate who joins while it drives. The ride starts when a rider
  turns his view, or after `scr_coop_intro_wait` (8 s). VPS evidence for the
  old behaviour: the ride ran 1.5 s after the spawn, while the real client was
  still fading in.
* **Gates.** The road gates the jeep drives through are opened at load
  (`slide_gate_03` east cluster, `slide_gate_01` / `_02`, the double car gates).
* **Driver.** Price (`script_noteworthy driver`) is placed at his authored
  drop-off spot when the ride ends. His lines play from the jeep during it.

### 9.2 Stations, gates and lines (`_cooptraining.gsc`)

The order and station of every line was established by listening to the
shipped `pit_*` sound files. That is a private transcription, kept outside the
repository; no line text appears here. Durations are the files' own lengths.
Aliases are the zone's (`docs/zone_assets/coop_training_mp_killhouse_sh.txt`).
The `_r`-suffixed file names have no alias, which was MEASURED as a script
runtime error.

| station | speaker (addon key) | waits for | doors / gates opened | lines |
|---|---|---|---|---|
| jeep | jeep (Price drives) | ride | `slide_gate_01/02/03` (east) at load | pri_001, 002_1..3, 003 |
| to the range | Price (`driver`) | flag `inside_firing_range` or near Tiny | `arsenal_door` raised | nags pri_004..006 |
| range | Tiny Man (`waters`) | hits on `lane1` rifle dummies + `ads_tutorial_target` (3, 3, 2, 5 timed), then the knife on `melee_target_dummy` | — | tiny_001..003, 005..009, 011_1/2, 018 or 017, 012, 019..025; rifle and sidearm given; flags `sidearm_complete`, `start_frag_training` |
| demolitions | Sgt. Newcastle (`nwc`) | near him; flag `near_grenade_area`; 3 `grenade_fire`; launcher fire (`missile_fire` or `grenade_fire`) 1 then 3 | `gate_cqb_enter`, `gate_enter_grenade` (pit), `gate_cqb_exit` (−95) | ncastle_001, 002, (003 nag), 004..010, 012..014_2; frags and `m16_grenadier_reddot_so` given |
| obstacle course | Mac (`mac`) | flag `wait_line_up`, then `obstacle_course_end` | `gate_cqb_exit2` (both), then `gate_cqb_enter_main` (−170) | mac_001..003 (sets `start_obstacle`, `start_course`), 005 on `obstacleTraining_mantle2`, 006 on `crawl_dialog`, 004 / 007 timed nags, 008_1 |
| to the hangar | Price (`price2`) | flag `come_to_price`, then `at_ssdd_hanger` | `slide_gate_03` west (hangar yard, two halves part) | — |
| pit briefing | Harker (`sas3`), Price (`price`), Soap (`sas1`), Ghost (`sas2`) | — | `gate_cqb_start` | har_001..003, pri_007, har_004, mct_001, har_005, gst_001, pri_008..011 |
| pit course | Price | a run of the `_coopcourse` pit course under 60 s (repeats) | `gate_cqb_finish`, `gate_cqb_out` after | pri_050 per slow run; pri_058 (≤ 23 s) or pri_056 |
| ship mock-up | Price | near the ladder; each `position_*_targets` group cleared (one, two, 4, melee, 5, 6) | `ship_hanger_door`, `ship_hanger_clip`, `hanger_one_door` raised | pri_012..019, 025, 030, 029, 035, 039, 041, then 042 (< 60 s) or 055, 044; MP5 and flashbangs given |
| debrief | Price | near the monitors (`debrief_trigger` area) | — | pri_061..064, 065_1, 065_2 → **mission complete** |

* **Doors.** A prefab door is IW4 `make_door_from_prefab` / `door_open`:
  brushes and models link to the nearest `script_origin` of the targetname
  (several prefabs may share one), which rotates by its open angle (default
  +90). The `blocker` brush is hidden and made non-solid. The sound is
  `scn_training_fence_open`.
* **Sliding gates.** Each panel slides 320 units away from the wheels' centre.
  This direction is INFERRED; no open direction is authored.
* **Timeouts.** Every wait has one (15–180 s), so a task the server cannot
  judge (a grenade into a window, a flashbang) never blocks the mission.
* `_coopmission` runs no other mission type while the training flow is active.
  `_coopcourse` leaves the `cargoship_target` groups to it.

### 9.3 The cast

Friendly soldiers of the coop soldier species. The template is
`setspawnerteam("allies")` and the kind carries `team allies` and the authored
body and head. They have `setCanDamage(false)`, no hunting, and stand at the
authored spawner origin and angles. `soldier_stop` loops the spawner's
`animation` (`self.coop_idle_anim`, `setFlaggedAnimKnobAllRestart` +
`getAnimLength`) or the casual idle. 26 are placed: the named instructors
(waters, nwc, mac, price2, chair_guy, price, sas1..3) and 18 ambient soldiers
with an authored idle (sentry, sitman, chatter, leaner, grumbler, phoneguy,
repair, engine, writter, pushups, package, loading, sitting_talking, toss,
chatter2, basketball 1/2, ...). Price (driver) is placed after the ride. Every
6 s, an ambient soldier within 350 units of a player says the next
`pit_rgr1..4_*` line, at most once per 25 s per soldier. Body / head by
classname: price → `body_desert_tf141_assault_a` + `head_hero_price_desert`;
soap, ghost (socom / forest), harker and socom → seal socom; tf_141 forest;
sas woodland; blackkit; ustshirt; the rest `body_complete_sp_lw` +
`head_complete_sp_lw`. All are precached in StartGameType.

### 9.4 Probe (port 17777, test clients, `scr_coop_test_cmd route`)

`route` in `_cooptest.gsc` acts on `level.coop_training_step` the way a player
would: it walks to flag triggers and points, shoots or knifes the range
dummies, raises the throw notifies, runs the pit course, and shoots the
mock-up's groups. MEASURED, one fresh load:

* bot0 spawned and was seated (seat 0). The ride started after 8 s and played
  pri_001..003.
* bot1, added 12 s later during the ride (late begin), was seated in seat 1
  with `ride started 1`.
* Both were set down at (1933 −1748, z 3), and Price was placed.
* Every station in 9.2 ran in order, every door and gate logged its parts
  opening, and every line fired without a script error.
* The pit course took 11.5 s (pri_058), the mock-up 26.4 s (pri_042), and the
  debrief ended in **MISSION COMPLETE**.
* The cast showed 26 actors, stationary on their authored idles.

The first run found `pit_pri_0xx_r` aliases missing (fixed: the zone's aliases
have no `_r`).

**Not verified with a real C1 client:** the jeep's look from the seat, the gates'
slide direction against the fences, and the prefab doors' swing. Also not
done: the obstacle-course timer, Mac's buddies running the course, Price
walking the player between stations, the deck course, and the
`gl_too_low` / window-specific grenade checks.

## 10. Killhouse, fourth pass: animations, Price at the wheel, nametags, the range door (2026-10-07)

User report on 9.x (build 647dc7ae): "the characters need their animations, captain price isn't driving the jeep, and they
should have their nametags as well, finally the door to the firing range didn't open so I couldn't progress".

### 10.1 NPC animations

Two faults stacked:

* **The idles are animations a coop client cannot play (MEASURED, static analysis of C1.exe and the zones).**
  * C1 loads one map animlist per level, `animlists/<mapname>.animlist`. The loader is `0x450b3e` (`Dvar_FindVar("mapname")`), which hands it to `0x408310`.
  * The gamestate sets `mapname` to the level, `mp_killhouse_sh`. That animlist lists only a fan.
  * Every trainer idle chosen in pass 9 (`killhouse_*`, `civilian_sitting_*`, `training_*`, `cliffhanger_welder_*`, `humvee_*`) appears only in `animlists/coop_training_mp_killhouse_sh.animlist`. No client loads that list.
  * The idles now come from the animlists every coop client loads: `common_human` and `common_actor_shared`. Their xanims ship in `common_gametype_coop`.
  * Mapping, authored idle → common anim:
    * sitting, repair, welding and push-ups → `casual_crouch_idle`
    * leaning → `parabolic_leaning_guy_smoking_idle`
    * texting → `patrol_bored_idle_cellphone`
    * sas/toss → `patrol_bored_idle_smoke`
    * stand-talk → `casual_stand_v2_idle`
    * the rest → `patrol_bored_idle`
  * Per instructor:
    * Tiny Man and Newcastle → `patrol_bored_idle`
    * Price and Mac → `casual_stand_v2_idle`
    * Lootz → `casual_crouch_idle`
    * SAS → `exposed_idle_alert_v1`
  * A talking instructor plays `casual_stand_v2_twitch_talk` for the length of each line, then returns to his idle.
* **The idle loop broke the whole animation delivery.**
  * `soldier_stop` restarted the non-looping idle every cycle (`setFlaggedAnimKnobAllRestart`). That left a fading clone at phase 0.
  * The pose bootstrap refuses that state. The VPS log shows `pose bootstrap: ent 109 ref 1039: non-looping clone at phase 0` and `1025 commands exceed the 1024 per-snapshot cap`.
  * A refused bootstrap drops that client's animation delivery for the snapshot.
  * Fix: a cast member's pose is now sent once per change, with `setFlaggedAnimKnobAll` and no restart. Gestures are replayed with `setAnimTime 0`.
* MEASURED on the probe: the cast's anim commands use indices 820–1021 (common lists only). There were no bootstrap or delivery faults.

### 10.2 Price drives the jeep

* A model of Price (body `body_desert_tf141_assault_a` plus his head) is linked to the jeep in the front-left seat. The players' seat 0 is front right.
* The model plays `bh_pilot_idle` through `PrecacheMpAnim` / `scriptModelPlayAnim`. That is the one seated idle in the common lists (`common_actor_shared`); its xanim ships in the training zone. The trainer's `humvee_idle_frontl` is in the playlist list only.
* Price's ride lines play from that model.
* At the stop the model is removed and the Price soldier stands at the driver's door.
* INFERRED: the seat offset (−6, −17, 20) and the pose under the wheel are not verified on a client.

### 10.3 Nametags (native: `src/online/script/script_overhead_name.c`)

* **C1 draws no name for an actor (MEASURED, C1.exe):**
  * Every overhead-name path ends in `0x439520`, which prints `CS 1280 + client`.
  * Its script-mover caller `0x43a3b0` prints it over an eType-6 entity whose entityState `+0x64` is client + 1 (1..0x13). The text sits at the model's tag + 10, or at origin + 82.
  * That caller is gated by client dvar `"772"` (default 0).
* **What the server now does:**
  * New method `<script_model> codol_setOverheadName( name )`, coop only.
  * It reserves the highest player-name slot no client occupies, puts the name in `CS 1280 + slot`, and sets the mover's `characterClient` (published at `+0x64`) to slot + 1.
  * `RefreshServerPlayerNames` keeps reserved slots. It sends them quoted, because names have spaces.
  * A client connecting into a reserved slot takes it over; the reservation is dropped and its movers cleared.
* **What the script does:**
  * Each named cast member (`script_friendname`: Captain Price, Tiny Man, Sgt. Newcastle, Mac, Pvt. Lootz, Colonel Harker, Soap, Ghost, Harker) gets an invisible `tag_origin` mover at its feet.
  * Each player is sent `772 1` on spawn.
  * `+set scr_coop_cast_nametags 0` turns it off.
* **NOT verified on a client:**
  * `+0x64` is also C1's character-clone selector (`0x43e809` → `0x4437c0` composes that client's character on the mover). An unused slot has no character, so nothing should be composed, but that is INFERRED.
  * With `sv_maxclients 18` only slot 18 lies above the client range. The other names use client slots 17 downward while nobody occupies them.

### 10.4 The range door

* `rifle_range_door` (`*104`, 1384 −1182) is a brush in the range hangar's south wall. It spans x 1388..1428, measured by trace, right across the doorway the trainer's path nodes use (x 1407).
* Pass 9 never moved it; only `arsenal_door` was raised. VPS log of the user's session: the `inside_firing_range` trigger (in the doorway) fired, Tiny's lines ran, and every range hit wait timed out.
* Fixes:
  * The door swings in about its west edge and is made non-solid. `obj_door_frame` is hidden and made non-solid.
  * The range has screens. Seen from the stall, every dummy was behind `range_metalwall` (1888 −455) or `plywood` (1888 −367), so real bullets could never reach a dummy; the old probe only passed by notifying damage. The metal wall comes down, and the plywood stands only for the penetration part.
  * The station objective is firing station one (`obj_station_one_glowing`, 1939 −851). `obj_rifle_stall` has the knife dummy in front of it.
* Blockers removed or opened when their station opens:
  * `so_gate` (Spec Ops gates across the demolition exit and the pit entrance)
  * `door_fake_collision` (with `gate_cqb_enter`)
  * `clip_obstacle_start` (at Mac's GO)
  * `course_end_blocker` (after the pit course)
  * `ship_melee_clip` (with the knife position)

### 10.5 Probe (port 17777, test client)

`playroute` in `_cooptest.gsc` drives player 0 like a player:
* It walks (`botMoveTo`) into triggers and through doors.
* It aims (`botLookAt`) and fires real bullets (button 1) at the dummies.
* It knifes (button 4).
* It throws real frags (button 0x4000).

MEASURED results:
* The bot walks in through the range door. All three range hit tasks and the timed task completed with real bullets, through the plywood for the penetration part. The knife hit the melee dummy.
* The mission completes.
* `opengates` + `walk` crossed every route gate: the range door (both ways), `gate_cqb_enter`, `gate_cqb_exit`, `gate_cqb_start`, `gate_cqb_finish`, `gate_cqb_out`, and the hangar yard slide gate.
* `gate_cqb_exit2` / `gate_cqb_enter_main` lie under the terrain (z −137..−154; a test client teleported there falls out of the world), so they are not on the route.
* Long straight-line walks between stations stop on buildings and the obstacle course's mantles. Those are walker limits, not closed doors.

## 11. The pit's way in (2026-10-07, fifth pass)

User report on 53f77b7a: "one of the gates doesn't open for the cqb course, unable to get past that".

* **VPS log of that session (codol-match-17733).**
  * The obstacle course finished, `gate_cqb_enter_main` opened, the hangar yard gate slid and Harker's briefing ran.
  * `gate_cqb_start` opened, then `waiting for course`. `player_has_started_course` was never set and no run started.
* **Cause (MEASURED with `_cooptest colmap`, a top-down map of what stops a player).**
  * The pit's start room (x −370..−125, y −836..−980) and the corridor east of it are walled off from the hangar along y −989, from x −376 to 300.
  * The one opening in that wall is `gate_cqb_out` (brush x 84..196, y −990).
  * `gate_cqb_start` (x −121, y −836..−922) joins the corridor to the start room.
  * Pass 9 opened `gate_cqb_out` only after the course was passed. A player could open nothing on the way to the start trigger.
* **Fix.**
  * `gate_cqb_out` opens together with `gate_cqb_start` when the briefing ends.
  * After every run, passed or not, the finish (`gate_cqb_finish` + `course_end_blocker`, which close the lane x 476..572 at y −840) opens. A slow player walks out into the same corridor and back to the start for another try.
* **The other gates, by measured hull (bullet traces against each brush) rather than origin.**
  * `gate_cqb_exit2` (z −151..−79) and `gate_cqb_enter_main` (z −134..−70) lie below the obstacle-course ground (z ≈ 0, floor found everywhere at z 0). With all gates open, that area maps clear of any mover.
  * They open at their steps anyway, and the user passed them.
  * The other CQB-area gates (`gate_cqb_enter`, `gate_cqb_exit`, `so_gate`, `door_fake_collision`, `clip_obstacle_start`, `course_end_blocker`) open or are removed at their steps (section 10).
* **Probe (`playroute`, walking).**
  * Hangar (89 −1315) → through `gate_cqb_out` → corridor → through `gate_cqb_start` → start trigger: walked without a stop. The run started and was cleared (24/24).
  * The finish opened and the walker left through `gate_cqb_finish`. The mission completed with no script error.
  * Inside the course and the obstacle course the straight-line walker stops on walls, stairs and mantles ("world or clip"). One stop was a pop-up civilian target (a `target_friendly` brush in the lane), not a gate.

## 12. Sixth pass: the pit melee clip, the CoD4 flow, Price at the wheel, two primaries (2026-10-08)

### 12.1 The pit's melee target (user report on 0db5650d)

User report: "script brush model for the melee target in the cqb course doesn't get removed after knifing the target".

* **Which brush.** `melee_clip` (`*310`, 166 1264 209) is a player clip across the upstairs walkway. It sits beside the course's melee-only target (`target_enemy`, `script_parameters melee`, linkName 14, sideways pivot 168 1214 192).
* **How IW4 handles it.** IW4 `trainer.gsc` `target_think` shows the clip when that target pops up, takes the target down only with `MOD_MELEE`, and hides the clip with the hit. `so_killspree_trainer` hides the clip at load.
* **The cause.** `_coopcourse` never touched the clip, so it stood across the walkway for the whole mission. The VPS log of the session: the course started, `player_near_melee_target`, `player_course_upstairs`, and then nothing more.
* **The fix (`_coopcourse.gsc`).** It now does what IW4 does: the clip is hidden at load, shown while the melee target is up and hidden on the knife hit. Every pop-up target is also made non-solid whenever it is down, including before its first pop-up.
* **A real knife, MEASURED with `meleeDebug`.** The test client faced the panel and pressed melee (C1 button 4). The engine's melee trace hit the target brush (`hitId` = the target), applied damage, and the target logged `MOD_MELEE`; `melee_clip` then went false.
  * The panel stands at x 164..172, y 1224..1280 (measured with `colmap`). Its pivot is in the wall corner, so a knife aimed at the pivot strikes the wall.

### 12.2 CoD4 killhouse as the reference (all but the pit)

**C1's voice lines.** C1's `pit_*` lines are CoD4 killhouse's dialogue, re-recorded, in CoD4's order:
* Gaz's range lines are Tiny Man's (`pit_tiny_*`).
* Newcastle's frag and launcher lines are `pit_ncastle_*`. C1 cut the C4 part: 010 goes straight to `C4_complete_dialog`.
* Mac's lines are `pit_mac_*`.
* Price's ladder, cargoship and debrief lines are `pit_pri_012..065`. The `_r` files are his radio, so they play locally to every player.

C1 ships none of CoD4's `killhouse_*` dialogue aliases.

**Stations by CoD4 function:**
* **Range** (`navigationTraining`, `rifleTraining`, `rifle_hip_shooting`, `rifle_penetration_shooting`, `rifle_timed_shooting`, `sideArm_Training`, `melee_training`):
  * `rifle_range_obj`, then `at_rifle_range`.
  * `open_firing_range_door`: rotate +88 about its origin, `door_metal_slow_open`, `connectpaths`.
  * `inside_firing_range`; Gaz to `stationone_node` on `past_gaz`.
  * `obj_rifle_stall` / `rifleTraining_stall`.
  * Hip fire: hits while aiming down the sights do not count and draw `004_1` / `004_2`.
  * Down the sights.
  * Plywood raised and lowered with CoD4's `rotateRoll ±90` (silently lowered at load).
  * Timed rounds: 10 s, more than 6 hits, CoD4's too-slow lines, then on.
  * The sidearm from `obj_rifle_ammo`; switch to the rifle, then to the sidearm (waits on the current weapon).
  * The "watermelon" is `melee_target_dummy`. It stays hidden until the knife lesson, as `melon_think` hides the melon; at the stall it had taken every shot meant for the lanes. Then `near_melee` and a knife.
* **Demolitions** (`frag_Training`, `launcherTraining`, `C4_complete_dialog`):
  * `near_grenade_area`; frags.
  * `safety_pit` (with the `003` nag); Newcastle to `watch_pit_node`.
  * Windows 2–4: the `grenade_damage_trigger2..4` trigger_damage, with `enablegrenadetouchdamage` and the buzzer.
  * Newcastle to `watch_table_node`; the launcher (`m79_so`, the map's `launcher_ammoitem`).
  * Back to the pit.
  * Wall 1: `launcher_wall_trigger`.
  * Windows 5–7: `launcher_damage_trigger5..7`.
  * `011..014_2`.
* **Obstacle course** (`obstacle_Training`, `obstacleTraining_dialog`):
  * `obstacleTraining_objective`, then `obstacle_course_start`, then `start_obstacle`.
  * `misssoap`, then `lineup` until `start_course`, then GO.
  * The charity walk, then the jump on `obstacleTraining_mantle2`, the crawl on `crawl_dialog`, and the commandos nag.
  * `obstacle_course_end`, then `passedtest`.
* **The pit:** MW2, unchanged (sections 9–11).
* **Ship** (`report_to_price`, `reveal_anims`, `cargoship_training`, `wait_till_pos_cleared`, `wait_till_flashed`, `rope`):
  * Price and the three SAS stand at `price/soap/ghost/harker_reveal_node` by the ship hangar (their spawners target these nodes).
  * `ship_hanger_door` `moveX 40`, then `moveX 110` over 8 s with `door_hanger_metal_open`; `ship_hanger_clip` deleted.
  * The ladder (`012`), then `at_top_of_ladder`, then MP5 and four flashbangs (`013`), the plan (`014..016`) and the signal (`017`).
  * The rope: `top_of_rope_trigger` use, then a 1.5 s slide to `bottom_of_rope`.
  * GO (`018`), with a 60 s timer.
  * Positions:
    * one (`position_one` trigger → `position_one_targets`); two.
    * three (flash into `position_three_vol`).
    * four.
    * CoDOL's `position_melee` (`ship_melee_clip` removed).
    * five.
    * six (flash into `position_6_vol`, then its targets).
  * Final (`039`), `sprint`, `at_finish`.
  * The verdict (`042` within 60 s, else `050`), then `043`, `044`, then `debrief_trigger`, then `061..065_2`.

**Character places and poses** (`killhouse_anim.gsc` → C1's common animlists; section 10.1 explains why only those are used):

| CoD4 | here |
|---|---|
| Gaz `killhouse_gaz_idleA/B` | `casual_stand_v2_idle` / `patrol_bored_idle` |
| Gaz `killhouse_gaz_talk_side` / `point_side` | `casual_stand_v2_twitch_talk` on every line |
| Price / SAS `killhouse_sas_*_idle` | `casual_stand_v2_idle` / `exposed_idle_alert_v1` |
| chair guy `killhouse_laptop_*` at `chair_guy_origin` | `casual_crouch_idle` at 1548 −688 |

Gaz, Newcastle, Price and the SAS are placed on their CoD4 nodes when CoD4 moves them. The coop actors do not path, so they are placed rather than walked.

### 12.3 Price at the wheel

* **The fault.** The pass-10 driver model was linked at a guessed offset: 21 units behind and 16 below the humvee's real driver seat, inside the body shell.
* **The seat tags (MEASURED, `_cooptest jeeptags`).** `vehicle_hummer_viewmodel` has `tag_driver` at (15, −29, 36), `tag_passenger` at (15, 30, 36), `tag_guy0` / `tag_guy1` at the rear, and `tag_player` in the middle.
* **Price.** His body and head are now linked to `tag_driver`, playing `bh_pilot_idle` (`common_actor_shared`).
* **The riders.** Players ride crouched on `tag_passenger`, `tag_guy1`, `tag_guy0` and `tag_player`, and stand again at the stop.
* NOT verified on a C1 client: the seated pose against the wheel.

### 12.4 Two primary weapons

* **The rule.** `_coopmode::coop_give_weapon` is the one give path. The training stations and the map's weapon pickups use it; the loadout already starts with two.
* A new primary while two are held replaces the held primary of the same `weaponClass`, else the weapon in hand (CoD4's SP swap). The replaced weapon is taken, not dropped, as CoD4 killhouse's scripted gives drop nothing.
* **Probe.** The inventory stays at 2 through the range (m4 replaces the mp5, `usp_so` replaces `usp_silencer_so`) and the demolitions (`m79_so` replaces the pistol).
* **The native world-weapon pickup in coop already swaps.** `pickuptest`: walking over a world `weapon_m4_grunt_reflex_so` and a world `weapon_m79_so` left 2 primaries each time.

### 12.5 The minimap (investigated, not changed)

* C1 draws the compass/minimap in a HUD menu named `Compass`. Its fade-out is driven by numbered dvar `815` (IW4 `hud_fade_compass`): C1 `0x43aec0` / `0x43af24`, registered at `0x453710`.
* C1 also reads `g_hardcore` from the serverinfo string into the client dvar `g_hardcore` (`0x465bf1`, `0x465f0c`). Nothing in the C++ cgame reads that dvar back, so its effect lives in the UI.
* The HUD's menus and Scaleform movies are not in the cached zones, so which of these hides only the map, and what else hardcore hides, needs a C1 client check.
* The coop scripts never call `setMiniMap` (CS 823 → wire 1303 stays empty), yet the training zone carries `compass_map_killhouse`.

### 12.6 Probe (port 17777, `playroute` walking, one fresh load)

* **Ride.** Price is linked on `tag_driver`.
* **Range.** The door opens (+88; the doorway is clear both ways by `walk`), Gaz moves, and all hits are real bullets (hip, ADS, through the plywood, timed rounds). The knife is real (`damage 139 to 699`).
* **Demolitions.** The windows, wall and launcher windows are thrown and fired at for real. The test client's aim missed them, so they were triggered SIMULATED; the triggers, buzzer and flow ran.
* **Obstacle course.** Ran in order.
* **Pit.** The entrance was walked, the course cleared, and the melee clip goes with the knife.
* **Ship.** The rope slide ran (the use button was SIMULATED); the positions, both flash rooms (SIMULATED throws), sprint, finish and verdict ran.
* **End.** The debrief led to MISSION COMPLETE with no script error.
* **Harness limit.** The test client's straight-line walker cannot jump mantles, climb ladders or follow the pit's turns. It logs those as STUCK (world or clip) and teleports on.

---

## 13. Mission end: back to the lobby, and the next mission unlocked (2026-10-08)

User request: "after the campaign mission is finished send the player back to the lobby and
have them unlock the next mission".

### 13.1 How retail CoDOL keeps campaign progress (MEASURED in the shipped tables)

* **Completion is a challenge.** There is no mission field. Each story mission has a
  `storymission` challenge in the secure profile's `challenges[1000]`
  (`mp/allChallengesTable.csv` column 4 is the index): 800 `so_coop_training_mp_killhouse_sh`,
  801 `so_coop_take_mp_fav_tropical`, 802 `so_coop_defense_mp_crash_tropical`, 803
  `so_coop_take_mp_estate_tropical`, 804..810 the chapter 2-3 missions (deactivated in this
  build, no zones). Spec Ops missions have one challenge per difficulty,
  `so_<mission>_2` / `_3` (811..833).
* **The unlock is a flag gated on that challenge** (`mp/unlockFlags.csv`, name / id / display
  level / challenge / tier): 652 `uf_ch_coop_take_mp_fav_tropical` and 662
  `uf_ch_coop_spec1_mp_killhouse_sh` on training, 653 `uf_ch_coop_defense_mp_crash_tropical`
  on take fav, 654 `uf_ch_coop_take_mp_estate_tropical` on defense crash, 631/632/645 the
  Spec Ops maps on their story missions, and a `cf_ch_coop_<mission>` "completed" flag each
  (633, 635..644). 637 also opens the Spec Ops tab (`mp/uiNotifications.csv`).
* **The menu tests the flag.** `mp/comaplist.csv` names each coop map's unlock flag; C1's
  `MatchService.listMap` -> `AS_IsMapUnlocked` 0x005AC260 -> `IsUnlockFlagSet` 0x0048D220.
  Training and the prologue name no flag (always open).
* **Order** (comaplist chapter/mission/order columns): training (chapter 0) -> take fav ->
  defense crash -> take estate (chapter 1) -> evasion, sniperfi, escape, ac (chapter 2) ->
  suspension, chopper, defense suburbs (chapter 3). As shipped, evasion's flag (655) is gated
  on `so_coop_chopper_mp_suburbs`, a loop; chapters 2-3 ship no zone, so it is left as shipped.

### 13.2 Server

* `_coopmission.gsc` `end_level`: after the outcome HUD and `scr_coop_end_delay`, every
  player still in the mission gets `codol_reportCoopMission(<sv_sessionMap>, success)`,
  then `exitLevel(false)`.
* `codol_reportCoopMission` (`src/online/script/coop_progress.c`, coop category only)
  records the result on the player's match-report row; the report carries it as
  `progression.coop_mission_completed` / `coop_mission_failed`. On a completion it also sends
  `s cg_unlockFlagSet "<ids>"` with the flags `mp/unlockFlags.csv` gates on the mission's
  challenge, because C1 copies the lobby's task-30 flags only once per process.
* `exitLevel` in a coop match with `exitToLobby 1`: the report is written
  (reason `coopmission`) before the usual `t "EXE_MATCHENDED"`, and `ExitLevel` then loads
  nothing (no `SV_CategoryRotate` replay). `sh1x_status` reports `map=none running=0
  parked=1` until the next `map`, so the lobby's placement re-points the server even for the
  same mission. `exitToLobby 0` keeps the coop rotation.

### 13.3 Lobby (alpha0022)

* `career/progression.py` `credit(..., coop_completed, coop_failed)`: a completion completes
  the mission's challenge (once; a replay re-pays nothing) and records
  `profile["coop_missions"][<mission>] = {"completed": n, "failed": n}`; the unlock flags
  follow from `granted_flag_ids()` and reach C1 in task 30. A failure is recorded only.
* `career/rank_writeback.py` folds the two report keys; `store/player_profiles.py` passes
  them on.
* `handoff/match_instances.py`: a server counts as "already on" the selection only when the
  session map matches too and it is not parked. Before this, picking
  `coop_spec1_mp_killhouse_sh` on a server that had run `coop_training_mp_killhouse_sh`
  (both `mp_killhouse_sh`) was answered "already on" and reloaded nothing.
* Selection: C1 names the chosen mission in the coop session descriptor (`desc[24]` =
  crc32 of the playlist name), and `map_handoff.select()` already loads exactly that mission.

### 13.4 Verified

* Probe on 17783 (scratchpad home, developer 1, `scr_coop_test 1`, a test client):
  `complete` on training -> `CoDOL coop: client 0 ... a bot - unlock flags 633|652|662`
  (read from the mounted table, equal to the lobby's derivation), `exitLevel`, the report
  step, `the server is parked`, `sh1x_status ... running=0 parked=1`, no `map_rotate` in the
  following 20 s. A lobby-style dispatch of `coop_spec1_mp_killhouse_sh` loaded it
  (`parked=0`); `fail` on it reported `failed` and parked again; an mp dispatch after that
  loaded as `mp`. A test client owns no account row, so its report is "nothing owed".
* `tests/test_match_report.c` (built by hand, it has no makefile target) checks the
  coop row JSON; `tests/test_match_status.c` the parked line; `make test` passes.
* Lobby: `selftests/coop_progress_selftest.py` (spooled report -> GETFILE -> task 30) and
  `match_instances_selftest.py` IN-13..15.
* Not yet seen in C1: the unlock animation and the map list after the return.

---

## 14. Seventh pass: the training mission as the retail recording plays it (2026-10-08)

**The guide.** The user pointed at a public gameplay recording of the retail CoDOL client playing the training mission ("Recruit Camp"). It is now the authority for:

* the order of stations and lines;
* the cast's looks and poses;
* what moves and what glows.

How the recording was used:

* The frames were viewed in the scratchpad only. Nothing from the recording is in the repo.
* Lines were placed from the subtitles C1 draws on screen, matched by meaning and timing to C1's `pit_*` aliases. They are paraphrased here, never quoted.
* Times are recording time. Gameplay starts at 1:20; everything before it is the menu and the loading cinematic.

### 14.1 Timeline (what the recording shows)

| Time | Where / what | Lines (alias) | HUD |
|---|---|---|---|
| 1:21 | Title card: the camp's name, a date, a classified location | | |
| 1:23-1:34 | **Jeep ride.** The rookie rides front right. Price drives: boonie hat, sunglasses, dark jacket, flag patch, smoke grenades on the vest, hands on the wheel, a map on the dash. A foggy, overcast forest road with tall trees leads into a dusty yard of parked trucks and humvees, with soldiers in T-shirts. | Price welcomes him to the unit and tells him he is back in basic (`pit_pri_001`, `002_1`, `002_2`) | |
| 1:39 | The jeep has stopped. The rookie is out beside it, **no weapon in hand**, with only the frag and smoke icons. | | look-up / look-down hints, then the move-keys hint |
| 1:45-1:52 | On foot by a humvee, Price sends him to Tiny Man. Around them: a soldier beside Price, one on a humvee roof, a mechanic welding under a humvee (sparks), a parked UAZ. **Glowing yellow chevrons on the ground** lead to the range door, a white door with a mail slot and a **glowing yellow frame**. | `pit_pri_003` | objective: use the marker to find the range |
| 1:55-1:59 | Inside the range by the vending machines, still empty-handed. The rifle (M4, foregrip, holographic sight) is **taken off the table**. | `pit_tiny_002` (after `001`) | |
| 1:57 | Booths numbered 1-6. Booth one's number and marker glow; floor paint and arrows. | | |
| 2:01-2:19 | **Hip fire**: the dummies lie flat and pop up. Then **ADS**: a bullseye pops up, and Tiny Man says "good". | `pit_tiny_003`, `006`, `007` | shoot each target from the hip; aim down the sights |
| 2:23-2:53 | **Timed targets**, popping up **one at a time**. There is no plywood lesson. | `pit_tiny_011_2` | shoot each as fast as you can; reload hint |
| 2:57-3:09 | **Sidearm**: no pickup; the pistol was carried all along. First "pull out your sidearm", then "good, now the rifle", with three dummies up. | `pit_tiny_021`, then `020`, `022` | switch to your pistol |
| 3:13-3:17 | **Knife**: the cardboard soldier in front of the booth | `pit_tiny_025` | knife the target |
| 3:21-3:25 | Outside, overcast and foggy, with chevrons on the ground. The way runs through an open chain-link gate to the demolition house (windows numbered 2-7). | | go outside, report to Newcastle |
| 3:29-3:54 | Newcastle stands on the platform above the safety pit, beside a camera on a tripod: tan uniform, vest, cap, his name over him. The rookie has four frags. **Windows two, three and four carry markers, and each frame glows.** | `pit_ncastle_001` | enter the safety pit; throw a grenade into windows 2-4 |
| 3:59-4:24 | The launcher is an **M4 with an underbarrel grenade launcher and a red dot**; the rookie switches to it. **Windows five to seven glow.** | `pit_ncastle_007`, `008`, `010` | equip the launcher; fire at windows 5-7 |
| 4:29-4:34 | The rifle is back in hand (no launcher). He is sent to the course; **floor arrows run along an alley** by a brick wall; a watchtower stands beyond. | `pit_ncastle_013`, `014_2` | complete the obstacle course |
| 4:44-4:46 | Mac (boonie, sunglasses, backpack) **crouches** at the start line with recruits in T-shirts, one named Pvt. Ritchie. A truck is parked nearby. | `pit_mac_001`, `002` | |
| 4:49-5:17 | Countdown and GO. A **big glowing chevron sits on the first hurdle**, with more chevrons on the ground. Then the jump, the barbed-wire crawl, the sprint and the finish. The recruits stay at the line. | `pit_mac_003`..`005`, `008_1` | sprint hints |
| 5:23-5:41 | **Price stands by the barbed-wire fence at the end of the course**, arms crossed, under a watchtower and a bunker with a man on its roof. Helicopters pass overhead, and the rookie's weapons are down. The debrief: walk before you run, a simple intelligence job, a source called Ortiz, go and get it. | `pit_pri_062` .. `065_2` (`061` is not heard) | report to Captain Price; then **mission complete** |
| 5:53 | Back in the lobby's campaign map | | |

**The recording has no pit, no Harker briefing and no ship.** The mission ends with Price after the obstacle course.

**Weather and ambient.**

* The zone's own fx script (`maps/mp/coop_training_mp_killhouse_sh_fx.gsc`) loads:
  * river fog;
  * insect and moth trails;
  * carcass flies;
  * hangar dust and light-shaft dust;
  * the welders' sparks;
  * a green signal smoke;
  * a smoke screen.
* Its createfx places 107 fog oneshots, 40 fly swarms, 28 light-shaft dusts, 4 hangar dusts and a few insect/moth runners.
* All of these are client-side. C1 draws a map's createfx itself, so the recording's overcast fog needs nothing from the server.
* The welding sparks are the one effect the level script plays (14.2).
* No exploders are authored.

### 14.2 What changed (`_cooptraining.gsc`)

* **Order.**
  * Range, demolitions, obstacle course, then **Price at the course's end** (`station_price`: his authored `price2` spot, `come_to_price`).
  * Then the debrief `062`..`065_2`, weapons down, and `mission_complete`, which hands over to 13's report and lobby return.
* **The pit is gone from this mission** (user, 2026-10-08: it belongs to another training).
  * `station_pit` (the hangar briefing and the CQB run), 66da742e's corridor `station_debrief` and `station_ship` (the cargoship mock-up) are deleted, with their helpers (rope, flash rooms, radio calls). `open_everything` keeps only the route's gates.
  * `_coopcourse.gsc` still finds the map's pop-up targets, but it no longer arms the pit's course triggers or the cargoship/deck groups when the training flow is running.
  * The pit's gates stay as authored, closed, inside the hangar and off the route. Its `melee_clip` is hidden and not solid from load (`melee_clip 0`). The course end to Price measured clear (`colmap -1440 -2880 -1080 -2440`).
  * The hangar cast (Harker, Soap, Ghost, the second Price) still stand in the hangar as scenery.
* **Ride.** `pit_pri_003` moved from the jeep to Price on foot after the ride.
* **Weapons.**
  * Each player arrives with the pistol (`usp_so`) and his offhands only. Weapons are lowered from the end of the ride until the rifle is in hand.
  * The rifle is the addon's `range_m4tech` (`m4techgrip_eotech_so`) on the range table. It is taken with the use key within reach (96 units) or by the native touch pickup.
  * After the pickup the item is removed and every player gets the rifle. A player who spawns or respawns after the range gets the rifle too.
* **Range.**
  * The lane and bullseye dummies start flat (`rotateRoll -90` about their floor pivots) and pop up for each lesson (`+90`, `killhouse_target_up_metal`).
  * The plywood lesson is gone.
  * The timed lesson is rounds of eight single pop-ups, three seconds each; six hits pass a round.
  * The sidearm lesson is pistol first, then rifle (`021`, `020`, `022`), with no pickup.
  * The sheet-metal screen across lane one (`range_metalwall`, used by no lesson) is removed. It took every shot at the far dummy (MEASURED, `aimcheck` + `colmap`).
* **Glow.**
  * These base-map brushes all start hidden:
    * the `navigation_arrows` brushes: `arrows_to_hanger`, `_station_one`, `_newcastle`, `_obstacle`, `_end`, `_price` (`_debrief` and `_heli` stay dark);
    * `obj_station_one_glowing` / `_number`;
    * `glowing_window_two`..`_seven` and `glowing_wall_one`.
  * The arrows light while they are the way:
    * to the range door after the ride;
    * inside the range, to booth one;
    * to Newcastle after the knife;
    * to the course after demolitions;
    * along the course during the run;
    * to Price after the course.
  * The booth marker lights from the rifle pickup until the booth is reached.
  * Each window lights, with its own objective marker (objectives 1-3, beside the pit's 0), until it is hit.
  * The range door's glowing frame was already shown until the door opens.
* **Demolitions.**
  * The launcher is `m4techm203_reflex_so` (fallback `m79_so`).
  * Before wall one the player is asked to switch to its launcher half (`weaponAltWeaponName`, 30 s time-out).
  * Afterwards the launcher is taken and the range rifle comes back.
* **Cast.**
  * Mac wears the woodland SAS body with Mac's head (boonie and sunglasses) and crouches.
  * Newcastle wears `body_complete_tf_tan` / `head_complete_tf_tan`.
  * The three recruits (`buddy1..3`, never placed before) crouch on their `crouch1..3` nodes in T-shirts. `buddy2` carries the Pvt. Ritchie nametag.
  * Ambient soldiers are now capped at 15, to keep the 32-slot actor pool.
  * Props go on `tag_inhand` (all are in the zone):
    * the welder handle for the three welders, who also throw `misc/welding_runner` sparks every few seconds;
    * a PDA for the two texters;
    * the open book for the reader;
    * the pencil for the map writer;
    * a basketball for the players.
  * Price turns to face whoever reached him.
* **Objective text.** Each lesson announces its task, paraphrasing the recording's objectives.
* **Nametags and loadout on a fast spawn.** A test client's spawn notified `spawned_player` before a thread started on `connected` was waiting for it (MEASURED). The nametag dvar and the loadout now also apply to a player already in the world.

### 14.3 The minimap (`_coopmode.gsc` `coop_minimap`, every coop mission)

* **The gap.** The multiplayer level scripts set their compass map with `maps\mp\_compass::setupMiniMap`, a multiplayer module, so no coop mission ever called `setMiniMap`. Configstring 823 (C1 1303) stayed empty.
* **Material.** Each mission's own zone ships one, where it has one (MEASURED, `docs/zone_assets`):

  | Mission | Material |
  |---|---|
  | `coop_training_mp_killhouse_sh` | `compass_map_killhouse` |
  | `coop_spec1_mp_killhouse_sh` | `compass_map_cqbhanger` |
  | `coop_defense_mp_crash_tropical` | `compass_map_mp_crash_sp` |
  | `coop_take_mp_estate_tropical` | `compass_map_mp_estate_sp` |
  | `coop_take_mp_fav_tropical` | `compass_map_mp_fav_tropical_sp` |
  | `coop_spec_mp_fav_tropical` | `compass_map_mp_fav_tropical_spec` |

  The others ship none and use the base map's, as its multiplayer level script names it: `compass_map_mp_crash_dlc`, `compass_map_mp_crash_intro`, `compass_map_mp_estate_tropical`.
* **Bounds.** The `minimap_corner` script_origins, with `setupMiniMap`'s corner maths and `getNorthYaw`.
  * The killhouse addons overlay their own pair (`script_specialops 1`; spec1's also names its mission) on the base map's pair.
  * The mission pair wins, and the base pair is the fallback. `script_specialops` reads as defined on all four corners, so its value decides.
  * Training's pair spans −2368 1408 .. 4864 −5824, the whole camp. The base pair covers only the multiplayer killhouse.
  * The other missions author no pair and use their base map's.
* **Why this material.** The recording's corner map shows the camp, not the multiplayer killhouse.
* **Probe.** `CoDOL coop: minimap compass_map_killhouse (-2368, 1408, 0) .. (4864, -5824, 0) (north yaw 90, mission corners)`. The native configstring path (822/823 to C1 1302/1303, `RefreshServerMapConfigstrings`) is the one multiplayer already uses.
* **Square bounds (2026-10-14).** The user saw a squashed compass map on the estate missions.
  * **Cause.** `maps\mp\_load::main()` sets `level.requiredMapAspectRatio = getDvarFloat( "scr_RequiredMapAspectratio", 1 )` before every multiplayer level script's `setupMiniMap`. That widens the shorter side of the bounds so they form a square in the north-yaw frame. Coop never runs that loader, so its bounds went out unexpanded.
  * **The client side.** It derives the drawn extent the same way: T5 `CG_MiniMapChanged` gives east = `toLR · (north[1], −north[0])` and south = `toLR · −north`. C1's setMiniMap (0x547440) computes the same two values and also reads north yaw from the gamestate.
  * **Before and after.** All compass images are square (`main/_decrypted/*/images`, 512 or 1024).

    | Base map | North yaw | Corners | Compass frame before | After |
    |---|---|---|---|---|
    | `mp_estate_tropical` | 144 | (−4640, −1280) (1760, 5120) | 8940 × 1416 (6.3 : 1) | 8940 × 8940 |
    | `mp_crash_tropical` / `_intro` | 0 | (−1200, −2166) (1976, 2528) | 4694 × 3176 | 4694 × 4694 |
    | `mp_fav_tropical` | 0 | (2232, −1848) (−2312, 3656) | 5504 × 4544 | 5504 × 5504 |
    | killhouse spec1 (mission pair) | 90 | (4634, −4225) (2938, −2065) | 1696 × 2160 | 2160 × 2160 |
    | killhouse training (mission pair) | 90 | (−2368, 1408) (4864, −5824) | 7232 × 7232 | unchanged |

  * **Evidence.** The base map's AI path nodes (walkable ground) were plotted onto each mission's image under each frame.
    * Expanded: 98–100 % of the nodes land in the drawn area on estate, crash and favela, and buildings and the estate pond stay empty.
    * Unexpanded: estate 18 %, crash 65 %.
    * A free fit (any yaw, centre and side) of the estate image also lands on yaw 144 and the corners' centre.
  * **Crash intro.** `compass_map_mp_crash_intro` ships in no zone, so the prolog now uses `compass_map_mp_crash_dlc`. That material is in `patch_common_mp`, which is always mounted, and Crash's geometry and corners are the same.
* **Not yet checked:**
  * how the drawn map lines up on a C1 client (the node overlay is offline evidence only).

### 14.4 Not built, and why

* **Price's crossed arms.** That idle (`killhouse_price_stand_talk`), like every other trainer idle, is only in the map animlist, which C1 does not load (10.1). He keeps `casual_stand_v2_idle`.
* **The key hints.** No coop script drives C1's hint HUD element yet. That covers the look/move hints after the ride and the per-lesson hints (fire, aim, reload, G for grenades, 3 for the launcher, sprint). The objective text stands in.
* **The ambient helicopters** over the camp. No helicopter path is authored for them in the addon, and the coop vehicle layer has no flying route for this map.
* **The dash map, the camera on its tripod and the M18 smoke grenades.** These belong to the jeep model, a static map model and Price's body respectively. There is nothing to place.
* **The countdown before GO.** No alias carries it (`pit_mac_003` is the GO), so the recording's countdown subtitle has no voice line to match.
* **Which arrow brush covers which stretch.** This is inferred from names and origins. The brushes are not solid, so the `brushes` trace probe cannot measure them. A C1 client check should confirm that each lit set is on the stretch being walked.
* **The m203 alt on a C1 client.** The test client's selection of the launcher half logs "no published C1 index". A real client selects by C1's own index, and `m203_m4tech_reflex_so` is in the coop window (CS 5399), but this is not verified on a client.

### 14.5 Probe (port 17777, one test client, `playroute`, fresh load each time; the last two runs on 7d219d37 with this patch)

* **Ride.** Lines `001`..`002_3`, `weapons down 1`, Price out, `arrows_to_hanger` lit, `pit_pri_003`.
* **Range.**
  * The door opens.
  * The rifle pickup: `took (use) the range rifle` (the touch pickup alone did not reach it from the floor), then `carries 2 primaries: usp_so m4techgrip_eotech_so` and `weapons down 0`.
  * The booth glow went on and off.
  * Hip hits were real (3/3), the bullseye was hit for real, and the pop-up round passed 6/8 with real hits.
  * The pistol/rifle switches timed out, because the test client's `switchToWeapon` does not take. The knife was simulated, as before.
* **Demolitions.**
  * Every window and wall lit and went dark on its hit (SIMULATED triggers, as before).
  * The launcher was given, then swapped back for the rifle.
  * In both runs the test client died to its own launcher round at wall one and respawned with the rifle, as designed. Its aim from the pit, 8 degrees above the trigger, may put the round into the pit's own edge. A real player's aim was not tested.
* **Obstacle course.** The arrows lit along the run, and the recruits crouched at the line. The walker's mantle STUCKs teleported on, as before.
* **End.** `arrows_to_price`, `come_to_price`, the debrief `062`..`065_2`, then MISSION COMPLETE with no script error.
* 13's end path followed: `unlock flags 633|652|662`, the `returning them to the lobby` line, `exitLevel`, and the server parked.
* The last run (after the pit was removed) also logged the minimap line, `melee_clip 0` and one proximity group (the demolition dummies) with no course triggers armed.
* **NOT seen on a C1 client:**
  * the lit brushes;
  * the props;
  * the pop-up direction (the roll sign was chosen to lay each dummy away from the shooter);
  * the crouched poses.

---

## 15. Eighth pass: the jeep seat, Price at the wheel, wall one, name tags, the mission's look (2026-10-08)

### 15.1 Price at the wheel (user report, third time: "headless, T-posing, not driving")

* **Why.** The driver was a `script_model` with the head `attach()`ed and `scriptModelPlayAnim( "bh_pilot_idle" )`.
  * C1 composes a head for an actor (the character path) but not for a script model's attachment, so it showed headless.
  * `bh_pilot_idle` is in no animtree the server declares to the client for script models (`model_animation.h`), so the model stayed in its bind pose.
* **Now.** Price is an **actor** of the coop soldier species (the path that composes and animates the standing cast), with Price's body and head.
  * He runs in `animmode( "nophysics" )` and plays `bh_pilot_idle` through the actor tree.
  * The xanim is in the training zone, and `common_actor_shared.animlist` lists it, so both sides have it.
  * `technical_driver_idle` is listed too, but its xanim is in no mounted zone (`using the null animation 'void'`).
* **Placement.** The retained `linkTo` refuses actors ("does not currently support linkTo"), and a script-side follow lags the mover by a frame. So there is a new native, coop-only `<actor> codol_actorRide( <ent>, <tag> )` / `codol_actorRide()` (`actor_lifecycle.cpp`).
  * It takes the tag's place and yaw in the parent's frame once.
  * Every frame it places the actor there from the parent's own trajectories at `level.time`, with no AI or movement, and returns.
  * Once a second it prints `CoDOL actor ride: ent N on ent M at (...) yaw Y anim <leaf>`.
* At the camp `spawn_driver` takes the same actor off (`codol_actorRide()`, `animmode none`) and stands him at the driver's door.
* The jeep waits up to 10 s for him (`level.coop_intro_wait_driver`).
* **VPS note.** The `bh_pilot_idle ... not in any mounted zone` warning in the 17733 log is from a `coop_take_mp_fav_tropical` load, whose zone does not carry it. On the training load the clip is found and plays (probe: `anim bh_pilot_idle w 1.00 g 1.00 t 0.678`).

### 15.2 The rider's seat

* **Placement.** Seat tags as before (`tag_passenger`, then `tag_guy1`, `tag_guy0`, `tag_player`). The feet now go 12 below the tag, so the crouched eye (40) sits about 28 above the cushion.
* **Locks.** New native, coop-only `allowStand` / `allowCrouch` / `allowProne( <bool> )` (`script_seat_controls.c`).
  * They set C1's own stance-permission bits, `pm_flags` 0x800000 / 0x1000000 / 0x2000000.
  * C1 reads these in its stance code, on foot at 0x412882 and linked at 0x412e42. `pmove_c1.c` mirrors both.
  * `pm_flags` reach C1 unchanged, so its prediction refuses the same change.
* **While riding.** A rider gets `allowStand(false)`, `allowProne(false)`, `allowJump(false)`, `allowSprint(false)` and weapons down. Linked (PM_NORMAL_LINKED, type 1), a denied stand is rewritten to crouch.
* **At the camp.** All of it is released.
* **View clamps.** Not used. The playerLinkTo arcs live only in the retained `viewAngleClamp*`, and no C1 wire field for them is known (`script_startup_extra.c`), so a clamp would fight the client.
* **Trace.** `CoDOL coop ride: t N jeep (...) price (...) off tag D | <rider> (...) <stance> linked 1 pm_type T pm_flags 0x.. eFlags 0x.. otherFlags 0x.. viewheight V/T`, once a second, through the new coop-only `<player> codol_moveStateText()`.
* **Probe.**
  * Price `off tag 0` for the whole ride.
  * The rider showed `pm_type 1 pm_flags 0x2860000` (both deny bits set) but `viewheight 60/60`.
  * A test client's command never runs the C1 movement code (it falls back to the retained Pmove, and the stance trace stayed silent even with the bot filter lifted), so the crouched seat is NOT seen on the probe. A real client runs `PM_UpdateStanceC1` (linked branch).

### 15.3 Wall one (user report: "the wall for the grenade launcher inert test isn't there")

* **What the recording shows (3:59-4:10).** A free-standing square wall with a big "1" in front of the safety pit, tan while it is the target.
* **Why it was missing.** The map stores it under the ground for the level script to raise (MEASURED, new `_cooptest bounds`, which uses `getPointInBounds`):
  * `glowing_wall_one` (the wall itself, 100 x 15 x 92) spans z −354..−262;
  * `wall_num_1` (the "1") spans −324..−278;
  * `launcher_wall_trigger` is at −307;
  * the ground there is z 21.6.
* Nothing raised them, so the wall was never there and its trigger was under the floor.
* **Now.** `raise_wall_one` lifts all three by 376 as demolitions starts (wall 22..114 on the ground, the "1" on its face, the trigger in front of it). The wall is shown and solid for the whole station. Only its marker goes with the hit.

### 15.4 Name tags (user report: "all the allies draw a red name tag all at once ... should be green")

* **How C1 draws them (MEASURED, static disassembly).**
  * The script-mover path 0x43a3b0 draws every carrying model each frame at alpha 1, with no look test. The look test and fade belong to the teammate path 0x43a140 (players only: same team, look check 0x439ef0, fade 0x439e90).
  * The colour comes from 0x43e260, which reads numbered dvars. The registrations are at 0x52b1fa:

    | Dvar | Used for | Default |
    |---|---|---|
    | "291" | the viewer's team | 0.4 0.6 0.85 |
    | "292" | any other team | 0.75 0.25 0.25 |
    | "1250" | party | 0.5 0.8 1 |
    | "1257" | team 6 | — |

  * A borrowed name slot has no clientState, so its team is 0: never the viewer's, hence red.
* **Now.**
  * Each player has one name model, shown only to him (`showToPlayer`). It is placed over the named NPC whose chest or head his line of sight passes within 28 units of, within 1500 units with a clear trace, and held 0.6 s.
  * Each coop client's "292" is set to green (0.4 0.85 0.4). No other player in a coop match is ever on another team, so only these names use it.
  * Probe: `bot0 looks at Captain Price`, then `name tag hidden`. The colour and the per-player visibility are not seen on a client.

### 15.5 The mission's look (user report: "the weather and vision seem different from the video")

* **Vision.** The level script that runs is the BASE map's (`mp_killhouse_sh`), whose coop copy passed the multiplayer Killhouse vision. `levelMain` now takes the session's vision when the mission's zone ships one (seven do), so the probe logs `vision coop_training_mp_killhouse_sh`.
* **Ambient bed.** The training and spec1 zones ship `ambient_killhouse_exterior`, which coop never played. It is now `ambientPlay`ed.
* **Weather (fog, insects, light shafts).** It is the mission's createfx, and C1 draws it client-side. 0x42df10 loads `maps/mp/<mapname>_fx.gsc` and `maps/createfx/<mapname>_fx.gsc` from the client's `mapname` dvar. When that name starts with `coop_` it loads the level's AND the mission's pair. The server tells C1 `mapname mp_killhouse_sh`, so only the multiplayer map's weather is drawn.
  * C1 is built for the playlist spelling there. Every map-named lookup handles `coop_`:
    * the zone list, 0x4CD0FE;
    * the world file, 0x618950, which strips `coop_<mode>_` back to the level;
    * the fx pair, 0x42df10.
  * **Switch, ON by default since 2026-10-14:** `coopClientMapName` (`+set coopClientMapName 0` turns it off) (`sv_main.c`). It publishes the session name as serverinfo `mapname` and as the gamestate's map (`SetClientMapName`, `sv_state.c`). The server keeps loading the level, and the server-side mirror of the client's animtrees follows the published name.
  * **Was blocked, fixed 2026-10-14.** With it on, the server loaded `coop_training_mp_killhouse_sh.animlist` and stopped at map load: `CoDOL animation invasion_opening_hummer2_soldier1 has unsupported or invalid data` (`db_load.cpp`). Root cause (MEASURED, every xanim of the nine playlist zones and common_gametype_coop run through the native converter's checks): seven clips carry IW4's four-component ("delta3D") root-rotation track (`tracks[2]`, 8-byte frames, `(x,y,z,w)`), which `BuildNativeXAnim` refused: `invasion_opening_hummer1/2`, `invasion_opening_hummer1_soldier1`, `invasion_opening_hummer2_soldier1`, `invasion_opening_hummer2_player`, `training_basketball_ball` (training), `crash_intro_player01/02`, `crash_intro_allied_01/02`, `crash_intro_ortiz` (prolog, spec crash, defense crash). The retained IW3 evaluator has only the yaw track, so the converter now reduces the rotation to its twist about the up axis (the normalised `(z,w)` pair) and stores it as the quat2 track (`xanim_native.cpp`, test `test_xanim_native` delta3d). Translation, yaw, timing and notetracks are exact; only the pitch/roll of the root delta is lost, which the IW3 engine cannot represent. All nine playlists load with the switch on, and `tools/coop_anim_check.py` reports identical server/client `generic_human` trees for all nine with either map name (1058 entries with the level's list; 1244 training, 1110 spec1, 1142 prolog, 1120 spec crash, 1160 defense crash, 1090 spec estate, 1164 take estate, 1127 spec fav, 1149 take fav with the mission's).
* **Fog.** No `setExpFog` is authored for coop. The mission's own fx script carries only a commented-out one, and no createart ships. The fog in the recording is the createfx `fog_river_200` (107 placements), which needs the switch above.

### 15.6 Fastfiles (MEASURED from the shipped zones and the user's C1 console log)

* **What C1 loads for training.** In order: `coop_training_mp_killhouse_sh_load`, then `patch_` + base of `common_mp`, `localized_common_mp`, `common_gametype_coop`, `localized_common_gametype_coop`, `coop_training_mp_killhouse_sh` and `mp_killhouse_sh`.
* **What the server mounts.** All of those except `_load` (load-screen only: two materials and a rawfile) and the front-end `ui_mp` set. Each `patch_` is mounted after its base, and the last mounted wins.
* **What the patch zones carry.**

  | Zone | Contents |
  |---|---|
  | `patch_coop_training_mp_killhouse_sh` | `animlists/coop_training_mp_killhouse_sh.animlist`, `KILLHOUSE_HINT_FRAG`, xmodels `body_complete_tf_tan`, `weapon_m67_grenade_obj` |
  | `patch_mp_killhouse_sh` | its animlist, 31 `MP_TUTORIAL_*` strings, 31 sounds |
  | `patch_common_gametype_coop` | 11 stringtables, `beretta_so`, `flash_grenade_so`, 11 xanims, 39 xmodels |

* None ships a level script or map ents. Nothing is missing.
* The only difference from C1 is the order between the mission and the level zone. No rawfile, stringtable, weapon or xanim is in both, so the order changes nothing.

---

## 16. Ninth pass: the training HUD, the rider's eye, the end (2026-10-09)

Compared the user's 2026-10-08 playthrough (0:40-5:30 of the session recording) with the retail recording (1:19-5:53). The order of stations and the timing already matched (about 4.5 minutes each), and the return to the lobby worked (unlock flags 633|652|662).

* **HUD** (`_cooptraininghud.gsc`, new). Retail draws only the mission zone's own `KILLHOUSE_*` strings; ours printed English bold text. Now:
  * a typed title card (`INTROSCREEN_LINE_1..3`) over black that lifts into the ride;
  * a light-blue typed objective line at the top for each task (`ENTER_STATION_NUMBER`, `SHOOT_EACH_TARGET_WHILE1`, `THROW_A_GRENADE_INTO`, `DEBRIEF_WITH_CPT_PRICE`, ...);
  * white key hints on a dark band (`LOOK_UP`/`LOOK_DOWN`, `HINT_HIP_ATTACK_PC`, `ADS_THROW`, `RELOAD`, `SIDEARM_SWAP`/`PRIMARY_SWAP`, `MELEE_TARGET`, `FRAG`, `FIREMODE`, `SPRINT_PC`, ...);
  * the `OBSTACLE_COUNTDOWN_3/2/1` before GO;
  * 3D markers with distance (`objpoint_default`) on the current goal and Price, and `waypoint_target` squares on each window and wall still to hit.
* **Pistol round.** Retail's "shoot each target with your pistol" (3 hits, 20 s time-out) now runs between drawing the pistol and the pit.
* **The rider's eye.** The feet sit 2 below the seat tag (was 12); the view had been below the dash.
* **Name tags** sit 20 lower, at head level.
* **The end.** After Price's last line the view blurs and darkens (`setBlurForPlayer`).
* **T-pose sweep.** None in this mission: every cast idle and `bh_pilot_idle` resolves, and Price at the wheel is posed.
* **Probe.** Port 17781, `playroute`: mission complete and lobby return, no script error; 3 real pistol hits counted; rider offset z 33.77. Not yet seen on C1: all HUD rendering, the seat and tag heights, the end fade.
* **Still open:**
  * the fog, overcast and the mission's createfx need `coopClientMapName 1`, which loads only once a rejected xanim falls back to the null animation instead of ending the load (a `src/db_load.cpp` change, probe-verified on a private build, C1 untested, not integrated). The switch would also make the mission animlist's trainer idles (Price's crossed arms) usable;
  * subtitle speaker names (every pit line is labelled with one name): client zone data, not the server;
  * the English "MISSION COMPLETE" banner (shared `_coopmission`), ally arrows on the compass, grey unquoted name tags, the ambient helicopters, wall one's grey look, the darker range interior.

## Mission coop_take_mp_fav_tropical ("the meet"), from the retail recording "Campaign Walkthrough Part 2" (2026-10-08)

**The guide.** A public gameplay recording of the retail CoDOL client playing the second campaign mission. It is the authority for this mission's order, cast, places and moments, as section 14's recording is for the training.

* The frames were viewed in the scratchpad only. Nothing from the recording is in the repo.
* Lines were placed from the subtitles C1 draws on screen, matched to the zone's subtitle strings and `favela_*` aliases. They are paraphrased here, never quoted.
* Times are recording time. 0:00-1:28 is the lobby, the loading screen and the briefing; gameplay runs 1:29-4:15; 4:15-5:20 is the outro briefing; 5:20 is the lobby's "mission complete" card; 5:30 is the campaign map with the next mission (the crash site) selectable.

### M2.1 Which mission

* **The map.** The favela of `mp_fav_tropical`: a hillside slum, paved and dirt streets, a water tower, a yellow building at the far end. The minimap in the corner is the mission's own (`compass_map_mp_fav_tropical_sp`).
* **The HUD.** The title card names the mission with `COOP_TAKE_MP_FAV_TROPICAL_INTROSCREEN_LINE_1` (the meet), then a date with the running clock and the country (`_LINE_2`, `_LINE_3`). The objectives are `COOP_TAKE_MP_FAV_TROPICAL_GRAB` (grab the informant) and `_CAPTURE` (take Ortiz alive), and the marker over Ortiz is `_TARGET`.
* **So it is `coop_take_mp_fav_tropical`**, chapter 1 mission 1 after the training (challenge 801 `so_coop_take_mp_fav_tropical`, unlock 653 for the defense crash mission, section 13.1). The probe's completion logged `unlock flags 632|635|653`.

### M2.2 Timeline (what the recording shows)

| Time | Where / what | Lines (alias) | HUD |
|---|---|---|---|
| 1:29 | The rookie sits in the **front passenger seat** of a left-hand-drive sedan; the driver's seat is empty. No weapon in hand. The street ahead runs north-north-east; a blue car and a wall on the right. | | title card bottom left (mission, date + clock, country); objective top centre: grab the informant |
| 1:30-1:35 | A grey UAZ van passes on the left and stops ahead, its **rear doors toward the car**. | `favela_pri_simplejob01` (a simple job, stay sharp, we move on Ortiz's signal) | |
| 1:38 | Ortiz and two men walk from the van toward three men waiting by the blue car. | `favela_gst_02` (Ghost: Ortiz is there, the other two are our informants) | |
| 1:40 | | `favela_ortiz_01` (Ortiz: relax, you'll get paid) | |
| 1:43-1:45 | The waiting men shoot both informants; Ortiz breaks away. | `favela_gst_03` (Ghost: Ortiz sold us out, get him) + `favela_inf2_01` (an informant: we should never have come) | |
| 1:46-1:48 | The windscreen is shot through and crazes; the rookie flinches. | `favela_pri_intercept01` (Price: rookie, after Ortiz; Ghost will cut him off) | objective: take Ortiz alive; marker over Ortiz |
| 1:49-1:51 | The rookie opens the passenger door and steps out with an **M4 (red-dot sight)**; Ortiz runs west past the car. | `favela_gst_04` (Ghost: he'll pay for this) | |
| 1:55-1:59 | Ortiz (black shirt, jeans) runs down the street ahead. | `favela_pri_alive01` (Price: alive, Ghost!), `favela_gst_07` (Ghost: if I get him first, no promises) | |
| 2:04 | A civilian runs across the street. | | |
| 2:05-2:16 | A **white technical** comes round the corner at the end of the street and stops with its doors open; its crew jumps off. | `favela_pri_ambush01` (Price: ambush, Kortez's men), `favela_ortiz_03` (Ortiz: fire!), `favela_pri_getcover01` (Price: cover, Ortiz brought help, take them out) | distance marker on the next objective point (e.g. "21m") |
| 2:18-2:22 | A dirt yard with the pickup parked; the rookie switches to an **AA-12 with the heartbeat sensor** (HUD name `aa12_hb_so`). | `favela_ortiz_02` (Ortiz: take them out!) | |
| 2:24-2:44 | Narrow brick alleys north; enemies at the alley ends. | `favela_pri_tangos01` (Price: multiple targets, open fire), `favela_pri_ortiz01` (Price: Ortiz isn't alone), `favela_ortiz_04` (Ortiz: shoot them!) | |
| 2:46-3:18 | **Rooftops**: banana plants, chain-link fences, a long firefight across the roofs to the next block (no lines). | | the waypoint marker |
| 3:20-3:30 | Down into a street, then through a **green-tiled house**. | | |
| 3:32 | The rookie fires near Ortiz. | `favela_pri_0269` (Price: hold fire, we need Ortiz alive) | |
| 3:36-3:56 | Alleys, a hut, a dirt street with litter. | | |
| 3:57 | | `favela_gst_08` (Ghost: where do you think you're going, Ortiz) | |
| 4:00-4:14 | By the **yellow building** (a white van and a wrecked white car behind), **Ghost kneels on Ortiz**, who lies on his back, and punches him. | `favela_pri_0270` (Price: don't kill him, Ghost), `favela_gst_09` (Ghost, through his teeth), `favela_pri_0272` (Price: later; get him to the landing zone), `favela_gst_10` (Ghost: lucky him) | |
| 4:15-4:24 | The view blurs and fades: mission over. | | |

**Cast.** Ghost (skull balaclava, `body_hero_ghost_socom`, his nametag) appears only at the end; before that he is a voice. Price is a voice throughout. Ortiz runs unarmed. The enemies are militia (`body_militia_*`) with pistols, rifles, an LMG and shotguns. A few civilians in slum clothes. No dogs, no airliner and no helicopter were seen.

**Weather / vision.** Overcast tropical daylight with haze: the mission's vision (`coop_take_mp_fav_tropical`, applied by `_coopmode::levelMain` since c306aa91). The zone's createfx (car lights, fire drips) is client-side.

### M2.3 The map's own events (MEASURED, the addon MapEnts)

* **The meet** (`group opening_scene`): `player_vehicle2` (`vehicle_luxurysedan_viewmodel`, -258 2127, yaw 28) and `player_vehicle` (`_2009_viewmodel`, 372 2402, yaw 180); `van` (`script_vehicle_uaz_van`, target `van_start`, which is NOT in the addon); `chase_target_spawner` (Ortiz, `actor_enemy_villian_ortiz_so`, `body_airport_assault_a`); `opening_driver/gunner1/gunner2_spawner` (`actor_enemy_favela_deserteagle_so`, by the blue car at 182 2169, facing the meet); `opening_scene_node` (157 2239) and `_vehicles` (269 2239).
  * The van's place and facing were worked out from the recording: from `player_vehicle2` it appears 17 degrees left at about 450 units, rear on. That is ahead of the car and beside Ortiz's spawner, not the van's authored spot (which faces west, 870 units off, and is out of view at 1:29).
* **Ortiz's run** (IW4 favela's runner): 32 nodes from `escape_path_start` (animation `favela_traverse_rail_ortiz`) to `me0_auto323` (1088 1216, `script_flag_set ending_tackle_arrived`, `script_flag_wait ending_tackle`). Nodes set `chase_<n>_arrive` and wait for `chase_<n>_leave`, which trigger volumes beside the objective points set. `me0_auto318` carries `favela_escape_bigjump_faust`, the jump down from the rooftops.
* **Objective points** `obj_10` .. `obj_100` (script_origins), each beside a leave or arrive trigger (`chase_05_leave` .. `chase_90_leave`, `obj_40/45/47_arrive`, `obj_50_complete`, `obj_75_arrive`).
* **Helpers**: `combat_zone_10 .. _60` flag triggers and spawner groups (25 enemy spawners in all). `combat_zone_51` / `_55` have no player trigger, only `*_ai_trigger` volumes with spawnflags 9 (AI_AXIS | NOTPLAYER): Ortiz releases them as he passes.
* **The technical**: `script_vehicle_pickup_technical` (-602 -227) on `vnode_technical1_start` (25 mph, five nodes ending at -1052 439), `script_turretmg 1`, its M2 a `misc_turret` (`delete_on_load`). It is started by `start_technical`, another AI-only trigger, at Ortiz's `chase_10` node. Its crew are the three `combat_zone_10` spawners with `script_vehicleride 0` and `script_startingposition 0/1/2`. The model's tags (MEASURED, `checkmodel`): `tag_driver`, `tag_passenger`, `tag_gunner`, `tag_50cal`.
* **Ghost**: `ghost_intro` (546 2345) with `ghost_intro_goal` (980 1192) and `delete_ghost_intro` (-1104 1440); `ghost_ending` (978 1191) targets the chase target's last origin.
* **Civilians**: 21 `actor_civilian_slum_male_so` in groups `civ_start` (delays 0-13 s), `civ_05` .. `civ_70`, `civ_window_1`, `civ_garage_door`, `civ_hiding_door`, released by their flags.
* **Others**: `van_blocker` (a static van that backs up on `move_van`), `sprint_hint` / `ads_hint` triggers, `airliner_flyby` (a struct path, no airliner model in the zone), `fence_dog_spawner` + `fence_dog` (a dog vaulting a fence, `sniper_escape_dog_fence`), `chase_knock_phys` (physics pushes), `destructible_toy` chickens, `easy_model` cover props.

### M2.4 What was built (`coop/maps/mp/gametypes/_coopmission_take_fav.gsc`)

Selected by `sv_sessionMap == "coop_take_mp_fav_tropical"`, like `_cooptraining`. It sets `level.coop_mission_flow`, so `_coopmission` skips its generic phases (the objective chain and group waves the first pass gave this map). The shared flag triggers, flag groups, `spawn_soldier`, death and failure watches, nametags and the lobby-return end still serve it.

* **Loadout.** Every spawn carries `m4_grunt_so` (in hand) and `aa12_hb_so`, nothing else: no pistol, no offhands, as the recording's HUD shows.
* **The meet**, timed to the recording (each line within a second or two of its subtitle):
  * Each player is seated as he spawns. The first takes the front passenger seat of `player_vehicle2`, the second that of `player_vehicle`, then the rear seats. Weapons are down and the seat is locked (`_coopintro::seat_lock`). The seat is MEASURED: `tag_passenger` sits under the floor, so it is `tag_driver` mirrored.
  * The meet starts when a rider looks around, or after 8 s. The title card and objective text (the zone's own strings, `newHudElem`) come up, and the van drives in (`scn_favela_van_pullup`, `_skid2stop`, `_door_open`).
  * Ortiz and two informants (slum bodies) step out and walk to `opening_scene_node`. The three authored gunmen wait by the blue car with their fire held.
  * The gunmen shoot both informants (`magicBullet`, then `doDamage`), and Ortiz runs. The windscreen shatters: `scn_favela_car_glass_shatter`, and `tag_glass_front` is swapped for `tag_glass_front_d`.
  * The objective changes, and objective 1 goes on Ortiz. The riders get out on the passenger side (`scn_mpfav_cardoor_open`), and the gunmen open fire.
* **The chase.**
  * Ortiz (team axis, `ignoreall`, cannot be killed) runs the 32 nodes on the bot waypoint graph and waits at each `script_flag_wait` node until its leave flag is set (or a player is within 220 units).
  * A leg he cannot walk (the rail vault, the rooftop jump) is placed after 3 s without headway. A long leg is cut short while no player can see him, so he keeps the lead the recording's runner has.
  * Objective 0 walks the 18 points in order. A point is shown only once Ortiz waits at the node its flag releases, and reaching it counts as crossing its trigger.
  * Respawns and late joiners rejoin behind a living teammate, or else at the last point reached.
  * Shooting Ortiz draws Price's alive lines (`0269`, `0266`, `0268`; one per 8 s).
* **The technical.** The pickup is a mover on its node path (`_coopvehicle::follow_path`) with the M2 attached to `tag_50cal`. The crew ride it as actors (`codol_actorRide`): driver and passenger on their tags in `technical_driver_idle`, the gunner on `tag_gunner`.
  * At the stop the driver and passenger jump off and fight. The gunner fires five-round bursts at a visible player from the mount with his own role weapon (a coop-window slot).
  * It starts at Ortiz's corner, or when a player reaches `combat_zone_10`. The ambush lines play on it.
* **Helpers.** The flag groups spawn as the player advances. A player who reaches the objective point beyond a group's trigger volume without crossing it (another way round, or a fall) releases that group there. Ortiz releases `combat_zone_51` / `_55` by crossing their AI triggers. The chase lines play on `combat_zone_20 / _30 / _35`.
* **Civilians** come out on their flags (`civ_start` with the meet). They run to their target struct and are removed, and cannot be hurt.
* **Ghost** stands at `ghost_intro`, runs for `ghost_intro_goal` when Price sends the rookie, and is gone at `delete_ghost_intro`. At the end he comes from `ghost_ending` with `favela_gst_08` and takes Ortiz down (`scn_ghost_takedown`).
  * Ortiz falls (`run_pain_fall`) and lies on his back (`dying_back_idle`). Ghost kneels by him (`exposed_crouch_idle_alert_v1`), with `scn_favela_ortiz_punch`.
  * When the rookie comes within 400 units, Price and Ghost close it out (`0270`, `gst_09`, `0272`, `gst_10`). Then **mission complete**, with section 13's report and lobby return.
* **Also:** `van_blocker` backs up on `move_van`, and the sprint and ADS hints (the zone's strings) show on their triggers. Ghost carries the look-only nametag (`_cooptraining::show_nametags`).

**Shared files touched (minimal):**

| File | Change |
|---|---|
| `_coopmode.gsc` | `_coopmission_take_fav::init()` after `_cooptraining::init()`; `level thread ..::main()` after `_cooptraining::main()` |
| `_coopmission.gsc` | `main()`: `else if ( isDefined( level.coop_mission_flow ) )` skips the generic phases (any mission script can set it) |
| `_cooptest.gsc` | `default:` forwards an unknown verb to `level.coop_mission_testcmd` (any mission script can set it) |

### M2.5 Not built, and why (closest substitute in brackets)

* **Ortiz's and Ghost's map animations**:
  * the rail vault `favela_traverse_rail_ortiz`;
  * the rooftop jump `favela_escape_bigjump_faust`;
  * the tackle and the beating (`scn_favela_escape_end_*`);
  * the window smash `window_smash_stop_inside`;
  * the civilians' `unarmed_close_garage` and `unarmed_shout_window`;
  * the dog's fence vault.

  These are only in `animlists/coop_take_mp_fav_tropical.animlist`, which C1 does not load (section 10.1). [He is placed past the vault and the jump. The tackle is a fall, a lie-down and a kneel from the common lists, with the zone's sounds.]
* **The floating "take alive" label over Ortiz** needs `objective_setpointertextoverride` (IW4), which is not registered. [`objective_onEntity` on him; the zone's `_TARGET` string is not shown.]
* **The van's drive path** (`van_start`) is not in the addon. [A straight drive-in, placed from the recording.]
* **The technical's turret** (`misc_turret` `50cal_turret_technical`) is not in C1's coop weapon window, and a `magicBullet` of an unregistered weapon crashes C1. [The gunner's bursts use his role weapon from the mount.] The technical cannot be destroyed (`vehicle_pickup_technical_destroyed` ships; nothing breaks it).
* **The fence dog** (`actor_enemy_dog_so`): there is no coop dog species (section 7). The recording shows none.
* **The airliner flyby**: no airliner model ships in the zone.
* **Civilians' own walks** (`civilian_hurried_walk`, `_cool_walk`) are in `animlists/common_civilian`, which no species mask makes C1 load. [They run with the soldier run.]
* **Goal volumes** (the `info_volume` targets of the helper spawners) are not used. [Soldiers hold near their spawner and one in three hunts the players, as everywhere in coop.]
* **The end blur and fade.** [`_coopmission`'s outcome text, frozen controls and the 10 s delay.]
* **Line lengths** are read off the subtitles, not the sound files.

### M2.6 Probe (port 17793, scratchpad home, developer 1, test clients, fresh `map` each run)

* **Script checks.** `script_resolve.py --category coop ... --seed maps/mp/gametypes/coop.gsc` and `--only maps/mp` pass. The mission itself raised no script runtime error. One error came from the harness's own `favstatus` verb (an undefined chase index before the chase), which ended that map; it is fixed.
* **One client** (`favroute`: the test client WALKS to each objective point and its trigger with `botMoveTo`, and fires real rounds at enemies it can see; a stalled walk is logged STUCK and teleported). Last run:
  * **Meet.** Seated `player_vehicle2 row 0` with `m4_grunt_so aa12_hb_so`; lines `simplejob01` .. `gst_07` in order; the van stops at (61 2444); `informant 0 shot, alive 0`, `informant 1 shot, alive 0`; out at (-228 2070).
  * **Chase.** Ortiz waited at each node and ran on as the walker reached each point. He crossed `start_technical`, `combat_zone_51_ai_trigger` and `combat_zone_55_ai_trigger`. He was placed "out of sight" past the rooftop jump.
  * **Technical.** `crew 3` rode the technical to (-1052 439); `ambush01`, `ortiz_03`, `getcover01`.
  * **Helpers.** `combat_zone_10/20/30/35/40/45/50/60` released their groups, four of them by the reached-point fallback because the walker teleported past their volumes. Lines `ortiz_02`, `tangos01`, `pri_ortiz01`, `ortiz_04`, and `0269` on a wounding of Ortiz (`favhitortiz`). The sprint hint showed.
  * **End.** `gst_08`, `Ortiz taken down at (1119 1194 192)`, `0270` .. `gst_10`, then `over, success 1 (Ortiz captured), 23 killed, 24 spawned`, `unlock flags 632|635|653`, `exitLevel`, parked. The test client died once (at obj_70) and rejoined at the last point.
* **Two clients.** bot0 in `player_vehicle2`, bot1 in `player_vehicle`; both out at their passenger doors.
* **SIMULATED / harness limits:**
  * The straight-line walker cannot climb to the rooftops or follow the alleys' turns, so most legs were teleported.
  * Its kills were real rounds, but its aim is the harness's, not a player's.
  * A test client does not turn its view, so the meet always started on the 8 s timeout.
* **NOT seen on a C1 client:**
  * the seat and exit positions in the sedan;
  * the van's look as it drives in;
  * the windscreen part swap;
  * the title card and objective text (including the clock token in `_LINE_2`);
  * the crew's seated pose on the technical;
  * the kneel and lie-down at the end.

---

### M2.7 Second pass: the playthrough against the recording (2026-10-09)

The user's 2026-10-08 playthrough (take fav from 7:36 to 10:22 in the session recording) was compared frame by frame with the retail recording.

* **The car.** The recording's interior is the dark 2009 sedan, `player_vehicle`, at 372 2402 facing west: grey dashboard with a radio, and the blood layer IW4 hides on the player's car. Its authored place puts the van's spawn (605 2294, facing west) behind it on the left. The first rider now sits there, and the second takes `player_vehicle2`.
* **The van.** It drives from its authored spawn down the left lane and stops about 390 ahead and 50 left, at (-18 2352); the lane traced clear.
* **The meet.** It plays 210 ahead and 70 left of the car. The gunmen wait 115 left of the meet, out of the van's lane.
* **The exit** is searched: a player hull must fit at the spot and the seat must see it. The first choice is the passenger door (372 2466).
* **Ghost** is no longer at the meet; retail shows him only at the end. At the end he kneels with `casual_crouch_idle`. `exposed_crouch_idle_alert_v1` froze at time 1.0 on the server and C1 drew Ghost in his bind pose.
* **Markers.** Objective points and Ortiz carry `objpoint_default` waypoints. Ortiz's text label still needs `objective_setpointertextoverride`, which is not registered.
* **The end.** The view blurs (`setBlurForPlayer`) after Ghost's last line.
* **Hints.** The sprint and ADS hints are off: retail shows none, and C1 printed "[unbound]".
* **Open items:**
  * the outro video: `common_gametype_coop`'s `coop_eog_summary` menu runs `uiScript playOutroVideo` (C1 ~0x5ecba0), which plays `<ui_mapname>_outro.bk2`; nothing opens it yet (a shared `_coopmission` change, not yet integrated);
  * weapon drops from dead enemies;
  * M4 clip 20 in retail against 30 here (the def's clip size; only `m4_grunt_so` ships in the zone);
  * retail's heavier enemy fire;
  * the windscreen flinch and door view animations.
* **Probe.** Port 17778, `favroute`: a complete run with no script errors (`success 1 (Ortiz captured), 23 killed, 24 spawned`, unlock flags 632|635|653). Nothing yet seen on C1.

## Take estate ("Safe House"): the mission as the retail recording plays it (2026-10-08)

**The guide.** A public recording of the retail CoDOL client playing the fourth campaign video ("Walkthrough Part 4") is the authority for this mission's order of events, cast and moments. It was used the way section 14 used the training recording: frames viewed in the scratchpad only, nothing from it in the repo, no audio. Lines were placed by matching the subtitles C1 draws on screen to the subtitle each sound alias carries in the mission's zone (see "Voice lines" below). Subtitles and HUD text are paraphrased here, never quoted. Times are recording time.

### Which mission

* **Playlist `coop_take_mp_estate_tropical`**: chapter 1, the mission after `coop_defense_mp_crash_tropical`; challenge 803 `so_coop_take_mp_estate_tropical` (section 13.1). MEASURED:
  * The opening HUD objective, the title card and every later objective line are this zone's own strings (read from `coop_take_mp_estate_tropical.zone`, GBK text): `COOP_TAKE_MP_ESTATE_TROPICAL_STAGE_HINT_STEALTH`, `INTROSCREEN_LINE_0..2`, `OBJ_KILL_GUARDS`, `OBJ_INTEL`, `STAGE_HINT_BREACH`, `OBJ_LEAD`, `OBJ_CAPTURE`, and the `VAZQUEZ` and `OBJ_FOLLOW` marker labels.
  * The level is the estate: the river beach, the ruined houses up the hill, the road and the wooden villa with its basement safe room. The corner map is the mission's `compass_map_mp_estate_sp`.
  * The weapons are the zone's: an M21 EBR with scope and silencer (`m14_scoped_silencer_woodland_codo_so`) and a silenced red-dot MP5K (`mp5_silencer_reflex_codo_so`). The safe room's shelves hold the addon's `weapon_*` pickups (M240, AUG, WA2000 thermal, L86, P90, frags, flashbangs).
* The menu calls the mission "Safe House". The pre-mission briefing (Price over a ship and a map of the region) is the zone's `estate_pri_08` line.

### Timeline

| Time | What happens | Lines (alias) | HUD |
|---|---|---|---|
| 0:00-1:20 | Lobby, mission select, loading screen, briefing cinematic | `estate_pri_08` | |
| 1:25 | **Zodiac up a foggy river.** The rookie sits in the bow, weapons down; Ghost rides behind. The title card, lower left: the mission's name, a date and time, the region's northern border (three typed lines). | Price: take Vazquez alive, and any intel on the cargo (`estate_pri_0410`) | objective: infiltrate the main building |
| 1:30 | Still on the water. | Ghost: the intel said nothing about a private army guarding the estate (`estate_gst_141`) | a "follow" marker on Ghost |
| 1:32-1:44 | Out in the shallows beside the boat, M21 up. A three-man patrol up the bank. | Ghost: target spotted, 20 m, patrol (`_112`); we take them at the same time (`_113`) | hold-breath hint in the scope |
| 1:47-1:52 | Scope shots; Ghost takes the other man. | praise (`_129`..`_137`) | |
| 1:56-2:02 | Up the hill behind Ghost; the rookie switches to the MP5K for a while. | up the hill (`_114`); patience, nothing stupid (`_115`) | |
| 2:04-2:34 | The ruined houses: a pair, then men on the hill road stopping to smoke; scope kills. | your call (`_116`); two, then three, stopped for a smoke on the road, move up (`_121`, `_122`) | |
| 2:44-3:04 | Past a farm house, a tractor and cars; two smokers by the mid house, killed together. | keep moving (`_117`); two smoking, one each (`_118`) | |
| 3:06-3:18 | Through the fog past more buildings, no fight. | go (`_140`); enemies in those buildings (`_138`) | |
| 3:20-3:28 | The villa comes into view, a 35 m marker on it. | Vazquez is in the house ahead (`_146`); rookie, you go in first (`_150`) | marker on the house |
| 3:30-3:38 | A big group walks out of the house with Vazquez (a white "Vazquez" label over him). | a big group coming out (`_147`); we can't slip past (`_148`) | |
| 3:40-3:56 | Firefight among the cars in the front yard; the rookie on the MP5K, an enemy's MP5K offered as a swap. | Vazquez: lock the doors (`estate_vaz_04`) | objective: kill Vazquez's guards |
| 3:58-4:32 | Into the villa (hall, stairs, kitchen, pool deck); guards inside; the Vazquez marker leads through the house. | Ghost: there he is, kill his guards (`_152`); he's running, we must catch him (`_153`); Vazquez: cover me, to the safe room (`_vaz_06`) | |
| 4:34-4:36 | Downstairs, a game room with a pool table and a bar; a **glowing yellow laptop** on the counter with a white "intel" marker. | | objective: get the intel on Vazquez; use hint |
| 4:38-4:42 | A door at the foot of the stairs, **glowing yellow**, with a breach icon. | Price: breach, and remember, alive (`estate_pri_0461`) | objective: breach and capture Vazquez; breach hint |
| 4:44-4:50 | Charge on the door, the door blows in, **slow motion**; four guards around Vazquez. | | |
| 4:50-5:05 | The rookie dies in the slow motion; the retry resumes at the laptop checkpoint (the title card again, with the checkpoint's time). | | death tip |
| 5:08-5:24 | Second try: laptop taken, breach, slow motion, guards killed; Vazquez crouches in a corner. | take the laptop, we need what's on it (`_154`) | |
| 5:26-5:40 | The safe room is an armoury: weapons on the floor and shelves, frags and flashbangs. | Ghost: tell us what you shipped out, or I'll enjoy making you (`_158`); Vazquez: don't kill me, I'll talk, protect me (`_vaz_08`) | objective: lead Vazquez out of the villa |
| 5:42-5:50 | Vazquez knocks Ghost aside and bolts out of the back. | Ghost: a cry (`_159`); Price: he's going to run (`estate_pri_0470`); Ghost: mercenaries again, rookie, kill them and catch him (`_160`) | objective: catch Vazquez |
| 5:52-6:30 | Out the back into the rain; mercenaries (grey Shadow Company kit) and militia among parked cars and a shed; the rookie picks up an L86 LSW. | there he is (`_162`); don't stop, he mustn't get away (`_161`) | Vazquez marker |
| 6:36-6:41 | **The cliff over the water**: power lines, an island in the fog. Vazquez turns, hands up. | Vazquez: I surrender (`_vaz_09`) | |
| 6:41-6:47 | He is shot from across the water and falls. | | |
| 6:53-7:02 | Standing over the body. | Price: who was that (`estate_pri_0474`); Ghost: no idea, a clean professional hit (`_163`); Price: you have the laptop, and his killer is likely his buyer (`estate_pri_0477`) | |
| 7:05-7:11 | | | mission complete, fade |
| 7:15-8:00 | Debrief cinematic (Price over the map), then the lobby | `estate_pri_09`..`13` | |

**Weather and vision.** Rain, low cloud and thick ground fog the whole mission. The zone ships no vision of its own (only `slomo_breach`), so the base level's `mp_estate_tropical` vision is used (section 15.5's session rule passes it through). The fog, rain and river effects are the mission zone's client-side createfx.

**Allies and enemies.**
* Ghost is the only ally. He leads the stealth part and fights alongside afterwards (addon `actor_ally_hero_ghost_estate_so`, `script_friendname Ghost`). Price is on the radio only.
* Militia (`actor_enemy_favela_*`) in the forest and the house: patrols of two to four with smoking and phone idles, and two snipers on the villa balcony.
* Shadow Company mercenaries (`actor_enemy_desert_elite_*`, the `ending_attacker` sets) arrive for the chase.
* Vazquez: in the front group, then in the safe room, then the chase.
* No helicopter is seen. The three authored Mi-28s are lost in the fog; they leave on `vazquez_dead`.

**Authored in the addon but not in the recording.** The bouncing betties (`bouncing_betty_gameplay` radius triggers, `alternate_bb_location` origins), the mortars (`end_mortar`, `ending_mortarhit_*`), the smoke pots (`smokepot1..3`), the javelin, the helicopter crash point and the escape back to the boat (`player_escape*`, `escape_node*`, the `trigger_order` triggers, the `TEMPVO_1..4` "back to the boat" strings, `STAGE_HINT_ESCAPE`, `TIMER_EVAC`). They belong to a longer version of the mission: an ambush and an escape after the sniper. The retail run ends at the cliff, so they are not played.

### Voice lines: how each alias was placed

* The mission's dialogue aliases are `estate_gst_*` (Ghost), `estate_pri_*` (Price) and `estate_vaz_*` (Vazquez) (docs/zone_assets). Each alias carries its subtitle inline in the zone: 52 Ghost, 14 Price and 9 Vazquez subtitles, each behind a pointer into a 204-byte-stride run of alias records, in the zone's order.
* The counts match the numbered aliases exactly: Ghost 110, 112..131 and 133..163 = 52 (the `_100_2`, `_200_*` and `_300_2` variants carry none); Price 0410..0478 and 08..13 = 14; Vazquez 01..09 = 9. So the n-th subtitle belongs to the n-th alias. That is INFERRED from the counts and the order. It is consistent with the recording: the subtitles appear in alias order through the mission.
* The private mapping (alias to subtitle) is kept outside the repository. The script's line lengths are estimates from the subtitle text, since the audio was not used.
* Some names in the zone's sound index are not aliases: `ammo_box_762_col` is a loaded sound, and playing it stopped the probe with a fatal script error. The mission plays every sound through a `soundExists` check.

### Implementation (`coop/maps/mp/gametypes/_coopmission_estate.gsc`)

Selected by `sv_sessionMap == coop_take_mp_estate_tropical`, like take fav's script. Its `init()` sets `level.coop_mission_flow = "estate"` (the shared hook that turns off `_coopmission`'s generic phases), and two opt-ins of its own: `level.coop_mission_owns_groups` (no flag-named group release) and `level.coop_mission_owns_markers` (no `obj_*` markers or escape warnings from `_coopevents`). Its probe verbs come in through `level.coop_mission_testcmd`.

| Stage | Built from | Implemented |
|---|---|---|
| Zodiac | addon `script_vehicle_zodiac zodiacs_intro`, its `info_vehicle_node` chain to `boat_last_node` | A `vehicle_zodiac` mover rides the river's surface; the nodes lie 15-48 units under it (MEASURED with `_cooptest trace`). Players sit in the bow, crouched and locked (`_coopintro::seat_lock`), weapons down; Ghost rides behind (`codol_actorRide`). The ride waits for a rider to look around (or 8 s), plays the zodiac loop, then sets everyone down in the shallows. The title card is three HUD lines of `INTROSCREEN_LINE_0..2`. Price's and Ghost's opening lines play on the way. |
| Stealth | addon `patrol_initial`, `patrol_ruinhouse` (`ruinhouse1_encounter_enemy`, `leave_ruinhouse_patrol`), `guard_midhouse` and their `script_struct` chains | Spawned through `_coopmission::spawn_soldier` while the boat is on the river. Each walks his chain (IW4 `_patrol`: `script_flag_wait`, capped; `script_flag_set`; `script_delay_post`) and stands at its end with `patrol_bored_idle_smoke` or `_cellphone` (common animlist). They hold fire (`ignoreall`) and never hunt (`coop_no_hunt`). A group is alerted by: a man surviving a shot (0.8 s grace for a follow-up), a squadmate seeing a body within 1.5 s, a player in the open within 240 / 160 / 96 units (standing / crouched / prone), or an unsuppressed shot within 2000 units. An alerted group fights and hunts, and its area's reinforcements come (`reinforce_ruinhouse` or `reinforce_midhouse`, five or six). When a player opens on a group, Ghost kills another man of it (a silenced `magicBullet`, then the kill). Ghost praises kills. The flags `patrol_start_ruinhouse`, `ruinhouse1_enemies_done` and `guard_midhouse` drive the patrols as authored. |
| Ghost | base `ghost_color_trigger` triggers and colour nodes `r0..r15` | A friendly coop soldier (`ally_template`) in `body_forest_tf141_ghost` + `head_hero_ghost_forest`: invulnerable, a silenced MP5, the look-only green name tag (`_cooptraining::show_nametags`), a compass marker during the stealth part. A player in a colour trigger sends him to that colour's node. Left more than 1100 units behind and out of every player's sight, he is put at his goal. |
| House front | addon `patrol_prehouse` (six, and two balcony snipers), Vazquez | The group comes out with Vazquez among them; after the lines it goes loud. Objective: kill the guards, with a marker on Vazquez. He runs back in to `vazquez_run_away` (which sets `vazquez_out_of_sight`) and is removed there, as authored (`delete_enemy`). |
| House | addon `house_initial`, `reinforce_house`, `spawner_cell`; flags `in_doorway`, `in_house`; base `stairs_blocker` | On `in_house`: the kitchen guards and five house guards, Vazquez's way marked by `obj_vazquez_1` / `_2`. Then the basement stairs open, with the two cell guards below. |
| Intel | common `com_laptop_2_open_obj` | The glowing laptop on the game room's bar counter (440 730 81; MEASURED, a ceramic top at z 80 over the basement floor at z 38). No laptop is authored, so the spot is INFERRED from the room. Use within 90 units takes it (the `HINT_INTEL` line when near); Ghost nags (`_155`..`_157`). |
| Breach | base `breach_solid`, `trigger_use_breach`, `breach_fx`; addon `trigger_multiple_breachIcon`, `info_volume_breachroom`, `breach_enemy_spawner` ×4, `vazquez_breach`; zone `mil_frame_charge(_obj)`, fx `explosions/breach_door` / `breach_room`, `vision/slomo_breach`, `slomo_whoosh` | The glowing charge frame on the door. Use in the breach icon's volume places the charge. 1.5 s later: the door fx, the door brush gone, `setSlowMotion` 1 to 0.3 with the slow-motion vision while the four guards spawn and fight, normal speed after 4 s or when they are dead. Vazquez cowers (`covercrouch_hide_idle`). Sets `room_1_cleared`. |
| Lead out, chase | addon `obj_evac_hostage`, `post_breach_outside`, `ending_attacker` (three sets by position), `final_wave`, the technical's node path and `final_wave_enemies`; flags `vazquez_run_away_to_cliff_trigger`, `final_enemy_wave`; base `post_breach_player_blocker` | The lead objective until a player leaves the room (or 25 s). Then Ghost's cry, Vazquez runs for `vazquez_run_away_first`, the objective becomes "catch Vazquez" with his marker, the blocker opens, and the militia outside and each mercenary set spawn as a player nears them. On `final_enemy_wave` the technical drives in down its path and drops its riders, and `final_wave` joins. On `vazquez_run_away_to_cliff_trigger` he runs to `vazquez_run_away_cliff`. A Vazquez wedged for 5 s out of sight is moved on 400 units. |
| Ending | addon `sniper_shot_start`, `sniper_glint_org`, `sniper_sound`, `shadow_company_heli` ×3 and their structs | At the cliff: his surrender line, a scope glint across the water, `scn_estate_vazqueztakeout`, a `cheytac_silencer_so` round from `sniper_shot_start`, the blood fx and his death. `vazquez_dead` is set, the Mi-28s fly off to the far ends of their paths, the survivors stand down, then the three closing lines and mission complete through `_coopmission` (the result report and the lobby return of section 13). |
| Respawn | addon and stage checkpoints | A player who respawns comes back at the stage's checkpoint: the beach, `checkpoint_midhouse`, the road, `checkpoint_breach_player`, `post_breach_scene_node`, `vazquez_run_away_first`. |

**Shared-file edits** (kept minimal for the parallel mission work):
* `_coopmode.gsc` `Callback_StartGameType`: `_coopmission_estate::init()` and its `main()` thread, beside take fav's (two calls).
* `_coopmission.gsc`: `main()` skips `setup_flag_groups` when `level.coop_mission_owns_groups` is set (take estate's `guard_midhouse`, `reinforce_midhouse` and `patrol_prehouse` flags name groups that come only when the stealth breaks or a stage begins); `soldier_think` returns for a spawner marked `coop_no_hunt`.
* `_coopevents.gsc` `main()`: no `escape_watch` and no `objective_markers` when `level.coop_mission_owns_markers` is set. Take estate's `obj_house` marker and its interior "escape" volumes would otherwise nag along the route.
* `_cooptest.gsc`: `status` no longer faults on an ally without `coop_cast` (Ghost). The `estateroute` and `trace` verbs live in the mission script (`level.coop_mission_testcmd`).

### Not built yet, and why

* **The mission's own animations.** The breach reactions (`breach_react_*`), Vazquez's cowering and surrender (`estate_breach_hvt_cower*`, `estate_surrender_vasquez*`), the civilian cower set and the zone's smoking idles are only in `animlists/coop_take_mp_estate_tropical.animlist` (and `common_civilian`), which C1 does not load (section 10.1). The stand-ins come from `common_human`: `covercrouch_hide_idle` (cowering), `exposed_idle_reacta` (at the cliff), `patrol_bored_idle_smoke` / `_cellphone`.
* **Vazquez knocking Ghost down, and the hands-up surrender.** The same animlist limit. The lines and the run are kept.
* **The charge-placing hands and the door blowing in.** The hands are a client viewmodel sequence. The door here is the base map's brush, removed with the fx, rather than `breach_door_metal_right` swinging in.
* **Checkpoint restarts.** The retail retry reloads the checkpoint with a fade and the title card. Here a dead player respawns at the stage's checkpoint (coop lives, section 5.3); the world is not rewound.
* **Weapon drops.** The coop soldiers drop no weapons, so the recording's prompts to swap for an enemy's MP5K or Intervention do not occur. The safe room's authored pickups are there.
* **Ghost's leading.** The coop actors path over the MP bot graph. Ghost goes colour node to colour node and is caught up by teleport when left behind out of sight, rather than leading on IW4's colour-node paths.
* **The "follow" and "Vazquez" labels on the markers.** C1's objective label strings are not reachable through the objective builtins this server registers; the markers themselves are placed.
* **Stealth.** A reduction of IW4 `_stealth`: no awareness meters, no investigation of sounds, no corpse found after the 1.5 s window.
* **Not seen on a C1 client:** the boat seat's view, the title card's placement, the localized objective lines through `iPrintLnBold`, the charge frame's glow and angle on the door, the laptop on the counter, the slow motion and vision.

### Probe (port 17797, scratchpad home, developer 1, one test client, `estateroute`; the last run on 453bd86f with this patch)

Four fresh loads end to end; the last, on the final scripts:

* **Zodiac.** The client was seated (seat 0) and Ghost rode it (`codol_actorRide`, ent on the mover). The ride started after the 8 s wait, crossed the river on the water's surface (-594 to -518) and set the client down in the shallows. `estate_pri_0410` and `estate_gst_141` played, and the title card was shown. The forest patrols were spawned during the ride (3 + 3 + 3).
* **Stealth.** `_112`, `_113`; the beach patrol walked to its smoke and phone spots (flag `first_encounter_enemies_ready` set by its chain). When the client shot, Ghost took his man; the third saw the bodies and the group went loud. Then `_114`, Ghost to r3, `_115`, `patrol_start_ruinhouse`, `_116`, the ruin house pair and trio (`ruinhouse1_enemies_done` set when the pair fell, releasing the trio's chain), `_117`, the mid-house smokers spawned on `guard_midhouse`, `_120`, killed, `_140`, `_138`. In a run where the ruin house group went loud its `reinforce_ruinhouse` five came.
* **House.** `_146`, `_150`, `patrol_prehouse`: seven out of the house with Vazquez, `_147`, `_148`, `_151`, loud, `estate_vaz_04`, `_152`, `_153`, `estate_vaz_06`; Vazquez ran in and `vazquez_out_of_sight` was set. `in_doorway`, `in_house`: three kitchen guards and five more; then the cell guards.
* **Intel.** The laptop on the counter, `_154`; the client walked up and pressed use: taken for real (`keyboard_used`).
* **Breach.** `estate_pri_0461`, the charge, the door, slow motion, four guards and Vazquez in the room; `room_1_cleared`. The client's use at the door did not register (it was set down on the frame's position by a STUCK walk), so the breach press was SIMULATED.
* **Chase.** `_158`, `estate_vaz_08`, the walk out, `_159`, `estate_pri_0470`, `_160`. Vazquez ran out of the safe room's east door on his own feet to r10 and r1, was put on r8 and on `vazquez_run_away_first` (no progress for 4 s), and ran to r5 and the cliff on foot after `vazquez_run_away_to_cliff_trigger`. The militia outside and the three mercenary sets spawned as the client came near.
* **Ending.** `estate_vaz_09`, the sniper, `vazquez_dead`, the Mi-28s away, `estate_pri_0474`, `_163`, `estate_pri_0477`, MISSION COMPLETE; then `unlock flags 637|645|666`, the lobby return and the parked server (section 13).
* No script runtime error and no missing sound alias in the last run. Two runtime errors were found on the way and fixed: `ammo_box_762_col` is no alias (every sound is now checked with `soundExists`), and `_cooptest status` faulted on Ghost (no `coop_cast`).
* **What was simulated.** The test client's straight-line walker cannot follow the estate's paths, stairs and clip, so most walks between stages ended STUCK and teleported on. Of 27 enemies the route shot at, 9 fell to the client's own aimed rounds and 18 were killed by the harness after six rounds (SIMULATED). The breach use was SIMULATED once. Enemy fire, Ghost's fire (`mp5_silencer_so` at the militia), the patrols' walks and Vazquez's run were the actors' own. The test client was kept at full health (`godmode`) but still died twice in heavy fire and respawned at the stage's checkpoint, as designed.
* MP and coop static gates: `script_resolve.py --category coop --root .../coop --root ... --seed maps/mp/gametypes/coop.gsc` and `--only maps/mp` both pass.

---

### Take estate, second pass: the user's run against the retail recording (2026-10-09)

* **The user's run** (session recording 2026-10-08, 1085-1196 s; log from line 23299) ended on the beach. The zodiac, title card, opening lines, landing and first patrol played, then the user walked into an invisible wall at the top of the beach, Ghost stood wedged at the same spot, and the user left.
* **The wall.** The base maps' `script_specialops 0` entities exist only outside Spec Ops (IW4 `_specialops_code::specialops_remove_flag_check` deletes them), and coop kept them. On estate, *190 is a clip wall across the top of the beach, (-5033 2423 -505)..(-4927 2665 1217); *10 (villa roof), *7 and *20 (villa) and *51 (road) are the others. `_coopevents::cleanup` now deletes every such entity on every coop map except killhouse's `minimap_corner` origins (on crash they are the S&D bomb sites and their triggers; none is used by a coop script). With them gone, a test client walking the map's own path nodes reaches the villa front from the landing.
* **Ghost's way.** The beach is off the multiplayer bot graph (nearest waypoint 725 units away), so the native planner sent Ghost into the bank. He now walks A* routes over 617 of the base map's own SP path nodes (`_coopmission_estate_nav.gsc`, generated; links by distance, checked with sight traces at +32 and +48, a closed link retried after 15 s). He rides a mover up the beach at the patrol walk's pace with that walk playing; if he makes no progress he is put on his next node (3 s unseen, 8 s seen). Colour triggers only move him forward. Aboard the zodiac he crouches (`casual_crouch_idle`).
* **The look.** `setExpFog( 96, 1100, 0.63, 0.66, 0.67, 0.86, 0 )` gives the recording's grey river fog (`scr_coop_estate_fog` overrides it, "0" turns it off). The createfx ground fog still needs `coopClientMapName`.
* **T-pose sweep.** None: every animation the mission names is a whole pose in common_gametype_coop and resolves.
* **Still open.** A "Missing FX" box where an actor's round hits (native; see the crash pass); no marker labels; the flow keeps running after every player has left; off-graph script goals in the native planner (`ActorGoalDirection` routes a goal nearer than the graph's start node through that node; proposed, not built).
* **Probe** (17780, test client, `estateroute`): beach to landing on the ride in about 17 s; r2 to r3 walked with no rescue; MISSION COMPLETE and unlock flags 637|645|666. Later stages were driven by harness teleports. Not seen on a C1 client: the fog, the crouch in the boat, the walk up the beach, the removed wall.

## Mission coop_defense_mp_crash_tropical ("Crash Site"), from the retail recording "Campaign Walkthrough Part 3" (2026-10-08)

**The guide.** A public gameplay recording of the retail CoDOL client (Chinese build) playing the third campaign video. It is the authority for this mission's order of events, cast, places and moments, as section 14's recording is for the training.

* The frames were viewed in the scratchpad only. Nothing from the recording is in the repo, and no audio was used.
* Lines were placed from the subtitles C1 draws on screen, matched to the subtitle each sound alias carries in the mission's zone (M3.3). They are paraphrased here, never quoted.
* Times are recording time. 0:00-0:58 is the lobby and the loading screens; gameplay runs 1:00-7:30; 7:40 is the lobby's unlock card; 8:00 is the campaign map with the next mission (the safe house) selectable.

### M3.1 Which mission

* **The map.** The tropical town of `mp_crash_tropical`: shanty alleys with wooden fences, white pickups, rubble, and the crashed helicopter in the middle of the town. The corner map is the mission's own (`compass_map_mp_crash_sp`).
* **The HUD.** The title card is the zone's `COOP_DEFENSE_MP_CRASH_TROPICAL_INTROSCREEN_LINE_0..2` (the mission's name, a date with a running clock, the region). The objectives are the zone's `_OBJ_PAUSE_FOLLOW` (follow Ghost to Vazquez's office), `_OBJ_PICK_UP_FLASH`, `_OBJ_THROW_FLASH`, `_OBJ_PICK_UP_CLAYMORE`, `_OBJ_PLACE_CLAYMORES`, `_OBJ_START_DOWNLOAD`, `_OBJ_PAUSE_FOLLOW_LZ`, `_DESTROY_HELI_OBJ`, `_OBJ_PAUSE_ROOF`, and the download HUD is the common coop zone's IW4 DSM set (`PMC_DSM_*`, `PMC_DOWNLOAD_COMPLETE`) with the zone's `_SIGNAL_STRENGTH`.
* **The unlock card** after the mission names the safe house (take estate) as the next mission.
* **So it is `coop_defense_mp_crash_tropical`**, chapter 1 mission 2 (challenge 802, unlock 654 for take estate; section 13.1). The probe's completion logged `unlock flags 631|636|654`.

### M3.2 Timeline (what the recording shows)

| Time | Where / what | Lines (alias) | HUD |
|---|---|---|---|
| 1:00-1:04 | Black: the title card | | mission name, date and clock, region |
| 1:05-1:09 | The rookie lies on his back outside, in daylight, under palm trees and power lines; a mercenary in a mask and camo stands over him | a thug: crossing Vazquez was a mistake (`crash_car1_269`) | |
| 1:10-1:15 | Black | | |
| 1:16-1:27 | An upper-floor room with a long row of windows. The gang's boss (camo jacket, bandolier; his subtitles name him "mercenary boss", his aliases are Ortiz's) is on the phone with his back turned; three or four armed men around the room. He turns to the rookie. | the boss on the phone: yes boss, I'll see to it myself (`crash_ortiz_04`); Vazquez's message: goodbye (`crash_ortiz_05`) | |
| 1:27-1:30 | A white flash: Ghost's flashbang. The men stagger, crouching. | | |
| 1:30-1:34 | The rookie has an M4A1 (M203, holographic sight) and fires; Ghost comes in shooting | Ghost: catch -- in 141 nobody is left behind (`crash_gst_12`) | |
| 1:36-1:39 | The room's door, then Ghost leads down the stairs | Price (radio): what is Vazquez doing with the Kortez cartel, we need intel (`crash_pri_0293`) | objective: follow Ghost to Vazquez's office; a "follow" marker over Ghost |
| 1:40-1:50 | Dark ground-floor rooms behind Ghost | Ghost: he saw the office on the way in, heavily guarded (`crash_gst_14`); grab your gear, let's go (`crash_gst_15`) | |
| 1:52-2:10 | Out into the street: fenced alleys, a red car; militia come at them | Ghost: here they come, flank them, I'll cover (`crash_gst_16`); not bad, keep up (`crash_gst_17`) | |
| 2:15-2:19 | Three dogs by a white pickup | Ghost: dogs -- kill them, snap the neck if one gets you (`crash_gst_24`) | |
| 2:20-2:40 | North up an alley (white pickup, oil drums); another wave | Ghost: let's go (`crash_gst_45`); another wave, cover (`crash_gst_18`); looks like you can shoot, keep up (`crash_gst_20`) | reload hint |
| 2:42-2:56 | Men on the rooftops and walls; a car burns and explodes; black smoke columns over the town | Ghost: keep moving, we need the intel (`crash_gst_21`) | |
| 3:00-3:06 | A street with parked cars and the office building | Ghost: office on the second floor, heavy resistance (`crash_gst_47`); Price: worth protecting then, go and look (`crash_pri_0327`) | |
| 3:08-3:12 | Ghost crouches at the office's corner; his backpack glows | Ghost: take flashbangs from my pack (`crash_gst_48`) | hold use for flashbangs |
| 3:12-3:15 | The doorway's frame glows yellow, a floating label says to throw a flashbang there | Ghost: now throw it in and look away (`crash_gst_50`); flash before you go in (`crash_gst_51`) | throw-flashbang hint |
| 3:16-3:24 | Inside: dazed men, one crouched by a chair | Ghost: office on the second floor, go up (`crash_gst_57`) | aim-down-sights hint |
| 3:26-3:34 | Upstairs, the claymores glow on the floor; an enemy SCAR-H lies there | Ghost: there, a laptop with a wireless card (`crash_gst_58`); Price: lay a defence before connecting (`crash_pri_0339`) | label over the claymores; pick-up hint |
| 3:36-3:44 | Two "place claymore" labels at two doorways | Ghost: claymores at the doors for our flanks (`crash_gst_59`); one more (`crash_gst_61`) | press the tactical key |
| 3:46-3:52 | The laptop glows, a "start download" label over it | Ghost: once connected they'll know where we are (`crash_gst_62`) | objective: obtain the enemy's intel |
| 3:54-4:10 | The DSM starts: initialising, connecting, skipping the login, locating files, each to 100% | Ghost: they're coming; stay near the computer or the transfer breaks (`crash_gst_65`); Price: signal, initialising (`crash_pri_0343`) | DSM lines top right |
| 4:10-5:02 | Files copied counting up to about 30, signal strength beside it. Attacks up the stairs, from the garage across the street, the east ladder; the rookie swaps weapons off the floor (M4A1, RPD, Model 1887, Ranger) | Ghost: on the stairs; garage across the street (`crash_gst_68`); good shot (`crash_gst_38`); Price: data is fine, hold (`crash_pri_0353`); shipping manifests -- what are they moving (`crash_pri_0354`); nearly done (`crash_pri_0351`) | files copied / total; signal strength, red at 16% when the rookie strays |
| 5:04-5:10 | | Ghost: nice work, kid, back to the landing zone (`crash_gst_93`); Price: download complete (`crash_pri_0364`) | download complete; objective: follow Ghost to the extraction point |
| 5:16-5:30 | South through the town to the crashed helicopter; men search the wreck | Ghost: enemies checking the wreck (`crash_gst_94`) | objective: destroy the helicopter, label over the wreck |
| 5:36-5:44 | Firefight at the wreck | Ghost: look around, there must be something to destroy it with (`crash_gst_99`) | |
| 5:46-6:06 | Markers lead to a shop full of lamps (the enemy's supply depot); its defenders; a glowing suitcase on the counter | Ghost: there, take the explosives and go plant them (`crash_gst_106`) | hold use to pick up the explosives |
| 6:10-6:35 | Back to the wreck under fire | Price: set the timer, then fire a flare from the roof, that's the extraction (`crash_pri_0404`); Ghost: good shot | |
| 6:35-6:46 | The suitcase bomb opened on the wreck, the plant bar fills | | hold use to plant |
| 6:47-7:11 | A run to the roof of the building east of the wreck: close fights in its doorway, up the stairs | Ghost: go go go (`crash_gst_107`); to the roof now (`crash_gst_108`) | objective: get to the roof, distance marker |
| 7:13-7:20 | (cut) First person, the rookie lights a green flare on the roof; green smoke | Price: good work, valuable intel (`crash_pri_0408`) | |
| 7:21-7:29 | The wreck explodes behind; a Black Hawk flies in over the roof | | mission complete |

**Cast.** Ghost is the only ally: skull balaclava, `body_hero_ghost_socom` + `head_hero_ghost_soccom`, a "follow" marker over him on the move. Price is a radio voice. The enemies are militia (`body_militia_*`: shirts, masks, rifles, LMGs, SMGs, shotguns), a few in black with gas masks during the download, three dogs, and the boss in the room.

**Weather / vision.** Bright, humid tropical daylight with drifting clouds, black smoke columns over the town. The mission's vision (`vision/coop_defense_mp_crash_tropical.vision`, applied by `_coopmode::levelMain`); the createfx (fires, smoke, dust) is client-side.

### M3.3 Voice lines: how each alias was read

* The playlist zone carries 129 subtitled aliases (`crash_car1_*`, `crash_gst_12..109`, `crash_ortiz_01..05`, `crash_pilot_01/02`, `crash_pri_0293..0408`). Each alias names a subtitle key `SUBTITLE_CODO_CRASH_*`.
* The zone's localize entries for those keys store their name as a pointer into the sound section, not as a string. The pointers were turned into aliases by walking the sound records in zone order: each record is 180 bytes in memory (188 for a 14-character name), plus the speaker-map blocks of `crash_car1_267` (636 in all) and `crash_pilot_01` (+424). With those sizes all 108 localize entries land exactly on an alias. (The aliases are not always named for their speaker: the boss's lines are `crash_ortiz_04/05`, the thugs' are `crash_car1_*` and `crash_gst_100/101`.)
* The zone also ships a second, older subtitle set (`SUBTITLE_COOP_DEFENSE_MP_CRASH_TROPICAL_*`, a "Silver Eagle" radio voice and a DSM defence at the wreck); no alias names it and the recording never shows it.
* A line's length on screen is INFERRED from its subtitle length (about five characters a second); the sound files were not used.

### M3.4 The map's own entities (MEASURED, the addon MapEnts) and what each became

| Addon entities | Used for |
|---|---|
| `player_interrogation_start`, `player1_3rd_person_idle`, `drag_players` | the rookie's places in the intro |
| `intro_dragger` spawner | the masked man over the rookie |
| `intro_ai_puncher_spawner` (makarov role) at `loc_punch_scene_start`; `intro_ai_watcher01..04_spawner` at `loc_ai_watcher01..04` | the boss (Ortiz's `body_airport_assault_a_clean`) and his four watchers |
| `actor_ally_hero_ghost_socom_so` (`script_friendname Ghost`), `price_flash_point`, `magic_flashbang` | Ghost and his flashbang |
| base-map `price_door` (two halves) | the door Ghost opens between the building's two ground-floor rooms |
| `post_intro_checkpoint_*`, `pre_download_checkpoint_*`, `post_download_checkpoint_*`, `player0/1_crash_end`, `4_coop_start_0*` | respawn points for each stretch |
| `group_a`, `old_group_b`, `group_c`, `group_g` + `group_j`, `group_h` | the waves on the way to the office (INFERRED from their positions along the route and the recording) |
| `group_d` (lean/smoke/phone idles, structs `me0_auto3383/3387/3406/3407`), `throw_flash_window_obj`, `get_flash_bangs`, `player_entry_room` | the office: the occupants before the flash, the window, Ghost's pack |
| `claymore_pick_up_objective`, `pick_up_claymores` (5 models), `claymores_trigger`, `claymore_place_objective` x2 + their `weapon_claymore_bombsquad` | the claymores |
| `the_laptop`, `the_laptop_obj` (radius 800), `me0_auto3366` | the laptop and the signal radius |
| `group_wave_1..3`, `callout_*` (AI-only volumes) | the attack on the office and Ghost's callouts |
| `group_investigate_heli` (crouch/stand search structs), `investigate_heli_volume`, `destroy_heli_obj` | the searchers at the wreck |
| `bomb_site01` | the lamp store's explosives |
| `bomb_plant_site` (`prop_suitcase_bomb_bombsquad`) | the plant spot on the wreck |
| `group_e`, `group_f`, `group_i` | the counter-attack at the wreck and the run to the roof |
| `trigger_gathering_onRoof`, `flare_wave`, `smoke_point`, `chopper_end` on `so_chopper_end_path` | the roof, the flare, the extraction helicopter |
| Not used | `group_b` (dogs), `removed_group_f`, `old_*`, `ortiz_heli`, `wounded_spawner` / `npc_1/2` (US Army allies the recording never shows), `crash_ch46e` (the prologue's helicopter), `bridge_collapse`, `price_kick_door` (no door is kicked in the recording), the sentry-gun / turret strings of the older defence set |

### M3.5 What was built (`coop/maps/mp/gametypes/_coopmission_defense_crash.gsc`)

Selected by `sv_sessionMap == "coop_defense_mp_crash_tropical"`. It sets `level.coop_mission_flow` (no generic group waves any more), `level.coop_mission_owns_groups` and `level.coop_mission_owns_markers`, and `level.coop_mission_testcmd` for its probe verbs. Enemies come through `_coopmission::spawn_soldier`; the end is `_coopmission::mission_complete` (section 13's report and lobby return); lives and failure are `_coopmode`'s.

* **Intro.** Players are frozen with no weapons. The title card (the zone's strings) on black; the rookie prone at `drag_players` looking up at the dragger (`crash_car1_269`); black; the interrogation room, the boss on the phone (`patrol_bored_idle_cellphone`) with four watchers (`crash_ortiz_03`, `_04`, then he turns, `_05`). The flash (`explosions/flashbang`, `flashbang_explode_default`, `shellShock flashbang_mp_scripted` on the players; the men drop to a crouch). Ghost appears at `price_flash_point`; the players get the rifle (`m4m203_eotech_so`), a pistol and three frags (`scn_crash_player_catch_gun`, `crash_gst_12`). The boss runs for the stairs and is gone; the watchers recover and fight.
* **Ghost** is a friendly soldier-species actor (the training's ally template), invulnerable, with the look-only nametag (`_cooptraining::show_nametags`). He runs (`run_lowready_f`), stands in the alert idle, and fires bursts at the nearest enemy he can see inside 1400 units (`magicBullet` with his role weapon, 30% hits, a burst every 0.6-1.4 s, held when a player or an ally is in the line), so the rookie does most of the killing, as in the recording. On the town's streets he paths over the bot waypoint graph (`setGoalPos`, placed when the planner makes no headway for 5 s); inside the intro building, where the graph is thin, he rides an invisible mover along hand-measured points (stairs and rooms, MEASURED with `colmap` / `floors`) with `codol_actorRide`. He waits for a player within 450 units at each point.
* **The route** follows graph nodes 7, 21, 23, 18, 305, 19 (the dogs' yard), 28, 37, 38, 39, 98..95, 78, 77, 48, 54 to the office, releasing a group at each stretch with the recording's lines; each group is sent at Ghost's position, so its men come out of the houses (IW4 gave them goal volumes from the unshipped level script). A group must be cleared before Ghost moves on, except the men on upper floors and roofs: up to two may stay up, and at the rooftops and the office's street Ghost leads on after 25 / 30 s whoever is still up, as he does in the recording. After a time-out the rest are marked on the compass and sent at the players; a straggler stranded more than 900 units from everyone is removed, and the rest are removed at 2.2 times the time-out so the mission cannot stall.
* **Office.** `group_d` spawns unaware in its authored idles (`parabolic_leaning_guy_smoking_idle`, `patrol_bored_idle_smoke`, `patrol_bored_idle_cellphone`). Flashbangs are a hold-use at Ghost's pack (or the map's `get_flash_bangs`); the throw is a real `grenade_fire` of the flashbang (30 s time-out); the room is "flashed" and wakes 2 s later.
* **Download.** Claymores are a hold-use at the crate (or `claymores_trigger`); placing is a real `grenade_fire` of `claymore_so` within its spot's radius (45 s time-out). The laptop is a hold-use; the DSM HUD is IW4 `so_download_arcadia`'s (frame, the four start-up lines counting to 100%, files copied / total, 30-33 files), with the signal strength from the nearest player's distance to the laptop (red under 30%). Leaving the 800-unit radius for 6 s interrupts it (`PMC_DOWNLOAD_INTERRUPTED`, `crash_pri_0355`, resume at the laptop). 70 s of connected time completes it. The three waves are sent at the laptop; Price's progress lines play at 30/50/65/85/93%; Ghost calls out a wave entering a callout volume.
* **Wreck.** `group_investigate_heli` spawns unaware on its search structs and wakes when a player comes within 700 units. Then the lamp store: a glowing suitcase (`prop_suitcase_bomb_obj`) at `bomb_site01`, guarded by six of `group_f`, picked up with a 1 s hold (progress bar). Back at the wreck (six of `group_e` counter-attack), `bomb_plant_site` glows; a 3 s hold plants `prop_suitcase_bomb` (`mp_bomb_plant`, `bomb_tick` loop), on a 40 s timer.
* **Roof.** `group_f` and `group_i` spawn; Ghost runs there (graph nodes 102, 103, 104, 170). A player on the roof (`trigger_gathering_onRoof` or within 140 units) ends it: controls frozen, `scn_crash_tropical_grab_flare`, the green flare and smoke at `flare_wave`, the extraction Black Hawk (`vehicle_blackhawk_minigun_hero`) flies `so_chopper_end_path` and settles over the roof, the wreck blows (`explosions/helicopter_explosion`, `exp_suitcase_bomb_main`, an earthquake), `crash_pri_0408`, **mission complete**.
* **The actor budget** is raised to 14 alive. When a stretch begins, men an earlier one left behind (on roofs, upper floors, or stranded by the waypoint graph) farther than 1500 units from the players (1200 at the wreck, 700 when the download starts) are taken out of the scene, and a group waits up to 8 s per soldier for a free slot, so the download's three waves come at full strength (MEASURED: 6, 3, 0 before, 13, 11, 10 after).
* **Respawns** return a player to the stretch's checkpoint with the kit he has earned (rifle, pistol, frags, then flashbangs and claymores). Developer start points: `+set scr_coop_crash_skip 2..5` (office, download, wreck, roof). `+set scr_coop_crash_download_time` changes the 70 s.
* Every sound is checked with `soundExists` before it plays (a missing alias is a script runtime error).

**Shared files touched (minimal):**

| File | Change |
|---|---|
| `_coopmode.gsc` | `_coopmission_defense_crash::init()` after take estate's `init()`; `level thread ..::main()` after take estate's `main()` |

### M3.6 Not built, and why (closest substitute in brackets)

* **The dogs** (`group_b`, `actor_enemy_dog_so` x3): there is no coop dog species (section 7). [The yard's `old_group_b` soldiers; Ghost's dog line is not played.]
* **The scripted scenes' animations**: the drag (`scn_crash_intg_drag`), the punch and phone sequences (`intro_punch_sequence`, `scn_crash_intg_*`), Ghost's peek and throw (`crash_flash_price_peek` / `_throw`), the gun throw and catch, the flare grab. Their xanims are listed only in `animlists/coop_defense_mp_crash_tropical.animlist`, which C1 does not load (section 10.1). [Common-list idles, the zone's sounds where they ship, and the shellshock.]
* **The third-person intro** (the rookie on the floor seen from outside) needs a scripted camera. [The rookie's own view, prone, frozen.]
* **The floating labels** on the markers ("follow", "throw a flashbang here", "place claymore", "destroy the helicopter") are C1 objective label strings this server cannot reach (as take estate found). [A 3D waypoint (`objpoint_default`, distance shown) plus the compass objective; the label text is the objective line at the top.]
* **The glow** on Ghost's backpack, the doorway frame and the claymores: `body_hero_ghost_socom_obj` and `weapon_claymore_obj` ship, but a body swap on a live actor resets his attachments. [The waypoint marks them; the laptop and the plant spot glow with their own `_obj` / bombsquad models.]
* **The key hints** use the zone's strings with key tokens (`[{+activate}]`, `[{+smoke}]`, `[{+tact}]`) in a HUD text element. NOT seen on a C1 client whether those tokens expand outside the hint HUD.
* **Weapon drops**: coop soldiers drop no weapons, so the recording's swaps (SCAR-H, RPD, Model 1887, Ranger) do not occur.
* **Enemy grenades and the burning car** (2:52): no AI grenades in coop, and no destructible car is scripted. [The map's own destructibles still break when shot.]
* **The plant animation** (the suitcase opened on the wreck in first person) is `briefcase_bomb_mp`'s view model, an MP weapon with no coop bomb-zone logic. [A hold-use with a progress bar and the plant sound.]
* **Line lengths** are read off the subtitles, not the sound files.

### M3.7 Probe (port 17795, scratchpad home, developer 1, one test client, `crashroute`)

* **Script checks.** `script_resolve.py --category coop --root .../coop --root ... --seed maps/mp/gametypes/coop.gsc` and `--root bin/home/codol_rawfiles --only maps/mp` pass. No script runtime error and no missing sound alias in the last five full runs. On the way, a harness `0x28` literal (the compiler has no hex literals) and an `int( undefined )` were found and fixed.
* **The `crashroute` verb** drives player 0 through the published waits (`level.crash_step`): it walks (`botMoveTo`) to each point and teleports when the straight-line walker is STUCK, presses the real use button (40) at pickups, presses the real special-offhand button (0x8000) at the flash window and the equipment button (0x10000) at each claymore spot, and fires real rounds at the nearest enemy it can see.
* **Last run (8d78582e + this patch, one client, fresh load):**
  * **Intro.** Title card, the dragger and `crash_car1_269`, the room with the boss and four watchers (`crash_ortiz_03..05`), the flash, Ghost at `price_flash_point`, the kit (`m4m203_eotech_so`, `usp_silencer_so`, three frags), `crash_gst_12`; the watchers fell in about 5-8 s.
  * **The building.** Ghost walked to the stairs (1403 -1982 202), down them to (1656 -1815 66), opened `price_door` (2 halves) and walked out to the street (1599 -1331 67), with `crash_pri_0293`, `crash_gst_14`, `_15`.
  * **The route.** `group_a` (cleared in 11 s), the yard (`old_group_b`, 6 s), `group_c` (three stragglers removed at 64 s), the rooftops (Ghost moved on after 25 s with five still up), `group_h` (16 s), in order, each with its lines; Ghost reached every graph point on foot.
  * **Office.** Flashbangs from Ghost's pack; **the test client's flashbang throw was real** (`threw flash_grenade_so`); the room woke; `crash_gst_54`, `_57`.
  * **Download.** The claymore pickup by the real use button; **both claymores placed for real** (spot 0 at (-692 1860 410), spot 1 at (-771 2156 392)); the laptop by the real use button; the DSM start-up lines and 31 of 31 files in 70 s; all three waves at full strength (13, 11, 10) after the leftovers were cleared; the callouts (`crash_gst_68`, `_69`, `_70`, `_87`..`_89`) as the waves crossed their volumes; Price's progress lines; `crash_gst_93`, `crash_pri_0364`.
  * **Wreck.** The searchers woke as the client came and fell in 7 s; `crash_gst_99`, `_95`; the lamp store, the explosives picked up by the real use button, `crash_pri_0404`; the plant at `bomb_plant_site` by the real use button.
  * **Roof.** `group_f` / `group_i` spawned, `crash_gst_107`, `_108`; the client on the roof; the flare, the Black Hawk on `so_chopper_end_path`, the wreck destroyed, `crash_pri_0408`, **MISSION COMPLETE**, `unlock flags 631|636|654`, the lobby return and the parked server (section 13).
  * A two-client run (bot1 standing in at full health) placed both clients in the intro and reported the result for both.
* **SIMULATED / harness limits:**
  * The straight-line walker cannot follow the town's turns, stairs or the building interiors, so most legs ended STUCK and were teleported (the log names each one).
  * The test client killed few enemies with its own rounds (4 of 89 in the last run); Ghost and the stragglers' removal did the rest, so a run took 20 minutes against the recording's 6.5. With a player shooting, each stretch ends as soon as its group is down.
  * The harness logs a leftover "use SIMULATED" line after a pickup that the real button had already completed; nothing was forced in those runs.
* **NOT seen on a C1 client:**
  * the title card, the objective line and the DSM HUD (localized strings in `newHudElem`s), and whether the key tokens in the hint strings expand;
  * the 3D markers (`objpoint_default` waypoints) and Ghost's look-only nametag;
  * Ghost's ride down the stairs (the run pose on the mover);
  * the intro's frozen, prone view, the flash and the shellshock;
  * the door swing, the suitcase models and the plant bar;
  * the Black Hawk's path and the wreck's explosion.

---

### M3.8 Second pass: the user's playthrough against the recording (2026-10-09)

Compared at 5-second keyframes (session recording 11:30-18:00 against the retail recording 0:55-8:05), with the server log. Both play every phase in order with the right lines (337 s against about 390 s).

* **Ghost T-posed when idle.** `exposed_idle_alert_v1` (and `exposed_crouch_idle_alert_v1`) is built additive in common_gametype_coop (the xanim header's additive flag, as for `exposed_shoot_*`); as an actor's only animation it leaves the bind pose. Ghost now stands in `exposed_aim_5` and crouches in `exposed_crouch_aim_5` at the office corner until the flash. The same fix went into take fav's Ghost and training's SAS idles.
* **The office** occupants wake only to a shot or a player within 72 units (was 160), so they no longer open fire on the doorway before the flash.
* **The DSM count and signal** get values when created; no more "&&1%" on the first frame.
* **The ending** waits for the Black Hawk over the roof (11.3 s after the flare on the probe, 14 s cap), with the frozen players' view easing toward it.
* **The wreck searchers** spawn halfway to the wreck if the actor budget blocked them at the office (probe: 0, then 5 of 5).
* **Shared fixes from this pass:** `soldier_jump::main` takes the native traversal's four arguments (it took none: a fatal "too many parameters" on any graph with negotiation links); the success banner is the localized `SCRIPT_MISSION_COMPLETE` (none after the training).
* **Still open:**
  * a "Missing FX" box wherever an actor fires or its rounds land (native; the weapons are inside the published window, wire 5/6/7/16; lead C1 0x481bf8 -> 0x444d80 and the client-side impacts for an eType 18 shooter);
  * no 3D marker labels (C1 gates waypoint drawing on flags 0x208);
  * no dogs (no coop dog species);
  * the intro boss's body and head.
* **Probe** (17779, `crashroute`): one full run, two office-start runs, the download 31/31 in 70 s, no script error. Not seen on C1.

## Coop AI combat: damage, fire events, gun models and collision (2026-10-08)

User report: "the actors in the coop mode currently do too much damage too quickly, they have no muzzle flash, shooting sounds or anything like that. They sometimes clip through walls too." This applies to every coop mission, because all of them spawn their soldiers through `_coopmission::spawn_soldier` and the soldier animscripts.

### Damage and fire rate (regular difficulty)

**Before (MEASURED, probe, `_cooptest duel`).** One soldier against a standing test client in the open:
* AR at 1200 units: dead in 2.5 s from three hits (raw 97 each, 40 after the old 0.5 scale and 40 cap).
* SMG at 400 units: dead in 6 s (three hits).
* AA12 shotgun at 300 units: dead in 0.15 s, four pellets of one blast (25 each).
The old model was `self.accuracy` halved at the edge of the band, bursts of 3 to 5 with a 0.3 to 0.9 s pause, every non-player hit at 0.5 scale capped at 40, and regeneration 5 s after the last hit.

**Now.** This follows CoD4/IW4 `maps\_gameskill.gsc` on regular.

`soldier_combat.gsc` (the AI side):
* **Bursts per class, by range.** AR: 3 to 5 rounds under 600 units, 2 to 4 under 1100, single shots beyond, at 0.1 s per round. SMG: 4 to 7 / 2 to 4 at 0.075 s. LMG: 5 to 10 at 0.09 s. Shotgun and pistol: 1 to 3. Sniper: single shots. Pauses between bursts: AR 0.5 to 1.1 s, SMG 0.4 to 0.9 s, LMG 0.6 to 1.2 s, shotgun 0.9 to 1.5 s, sniper 2 to 3 s (regular `min/max_sniper_burst_delay_time`).
* **Hit chance per round** is `self.accuracy × scr_coop_ai_accuracy (1.6) × class (LMG 0.65, SMG 0.85) × range × time on target × stance × movement`.
  * Range: 1 inside the class's near distance, falling linearly to `scr_coop_ai_far_accuracy` (0.25) at its far distance.
  * Time on target: no hit during the first `0.3 + 0.0004 × distance` seconds of an engagement, then a ramp from 0.4 to 1 over 3 s.
  * Stance: crouched 0.75, prone 0.5.
  * Movement: down to 0.5 at sprint speed.
* **Misses never hit.** They are aimed 30 to 64 units wide of the chest, or (35%) into the ground 30 to 72 units beside the feet, and carry on 384 units past, so they crack by and hit the wall or dirt near the player.

`_coopmode.gsc` (the player side, AI bullets only):
* Damage per hit is raw × `scr_coop_ai_damage_scale` (0.18), capped at `scr_coop_ai_damage_cap` (30). The head multiplier is taken out, so a body or head hit from an AR is 18.
* Invulnerability windows (regular `invulTime_*`):
  * 0.35 s after a worthy hit (at least 10% of max health);
  * 0.5 s on the hit that first puts the player in the red (20%);
  * 0.3 s after that.
* A would-be kill from above 1 health leaves the player at 1 and untouchable for 1.7 s (`player_deathInvulnerableTime`), once per trip into the red.
* Regeneration starts 2 s after the last hit, or 5 s after a hit that put the player in the red, and refills the bar in 2 s.
* Friendly AI rounds (the cast, `coop_friendly`) never hurt a player. Non-bullet, non-player damage keeps the old 0.5 scale and 40 cap.

Every number is a dvar: `scr_coop_ai_*`, `scr_coop_invul_*`, `scr_coop_worthy_ratio`, `scr_coop_red_cutoff`, `scr_coop_death_grace`, `scr_coop_regen_*`.

**After (MEASURED, the same duels, final scripts).** One soldier, test client standing still in the open unless noted:

| soldier | distance | result | hits taken | shots / bursts | soldier's aimed hits |
|---|---|---|---|---|---|
| AR | 600 | dead in 10.8 s | 7 | 29 / 8 | 11 |
| AR | 600 (crash spec) | dead in 16.5 s | 7 | 31 / 11 | 10 |
| AR | 1000, crouched (crash spec) | alive after 30 s, health 100 | 8 | 61 / 20 | 19 |
| AR | 1200 | alive after 30 s, health 97 | 5 | 39 / 25 | 5 |
| SMG | 400 | dead in 10.9 s | 7 | 47 / 7 | 19 |
| LMG | 800 | dead in 9.2 s | 6 | 48 / 5 | 6 |
| AA12 | 300 | dead in 6.4 s | 2 blasts | 6 / 3 | 2 |
| sniper | 1500 | alive after 30 s | 2 (30 each) | 10 / 10 | 2 |

Notes on the table:
* At 1000 and 1200 the player heals between hits.
* Misses on crash spec: 21 of 31 for the AR at 600, 8 of them into the ground, aimed 55 units from the chest on average.
* The cadence on the wire (actor fire events, crash spec):
  * AR: bursts of 1 to 4 with in-burst gaps of 100 ms and pauses of 0.8 to 1.3 s.
  * LMG: bursts of 3 to 10 with gaps of 83 ms and pauses of 0.9 to 1.4 s.
  * Shotgun: single blasts.

### Muzzle flash, fire sound, tracer

**Root cause (MEASURED in C1.exe and with `codol_traceFireEvents`).**
* `magicBullet` puts `EV_FIRE_WEAPON` on a temporary event entity at the muzzle point. The snapshot writer never published that entity's weapon word (+d4). The trace showed "type 47->51 event 26 native weapon 29 wire weapon 0".
* C1 `CG_FireWeapon` (0x481f50) returns at once on weapon 0. So there was no sound, no flash, nothing.
* Even with the word, the flash placement switch (0x481b10, by eType) has no arm for a temporary entity.
* For an actor (eType 0x12), C1 plays the weapon's world flash effect (WeaponDef+0x360) on the actor's DObj at the flash tag (0x481bf8 → 0x444d80) and the fire sound from the actor (0x481d60). The actor DObj builder (0x428b40) reads the same weapon word.
* The impact events name the shooter, and C1 draws the tracer from the shooter's flash tag.

**Fix.**
* New coop-only actor methods (`src/online/player/actor_fire.c`, registered in `actor_lifecycle.cpp`):
  * `codol_actorShoot( weapon, start, end )` follows IW4 `Actor_Shoot` / T5 `Actor_ShootWeapon`. It calls `Bullet_Fire` with the actor as attacker and weapon entity, which passes the actor's own hit boxes and makes the impacts name him. It then calls `G_AddEvent( actor, EV_FIRE_WEAPON, 0 )`, with the actor's `s.weapon` naming the weapon.
  * `codol_actorSetWeapon( weapon )` sets that weapon at spawn.
* `engine_state.c` publishes an actor's weapon word through a filter. Only the coop fire registers the filter, so mp dogs and zm zombies, whose melee also writes `s.weapon`, are unchanged.
* `sv_gameframe.c` publishes a temporary `EV_FIRE_WEAPON`'s weapon word in coop matches (scripted `magicBullet`s such as the estate sniper).
* The scripts now fire this way: `soldier_combat.gsc`, Ghost in defense crash and estate, the take fav gunmen (informants and the technical) through `_coopmission::actor_fire_round`.

**MEASURED, on the wire.** Every soldier and Ghost fire event is eType 18 (actor) → 18, event 26 (C1 30), with a nonzero wire weapon. Counts:
* estate spec soak: 2074 events, 0 with weapon 0;
* defense crash: 508 soldier and Ghost events (Ghost ent 312, wire weapon 7);
* take fav: 414 events.

### Gun models

**Root cause (native, MEASURED with the developer command `modelslot` and a model-configstring trace).**
* `G_ModelIndex`'s level-init path gives a name that has a slot in IW3's constant configstring table that slot, even when another model already holds it.
* On coop_spec_mp_crash_tropical the aitype precached `weapon_cheytac` into slot 29. Moments later `precacheModel("weapon_m82")` rewrote slot 29 to `weapon_m82`, because IW3's constant table puts `weapon_m82` at cs 859 = slot 29.
* Every later `attach( "weapon_cheytac" )` raised "model 'weapon_cheytac' not precached" and stopped the server. This happened on a fresh load of that map too, not only after a map change.

**Fix (`_g_utils_mp.asm`, `%ifdef OL_SERVER`).** The constant slot is taken only while it is free. Otherwise the name is registered like any other model. On crash spec, `weapon_cheytac` now stays in slot 29 and `weapon_m82` gets slot 30.

**Changes in the coop scripts.**
* Gun models are on by default; `scr_coop_ai_weapon_models 0` hides them.
* `_coopmission::arm_soldier` attaches the gun on `tag_weapon_right` and sets the actor's weapon. Mission casts that fire (Ghost) call it too.

**MEASURED.** After estate → crash spec (an in-process map change), all 10 live soldiers had their gun attached (attach count 2: head + gun), including both cheytac snipers. There was no script error.

### Clipping through walls

**What the audit found** (`codol_actorAudit 1`: each actor frame, the capsule shrunk by 1 unit against solid, and against player clip, plus a chest-height line from the last origin to the new one):

| run | actor frames | inside solid | inside player clip | crossings |
|---|---|---|---|---|
| before, estate spec, about 4 min | 99 568 | 408 | 55 | 0 |
| after, estate spec, 5 min soak | 173 911 | 0 | 0 | 0 |

**Causes:**
1. Authored SP spawner origins inside the MP level's geometry. `forceTeleport` only tried straight up, and on failure left the actor where it was. One soldier stood inside a wall at (1382 2818 153) for his whole life.
2. The actor clipmask (C1's 0x2820015) has monster clip but not player clip. MP levels never author monster clip; they block players with player clip (fences, rails, window gaps, roof edges), and the bot waypoint graph the soldiers route on was built for players.
3. Scripted placements with no checks: getting off a ride left the actor inside the vehicle's clip.

**Fixes:**
* A coop species collides with player clip (`clipmask |= 0x10000`). Sight ignores player clip.
* A coop `forceTeleport` uses `ActorCoopPlace`:
  * the point and small lifts first, then rings of 8 to 96 units;
  * a candidate needs clear capsule and walkable floor within 160 units;
  * from an open centre, a candidate needs a clear chest-height line, so the actor never comes out on the far side of a wall;
  * the nearest waypoint node within 192 units is the last resort;
  * it returns whether it placed the actor, and `_coopmission::spawn_soldier` logs a spawner with no clear floor.
* Getting off a ride uses the same placement.
* A rider on a collision-less mover (Ghost's stairs mover) is lifted up to 24 units out of the world.

**Still seen:**
* Ghost's ride down the defense-crash stairs grazes the stair geometry (89 frames, at most 2 units deep).
* The take fav technical's riders overlap low geometry along the truck's path (27 frames more than 3 units deep). Their seats are left where the tags put them.

### MP and zm

* The model-slot fix is shared. It changes only the case where a model would have been overwritten. MEASURED: on mp_crash_tropical war, all 221 model slots are identical before and after the fix, with no script error.
* Every other native change is gated: coop species, the coop zone, or a filter only the coop fire registers. Developer probes are gated on `developer` plus their own dvar.
* **Shared bug left alone in mp and zm:** a scripted `magicBullet` is silent there too, for the same missing weapon word. The zm Juggernaut's minigun fire is an example; it has its own loop sound and flash fx. Publishing the word everywhere is a one-line change in `sv_gameframe.c` (drop the zone test). The Juggernaut could instead fire through an actor-fire path, as the coop soldiers now do.

### Still to check in C1 by eye

* The flash on the gun's `tag_flash`, the weapon's fire sound and the tracer from the soldier.
* That the gun sits in the hand on every body the coop zones ship (`tag_weapon_right`).
* That an actor with a weapon word does not also draw a second gun.
* The miss impacts near the player.
* Ghost on the stairs, and the technical's riders.

## Actor characters, the IW4 way (2026-10-13)

Every actor the coop missions place now gets its look from its authored
classname's `aitype` and `character` scripts, as IW4 SP does it.

**Engine** (`actor_scripts.cpp`). In a coop match, an `actor_` classname
in the map entity string whose `aitype/<lower-case name>.gsc` exists is a
soldier-species type (`authored_families[]`). It is loaded with the other
types, precached by `ActorScriptsPrecache` at level init, and its
`main()` runs at `spawnactor`. The map's own spawners get their `spawner()`
once precache is done (T5 FinishSpawningAll order). A classname with no
aitype stays inert (the coop dogs). Types are matched without regard to case;
the addons spell `favela_AR_so` and `favela_ar_so` both ways. There are now 64
type slots. The species preload still loads the `actor_enemy_coop_soldier`
template, which is the fallback.

**New builtin: `codol_isModelMounted( <xmodel> )`** (find-only zone probe,
`actor_spawn.cpp`). C1 ships each character set per mission zone, so
`codescripts\character` filters every xmodelalias array, single-model precache
and body choice through it. No model a zone lacks is ever precached or
attached.

**Scripts** (`bin/home/codol_rawfiles`):
* `xmodelalias/`: 12 head lists.
* `character/`: 63 IW4-layout characters, ported from IW4 where it has one.
* `aitype/*_so.gsc`: 37 aitypes, one per authored classname. Each sets the IW4
  fields and calls `aitype\enemy_coop_soldier::soldier_setup( role )` for the
  coop weapon, engagement bands and mission overrides. It then picks its
  character with `get_random_mounted_character`.
* The missions spawn through `_coopmission::template_for( classname, team )`,
  an unlimited copy of the authored spawner, and through `spawn_class_actor`
  for their cast. Both carry `script_accuracy` and `script_char_index`.

**Measured (2026-10-14, probes on 17765-17767, test client, `scr_coop_test_cmd
characters` / `status`)** on all nine playlists:
* Every placed classname (except the dogs) spawned with a body and a head that
  ship, with 0 missing models.
* The mission soldiers and the 27 killhouse cast members wore their
  classname's characters.
* There were no script errors.

**Gaps.**
* The coop dog spawners (`actor_enemy_dog_so`: defense crash, spec fav, take
  fav) have no coop dog species and are still not spawned.
* Harker's head (`head_hero_russian_president`) and the CoDO Task Force
  body/head pairs (`hea_*`) are inferred from the zone contents, not from a
  shipped character table.

## Soldier animations: C1's IW4 animset (2026-10-14)

User report: "for the campaign missions most of the animations are missing/wrong".

### What actually animates a coop actor (MEASURED)

* Every coop actor is the native `soldier` species and runs `animscripts/soldier_*.gsc` (the nine species modules). The IW4 `animscripts/` tree (combat, run, cover_*, riotshield, ...) and IW4's mission helpers (`maps/_bm21`, `_casual_killer`, `_stretcher`, ...) are in the rawfiles but **no coop match loads them** (`script_resolve.py --list-reachable`). Their missing names do not matter at runtime.
* C1 builds the actor's `generic_human` tree from the animlists it loads: `common` (code_post_gfx_mp), `common_actor_shared` and `common_human` (common_gametype_coop), plus `animlists/<mapname>` (the level's, which names no human clip). `common_human` is IW4's SP soldier set: all 184 clips IW4's `init_animset_*` tables name, IW4's exposed/crouch/pistol/RPG/shotgun/CQB/heat sets, the exposed pains and deaths, run, walk, sprint, run-and-gun, stance changes, twitches, flashbang reactions, cover and corner sets.
* The tree is PRUNED to the names listed (`src/cscr_animtree.cpp`), and the actor channel sends the **server** tree's entry index. A server `%anim` the client lists do not name would shift every later index (wrong clips, not just missing ones). `tools/coop_anim_check.py` simulates both trees and must report them identical.
* Before this pass the soldier set used 25 clips: one aim pose (no aim direction, no crouch), rifle anims for every weapon, three pains, six deaths, two flashbang clips, a single reload.

### Inventory (generic_human names; `inv.py`-style count against the zones)

| referenced by | client-loaded | civilian list only | a coop mission animlist only | xanim ships but no list | atr node, no xanim | absent |
|---|---|---|---|---|---|---|
| IW4 `animscripts/` (not run in coop) | 1006 | 28 | 7 | 6 | 91 (riotshield, humvee turret, saw gunner, `_zm` aims) | 6 |
| IW4 `maps/_*.gsc` helpers (not run in coop) | 125 | 0 | 166 | 31 | 265 | 7 |
| soldier set + coop mission scripts (run) | 228 (all) | 0 | 0 | 0 | 0 | 0 |

### The mapping applied (`animscripts/soldier_shared.gsc`)

IW4 `animset.gsc` tables, chosen by the weapon in his hands (IW4 picks the set from the weapon, not the authored role): rifle (default, SMG, LMG, sniper, auto shotgun), pistol (`pistol_stand_*`, never crouches), pump shotgun (`shotgun_aim_*`, `shotgun_stand_fire_1A/1B`, `shotgun_*_reload`), launcher (`rpg_stand/crouch_*`). A mission or aitype may force a family with `self.coop_anim_kind`.

* **stop**: with an enemy, IW4's exposed aim (`%exposed_aiming` + straight pose + the four additive aim groups `aim_2/4/6/8`, driven by the yaw/pitch to the enemy, IW4 trackLoop limits) with the alert idles on `%add_idle` and the exposed twitches; without one, IW4's casual idle sets (`casual_stand_idle` + twitches or `casual_stand_v2_*`), the bored patrol idles after a patrol walk, `casual_crouch_idle` when crouched. Cast members (`coop_cast`) are unchanged.
* **combat**: stand or crouch (`exposed_stand_2_crouch` / `exposed_crouch_2_stand`; crouch odds by role, only at 400+ units and with a clear crouched line of fire), aim tracking, fire on `%add_fire` (auto loop for LMG/SMG, `burstN` for rifles, `single`/`semiN` for sniper, shotgun, pistol), the family's reload for the pose.
* **move**: `run_lowready_F`; short `sprint1_loop` sprints while hunting; run-and-gun (`run_n_gun_F/L/R`, bursts on `%add_fire`, hit chance halved; `scr_coop_ai_run_n_gun 0` turns it off) at a visible enemy within 120 degrees of the path; `crouch_2run_F` from a crouch; patrol walk with its twitch; `self.coop_cqb` for the CQB walk/run.
* **pain / death**: IW4 `getPainAnim` / `getDeathAnim` by pose, movement, weapon family, hit location (from the "damage" notify's bone) and side (`damageyaw` converted to IW4's convention): running pains and deaths, extended pains (those ending on a knee leave him crouched), crouched pains and deaths, pistol pains and deaths, `rpg_stand_death*`, close shotgun "strong" deaths, explosion deaths by side.
* **flashed**: the five `exposed_flashbang_v1..v5`.

### Diagnostics

* `+set animTrace 1` (native, `script_anim.c`): one line per animation transaction with the clip name; a clip that resolved to the null animation is tagged `NULL-ANIM`.
* `+set scr_coop_anim_trace 1`: the soldier set's state choices (pose, refused crouch).
* `_cooptest` `hurt <damage>`: wounds every live enemy (pain probe).

### Left

* ~~No cover~~ and ~~no arrivals/turns~~: done in the next pass ("Soldier combat: cover, transitions, traversals").
* Juggernaut, technical-turret, saw-gunner, breach, heli-rope and mission-scene clips are only in the coop mission animlists, which C1 loads under `coopClientMapName 1` (on since 2026-10-14; section 15.5). The scripted scenes use them (next section); the juggernaut and turret sets belong to the soldier animscripts.
* The `pistol` role resolved to `m16_basic_so` on the probes (no pistol weapon published in the coop window), so pistol soldiers hold and animate a rifle; the pistol set is used as soon as the aitype hands them a pistol.

## Soldier combat: cover, transitions, traversals, grenades (2026-10-14, second pass)

User request: "use all of the animations for the actors that ship with the game". The soldier set went from 208 to 744 of the 1025 `common_human` clips.

### The level's own node graph (native, `bot_nav.c`)

* The base levels carry IW4's SP node set in their entity lumps (MEASURED, `%TEMP%\codol_zones`): mp_estate_tropical 2484 `node_*` (403 cover left/right, 115 stand/crouch/window, 72 concealment, 19 negotiation pairs), fav 978, crash 1080, crash_intro 1056, killhouse 732.
* In a coop match that graph replaces the player waypoint CSV for the actors (built at load, T5 Path_ConnectPaths rule: estate 26098 links in ~1 s). A level whose graph falls apart keeps the CSV: killhouse (13 components, main 243 of 732). `coopMapGraph 0` keeps the CSV everywhere.
* `BOT_NODES` is 4096; the planner's open set is a binary heap.

### Cover (native `actor_cover.c` + `ActorCoverThink`; GSC `soldier_cover.gsc`)

* A coop enemy soldier with cover enabled (`codol_actorUseCover`; off for `coop_cast`, `coop_no_cover`, allies, `scr_coop_ai_cover 0`) claims an IW4 cover node: unclaimed, none held within 32 units, on his graph component, inside the engagement band, the enemy inside the node's arc (IW4 corner.gsc: a left corner covers yaw -20..80 counter-clockwise, a right corner -80..20, walls +-65), a wall that hides him, preferring a node he can shoot from. He routes onto it; on arrival the combat module runs the node type's IW4 behaviour.
* The node is given up (and avoided 4-8 s) when the enemy flanks it, sees past the wall, closes inside 120 units or leaves the band; the script gives it up after 8-15 s without sight of the enemy (IW4 boredom), avoided 20 s.
* Corners: hide side-on (alert idle, twitches, flinch, reactions), step out to lean / A / B / over by the enemy's yaw (IW4 getCornerMode) with the lean or exposed aim sets, fire bursts, switch A/B, step back (reload variant), blind fire when suppressed, look and fast look, grenades (exposed and safe throws), stance changes.
* Walls: hide idle, pop up over / stand / lean / left / right with their aim sets, duck back, blind fire, peek, reload, grenades, reactions.
* Pains and deaths at cover use IW4's cover sets.

### Transitions and turns (`soldier_move.gsc`, `soldier_stop.gsc`, `soldier_combat.gsc`)

* IW4 init_move_transitions tables (arrivals, exits, root motion and yaw of every clip) built once per match.
* Exits from cover (corner/wall/CQB) and from an exposed stand or kneel toward the route; arrivals onto the claimed node fitted so the clip ends on it; exposed stops at plain goals.
* In-place turns (exposed_tracking_turn*, crouch and pistol turns) with "face current" (on a coop actor the yaw is left to the clip's rotation); run turns (run_turn_*, CQB walk turns); run-and-gun incl. _120 and backwards fire; run reactions; run reload; backpedal; patrol start/stop and casual idle transitions; CQB stand set.

### Traversals (`soldier_jump.gsc`)

* Each negotiation pair's IW4 animscript clip (wall_hop, jumpdown_N, stepup_52, mantle_on_56, jump_up_30, slide_across_car, wall_over_40, trench_jump_out, ladders incl. the window ladders, ...), in noclip, fitted to the pair's displacement and landed on the end node. Coop negotiations may run 15 s.

### Grenades, melee, long deaths

* Grenades: `codol_actorThrowGrenade` (G_FireGrenade owned by the actor) on an arc that clears the world, released at the clip's throw notetrack; one AI grenade every 8-15 s, 1-2 per soldier, 250-1250 units. A player grenade within 256 is thrown back (grenade_return_*) or dived from (exposed_dive_grenade_*, squat).
* Melee: melee_1 / stand_2_melee_1 / run_2_melee_charge inside 72 units, the blow at the "fire" notetrack.
* Long death (IW4 crawlingPain conditions): dying_stand/crouch_2_back/crawl, dying_back idle and twitches, dying_crawl, their deaths.

### New builtins (coop only, actor methods)

`codol_actorUseCover( on )`, `codol_actorCoverNode()`, `codol_actorCoverPoint()`, `codol_actorCoverArrived()`, `codol_actorLeaveCover( avoidMs )`, `codol_actorLookahead()`, `codol_actorGoal()`, `codol_actorThrowGrenade( weapon, start, velocity, fuse )`; pathnode field `node.type`.

### Probe tools

* `_cooptest`: `animstats` (behaviour counters), `travtest <animscript> [k]`, `skirmish <n> <dist>`, `nadetest [dist]`, `fragnear`, `bulletkill [k]`.
* `debugTrace 1`: an actor frame over 4 ms is logged with its route / cover / goal / sight / animscript-switch split.
* Dvars: `scr_coop_ai_cover`, `scr_coop_ai_transitions`, `scr_coop_ai_grenades`, `scr_coop_ai_grenade_react`, `scr_coop_ai_melee`, `scr_coop_ai_long_death`, `coopMapGraph` (all default on).

### Still unused, and why

* Prone (prone_*, stand/crouch_2_prone, prone_grenade): one prone node per level; prone nodes are skipped.
* Heat (heat_*): IW4's level-enabled heat mode; no mission enables it.
* SAW gunner (saw_gunner_*) and sm_* mounted-gun sets: need turret nodes / a mounted weapon.
* Synced melee (melee_sync_*, melee_f_awin/dwin, cornersd*_melee, bog_melee_*, ai_attacked_german_shepherd_*): paired with a player or dog animation.
* Wounded pistol fire (dying_back_fire/aim): the dying soldier is already dead to the game and fires nothing.
* Shuffles between adjacent cover nodes, corner doors, the pains out of a step-out (pain is deferred until he is back in cover), additive pains, walk-aim/strafe walks, crouch walks, RPG throw-downs (the rpg role carries a rifle), pistol pullout/switch (no sidearms), the long run pains (300-unit travel), the alt pistol aims, and blend nodes (cover, corner, turn, reload, add_pain, exposed_modern, ...).

## Scripted scenes: the missions' own scene clips (2026-10-14)

User report: characters "weirdly standing around doing nothing while the script plays". Each mission's scene clips ship only in its own animlist (`animlists/coop_<mode>_<level>.animlist`), which C1 now loads (`coopClientMapName`, on; section 15.5). The scenes are scripted the IW4 way, after each level's `*_anim.gsc` and `maps\_anim`.

### How a scene is played (`maps/_coopanim.gsc`)

* **The table.** `level.coop_scr_anim[ animname ][ scene ]` (IW4 `level.scr_anim`), filled by `add_anim` / `add_anim_variants`. Each mission has its own table file: `_coopscene_killhouse`, `_coopscene_crash`, `_coopscene_estate`, `_coopscene_fav`.
* **No `%anim` for a mission clip.** Every coop mission module compiles in every coop match, and a `%anim` adds its node to the server tree in all of them. The actor channel sends the server tree's entry index, so a node the client lacks shifts every later index. Scene clips are looked up by name with the native `codol_getAnim( "generic_human", name )`, which answers only what the client's lists already put in the tree, and `undefined` in a mission that does not ship the clip. Common clips (common_human, common_actor_shared) stay `%anim` and are the fallbacks. `tools/coop_anim_check.py` stays identical for all nine playlists with either map name.
* **Playing.**
  * `anim_loop_solo` (IW4 `anim_loop`): the soldier_stop cast pose holds the idle. Most C1 scene idles are not looping clips (their xanim loop flag is off), so each cycle is replayed from frame 0, with an optional random variant per cycle.
  * `anim_single_solo` / `play_anim`, in place: through the cast pose's one-shot slot (talk gestures, reactions). The idle resumes afterwards.
  * `anim_single_solo` / `play_anim` at a node: `animscripted` with the clip's root motion, the actor first placed at `getStartOrigin` / `getStartAngles` (walk-outs, vaults, drags, rope drops). Modes: `zonly_physics` (collision and gravity) or `noclip` (exact).
  * `anim_reach_solo` walks the actor to the start spot. `release` hands him back to the soldier animscripts.
* **Vehicles.** The vehicles tree is declared server side (`_destructible_types`), so `codol_getAnim( "vehicles", name )` works on a script_model mover; its first `setAnim` binds the tree. `_coopevents::play_drive_anim` turns the rotors (IW4 `build_drive`).

### Native support added

| What | Where |
|---|---|
| delta3D xanims load (section 15.5) | `xanim_native.cpp` |
| `codol_getAnim( tree, name )` | `script_anim.c`, registered in `scr_vm_main.c` |
| `getStartOrigin` / `getStartAngles( origin, angles, anim )` (IW4) | `script_anim.c` |
| `animscripted` in a delta mode leaves the yaw to the clip after the start facing | `actor_lifecycle.cpp` |
| `codol_actorRide( parent, undefined, true )` rides from where the actor stands (a seat clip's start offset) | `actor_lifecycle.cpp` |

### Per mission

**Training killhouse** (`_cooptraining`, `_coopscene_killhouse`; CoD4 `killhouse_anim`, IW4 `trainer_anim`, `_humvee`).

* **The jeep.** Price drives in `humvee_idle_frontl` and climbs out with `humvee_driver_climb_out`.
* **The instructors.**
  * Price by the jeep and at the debrief: `killhouse_price_stand_idle`, talking with `killhouse_price_stand_talk`.
  * Tiny Man at the range: greets with `killhouse_gaz_intro`, idles in `killhouse_gaz_idleb`, talks with `talk_side` / `point_side` / `idlea`.
  * Newcastle: the same instructor set.
  * Lootz: `killhouse_laptop_idle` / `_twitch` / `_lookup`.
* **The SAS reveal.** `killhouse_sas_price_idle` and `killhouse_sas_1/2/3_idle`.
* **The camp.** The 15 ambient soldiers play their spawners' own `animation` clips: `civilian_reader_2`, `civilian_sitting_talking_*`, `civilian_texting_standing`, `training_pushups_guy1`, `training_basketball_guy1/2`, `cliffhanger_welder_*`, `training_humvee_repair`, `sitting_guard_loadak_idle`, `parabolic_leaning_guy_idle_training`, `bunker_toss_idle_guy1`, `roadkill_humvee_map_sequence_quiet_idle`, `training_intro_trainee_1_end_idle`, `training_pit_stand_idle`.

**Spec1 killhouse** (`_coopcourse`). The two black-kit Prices are placed: `killhouse_sas_price_idle` at the pit entrance, and on the tower `killhouse_gaz_intro` as a player comes near, then `killhouse_gaz_idlea`.

**Defense crash** (`_coopmission_defense_crash`, `_coopscene_crash`).

| Scene | Who | Clips |
|---|---|---|
| The drag | the dragger | `crash_body_drag_enemy`, then `drone_stand_idle` |
| The interrogation | the nearest watcher | `village_interrogationb_price` |
| The flash | Ghost | `crash_flash_price_peek` / `_throw`; the bang on the clip's release |
| The flash | the room | `breach_react_guntoss_v2_guy1`, `exposed_flashbang_v1..v5`; the boss plays `crash_flash_react_ortiz` before he runs |
| The double door | Ghost | `hunted_open_barndoor_flathand` |
| Price on the radio | Ghost | `estate_ghost_radio`, held for the line |
| The office | the flashed men | the flashbang reactions |
| The wreck | the searchers | `crash_search_wreckage_crouch_idle` / `_patrol_idle`, getting up with `_crouch_end` |

**Take estate** (`_coopmission_estate`, `_coopscene_estate`; IW4 `estate_anim`, `_slowmo_breach`).

| Scene | Who | Clips |
|---|---|---|
| The zodiac | Ghost | `estate_intro_3rd_character_right` |
| Dialogue | Ghost | talk gestures |
| The breach stack | Ghost | `breach_flash_r1_enter` / `_idle` |
| The breach | the guards | their spawners' reactions (`breach_react_push_guy1/2`, `breach_react_knife_charge`, `exposed_idle_reactA`) |
| The breach | Vazquez | `estate_breach_hvt_cower` / `_idle` |
| The plea | Vazquez | `unarmed_cowerstand_idle2point` / `_pointidle` |
| The walk-out | Vazquez and Ghost | `estate_breach_hvt_walkout` + `estate_breach_buddy_walkout`, `_p2` for the bolt |
| The surrender | Vazquez | `estate_surrender_vasquez`, `_02`, at the authored struct; the sniper shot `_02_shot` |
| Stealth | patrols | `patrol_bored_react_walkstop` before the hunt |
| The technical | the crew | `technical_*_climb_out` |

**Take fav** (`_coopmission_take_fav`, `_coopscene_fav`; IW4 `favela_anim`, `favela_escape_anim`).

* **The meet.** `favela_van_shootout_guy1..4` at `opening_scene_node`: Ortiz, the two informants and the van driver, timed by the clips' fire and bodyfall notes.
* **Ortiz's run.**
  * At the chase path's `animation` nodes: `favela_traverse_rail_ortiz`, `favela_escape_bigjump_faust`.
  * With his shouts: `favela_order_grunts_ortiz`.
* **The tackle.** `favela_tackle_ghost` / `_ortiz`, then their idles.
* **The technical.** The crew in `uaz_driver_idle_drive`, `technical_passenger_idle` and `technical_turret_driveidle`, climbing out with `technical_*_climb_out`. This replaces `technical_driver_idle`, which ships in no zone.
* **The houses.** The soldiers smash the windows (`window_smash_stop_inside`). The civilians play `unarmed_shout_window`, `unarmed_close_garage` and `doorpeek_open/idle/close`.

**Spec crash and the prolog** (`_coopevents`).

* **The Mi-17s.** The two Mi-17s carry their six `desert_elite` riders each (`script_vehicleride` / `script_startingposition`; before, the helicopters flew in empty and the riders never spawned).
* **The riders.**
  * They sit in `mi17_<n>_idle`.
  * The helicopter hovers at its `script_unload` node at the height the drop clips need over the ground under it.
  * They go down the two ropes with `mi17_<n>_drop`, then fight.
* **The rotors.** They turn (`mi17_heli_rotors`).
* **Probe.** MEASURED, 12 riders landed on the ground on both maps.

**Spec estate, spec fav.** No scripted scene is authored beyond the soldiers' combat: two orphan `script_animation` structs on estate, and the civilians of fav, which have no walker (below).

### Left, and why

* **Clips that play on a player** (`crash_interrogate_*`, `crash_body_drag_player`, `favela_van_shootout_car*_3rd_*`, `invasion_opening_hummer2_player`): no actor stands in for a player.
* **Prop trees no server script declares** (`script_model`: the phone, breach doors, the door peek door; `animated_props`: the basketball; `door`): they need a `#using_animtree` and a script_model for the prop.
* **The breach stack in training and the hostage execution set in take estate**: no actor or moment in the mission flow. The training stack would also exceed the 28-actor cap.
* **The prolog's own intro** (`crash_opening_player_hit`, `intro_dragger`): the prologue is disabled in retail (comaplist row 57) and its flow does not spawn them.
* **The spec fav civilians and every coop dog**: they need a civilian walker and a coop dog species.
* **Juggernaut, turret and saw-gunner sets**: soldier animscripts (combat), not scenes.

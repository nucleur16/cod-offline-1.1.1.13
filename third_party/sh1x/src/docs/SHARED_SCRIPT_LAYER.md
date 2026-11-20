# The shared script layer: common scripts every CoDOL mode can use

2026-10-09. How IW3, IW4 and T5 split their scripts into a mode-agnostic common
layer and per-mode layers, what the retail CoDOL zones show about CoDOL's own
split, what in this tree was mode-agnostic in substance but unusable outside
multiplayer, and the shared modules that now fill the coop and zm holes.

Evidence classes are kept apart throughout: **source written**,
**resolver-checked** (`tools/script_resolve.py` exit 0), **probe-tested**
(a dedicated server on an isolated port and home, test clients, log read back)
and **seen in real C1** (nothing in this document is).

---

## 1. Reference structure

### 1.1 IW3 (CoD4)

Not verifiable locally: no IW3 raw script dump is on this machine (the
CoD4x_Server tree carries the IW3 engine, not its scripts). From the shipped
game: `common_scripts/utility.gsc` (flags, arrays, `getstruct`), `_createfx`
and `_fx` are shared by the campaign and multiplayer; `maps/_utility.gsc`,
`maps/_load.gsc`, `maps/_hud_util.gsc` are the campaign's, `maps/mp/_utility.gsc`,
`maps/mp/_load.gsc` and `maps/mp/gametypes/*` (with `_globallogic`,
`_hud_util`) multiplayer's; `animscripts/`, `aitype/`, `character/` are the
campaign's AI; `codescripts/` (delete, struct) is loaded by the engine in every
mode.

### 1.2 IW4 (MW2) — `D:\CoD\COD-GSC-Source\IW4-GSC` (MEASURED)

| layer | files | used by |
|---|---|---|
| `common_scripts/` | `utility`, `_fx`, `_createfx`, `_createfxmenu`, `_destructible` (+ `_destructible_types*`), `_dynamic_world`, `_elevator`, `_pipes`, `_painter`, `_artcommon`, `_sentry` | campaign, Spec Ops and multiplayer |
| `codescripts/` | `delete`, `struct` | engine, every mode |
| campaign | `maps/_utility`, `maps/_load`, `maps/_hud_util`, `maps/_compass`, `maps/_coop` (Spec Ops downs and revives), `maps/_specialops*`, `maps/_gameskill`, `animscripts/`, `aitype/`, `character/` | campaign and Spec Ops |
| multiplayer | `maps/mp/_utility`, `maps/mp/_load`, `maps/mp/_compass`, `maps/mp/gametypes/*` (`_hud_util`, `_damage`, `_class` ...), `maps/mp/perks/*`, `maps/mp/killstreaks/*` | multiplayer |

Both loaders start the same `common_scripts` systems over the map entities:
`maps/_load.gsc:289-371` (`_painter`, `_fx::initFX`, `_destructible::init`,
`_elevator::init`, `_pipes::main`, `_dynamic_world::init`) and
`maps/mp/_load.gsc` (the same list plus the multiplayer minefields, radiation,
shutter, destructables). The same API exists twice where the element code is
mode-agnostic but the includes differ: `maps/_hud_util` vs
`maps/mp/gametypes/_hud_util`, `maps/_compass` vs `maps/mp/_compass`.

### 1.3 IW5 (MW3) — `IW5-GSC`

Same shape as IW4 (`common_scripts/`, `maps/`, `maps/mp/`); adds
`_destructible_types_anim_chicken_dlc`, `_interactive`. Nothing new about the
split.

### 1.4 T5 (Black Ops) — `T5-STEAM-GSC`, `BO1-GSC`, `D:\CoD\OpenBLOPS` (MEASURED)

T5 makes the shared half explicit and adds Zombies as a third consumer:

* `maps/_load_common.gsc` — the level-load half shared by the campaign
  `maps/_load.gsc` and `maps/_zombiemode_load.gsc` (which `#include`s it).
* `maps/_laststand.gsc` — downs, bleed-out and revive, run by campaign co-op
  and Zombies; `maps/_zombiemode.gsc` overrides only hooks
  (`player_laststand` :2140, `player_revive_monitor` :2025,
  `wait_and_revive` :4818, the all-down rule :4700, `level.can_revive` :4176).
* `maps/_hud_util.gsc`, `maps/_loadout.gsc`, `maps/_utility.gsc` shared by
  campaign, co-op and Zombies; `maps/_zombiemode_utility.gsc` includes
  `maps/_utility` and `common_scripts/utility` rather than re-implementing them.
* `maps/mp/` keeps its own `_laststand`, `_utility`, `gametypes/_hud_util`.

### 1.5 Retail CoDOL zones (MEASURED, `%TEMP%\codol_zones`, `docs/zone_assets/`)

The client zones ship **no gameplay GSC** — the server owned those — so the
evidence is indirect but consistent:

* Every map zone of **all three categories** ships `maps/mp/<map>_fx.gsc` and
  `maps/createfx/<map>_fx.gsc`, and every createfx file is written against the
  same shared modules: `#include common_scripts\utility; #include
  common_scripts\_createfx;` (27 zones: mp_*, coop_*, zm_*). The `_fx` files call
  `common_scripts\utility::shouldRunServerSideEffects()`.
* The coop training zones' `_fx.gsc` include **`common_scripts\shared`** (for
  `setFootstepEffect`), a CoDOL module name IW4 does not have — CoDOL kept a
  category-independent module under `common_scripts/`.
* `localized_common_gametype_coop` and `localized_common_gametype_zm` ship the
  **identical** seven revive strings of IW4 `maps\_coop.gsc`
  (`SCRIPT_COOP_REVIVE`, `_REVIVING`, `_REVIVING_PARTNER`, `_BLEEDING_OUT`,
  `_BLEEDING_OUT_PARTNER`, `_PICKUP`, `_DROP`): one revive layer serving both
  modes, T5's `_laststand` arrangement.
* `common_mp` (mounted for every category) carries the shared animtrees
  (`destructibles.atr`, `vehicles.atr`, `generic_human.atr`, `dog.atr`,
  `script_model.atr` ...); `common_gametype_coop`/`_zm` carry only their
  animlists, shocks and rumbles.
* The perk table `mp/coPerkStatsTable.csv` (`code_post_gfx_mp`) is one table for
  all categories; the coop perks are its rows 64-87 (`specialty_coop_*`).

**Conclusion.** CoDOL followed the IW4/T5 pattern: a `common_scripts/` layer
used by mp, coop and zm, with mode layers above it. In this tree that layer's
*location* already existed — every category falls back to the common root — but
almost nothing in it was usable outside mp because of what it included.

---

## 2. How the tree resolves a script

**Updated later the same day: the tree now follows the IW4/T5 layout
(docs/GSC_TREE_LAYOUT.md).** `bin/home/codol_rawfiles/` is one search
path for every category; coop lives in `maps/_coop*.gsc` and `maps/<map>.gsc`,
zm in `maps/_zombiemode*.gsc` and `maps/zm_*.gsc`, their actors in the
top-level `animscripts/`, `aitype/`, `character/`, and the engine asks each
category for its own entry points (`ScriptEntryPoints`). The `coop/` and
`zm/` overlays, and the `<root>/<category>/<name>` probe that read them, are
retired. Paths below that still say `coop/maps/mp/gametypes/_coopmode.gsc` or
`zm/...` describe the tree as it was when this pass was written; the old-to-new
table is in docs/GSC_TREE_LAYOUT.md section 5.

What kept coop and zm from using the common files was never the loader but the
GSC compile rule that a `#include` or a `file::function` reference compiles the
whole target closure.

---

## 3. Audit: mode-agnostic in substance, mp-only by dependency

Closure sizes MEASURED with
`script_resolve.py --root bin/home/codol_rawfiles --only-seeds --seed <file> --list-reachable`:

| module | closure | why | usable outside mp before? |
|---|---|---|---|
| `common_scripts/utility` (+ `_fx`, `_createfx`, `_createfxmenu`) | 4 | includes only common_scripts | yes, but unused by coop/zm |
| `common_scripts/_destructible` | 6 | common only | yes; never started outside mp |
| `common_scripts/_elevator`, `_dynamic_world`, `_pipes` | 5 each | common only | yes; never started |
| `maps/mp/_compass` | 1 | includes nothing | yes; coop copied its maths |
| `maps/mp/_minefields`, `_destructables` | 1 | include nothing | yes; never started |
| `maps/mp/_animatedmodels`, `_art` | 5-6 | common only | yes (needs map registrations) |
| `maps/mp/_load` | 103 | `#include maps\mp\_utility` | **no** |
| `maps/mp/_utility` | 103 | references gametype modules | **no** |
| `maps/mp/gametypes/_hud_util` | 103 | `#include maps\mp\_utility` (uses only `strip_suffix`) | **no** |
| `maps/mp/perks/_perks`, `_perkfunctions` | 103 | `_utility`, `_hud_util` | **no** |
| `maps/mp/gametypes/_class` | 103 | gametype chain | **no** |
| `maps/mp/gametypes/_hud_message`, `_gameobjects`, `_objpoints` | 103 | `_utility` | **no** |
| `maps/mp/_radiation`, `_shutter`, `_global_fx` | 103 | `_utility` | **no** |

### 3.1 Holes in coop and zm, ranked by gameplay impact

1. **coop: no downs or revives.** A lethal hit killed the player outright; IW4
   Spec Ops downs him and a partner revives (`maps\_coop.gsc`), and the coop
   zone ships the revive strings. zm had a complete T5 port, locked in `zm/`.
2. **coop: the base map's systems never ran.** Coop plays on the multiplayer
   level zones, so it stands on the same destructible cars and props (84 on
   mp_crash_tropical, 3 on mp_killhouse_sh, MEASURED), exploders (14 createfx
   entries), pipes, dynamic-world triggers and minefields — all inert, because
   only `maps\mp\_load` started them.
3. **zm: the maps' destructibles never ran.** zm_prototype_sh places 48
   (18 `vehicle_coupe_white` cars IW4's table defines), zm_asylum_sh 114
   (MEASURED; the `codo_toy_*` types are not in IW4's table and stay static, as
   in mp).
4. **coop: class perks never applied.** `giveStartingLoadout` gave weapons
   only; the class table's `loadoutPerk1..3` (`specialty_coop_*`) were never
   set, and `specialty_coop_armorvest`'s modMaxHealth +50 had no consumer.
5. **coop and zm: no HUD helper library.** Every coop mission and zm module
   builds text and bars on raw `newClientHudElem` (7 in the training HUD, 8 in
   defense crash, zm's `create_simple_hud`, the revive bars).
6. **coop: minimap maths copied** from `_compass` (it works; duplication only).
7. **coop and zm: re-implemented utilities.** zm `_zombiemode_utility` re-
   implements `flag_*`, `get_struct*`, `array_*` from `common_scripts/utility`;
   coop `_coopmission` keeps its own flags (`level.coop_flag`), forwarded by the
   mission files; coop and zm each keep a health-regen loop. Low gameplay
   impact; the shared versions exist now.
8. **coop: class killstreaks** (`sentry`, `uav`, `rcbomb_mp`, `helicopter`) —
   the killstreak modules are deep in the mp chain. Not addressed.
9. **coop: level `_fx` scripts** (`common_scripts\shared`, `setFootstepEffect`).
   C1 draws createfx itself and the missions load their own fx. Not addressed.

### 3.2 A defect found on the way (pre-existing, coop)

A coop player who has been killed keeps the CLIENT_DEAD entity handler, which
has no die function. Coop's `Callback_PlayerDamage` calls `finishPlayerDamage`
without IW4's `isReallyAlive` gate, so any further damage to the body raises
"No die handler for player entity type 12" and the server shuts down
(probe-tested: a second hit on a killed test client, with downs disabled). The
shared destructibles add explosions near bodies, so the gate is part of the
proposed coop change (section 5) and was added to zm.

---

## 4. What was built

All new files are under `bin/home/codol_rawfiles/common_scripts/`, include
nothing mode-specific, and compile alone (closure = the file, plus
common_scripts for `_load_common`). No existing mp-reachable file was edited.

| file | provenance | API |
|---|---|---|
| `_laststand.gsc` | zm's T5 `_laststand` port with every zombies call behind a hook | `init()`, `enable_player()`, `player_laststand()`, `revive_success(reviver)`, `checkForAllDead()`, `player_is_in_laststand()`, `laststand_die()`; config `level.laststand_vars[...]`, hooks `level.laststand_funcs[...]` (`downed`, `downed_post`, `bled_out`, `revived`, `revive_done`, `can_revive`, `revive_time`, `vox_code`, `spare_life`, `use_spare`, `pistol`, `all_down`) |
| `_load_common.gsc` | the map-system half of `maps/mp/_load.gsc` (IW4), T5's `_load_common` role | `main()`; `level.load_common_skip[name]`; starts minefields, destructables, elevator, dynamic world, destructibles, pipes, lanterns, exploders, `_fx::initFX` |
| `_perks.gsc` | IW4 `_utility::_setPerk` family, `_perks::perkTableModifierDelta`, `_class::codolPerkBase` | `init()`, `_setPerk/_unsetPerk/_hasPerk/_clearPerks`, `perkBase`, `perkUpgrade`, `perkModifierDelta`, `giveLoadoutPerks(perks)`, `applyHealthPerks(base)`, `perkList` |
| `_hud_util.gsc` | `maps/mp/gametypes/_hud_util.gsc` (IW4) byte for byte, `strip_suffix` local, `init()` from `_hud.gsc` | `init()` then every IW4 helper (`createFontString`, `setPoint`, `createBar`, `createTimer`, ...) |
| `_compass.gsc` | IW4 `_compass::setupMiniMap`, split to accept the corners | `setupMiniMap(material)`, `setupMiniMapCorners(material, corners)` |

Rules for using the layer:

* Call shared modules **namespaced** (`common_scripts\_perks::giveLoadoutPerks`)
  rather than `#include`-ing them next to the mp files that define the same
  names (`_hud_util`, `_setPerk`).
* A mode configures a shared module (vars and hooks); the module never names a
  mode. Keep it that way — one mode name in a shared file drags that mode's
  closure into every category.
* `_load_common` must run from the level script's `main()` (the precache
  window). The map's destructible anim registrations
  (`common_scripts\_destructible_types_anim_*::main()`, the `<map>_precache`
  script) belong in the level script, not in the shared loader: every `%anim`
  a compiled script names joins the server animtree, so a map must name exactly
  its own. A destructible whose anim the level script did not register is left
  static and logged instead of ending the load (MEASURED: without the
  registrations mp_crash_tropical's ceiling fan was a fatal
  `get_precached_anim` error).
* `level.isSP = false` is set by `_load_common`; `common_scripts/utility::isSP()`
  otherwise answers "singleplayer" for every zm_* and coop_* map name.

### 4.1 Wiring

* **zm** (`zm/` is edited directly): `_zombiemode_laststand.gsc` is now the
  zombies configuration of the shared module (same public names, so no other
  zm file changed); `_zombiemode::levelMain` calls
  `common_scripts\_load_common::main()`; `Callback_PlayerDamage` returns for a
  player who is not alive (IW4 `_damage.gsc` gate).
* **coop**: the coop files are owned by the parent and the mission workers, so
  the wiring is a proposed patch (section 5), tested on a scratch copy of the
  tree.
* **mp**: unchanged. The shared modules are copies of mp code, not wrappers
  that mp calls; moving mp onto them is a later step (section 7).

---

## 5. Proposed coop change (not applied)

`coop/maps/mp/gametypes/_coopmode.gsc` and the base-spelling level scripts
`coop/maps/mp/mp_{crash_tropical,killhouse_sh,estate_tropical,fav_tropical}.gsc`.
The full diff is in the worker report; in summary:

* `init()`: `level.coop_laststand` (dvar `scr_coop_laststand`, default on).
* `levelMain()`: `common_scripts\_load_common::main()` before the minimap;
  `coop_minimap()` uses `common_scripts\_compass::setupMiniMapCorners` (its
  copied maths and `coop_vecscale` go).
* `resolveLoadout()`: `level.coop_perks` from `loadoutPerk1..3`;
  `level.coop_laststand_pistol` = `beretta_so` (IW4's forced "Beretta").
* `giveStartingLoadout()`: `common_scripts\_perks::giveLoadoutPerks`.
* `spawnPlayer()`: `applyHealthPerks( level.coop_player_health )`,
  `self.laststand = false`, `common_scripts\_laststand::enable_player()`.
* `healthRegen()`: no regeneration while downed.
* `Callback_StartGameType()`: `common_scripts\_perks::init()`,
  `coop_laststand_init()` (bleed-out 120 s, revive hold 1.5 s, reach 128 —
  IW4 `_coop.gsc` regular; own pistol else beretta; everyone down = every
  downed player bleeds out now, so the existing lives rule decides failure).
* `Callback_PlayerDamage()`: return when `!isAlive( self )` (section 3.2).
* `Callback_PlayerKilled()`: clear `self.laststand` and the revive trigger.
* `Callback_PlayerLastStand()`: `common_scripts\_laststand::player_laststand()`
  when downs are on.
* The four base level scripts register their map's destructible anims before
  `levelMain`, exactly as the multiplayer level scripts do.

---

## 6. Verification

Resolver (exit 0 for all):

* mp: `--root bin/home/codol_rawfiles --seed maps/mp/gametypes/war.gsc --seed
  maps/mp/gametypes/dm.gsc --seed maps/mp/mp_killhouse_sh.gsc` — 124 scripts,
  **identical file list before and after**; no shared module is reachable from
  dm, war, sd, dom, koth, sab or ctf.
* zm: `--category zm --root bin/home/codol_rawfiles` with zm.gsc and all three
  zm level scripts — 76 scripts; versus before (training only) +13 files, all
  common_scripts plus `maps/mp/_minefields`, `_destructables`.
* coop (current tree): 35 scripts; with the proposed patch: 61, every name
  resolving, for all five base level scripts.

Probes (isolated home, `developer 1`, port 17791 — 17781 was already bound by
a sibling's server):

* **mp** `mp_killhouse_sh dm`, 6 bots: loads, bots play, 0 script errors; the
  129 lowered scripts are the same paths in the same order as the baseline
  (the two size differences, `_autosentry` and `_codol_equiptest`, are the
  turret commit a777f4ca landed between the runs, not this work).
* **zm** `zm_zombie_training`: two test clients downed (native last stand →
  shared `player_laststand`), scripted revive, a 45 s bleed-out, all-down →
  `end_game`; `_load_common` starts with 0 destructibles. `zm_prototype_sh`:
  48 destructibles, three coupes shot to their `_destroyed` models, a down and
  a revive. `zm_asylum_sh`: 114 destructibles, loads clean.
* **coop** (scratch tree with the proposed patch) `mp_crash_tropical` /
  `coop_defense_mp_crash_tropical`: 84 destructibles, cars destroyed
  (`vehicle_80s_sedan1_*_destructible_mp` → `_destroyed`), test clients downed
  by the mission's soldiers and by a car explosion, all-down bleed-out.
  `mp_killhouse_sh` / `coop_training_mp_killhouse_sh`: shared-compass minimap
  `compass_map_killhouse (-2368, 1408) .. (4864, -5824)` (the mission pair, as
  before), class perk `specialty_coop_lightweight` set (`hasPerk` 1),
  `modPistolDeath` granted, down → revive → down → 20 s bleed-out.
  Not verifiable with test clients: the death after a bleed-out completes only
  for a player with a server DObj (`player_die refused: ... missing server
  DObj`), and coop gives test clients no character, so the respawn after a
  bleed-out was not observed.

Nothing here has been seen in real C1.

---

## 7. Left

* Apply and C1-test the coop patch (revive prompt, bars and waypoint icon on
  the client; destructible explosions on the client).
* mp onto the shared layer: `maps/mp/_load` calling `_load_common`,
  `maps/mp/gametypes/_hud_util` and the `_setPerk` family as wrappers. Not done:
  mp must not change behaviour, and copies kept that provable.
* coop class killstreaks; coop class upgrade columns (`_up1`, `_up2`); the
  class chosen in the profile.
* zm `_zombiemode_utility`'s re-implemented flags/structs/arrays and the coop
  mission flag helpers onto `common_scripts/utility` (low impact, many call
  sites, owned by other workers).
* `common_scripts/shared.gsc` (`setFootstepEffect`) and the retail coop
  `_fx.gsc` files, if server-side level effects are ever wanted.
* `_animatedmodels` in `_load_common` (needs the map's `level.anim_prop_models`
  registrations in the mode's level script, as for destructible anims).

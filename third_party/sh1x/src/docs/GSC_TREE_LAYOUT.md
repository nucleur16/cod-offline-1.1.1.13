# The GSC tree layout (IW4 / T5 conventions)

2026-10-09. `bin/home/codol_rawfiles/` is the one script tree the server loads
(AGENTS.md, "Script tree policy"). Since this date it is laid out the way IW4
and T5 lay out their own script trees: multiplayer in `maps/mp/`, the
singleplayer-style modes in `maps/`, actors in the top-level `animscripts/`,
`aitype/` and `character/`, shared code in `common_scripts/` and the engine's
own scripts in `codescripts/`. The per-category overlay directories `coop/`
and `zm/` are retired.

References: `D:\CoD\COD-GSC-Source\IW4-GSC` (IW4 SP in `maps/`, MP in
`maps/mp/`), `D:\CoD\COD-GSC-Source\BO1-GSC` (T5: `maps/_zombiemode*.gsc`,
zombie level scripts `maps/zombie_cod5_*.gsc`, `animscripts/zombie_*.gsc`,
`maps/_callbacksetup.gsc`).

## 1. The layout

| path | what | IW4 / T5 counterpart |
|---|---|---|
| `codescripts/` | `delete`, `struct`, `character` -- loaded by the engine for every category | same in IW4 and T5 |
| `common_scripts/` | mode-agnostic shared code: `utility`, `_fx`, `_createfx`, `_destructible*`, `_load_common`, `_laststand`, `_perks`, `_compass`, `_hud_util`, `_pipes`, ... (docs/SHARED_SCRIPT_LAYER.md) | IW4 `common_scripts/`; T5 `maps/_load_common`, `maps/_laststand` |
| `maps/mp/` | **multiplayer**: `_load`, `_utility`, `_*` MP modules, MP level scripts `mp_<map>.gsc` (+ `_fx`, `_precache`) | IW4 `maps/mp/` |
| `maps/mp/gametypes/` | MP rules scripts (`dm`, `war`, `sd`, ...), `_callbacksetup`, `_gamelogic`, `_class`, ... and the C1 gametype name tables `*.txt` (including `coop.txt`, `zm.txt`, which are C1's localized names, not scripts) | IW4 `maps/mp/gametypes/` |
| `maps/mp/killstreaks/`, `maps/mp/perks/`, `maps/mp/equipment/` | MP killstreaks, perks, equipment | IW4 MP / T5 MP |
| `maps/mp/animscripts/dog_*.gsc` | the MP dog (dogs killstreak) actor scripts | T5 MP `maps/mp/animscripts/dog_*` |
| `maps/_coop_gametype.gsc` | **coop** rules entry (`main()`: callbacks + `_coopmode::init`) | none -- IW4 SP has no gametype; the engine needs one |
| `maps/_coop_callbacksetup.gsc` | coop `CodeCallback_*` labels | T5 `maps/_callbacksetup.gsc` |
| `maps/_coopmode.gsc`, `maps/_coopmission*.gsc`, `maps/_coopintro.gsc`, `maps/_coopcourse.gsc`, `maps/_coopevents.gsc`, `maps/_cooptraining*.gsc`, `maps/_coopvehicle.gsc`, `maps/_cooptest.gsc` | the Spec Ops / campaign co-op layer | IW4 `maps/_specialops*.gsc`, `maps/_coop.gsc` (the stock IW4 files of those names stay as reference) |
| `maps/coop_<mission>_<map>.gsc` | coop level scripts, playlist spelling (`coop_defense_mp_crash_tropical`, ...) | IW4 `maps/so_<mission>_<map>.gsc` |
| `maps/mp_<map>.gsc` | coop level scripts, base spelling (`mp_crash_tropical`, `mp_killhouse_sh`, `mp_fav_tropical`, `mp_estate_tropical`, `mp_crash_intro`) | IW4 SP `maps/<level>.gsc` |
| `maps/_zombiemode_gametype.gsc` | **zm** rules entry (`main()`: callbacks + `_zombiemode::init`) | none -- T5 enters from the level script |
| `maps/_zombiemode_callbacksetup.gsc` | zm `CodeCallback_*` labels | T5 `maps/_callbacksetup.gsc` |
| `maps/_zombiemode*.gsc` | the zombies layer | T5 `maps/_zombiemode*.gsc` (same names) |
| `maps/zm_<map>.gsc` | zm level scripts (`zm_prototype_sh`, `zm_asylum_sh`, `zm_zombie_training`) | T5 `maps/zombie_cod5_<map>.gsc` |
| `animscripts/` | SP actor animscripts: IW4's stock set, plus `soldier_*` (coop soldier species) and `zombie_*`, `zombie_dog_*` (zm species) | IW4 `animscripts/`; T5 `animscripts/zombie_*`, `zombie_dog_*` |
| `aitype/` | actor types: `enemy_dog_mp` (mp), `enemy_coop_soldier` (coop), `enemy_zombie_zm`, `enemy_zombie_dog_zm` (zm), IW4's stock set | IW4 / T5 `aitype/` |
| `character/` | character scripts: `character_mp_german_shepherd`, `character_coop_soldier`, `character_zm_zombie`, `character_zm_zombie_dog` | IW4 / T5 `character/` |
| `animtrees/` | fallback `.atr` files (the mounted C1 archives win, `Scr_ReadFile`) | IW4 `animtrees/` |
| `maps/animated_models/`, `maps/createart/`, `maps/createfx/` | IW4 SP shared map data | same |
| `mp/`, `sp/`, `ap/`, `info/`, `shock/`, `*.csv` | string tables and data | same |

Namespacing: no coop or zm module shares a name with an MP module. The one
real collision the overlay used to hide -- a coop level script for a base MP
map -- is now two modules, `maps/mp_crash_tropical.gsc` (coop) and
`maps/mp/mp_crash_tropical.gsc` (multiplayer), exactly as IW4 keeps
`maps/<level>.gsc` (SP) and `maps/mp/<level>.gsc` (MP) apart. The two
category callback files are named after their mode rather than sharing T5's
single `maps/_callbacksetup`, because a GSC function pointer is a compile-time
dependency: one file naming both `_coopmode` and `_zombiemode` callbacks would
compile both modes into every match.

Cross-namespace references that stay on purpose (they are shared modules,
not category files): coop and zm name `common_scripts/*` and a few
multiplayer modules the shared layer already used -- `maps/mp/_destructables`,
`maps/mp/_minefields` (through `_load_common`), `maps/mp/<map>_precache`
(coop base levels), `maps/mp/gametypes/_codol_equiptest` and
`maps/mp/killstreaks/_rcbomb_presentation` (zm). Zone rawfile paths are not
script modules and keep the zone's own spelling: `maps/mp/<map>_fx.gsc`,
`maps/createfx/<map>_fx.gsc`, `maps/mp/<playlist>.mapents`.

## 2. How the engine finds each category's entry points

One table, `entry_points[]` in `src/online/script/script_source.c`
(`ScriptEntryPoints`), selected by the mounted map's asset category
(`ServerZoneType`, latched by `SelectScriptEntryPoints` at
`GScr_LoadGameTypeScript`):

| category | rules script | callback module | level script |
|---|---|---|---|
| mp | `maps/mp/gametypes/<g_gametype>` (`combat`/`tdm` -> `war`; missing -> `dm`) | `maps/mp/gametypes/_callbacksetup` | `maps/mp/<mapname>` |
| coop | `maps/_coop_gametype` (whatever `g_gametype` says; missing = `Com_Error`) | `maps/_coop_callbacksetup` | `maps/<mapname>` |
| zm | `maps/_zombiemode_gametype` (same) | `maps/_zombiemode_callbacksetup` | `maps/<mapname>` |

No identity (script tooling, early init) answers the mp row. Consumers:

* `GScr_LoadGameTypeScript` (`src/scr_vm_main.c`): rules `main`, the six
  mandatory `CodeCallback_*` labels and the optional `CodeCallback_ScriptCommand`.
* `GScr_LoadScripts`: the level script `<levelPrefix><mapname>`.
* `ActorDamage_LoadCallbacks` (`actor_lifecycle.cpp`) and
  `VehicleDamage_LoadCallback` (`script_vehicle_builtins.c`):
  `CodeCallback_ActorDamage/ActorKilled/VehicleDamage` from the bound module
  (`CurrentScriptEntryPoints()->callbackModule`).
* The actor species table (`actor_scripts.cpp`): `maps/mp/animscripts/dog`
  (mp), `animscripts/soldier` (coop), `animscripts/zombie` and
  `animscripts/zombie_dog` (zm), each `<prefix>_init .. _jump`; the aitype is
  `aitype/<classname without actor_>`.
* `codescripts/delete` and `codescripts/struct` for every category.

## 3. Source resolution order

`Scr_ReadFile` (`src/cscr_parser.cpp`) ->
`ReadScriptSourceWithNativeOverlay`, the same for every category:

1. `animtrees/*` from the mounted C1 archives first (unchanged);
2. `<scriptNativeOverlay>/<name>`;
3. `<scriptOverlay>/<name>`;
4. `<scriptPath>/mp/<name>`, then `<scriptPath>/<name>`
   (plus the two alpha0022 export-filename corrections);
5. otherwise the retained FS / fastfile path.

The launcher points all three dvars at `bin/home/codol_rawfiles`, so in
practice it is one root. The old step 1.5, `<nativeOverlay>/<category>/<name>`,
is gone: nothing needs to shadow a multiplayer module any more. A `.gsc` left
under `coop/` or `zm/` is never loaded; `tools/script_resolve.py` fails
the gate when it finds one.

## 4. Static gate

```
python tools/script_resolve.py --category mp --gametype war --map mp_killhouse_sh
python tools/script_resolve.py --category coop --map mp_crash_tropical --map coop_defense_mp_crash_tropical
python tools/script_resolve.py --category zm --map zm_prototype_sh
```

The tool seeds each category's rules, callback and species scripts itself
(`ENTRY_POINTS`/`SPECIES`, mirrored from the native table and checked by
`tests/test_script_resolve.py`); `--map` seeds the level script in the
category's namespace. `--root` defaults to `bin/home/codol_rawfiles`.

## 5. Old -> new paths (2026-10-09)

All moves are `git mv` (commit "gsc tree: move the coop and zm overlays ...").
Under `bin/home/codol_rawfiles/`:

| old | new |
|---|---|
| `coop/maps/mp/gametypes/coop.gsc` | `maps/_coop_gametype.gsc` |
| `coop/maps/mp/gametypes/_callbacksetup.gsc` | `maps/_coop_callbacksetup.gsc` |
| `coop/maps/mp/gametypes/_coop<x>.gsc` (13: `_coopcourse`, `_coopevents`, `_coopintro`, `_coopmission`, `_coopmission_defense_crash`, `_coopmission_estate`, `_coopmission_estate_nav`, `_coopmission_take_fav`, `_coopmode`, `_cooptest`, `_cooptraining`, `_cooptraininghud`, `_coopvehicle`) | `maps/_coop<x>.gsc` |
| `coop/maps/mp/coop_<mission>.gsc` (9) | `maps/coop_<mission>.gsc` |
| `coop/maps/mp/mp_<map>.gsc` (5: crash_intro, crash_tropical, estate_tropical, fav_tropical, killhouse_sh) | `maps/mp_<map>.gsc` |
| `coop/maps/mp/animscripts/soldier_*.gsc` (10) | `animscripts/soldier_*.gsc` |
| `coop/aitype/enemy_coop_soldier.gsc` | `aitype/enemy_coop_soldier.gsc` |
| `coop/character/character_coop_soldier.gsc` | `character/character_coop_soldier.gsc` |
| `zm/maps/mp/gametypes/zm.gsc` | `maps/_zombiemode_gametype.gsc` |
| `zm/maps/mp/gametypes/_callbacksetup.gsc` | `maps/_zombiemode_callbacksetup.gsc` |
| `zm/maps/mp/gametypes/_zombiemode*.gsc` (40, plus `_zombiemode_betty`, added on main during the move and placed by the merge) | `maps/_zombiemode*.gsc` |
| `zm/maps/mp/zm_<map>.gsc` (3) | `maps/zm_<map>.gsc` |
| `zm/maps/mp/animscripts/zombie_*.gsc`, `zombie_dog_*.gsc` (21) | `animscripts/zombie_*.gsc`, `animscripts/zombie_dog_*.gsc` |
| `zm/aitype/enemy_zombie_zm.gsc`, `enemy_zombie_dog_zm.gsc` | `aitype/` |
| `zm/character/character_zm_zombie.gsc`, `character_zm_zombie_dog.gsc` | `character/` |

GSC module names change accordingly: `maps\mp\gametypes\_coopmode` ->
`maps\_coopmode`, `maps\mp\gametypes\_zombiemode_utility` ->
`maps\_zombiemode_utility`, `maps\mp\animscripts\zombie_shared` ->
`animscripts\zombie_shared`, and so on. Nothing under `maps/mp/` moved; the mp
closure (335 scripts for every MP gametype and level) is byte-identical.

When a branch written against the old layout adds a new file under `coop/` or
`zm/`, move it by the table above and rename its references; the resolver
refuses a tree that still has one.

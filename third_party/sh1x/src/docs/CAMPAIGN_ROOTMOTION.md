# Campaign / co-op script models and native root motion

2026-10-19. This inventory covers every script_model in the co-op (campaign
and Spec Ops) GSC that plays an animation or is moved by script. For each
site it decides whether the site should use the native root-motion method
`codol_animScriptedModel` (see `docs/ENDGAME_TOP3.md`, "Fourth stage:
native root motion").

**Scope.** The tree is the set the coop loader actually compiles:

- `maps/_coop*.gsc`
- the level scripts `maps/mp_<map>.gsc` and `maps/coop_<mission>_<map>.gsc`
- `common_scripts/*`

These come from `tools/script_resolve.py --category coop --list-reachable`.
The ~470 stock IW4 SP files in `maps/` (`_drone`, `_anim`, `_vehicle` ...) are
not reachable in a coop match and are not counted.

**Actors are out of scope.** An actor gets root motion from native
`animscripted`.

## Method

Each script-model clip was measured on a probe (udp/17755, own home and exe
copy, `scr_coop_test 1`). The measurement is `getMoveDelta( clip, 0, 1 )` /
`getAngleDelta`, logged by the new `_cooptest` verb
`rootdelta <tree> <clip> ...`.

A clip whose root does not move gains nothing from root motion. For each
site, the IW4 original was checked to see how the scene was driven
(`D:\CoD\COD-GSC-Source\IW4-GSC`):

- `_anim` anim_single, which is animScripted on a non-AI
- `build_drive` / `_vehicle_aianim`, which is setAnim on a vehicle
- the vehicle path system

## Result

| | count |
|---|---|
| Script-model anim sites (calls that play a clip on a script_model) | 10 |
| ... converted to `codol_animScriptedModel` | 0 (no clip at these sites has root motion that IW4 applies) |
| Script-model mover sites without a clip (moveTo / rotateTo; listed below) | 17 |
| ... converted | 0 (no clip involved: vehicle paths, doors, gates, props) |
| Shared helper converted | 1: `maps/_coopanim.gsc` |

`maps/_coopanim.gsc` is the coop `anim_single` family. A script_model scene
member now goes through `codol_animScriptedModel`, as IW4 `_anim` plays a
non-AI with animScripted.

No mission script puts a character in a scene as a script_model. Every scene
character is an actor (`maps/_coopanim`, the soldier species). No script-model
character stands still or slides on moveTo while a moving clip plays.

### Script-model anim sites (all kept)

| Site | Clip(s) | Measured root delta | IW4 original | Decision |
|---|---|---|---|---|
| `_coopintro.gsc` `drive_anims` (training jeep) | `humvee_50cal_driving_idle_forward`, `humvee_antennas_idle_movement` | 0 / 0 | `_humvee` build_drive (setAnim loop) | keep: body sway in place; the jeep follows its vehicle path |
| `_coopintro.gsc` `door_anim` | `uaz_driver_exit_into_run_door` | 0 | `_vehicle_aianim` guy_unload (setflaggedanimrestart on the vehicle) | keep: door only |
| `common_scripts/_heli_rotors.gsc` `spin_rotors` (every helicopter mover; `_coopevents` `play_drive_anim`) | `mi17_heli_rotors`, `mi28_rotors`, `bh_rotors` | 0 | build_drive rotor loop | keep: rotor loop |
| same | `sniper_escape_ch46_rotors` | **2932 units in 0.3 s** (2901, 63, 421) | `_seaknight` build_drive (setAnim) | keep, and **never** route it through root motion. The delta is junk in a rotor loop; applying it would throw the CH-46 about 3000 units every 0.3 s. |
| `_coopmission_take_fav.gsc` `van_pullup`, `van_scene` (wheels) | `uaz_driving_idle_forward` | 0 | `_uaz` build_drive | keep |
| `_coopmission_take_fav.gsc` `technical` (wheels) | `technical_driving_idle_forward` | 0 | `_technical` build_drive | keep |
| `_coopmission_take_fav.gsc` `van_pullup`, `van_scene` | `favela_van_shootout_van` (19.2 s) | 0 | `favela.gsc` 499: `van anim_single_solo( van, "door_open" )`, animScripted at the van's own spot after it drove its path | keep: animScripted with zero root motion is the same as setAnim. The van's arrival is a vehicle path in IW4 (`spawn_vehicle_from_targetname_and_drive( "van" )`), not part of the clip. |
| `_coopmission_take_fav.gsc` `cars_scene` | `favela_van_shootout_car1/2_passenger_door_open_only` | 0 / 0 | `favela_code.gsc` 630 setAnim | keep |
| `common_scripts/_destructible.gsc` `destructible_anim` | destruction / fan / swing / locker clips | not applicable (in-place prop clips) | the same file in IW4 (setanimknob / scriptModelPlayAnim) | keep: intentionally static |

### Script-model movers without a clip (all kept)

These are moved by script and play no clip that carries the motion. Their IW4
originals are the vehicle path system or plain moveTo/rotateTo. Root motion
has nothing to replace here.

- **Vehicle path followers**: `_coopvehicle.gsc` `follow_path`, used by:
  - the training jeep and the prolog CH-46 (`_coopintro`)
  - the estate zodiac (`_coopmission_estate` `stage_boat`)
  - the Mi-17 / Mi-28 groups (`_coopevents` `vehicle_run`)
  - the favela technical
- **Paths written as moveTo legs**:
  - `_coopmission_defense_crash` `extraction_chopper`: the Black Hawk's
    path legs and the run-in to the roof
  - `_coopevents` `hover_for_ropes`: the hover height
  - `_coopmission_take_fav`:
    - `van_pullup`, `van_scene`: the van's path is missing from the addon,
      so it drives two straight legs
    - `van_blocker`: IW4 `move_van`
- **Doors, gates and targets**:
  - `_cooptraining`: `open_doors`, the slide gate, `open_range_door`,
    `raise_door`
  - `_coopcourse` target pivots
  - `_coopmission_defense_crash` `open_price_door`
- **common_scripts**: `_dynamic_world` (jet fly-by parts, soda can, copier
  bar, swinging doors) and `_elevator`
- **Static props**:
  - the revive marker (`_coop`)
  - the defense case and bomb
  - the estate laptop and charge

## What changed

**`maps/_coopanim.gsc`.** A scene member that is a script_model, tested with
`is_model`, now gets IW4 `_anim`'s non-AI behaviour:

- `anim_single_solo` / `play_anim` / `anim_single` go to `play_model_anim`.
  It calls
  `codol_animScriptedModel( "coop_scene", getStartOrigin( node ), getStartAngles( node ), clip, 1, 0.2 )`,
  or plays the clip from the model's own spot when there is no node. The model
  holds the clip's last position afterwards.
- `place_at_start` / `anim_first_frame_solo` place the model at the scene's
  start spot. `anim_first_frame_solo` then holds the clip's first frame with
  `setAnimKnobRestart( clip, 1, 0, 0 )`.
- `release` calls `codol_stopAnimScriptedModel`.

A future scene that puts a prop or a script-model character into a scene can
call the same helpers it would call for an actor.

`anim_loop_solo` remains actor-only (the soldier_stop cast pose).

**`maps/_cooptest.gsc`** (probe harness, inert without `scr_coop_test 1`) has
three new verbs:

- `rootdelta <tree> <clip> ...`: logs each clip's length, root move and yaw.
- `rootplay <tree> <clip> <model|ai> [x y z yaw]`: a script_model plays the
  clip through `codol_animScriptedModel`.
- `rootscene ... x y z yaw`: the same through `maps\_coopanim::play_anim` with
  that spot as the scene node.

For `<model>`, `ai` takes the body of a live enemy. A model that is not
precached is a fatal script error.

## Verification (probe udp/17755, exe copied from bin/sh1x_dedrun.exe = server main 027639b2)

- `tools/script_resolve.py --category coop` exits 0 for all nine
  playlists:
  - `coop_defense_mp_crash_tropical`
  - `coop_prolog_mp_crash_intro`
  - `coop_spec1_mp_killhouse_sh`
  - `coop_spec_mp_crash_tropical`
  - `coop_spec_mp_estate_tropical`
  - `coop_spec_mp_fav_tropical`
  - `coop_take_mp_estate_tropical`
  - `coop_take_mp_fav_tropical`
  - `coop_training_mp_killhouse_sh`
- Each of the nine was loaded on the probe with a test client. Each load had:
  - zero script compile errors and zero runtime errors
  - the harness active
- Each mission then ran `rootscene generic_human patrol_bored_patrolwalk ai`
  at the player's spot. The run goes through the new `_coopanim` script-model
  path, with `rootMotionTrace 1`.
- In all nine missions the trace printed "begins ... (5133 ms)" and "clip done
  at 1.000 after 5134 ms". The model held the computed clip end to within
  0.01 units, for example 141.9 units from (360.7 2420.2) to (485.5 2487.8) on
  take fav. `release` then stopped the root motion ("stopped after 5634 ms").
- The `anim_first_frame_solo` script-model branch was not exercised. No
  mission calls it on a model yet.

## Actor scenes given their root motion (second pass, 2026-10-19)

A scene played over the cast pose (`"inplace"`) gets no root motion. The actor
slides his feet on one spot and never makes the clip's turn. These scenes now
play through `maps\_coopanim::anim_single_root` / `play_root`, which is
animscripted `zonly_physics`.

- **Start.** The clip starts where the actor stands, or from its IW4 scene
  node.
- **Path check.** The clip's root path is checked first, in tenths, by
  `root_path_clear`:
  - no wall between samples (a physicsTrace at 36 and 64 units)
  - a floor under each sample
- **Fallback.** A blocked path is logged and the clip is played in place, as
  before.
- **Stopping a scene.** `stop_scripted` ends a scene early. `settle_scene`
  waits for the animscript to restart before an actor is put on a ride. If the
  ride starts sooner, the old clip stays on him through it.

| Clip | Delta | Site | Now | Probe result (udp/17755) |
|---|---|---|---|---|
| `crash_flash_price_peek` | out 43 and back | crash intro, Ghost | played at its scene node `price_peek_point` (1660 -1820.5 222, yaw 0). Both crash clips start 46 units behind that node (MEASURED) | ends 14 units from its start |
| `crash_flash_price_throw` | 44 units, yaw -174 | crash intro, Ghost | from where the peek leaves him | 46 units, ends (1605 -1878) |
| `village_interrogationb_price` | 38 units | crash intro, the puncher | from his spot; a flash reaction gesture ends it | clear, from (1439 -1423) toward (1471 -1404) |
| `breach_react_guntoss_v2_guy1` | 32 units, yaw -130 | crash intro, one watcher | from his spot | blocked by the room wall at 50%; played in place (correct) |
| `crash_flash_react_ortiz` | 71 units | crash intro, the boss | from his spot, then he runs | (1467 -1477) to (1443 -1411) |
| `hunted_open_barndoor_flathand` | 76 units, yaw 109 | crash, Ghost at price_door | he turns east at the foot of the stairs; the clip carries him north through the doorway; the walk out waits for the clip | (1657 -1816) to (1674 -1741), through the door gap (x 1635..1705, y -1784); the run out then plays run_lowready_f |
| `estate_ghost_radio` | about 120 units in its first 5 s | crash, Ghost during Price's lines | for the line's length, from his spot; skipped while another scene plays | (1628 -1870) to (1542 -1780) clear. Blocked at his intro spot, so played in place there. |
| `crash_search_wreckage_crouch_end` | 24 units, yaw 180 | crash, the wreck searchers | from their spots | 23 units, both searchers |
| `killhouse_gaz_intro` | 25 units, yaw 127 | training (Gaz at the range), spec1 (tower Price) | from his spot, then the idle resumes | training (1372 -1115) to (1395 -1104); spec1 tower 25 units to (3745 -2513) on the tower floor |
| `breach_flash_r1_enter` | 115 units, yaw -143 | estate breach | It is IW4 `_slowmo_breach`'s way IN after the blast, not a step into the stack: r1_idle starts on the same spot (MEASURED). Ghost now holds the stack idle at `checkpoint_breach_ghost`. At the blast (+1.6 s) the clip runs him from there to the blown doorway, turning to face it. Then he fights. | `pathcheck` from (768 734 41) yaw 230: clear, ends (657 764) facing 87, in front of the door (obj_house_breach 690 792) |

Kept in place on purpose:

- `estate_intro_3rd_character_right` (241 units): Ghost rides the zodiac.
- `crash_body_drag_enemy`: 0 delta.

**Mover-carried walks.** These now go at the clip's own pace (`clip_pace`:
root travel over length):

- estate `ghost_up_the_beach`: 27.65 u/s, was 30
- crash `ghost_walk`: 181.8 u/s, was 150

Both follow hand-placed polylines, so they stay mover rides.

`_cooptest` has new verbs:

- `where <targetname>`: scene-node structs
- `pathcheck <clip> x y z yaw`
- `searchers`: the crash wreck searchers
- `stacktest`
- `rootdelta` now also prints the path in tenths and the start offset

## Left over

**IW4 model scenes that CoDOL does not stage at all.** These are candidates
for the new `_coopanim` script-model path if they are ported later:

- the interrogation phone prop `village_interrogationb_phone` (=script_model
  clip in the defense crash animlist; the boss holds the common phone idle
  instead)
- IW4 favela's torture cables, curtain pulldown and hula girl, and estate's
  ending Pave Low / player body: parts of IW4 favela and estate that the
  CoDOL missions do not include

# zm_zombie_training: the training mission, reconstructed from the map

Date: 2026-10-07. Scope: `bin/home/codol_rawfiles/zm/maps/mp/zm_zombie_training.gsc`
(the mission), plus two hooks in shared zm scripts (`_zombiemode.gsc`
`getSpawnPoint`, `_zombiemode_extraction.gsc` `extraction_horde`).

The user report: players start in an elevator whose doors never open. The
cause: zm_zombie_training is not a round map. It is a scripted tutorial
(C1's "Iron Zombie" challenge, `zm_zombie_training_1`), and no CoDOL zone
ships the level script that ran it. The generic zm loop spawned players in
the lift (`mp_tdm_spawn_allies_start`) and waited for round spawning that the
map's spawners cannot feed.

Nothing of the original mission script survives. This document rebuilds the
flow from the entity string (878 entities, `zm_zombie_training.ff` MapEnts),
the brush bounds measured in-game (`getAbsMins`/`getAbsMaxs`), and the asset
names only this map's zone carries. Extracted game text is not reproduced
here; localize keys are named instead.

## 1. Evidence that fixes the order

**Localize keys only in `zm_zombie_training.ff`** (docs/zone_assets/ALL_localize.txt,
set difference against the Prototype and Asylum zones): `ZOMBIE_TUTORIAL_NEW_*`:
`WELCOME_01..07`, `DOOR1_*` (OPEN_IT, OPENED, WALLGUN_INTRO_01/02, BOUGHT_GUN,
NOT_OPEN, TURN_BACK, REBUKE, RANDOM_SHOOT, AMMO_BELOW_TEN), `DOOR2_*`
(1ST_CYBORG, KILLED, PROCEED, LMB, NOT_KILLED_IN_TEN, NOT_AT_DOOR_IN_TEN),
`DOOR3_*` (MUSEUM, MUSEUM_A_01..03, MUSEUM_B_01..02, MUSEUM_C_01..02,
MUSEUM_GUIDE_01..03, WALLGUN, BOUGHT_GUN, SWITCH_WEAPON, SWITCH_SUCCESS,
OPENED, TO_DOOR, WAIT_LONG, RANDOM_SHOOT), `DOOR4_*` (TIPS, TO_WINDOW,
REPAIR_WINDOW, WARNING, REMINDER, LAST_KILLED, PROCEED), `DOOR5_*`
(DEBRIS_FOUND, DEBRIS_NEED_CLEAR, DEBRIS_CLEARED, IN_STAIRS, NO_ACTION,
LEVER_UP, SPEED_SODA, DRINKING, NOT_DRINKING, REMINDER, PROCEED, NOT_MOVING),
`DOOR6_*` (ENTERING, INSTRUCTION, ENCOURAGE, KILLING_CYBORG, ONE_LEFT,
CONGRATS, PROCEED), `END_*` (IN_CORRIDOR, LEAVING_DOME, DISTANT,
NOT_MOVING_1..3), `OPEN_MAGIC_BOX`, `GET_RAY_GUN`, `NOT_TAKE_RAY_GUN`,
`ZAPPER_KILL`, `TOKEN_INSTRUCTION`. Read in the English table, the key names
and their lines give the stage order below: welcome in the lift, open the
first door, buy a wall gun, kill a caged cyborg, the museum and a second
gun with a weapon-switch drill, window repair and a first wave, debris /
power / Speed Cola, a survival test in the last room, a corridor out of the
dome, a helicopter.

**Sound aliases only in this zone** (docs/zone_assets/ALL_sound.txt; loop
flag read from the zone's alias records): `cbg_tut_elevator_down` (the lift
ride), `cbg_tut_energy_door_open` (the `elec_door_*` barriers),
`cbg_tut_final_gate_open`, `cbg_tut_claw_up` (the display cyborg),
`cbg_tut_museum_click`, `cbg_tut_switch_open`, `cbg_tut_direct_arrow_lp`
(loop; the navigation arrows), `cbg_tut_amb_*` (room ambience; the
`final_gate_rumble`, `final_wind` and `final_outside` ones are loops),
`cyborg_tutorial_m_vo_001..004` (museum narration),
`cyborg_tutorial_vo_NNN` (41 instructor lines), `mus_cyborg_final_run`.

**Navigation arrows**: script_brushmodels `arrows_to_first .. _twelfth`
(script_noteworthy `navigation_arrows`), floor or wall decals laid along the
route in order; their bounds are the route itself (section 3).

## 2. Entity catalogue (what the mission uses)

Coordinates are origins; brush extents are the measured absmin..absmax.
Floors: ground floor about z 225-245, upper floor z 433, outside 433-477.

| Ent(s) | Where | Role |
|---|---|---|
| `mp_tdm_spawn_allies_start` (1482 -2746 233, yaw 225); `initial_spawn_points` struct (1458 -2728 233) | inside the lift | the start; the other three `initial_spawn_points` and `info_player_start` are Prototype's, at Prototype's coordinates |
| `right_lift_door` *72 + *75, `left_lift_door` *73 + *74 | door line (1338 -2762)..(1415 -2843), z 222-329 | the lift doors; closed |
| `right_door_origin` (1327 -2735), `left_door_origin` (1441 -2853) | beside the doors | each leaf's pocket (open position) |
| `lift_sound` (1383 -2803), `first_tele` (1375 -2735) | lift | ride sound; stage marker |
| `arrows_to_first` (326..1245, -2643..-1724) | hallway floor | lift to gate |
| zombie_door `door2_tutorial` (179 -1790, cost 1000) -> `zm_main_gate` *30 *35 *36 *37 (slide_apart +-45 45) | gate | first purchase |
| `first_door_blocker` *45 (134..243, -1834..-1726) | same opening | a second blocker in the gate opening; nothing in the generic scripts removes it |
| `zm_main_gate_left` / `_right` (slide inward), `door1_opening`, `door1_close`, `se_hut_blocker` (rotate, z 380) | gate | a closing pair and markers; unused (section 6) |
| `exterior_vision` / `interior_vision` trigger_multiples | either side of the gate | vision switch triggers; unused |
| weapon_upgrade `spas12_zm` (-26 -1633 298), `weapon_point` / `refill_point` | gun room | the wall gun |
| `arrows_to_second` (-47..100, -1828..-1562) | gun room floor | to the wall gun |
| `elec_door_01` *88 (57..192, y -1562..-1554, z 236..364), `elec_point_01` (z 161), `elec_sound` | gun room / display room | energy door; sinks to elec_point_01 |
| `start_zone_spawners` struct (125.4 -1246.9 272.6, yaw 270), `zombie_barrier` platform model (124 -1248 255), three `zombie_clip` models, `kill_point`, `face_point`, `barrier_point` | display room | the caged cyborg on its platform |
| `arrows_to_fourth` (-80..333, -1049..-921) | display room | to the museum door |
| zombie_door `door3_tutorial` (131 -917) -> `zm_door_01` *1 *38 *39 | display/museum | second purchase |
| `button_1..3` (38, -605 / -192 / 167, z 264), `male_vo_1..3` (-151, -596 / -196 / 172) | museum wall | the three exhibits and their narration points |
| `arrows_to_sixth` (117..140, -896..436), `arrows_to_fifth` (233..256, 432..436, z 307..432) | museum | museum floor; the mp5k wall |
| weapon_upgrade `mp5k_zm` (243 430 297), `stakeout_point` | museum end | the second gun |
| `elec_door_03` *48 (61..197, 448..455) in front of zombie_door `door4_tutorial` (120 443) -> `zm_door_02` *32 *40; `zm_door_02_left/_right` (slide inward), `door3_back`, `door4_back` | museum/repair room | energy door, then the third purchase |
| exterior_goal `exterior_goal_tutorial` (565 1335 272) -> barrier `pf4_auto1` (6 boards + clip), `broken_window` trigger_radius (483 1246, r 200), `repair_point`, `arrows_to_seventh` | repair room | the window to repair |
| `window_spawners` structs (590 1676 256 -> node pf0_auto501; 961 1368 256 -> pf0_auto513) | yard behind it | the first wave |
| zombie_debris `north_upstairs_debris` (-315 1333, cost 1000, flag north_upstairs_blocker) -> obstacle model + clip | repair room | debris |
| `stairs_point` trigger_radius (-545 1572 356), `arrows_to_eighth`, `arrows_to_power` | stairs (east flight along y 1700 z 337..389, north flight along x -350 to z 433) | to the upper floor |
| `elec_door_04` *49 (-272..-174, 1861..1958, z 427..556) | top of the stairs | energy door |
| `use_elec_switch` (-18 2041 481), `elec_switch` lever, `arrows_to_nineth` | power room | the power switch (handled by `_zombiemode_power`) |
| zombie_vending `specialty_fastreload` (-26 2339) + `arrows_to_tenth` | power room | Speed Cola |
| `elec_door_02` *86 (-122..72, 2801..2808, z 426..554), `elec_point_02` (z 322), `elec_point_03` / `_sound` | power room / final room | energy door |
| info_volume `final_zone` (-538 2820 415)..(514 3859 564) | final room | |
| windows: `pf6_auto1` east (348 3702), `pf5_auto1` north (-21 3848), `pf7_auto1` west (-399 3700); trigger_radius `window1` (313 3661), `window4` (-23 3808), `window3` (-346 3648), `window2` (-21 3560, r 150); `arrows_to_eleventh_1/_2/_3` | final room | the three windows |
| spawner groups `window1_wave1..3`, `window2_wave1..3`, `window3_wave1..3` (2/2/3 structs per window per wave; targets pf0_auto502/503, 504, 505) | outside those windows | three waves: 6, 6, 9 |
| `treasure_chest_use` (-20 3270), `zombie_vending` doubletap (465 3329) | final room | box and Double Tap |
| `bot_spawn` (337 3488), `bot_killer` spawners (336 3613), `dumb_*` animated-NPC markers | final room / around the map | not used (section 6) |
| zombie_door `door5_tutorial` (-535 3325) -> `zm_door_03` *33 *34 *41 *42, `elec_door_fk` (3 brushes, x -543..-536), `final_door_hint`, `arrows_to_twelfth` | final room west wall | energy door, then the last purchase |
| corridor (y ~3270..3380 westward, then diagonal NW to `guide_point` trigger_radius -936 3485) | | |
| `elec_door_05` (3 brushes, -1018..-895, 3478..3545), `zapper_enter` brush (z 585..757, raised) + model, `zapper_door_down` | zapper room south entry | energy door; the raised door could close behind |
| `zombie_trap` trigger_multiple (script_string `fk`) -> `pf0_auto407` trigger + structs, `fk_trap_fx` structs, `last_stand_point` (-964 3774) | zapper room | the trap |
| `zapper_exit` brush *50 (z 432..604) + model (-969 3830 448), `zapper_up_point` (592), `zapper_down_point` | zapper room north exit | rises 144 |
| `inside_vision` / `outside_vision`, `heli_spawn_point` trigger_radius (-961 4302), `final_door` model (-969 4294 448), `final_door_up` (592) / `_down`, `final_door_trigger`, `wind_blow` | north corridor | the final door rises 144 |
| `rescue_dest` (-737 5236 508), `final_point` trigger_radius (-776 5184), `light_blue_steady_FX_origin` x4 | outside | landing zone |
| `ending_heli_start` (418 6315 1151, blackhawk_minigun) -> pf0_auto514 -> `path_a` (stop, -608 5375 655) -> pf0_auto521 | sky | the rescue route (already used by `_zombiemode_extraction`) |
| `heli_start` x3 / `heli_loop_start` chains, `heli_crash_start`, `heli_leave` | around the dome | ambient flybys; not used |

Prototype leftovers in the same entity string (Prototype coordinates, no
training floor under most of them): `start_zone_spawners_dog`,
`upstairs_zone_spawners`, `box_zone_spawners`, the `start_2_upstairs` /
`start_2_box` / `box_2_upstairs` debris and door triggers, the m14ebr, m16,
pp2000, tmp, second spas12 and l96a1 cabinet wall buys, `zombie_care_package`.

## 3. The reconstructed flow

| Stage | Steps (ents) | Lines (keys) |
|---|---|---|
| 0 lift | spawn in the lift; `cbg_tut_elevator_down` with a small quake; the two leaves slide into their pockets | WELCOME_01, 02 |
| 1 gate | `arrows_to_first`; purse topped up to the gate's cost ("preloaded"); buy `door2_tutorial`; `first_door_blocker` removed with it | WELCOME_03..07, DOOR1_OPEN_IT near the gate, nags TURN_BACK / NOT_OPEN |
| 2 gun | `arrows_to_second`; buy the spas12; `elec_door_01` sinks to `elec_point_01` | DOOR1_OPENED, WALLGUN_INTRO_01/02, BOUGHT_GUN |
| 3 display | the cyborg on the platform (`start_zone_spawners` struct, frozen, `cbg_tut_claw_up`); kill it; `arrows_to_fourth`; buy `door3_tutorial` | DOOR2_1ST_CYBORG, NOT_KILLED_IN_TEN, LMB, KILLED, NOT_AT_DOOR_IN_TEN |
| 4 museum | exhibits: walking up to `button_N` plays `cbg_tut_museum_click` and `cyborg_tutorial_m_vo_00N` at `male_vo_N`; `arrows_to_sixth` + `_fifth`; buy the mp5k; switch weapons twice; `elec_door_03` sinks; buy `door4_tutorial` | DOOR3_MUSEUM, MUSEUM_GUIDE_0N + MUSEUM_A/B/C per exhibit, WALLGUN, TO_DOOR, WAIT_LONG, BOUGHT_GUN, SWITCH_WEAPON, SWITCH_SUCCESS, OPENED |
| 5 repair | the tutorial window starts broken; `arrows_to_seventh`; reach `broken_window`; repair all boards; 4 zombies from `window_spawners` climb in through it; kill them | DOOR4_TIPS, TO_WINDOW, REPAIR_WINDOW, REMINDER, WARNING, LAST_KILLED, PROCEED |
| 6 power | `arrows_to_eighth`; buy the debris; `arrows_to_power`; reach `stairs_point`; `elec_door_04` sinks; `arrows_to_nineth`; throw the switch; `arrows_to_tenth`; buy Speed Cola; `elec_door_02` sinks | DOOR5_DEBRIS_FOUND, NEED_CLEAR, CLEARED, IN_STAIRS, NO_ACTION, REMINDER, LEVER_UP, SPEED_SODA, NOT_DRINKING, DRINKING, PROCEED |
| 7 final room | enter `final_zone`; find the three windows (window1 / window4 / window3, with `arrows_to_eleventh_1/2/3`); waves 1..3 from `windowN_waveK`; the box is offered; `elec_door_fk` sinks; `arrows_to_twelfth`; buy `door5_tutorial` | DOOR5_NOT_MOVING, DOOR6_ENTERING, INSTRUCTION, ENCOURAGE, KILLING_CYBORG, ONE_LEFT, CONGRATS, OPEN_MAGIC_BOX, DOOR6_PROCEED, END_NOT_MOVING_3 |
| 8 exit | `mus_cyborg_final_run`; corridor to `guide_point`; `elec_door_05` sinks; the zapper room fires its trap points (`fk_trap_fx`, `pf0_auto407`) and shocks whoever is in the `zombie_trap` volume; `zapper_exit` rises; reach `heli_spawn_point`; `final_door` rises (`cbg_tut_final_gate_open`, rumble loop); outside the dome (`outside_vision`: wind loop); the four LZ lights; the rescue on the map's route; lift-off awards `zm_zombie_training_1` | ZAPPER_KILL, TOKEN_INSTRUCTION, END_IN_CORRIDOR, END_NOT_MOVING_1/2, END_LEAVING_DOME, END_DISTANT |

Lines are shown on one subtitle hud element and queued, so a stage never
waits on reading. The objective marker is a waypoint with
`icon_objective_onscreen` (common_gametype_zm). Every purchase step tops
each player's purse up to that step's cost.

Where the localize keys disagree with the geometry: the `DOOR5`/`DOOR6` keys
read as if a purchasable door led into the last room, but the only way in
from the power room is `elec_door_02`, and `door5_tutorial` is in the last
room's west wall, leading out to the zapper corridor. The flow follows the
geometry: `elec_door_02` opens after the drink, the waves are fought inside,
and `door5_tutorial` (behind `elec_door_fk`) is the way out.

## 4. What is implemented

- `zm_zombie_training.gsc` runs the flow above as the round loop
  (`level.round_think_func`, T5's hook): no round spawning, no hound rounds
  (they wait on `between_round_over`), no boss (it waits on `start_of_round`);
  `level.zm_round_cap = 0`. Every zombie is spawned by a stage through
  `_zombiemode_spawner::spawn_zombie` from the map's own structs, in that
  stage's area: the frozen display cyborg (zombie_think stopped with
  `zombie_delete`, goal on its own origin), the repair wave, the three
  final-room waves (150/250/350 health, walkers then faster).
- Spawning: `getSpawnPoint` asks `level.zm_spawn_point_func` first. Stage 0
  is the lift (the two authored points plus two inside the car, skipping any
  that would telefrag); later stages use a floor point inside the stage's
  area. A player dead at a stage change respawns there (the map has no
  rounds to respawn on).
- The rescue: `extraction_start()` at the end, the arrival shortened to 15 s
  unless `scr_zm_extract_arrive` is set, and no horde
  (`level.zm_extraction_horde_func`).
- Developer verbs, `scr_zt_test` (developer > 0 only): `status`, `skip`
  (completes the current objective through the real door/debris/weapon/perk
  paths, unpaid), `goto <client> [stage]`, `trace`, `walk`, `profile`.

## 5. Navigation

The map's 334 path nodes form two areas (docs/ZM_PATH_NODES_20260921.md
section 5): the repair room and its yard (z ~245) and the upper floor from the
power room through the final room and the yard behind its windows (z ~433).
Those are exactly the two stages with walking zombies. The start, the
hallway, the gun and display rooms, the museum, the stairs, the zapper room
and the outside have no nodes, and no stage puts a walking zombie there; the
display cyborg does not move. Nothing native was changed.

## 6. Verified (private probe, udp/17779, developer 1, a test client)

- Lift: before the doors open a playerPhysicsTrace from the spawn out of the
  car stops at the wall; after, lift -> landing -> hallway -> gate is clear.
  Every door, energy door and raised door opens on its step, and stepping
  walks / traces pass each opening once it is open (gate, elec_door_01,
  museum door, elec_door_03 + door4, debris, stairs top through elec_door_04,
  elec_door_02, door5 + elec_door_fk, corridor diagonal to `guide_point`,
  zapper room, zapper_exit, north corridor, final door).
- The display cyborg stands on its platform (moved 0/s) and dies to the skip.
- Repair wave: the four zombies walk from the yard to the tutorial window,
  tear it open ("opened the window at (565.355, 1335.86, 272)"), climb in and
  reach the player (distance 30) at both the room centre and the window.
- Final room: wave 1's six zombies tear the three windows, climb in and reach
  the player (distance 30); waves 2 and 3 spawn 6 and 9.
- Natural detection (no skip): the broken-window approach, the stairs point,
  final-room entry, the three windows, the guide point, the zapper room and
  the final-door point all advance the flow when the client stands there.
- Exit: the final door opens, the helicopter spawns at `ending_heli_start`,
  flies to `path_a`; a client standing in the landing zone at lift-off is
  aboard; "WIN -- 1 extracted"; "mission complete".
- No script runtime error in any run. zm_prototype_sh and zm_asylum_sh load,
  start round 1 and spawn zombies with the same scripts.

## 7. Open questions

- The outside terrain between the final door and the landing zone has 20-50
  unit ledges and rubble on the straight lines a stepping walker tried; a
  diagonal ramp (443 -> 459 -> 477 around (-850 5050)) exists. The landing
  zone radius (220) already covers (-850 5060). A real client walk is the
  remaining check.
- `ZAPPER_KILL` / `TOKEN_INSTRUCTION`: the original presumably downed the
  player in the zapper room to teach the revive token. Here the room shocks
  without downing (a solo down with no token consumer would end the match).
- `bot_spawn` / `bot_killer`: probably a scripted teammate to revive; not
  implemented.
- The instructor NPCs (`dumb_*`, with xanims killhouse_price_stand_talk_zm,
  training_intro_translator_begining_zm, laptop_officer_idle, ...) need a
  script-model animtree this server does not have.
- The 41 instructor voice lines (`cyborg_tutorial_vo_NNN`) are not mapped to
  keys; only the museum narration is placed.
- `zm_main_gate_left/_right` and `zm_door_02_left/_right` slide inward (they
  close the doorway behind the player); not closed, so a late or respawned
  player is never shut out. `GET_RAY_GUN` / `NOT_TAKE_RAY_GUN` would need a
  forced box roll; the box is offered but not rigged.

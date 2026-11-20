# Zombies hellhounds: T5 parity audit (2026-10-15)

Scope: the zm hellhound ("zombie_dog" actor species) against Black Ops 1 /
T5. References:

- T5 GSC: `D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\_zombiemode_ai_dogs.gsc`
  (cited below as `dogs:<line>`), `animscripts\zombie_dog_*.gsc`
  (`zdinit:`, `zdmove:`, `zdcombat:`, `zddeath:`, `zdpain:`),
  `maps\_zombiemode.gsc` (`zm:`), `clientscripts\zombie_cod5_*_fx.csc`.
- T5 engine: `D:\CoD\OpenBLOPS\Game\Server\game\actor_threat.cpp`,
  `actor_fields.cpp`, `game_mp\actor_mp.cpp`.
- CoDOL: `bin/home/codol_rawfiles/maps/_zombiemode_ai_dogs.gsc`,
  `animscripts/zombie_dog_*.gsc`, `character/character_zm_zombie_dog.gsc`,
  `src/online/player/actor_lifecycle.cpp`, `dog_states.cpp`.

Verification was on a probe server (udp/17745, own fs_homepath,
developer 1) on zm_prototype_sh and zm_sumpf_sh with test clients:
`scr_fftest addbot / invuln / zdmg`, `scr_zm_extest dogs / endround /
killdog`, `scr_dogAttackLog 1`. No C1 client was attached, so the
visual and audio items below are verified as server-side events only.

## Status by area

| Area | T5 | CoDOL before | Now |
|---|---|---|---|
| Round scheduling | first hound round random 5..7, then +4..5 (`dogs:446`, `:471`) | endless: same; finite tiers: rounds 6/10/14 as the tier length allows (C1 1/2/3 hound rounds) | unchanged (the finite schedule is C1's tier design, kept) |
| Round start | `dog_round_start`: music `dog_start`, `clientnotify dog_start` fog, AI melee dvars (`dogs:488-517`) | music stinger, C1 `ZOMBIE_WARNING_DOG_ARRIVED` text, HUD "Special round"; fog only on zm_sumpf_sh (map script) | unchanged; `dog_clips` flag added |
| Intro sounds | `zmb_dog_round_start` local, +1 s announcer `zmb_vox_ann_dogstart`, +3.5 s a player's `spawn_dog` line (`dogs:542-555`) | announcer only | unchanged: **no C1 zone ships `zmb_dog_round_start` or any player hound line** (checked every zm zone's alias strings); documented in place |
| Count / concurrency | `players*6` (rounds 1-2), else `*8`; at most `2 * valid players` alive (`dogs:141-172`) | same | same |
| Spawn interval | 3 / 2.5 / 2 / 1.5 s minus `count/max` (`dogs:217-242`) | same, scaled by the C1 tier (x1.2 easy, x0.8 hard) | unchanged (tier scaling is C1's) |
| Spawn location | Factory logic: random zone `_dog` struct 400..1000 units from the quarry, never the previous one (`dogs:352-373`) | same, plus fallbacks (nearest > 300, window insides, path nodes) for maps whose structs are unusable | unchanged |
| Spawn FX | `lightning_dog_spawn` (buildup) + `zmb_hellhound_prespawn`, 1.5 s, `zmb_hellhound_bolt`, earthquake 0.5/0.75/1000, rumble `explosion_generic`, `zmb_hellhound_spawn` (`dogs:281-288`) | also played `fx_zombie_dog_lightning_spawn` (no T5 script plays it); no rumble | **fixed**: T5's set exactly, rumble added |
| Reveal | hidden + magic bullet shield, revealed 0.1 s after the strike, `ignoreme` cleared, `"visible"` (`dogs:301-307`) | spawned into the strike, visible at once | **fixed**: spawned after the strike (equivalent on screen), `ignoreall/ignoreme` held until the reveal, `"visible"` notified; eye glow and trail start on it (`dogs:929-953`) |
| Health | 400 / 900 / 1300 / 1600, capped 1600 (`dogs:412-437`) | same | same |
| Target selection | `get_favorite_enemy` = least-hunted valid player (`dogs:376-409`), pinned as `favoriteenemy`, which the engine threat pass scores 0x7ffffffe (`actor_threat.cpp:317-321`) | least-hunted player computed, but `favoriteenemy` was a plain script variable; the native pass always took the nearest player | **fixed (native)**: `favoriteenemy` is an actor field; the perception pass prefers it while it is targetable |
| Downed players | last stand sets `ignoreme`; ignored sentients score 0x80000000 (`actor_threat.cpp:245`) | `setDontTarget(true)` in last stand (already native) | verified: when bot1 went down every hound switched to bot0 |
| Movement | `a.movement = "walk"` (trot) at spawn, sprint via `need_to_run`: hurt, or enemy in sight within `scr_dog_run_distance` 500, 64 high, 20 deg yaw, 30 deg pitch (`zdinit:31`, `zdmove:104-110`, `:250-317`) | always sprinting (MP dog's `"run"`) | **fixed**: trot -> sprint per T5 (probe: walk->run transitions logged) |
| Arrivals / exits | disabled in `zombie_setup_attack_properties_dog` (`dogs:759-760`) | enabled | **fixed** |
| Pain | `allowpain = false`, `a.disablePain` (`dogs:566`, `:619`, `zdpain:7`); engine gate `actor_mp.cpp:2080` | every hit ran the pain animscript (hounds flinched) | **fixed (native + GSC)**: `allowpain` actor field gates the native pain push; the pain script returns early. Probe: three m16 hits, 0 pain animscripts |
| Melee | `meleeAttackDist` 64 for players (`zdinit:215`, `dogs:756`), run_attack lunge, bite on `dog_melee` notetrack, `dog_bite` shellshock 0.35, 40 damage (`dogs:625`) | 102 (MP dog), miss played the MP `attack_player_miss` recovery | **fixed**: 64; a missed bite just ends the lunge (`zdcombat:71-74`). Probe: 47/47 and 42/42 attempts hit an invulnerable target |
| Low attack | prone or blocked jump -> `run_attack_low` (`zdcombat:169-187`, `:299-320`) | same | same |
| Fire trail | 50% fire (`zmb_hellhound_loop_fire`), else ash (`_loop_breath`) on tag_origin (`dogs:650-672`) | same | same; now started on reveal and stopped on death |
| Eye glow | `fx_zombie_dog_eyes` on `J_EyeBall_LE` (`dogs:639-647`, `:947`) | played on the bone at spawn, never stopped | **fixed**: starts on reveal, `stopFxOnTag` on death |
| Death | fire hounds (`a.nodeath`) burst (`dog_explode_fx`: `fx_zombie_dog_explosion` + `zmb_hellhound_explode`, no damage) and are deleted; ash hounds play the directional death and `zmb_hellhound_vocals_death` (`dogs:674-742`, `zddeath:7-13`) | **every** hound burst; no death yelp | **fixed**: per T5. The burst deals no damage, as in T5 |
| Tesla death | five `zombie_dog_tesla_death_*` clips (`zddeath:383-395`) | not available | **fixed (native)**: states `death_tesla_a..e` bound to the five clips (they are in common_zombie.animlist's `=zombie_dog` group). Probe: `setanimstate 'death_tesla_d'` after a tesla kill |
| Kill points | `player_add_points("death", mod, loc)` (`dogs:695`); 20% `kill_hellhound` line (`dogs:697`) | points same; no line | points same; **no C1 hound line exists**, documented |
| Dog vox | `dog_stalk_audio` (`zmb_hellhound_vocals_amb` every 1-4 s while trotting, `dogs:955`), `dog_behind_audio` (`zmb_hellhound_vocals_close` when behind a player, `dogs:772`) | the MP dog's movement-bark state machine (`vocals_amb` up to 20x/s near the enemy) | **fixed**: T5's two loops, MP barks removed |
| Round end | `round_wait` hound arm waits 1 + 7 s, then while `dog_intermission` (`zm:4146-4155`); aftermath: Max Ammo at the last hound, music `dog_end`, `dog_stop` after 2 s, intermission over 6 s later (`dogs:245-266`) | round ended the instant the last hound died (the next round's countdown ran over the Max Ammo) | **fixed**: `dog_round_wait` installed as `level.round_wait_func` for hound rounds; level notify `dog_stop`. Probe: Max Ammo dropped and grabbed |
| Max Ammo | `specific_powerup_drop("full_ammo", last origin)` | same | same (verified grabbed on both maps) |
| Stuck handling | `round_spawn_failsafe`: below world, or < 24 units in 30 s -> killed (`zm:4059`) | `dog_failsafe`: same, except a hound within 160 units of a valid player is never culled (measured: biting hounds stand still) | unchanged (deliberate) |
| dog_clips | brushes on while a hound lives (`dogs:806-850`) | absent | **ported** (no shipped zm map authors `dog_clips`, so it idles) |
| Thundergun | `thundergun_disintegrate_func` / `knockdown_func` (`dogs:627`, `:969-982`) | absent | **ported** hooks (thundergun_zm ships in zm_prototype_sh; its blast is not ported) |
| Bite weapon | `dog_bite` WeaponDef | `dog_bite_zm`, which zm_sumpf_sh does not ship: the first bite there was a fatal script error | **fixed**: falls back to `zombie_melee` (common_gametype_zm) |
| Mixed rounds | Factory only, round > 15 (`zm:3221-3272`) | absent | not applicable: no CoDOL zm map is Factory |
| Melee dvars | `ai_meleeRange` 100 etc. for the round (`dogs:503-516`) | native actor melee reads its own values | n/a |

## Native changes (need the matching exe)

`src/online/player/actor_lifecycle.cpp` (SHARED actor code, marked in
the struct comment):

- actor field `allowpain` (0x800e, T5 `actor_fields.cpp:93`, default 1):
  `ActorPain` sets `painPending` only while it is set.
- actor field `favoriteenemy` (0x800f, T5 `actor_fields.cpp:92`): entity
  number + use count; undefined clears it. `ActorUpdateVisibleEnemy`
  takes it over every other candidate when it is a live, playing,
  non-`dontTarget`, hostile sentient the actor is aware of (perfect-info
  species, or in sight for the others), per `actor_threat.cpp:317-321`.
  This also affects coop soldiers whose scripts set `favoriteenemy`
  (af_caves), which is the IW4/T5 meaning of the field.

`src/online/player/dog_states.cpp`: clip table grown from 64 to 69
(`DOG_CLIP_COUNT`), states `death_tesla_a..e` (57 states). The MP dog's
table repeats `german_shepherd_death_front` in those slots.

GSC on the `dog-parity` rawfiles branch writes `allowpain` and
`favoriteenemy` and selects `death_tesla_*`; on an older exe the first two
are ordinary script variables (no effect) and the tesla states fail to set.
Ship the GSC and exe together.

## Remaining gaps

- No C1 client view was taken: spawn strike, trail/eye FX, explosion and
  sounds are verified as server events, not on screen.
- `zmb_dog_round_start`, the player `spawn_dog` and `kill_hellhound`
  lines: no C1 asset; dropped.
- Hound-round fog: T5 pulls fog in for Factory/Sumpf (`dog_start_monitor`);
  CoDOL has it for zm_sumpf_sh only. Prototype/Asylum/Training had no T5
  hound rounds, so there is no reference fog for them.
- `ballistic_knife_death` scoring for hounds (`dogs:690`): the event is not
  ported in `_zombiemode_score` (owned by the zombie scripts).
- Tesla shock FX on a hound (`_zombiemode_weap_tesla`, shared weapon
  script): T5 plays it on `J_Spine1` for dogs; CoDOL plays it at
  origin + 48, which is above a hound's back. Suggested change for the
  owner of that script: use `J_Spine1` when `isdog`.
- Trot speed is the `zombie_dog_trot` clip's own root motion (~50 u/s on
  the probe), as T5 moves actors by animation delta; not compared with a
  live T5 capture.

## Animation pass: clip timelines measured against T5 (2026-10-22)

Real T5 (Plutonium t5sp, zombie_cod5_sumpf) and CoDOL (probe, zm_sumpf_sh,
player held at T5's spawn spot 10213,536) were recorded with the t5cap
capture (`tools/t5cap/zz_t5cap.gsc`, regenerated into
`maps/_zombiemode_t5cap.gsc` by `tools/t5cap/mk.py`). New in the
capture: a per-frame hound clip timeline (`D` lines: every zombie_dog clip
whose `getanimtime` moved, so the playing clip, its phase and its rate), the
hound's eye offset during `run_attack`, a kill scenario (`t5cap_dogkill`:
each hound is shot from its front, left, back or right 3 s after it appears)
and a static bite-reach scenario (`t5cap_reach`). Report:
`python tools/t5cap_dogclips.py <log>...`. C1's hound clips are T5's
set under the same names (common_zombie.animlist `=zombie_dog`, 59 tree
entries): trot 1.8 s / 245 u, run 2.0 s / 724 u, run_stop 0.6 s / 70.6 u,
run_attack 1.5 s with sound_dog_attack_jump .111, stop_tracking .156 and
dog_melee .200, the same lengths and notes T5's capture shows.

Scenarios: `frozen` (six hounds at a frozen player, 120 s), `move` (the
player walks (10600,620)-(9900,620) at 150 u/s, 100 s), `kill` (12 hounds,
each shot 3 s after it appears). Before = server main d1b37bd7 / rawfiles
52419f5; after = this pass.

| Behaviour | T5 | CoDOL before | CoDOL after |
|---|---|---|---|
| Trot (stalking) | whole 1.8 s cycles (segments 0.35-1.75 s, rate 1.00) | restarted every 0.2 s by moveLoop and every frame by the walk think: the clip never passed 0.11, the hound slid along in the trot's first frames | whole cycles (segments 0.25-2.3 s, rate 1.00) |
| run_attack rate | 1.00 (hounds of animplaybackrate 0.91-1.08) | 0.96 (0.90-1.08) | 1.00 (0.98-1.00) |
| Attack idles in combat (combatIdle) | rate 1.00 | 0.94-0.95 (0.88-1.00) | 0.98-0.99 |
| Death clip rate | 0.97-0.99 | 1.01-1.09 | 0.97-1.00 |
| Death direction, shot from the left / right / behind | death_hit_left / _right / _back | death_hit_right / death_front / death_front (front hits played death_hit_back) | death_hit_left / _right / _back (6 of 6) |
| Look additives while moving | never (T5 move leaves lookAtTarget commented out) | look_up/left/right moving in 14 (frozen) and 208 (move) frames | 0 |
| Turn clips | played to the end (1.00) | cut at 0.5/0.7 s (turn_90_left ended at 0.86) | to the end (1.00) |
| Stop-script idle choice | growl 67 %, bark 33 % (zombie_dog_stop :110-121) | MP dog's: plain attackidle 33 % | T5's |
| Hound within ai_meleeRange in the stop script | bites (zombie_dog_stop :84-93) | idled until the native combat switch | bites |
| Hound-round melee dvars | ai_meleeRange 100, width 0, height 0; restored after (dogs :503-516, :536) | unchanged | set and restored (probe: 100/0 in the round, 64/20 after) |
| Frame share, frozen: attack / attack idle | 55 % / 39 % | 40 % / 58 % | 63 % / 35 % |
| Bite interval (run_attack to run_attack) | 1400 ms (p10 1250, p90 1550) | 1400 ms | 1450 ms |
| Spawn: strike to the hound moving | +1.5 s strike, shown 0.1 s after, trot/run at once | spawned at the strike, trot at once | unchanged |
| Burning hound death | deleted on the frame it dies | hidden at frame end, deleted after 3 s | deleted at the end of its death frame (follow-up below) |

Native (`src/online/player`):
- `actor_lifecycle.cpp` ActorStateRate: a zm hound plays every native
  state at rate 1 except the `stop_*` idles, which keep animplaybackrate
  (T5 zombie_dog_stop is the only hound script that passes it).
- `dog_states.cpp` BG_Dog_Move_Walk / _Think: the trot restarts only
  when move_walk is entered from another state; the per-frame think never
  restarts it.

GSC (rawfiles): `animscripts/zombie_dog_{shared,death,pain,stop,move,turn}.gsc`,
`maps/_zombiemode_ai_dogs.gsc` (dog_round_start/stop), the MP dog's
`maps/mp/animscripts/dog_{shared,death,pain}.gsc` (same damageyaw
conversion; its tree has one death and one pain clip, so nothing changes on
screen there).

### Bite reach (follow-up, 2026-10-22)

At the bite frame (run_attack 0.20) T5's hound has reared up: its eye, where
the melee trace starts, is 0-6 units ahead of its origin and 66 up. On this
server the same frame puts the eye 36 ahead and 62 up. The pose is C1's own:
a static pose test (`t5cap_pose`: a script_model of zombie_wolf holding each
clip at fixed times) matches T5 bone for bone on run, run_stop, the attack
idles, run_attack_low, run_attack_miss, tesla_death_a, turn_90_left and
death_hit_left, but not on run_attack (j_mainroot +29 units forward at 0.20)
or pain_hit_front. AssetExplorer's `anim-pose` (zombie_dog_run_attack on
zombie_wolf, frame 9) gives the server's numbers (eye 36.4 / 61.9), so C1's
clip really lunges where T5's rears up; the server evaluates it correctly.

So the bite is gated by T5's reach (`zombie_dog_combat.gsc` t5BiteReaches):
it connects only when T5's eye (origin + 3 forward + 66 up) is within
ai_meleeRange of the player's eye with no other body in between (T5's
Melee_Trace hits actors, and from T5's eye a queued hound bites into the
hound in front of it); C1's own trace then decides as before.

| Bites | T5 | before this pass | animation pass | with the gate |
|---|---|---|---|---|
| Frozen player, per hound-minute | 15.0 | 16.6 | 25.3 | 17.3 |
| Frozen, bites landed from 50-59 / 60-69 units | 19 of 25 / 16 of 44 | 6 of 6 / 73 of 78 | 72 of 73 / 69 of 77 | 58 of 80 / 10 of 80 |
| Moving player, per hound-minute | 7.7 | 16.0 | 15.1 | 7.1 |
| Moving, bites landed from 70+ units | 4 of 31 | 59 of 83 | 37 of 51 | 2 of 56 |

The frozen rate stays a little higher: these hounds queue at 59-62 units
(inside the stop-script bite range) where T5's stand at 64-77.

**Burning hounds** are now deleted at the end of their death frame, as T5
deletes them (probe: gone within 50 ms, 0 runtime errors).

### Queue stand-off (follow-up, 2026-10-22)

The hound used T5 MP's four attack spots (actor_dog_exposed.cpp), 64 units
from the player, so every hound stood on a ring 59-62 units out. T5 zombie
mode's hounds take no spot: the first runs in until it touches the player
and the rest queue behind it, each stopped by the one in front, 37-41 units
apart. `ZmHoundCombatGoal` (dog_combat.c) now chases the player
and attacks once touching (36) or once inside meleeAttackDist + 15 with
another body in between; two zm hounds keep 40 units apart.

| Frozen player, six hounds | T5 | before | now |
|---|---|---|---|
| Second-row stand-off, median (p10-p90) | 67 (53-81) | 60 (50-63) | 65 (62-69) |
| Bites per hound-minute | 15.0 | 17.3 | 16.0 |
| Bites landed from 60-69 units | 16 of 44 | 10 of 80 | 44 of 141 |
| Moving player, bites per hound-minute | 7.7 | 7.1 | 8.8 |

T5's third and later hounds queue on in a line (100-137 units); here they
spread round the player at the second-row distance.

### Not matched

- **Traversals.** No capture staged a hound traversal in either game; the
  traverse states now play at rate 1 like T5's dog hops.

# Zombies: T5 (Black Ops) parity for regular zombies

2026-10-15. Scope: the regular zombie (`actor_enemy_zombie_zm`) on
zm_prototype_sh, zm_asylum_sh and zm_sumpf_sh. Hellhounds belong to the dog
parity pass (branch `feat/dog-parity` / rawfiles `dog-parity`).

References:

- **T5 GSC:** `D:\CoD\COD-GSC-Source\T5-STEAM-GSC\`. Abbreviated below as
  `T5 sp:` for `maps/_zombiemode_spawner.gsc`, `T5 zm:` for
  `maps/_zombiemode.gsc`, `T5 pu:` for `maps/_zombiemode_powerups.gsc`, and
  `T5 as/...` for `animscripts/...`.
- **T5 engine:** `D:\CoD\OpenBLOPS`.
- **CoDOL GSC:** `bin/home/codol_rawfiles`, branch `zombie-parity`.
  Abbreviated `C sp:` and so on, with the same short names as above.

Statuses:

| Status | Meaning |
|---|---|
| **FIXED** | Changed on this branch and checked on a probe server |
| **FIXED (src)** | Changed, passes the static resolver, not yet seen in a probe |
| **OPEN** | Still a gap |
| **DESIGN** | A deliberate CoDOL / C1 difference, kept |

## 1. Movement and pathing

| # | Gap | T5 | CoDOL before | Status |
|---|---|---|---|---|
| 1.1 | Window climb stalls | Each `zombie_mantle_over_40` traversal ends on the clip's "end" note. | `ai_zombie_traverse_v2` authors "blend" and "end" at the same frame (t 1.0, frame 44/44). Both notifies went out in one update, so `DoNoteTracks` saw only "blend". The climb then held for the 8 s negotiation timeout, and the zombie stood still inside the window for about 6.5 s. Measured: 24 of 48 climbs timed out. | **FIXED** native (server 756982d8, `xanim_notify_queue.c`). Server notetracks are queued and raised one per script run after the tree update. Prototype soak afterwards: 103/103 climbs complete, mean 1.73 s, longest 2.17 s. |
| 1.2 | Move speed | `moveplaybackrate = 1` for every zombie (`T5 as/zombie_init.gsc:320`). Only `animplaybackrate` is jittered, 0.9-1.1. Travel is at the clip's root motion: walk_v1 36 u/s, run_v2 109, sprint_v1 140 (xanim dump). | `moveplaybackrate` was `animplaybackrate × bucket/0.9`. That gave sprint ×1.56 (about 218 u/s, faster than a sprinting player) and crawler ×0.39. | **FIXED** (C as/zombie_init, C sp `apply_move_speed_scale`, black hole pull) |
| 1.3 | Crawlers re-rolled to full speed | A crawler keeps its crawl cycle (`T5 sp:4782`, `:2948`). | `make_crawler` set `needs_run_update`. About 1 s later `zombie_think` re-rolled the walk/run/sprint bucket and the clip, so crawlers stood back up in their animation set. | **FIXED**. The re-roll block is gone; T5 never re-rolls a living zombie. |
| 1.4 | Run-cycle variants | walk1-7, run1-5, sprint1-3 (`T5 sp:356-372`) | walk1-8, run1-6, sprint1-4. walk8 repeats v9; run6 (`run_v3`) is never played by T5. | **FIXED** |
| 1.5 | Last-zombie sprint | `zombie_speed_up` (`T5 zm:3294`), started for rounds < 10. From round 4, once spawning is nearly done and at most 3 remain, the single last legged zombie switches to a sprint clip. | The function existed but was never started. Its version sprinted every remaining zombie, crawlers included. | **FIXED** |
| 1.6 | Turn rate | `ACTOR_MAX_BODY_YAW_RATE` 300 °/s (`OpenBLOPS actor_orientation.cpp:14`) | 360 °/s for every species (`ActorOrientToDirection`) | **FIXED** native: bipeds turn at 300 °/s; quadrupeds keep 360. **Superseded 2026-10-21:** real T5 SP caps every actor, hounds included, at 220 °/s (`ZOMBIE_T5_MEASURED.md` §1). |
| 1.7 | Chase target / repath | `find_flesh` sets `favoriteenemy` to the closest valid player. `zombie_follow_enemy` re-sets the goal every 0.1 s. A `bad_path` falls back to the player's breadcrumbs, rings of points within 48 units of where he last stood (`T5 sp:3922-4354`, `T5 zm:5442`). | The native chase targets the nearest non-`dontTarget` sentient and re-goals whenever the enemy moves more than 15 units. Downed players are `setDontTarget`. The route ends at the nearest node routable from the zombie among the 8 closest to the player (`ActorNearestWalkableNode`), which covers most of what the crumbs do. | OPEN (medium): an explicit breadcrumb fallback, and the crowd split for more than 12 zombies on one player (`T5 sp:3950`). |
| 1.8 | Crawler collision box | `setPhysParams( 15, 0, 24 )` so a player can jump over a crawler (`T5 sp:2955`) | Not a registered method; the full-height hull is kept | **FIXED** native `setphysparams` (`actor_spawn.cpp`, T5 SP handler semantics). `make_crawler` calls it. **New builtin, so the GSC needs this exe.** |
| 1.9 | Spawn position | Spawns at the authored spawner (`T5 utility spawn_zombie`) | `near_window_spawn` moves the spawn to a path node 160-320 units outside its window when the C1 struct stands far out (Prototype's are 666-2397 units from their windows). | **FIXED 2026-10-21:** not a C1 layout difference. T5 spawns at the same far structs (measured spawn -> first board 38 s). Near-window spawning is now off by default (`ZOMBIE_T5_MEASURED.md` §2). |
| 1.10 | Risers and chasers (Shi No Numa) | A spawner with `script_string "riser"` rises at a random active zone's `<target>_rise` point (`T5 sp:4421`). A rise point with `script_noteworthy "find_flesh"`, or a `"zombie_chaser"` spawner, skips the barricades (`:398`). | No rise path; risers and chasers walked to windows | **FIXED** (zone manager collects rise points; rise clip from 45 / 14 units under the floor via a native noclip `animscripted` start (server 5c4725ba); rise deaths by `deathout`/`deathhigh`). Probe: Sumpf riser rose from z -754 to -709, then hunted. The dirt and water rise effects ship only in zm_asylum_sh's zone; Sumpf plays common `misc/fx_zombie_body_dirt_billowing`. |

## 2. Windows and barricades

| # | Gap | T5 | CoDOL before | Status |
|---|---|---|---|---|
| 2.1 | Taunt frequency | `do_a_taunt` at 5 % (`zombie_taunt_freq`, `T5 sp:937`) | A taunt after every board and whenever no attack spot was free | **FIXED** |
| 2.2 | Reach-through attack | `should_attack_player_thru_boards` (`T5 sp:969`). Legged zombies only, none while glass is unbroken, 50 % (`zombie_reachin_freq`), a player within 90 units in 2D. The arm follows the attack-spot index. Plays the attack vocal. On each "fire" note `window_notetracks` (`:1051`) deals `meleeDamage` to a target within 90 of the zombie and 51 of the window trigger. | Always within 128 units in 3D, any zombie, random arm, and **no damage**: "fire" was never handled. | **FIXED** |
| 2.3 | Tear, taunt and reach rate | `AnimScripted(..., "normal", undefined, 1, 0.3)`: rate 1 | `animplaybackrate` (0.9-1.1) | **FIXED** |
| 2.4 | Teardown vocal | `do_zombies_playvocals( "teardown" )` | Unregistered ref, so silent. C1 ships no teardown alias. | OPEN (low). The boardtear clips carry their own notetrack audio. |

## 3. Melee and player damage

| # | Gap | T5 | CoDOL before | Status |
|---|---|---|---|---|
| 3.1 | Double swipe | Every "fire" note calls `melee()` (`T5 as/zombie_melee.gsc:126-150`), so a double swipe can down a player without Juggernog. | One hit per animation (a `landed` gate) | **FIXED** (the 0.4 s per-attacker ignore window stays) |
| 3.2 | Swing rate | Attack at rate 1 (`:116`) | `animplaybackrate` | **FIXED** |
| 3.3 | Health regen | `_gameskill::playerHealthRegen` at normal (0.75): after 2.4 s without damage, a player above 20 % refills fully. At ≤20 %, nothing for 5 s, then +10 % per 0.05 s. | +10 HP per 0.1 s after 5 s | **FIXED** (C zm `zm_player_health_regen`, `player_health_regen_delay` 2400) |
| 3.4 | Touch failsafe | `zombie_damage_failsafe` (`T5 sp:284`): kills a player a zombie has been touching for 5 s without moving | Absent | **FIXED (src)**. It runs, but the native chase stops 32 units short, so a touch is rare. |
| 3.5 | Player overlap | `track_players_intersection_tracker` (`T5 sp:34`) | Absent | **FIXED (src)**. No laugh: `zmb_laugh_child` doesn't ship. |

## 4. Damage, points and headshots

| # | Gap | T5 | CoDOL before | Status |
|---|---|---|---|---|
| 4.1 | Aim-down-sights hit points | Every surviving bullet hit pays "damage" (10). `damage_ads` is used only by the flame branch (`T5 sp:3684-3697`). | 15 points while aiming | **FIXED** |
| 4.2 | Explosive scaling | A surviving grenade hit adds round + 100..199. A projectile or explosive hit adds round × 0..99 (`T5 sp:3583-3625`). | None, so explosives fell off with the round | **FIXED** (`zombie_explosive_bonus_damage`) |
| 4.3 | Kill points | Paid only to a player killer (`T5 sp:3259`) | The world's kills (failsafe, sweep) paid the last player who hit the zombie | **FIXED** |
| 4.4 | Bleed-out kill | `damage_over_time` keeps the head location (`T5 sp:2689`), so the kill pays the headshot bonus, and ticks pay 10 | Tick location overwrote the head; ticks paid nothing | **FIXED** |
| 4.5 | Head gib gate | `head`/`helmet`/`neck` hits; never the m1911 (`T5 sp:2771-2776`) | `head` only, and the starting pistol popped heads | **FIXED** |
| 4.6 | Difficulty tiers | One tier (column 2) | `g_difficulty` 0 scales health by ×0.85 (127 start); 2+ by ×1.2; also changes spawn delay and AI caps | DESIGN (C1 lobby difficulty). Mode 1 equals T5. |
| 4.7 | Weapon damage tables | T5 `weapons/sp/*_zm` | C1 WeaponDefs | OPEN. No T5 weapon files are on disk (T5's are inside its fastfiles), so the two can't be compared yet. |
| 4.8 | Ballistic knife scoring | `ballistic_knife_death` event, 50+80 (`T5 sp:3284`) | Not ported | **FIXED (src)** |
| 4.9 | Flame damage over time | `zombie_flame_damage` / `damage_on_fire` (`T5 sp:3470`, `:3757`) | Absent | OPEN (low) |

## 5. Power-ups

| # | Gap | T5 | CoDOL before | Status |
|---|---|---|---|---|
| 5.1 | Drop arming | First drop at start score + 2000. The increment then grows ×1.14 at each drop (`T5 pu:523`). | A fixed 2000, counted from score 0 | **FIXED** |
| 5.2 | Which power-up | `get_valid_powerup` (`T5 pu:368`): carpenter needs 5 open windows; fire sale needs a box move and none active; minigun needs power, or a bought life in solo; free_perk, tesla and bonus points never drop normally; insta-kill and double points can repeat. | Carpenter on any broken board; insta and double skipped while active; the T5-excluded pickups dropped | **FIXED** (C1's own sentry and pandora keep their gates) |
| 5.3 | Who can drop | Player kills or nuked zombies, only after the window traverse, not tactical-grenade kills, on the ground under the body (`T5 sp:3234-3292`, `:3388`) | Any kill (traps, world), nuked zombies never, at origin + 40 | **FIXED** |
| 5.4 | Timeout | Solid for 15 s, then 40 blinks (0.5 / 0.25 / 0.1 s), about 26.5 s (`T5 pu:1626`) | 25 s, then 5 s at 4 Hz | **FIXED** |
| 5.5 | Nuke | White flash; after 0.5 s, closest first, 0.1-0.7 s apart, a head gib each, world kills; 400 to **every** player (`T5 pu:1677`) | 0.05 s apart in list order, no gib or flash, 400 to the grabber only | **FIXED**. `misc/fx_zombie_mini_nuke`, `evt_nuked` and `evt_nuke_flash` ship in no CoDOL zone, so they are not played. |
| 5.6 | Bag contents | Nacht and Verrückt: nuke, insta, double, ammo, carpenter | Plus C1's pandora, sentry, fire sale, all_revive, minigun | DESIGN (C1 content). The T5-excluded ones no longer drop. |
| 5.7 | Wobble, downed grab, max ammo | Random rotate every 2.5-5 s; a downed player can grab (except weapons); max ammo skips downed players | Yaw spin; downed players can't grab; ammo to everyone | **FIXED** |

## 6. Gibs, death, pain, crawlers

| # | Gap | T5 | CoDOL before | Status |
|---|---|---|---|---|
| 6.1 | Hit flinches | `self.a.disablepain = true` (`T5 sp:192`): zombies never flinch | Played `ai_zombie_shot_arm_*`, and the crawlers' hand-walk attack clips as "pain" | **FIXED** |
| 6.2 | Ragdoll | Every death goes to ragdoll 35 % into a clip without `start_ragdoll` (`T5 as/zombie_death.gsc:79`) | Only the boss set `allow_ragdoll` | **FIXED (src)** (`allow_ragdoll` in zombie_init) |
| 6.3 | Death hook timing | `deathFunction` runs before the clip: frees the spot, plays the death vocal, eyes out | Ran after the clip, so the vocal was 1-3 s late and the eyes glowed through the fall | **FIXED** |
| 6.4 | Crawler setup | One leg: crawl1-5 with paired deaths (crawl2/4 → `crawl_death_v2`). No legs: hand-walk 50/50. The bucket is unchanged. | crawl1-6, a random crawl death, speed forced to walk at ×0.35 | **FIXED** |
| 6.5 | Trap kills | Gib a random ref, or 10 % burn (`T5 traps:888-944`) | No gib | **FIXED** (burn uses C1 `zmb_ignite` and `env/fire/fx_fire_player_torso`) |
| 6.6 | Explosive death launch | `death_anim_short_circuit` / `launch_ragdoll_based_on_damage_type` (`T5 as/zombie_death.gsc:1712`) | Absent; `launchragdoll` isn't registered | PARTLY. `launchragdoll` and C1's `startragdollfromimpact` are native (`actor_ragdoll.c`, C1's ragdoll impact words +5c/+60), and a started ragdoll now survives the actor's frame; the Thundergun fling uses them. The explosive-death short circuit itself is still OPEN. |
| 6.7 | Tesla deaths | `ai_zombie_tesla_death_a-e`, crawl `_a/_b`, 50 % head gib | "No clips", but all ship in common_gametype_zm | **FIXED (src)**. The head gib keeps T5's `endon( "death" )`, so, as in T5, it rarely fires before the kill; the shock-eyes effect plays. |
| 6.10 | Tesla shock effect | Shock on `J_SpineUpper`, arc bolt between targets (`T5 tesla:316`, `:341`) | The shock was queued first in a six-event burst on the dying zombie and pushed out of the four-slot event queue; the arc bolt was played on its carrier's spawn frame | **FIXED** (2026-10-21): kill first, shock and eyes 0.15 s later, impact sound positional; bolt 0.1 s after the carrier spawns |
| 6.11 | Thundergun | `_zombiemode_weap_thundergun.gsc`: fling (kill + ragdoll launch), gib, knockdown and getup | Not ported: the gun did nothing | **FIXED** (2026-10-21), with native `getweaponmuzzlepoint`, `getweaponforwarddir`, `getaiarray`, `getaispeciesarray`, `launchragdoll`, and `physicsexplosioncylinder` reaching C1 |
| 6.8 | Limb launch | Pieces at 600-1000 sideways and 400-700 up, with a blood trail | 120-220 / 140-240, no trail | OPEN (low) |
| 6.9 | Both-legs death | `ai_gib_bothlegs_gib` | Not shipped; `ai_zombie_crawl_death_v1` stands in | DESIGN |

## 7. Audio

| # | Gap | T5 | CoDOL before | Status |
|---|---|---|---|---|
| 7.1 | Ambient vocals | Every 1-4 s by bucket (ambient, sprint, crawler), no overlap (`T5 sp:4983`) | Every 3-8 s | **FIXED**. C1 has no sprint vocal, so `zmb_zombie_amb` covers runners. There is no `sounddone`, so a 2.5 s spacing stands in. |
| 7.2 | Behind vocal | `zombie_behind_vox` (`T5 audio:463`): within 200/250/275 units, more than 95° behind, 5 s per-player cooldown | `zmb_zombie_behind` was registered but never played | **FIXED** |
| 7.3 | Spawn sound | Only for risers | Every spawn | **FIXED** (removed) |
| 7.4 | Bodyfall and scrape notetracks | `fly_bodyfall_*` foley | C1's death clips carry the foley as client notetracks (`ai_zombie_death_v1`: `sndnt#fly_fall_zombie`); most carry no bodyfall note at all | DESIGN. Nothing to play on the server. |

## 8. Rounds

| # | Gap | T5 | CoDOL before | Status |
|---|---|---|---|---|
| 8.1 | Failsafe re-queue | `put_timed_out_zombies_back_in_queue`: a culled zombie goes back into `zombie_total` (`T5 zm:4059-4127`) | Lost, so rounds came out short | **FIXED** |
| 8.2 | Round start timing | About 10 s before round 1's first spawn; about 12.5 s between rounds (chalk) | 2 s / 10 s | **FIXED** (`chalk_one_up` waits of 8.25 s / 2.5 s). The round number still increments before the between-round wait, because the C1 round HUD shows it. |
| 8.3 | Asylum zone graph | `asylum_zone_init` adjacency (`zombie_cod5_asylum.gsc:131`) | Zones open by occupancy only | **FIXED**. The volume names and door flags measured on the map match T5's; both west rooms start enabled. |
| 8.4 | Spawn delay decay | ×0.95 per round, floor 0.08 | Tiered 0.97/0.95/0.93, floors 0.30/0.20/0.12 | DESIGN (tiers) |

## 9. Remaining work, by priority

1. Weapon damage parity (4.7). This needs a C1 WeaponDef dump of every
   `*_zm` weapon (damage, range, the head/neck/torso multipliers) compared
   with T5's `weapons/sp/*_zm`.
2. An explicit breadcrumb fallback and the crowd split (1.7).
3. Native: `launchragdoll` with T5's death short-circuit (6.6).
4. Flame damage over time (4.9) and the limb launch speeds (6.8). CoDOL's
   pieces move as script models, so T5's dynent launch forces don't carry
   over as numbers.
6. A visual check on the C1 client of: the zombie ragdoll (6.2; coop
   soldiers already ragdoll the same way), the crawler box (1.8), and the
   rise (1.10).
5. Sumpf's own rise water effects, if its usermap zone ships T5's `*_smpf`
   ones. The asset lister cannot open the IWffu100 container yet.

## Verification log

- **Static resolver** (`tools/script_resolve.py --category zm --map
  <map>`): exits 0 on zm_prototype_sh, zm_asylum_sh and zm_sumpf_sh.
- **Native notetrack fix:** Prototype pathtest soak (150 s, 10 alive,
  every spawner). Before the fix: 24 of 48 negotiations timed out. After it:
  103/103 completed, 0 stuck, 0 runtime errors.
- **Round flow:** Prototype with a test client and the `pathtest kill`
  verb ran rounds 1-5 (budgets 5/6/11/15/19 on difficulty 0) with 0 runtime
  errors. Mean speeds per clip: walk_v1 36 u/s, run_v1 111, run_v2 100,
  sprint_v1 131 (before the fix sprinters ran ×1.56).
- **Reach-through:** `pathtest towindow` with `zombie_reachin_freq 100`. The
  zombie reached through on each board and every "fire" took 60 from the
  player.
- **Nuke:** grabbed mid-round. World kills (no kill points), then 400 paid
  to each player.
- **Asylum:** both west rooms enabled at start; the occupied room spawns;
  its neighbours open by door flags.
- **Sumpf:** after `pathtest openall`, a riser rose 45 units with the
  "noclip" scripted clip and then chased.
- **Regression** (all three maps in parallel, test client plus the `kill`
  verb, about 6 min each, 0 runtime errors):
  - Prototype: rounds 1-5, 47/47 window climbs complete, the last-zombie
    sprint fired.
  - Asylum: rounds 1-5, 56/56 climbs, the sprint fired twice.
  - Sumpf: rounds 1-2, 9/9 climbs, one riser.
- **Crawler:** `scr_gibtest gib right_leg` made a live crawler, which
  went on to the crawl lunge melee. 0 errors with `setphysparams`.

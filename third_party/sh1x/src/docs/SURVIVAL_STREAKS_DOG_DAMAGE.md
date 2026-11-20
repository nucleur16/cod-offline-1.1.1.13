# Survival MP streak exclusion and dog bite balance

User direction: Survival uses purchased support and credits rather than MP loadout killstreak progression. Dog melee must not down an unhurt 100 HP player in one hit. MP dog behavior is preserved.

## Source

- `bin/home/codol_rawfiles/maps/mp/gametypes/_survival.gsc`: disable the old `scr_survival_killstreaks` opt-in; apply a separate Survival dog melee cap of 35 (`scr_survival_dog_damage_cap`) before purchased armor absorbs damage. Incoming lower damage stays lower; zero/negative configured caps fall back to 35. Non-dog melee, bullets, and dog explosive splash retain their existing paths.
- `bin/home/codol_rawfiles/maps/mp/killstreaks/_killstreaks.gsc`: `g_gametype=survival` bars earned MP rewards and progression notifications, clears preexisting rewards on the first Survival spawn only, keeps progression HUD at zero, and refuses stale earned rewards at both activation entry points. Subsequent spawns keep deliberate support purchases; kiosk `giveKillstreak(name,false,false,self)` continues through the shared implementation.
- `_survival_ai.gsc::award_kill` is coordinated with the Survival wave/AI worker: count kills without incrementing MP streak count or dispatching `checkKillstreakReward`. Credit awards precede that function and remain intact.

## Damage trace

`maps/mp/animscripts/dog_combat.gsc` calls the actor `melee()` method. Native `src/online/player/actor_lifecycle.cpp::ActorMelee` resolves the actor's real aiweapon and calls `ActorMeleeTrace`, linked through `src/asmsource/_codol_actor_melee.asm` to retained `Weapon_Melee_internal`. Survival previously deliberately skipped scaling every `MOD_MELEE` callback, so the MP bite weapon reached the player at full damage. The narrow Survival callback now recognizes the `isdog` flag authored by `_survival_utility::spawn_dog` and caps that bite. No aiweapon assets, MP scripts or native damage defaults change. The dog finisher script's `killplayer` currently does not apply damage; it only sets animation/death state.

At 100 HP without armor and without intervening regeneration, 35-damage bites leave 65 HP, 30 HP, then down/kill. Hurt players can still go down sooner. Armor absorbs the capped bite normally.

## Validation

`node tests/survival_rules_test.js` executes the actual GSC helper and complete player-damage body against engine boundary stubs. It covers large/small/zero bites, configurable and invalid caps, three bites, armor order, explosive/non-dog isolation, mode-specific earned vs purchased allowance, guard bodies before award side effects, both activation guard locations, one-time carryover clearing, and AI kill counting without MP progression.

Full static resolver gates passed for MP war/Killhouse, Survival/Favela, and coop/Favela. Full ZM training has unrelated existing unresolved registrations in the resolver's source view (checkpoint/status-colour/localtest, canreachplayer, revive tokens and FX helpers); none is introduced by this patch. The narrowed reachable shared-killstreak gate is used to check this change in ZM as well.

No native binary build is required for these script-only edits. No live client gameplay validation, deployment, restart, or commit performed by this worker.

## Owned native progression probe (2026-10-29)

The final source binary and deployed progression FF were exercised on an owned isolated server (`127.0.0.1:17761`, separate `fs_homepath`) with a real developer test bot. Results are retained in `tests/fixtures/survival_progression_native_probe.json`; full raw logs are under `obj/survival_progression_probe`.

- Wave 2 passed: four easy infantry at 25 HP and two regular dogs at 25 HP, six spawned, no spawn failures or script runtime errors.
- Wave 8 passed: four regular infantry at 50 HP, two martyrdom bombers at 50 HP using actual bomb-vest body models, and two regular dogs at 25 HP. The bot later recorded one kill and 175 credits; no script runtime error occurred during the 35-second exercise.
- Wave 7 failed: the authored chemical troop carries `m79_mp`, which reaches `_survival_utility::arm_soldier` and is rejected by native `codol_actorSetWeapon` as not a bullet weapon. This is a concrete unsupported projectile-weapon producer, not a wave-table error. Native/AI owners were notified; wave 7 is explicitly recorded as failed pending implementation and retest.

The first wave-2 probe queried the preexisting test harness's `status()` before the bot finished joining; its spectator credit was undefined and the harness's string concatenation raised an error. The runner now waits four seconds before status/godmode. This failed harness attempt is retained in `obj/survival_progression_probe/wave2`; it is not gameplay evidence.

The probe runner now supports `--start-wave` and `--exercise-seconds`, uses hidden Windows startup, reads actual qconsole logs, and rejects runs containing script runtime errors even if native automatic recovery reaches running state again. All owned probe processes were stopped after capture. No user game or backend was stopped or changed by the worker. These results establish native spawn paths, not real C1 visuals or gas/C4 damage behavior.


### Projectile implementation and death-work retests

After the native projectile implementation, wave 7 spawned four hardened soldiers and two chemical troops, each at 75 HP; the chemical troops held the authored `m79_mp` and gas-mask head model. Killing all six and observing twelve seconds afterward passed without runtime errors (bot remained playing at 100 HP, six kills and 1867 credits). This proves actual actor spawn/death paths; no M79 shot was observed in this run.

Wave 8's early-kill retest spawned all eight ground actors (regular infantry, bombers and dogs), killed all eight, and observed twelve seconds afterward without runtime errors. The bot remained playing at 100 HP with eight kills and 1248 credits. The chopper was still live. This does not claim a measured C4/gas damage amount or a C1 visual result.

An earlier longer wave-8 run captured the bot's health changing from 100 to 65 at dog contact, providing actual production-path evidence for the 35-damage cap. It later died, so that run is not counted as a completed death-hazard test. The temporary `setCanDamage(false)` operator verb did not prevent that damage and was removed entirely; `_survival_test.gsc` has no retained changes. The runner uses the existing health-refill command and spaces harness commands by one second to prevent the polled single-command dvar being overwritten.

The short-intermission defect exposed during these tests is fixed in `_survival.gsc`: remaining ready-up time is clamped to zero and `waittill_timeout` returns immediately for nonpositive durations. Actual-function tests cover negative, zero and normal waits; normal 30-second intermission behavior is preserved.

## Mixed-wave dog target admission (2026-10-29)

`ActorUpdateMpDogThreat` applied the MP `ai_maxAttackerCount` (default 2) to Survival dogs. Its `ActorAttackerCount` counts all living native actors targeting the player, including infantry. Four infantry in wave 2 can consume that budget before either newly spawned dog selects an enemy. A GSC movement goal can still make a dog move toward a player without supplying the native combat enemy required for attacks. This is a target-starvation condition, not a claim that every dog was unable to bite: an earlier wave-8 runtime recorded one 35-damage bite.

The native admission predicate now uses the 32-slot authored wave actor budget only for category `survival`. MP and other categories keep the configured attacker cap and current-enemy retention. Team hostility, ignore groups, awareness, visibility, session and health gates remain in the actual production selector. Dog attack range, animation notetracks, native melee trace, weapon damage and the Survival 35-damage callback remain unchanged.

`python3 tests/test_dog_threat_production.py` extracts and compiles the actual production threat selector and attacker-count/hostility helpers with deterministic engine-boundary stubs under ASAN/UBSAN. It reproduces MP rejection with four existing infantry attackers, verifies Survival admission with four and 31 existing attackers, and checks hostile-team, occlusion, ignored-group, dead-player, spectator and current-enemy cases. `tests/test_actor_threat_limits.c` additionally exercises boundary counts and configured zero. These tests verify actual selection logic; they do not replace the separate native runtime contact test.

Latest Windows build linked both `bin/sh1x_dedrun.exe` (SHA256 `5BE2D5F7942F6A46749AA714FDDEFC922BF1348D5DD52BA33C6D3BCC8B9D6EC5`) and `bin/sh1x_host.dll` (`C0FD08D212A2D06824000D85B27245419A0FCFEB7D8E379119930C9DF4730882`). An isolated owned `mp_fav_tropical` wave-2 run on UDP17761 spawned four easy infantry and two dogs without failures. Both dogs acquired enemy 0 and chased from their authored map spawns into contact with the bot. The bot changed from 100 to 30 HP at dog contact while infantry were distant, consistent with two capped bites, then died. This post-fix sample does not isolate one bite; the earlier wave-8 sample measured 100 to 65. No script runtime error or missing-table fatal occurred. The probe requested native contact telemetry but no telemetry records appeared, so none are claimed. Raw evidence is `obj/survival_dog_target_probe/mp_fav_tropical/home/main/qconsole.log`, particularly lines 371-389. The owned process exited after capture; deployed binaries and user processes were untouched.

# Native weapon lock state

`WeaponLockFinalize` remains a missing vendor-script method. The Python
`gsc/builtin_surface.py` implementations store object markers and do not
establish native weapon-lock semantics. No placeholder binding was added.

Offline disassembly of `D:/CoD/shx/iw4x.exe` establishes:

| Method | Address | Behavior |
| --- | --- | --- |
| weaponlockstart | 0x5dd3e0 | Select entity or vector target, set bit 1, clear bits 2/4/8 |
| weaponlockfinalize | 0x5dd500 | Select target, set bits 1/2, set optional missile offset, OR bit 4 for true third argument or bit 8 otherwise |
| weaponlockfree | 0x5dd670 | Clear bits 1/2/4/8 and set entity target to 2047 |
| weaponlocktargettooclose | 0x5dd710 | Set/clear bit 0x10 |
| weaponlocknoclearance | 0x5dd790 | Set/clear bit 0x20 |

IW4 stores flags at player state +0x404, entity at +0x408, position at +0x40c,
and the server missile offset at gclient +0x3654. Position targeting sets bit
0x40 and preserves the stored entity; entity targeting clears bit 0x40 and
preserves the stored position. Start and free preserve warnings and free also
preserves the target-mode bit. Finalize ORs its fire-mode bit rather than
clearing the opposite mode. These details are preserved in the native
`weapon_lock.c` state transitions outside the retained IW3 ABI.

C1 `0x41b9b5` tests player state +0x48c bit 2 before allowing a lock-required
weapon to fire; absent lock emits event 0x70. At `0x41b9fb` it reads the same
field and tests bits 2/0x10 for event 0x6e and bit 0x20 for event 0x6f. Thus
the client flag field and these three flag meanings are directly confirmed.
The existing CoDOL wire table represents +0x48c with seven bits. Further C1
disassembly found its own finalize implementation: entity target writes
+0x490 at 0x519b6d, vector target writes +0x494 at 0x519c1f, and missile
offset writes gclient +0x3a44 at 0x519bf5. Its mode and warning behavior
matches the IW4 transitions above. `OverlayWeaponLock` now maps the five
confirmed player-state fields into the semantic snapshot state, preserving
unrelated fields and excluding the server missile offset. The focused test
also verifies this mapping and atomic rejection of out-of-range flags.

The focused 32-bit ASan/UBSan test covers entity/position transitions, repeated
finalization, warning preservation, offset reset, and atomic rejection of
invalid entity numbers or nonfinite vectors. Windows build passed with the
native state module (`/tmp/sh1x-weapon-lock-state-win32.log`).

## Live state and snapshot integration

`weapon_lock_clients.c` owns thirty-two independent native states,
initialized with entity target 2047. `ResetClientInput` clears the selected
slot on the existing bot/connect/disconnect paths, and `ResetGameInput`
clears every slot on map initialization. The focused lifecycle test invokes
these production reset functions and verifies that other clients remain intact
until the map reset.

`BuildGameplayFrame` seeds its player fields from the state selected by
`source->ps.clientNum`, before retained IW3 overlays. The production frame test
now verifies flags/target survival through the full builder and that recipient 1
following player 0 receives player 0's target rather than its own. The existing
snapshot serialization check then runs with those fields populated. Both the
lifecycle test and production frame test passed with 32-bit ASan/UBSan; Windows
build passed (`/tmp/sh1x-weapon-lock-clients-win32.log`). This is controlled
engine-state validation, not a live script or C1 missile demonstration.

The retained `Weapon_RocketLauncher_Fire` already forwards target and offset
arguments to `G_FireRocket`; that function stores an `EntHandle`, target offset,
and missile flight state. This provides a source integration point without
changing the IW3 ABI. Still required before registering the methods: GSC input
validation, respawn behavior, server firing gates, and consuming the selected
lock/offset/mode in native missile launch and guidance. Full vendor-script
loading and real C1 match entry remain incomplete.

## Entity-target missile consumption

The CoDOL build of retained `G_FireRocket` now calls
`ApplyRocketWeaponLock` after default stage/flight initialization and before
linking or `missile_fire` notification. A finalized entity lock with an active
native target copies the server missile offset, selects top/direct flight mode,
and assigns the target through the existing `EntHandle::setEnt` bookkeeping.
Unlocked states, invalid/inactive targets, non-player owners, and position-only
locks leave the missile unchanged. Position-only guidance is still missing and
must not accidentally reuse the stale stored entity target.

This uncovered a retained C header mismatch: `missile_ent_t` contained a
nonexistent inline `targetEntNum`. The assembly stores the target in the outer
handle at +0x214 and uses +0x1dc for offset, +0x1e8 for stage, and +0x1ec for
flight mode. Removing that inline member makes the C fields match those
instructions. Compile-time assertions in `test_weapon_lock_missile.c`
verify all four offsets. The focused test passed with 32-bit ASan/UBSan and
covers entity offset/mode consumption plus unchanged output for inactive and
position targets. It does not prove in-client guidance or script execution.
The production frame regression also passed with the corrected header, and a
full forced Windows rebuild passed (`/tmp/sh1x-weapon-lock-missile-win32.log`).

Position guidance, firing gates, respawn reset, and the GSC method bindings
remain outstanding. No new method has been advertised as implemented yet.

## Respawn and lock-required firing gate

`ClientSpawn` now resets the independent weapon-lock slot immediately after
clearing `gclient_s`. The existing connect/disconnect/map reset behavior remains
in place; a finalized target cannot survive a new life through the side storage.

The native weapon converter now copies the validated byte at C1 WeaponDef
+0xbbe into `requireLockonToFire`. C1 tests this byte at 0x41b9ad and requires
player lock bit 2 at 0x41b9b5, emitting event 0x70 if absent. The retained
`PM_Weapon_1310` now checks the corresponding native state after weapon delay
expires and before ammunition consumption; denial emits the retained
`EV_LOCKON_REQUIRED_HINT`, which the existing event mapping converts to 0x70.
The call resumes normal firing only with a finalized lock, or for a weapon
without this requirement. It is a compiled source call, not a runtime hook.

The focused conversion-to-gate test verifies no lock, started lock, finalized
lock, freed lock, optional lock, and atomic rejection of malformed source flag
values. It and the native weapon conversion regression passed with 32-bit
ASan/UBSan. Windows builds passed for both respawn and firing integration
(`/tmp/sh1x-weapon-lock-respawn-win32.log`,
`/tmp/sh1x-weapon-lock-fire-win32.log`). No live respawn or shot was claimed
from these controlled tests.

Remaining work in this family: position guidance, the separate C1 +0xbbd
warning gate for target proximity/clearance, and GSC bindings with validated
arguments. Full script startup and real C1 match entry remain unverified.

## Proximity and clearance firing checks

The native core now retains C1 WeaponDef +0xbbd as `lock_warning_gate`, without
guessing an equivalent IW3 weapon category. Its lookup uses the exact native
definition pointer across active and retired archive generations, so an old
registered definition retains its own flag after an archive replacement.
Malformed non-boolean bytes reject core conversion atomically.

The existing native firing check now follows C1 0x41b9ad..0x41ba76 in order:

1. A required but unfinished lock emits `EV_LOCKON_REQUIRED_HINT` (wire 0x70).
2. With the warning gate enabled, both finalized-lock bit 2 and proximity bit
   0x10 emit `EV_TARGET_TOO_CLOSE_HINT` (wire 0x6e).
3. Otherwise, enabled clearance bit 0x20 emits
   `EV_TARGET_NOT_ENOUGH_CLEARANCE` (wire 0x6f), even without a finalized lock.
4. A disabled warning gate ignores both warnings.

The production call remains before ammunition consumption. Focused sanitizer
tests cover this precedence, flag conversion, and atomic malformed input.
The Windows archive-registry regression checks the warning flag before and
after replacement, including the retained old definition. It passed, as did
the full forced Windows build (`/tmp/sh1x-weapon-lock-warnings-win32.log`).

Position-target guidance and GSC argument/binding integration are still missing.
These checks are not evidence of a real client entering a match.

## Position-target launch and retained guidance

C1 launch code 0x57498d..0x574a23 distinguishes position locks from entity
locks. With bit 0x40 it passes a null target entity and copies player-state
+0x494..0x49c as an absolute target; only the entity branch copies gclient
+0x3a44 missile offset. Both branches set the launch's top-attack byte from bit 4 (corrected 2026-10-13: bit 4 is top attack, see "Javelin flight").

The native launch path now records this absolute target independently per
missile, clears the entity handle through existing bookkeeping, and zeroes the
entity-relative offset. Retained `MissileTrajectory` accepts this target without
an entity handle and reads its coordinates in both guidance branches. Existing
steering/stage calculations remain in the retained source. The launched target
does not change when the player's lock changes or resets.

`G_InitGentity`'s existing generation reset also clears position metadata, and
map generation reset clears every slot. Every rocket launch additionally clears
its reused slot before choosing a target. Tests cover absolute coordinates,
ignored missile offset, independent lifetime, and resets. The missile ownership
test and production frame regression passed with 32-bit ASan/UBSan; Windows
build passed (`/tmp/sh1x-missile-position-win32.log`). Guidance branches have
compiled, but actual in-flight position guidance still needs runtime validation.
GSC bindings and real C1 match entry remain incomplete.

## Native GSC bindings

The registry now binds `weaponlockstart`, `weaponlockfinalize`, `weaponlockfree`,
`weaponlocktargettooclose`, and `weaponlocknoclearance` to native state operations.
Entity/vector arguments are validated through the retained VM. Finalize reads
its optional offset and direct-fire argument before committing state; VM errors
leave the prior lock intact. The adapter uses the retained VM's `VAR_ENTITY`
value rather than copying C1's different internal type number.

The focused adapter test passed with 32-bit ASan/UBSan, covering both target
forms, default offset, warning/free behavior, wrong receivers, and getter errors.
Windows build passed (`/tmp/sh1x-weapon-lock-gsc-win32.log`). The full vendor
script retry no longer reports `WeaponLockFinalize`; its next compile failure
is `SortByDistance` in `maps/mp/_stinger.gsc:304`. The isolated probe's log is
`obj/lock-gsc-vendor.log`. These results establish registry resolution and
controlled adapter behavior, not a successful live missile or match entry.

The regenerated inventory has 479 matching names and 828 missing names. As
before, name coverage is not a semantic-completeness score. Full script startup,
in-flight guidance validation, and real C1 match entry remain outstanding.

## Stinger lock-on and homing, first live run (2026-10-06)

`maps/mp/_stinger.gsc` (IW4) runs from `_weapons.gsc` `onPlayerSpawned` and
its target list is filled by the killstreak ports (`level.chopper`,
`level.UAVModels`, `level.harriers`, `level.littleBird`, `level.ac130`). A
probe on mp_killhouse_sh (bot holding ADS on the stinger, aimed by
`setPlayerAngles`) reached stage 2 and `WeaponLockFinalize` set flags 0xb on
the target. Three native defects then kept the shot from working:

1. `G_FireRocket` notified `missile_fire` only on its destabilizing-rocket
   path, so stinger/javelin/AT4 shots never reached `watchMissileUsage`,
   `stinger_fired` was never sent and the UAV/AC-130 proximity detonation
   never armed. Every rocket now notifies.
2. The weapon converter did not copy C1 WeaponDef +0x8e4
   `maxSteeringAccel` (+0x8e8 `projIgnitionDelay` follows it): stinger
   10000/500, javelin 3000/500, at4 3000/0. With zero steering the retained
   guidance had an infinite turn radius.
3. `Vec2NormalizeTo` (q_math.c) scaled by the length instead of its
   reciprocal (IW3 original: `_com_math.asm:2932`), so the guidance frame
   was never normalized; with steering on, the missile pinned its vertical
   correction at +maxSteeringAccel and climbed past the target.

After the three fixes a locked stinger turned onto a moving helicopter
(distance 5292 -> 129 in 1.2 s) and destroyed it. With the missile_fire fix alone, a
straight UAV shot passing within 880 units was detonated by
`stingerProximityDetonate`; no UAV kill was captured after the steering fix
because killhouse structures blocked the shots. C1 rendering of the lock box/tone was not
checked with a real client.

## Javelin flight (2026-10-13)

A probe (udp/17752, mp_rust, one bot driven by a scratch test hook holding ADS
with `javelin_mp`) showed the lock itself worked: a ground point reached stage
3 after 12 gathered points and 1150 ms and `WeaponLockFinalize` set the
position lock. The missile did not: after its 500 ms soft launch it bled speed
every frame and hung in the air at ~220 units forever (logged positions
converged to one point and never exploded). Three defects:

1. `JavelinRotateDir` (`_g_missile.asm`) computes the turn between the current
   and desired heading as `(1 - (dot + 1) * 0.5) * 180`, the IW3/IW4 formula
   (iw4x 0x5e7f5a). CoDOL inherited CoD4x 0d089339 "Fix missile timescaling",
   which replaced every 0.05 frame constant with `missile_frametime` and also
   swept up this function's -0.5, deleting the constant. Every javelin then read
   a ~186 degree turn: `MissileTrajectory` never accelerates above 30 degrees
   and applies `missileJavTurnDecel` instead, so the rocket stopped. The -0.5 is
   restored.
2. The flight mode was inverted. C1 0x574a1a sets its launch target's
   top-attack byte from lock bit 4 and the rocket spawn 0x53139a flies TOP (0)
   when it is set and DIRECT (1) otherwise; IW4 is identical (0x4a4f6b,
   0x4910a0). Bit 4 is `weaponLockFinalize`'s third argument, which IW4
   `_javelin.gsc` sets for ground points and clears for vehicles.
   `ApplyRocketWeaponLock` mapped bit 4 to DIRECT, so ground shots flew
   flat and vehicle shots flew top attack. The argument is now named
   `top_attack`.
3. IW4 and C1 raised `missileJavAccelDescend` to 4500 and lowered
   `missileJavSpeedLimitDescend` to 2500 (iw4x 0x46707b/0x4670eb; C1's
   numeric dvars 1181/1183 at 0x52e1e1/0x52e251, registered in IW4's order).
   The retained IW3 defaults were 3000 and 6000. The turn bleed is also scaled
   by `frametime / 0.05`, because IW4 applies it once per 50 ms frame and this
   server runs sv_fps 60.

After the fix (same probe): a ground lock climbs to ~3400 units and dives onto
the point (impact ~6.5 s at ~4900 units). A bot pinned at the point died to
the splash with the shooter credited (`K;...;javelin_mp;241;
MOD_PROJECTILE_SPLASH`). A lock on the shooter's own attack helicopter flies
direct mode, climbs above it and steers after it; against a manoeuvring
helicopter it missed and bled speed while turning, as the IW4 algorithm does.

`checkweapon` prints the lock bytes: C1 `javelin_mp` has steering
3000, ignition delay 500, warning gate 1 and requireLockonToFire 1, so an
unlocked javelin cannot fire (the probe's unlocked trigger pull fired
nothing). The IW4/C1 no-lock javelin target (start + dir * 10000, DIRECT) is
therefore unreachable and not implemented.

Still differs from IW4: IW4 rotates an entity lock's missile offset by the
target's axis (0x5e7cdc); the retained code adds it unrotated. Every shipped
script passes a zero offset. Two hellfire-guidance steps in `MissileTrajectory`
still multiply by 20 (1 / 0.05) where 0d089339 changed the matching 0.05 to
the frame time; they are not javelin paths and were left alone.

## Javelin ADS after the shot, getEye, and the minimum distance (2026-10-13)

User report after the flight fix: "a weird ADS glitch after firing and before
reloading the second shot", and "what counts as a close distance is way too
close".

**ADS after the shot.** C1 `BG_PlayerCanAds` 0x418140 ends with two tests the
server port did not have: `weapFlags & 0x20` refuses the sight (0x4181fa), and
a definition with byte +0xbc0 set refuses it while the selected weapon's hand-0
magazine is empty (0x4181ff, `BG_GetAmmoInClip` 0x417fc0). +0xbc0 is IW4
`noAdsWhenMagEmpty`, the byte before `avoidDropCleanup` (+0xbc1), and
`checkweapon` now prints it: `javelin_mp` 1; `stinger_mp`, `at4_mp`,
`rpg_mp`, `m4_mp` 0. The converter never copied it, so after a javelin shot the
server held PMF_ADS_BUTTON and `fWeaponPosFrac` 1.0 for the whole 1076 ms fire
time and only lerped out when the reload began, while C1 predicted the sight
dropping on the command after the shot. Every snapshot in that second pulled
the predicted view back into the CLU. Probe trace (`traceWeaponLock 1`,
bot on mp_rust, lock at ~2000 units, fire):

    before: t35765 st 5 wt 1059 clip 0 posFrac 1.000 pmf 0x10 lock 0x47
            ... posFrac 1.000 on every command until
            t36848 st 7 (reload) posFrac 0.961, 0.925, ... 0.000 at t37298
    after:  t39265 st 5 wt 1059 clip 0 posFrac 1.000 pmf 0x10 lock 0x47 (the shot)
            t39281 st 5 posFrac 0.964 pmf 0x0
            t39315 st 5 posFrac 0.886 pmf 0x0 lock 0x40 (_javelin.gsc frees the lock)
            t40331 posFrac 0.000; t40348 st 7 (reload); the sight returns once
            the reload refills the magazine (t41615 clip 1 pmf 0x10)

`noAdsWhenMagEmpty` is now converted (+0xbc0) and the C1 port of
`PM_UpdateAimDownSightFlag` applies both tests. The retained IW3
`PM_UpdateAimDownSightFlag` already read the same field (its +0x3f8 test), so
it now agrees as well.

**getEye.** IW4 `ScrCmd_GetEye` 0x5f0920 returns `G_GetPlayerViewOrigin`
0x4b6d00 for a player (and errors on anything else). The retained IW3 method
returned origin + 40 for every entity, so `_javelin.gsc` `EyeTraceForward`
and `VehicleLockSightTest` traced from 20 units below a standing player's eye
(and 29 above a prone one's). A player now gets its view origin, the origin
`WorldPointInReticle_Circle` already used; other entities keep origin + 40 so
scripts that call it on non-players keep running. Probe: `getEye()[2] -
origin[2]` was 40 and is now 60 for a standing bot.

**Minimum distance.** `TargetPointTooClose` is IW4's (and IW5's) 1100 units
from `self.origin`, and the stage logic is IW4's line for line. Probe results:

- aiming at ground ~630 units away: stage 1 never gathers points, lock flags
  0x50 (position mode + too close), trigger pulled: nothing fires (no lock);
- lock finalized at ~1900 units, then moved to ~700 units with the lock held:
  flags 0x57, trigger pulled: `EV_TARGET_TOO_CLOSE_HINT`, magazine stays 1, no
  missile (server gate, C1 0x41b9fb order);
- C1's HUD reads the same bit: `weaplocktooclose` is ps+0x48c bit 0x10
  (0x5e1540), `weaplocked` bit 2, `adsjavelin` = full ADS on an overlay
  weapon. C1 has no other too-close field or weapon-definition distance.

Vehicle locks (`javelinLockVehicle`) have no minimum distance in IW4 either.

**Diagnostics.** `traceWeaponLock <client + 1>` prints one line per
command after `PM_Weapon` whenever that client's hand state, ADS fraction,
PMF_ADS/PMF_ADS_BUTTON, lock flags or magazine change.

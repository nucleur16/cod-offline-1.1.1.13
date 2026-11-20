# Survival roster, endless scaling and special enemies

The authoritative scripts remain in `bin/home/codol_rawfiles`. This change does
not replace actors with script-model soldiers or substitute synthetic attacks.
Soldiers use the native actor species, composed MP weapons and native shots;
dogs use the native MP dog species. Script models below are the normal C4/gas
hazard entities, not actor proxies.

## Findings and correction

The authored CoDOL AI table already progresses from easy shotgun infantry
(`model1887_mp`, 25 health), regular MP5K (50), hardened SCAR (75), veteran
AK104 (100), to elite FN2000 (125). The twenty-wave tables deliberately mix
these tiers on boss waves; they are not replaced with an invented monotonic
roster. Spawn now passes the table health and accuracy into the aitype setup
and retains them on the spawned actor. The held weapon still passes through
the real party composer and `codol_actorSetWeapon` path.

Column 6's alternate weapon was ignored. The shared soldier code always chose
a global frag grenade, so even early infantry with an empty alternate could
throw frags, while chemical/hazmat could never throw their authored tabun.
The Survival actor now owns its offhand and grenade count. The existing throw
animation releases the actual native grenade returned by
`codol_actorThrowGrenade`; a category-owned callback watches chemical grenade
explosion. Ordinary Coop continues using its existing role pool. A native
weapon-type check prevents juggernaut RPG secondaries from entering the grenade
throw path, which requires WEAPTYPE_GRENADE.

The eleven non-gas map tables exchange one existing special-enemy type group
at waves 7, 14 and 18 for chemical, hazmat and hazmat respectively. Every count,
dog, boss, ordinary squad and other cell remains unchanged. Firing Range and
Storm retain their original chemical/hazmat schedules. Favela consequently
has two chemical enemies on wave 7, three hazmat on 14, and two hazmat plus its
existing explosive dog on 18. Its existing regular dogs and bombers remain
in their other authored waves.

Hardhat and Terminal exports had only nine columns, losing the repeat metadata
entirely. Add column 9 with repeat flags for waves 17–20, retaining all their
original nine cells. Other maps retain their authored repeat flags.

IW5 `scripts/1571.gsc:update_wave` increments the repeated-wave count on every
generated wave. `scripts/1564.gsc:get_ai_health/get_ai_speed/get_ai_accuracy`
then use independent +10% health, +5% speed (cap 1.5), and +20% accuracy
multipliers. The previous implementation increased health and accuracy by
15% per four-wave cycle, never speed. The new implementation uses the reference
per-wave multipliers, retaining the MP soldier contract's existing 0.3 base
accuracy and 0.6 ceiling. Dogs now use their authored 25 health and repeat
scaling, removing an undocumented fourfold health multiplier. Their bite damage
is handled independently by the Survival rules change.

## Projectile-primary follow-up

An isolated real wave-7 probe reached the authored M79 and exposed a native
bullet-only actor arming API. The weapon is retained; real projectile arming,
firing and ammo production are a separate native implementation requirement,
owned by the actor/fire worker. Replacing M79 with a bullet gun would conceal
the missing engine path and is not part of this change.

The GSC side now classifies the actual primary through `weaponType` and
`weaponClass` before aitype setup, assigning a grenade-launcher or RPG role
instead of the old generic SMG/AR role. Projectile weapons fire single shots,
using the definition's `weaponFireTime`; the native actor magazine supplies the
actual capacity/remaining count. The shot passes the intended aim target to
the native projectile path, retaining the past-target extension only for
bullet traces. Native GL/rocket physics, speed, gravity and explosion remain
authoritative; no script direct-damage substitute is introduced.

IW4 animset selection uses shoulder RPG clips only for the rocketlauncher
class. M79's grenade-launcher class remains rifle-held; the C1 generic human
tree has no invented M79-specific clips. Rocket firing/aiming is no longer
overwritten with rifle cover clips. A projectile cover reload uses its posed
reload animation and waits at least the definition's `weaponReloadTime` before
native magazine completion. Rifle running/combined return-to-cover reloads
cannot refill launchers. Existing interruption behavior is retained.

`tests/soldier_projectile_behavior_test.js` executes the actual family, burst,
timing, animation and reload functions, including ordinary SMG behavior and
interrupted projectile reload. The existing four-path reload regression also
passes. Native projectile physics and isolated wave-7 runtime acceptance must
be verified after the native actor implementation is built; these GSC tests
do not count as that acceptance.

## Real special-enemy behavior

- Bomber and explosive-dog death workers capture the death position and hand
  the fuse to a level-owned C4 hazard before corpse cleanup can delete the
  actor thread. The native radius-damage path uses IW5's 192-unit radius and
  100/50 maximum/minimum damage, with warning/explosion FX and cleanup. Marking
  the attacker as an enemy hazard preserves the existing wave friendly-fire
  exclusion. Regular and volatile bombers retain their separate fuse values.
- Chemical and hazmat deaths, plus actual tabun grenade detonations, create a
  finite six-second cloud. The former `chemical_cloud` effect was never loaded.
  The new cloud uses the installed C1 MP tabun effect
  `weapon/grenade/fx_gas_poison_mp`, a real FX entity, native MOD_GAS damage,
  tabun shellshock, line-of-sight and gas-mask checks. FX and the damage owner
  are removed on expiry or match end. Work belongs to the level so deleting a
  dead soldier cannot cancel the gas. The original IW5 chemical tank/cloud
  lifecycle is the reference; C1's shipped MP effect/shock replace unavailable
  MW3-only FX names.
- AI kills retain scoreboard kill counts and Survival credit/performance
  rewards, without advancing ordinary MP class killstreaks. Purchased Survival
  kiosk support retains its separate path.

AI skins remain the existing real aitype/character contract, which checks model
mount availability before attaching later-build gas-mask/backpack assets. This
iteration does not invent missing C1 gas character models or claim live visual
acceptance of every imported skin. The poison effect and its weapon/shock are
confirmed in the installed common MP asset inventory.

## Mounted table requirement and verification

Mounted StringTables precede loose fallback CSVs. The matching client source
change packages the eleven changed tables into the existing
`patch_common_gametype_survival.ff`, before the base Survival tables. Merely
copying the server CSV would leave the old mounted roster active.

Tests execute the actual GSC bodies for type attributes, repeated waves,
soldier/dog spawn contracts, offhand selection, C4 handoff/damage and chemical
grenade/cloud paths. VM/native boundaries are doubled; these tests do not claim
live projectile or GFx acceptance. A compressed before-table fixture verifies
all 260 authored rows, preserving counts/bosses/dogs and allowing only the 33
reviewed type swaps and two missing repeat-column repairs. Static resolver
gates pass for Survival, MP and Coop. The built candidate fastfile was decrypted
and decoded: all 13 original kiosk assets are byte-identical, and all eleven
new StringTables contain the intended gas and repeat values.

Commands:

    node tests/survival_progression_test.js
    node tests/soldier_projectile_behavior_test.js
    node tests/soldier_reload_magazine_test.js
    python tests/test_survival_progression.py
    python tests/test_survival_wave_tables.py
    node tests/survival_rules_test.js
    python tools/script_resolve.py --category survival --map mp_fav_tropical --only maps/mp/gametypes/_survival --only animscripts/soldier
    python tools/script_resolve.py --category mp --gametype war --map mp_killhouse_sh --only animscripts/soldier
    python tools/script_resolve.py --category coop --map mp_killhouse_sh --map coop_training_mp_killhouse_sh --only animscripts/soldier

No commit, deployment or restart is part of this worker's change.

# Survival pursuit, persistent radar and crate class choice (2026-10-29)

User direction: enemies approach players, Survival alone has permanent UAV,
and the Change Class crate uses actual custom classes and charges only on choice.
The parent separately owns the native HUD menu asset change hiding killstreaks.

## Production script changes

- `_survival_ai.gsc`: leaders update a close 128-unit goal each second instead
  of stopping900/384 units away. Newly spawned Survival infantry disable native
  static cover immediately and mark the existing `coop_no_cover` contract, so
  shared animscripts do not re-enable it. Native threat acquisition, weapon
  engagement range, real aiming/shooting, grenade and dog combat remain active.
- `_survival_squad.gsc`: followers stay within128 of the leader rather than
  expanding their allowed lag to600 when any squad member sees an enemy.
  Existing promotion and aggressive handoff remain intact.
- `_survival.gsc` and `_uav.gsc`: initialize the normal team radar path and give
  its local effective UAV count a floor1 only for category `survival`, team
  `allies`. No UAV is awarded, aircraft count mutated or fake reward granted.
  Purchased UAV expiry cannot remove the baseline; double-UAV and native
  counter-UAV jamming semantics remain. MP/Coop/Zombies and axis are unchanged.
  The real retained `GScr_SetTeamRadar` writes `level.teamHasRadar`; native
  `ClientEndFrame` supplies `ps.radarEnabled`, with C1 radar/type publication in
  `radar_state.c`/`radar_types.c` (the existing IW4/T5 native path).
- `_survival_kiosk.gsc`: Change Class no longer buys the next weapon from the
  ladder. It closes the kiosk and opens existing `game["menu_changeclass"]`
  through native `openMenu`, preserving the real profile-populated class UI.
  It saves/wraps the existing `level.class` callback, validating an outstanding
  transaction before the normal `_menus::menuClass` mutates class state. Only
  mapped custom classes supported by the existing `_class::codol_hasLoadoutClass`
  helper qualify. Balance is rechecked on selection and debited once; same,
  invalid, unaffordable, cancelled or no-longer-live choices cost nothing.
  No wait separates acceptance/debit/intent consumption and normal class apply.
  The one-shot immediate-change permission is cleared even when MP grace
  short-circuits its callback. Old bought primary weapons no longer overwrite
  the selected class; purchased armor/perks/support persist. The normal class
  rebuild clears action slots, so `after_class_change` republishes owned support
  without granting or buying it again.
- `_survival_test.gsc`: its existing developer-only class verb now invokes
  `level.class`, so native tests exercise the same registered handler as C1.
  No `_menus`, `_teams` or `_bots` source changes were required.

## Validation

Actual function tests pass:

    node tests/survival_pursuit_radar_test.js
    node tests/survival_kiosk_class_choice_test.js
    node tests/survival_kiosk_economy_test.js
    node tests/survival_progression_test.js
    node tests/survival_rules_test.js
    node tests/survival_kiosk_markers_test.js
    node tests/soldier_projectile_behavior_test.js
    node tests/soldier_reload_magazine_test.js

The new tests execute actual hunt/follower/UAV/transaction/cancel/open bodies.
They cover moving/no player goals, aggressive handoff, native cover input,
permanent radar despite support expiry, doubles/jamming, other categories/team,
free menu open, valid choice, duplicate reply, same/invalid custom class,
insufficient selection-time wallet, Escape/back/quit and death/down/disconnect.

Full Survival/Favela and MP/war/Killhouse built-EXE resolvers pass; narrow
Coop training soldier closure also passes. Existing EXE SHA256 is
`5BE2D5F7942F6A46749AA714FDDEFC922BF1348D5DD52BA33C6D3BCC8B9D6EC5`;
no native code rebuild was required for these registered script facilities.

Owned native probe118032 on UDP17761 ran Favela wave1 with four spawned actors,
no spawn failures or script runtime/missing-table fatal. Infantry advanced from
roughly y2900 to2100 while the bot moved. A passive read of this owned process,
relocated using its actual EXE base610000, found `teamHasRadar=[0,0,1,0]` and
player0 `ps.radarEnabled=1` at level time43750. This verifies the real radar
producer, not a fake marker or award.

A separate owned native probe117512 opened crate row1 with wallet5000: the log
retained5000 after open. Choosing custom2 through the production callback
changed the composed weapon and left4000 (the actual authored price is1000).
A duplicate custom2 reply kept the same weapon and wallet4000. No runtime fatal
occurred. Both owned processes exited. Compact evidence is
`tests/fixtures/survival_pursuit_radar_class_probe.json`; full logs are under
`obj/survival_pursuit_radar_probe` and `obj/survival_kiosk_class_probe`.

No user process, input, restart, deployment or commit was performed. Real C1
class-menu mouse/Escape and UAV appearance remain parent/user acceptance work.
The native movement sample proves progress, not completion of every map route.

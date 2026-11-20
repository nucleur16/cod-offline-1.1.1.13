# Rawfiles branch consolidation audit, 2026-10-29

Repository: CODOL-EMU/13-rawfiles, active native submodule `bin/home/codol_rawfiles`.
Initial active tip: `12d97c73e18569252795ac08d81de985c94027c6`.
The non-pruning fetch moved origin/main from 1a4d7de to b72688e, moved
feat/survival-mode from 70903dc to 56fca73 and discovered deploy/survival-mode.
There are 25 live remote heads, two local heads, and one active worktree. No
literal dev branch exists in this snapshot. Preserve it if subsequently found.

Before merge work, 29 local archival refs were created beneath
`refs/archive/rawfiles-consolidation-20261029`: all local/remote branch tips,
plus pre-fetch main and feat/survival-mode tips. No remote refs were changed.
The machine-readable inventory, patch equivalence and merge-tree receipts are
in `rawfiles-branch-audit-20261029.json`. Merge-tree previews write object data
but do not change HEAD, index or working files.

## Integration order and source review

1. Merge origin/main into the current Survival branch. Its six changed Zombie
   paths have no overlap with the current Survival work. Root performed the
   actual clean merge; the worker reviewed those six staged paths. Preserve
   current Survival, native launchers, waves, economy and reward rules.
2. Merge the small Zombie feature branches, rechecking after each commit:
   factory teleporter audio (589333b, two files), wallbuy terminals (5d2601c,
   one file), factory fence traversal (95318ca, one file), and perk-machine
   collision (6f87f62, three files). Before main integration all four previewed
   clean; several touch the same paths as main and require fresh previews.
3. Integrate HotJoin's three semantic fixes: denied-team-popup feedback and
   bot-seat reservation (da08746), integral reservation counts and bounded bot
   removals/permission recheck (c1b41d2), and prematch-completion gating of live
   balance warnings (045da5f). They are absent from current source. The branch
   conflicts in `_teams.gsc`; retain Survival's forced-team logic while adding
   the explicit reservation and countdown behavior.
4. Consolidate represented historical checkpoints only after explicit semantic
   review. Do not restore entire historical trees to satisfy ancestry.

The historical MP checkpoint a1dbc86 changes 139 files (+4481/-5190); its tip
with podium/postgame fixes has 50 conflict paths against the original current
branch. The Zombies checkpoint 567d38a changes 494 files (+12192/-38059) and its
settlement tip has 73 conflict paths. Their narrow leaf fixes are already
present: `_codol_top_players` requires hasSpawned, `_menus` preserves postgame
state for queued party class replies, and `_zombiemode` initializes, guards and
sets zm_end_report_requested before settlement. Broader old checkpoint changes
need individual comparison against evolved native-compatible implementations.

Eight of nine Survival feature commits are patch-equivalent to current. The
remaining migration commit 6e2db39 has an evolved equivalent: current Survival
already runs on the MP chain, with subsequent authored fixes. The feature test
harness is byte-identical to current. A blind merge produces 17 conflicts and
would risk restoring superseded gameplay/wave tables. Security commit 2cdc49c
is patch-equivalent to current; deployed equivalent 2ea31a7 is already an
ancestor. Record those reasons when consolidating ancestry without tree changes.

## Incoming main validation

Source review: melee timing follows the C1 clip's notes with four measured
exceptions; the one-contact-per-swing rule remains. Theater's four machines use
actual mounted old-model variants when available and keep the chosen skin when
power turns on. Script-origin machines avoid the native trigger_use-only
look-at builtin. Wall buys retain authoritative prices and refill the player's
own lethal without charging full stock or switching to offhand equipment.

`node tests/zm_branch_integration_test.js` executes actual GSC bodies for strike
lookup/override/scheduling/cancel and lethal-wall purchases, including full
stock, insufficient funds and fallback lethal behavior. It passes. Existing
Survival rules and kiosk economy tests also pass.

Full source+binary ZM resolver fails with ten preexisting missing builtin names.
The exact before/after comparison has zero new unresolved names; see
`rawfiles-main-zm-gate-20261029.json`. The five changed scripts outside the main
_zombiemode file pass a focused source+binary resolver gate. This is not a claim
of complete Zombie support; the native owner must restore the actual missing
producers before that full gate can pass. No deployment/runtime claim is made.

## Remote cleanup candidates

Already contained in fetched origin/main: codex/hellhound-chase-audio-20261024,
codex/zombies-script-fixes-20261019, dog-parity, feat/zm-old-perk-machines,
feat/zm-theater-perks, feat/zm-wallbuys, fix/zm-factory-pap-door,
fix/zm-melee-notetrack-sync and integrate/zm-20261026. Their tips were archived.
Other heads require integration or documented semantic-equivalent consolidation
before deletion. Root owns all remote cleanup and commits. No worker branch
deletion, merge commit or runtime edit was performed during this audit.

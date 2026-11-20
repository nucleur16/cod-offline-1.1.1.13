# Historical rawfiles checkpoint review

Reviewed snapshots: a1dbc86 (MP maintenance,139 changed paths) and 567d38a
(Zombies maintenance,494 changed paths). These are broad old snapshots, not
small independent patches. The function/path inventory is retained in
rawfiles-historical-function-delta-20261029.json. It records each old function
missing from the same current file, current added functions, normalized body
matches, and the old missing bodies. Cross-file movement and renamed evolved
implementations were then reviewed directly.

No historical code file is missing from the current tree. Only the two
release-lineage JSON files were absent; both are archived byte-for-byte under
rawfiles-historical-lineage/{a1dbc86,567d38a}. The old animated_props.atr is also
archived there; current retains its entries and adds149 lines for later map
ports. The snapshots deleted3/192 paths respectively; those obsolete deletions
are not applied to the evolved tree.

## Missing same-file functions and dispositions

- a1 end_game_text: deliberately superseded by current C1 terminal outcome,
  report settlement and existing defeat/extraction POV. Current explicitly
  excludes the old BO1 GAME OVER touring overlay; restoring it would regress
  that design.
- a1 play_sound_2d_all: old music helper is superseded by current native
  play_music_cue/music_wave/music_game_over ownership. No current call remains.
- a1 quickrevive_hint_think: the old solo-life price behavior is intentionally
  absent; current follows C1's faster teammate revival and explicitly awards no
  solo spare life. Preserve current descriptions/economy.
- a1 six powerup_test_* helpers: useful unique diagnostic tools. Ported narrowly
  into maps/_zombiemode_powerup_test.gsc and called from current powerups init.
  They are inert unless developer=1 AND scr_zm_powerup_test_enabled=1. Existing
  powerup/equipment/native button handlers are used; no historical gameplay
  replacement is added. Source+binary focused resolver passes. Actual-function
  tests cover double opt-in and exact client selection/refusal.
- 567 aim_point: moved to animscripts/soldier_shared.gsc and included by current
  soldier combat. It is not missing behavior.
- 567 role_fire_anim: replaced by weapon-family fire_anim_for(shots), including
  proper pistol/shotgun and projectile handling. Preserve that implementation.
- 567 pick_death_anim: replaced by get_death_anim, retaining damage yaw, cover,
  traverse and explosive-aware animation choice rather than a small random list.
- 567 civ_body/civ_head and cast_models/cast_models_for: hardcoded model/head
  pairs are replaced by authored character/aitype composition and actual actor
  cast. Current coop training explains why script_model head attachments were
  replaced; Favela civilians call spawn_actor with their authored classname.
- 567 six simple Thundergun helper functions: superseded by the complete T5
  Thundergun port (current file documents source lines, actual muzzle/forward,
  range/cone selection, fling/knockdown/ragdoll/points and view effects). Restoring
  the old 480/1200-distance helper would replace richer current behavior.
- 567 box_weapon_model_plain: generic optional-attachment blacklist is replaced
  by weapon_model_hide_tags using the actual awarded weapon's hide-tag data,
  intersected with the model skeleton. The old blacklist hides required scopes.
- 567 codolNoteClientSplash/codolClientSplashVisible/codolDrainMenuPulses:
  historical timer/queued open-close workaround is superseded by current native
  closed splash-menu slots and pulseScriptMenu C1 command-batch semantics.
  Current source documents that old queued q/close commands grabbed input or
  closed active splashes. Preserve current per-slot close ownership.

The first inventory pass falsely marked dump_missing_anims/delete_glock as
missing because of mixed source formatting; direct definitions were verified
and those entries removed. The breach diagnostic remains in current source.

## Remaining integration policy

Narrow incoming fixes were merged as code (main Zombie changes, four authored
Zombie branches, HotJoin) and tested separately. For these historical snapshots,
retain evolved implementations and restore only missing diagnostics/data above.
A subsequent ancestry merge must explicitly describe historical reconciliation,
not claim that all old source was applied verbatim. Commit/reference archives
preserve exact old work. Source review and focused tests do not qualify runtime
visual behavior or the still-pending full Zombie native builtin gate.

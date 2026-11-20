# Local Survival catalogue and deployment (2026-10-28)

The active in-process host uses `p2p/p2p_host.ini`; its script root is
`main/sh1x_rawfiles` in the game install, not `p2p/home`.

Changes in this iteration:

- Staged the tracked Survival script/data set from `rawfiles_survival_deploy_wt`
  e079af0, then the militia guard fd2203e, into the active script root. The data
  comes from CoDOL's authored addon entities; no coordinates were invented.
- The native host rawfiles submodule merges fd2203e with its newer MP quick-team
  allocation and team-balance changes. Survival forces allies, prevents room-fill
  bots/autobalance, permits class changes between waves and gates wave respawns.
- `tools/survival_catalogue.py --game <install>` writes
  `p2p/survival_maps.txt`. A listed map needs its MP archive, an actual placement
  script with player starts/squad leaders/kiosks, a dispatch case, and the shared
  Survival rules/archive. The installed catalogue contains 14 maps.
- Client `usermaps_patch` comaplist row 59 is Favela Survival; thirteen other
  supported choices replace the dead Afghan-only catalogue. Existing map names
  and previews are reused; four development-map preview cells are empty because
  their image assets are excluded from the shipping UI pack.
- MatchController's Survival quickmatch button requests the supported Favela
  session ID, matching row 59. Its stored ActionScript class was recompiled.
- The Python lobby normalizes Survival after all map-selection branches, so
  virtual-lobby rooms and explicit map pins also load an MP level with Survival
  rules. The obsolete Afghan spelling uses a stable supported default; repeated
  lobby/handoff calls no longer advance a process-local map rotation.

Validation:

- Native GSC resolver against active deployed scripts: all 14 Survival MP maps,
  plus normal MP Killhouse, passed (exit 0).
- `python tests/test_survival_catalogue.py`: passed missing-archive/incomplete-
  placement availability checks.
- Backend `python selftests/map_handoff_selftest.py`: all tests passed, including
  stable Survival resolution and forced-map routing.
- Built candidate `common_extras`, `chinese_locale`, `english_locale`,
  and `common_ui_mp` using Kernel33-consol's zone builder (zero failures).
- Verified all fourteen table names match the generated installed catalogue,
  table IDs are unique, row 59 matches the compiled Survival button's ID.
- Deployed all four candidate zones; previous files are saved under
  `p2p/survival_zone_backup_20261028`. Shared initial script replacements are
  backed up under `p2p/survival_script_backup_20261028`.

These checks establish compilation and installed data availability. They do not
claim every one of the fourteen maps has been played through in C1. The parent
session performs native map/rotation probes and the final client/lobby restart.

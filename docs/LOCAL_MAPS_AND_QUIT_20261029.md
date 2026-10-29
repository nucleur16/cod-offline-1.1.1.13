# Local map ports and quit confirmation, 2026-10-29

The local test install uses the existing `--maps all` build. `shipping_maps.txt`
is unchanged: all13 development ports, including Hunger and Derail, remain dev.
No map archives or images are promoted into a public shipping pack by this change.

The local selector adds Shi No Numa (`zm_sumpf_sh`), Der Riese
(`zm_factory_sh`), Kino der Toten (`zm_theater_sh`), and the ten dev MP ports:
`mp_abandon_sh`, `mp_dome`, `mp_sd_jardin`, `mp_hardhat_sh`, `mp_strike_sh`,
`mp_qadeem_sh`, `mp_highrise`, `mp_hunger`, `mp_vacant`, `mp_brl_derail`.
All13 active cb2 rows occur exactly once and their main FFs are installed.
The installed AssetExplorer decoder resolves all13 actual map thumbnails,
including the streamed usermap images. Derail currently has no separate
`_load.ff`; enabling its selector does not qualify its gameplay/load behavior.

Rebuild the four relevant local packs without altering the ship/dev manifest:

```powershell
python build_zones.py codol_common codol_common_ui_mp codol_english_locale codol_chinese_locale --candidate --maps all --out output_local_maps_test
```

The local install also takes `files/client_shipRetail/patch/sound_channels.txt`
with the same all-maps filter. The selector table, arena entries, map images and
both sets of localized names come from the existing source assets.
Before installation, decoded pack comparison found no removed existing assets:
only the common map table/arena and UI movie changed, plus new map images/strings.
The previous pause-class movie, Survival HUD postgfx pack and native binaries
are preserved.

The top-right Quit button already requests PAGE_POPUP_SUMMARY. Navigation was
bypassing that page and routing straight to PAGE_QUIT whenever AS_IsFirstLogin
returned true. It now always displays the original SummaryView progress/quit
screen. Its existing Back to Game and Quit Game buttons retain their events;
explicit confirmed PAGE_QUIT still reaches CommonUtil.quit.
Only gotoPageByPageObj changes in the compiled controller. All15 other method
bodies, class/script metadata and existing constant-pool entries are unchanged.
The baseline also refreshes the previously stale patch_locale_policy.h pin to
its already committed Survival HUD merge entry; that native source is unchanged.

Validation: three quit regression tests, six pack-policy tests and five shared
UI baseline tests pass. Quit behavior fixtures execute the real AS method bodies
in Node, reproduce/reject the old first-login bypass, and exercise both first-login
states with and without ranked-match state. They do not claim native GFx execution.
The compiled ABI audit and its before artifact are in
`docs/baselines/20261029-quit-confirmation/`. New live-client popup/map-play
acceptance remains pending. Runtime hashes, map selection and image provenance
are recorded in the local install manifest, outside Git.

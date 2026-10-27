# Consolidated patch zones (2026-10-26)

The 17 project zones that installed in `client_shipRetail\patch` under their own names are built into five zones,
one per load list and language; codol_infected_cyborg_postgfx_mp stays on its own (C#-written: its assets point into
its own runtime blocks). The source zones and their `--verify` against their origins are unchanged.

## Old name -> new zone

| old zone (codol-classic-zz and installs before this) | new zone | list | languages |
|---|---|---|---|
| 000_codol_party_modes | codol_common | ahead of code_pre_gfx | both |
| codol_loading_mode_names | codol_common | ahead of code_pre_gfx | both |
| codol_zm_deathmachine_audio | codol_common | ahead of code_pre_gfx | both |
| controller_ingame | codol_common | ahead of code_pre_gfx | both |
| usermaps_patch (tables, arena, animtree, vision) | codol_common | ahead of code_pre_gfx | both |
| usermaps_patch (its GBK map names) | codol_chinese_locale | ahead of code_pre_gfx | Chinese / no valid language |
| voice_ui_mp | codol_common_ui_mp | ahead of the UI zones | both |
| usermaps_ui_mp | codol_common_ui_mp | ahead of the UI zones | both |
| codol_field_orders_postgfx_mp | codol_common_postgfx_mp | after code_post_gfx | both |
| controller_glyphs_postgfx_mp | codol_common_postgfx_mp | after code_post_gfx | both |
| codol_english, codol_english_font, codol_english_ui, codol_english_ui_extra, codol_english_usermaps, codol_english_zombies_base | codol_english_locale | ahead of code_pre_gfx | English (`k33_english 1`, default) |
| codol_chinese_ui, codol_chinese_ui_extra | codol_chinese_locale | ahead of code_pre_gfx | Chinese |
| codol_infected_cyborg_postgfx_mp | codol_infected_cyborg_postgfx_mp (unchanged) | after code_post_gfx | both |
| codol_zombies_endless (retired 2026-10-23) | - (folded into usermaps_patch's comaplist, now codol_common) | | |

Kernel33 skips each old name while its new zone is in the patch folder (`patch folder: skipped <old>.ff: merged into
<new>.ff`); a folder with only old names loads as before. controller_glyphs.c looks its materials up in
codol_common_postgfx_mp, else in controller_glyphs_postgfx_mp.

## What was proven

`tests/check_zone_packs.py` (shipping and `--maps all`): for English, Chinese and native language selection the old
16 / 12 / 12 zones and the new 5 / 5 / 5 put the same assets into the game, byte for byte and in the same list
(shipping build: 10500 / 769 / 769 assets; all maps: 10521 / 790 / 790); no list carries a name twice; every old
asset is in exactly one new zone except five copies no language ever used (an earlier old zone carried the name,
C1 keeps the first): codol_loading_mode_names' maps/mp/gametypes/gun.txt, oic.txt, shrp.txt (000_codol_party_modes'
win; both name the same English text) and codol_english_zombies_base's ZOMBIE_THUNDERGUN / _UPGRADED (codol_english's
"Thundergun" / "ZeusCannon" win, as before). codo.gfx's LevelChooser is #18's endless tag, and comaplist column 23 is
1 for zm_asylum_sh and zm_prototype_sh.

Against the 1.1.1.13 install's patch folder of 2026-10-26 (`--old`, all maps) the only difference is codo.gfx: the
installed voice_ui_mp.ff (md5 15b1062a) predates #18 and main's #22-#27; the new zone carries main's movie
(voice_ui_mp source zone build bf32388d, recorded as its `>build-md5`).

Recorded builds (`zone_packs/*.csv` `>build-md5` / `>build-md5-all`):

| zone | shipping | all maps |
|---|---|---|
| codol_common | 427b0963779bd150429a73f97b7889f1 | be9e2f84f94a7c36ab8fef7b867fa0ce |
| codol_common_ui_mp | 30123329867ab7e907f89dd5fb7a7ffc | ce658995bd4eea828d7b6bda876850ed |
| codol_common_postgfx_mp | c12ab5a0b605277e16ae638ffcf0ec34 | c12ab5a0b605277e16ae638ffcf0ec34 |
| codol_english_locale | b26d2912b633a512bc74e5fcedb4bf9b | 63a92dc820de8af6a5c0258fbac40528 |
| codol_chinese_locale | 09548379a33622ac7452633f39a18d68 | 1de3a9975b50b6ffa51594db04158fe6 |
| codol_infected_cyborg_postgfx_mp | 588e52471f8779fc272005914b99b1ce | same |

`ui_baseline.json` was re-recorded: kernel33.c (already drifted on main since #22) and patch_locale_policy.h changed.

Not yet seen in the client: nothing here has been loaded by C1. Acceptance is a start in English and in Chinese with
the old zones removed (Kernel33.log: 5 zones queued: 2 ahead of code_pre_gfx, 2 after code_post_gfx, 1 ahead of
the UI zones), the front end, a zombies room with the
endless button on Asylum / Prototype, an MP match with the pad glyphs, an Infected match.

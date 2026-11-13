# Selectable HUD styles (`k33_hudStyle`)

The combat HUD of 1.1.1.13 can show the layout of two later Call of Duty Online builds, switched live.

| `k33_hudStyle` | id | build | state |
|---|---|---|---|
| 0 | `stock` | 1.1.1.13 | the game's own HUD |
| 1 | `v12` | 1.2.1.12 | shipped |
| 2 | `v26` | 2.6.9.7 | shipped |
| 3 | `v33` | 3.3.3.3 | not shipped (blocked, see below) |
| 4 | `v35` | 3.5.2.6 | not shipped (blocked, see below) |

`k33_hudStyleList` in this build: `0:stock;1:v12;2:v26`.

Nothing here has run in a client yet. The first half of this document is static evidence; "Needs live acceptance"
lists what only a match can show.

## How it works

The weapon bar, score bar, minimap, killstreak column and perk display of every CODOL build are native menus of
`ui_mp/hud_720.txt` (menu assets, type 26), not Scaleform. A style is therefore menu data:

- `tools/build_hud_styles.py` converts the items of a later build's HUD menus to the 1.1.1.13 layout and appends
  them to 1.1.1.13's own menu of the same name. The stock items stay, each behind
  `dvarint("k33_hudStyle") != 1 && dvarint("k33_hudStyle") != 2 && {its own rule}`; a style's items sit behind
  `dvarint("k33_hudStyle") == N && {their own rule}`. The menu's own visibility becomes
  `(stock style && {1.1.1.13's rule}) || (style 1 && {1.2.1.12's rule}) || (style 2 && {2.6.9.7's rule})`.
  `{..}` is a whole statement used as one operand, the form the stock menus use for their sub-expressions, so no
  wrapped expression is re-parenthesised.
- C1 evaluates these expressions every frame. `set k33_hudStyle 2` therefore changes the HUD in the same frame:
  no menu reload, no vid_restart, no patch in C1. A value that is not a shipped style shows the stock items, so
  the HUD is never empty.
- `hud_style.c` only owns the dvars: `k33_hudStyle` (int 0..4, archived, registered once `C1DvarsReady()`, a
  command-line `+set k33_hudStyle N` re-issued as voice.c does) and `k33_hudStyleList` (string). A saved value that
  is not in the list is set back to 0 and logged once.

### One copy of each menu

C1 keeps the first menu it links under a name (docs/SURVIVAL_HUD_20261029.md: `Load_MenuDefPtr` 0x4C17B0 ->
0x4CAFF0 -> `DB_LinkXAssetEntry`). `survival_hud_postgfx_mp` already replaces `killstreaks_hd` and `scorebar_hd`
to hide them in Survival, so those two menus are not shipped a second time: `tools/build_survival_hud.py`
`styled()` passes its own authoring through `fold_survival()`, and the survival zone's menufile carries both
gates. `hud_styles_postgfx_mp` carries the two menus the survival zone does not own:

| menu | zone that carries it | items: stock + v12 + v26 |
|---|---|---|
| `weaponbar_hd` | hud_styles_postgfx_mp | 25 + 26 + 26 |
| `minimap_fullscreen` | hud_styles_postgfx_mp | 16 + 17 + 17 |
| `killstreaks_hd` | survival_hud_postgfx_mp | 69 + 83 + 83 |
| `scorebar_hd` | survival_hud_postgfx_mp | 33 (+4 Survival panel) + 33 + 41 |

Survival behaves as before in every style: `killstreaks_hd` is hidden by its menu, the stock score items by each
item, and the Survival panel's four items do not depend on the style. A style's own score-panel items carry
`g_gametype != "survival"` as well (they are MP items, like the stock ones); the kill/death bar keeps the later
build's own rule, which shows it in Survival.

Both zones are parts of `codol_common_postgfx_mp` (after `code_post_gfx`, permanent). `hud_styles_postgfx_mp`
is listed ahead of `survival_hud_postgfx_mp`, so the images and materials are linked before either menufile
names them.

### Menus 1.1.1.13 does not have

`weaponbar_hd_extend`, `reloadbar` and `kd_rate_bar` exist only in the later builds. 1.1.1.13 draws the menus its
own `hud_720.txt` lists, so their items join a menu it does draw (`weaponbar_hd`, `weaponbar_hd`, `scorebar_hd`),
each with its own menu's visibility as an extra operand and moved by the difference of the two menus' origins
(the generator refuses a menu whose rect size or alignment differs from the host's, and an item whose position
is an expression when the origins differ).

## What each style covers

Counts are items kept of the later build's menu (`zones/hud_styles_postgfx_mp/menufile/hud_styles.report.txt` has
every dropped item with its reason).

| later menu | v12 (1.2.1.12) | v26 (2.6.9.7) | lands in |
|---|---|---|---|
| `weaponbar_hd` (weapon name, ammo counter, grenade slots, alt-weapon hint, bar art) | 24 of 24 | 24 of 26 | weaponbar_hd |
| `weaponbar_hd_extend` (zombies action slot 1) | 1 of 5 | 1 of 4 | weaponbar_hd |
| `reloadbar` (low ammo / reload warning) | 1 of 1 | 1 of 1 | weaponbar_hd |
| `scorebar_hd` (team scores and bars, timer, game type, winning/losing, bomb timers) | 26 of 30 | 27 of 61 | scorebar_hd |
| `kd_rate_bar` (score / kills / deaths bar) | 7 of 11 | 14 of 20 | scorebar_hd |
| `killstreaks_hd` (streak pips, five slots with plates and key hints) | 83 of 83 | 83 of 83 | killstreaks_hd |
| `minimap_fullscreen` (115x115 frame and compass layers) | 17 of 17 | 17 of 17 | minimap_fullscreen |

The two builds share their HUD art (the 13 images are pixel-identical in both installs, so one set is carried,
`k33hud12_*`), but not their layout: every `weaponbar_hd` grenade slot, the killstreak pips and plates, and the
score bar differ in position, size or rule (compared after conversion: 46, 356 and 103 differing listing lines),
which is why v12 is a style of its own and not left out as a duplicate.

### Visibly missing versus the original builds

- **Perks.** 2.6.9.7's `perk_sidebar_hud` needs `hasperk`, `getperktime`, `getperkstartindex`, `getperkstartcount`
  and `getperksshow`; 1.2.1.12's perk menus are driven by `ui_perk_menu_*` dvars and local variables its own
  scripts set. 1.1.1.13 has neither, so **1.1.1.13's own `perks_info_hd` stays in every style**.
- **Fire-mode tag** above the weapon name (v26): `getweaponfiretype` is not a 1.1.1.13 operator. Text and plate
  are both left out.
- **Assists** (and revives in co-op / zombies) in the kill/death bar: `player("assists")` / `player("revives")`
  are not fields of 1.1.1.13's `player` operator. The value and its caption are left out; the bar's art keeps
  an empty fourth cell.
- **Zombies charge-attack hint** (`weaponbar_hd_extend`): `canChargeAttackIconShow` and ownerDraw 297 do not
  exist in 1.1.1.13. The slot's killstreak icon item is kept.
- **The later builds' Survival / Evac panels** inside `scorebar_hd` (33 items in 2.6.9.7, 4 in 1.2.1.12): Evac does
  not exist in 1.1.1.13, and Survival keeps Kernel33's own panel (the survival HUD zone's component).
- **Fonts.** The later menus use font numbers 11, 12 and 14, which 1.1.1.13's `UI_GetFontHandle` (0x005EC790, 3..10)
  does not know; they become 0, which picks a font by text scale (the survival HUD panel was converted the same way).
  Text metrics can differ from the original.
- **Killstreak icons** are 1.1.1.13's: the slots read `tablelookup("mp/coKillstreakStatsTable.csv", 0, id, 15)`
  like the later builds, against 1.1.1.13's own table (see evidence).
- `hud_fullscreen`, `killcam_fullscreen`, the player cards, `field_orders`, `combat_performance` and the Scaleform
  overlay (scoreboard, chat, names, notifications) are not part of a style: they stay 1.1.1.13's.

## Static evidence

Measured means read from the user's own installs or the built files; inferred is marked.

- **Operator tables.** 1.1.1.13: 218 names at 0x00CD08A8 (zonekit/menus.py). 1.2.1.12: 232 names at 0x00D01690 of
  its `codoMP_client_shipRetail.exe` (measured; data sections are readable in that build), equal to the first 232
  of the table zonekit/menus.py uses for 2.6.9.7. A later item is kept only if every operator maps by name;
  `istrain` and `isevac` become 0 (modes 1.1.1.13 does not have), `issurvival` becomes
  `dvarstring("g_gametype") == "survival"`, and `localvarint("isCombat")` becomes
  `dvarstring("ui_gametype") == "combat"` (what the later `scorebar_hd` onOpen stores in that variable; the host
  keeps 1.1.1.13's menu, which has no onOpen).
- **`player()` fields.** C1.exe 0x5DE844..0x5DED9A compares the argument with teamname, otherteamname, dead,
  clipAmmo, clipAmmo_left, stockAmmo, fragAmmo, smokeAmmo, tactAmmo, nightvision, killstreak, killstreakTarget,
  killstreakID0..4, killstreakTarget0..4, score, deaths, kills, ping (measured: the strings the code references).
  Every kept item uses only these.
- **ownerDraw numbers.** Kept items use only numbers a stock 1.1.1.13 HUD menu uses (50 of them, `facts.json`).
  Where both builds name an item the numbers agree (offhandFragIcon 103, offhandSmokeIcon 104, offhandTactIcon
  284, lowammowarning 120, minimap_map 159, minimap_player 150, ...). That a number draws the same thing in
  both engines is inferred from this, not disassembled.
- **Layout.** 2.6.9.7 menus: window 0xA8, six item statements (zonekit/menus.py Converter); 1.2.1.12: the
  1.1.1.13 layout (its loader reads itemDef_s as 380 bytes and menuDef_t as 400, measured in AssetExplorer's
  transpiled loader). Items that set a 2.6.9.7-only field (window image, statement slots 3 and 5) are dropped.
- **Materials.** Each material a kept item names is resolved in `sources/facts.json`:
  *carry* (13: not in 1.1.1.13, or 1.1.1.13's has other pixels) or *stock* (same pixels as 1.1.1.13's, or a
  reference in the later build too, or - for the three stencil/nuke materials whose image has another name -
  a material 1.1.1.13's own HUD menus draw under that name; the last case is inferred equal, not compared).
  A carried material is written in the exact layout of 1.1.1.13's own HUD materials (measured:
  `hud_scorebar_topbar` inline in common_mp's `ui_mp/hud_720.txt`: technique set `2d`, sort 0x30, one colorMap,
  state bits 0x18125165 / 0xE00E0002), the layout `controller_glyphs_postgfx_mp` already ships; the test
  rebuilds that zone's material byte for byte with the same function. The later builds' materials are `2d` with one
  image (measured with zone-materials; their state bits were not decoded, so equal blending is inferred from
  the equal technique set and state-flags byte). Carried assets are named `k33hud12_<name>`, never a stock name.
- **Images.** In-zone A8R8G8B8 with a full mip chain and load flags 0x1, the form of the glyph images in the
  same pack. Unlike those, these are not powers of two (8x6 .. 261x37); 1.1.1.13's own HUD images are not either
  (hud_scorebar_topbar is 64x18), but those are streamed without mips. Mipmapped non-power-of-two upload is
  therefore inferred, not measured.
- **Texts.** Every `@KEY` a kept item shows is in 1.1.1.13's localize table, or is one a stock 1.1.1.13 HUD item
  shows too, or is carried: `MPUI_SCORE_CAPS`, `MPUI_KILL_CAPS`, `MPUI_DIE_CAPS` (the kill/death bar's captions)
  go into `codol_chinese_hud_styles` (2.6.9.7's own values) and `codol_english_hud_styles` (SCORE / K / D), parts
  of the two locale packs.
- **Killstreak table.** `mp/coKillstreakStatsTable.csv` column 15 holds the same value in 1.1.1.13 and in each
  later build for every killstreak both have (48 and 47 rows compared by name, 0 differ; the ids in column 0 were
  renumbered, which does not matter: the lookup runs on 1.1.1.13's table with 1.1.1.13's ids). Every column-15
  material of 1.1.1.13's table is defined in the zone of its mode (common_gametype_mp / _coop for the MP streaks,
  common_gametype_zm for the zombies ones, common_mp for the rest), measured with zone-materials.
- **The built pack loads.** `python build_zones.py --candidate` builds all 11 installed zones; AssetExplorer's
  C1 loader emulation loads `codol_common_postgfx_mp.ff` with 0 warnings (179 assets), and the two menufiles it
  resolves list identically to the authored `.txt` files. `tests/check_zone_packs.py` passes on the candidate.
- **Gates.** `tests/test_hud_styles.py` evaluates every generated gate for `k33_hudStyle` 0..5: exactly the selected
  style's items are visible, a value that is not shipped shows the stock items, the menu follows the selected
  build's rule, Survival hides what it hid. It also proves the stock items are byte-identical under their gate.

## Needs live acceptance

1. The HUD changes in the frame `k33_hudStyle` changes, in a match, in both directions (this is the survival HUD's
   mechanism, accepted live there, with one more operand).
2. Each style in Team Deathmatch and Free-for-All: weapon bar, score bar, minimap, killstreak column; positions,
   overlaps with 1.1.1.13's `perks_info_hd`, Scaleform notifications and the field-order panel.
3. Text size and font of the converted items (font 0 instead of 11 / 12 / 14), the ammo counter (ownerDraw 5
   with the later item's font and scale), English captions K / D in their 15-unit cells.
4. The carried images draw (mipmapped non-power-of-two upload) and blend as in the later builds.
5. Search and Destroy / Sabotage (bomb timers), Survival (panel unchanged in every style, kill/death bar shown
   in styles 1 and 2), zombies (the slot-1 item, `! iszm` grenade rules), killcam and spectating.
6. Dvars the later rules read that 1.1.1.13 may not have (`cl_isHostAsSpectator`, `cl_minimapInShoutcast`,
   `heli_remote`, `so_survival`, `ui_gametype`): the rules are written so that 0 / "" is the normal case; that a
   missing dvar reads as 0 in a static-dvar operand is inferred from the stock menus doing the same with
   script dvars.
7. A `set k33_hudStyle 3` (not shipped) returns to 0 with one log line in a local-testing build.

### Manual test

1. `python build_zones.py --candidate`, copy `output_candidate\client_shipRetail\patch\codol_common_postgfx_mp.ff`,
   `codol_english_locale.ff` and `codol_chinese_locale.ff` over the test install's, and a `Kernel33.dll` built from
   this branch (`K33_LOCAL_TESTING=1` for the log lines and `k33_cmdFile`).
2. Start with `+set k33_cmdFile 1`; `Kernel33.log` must have
   `hud style: k33_hudStyle registered (k33_hudStyleList 0:stock;1:v12;2:v26), value 0`.
3. In a private Team Deathmatch, write `set k33_hudStyle 2` into `<exe dir>\k33_cmd_<pid>.txt` (or use the pause
   menu's HUD style control): the weapon bar, score bar, minimap frame and killstreak column switch to the
   2.6.9.7 layout within half a second, without a reload. `set k33_hudStyle 1`: the 1.2.1.12 layout.
   `set k33_hudStyle 0`: stock.
4. `set k33_hudStyle 3`: the stock HUD, the dvar back at 0, one line
   `hud style: k33_hudStyle 3 is not in this build (0:stock;1:v12;2:v26); showing the stock HUD`.
5. Quit and start again: the last shipped value is still set (archived).
6. Repeat step 3 in Survival: the enemy / credit panel is the same in all three, the MP score panel and the
   killstreak column stay hidden.

## Why 3.3.3.3 and 3.5.2.6 are not shipped

- **Their operator numbering is unknown.** 3.3.3.3's `hud_720.txt` does not decode with 2.6.9.7's table: the
  weapon bar's `( weaponname ) != "none"` reads as `( savegameavailable ) != "none"` (operators were inserted
  ahead of it), and items use numbers up to 273. The table could not be read from either executable: 3.3.3.3's
  has no operator name strings in the file, 3.5.2.6's is packed (.tp3 / .tvm0). Converting with a guessed
  numbering would produce expressions that parse and mean something else.
- **Their HUD leans on engine code 1.1.1.13 lacks.** `killstreaks_hd` is replaced by `killstreaks_hd2` (124 items
  with three unknown operators), `scorebar_hd` uses `hasperk`, `isdeathmarch` and ten operators past 2.6.9.7's
  table, and the dumper cannot resolve their material names (the 0x68-byte material layout).
- What would unblock them: the 3.x operator table (an unpacked 3.3.3.3 or 3.5.2.6 client, or a memory dump), then
  the same generator with a `v33` / `v35` entry in `STYLES`; materials would be re-authored from the exported
  images exactly as for the two shipped styles, so the 0x68 materials and their technique sets are not an obstacle
  for `2d` HUD art.

## Rebuilding

- `python tools/build_hud_styles.py` regenerates `zones/hud_styles_postgfx_mp/{menufile,material}`,
  `zone_source/hud_styles_postgfx_mp.csv`, the two `codol_*_hud_styles` zones and the survival HUD's menufile
  from `zones/hud_styles_postgfx_mp/sources` and `image/`. No install is needed.
- `python tools/build_hud_styles.py extract --dumper <CodolSoundDumper.exe> [--c1 DIR] [--v26 DIR] [--v12 DIR]
  [--images v12=DIR] [--work DIR]` re-reads the installs (defaults under `D:\CoD\CODOL Builds`). It needs a
  CodolSoundDumper built from CODOL-AssetExplorer with the `zone-menus` command (branch `feat/zone-menus`,
  `MenuCommands.cs`) and two changes for 1.2.1.12: the 1.1.1.13 menu layout (`GameBuild.V1_2_1_12` in the layout
  choice) and the material name read at +8 when +0 is null. 1.2.1.12 has a loose `main\` folder without IWDs,
  which hides its packaged images from `export-textures`: `--images v12=DIR` names a folder that holds only the
  client exe and a link to the install's `IIPS` folder.
- Adding or removing a shipped style changes three places that the test compares: the sources, `HUD_STYLE_LIST` in
  `hud_style.c`, and the expected list in `tests/test_hud_styles.py`.

## Integration notes

- `kernel33.c`: `#include "hud_style.c"` after `theatre.c`, `InstallHudStyle();` after `InstallTheatre();`.
- `patch_locale_policy.h` `kCodolPatchZoneMerged`: three rows (`hud_styles_postgfx_mp`,
  `codol_english_hud_styles`, `codol_chinese_hud_styles`), required by tests/test_zone_packs.py. Both files are
  pinned by `ui_baseline.json`, which is not re-recorded here.
- `hud_style.c` takes one `K33OnMainTick` slot. A local-testing build then registers seven before Product Pi's
  on-demand one. K33_MAX_TICKS was 8 and is 12 since the language switch and this feature each took a slot.
- `zone_packs/codol_common_postgfx_mp.csv` has its new build md5 recorded; the two locale packs gained a part and
  were left unrecorded (they already differed from their recorded md5 before this change).

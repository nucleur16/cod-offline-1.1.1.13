# Selectable HUD styles (`k33_hudStyle`)

The combat HUD of 1.1.1.13 can show the layout of a later Call of Duty Online build (2.6.9.7), switched live.

| `k33_hudStyle` | id | build | state |
|---|---|---|---|
| 0 | `stock` | 1.1.1.13 | the game's own HUD |
| 1 | `v12` | 1.2.1.12 | not shipped: a visual duplicate of v26 (see below) |
| 2 | `v26` | 2.6.9.7 | shipped |
| 3 | `v33` | 3.3.3.3 | not shipped (blocked, see below) |
| 4 | `v35` | 3.5.2.6 | not shipped (blocked, see below) |

`k33_hudStyleList` in this build: `0:stock;2:v26`.

Runtime-checked on 2026-11-12 in the offline fork's client (own test folder, Team Deathmatch on Crash, 1920x1080
window): see "Live check". The static evidence is what the generator and the tests check without a client.

## How it works

The weapon bar, score bar, minimap, killstreak column and perk display of every CODOL build are native menus of
`ui_mp/hud_720.txt` (menu assets, type 26), not Scaleform. A style is therefore menu data:

- `tools/build_hud_styles.py` converts the items of a later build's HUD menus to the 1.1.1.13 layout and appends
  them to 1.1.1.13's own menu of the same name. The stock items stay, each behind
  `dvarint("k33_hudStyle") != 2 && {its own rule}`; a style's items sit behind
  `dvarint("k33_hudStyle") == N && {their own rule}`. The menu's own visibility becomes
  `(stock style && {1.1.1.13's rule}) || (style 2 && {2.6.9.7's rule})`.
  `{..}` is a whole statement used as one operand, the form the stock menus use for their sub-expressions, so no
  wrapped expression is re-parenthesised.
- C1 evaluates these expressions every frame. `set k33_hudStyle 2` therefore changes the HUD in the same frame:
  no menu reload, no vid_restart, no patch in C1. A value that is not a shipped style shows the stock items, so
  the HUD is never empty.
- `src/hud_style.c` owns the dvars: `k33_hudStyle` (int 0..4, archived, registered once `C1DvarsReady()`, a
  command-line `+set k33_hudStyle N` re-issued as voice.c does) and `k33_hudStyleList` (string). A saved value that
  is not in the list is set back to 0 and logged once. It also implements what the later HUD reads and 1.1.1.13
  lacks ("Implemented natively").

### One copy of each menu

C1 keeps the first menu it links under a name (docs/SURVIVAL_HUD_20261029.md: `Load_MenuDefPtr` 0x4C17B0 ->
0x4CAFF0 -> `DB_LinkXAssetEntry`). `survival_hud_postgfx_mp` already replaces `killstreaks_hd` and `scorebar_hd`
to hide them in Survival, so those two menus are not shipped a second time: `tools/build_survival_hud.py`
`styled()` passes its own authoring through `fold_survival()`, and the survival zone's menufile carries both
gates. `hud_styles_postgfx_mp` carries the two menus the survival zone does not own:

| menu | zone that carries it | items: stock + v26 |
|---|---|---|
| `weaponbar_hd` | hud_styles_postgfx_mp | 25 + 30 |
| `minimap_fullscreen` | hud_styles_postgfx_mp | 16 + 17 |
| `killstreaks_hd` | survival_hud_postgfx_mp | 69 + 83 |
| `scorebar_hd` | survival_hud_postgfx_mp | 33 (+4 Survival panel) + 43 |

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

## What the style covers

Counts are items kept of 2.6.9.7's menu (`zones/hud_styles_postgfx_mp/menufile/hud_styles.report.txt` has every
dropped item with its reason).

| 2.6.9.7 menu | kept | lands in |
|---|---|---|
| `weaponbar_hd` (weapon name, fire-mode tag, ammo display, grenade slots, alt-weapon hint, bar art) | 26 of 26 | weaponbar_hd |
| `weaponbar_hd_extend` (zombies action slot 1) | 1 of 4 | weaponbar_hd |
| `reloadbar` (low ammo / reload warning) | 1 of 1 | weaponbar_hd |
| `scorebar_hd` (team scores and bars, timer, game type, winning/losing, bomb timers) | 27 of 61 | scorebar_hd |
| `kd_rate_bar` (score / kills / deaths / assists bar) | 16 of 20 | scorebar_hd |
| `killstreaks_hd` (streak pips, five slots with plates and key hints) | 83 of 83 | killstreaks_hd |
| `minimap_fullscreen` (115x115 frame and compass layers) | 17 of 17 | minimap_fullscreen |

### The ammo display (ownerDraw 5)

The later builds draw it with ownerDraw 5 like 1.1.1.13, but their CG_DrawPlayerAmmoValue is another function
(MEASURED: 1.2.1.12 0x00455EF0 against C1 0x004540F0). With the item's rect (x, y, w, h) it draws the clip count
"%1i" at (x, y + h); a white gauge at the fixed placement x = (1 - f) * 99 - 133, y = -41, 99 * f wide, 3.5 high
(f = clip / clip size: it empties toward the right end of weaponbar_bg's frame); and the reserve "%2i" at
(x + 12, y + h + 9.78), scale 0.2991. C1's own function draws clip and reserve side by side from the rect's left
edge and no gauge, so in the later item's small rect the two numbers landed on each other ("4207") and the frame
stayed empty. The generator writes ownerDraw 5 as three items: the clip count (ownerDraw 3001) and the gauge
(ownerDraw 3002), which `hud_style.c` draws when the menu paints, and the reserve, a text item on
`player("stockAmmo")`.

- The clip count is the right hand's clip, plus the left hand's only when C1's own dual-wield test passes
  (0x00417F50: the weapon's akimbo record and weaponDef + 0xBD5). `player("clipAmmo_left")` is not 0 for a
  one-handed weapon; summing it showed twice the magazine (live, first round of fixes).
- Count and gauge are read at draw time from the playerState C1's ammo display reads, so they change in the same
  frame as the reserve (live: a count published on the main tick trailed it, second round).
- The gauge uses the later build's screen placement, not the menu's: the host menu's origin (-10, 0) is taken off
  its rect (live: it sat 10 units left of the frame, first round).

### Implemented natively

| what the later HUD reads | in 1.1.1.13 | here |
|---|---|---|
| its ammo display (ownerDraw 5) | another layout | ownerDraws 3001 / 3002, above |
| `getweaponfiretype` (the fire-mode tag above the weapon name, and its plate) | no operator | `dvarstring("k33_hudFireType")`: `hud_style.c` publishes the `@SF_WEAPON_*_NAME` key of the weapon in hand from weaponDef + 0x350 (fire type) and + 0xB64 (class). Both offsets were located by value over the stock weapon defs (fire type m4 0, m16 / famas 3, usp / spas12 / m21 / rpg 1; class rifle 2, pistol 1, shotgun 5, rocket 9; no other offset fits). Which key a fire type and class get is INFERRED from the keys' names and values (`src/hud_style_policy.h`); 2.6.9.7's code is packed. The labels are carried in both languages |
| `player("assists")` (kill/death bar) | not a field of `player` | `dvarint("k33_hudAssists")`: the local client's row of C1's score records (CG_ParseScores 0x00465240: 19 rows of 0x40 bytes at cg + 0xBEB70, the 7th value of a row at +0x1C, which the sh1x server fills with the assists, `server/sh1x/src/codol/stats/codol_scoreboard_wire.c`) |

The two dvars are written on the main tick while a style is on (they change with a weapon switch or a score message,
not per shot). The ownerdraws share voice.c's CG_OwnerDraw entry hook (0x00459E10): `VoiceOwnerDraw` calls
`HudStyleOwnerDraw` first (a declaration and one call in voice.c); without that hook they draw nothing, which is
logged. `src/hud_style_policy.h` holds the pure parts (fire-type key, clip count, gauge fill and rect), tested by
`tests/hud_style_policy_test.c`.

### Not implemented, and why

- **Revives** (the co-op / zombies captions of the kill/death bar): C1's MP score rows have no revives value, and the
  zombies rows are another message. The value and its caption stay out.
- **Zombies charge attack** (`weaponbar_hd_extend`: `canChargeAttackIconShow`, ownerDraw 297): 1.1.1.13 has no charge
  weapons (1.2.1.12's ammo display reads `zmIsChargeWeaponActive`, which C1 does not have), so there is no state to
  show. The slot's killstreak icon item is kept.
- **The perk sidebar** (`perk_sidebar_hud`: `hasperk`, `getperktime`, `getperkstartindex`, `getperkstartcount`,
  `getperksshow`): those read 2.6.9.7's timed-perk system, which 1.1.1.13's perks do not have. 1.1.1.13's own
  `perks_info_hd` stays in every style.
- **The later Survival / Evac panels** inside `scorebar_hd` (33 items): Evac does not exist in 1.1.1.13, and Survival
  keeps Kernel33's own panel (the survival HUD zone's component).
- The red pulse of a low clip / low reserve in the later ammo display.
- `hud_fullscreen`, `killcam_fullscreen`, the player cards, `field_orders`, `combat_performance` and the Scaleform
  overlay (scoreboard, chat, names, notifications) are not part of a style: they stay 1.1.1.13's.

### Fonts

The later menus use font numbers 11..14, which 1.1.1.13's `UI_GetFontHandle` (0x005EC790, 3..10) does not know.
MEASURED in 1.2.1.12 (0x005F170C..0x005F181D, `UI_GetFontHandle` 0x005F21B0): its fonts 11..14 are four copies of
`fonts/normalFont`'s Font_s under the names "1".."4", so here they are font 7 (normalFont). Before this they were 0,
which picks a font by scale.

### Why v12 (1.2.1.12) is not shipped

The user could not tell v12 from v26 in a client, and the captures agree. The same scene with `k33_hudStyle` 1 and 2
(`shots/b_v12_before.png`, `shots/b_v26_before.png` of the live check) differs in 0 pixels in the minimap, 4 in the
kill/death bar and a few hundred in the weapon bar, killstreak column and score bar: slot plates 25.5 against 31
units, grenade icons two units apart. Both builds' 13 HUD images are pixel-identical. Its sources were removed; the
number 1 stays reserved.

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
  shows too, or is carried in `codol_chinese_hud_styles` (2.6.9.7's own values) and `codol_english_hud_styles`
  (parts of the two locale packs): the kill/death bar's captions `MPUI_SCORE_CAPS`, `MPUI_KILL_CAPS`,
  `MPUI_DIE_CAPS`, `MPUI_ASSIST_CAP` (SCORE / K / D / A) and the seven fire-mode labels `SF_WEAPON_*_NAME`.
- **Killstreak table.** `mp/coKillstreakStatsTable.csv` column 15 holds the same value in 1.1.1.13 and in each
  later build for every killstreak both have (48 and 47 rows compared by name, 0 differ; the ids in column 0 were
  renumbered, which does not matter: the lookup runs on 1.1.1.13's table with 1.1.1.13's ids). Every column-15
  material of 1.1.1.13's table is defined in the zone of its mode (common_gametype_mp / _coop for the MP streaks,
  common_gametype_zm for the zombies ones, common_mp for the rest), measured with zone-materials.
- **The built pack loads.** `python scripts/build_zones.py --candidate` builds all 11 installed zones; AssetExplorer's
  C1 loader emulation loads `codol_common_postgfx_mp.ff` with 0 warnings (179 assets), and the two menufiles it
  resolves list identically to the authored `.txt` files. `tests/check_zone_packs.py` passes on the candidate.
- **Gates.** `tests/test_hud_styles.py` evaluates every generated gate for `k33_hudStyle` 0..5: exactly the selected
  style's items are visible, a value that is not shipped shows the stock items, the menu follows the selected
  build's rule, Survival hides what it hid. It also proves the stock items are byte-identical under their gate.

## Needs live acceptance

Accepted live (captures below): the switch in the frame the dvar changes (both directions), the minimap, kill/death
bar, killstreak column and weapon bar of v26 in Team Deathmatch, the carried images drawing and blending, the ammo
display (clip / gauge / reserve, single hand, after firing and after a reload, same frame), the fire-mode tag and the
assists column, the gauge inside its frame.

Still open:

1. The counters' shadowed text style 3 and the stock-coloured team bars of the score panel: built, generated and
   tested, not yet captured in a client (the later test runs could not log in: other clients held the DW port).
2. An akimbo weapon live. C1's MP has no akimbo weapon defs; the dual-wield path is checked statically (weaponDef +
   0xBD5 is 1 for glockdw_zm and usp_silencer_akimbo_so, 0 for glock_zm and usp_silencer_so) and in
   `tests/hud_style_policy_test.c`. Test with glockdw_zm in a zombies match.
3. Search and Destroy / Sabotage (bomb timers), Survival (panel unchanged, kill/death bar shown), zombies (the slot-1
   item, `! iszm` grenade rules), killcam and spectating, other aspect ratios (4:3, 16:10) for the right-anchored
   groups.
4. Dvars the later rules read that 1.1.1.13 may not have (`cl_isHostAsSpectator`, `cl_minimapInShoutcast`,
   `heli_remote`, `so_survival`, `ui_gametype`): written so that 0 / "" is the normal case.

### Manual test

1. `python scripts/build_zones.py --candidate`, copy `output_candidate\client_shipRetail\patch\codol_common_postgfx_mp.ff`,
   `codol_english_locale.ff` and `codol_chinese_locale.ff` over the test install's, and a `Kernel33.dll` built from
   this branch (`K33_LOCAL_TESTING=1` for the log lines and `k33_cmdFile`).
2. Start with `+set k33_cmdFile 1`; `Kernel33.log` must have
   `hud style: k33_hudStyle registered (k33_hudStyleList 0:stock;2:v26), value 0`.
3. In a private Team Deathmatch, write `set k33_hudStyle 2` into `<exe dir>\k33_cmd_<pid>.txt` (or use the pause
   menu's HUD style control): the weapon bar, score bar, minimap frame and killstreak column switch to the
   2.6.9.7 layout within half a second, without a reload. `set k33_hudStyle 0`: stock. Fire a few rounds: the
   big clip count and the gauge drop in the same frame as the reserve; reload refills both. With an akimbo weapon
   the clip count is both hands.
4. `set k33_hudStyle 1` (or 3): the stock HUD, the dvar back at 0, one line
   `hud style: k33_hudStyle 1 is not in this build (0:stock;2:v26); showing the stock HUD`.
5. Quit and start again: the last shipped value is still set (archived).
6. Repeat step 3 in Survival: the enemy / credit panel is the same in all three, the MP score panel and the
   killstreak column stay hidden.

## Why 3.3.3.3 and 3.5.2.6 are not shipped

Their operator numbering was inferred by aligning the menus each build shares with 2.6.9.7: the same menu, the same
statement place, the same number of entries and the same operands give one operator pair per position
(`scratchpad .../scripts/opalign.py`; the dumps are the same `zone-menus` dumps as for v26).

- 3.3.3.3: identical to 2.6.9.7 up to 97 (`spectatingfree`), then 2.6.9.7 + 2 from `keybinding` (107) through
  `getweaponfiretype` (241). 151 operator numbers paired; 2 conflicting pairs, each a single statement
  (`demofollowcamera` / `demofreecamera`). So the table does extend rather than reorder: two operators inserted
  around 98..106, new ones appended from 242.
- 3.5.2.6 (its common_mp dumps with the same tool): identical up to 96, +1 at 97, +8 from `keybinding` (113).

What stays unresolved are the operators new in 3.x (numbers past 2.6.9.7's table). No shared menu pairs them with
a name, and the executables cannot be read (3.3.3.3 has no operator names in the file, 3.5.2.6 is packed). They
sit exactly in the HUD:

| build | weapon bar | killstreaks_hd2 | score bar | all HUD menus |
|---|---|---|---|---|
| 3.3.3.3 | 26 of 26 items blocked (the menu's own visibility uses 258, 273) | 102 of 124 (255, 256, 271) | 45 of 59 (10 operators, 242..265) | 191 of 277 items |
| 3.5.2.6 | 38 of 38 blocked (menu visibility 260; also 275, 276, 278) | 128 of 128 (257, 258, 273, 279) | 26 of 47 (260, 266, 267, 291) | 195 of 314 items |

The minimap, reload warning and kill/death bar would convert, but a style without its weapon bar and killstreak
column is half a style, so neither ships. What would unblock them: the names of those operators (an unpacked 3.x
client or a memory dump of its operator table); then the same generator takes a `v33` / `v35` entry in `STYLES`,
and the 0x68-byte materials are no obstacle (the HUD art is re-authored from exported images as for v26).

## Live check

Own client folder (`D:\CoD\k33_hud_fix_client`: the model test folder's root files, players\ and p2p\ copied; patch\
a real folder with the candidate zones; chinese, temp, miles, usermaps junctions; main\ a real folder with
junctions for sh1x_rawfiles and video, hard links for the .iwd files, copies of the small files), a
`K33_LOCAL_TESTING=1 K33_DIAGNOSTICS=1` DLL, `+set k33_cmdFile 1 +set codol_local_test_multi 1`, Team Deathmatch on
Crash hosted in-process, pictures through `k33_shot.req`.

| capture | shows |
|---|---|
| `b_stock.png`, `b_v26_before.png`, `b_v12_before.png` | before the fixes: v26 / v12 clip over reserve ("4207"), empty gauge frame; stock "27 / 40" |
| `g_v26_full.png` | first fix: clip 30, reserve 30, full gauge inside the frame |
| `g_v26_fired_br.png`, `g_stock_fired_br.png` | after a burst: v26 clip 19, reserve 30, gauge 19/30 anchored right; stock shows 19 / 30 |
| `i_v26_full.png` | native ownerdraws: M9 clip 15 (its magazine, not doubled), reserve 30, full gauge; fire-mode tag 半自动 on its plate; the kill/death bar's fourth column 助 (assists) |
| `j_shots.png` | Vector: full auto, clip 30 / reserve 60 (one hand, not 60) |
| `k_seq.png` | fired, then reloaded, eight consecutive captures: clip 23 -> 30 and reserve 60 -> 53 change in the same frame; the gauge 23/30, then full |
| `v12_vs_v26.png` | the same scene with styles 1 and 2 (why v12 was dropped) |

Captures are in the session scratch folder (`hud_impl/shots`), not in the repository.

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

- `src/kernel33.c`: `#include "hud_style.c"` after `theatre.c`, `InstallHudStyle();` after `InstallTheatre();`.
- `src/patch_locale_policy.h` `kCodolPatchZoneMerged`: three rows (`hud_styles_postgfx_mp`,
  `codol_english_hud_styles`, `codol_chinese_hud_styles`), required by tests/test_zone_packs.py. Both files are
  pinned by `ui_baseline.json`, which is not re-recorded here.
- `hud_style.c` takes one `K33OnMainTick` slot. A local-testing build then registers seven before Product Pi's
  on-demand one. K33_MAX_TICKS was 8 and is 12 since the language switch and this feature each took a slot.
- `zone_packs/codol_common_postgfx_mp.csv` has its new build md5 recorded; the two locale packs gained a part and
  were left unrecorded (they already differed from their recorded md5 before this change).
